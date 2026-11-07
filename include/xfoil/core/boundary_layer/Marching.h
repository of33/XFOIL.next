// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

// File: include/xfoil/core/boundary_layer/Marching.h
// BL marching and Newton assembler of the new 0-based cores,
// directly ported from fortran-src/xbl.f MRCHUE (542-874),
// MRCHDU (875-1193) and SETBL (21-516).
//
// Formulas, constants, case distinctions, and operation order
// are adopted faithfully to the Fortran; internal computation in
// double (Fortran REAL). No raw pointers, no global state; the
// Fortran WRITEs (MRCHUE: inverse mode, convergence failed, etc.) are
// mapped to structured diagnostic states, not to stdout.
//
// No dependency on app, CLI, or tests.

#pragma once

#include <array>
#include <span>
#include <vector>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverKernels.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: COM state for marching (XBL.INC COM1/COM2 — the
// "1" station complete, the transition data of the "2" interval).
struct MarchSideState {
  BldifStation com1;    // Fortran COM1 ("1"-Station)
  double ueEi1{1.0};    // Fortran U1_UEI (BLPRV the "1"-Station)
  double ueMs1{0.0};    // Fortran U1_MS
  double ampl1{0.0};    // Fortran AMPL1 (separate of COM)
  double xt{0.0};       // Fortran XT (TRCHEK2 exit)
  double xtA1{0.0};     // Fortran XT_A1
  double xtT1{0.0};     // Fortran XT_T1
  double xtD1{0.0};     // Fortran XT_D1
  double xtU1{0.0};     // Fortran XT_U1
  double xtX1{0.0};     // Fortran XT_X1
  double xtT2{0.0};     // Fortran XT_T2
  double xtD2{0.0};     // Fortran XT_D2
  double xtU2{0.0};     // Fortran XT_U2
  double xtX2{0.0};     // Fortran XT_X2
  double xtMs{0.0};     // Fortran XT_MS
  double xtRe{0.0};     // Fortran XT_Re
  double xtXf{0.0};     // Fortran XT_XF
  bool tran{false};     // Fortran TRAN
  bool turb{false};     // Fortran TURB
  bool trforc{false};   // Fortran TRFORC
};

// Fortran: MRCHUE/MRCHDU inputs (xbl.f:542/875) — the mapping, the
// station arrays per side, the operating quantities, and the BL state (in/out).
struct MarchInput {
  BoundaryLayerMapping mapping;                // IBLTE/NBL/IPAN/VTI
  std::array<std::span<const double>, 2> xssi; // Fortran XSSI per Page
  std::span<const double> wgap;                // Fortran WGAP
  std::array<double, 2> acrit{9.0, 9.0};       // Fortran ACRIT
  std::array<double, 2> xiforc{0.0, 0.0};      // Fortran XIFORC (xifset)
  double ante{0.0};                            // Fortran ANTE
  BoundaryLayerProperties props;               // REYBL/GM1BL/HSTINV/...
  BoundaryLayerParameters blpar;               // BLPAR constants
  // BL state (in/out)
  BoundaryLayerStationField thet;              // Fortran THET
  BoundaryLayerStationField dstr;              // Fortran DSTR
  BoundaryLayerStationField ctau;              // Fortran CTAU
  BoundaryLayerStationField uedg;              // Fortran UEDG
  BoundaryLayerStationField mass;              // Fortran MASS
  BoundaryLayerStationField tau;               // Fortran TAU
  BoundaryLayerStationField dis;               // Fortran DIS
  BoundaryLayerStationField ctq;               // Fortran CTQ
  BoundaryLayerStationField delt;              // Fortran DELT
  BoundaryLayerStationField tstr;              // Fortran TSTR
  std::array<int, 2> itran{0, 0};              // Fortran ITRAN (0-based)
  std::array<bool, 2> tforce{false, false};    // Fortran TFORCE
  std::array<double, 2> xssitr{0.0, 0.0};      // Fortran XSSITR
  std::array<double, 2> xoctr{1.0, 1.0};       // Fortran XOCTR (COMMON,
                                               // display-only, persisted;
                                               // initial value 1.0, xfoil.f:679-680)
};

// Fortran: canonical WRITE values in BL marching/SETBL (xbl.f:84-93,
// 505-509, 561, 723-724, 779-781, 1102-1104). Pure value records without
// texts; byte-exact formatting lives in the CLI layer. The
// chronological order is produced by the core in execution order
// and must not be sorted afterwards.
enum class ViscousEventKind {
  boundaryLayerInitialization,  // xbl.f:85 'Initializing BL ...'
  marchingSideStart,            // xbl.f:561 ' side IS ...' (MRCHUE)
  mrchueInverseMode,            // xbl.f:723 (FORMAT 1300)
  mrchueConvergenceFailure,     // xbl.f:779 (FORMAT 1350)
  mrchduConvergenceFailure,     // xbl.f:1102 (FORMAT 1350)
  transitionFree,               // xbl.f:508 (FORMAT 9200)
  transitionForced,             // xbl.f:505 (FORMAT 9100)
  sourceInfluenceCalculation,   // xpanel.f:1156 QDCALC line in VISCAL
  stagnationPointNotFound,      // xpanel.f:1366 STFIND message
                                // (VISCAL header xoper.f:2919,
                                // Iterations via STMOVE xoper.f:2998)
  trchek2ConvergenceFailed,     // xblsys.f:423-425 (Label 100, FORMAT 6700)
  setblXtrWarning,              // xbl.f:228 'SETBL: Xtr??? n1 n2:'
};

// Phase within a VISCAL iteration: preHead = QDCALC before the
// VISCAL header (xoper.f:2964); initMarch = SETBL init block
// and MRCHUE messages (only for LBLINI=FALSE); postMarch = MRCHDU
// messages and the transition lines at the end of the SETBL side loop.
// The blank line xbl.f:90 lies between both phases.
enum class ViscousEventPhase {
  preHead,
  initMarch,
  postMarch,
};

struct ViscousEventRecord {
  ViscousEventKind kind{ViscousEventKind::boundaryLayerInitialization};
  ViscousEventPhase phase{ViscousEventPhase::postMarch};
  int iteration{0};              // 0-based VISCAL iteration
  int side{-1};                  // 0-based; -1 if not side-related
  int station{-1};               // 0-based (IBL); -1 if not station-related
  double hk{0.0};                // HTARG for inverse mode
  double residual{0.0};          // DMAX for convergence error
  double xOverC{0.0};            // XOCTR for transition
  int transitionStation{-1};     // ITRAN (0-based) for transition
  // Fortran format 6700 (xblsys.f:425) at the TRCHEK2 N2 failure:
  // X1, XT, X2, AMPL1, AMPLT, AMPL2, AX, DA2 from the last iteration.
  double trchekX1{0.0};
  double trchekXt{0.0};
  double trchekX2{0.0};
  double trchekAmpl1{0.0};
  double trchekAmplt{0.0};
  double trchekAmpl2{0.0};
  double trchekAx{0.0};
  double trchekDa2{0.0};
};

// Fortran: MRCHUE diagnostics (structured instead of stdout WRITE).
struct MarchDiagnostics {
  int lastInverseStation{-1};   // Fortran 'MRCHUE: Inverse mode at'
  double lastInverseHk{0.0};
  bool converged{true};         // false: 'Convergence failed at'
  int failedStation{-1};        // 0-based
  int failedSide{-1};           // 0-based
  double failedResidual{0.0};
  // Chronological event sequence of the WRITE values of this marching run.
  std::vector<ViscousEventRecord> events;
};

// Fortran: SUBROUTINE MRCHUE (xbl.f:542-874).
// Marches the BLs and the wake in direct mode with the UEDG array.
// For separation a plausible HK value is extrapolated upstream.
// Continuous transition check.
[[nodiscard]] MarchDiagnostics mrchue(MarchInput& in,
                                      std::array<MarchSideState, 2>& state);

// Fortran: SUBROUTINE MRCHDU (xbl.f:875-1193).
// Marches the BLs and the wake in mixed mode with the current
// Ue and Hk (Ue-Hk characteristic, Goldstein singularity never reached).
[[nodiscard]] MarchDiagnostics mrchdu(MarchInput& in,
                                      std::array<MarchSideState, 2>& state);

// Fortran: SETBL inputs (xbl.f:21-516) — the coupling state for the
// Newton assembler.
struct SetblInput {
  MarchInput march;                      // station arrays + BL state
  BoundaryLayerMapping mapping;          // IPAN/VTI/NBL/IBLTE
  std::array<std::vector<int>, 2> isys;  // Fortran ISYS (0-based lines)
  int nsys{0};                           // Fortran NSYS
  BoundaryLayerStationField uinv;        // Fortran UINV
  BoundaryLayerStationField uinvA;       // Fortran UINV_A
  std::span<const double> dij;           // Fortran DIJ (flat, row-major)
  int dijStride{0};
  double sstGo{0.0};                     // Fortran SST_GO (Stagnation point)
  double sstGp{0.0};                     // Fortran SST_GP
  double sst{0.0};                       // Fortran SST
  double xle{0.0};                       // Fortran XLE
  double xte{0.0};                       // Fortran XTE
  double yle{0.0};                       // Fortran YLE
  double yte{0.0};                       // Fortran YTE
  std::span<const double> xPan;          // Fortran X (Panel geometry)
  std::span<const double> xpPan;         // Fortran XP (spline derivative)
  std::span<const double> yPan;          // Fortran Y
  std::span<const double> ypPan;         // Fortran YP
  std::span<const double> sPan;          // Fortran S (panel arc length)
  int nPan{0};                           // Fortran N
  int ist{0};                            // Fortran Is (0-based, TINDEX)
  double cl{0.0};                        // Fortran CL
  double clspec{0.0};                    // Fortran CLSPEC
  bool lalfa{false};                     // Fortran LALFA
  bool lblini{false};                    // Fortran LBLINI (in/out)
  int machType{1};                       // Fortran MATYP (MRCL input)
  int reType{1};                         // Fortran RETYP (MRCL input)
  double minf1{0.0};                     // Fortran MINF1 (MRCL input)
  double reinf1{3.0e6};                  // Fortran REINF1 (MRCL input)
  double minf{0.0};                      // Fortran MINF (in/out, MRCL output)
  double reinf{1.0e6};                   // Fortran REINF (in/out, MRCL output)
  double qinf{1.0};                      // Fortran QINF
  double gamma{1.4};                     // Fortran GAMMA
  double gamm1{0.4};                     // Fortran GAMM1
  double hvrat{0.0};                     // Fortran HVRAT
  int idamp{0};                          // Fortran IDAMP
  double tklam{0.0};                     // Fortran TKLAM (COMSET exit)
  double tkMsq{0.0};                     // Fortran TK_MSQ
  // Newton workspace (in/out)
  BlsysWorkspace ws;
};

// Fortran: SETBL diagnostics (structured instead of stdout WRITE).
struct SetblDiagnostics {
  std::array<bool, 2> tforce{false, false};     // Fortran TFORCE
  std::array<double, 2> xoctr{0.0, 0.0};        // Fortran XOCTR
  std::array<double, 2> yoctr{0.0, 0.0};        // Fortran YOCTR
  std::array<int, 2> itran{0, 0};               // Fortran ITRAN (0-based)
  std::array<double, 2> xssitr{0.0, 0.0};       // Fortran XSSITR
  std::array<double, 2> tindex{0.0, 0.0};       // Fortran TINDEX
  BoundaryLayerStationField uslp;               // Fortran USLP (plot exit)
  bool xtrWarning{false};                       // Fortran 'SETBL: Xtr???'
  double xtrAmpl1{0.0};
  double xtrAmpl2{0.0};
  MarchDiagnostics marchDiagnostics;            // MRCHDU exit (last marching)
  // Chronological event sequence of this SETBL call: init block and
  // MRCHUE messages (phase initMarch), then MRCHDU messages and the
  // transition lines at the end of the side loop (phase postMarch).
  std::vector<ViscousEventRecord> events;
  bool lblini{false};                           // Fortran LBLINI (in/out)
};

// Fortran: SUBROUTINE SETBL (xbl.f:21-516).
// Sets the Newton-system coefficients for the current BL variables
// and edge velocities, and adds them to the global Newton
// system.
[[nodiscard]] SetblDiagnostics setbl(SetblInput& in,
                                     std::array<MarchSideState, 2>& state);

}  // namespace xfoil::core::boundary_layer

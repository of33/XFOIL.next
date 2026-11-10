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

// File: include/xfoil/core/boundary_layer/Viscal.h
// VISCAL of the new 0-based cores, directly
// ported from fortran-src/xoper.f VISCAL (2892-3116).
//
// Contained routine:
// VISCAL -> viscal session-orchestrated Newton loop driver
//
// The `.bl` block (xoper.f:3031-3098) is part of the canonical
// Fortran routine VISCAL. The numerical VISCAL kernel belongs to it;
// the `.bl` file/presentation side lives in the IO layer,
// which is why the complete Fortran routine VISCAL is not yet
// fully migrated here. The Fortran WRITEs
// (iteration output, convergence message) are mapped to structured
// diagnostics, not to stdout.
//
// XYWAKE (xoper.f:2905) is not part of the driver: the wake geometry
// is an input (LWAKE-like: the caller has executed XYWAKE). The
// remaining session calls (QWCALC, QISET, LIPAN section, UICALC,
// LBLINI init, LVCONV block, QDCALC, Newton loop, CPCALC/MHINGE) follow
// the Fortran control-flow order formula-faithfully.
//
// No raw pointers, no global state. No dependency on app,
// CLI, or tests.

#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>
#include <cstddef>
#include <optional>
#include <vector>

#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/boundary_layer/Marching.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: VISCAL session parameters (xoper.f:2892-3116) — fixed per call.
// DTOR is the Fortran session constant PI/180 (xfoil.f:395); VACCEL the
// xfoil.f INIT value 0.01 (xfoil.f:701); ACRIT/XIFORC are session
// operating quantities (XTR/INIT).
struct ViscalInput {
  std::size_t niter{0};   // Fortran NITER1 (0 -> Fortran ASKI; here
                          // controlled rejected)
  double qinf{1.0};       // Fortran QINF
  double xref{0.0};       // Fortran XCMREF (moment reference point)
  double yref{0.0};       // Fortran YCMREF
  double vaccel{0.01};    // Fortran VACCEL (COMMON/CR17/, xfoil.f INIT)
  double dtor{0.017453292519943295};  // Fortran DTOR = PI/180 (xfoil.f:395)
  bool lalfa{true};       // Fortran LALFA (alpha given)
  bool lvisc{true};       // Fortran LVISC
  bool lflap{false};      // Fortran LFLAP (MHINGE call at the end)
  double xof{0.0};        // hinge location x (only LFLAP)
  double yof{0.0};        // hinge location y (only LFLAP)
  int machType{1};        // Fortran MATYP
  int reType{1};          // Fortran RETYP
  double minf1{0.0};      // Fortran MINF1 (MRCL input)
  double reinf1{0.0};     // Fortran REINF1 (MRCL input)
  double clspec{0.0};     // Fortran CLSPEC (LALFA=false)
  int idamp{0};           // Fortran IDAMP (damping flag, session)
  double minfCl{0.0};     // Fortran MINF_CL (session, MRCL exit)
  std::size_t ivx{283};   // Fortran IVX (canonical BL capacity)
  std::array<double, 2> acrit{9.0, 9.0};   // Fortran ACRIT(2)
  std::array<double, 2> xiforc{0.0, 0.0};  // Fortran XIFORC(2) (initial value;
                                           // reset per iteration by XIFSET,
                                           // xbl.f:165/566/902)
  std::array<double, 2> xstrip{1.0, 1.0};  // Fortran XSTRIP(2) (XTR, session)
  // BLPAR constants (COMMON/BLPAR) passed explicitly by the caller;
 // no default fallback in the kernel (single source).
  BoundaryLayerParameters blpar;
};

// Fortran QVIS/QINV_A are COMMON fields (XFOIL.INC CR04/CR05, fixed
// dimension IZX), present zero-initialized at program start long
// before the first viscous point is computed. GAMQV
// (xpanel.f 1616-1625) reads QVIS(1..N) already in this state
// if OPER INIT switches LBLINI to true without a previous VISCAL
// run (binary case oper-state/oper-init-inviscid-then-bang). The
// C++ session distinguishes explicitly:
// commonInitialized - zero-initialized COMMON field,
// still never viscously computed;
// solved - QVIS contains the result of a QVFUE evaluation.
enum class ViscousVelocityFieldState {
  commonInitialized,
  solved,
};

// Fortran: session and BL state (XFOIL.INC COMMON block equivalent),
// mutable during the Newton loop (in/out). The caller
// initializes the inviscid basis (Alfa/MINF/REINF/Cl, GAM/GAM_A) and
// the BL state; the driver carries it forward (continuation with further
// iterations possible).
struct ViscalState {
  // Fortran QINVU(IQX,2) basis (GGCALC + QWCALC exit, QISET input in the
  // Newton loop).
  std::vector<double> qinvu1;  // n+1+nw (basis alpha=0)
  std::vector<double> qinvu2;  // n+1+nw (basis alpha=90)
  // Fortran QINV/QINV_A/QVIS/GAM/GAM_A.
  std::vector<double> qinv;   // n+1+nw
  std::vector<double> qinvA;  // n+1+nw
  std::vector<double> qvis;   // n+1+nw (only airfoil nodes described)
  // Fortran COMMON initialization state of QVIS (see
  // ViscousVelocityFieldState). commonInitialized corresponds to the
  // zero-initialized COMMON field before the first QVFUE.
  ViscousVelocityFieldState qvisState{
      ViscousVelocityFieldState::commonInitialized};
  std::vector<double> gam;    // n
  std::vector<double> gamA;   // n
  // Fortran IPAN/VTI/IBLTE/NBL (LIPAN), XSSI, WGAP, ISYS/NSYS, DIJ.
  BoundaryLayerMapping mapping;
  BoundaryLayerArcLengths arcs;
  WakeGap wgap;
  BlsysMapping isys;
  std::vector<double> dij;  // flat, row-major (n+1)*(n+nw)
  int dijStride{0};
  // Fortran LADIJ/LWDIJ (XFOIL.INC): the airfoil DIJ block (LADIJ) depends
  // only on the panel geometry (BIJ-invariant) and is preserved between
  // points; the wake-dependent DIJ block (LWDIJ) is invalidated per point
  // by XYWAKE (xpanel.f:1350 LWDIJ=.FALSE.) and rebuilt by QDCALC with
 // the new wake geometry. dij.empty corresponds to .NOT.LADIJ;
  // dijWakeValid corresponds to LWDIJ.
  bool dijWakeValid{false};
  // Fortran UINV/UINV_A and BL station fields (CR15).
  BoundaryLayerStationField uinv;
  BoundaryLayerStationField uinvA;
  BoundaryLayerStationField thet;
  BoundaryLayerStationField dstr;
  BoundaryLayerStationField ctau;
  BoundaryLayerStationField uedg;
  BoundaryLayerStationField mass;
  BoundaryLayerStationField tau;
  BoundaryLayerStationField dis;
  BoundaryLayerStationField ctq;
  BoundaryLayerStationField delt;
  BoundaryLayerStationField tstr;
  BoundaryLayerStationField dssr;
  BoundaryLayerStationField mdef;
  BoundaryLayerStationField pdef;
  BoundaryLayerStationField kdef;
  std::array<int, 2> itran{0, 0};       // Fortran ITRAN (0-based)
  std::array<double, 2> xoctr{1.0, 1.0};  // Fortran XOCTR (COMMON, display-only;
                                          // initial value 1.0, xfoil.f:679-680)
  std::array<double, 2> tindex{0.0, 0.0};  // Fortran TINDEX (COMMON, set per side
                                           // by SETBL; PLRADD xpol.f:596)
  std::array<double, 2> xssitr{0.0, 0.0};  // Fortran XSSITR (COMMON/XBL.INC,
                                           // set to XT per side by SETBL;
                                           // dplot.f FBLGET TURB decision)
  std::array<MarchSideState, 2> marchState{};  // Fortran COM1/XT states
  // Fortran ALFA/MINF/REINF/CL/CM/CDP/CD/CDF/CLF/TKLAM/MINF_CL (Session).
  double alfa{0.0};
  double minf{0.0};
  double reinf{0.0};
  double cl{0.0};
  double cm{0.0};
  double cdp{0.0};
  double cd{0.0};
  double cdf{0.0};
  double clf{0.0};
  double tklam{0.0};   // Fortran TKLAM (COMSET exit)
  double tkMsq{0.0};   // Fortran TK_MSQ
  double minfCl{0.0};  // Fortran MINF_Cl (for the next UPDATE)
  // Fortran SST/SST_GO/SST_GP/Is (Stagnation point position).
  double sst{0.0};
  double sstGo{0.0};
  double sstGp{0.0};
  std::size_t ist{0};  // Fortran Is (0-based)
  // Fortran LVCONV/LBLINI (in/out).
  bool lvconv{false};
  bool lblini{false};
  // Fortran AVISC/MVISC (converged operating-point values, in/out).
  double avisc{0.0};
  double mvisc{0.0};
  // Fortran SETBL data part (RSTBL/HSTINV/REYBL/DWTE, in/out; IDAMPV
  // is preserved from the previous value).
  BoundaryLayerProperties props;
};

// The station fields read by PLXADD (IPAN, THET, DSTR, CTAU, TAU)
// form only a fixed-dimensioned COMMON field in Fortran
// (CR15, IVX). syncPlxaddStationCapacity keeps these views on a
// shared capacity: grow-only, new positions zero (INITCOM
// state never wrote those entries), existing values remain
// unchanged. PLXADD (xpol.f) reads up to NSIDE(IS) from exactly this
// shared state.
inline void syncPlxaddStationCapacity(
    BoundaryLayerMapping& map,
    std::initializer_list<BoundaryLayerStationField*> fields) {
  std::size_t top = map.top.ipan.size();
  std::size_t bottom = map.bottom.ipan.size();
  for (BoundaryLayerStationField* f : fields) {
    top = std::max(top, f->top.size());
    bottom = std::max(bottom, f->bottom.size());
  }
  ensureIpanCapacity(map, top, bottom);
  for (BoundaryLayerStationField* f : fields) {
    if (f->top.size() < top) {
      f->top.resize(top, 0.0);
    }
    if (f->bottom.size() < bottom) {
      f->bottom.resize(bottom, 0.0);
    }
  }
}

inline void syncPlxaddStationCapacity(ViscalState& state) {
  syncPlxaddStationCapacity(state.mapping,
                            {&state.thet, &state.dstr, &state.ctau,
                             &state.tau});
}

// Fortran: VISCAL exit (xoper.f:3026-3028) — session results after
// convergence or iteration limit.
// Fortran: UPDATE diagnostics per Newton iteration (xbl.f:1253-1560,
// WRITE in the VISCAL loop xoper.f:2990-3000). The aerodynamic fields
// alfa/cl/cm/cd/cdf correspond to the state immediately after CLCALC/
// CDCALC of the same iteration (xoper.f:3001-3013) and are the only
// sources for format 2020 (CDPDIF = Cd - CDF). Pure value data without
// formatted text.
struct ViscalIterationRecord {
  double rmsbl{0.0};   // Fortran RMSBL
  double dac{0.0};     // Fortran DAC
  double rlx{0.0};     // Fortran RLX
  double rmxbl{0.0};   // Fortran RMXBL
  char vmxbl{'\0'};    // Fortran VMXBL ('n'/'C'/'T'/'D'/'U'); COMMON start NUL
  int imxbl{-1};       // Fortran IMXBL (0-based; COMMON start 0 -> -1)
  int ismxbl{-1};      // Fortran ISMXBL (0-based; COMMON start 0 -> -1)
  double alfa{0.0};    // Fortran Alfa (radians) after this UPDATE
  double cl{0.0};      // Fortran Cl from CLCALC this iteration
  double cm{0.0};      // Fortran CM from CLCALC this iteration
  double cd{0.0};      // Fortran Cd from CDCALC this iteration
  double cdf{0.0};     // Fortran CDF from CDCALC this iteration
};

// Fortran: VISCAL completion block (xoper.f:3029-3057) before format 9922
// (xoper.f:3060). Computed on side 1 (IS=1, top) from the retained
// BL state after label 90, independent of convergence or
// non-convergence. XOCTR(1) comes from the last SETBL call of the point.
// Pure value data; the text formatting lives in the CLI layer.
struct ViscalCompletionRecord {
  double acrit{0.0};  // Fortran ACRIT(1)
  double hkmax{0.0};  // Fortran HKMAX = max DSTR/THET on side 1
  double cd{0.0};     // Fortran Cd of the Completion state
  double psep{0.0};   // Fortran PSEP (Hk crossing 4.0 descending)
  double patt{0.0};   // Fortran PATT (Hk crossing 4.0 ascending)
  double delp{0.0};   // Fortran DELP = PATT - PSEP
  double xoctr{0.0};  // Fortran XOCTR(1) of the last SETBL
};

struct ViscalResult {
  bool converged{false};       // Fortran LVCONV after RMSBL < EPS1
  std::size_t iterations{0};   // executed Newton iterations
  double alfa{0.0};            // Fortran ALFA
  double minf{0.0};            // Fortran MINF
  double cl{0.0};              // Fortran CL
  double cm{0.0};              // Fortran CM
  double cdp{0.0};             // Fortran CDP (from CLCALC)
  double cd{0.0};              // Fortran CD (Squire-Young)
  double cdf{0.0};             // Fortran CDF (friction)
  double clf{0.0};             // Fortran CLF (friction lift)
  double rmsbl{0.0};           // Fortran RMSBL after the last UPDATE
  std::vector<double> rmsblHistory;  // Fortran RMSBL per iteration
  std::vector<ViscalIterationRecord> iterationRecords;  // UPDATE per iteration
  // Chronological BL events (SETBL/MRCHUE/MRCHDU/TRCHEK writes) per
  // iteration in execution order; the iteration field is 0-based.
  std::vector<ViscousEventRecord> events;
  // Fortran completion block xoper.f:3029-3057 (format 9922); valid if
  // VISCAL reaches label 90 (converged or not).
  ViscalCompletionRecord completion;
  bool completionValid{false};
  std::vector<double> cp;      // Fortran CPI (n+1+nw)
  std::vector<double> cpv;     // Fortran CPV (n+1+nw)
  // Fortran TINDEX(1..2) (COMMON, set per side by SETBL; PLRADD
  // xpol.f:596 reads the value of the last SETBL call). Diagnostic
  // handover without numerical effect.
  std::array<double, 2> tindex{0.0, 0.0};
  std::optional<inviscid::HingeMomentResult> hinge;  // only for LFLAP
};

// Fortran: SUBROUTINE VISCAL(NITER1) (xoper.f:2892-3116).
// Converges the viscous operating point. Session-orchestrated
// routine: QWCALC -> QISET -> (LIPAN: GAMQV/STFIND/IBLPAN/XICALC/IBLSYS
// if mapping invalid) -> UICALC -> LBLINI init (UEDG = UINV) ->
// (LVCONV block) -> QDCALC if DIJ missing -> Newton loop (SETBL ->
// BLSOLV -> UPDATE -> MRCL/COMSET or QISET/UICALC -> QVFUE -> GAMQV ->
// STMOVE -> CLCALC -> CDCALC -> convergence test RMSBL < EPS1 = 1e-4) ->
// CPCALC/CPCALC -> MHINGE if LFLAP. NITER1=0 is rejected in a
// controlled way (Fortran ASKI dialog). The `.bl` block (xoper.f:3031-
// 3098) is a canonical component and is not yet fully
// migrated here.
[[nodiscard]] ViscalResult viscal(const ViscalInput& in,
                                  const paneling::PanelGeometry& panel,
                                  const inviscid::InfluenceSystem& gs,
                                  const inviscid::WakeState& wake,
                                  const inviscid::TrailingEdgeState& te,
                                  ViscalState& state);

}  // namespace xfoil::core::boundary_layer

// End: include/xfoil/core/boundary_layer/Viscal.h

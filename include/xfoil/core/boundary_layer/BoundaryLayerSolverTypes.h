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

// File: include/xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h
// Viscous and coupled solver of the new 0-based cores,
// ported from fortran-src/xbl.f, xblsys.f, xsolve.f, xpanel.f and xoper.f.
//
// Contained types:
// BoundaryLayerIterationState iteration/convergence state (RLX/RMSBL)
// BoundaryLayerResidual residual vector (VDEL) and RMS/max
// BoundaryLayerSensitivity sensitivities of the BL variables (V_VARA)
// BoundaryLayerNewtonSystem VA/VB block system and workspace
// CouplingState viscous-inviscid coupling (QV/QF/GAM)
// ViscousResult viscous result (CL/CM/CDP/CDF/CD)
// ViscousConvergence convergence/diagnostic state
//
// Canonical sources:
// xbl.f SETBL/IBLSYS/MRCHUE/MRCHDU/UPDATE/DSLIM
// xblsys.f BLSYS/TESYS/BLVAR/BLMID/TRDIF/BLDIF/DAMPL/DAMPL2 + Correlations
// xsolve.f BLSOLV (Block-Thomas with LUDCMP/BAKSUB)
// xoper.f VISCAL/FCPMIN + viscous branches SPECAL/SPECCL/MHINGE
// xfoil.f CDCALC
// xpanel.f UICALC/UECALC/QVFUE/GAMQV/STMOVE/UESET/DSSET
//
// Requirements :
// - permanent station values separate from temporary Newton workspaces,
// - upper/lower side and wake explicit,
// - no Fortran EQUIVALENCE memory overlay (V_VAR1/V_VAR2
// grouped per routine, not as 73 public fields),
// - 0-based indices,
// - no global flags, no raw pointers,
// - controlled errors and diagnostics (Result<T>/Diagnostic).
//
// No dependency on app, CLI, or tests.

#pragma once

#include <cstddef>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::boundary_layer {

// Fortran: RLX/RMSBL/RMXBL from UPDATE; ITER from VISCAL.
struct BoundaryLayerIterationState {
  double rlx{0.0};       // Fortran RLX (Under-relaxation factor)
  double rmsbl{0.0};     // Fortran RMSBL (RMS of the changes)
  double rmxbl{0.0};     // Fortran RMXBL (maximum change)
  int iter{0};           // Fortran ITER (Newton-iteration count)
  int niterMax{0};       // Fortran NITER1 (iteration limit)
};
// Fortran: THET/DSTR/CTAU/UEDG/MASS (XFOIL.INC CR15, (IVX,ISX)) —
// permanent station values per side, separate from the temporary
// Newton workspaces. Fortran IBL 1..NBL -> C++ 0-based.
struct BoundaryLayerStationField {
  std::vector<double> top;     // Fortran (IBL,1), 0-based
  std::vector<double> bottom;  // Fortran (IBL,2), 0-based
};

// Fortran: persistent BL solver state (CR15 station arrays + CI05-/
// Cl02-transition indices). No EQUIVALENCE overlay; the
// station fields are kept 0-based per side.
struct BoundaryLayerSolverState {
  BoundaryLayerStationField thet;   // Fortran THET (momentum thickness)
  BoundaryLayerStationField dstr;   // Fortran DSTR (displacement thickness)
  BoundaryLayerStationField ctau;   // Fortran CTAU (sqrt(max skin-friction coefficient)
                                    // or laminar amplification factor)
  BoundaryLayerStationField uedg;   // Fortran UEDG (edge velocity)
  BoundaryLayerStationField mass;   // Fortran MASS (mass defect = DSTR*UEDG)
  int itranTop{0};                  // Fortran ITRAN(1) (0-based)
  int itranBottom{0};               // Fortran ITRAN(2) (0-based)
  bool tforceTop{false};            // Fortran TFORCE(1)
  bool tforceBottom{false};         // Fortran TFORCE(2)
  double xssitrTop{0.0};            // Fortran XSSITR(1)
  double xssitrBottom{0.0};         // Fortran XSSITR(2)
};

// Fortran: VDEL residual vector (XBL.INC) with RMSBL/RMXBL summary.
struct BoundaryLayerResidual {
  std::vector<double> vdel;  // Fortran VDEL (NVX entries, 0-based)
  double rms{0.0};           // Fortran RMSBL
  double rmax{0.0};          // Fortran RMXBL
};

// Fortran: sensitivity arrays V_VARA (XBL.INC) — d(variable)/d(alpha).
// One set per side (SurfaceSide 0/1), 0-based.
struct BoundaryLayerSensitivity {
  std::vector<double> top;
  std::vector<double> bottom;
};

// Fortran: VA/VB block system (XBL.INC COMMON/VDA1/) and VM/VZ workspace.
// VA/VB: (NSYS x 3) block matrix (Fortran VA(NVX,3), VB(NVX,2*NX)),
// 0-based; vdel: residual vector; vm: solution vector.
struct BoundaryLayerNewtonSystem {
  std::vector<double> va;   // Fortran VA(NSYS,3), 0-based
  std::vector<double> vb;   // Fortran VB(NSYS,2*NSYS), 0-based
  std::vector<double> vm;   // Fortran VM (solution vector)
  std::vector<double> vdel; // Fortran VDEL (Residual vector)
  std::vector<double> vz;   // Fortran VZ (Workspace)
  int nsys{0};              // Fortran NSYS (number of Newton lines)
};

// Fortran: QV/QF mixing (QVFUE) and GAM/QV coupling (GAMQV).
struct CouplingState {
  std::vector<double> qv;   // Fortran QV (viscous edge velocity)
  std::vector<double> qf;   // Fortran QF (inviscid edge velocity)
  double relx{0.0};         // Fortran RELX (coupling relaxation)
};

// Fortran: Cl/CM/CDP/CDF/Cd from VISCAL/CDCALC.
struct ViscousResult {
  double cl{0.0};   // Fortran Cl (viscous lift)
  double cm{0.0};   // Fortran CM (moment)
  double cdp{0.0};  // Fortran CDP (form drag)
  double cdf{0.0};  // Fortran CDF (skin-friction drag)
  double cd{0.0};   // Fortran CD (overall drag)
  double clspec{0.0};  // Fortran CLSPEC (target Cl)
};

// Fortran: VISCAL convergence/diagnostic state.
struct ViscousConvergence {
  bool converged{false};    // Fortran RMSBL < ACCTOL
  int iter{0};              // Newton-iteration count
  bool newtonDiverged{false};  // Fortran 'Newton divergence'
  bool reInitialized{false};   // Fortran LBLINI=FALSE (new BL initialization)
  int maxIter{0};              // Fortran NITER1
};

// Safety contract: all routines check their span lengths before
// use. Invalid sizes throw std::invalid_argument.

}  // namespace xfoil::core::boundary_layer

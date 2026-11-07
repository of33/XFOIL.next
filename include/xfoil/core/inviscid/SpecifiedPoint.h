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

// File: include/xfoil/core/inviscid/SpecifiedPoint.h
// SPECAL/SPECCL kernels of the new 0-based cores, ported
// from fortran-src/xoper.f (SPECAL, SPECCL) — only the inviscid numerical
// semantics.
//
// Contained operations:
// SPECAL -> specAlpha SPECCL -> specCl
//
// Only the inviscid numerical kernel lives here: GAM overlay,
// TECALC, QISET, MRCL, COMSET, CLCALC, CPCALC, and the Newton loops.
// Outside this kernel: VISCAL call, polar accumulation, plot,
// stdout orchestration, menu/repeat logic.
//
// Convergence bounds exact from the Fortran: |DCLM| <= 1.0E-6 or
// |DALFA| <= 1.0E-6, max 20 iterations, SPECAL under-relaxation max 12.
// The non-convergence messages are returned as structured diagnostics
// (Fortran: 'SPECAL: Minf convergence failed' or
// 'SPECCL: CL convergence failed').
//
// No raw-pointer API, no dependency on app, CLI, or tests.

#pragma once

#include <optional>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/inviscid/TrailingEdge.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::inviscid {

// Fortran: Newton iteration limit (SPECAL and SPECCL).
constexpr int kMaxSpecIterations = 20;

// Fortran: Convergence tolerances.
constexpr double kSpecTolerance = 1.0e-6;

// Fortran: LVISC/LFLAP branch inputs (xoper.f:2807-2813 or 2880-2886).
// The wake bases (qinvu1/qinvu2) must carry n+1+nw entries
// (GGCALC + QWCALC), qvis n+1+nw (QVIS, only airfoil nodes described).
// When default-constructed, the branch is inactive (inviscid CPCALC over N).
struct SpecViscousExt {
  bool lvisc{false};
  std::span<const double> qinvu1;  // n+1+nw (basis alpha=0)
  std::span<const double> qinvu2;  // n+1+nw (basis alpha=90)
  std::span<const double> qvis;    // n+1+nw (viscous panel velocity)
  bool lflap{false};
  double xof{0.0};                 // hinge location (only LFLAP)
  double yof{0.0};
};

// Result of both inviscid control operations.
// Fortran: canonical convergence aborts in SPECAL/SPECCL. Both
// branches write their message and then fall back to the
// completion block (xoper.f:2798-2800, 2873-2875); no earlier RETURN
// occurs. The types are uniquely distinguishable.
enum class SpecifiedPointFailure {
  none,
  specalMachConvergenceFailure,  // xoper.f:2799 'SPECAL: Minf convergence failed'
  specclLiftConvergenceFailure,  // xoper.f:2874 'SPECCL: CL convergence failed'
};

struct SpecPointResult {
  double alfa{0.0};
  double clm{0.0};      // SPECAL: converged CLM (Newton variable)
  double cl{0.0};
  double cm{0.0};
  double cdp{0.0};
  double minf{0.0};
  double reinf{0.0};
  std::vector<double> gam;    // n
  std::vector<double> gamA;   // n
  std::vector<double> qinv;   // n (inviscid) or n+1+nw (LVISC branch)
  std::vector<double> qinvA;  // ditto
  std::vector<double> cp;     // n (inviscid) or n+1+nw (LVISC branch, CPI)
  std::vector<double> cpv;    // only LVISC branch: CPV from QVIS (n+1+nw)
  std::optional<HingeMomentResult> hinge;  // only LFLAP
  TrailingEdgeState te;
  bool converged{true};       // tolerance satisfied (otherwise diagnostics)
  SpecifiedPointFailure failure{SpecifiedPointFailure::none};
};

// Fortran: SUBROUTINE SPECAL (inviscid kernel)
// Converges to a prescribed alpha. system is the influence system from
// ggCalc; panel the panel geometry; settings the MRCL inputs; alfa the
// target angle of attack (radians); xref/yref the CM reference point
// (session defaults 0.25/0.0). ext enables the viscous LVISC/LFLAP
// branches (xoper.f:2807-2813).
Result<SpecPointResult> specAlpha(const InfluenceSystem& system,
                                  const paneling::PanelGeometry& panel,
                                  const MachReynoldsSettings& settings,
                                  double alfa, double qinf, double xref,
                                  double yref,
                                  const SpecViscousExt& ext = {});

// Fortran: SUBROUTINE SPECCL (inviscid kernel)
// Converges to a prescribed inviscid Cl. clspec is the target Cl;
// alfa the starting angle of attack (Newton variable, radians). ext
// enables the viscous LVISC/LFLAP branches (xoper.f:2880-2886).
Result<SpecPointResult> specCl(const InfluenceSystem& system,
                               const paneling::PanelGeometry& panel,
                               const MachReynoldsSettings& settings,
                               double clspec, double alfa, double qinf,
                               double xref, double yref,
                               const SpecViscousExt& ext = {});

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/SpecifiedPoint.h

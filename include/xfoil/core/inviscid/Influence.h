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

// File: include/xfoil/core/inviscid/Influence.h
// Influence coefficient and circulation kernels of the new 0-based cores,
// directly ported from fortran-src/xpanel.f (PSILIN, PSWLIN,
// GGCALC, QWCALC, QDCALC, XYWAKE).
//
// Contained routines:
// PSILIN -> psiLin PSWLIN -> pswLin
// GGCALC -> ggCalc QWCALC -> qwCalc
// QDCALC -> qdCalc XYWAKE -> xywake
//
// Formulas, operation order, and special cases are adopted exactly from
// the Fortran. The SIGLIN parts use SIG as input (the
// influence coefficients DZDM/DQDM are SIG-independent); the GEOLIN part
// (dPsi/dn, Z_QDOF0..3 inverse DOFs) belongs to the inverse kernels
// with geolin=false. The LIMAGE image-point system is inactive in the
// canonical workflow (INIT: LIMAGE=.FALSE.) and is ported for
// completeness.
//
// The numerical kernel does not write to stdout. No raw-pointer API,
// no dependency on app, CLI, or tests.

#pragma once

#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::inviscid {

// Fortran gas constants from COMMON/CR11/: PI = 4.0*ATAN(1.0) (xfoil.f INIT).
double invPi();

// Fortran: SUBROUTINE PSILIN(I,XI,YI,NXI,NYI,PSI,PSI_NI,GEOLIN,SIGLIN)
// Stream function and sensitivities at a point (airfoil or
// wake node). The panel geometry (x/y/s/nx/ny/apanel, n entries each)
// and the vortex state (gam/gamA/gamu1/gamu2, n each) are inputs;
// sig is the source distribution (n entries, inviscid: 0). node is the
// 0-based query node; nodeOnAirfoil=true for airfoil nodes
// (Fortran IO in 1..N -> SGN=1). geolin is not supported here;
// siglin enables the source share.
struct PsiLinInputs {
  std::size_t node{0};
  double xi{0.0};
  double yi{0.0};
  double nxi{0.0};
  double nyi{0.0};
  double alfa{0.0};
  bool nodeOnAirfoil{true};
  bool siglin{false};
  bool geolin{false};
  // DOF shape functions QF0..QF3 (used only for geolin=true).
  std::span<const double> qf0;
  std::span<const double> qf1;
  std::span<const double> qf2;
  std::span<const double> qf3;
};

struct PsiLinContext {
  std::span<const double> x;
  std::span<const double> y;
  std::span<const double> s;
  std::span<const double> nx;
  std::span<const double> ny;
  std::span<const double> apanel;
  std::span<const double> gam;
  std::span<const double> gamA;
  std::span<const double> gamu1;
  std::span<const double> gamu2;
  std::span<const double> sig;
  bool sharp{false};
  double dste{0.0};
  double ante{0.0};
  double aste{0.0};
  double qinf{0.0};
  bool image{false};   // Fortran LIMAGE (INIT: .FALSE.)
  double yimage{0.0};
};

PsiLinResult psiLin(const PsiLinInputs& in, const PsiLinContext& ctx);

// Fortran: SUBROUTINE PSWLIN(I,XI,YI,NXI,NYI,PSI,PSI_NI)
// Wake-source stream function and sensitivities. The wake nodes
// (x/y/apanel/sig, nw entries each) are inputs; nodeWake=true for a
// wake query node (Fortran IO in N+1..N+NW -> SGN=1).
struct PswLinInputs {
  double xi{0.0};
  double yi{0.0};
  double nxi{0.0};
  double nyi{0.0};
  double alfa{0.0};
  bool nodeWake{false};
};

struct PswLinContext {
  std::span<const double> x;
  std::span<const double> y;
  std::span<const double> apanel;
  std::span<const double> sig;
};

PswLinResult pswLin(const PswLinInputs& in, const PswLinContext& ctx);

// Fortran: SUBROUTINE GGCALC (no arguments, COMMON-based)
// Computes the alpha=0/90 vortex distributions and the influence system
// (AIJ, BIJ, Pivot). panel is the panel geometry; sig is the
// source distribution (inviscid: 0). minf/qinf are operating quantities.
Result<InfluenceSystem> ggCalc(const paneling::PanelGeometry& panel,
                               std::span<const double> sig, double minf,
                               double qinf);

// Fortran: SUBROUTINE QWCALC (no arguments, COMMON-based)
// Sets the wake base velocities QINVU(N+1..N+NW) from the
// freestream and the airfoil vortex distribution. system is the factorized
// influence system from ggCalc; wake is the wake-node state.
// Returns QINVU for the wake nodes (0-based, nw entries
// per basis).
struct WakeBasisResult {
  std::vector<double> qinvu1;
  std::vector<double> qinvu2;
};

WakeBasisResult qwCalc(const InfluenceSystem& system, const WakeState& wake,
                       const paneling::PanelGeometry& panel, double alfa, double qinf);

// Fortran: SUBROUTINE QDCALC (no arguments, COMMON-based)
// Computes the source-influence matrix (dGam/dSig and dQtan/dSig) for
// the current airfoil and wake geometry. Returns DIJ as a
// row-major (n+1) x (n+nw) matrix (Fortran DIJ(I,J), I=1..N+1,
// J=1..N+NW).
struct SourceInfluenceResult {
  std::vector<double> dij;  // (n+1)*(n+nw) row-major
};

SourceInfluenceResult qdCalc(const InfluenceSystem& system,
                             const WakeState& wake,
                             const paneling::PanelGeometry& panel, double alfa,
                             double qinf, std::span<const double> sig);

// Fortran: SUBROUTINE XYWAKE (no arguments, COMMON-based)
// Sets the wake coordinates from the freestream and the current
// vortex distribution. waklen is the session parameter (input).
// snew is the SETEXP distribution (Fortran SNEW(N+1..N+NW), 0-based
// 0..nw-1). Returns the wake-node state. The IWX limit
// (NW > IQX/8+2 = 48) is applied as in the Fortran and the routine
// continues.
Result<WakeState> xywake(const paneling::PanelGeometry& panel,
                         std::span<const double> gam,
                         std::span<const double> gamA,
                         std::span<const double> gamu1,
                         std::span<const double> gamu2, double alfa,
                         double waklen, double qinf);

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/Influence.h

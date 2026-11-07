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

// File: include/xfoil/core/geometry_design/CamberThickness.h
// Thickness/camber distribution of the new 0-based cores,
// directly ported from fortran-src/xtcam.f.
//
// Contained routines:
// GETCAM -> getCamberThickness GETMAX -> getMaxPoint
//
// All routines are 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).
// Formulas, operation order, tolerances (1.0E-5, 1.0E-6), and the
// iteration limit (Newton 10) are adopted exactly from the Fortran.
//
// GETCAM writes the local REAL YOF in the Fortran (no COMMON variable in
// GETCAM); re-origination of the camber at Y(1)=0 is part of the
// return value, with no side effect on COMMON /CR14/ XOF,YOF.
//
// GETMAX: Fortran SEGSPL(Y,YP,X,N) computes YP (spline coefficients of
// Y against X); this behavior is preserved as an in/out workspace.
//
// C++ safety deviation GETMAX (documented; the harness case avoids the edge case):
// - Fortran DDX is local and is initialized only if a point
// satisfies |Y(I)| > |YMAX0|; otherwise it stays uninitialized.
// The core initializes ddx = 0.0.
// - Fortran reads element X(N+1) for the maximum at I=N (outside the
// active length, the COMMON array is dimensioned larger). The core
// clamps the source index to n-1.
//
// No raw-pointer API, no global states, no dependency on
// app, CLI, or tests.

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::geometry_design {

// Fortran GETCAM initial group: sorted thickness/camber distribution.
// ncm/ntk are the active lengths after SORTOL (<= n+1); the vectors
// contain exactly ncm or ntk entries.
struct ThicknessCamberDistribution {
  std::vector<double> xcm;
  std::vector<double> ycm;
  std::vector<double> xtk;
  std::vector<double> ytk;
  std::size_t ncm{0};
  std::size_t ntk{0};
  // SOPPS failures (Fortran WRITE 'SOPPS: Opposite-point location
  // failed. Continuing...' per failure); the kernel only counts them.
  std::size_t oppositePointFailures{0};
};

// Fortran GETMAX output pair (XMAX, YMAX).
struct MaxPoint {
  double x{0.0};
  double y{0.0};
};

// Result of GETMAX including structured completion status. The
// canonical Fortran diagnostic 'GETMAX: Newton iteration for max
// camber/thickness failed.' (fallback to YMAX0/XMAX0) is returned as
// Diagnostic::limitReached; the numerical fallback
// behavior stays unchanged.
struct MaxPointResult {
  MaxPoint max;
  math::Diagnostic status{math::Diagnostic::completed};
};

// Safety contract: all routines check their span lengths before
// use. Invalid sizes throw std::invalid_argument, without partially
// changing state.

// Fortran: SUBROUTINE GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,X,XP,Y,YP,S,N)
// determines the thickness and camber distribution of the splined contour
// (XLFIND, SOPPS per point, LE point as N+1, SORTOL with
// TOL = 1.0E-5*(S(N)-S(1)), re-origination of the camber at
// YCM(1)=0). N >= 3 (XLFIND contract).
ThicknessCamberDistribution getCamberThickness(std::span<const double> x,
                                               std::span<const double> xp,
                                               std::span<const double> y,
                                               std::span<const double> yp,
                                               std::span<const double> s);

// Fortran: SUBROUTINE GETMAX(X,Y,YP,N,XMAX,YMAX)
// Maximum of the distribution y over x (approximate maximum plus
// Newton loop, max. 10 iterations, RESP clamping
// SIGN(MIN(0.5*DDX,|DX|),DX), XTOL = 1.0E-5*XLEN). SEGSPL computes
// yp (in/out workspace, canonical Fortran side-effect behavior).
// N >= 2; x/y at least n, yp exactly n long.
MaxPointResult getMaxPoint(std::span<const double> x,
                           std::span<const double> y,
                           std::span<double> yp);

// Fortran: SUBROUTINE CPCAM(N,X,Y,DYDX,P,DPDX)
// Camber line y(x) from a given DCp distribution P(x) (Cauchy
// integral with removed singularity, trapezoidal integration from the LE,
// analytically integrated singular parts YSING, offset/AAnk
// correction y(0)=y(1)=0). QOPI = 7.9577471545948E-02. N >= 2;
// x/p/dpdx equal length. Duplicate x values lead, as in Fortran, to
// division by zero (no additional kernel check).
struct CamberLoadingResult {
  std::vector<double> y;
  std::vector<double> dydx;
};

CamberLoadingResult camberFromLoading(std::span<const double> x,
                                      std::span<const double> p,
                                      std::span<const double> dpdx);

}  // namespace xfoil::core::geometry_design

// End: include/xfoil/core/geometry_design/CamberThickness.h

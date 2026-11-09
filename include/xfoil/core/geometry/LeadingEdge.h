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

// File: include/xfoil/core/geometry/LeadingEdge.h
// LE pipeline of the new 0-based cores, directly ported
// from fortran-src/xgeom.f.
//
// Contained routines:
// LEFIND -> lefind XLFIND -> xlfind NSFIND -> nsfind
// SOPPS -> sopps
//
// All routines are 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).
// Guess-loop bounds, Newton iterations, clamps, and convergence tolerances
// are adopted exactly from the Fortran. No raw-pointer API.
//
// Spline dependencies: lefind/xlfind/nsfind/sopps use
// exclusively the spline functions (seval, deval, d2val, curv,
// triSol). The discrete LE coordinate and the computed spline LE position
// remain separate quantities.

#pragma once

#include <cstddef>
#include <span>

#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::geometry {

// Fortran LEFIND/XLFIND/NSFIND completion status. The canonical
// Fortran diagnostics (Fortran WRITE) are returned structured;
// the numerical kernel does not write to stdout.
enum class LeStatus {
  // Regularly completed (Newton converged or parabolic method).
  completed,
  // Duplicated point at the LE (Fortran S(I)==S(I-1)): immediate return.
  sharpLE,
  // Newton iteration limit reached without convergence (Fortran
  // 'LEFIND: LE point not found. Continuing...' or 'XLFIND: Left
  // point not found. Continuing...'); SLE is reset to the estimate
  // (canonical Fortran behavior).
  limitReached,
};

// Result of LEFIND/XLFIND/NSFIND: spline LE position plus status.
struct LeadingEdgeResult {
  double sle{0.0};
  LeStatus status{LeStatus::completed};
};

// Result of SOPPS: arc length of the opposite point plus
// status (Fortran 'SOPPS: Opposite-point location failed. Continuing...'
// is returned as Diagnostic::limitReached).
struct OppositePointResult {
  double sopp{0.0};
  math::Diagnostic status{math::Diagnostic::completed};
};

// Safety contract: all routines check their span lengths before
// use. Invalid sizes throw std::invalid_argument.
//
// Minimum point count: Fortran LEFIND/XLFIND read S(3)/S(2), i.e. for
// zero passes of the guess loop (the Fortran DO variable stays at its
// initial value 3); for N < 3 Fortran would read out of range. The core
// therefore requires N >= 3 (controlled error).

// Fortran: SUBROUTINE LEFIND(SLE,X,XP,Y,YP,S,N)
// Locates the spline LE position. Defining condition: the tangent at the
// LE point is perpendicular to the line connecting it to the TE point.
// 6.996 first-guess bracket: previous and forward chord dot products
// (DOTM > 0 and DOTP <= 0); N < 5 uses the discrete midpoint index.
// N >= 3; x/xp/y/yp must be at least n long.
LeadingEdgeResult lefind(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s);

// Fortran: SUBROUTINE XLFIND(SLE,X,XP,Y,YP,S,N)
// Locates the leftmost (minimum x) point; X' = 0. N >= 3.
LeadingEdgeResult xlfind(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s);

// Fortran: SUBROUTINE NSFIND(SLE,X,XP,Y,YP,S,N)
// Locates the "nose" (maximum curvature) via TRISOL-smoothed
// curvatures and parabolic fitting. Fortran NMAX=500 (N > 500 throws
// std::invalid_argument like the Fortran STOP). IVMAX==0 (all zeros)
// also throws a controlled error (Fortran would read S(0)
// outside the range). N >= 2.
LeadingEdgeResult nsfind(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s);

// Fortran: SUBROUTINE SOPPS(SOPP,SI,X,XP,Y,YP,S,N,SLE)
// Arc length of the point on the other side of the contour opposite
// point SI (same chord-line x value XBAR).
// Newton iteration (max. 12), restoration of the first-guess position
// on failure. N >= 2.
OppositePointResult sopps(double si, std::span<const double> x,
                          std::span<const double> xp,
                          std::span<const double> y,
                          std::span<const double> yp,
                          std::span<const double> s, double sle);

}  // namespace xfoil::core::geometry

// End: include/xfoil/core/geometry/LeadingEdge.h

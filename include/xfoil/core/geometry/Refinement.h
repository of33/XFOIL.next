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

// File: include/xfoil/core/geometry/Refinement.h
// Interpolation and refinement of the new 0-based cores,
// directly ported from fortran-src/xgeom.f.
//
// Contained routines:
// INTER -> inter INTERX -> interX IJSECT -> ijsect
// AREFINE -> arefine SCHECK -> scheck HALF -> half
//
// All routines are 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).
// Interpolation formulas, segment selection, intersection semantics, point and
// capping systems are adopted exactly from the Fortran. The dialog and
// file orchestration (INTE/INTX dialog) follows later. No raw-
// pointer API.
//
// Fortran stdout output (AREFINE progress, SCHECK distances,
// SCHECK STOL warning) is returned structured; the numerical
// kernel does not write to stdout.

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::geometry {

// Fortran IJSECT output group. XEXINT/YEXINT are never set in the
// canonical Fortran (only commented assignments) and PEX is unused;
// both are not part of the C++ API.
struct SectionBending {
  double area{0.0};
  double slen{0.0};
  double xmin{0.0};
  double xmax{0.0};
  double ymin{0.0};
  double ymax{0.0};
  double xc{0.0};
  double yc{0.0};
  double xct{0.0};
  double yct{0.0};
  double aixx{0.0};
  double aiyy{0.0};
  double aixxt{0.0};
  double aiyyt{0.0};
  double aj{0.0};
  double ajt{0.0};
};

// Fortran AREFINE initial group: new point count, new contour, and status.
// The Fortran overflow path ('SDOUBLE: Arrays will overflow. No action
// taken.') is returned as nnew == 0 with an empty contour and
// Diagnostic::limitReached (no partial mutation).
struct RefineResult {
  std::size_t nnew{0};
  std::vector<double> x;
  std::vector<double> y;
  math::Diagnostic status{math::Diagnostic::completed};
};

// Fortran SCHECK initial group. The Fortran messages ('SCHECK segment
// removed at I', 'SCHECK: Bad value for small panels (STOL > 0.3)')
// are returned structured (removedIndices, badStol); the
// numerical kernel does not write to stdout.
struct ScheckResult {
  bool changed{false};
  bool badStol{false};
  std::vector<std::size_t> removedIndices;  // 0-based node indices
  math::Diagnostic status{math::Diagnostic::completed};
};

// Safety contract: all routines check their span lengths before a
// mutation (no partial mutation for a controlled error).

// Fortran: SUBROUTINE INTER(X0,XP0,Y0,YP0,S0,N0,SLE0,X1,XP1,Y1,YP1,S1,N1,
// SLE1,X,Y,N,FRAC)
// Interpolates two airfoils into an intermediate form (weighted averaging
// of the same normalized spline parameters; N = n0). p0/p1 must
// remain consistent (x/y/s/xp/yp equal length, N >= 2).
AirfoilCoordinates inter(const SplineProfile& p0, const SplineProfile& p1,
                         double frac);

// Fortran: SUBROUTINE INTERX(...) — interpolation via the normalized
// x parameter with SINVRT inversion (sinvrt). N = n0.
AirfoilCoordinates interX(const SplineProfile& p0, const SplineProfile& p1,
                          double frac);

// Fortran: SUBROUTINE IJSECT(N,X,Y,PEX,AREA,SLEN,...)
// Bending parameters of the cross-section. AREA == 0 returns early
// with zero values (Fortran undefined; controlled behavior
// documented). N >= 1.
SectionBending ijsect(std::span<const double> x, std::span<const double> y);

// Fortran: SUBROUTINE AREFINE(X,Y,S,XS,YS,N,ATOL,NDIM,NNEW,XNEW,YNEW,X1,X2)
// Adds points where the angle between adjacent segments exceeds atol
// (degrees); only within the window [x1, x2]. ndim is
// the capacity of the result contour. N >= 2; x/y/s/xs/ys must
// be at least n long.
RefineResult arefine(std::span<const double> x, std::span<const double> y,
                     std::span<const double> s, std::span<const double> xs,
                     std::span<const double> ys, double atol,
                     std::size_t ndim, double x1, double x2);

// Fortran: SUBROUTINE SCHECK(X,Y,N,STOL,LCHANGE)
// Removes micro-segments (in-place; n in/out). stol > 0.3 returns
// badStol == true without change (structured Fortran warning).
// x/y must be at least n long (capacity).
ScheckResult scheck(std::span<double> x, std::span<double> y,
                    std::size_t& n, double stol);

// Fortran: SUBROUTINE HALF(X,Y,S,N)
// Halves the point count (in-place; corners are preserved). s is
// read-only (respline by the caller). n in/out.
void half(std::span<const double> s, std::span<double> x,
          std::span<double> y, std::size_t& n);

}  // namespace xfoil::core::geometry

// End: include/xfoil/core/geometry/Refinement.h

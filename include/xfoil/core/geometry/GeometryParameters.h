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

// File: include/xfoil/core/geometry/GeometryParameters.h
// Geometry parameters of the new 0-based cores, directly
// ported from fortran-src/xgeom.f.
//
// Contained routines:
// NORM -> norm GEOPAR -> geopar
// AECALC -> aeCalc TCCALC -> tccalc
//
// All routines are 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).
// Formulas, operation order, tolerances, and special cases
// (AINT==0, EI11/EI22==0, rotation-invariant cross-sections) are adopted
// exactly from the Fortran. No EQUIVALENCE overlay, no
// unused GETCAM arrays. No raw-pointer API.
//
// Fortran diagnostics (GEOPAR WRITE of the 1000 formats) are
// represented as structured fields (thick/thickX/cambr/cambrX); the
// numerical kernel does not write to stdout.

#pragma once

#include <cstddef>
#include <span>

#include "xfoil/core/geometry/LeadingEdge.h"

namespace xfoil::core::geometry {

// Fortran AECALC output group (area, centroid, principal moments
// of inertia, principal axis angles).
struct SectionProperties {
  double area{0.0};
  double xcen{0.0};
  double ycen{0.0};
  double ei11{0.0};
  double ei22{0.0};
  double apx1{0.0};
  double apx2{0.0};
};

// Fortran TCCALC initial group (discrete maxima of thickness and
// camber with their x positions). oppositePointFailures counts the SOPPS
// failures (Fortran WRITE 'SOPPS: Opposite-point location failed.
// Continuing...' per failure); the kernel itself performs no writes.
struct ThicknessCamberResult {
  double thick{0.0};
  double thickX{0.0};
  double cambr{0.0};
  double cambrX{0.0};
  std::size_t oppositePointFailures{0};
  // Counts failed LEFIND calls (Fortran
  // 'LEFIND: LE point not found. Continuing...' per call; the kernel
  // itself performs no writes).
  std::size_t lefindFailures{0};
};

// Fortran GEOPAR initial group: SLE, CHORD, AREA, RADLE, ANGTE,
// EI11A..APX2A, EI11T..APX2T, THICK/CAMBR including the structured
// 1000-format diagnostics (thick/thickX/cambr/cambrX).
struct GeometryMetrics {
  double sle{0.0};
  double chord{0.0};
  double area{0.0};
  double radle{0.0};
  double angte{0.0};
  double ei11a{0.0};
  double ei22a{0.0};
  double apx1a{0.0};
  double apx2a{0.0};
  double ei11t{0.0};
  double ei22t{0.0};
  double apx1t{0.0};
  double apx2t{0.0};
  double thick{0.0};
  double thickX{0.0};   // Fortran XTHICK (structured instead of WRITE)
  double cambr{0.0};
  double cambrX{0.0};   // Fortran XCAMBR (structured instead of WRITE)
  // SOPPS failures of the TCCALC phase of this GEOPAR call (Fortran
  // WRITE('SOPPS: Opposite-point location failed. Continuing...') per
  // failure before the 1000-format output); the kernel itself performs no writes.
  std::size_t oppositePointFailures{0};
  // Counts failed LEFIND calls of this GEOPAR call
  // (its own LEFIND plus TCCALC LEFIND; Fortran writes per failure
  // 'LEFIND: LE point not found. Continuing...').
  std::size_t lefindFailures{0};
};

// Safety contract: all routines check their span lengths before
// use. Invalid sizes throw std::invalid_argument.

// Fortran: SUBROUTINE NORM(X,XP,Y,YP,S,N)
// Scales the contour to unit chord (in-place): SCALC/SEGSPL, LEFIND,
// then shift to the LE and scaling by the LE-TE chord-vector length.
// XP/YP remain as set by SEGSPL before the scaling (canonical Fortran
// behavior). N >= 2.
void norm(std::span<double> x, std::span<double> xp, std::span<double> y,
          std::span<double> yp, std::span<double> s);

// Fortran: SUBROUTINE GEOPAR(X,XP,Y,YP,S,N,T,...)
// Sets the geometric parameters of the contour. T (skin thickness) is
// internally set to 1.0 as in Fortran (workspace, not part of the API).
// The unused Fortran GETCAM arrays (IBX=600) are not
// ported. N >= 2.
GeometryMetrics geopar(std::span<const double> x,
                       std::span<const double> xp,
                       std::span<const double> y,
                       std::span<const double> yp,
                       std::span<const double> s);

// Fortran: SUBROUTINE AECALC(N,X,Y,T,ITYPE,AREA,XCEN,YCEN,EI11,EI22,APX1,APX2)
// Geometric properties of the contour. ITYPE=1: area (dx dy),
// ITYPE=2: skin area (t ds). Closing segment as in Fortran
// (IO==N -> IP=1). itype != 1 && itype != 2 throws a controlled
// error. N >= 1; t must be at least n long.
SectionProperties aeCalc(std::span<const double> x,
                         std::span<const double> y,
                         std::span<const double> t, int itype);

// Fortran: SUBROUTINE TCCALC(X,XP,Y,YP,S,N,THICK,XTHICK,CAMBR,XCAMBR)
// Maxima of thickness and camber on the discrete points (uses
// LEFIND, SOPPS, and SEVAL). N >= 2.
ThicknessCamberResult tccalc(std::span<const double> x,
                             std::span<const double> xp,
                             std::span<const double> y,
                             std::span<const double> yp,
                             std::span<const double> s);

}  // namespace xfoil::core::geometry

// End: include/xfoil/core/geometry/GeometryParameters.h

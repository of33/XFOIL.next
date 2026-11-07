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

// File: include/xfoil/core/geometry/Transformations.h
// Transformations and diagnostic groups of the new 0-based cores,
// directly ported from fortran-src/xgeom.f and
// fortran-src/xfoil.f.
//
// Contained routines:
// YSYM -> ysym LERSCL -> lerscl SSS -> sss
// CLIS -> clis CANG -> cang ROTATE -> rotate
// TECALC -> tecalc
//
// All routines are 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).
// Formulas, operation order, clamps, iteration limits, and
// special cases are adopted exactly from the Fortran. No raw pointers.
//
// Fortran stdout output (CLIS curve-mean table, CANG angles,
// SSS non-convergence) is returned structured; the numerical
// kernel does not write to stdout.

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::geometry {

// Fortran SSS initial group: arc lengths of the area discontinuity points
// (S1 at the fixed profile part, S2 at the flap) plus status. The Fortran
// non-convergence ('SSS: failed to converge subtending angle points') is
// returned as Diagnostic::limitReached.
struct BreakPointResult {
  double s1{0.0};
  double s2{0.0};
  math::Diagnostic status{math::Diagnostic::completed};
};

// Fortran CLIS output line (i, x, y, s, theta, curv). The row index
// stays 0-based (Fortran I is 1-based and display-format).
struct CurvatureRow {
  double x{0.0};
  double y{0.0};
  double s{0.0};
  double angleDeg{0.0};
  double curvature{0.0};
};

// Fortran CLIS initial group: curve-average table plus maximum value.
struct CurvatureReport {
  std::vector<CurvatureRow> rows;
  double maxCurvature{0.0};
  std::size_t maxIndex{0};  // 0-based index (Fortran IMAX 1-based)
};

// Fortran CANG initial group: panel node corner angle plus maximum value.
// The Fortran IPrint display (IPRINT=0/1/2) controls only the display and
// is not part of the core API.
struct CornerAngleReport {
  std::vector<double> angles;  // Fortran I=2..N-1 -> 0..n-2
  double maxAngle{0.0};
  std::size_t maxIndex{0};  // 0-based index (Fortran IMAX 1-based)
};

// Fortran TECALC inputs (scalar final panel values). In Fortran, TECALC
// reads the COMMON states X/Y/XP/YP/CHORD (CR05), GAM/GAM_A (CR06),
// and N (CI04); here the scalar final values are passed explicitly.
struct TrailingEdgeInputs {
  double x0{0.0};     // Fortran X(1)
  double y0{0.0};     // Fortran Y(1)
  double xp0{0.0};    // Fortran XP(1)
  double yp0{0.0};    // Fortran YP(1)
  double xn{0.0};     // Fortran X(N)
  double yn{0.0};     // Fortran Y(N)
  double xpn{0.0};    // Fortran XP(N)
  double ypn{0.0};    // Fortran YP(N)
  double gam0{0.0};   // Fortran GAM(1)
  double gamn{0.0};   // Fortran GAM(N)
  double gama0{0.0};  // Fortran GAM_A(1)
  double gaman{0.0};  // Fortran GAM_A(N)
  double chord{0.0};  // Fortran CHORD
};

// Fortran TECALC output group (the COMMON write state CR06/Cl01 is
// returned as a structured result; ownership of the panel/session
// state follows later).
struct TecalcResult {
  bool sharp{false};
  double ante{0.0};
  double aste{0.0};
  double dste{0.0};
  double scs{0.0};
  double sds{0.0};
  double sigte{0.0};
  double gamte{0.0};
  double sigteA{0.0};
  double gamteA{0.0};
};

// Safety contract: all routines check their span lengths before a
// mutation (no partial mutation for a controlled error).

// Fortran: SUBROUTINE YSYM(X,XP,Y,YP,S,NX,N,ISIDE,XNEW,YNEW)
// Makes the contour symmetric about the chord line (in-place incl. N and
// re-spline). Fortran STOP 'YSYM: Array overflow on passed arrays.'
// (NNEW > NX) is mapped to std::invalid_argument before each mutation.
// The spans x/xp/y/yp/s have capacity NX, n is the
// active point count (in/out), and xnew/ynew are work arrays (>= NX).
// iside == 1 selects the upper side as symmetry source, any other
// value the lower side (as in Fortran).
Status ysym(std::span<double> x, std::span<double> xp, std::span<double> y,
            std::span<double> yp, std::span<double> s, std::size_t& n,
            int iside, std::span<double> xnew, std::span<double> ynew);

// Fortran: SUBROUTINE LERSCL(X,XP,Y,YP,S,N,DOC,RFAC,XNEW,YNEW)
// Scales the LE radius by factor rfac with decay length doc. Result
// as new contour (XNEW/YNEW), inputs unchanged. N >= 2.
AirfoilCoordinates lerscl(std::span<const double> x,
                          std::span<const double> xp,
                          std::span<const double> y,
                          std::span<const double> yp,
                          std::span<const double> s, double doc,
                          double rfac);

// Fortran: SUBROUTINE SSS(SS,S1,S2,DEL,XBF,YBF,X,XP,Y,YP,S,N,ISIDE)
// arc lengths of the area break points. DEL > 0: the area in
// S1 < s < S2 drops; DEL == 0: S1 == S2 (perpendicular to the
// flap hinge). iside == 1 selects SSGN = -1.0, any other value
// +1.0 (as in Fortran). N >= 2.
BreakPointResult sss(double ss, double xbf, double ybf, double del,
                     int iside, std::span<const double> x,
                     std::span<const double> xp, std::span<const double> y,
                     std::span<const double> yp, std::span<const double> s);

// Fortran: SUBROUTINE CLIS(X,XP,Y,YP,S,N)
// Curve-average table on the points (without stdout output). N >= 2.
CurvatureReport clis(std::span<const double> x,
                     std::span<const double> xp,
                     std::span<const double> y,
                     std::span<const double> yp,
                     std::span<const double> s);

// Fortran: SUBROUTINE CANG(X,Y,N,IPRINT,IMAX,AMAX)
// Panel node corner angle (without stdout output). The Fortran display
// (IPRINT) controls only the display. N >= 3 (Fortran I=2..N-1).
CornerAngleReport cang(std::span<const double> x, std::span<const double> y);

// Fortran: SUBROUTINE ROTATE(X,Y,N,ALFA)
// Rotates the contour by alfa (in-place, XOFF=YOFF=0.0 as in Fortran;
// the commented 0.25 offsets are not adopted).
// x.size == y.size.
void rotate(std::span<double> x, std::span<double> y, double alfa);

// Fortran: SUBROUTINE TECALC (only COMMON)
// Computes total and projected TE-gap areas and TE panel
// strengths from the scalar panel end values.
TecalcResult tecalc(const TrailingEdgeInputs& in);

}  // namespace xfoil::core::geometry

// End: include/xfoil/core/geometry/Transformations.h

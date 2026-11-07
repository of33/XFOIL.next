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

// File: include/xfoil/core/math/Spline.h
// Spline construction, spline evaluation and tridiagonal solver of the new
// 0-based cores, ported directly from fortran-src/spline.f.
//
// Contained routines:
// SPLINE -> spline SPLIND -> splind SPLINA -> splina
// TRISOL -> triSol SEVAL -> seval DEVAL -> deval
// D2VAL -> d2val CURV -> curv CURVS -> curvs
// SINVRT -> sinvrt SCALC -> scalc SPLNXY -> splnxy
// SEGSPL -> segspl SEGSPLD-> segsplD
//
// All routines are 0-based. Interval selection, boundary conditions,
// segment detection and evaluation formulas are adopted exactly from
// Fortran. No hidden 1-based convention, no artificial
// sentinel slots. No raw-pointer API.
//
// Note: The local Fortran work arrays (NMAX=1000 for SPLINE/SPLIND,
// KMAX=32 for SPLNXY) are held in the core as std::array of the same
// size, so that the valid Fortran path (N <= NMAX) stays
// unchanged.

#pragma once

#include <cstddef>
#include <span>

#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::math {

// Safety contract: all routines check their span lengths and the
// canonical minimum point count before mutation. Invalid sizes
// throw std::invalid_argument, without any mutation occurring. No
// state is partially changed.

// Fortran: SUBROUTINE SPLINE(X,XS,S,N)
// Computes the spline coefficients XS for X(S) with zero
// second-derivative boundary conditions. Solves the tridiagonal system via
// triSol. N = x.size (>= 2, <= 1000 as Fortran NMAX).
void spline(std::span<const double> x, std::span<const double> s,
            std::span<double> xs);

// Fortran: SUBROUTINE SPLIND(X,XS,S,N,XS1,XS2)
// Spline coefficients with specified first derivatives or
// zero-second/zero-third-derivative boundary conditions. XS1, XS2 == 999.0
// selects zero-second, == -999.0 zero-third, otherwise the specified value.
// N >= 2, N <= 1000.
void splind(std::span<const double> x, std::span<const double> s,
            std::span<double> xs, double xs1, double xs2);

// Fortran: SUBROUTINE SPLINA(X,XS,S,N)
// Non-oscillating spline by averaging of adjacent
// segment slopes; boundary conditions from the end-segment slope.
// N >= 2.
void splina(std::span<const double> x, std::span<const double> s,
            std::span<double> xs);

// Fortran: SUBROUTINE TRISOL(A,B,C,D,KK)
// Solves a tridiagonal system of length KK. A and C are destroyed; D is
// replaced by the solution. B is only read. KK >= 1; KK == 0 is
// rejected as an invalid size.
void triSol(std::span<double> a, std::span<const double> b,
            std::span<double> c, std::span<double> d);

// Fortran: FUNCTION SEVAL(SS,X,XS,S,N)
// Value X(SS) of the spline curve built by spline/splind. N >= 2;
// x, xs, s must have consistent lengths (s.size >= 2,
// x.size >= s.size, xs.size >= s.size).
double seval(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> s);

// Fortran: FUNCTION DEVAL(SS,X,XS,S,N)
// First derivative dX/dS(SS). N >= 2; lengths as for seval.
double deval(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> s);

// Fortran: FUNCTION D2VAL(SS,X,XS,S,N)
// Second derivative d2X/dS2(SS). N >= 2; lengths as for seval.
double d2val(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> s);

// Fortran: FUNCTION CURV(SS,X,XS,Y,YS,S,N)
// curvature of the splined 2-D curve at abscissa S=SS. N >= 2;
// all spans (x, xs, y, ys) must be at least s.size long.
double curv(double ss, std::span<const double> x,
            std::span<const double> xs, std::span<const double> y,
            std::span<const double> ys, std::span<const double> s);

// Fortran: FUNCTION CURVS(SS,X,XS,Y,YS,S,N)
// curvature derivative of the splined 2-D curve at the value S=SS.
// N >= 2; all spans (x, xs, y, ys) must be at least s.size long.
double curvs(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> y,
             std::span<const double> ys, std::span<const double> s);

// Fortran: SUBROUTINE SINVRT(SI,XI,X,XS,S,N)
// "Inverse" spline function S(XI) via Newton iteration (max. 10
// iterations). si is input (initial value) and output (result). On
// non-convergence the input value is restored.
// Return value: Diagnostic::limitReached if the Newton iteration did
// not converge within 10 iterations (Fortran 'SINVRT: spline
// inversion failed'); in this case si is reset to the input
// value.
Diagnostic sinvrt(double& si, double xi, std::span<const double> x,
                  std::span<const double> xs, std::span<const double> s);

// Fortran: SUBROUTINE SCALC(X,Y,S,N)
// Arc-length array S for the 2-D points (X,Y); S(0)=0. N >= 1;
// empty input (N == 0) is rejected.
void scalc(std::span<const double> x, std::span<const double> y,
           std::span<double> s);

// Fortran: SUBROUTINE SPLNXY(X,XS,Y,YS,S,N)
// Splines the 2-D form X(S), Y(S) with preserved arc length S (several
// Simpson re-integration passes until SERR < 1.0E-7 or NPASS=10 is reached).
// N >= 2. Return value: Diagnostic::limitReached if the tolerance
// is not met after 10 passes; the numerical result stays unchanged.
Diagnostic splnxy(std::span<const double> x, std::span<double> xs,
                  std::span<const double> y, std::span<double> ys,
                  std::span<double> s);

// Fortran: SUBROUTINE SEGSPL(X,XS,S,N)
// Segmented spline like spline, but derivative jumps are allowed at
// segment junctions (defined by identical S values on consecutive
// points). Each segment uses zero third-derivative boundary conditions.
// N >= 2. Throws std::invalid_argument for a duplicated first/last
// point or invalid size.
void segspl(std::span<const double> x, std::span<const double> s,
            std::span<double> xs);

// Fortran: SUBROUTINE SEGSPLD(X,XS,S,N,XS1,XS2)
// segmented spline like segspl, with specified end derivatives
// XS1, XS2 per segment. N >= 2.
void segsplD(std::span<const double> x, std::span<const double> s,
             std::span<double> xs, double xs1, double xs2);

}  // namespace xfoil::core::math

// End: include/xfoil/core/math/Spline.h

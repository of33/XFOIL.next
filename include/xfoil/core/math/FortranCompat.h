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

// File: include/xfoil/core/math/FortranCompat.h
// Scalar numerical helper functions of the new 0-based cores,
// directly ported from fortran-src/xutils.f.
//
// Contained routines:
// SETEXP -> setExp (geometrically stretched array)
// ATANC -> atanC (ATAN2 with branch check)
//
// The port runs directly from Fortran (0-based).
// No raw-pointer API.

#pragma once

#include <cstddef>
#include <span>

#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::math {

// Fortran: SUBROUTINE SETEXP(S,DS1,SMAX,NN)
// Fixes the geometrically stretched array S(0..NN-1) with
// S(i+1)-S(i) = ratio * (S(i)-S(i-1)).
// s (output) array, filled (0-based, length NN)
// ds1 (input) first S step: S(1)-S(0)
// smax (input) last S value: S(NN-1)
// NN = s.size
// Error (NN<=2, i.e. NEX<=1): throws std::invalid_argument.
// Return value: Diagnostic::limitReached if the Newton iteration did not
// converge within 100 iterations (Fortran 'SETEXP: Convergence
// failed. Continuing anyway...'); the numerical result stays
// unchanged (it continues with the last ratio).
Diagnostic setExp(std::span<double> s, double ds1, double smax);

// Fortran: FUNCTION ATANC(Y,X,THOLD)
// ATAN2(Y,X) with branch-cut check: returns the position angle of
// (X,Y) starting from the near angle THOLD, without the ATAN2 branch cut
// (in the -x direction) causing a jump of about +/-2*pi.
double atanC(double y, double x, double thold);

}  // namespace xfoil::core::math

// End: include/xfoil/core/math/FortranCompat.h

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

// File: include/xfoil/core/math/LinearAlgebra.h
// Real and complex linear system solvers of the new 0-based
// cores, ported directly from fortran-src/xsolve.f.
//
// Contained routines:
// GAUSS -> Gauss
// CGAUSS -> cgauss
// LUDCMP -> ludcmp
// BAKSUB -> baksub
//
// All solvers work on flat, 0-based, row-wise (row-major)
// Spans. The Fortran matrices are column-major; the
// translation to row-major does not change the formula or operation order.
// Pivot indices are stored 0-based in the core; the conversion
// happens at the documented boundary (see implementation).
//
// Safety checks before mutation:
// - Gauss/cgauss: nn >= 1 (nn==0 is rejected), matrix square
// (nn*nn <= z.size), RHS matching (nn*nrhs <= r.size),
// - ludcmp: n >= 1 (n==0 is rejected), n*n <= a.size,
// indx.size >= n,
// - baksub: n*n <= a.size, indx.size >= n, b.size >= n; all
// pivot indices are checked on [0, n) before mutation.
// Invalid sizes/indices throw std::invalid_argument; nothing is
// mutated. The canonical singularity/error behavior stays
// unchanged.
//
// No raw-pointer API. BLSOLV belongs to the viscous solver and is
// not contained here.

#pragma once

#include <complex>
#include <cstddef>
#include <span>

namespace xfoil::core::math {

// Fortran: SUBROUTINE GAUSS(NSIZ,NN,Z,R,NRHS)
// Solves a general NN x NN system with NRHS right-hand sides.
// z (flat, row-major, nn*nn) is destroyed; r (flat, nn*nrhs) is
// replaced by the solution. Partial pivoting with row interchanges.
void gauss(std::span<double> z, std::span<double> r, std::size_t nn,
           std::size_t nrhs);

// Fortran: SUBROUTINE CGAUSS(NSIZ,NN,Z,R,NRHS)
// As Gauss, but for complex systems. ABS(Z) = magnitude (modulus).
void cgauss(std::span<std::complex<double>> z,
            std::span<std::complex<double>> r, std::size_t nn,
            std::size_t nrhs);

// Fortran: SUBROUTINE LUDCMP(NSIZ,N,A,INDX)
// Splits an N x N matrix into LU factors (in-place, flat row-major,
// n*n) with partial pivoting and implicit scaling. indx (n) holds
// the 0-based pivot indices. n <= 500 (Fortran NVX).
void ludcmp(std::span<double> a, std::size_t n, std::span<int> indx);

// Fortran: SUBROUTINE BAKSUB(NSIZ,N,A,INDX,B)
// Back substitution after ludcmp. a (n*n, read-only) holds the
// LU factors, indx the 0-based pivot indices; b (n) is replaced by the
// solution.
void baksub(std::span<const double> a, std::size_t n,
            std::span<const int> indx, std::span<double> b);

}  // namespace xfoil::core::math

// End: include/xfoil/core/math/LinearAlgebra.h

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

// File: include/xfoil/math/LinearAlgebra.h
// Stateless linear-algebra module implementing the GAUSS, LUDCMP,
// BAKSUB and CGAUSS routines of fortran-src/xsolve.f and the TRISOL
// routine of fortran-src/spline.f. Algorithms, pivot choice, and
// operation orders of the Fortran routines are adopted unchanged.
//
// Dimensioning contract (1-based convention):
// - Active rows and columns are 1..equationCount; index 0 is
// reserved and is not used domain-specifically (and is not changed).
// - matrix.size >= equationCount + 1 (row 0 must be present as a
// reserved row).
// - each active row 0..equationCount must hold at least equationCount + 1
// elements.
// - Vectors must have at least equationCount + 1 elements.
// - The algorithms change the matrix and right-hand side in place.
// - On an error (status != success) the contents may already be
// partially changed. For transactional behavior the
// caller must copy beforehand.
// - Singularity is recognized exclusively as an exactly zero pivot;
// no epsilon, scaling, or near-zero tolerance is deliberately
// introduced, because that would classify real XFoil systems
// differently.
// - Non-finite inputs (NaN/Inf) are rejected before the computation starts with
// nonFiniteInput.
//
// The API contains no raw pointers, stores no spans beyond a
// call and allocates no heap memory.

#pragma once

#include <complex>
#include <span>

namespace xfoil::math::linear_algebra {

enum class SolveStatus {
  success,
  invalidDimension,
  singularMatrix,
  nonFiniteInput
};

// Row-span matrix representation: each row is its own span,
// so differently sized matrix rows (e.g. double[5][6] and
// double[IQX][IQX]) can be mapped without flattening.
using RealMatrixRows = std::span<std::span<double>>;
using ComplexMatrixRows =
    std::span<std::span<std::complex<double>>>;

// Implements GAUSS (fortran-src/xsolve.f:22): Gaussian elimination
// with pivoting on maximum magnitude (strict >), in-place
// solution of a system with exactly one right-hand side. matrix is
// destroyed, rightHandSide carries the solution.
[[nodiscard]]
SolveStatus solveGaussianInPlace(
    RealMatrixRows matrix,
    std::span<double> rightHandSide,
    int equationCount) noexcept;

// Implements LUDCMP (fortran-src/xsolve.f:174): LU factorization with scaled pivoting
// (comparison >=, so that on a tie the later row is chosen).
// matrix is replaced by its LU factors, pivotIndices[j] carries the
// pivot index of column j.
[[nodiscard]]
SolveStatus factorLuInPlace(
    RealMatrixRows matrix,
    std::span<int> pivotIndices,
    int equationCount) noexcept;

// Implements BAKSUB (fortran-src/xsolve.f:250): back substitution for a matrix previously
// factorized with factorLuInPlace. rightHandSide carries the solution.
[[nodiscard]]
SolveStatus backSubstituteLuInPlace(
    RealMatrixRows factoredMatrix,
    std::span<const int> pivotIndices,
    std::span<double> rightHandSide,
    int equationCount) noexcept;

// Implements CGAUSS (fortran-src/xsolve.f:103): Gaussian elimination for complex
// systems with exactly one right-hand side; pivoting on magnitude with the
// unchanged comparison abs(candidate) - abs(current) > 0.
[[nodiscard]]
SolveStatus solveComplexGaussianInPlace(
    ComplexMatrixRows matrix,
    std::span<std::complex<double>> rightHandSide,
    int equationCount) noexcept;

// Implements the TRISOL tridiagonal solve (fortran-src/spline.f:185):
// diagonal, upper and rightHandSide are changed,
// lower stays unchanged. New are exact null-pivot checks before each
// division and the finiteness check of the active inputs.
[[nodiscard]]
SolveStatus solveTridiagonalInPlace(
    std::span<double> diagonal,
    std::span<double> lower,
    std::span<double> upper,
    std::span<double> rightHandSide,
    int equationCount) noexcept;

}  // namespace xfoil::math::linear_algebra

// End: include/xfoil/math/LinearAlgebra.h

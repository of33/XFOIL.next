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

// File: src/core/math/LinearAlgebra.cpp
// Implementation of the stateless linear-algebra module.
// The algorithms follow the formulas and operation order of
// the GAUSS, LUDCMP, BAKSUB and CGAUSS routines in
// fortran-src/xsolve.f and of TRISOL in fortran-src/spline.f;
// 1-based indexing with reserved index 0 is preserved
// (see the dimensioning contract in LinearAlgebra.h).

#include "xfoil/math/LinearAlgebra.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <span>

#include "xfoil/Limits.h"

namespace xfoil::math::linear_algebra {
namespace {

// 1-based element access: the domain indices are restricted to
// 0..equationCount by the dimension check; the conversion
// to size_type is deliberate and lossless here.
template <class Container>
[[nodiscard]] auto& elementAt(Container& container, int index) noexcept {
  return container[static_cast<std::size_t>(index)];
}

[[nodiscard]] constexpr std::size_t requiredSize(int equationCount) noexcept {
  return static_cast<std::size_t>(equationCount) + 1;
}

// Active Indices are 1..equationCount; equationCount < 1 is invalid.
[[nodiscard]] constexpr bool hasValidEquationCount(int equationCount) noexcept {
  return equationCount >= 1;
}

// Each line 0..equationCount must own at least equationCount + 1
// elements. Line 0 is reserved but must still be allocated.
template <class Row>
[[nodiscard]] bool hasValidMatrixRows(std::span<Row> matrix,
                                      int equationCount) noexcept {
  const auto required = requiredSize(equationCount);
  if (matrix.size() < required) {
    return false;
  }
  for (std::size_t row = 0; row <= static_cast<std::size_t>(equationCount);
       ++row) {
    if (matrix[row].size() < required) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool hasValidRealDimensions(
    RealMatrixRows matrix,
    std::span<const double> vector,
    int equationCount) noexcept {
  if (!hasValidEquationCount(equationCount)) {
    return false;
  }
  if (vector.size() < requiredSize(equationCount)) {
    return false;
  }
  return hasValidMatrixRows(matrix, equationCount);
}

[[nodiscard]] bool hasValidComplexDimensions(
    ComplexMatrixRows matrix,
    std::span<const std::complex<double>> vector,
    int equationCount) noexcept {
  if (!hasValidEquationCount(equationCount)) {
    return false;
  }
  if (vector.size() < requiredSize(equationCount)) {
    return false;
  }
  return hasValidMatrixRows(matrix, equationCount);
}

// For the LU factorization the pivot span is checked instead of the
// right-hand side.
[[nodiscard]] bool hasValidLuDimensions(RealMatrixRows matrix,
                                        std::span<const int> pivotIndices,
                                        int equationCount) noexcept {
  if (!hasValidEquationCount(equationCount)) {
    return false;
  }
  if (pivotIndices.size() < requiredSize(equationCount)) {
    return false;
  }
  return hasValidMatrixRows(matrix, equationCount);
}

[[nodiscard]] bool isFinite(double value) noexcept {
  return std::isfinite(value);
}

[[nodiscard]] bool isFinite(const std::complex<double>& value) noexcept {
  return std::isfinite(value.real()) && std::isfinite(value.imag());
}

// Finiteness check of the active matrix blocks 1..equationCount in both
// directions. The reserved index 0 is deliberately not checked.
template <class Row>
[[nodiscard]] bool allActiveMatrixValuesFinite(
    std::span<Row> matrix,
    int equationCount) noexcept {
  for (int row = 1; row <= equationCount; ++row) {
    auto& matrixRow = matrix[static_cast<std::size_t>(row)];
    for (int column = 1; column <= equationCount; ++column) {
      if (!isFinite(elementAt(matrixRow, column))) {
        return false;
      }
    }
  }
  return true;
}

template <class T>
[[nodiscard]] bool allActiveVectorValuesFinite(std::span<T> vector,
                                               int equationCount) noexcept {
  for (int index = 1; index <= equationCount; ++index) {
    if (!isFinite(elementAt(vector, index))) {
      return false;
    }
  }
  return true;
}

}  // namespace

SolveStatus solveGaussianInPlace(RealMatrixRows matrix,
                                 std::span<double> rightHandSide,
                                 int equationCount) noexcept {
  if (!hasValidRealDimensions(matrix, rightHandSide, equationCount)) {
    return SolveStatus::invalidDimension;
  }
  if (!allActiveMatrixValuesFinite(matrix, equationCount) ||
      !allActiveVectorValuesFinite(rightHandSide, equationCount)) {
    return SolveStatus::nonFiniteInput;
  }

  // Adopted from GAUSS (fortran-src/xsolve.f:22).
  // Operation order unchanged.
  auto cell = [matrix](int row, int column) -> double& {
    return matrix[static_cast<std::size_t>(row)]
                  [static_cast<std::size_t>(column)];
  };

  for (int np = 1; np <= equationCount - 1; np++) {
    const int nnpp = np + 1;

    // --- find max pivot index nx (strict >, keeps the previous
    // --- selection on a tie)
    int nx = np;
    for (int nt = nnpp; nt <= equationCount; nt++) {
      if (std::fabs(cell(nt, np)) > std::fabs(cell(nx, np))) {
        nx = nt;
      }
    }

    // --- exact zero-pivot check (no near-zero tolerance)
    if (cell(nx, np) == 0.0) {
      return SolveStatus::singularMatrix;
    }
    const double pivot = 1.0 / cell(nx, np);

    // --- switch pivots
    cell(nx, np) = cell(np, np);

    // --- switch rows & normalize pivot row
    for (int loc = nnpp; loc <= equationCount; loc++) {
      const double temp = cell(nx, loc) * pivot;
      cell(nx, loc) = cell(np, loc);
      cell(np, loc) = temp;
    }

    const double rhsTemp = elementAt(rightHandSide, nx) * pivot;
    elementAt(rightHandSide, nx) = elementAt(rightHandSide, np);
    elementAt(rightHandSide, np) = rhsTemp;

    // --- forward eliminate everything
    for (int k = nnpp; k <= equationCount; k++) {
      const double ztmp = cell(k, np);
      for (int loc = nnpp; loc <= equationCount; loc++) {
        cell(k, loc) = cell(k, loc) - ztmp * cell(np, loc);
      }
      elementAt(rightHandSide, k) =
          elementAt(rightHandSide, k) - ztmp * elementAt(rightHandSide, np);
    }
  }

  // --- solve for last row (exact zero-pivot check before the division)
  if (cell(equationCount, equationCount) == 0.0) {
    return SolveStatus::singularMatrix;
  }
  elementAt(rightHandSide, equationCount) =
      elementAt(rightHandSide, equationCount) /
      cell(equationCount, equationCount);

  // --- back substitute everything
  for (int np = equationCount - 1; np >= 1; np--) {
    const int nnpp = np + 1;
    for (int k = nnpp; k <= equationCount; k++) {
      elementAt(rightHandSide, np) = elementAt(rightHandSide, np) -
                                     cell(np, k) * elementAt(rightHandSide, k);
    }
  }

  return SolveStatus::success;
}

SolveStatus factorLuInPlace(RealMatrixRows matrix,
                            std::span<int> pivotIndices,
                            int equationCount) noexcept {
  if (!hasValidLuDimensions(matrix, pivotIndices, equationCount)) {
    return SolveStatus::invalidDimension;
  }
  if (!allActiveMatrixValuesFinite(matrix, equationCount)) {
    return SolveStatus::nonFiniteInput;
  }

  // Fixed scaling workspace; active indices are 1..equationCount.
  // The last usable index is
  // surfaceNodeStorage - 1, so equationCount > surfaceNodeStorage - 1
  // is invalid (the real maximum equation count lies well
  // below it). No heap use in the solver hot paths.
  if (static_cast<std::size_t>(equationCount) >=
      xfoil::limits::surfaceNodeStorage) {
    return SolveStatus::invalidDimension;
  }
  std::array<double, xfoil::limits::surfaceNodeStorage> scaling{};

  // Adopted from LUDCMP (fortran-src/xsolve.f:174). Operation order unchanged;
  // pivot comparison with >= (selects the later row on a tie).
  auto cell = [matrix](int row, int column) -> double& {
    return matrix[static_cast<std::size_t>(row)]
                  [static_cast<std::size_t>(column)];
  };

  int imax = 0;
  double sum = 0.0;
  double aamax = 0.0;
  double dum = 0.0;

  for (int i = 1; i <= equationCount; i++) {
    aamax = 0.0;
    for (int j = 1; j <= equationCount; j++) {
      aamax = std::max(std::fabs(cell(i, j)), aamax);
    }
    // --- exact zero-row check before 1.0 / rowMaximum
    if (aamax == 0.0) {
      return SolveStatus::singularMatrix;
    }
    scaling[static_cast<std::size_t>(i)] = 1.0 / aamax;
  }

  for (int j = 1; j <= equationCount; j++) {
    for (int i = 1; i <= j - 1; i++) {
      sum = cell(i, j);
      for (int k = 1; k <= i - 1; k++) {
        sum = sum - cell(i, k) * cell(k, j);
      }
      cell(i, j) = sum;
    }

    aamax = 0.0;

    for (int i = j; i <= equationCount; i++) {
      sum = cell(i, j);
      for (int k = 1; k <= j - 1; k++) {
        sum = sum - cell(i, k) * cell(k, j);
      }
      cell(i, j) = sum;
      dum = (scaling[static_cast<std::size_t>(i)] * std::fabs(sum));
      if (dum >= aamax) {
        imax = i;
        aamax = dum;
      }
    }

    if (j != imax) {
      for (int k = 1; k <= equationCount; k++) {
        dum = cell(imax, k);
        cell(imax, k) = cell(j, k);
        cell(j, k) = dum;
      }
      scaling[static_cast<std::size_t>(imax)] =
          scaling[static_cast<std::size_t>(j)];
    }

    elementAt(pivotIndices, j) = imax;
      if (j != equationCount) {
      // Exact zero-pivot check before 1.0 / matrix[j][j].
      if (cell(j, j) == 0.0) {
        return SolveStatus::singularMatrix;
      }

      dum = 1.0 / cell(j, j);
      for (int i = j + 1; i <= equationCount; ++i) {
        cell(i, j) = cell(i, j) * dum;
      }
    }
  }

  // In the last column no normalization of the elements
  // below takes place anymore. The last pivot must still be checked for
  // exact singularity, so that a linearly dependent matrix is not
  // falsely reported as successfully factorized.
  if (cell(equationCount, equationCount) == 0.0) {
    return SolveStatus::singularMatrix;
  }

  return SolveStatus::success;
}

SolveStatus backSubstituteLuInPlace(RealMatrixRows factoredMatrix,
                                    std::span<const int> pivotIndices,
                                    std::span<double> rightHandSide,
                                    int equationCount) noexcept {
  if (!hasValidRealDimensions(factoredMatrix, rightHandSide, equationCount) ||
      pivotIndices.size() < requiredSize(equationCount)) {
    return SolveStatus::invalidDimension;
  }
  for (int index = 1; index <= equationCount; ++index) {
    const int pivot = elementAt(pivotIndices, index);
    if (pivot < 1 || pivot > equationCount) {
      return SolveStatus::invalidDimension;
    }
  }
  if (!allActiveMatrixValuesFinite(factoredMatrix, equationCount) ||
      !allActiveVectorValuesFinite(rightHandSide, equationCount)) {
    return SolveStatus::nonFiniteInput;
  }

  // Adopted from BAKSUB (fortran-src/xsolve.f:250). ii logic, pivot swaps of the right-hand
  // side, forward and back substitution with unchanged
  // operation order.
  auto cell = [factoredMatrix](int row, int column) -> double& {
    return factoredMatrix[static_cast<std::size_t>(row)]
                         [static_cast<std::size_t>(column)];
  };

  double sum = 0.0;
  int ii = 0;
  for (int i = 1; i <= equationCount; i++) {
    const int ll = elementAt(pivotIndices, i);
    sum = elementAt(rightHandSide, ll);
    elementAt(rightHandSide, ll) = elementAt(rightHandSide, i);
    if (ii != 0) {
      for (int j = ii; j <= i - 1; j++) {
        sum = sum - cell(i, j) * elementAt(rightHandSide, j);
      }
    } else if (sum != 0.0) {
      ii = i;
    }

    elementAt(rightHandSide, i) = sum;
  }

  for (int i = equationCount; i >= 1; i--) {
    sum = elementAt(rightHandSide, i);
    if (i < equationCount) {
      for (int j = i + 1; j <= equationCount; j++) {
        sum = sum - cell(i, j) * elementAt(rightHandSide, j);
      }
    }

    // --- exact zero-pivot check before the division by the diagonal
    if (cell(i, i) == 0.0) {
      return SolveStatus::singularMatrix;
    }
    elementAt(rightHandSide, i) = sum / cell(i, i);
  }

  return SolveStatus::success;
}

SolveStatus solveComplexGaussianInPlace(
    ComplexMatrixRows matrix,
    std::span<std::complex<double>> rightHandSide,
    int equationCount) noexcept {
  if (!hasValidComplexDimensions(matrix, rightHandSide, equationCount)) {
    return SolveStatus::invalidDimension;
  }
  if (!allActiveMatrixValuesFinite(matrix, equationCount) ||
      !allActiveVectorValuesFinite(rightHandSide, equationCount)) {
    return SolveStatus::nonFiniteInput;
  }

  // Adopted from CGAUSS (fortran-src/xsolve.f:103). Operation order and the
  // unchanged comparison semantics of the pivot choice
  // (abs(candidate) - abs(current) > 0.0) are retained.
  using std::complex;
  auto cell = [matrix](int row, int column) -> complex<double>& {
    return matrix[static_cast<std::size_t>(row)]
                  [static_cast<std::size_t>(column)];
  };

  for (int np = 1; np <= equationCount - 1; np++) {
    const int np1 = np + 1;

    // --- find max pivot index nx
    int nx = np;
    for (int n = np1; n <= equationCount; n++) {
      if (std::abs(cell(n, np)) - std::abs(cell(nx, np)) > 0.0) {
        nx = n;
      }
    }

    // --- exact singularity check (no tolerance adjustment)
    if (std::abs(cell(nx, np)) == 0.0) {
      return SolveStatus::singularMatrix;
    }
    const complex<double> pivot = complex<double>(1.0, 0.0) / cell(nx, np);

    // --- switch pivots
    cell(nx, np) = cell(np, np);

    // --- switch rows & normalize pivot row
    for (int l = np1; l <= equationCount; l++) {
      const complex<double> temp = cell(nx, l) * pivot;
      cell(nx, l) = cell(np, l);
      cell(np, l) = temp;
    }

    const complex<double> rhsTemp = elementAt(rightHandSide, nx) * pivot;
    elementAt(rightHandSide, nx) = elementAt(rightHandSide, np);
    elementAt(rightHandSide, np) = rhsTemp;

    // --- forward eliminate everything
    for (int k = np1; k <= equationCount; k++) {
      const complex<double> ztmp = cell(k, np);
      for (int l = np1; l <= equationCount; l++) {
        cell(k, l) = cell(k, l) - ztmp * cell(np, l);
      }
      elementAt(rightHandSide, k) =
          elementAt(rightHandSide, k) - ztmp * elementAt(rightHandSide, np);
    }
  }

  //--- solve for last row (exact singularity check before the division)
  if (std::abs(cell(equationCount, equationCount)) == 0.0) {
    return SolveStatus::singularMatrix;
  }
  elementAt(rightHandSide, equationCount) =
      elementAt(rightHandSide, equationCount) /
      cell(equationCount, equationCount);

  //--- back substitute everything
  for (int np = equationCount - 1; np >= 1; np--) {
    const int np1 = np + 1;
    for (int k = np1; k <= equationCount; k++) {
      elementAt(rightHandSide, np) =
          elementAt(rightHandSide, np) -
          cell(np, k) * elementAt(rightHandSide, k);
    }
  }

  return SolveStatus::success;
}

SolveStatus solveTridiagonalInPlace(std::span<double> diagonal,
                                    std::span<double> lower,
                                    std::span<double> upper,
                                    std::span<double> rightHandSide,
                                    int equationCount) noexcept {
  const auto required = hasValidEquationCount(equationCount)
                            ? requiredSize(equationCount)
                            : 0;
  if (!hasValidEquationCount(equationCount) || diagonal.size() < required ||
      lower.size() < required || upper.size() < required ||
      rightHandSide.size() < required) {
    return SolveStatus::invalidDimension;
  }
  // Checked against the active inputs actually used:
  // diagonal[1..n], lower[2..n], upper[1..n-1], rightHandSide[1..n].
  for (int index = 1; index <= equationCount; ++index) {
    if (!isFinite(elementAt(diagonal, index)) ||
        !isFinite(elementAt(rightHandSide, index))) {
      return SolveStatus::nonFiniteInput;
    }
    if (index >= 2 && !isFinite(elementAt(lower, index))) {
      return SolveStatus::nonFiniteInput;
    }
    if (index < equationCount && !isFinite(elementAt(upper, index))) {
      return SolveStatus::nonFiniteInput;
    }
  }

  // Canonical implementation of the TRISOL tridiagonal solver
  // (fortran-src/spline.f:185), shared with the
  // xfoil::math::spline forwarding wrapper.
  // Formula and operation order unchanged; diagonal, upper and
  // rightHandSide are changed, lower stays unchanged. The only addition
  // is the exact zero-pivot checks before each division.
  for (int k = 2; k <= equationCount; k++) {
    const int km = k - 1;
    // --- exact zero-pivot check before the division by diagonal[km]
    if (elementAt(diagonal, km) == 0.0) {
      return SolveStatus::singularMatrix;
    }
    elementAt(upper, km) = elementAt(upper, km) / elementAt(diagonal, km);
    elementAt(rightHandSide, km) =
        elementAt(rightHandSide, km) / elementAt(diagonal, km);
    elementAt(diagonal, k) = elementAt(diagonal, k) -
                             elementAt(lower, k) * elementAt(upper, km);
    elementAt(rightHandSide, k) =
        elementAt(rightHandSide, k) -
        elementAt(lower, k) * elementAt(rightHandSide, km);
  }

  // --- exact zero-pivot check before the last division
  if (elementAt(diagonal, equationCount) == 0.0) {
    return SolveStatus::singularMatrix;
  }
  elementAt(rightHandSide, equationCount) =
      elementAt(rightHandSide, equationCount) /
      elementAt(diagonal, equationCount);

  for (int k = equationCount - 1; k >= 1; k--) {
    elementAt(rightHandSide, k) =
        elementAt(rightHandSide, k) -
        elementAt(upper, k) * elementAt(rightHandSide, k + 1);
  }
  return SolveStatus::success;
}

}  // namespace xfoil::math::linear_algebra

// End: src/core/math/LinearAlgebra.cpp

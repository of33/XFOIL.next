// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
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

// File: tests/unit/LinearAlgebraTests.cpp
// tests of the linear-algebra modules. All test data
// use the documented 1-based convention with reserved
// index 0; active indices are 1..equationCount. Solvable systems are
// additionally checked against the residual of the unchanged original matrix
// (solution tolerance 1e-12, residual 1e-9).

#include "TestSupport.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

#include "xfoil/math/LinearAlgebra.h"

namespace la = xfoil::math::linear_algebra;

using la::SolveStatus;

namespace {

// Builds separate row spans from a real C matrix array
// (1-based addressing stays preserved, index 0 reserved).
template <class T, std::size_t Rows, std::size_t Columns>
auto rowSpans(T (&matrix)[Rows][Columns]) {
  std::array<std::span<T>, Rows> rows{};

  for (std::size_t row = 0; row < Rows; ++row) {
    rows[row] = std::span<T>(matrix[row], Columns);
  }

  return rows;
}

// Produces a vector with reserved index 0 and fills the active
// indices 1..n with the values from values1Based (count n).
std::vector<double> makeOneBased(const double* values1Based, int n) {
  std::vector<double> result(static_cast<std::size_t>(n) + 1, 0.0);
  for (int i = 1; i <= n; i++) {
    result[static_cast<std::size_t>(i)] = values1Based[i - 1];
  }
  return result;
}

// Checks |b_orig - A_orig * x| component-wise against a tight tolerance.
template <std::size_t Size>
void checkResidual(const double (&original)[Size][Size],
                   const std::vector<double>& solution,
                   const std::vector<double>& originalRhs,
                   int equationCount) {
  for (int i = 1; i <= equationCount; i++) {
    double sum = 0.0;
    for (int j = 1; j <= equationCount; j++) {
      sum += original[i][j] * solution[static_cast<std::size_t>(j)];
    }
    const double residual =
        std::fabs(originalRhs[static_cast<std::size_t>(i)] - sum);
    CHECK(residual < 1e-9);
  }
}

template <std::size_t Size>
void checkComplexResidual(
    const std::complex<double> (&original)[Size][Size],
    const std::vector<std::complex<double>>& solution,
    const std::vector<std::complex<double>>& originalRhs,
    int equationCount) {
  for (int i = 1; i <= equationCount; i++) {
    std::complex<double> sum(0.0, 0.0);
    for (int j = 1; j <= equationCount; j++) {
      sum += original[i][j] * solution[static_cast<std::size_t>(j)];
    }
    const double residual =
        std::abs(originalRhs[static_cast<std::size_t>(i)] - sum);
    CHECK(residual < 1e-9);
  }
}

const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

}  // namespace

// ---
// real Gauss solver
// ---

TEST(unit, RealGaussianSolvesOneByOneSystem) {
  const double rhsInput[1] = {5.0};
  double matrix[2][2] = {{0.0, 0.0}, {0.0, 2.5}};
  const double original[2][2] = {{0.0, 0.0}, {0.0, 2.5}};
  auto rhs = makeOneBased(rhsInput, 1);
  const std::vector<double> originalRhs = makeOneBased(rhsInput, 1);

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 1) == SolveStatus::success);
  CHECK_NEAR(rhs[1], 2.0, 1e-12, 0.0);
  checkResidual(original, rhs, originalRhs, 1);
}

TEST(unit, RealGaussianSolvesKnownFourByFourSystem) {
  // active system (indices 1..4), solution (1, -2, 3, -1).
  double matrix[5][5] = {
      {0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 4.0, 1.0, 0.0, 0.0},
      {0.0, 1.0, 3.0, 1.0, 0.0},
      {0.0, 0.0, 1.0, 2.0, 1.0},
      {0.0, 0.0, 0.0, 1.0, 1.0},
  };
  const double original[5][5] = {
      {0.0, 0.0, 0.0, 0.0, 0.0},
      {0.0, 4.0, 1.0, 0.0, 0.0},
      {0.0, 1.0, 3.0, 1.0, 0.0},
      {0.0, 0.0, 1.0, 2.0, 1.0},
      {0.0, 0.0, 0.0, 1.0, 1.0},
  };
  const double rhsValues[4] = {2.0, -2.0, 3.0, 2.0};
  auto rhs = makeOneBased(rhsValues, 4);
  const std::vector<double> originalRhs = makeOneBased(rhsValues, 4);

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 4) == SolveStatus::success);

  CHECK_NEAR(rhs[1], 1.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[2], -2.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[3], 3.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[4], -1.0, 1e-12, 0.0);
  checkResidual(original, rhs, originalRhs, 4);
}

TEST(unit, RealGaussianPerformsPivotRowSwap) {
  // z[1][1] is exactly zero; without a pivot swap (strict >) the
  // chosen pivot could not be made nonzero. Solution (-1, 2).
  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 2.0}, {0.0, 1.0, 3.0}};
  const double original[3][3] = {
      {0.0, 0.0, 0.0}, {0.0, 0.0, 2.0}, {0.0, 1.0, 3.0}};
  const double rhsValues[2] = {4.0, 5.0};
  auto rhs = makeOneBased(rhsValues, 2);
  const std::vector<double> originalRhs = makeOneBased(rhsValues, 2);

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 2) == SolveStatus::success);

  CHECK_NEAR(rhs[1], -1.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[2], 2.0, 1e-12, 0.0);
  checkResidual(original, rhs, originalRhs, 2);

  // documented row swap: at the swapped pivot position stands
  // the old value of z[1][1] (exactly 0.0) after the swap.
  CHECK_EQ(matrix[2][1], 0.0);
}

TEST(unit, RealGaussianRejectsInvalidDimension) {
  double matrix[3][3] = {{1.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 3.0}};
  std::vector<double> rhs(3, 1.0);

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 0) ==
        SolveStatus::invalidDimension);
  CHECK(la::solveGaussianInPlace(rows, rhs, -1) ==
        SolveStatus::invalidDimension);

  // only two rows present, equationCount = 2 requires three.
  std::array<std::span<double>, 2> tooFewRows = {
      std::span<double>(matrix[0], 3), std::span<double>(matrix[1], 3)};
  CHECK(la::solveGaussianInPlace(tooFewRows, rhs, 2) ==
        SolveStatus::invalidDimension);
}

TEST(unit, RealGaussianRejectsShortRightHandSide) {
  double matrix[3][3] = {{1.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 3.0}};
  std::vector<double> rhs(2, 1.0);  // 2 instead of at least 3 elements

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 2) ==
        SolveStatus::invalidDimension);
}

TEST(unit, RealGaussianRejectsShortMatrixRow) {
  std::array<double, 4> row0{0.0, 0.0, 0.0, 0.0};
  std::array<double, 4> row1{0.0, 1.0, 0.0, 0.0};
  std::array<double, 3> shortRow{0.0, 0.0, 2.0};  // 3 instead of at least 4
  std::array<double, 4> row3{0.0, 0.0, 0.0, 3.0};

  std::array<std::span<double>, 4> rows = {
      std::span<double>(row0), std::span<double>(row1),
      std::span<double>(shortRow), std::span<double>(row3)};
  std::vector<double> rhs(4, 1.0);

  CHECK(la::solveGaussianInPlace(rows, rhs, 3) ==
        SolveStatus::invalidDimension);
}

TEST(unit, RealGaussianRejectsSingularMatrix) {
  // row 2 = 2 * row 1: after elimination the last pivot is
  // exactly zero.
  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 1.0, 2.0}, {0.0, 2.0, 4.0}};
  const double rhsValues[2] = {1.0, 2.0};
  auto rhs = makeOneBased(rhsValues, 2);

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 2) == SolveStatus::singularMatrix);
}

TEST(unit, RealGaussianRejectsNonFiniteInput) {
  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, kNan, 1.0}, {0.0, 1.0, 2.0}};
  std::vector<double> rhs(3, 1.0);

  auto rows = rowSpans(matrix);
  CHECK(la::solveGaussianInPlace(rows, rhs, 2) == SolveStatus::nonFiniteInput);

  double goodMatrix[3][3] = {
      {0.0, 0.0, 0.0}, {0.0, 2.0, 1.0}, {0.0, 1.0, 2.0}};
  const double infValues[2] = {1.0, kInf};
  std::vector<double> infRhs = makeOneBased(infValues, 2);
  auto goodRows = rowSpans(goodMatrix);
  CHECK(la::solveGaussianInPlace(goodRows, infRhs, 2) ==
        SolveStatus::nonFiniteInput);
}

// ---
// LU factorization and back substitution
// ---

TEST(unit, LuFactorAndBackSubstitutionSolveKnownSystem) {
  // Symmetric diagonally dominant system with solution (1.5, 2, 2.5).
  double matrix[4][4] = {{0.0, 0.0, 0.0, 0.0},
                         {0.0, 2.0, -1.0, 0.0},
                         {0.0, -1.0, 2.0, -1.0},
                         {0.0, 0.0, -1.0, 2.0}};
  const double original[4][4] = {{0.0, 0.0, 0.0, 0.0},
                                 {0.0, 2.0, -1.0, 0.0},
                                 {0.0, -1.0, 2.0, -1.0},
                                 {0.0, 0.0, -1.0, 2.0}};
  const double rhsValues[3] = {1.0, 0.0, 3.0};
  auto rhs = makeOneBased(rhsValues, 3);
  const std::vector<double> originalRhs = makeOneBased(rhsValues, 3);
  std::array<int, 4> pivots{0, 0, 0, 0};

  auto rows = rowSpans(matrix);
  CHECK(la::factorLuInPlace(rows, pivots, 3) == SolveStatus::success);
  CHECK(la::backSubstituteLuInPlace(rows, std::span<const int>(pivots), rhs,
                                    3) == SolveStatus::success);

  CHECK_NEAR(rhs[1], 1.5, 1e-12, 0.0);
  CHECK_NEAR(rhs[2], 2.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[3], 2.5, 1e-12, 0.0);
  checkResidual(original, rhs, originalRhs, 3);
}

TEST(unit, LuFactorPerformsScaledPivoting) {
  // Both rows give the same scaled magnitude in column 1:
  //
  // row 1: max(|2|, |4|) = 4, so (1/4) * |2| = 0.5
  // row 2: max(|1|, |-2|) = 2, so (1/2) * |1| = 0.5
  //
  // the unchanged comparison >= selects the later row on a tie,
  // so pivot index 2. The matrix is not singular.
  double matrix[3][3] = {
    {0.0, 0.0, 0.0},
    {0.0, 2.0, 4.0},
    {0.0, 1.0, -2.0},
  };
  std::array<int, 3> pivots{0, 0, 0};

  auto rows = rowSpans(matrix);

  CHECK(la::factorLuInPlace(rows, pivots, 2) ==
  SolveStatus::success);
  CHECK_EQ(pivots[1], 2);

  // row swap performed: the new row 1 contains the old
  // row 2.
  CHECK_EQ(matrix[1][1], 1.0);
  CHECK_EQ(matrix[1][2], -2.0);

  // the last pivot must be nonzero for this non-singular
  // matrix.
  CHECK(matrix[2][2] != 0.0);
}

TEST(unit, LuFactorRejectsDependentRows) {
  // Both rows are nonzero but linearly dependent. The singular
  // last pivot appears only during factorization.
  double matrix[3][3] = {
    {0.0, 0.0, 0.0},
    {0.0, 1.0, 2.0},
    {0.0, 2.0, 4.0},
  };
  std::array<int, 3> pivots{0, 0, 0};

  auto rows = rowSpans(matrix);

  CHECK(la::factorLuInPlace(rows, pivots, 2) ==
  SolveStatus::singularMatrix);
}

TEST(unit, LuFactorNvxBoundary) {
  // XFOIL 6.996 NVX=515: 515 is valid, 516 is rejected before mutation.
  const int dim = 516;
  std::vector<std::vector<double>> data(
      static_cast<std::size_t>(dim) + 1,
      std::vector<double>(static_cast<std::size_t>(dim) + 1, 0.0));
  std::vector<std::span<double>> rows;
  rows.reserve(data.size());
  for (auto& row : data) {
    rows.emplace_back(row);
  }
  std::vector<int> pivots(static_cast<std::size_t>(dim) + 1, 0);
  for (int i = 1; i <= dim; ++i) {
    data[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = 1.0;
  }

  CHECK(la::factorLuInPlace(rows, pivots, 515) == SolveStatus::success);
  CHECK_EQ(pivots[1], 1);
  CHECK_EQ(pivots[515], 515);

  CHECK(la::factorLuInPlace(rows, pivots, 516) ==
        SolveStatus::invalidDimension);
}

TEST(unit, LuFactorRejectsZeroRow) {
  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 1.0, 2.0}, {0.0, 0.0, 0.0}};
  std::array<int, 3> pivots{0, 0, 0};

  auto rows = rowSpans(matrix);
  CHECK(la::factorLuInPlace(rows, pivots, 2) == SolveStatus::singularMatrix);
}

TEST(unit, LuFactorRejectsInvalidDimension) {
  double matrix[3][3] = {{1.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 3.0}};
  std::array<int, 3> pivots{0, 0, 0};

  auto rows = rowSpans(matrix);
  CHECK(la::factorLuInPlace(rows, pivots, 0) == SolveStatus::invalidDimension);

  // pivot span too short (2 instead of at least 3 elements).
  std::array<int, 2> shortPivots{0, 0};
  CHECK(la::factorLuInPlace(rows, shortPivots, 2) ==
        SolveStatus::invalidDimension);

  // matrix row too short.
  std::array<double, 4> row0{0.0, 0.0, 0.0, 0.0};
  std::array<double, 4> row1{0.0, 2.0, 0.0, 0.0};
  std::array<double, 3> shortRow{0.0, 0.0, 3.0};
  std::array<std::span<double>, 4> mixedRows = {
      std::span<double>(row0), std::span<double>(row1),
      std::span<double>(shortRow), std::span<double>(row0)};
  CHECK(la::factorLuInPlace(mixedRows, pivots, 3) ==
        SolveStatus::invalidDimension);
}

TEST(unit, LuBackSubstitutionRejectsInvalidPivotIndex) {
  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 2.0, 1.0}, {0.0, 0.0, 3.0}};
  std::vector<double> rhs(3, 1.0);

  std::array<int, 3> zeroPivot{0, 0, 1};
  auto rows = rowSpans(matrix);
  CHECK(la::backSubstituteLuInPlace(rows, std::span<const int>(zeroPivot), rhs,
                                    2) == SolveStatus::invalidDimension);

  std::array<int, 3> tooLargePivot{0, 3, 1};
  CHECK(la::backSubstituteLuInPlace(rows, std::span<const int>(tooLargePivot),
                                    rhs, 2) == SolveStatus::invalidDimension);
}

TEST(unit, LuBackSubstitutionRejectsZeroDiagonal) {
  // Factorized matrix with an exactly zero diagonal at position (2,2).
  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 1.0, 1.0}, {0.0, 0.0, 0.0}};
  std::array<int, 3> pivots{0, 1, 2};
  const double rhsValues[2] = {1.0, 2.0};
  auto rhs = makeOneBased(rhsValues, 2);

  auto rows = rowSpans(matrix);
  CHECK(la::backSubstituteLuInPlace(rows, std::span<const int>(pivots), rhs,
                                    2) == SolveStatus::singularMatrix);
}

TEST(unit, LuRejectsNonFiniteInput) {
  double nanMatrix[3][3] = {
      {0.0, 0.0, 0.0}, {0.0, kNan, 1.0}, {0.0, 1.0, 2.0}};
  std::array<int, 3> pivots{0, 0, 0};
  auto nanRows = rowSpans(nanMatrix);
  CHECK(la::factorLuInPlace(nanRows, pivots, 2) ==
        SolveStatus::nonFiniteInput);

  double matrix[3][3] = {{0.0, 0.0, 0.0}, {0.0, 2.0, 1.0}, {0.0, 0.0, 3.0}};
  std::array<int, 3> validPivots{0, 1, 2};
  std::vector<double> infRhs(3, kInf);
  auto rows = rowSpans(matrix);
  CHECK(la::backSubstituteLuInPlace(rows, std::span<const int>(validPivots),
                                    infRhs, 2) ==
        SolveStatus::nonFiniteInput);
}

// ---
// complex Gauss solver
// ---

TEST(unit, ComplexGaussianSolvesKnownTwoByTwoSystem) {
  using std::complex;
  // A * (1+i, 2-i) = (1+3i, 8-3i) with real complex coefficients.
  complex<double> matrix[3][3] = {
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(0.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(0.0, 2.0),
       complex<double>(1.0, 1.0)},
      {complex<double>(0.0, 0.0), complex<double>(1.0, -1.0),
       complex<double>(3.0, 0.0)},
  };
  const complex<double> original[3][3] = {
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(0.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(0.0, 2.0),
       complex<double>(1.0, 1.0)},
      {complex<double>(0.0, 0.0), complex<double>(1.0, -1.0),
       complex<double>(3.0, 0.0)},
  };
  std::vector<complex<double>> rhs(3, complex<double>(0.0, 0.0));
  rhs[1] = complex<double>(1.0, 3.0);
  rhs[2] = complex<double>(8.0, -3.0);
  const std::vector<complex<double>> originalRhs = rhs;

  auto rows = rowSpans(matrix);
  CHECK(la::solveComplexGaussianInPlace(rows, rhs, 2) ==
        SolveStatus::success);

  CHECK_NEAR(rhs[1].real(), 1.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[1].imag(), 1.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[2].real(), 2.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[2].imag(), -1.0, 1e-12, 0.0);
  checkComplexResidual(original, rhs, originalRhs, 2);
}

TEST(unit, ComplexGaussianPerformsPivotSwap) {
  using std::complex;
  // z[1][1] is exact zero; the pivot swap on line 2 (magnitude sqrt(2))
  // is required. Solution (1, i).
  complex<double> matrix[3][3] = {
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(0.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(1.0, 1.0)},
      {complex<double>(0.0, 0.0), complex<double>(1.0, -1.0),
       complex<double>(2.0, 0.0)},
  };
  std::vector<complex<double>> rhs(3, complex<double>(0.0, 0.0));
  rhs[1] = complex<double>(-1.0, 1.0);
  rhs[2] = complex<double>(1.0, 1.0);

  auto rows = rowSpans(matrix);
  CHECK(la::solveComplexGaussianInPlace(rows, rhs, 2) ==
        SolveStatus::success);

  CHECK_NEAR(rhs[1].real(), 1.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[1].imag(), 0.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[2].real(), 0.0, 1e-12, 0.0);
  CHECK_NEAR(rhs[2].imag(), 1.0, 1e-12, 0.0);

  // documented swap: at the swapped pivot position stands the
  // old value of z[1][1] (exactly 0).
  CHECK_EQ(matrix[2][1], complex<double>(0.0, 0.0));
}

TEST(unit, ComplexGaussianRejectsSingularMatrix) {
  using std::complex;
  // after the elimination is the last pivot exact zero.
  complex<double> matrix[3][3] = {
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(0.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(1.0, 1.0),
       complex<double>(2.0, 2.0)},
      {complex<double>(0.0, 0.0), complex<double>(2.0, 0.0),
       complex<double>(4.0, 0.0)},
  };
  std::vector<complex<double>> rhs(3, complex<double>(0.0, 0.0));
  rhs[1] = complex<double>(3.0, 3.0);
  rhs[2] = complex<double>(6.0, 0.0);

  auto rows = rowSpans(matrix);
  CHECK(la::solveComplexGaussianInPlace(rows, rhs, 2) ==
        SolveStatus::singularMatrix);
}

TEST(unit, ComplexGaussianRejectsNonFiniteInput) {
  using std::complex;
  complex<double> matrix[3][3] = {
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(0.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(2.0, kNan),
       complex<double>(1.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(1.0, 0.0),
       complex<double>(2.0, 0.0)},
  };
  std::vector<complex<double>> rhs(3, complex<double>(1.0, 0.0));

  auto rows = rowSpans(matrix);
  CHECK(la::solveComplexGaussianInPlace(rows, rhs, 2) ==
        SolveStatus::nonFiniteInput);

  complex<double> goodMatrix[3][3] = {
      {complex<double>(0.0, 0.0), complex<double>(0.0, 0.0),
       complex<double>(0.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(2.0, 0.0),
       complex<double>(1.0, 0.0)},
      {complex<double>(0.0, 0.0), complex<double>(1.0, 0.0),
       complex<double>(2.0, 0.0)},
  };
  std::vector<complex<double>> infRhs(3, complex<double>(0.0, kInf));
  auto goodRows = rowSpans(goodMatrix);
  CHECK(la::solveComplexGaussianInPlace(goodRows, infRhs, 2) ==
        SolveStatus::nonFiniteInput);
}

TEST(unit, ComplexGaussianRejectsInvalidDimension) {
  using std::complex;
  complex<double> matrix[3][3];
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      matrix[row][column] = complex<double>(0.0, 0.0);
    }
  }
  std::vector<complex<double>> rhs(3, complex<double>(1.0, 1.0));

  auto rows = rowSpans(matrix);
  CHECK(la::solveComplexGaussianInPlace(rows, rhs, 0) ==
        SolveStatus::invalidDimension);

  std::array<std::span<complex<double>>, 2> tooFewRows = {
      std::span<complex<double>>(matrix[0], 3),
      std::span<complex<double>>(matrix[1], 3)};
  CHECK(la::solveComplexGaussianInPlace(tooFewRows, rhs, 2) ==
        SolveStatus::invalidDimension);

  std::vector<complex<double>> shortRhs(2, complex<double>(1.0, 1.0));
  CHECK(la::solveComplexGaussianInPlace(rows, shortRhs, 2) ==
        SolveStatus::invalidDimension);
}

// ---
// Tridiagonal solver
// ---

TEST(unit, LinearAlgebraTridiagonalMatchesKnownSolution) {
  // Solves 2*x1 - x2 = 1, -x1 + 2*x2 - x3 = -2, -x2 + 2*x3 = 3
  // with the known solution (0.5, 0, 1.5).
  std::vector<double> a = {0.0, 2.0, 2.0, 2.0};
  std::vector<double> b = {0.0, 0.0, -1.0, -1.0};
  std::vector<double> c = {0.0, -1.0, -1.0, 0.0};
  std::vector<double> d = {0.0, 1.0, -2.0, 3.0};

  CHECK(la::solveTridiagonalInPlace(a, b, c, d, 3) == SolveStatus::success);

  CHECK_NEAR(d[1], 0.5, 1e-12, 0.0);
  CHECK_NEAR(d[2], 0.0, 1e-12, 0.0);
  CHECK_NEAR(d[3], 1.5, 1e-12, 0.0);
}

TEST(unit, LinearAlgebraTridiagonalLeavesLowerDiagonalUnchanged) {
  std::vector<double> a = {0.0, 2.0, 2.0, 2.0};
  std::vector<double> b = {0.0, 0.0, -1.0, -1.0};
  std::vector<double> c = {0.0, -1.0, -1.0, 0.0};
  std::vector<double> d = {0.0, 1.0, -2.0, 3.0};
  const std::vector<double> bBefore = b;

  CHECK(la::solveTridiagonalInPlace(a, b, c, d, 3) == SolveStatus::success);

  for (std::size_t i = 0; i < bBefore.size(); i++) {
    CHECK_EQ(b[i], bBefore[i]);
  }
}

TEST(unit, LinearAlgebraTridiagonalRejectsInvalidDimension) {
  std::vector<double> a = {0.0, 2.0, 2.0};
  std::vector<double> b = {0.0, 0.0, -1.0};
  std::vector<double> c = {0.0, -1.0, 0.0};
  std::vector<double> d = {0.0, 1.0, -2.0};

  CHECK(la::solveTridiagonalInPlace(a, b, c, d, 0) ==
        SolveStatus::invalidDimension);

  // equation count = 3, but the spans contain only 3 instead of 4 elements.
  std::vector<double> shortDiag = {0.0, 2.0, 2.0};
  CHECK(la::solveTridiagonalInPlace(shortDiag, b, c, d, 3) ==
        SolveStatus::invalidDimension);
}

TEST(unit, LinearAlgebraTridiagonalRejectsExactZeroPivot) {
  // First diagonal entry exactly zero: division by diagonal[1].
  std::vector<double> a = {0.0, 0.0, 2.0, 2.0};
  std::vector<double> b = {0.0, 0.0, -1.0, -1.0};
  std::vector<double> c = {0.0, -1.0, -1.0, 0.0};
  std::vector<double> d = {0.0, 1.0, -2.0, 3.0};
  CHECK(la::solveTridiagonalInPlace(a, b, c, d, 3) ==
        SolveStatus::singularMatrix);

  // One-equation case: division by diagonal[equationCount].
  std::vector<double> singleA = {0.0, 0.0};
  std::vector<double> singleB = {0.0, 0.0};
  std::vector<double> singleC = {0.0, 0.0};
  std::vector<double> singleD = {0.0, 1.0};
  CHECK(la::solveTridiagonalInPlace(singleA, singleB, singleC, singleD, 1) ==
        SolveStatus::singularMatrix);
}

TEST(unit, LinearAlgebraTridiagonalRejectsNonFiniteInput) {
  std::vector<double> a = {0.0, 2.0, 2.0};
  std::vector<double> b = {0.0, 0.0, -1.0};
  std::vector<double> c = {0.0, -1.0, 0.0};
  std::vector<double> d = {0.0, kNan, -2.0};
  CHECK(la::solveTridiagonalInPlace(a, b, c, d, 2) ==
        SolveStatus::nonFiniteInput);

  std::vector<double> nanLower = {0.0, 0.0, kInf};
  std::vector<double> goodD = {0.0, 1.0, -2.0};
  CHECK(la::solveTridiagonalInPlace(a, nanLower, c, goodD, 2) ==
        SolveStatus::nonFiniteInput);
}

// ---
// Status values
// ---

TEST(unit, SolveStatusValuesAreDistinct) {
  // No dependency on numeric enum values; only distinguishability.
  // Plain CHECKs instead of CHECK_NE, so no streamer is needed for the
  // enum.
  CHECK(SolveStatus::success != SolveStatus::invalidDimension);
  CHECK(SolveStatus::success != SolveStatus::singularMatrix);
  CHECK(SolveStatus::success != SolveStatus::nonFiniteInput);
  CHECK(SolveStatus::invalidDimension != SolveStatus::singularMatrix);
  CHECK(SolveStatus::invalidDimension != SolveStatus::nonFiniteInput);
  CHECK(SolveStatus::singularMatrix != SolveStatus::nonFiniteInput);
}

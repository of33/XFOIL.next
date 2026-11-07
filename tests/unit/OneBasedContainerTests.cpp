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

// File: tests/unit/OneBasedContainerTests.cpp
// tests for the 1-based containers: first/last access,
// fill, const access, at exceptions for invalid indices, and
// contiguous storage. Since the test framework has no CHECK_THROWS,
// exceptions are checked explicitly with try/catch. Deliberately no
// invalid operator() calls are tested, since those assert and would end
// the test process.

#include "TestSupport.h"

#include <stdexcept>

#include "xfoil/OneBasedArray.h"
#include "xfoil/OneBasedMatrix.h"

namespace {

// Aliases without template commas so that the CHECK macros are not
// split by the template argument list.
using IntArray3 = xfoil::OneBasedArray<int, 3>;
using IntMatrix2x3 = xfoil::OneBasedMatrix<int, 2, 3>;

}  // namespace

TEST(unit, OneBasedArrayStoresFirstAndLastElement) {
  xfoil::OneBasedArray<int, 3> values;
  values.fill(0);

  values(1) = 11;
  values(3) = 33;

  CHECK_EQ(values(1), 11);
  CHECK_EQ(values(2), 0);
  CHECK_EQ(values(3), 33);
  CHECK_EQ(IntArray3::maxIndex(), std::size_t{3});
  CHECK_EQ(IntArray3::storageSize(), std::size_t{4});
}

TEST(unit, OneBasedArrayFillInitializesElements) {
  xfoil::OneBasedArray<double, 4> values;
  values(1) = 1.0;
  values(4) = 4.0;

  values.fill(-2.5);

  CHECK_EQ(values(1), -2.5);
  CHECK_EQ(values(2), -2.5);
  CHECK_EQ(values(3), -2.5);
  CHECK_EQ(values(4), -2.5);
}

TEST(unit, OneBasedArrayConstAccessWorks) {
  xfoil::OneBasedArray<int, 3> values;
  values.fill(7);

  const xfoil::OneBasedArray<int, 3>& view = values;
  CHECK_EQ(view(1), 7);
  CHECK_EQ(view(3), 7);
  CHECK_EQ(view.at(2), 7);
}

TEST(unit, OneBasedArrayAtRejectsZero) {
  xfoil::OneBasedArray<int, 3> values;
  values.fill(0);

  bool threw = false;
  try {
    (void)values.at(0);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, OneBasedArrayAtRejectsIndexPastEnd) {
  xfoil::OneBasedArray<int, 3> values;
  values.fill(0);

  bool threwRead = false;
  try {
    (void)values.at(4);
  } catch (const std::out_of_range&) {
    threwRead = true;
  }
  CHECK(threwRead);

  bool threwWrite = false;
  try {
    values.at(4) = 1;
  } catch (const std::out_of_range&) {
    threwWrite = true;
  }
  CHECK(threwWrite);
}

TEST(unit, OneBasedArrayHasContiguousStorage) {
  xfoil::OneBasedArray<int, 3> values;
  values.fill(0);

  CHECK(&values(2) == &values(1) + 1);
  CHECK(&values(3) == &values(1) + 2);
}

TEST(unit, OneBasedMatrixStoresCorners) {
  xfoil::OneBasedMatrix<int, 2, 3> matrix;
  matrix.fill(0);

  matrix(1, 1) = 11;
  matrix(1, 3) = 13;
  matrix(2, 1) = 21;
  matrix(2, 3) = 23;

  CHECK_EQ(matrix(1, 1), 11);
  CHECK_EQ(matrix(1, 3), 13);
  CHECK_EQ(matrix(2, 1), 21);
  CHECK_EQ(matrix(2, 3), 23);
  CHECK_EQ(matrix(1, 2), 0);
  CHECK_EQ(IntMatrix2x3::maxRow(), std::size_t{2});
  CHECK_EQ(IntMatrix2x3::maxColumn(), std::size_t{3});
  CHECK_EQ(IntMatrix2x3::storageSize(), std::size_t{12});
}

TEST(unit, OneBasedMatrixFillInitializesElements) {
  xfoil::OneBasedMatrix<double, 2, 3> matrix;
  matrix(1, 1) = 1.0;
  matrix(2, 3) = 2.0;

  matrix.fill(-1.5);

  CHECK_EQ(matrix(1, 1), -1.5);
  CHECK_EQ(matrix(1, 2), -1.5);
  CHECK_EQ(matrix(1, 3), -1.5);
  CHECK_EQ(matrix(2, 1), -1.5);
  CHECK_EQ(matrix(2, 2), -1.5);
  CHECK_EQ(matrix(2, 3), -1.5);
}

TEST(unit, OneBasedMatrixConstAccessWorks) {
  xfoil::OneBasedMatrix<int, 2, 3> matrix;
  matrix.fill(5);

  const xfoil::OneBasedMatrix<int, 2, 3>& view = matrix;
  CHECK_EQ(view(1, 1), 5);
  CHECK_EQ(view(2, 3), 5);
  CHECK_EQ(view.at(2, 2), 5);
}

TEST(unit, OneBasedMatrixAtRejectsZeroRow) {
  xfoil::OneBasedMatrix<int, 2, 3> matrix;
  matrix.fill(0);

  bool threw = false;
  try {
    (void)matrix.at(0, 1);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, OneBasedMatrixAtRejectsZeroColumn) {
  xfoil::OneBasedMatrix<int, 2, 3> matrix;
  matrix.fill(0);

  bool threw = false;
  try {
    (void)matrix.at(1, 0);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, OneBasedMatrixAtRejectsPastEnd) {
  xfoil::OneBasedMatrix<int, 2, 3> matrix;
  matrix.fill(0);

  bool threwRow = false;
  try {
    (void)matrix.at(3, 1);
  } catch (const std::out_of_range&) {
    threwRow = true;
  }
  CHECK(threwRow);

  bool threwColumn = false;
  try {
    (void)matrix.at(1, 4);
  } catch (const std::out_of_range&) {
    threwColumn = true;
  }
  CHECK(threwColumn);
}

TEST(unit, OneBasedMatrixUsesContiguousRowMajorStorage) {
  xfoil::OneBasedMatrix<int, 2, 3> matrix;
  matrix.fill(0);

  // Within a line: step size 1.
  CHECK(&matrix(1, 2) == &matrix(1, 1) + 1);
  CHECK(&matrix(1, 3) == &matrix(1, 1) + 2);

  // next row: distance MaxColumn + 1 == 4 because of the reserved
  // column 0.
  CHECK(&matrix(2, 1) == &matrix(1, 1) + 4);
}

// end: tests/unit/OneBasedContainerTests.cpp

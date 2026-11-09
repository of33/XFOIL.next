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

// File: tests/core/FoundationTests.cpp
// Unit tests for the new 0-based core foundation:
// canonical core limits, 0-based matrix/views, and structured
// error/result objects. The tests use only xfoil::core and no
// app, or CLI code.

#include "TestSupport.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "xfoil/core/Limits.h"
#include "xfoil/core/Matrix.h"
#include "xfoil/core/Result.h"

namespace {

// returns true exactly when func throws ExceptionType.
template <class ExceptionType, class Func>
bool throwsExactly(const Func& func) {
  try {
    func();
  } catch (const ExceptionType&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

}  // namespace

// ---
// limits
// ---

// compile-time checks of the canonical Fortran values.
static_assert(xfoil::core::surfaceSystemCapacity == 601);
static_assert(xfoil::core::qspecDistributionCapacity == 5);
static_assert(xfoil::core::sideCount == 2);
static_assert(xfoil::core::wakeCapacity == 77);
static_assert(xfoil::core::bufferCapacity == 2404);
static_assert(xfoil::core::panelAndWakeCapacity == 678);
static_assert(xfoil::core::boundaryLayerCapacity == 427);
static_assert(xfoil::core::polarPointCapacity == 800);
static_assert(xfoil::core::polarSetCapacity == 20);
static_assert(xfoil::core::referencePointCapacity == 128);
static_assert(xfoil::core::camberTableCapacity == 4808);
static_assert(xfoil::core::circlePointCapacity == 257);
static_assert(xfoil::core::mappingCoefficientCapacity == 64);

TEST(unit, CoreLimitsMatchFortranValues) {
  using namespace xfoil::core;
  CHECK_EQ(surfaceSystemCapacity, std::size_t{601});
  CHECK_EQ(qspecDistributionCapacity, std::size_t{5});
  CHECK_EQ(sideCount, std::size_t{2});
  CHECK_EQ(wakeCapacity, std::size_t{77});
  CHECK_EQ(bufferCapacity, std::size_t{2404});
  CHECK_EQ(panelAndWakeCapacity, std::size_t{678});
  CHECK_EQ(boundaryLayerCapacity, std::size_t{427});
  CHECK_EQ(polarPointCapacity, std::size_t{800});
  CHECK_EQ(polarSetCapacity, std::size_t{20});
  CHECK_EQ(referencePointCapacity, std::size_t{128});
  CHECK_EQ(camberTableCapacity, std::size_t{4808});
  CHECK_EQ(circlePointCapacity, std::size_t{257});
  CHECK_EQ(mappingCoefficientCapacity, std::size_t{64});
}

TEST(unit, CoreLimitsRelationsHold) {
  using namespace xfoil::core;
  // Relations derived directly from XFOIL.INC / CIRCLE.INC.
  CHECK_EQ(wakeCapacity, surfaceSystemCapacity / 8 + 2);
  CHECK_EQ(bufferCapacity, 4 * surfaceSystemCapacity);
  CHECK_EQ(panelAndWakeCapacity, surfaceSystemCapacity + wakeCapacity);
  CHECK_EQ(boundaryLayerCapacity,
           surfaceSystemCapacity / 2 + wakeCapacity + 50);
  CHECK_EQ(camberTableCapacity, 2 * bufferCapacity);
  CHECK_EQ(mappingCoefficientCapacity, (circlePointCapacity - 1) / 4);
}

TEST(unit, CoreLimitsAreNotPlusOnePadded) {
  // No blanket +1 storage quantities: the capacities are exactly the
  // Fortran values (no index-0 padding).
  using namespace xfoil::core;
  CHECK_NE(surfaceSystemCapacity, std::size_t{602});
  CHECK_NE(panelAndWakeCapacity, std::size_t{679});
  CHECK_NE(bufferCapacity, std::size_t{2405});
  CHECK_NE(qspecDistributionCapacity, std::size_t{6});
  CHECK_NE(sideCount, std::size_t{3});
}

// ---
// matrix
// ---

TEST(unit, MatrixEmptyState) {
  xfoil::core::Matrix m;
  CHECK_EQ(m.rows(), std::size_t{0});
  CHECK_EQ(m.cols(), std::size_t{0});
  CHECK_EQ(m.size(), std::size_t{0});
  CHECK(m.empty());
  CHECK(m.data().empty());
  CHECK(m.data().size() == 0);
}

TEST(unit, MatrixOneByOne) {
  xfoil::core::Matrix m(1, 1, 0.0);
  CHECK_EQ(m.rows(), std::size_t{1});
  CHECK_EQ(m.cols(), std::size_t{1});
  CHECK_EQ(m.size(), std::size_t{1});
  CHECK_FALSE(m.empty());
  m.at(0, 0) = 3.25;
  CHECK_EQ(m.at(0, 0), 3.25);
}

TEST(unit, MatrixRectangular) {
  xfoil::core::Matrix m(2, 3, -1.5);
  CHECK_EQ(m.rows(), std::size_t{2});
  CHECK_EQ(m.cols(), std::size_t{3});
  CHECK_EQ(m.size(), std::size_t{6});
  for (std::size_t r = 0; r < m.rows(); ++r) {
    for (std::size_t c = 0; c < m.cols(); ++c) {
      CHECK_EQ(m.at(r, c), -1.5);
    }
  }
}

TEST(unit, MatrixZeroBasedFirstLastIndex) {
  xfoil::core::Matrix m(3, 4, 0.0);
  // First valid index (0,0), last valid index (rows-1, cols-1).
  m.at(0, 0) = 10.0;
  m.at(2, 3) = 20.0;
  CHECK_EQ(m.at(0, 0), 10.0);
  CHECK_EQ(m.at(2, 3), 20.0);
  // index (3, *) and (*, 4) are outside (0-based).
  CHECK(throwsExactly<std::out_of_range>(
      [&m] { (void)m.at(3, 0); }));
  CHECK(throwsExactly<std::out_of_range>(
      [&m] { (void)m.at(0, 4); }));
}

TEST(unit, MatrixInvalidRowThrows) {
  xfoil::core::Matrix m(2, 2, 0.0);
  CHECK(throwsExactly<std::out_of_range>(
      [&m] { (void)m.at(2, 0); }));
  CHECK(throwsExactly<std::out_of_range>(
      [&m] { (void)m.at(5, 1); }));
}

TEST(unit, MatrixInvalidColumnThrows) {
  xfoil::core::Matrix m(2, 2, 0.0);
  CHECK(throwsExactly<std::out_of_range>(
      [&m] { (void)m.at(0, 2); }));
  CHECK(throwsExactly<std::out_of_range>(
      [&m] { (void)m.at(1, 9); }));
}

TEST(unit, MatrixDimensionOverflowThrows) {
 // Portable overflow case: tooLarge * 2 exceeds size_t::max without
  // pinning a specific platform word width beforehand. tooLarge is computed
  // in the lambda so that no constant-expression capture is
  // needed (clang -Wunused-lambda-capture).
  CHECK(throwsExactly<std::length_error>([] {
    const std::size_t tooLarge =
        std::numeric_limits<std::size_t>::max() / 2 + 1;
    xfoil::core::Matrix m(tooLarge, 2);
    (void)m;
  }));
}

TEST(unit, MatrixZeroRows) {
  // 0 x N: no valid row and no valid element index.
  xfoil::core::Matrix m(0, 3, 0.0);
  CHECK_EQ(m.rows(), std::size_t{0});
  CHECK_EQ(m.cols(), std::size_t{3});
  CHECK_EQ(m.size(), std::size_t{0});
  CHECK(m.empty());
  CHECK(m.data().empty());
  // there is no valid row and no valid element index.
  CHECK(throwsExactly<std::out_of_range>([&m] { (void)m.row(0); }));
  CHECK(throwsExactly<std::out_of_range>([&m] { (void)m.at(0, 0); }));
}

TEST(unit, MatrixZeroColumns) {
  // N x 0: each valid row returns an empty span, but there are
  // no valid element indices.
  xfoil::core::Matrix m(3, 0, 0.0);
  CHECK_EQ(m.rows(), std::size_t{3});
  CHECK_EQ(m.cols(), std::size_t{0});
  CHECK_EQ(m.size(), std::size_t{0});
  CHECK(m.empty());
  CHECK(m.data().empty());
  // each valid row is an empty span (only size/empty checked).
  for (std::size_t r = 0; r < m.rows(); ++r) {
    const std::span<double> row = m.row(r);
    CHECK(row.empty());
    CHECK_EQ(row.size(), std::size_t{0});
  }
  // No valid element index.
  CHECK(throwsExactly<std::out_of_range>([&m] { (void)m.at(0, 0); }));
}

TEST(unit, MatrixMutableAndConstSpans) {
  xfoil::core::Matrix m(2, 2, 0.0);
  std::span<double> mutableSpan = m.data();
  CHECK_EQ(mutableSpan.size(), std::size_t{4});
  mutableSpan[0] = 1.0;
  mutableSpan[3] = 4.0;
  CHECK_EQ(m.at(0, 0), 1.0);
  CHECK_EQ(m.at(1, 1), 4.0);

  const xfoil::core::Matrix& cm = m;
  std::span<const double> constSpan = cm.data();
  CHECK_EQ(constSpan.size(), std::size_t{4});
  CHECK_EQ(constSpan[0], 1.0);
  CHECK_EQ(constSpan[3], 4.0);
}

TEST(unit, MatrixRowViews) {
  xfoil::core::Matrix m(2, 3, 0.0);
  m.at(0, 0) = 1.0;
  m.at(0, 1) = 2.0;
  m.at(0, 2) = 3.0;
  m.at(1, 0) = 4.0;
  m.at(1, 1) = 5.0;
  m.at(1, 2) = 6.0;

  std::span<double> row0 = m.row(0);
  CHECK_EQ(row0.size(), std::size_t{3});
  CHECK_EQ(row0[0], 1.0);
  CHECK_EQ(row0[1], 2.0);
  CHECK_EQ(row0[2], 3.0);

  std::span<double> row1 = m.row(1);
  CHECK_EQ(row1[0], 4.0);
  CHECK_EQ(row1[2], 6.0);

  // row index out of range throws.
  CHECK(throwsExactly<std::out_of_range>([&m] { (void)m.row(2); }));
}

TEST(unit, MatrixConstRowView) {
  xfoil::core::Matrix m(2, 3, 0.0);
  m.at(1, 2) = 42.0;
  const xfoil::core::Matrix& cm = m;
 // row via const matrix& returns exactly std::span<const double>.
  static_assert(std::is_same_v<decltype(cm.row(0)), std::span<const double>>);
  const std::span<const double> crow = cm.row(1);
  CHECK_EQ(crow.size(), std::size_t{3});
  CHECK_EQ(crow[2], 42.0);
}

TEST(unit, MatrixContiguousElementOrder) {
  // row-major: element (r,c) lies at offset r*cols + c.
  xfoil::core::Matrix m(2, 3, 0.0);
  for (std::size_t r = 0; r < m.rows(); ++r) {
    for (std::size_t c = 0; c < m.cols(); ++c) {
      m.at(r, c) = static_cast<double>(r * 3 + c);
    }
  }
  const std::span<const double> data = m.data();
  CHECK_EQ(data.size(), std::size_t{6});
  for (std::size_t i = 0; i < data.size(); ++i) {
    CHECK_EQ(data[i], static_cast<double>(i));
  }
}

// ---
// MatrixView
// ---

TEST(unit, MatrixViewValidatesSize) {
  std::vector<double> storage = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::span<const double> elements(storage);
  // Exact size: 2*3 == 6.
  xfoil::core::MatrixView view(elements, 2, 3);
  CHECK_EQ(view.rows(), std::size_t{2});
  CHECK_EQ(view.cols(), std::size_t{3});
  // Wrong size: 3*3 != 6.
  CHECK(throwsExactly<std::invalid_argument>(
      [&elements] { xfoil::core::MatrixView v(elements, 3, 3); (void)v; }));
 // Portable overflow case: tooLarge * 2 exceeds size_t::max. The
  // overflow must be detected before the span-size check. tooLarge
  // is computed in the lambda (no constant-expression capture).
  CHECK(throwsExactly<std::invalid_argument>([&elements] {
    const std::size_t tooLarge =
        std::numeric_limits<std::size_t>::max() / 2 + 1;
    xfoil::core::MatrixView v(elements, tooLarge, 2);
    (void)v;
  }));
}

TEST(unit, MatrixViewZeroRows) {
  // 0 x N via empty span: empty, no valid row.
  std::vector<double> empty;
  std::span<const double> elements(empty);
  xfoil::core::MatrixView view(elements, 0, 3);
  CHECK_EQ(view.rows(), std::size_t{0});
  CHECK_EQ(view.cols(), std::size_t{3});
  CHECK_EQ(view.size(), std::size_t{0});
  CHECK(view.empty());
  CHECK(view.data().empty());
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.row(0); }));
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.at(0, 0); }));
}

TEST(unit, MatrixViewZeroColumns) {
  // N x 0 via empty span: empty, each valid row is an empty span.
  std::vector<double> empty;
  std::span<const double> elements(empty);
  xfoil::core::MatrixView view(elements, 3, 0);
  CHECK_EQ(view.rows(), std::size_t{3});
  CHECK_EQ(view.cols(), std::size_t{0});
  CHECK_EQ(view.size(), std::size_t{0});
  CHECK(view.empty());
  CHECK(view.data().empty());
  for (std::size_t r = 0; r < view.rows(); ++r) {
    const std::span<const double> row = view.row(r);
    CHECK(row.empty());
    CHECK_EQ(row.size(), std::size_t{0});
  }
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.at(0, 0); }));
}

TEST(unit, MatrixViewBounds) {
  std::vector<double> storage = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::span<const double> elements(storage);
  xfoil::core::MatrixView view(elements, 2, 3);
  // last valid index (rows-1, cols-1).
  CHECK_EQ(view.at(1, 2), 6.0);
  // Invalid column.
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.at(0, 3); }));
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.at(1, 9); }));
 // Invalid row for at and row.
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.at(2, 0); }));
  CHECK(throwsExactly<std::out_of_range>([&view] { (void)view.row(2); }));
}

TEST(unit, MatrixViewAccessAndRow) {
  std::vector<double> storage = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::span<const double> elements(storage);
  xfoil::core::MatrixView view(elements, 2, 3);
  CHECK_EQ(view.at(0, 0), 1.0);
  CHECK_EQ(view.at(1, 2), 6.0);
  std::span<const double> row1 = view.row(1);
  CHECK_EQ(row1.size(), std::size_t{3});
  CHECK_EQ(row1[0], 4.0);
  CHECK_EQ(row1[2], 6.0);
  CHECK(throwsExactly<std::out_of_range>(
      [&view] { (void)view.at(2, 0); }));
}

TEST(unit, MatrixViewNoOwnership) {
  std::vector<double> storage = {1.0, 2.0, 3.0, 4.0};
  std::span<const double> elements(storage);
  xfoil::core::MatrixView view(elements, 2, 2);
  CHECK_EQ(view.at(0, 0), 1.0);
  // the view does not own the memory: changes to the underlying storage
  // are visible, but the view itself changes nothing.
  storage[0] = 99.0;
  CHECK_EQ(view.at(0, 0), 99.0);
  CHECK_EQ(view.data().data(), storage.data());
}

// ---
// result / error / Status
// ---

TEST(unit, ResultOkValue) {
  const xfoil::core::Result<int> ok = xfoil::core::Result<int>::ok(42);
  CHECK(ok.isOk());
  CHECK_FALSE(ok.isErr());
  CHECK_EQ(ok.value(), 42);
}

TEST(unit, ResultMoveOnlyValue) {
  // Movable, non-copyable value type: the success case must
  // compile and own the value correctly.
  auto payload = std::make_unique<int>(7);
  const xfoil::core::Result<std::unique_ptr<int>> ok =
      xfoil::core::Result<std::unique_ptr<int>>::ok(std::move(payload));
  CHECK(ok.isOk());
  CHECK_FALSE(ok.isErr());
  CHECK(ok.value() != nullptr);
  CHECK_EQ(*ok.value(), 7);
  // the original pointer is no longer set after the move.
  CHECK(payload == nullptr);
}

TEST(unit, ResultErrorValue) {
  const xfoil::core::Result<int> err = xfoil::core::Result<int>::fail(
      xfoil::core::Error::make(xfoil::core::ErrorCode::sizeMismatch,
                               "Matrix size invalid"));
  CHECK(err.isErr());
  CHECK_FALSE(err.isOk());
  CHECK(err.error().code == xfoil::core::ErrorCode::sizeMismatch);
  CHECK_EQ(err.error().message, std::string("Matrix size invalid"));
}

TEST(unit, ResultValueAccessInErrorStateThrows) {
  const xfoil::core::Result<int> err = xfoil::core::Result<int>::fail(
      xfoil::core::Error::make(xfoil::core::ErrorCode::unavailable, "x"));
  CHECK(throwsExactly<std::logic_error>([&err] { (void)err.value(); }));
}

TEST(unit, ResultErrorAccessInOkStateThrows) {
  const xfoil::core::Result<int> ok = xfoil::core::Result<int>::ok(1);
  CHECK(throwsExactly<std::logic_error>([&ok] { (void)ok.error(); }));
}

TEST(unit, ResultErrorCodeClassesExist) {
 // All general error classes are present.
  using xfoil::core::ErrorCode;
  const ErrorCode all[] = {
      ErrorCode::invalidArgument,
      ErrorCode::sizeMismatch,
      ErrorCode::outOfRange,
      ErrorCode::capacityExceeded,
      ErrorCode::invalidState,
      ErrorCode::numericalFailure,
      ErrorCode::nonConvergence,
      ErrorCode::ioError,
      ErrorCode::unavailable,
  };
  CHECK_EQ(sizeof(all) / sizeof(all[0]), std::size_t{9});
}

TEST(unit, StatusOkAndFail) {
  const xfoil::core::Status ok = xfoil::core::Status::ok();
  CHECK(ok.isOk());
  CHECK_FALSE(ok.isErr());

  const xfoil::core::Status err = xfoil::core::Status::fail(
      xfoil::core::Error::make(xfoil::core::ErrorCode::nonConvergence,
                               "no Convergence"));
  CHECK(err.isErr());
  CHECK_FALSE(err.isOk());
  CHECK(err.error().code == xfoil::core::ErrorCode::nonConvergence);
  CHECK_EQ(err.error().message, std::string("no Convergence"));

  // error access in the success state throws.
  CHECK(throwsExactly<std::logic_error>([&ok] { (void)ok.error(); }));
}

// End: tests/core/FoundationTests.cpp

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

// File: include/xfoil/core/Matrix.h
// Small, contiguous, 0-based matrix abstraction of the new cores.
// The matrix is an owning value type with contiguous
// row-major memory. Valid indices: row 0..rows-1,
// column 0..cols-1. No index-0 padding, no hidden Fortran semantics.
//
// Access and error behavior (binding):
// - at(row, col) is checked and throws std::out_of_range.
// - operator(row, col) is checked in debug (assert), unchecked in
// release.
// - The constructor checks rows*cols for overflow (std::length_error).
// - An empty matrix (rows == 0 or cols == 0) is permitted.
// - Neither at nor operator implicitly changes the size.
//
// The public API has no raw-pointer parameters and allocates
// exclusively via std::vector.

#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace xfoil::core {

// 0-based, owning matrix value type (row-major).
class Matrix {
 public:
  using value_type = double;
  using size_type = std::size_t;

  // Empty matrix (0 rows, 0 columns).
  Matrix() = default;

  // Matrix with rows rows and cols columns, all elements = value.
  // Throws std::length_error on rows*cols overflow.
  Matrix(size_type rows, size_type cols, value_type value = 0.0);

  [[nodiscard]]
  size_type rows() const noexcept {
    return rows_;
  }

  [[nodiscard]]
  size_type cols() const noexcept {
    return cols_;
  }

  [[nodiscard]]
  size_type size() const noexcept {
    return rows_ * cols_;
  }

  [[nodiscard]]
  bool empty() const noexcept {
    return rows_ == 0 || cols_ == 0;
  }

  // Checked access; throws std::out_of_range for an invalid index.
  value_type& at(size_type row, size_type col);
  const value_type& at(size_type row, size_type col) const;

  // Unchecked access (in debug via assert; in release unchecked).
  value_type& operator()(size_type row, size_type col) noexcept;
  const value_type& operator()(size_type row, size_type col) const noexcept;

  // Entire element range (row by row, contiguous).
  [[nodiscard]]
  std::span<value_type> data() noexcept {
    return std::span<value_type>(storage_);
  }

  [[nodiscard]]
  std::span<const value_type> data() const noexcept {
    return std::span<const value_type>(storage_);
  }

  // Row view: one row as std::span.
  [[nodiscard]]
  std::span<value_type> row(size_type row);
  [[nodiscard]]
  std::span<const value_type> row(size_type row) const;

 private:
  size_type rows_{0};
  size_type cols_{0};
  std::vector<value_type> storage_;
};

// Non-owning, 0-based matrix view over a contiguous,
// row-wise element range. The view does not own the memory and
// does not extend its lifetime; std::span stores a
// non-owning address and length. Callers must ensure that the
// underlying memory outlives the view and all row views derived from it.
// Construction exclusively via std::span (no construction
// from an unbound raw pointer).
class MatrixView {
 public:
  using value_type = double;
  using size_type = std::size_t;

  // Construction from a contiguous row-major span.
 // Checks exactly rows*cols == elements.size (including overflow check)
  // and otherwise throws std::invalid_argument.
  MatrixView(std::span<const value_type> elements, size_type rows,
             size_type cols);

  [[nodiscard]]
  size_type rows() const noexcept {
    return rows_;
  }

  [[nodiscard]]
  size_type cols() const noexcept {
    return cols_;
  }

  [[nodiscard]]
  size_type size() const noexcept {
    return rows_ * cols_;
  }

  [[nodiscard]]
  bool empty() const noexcept {
    return rows_ == 0 || cols_ == 0;
  }

  // Checked access; throws std::out_of_range for an invalid index.
  const value_type& at(size_type row, size_type col) const;

  // Unchecked access (in debug via assert; in release unchecked).
  const value_type& operator()(size_type row, size_type col) const noexcept;

  [[nodiscard]]
  std::span<const value_type> data() const noexcept {
    return data_;
  }

  // Row view: one row as std::span.
  [[nodiscard]]
  std::span<const value_type> row(size_type row) const;

 private:
  std::span<const value_type> data_;
  size_type rows_{0};
  size_type cols_{0};
};

}  // namespace xfoil::core

// End: include/xfoil/core/Matrix.h

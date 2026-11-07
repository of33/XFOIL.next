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

// File: src/core/foundation/Matrix.cpp
// Implementation of the 0-based matrix and view foundation.
// No numeric Fortran routine; only structure, 0-based access,
// overflow and size checks. Memory layout: row-major, contiguous.

#include "xfoil/core/Matrix.h"

#include <limits>
#include <stdexcept>

namespace xfoil::core {

namespace {

// Checks, whether rows * cols in size_type fits. Returns false for Via run.
bool productFits(std::size_t rows, std::size_t cols, std::size_t& out) {
  if (rows != 0 &&
      cols > std::numeric_limits<std::size_t>::max() / rows) {
    return false;
  }
  out = rows * cols;
  return true;
}

}  // namespace

Matrix::Matrix(size_type rows, size_type cols, value_type value)
    : rows_(rows), cols_(cols) {
  size_type total;
  if (!productFits(rows_, cols_, total)) {
    throw std::length_error(
        "xfoil::core::Matrix: rows * cols overflow");
  }
  storage_.assign(total, value);
}

Matrix::value_type& Matrix::at(size_type row, size_type col) {
  if (row >= rows_ || col >= cols_) {
    throw std::out_of_range("xfoil::core::Matrix: index out of range");
  }
  return storage_[row * cols_ + col];
}

const Matrix::value_type& Matrix::at(size_type row, size_type col) const {
  if (row >= rows_ || col >= cols_) {
    throw std::out_of_range("xfoil::core::Matrix: index out of range");
  }
  return storage_[row * cols_ + col];
}

Matrix::value_type& Matrix::operator()(size_type row, size_type col) noexcept {
  assert(row < rows_ && col < cols_);
  return storage_[row * cols_ + col];
}

const Matrix::value_type& Matrix::operator()(size_type row, size_type col)
    const noexcept {
  assert(row < rows_ && col < cols_);
  return storage_[row * cols_ + col];
}

std::span<Matrix::value_type> Matrix::row(size_type row) {
  if (row >= rows_) {
    throw std::out_of_range(
        "xfoil::core::Matrix: row index out of range");
  }
  return std::span<value_type>(storage_).subspan(row * cols_, cols_);
}

std::span<const Matrix::value_type> Matrix::row(size_type row) const {
  if (row >= rows_) {
    throw std::out_of_range(
        "xfoil::core::Matrix: row index out of range");
  }
  return std::span<const value_type>(storage_).subspan(row * cols_, cols_);
}

MatrixView::MatrixView(std::span<const value_type> elements, size_type rows,
                       size_type cols)
    : data_(elements), rows_(rows), cols_(cols) {
  size_type total;
  if (!productFits(rows_, cols_, total) || total != data_.size()) {
    throw std::invalid_argument(
        "xfoil::core::MatrixView: rows * cols unequal Span-Quantity");
  }
}

const MatrixView::value_type& MatrixView::at(size_type row, size_type col) const {
  if (row >= rows_ || col >= cols_) {
    throw std::out_of_range("xfoil::core::MatrixView: index out of range");
  }
  return data_[row * cols_ + col];
}

const MatrixView::value_type& MatrixView::operator()(size_type row,
                                                     size_type col) const noexcept {
  assert(row < rows_ && col < cols_);
  return data_[row * cols_ + col];
}

std::span<const MatrixView::value_type> MatrixView::row(size_type row) const {
  if (row >= rows_) {
    throw std::out_of_range(
        "xfoil::core::MatrixView: row index out of range");
  }
  return data_.subspan(row * cols_, cols_);
}

}  // namespace xfoil::core

// End: src/core/foundation/Matrix.cpp

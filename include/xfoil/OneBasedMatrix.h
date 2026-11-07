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

// file: include/xfoil/OneBasedMatrix.h
// Safe 1-based matrix container with contiguous storage.
// storage layout: row-major, contiguous, with
// reserved row 0 and reserved column 0. valid indices in this domain
// are 1..MaxRow and 1..MaxColumn. No operator[]; no heap allocation.

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>

namespace xfoil {

template<class T,
         std::size_t MaxRow,
         std::size_t MaxColumn>
class OneBasedMatrix {
 public:
  using value_type = T;
  using size_type = std::size_t;

  static_assert(MaxRow >= 1);
  static_assert(MaxColumn >= 1);

  [[nodiscard]]
  static constexpr size_type maxRow() noexcept {
    return MaxRow;
  }

  [[nodiscard]]
  static constexpr size_type maxColumn() noexcept {
    return MaxColumn;
  }

  [[nodiscard]]
  static constexpr size_type storageSize() noexcept {
    return (MaxRow + 1) * (MaxColumn + 1);
  }

  constexpr T& operator()(size_type row,
                          size_type column) noexcept {
    assert(row >= 1 && row <= MaxRow);
    assert(column >= 1 && column <= MaxColumn);
    return data_[offset(row, column)];
  }

  constexpr const T& operator()(size_type row,
                                size_type column) const noexcept {
    assert(row >= 1 && row <= MaxRow);
    assert(column >= 1 && column <= MaxColumn);
    return data_[offset(row, column)];
  }

  constexpr T& at(size_type row, size_type column) {
    if (row < 1 || row > MaxRow ||
        column < 1 || column > MaxColumn) {
      throw std::out_of_range(
          "OneBasedMatrix index out of range");
    }
    return data_[offset(row, column)];
  }

  constexpr const T& at(size_type row,
                        size_type column) const {
    if (row < 1 || row > MaxRow ||
        column < 1 || column > MaxColumn) {
      throw std::out_of_range(
          "OneBasedMatrix index out of range");
    }
    return data_[offset(row, column)];
  }

  constexpr void fill(const T& value) {
    data_.fill(value);
  }

  [[nodiscard]]
  constexpr T* data() noexcept {
    return data_.data();
  }

  [[nodiscard]]
  constexpr const T* data() const noexcept {
    return data_.data();
  }

 private:
  [[nodiscard]]
  static constexpr size_type offset(size_type row,
                                    size_type column) noexcept {
    return row * (MaxColumn + 1) + column;
  }

  std::array<T, (MaxRow + 1) * (MaxColumn + 1)> data_{};
};

}  // namespace xfoil

// End: include/xfoil/OneBasedMatrix.h

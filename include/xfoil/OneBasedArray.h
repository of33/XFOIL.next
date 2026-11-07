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

// file: include/xfoil/OneBasedArray.h
// Safe 1-based array container with contiguous storage.
// valid indices in this domain are 1..MaxIndex; index 0 is only
// reserved memory and is not accessible via operator[] or at().
// No operator[]: the 1-based access stays visible in the code.
// No heap allocation.

#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>

namespace xfoil {

template<class T, std::size_t MaxIndex>
class OneBasedArray {
 public:
  using value_type = T;
  using size_type = std::size_t;

  static_assert(MaxIndex >= 1);

  [[nodiscard]]
  static constexpr size_type maxIndex() noexcept {
    return MaxIndex;
  }

  [[nodiscard]]
  static constexpr size_type storageSize() noexcept {
    return MaxIndex + 1;
  }

  constexpr T& operator()(size_type index) noexcept {
    assert(index >= 1 && index <= MaxIndex);
    return data_[index];
  }

  constexpr const T& operator()(size_type index) const noexcept {
    assert(index >= 1 && index <= MaxIndex);
    return data_[index];
  }

  constexpr T& at(size_type index) {
    if (index < 1 || index > MaxIndex) {
      throw std::out_of_range("OneBasedArray index out of range");
    }
    return data_[index];
  }

  constexpr const T& at(size_type index) const {
    if (index < 1 || index > MaxIndex) {
      throw std::out_of_range("OneBasedArray index out of range");
    }
    return data_[index];
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
  std::array<T, MaxIndex + 1> data_{};
};

}  // namespace xfoil

// End: include/xfoil/OneBasedArray.h

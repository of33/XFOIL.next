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

// File: src/app/cli/NumericArguments.h
// Numerical argument parser for the OPER/VPAR menu.
// Orientation: fortran-src/userio.f::GETFLT and GETINT. Supports space
// and comma separation, leading sign, decimal point, exponents with
// e/E/d/D and several values. NaN, Inf, partial conversions,
// non-numeric leftover characters and overflowing values are rejected;
// an empty argument string returns an empty value list.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace xfoil::app::cli {

enum class NumericArgumentError {
  none,
  invalidToken,
  outOfRange
};

template<class T>
struct NumericArgumentResult {
  std::vector<T> values;
  NumericArgumentError error{NumericArgumentError::none};
  std::string diagnostic;

  [[nodiscard]]
  bool success() const noexcept {
    return error == NumericArgumentError::none;
  }
};

// Splits an argument string into double values. Diagnostics for an
// invalid token:
// "Invalid numeric argument: <token>", for overflow of the value:
// "Numeric argument out of range: <token>". Everything after a '!' is
// ignored, as in GETFLT/GETINT. maxCharacters defaults to the 6.996
// GETFLT/GETINT view of 256; A80 prompt readers pass 80.
[[nodiscard]]
NumericArgumentResult<double> parseRealArguments(
    std::string_view text,
    std::size_t maxCharacters = 256);

// Splits an argument string into int values. Fractions, exponents and
// non-digits are invalid; values outside the int range count as a
// range error. maxCharacters follows the same GETINT/A80 contract as
// parseRealArguments.
[[nodiscard]]
NumericArgumentResult<int> parseIntegerArguments(
    std::string_view text,
    std::size_t maxCharacters = 256);

}  // namespace xfoil::app::cli

// End: src/app/cli/NumericArguments.h

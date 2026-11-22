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

// File: src/io/NumericText.h
// Locale-independent decimal conversion for text codecs and app readers.
// Parsing accepts the canonical text syntax (sign, decimal point, E/e
// exponent and the Fortran-compatible D/d exponent). Formatting always uses
// a decimal point and never mutates the process locale.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace xfoil::io {

struct RealPrefix {
  double value{0.0};
  std::size_t consumed{0};
  bool rangeError{false};
};

// Parses one real prefix. Leading whitespace is not consumed. Returns
// consumed == 0 when no real starts at the beginning.
[[nodiscard]]
RealPrefix parseRealPrefix(std::string_view text,
                          bool allowFortranExponent = true) noexcept;

// Parses one complete real. Leading/trailing whitespace is invalid.
[[nodiscard]]
bool parseReal(std::string_view text, double& value,
               bool allowFortranExponent = true) noexcept;

// Parses one complete signed integer in base 10.
[[nodiscard]]
bool parseInteger(std::string_view text, long& value) noexcept;

// Locale-independent fixed/scientific formatting with printf-compatible
// precision after the decimal point.
[[nodiscard]]
std::string formatFixed(double value, int decimals);
[[nodiscard]]
std::string formatScientific(double value, int decimals);

}  // namespace xfoil::io

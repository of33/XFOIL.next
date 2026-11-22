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

#include "io/NumericText.h"

#include <charconv>
#include <iomanip>
#include <locale>
#include <sstream>
#include <system_error>

namespace xfoil::io {
namespace {

bool isExponentMarker(char character) noexcept {
  return character == 'd' || character == 'D';
}

}  // namespace

RealPrefix parseRealPrefix(std::string_view text,
                          bool allowFortranExponent) noexcept {
  RealPrefix result;
  std::size_t offset = 0;
  bool negative = false;
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    negative = text.front() == '-';
    offset = 1;
  }
  if (offset == text.size()) {
    return result;
  }

  // from_chars does not accept a leading '+' and does not know the Fortran
  // D exponent. Remove the sign and normalize D/d without changing length.
  std::string normalized(text.substr(offset));
  if (allowFortranExponent) {
    for (char& character : normalized) {
      if (isExponentMarker(character)) {
        character = 'e';
      }
    }
  }

  double value = 0.0;
  const char* const first = normalized.data();
  const char* const last = first + normalized.size();
  const std::from_chars_result parsed =
      std::from_chars(first, last, value, std::chars_format::general);
  if (parsed.ptr == first) {
    return result;
  }
  result.consumed =
      offset + static_cast<std::size_t>(parsed.ptr - first);
  if (parsed.ec == std::errc::result_out_of_range) {
    result.rangeError = true;
    return result;
  }
  if (parsed.ec != std::errc{}) {
    return result;
  }
  result.value = negative ? -value : value;
  return result;
}

bool parseReal(std::string_view text, double& value,
               bool allowFortranExponent) noexcept {
  const RealPrefix parsed = parseRealPrefix(text, allowFortranExponent);
  if (parsed.consumed != text.size() || parsed.rangeError) {
    return false;
  }
  value = parsed.value;
  return true;
}

bool parseInteger(std::string_view text, long& value) noexcept {
  if (text.empty()) {
    return false;
  }
  bool negative = false;
  if (text.front() == '+' || text.front() == '-') {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  if (text.empty()) {
    return false;
  }
  long parsedValue = 0;
  const std::from_chars_result parsed =
      std::from_chars(text.data(), text.data() + text.size(), parsedValue, 10);
  if (parsed.ec != std::errc{} ||
      parsed.ptr != text.data() + text.size()) {
    return false;
  }
  value = negative ? -parsedValue : parsedValue;
  return true;
}

std::string formatFixed(double value, int decimals) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(decimals) << value;
  return stream.str();
}

std::string formatScientific(double value, int decimals) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::scientific << std::setprecision(decimals) << value;
  return stream.str();
}

}  // namespace xfoil::io

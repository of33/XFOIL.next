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

// File: src/app/cli/NumericArguments.cpp
// Implementation of the numeric argument parser.
// Orientation: fortran-src/userio.f::GETFLT and GETINT. As there,
// everything after a '!' is ignored and tabs are treated as spaces.
// Unlike the original, faulty tokens are not silently degraded to
// "no argument" but reported with precise diagnostics.

#include "app/cli/NumericArguments.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <string>

namespace xfoil::app::cli {
namespace {

bool isSeparator(char character) {
  return character == ' ' || character == ',' || character == '\t';
}

bool isDigit(char character) {
  return character >= '0' && character <= '9';
}

// Removes the part from the first '!' as GETFLT/GETINT do and converts
// tabs to spaces. 6.996 GETFLT/GETINT consider the first 256 characters;
// the boundary applies before '!' and tab processing.
std::string prepareText(std::string_view text, std::size_t maxCharacters) {
  const std::size_t limit = std::min(text.size(), maxCharacters);
  std::string prepared;
  prepared.reserve(limit);
  for (std::size_t index = 0; index < limit; ++index) {
    const char character = text[index];
    if (character == '!') {
      break;
    }
    prepared.push_back(character == '\t' ? ' ' : character);
  }
  return prepared;
}

// Returns the next token boundary from position; tokens end on
// space or comma.
[[nodiscard]]
std::size_t tokenEnd(const std::string& text, std::size_t position) {
  while (position < text.size() && !isSeparator(text[position])) {
    ++position;
  }
  return position;
}

// Grammar of a double token: unsigned digit sequence with at most one
// decimal point and optional exponent e/E/d/D.
// The exponent letter is normalized to 'e' internally.
[[nodiscard]]
bool scanRealToken(const std::string& token, std::string& normalized) {
  normalized.clear();
  std::size_t position = 0;
  if (position < token.size() &&
      (token[position] == '+' || token[position] == '-')) {
    ++position;
  }

  std::size_t mantissaDigits = 0;
  bool seenDot = false;
  while (position < token.size()) {
    const char character = token[position];
    if (isDigit(character)) {
      ++mantissaDigits;
    } else if (character == '.' && !seenDot) {
      seenDot = true;
    } else {
      break;
    }
    ++position;
  }

  if (mantissaDigits == 0) {
    return false;
  }

  // Adopt sign and mantissa unchanged; strtod accepts
  // a leading '+'. The section from 0 contains both.
  normalized = token.substr(0, position);

  if (position < token.size()) {
    const char exponentMarker = token[position];
    if (exponentMarker != 'e' && exponentMarker != 'E' &&
        exponentMarker != 'd' && exponentMarker != 'D') {
      return false;
    }
    normalized.push_back('e');
    ++position;
    if (position < token.size() &&
        (token[position] == '+' || token[position] == '-')) {
      normalized.push_back(token[position]);
      ++position;
    }
    std::size_t exponentDigits = 0;
    while (position < token.size() && isDigit(token[position])) {
      normalized.push_back(token[position]);
      ++exponentDigits;
      ++position;
    }
    if (exponentDigits == 0) {
      return false;
    }
  }

  return position == token.size();
}

// Grammar of an integer token: optional sign, then only
// digits. Fractions and exponents are invalid.
[[nodiscard]]
bool scanIntegerToken(const std::string& token) {
  std::size_t position = 0;
  if (position < token.size() &&
      (token[position] == '+' || token[position] == '-')) {
    ++position;
  }
  if (position >= token.size()) {
    return false;
  }
  while (position < token.size()) {
    if (!isDigit(token[position])) {
      return false;
    }
    ++position;
  }
  return true;
}

template<class T>
[[nodiscard]]
NumericArgumentResult<T> makeInvalid(const std::string& token) {
  NumericArgumentResult<T> result;
  result.error = NumericArgumentError::invalidToken;
  result.diagnostic = "Invalid numeric argument: " + token;
  return result;
}

template<class T>
[[nodiscard]]
NumericArgumentResult<T> makeOutOfRange(const std::string& token) {
  NumericArgumentResult<T> result;
  result.error = NumericArgumentError::outOfRange;
  result.diagnostic = "Numeric argument out of range: " + token;
  return result;
}

}  // namespace

NumericArgumentResult<double> parseRealArguments(std::string_view text,
                                                std::size_t maxCharacters) {
  NumericArgumentResult<double> result;
  const std::string prepared = prepareText(text, maxCharacters);

  std::size_t position = 0;
  while (position < prepared.size()) {
    if (isSeparator(prepared[position])) {
      ++position;
      continue;
    }

    const std::size_t end = tokenEnd(prepared, position);
    const std::string token = prepared.substr(position, end - position);
    position = end;

    std::string normalized;
    if (!scanRealToken(token, normalized)) {
      return makeInvalid<double>(token);
    }

    errno = 0;
    char* parseEnd = nullptr;
    const double value = std::strtod(normalized.c_str(), &parseEnd);
    if (parseEnd == nullptr || *parseEnd != '\0') {
      return makeInvalid<double>(token);
    }
    if (errno == ERANGE || !std::isfinite(value)) {
      return makeOutOfRange<double>(token);
    }

    result.values.push_back(value);
  }

  return result;
}

NumericArgumentResult<int> parseIntegerArguments(std::string_view text,
                                                  std::size_t maxCharacters) {
  NumericArgumentResult<int> result;
  const std::string prepared = prepareText(text, maxCharacters);

  std::size_t position = 0;
  while (position < prepared.size()) {
    if (isSeparator(prepared[position])) {
      ++position;
      continue;
    }

    const std::size_t end = tokenEnd(prepared, position);
    const std::string token = prepared.substr(position, end - position);
    position = end;

    if (!scanIntegerToken(token)) {
      return makeInvalid<int>(token);
    }

    errno = 0;
    char* parseEnd = nullptr;
    const long long value = std::strtoll(token.c_str(), &parseEnd, 10);
    if (parseEnd == nullptr || *parseEnd != '\0') {
      return makeInvalid<int>(token);
    }
    if (errno == ERANGE || value < static_cast<long long>(INT_MIN) ||
        value > static_cast<long long>(INT_MAX)) {
      return makeOutOfRange<int>(token);
    }

    result.values.push_back(static_cast<int>(value));
  }

  return result;
}

}  // namespace xfoil::app::cli

// End: src/app/cli/NumericArguments.cpp

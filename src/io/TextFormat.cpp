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

#include "xfoil/io/TextFormat.h"

#include "xfoil/core/polar/PolarOps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xfoil::io {
namespace {

std::string stars(int width) { return std::string(static_cast<std::size_t>(width), '*'); }

std::string padLeft(const std::string& text, int width) {
  if (static_cast<int>(text.size()) >= width) return text;
  return std::string(static_cast<std::size_t>(width - static_cast<int>(text.size())), ' ') + text;
}

std::string padRight(std::string_view text, int width) {
  if (static_cast<int>(text.size()) >= width) return std::string(text);
  return std::string(text) + std::string(static_cast<std::size_t>(width - static_cast<int>(text.size())), ' ');
}

}  // namespace

std::string fortranF(int width, int decimals, double value) {
  (void)decimals;
  if (std::isnan(value)) return padLeft("NaN", width);
  if (std::isinf(value)) return padLeft(value > 0 ? "Infinity" : "-Infinity", width);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
  std::string text(buf);
  // Fortran Fw.0 writes the trailing decimal point.
  if (decimals == 0 && text.find('.') == std::string::npos) {
    text += '.';
  }
  if (static_cast<int>(text.size()) > width) {
    // Canonical: Fortran drops the leading zero if the field
    // would otherwise overflow (F8.6 of -0.014939 -> '-.014939').
    std::string trimmed = text;
    if (trimmed.rfind("-0.", 0) == 0) {
      trimmed = "-" + trimmed.substr(2);
    } else if (trimmed.rfind("0.", 0) == 0) {
      trimmed = trimmed.substr(1);
    }
    if (static_cast<int>(trimmed.size()) <= width) {
      return padLeft(trimmed, width);
    }
    // Canonical: an overflowing field is filled with '*'.
    return stars(width);
  }
  return padLeft(text, width);
}

std::string fortranE(int width, int decimals, double value) {
  if (std::isnan(value)) return padLeft("NaN", width);
  if (std::isinf(value)) return padLeft(value > 0 ? "Infinity" : "-Infinity", width);
  // Canonical Fortran E: mantissa in the range [0.1,1) (one digit after
  // the decimal point, exponent +1 versus C normalization),
  // exponent with sign and at least two digits.
  const bool negative = std::signbit(value);
  const double magnitude = std::fabs(value);
  int exponent = 0;
  double mantissa = magnitude;
  if (magnitude != 0.0) {
    exponent = static_cast<int>(std::floor(std::log10(magnitude))) + 1;
    mantissa = magnitude / std::pow(10.0, exponent);
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, mantissa);
  std::string mantissaText(buf);
  // Rounding to 1.00..0 leads to reduction to 0.10..0 with exponent+1
  // (canonical, e.g. E10.4 of 0.99996 -> 0.1000E+01).
  if (mantissaText == std::string("1.") + std::string(static_cast<std::size_t>(decimals), '0')) {
    mantissaText = std::string("0.") + "1" +
                   std::string(static_cast<std::size_t>(decimals - 1), '0');
    exponent += 1;
  }
  if (negative) mantissaText.insert(mantissaText.begin(), '-');
  // Fortran writes the leading zero only for a value without a sign.
  if (mantissaText.rfind("-0.", 0) == 0) {
    mantissaText.erase(1, 1);
  }
  const char expSign = exponent < 0 ? '-' : '+';
  const int absExponent = exponent < 0 ? -exponent : exponent;
  char expBuf[16];
  std::snprintf(expBuf, sizeof(expBuf), "%c%02d", expSign, absExponent);
  std::string formatted = mantissaText + "E" + expBuf;
  if (static_cast<int>(formatted.size()) > width) return stars(width);
  return padLeft(formatted, width);
}

std::string fortranG(int width, int digits, double value) {
  // Fortran Gw.d following the behavior verified with gfortran (Step
  // 17d1, AirfoilWriter): after rounding to d significant digits, fixed-format
  // output is used if the decimal exponent EE (value = m*10^EE,
  // 0.1 <= m < 1) lies between 0 and d; the fixed format reserves the
  // last four field characters as if the exponent were omitted.
  // Otherwise the scientific format "0.ddddE+ee" with a two-digit
  // exponent appears. The sign of -0.0 is preserved (gfortran).
  if (!std::isfinite(value)) return stars(width);
  if (digits < 1 || digits > 17) return stars(width);

  const bool negative = std::signbit(value);
  const double magnitude = std::fabs(value);
  char buffer[64];

  if (magnitude == 0.0) {
    std::snprintf(buffer, sizeof buffer, "%.*f", digits - 1, 0.0);
    std::string text = negative ? "-" + std::string(buffer) : std::string(buffer);
    if (static_cast<int>(text.size()) > width - 4) return stars(width);
    return padLeft(text, width - 4) + "    ";
  }

  std::snprintf(buffer, sizeof buffer, "%.*e", digits - 1, magnitude);
  const std::string normalized(buffer);
  const std::size_t exponentPosition = normalized.find('e');
  const double mantissa =
      std::strtod(normalized.substr(0, exponentPosition).c_str(), nullptr);
  const int baseExponent =
      std::atoi(normalized.substr(exponentPosition + 1).c_str());
  const int ee = baseExponent + 1;

  std::string sign = negative ? "-" : "";
  if (ee >= 0 && ee <= digits) {
    const int fractionDigits = digits - ee;
    if (fractionDigits > 0) {
      std::snprintf(buffer, sizeof buffer, "%.*f", fractionDigits, magnitude);
    } else {
      std::snprintf(buffer, sizeof buffer, "%.0f.", magnitude);
    }
    const std::string text = sign + buffer;
    if (static_cast<int>(text.size()) > width - 4) return stars(width);
    return padLeft(text, width - 4) + "    ";
  }

  std::snprintf(buffer, sizeof buffer, "%.*fE%c%02d", digits, mantissa / 10.0,
                ee >= 0 ? '+' : '-', ee >= 0 ? ee : -ee);
  const std::string text = sign + buffer;
  if (static_cast<int>(text.size()) > width) return stars(width);
  return padLeft(text, width);
}

std::string fortranI(int width, long value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%ld", value);
  std::string text(buf);
  if (static_cast<int>(text.size()) > width) return stars(width);
  return padLeft(text, width);
}

std::string fortranL2(bool value) { return value ? " T" : " F"; }

// An edit descriptor: left-aligned, padded to width; longer text is
// not truncated (canonical Fortran semantics).
std::string fortranA(int width, std::string_view text) {
  if (static_cast<int>(text.size()) >= width) {
    return std::string(text);
  }
  return padRight(text, width);
}

std::string stripBlanks(std::string_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && text[begin] == ' ') ++begin;
  while (end > begin && text[end - 1] == ' ') --end;
  return std::string(text.substr(begin, end - begin));
}

FloatParseResult parseFloatList(std::string_view line, std::size_t maxCount) {
  FloatParseResult result;
  // GETFLT: only the first 128 characters, '!' terminates the line,
  // tabs become spaces, an appended " ," acts as sentinel.
  const std::size_t ilen = std::min<std::size_t>(line.size(), 128);
  std::string rec(line.substr(0, ilen));
  const std::size_t bang = rec.find('!');
  if (bang != std::string::npos) rec = rec.substr(0, bang);
  std::replace(rec.begin(), rec.end(), '\t', ' ');
  rec += " ,";
  const std::size_t scanLimit = std::min(rec.size(), ilen);

  // First pass: count numbers (GETFLT pass 1).
  int count = 0;
  std::size_t k = 0;
  const std::size_t len = rec.size();
  for (std::size_t pass = 0; pass < len && k < scanLimit; ++pass) {
    const std::size_t space = rec.find(' ', k);
    const std::size_t comma = rec.find(',', k);
    if (k == space) {
      ++k;
      continue;
    }
    if (k == comma) {
      ++count;
      ++k;
      continue;
    }
    // A number begins at k: GETFLT jumps past the next separator.
    ++count;
    const std::size_t sep = std::min(space, comma);
    k = (sep == std::string::npos) ? len : sep + 1;
  }
  if (maxCount != 0 && count > static_cast<int>(maxCount)) count = static_cast<int>(maxCount);

  // Second pass: scan numbers (list-directed).
  k = 0;
  for (int n = 0; n < count; ++n) {
    while (k < len && (rec[k] == ' ' || rec[k] == ',')) ++k;
    std::size_t end = k;
    while (end < len && rec[end] != ' ' && rec[end] != ',') ++end;
    const std::string token = std::string(rec.substr(k, end - k));
    k = end;
    if (token.empty()) {
      result.error = true;
      break;
    }
    // Fortran list-directed: D exponents are allowed, fixed kernel.
    std::string normalized = token;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(normalized.begin(), normalized.end(), 'd', 'e');
    char* stop = nullptr;
    const double value = std::strtod(normalized.c_str(), &stop);
    if (stop == normalized.c_str() || *stop != '\0') {
      result.error = true;
      break;
    }
    result.values.push_back(value);
  }
  result.extracted = static_cast<int>(result.values.size());
  if (result.extracted != count) result.error = true;
  return result;
}

std::string modifyNameWithVersion(std::string_view name, int kdel, int kmod0) {
  // Thin adapter: the canonical NAMMOD kernel lives in
  // core::polar::applyVersionTag. On a READ error in the bracket contents
 // Fortran returns the unchanged NAMDEF (prompt path).
  const auto result = core::polar::applyVersionTag(std::string(name), kdel, kmod0);
  if (result.isOk()) {
    return result.value();
  }
  return stripBlanks(name);
}

}  // namespace xfoil::io

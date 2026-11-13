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

// File: src/io/DxfExport.cpp
// DXFOUT R12 byte projection (fortran-src/xfoil-6.996/xfoil.f:1710-1829).
// Locale-independent decimal formatting; canonical LF output.

#include "xfoil/io/DxfExport.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <system_error>

namespace xfoil::io {
namespace {

std::string padLeft(std::string text, std::size_t width) {
  if (text.size() >= width) {
    return text;
  }
  return std::string(width - text.size(), ' ') + text;
}

std::string stars(std::size_t width) {
  return std::string(width, '*');
}

std::string fixedField(int width, int decimals, double value) {
  char buffer[128];
  const auto converted = std::to_chars(
      buffer, buffer + sizeof(buffer), value, std::chars_format::fixed,
      decimals);
  if (converted.ec != std::errc{}) {
    return stars(static_cast<std::size_t>(width));
  }
  std::string text(buffer, converted.ptr);
  if (decimals == 0 && text.find('.') == std::string::npos) {
    text += '.';
  }
  const std::size_t fieldWidth = static_cast<std::size_t>(width);
  if (text.size() > fieldWidth) {
    if (text.rfind("-0.", 0) == 0) {
      text = "-" + text.substr(2);
    } else if (text.rfind("0.", 0) == 0) {
      text = text.substr(1);
    }
    if (text.size() <= fieldWidth) {
      return padLeft(text, fieldWidth);
    }
    return stars(fieldWidth);
  }
  return padLeft(text, fieldWidth);
}

std::string headerValue(double value) {
  return " " + fixedField(10, 4, value);
}

std::string coordinateValue(double value) {
  std::string text = fixedField(14, 6, value);
  const std::size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(' ');
  return text.substr(first, last - first + 1);
}

void append(std::string& out, std::string_view text) {
  out += text;
  out += '\n';
}

}  // namespace

core::Result<std::string> dxfR12TextChecked(
    std::span<const double> x, std::span<const double> y) {
  if (x.size() != y.size()) {
    return core::Result<std::string>::fail(core::Error::make(
        core::ErrorCode::sizeMismatch, "DXF X/Y arrays differ in length"));
  }
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (!std::isfinite(x[i]) || !std::isfinite(y[i])) {
      return core::Result<std::string>::fail(core::Error::make(
          core::ErrorCode::invalidArgument,
          "DXF contour contains a non-finite coordinate"));
    }
  }

  double xmin = 0.0;
  double xmax = 0.0;
  double ymin = 0.0;
  double ymax = 0.0;
  if (!x.empty()) {
    xmin = x[0];
    xmax = x[0];
    ymin = y[0];
    ymax = y[0];
    for (std::size_t i = 1; i < x.size(); ++i) {
      xmin = std::min(xmin, x[i]);
      xmax = std::max(xmax, x[i]);
      ymin = std::min(ymin, y[i]);
      ymax = std::max(ymax, y[i]);
    }
    const double alen = std::max(xmax - xmin, ymax - ymin);
    xmin -= 0.1 * alen;
    xmax += 0.1 * alen;
    ymin -= 0.1 * alen;
    ymax += 0.1 * alen;
  }

  std::string out;
  out.reserve(16000U + 64U * x.size());

  append(out, "999");
  append(out, "\"DXF R12 Output\" (www.mydxf.blogspot.com)");
  append(out, " 0 ");
  append(out, "SECTION");
  append(out, " 2 ");
  append(out, "HEADER");
  append(out, " 9 ");
  append(out, "$ACADVER");
  append(out, " 1 ");
  append(out, "AC1009");
  append(out, " 9 ");
  append(out, "$EXTMIN");
  append(out, " 10 ");
  append(out, headerValue(xmin));
  append(out, " 20 ");
  append(out, headerValue(ymin));
  append(out, " 9 ");
  append(out, "$EXTMAX");
  append(out, " 10 ");
  append(out, headerValue(xmax));
  append(out, " 20 ");
  append(out, headerValue(ymax));
  append(out, " 0 ");
  append(out, "ENDSEC");
  append(out, " 0 ");
  append(out, "SECTION");
  append(out, " 2 ");
  append(out, "ENTITIES");

  for (std::size_t i = 0; i + 1 < x.size(); ++i) {
    append(out, "0");
    append(out, "LINE");
    append(out, "8");
    append(out, "Layer 1");
    append(out, "10");
    append(out, coordinateValue(x[i]));
    append(out, "20");
    append(out, coordinateValue(y[i]));
    append(out, "11");
    append(out, coordinateValue(x[i + 1]));
    append(out, "21");
    append(out, coordinateValue(y[i + 1]));
  }

  append(out, " 0 ");
  append(out, "ENDSEC");
  append(out, " 0 ");
  out += "EOF";
  return core::Result<std::string>::ok(std::move(out));
}

std::string dxfR12Text(std::span<const double> x,
                       std::span<const double> y) {
  const auto encoded = dxfR12TextChecked(x, y);
  return encoded.isOk() ? encoded.value() : std::string{};
}

}  // namespace xfoil::io

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

#ifndef XFOIL_IO_TEXTFORMAT_H
#define XFOIL_IO_TEXTFORMAT_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace xfoil::io {

// Emulation of the canonical Fortran output formats (gfortran compatible),
// as used in iopol.f, xoper.f, xfoil.f and xmdes.f.
// For unrepresentable values the canonical '*' fill rule applies.
std::string fortranF(int width, int decimals, double value);
std::string fortranE(int width, int decimals, double value);
std::string fortranG(int width, int digits, double value);
std::string fortranI(int width, long value);
std::string fortranA(int width, std::string_view text);
std::string fortranL2(bool value);

// Removes leading and trailing spaces (STRIP semantics).
std::string stripBlanks(std::string_view text);

// GETFLT semantics (userio.f:351): reads at most maxCount reals from a
// line; maxCount == 0 means all. Separators are spaces and commas,
// a '!' terminates the line, tabs become spaces.
struct FloatParseResult {
  std::vector<double> values;
  bool error = false;
  int extracted = 0;
};
FloatParseResult parseFloatList(std::string_view line, std::size_t maxCount);

// NAMMOD kernel (xoper.f:1831): bracket version counting without prompt path.
// Returns the canonical NAMDEF (result for empty user input).
std::string modifyNameWithVersion(std::string_view name, int kdel, int kmod0);

}  // namespace xfoil::io

#endif  // XFOIL_IO_TEXTFORMAT_H

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

#ifndef XFOIL_IO_DEFAULTSCODEC_H
#define XFOIL_IO_DEFAULTSCODEC_H

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xfoil::io {

// Positional values of the canonical xfoil.def (GETDEF/WRTDEF, xfoil.f).
// Plot fields are read in position order but are not interpreted
// as product state.
struct DefaultsValues {
  int npan = 0;
  double cvpar = 0.0;
  double cterat = 0.0;
  double ctrrat = 0.0;
  double xsref1 = 0.0;
  double xsref2 = 0.0;
  double xpref1 = 0.0;
  double xpref2 = 0.0;
  double size = 0.0;
  double plotar = 0.0;
  double ch = 0.0;
  double scrnfr = 0.0;
  double xpage = 0.0;
  double ypage = 0.0;
  double xmarg = 0.0;
  double ymarg = 0.0;
  bool lcolor = false;
  bool lcurs = false;
  double cpmax = 0.0;
  double cpmin = 0.0;
  double cpdel = 0.0;
  double xofair = 0.0;
  double facair = 0.0;
  double uprwt = 0.0;
  std::array<double, 3> clpol{};
  std::array<double, 3> cdpol{};
  std::array<double, 3> alpol{};
  std::array<double, 3> cmpol{};
  int matyp = 0;
  double minf1 = 0.0;
  double vaccel = 0.0;
  int retyp = 0;
  double rmill = 0.0;
  std::array<double, 2> acrit{};
  std::array<double, 2> xtrip{};
};

enum class DefaultsError {
  kNone,
  kBadLine,
  // File ends before the last expected line. Canonical code does not
  // catch this case with READ(...,ERR=80); without an END= specification EOF is a
  // runtime error (verified with gfortran). The parser distinguishes
  // it so callers can replicate the canonical abort.
  kEndOfFile,
};

// Structured partial result: canonically, already read values remain
// effective if a later line is invalid.
struct DefaultsParseResult {
  DefaultsValues values;
  int linesRead = 0;
  bool complete = false;
  DefaultsError error = DefaultsError::kNone;
  int firstBadLine = 0;
};

// Parses the text of an xfoil.def (14 positional lines).
DefaultsParseResult parseDefaults(std::string_view text);

// WRTDEF semantics. LCOLOR is canonically derived from idevrp == 4.
std::string writeDefaults(const DefaultsValues& values, int idevrp);

}  // namespace xfoil::io

#endif  // XFOIL_IO_DEFAULTSCODEC_H

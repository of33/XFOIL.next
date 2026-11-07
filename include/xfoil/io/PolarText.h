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

#ifndef XFOIL_IO_POLARTEXT_H
#define XFOIL_IO_POLARTEXT_H

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace xfoil::io {

// Canonical PINDEX indices (fortran-src/PINDEX.INC).
inline constexpr int kIal = 1;
inline constexpr int kIcl = 2;
inline constexpr int kIcd = 3;
inline constexpr int kIcm = 4;
inline constexpr int kIcw = 5;
inline constexpr int kIcv = 6;
inline constexpr int kIcp = 7;
inline constexpr int kIma = 8;
inline constexpr int kIre = 9;
inline constexpr int kIch = 10;
inline constexpr int kImc = 11;
inline constexpr int kIcdh = 12;
inline constexpr int kIcmdot = 13;
inline constexpr int kIptot = 13;

inline constexpr int kJnc = 1;
inline constexpr int kJtp = 2;
inline constexpr int kJtn = 3;
inline constexpr int kJti = 4;
inline constexpr int kJptot = 4;

inline constexpr int kNax = 800;
inline constexpr int kNfx = 128;
inline constexpr int kNpx = 12;

// A polar point: 13 coefficient columns plus side values
// (sides 1/2, per JPTOT quantities).
struct PolarPoint {
  std::array<double, kIptot> cpol{};
  std::array<std::array<double, kJptot>, 2> side{};
};

// Parse result with canonical metadata and column layout.
struct ParsedPolar {
  bool error = false;
  int errorLine = 0;
  std::string code;
  double version = 0.0;
  std::string name;
  int nbl = 1;
  int iretyp = 0;
  int imatyp = 0;
  double mach1 = 0.0;
  double reyn1 = 0.0;
  std::array<double, 2> acrit{};
  std::array<double, 2> xtrip{};
  double ptrat = 0.0;
  double etap = 0.0;
  std::array<bool, kIptot> columnPresent{};
  std::array<bool, kJptot> sidePresent{};
  std::vector<int> ipolOrder;  // PINDEX columns in file order
  std::vector<int> jpolOrder;  // JP quantities in file order
  std::vector<PolarPoint> points;
};

// POLREAD semantics on text. maxPoints corresponds to NAX; an overflow is
// reported as a structured error without partial mutation.
ParsedPolar readPolarText(std::string_view text, int maxPoints = kNax);

// Write request for POLWRIT. ia1/ia2 are 1-based; ia1 == 0 and
// ia2 == -1 write only the header (PLRINI new-file path).
struct PolarWriteRequest {
  bool lhead = true;
  int ia1 = 1;
  int ia2 = 0;
  std::vector<int> ipol;  // 1-based PINDEX columns
  std::vector<int> jpol;  // 1-based JPTOT quantities
  std::vector<PolarPoint> points;
  std::string code = "XFOIL";
  double version = 6.99;
  std::string name;
  int nbl = 1;
  int iretyp = 1;
  int imatyp = 1;
  double mach1 = 0.0;
  double reyn1 = 0.0;
  std::array<double, 2> acrit{};
  std::array<double, 2> xtrip{};
  double ptrat = 0.0;
  double etap = 0.0;
};

// POLWRIT semantics: returns the exact file bytes including
// line endings. Overwrite queries (LQUERY) do not belong here.
std::string writePolarText(const PolarWriteRequest& request);

// POLREF semantics: four blocks with a 999.0 sentinel.
struct ParsedReferencePolar {
  bool error = false;
  std::string label;
  std::array<int, 4> blockSizes{};
  std::array<std::vector<std::array<double, 2>>, 4> blocks;
};
ParsedReferencePolar readReferencePolarText(std::string_view text,
                                            int maxPerBlock = kNfx);

}  // namespace xfoil::io

#endif  // XFOIL_IO_POLARTEXT_H

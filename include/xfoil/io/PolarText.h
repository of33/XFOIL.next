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

#include "xfoil/core/Result.h"
#include "xfoil/core/polar/PolarTypes.h"

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
inline constexpr int kNpx = 20;

inline constexpr int kMaxPolarElements =
    static_cast<int>(core::polar::maxPolarElements);

enum class PolarFieldKind { base, sideTop, sideBottom, element };

struct PolarField {
  PolarFieldKind kind{PolarFieldKind::base};
  int baseIndex{0};
  int sideIndex{0};
  int elementIndex{0};
  core::polar::PolarElementQuantity elementQuantity{
      core::polar::PolarElementQuantity::cx};

  [[nodiscard]] bool operator==(const PolarField&) const = default;
};

struct PolarLayout {
  int nbl{1};
  std::vector<PolarField> fields;

  [[nodiscard]] bool operator==(const PolarLayout&) const = default;
};

enum class PolarReadCompatibility {
  canonical,
  xfoil6996BrokenSideOrder,
};

// A polar point: 13 coefficient columns plus side values
// (up to 2*maxPolarElements sides, per JPTOT quantities).
struct PolarPoint {
  std::array<double, kIptot> cpol{};
  std::array<std::array<double, kJptot>, 2 * kMaxPolarElements> side{};
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
  PolarLayout layout;
  core::polar::PolarElementSelection elementSelection;
  std::vector<core::polar::PolarElementRow> elementData;
  std::vector<PolarPoint> points;
  std::string errorMessage;
};

// POLREAD semantics on text. maxPoints corresponds to NAX; an overflow is
// reported as a structured error without partial mutation.
ParsedPolar readPolarText(
    std::string_view text, int maxPoints = kNax,
    PolarReadCompatibility compatibility = PolarReadCompatibility::canonical);

// Write request for POLWRIT. ia1/ia2 are 1-based; ia1 == 0 and
// ia2 == -1 write only the header (PLRINI new-file path).
struct PolarWriteRequest {
  bool lhead = true;
  int ia1 = 1;
  int ia2 = 0;
  std::vector<int> ipol;  // 1-based PINDEX columns
  std::vector<int> jpol;  // 1-based JPTOT quantities
  core::polar::PolarElementSelection elementSelection;
  std::vector<PolarPoint> points;
  std::vector<core::polar::PolarElementRow> elementData;
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

// Builds the one canonical layout used by writer, reader and append check.
[[nodiscard]] core::Result<PolarLayout> makePolarLayout(
    const PolarWriteRequest& request);

// POLWRIT semantics: returns the exact file bytes including
// line endings. Overwrite queries (LQUERY) do not belong here.
[[nodiscard]] core::Result<std::string> writePolarTextChecked(
    const PolarWriteRequest& request);

// Compatibility wrapper for existing callers. Invalid requests produce an
// empty string; new code should use writePolarTextChecked.
std::string writePolarText(const PolarWriteRequest& request);

struct PolarAppendCheck {
  bool compatible{false};
  std::string message;
};

// Parses the existing canonical file and compares its layout and element
// count with the requested writer layout. Does not access the filesystem.
[[nodiscard]] PolarAppendCheck checkPolarAppend(
    std::string_view existing, const PolarWriteRequest& request,
    PolarReadCompatibility compatibility = PolarReadCompatibility::canonical);

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

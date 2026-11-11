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

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <string>

namespace testing {

// S12 target migration for old 6.99 dump references: only the 8-byte
// VERSION field of record 1 changes (6.99 -> 6.996). Every other byte of
// the unchanged reference remains part of the exact comparison.
inline std::string withS12DumpHeaderVersion(std::string bytes) {
  constexpr std::size_t kRecordMarker = 4;
  constexpr std::size_t kNameAndCode = 48 + 8;
  constexpr std::size_t kVersionOffset = kRecordMarker + kNameAndCode;
  if (bytes.size() < kVersionOffset + sizeof(double)) {
    return bytes;
  }
  const std::uint64_t bits = std::bit_cast<std::uint64_t>(6.996);
  for (std::size_t i = 0; i < sizeof(bits); ++i) {
    bytes[kVersionOffset + i] =
        static_cast<char>((bits >> (8 * i)) & 0xffu);
  }
  return bytes;
}

}  // namespace testing

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

#ifndef XFOIL_IO_ORRSOMMERFELDCODEC_H
#define XFOIL_IO_ORRSOMMERFELDCODEC_H

#include <cstdint>
#include <optional>
#include <string>

#include "xfoil/core/inverse/OrrSommerfeldMap.h"

namespace xfoil::io {

enum class OsmapError {
  kNone,
  kTruncated,
  kMarkerMismatch,
  kTooLarge,
  kBadDimensions,
};

struct OsmapDecodeResult {
  std::optional<core::inverse::OrrSommerfeldRaw> raw;
  OsmapError error = OsmapError::kNone;
  std::string message;
};

// Decodes the canonical osmap.dat (Fortran unformatted, REAL*4,
// little-endian) into the raw table value type of the cores. No
// path or environment logic, no interpolation.
OsmapDecodeResult decodeOrrSommerfeldRaw(const std::uint8_t* data, std::size_t size);

}  // namespace xfoil::io

#endif  // XFOIL_IO_ORRSOMMERFELDCODEC_H

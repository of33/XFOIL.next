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

// File: include/xfoil/io/DxfExport.h
// S14 DXF R12 text projection (XFOIL 6.996 DXFOUT).
// Pure encoder: ordered 2D contour in, canonical file bytes out. No file
// open, command, prompt, overwrite or OutputPathPolicy logic.

#pragma once

#include <span>
#include <string>

#include "xfoil/core/Result.h"

namespace xfoil::io {

// Encodes the current contour as an XFOIL 6.996-compatible DXF R12 file.
// X and Y are used unchanged; N points produce N-1 LINE entities without a
// closing segment. Output uses LF only and ends with an unterminated EOF
// token. An empty contour produces the canonical zero-extent skeleton.
// Non-finite coordinates and unequal array lengths are rejected without
// partial output.
[[nodiscard]]
core::Result<std::string> dxfR12TextChecked(
    std::span<const double> x, std::span<const double> y);

// Unchecked convenience wrapper: returns an empty string on validation error.
std::string dxfR12Text(std::span<const double> x,
                       std::span<const double> y);

}  // namespace xfoil::io

// End: include/xfoil/io/DxfExport.h

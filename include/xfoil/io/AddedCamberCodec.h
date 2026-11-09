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

// File: include/xfoil/io/AddedCamberCodec.h
// Pure IO codec for CAMB-RDAC (xtcam.f 330-372): list-directed
// parse of x/c,y/c pairs after skipping the header line.
// No session or CLI dependency; no geometry numerics.
#ifndef XFOIL_IO_ADDEDCAMBERCODEC_H_
#define XFOIL_IO_ADDEDCAMBERCODEC_H_

#include <cstddef>
#include <iosfwd>
#include <vector>

#include "xfoil/core/Limits.h"

namespace xfoil::io {

// NTX = 2*IBX = 8*IQX = 4808 (XFOIL.INC parameter). Fortran reads
// at most NTX pairs (DO K = 1, NTX).
inline constexpr std::size_t kAddedCamberPointLimit =
    xfoil::core::camberTableCapacity;

struct AddedCamberPoint {
  double x{0.0};
  double y{0.0};
};

// Result of the pure read process. Fortran leaves the READ loop on
// ERR or END and uses the points read up to that point
// (partial-mutation contract of the handlers); a parse error in the
// middle of the file keeps the already read pairs.
struct AddedCamberFileData {
  std::vector<AddedCamberPoint> points;
  bool truncatedByParseError{false};
  bool truncatedByPointLimit{false};
};

// Fortran: READ(LU,1000) LINE (discard header), then
// READ(LU,*,ERR=30,END=30) XX,YY per iteration. Empty records are
// skipped list-directed; a READ pair may span records;
// the remaining tokens of a record after a complete pair are
// ignored.
AddedCamberFileData decodeAddedCamberFile(std::istream& in);

}  // namespace xfoil::io

#endif  // XFOIL_IO_ADDEDCAMBERCODEC_H_

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

// File: include/xfoil/io/MdesInverseFileCodec.h
// Pure IO codecs for the MDES file contracts GETVOV (READ,
// xmdes.f 1769-1826) and CNDUMP (DUMP, xmdes.f 1747-1766).
// No session, CLI or filesystem dependency; no
// inverse numerics in the codec.

#ifndef XFOIL_IO_MDESINVERSEFILECODEC_H_
#define XFOIL_IO_MDESINVERSEFILECODEC_H_

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace xfoil::io {

// IQX = 370 (XFOIL.INC 23): GETVOV reads at most IQX pairs
// (DO K = 1, IQX).
inline constexpr std::size_t kGetvovPointLimit = 370;

// An (s, V/Vinf) pair in raw form (arc length s unnormalized,
// V/Vinf dimensionless). The normalization s -> 1-(s-SSTART)/SSPAN is
// part of the core kernels, not of the codec.
struct GetvovPoint {
  double s{0.0};
  double vov{0.0};
};

// Result of the pure read process. Fortran leaves the READ loop
// on END (EOF) or ERR (parse error) and uses the completely
// read pairs up to that point (canonical partial-mutation contract,
// xmdes.f 1784-1787).
struct GetvovFileData {
  std::vector<GetvovPoint> points;
  bool truncatedByParseError{false};
  bool truncatedByPointLimit{false};
};

// Fortran: READ(LU,*,END=11,ERR=99) W1(K), W2(K) per iteration, no
// header line. Empty records are skipped list-directed; a
// pair may span records; the remaining tokens of a record after the
// last complete pair are ignored. D-/d exponents are
// treated as in the other list-directed codecs.
GetvovFileData decodeGetvovFile(std::istream& in);

// A CNDUMP line: m, real CN(m), imaginary CN(m), real PIQ(m+1), imaginary PIQ(m+1).
struct CnDumpRow {
  int m{0};
  double cnReal{0.0};
  double cnImag{0.0};
  double piqReal{0.0};
  double piqImag{0.0};
};

// Format 7000 (xmdes.f 1763): 1X,I3,4F11.6, one LF per line.
std::string cnDumpText(const std::vector<CnDumpRow>& rows);

}  // namespace xfoil::io

#endif  // XFOIL_IO_MDESINVERSEFILECODEC_H_

// End: include/xfoil/io/MdesInverseFileCodec.h

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

#ifndef XFOIL_IO_DUMPCODEC_H
#define XFOIL_IO_DUMPCODEC_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "xfoil/core/export_data/ExportData.h"

namespace xfoil::io {

// Canonical Fortran unformatted format of the polar dump files
// (PLXINI/PLXADD/BOTTOMX). The canonical reference binary was built with
// -fdefault-real-8, so REAL fields are 8 bytes wide;
// REAL(4) layout is additionally supported.
enum class DumpRealWidth { k4 = 4, k8 = 8 };

// S12 target: the frozen 6.996 patched oracle writes this code version in
// the dump header. This is a dump-record version, not the product version.
inline constexpr double kDumpHeaderVersion = 6.996;

struct DumpHeader {
  std::string name;
  std::string code;
  double version = 0.0;
  double mach = 0.0;
  double reynMillions = 0.0;
  double acritTop = 0.0;
  double acritBottom = 0.0;
  int matyp = 0;
  int retyp = 0;
  int iix = 0;
  int ilex = 0;
  int itex = 0;
  int iibx = 0;
  std::vector<std::pair<double, double>> geometry;
};

struct DumpSide {
  std::vector<double> x;
  std::vector<double> cp;
  std::vector<double> thet;
  std::vector<double> dstr;
  std::vector<double> cf;
  std::vector<double> ctau;
};

struct DumpPoint {
  double alfaRad = 0.0;
  double cl = 0.0;
  double cdTot = 0.0;
  double zero = 0.0;
  double cm = 0.0;
  double xtrTop = 0.0;
  double xtrBottom = 0.0;
  int nside1 = 0;
  int nside2 = 0;
  int iblte1 = 0;
  int iblte2 = 0;
  DumpSide sides[2];
};

struct DumpFile {
  DumpHeader header;
  std::vector<DumpPoint> points;
};

enum class DumpError {
  kNone,
  kTruncated,
  kMarkerMismatch,
  kRecordLength,
  kTooLarge,
  kUnexpectedRecord,
  kMissingRecord,
};

struct DumpDecodeResult {
  std::optional<DumpFile> file;
  DumpError error = DumpError::kNone;
  std::string message;
};

// Decodes a complete dump file (including leading and trailing
// 4-byte record markers, little-endian).
DumpDecodeResult decodeDump(const std::uint8_t* data, std::size_t size,
                            DumpRealWidth realWidth = DumpRealWidth::k8);

// Mechanical, formula-free adapter: maps the domain-specific projected
// core record (B) onto the pure codec record model.
DumpPoint toDumpPoint(const core::export_data::PlxaddPoint& point);

// Checked encoder: validates field counts, element counts, record sizes
// and the uint32 record-marker contract before producing bytes.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encodeDumpHeaderChecked(
    const DumpHeader& header, DumpRealWidth realWidth = DumpRealWidth::k8);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encodeDumpPointChecked(
    const DumpPoint& point, DumpRealWidth realWidth = DumpRealWidth::k8);

// Compatibility wrappers. Invalid input yields an empty byte vector; new
// productive code uses the checked variants.
std::vector<std::uint8_t> encodeDumpHeader(const DumpHeader& header,
                                           DumpRealWidth realWidth = DumpRealWidth::k8);
std::vector<std::uint8_t> encodeDumpPoint(const DumpPoint& point,
                                          DumpRealWidth realWidth = DumpRealWidth::k8);

struct DumpAppendCheck {
  bool compatible{false};
  std::string message;
};

// Parses a complete dump and compares the PLXINI-relevant header fields
// and geometry schema with the requested header. Code/version text is not
// part of the append schema. No filesystem access.
[[nodiscard]] DumpAppendCheck checkDumpAppend(
    const std::uint8_t* data, std::size_t size, const DumpHeader& expected,
    DumpRealWidth realWidth = DumpRealWidth::k8);

// Append one validated PLXADD group. The existing file is read and checked
// before it is opened for writing; an incompatible or malformed file is
// left byte-identical.
[[nodiscard]] core::Status appendDumpPointFile(
    const std::string& path, const DumpHeader& expected,
    const DumpPoint& point, DumpRealWidth realWidth = DumpRealWidth::k8);

}  // namespace xfoil::io

#endif  // XFOIL_IO_DUMPCODEC_H

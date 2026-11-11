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

#ifndef XFOIL_IO_EXPORTFORMATS_H
#define XFOIL_IO_EXPORTFORMATS_H

#include <array>
#include <string>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::io {

// Delimiter selection of the canonical dump commands (KDELIM).
enum class DumpDelimiter { kBlank = 0, kComma = 1, kTab = 2, kIllegal = 3 };

// Input values for BLDUMP (xoper.f:1891). Arrays are 0-based; the
// side indices follow the canonical BL memory order.
struct BoundaryLayerDumpInput {
  int n = 0;
  int nw = 0;
  DumpDelimiter delimiter = DumpDelimiter::kBlank;
  bool lipan = false;
  bool lvisc = false;
  bool lwake = false;
  double minf = 0.0;
  double qinf = 0.0;
  double gamm1 = 0.4;
  double reinf = 0.0;
  std::vector<double> s, x, y, gam;  // count n (+ nw in the wake)
  std::array<int, 2> iblte{};
  std::array<int, 2> nbl{};
  std::array<std::vector<int>, 2> ipan;
  std::array<std::vector<double>, 2> xssi, dstr, thet, tstr, tau, dis, uedg, ctau;
  // S08 DSSR(IBL,IS), required by BLDUMP3 only.
  std::array<std::vector<double>, 2> dssr;
};

// BLDUMP semantics: returns the exact file contents.
std::string boundaryLayerDumpText(const BoundaryLayerDumpInput& input);

// BLDUMP2 value contract. The default keeps the existing 6.99 behavior.
enum class BoundaryLayerExtendedDumpContract {
  Xfoil699,
  Xfoil6996DensityCorrected,
};

// BLDUMP2 semantics (xoper.f:2050), 24 columns. The 6.996 variant changes
// only the eight RH-scaled values; header and widths are unchanged.
[[nodiscard]] core::Result<std::string> boundaryLayerExtendedDumpTextChecked(
    const BoundaryLayerDumpInput& input,
    BoundaryLayerExtendedDumpContract contract =
        BoundaryLayerExtendedDumpContract::Xfoil699);
std::string boundaryLayerExtendedDumpText(
    const BoundaryLayerDumpInput& input,
    BoundaryLayerExtendedDumpContract contract =
        BoundaryLayerExtendedDumpContract::Xfoil699);

// S13 BLDUMP3 (xoper.f:2290): separate write-only 20-column format.
[[nodiscard]] core::Result<std::string> boundaryLayerDump3TextChecked(
    const BoundaryLayerDumpInput& input);
std::string boundaryLayerDump3Text(const BoundaryLayerDumpInput& input);

// CPDUMP semantics (xoper.f:2280).
struct PressureDumpInput {
  int n = 0;
  DumpDelimiter delimiter = DumpDelimiter::kBlank;
  double minf = 0.0;
  double qinf = 0.0;
  std::vector<double> x, gam;
};
std::string pressureDumpText(const PressureDumpInput& input);

// CNDUMP semantics (xmdes.f:1747): Fourier coefficients.
struct CircleCoefficientDumpInput {
  int nc = 0;
  int mc = 0;
  std::vector<std::array<double, 2>> cn;   // cn[m] = {real, imag}
  std::vector<std::array<double, 2>> piq;  // piq[m+1] = {real, imag}
};
std::string circleCoefficientDumpText(const CircleCoefficientDumpInput& input);

// GETVOV file input (xmdes.f:1769): normalize and sort the V/Vinf-vs-s
// table. Resampling stays in xfoil::core::export_data.
struct VelocityRatioTableDecodeResult {
  bool error = false;
  std::size_t errorLine = 0;
  std::size_t count = 0;
  std::vector<double> s;    // normalized (1 - (s-s0)/sspan), sorted
  std::vector<double> vov;
};
VelocityRatioTableDecodeResult decodeVelocityRatioTable(std::string_view text);

}  // namespace xfoil::io

#endif  // XFOIL_IO_EXPORTFORMATS_H

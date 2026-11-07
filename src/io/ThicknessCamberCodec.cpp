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

// File: src/io/ThicknessCamberCodec.cpp
// Implementation of the WRTT/WRTC write contracts (xtcam.f 257-328).

#include "xfoil/io/ThicknessCamberCodec.h"

#include <cstddef>
#include <span>
#include <string>

#include "xfoil/io/TextFormat.h"

namespace xfoil::io {
namespace {

constexpr std::size_t kFortranNameLength = 48;

std::string dataLine(std::initializer_list<double> values) {
  std::string line = " ";
  for (const double value : values) {
    line += fortranG(15, 7, value);
  }
  line += '\n';
  return line;
}

}  // namespace

std::string fortranNameField48(const std::string& name) {
  std::string field = name.substr(0, kFortranNameLength);
  field.resize(kFortranNameLength, ' ');
  return field;
}

std::string thicknessDistributionText(
    const std::string& name, double xbch,
    std::span<const double> xtk, std::span<const double> ytk) {
  std::string out = "Thickness: " + fortranNameField48(name) + "\n";
  const std::size_t n = xtk.size() < ytk.size() ? xtk.size() : ytk.size();
  if (n == 0) {
    return out;
  }
  for (std::size_t k = 0; k < n; ++k) {
    const double xtn = (xtk[k] - xtk[0]) / xbch;
    const double ytn = (ytk[k] - ytk[0]) / xbch;
    const std::size_t km = (k == 0) ? 0 : k - 1;
    const std::size_t kp = (k + 1 < n) ? k + 1 : n - 1;
    const double dcdx = (ytk[kp] - ytk[km]) / (xtk[kp] - xtk[km]);
    out += dataLine({xtn, ytn, dcdx});
  }
  return out;
}

std::string camberDistributionText(
    const std::string& name, double xbch,
    std::span<const double> xcm, std::span<const double> ycm) {
  std::string out = "Camber: " + fortranNameField48(name) + "\n";
  const std::size_t n = xcm.size() < ycm.size() ? xcm.size() : ycm.size();
  if (n == 0) {
    return out;
  }
  for (std::size_t k = 0; k < n; ++k) {
    const double xcn = (xcm[k] - xcm[0]) / xbch;
    const double ycn = (ycm[k] - ycm[0]) / xbch;
    out += dataLine({xcn, ycn});
  }
  return out;
}

}  // namespace xfoil::io

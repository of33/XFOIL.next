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

// File: tests/unit/BlouCommandS16Tests.cpp
// S16 BLOU support contract: the canonical BLDUMP3 header and the
// deliberate rejection of incomplete station data by the S13 checked
// writer. The command-level fallback to the canonical header-only
// artifact is covered end-to-end in tests/integration.

#include "TestSupport.h"

#include "xfoil/io/ExportFormats.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string fixture(const std::string& relative) {
  return readBytes(std::filesystem::path("tests/data/xfoil-6.996/s16") /
                   relative);
}

}  // namespace

TEST(unit, BlouCommandS16HeaderMatchesOracle) {
  const std::string noStations = fixture("blou-no-stations.bl");
  CHECK(!noStations.empty());
  const std::string header = xfoil::io::boundaryLayerDump3HeaderText();
  CHECK_EQ(noStations.rfind(header, 0), std::size_t{0});
  CHECK_EQ(noStations.size(), header.size() + 1U);
  CHECK_EQ(noStations.back(), '\n');
}

TEST(unit, BlouCommandS16CheckedWriterRejectsIncompleteState) {
  xfoil::io::BoundaryLayerDumpInput input;
  const auto checked = xfoil::io::boundaryLayerDump3TextChecked(input);
  CHECK(checked.isErr());
  CHECK_EQ(xfoil::io::boundaryLayerDump3Text(input), std::string{});
}

// End: tests/unit/BlouCommandS16Tests.cpp

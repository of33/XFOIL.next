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

// File: tests/integration/DxfExportS14Tests.cpp
// S14 encoder against the stock-6.996 DXF R12 oracle fixtures. The current
// panel contour is produced through the accepted NACA/NACS and PANGEN
// session path, but no DXF command, filename or file writing is activated.

#include "TestSupport.h"

#include "xfoil/core/geometry/Naca.h"
#include "xfoil/io/DxfExport.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct PanelContour {
  std::vector<double> x;
  std::vector<double> y;
};

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string fixture(const std::string& name) {
  return readBytes(std::filesystem::path("tests/data/xfoil-6.996/s14") /
                   name);
}

std::size_t countSubstring(const std::string& text,
                           const std::string& needle) {
  std::size_t count = 0;
  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
}

PanelContour makePanel(int designation, bool sharpTrailingEdge) {
  xfoil::session::SessionComposition session;
  const auto naca = xfoil::core::geometry::nacaAirfoil(
      designation, sharpTrailingEdge);
  CHECK(naca.isOk());
  const auto bufferSet =
      xfoil::session::setBufferGeometry(session, naca.value().buffer);
  CHECK(bufferSet.success());
  const auto paneled = xfoil::session::applyPaneling(session);
  CHECK(paneled.ok);
  CHECK(xfoil::session::panelingValid(session));
  const auto& panel = xfoil::session::currentPanelGeometry(session);
  return PanelContour{panel.x, panel.y};
}

void checkEncoded(const PanelContour& panel, const std::string& fixtureName,
                  std::size_t expectedSize, std::size_t expectedLines) {
  const std::string expected = fixture(fixtureName);
  if (expected.empty()) {
    testing::reportFailure(__FILE__, __LINE__,
                           "fixture not readable: " + fixtureName);
    return;
  }
  CHECK_EQ(expected.size(), expectedSize);
  CHECK_EQ(countSubstring(expected, "\nLINE\n"), expectedLines);

  std::vector<double> x = panel.x;
  std::vector<double> y = panel.y;
  const auto encoded = xfoil::io::dxfR12TextChecked(x, y);
  CHECK(encoded.isOk());
  CHECK_EQ(encoded.value().size(), expectedSize);
  CHECK_EQ(encoded.value(), expected);
  CHECK_EQ(countSubstring(encoded.value(), "\nLINE\n"), expectedLines);
  CHECK(encoded.value().find('\r') == std::string::npos);
  CHECK(!encoded.value().empty());
  CHECK_EQ(encoded.value().back(), 'F');
  CHECK_EQ(encoded.value().rfind("EOF"),
           encoded.value().size() - std::size_t{3});
  CHECK(encoded.value().find("foil.dxf") == std::string::npos);

  CHECK_EQ(x.size(), panel.x.size());
  CHECK_EQ(y.size(), panel.y.size());
  for (std::size_t i = 0; i < x.size(); ++i) {
    CHECK_EQ(x[i], panel.x[i]);
    CHECK_EQ(y[i], panel.y[i]);
  }

  const auto repeated = xfoil::io::dxfR12TextChecked(x, y);
  CHECK(repeated.isOk());
  CHECK_EQ(repeated.value(), encoded.value());
}

}  // namespace

TEST(integration, DxfExportS14OracleBytes) {
  const PanelContour naca0012 = makePanel(12, false);
  CHECK_EQ(naca0012.x.size(), std::size_t{240});
  checkEncoded(naca0012, "dxf-naca0012-240.dxf", std::size_t{16013},
               std::size_t{239});

  const PanelContour naca2412 = makePanel(2412, false);
  CHECK_EQ(naca2412.x.size(), std::size_t{240});
  checkEncoded(naca2412, "dxf-naca2412-240.dxf", std::size_t{16009},
               std::size_t{239});

  const PanelContour nacs0012 = makePanel(12, true);
  CHECK_EQ(nacs0012.x.size(), std::size_t{240});
  checkEncoded(nacs0012, "dxf-nacs0012-240.dxf", std::size_t{16013},
               std::size_t{239});
}

TEST(integration, DxfExportS14EmptyContourAndLFContract) {
  const std::vector<double> x;
  const std::vector<double> y;
  const auto encoded = xfoil::io::dxfR12TextChecked(x, y);
  CHECK(encoded.isOk());
  CHECK_EQ(encoded.value().size(), std::size_t{239});
  CHECK_EQ(encoded.value(), fixture("dxf-empty.dxf"));
  CHECK_EQ(countSubstring(encoded.value(), "\nLINE\n"), std::size_t{0});
  CHECK(encoded.value().find('\r') == std::string::npos);
  CHECK_EQ(encoded.value().back(), 'F');
  CHECK_EQ(encoded.value().rfind("EOF"),
           encoded.value().size() - std::size_t{3});

  const auto repeated = xfoil::io::dxfR12TextChecked(x, y);
  CHECK(repeated.isOk());
  CHECK_EQ(repeated.value(), encoded.value());
}

// End: tests/integration/DxfExportS14Tests.cpp

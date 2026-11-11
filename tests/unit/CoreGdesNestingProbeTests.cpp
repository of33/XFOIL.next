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

// File: tests/unit/CoreGdesNestingProbeTests.cpp
// Router probe of menu nesting using the same handler/session paths
// as the production GDES menu handler; the test calls the handlers
// directly instead of going through the TOP dispatcher.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

session::SessionComposition makeSession() {
  session::SessionComposition s;
  std::ostringstream setup;
  testing::handleLegacyNacaCore(s, setup, 12);
  return s;
}

}  // namespace

TEST(unit, CoreGdesNestingCambProbe) {
  // GDES -> CAMB -> (SCAL, <return> back) -> GDES -> '!' redo CAMB.
  auto s = makeSession();
  std::istringstream in("CAMB\nSCAL 2.0\n\n!\n\n\n");
  std::ostringstream out;
  const auto status = cli::handleGeometryDesignCore(s, in, out);
  if (status != cli::GeometryDesignCoreStatus::returned) {
    std::cerr << "status=" << static_cast<int>(status)
              << " tail=" << out.str().substr(out.str().size()>200?out.str().size()-200:0) << "\n";
  }
  CHECK(status == cli::GeometryDesignCoreStatus::returned);
  const std::string text = out.str();
  // the CAMB submenu was entered (block-200 report 'Added loading'
  // or CAMB prompt); after leaving, the GDES loop continued
  // ('!' invokes CAMB again).
  CHECK(text.find("..CAMB") != std::string::npos);
  const std::size_t first = text.find("..CAMB");
  const std::size_t second = text.find("..CAMB", first + 1);
  CHECK(second != std::string::npos);
  // GDES redo stores CAMB canonically (COMOLD/ARGOLD).
  CHECK(s.design.menu.gdesRedo.has_value());
  CHECK_EQ(s.design.menu.gdesRedo->first, std::string("CAMB"));
  // CAMB redo is CAMB-local state and is cleared on exit.
  CHECK(!s.design.menu.cambRedo.has_value());
}

TEST(unit, CoreGdesNestingCambEofProbe) {
  // EOF in the CAMB submenu aborts both menu levels with endOfFile;
  // the GDES redo stays stored.
  auto s = makeSession();
  std::istringstream in("CAMB\nSCAL 2.0\n");
  std::ostringstream out;
  const auto status = cli::handleGeometryDesignCore(s, in, out);
  CHECK(status == cli::GeometryDesignCoreStatus::endOfFile);
  CHECK(s.design.menu.gdesRedo.has_value());
  CHECK_EQ(s.design.menu.gdesRedo->first, std::string("CAMB"));
}

TEST(unit, CoreGdesNestingCambBufferMutationVisible) {
 // CAMB ADD mutates the GDES buffer (buffer contract) and leaves
  // the panel contour unchanged; after the CAMB exit the buffer
  // differs from the one set at entry.
  auto s = makeSession();
  (void)session::initializeGeometryDesign(s);
  const std::size_t bufferRevisionBefore = s.geometry.bufferRevision;
  std::istringstream in("CAMB\nSETC\nADD\n\n\n");
  std::ostringstream out;
  const auto status = cli::handleGeometryDesignCore(s, in, out);
  CHECK(status == cli::GeometryDesignCoreStatus::returned);
  CHECK(s.geometry.bufferRevision > bufferRevisionBefore);
  CHECK(s.paneling.panelingValid);
}

TEST(unit, CorePlopTopLevelProbe) {
  // PLOP is TOP-level (xfoil.f 302), not a GDES command.
  // the probe uses the same handler as the TOP PLOP route:
  // menu table, options, return with '0'/empty.
  auto s = makeSession();
  const std::size_t bufferBefore = s.geometry.buffer.x.size();
  std::istringstream in("S\n10.\nO\n0\n");
  std::ostringstream out;
  const auto status = cli::handlePlotOptionsCore(s, in, out);
  CHECK(status == cli::PlotOptionsCoreStatus::returned);
  std::cerr << "PLOP len=" << out.str().size() << " status=" << static_cast<int>(status) << "\n";
  CHECK(out.str().find("raphics-enable flag") != std::string::npos);
  CHECK_EQ(s.geometry.buffer.x.size(), bufferBefore);
  // GDES has no PLOP branch: PLOP is unknown in the GDES menu.
  std::istringstream inGdes("PLOP\n\n");
  std::ostringstream outGdes;
  const auto gdesStatus = cli::handleGeometryDesignCore(s, inGdes, outGdes);
  CHECK(gdesStatus == cli::GeometryDesignCoreStatus::returned);
  CHECK(outGdes.str().find("not recognized") != std::string::npos);
}

// end: tests/unit/CoreGdesNestingProbeTests.cpp

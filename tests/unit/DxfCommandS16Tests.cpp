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

// File: tests/unit/DxfCommandS16Tests.cpp
// S16 DXF command follow-up: filename, overwrite, path-policy, non-fatal
// error and S14-byte reuse without command registration.

#include "TestSupport.h"

#include "app/cli/CoreDxfHandlers.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/io/OutputPathPolicy.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

std::string fixture(const std::string& name) {
  return readBytes(std::filesystem::absolute(
      std::filesystem::path("tests/data/xfoil-6.996/s14") / name));
}

void makeNaca0012Panel(xfoil::session::SessionComposition& session) {
  const auto naca = xfoil::core::geometry::nacaAirfoil(12, false);
  CHECK(naca.isOk());
  CHECK(xfoil::session::setBufferGeometry(session, naca.value().buffer)
            .success());
  CHECK(xfoil::session::applyPaneling(session).ok);
  CHECK(xfoil::session::panelingValid(session));
  CHECK_EQ(session.paneling.panel.x.size(), std::size_t{240});
}

xfoil::app::cli::DxfExportStatus runDxf(
    xfoil::session::SessionComposition& session, std::ostringstream& out,
    std::string_view argument, bool hasArgument,
    std::string_view filenameAnswer, bool hasFilenameAnswer,
    std::string_view overwriteAnswer, bool hasOverwriteAnswer) {
  return xfoil::app::cli::handleDxfExportCore(
      session, out, argument, hasArgument, filenameAnswer, hasFilenameAnswer,
      overwriteAnswer, hasOverwriteAnswer);
}

}  // namespace

TEST(unit, DxfCommandS16InlineWritesExactS14Bytes) {
  const std::string expected = fixture("dxf-naca0012-240.dxf");
  CHECK(!expected.empty());
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);
  const std::vector<double> beforeX = session.paneling.panel.x;
  const std::vector<double> beforeY = session.paneling.panel.y;

  testing::ScopedTempDirectory work("s16-dxf-inline", true);
  std::ostringstream out;
  const auto status = runDxf(session, out, "out.dxf", true, "", false, "",
                             false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::written);
  CHECK(out.str().empty());
  CHECK_EQ(readBytes("out.dxf"), expected);
  CHECK(session.paneling.panel.x == beforeX);
  CHECK(session.paneling.panel.y == beforeY);
}

TEST(unit, DxfCommandS16InlineKeepsMissingExtension) {
  const std::string expected = fixture("dxf-naca0012-240.dxf");
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);

  testing::ScopedTempDirectory work("s16-dxf-noext", true);
  std::ostringstream out;
  const auto status =
      runDxf(session, out, "noext", true, "", false, "", false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::written);
  CHECK_EQ(readBytes("noext"), expected);
  CHECK(!std::filesystem::exists("noext.dxf"));
}

TEST(unit, DxfCommandS16PromptHasNoPrefixDefaultAndTrimsAnswer) {
  const std::string expected = fixture("dxf-naca0012-240.dxf");
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);
  session.geometry.filePrefix = "CLARK_Y";

  testing::ScopedTempDirectory work("s16-dxf-prompt", true);
  std::ostringstream promptOut;
  const auto promptStatus =
      runDxf(session, promptOut, "", false, "", false, "", false);
  CHECK(promptStatus ==
        xfoil::app::cli::DxfExportStatus::filenamePromptRequested);
  CHECK(promptOut.str().find("\nEnter output filename   s>  ") !=
        std::string::npos);
  CHECK(promptOut.str().find("CLARK_Y.dxf") == std::string::npos);
  CHECK(!std::filesystem::exists("prompted.dxf"));

  std::ostringstream out;
  const auto status = runDxf(session, out, "", false, "  prompted.dxf  ",
                             true, "", false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::written);
  CHECK_EQ(readBytes("prompted.dxf"), expected);
}

TEST(unit, DxfCommandS16PromptBlankIsBadFilename) {
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);

  testing::ScopedTempDirectory work("s16-dxf-blank", true);
  std::ostringstream out;
  const auto status =
      runDxf(session, out, "", false, "   ", true, "", false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::badFilename);
  CHECK_EQ(out.str(),
           std::string(" Bad filename.\n DXF file not written.\n"));
  CHECK(!std::filesystem::exists("out.dxf"));
}

TEST(unit, DxfCommandS16OverwriteDeclineKeepsExistingFile) {
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);

  for (const std::string answer : {"N", "n"}) {
    testing::ScopedTempDirectory work("s16-dxf-decline", true);
    {
      std::ofstream existing("out.dxf", std::ios::binary);
      existing << "OLD";
    }
    std::ostringstream out;
    const auto status = runDxf(session, out, "out.dxf", true, "", false,
                               answer, true);
    CHECK(status == xfoil::app::cli::DxfExportStatus::notWrittenDeclined);
    CHECK(out.str().find(" DXF file not written.\n") != std::string::npos);
    CHECK_EQ(readBytes("out.dxf"), std::string("OLD"));
  }
}

TEST(unit, DxfCommandS16OverwriteDefaultAndOtherAnswerWrite) {
  const std::string expected = fixture("dxf-naca0012-240.dxf");
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);

  for (const std::string answer : {"", "Y", "yes"}) {
    testing::ScopedTempDirectory work("s16-dxf-overwrite", true);
    {
      std::ofstream existing("out.dxf", std::ios::binary);
      existing << "OLD";
    }
    std::ostringstream out;
    const auto status = runDxf(session, out, "out.dxf", true, "", false,
                               answer, true);
    CHECK(status == xfoil::app::cli::DxfExportStatus::written);
    CHECK_EQ(readBytes("out.dxf"), expected);
  }
}

TEST(unit, DxfCommandS16OutputPathPolicyAppliedOnce) {
  const std::string expected = fixture("dxf-naca0012-240.dxf");
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);
  session.outputPaths.prefix = "pre-";

  testing::ScopedTempDirectory work("s16-dxf-prefix", true);
  std::ostringstream out;
  const auto status =
      runDxf(session, out, "out.dxf", true, "", false, "", false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::written);
  CHECK_EQ(readBytes("pre-out.dxf"), expected);
  CHECK(!std::filesystem::exists("out.dxf"));
  CHECK(!std::filesystem::exists("pre-pre-out.dxf"));
}

TEST(unit, DxfCommandS16BadPathIsNonFatalAndLeavesNoFile) {
  xfoil::session::SessionComposition session;
  makeNaca0012Panel(session);

  testing::ScopedTempDirectory work("s16-dxf-badpath", true);
  std::ostringstream out;
  const auto status = runDxf(session, out, "missing/out.dxf", true, "", false,
                             "", false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::badFilename);
  CHECK_EQ(out.str(),
           std::string(" Bad filename.\n DXF file not written.\n"));
  CHECK(!std::filesystem::exists("missing/out.dxf"));
}

TEST(unit, DxfCommandS16EmptyContourUsesS14Skeleton) {
  const std::string expected = fixture("dxf-empty.dxf");
  xfoil::session::SessionComposition session;

  testing::ScopedTempDirectory work("s16-dxf-empty", true);
  std::ostringstream out;
  const auto status =
      runDxf(session, out, "empty.dxf", true, "", false, "", false);
  CHECK(status == xfoil::app::cli::DxfExportStatus::written);
  CHECK_EQ(readBytes("empty.dxf"), expected);
  CHECK_EQ(expected.size(), std::size_t{239});
}

// End: tests/unit/DxfCommandS16Tests.cpp

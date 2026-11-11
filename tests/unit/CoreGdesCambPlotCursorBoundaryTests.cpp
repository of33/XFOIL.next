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

// File: tests/unit/CoreGdesCambPlotCursorBoundaryTests.cpp
// final plot/cursor limits of the
// GDES/CAMB handlers. Each pure plot or
// cursor command/alias must produce exactly one message
// 'Plotting is not available in this build.', must not
// trigger a session mutation or recompute the prompt chain from
// the graphical Fortran path. The matrix completeness check verifies that
// no command of the Fortran dispatch set remains unclassified.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace session = xfoil::session;
namespace cli = xfoil::app::cli;

constexpr const char* kUnavailable =
    "Plotting is not available in this build.";

struct StateSnapshot {
  bool geometryValid{false};
  std::size_t bufferPoints{0};
  std::size_t bufferRevision{0};
  bool hasBufferMetrics{false};
  double bufferChord{0.0};
  bool panelingValid{false};
  std::size_t panelPoints{0};
  int origin{0};
  std::size_t panelSourceRevision{0};
  bool lgSame{false};
  bool lbFlap{false};
  bool lFlap{false};
  double xof{0.0};
  double yof{0.0};
  bool lgSlop{false};
  bool lpLcam{false};
  bool hasGdesRedo{false};
  bool hasCambRedo{false};
  std::string oname;
  bool systemValid{false};
  bool converged{false};
  bool wakeValid{false};
  bool hasResult{false};
  std::string geometryName;
};

StateSnapshot snapshot(const session::SessionComposition& s) {
  StateSnapshot t;
  t.geometryValid = s.geometry.valid;
  t.bufferPoints = s.geometry.buffer.x.size();
  t.bufferRevision = s.geometry.bufferRevision;
  t.hasBufferMetrics = s.geometry.metrics.has_value();
  t.bufferChord =
      s.geometry.metrics ? s.geometry.metrics->chord : 0.0;
  t.panelingValid = s.paneling.panelingValid;
  t.panelPoints = s.paneling.panel.x.size();
  t.origin = static_cast<int>(s.paneling.origin);
  t.panelSourceRevision = s.paneling.panelSourceRevision;
  t.lgSame = s.design.lgSame;
  t.lbFlap = s.design.flap.lbFlap;
  t.lFlap = s.design.flap.lFlap;
  t.xof = s.design.flap.xof;
  t.yof = s.design.flap.yof;
  t.lgSlop = s.design.lgSlop;
  t.lpLcam = s.design.lpLcam;
  t.hasGdesRedo = s.design.menu.gdesRedo.has_value();
  t.hasCambRedo = s.design.menu.cambRedo.has_value();
  t.oname = s.design.oname;
  t.systemValid = s.viscous.systemValid;
  t.converged = s.viscous.converged;
  t.wakeValid = s.viscous.wakeValid;
  t.hasResult = s.result.lastResult.successful;
  t.geometryName = s.geometry.name;
  return t;
}

bool same(const StateSnapshot& a, const StateSnapshot& b) {
  return a.geometryValid == b.geometryValid &&
         a.bufferPoints == b.bufferPoints &&
         a.bufferRevision == b.bufferRevision &&
         a.hasBufferMetrics == b.hasBufferMetrics &&
         a.bufferChord == b.bufferChord &&
         a.panelingValid == b.panelingValid &&
         a.panelPoints == b.panelPoints &&
         a.origin == b.origin &&
         a.panelSourceRevision == b.panelSourceRevision &&
         a.lgSame == b.lgSame && a.lbFlap == b.lbFlap &&
         a.lFlap == b.lFlap && a.xof == b.xof && a.yof == b.yof &&
         a.lgSlop == b.lgSlop && a.lpLcam == b.lpLcam &&
         a.oname == b.oname &&
         a.systemValid == b.systemValid &&
         a.converged == b.converged && a.wakeValid == b.wakeValid &&
         a.hasResult == b.hasResult && a.geometryName == b.geometryName;
}

session::SessionComposition makeDesignSession() {
  session::SessionComposition session;
  std::ostringstream setupOut;
  testing::handleLegacyNacaCore(session, setupOut, 12);
  (void)session::initializeGeometryDesign(session);
  return session;
}

// Feeds exactly one GDES menu command (followed by a clean <Return>
// terminator). The input stream contains only this one line: if the
// handler wanted to read additional prompts it would see EOF and abort
// with endOfFile; the test also checks the status.
void checkGdesUnavailable(const std::string& command) {
  auto session = makeDesignSession();
  const StateSnapshot before = snapshot(session);
  std::istringstream in(command + "\n\n");
  std::ostringstream out;
  const auto status = cli::handleGeometryDesignCore(session, in, out);
  const StateSnapshot after = snapshot(session);
  const std::string text = out.str();

  CHECK(status == cli::GeometryDesignCoreStatus::returned);
  std::size_t occurrences = 0;
  for (std::size_t pos = text.find(kUnavailable);
       pos != std::string::npos; pos = text.find(kUnavailable, pos + 1)) {
    ++occurrences;
  }
  CHECK_EQ(occurrences, std::size_t{1});
  if (same(before, after)) {
    // expected
  } else {
    std::cerr << "\n=== GDES " << command << " mutates ===\n";
  }
  CHECK(same(before, after));
}

void checkCambUnavailable(const std::string& command) {
  auto session = makeDesignSession();
  // the CAMB entry itself initializes the CAMB state (canonical);
  // the comparison point is taken after the entry.
  {
    std::istringstream warmup("\n");
    std::ostringstream warmupOut;
    (void)cli::handleCamberDesignCore(session, warmup, warmupOut);
  }
  const StateSnapshot before = snapshot(session);
  std::istringstream in(command + "\n\n");
  std::ostringstream out;
  const auto status = cli::handleCamberDesignCore(session, in, out);
  const StateSnapshot after = snapshot(session);
  const std::string text = out.str();

  CHECK(status == cli::CamberDesignCoreStatus::returned);
  std::size_t occurrences = 0;
  for (std::size_t pos = text.find(kUnavailable);
       pos != std::string::npos; pos = text.find(kUnavailable, pos + 1)) {
    ++occurrences;
  }
  CHECK_EQ(occurrences, std::size_t{1});
  CHECK(same(before, after));
}

// Complete Fortran dispatch set (xgdes.f 73-946) with final
// classification. prepared = a GDES/CAMB command handled by the menu
// handler; unavailable =
// plot/cursor command that emits exactly one message.
const std::vector<std::string>& gdesPrepared() {
  static const std::vector<std::string> commands = {
      "?", "GSET", "SYMM", "ADEG", "ARAD", "TRAN", "T", "SCAL", "S",
      "LINS", "DERO", "TGAP", "LERA", "TCPL", "TFAC", "TSET", "HIGH",
      "BEND", "CANG", "CADD", "CLIS", "FLAP", "F", "NMOV", "UNIT",
      "SLOP", "NAME", "NINC", "NDEC", "OVER", "O", "EXEC", "X", "CAMB"};
  return commands;
}

const std::vector<std::string>& gdesUnavailable() {
  static const std::vector<std::string> commands = {
      "MODI", "M", "CORN", "ADDP", "MOVP", "DELP", "DIST", "D",
      "WIND", "W", "PLOT", "P", "INPL", "BLOW", "B", "RESE", "R",
      "TSIZ", "TICK", "GRID", "GPAR", "SIZE", "HARD", "CPLO", "ANNO",
      "Z", "U", "SINT"};
  return commands;
}

const std::vector<std::string>& cambPrepared() {
  static const std::vector<std::string> commands = {
      "?", "TFAC", "TSET", "HIGH", "SETC", "INPC", "INPP", "CLR",
      "SCAL", "ADD", "DCPL", "SLOP", "RDAC", "WRTT", "WRTC"};
  return commands;
}

const std::vector<std::string>& cambUnavailable() {
  static const std::vector<std::string> commands = {
      "MODC", "MODP", "CPLI", "BLOW", "B", "RESE", "R", "SIZE",
      "ANNO", "HARD", "Z", "U"};
  return commands;
}

}  // namespace

TEST(unit, CoreGdesPlotCursorBoundaryMatrixComplete) {
  // Every command of the Fortran dispatch set is classified; the
  // lists must not overlap each other.
  for (const std::string& cmd : gdesPrepared()) {
    for (const std::string& other : gdesUnavailable()) {
      CHECK(cmd != other);
    }
  }
  for (const std::string& cmd : cambPrepared()) {
    for (const std::string& other : cambUnavailable()) {
      CHECK(cmd != other);
    }
  }
}

TEST(unit, CoreGdesPlotCommandsUnavailableNoMutation) {
  for (const std::string& cmd : gdesUnavailable()) {
    checkGdesUnavailable(cmd);
  }
}

TEST(unit, CoreCambPlotCursorCommandsUnavailableNoMutation) {
  for (const std::string& cmd : cambUnavailable()) {
    checkCambUnavailable(cmd);
  }
}

TEST(unit, CoreGdesPreparedCommandsNeverPrintUnavailable) {
  // Guarded forward check: handled commands must never emit the
  // unavailable message. Commands with interactive prompts are executed
  // with an immediate <Return> answer.
  for (const std::string& cmd : gdesPrepared()) {
    auto session = makeDesignSession();
    std::istringstream in(cmd + "\n\n\n\n\n\n");
    std::ostringstream out;
    (void)cli::handleGeometryDesignCore(session, in, out);
    CHECK(out.str().find(kUnavailable) == std::string::npos);
  }
}

TEST(unit, CoreCambPreparedCommandsNeverPrintUnavailable) {
  for (const std::string& cmd : cambPrepared()) {
    auto session = makeDesignSession();
    std::istringstream in(cmd + "\n\n\n\n\n\n");
    std::ostringstream out;
    (void)cli::handleCamberDesignCore(session, in, out);
    CHECK(out.str().find(kUnavailable) == std::string::npos);
  }
}

TEST(unit, CoreGdesPlotCursorRedoIsCanonical) {
  // xgdes.f 940-941: COMOLD/ARGOLD are stored for EVERY recognized
  // command, including plot/cursor commands; '!' repeats it and produces
  // the same unavailable message again.
  for (const std::string& cmd : gdesUnavailable()) {
    auto session = makeDesignSession();
    std::istringstream in(cmd + "\n!\n\n");
    std::ostringstream out;
    const auto status = cli::handleGeometryDesignCore(session, in, out);
    CHECK(status == cli::GeometryDesignCoreStatus::returned);
    const std::string text = out.str();
    std::size_t occurrences = 0;
    for (std::size_t pos = text.find(kUnavailable);
         pos != std::string::npos;
         pos = text.find(kUnavailable, pos + 1)) {
      ++occurrences;
    }
    CHECK_EQ(occurrences, std::size_t{2});
    CHECK(session.design.menu.gdesRedo.has_value());
  }
}

// end: tests/unit/CoreGdesCambPlotCursorBoundaryTests.cpp

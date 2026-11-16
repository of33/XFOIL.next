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

// File: tests/tools/LibraryNoIoProof.cpp
// L7 armed no-I/O proof consumer. Library calls are made only while the
// test shim is armed. PASS is printed after disarm.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

#include "LibraryNoIoShim.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Progress.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

std::string currentDirectory() {
  char buffer[4096] = {};
  if (getcwd(buffer, sizeof(buffer)) == nullptr) {
    return {};
  }
  return buffer;
}

std::string environmentValue(const char* name) {
  const char* value = std::getenv(name);
  return value == nullptr ? std::string{} : std::string(value);
}

xfoil::AnalysisConfig inviscidConfig() {
  xfoil::AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 60;
  return config;
}

bool runApiWorkflow(const xfoil::geometry::AirfoilGeometry& memoryGeometry) {
  using xfoil::AlphaSequence;
  using xfoil::AnalysisRequest;
  using xfoil::ConvergenceStatus;
  using xfoil::Degrees;
  using xfoil::PanelingOptions;
  using xfoil::ProgressCallback;
  using xfoil::ProgressEvent;
  using xfoil::Session;

  int progressEvents = 0;
  const ProgressCallback progress = [&progressEvents](
                                        const ProgressEvent&) {
    ++progressEvents;
  };

  Session first;
  if (first.setNaca("2412", progress).isErr()) {
    return false;
  }
  PanelingOptions options;
  options.panelCount = 180;
  if (first.generatePanel(options, progress).isErr()) {
    return false;
  }
  if (first.configure(inviscidConfig()).isErr()) {
    return false;
  }
  if (!first.solveAlpha(Degrees{2.0}, nullptr, progress).isOk()) {
    return false;
  }
  if (!first.solveCl(0.4, nullptr, progress).isOk()) {
    return false;
  }
  AlphaSequence sequence;
  sequence.start = Degrees{-1.0};
  sequence.endExclusive = Degrees{2.0};
  sequence.step = Degrees{1.0};
  if (first.solveAlphaSequence(sequence, nullptr, progress).isErr()) {
    return false;
  }
  if (first.currentPanel().isErr() ||
      first.currentSurfaceDistribution().isErr() ||
      first.lastOperatingPoint().isErr()) {
    return false;
  }
  (void)first.activePolar();

  AnalysisRequest request;
  request.geometry = memoryGeometry;
  request.paneling = options;
  request.configuration = inviscidConfig();
  request.target = xfoil::AlphaTarget{Degrees{1.0}};
  const auto analyzed = xfoil::analyze(request, nullptr, progress);
  if (analyzed.isErr() ||
      analyzed.value().convergence != ConvergenceStatus::converged) {
    return false;
  }

  AnalysisRequest invalid;
  if (!xfoil::analyze(invalid).isErr()) {
    return false;
  }

  Session second;
  if (second.setNaca("0012", progress).isErr() ||
      second.generatePanel(PanelingOptions{}, progress).isErr() ||
      second.configure(inviscidConfig()).isErr()) {
    return false;
  }
  if (!second.solveAlpha(Degrees{1.0}, nullptr, progress).isOk()) {
    return false;
  }
  xfoil::CancellationToken callbackToken;
  const ProgressCallback cancelFromCallback = [&callbackToken](
                                                  const ProgressEvent& event) {
    if (event.phase == xfoil::ProgressPhase::inviscidSolve &&
        event.completed == 0) {
      callbackToken.requestCancellation();
    }
  };
  const auto callbackCancelled =
      second.solveAlpha(Degrees{0.0}, &callbackToken, cancelFromCallback);
  if (callbackCancelled.isErr() &&
      callbackCancelled.error().code == "analysis.cancelled") {
    callbackToken.reset();
  } else {
    return false;
  }

  Session errors;
  if (!errors.solveAlpha(Degrees{1.0}).isErr()) {
    return false;
  }
  if (errors.setNaca("0012").isErr()) {
    return false;
  }
  PanelingOptions invalidPanel;
  invalidPanel.panelCount = 1;
  if (!errors.generatePanel(invalidPanel).isErr()) {
    return false;
  }
  xfoil::AnalysisConfig invalidConfig = inviscidConfig();
  invalidConfig.reynoldsNumber = -1.0;
  if (!errors.configure(invalidConfig).isErr()) {
    return false;
  }
  if (errors.configure(inviscidConfig()).isErr()) {
    return false;
  }
  const ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == xfoil::ProgressPhase::geometry) {
      throw 1;
    }
  };
  const auto callbackFailure = errors.setGeometry(memoryGeometry, throwing);
  if (!callbackFailure.isErr() ||
      callbackFailure.error().code != "progress.callbackFailure") {
    return false;
  }
  if (!errors.solveAlpha(Degrees{std::nan("")}).isErr()) {
    return false;
  }
  if (!errors.solveCl(std::nan("")).isErr()) {
    return false;
  }
  AlphaSequence zeroStep;
  zeroStep.start = Degrees{0.0};
  zeroStep.endExclusive = Degrees{2.0};
  zeroStep.step = Degrees{0.0};
  if (!errors.solveAlphaSequence(zeroStep).isErr()) {
    return false;
  }

  xfoil::CancellationToken token;
  token.requestCancellation();
  const auto cancelled = first.solveAlphaSequence(sequence, &token);
  if (cancelled.isErr() ||
      cancelled.value().stopReason !=
          xfoil::SequenceStopReason::cancelled) {
    return false;
  }

  if (progressEvents <= 0) {
    return false;
  }

  first.reset();
  Session destroyed;
  if (destroyed.setNaca("0012").isErr() ||
      destroyed.configure(inviscidConfig()).isErr() ||
      destroyed.solveAlpha(Degrees{0.0}).isErr()) {
    return false;
  }
  return true;
}

}  // namespace

int main() {
  const std::string beforeCwd = currentDirectory();
  const std::string beforePath = environmentValue("PATH");
  const std::string beforeHome = environmentValue("HOME");
  const std::string beforePwd = environmentValue("PWD");
  const std::string beforeTmp = environmentValue("TMPDIR");

  std::vector<xfoil::Point2D> points = {
      {1.0, 0.0}, {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  xfoil::geometry::AirfoilGeometry memoryGeometry;
  if (!memoryGeometry.assign("l7-memory", points).success()) {
    std::cout << "LibraryNoIoProof FAIL geometry setup\n";
    return 1;
  }

  xfoil_noio_reset();
  xfoil_noio_arm();
  const bool workflowOk = runApiWorkflow(memoryGeometry);
  xfoil_noio_disarm();

  const unsigned long long hits = xfoil_noio_hits();
  const std::string firstHit = xfoil_noio_first_hit();
  const std::string afterCwd = currentDirectory();
  const bool environmentUnchanged =
      beforePath == environmentValue("PATH") &&
      beforeHome == environmentValue("HOME") &&
      beforePwd == environmentValue("PWD") &&
      beforeTmp == environmentValue("TMPDIR");

  if (!workflowOk) {
    std::cout << "LibraryNoIoProof FAIL workflow\n";
    return 1;
  }
  if (hits != 0) {
    std::cout << "LibraryNoIoProof FAIL forbidden calls=" << hits
              << " first=" << firstHit << "\n";
    return 1;
  }
  if (beforeCwd != afterCwd || beforeCwd.empty()) {
    std::cout << "LibraryNoIoProof FAIL cwd changed\n";
    return 1;
  }
  if (!environmentUnchanged) {
    std::cout << "LibraryNoIoProof FAIL environment changed\n";
    return 1;
  }

  std::cout << "LibraryNoIoProof PASS\n";
  return 0;
}

// End: tests/tools/LibraryNoIoProof.cpp

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

// File: tests/integration/LibrarySessionL3Tests.cpp
// End-to-end public in-memory session workflows without file or console I/O.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::geometry::AirfoilGeometry;
using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::Session;
using xfoil::SurfaceDistributionKind;
using xfoil::SurfaceGeometry;

AnalysisConfig inviscidConfig() {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  return config;
}

AnalysisConfig viscousConfig() {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 100;
  return config;
}

AlphaSequence alphaSequence(double start, double endExclusive, double step) {
  AlphaSequence sequence;
  sequence.start = Degrees{start};
  sequence.endExclusive = Degrees{endExclusive};
  sequence.step = Degrees{step};
  return sequence;
}

bool containsUnconverged(const SequenceResult& sequence) {
  for (const auto& point : sequence.points) {
    if (point.convergence == ConvergenceStatus::unconverged) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(integration, LibrarySessionL3FullWorkflow) {
  testing::ScopedTempDirectory work("library-session-l3-integration", true);
  CHECK(std::filesystem::is_empty(work.path()));

  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
  std::streambuf* oldErr = std::cerr.rdbuf(err.rdbuf());

  std::optional<Result<AirfoilGeometry>> geometry;
  std::optional<Result<SurfaceGeometry>> panel;
  std::optional<Result<void>> inviscidConfigured;
  std::optional<Result<OperatingPointData>> inviscidPoint;
  std::optional<Result<OperatingPointData>> inviscidRepeat;
  std::optional<Result<OperatingPointData>> clPoint;
  std::optional<Result<void>> viscousConfigured;
  std::optional<Result<OperatingPointData>> viscousPoint;
  std::optional<Result<SequenceResult>> sequence;

  {
    Session session;
    geometry = session.setNaca("2412");
    PanelingOptions options;
    panel = session.generatePanel(options);
    inviscidConfigured = session.configure(inviscidConfig());
    inviscidPoint = session.solveAlpha(Degrees{2.0});
    inviscidRepeat = session.solveAlpha(Degrees{2.0});
    clPoint = session.solveCl(0.6);
    viscousConfigured = session.configure(viscousConfig());
    viscousPoint = session.solveAlpha(Degrees{2.0});
    sequence = session.solveAlphaSequence(alphaSequence(-1.0, 2.0, 1.0));
  }

  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);

  CHECK(out.str().empty());
  CHECK(err.str().empty());
  CHECK(std::filesystem::is_empty(work.path()));

  CHECK(geometry.has_value());
  CHECK(geometry->isOk());
  CHECK(panel.has_value());
  CHECK(panel->isOk());
  CHECK_EQ(panel->value().points.size(), std::size_t{240});
  CHECK(inviscidConfigured.has_value());
  CHECK(inviscidConfigured->isOk());
  CHECK(inviscidPoint.has_value());
  CHECK(inviscidPoint->isOk());
  CHECK(inviscidPoint->value().convergence ==
        ConvergenceStatus::converged);
  CHECK(inviscidPoint->value().surface.has_value());
  CHECK(inviscidPoint->value().surface->kind ==
        SurfaceDistributionKind::inviscid);
  CHECK(inviscidRepeat.has_value());
  CHECK(inviscidRepeat->isOk());
  CHECK_NEAR(inviscidRepeat->value().liftCoefficient,
             inviscidPoint->value().liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(inviscidRepeat->value().dragCoefficient,
             inviscidPoint->value().dragCoefficient, 0.0, 0.0);
  CHECK_NEAR(inviscidRepeat->value().momentCoefficient,
             inviscidPoint->value().momentCoefficient, 0.0, 0.0);

  CHECK(clPoint.has_value());
  CHECK(clPoint->isOk());
  CHECK(clPoint->value().convergence == ConvergenceStatus::converged);
  CHECK_NEAR(clPoint->value().liftCoefficient, 0.6, 1.0e-6, 1.0e-6);

  CHECK(viscousConfigured.has_value());
  CHECK(viscousConfigured->isOk());
  CHECK(viscousPoint.has_value());
  CHECK(viscousPoint->isOk());
  CHECK(viscousPoint->value().viscous);
  CHECK(viscousPoint->value().surface.has_value());
  CHECK(viscousPoint->value().surface->kind ==
        SurfaceDistributionKind::viscous);

  CHECK(sequence.has_value());
  CHECK(sequence->isOk());
  CHECK_EQ(sequence->value().points.size(), std::size_t{3});
  CHECK(sequence->value().stopReason == SequenceStopReason::completed);
}

TEST(integration, LibrarySessionL3NacaNacsAndIsolation) {
  Session normal;
  Session sharp;
  const Result<AirfoilGeometry> normalGeometry = normal.setNaca("0012");
  const Result<AirfoilGeometry> sharpGeometry = sharp.setNacs("0012");
  CHECK(normalGeometry.isOk());
  CHECK(sharpGeometry.isOk());
  CHECK_EQ(normalGeometry.value().pointCount(), std::size_t{399});
  CHECK_EQ(sharpGeometry.value().pointCount(), std::size_t{399});
  CHECK(normalGeometry.value().coordinates().front().y !=
        sharpGeometry.value().coordinates().front().y);
  CHECK(normalGeometry.value().coordinates().back().y !=
        sharpGeometry.value().coordinates().back().y);

  CHECK(normal.configure(inviscidConfig()).isOk());
  CHECK(sharp.configure(inviscidConfig()).isOk());
  const Result<OperatingPointData> normalPoint =
      normal.solveAlpha(Degrees{2.0});
  const Result<OperatingPointData> sharpPoint =
      sharp.solveAlpha(Degrees{2.0});
  CHECK(normalPoint.isOk());
  CHECK(sharpPoint.isOk());
  CHECK(normalPoint.value().convergence == ConvergenceStatus::converged);
  CHECK(sharpPoint.value().convergence == ConvergenceStatus::converged);

  normal.reset();
  CHECK(normal.currentPanel().isErr());
  CHECK(normal.lastOperatingPoint().isErr());
  CHECK(sharp.currentPanel().isOk());
  CHECK(sharp.solveAlpha(Degrees{2.0}).isOk());
  CHECK(sharp.lastOperatingPoint().isOk());
}

TEST(integration, LibrarySessionL3ErrorsCancellationAndNonConvergence) {
  Session session;
  CHECK(session.solveAlpha(Degrees{2.0}).isErr());
  CHECK(session.solveAlphaSequence(alphaSequence(0.0, 2.0, 1.0)).isErr());
  CHECK(session.currentPanel().isErr());

  CHECK(session.setNaca("0012").isOk());
  CHECK(session.solveAlpha(Degrees{2.0}).isErr());
  CHECK(session.configure(inviscidConfig()).isOk());
  CHECK(session.solveAlpha(Degrees{std::nan("")}).isErr());
  CHECK(session.solveCl(std::nan("")).isErr());
  CHECK(session.solveAlphaSequence(alphaSequence(0.0, 2.0, 0.0)).isErr());
  CHECK(session.solveAlphaSequence(alphaSequence(2.0, 0.0, 1.0)).isErr());
  CHECK(session.currentPanel().isOk());

  CancellationToken token;
  token.requestCancellation();
  const Result<SequenceResult> cancelled =
      session.solveAlphaSequence(alphaSequence(0.0, 3.0, 1.0), &token);
  CHECK(cancelled.isOk());
  CHECK(cancelled.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(cancelled.value().points.size(), std::size_t{1});
  CHECK(cancelled.value().points[0].convergence ==
        ConvergenceStatus::cancelled);

  AnalysisConfig limited = viscousConfig();
  limited.maximumIterations = 1;
  CHECK(session.configure(limited).isOk());
  const Result<SequenceResult> partial =
      session.solveAlphaSequence(alphaSequence(12.0, 14.0, 1.0));
  CHECK(partial.isOk());
  CHECK_EQ(partial.value().points.size(), std::size_t{2});
  CHECK(partial.value().stopReason == SequenceStopReason::completed);
  CHECK(containsUnconverged(partial.value()));

  CHECK(session.currentPanel().isOk());
  CHECK(session.solveAlpha(Degrees{2.0}).isOk());
}

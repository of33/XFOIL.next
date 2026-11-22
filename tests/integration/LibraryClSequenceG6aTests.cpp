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

// File: tests/integration/LibraryClSequenceG6aTests.cpp
// Public Session CL sequence integration with real NACA geometry.

#include <cstddef>
#include <optional>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Progress.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"

namespace {

using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::LiftSequence;
using xfoil::PanelingOptions;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::SequenceStopReason;
using xfoil::Session;

void configureInviscid(Session& session, const char* designation = "0012") {
  CHECK(session.setNaca(designation).isOk());
  PanelingOptions options;
  options.panelCount = 120;
  CHECK(session.generatePanel(options).isOk());
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 50;
  CHECK(session.configure(config).isOk());
}

void configureViscous(Session& session, int iterations = 100) {
  CHECK(session.setNaca("2412").isOk());
  PanelingOptions options;
  options.panelCount = 140;
  CHECK(session.generatePanel(options).isOk());
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = iterations;
  CHECK(session.configure(config).isOk());
}

}  // namespace

TEST(integration, LibraryClSequenceG6aNaca0012InviscidAscending) {
  Session session;
  configureInviscid(session);
  const auto result = session.solveClSequence(LiftSequence{0.4, 0.8, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  CHECK(result.value().stopReason == SequenceStopReason::completed);
  CHECK_NEAR(result.value().points.front().liftCoefficient, 0.4, 1.0e-8, 0.0);
  CHECK_NEAR(result.value().points.back().liftCoefficient, 0.6, 1.0e-8, 0.0);
}

TEST(integration, LibraryClSequenceG6aNaca0012InviscidDescending) {
  Session session;
  configureInviscid(session);
  const auto result = session.solveClSequence(LiftSequence{0.8, 0.4, -0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  CHECK_NEAR(result.value().points.front().liftCoefficient, 0.8, 1.0e-8, 0.0);
  CHECK_NEAR(result.value().points.back().liftCoefficient, 0.6, 1.0e-8, 0.0);
}

TEST(integration, LibraryClSequenceG6aNaca2412ViscousDetailed) {
  Session session;
  configureViscous(session);
  const auto result =
      session.solveDetailedClSequence(LiftSequence{0.4, 0.8, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  for (const auto& point : result.value().points) {
    CHECK(point.polar.convergence == xfoil::ConvergenceStatus::converged);
    CHECK(point.polar.dragCoefficientFriction.has_value());
    CHECK(point.operatingPoint.has_value());
    if (point.operatingPoint.has_value()) {
      CHECK(point.operatingPoint->surface.has_value());
      CHECK(point.operatingPoint->skinFriction.has_value());
    }
  }
}

TEST(integration, LibraryClSequenceG6aSinglePointMatchesSolveCl) {
  const LiftSequence sequence{0.4, 0.6, 0.2};
  Session sequenceSession;
  configureInviscid(sequenceSession);
  const auto sequenceResult = sequenceSession.solveClSequence(sequence);
  CHECK(sequenceResult.isOk());

  Session pointSession;
  configureInviscid(pointSession);
  const auto point = pointSession.solveCl(0.4);
  CHECK(point.isOk());
  if (sequenceResult.isErr() || point.isErr()) {
    return;
  }
  CHECK_EQ(sequenceResult.value().points.size(), std::size_t{1});
  const auto& row = sequenceResult.value().points.front();
  CHECK_NEAR(row.alpha.value, point.value().alpha.value, 1.0e-12, 0.0);
  CHECK_NEAR(row.liftCoefficient, point.value().liftCoefficient, 1.0e-12, 0.0);
  CHECK_NEAR(row.dragCoefficient, point.value().dragCoefficient, 1.0e-12, 0.0);
}

TEST(integration, LibraryClSequenceG6aNonConvergenceKeepsPartialPoints) {
  Session session;
  configureViscous(session, 1);
  const auto result =
      session.solveDetailedClSequence(LiftSequence{0.4, 1.0, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{3});
  CHECK(result.value().stopReason == SequenceStopReason::completed);
  for (const auto& point : result.value().points) {
    CHECK(point.polar.convergence ==
          xfoil::ConvergenceStatus::unconverged);
    CHECK(point.operatingPoint.has_value());
  }
}

TEST(integration, LibraryClSequenceG6aProgressAndCancellation) {
  Session session;
  configureInviscid(session);
  CancellationToken token;
  std::size_t sequenceEvents = 0;
  const auto progress = [&](const ProgressEvent& event) {
    if (event.phase != ProgressPhase::sequence) {
      return;
    }
    ++sequenceEvents;
    if (event.completed == 1) {
      token.requestCancellation();
    }
  };
  const auto result = session.solveClSequence(
      LiftSequence{0.4, 0.8, 0.2}, &token, progress);
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK(sequenceEvents >= 2);
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().stoppedAtIndex.value(), std::size_t{1});
}

TEST(integration, LibraryClSequenceG6aOwningAfterSessionResetAndIsolation) {
  Session first;
  configureViscous(first);
  const auto detailed =
      first.solveDetailedClSequence(LiftSequence{0.4, 0.6, 0.2});
  CHECK(detailed.isOk());
  if (detailed.isErr()) {
    return;
  }
  first.reset();
  CHECK(!detailed.value().points.empty());
  CHECK(detailed.value().points.front().operatingPoint.has_value());
  if (detailed.value().points.front().operatingPoint.has_value() &&
      detailed.value().points.front().operatingPoint->surface.has_value()) {
    CHECK(!detailed.value().points.front().operatingPoint->surface->cp.empty());
  }

  Session second;
  configureViscous(second);
  const auto secondResult =
      second.solveDetailedClSequence(LiftSequence{0.4, 0.6, 0.2});
  CHECK(secondResult.isOk());
  if (secondResult.isErr()) {
    return;
  }
  CHECK_EQ(secondResult.value().points.size(),
           detailed.value().points.size());
}

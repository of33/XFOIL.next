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

// File: tests/unit/LibraryClSequenceG6aTests.cpp
// Public compact and detailed CL sequence contract tests.

#include <cmath>
#include <cstddef>
#include <limits>
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
using xfoil::DetailedSequenceResult;
using xfoil::LiftSequence;
using xfoil::PanelingOptions;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::Session;

void configureInviscid(Session& session, const char* designation = "0012",
                       std::size_t panels = 100) {
  CHECK(session.setNaca(designation).isOk());
  PanelingOptions options;
  options.panelCount = panels;
  CHECK(session.generatePanel(options).isOk());
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 50;
  CHECK(session.configure(config).isOk());
}

void configureViscous(Session& session, const char* designation = "2412",
                      int iterations = 100, std::size_t panels = 140) {
  CHECK(session.setNaca(designation).isOk());
  PanelingOptions options;
  options.panelCount = panels;
  CHECK(session.generatePanel(options).isOk());
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = iterations;
  CHECK(session.configure(config).isOk());
}

bool sameEvent(const ProgressEvent& left, const ProgressEvent& right) {
  return left.phase == right.phase && left.completed == right.completed &&
         left.total == right.total && left.fraction == right.fraction &&
         left.sequenceIndex == right.sequenceIndex &&
         left.iteration == right.iteration;
}

}  // namespace

TEST(unit, LibraryClSequenceG6aRequestDefaultsAreRejected) {
  Session session;
  configureInviscid(session);
  const auto result = session.solveClSequence(LiftSequence{});
  CHECK(result.isErr());
  CHECK_EQ(result.error().code, std::string{"sequence.zeroStep"});
}

TEST(unit, LibraryClSequenceG6aPositiveAndNegativeExclusiveRanges) {
  Session positive;
  configureInviscid(positive);
  const LiftSequence up{0.4, 0.8, 0.2};
  const auto ascending = positive.solveClSequence(up);
  CHECK(ascending.isOk());
  if (ascending.isErr()) {
    return;
  }
  CHECK_EQ(ascending.value().points.size(), std::size_t{2});
  CHECK(ascending.value().stopReason == SequenceStopReason::completed);
  CHECK_NEAR(ascending.value().points[0].liftCoefficient, 0.4, 1.0e-8, 0.0);
  CHECK_NEAR(ascending.value().points[1].liftCoefficient, 0.6, 1.0e-8, 0.0);
  CHECK(ascending.value().points[0].alpha.value <
        ascending.value().points[1].alpha.value);

  Session negative;
  configureInviscid(negative);
  const LiftSequence down{0.8, 0.4, -0.2};
  const auto descending = negative.solveClSequence(down);
  CHECK(descending.isOk());
  if (descending.isErr()) {
    return;
  }
  CHECK_EQ(descending.value().points.size(), std::size_t{2});
  CHECK_NEAR(descending.value().points[0].liftCoefficient, 0.8, 1.0e-8, 0.0);
  CHECK_NEAR(descending.value().points[1].liftCoefficient, 0.6, 1.0e-8, 0.0);
}

TEST(unit, LibraryClSequenceG6aSinglePointAndInvalidRanges) {
  Session session;
  configureInviscid(session);
  const auto single = session.solveClSequence(LiftSequence{0.4, 0.6, 0.2});
  CHECK(single.isOk());
  if (single.isOk()) {
    CHECK_EQ(single.value().points.size(), std::size_t{1});
    CHECK_NEAR(single.value().points[0].liftCoefficient, 0.4, 1.0e-8, 0.0);
  }

  const auto wrongDirection =
      session.solveClSequence(LiftSequence{0.4, 0.8, -0.2});
  CHECK(wrongDirection.isErr());
  CHECK_EQ(wrongDirection.error().code, std::string{"sequence.direction"});

  const auto equalRange =
      session.solveClSequence(LiftSequence{0.4, 0.4, 0.2});
  CHECK(equalRange.isErr());
  CHECK_EQ(equalRange.error().code, std::string{"sequence.direction"});

  const auto nonFinite = session.solveClSequence(
      LiftSequence{0.4, std::numeric_limits<double>::quiet_NaN(), 0.2});
  CHECK(nonFinite.isErr());
  CHECK_EQ(nonFinite.error().code, std::string{"sequence.invalidRange"});

  const auto tooMany = session.solveClSequence(LiftSequence{0.0, 100001.0, 1.0});
  CHECK(tooMany.isErr());
  CHECK_EQ(tooMany.error().code, std::string{"sequence.pointLimit"});
}

TEST(unit, LibraryClSequenceG6aCompactScalarProjection) {
  Session session;
  configureInviscid(session);
  const auto result = session.solveClSequence(LiftSequence{0.4, 0.8, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  for (const xfoil::PolarPoint& point : result.value().points) {
    CHECK(point.convergence == xfoil::ConvergenceStatus::converged);
    CHECK(point.dragCoefficientPressure.has_value());
    CHECK(point.dragCoefficientFriction == std::nullopt);
    CHECK(point.dragCoefficientViscous.has_value());
    CHECK(point.iterations.has_value());
    CHECK_EQ(*point.iterations, std::size_t{0});
  }
}

TEST(unit, LibraryClSequenceG6aDetailedInviscidOwnsSurfaceOnly) {
  Session session;
  configureInviscid(session);
  const auto result =
      session.solveDetailedClSequence(LiftSequence{0.4, 0.8, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  for (const xfoil::DetailedSequencePoint& point : result.value().points) {
    CHECK(point.operatingPoint.has_value());
    if (!point.operatingPoint.has_value()) {
      continue;
    }
    CHECK(point.operatingPoint->surface.has_value());
    CHECK(!point.operatingPoint->skinFriction.has_value());
    CHECK(!point.operatingPoint->dragCoefficientFriction.has_value());
  }
}

TEST(unit, LibraryClSequenceG6aDetailedViscousOwnsCpCfAndCdf) {
  Session session;
  configureViscous(session);
  const auto result =
      session.solveDetailedClSequence(LiftSequence{0.4, 0.6, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{1});
  const auto& point = result.value().points.front();
  CHECK(point.polar.dragCoefficientFriction.has_value());
  CHECK(point.polar.iterations.has_value());
  CHECK(point.operatingPoint.has_value());
  if (!point.operatingPoint.has_value()) {
    return;
  }
  CHECK(point.operatingPoint->surface.has_value());
  CHECK(point.operatingPoint->skinFriction.has_value());
  CHECK(point.operatingPoint->dragCoefficientFriction.has_value());
  if (point.operatingPoint->skinFriction.has_value()) {
    CHECK(!point.operatingPoint->skinFriction->upper.cf.empty());
    CHECK(!point.operatingPoint->skinFriction->lower.cf.empty());
  }
}

TEST(unit, LibraryClSequenceG6aCompactAndDetailedShareScalars) {
  Session compactSession;
  configureInviscid(compactSession);
  const LiftSequence sequence{0.4, 0.8, 0.2};
  const auto compact = compactSession.solveClSequence(sequence);
  CHECK(compact.isOk());

  Session detailedSession;
  configureInviscid(detailedSession);
  const auto detailed = detailedSession.solveDetailedClSequence(sequence);
  CHECK(detailed.isOk());
  if (compact.isErr() || detailed.isErr()) {
    return;
  }
  CHECK_EQ(compact.value().points.size(), detailed.value().points.size());
  for (std::size_t index = 0;
       index < compact.value().points.size() &&
       index < detailed.value().points.size();
       ++index) {
    const auto& left = compact.value().points[index];
    const auto& right = detailed.value().points[index].polar;
    CHECK_NEAR(left.alpha.value, right.alpha.value, 1.0e-12, 0.0);
    CHECK_NEAR(left.liftCoefficient, right.liftCoefficient, 1.0e-12, 0.0);
    CHECK_NEAR(left.dragCoefficient, right.dragCoefficient, 1.0e-12, 0.0);
    CHECK(left.iterations == right.iterations);
    CHECK(left.convergence == right.convergence);
  }
}

TEST(unit, LibraryClSequenceG6aProgressIsSequenceIndexed) {
  Session session;
  configureInviscid(session);
  std::vector<ProgressEvent> events;
  const auto progress = [&events](const ProgressEvent& event) {
    events.push_back(event);
  };
  const auto result =
      session.solveClSequence(LiftSequence{0.4, 0.8, 0.2}, nullptr, progress);
  CHECK(result.isOk());
  std::size_t sequenceEvents = 0;
  std::size_t lastCompleted = 0;
  for (const ProgressEvent& event : events) {
    if (event.phase != ProgressPhase::sequence) {
      continue;
    }
    ++sequenceEvents;
    CHECK(event.total.has_value());
    CHECK_EQ(*event.total, std::size_t{2});
    CHECK(event.sequenceIndex.has_value());
    lastCompleted = event.completed;
    if (event.completed == 0) {
      CHECK_EQ(*event.sequenceIndex, std::size_t{0});
    }
  }
  CHECK(sequenceEvents >= 3);
  CHECK_EQ(lastCompleted, std::size_t{2});
}

TEST(unit, LibraryClSequenceG6aCompactAndDetailedProgressMatch) {
  const LiftSequence sequence{0.4, 0.8, 0.2};
  std::vector<ProgressEvent> compactEvents;
  std::vector<ProgressEvent> detailedEvents;
  {
    Session session;
    configureInviscid(session);
    const auto result = session.solveClSequence(
        sequence, nullptr,
        [&compactEvents](const ProgressEvent& event) {
          compactEvents.push_back(event);
        });
    CHECK(result.isOk());
  }
  {
    Session session;
    configureInviscid(session);
    const auto result = session.solveDetailedClSequence(
        sequence, nullptr,
        [&detailedEvents](const ProgressEvent& event) {
          detailedEvents.push_back(event);
        });
    CHECK(result.isOk());
  }
  CHECK_EQ(compactEvents.size(), detailedEvents.size());
  for (std::size_t index = 0;
       index < compactEvents.size() && index < detailedEvents.size();
       ++index) {
    CHECK(sameEvent(compactEvents[index], detailedEvents[index]));
  }
}

TEST(unit, LibraryClSequenceG6aCancellationBeforeFirstPoint) {
  Session session;
  configureInviscid(session);
  CancellationToken token;
  token.requestCancellation();
  const auto result = session.solveClSequence(LiftSequence{0.4, 0.8, 0.2},
                                               &token);
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{1});
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().stoppedAtIndex.value(), std::size_t{0});
  CHECK(result.value().points.front().convergence ==
        xfoil::ConvergenceStatus::cancelled);
  CHECK(!result.value().points.front().dragCoefficientFriction.has_value());
  CHECK(!result.value().points.front().iterations.has_value());
}

TEST(unit, LibraryClSequenceG6aCancellationAfterPointsKeepsPartialResult) {
  Session session;
  configureInviscid(session);
  CancellationToken token;
  const auto progress = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::sequence && event.completed == 1) {
      token.requestCancellation();
    }
  };
  const auto result =
      session.solveClSequence(LiftSequence{0.4, 0.8, 0.2}, &token, progress);
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().stoppedAtIndex.value(), std::size_t{1});
  CHECK(result.value().points.front().convergence ==
        xfoil::ConvergenceStatus::converged);
  CHECK(result.value().points.back().convergence ==
        xfoil::ConvergenceStatus::cancelled);

  token.reset();
  const auto reused = session.solveClSequence(LiftSequence{0.4, 0.6, 0.2});
  CHECK(reused.isOk());
}

TEST(unit, LibraryClSequenceG6aCancellationDuringViscalLeavesStaleDataEmpty) {
  Session session;
  configureViscous(session);
  CancellationToken token;
  const auto progress = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration &&
        event.sequenceIndex.has_value() &&
        *event.sequenceIndex == 0) {
      token.requestCancellation();
    }
  };
  const auto result = session.solveDetailedClSequence(
      LiftSequence{0.4, 0.6, 0.2}, &token, progress);
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{1});
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  const auto& point = result.value().points.front();
  CHECK(point.polar.convergence == xfoil::ConvergenceStatus::cancelled);
  CHECK(!point.operatingPoint.has_value());
  CHECK(!point.polar.dragCoefficientFriction.has_value());
  CHECK(!point.polar.iterations.has_value());
}

TEST(unit, LibraryClSequenceG6aCallbackFailureIsStructured) {
  Session session;
  configureInviscid(session);
  const auto result = session.solveClSequence(
      LiftSequence{0.4, 0.8, 0.2}, nullptr,
      [](const ProgressEvent&) { throw 1; });
  CHECK(result.isErr());
  CHECK_EQ(result.error().code, std::string{"progress.callbackFailure"});

  const auto reused =
      session.solveClSequence(LiftSequence{0.4, 0.6, 0.2});
  CHECK(reused.isOk());
}

TEST(unit, LibraryClSequenceG6aNonConvergenceKeepsExecutedPoints) {
  Session session;
  configureViscous(session, "2412", 1, 100);
  const auto result = session.solveDetailedClSequence(
      LiftSequence{0.4, 1.0, 0.2});
  CHECK(result.isOk());
  if (result.isErr()) {
    return;
  }
  CHECK_EQ(result.value().points.size(), std::size_t{3});
  CHECK(result.value().stopReason == SequenceStopReason::completed);
  for (const auto& point : result.value().points) {
    CHECK(point.polar.convergence == xfoil::ConvergenceStatus::unconverged);
    CHECK(point.operatingPoint.has_value());
    if (point.operatingPoint.has_value()) {
      CHECK(point.operatingPoint->surface.has_value());
    }
  }
}

TEST(unit, LibraryClSequenceG6aResultsAreOwningAfterSessionReset) {
  Session session;
  configureViscous(session);
  const auto detailed = session.solveDetailedClSequence(
      LiftSequence{0.4, 0.6, 0.2});
  CHECK(detailed.isOk());
  if (detailed.isErr()) {
    return;
  }
  session.reset();
  CHECK(!detailed.value().points.empty());
  CHECK(detailed.value().points.front().operatingPoint.has_value());
  if (detailed.value().points.front().operatingPoint.has_value() &&
      detailed.value().points.front().operatingPoint->surface.has_value()) {
    CHECK(!detailed.value().points.front().operatingPoint->surface->cp.empty());
  }
}

TEST(unit, LibraryClSequenceG6aDeterministicRepeatedRuns) {
  const LiftSequence sequence{0.4, 0.8, 0.2};
  Session first;
  configureInviscid(first);
  Session second;
  configureInviscid(second);
  const auto left = first.solveClSequence(sequence);
  const auto right = second.solveClSequence(sequence);
  CHECK(left.isOk());
  CHECK(right.isOk());
  if (left.isErr() || right.isErr()) {
    return;
  }
  CHECK_EQ(left.value().points.size(), right.value().points.size());
  for (std::size_t index = 0;
       index < left.value().points.size() &&
       index < right.value().points.size();
       ++index) {
    CHECK_NEAR(left.value().points[index].alpha.value,
               right.value().points[index].alpha.value, 0.0, 0.0);
    CHECK_NEAR(left.value().points[index].liftCoefficient,
               right.value().points[index].liftCoefficient, 0.0, 0.0);
  }
}

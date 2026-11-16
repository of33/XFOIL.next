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

// File: tests/unit/LibraryProgressL12Tests.cpp
// Public progress event, callback failure and cancellation tests.

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using AirfoilGeometry = xfoil::geometry::AirfoilGeometry;
using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::ProgressCallback;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::Session;
using xfoil::SurfaceGeometry;

struct EventLog {
  std::vector<ProgressEvent> events;

  ProgressCallback callback() {
    return [this](const ProgressEvent& event) { events.push_back(event); };
  }
};

AirfoilGeometry diamond() {
  const std::vector<xfoil::Point2D> points = {
      {1.0, 0.0},   {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  AirfoilGeometry geometry;
  const auto assigned = geometry.assign("diamond", points);
  CHECK(assigned.success());
  return geometry;
}

AnalysisConfig inviscidConfig(int iterations = 40) {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = iterations;
  return config;
}

AnalysisConfig viscousConfig(int iterations = 50) {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = iterations;
  return config;
}

void makeReadyInviscid(Session& session, std::string_view designation,
                       std::size_t panelCount = 100) {
  CHECK(session.setNaca(designation).isOk());
  PanelingOptions options;
  options.panelCount = panelCount;
  CHECK(session.generatePanel(options).isOk());
  CHECK(session.configure(inviscidConfig()).isOk());
}

void makeReadyViscous(Session& session, std::string_view designation,
                      std::size_t panelCount = 120) {
  CHECK(session.setNaca(designation).isOk());
  PanelingOptions options;
  options.panelCount = panelCount;
  CHECK(session.generatePanel(options).isOk());
  CHECK(session.configure(viscousConfig()).isOk());
}

std::size_t countPhase(const EventLog& log, ProgressPhase phase) {
  std::size_t count = 0;
  for (const ProgressEvent& event : log.events) {
    if (event.phase == phase) {
      ++count;
    }
  }
  return count;
}

bool samePoint(const OperatingPointData& first,
               const OperatingPointData& second) {
  return first.alpha.value == second.alpha.value &&
         first.liftCoefficient == second.liftCoefficient &&
         first.dragCoefficient == second.dragCoefficient &&
         first.momentCoefficient == second.momentCoefficient &&
         first.centerOfPressureX == second.centerOfPressureX &&
         first.iterations == second.iterations &&
         first.convergence == second.convergence && first.viscous == second.viscous;
}

}  // namespace

TEST(unit, LibraryProgressL12EventValueSemantics) {
  static_assert(std::is_copy_constructible_v<ProgressEvent>);
  static_assert(std::is_copy_assignable_v<ProgressEvent>);
  static_assert(std::is_move_constructible_v<ProgressEvent>);
  static_assert(std::is_nothrow_move_constructible_v<ProgressEvent>);
  static_assert(std::is_aggregate_v<ProgressEvent>);

  ProgressEvent event;
  CHECK(event.phase == ProgressPhase::none);
  CHECK_EQ(event.completed, std::size_t{0});
  CHECK(!event.total.has_value());
  CHECK(!event.fraction.has_value());
  CHECK(!event.sequenceIndex.has_value());
  CHECK(!event.iteration.has_value());

  event.phase = ProgressPhase::viscousIteration;
  event.completed = 3;
  event.total = 10;
  event.fraction = 0.3;
  event.sequenceIndex = 2;
  event.iteration = 2;
  ProgressEvent copy = event;
  CHECK(copy.phase == ProgressPhase::viscousIteration);
  CHECK_EQ(copy.completed, std::size_t{3});
  CHECK_EQ(*copy.total, std::size_t{10});
  CHECK_NEAR(*copy.fraction, 0.3, 0.0, 0.0);
  CHECK_EQ(*copy.sequenceIndex, std::size_t{2});
  CHECK_EQ(*copy.iteration, std::size_t{2});
}

TEST(unit, LibraryProgressL12CoarseGeometryAndPaneling) {
  Session session;
  EventLog geometryLog;
  CHECK(session.setGeometry(diamond(), geometryLog.callback()).isOk());
  CHECK_EQ(geometryLog.events.size(), std::size_t{2});
  CHECK(geometryLog.events[0].phase == ProgressPhase::geometry);
  CHECK_EQ(geometryLog.events[0].completed, std::size_t{0});
  CHECK_EQ(*geometryLog.events[0].total, std::size_t{1});
  CHECK_EQ(*geometryLog.events[0].fraction, 0.0);
  CHECK(geometryLog.events[1].phase == ProgressPhase::geometry);
  CHECK_EQ(geometryLog.events[1].completed, std::size_t{1});
  CHECK_EQ(*geometryLog.events[1].fraction, 1.0);

  EventLog nacaLog;
  CHECK(session.setNaca("0012", nacaLog.callback()).isOk());
  CHECK_EQ(countPhase(nacaLog, ProgressPhase::geometry), std::size_t{2});
  EventLog nacsLog;
  CHECK(session.setNacs("0012", nacsLog.callback()).isOk());
  CHECK_EQ(countPhase(nacsLog, ProgressPhase::geometry), std::size_t{2});

  EventLog panelLog;
  PanelingOptions options;
  options.panelCount = 100;
  CHECK(session.generatePanel(options, panelLog.callback()).isOk());
  CHECK_EQ(panelLog.events.size(), std::size_t{2});
  CHECK(panelLog.events[0].phase == ProgressPhase::paneling);
  CHECK_EQ(panelLog.events[0].completed, std::size_t{0});
  CHECK(panelLog.events[1].phase == ProgressPhase::paneling);
  CHECK_EQ(panelLog.events[1].completed, std::size_t{1});
}

TEST(unit, LibraryProgressL12CoarseErrorHasNoCompletion) {
  Session session;
  EventLog geometryLog;
  AirfoilGeometry empty;
  const Result<void> geometry =
      session.setGeometry(empty, geometryLog.callback());
  CHECK(geometry.isErr());
  CHECK_EQ(geometryLog.events.size(), std::size_t{1});
  CHECK_EQ(geometryLog.events[0].completed, std::size_t{0});

  CHECK(session.setNaca("0012").isOk());
  EventLog panelLog;
  PanelingOptions invalid;
  invalid.panelCount = 1;
  const Result<SurfaceGeometry> panel =
      session.generatePanel(invalid, panelLog.callback());
  CHECK(panel.isErr());
  CHECK_EQ(panelLog.events.size(), std::size_t{1});
  CHECK_EQ(panelLog.events[0].completed, std::size_t{0});
}

TEST(unit, LibraryProgressL12InviscidPhasesAndNoNumericChange) {
  Session baseline;
  Session observed;
  makeReadyInviscid(baseline, "0012");
  makeReadyInviscid(observed, "0012");

  const Result<OperatingPointData> baselineResult =
      baseline.solveAlpha(Degrees{2.0});
  CHECK(baselineResult.isOk());

  EventLog log;
  const Result<OperatingPointData> observedResult =
      observed.solveAlpha(Degrees{2.0}, nullptr, log.callback());
  CHECK(observedResult.isOk());
  CHECK(samePoint(baselineResult.value(), observedResult.value()));

  CHECK_EQ(log.events.size(), std::size_t{4});
  CHECK(log.events[0].phase == ProgressPhase::initialization);
  CHECK_EQ(log.events[0].completed, std::size_t{0});
  CHECK(log.events[1].phase == ProgressPhase::initialization);
  CHECK_EQ(log.events[1].completed, std::size_t{1});
  CHECK(log.events[2].phase == ProgressPhase::inviscidSolve);
  CHECK_EQ(log.events[2].completed, std::size_t{0});
  CHECK(log.events[3].phase == ProgressPhase::inviscidSolve);
  CHECK_EQ(log.events[3].completed, std::size_t{1});
  for (const ProgressEvent& event : log.events) {
    CHECK_EQ(*event.total, std::size_t{1});
    CHECK(!event.sequenceIndex.has_value());
    CHECK(!event.iteration.has_value());
  }

  Session clBaseline;
  Session clObserved;
  makeReadyInviscid(clBaseline, "0012");
  makeReadyInviscid(clObserved, "0012");
  const Result<OperatingPointData> clExpected = clBaseline.solveCl(0.4);
  CHECK(clExpected.isOk());
  EventLog clLog;
  const Result<OperatingPointData> clActual =
      clObserved.solveCl(0.4, nullptr, clLog.callback());
  CHECK(clActual.isOk());
  CHECK(samePoint(clExpected.value(), clActual.value()));
  CHECK_EQ(clLog.events.size(), std::size_t{4});
}

TEST(unit, LibraryProgressL12ViscousIterationEvents) {
  Session baseline;
  Session observed;
  makeReadyViscous(baseline, "0012");
  makeReadyViscous(observed, "0012");
  const Result<OperatingPointData> expected = baseline.solveAlpha(Degrees{2.0});
  CHECK(expected.isOk());

  EventLog log;
  const Result<OperatingPointData> actual =
      observed.solveAlpha(Degrees{2.0}, nullptr, log.callback());
  CHECK(actual.isOk());
  CHECK(samePoint(expected.value(), actual.value()));

  const std::size_t viscousCount =
      countPhase(log, ProgressPhase::viscousIteration);
  CHECK(viscousCount > 0);
  CHECK(log.events.front().phase == ProgressPhase::initialization);
  CHECK(log.events[1].phase == ProgressPhase::initialization);
  CHECK(log.events[2].phase == ProgressPhase::inviscidSolve);
  CHECK(log.events[3].phase == ProgressPhase::inviscidSolve);

  std::size_t expectedIteration = 0;
  for (const ProgressEvent& event : log.events) {
    if (event.phase != ProgressPhase::viscousIteration) {
      continue;
    }
    CHECK_EQ(event.completed, expectedIteration + 1);
    CHECK_EQ(*event.iteration, expectedIteration);
    CHECK_EQ(*event.total, std::size_t{50});
    CHECK_NEAR(*event.fraction,
               static_cast<double>(event.completed) / 50.0, 0.0, 0.0);
    CHECK(!event.sequenceIndex.has_value());
    ++expectedIteration;
  }
  CHECK_EQ(expectedIteration, viscousCount);
}

TEST(unit, LibraryProgressL12SequenceEvents) {
  Session session;
  makeReadyInviscid(session, "0012");

  EventLog log;
  AlphaSequence sequence;
  sequence.start = Degrees{0.0};
  sequence.endExclusive = Degrees{3.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence, nullptr, log.callback());
  CHECK(result.isOk());
  CHECK_EQ(result.value().points.size(), std::size_t{3});

  std::vector<ProgressEvent> sequenceEvents;
  for (const ProgressEvent& event : log.events) {
    if (event.phase == ProgressPhase::sequence) {
      sequenceEvents.push_back(event);
    }
  }
  CHECK_EQ(sequenceEvents.size(), std::size_t{4});
  CHECK_EQ(sequenceEvents.front().completed, std::size_t{0});
  CHECK_EQ(*sequenceEvents.front().total, std::size_t{3});
  CHECK_EQ(*sequenceEvents.front().fraction, 0.0);
  for (std::size_t index = 0; index < 3; ++index) {
    CHECK_EQ(sequenceEvents[index + 1].completed, index + 1);
    CHECK_EQ(*sequenceEvents[index + 1].sequenceIndex, index);
    CHECK_EQ(*sequenceEvents[index + 1].total, std::size_t{3});
  }

  bool sawInner = false;
  for (const ProgressEvent& event : log.events) {
    if (event.phase == ProgressPhase::initialization ||
        event.phase == ProgressPhase::inviscidSolve) {
      sawInner = true;
      CHECK(event.sequenceIndex.has_value());
      CHECK(*event.sequenceIndex < 3);
    }
  }
  CHECK(sawInner);

  EventLog descendingLog;
  AlphaSequence descending;
  descending.start = Degrees{2.0};
  descending.endExclusive = Degrees{-1.0};
  descending.step = Degrees{-1.0};
  const Result<SequenceResult> descendingResult =
      session.solveAlphaSequence(descending, nullptr, descendingLog.callback());
  CHECK(descendingResult.isOk());
  CHECK_EQ(descendingResult.value().points.size(), std::size_t{3});
  CHECK_EQ(countPhase(descendingLog, ProgressPhase::sequence),
           std::size_t{4});
}

TEST(unit, LibraryProgressL12ViscousSequenceCarriesIndex) {
  Session session;
  makeReadyViscous(session, "0012");
  EventLog log;
  AlphaSequence sequence;
  sequence.start = Degrees{0.0};
  sequence.endExclusive = Degrees{2.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence, nullptr, log.callback());
  CHECK(result.isOk());
  CHECK_EQ(result.value().points.size(), std::size_t{2});

  std::size_t viscousEvents = 0;
  for (const ProgressEvent& event : log.events) {
    if (event.phase != ProgressPhase::viscousIteration) {
      continue;
    }
    ++viscousEvents;
    CHECK(event.sequenceIndex.has_value());
    CHECK(*event.sequenceIndex < 2);
    CHECK(event.iteration.has_value());
  }
  CHECK(viscousEvents > 0);
}

TEST(unit, LibraryProgressL12CallbackFailureAtGeometryStart) {
  Session session;
  ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::geometry && event.completed == 0) {
      throw std::runtime_error{"geometry start"};
    }
  };
  const Result<void> failed = session.setGeometry(diamond(), throwing);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"progress.callbackFailure"});
  CHECK(failed.error().category == xfoil::ErrorCategory::invalidArgument);
  CHECK(session.currentPanel().isErr());

  CHECK(session.setNaca("0012").isOk());
  CHECK(session.setNaca("0012").isOk());
}

TEST(unit, LibraryProgressL12CallbackFailureAfterPaneling) {
  Session session;
  CHECK(session.setNaca("0012").isOk());
  ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::paneling && event.completed == 1) {
      throw std::runtime_error{"paneling end"};
    }
  };
  const Result<SurfaceGeometry> failed =
      session.generatePanel(PanelingOptions{}, throwing);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"progress.callbackFailure"});
  CHECK(session.currentPanel().isOk());
  CHECK(session.configure(inviscidConfig()).isOk());
  CHECK(session.solveAlpha(Degrees{1.0}).isOk());
}

TEST(unit, LibraryProgressL12CallbackFailureDuringViscousAndReuse) {
  Session session;
  makeReadyViscous(session, "0012");
  ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration) {
      throw std::runtime_error{"viscous iteration"};
    }
  };
  const Result<OperatingPointData> failed =
      session.solveAlpha(Degrees{2.0}, nullptr, throwing);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"progress.callbackFailure"});
  CHECK(session.lastOperatingPoint().isErr());
  CHECK(session.solveAlpha(Degrees{2.0}).isOk());
}

TEST(unit, LibraryProgressL12CancellationFromViscousCallback) {
  Session session;
  makeReadyViscous(session, "0012");
  CancellationToken token;
  ProgressCallback cancelling = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration &&
        event.iteration == 0) {
      token.requestCancellation();
    }
  };
  const Result<OperatingPointData> cancelled =
      session.solveAlpha(Degrees{2.0}, &token, cancelling);
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});
  CHECK(cancelled.error().category == xfoil::ErrorCategory::cancelled);
  CHECK(session.lastOperatingPoint().isErr());

  token.reset();
  CHECK(session.solveAlpha(Degrees{2.0}).isOk());
}

TEST(unit, LibraryProgressL12CancellationFromLaterViscousCallback) {
  Session session;
  makeReadyViscous(session, "0012");
  CancellationToken token;
  ProgressCallback cancelling = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration &&
        event.iteration == 2) {
      token.requestCancellation();
    }
  };
  const Result<OperatingPointData> cancelled =
      session.solveAlpha(Degrees{4.0}, &token, cancelling);
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});
  token.reset();
  CHECK(session.solveAlpha(Degrees{1.0}).isOk());
}

TEST(unit, LibraryProgressL12SequenceCancellationFromCallback) {
  Session session;
  makeReadyViscous(session, "0012");
  CancellationToken token;
  ProgressCallback cancelling = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::sequence && event.completed == 1) {
      token.requestCancellation();
    }
  };
  AlphaSequence sequence;
  sequence.start = Degrees{0.0};
  sequence.endExclusive = Degrees{3.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence, &token, cancelling);
  CHECK(result.isOk());
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  CHECK(result.value().points.back().convergence ==
        ConvergenceStatus::cancelled);

  token.reset();
  CHECK(session.solveAlpha(Degrees{1.0}).isOk());
}

TEST(unit, LibraryProgressL12SequenceCancellationMidViscous) {
  Session session;
  makeReadyViscous(session, "0012");
  CancellationToken token;
  ProgressCallback cancelling = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration &&
        event.sequenceIndex == 0 && event.iteration == 0) {
      token.requestCancellation();
    }
  };
  AlphaSequence sequence;
  sequence.start = Degrees{0.0};
  sequence.endExclusive = Degrees{3.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence, &token, cancelling);
  CHECK(result.isOk());
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().points.size(), std::size_t{1});
  CHECK(result.value().points.back().convergence ==
        ConvergenceStatus::cancelled);

  token.reset();
  CHECK(session.solveAlpha(Degrees{1.0}).isOk());
}

TEST(unit, LibraryProgressL12SessionsHaveIndependentCallbacks) {
  Session cancelledSession;
  Session otherSession;
  makeReadyInviscid(cancelledSession, "0012");
  makeReadyInviscid(otherSession, "0012");
  CancellationToken token;
  token.requestCancellation();
  EventLog cancelledLog;
  const Result<OperatingPointData> cancelled =
      cancelledSession.solveAlpha(Degrees{2.0}, &token,
                                  cancelledLog.callback());
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});
  CHECK(cancelledLog.events.empty());

  EventLog otherLog;
  const Result<OperatingPointData> other =
      otherSession.solveAlpha(Degrees{2.0}, nullptr, otherLog.callback());
  CHECK(other.isOk());
  CHECK_EQ(otherLog.events.size(), std::size_t{4});
}

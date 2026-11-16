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

// File: tests/integration/LibraryProgressL12Tests.cpp
// Integration coverage for stateful and one-shot progress reporting,
// cancellation and callback failure without numerical changes.

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using AirfoilGeometry = xfoil::geometry::AirfoilGeometry;
using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::AnalysisRequest;
using xfoil::CancellationToken;
using xfoil::Degrees;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::ProgressCallback;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::Session;

AirfoilGeometry diamond() {
  const std::vector<xfoil::Point2D> points = {
      {1.0, 0.0},   {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  AirfoilGeometry geometry;
  const auto assigned = geometry.assign("diamond", points);
  CHECK(assigned.success());
  return geometry;
}

AnalysisConfig inviscidConfig() {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 40;
  return config;
}

AnalysisConfig viscousConfig() {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 50;
  return config;
}

struct EventLog {
  std::vector<ProgressEvent> events;

  ProgressCallback callback() {
    return [this](const ProgressEvent& event) { events.push_back(event); };
  }
};

void makeReady(Session& session, const AnalysisConfig& config) {
  CHECK(session.setNaca("0012").isOk());
  PanelingOptions options;
  options.panelCount = 100;
  CHECK(session.generatePanel(options).isOk());
  CHECK(session.configure(config).isOk());
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

TEST(integration, LibraryProgressL12StatefulInviscidAndViscous) {
  Session inviscidExpected;
  Session inviscidObserved;
  makeReady(inviscidExpected, inviscidConfig());
  makeReady(inviscidObserved, inviscidConfig());
  const Result<OperatingPointData> expectedInviscid =
      inviscidExpected.solveAlpha(Degrees{2.0});
  CHECK(expectedInviscid.isOk());
  EventLog inviscidLog;
  const Result<OperatingPointData> observedInviscid =
      inviscidObserved.solveAlpha(Degrees{2.0}, nullptr,
                                  inviscidLog.callback());
  CHECK(observedInviscid.isOk());
  CHECK(samePoint(expectedInviscid.value(), observedInviscid.value()));
  CHECK(!inviscidLog.events.empty());

  Session viscousExpected;
  Session viscousObserved;
  makeReady(viscousExpected, viscousConfig());
  makeReady(viscousObserved, viscousConfig());
  const Result<OperatingPointData> expectedViscous =
      viscousExpected.solveAlpha(Degrees{2.0});
  CHECK(expectedViscous.isOk());
  EventLog viscousLog;
  const Result<OperatingPointData> observedViscous =
      viscousObserved.solveAlpha(Degrees{2.0}, nullptr,
                                 viscousLog.callback());
  CHECK(observedViscous.isOk());
  CHECK(samePoint(expectedViscous.value(), observedViscous.value()));

  bool sawViscous = false;
  for (const ProgressEvent& event : viscousLog.events) {
    if (event.phase == ProgressPhase::viscousIteration) {
      sawViscous = true;
      CHECK(event.iteration.has_value());
      CHECK(event.total.has_value());
      CHECK(event.fraction.has_value());
    }
  }
  CHECK(sawViscous);
}

TEST(integration, LibraryProgressL12OneShotAlphaAndCl) {
  AnalysisRequest request;
  request.geometry = diamond();
  request.paneling.panelCount = 100;
  request.configuration = inviscidConfig();
  request.target = xfoil::AlphaTarget{Degrees{2.0}};

  const Result<OperatingPointData> expectedAlpha = xfoil::analyze(request);
  CHECK(expectedAlpha.isOk());
  EventLog alphaLog;
  const Result<OperatingPointData> observedAlpha =
      xfoil::analyze(request, nullptr, alphaLog.callback());
  CHECK(observedAlpha.isOk());
  CHECK(samePoint(expectedAlpha.value(), observedAlpha.value()));
  CHECK_EQ(alphaLog.events.size(), std::size_t{8});
  CHECK(alphaLog.events[0].phase == ProgressPhase::geometry);
  CHECK(alphaLog.events[2].phase == ProgressPhase::paneling);
  CHECK(alphaLog.events[4].phase == ProgressPhase::initialization);
  CHECK(alphaLog.events[6].phase == ProgressPhase::inviscidSolve);

  request.target = xfoil::LiftTarget{0.4};
  const Result<OperatingPointData> expectedCl = xfoil::analyze(request);
  CHECK(expectedCl.isOk());
  EventLog clLog;
  const Result<OperatingPointData> observedCl =
      xfoil::analyze(request, nullptr, clLog.callback());
  CHECK(observedCl.isOk());
  CHECK(samePoint(expectedCl.value(), observedCl.value()));
  CHECK_EQ(clLog.events.size(), std::size_t{8});
}

TEST(integration, LibraryProgressL12ViscousSequenceAndHostBar) {
  Session expectedSession;
  Session observedSession;
  makeReady(expectedSession, viscousConfig());
  makeReady(observedSession, viscousConfig());
  AlphaSequence sequence;
  sequence.start = Degrees{0.0};
  sequence.endExclusive = Degrees{2.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> expected =
      expectedSession.solveAlphaSequence(sequence);
  CHECK(expected.isOk());

  EventLog log;
  const Result<SequenceResult> observed =
      observedSession.solveAlphaSequence(sequence, nullptr, log.callback());
  CHECK(observed.isOk());
  CHECK_EQ(observed.value().points.size(), expected.value().points.size());
  for (std::size_t index = 0; index < expected.value().points.size();
       ++index) {
    CHECK_EQ(observed.value().points[index].alpha.value,
             expected.value().points[index].alpha.value);
    CHECK_EQ(observed.value().points[index].liftCoefficient,
             expected.value().points[index].liftCoefficient);
    CHECK_EQ(observed.value().points[index].dragCoefficient,
             expected.value().points[index].dragCoefficient);
  }

  std::map<std::pair<int, std::size_t>, double> lastFraction;
  std::size_t sequenceEvents = 0;
  for (const ProgressEvent& event : log.events) {
    if (event.fraction.has_value()) {
      CHECK(*event.fraction >= 0.0);
      CHECK(*event.fraction <= 1.0);
    }
    if (event.phase == ProgressPhase::sequence) {
      ++sequenceEvents;
      CHECK(event.sequenceIndex.has_value());
    }
    if (event.phase == ProgressPhase::viscousIteration ||
        event.phase == ProgressPhase::sequence) {
      CHECK(event.sequenceIndex.has_value());
      const auto key = std::make_pair(static_cast<int>(event.phase),
                                      *event.sequenceIndex);
      if (event.fraction.has_value()) {
        const auto found = lastFraction.find(key);
        if (found != lastFraction.end()) {
          CHECK(*event.fraction >= found->second);
        }
        lastFraction[key] = *event.fraction;
      }
    }
  }
  CHECK_EQ(sequenceEvents, observed.value().points.size() + 1);
}

TEST(integration, LibraryProgressL12CancellationAndCallbackFailure) {
  Session first;
  Session second;
  makeReady(first, viscousConfig());
  makeReady(second, viscousConfig());

  CancellationToken token;
  ProgressCallback cancelling = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration &&
        event.iteration == 0) {
      token.requestCancellation();
    }
  };
  const Result<OperatingPointData> cancelled =
      first.solveAlpha(Degrees{2.0}, &token, cancelling);
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});
  CHECK(first.lastOperatingPoint().isErr());
  token.reset();
  CHECK(second.solveAlpha(Degrees{2.0}).isOk());
  CHECK(first.solveAlpha(Degrees{1.0}).isOk());

  ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration) {
      throw std::runtime_error{"progress failure"};
    }
  };
  const Result<OperatingPointData> failed =
      second.solveAlpha(Degrees{2.0}, nullptr, throwing);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"progress.callbackFailure"});
  CHECK(second.lastOperatingPoint().isErr());
  CHECK(second.solveAlpha(Degrees{1.0}).isOk());
}

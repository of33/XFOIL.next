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

// File: tests/integration/LibrarySequencePointCompletenessTests.cpp
// End-to-end public sequence-point scalar completeness.

#include <cmath>
#include <cstddef>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::PolarPoint;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::Session;

AnalysisConfig viscousConfig(int iterations = 60) {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = iterations;
  return config;
}

AnalysisConfig inviscidConfig() {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 0.0;
  config.maximumIterations = 40;
  return config;
}

PanelingOptions paneling() {
  PanelingOptions options;
  options.panelCount = std::size_t{120};
  return options;
}

void makeReady(Session& session, const AnalysisConfig& config) {
  CHECK(session.setNaca("0012").isOk());
  CHECK(session.generatePanel(paneling()).isOk());
  CHECK(session.configure(config).isOk());
}

AlphaSequence sequence(double start, double end, double step) {
  AlphaSequence value;
  value.start = Degrees{start};
  value.endExclusive = Degrees{end};
  value.step = Degrees{step};
  return value;
}

void expectViscousScalars(const PolarPoint& point) {
  CHECK(point.dragCoefficientPressure.has_value());
  CHECK(point.dragCoefficientFriction.has_value());
  CHECK(point.dragCoefficientViscous.has_value());
  CHECK(point.iterations.has_value());
  CHECK(std::isfinite(point.dragCoefficientPressure.value()));
  CHECK(std::isfinite(point.dragCoefficientFriction.value()));
  CHECK(std::isfinite(point.dragCoefficientViscous.value()));
  CHECK(point.dragCoefficientFriction.value() >= 0.0);
  CHECK_NEAR(point.dragCoefficientViscous.value(),
             point.dragCoefficient, 0.0, 0.0);
  CHECK(point.iterations.value() > 0);
}

void expectInviscidScalars(const PolarPoint& point) {
  CHECK(point.dragCoefficientPressure.has_value());
  CHECK(std::isfinite(point.dragCoefficientPressure.value()));
  CHECK(!point.dragCoefficientFriction.has_value());
  CHECK(point.dragCoefficientViscous.has_value());
  CHECK_NEAR(point.dragCoefficientViscous.value(), 0.0, 0.0, 0.0);
  CHECK(point.iterations.has_value());
  CHECK_EQ(point.iterations.value(), std::size_t{0});
}

}  // namespace

TEST(integration, LibrarySequencePointCompletenessInviscidAlphaSequence) {
  Session session;
  makeReady(session, inviscidConfig());
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence(0.0, 3.0, 1.0));
  CHECK(result.isOk());
  const SequenceResult& value = result.value();
  CHECK(value.stopReason == SequenceStopReason::completed);
  CHECK_EQ(value.points.size(), std::size_t{3});
  for (const PolarPoint& point : value.points) {
    CHECK(point.convergence == ConvergenceStatus::converged);
    expectInviscidScalars(point);
  }
}

TEST(integration, LibrarySequencePointCompletenessViscousAlphaSequence) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence(0.0, 3.0, 1.0));
  CHECK(result.isOk());
  const SequenceResult& value = result.value();
  CHECK(value.stopReason == SequenceStopReason::completed);
  CHECK_EQ(value.points.size(), std::size_t{3});
  for (const PolarPoint& point : value.points) {
    CHECK(point.convergence == ConvergenceStatus::converged);
    expectViscousScalars(point);
  }
}

TEST(integration, LibrarySequencePointCompletenessMatchesSinglePoint) {
  Session sequenceSession;
  makeReady(sequenceSession, viscousConfig());
  const Result<SequenceResult> sequenceResult =
      sequenceSession.solveAlphaSequence(sequence(1.0, 2.0, 1.0));
  CHECK(sequenceResult.isOk());
  CHECK_EQ(sequenceResult.value().points.size(), std::size_t{1});
  const PolarPoint& sequencePoint = sequenceResult.value().points[0];

  Session singleSession;
  makeReady(singleSession, viscousConfig());
  const Result<OperatingPointData> single =
      singleSession.solveAlpha(Degrees{1.0});
  CHECK(single.isOk());
  CHECK(single.value().dragCoefficientPressure.has_value());
  CHECK(single.value().dragCoefficientFriction.has_value());
  CHECK_NEAR(sequencePoint.dragCoefficientPressure.value(),
             single.value().dragCoefficientPressure.value(), 0.0, 0.0);
  CHECK_NEAR(sequencePoint.dragCoefficientFriction.value(),
             single.value().dragCoefficientFriction.value(), 0.0, 0.0);
  CHECK_EQ(sequencePoint.iterations.value(),
           static_cast<std::size_t>(single.value().iterations));
  CHECK_NEAR(sequencePoint.dragCoefficientViscous.value(),
             single.value().dragCoefficient, 0.0, 0.0);
}

TEST(integration, LibrarySequencePointCompletenessPositiveAndNegative) {
  Session positive;
  makeReady(positive, inviscidConfig());
  const Result<SequenceResult> up =
      positive.solveAlphaSequence(sequence(0.0, 2.0, 1.0));
  CHECK(up.isOk());
  CHECK_EQ(up.value().points.size(), std::size_t{2});
  CHECK_NEAR(up.value().points[0].alpha.value, 0.0, 0.0, 0.0);
  CHECK_NEAR(up.value().points[1].alpha.value, 1.0, 0.0, 0.0);

  Session negative;
  makeReady(negative, inviscidConfig());
  const Result<SequenceResult> down =
      negative.solveAlphaSequence(sequence(2.0, -2.0, -1.0));
  CHECK(down.isOk());
  CHECK_EQ(down.value().points.size(), std::size_t{4});
  CHECK_NEAR(down.value().points[0].alpha.value, 2.0, 0.0, 0.0);
  CHECK_NEAR(down.value().points[1].alpha.value, 1.0, 0.0, 0.0);
  CHECK_NEAR(down.value().points[2].alpha.value, 0.0, 0.0, 0.0);
  CHECK_NEAR(down.value().points[3].alpha.value, -1.0, 0.0, 0.0);
  for (const PolarPoint& point : down.value().points) {
    expectInviscidScalars(point);
  }
}

TEST(integration, LibrarySequencePointCompletenessUnconvergedKeepsScalars) {
  Session session;
  makeReady(session, viscousConfig(1));
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence(0.0, 2.0, 1.0));
  CHECK(result.isOk());
  CHECK(result.value().points.size() >= std::size_t{1});
  const PolarPoint& first = result.value().points[0];
  CHECK(first.convergence == ConvergenceStatus::unconverged);
  CHECK(first.dragCoefficientPressure.has_value());
  CHECK(first.dragCoefficientFriction.has_value());
  CHECK(first.dragCoefficientViscous.has_value());
  CHECK(first.iterations.has_value());
  CHECK(first.iterations.value() >= std::size_t{1});
}

TEST(integration, LibrarySequencePointCompletenessCancelledHasNoScalars) {
  Session session;
  makeReady(session, viscousConfig());
  CancellationToken token;
  token.requestCancellation();
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence(0.0, 2.0, 1.0), &token);
  CHECK(result.isOk());
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().points.size(), std::size_t{1});
  const PolarPoint& point = result.value().points[0];
  CHECK(point.convergence == ConvergenceStatus::cancelled);
  CHECK(!point.dragCoefficientPressure.has_value());
  CHECK(!point.dragCoefficientFriction.has_value());
  CHECK(!point.dragCoefficientViscous.has_value());
  CHECK(!point.iterations.has_value());
}

TEST(integration, LibrarySequencePointCompletenessStatefulDeterministic) {
  const auto run = []() {
    Session session;
    makeReady(session, viscousConfig());
    return session.solveAlphaSequence(sequence(0.0, 3.0, 1.0));
  };

  const Result<SequenceResult> first = run();
  const Result<SequenceResult> second = run();
  CHECK(first.isOk());
  CHECK(second.isOk());
  CHECK_EQ(first.value().points.size(), second.value().points.size());
  if (first.value().points.size() == second.value().points.size()) {
    for (std::size_t index = 0; index < first.value().points.size(); ++index) {
      const PolarPoint& a = first.value().points[index];
      const PolarPoint& b = second.value().points[index];
      CHECK_NEAR(a.alpha.value, b.alpha.value, 0.0, 0.0);
      CHECK_NEAR(a.dragCoefficient, b.dragCoefficient, 0.0, 0.0);
      CHECK_NEAR(a.dragCoefficientPressure.value(),
                 b.dragCoefficientPressure.value(), 0.0, 0.0);
      CHECK_NEAR(a.dragCoefficientFriction.value(),
                 b.dragCoefficientFriction.value(), 0.0, 0.0);
      CHECK_NEAR(a.dragCoefficientViscous.value(),
                 b.dragCoefficientViscous.value(), 0.0, 0.0);
      CHECK_EQ(a.iterations.value(), b.iterations.value());
    }
  }

  SequenceResult owned = first.value();
  Session later;
  makeReady(later, viscousConfig());
  CHECK(later.solveAlpha(Degrees{4.0}).isOk());
  CHECK_NEAR(owned.points[0].dragCoefficientFriction.value(),
             first.value().points[0].dragCoefficientFriction.value(),
             0.0, 0.0);
}

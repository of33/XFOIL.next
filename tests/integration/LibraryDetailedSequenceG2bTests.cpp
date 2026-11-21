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

// File: tests/integration/LibraryDetailedSequenceG2bTests.cpp
// End-to-end detailed alpha sequences with owning Cp/Cf per executed point,
// compact parity, cancellation, progress, determinism and no file/console
// I/O.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/Types.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::DetailedSequenceResult;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::PolarPoint;
using xfoil::ProgressCallback;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::Session;
using xfoil::SurfaceDistributionKind;
using xfoil::SurfaceSide;

AnalysisConfig inviscidConfig() {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 80;
  return config;
}

AnalysisConfig viscousConfig() {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 100;
  return config;
}

AnalysisConfig unconvergedConfig() {
  AnalysisConfig config = viscousConfig();
  config.maximumIterations = 1;
  return config;
}

AlphaSequence sequence(double start, double endExclusive, double step) {
  AlphaSequence result;
  result.start = Degrees{start};
  result.endExclusive = Degrees{endExclusive};
  result.step = Degrees{step};
  return result;
}

void makeReady(Session& session, const AnalysisConfig& config) {
  CHECK(session.setNaca("2412").isOk());
  PanelingOptions paneling;
  paneling.panelCount = 100;
  CHECK(session.generatePanel(paneling).isOk());
  CHECK(session.configure(config).isOk());
}

bool samePolar(const PolarPoint& left, const PolarPoint& right) {
  return left.alpha.value == right.alpha.value &&
         left.liftCoefficient == right.liftCoefficient &&
         left.dragCoefficient == right.dragCoefficient &&
         left.momentCoefficient == right.momentCoefficient &&
         left.dragCoefficientPressure == right.dragCoefficientPressure &&
         left.dragCoefficientFriction == right.dragCoefficientFriction &&
         left.dragCoefficientViscous == right.dragCoefficientViscous &&
         left.iterations == right.iterations &&
         left.convergence == right.convergence;
}

bool sameEvent(const ProgressEvent& left, const ProgressEvent& right) {
  return left.phase == right.phase && left.completed == right.completed &&
         left.total == right.total && left.fraction == right.fraction &&
         left.sequenceIndex == right.sequenceIndex &&
         left.iteration == right.iteration;
}

bool sameEvents(const std::vector<ProgressEvent>& left,
                const std::vector<ProgressEvent>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (!sameEvent(left[index], right[index])) {
      return false;
    }
  }
  return true;
}

void appendScalars(std::vector<double>& values,
                   const OperatingPointData& point) {
  values.push_back(point.alpha.value);
  values.push_back(point.liftCoefficient);
  values.push_back(point.dragCoefficient);
  values.push_back(point.momentCoefficient);
  values.push_back(static_cast<double>(point.iterations));
  values.push_back(point.surface ? 1.0 : 0.0);
  values.push_back(point.skinFriction ? 1.0 : 0.0);
  if (point.dragCoefficientFriction.has_value()) {
    values.push_back(*point.dragCoefficientFriction);
  }
  if (point.surface.has_value()) {
    values.push_back(static_cast<double>(point.surface->cp.size()));
    values.insert(values.end(), point.surface->cp.begin(),
                  point.surface->cp.end());
  }
  if (point.skinFriction.has_value()) {
    values.insert(values.end(), point.skinFriction->upper.cf.begin(),
                  point.skinFriction->upper.cf.end());
    values.insert(values.end(), point.skinFriction->lower.cf.begin(),
                  point.skinFriction->lower.cf.end());
  }
}

std::vector<double> detailedValues(const DetailedSequenceResult& result) {
  std::vector<double> values;
  values.push_back(static_cast<double>(result.points.size()));
  values.push_back(static_cast<double>(result.stopReason));
  values.push_back(result.stoppedAtIndex.has_value() ? 1.0 : 0.0);
  if (result.stoppedAtIndex.has_value()) {
    values.push_back(static_cast<double>(*result.stoppedAtIndex));
  }
  for (const auto& point : result.points) {
    values.push_back(point.operatingPoint.has_value() ? 1.0 : 0.0);
    if (point.operatingPoint.has_value()) {
      appendScalars(values, *point.operatingPoint);
    }
  }
  return values;
}

}  // namespace

TEST(integration, LibraryDetailedSequenceG2bInvalidSequencesMatchCompactErrors) {
  Session session;
  const std::vector<AlphaSequence> invalid{
      sequence(0.0, 2.0, 0.0),
      sequence(2.0, 0.0, 1.0),
      sequence(0.0, 0.0, 1.0),
      sequence(std::nan(""), 2.0, 1.0)};
  for (const AlphaSequence& value : invalid) {
    const Result<SequenceResult> compact = session.solveAlphaSequence(value);
    const Result<DetailedSequenceResult> detailed =
        session.solveDetailedAlphaSequence(value);
    CHECK(compact.isErr());
    CHECK(detailed.isErr());
    CHECK_EQ(detailed.error().code, compact.error().code);
    CHECK(detailed.error().category == compact.error().category);
  }
}

TEST(integration, LibraryDetailedSequenceG2bInviscidOwnsCpPerPoint) {
  Session session;
  makeReady(session, inviscidConfig());
  const Result<DetailedSequenceResult> result =
      session.solveDetailedAlphaSequence(sequence(-1.0, 2.0, 1.0));
  CHECK(result.isOk());
  const DetailedSequenceResult& detailed = result.value();
  CHECK_EQ(detailed.points.size(), std::size_t{3});
  CHECK(detailed.stopReason == SequenceStopReason::completed);
  CHECK(!detailed.stoppedAtIndex.has_value());

  const double expectedAlpha[] = {-1.0, 0.0, 1.0};
  for (std::size_t index = 0; index < detailed.points.size(); ++index) {
    const auto& point = detailed.points[index];
    CHECK_NEAR(point.polar.alpha.value, expectedAlpha[index], 0.0, 0.0);
    CHECK(point.polar.convergence == ConvergenceStatus::converged);
    CHECK(point.operatingPoint.has_value());
    const OperatingPointData& detail = *point.operatingPoint;
    CHECK_NEAR(detail.alpha.value, point.polar.alpha.value, 0.0, 0.0);
    CHECK_NEAR(detail.liftCoefficient, point.polar.liftCoefficient, 0.0, 0.0);
    CHECK(detail.convergence == ConvergenceStatus::converged);
    CHECK(detail.surface.has_value());
    CHECK(detail.surface->kind == SurfaceDistributionKind::inviscid);
    CHECK(!detail.surface->cp.empty());
    CHECK_EQ(detail.surface->x.size(), detail.surface->cp.size());
    CHECK_EQ(detail.surface->y.size(), detail.surface->cp.size());
    CHECK(!detail.skinFriction.has_value());
    CHECK(!detail.dragCoefficientFriction.has_value());
    CHECK(!point.polar.dragCoefficientFriction.has_value());
  }
}

TEST(integration, LibraryDetailedSequenceG2bViscousOwnsCfAndCdfPerPoint) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<DetailedSequenceResult> result =
      session.solveDetailedAlphaSequence(sequence(0.0, 2.0, 1.0));
  CHECK(result.isOk());
  const DetailedSequenceResult& detailed = result.value();
  CHECK_EQ(detailed.points.size(), std::size_t{2});
  CHECK(detailed.stopReason == SequenceStopReason::completed);

  for (const auto& point : detailed.points) {
    CHECK(point.polar.convergence == ConvergenceStatus::converged);
    CHECK(point.polar.dragCoefficientFriction.has_value());
    CHECK(point.operatingPoint.has_value());
    const OperatingPointData& detail = *point.operatingPoint;
    CHECK(detail.viscous);
    CHECK(detail.convergence == ConvergenceStatus::converged);
    CHECK(detail.surface.has_value());
    CHECK(detail.surface->kind == SurfaceDistributionKind::viscous);
    CHECK(detail.dragCoefficientFriction.has_value());
    CHECK_NEAR(detail.dragCoefficientFriction.value(),
               point.polar.dragCoefficientFriction.value(), 0.0, 0.0);
    CHECK(detail.skinFriction.has_value());
    CHECK(detail.skinFriction->upper.side == SurfaceSide::upper);
    CHECK(detail.skinFriction->lower.side == SurfaceSide::lower);
    CHECK(!detail.skinFriction->upper.cf.empty());
    CHECK(!detail.skinFriction->lower.cf.empty());
    CHECK_EQ(detail.skinFriction->upper.cf.size(),
             detail.skinFriction->upper.s.size());
    CHECK_EQ(detail.skinFriction->lower.cf.size(),
             detail.skinFriction->lower.s.size());
    if (detail.dragCoefficientPressure.has_value()) {
      CHECK(std::fabs(detail.dragCoefficientFriction.value() -
                      (detail.dragCoefficient -
                       detail.dragCoefficientPressure.value())) > 1.0e-14);
    }
  }
}

TEST(integration, LibraryDetailedSequenceG2bExecutedNonConvergenceKeepsDetail) {
  Session session;
  makeReady(session, unconvergedConfig());
  const Result<DetailedSequenceResult> result =
      session.solveDetailedAlphaSequence(sequence(12.0, 14.0, 1.0));
  CHECK(result.isOk());
  const DetailedSequenceResult& detailed = result.value();
  CHECK_EQ(detailed.points.size(), std::size_t{2});
  CHECK(detailed.stopReason == SequenceStopReason::completed);
  CHECK(detailed.points[0].polar.convergence == ConvergenceStatus::unconverged);
  for (const auto& point : detailed.points) {
    CHECK(point.polar.convergence == ConvergenceStatus::unconverged);
    CHECK(point.operatingPoint.has_value());
    CHECK(point.operatingPoint->convergence ==
          ConvergenceStatus::unconverged);
    CHECK(point.operatingPoint->surface.has_value());
    CHECK(!point.operatingPoint->surface->cp.empty());
  }
}

TEST(integration, LibraryDetailedSequenceG2bCancellationBeforeAndAfterPoints) {
  Session before;
  makeReady(before, inviscidConfig());
  CancellationToken earlyToken;
  earlyToken.requestCancellation();
  const Result<DetailedSequenceResult> cancelledBefore =
      before.solveDetailedAlphaSequence(sequence(0.0, 3.0, 1.0),
                                        &earlyToken);
  CHECK(cancelledBefore.isOk());
  CHECK_EQ(cancelledBefore.value().points.size(), std::size_t{1});
  CHECK(cancelledBefore.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(cancelledBefore.value().stoppedAtIndex.value(), std::size_t{0});
  CHECK(cancelledBefore.value().points[0].polar.convergence ==
        ConvergenceStatus::cancelled);
  CHECK(!cancelledBefore.value().points[0].operatingPoint.has_value());

  Session after;
  makeReady(after, inviscidConfig());
  CancellationToken token;
  ProgressCallback callback = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::sequence && event.completed == 2) {
      token.requestCancellation();
    }
  };
  const Result<DetailedSequenceResult> cancelledAfter =
      after.solveDetailedAlphaSequence(sequence(0.0, 5.0, 1.0), &token,
                                       callback);
  CHECK(cancelledAfter.isOk());
  const DetailedSequenceResult& detailed = cancelledAfter.value();
  CHECK_EQ(detailed.points.size(), std::size_t{3});
  CHECK(detailed.stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(detailed.stoppedAtIndex.value(), std::size_t{2});
  CHECK(detailed.points[0].operatingPoint.has_value());
  CHECK(detailed.points[1].operatingPoint.has_value());
  CHECK(!detailed.points[2].operatingPoint.has_value());
  CHECK(detailed.points[2].polar.convergence ==
        ConvergenceStatus::cancelled);
}

TEST(integration, LibraryDetailedSequenceG2bCompactParityProgressAndDeterminism) {
  Session compactSession;
  Session detailedSession;
  Session repeatSession;
  makeReady(compactSession, inviscidConfig());
  makeReady(detailedSession, inviscidConfig());
  makeReady(repeatSession, inviscidConfig());
  const AlphaSequence values = sequence(-1.0, 2.0, 1.0);
  std::vector<ProgressEvent> compactEvents;
  std::vector<ProgressEvent> detailedEvents;
  std::vector<ProgressEvent> repeatEvents;
  ProgressCallback compactCallback = [&compactEvents](
                                         const ProgressEvent& event) {
    compactEvents.push_back(event);
  };
  ProgressCallback detailedCallback = [&detailedEvents](
                                          const ProgressEvent& event) {
    detailedEvents.push_back(event);
  };
  ProgressCallback repeatCallback = [&repeatEvents](
                                        const ProgressEvent& event) {
    repeatEvents.push_back(event);
  };

  const Result<SequenceResult> compact =
      compactSession.solveAlphaSequence(values, nullptr, compactCallback);
  const Result<DetailedSequenceResult> detailed =
      detailedSession.solveDetailedAlphaSequence(values, nullptr,
                                                 detailedCallback);
  const Result<DetailedSequenceResult> repeat =
      repeatSession.solveDetailedAlphaSequence(values, nullptr,
                                               repeatCallback);
  CHECK(compact.isOk());
  CHECK(detailed.isOk());
  CHECK(repeat.isOk());
  CHECK_EQ(detailed.value().points.size(), compact.value().points.size());
  CHECK(detailed.value().stopReason == compact.value().stopReason);
  CHECK(detailed.value().stoppedAtIndex == compact.value().stoppedAtIndex);
  for (std::size_t index = 0; index < compact.value().points.size(); ++index) {
    CHECK(samePolar(detailed.value().points[index].polar,
                    compact.value().points[index]));
  }
  CHECK(sameEvents(detailedEvents, compactEvents));
  CHECK(sameEvents(repeatEvents, detailedEvents));
  CHECK(detailedValues(repeat.value()) == detailedValues(detailed.value()));
}

TEST(integration, LibraryDetailedSequenceG2bOnePointAndDirectionOrder) {
  Session positive;
  Session negative;
  Session onePoint;
  makeReady(positive, inviscidConfig());
  makeReady(negative, inviscidConfig());
  makeReady(onePoint, inviscidConfig());

  const Result<DetailedSequenceResult> ascending =
      positive.solveDetailedAlphaSequence(sequence(1.0, 4.0, 1.0));
  CHECK(ascending.isOk());
  CHECK_EQ(ascending.value().points.size(), std::size_t{3});
  CHECK_NEAR(ascending.value().points[0].polar.alpha.value, 1.0, 0.0, 0.0);
  CHECK_NEAR(ascending.value().points[2].polar.alpha.value, 3.0, 0.0, 0.0);

  const Result<DetailedSequenceResult> descending =
      negative.solveDetailedAlphaSequence(sequence(2.0, -1.0, -1.0));
  CHECK(descending.isOk());
  CHECK_EQ(descending.value().points.size(), std::size_t{3});
  CHECK_NEAR(descending.value().points[0].polar.alpha.value, 2.0, 0.0, 0.0);
  CHECK_NEAR(descending.value().points[2].polar.alpha.value, 0.0, 0.0, 0.0);

  const Result<DetailedSequenceResult> single =
      onePoint.solveDetailedAlphaSequence(sequence(0.0, 0.5, 1.0));
  CHECK(single.isOk());
  CHECK_EQ(single.value().points.size(), std::size_t{1});
  CHECK_NEAR(single.value().points[0].polar.alpha.value, 0.0, 0.0, 0.0);
  CHECK(single.value().points[0].operatingPoint.has_value());
}

TEST(integration, LibraryDetailedSequenceG2bNoFileOrConsoleIo) {
  testing::ScopedTempDirectory work("library-detailed-sequence-g2b", true);
  CHECK(std::filesystem::is_empty(work.path()));

  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
  std::streambuf* oldErr = std::cerr.rdbuf(err.rdbuf());

  std::optional<Result<DetailedSequenceResult>> result;
  {
    Session session;
    makeReady(session, inviscidConfig());
    result = session.solveDetailedAlphaSequence(sequence(0.0, 2.0, 1.0));
  }

  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);

  CHECK(result.has_value());
  CHECK(result->isOk());
  CHECK_EQ(result->value().points.size(), std::size_t{2});
  CHECK(out.str().empty());
  CHECK(err.str().empty());
  CHECK(std::filesystem::is_empty(work.path()));
}

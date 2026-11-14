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

// File: tests/unit/LibrarySessionL3Tests.cpp
// Public in-memory session tests: object semantics, geometry, paneling,
// analysis, sequences, isolation, ownership and error mapping.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/XFoilSession.h"
#include "xfoil/core/geometry/Naca.h"
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

xfoil::geometry::AirfoilGeometry makeDiamond() {
  const std::vector<xfoil::Point2D> points = {
      {1.0, 0.0},   {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign("diamond", points);
  CHECK(assigned.success());
  return geometry;
}

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

void makeReadyInviscid(Session& session, const std::string& designation,
                       std::size_t panelCount = 240) {
  const Result<AirfoilGeometry> geometry = session.setNaca(designation);
  CHECK(geometry.isOk());
  PanelingOptions options;
  options.panelCount = panelCount;
  const Result<SurfaceGeometry> panel = session.generatePanel(options);
  CHECK(panel.isOk());
  const Result<void> configured = session.configure(inviscidConfig());
  CHECK(configured.isOk());
}

}  // namespace

TEST(unit, LibrarySessionL3ObjectSemantics) {
  static_assert(!std::is_copy_constructible_v<Session>);
  static_assert(!std::is_copy_assignable_v<Session>);
  static_assert(std::is_move_constructible_v<Session>);
  static_assert(std::is_move_assignable_v<Session>);

  Session first;
  Session second(std::move(first));
  first = std::move(second);
  first.reset();
  CHECK(first.lastOperatingPoint().isErr());
}

TEST(unit, LibrarySessionL3SetGeometryValidation) {
  Session session;
  xfoil::geometry::AirfoilGeometry empty;
  const Result<void> rejected = session.setGeometry(empty);
  CHECK(rejected.isErr());
  CHECK_EQ(rejected.error().code, std::string{"geometry.empty"});

  const AirfoilGeometry diamond = makeDiamond();
  const std::vector<xfoil::Point2D> before(diamond.coordinates().begin(),
                                           diamond.coordinates().end());
  const Result<void> accepted = session.setGeometry(diamond);
  CHECK(accepted.isOk());
  const Result<SurfaceGeometry> panel = session.currentPanel();
  CHECK(panel.isOk());
  CHECK(panel.value().points.size() > 0);
  CHECK_EQ(diamond.coordinates().size(), before.size());
  for (std::size_t index = 0; index < before.size(); ++index) {
    CHECK_NEAR(diamond.coordinates()[index].x, before[index].x, 0.0, 0.0);
    CHECK_NEAR(diamond.coordinates()[index].y, before[index].y, 0.0, 0.0);
  }
}

TEST(unit, LibrarySessionL3SetGeometryAtomicOnError) {
  Session session;
  makeReadyInviscid(session, "0012");
  const Result<SurfaceGeometry> before = session.currentPanel();
  CHECK(before.isOk());

  xfoil::geometry::AirfoilGeometry empty;
  const Result<void> rejected = session.setGeometry(empty);
  CHECK(rejected.isErr());
  const Result<SurfaceGeometry> after = session.currentPanel();
  CHECK(after.isOk());
  CHECK_EQ(after.value().points.size(), before.value().points.size());
}

TEST(unit, LibrarySessionL3SetNacaNacs) {
  Session session;
  const Result<AirfoilGeometry> normal = session.setNaca("0012");
  CHECK(normal.isOk());
  CHECK_EQ(normal.value().pointCount(), std::size_t{399});

  const xfoil::core::Result<xfoil::core::geometry::NacaAirfoilResult> coreNormal =
      xfoil::core::geometry::nacaAirfoil(12, false, 200);
  CHECK(coreNormal.isOk());
  CHECK_EQ(normal.value().pointCount(), coreNormal.value().buffer.x.size());
  for (std::size_t index = 0; index < normal.value().pointCount(); ++index) {
    CHECK_NEAR(normal.value().coordinates()[index].x,
               coreNormal.value().buffer.x[index], 0.0, 0.0);
    CHECK_NEAR(normal.value().coordinates()[index].y,
               coreNormal.value().buffer.y[index], 0.0, 0.0);
  }

  const Result<AirfoilGeometry> sharp = session.setNacs("0012");
  CHECK(sharp.isOk());
  CHECK_EQ(sharp.value().pointCount(), std::size_t{399});
  const xfoil::core::Result<xfoil::core::geometry::NacaAirfoilResult> coreSharp =
      xfoil::core::geometry::nacaAirfoil(12, true, 200);
  CHECK(coreSharp.isOk());
  CHECK_EQ(sharp.value().pointCount(), coreSharp.value().buffer.x.size());

  const Result<AirfoilGeometry> badText = session.setNaca("abc");
  CHECK(badText.isErr());
  CHECK_EQ(badText.error().code, std::string{"naca.invalidDesignation"});

  const Result<AirfoilGeometry> badSign = session.setNaca("-12");
  CHECK(badSign.isErr());
  CHECK_EQ(badSign.error().code, std::string{"naca.invalidDesignation"});

  const Result<AirfoilGeometry> badDigit = session.setNaca("99999");
  CHECK(badDigit.isErr());
  CHECK_EQ(badDigit.error().code,
           std::string{"naca.designationNotImplemented"});
}

TEST(unit, LibrarySessionL3GeneratePanel) {
  Session session;
  PanelingOptions options;
  const Result<SurfaceGeometry> noGeometry = session.generatePanel(options);
  CHECK(noGeometry.isErr());
  CHECK_EQ(noGeometry.error().code,
           std::string{"paneling.bufferUnavailable"});

  CHECK(session.setNaca("0012").isOk());
  const Result<SurfaceGeometry> defaults = session.generatePanel(options);
  CHECK(defaults.isOk());
  CHECK_EQ(defaults.value().points.size(), std::size_t{240});

  options.panelCount = 100;
  const Result<SurfaceGeometry> custom = session.generatePanel(options);
  CHECK(custom.isOk());
  CHECK_EQ(custom.value().points.size(), std::size_t{100});

  options.panelCount = 1;
  const Result<SurfaceGeometry> tooFew = session.generatePanel(options);
  CHECK(tooFew.isErr());
  CHECK(tooFew.error().category == xfoil::ErrorCategory::invalidArgument);

  options.panelCount = 596;
  const Result<SurfaceGeometry> tooMany = session.generatePanel(options);
  CHECK(tooMany.isErr());
  CHECK(tooMany.error().category ==
          xfoil::ErrorCategory::capacityExceeded);

  options.panelCount = 100;
  options.bunchingParameter = std::nan("");
  const Result<SurfaceGeometry> nonFinite = session.generatePanel(options);
  CHECK(nonFinite.isErr());
  CHECK_EQ(nonFinite.error().code,
           std::string{"paneling.invalidParameters"});

  const Result<SurfaceGeometry> unchanged = session.currentPanel();
  CHECK(unchanged.isOk());
  CHECK_EQ(unchanged.value().points.size(), std::size_t{100});
}

TEST(unit, LibrarySessionL3Configure) {
  Session session;
  AnalysisConfig invalid;
  invalid.reynoldsNumber = -1.0;
  const Result<void> rejected = session.configure(invalid);
  CHECK(rejected.isErr());
  CHECK_EQ(rejected.error().code,
           std::string{"config.invalidReynoldsNumber"});

  const Result<void> accepted = session.configure(inviscidConfig());
  CHECK(accepted.isOk());
}

TEST(unit, LibrarySessionL3SolveErrors) {
  Session session;
  const Result<OperatingPointData> noGeometry = session.solveAlpha(Degrees{2.0});
  CHECK(noGeometry.isErr());
  CHECK_EQ(noGeometry.error().code,
           std::string{"analysis.missingGeometry"});

  CHECK(session.setNaca("0012").isOk());
  const Result<OperatingPointData> noConfiguration =
      session.solveAlpha(Degrees{2.0});
  CHECK(noConfiguration.isErr());
  CHECK_EQ(noConfiguration.error().code,
           std::string{"analysis.missingConfiguration"});

  CHECK(session.configure(inviscidConfig()).isOk());
  const Result<OperatingPointData> invalidAlpha =
      session.solveAlpha(Degrees{std::nan("")});
  CHECK(invalidAlpha.isErr());
  CHECK_EQ(invalidAlpha.error().code, std::string{"analysis.invalidAlpha"});

  const Result<OperatingPointData> invalidCl = session.solveCl(std::nan(""));
  CHECK(invalidCl.isErr());
  CHECK_EQ(invalidCl.error().code,
           std::string{"analysis.invalidLiftCoefficient"});
}

TEST(unit, LibrarySessionL3SolveAlphaInviscidMatchesExistingApi) {
  Session session;
  const Result<AirfoilGeometry> geometry = session.setNaca("2412");
  CHECK(geometry.isOk());
  CHECK(session.configure(inviscidConfig()).isOk());
  const Result<OperatingPointData> point = session.solveAlpha(Degrees{2.0});
  CHECK(point.isOk());
  CHECK(point.value().convergence == ConvergenceStatus::converged);
  CHECK_NEAR(point.value().alpha.value, 2.0, 1.0e-12, 1.0e-12);
  CHECK(point.value().surface.has_value());
  CHECK(point.value().surface->kind == SurfaceDistributionKind::inviscid);
  const Result<SurfaceGeometry> panel = session.currentPanel();
  CHECK(panel.isOk());
  CHECK_EQ(point.value().surface->cp.size(), panel.value().points.size());

  xfoil::XFoilSession reference;
  CHECK(reference.setGeometry(geometry.value()).success());
  CHECK(reference.configure(inviscidConfig()).success());
  const xfoil::AnalysisResult expected =
      reference.analyzeAtAlpha(Degrees{2.0});
  CHECK(expected.successful);
  CHECK_NEAR(point.value().liftCoefficient, expected.liftCoefficient, 0.0,
             0.0);
  CHECK_NEAR(point.value().dragCoefficient, expected.dragCoefficient, 0.0,
             0.0);
  CHECK_NEAR(point.value().momentCoefficient, expected.momentCoefficient, 0.0,
             0.0);
  CHECK_EQ(point.value().iterations, expected.iterations);
}

TEST(unit, LibrarySessionL3SolveAlphaViscousMatchesExistingApi) {
  Session session;
  const Result<AirfoilGeometry> geometry = session.setNaca("2412");
  CHECK(geometry.isOk());
  CHECK(session.configure(viscousConfig()).isOk());
  const Result<OperatingPointData> point = session.solveAlpha(Degrees{2.0});
  CHECK(point.isOk());
  CHECK(point.value().convergence == ConvergenceStatus::converged);
  CHECK(point.value().viscous);
  CHECK(point.value().surface.has_value());
  CHECK(point.value().surface->kind == SurfaceDistributionKind::viscous);
  CHECK(point.value().dragCoefficientPressure.has_value());

  xfoil::XFoilSession reference;
  CHECK(reference.setGeometry(geometry.value()).success());
  CHECK(reference.configure(viscousConfig()).success());
  const xfoil::AnalysisResult expected =
      reference.analyzeAtAlpha(Degrees{2.0});
  CHECK(expected.successful);
  CHECK_NEAR(point.value().liftCoefficient, expected.liftCoefficient, 0.0,
             0.0);
  CHECK_NEAR(point.value().dragCoefficient, expected.dragCoefficient, 0.0,
             0.0);
  CHECK_NEAR(point.value().momentCoefficient, expected.momentCoefficient, 0.0,
             0.0);
  CHECK_EQ(point.value().iterations, expected.iterations);
}

TEST(unit, LibrarySessionL3SolveCl) {
  Session session;
  makeReadyInviscid(session, "0012");
  const Result<OperatingPointData> point = session.solveCl(0.5);
  CHECK(point.isOk());
  CHECK(point.value().convergence == ConvergenceStatus::converged);
  CHECK_NEAR(point.value().liftCoefficient, 0.5, 1.0e-6, 1.0e-6);
}

TEST(unit, LibrarySessionL3SequenceValidation) {
  Session session;
  AlphaSequence zeroStep;
  zeroStep.start = Degrees{0.0};
  zeroStep.endExclusive = Degrees{2.0};
  zeroStep.step = Degrees{0.0};
  CHECK(session.solveAlphaSequence(zeroStep).isErr());

  AlphaSequence wrongDirection;
  wrongDirection.start = Degrees{2.0};
  wrongDirection.endExclusive = Degrees{0.0};
  wrongDirection.step = Degrees{1.0};
  const Result<SequenceResult> direction =
      session.solveAlphaSequence(wrongDirection);
  CHECK(direction.isErr());
  CHECK_EQ(direction.error().code, std::string{"sequence.direction"});

  AlphaSequence tooMany;
  tooMany.start = Degrees{0.0};
  tooMany.endExclusive = Degrees{100001.0};
  tooMany.step = Degrees{1.0};
  const Result<SequenceResult> limit = session.solveAlphaSequence(tooMany);
  CHECK(limit.isErr());
  CHECK_EQ(limit.error().code, std::string{"sequence.pointLimit"});

  makeReadyInviscid(session, "0012");
  AlphaSequence ascending;
  ascending.start = Degrees{-1.0};
  ascending.endExclusive = Degrees{2.0};
  ascending.step = Degrees{1.0};
  const Result<SequenceResult> sequence =
      session.solveAlphaSequence(ascending);
  CHECK(sequence.isOk());
  CHECK_EQ(sequence.value().points.size(), std::size_t{3});
  CHECK(sequence.value().stopReason == SequenceStopReason::completed);
  CHECK_NEAR(sequence.value().points[0].alpha.value, -1.0, 1.0e-12,
             1.0e-12);
  CHECK_NEAR(sequence.value().points[1].alpha.value, 0.0, 1.0e-12,
             1.0e-12);
  CHECK_NEAR(sequence.value().points[2].alpha.value, 1.0, 1.0e-12,
             1.0e-12);

  AlphaSequence descending;
  descending.start = Degrees{2.0};
  descending.endExclusive = Degrees{-1.0};
  descending.step = Degrees{-1.0};
  const Result<SequenceResult> reverse =
      session.solveAlphaSequence(descending);
  CHECK(reverse.isOk());
  CHECK_EQ(reverse.value().points.size(), std::size_t{3});

  AlphaSequence single;
  single.start = Degrees{2.0};
  single.endExclusive = Degrees{3.0};
  single.step = Degrees{1.0};
  const Result<SequenceResult> one = session.solveAlphaSequence(single);
  CHECK(one.isOk());
  CHECK_EQ(one.value().points.size(), std::size_t{1});
}

TEST(unit, LibrarySessionL3SequenceCancellation) {
  Session session;
  makeReadyInviscid(session, "0012");
  CancellationToken token;
  token.requestCancellation();
  AlphaSequence sequence;
  sequence.start = Degrees{0.0};
  sequence.endExclusive = Degrees{3.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence, &token);
  CHECK(result.isOk());
  CHECK(result.value().stopReason == SequenceStopReason::cancelled);
  CHECK_EQ(result.value().points.size(), std::size_t{1});
  CHECK(result.value().points[0].convergence ==
        ConvergenceStatus::cancelled);
}

TEST(unit, LibrarySessionL3SequenceNonConvergencePartial) {
  Session session;
  const Result<AirfoilGeometry> geometry = session.setNaca("0012");
  CHECK(geometry.isOk());
  PanelingOptions options;
  const Result<SurfaceGeometry> panel = session.generatePanel(options);
  CHECK(panel.isOk());
  AnalysisConfig config = viscousConfig();
  config.maximumIterations = 1;
  CHECK(session.configure(config).isOk());

  AlphaSequence sequence;
  sequence.start = Degrees{12.0};
  sequence.endExclusive = Degrees{14.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      session.solveAlphaSequence(sequence);
  CHECK(result.isOk());
  CHECK_EQ(result.value().points.size(), std::size_t{2});
  CHECK(result.value().stopReason == SequenceStopReason::completed);
  bool sawUnconverged = false;
  for (const auto& point : result.value().points) {
    if (point.convergence == ConvergenceStatus::unconverged) {
      sawUnconverged = true;
    }
  }
  CHECK(sawUnconverged);
}

TEST(unit, LibrarySessionL3ProjectionOwnership) {
  Session session;
  makeReadyInviscid(session, "0012");
  const Result<OperatingPointData> first = session.solveAlpha(Degrees{1.0});
  CHECK(first.isOk());
  OperatingPointData copy = first.value();
  const double firstLift = copy.liftCoefficient;
  const std::size_t firstCpSize = copy.surface->cp.size();

  CHECK(session.solveAlpha(Degrees{3.0}).isOk());
  CHECK_NEAR(copy.liftCoefficient, firstLift, 0.0, 0.0);
  CHECK_EQ(copy.surface->cp.size(), firstCpSize);

  const Result<OperatingPointData> last = session.lastOperatingPoint();
  CHECK(last.isOk());
  CHECK(last.value().convergence == ConvergenceStatus::converged);
}

TEST(unit, LibrarySessionL3SessionIsolation) {
  Session first;
  Session second;
  makeReadyInviscid(first, "0012");
  makeReadyInviscid(second, "2412");
  const Result<OperatingPointData> firstPoint =
      first.solveAlpha(Degrees{2.0});
  const Result<OperatingPointData> secondPoint =
      second.solveAlpha(Degrees{2.0});
  CHECK(firstPoint.isOk());
  CHECK(secondPoint.isOk());
  CHECK(firstPoint.value().liftCoefficient !=
        secondPoint.value().liftCoefficient);

  first.reset();
  CHECK(first.lastOperatingPoint().isErr());
  CHECK(second.currentPanel().isOk());
  CHECK(second.solveAlpha(Degrees{2.0}).isOk());

  const Result<OperatingPointData> invalid =
      first.solveAlpha(Degrees{std::nan("")});
  CHECK(invalid.isErr());
  CHECK(second.lastOperatingPoint().isOk());
}

TEST(unit, LibrarySessionL3Reset) {
  Session session;
  makeReadyInviscid(session, "0012");
  CHECK(session.solveAlpha(Degrees{2.0}).isOk());
  CHECK(session.currentPanel().isOk());
  session.reset();
  CHECK(session.currentPanel().isErr());
  CHECK(session.lastOperatingPoint().isErr());
  CHECK(session.activePolar().isErr());
  makeReadyInviscid(session, "2412");
  CHECK(session.solveAlpha(Degrees{2.0}).isOk());
}

TEST(unit, LibrarySessionL3NoIoSideEffects) {
  testing::ScopedTempDirectory work("library-session-l3-no-io", true);
  CHECK(std::filesystem::is_empty(work.path()));

  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
  std::streambuf* oldErr = std::cerr.rdbuf(err.rdbuf());

  std::optional<Result<AirfoilGeometry>> geometry;
  std::optional<Result<SurfaceGeometry>> panel;
  std::optional<Result<void>> configured;
  std::optional<Result<OperatingPointData>> point;
  std::optional<Result<SequenceResult>> sequenceResult;
  {
    Session session;
    geometry = session.setNaca("0012");
    PanelingOptions options;
    panel = session.generatePanel(options);
    configured = session.configure(inviscidConfig());
    point = session.solveAlpha(Degrees{2.0});
    AlphaSequence sequence;
    sequence.start = Degrees{0.0};
    sequence.endExclusive = Degrees{2.0};
    sequence.step = Degrees{1.0};
    sequenceResult = session.solveAlphaSequence(sequence);
  }

  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);
  CHECK(geometry.has_value());
  CHECK(geometry->isOk());
  CHECK(panel.has_value());
  CHECK(panel->isOk());
  CHECK(configured.has_value());
  CHECK(configured->isOk());
  CHECK(point.has_value());
  CHECK(point->isOk());
  CHECK(sequenceResult.has_value());
  CHECK(sequenceResult->isOk());
  CHECK(out.str().empty());
  CHECK(err.str().empty());
  CHECK(std::filesystem::is_empty(work.path()));
}

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

// File: tests/unit/LibraryDataModelL2Tests.cpp
// L2 public data model and projection tests. Pure value checks; no
// solver run, no file I/O and no CLI mutation.

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Diagnostic.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/session/SessionProjections.h"
#include "xfoil/session/SessionState.h"

namespace {

using xfoil::AnalysisResult;
using xfoil::AnalysisStatus;
using xfoil::ConvergenceStatus;
using xfoil::Diagnostic;
using xfoil::DiagnosticSeverity;
using xfoil::Error;
using xfoil::ErrorCategory;
using xfoil::OperatingPointData;
using xfoil::PolarPoint;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::SurfaceDistribution;
using xfoil::SurfaceDistributionKind;
using xfoil::SurfaceGeometry;

xfoil::session::SessionComposition makeSessionWithPanel() {
  xfoil::session::SessionComposition session;
  xfoil::core::paneling::PanelGeometry panel =
      xfoil::core::paneling::PanelGeometry::make(
          {1.0, 0.5, 0.0, 0.5}, {0.0, 0.1, 0.0, -0.1},
          {0.0, 0.5, 1.0, 1.5}, {0.0, -0.5, -1.0, -0.5},
          {1.0, 0.6, 1.0, 0.6}, {0.0, 1.0, 0.0, -1.0},
          {1.0, 0.0, -1.0, 0.0}, {0.0, 1.5708, 3.1416, -1.5708},
          0.0, 0.0, 0.0, 1.0, 0.0, 1.0, false);
  session.geometry.valid = true;
  session.paneling.panelingValid = true;
  session.paneling.panel = std::move(panel);
  session.operating.configured = true;
  return session;
}

void setSuccessfulPoint(xfoil::session::SessionComposition& session,
                        bool viscous) {
  session.operating.viscous = viscous;
  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status = AnalysisStatus::converged;
  session.result.lastResult.successful = true;
  session.result.lastResult.converged = true;
  session.result.lastResult.iterations = 7;
  session.result.lastResult.alpha = xfoil::Degrees{3.0};
  session.result.lastResult.liftCoefficient = 0.7;
  session.result.lastResult.dragCoefficient = 0.01;
  session.result.lastResult.momentCoefficient = -0.1;
  session.result.lastResult.centerOfPressureX = 0.25;
  session.result.lastResult.diagnostic = "iteration 7";
  session.result.lastDiagnostics = xfoil::session::OperPointDiagnostics{};
  session.result.lastDiagnostics.viscalRan = viscous;
  session.viscous.cpi = {1.0, 0.9, 0.8, 0.7};
  session.viscous.cpv = {1.1, 1.0, 0.9, 0.8};
  if (viscous) {
    session.viscous.viscals.cdp = 0.004;
  }
}

}  // namespace

TEST(unit, LibraryDataModelL2ResultOkAndFail) {
  Result<int> ok = Result<int>::ok(42);
  CHECK(ok.isOk());
  CHECK(!ok.isErr());
  CHECK_EQ(ok.value(), 42);

  Result<int> copied = ok;
  CHECK_EQ(copied.value(), 42);

  Result<int> moved = std::move(copied);
  CHECK_EQ(moved.value(), 42);

  Result<int> failed = Result<int>::fail(
      Error::make(ErrorCategory::invalidState, "test.code", "message",
                  "context"));
  CHECK(failed.isErr());
  CHECK(!failed.isOk());
  CHECK(failed.error().category == ErrorCategory::invalidState);
  CHECK_EQ(failed.error().code, std::string{"test.code"});
  CHECK_EQ(failed.error().message, std::string{"message"});
  CHECK_EQ(failed.error().context, std::string{"context"});

  Result<void> status = Result<void>::ok();
  CHECK(status.isOk());
  Result<void> bad = Result<void>::fail(
      Error::make(ErrorCategory::unsupported, "test.unsupported", "no"));
  CHECK(bad.isErr());
  CHECK_EQ(bad.error().code, std::string{"test.unsupported"});
}

TEST(unit, LibraryDataModelL2ResultAccessContract) {
  Result<int> failed = Result<int>::fail(
      Error::make(ErrorCategory::internal, "test.internal", "failure"));
  bool valueThrew = false;
  try {
    static_cast<void>(failed.value());
  } catch (const std::logic_error&) {
    valueThrew = true;
  }
  CHECK(valueThrew);

  Result<int> ok = Result<int>::ok(1);
  bool errorThrew = false;
  try {
    static_cast<void>(ok.error());
  } catch (const std::logic_error&) {
    errorThrew = true;
  }
  CHECK(errorThrew);
}

TEST(unit, LibraryDataModelL2MissingStateErrors) {
  xfoil::session::SessionComposition empty;

  const Result<SurfaceGeometry> panel =
      xfoil::session::projectCurrentPanel(empty);
  CHECK(panel.isErr());
  CHECK(panel.error().category == ErrorCategory::invalidState);
  CHECK_EQ(panel.error().code, std::string{"geometry.unavailable"});

  const Result<SurfaceDistribution> surface =
      xfoil::session::projectSurfaceDistribution(empty);
  CHECK(surface.isErr());
  CHECK(surface.error().category == ErrorCategory::invalidState);

  const Result<OperatingPointData> point =
      xfoil::session::projectLastOperatingPoint(empty);
  CHECK(point.isErr());
  CHECK(point.error().category == ErrorCategory::invalidState);
  CHECK_EQ(point.error().code, std::string{"analysis.missingGeometry"});

  const Result<SequenceResult> polar =
      xfoil::session::projectActivePolar(empty);
  CHECK(polar.isErr());
  CHECK(polar.error().category == ErrorCategory::invalidState);
  CHECK_EQ(polar.error().code, std::string{"polar.noActiveSlot"});
}

TEST(unit, LibraryDataModelL2ProjectPanelGeometry) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  const Result<SurfaceGeometry> projected =
      xfoil::session::projectCurrentPanel(session);
  CHECK(projected.isOk());
  CHECK_EQ(projected.value().points.size(), std::size_t{4});
  CHECK_NEAR(projected.value().points[0].x, 1.0, 0.0, 0.0);
  CHECK_NEAR(projected.value().points[1].y, 0.1, 0.0, 0.0);

  // The projected value owns its data and is independent of later
  // session mutation.
  SurfaceGeometry copy = projected.value();
  session.paneling.panel.x[1] = 99.0;
  CHECK_NEAR(copy.points[1].x, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.paneling.panel.x[1], 99.0, 0.0, 0.0);
}

TEST(unit, LibraryDataModelL2ProjectSurfaceDistributionInviscid) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, false);

  const Result<SurfaceDistribution> projected =
      xfoil::session::projectSurfaceDistribution(session);
  CHECK(projected.isOk());
  CHECK(projected.value().kind == SurfaceDistributionKind::inviscid);
  CHECK_EQ(projected.value().x.size(), std::size_t{4});
  CHECK_EQ(projected.value().y.size(), std::size_t{4});
  CHECK_EQ(projected.value().cp.size(), std::size_t{4});
  CHECK_NEAR(projected.value().cp[0], 1.0, 0.0, 0.0);
  CHECK_NEAR(projected.value().cp[3], 0.7, 0.0, 0.0);
}

TEST(unit, LibraryDataModelL2ProjectSurfaceDistributionViscous) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);

  const Result<SurfaceDistribution> projected =
      xfoil::session::projectSurfaceDistribution(session);
  CHECK(projected.isOk());
  CHECK(projected.value().kind == SurfaceDistributionKind::viscous);
  CHECK_EQ(projected.value().cp.size(), std::size_t{4});
  CHECK_NEAR(projected.value().cp[0], 1.1, 0.0, 0.0);
  CHECK_NEAR(projected.value().cp[3], 0.8, 0.0, 0.0);
}

TEST(unit, LibraryDataModelL2ProjectSurfaceDistributionRejectsShortCp) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);
  session.viscous.cpv.resize(2);

  const Result<SurfaceDistribution> projected =
      xfoil::session::projectSurfaceDistribution(session);
  CHECK(projected.isErr());
  CHECK_EQ(projected.error().code,
           std::string{"surface.viscousCpUnavailable"});
}

TEST(unit, LibraryDataModelL2ProjectOperatingPointInviscid) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, false);

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isOk());
  CHECK_NEAR(projected.value().alpha.value, 3.0, 0.0, 0.0);
  CHECK_NEAR(projected.value().liftCoefficient, 0.7, 0.0, 0.0);
  CHECK_NEAR(projected.value().dragCoefficient, 0.01, 0.0, 0.0);
  CHECK(!projected.value().dragCoefficientPressure.has_value());
  CHECK(!projected.value().viscous);
  CHECK(projected.value().convergence == ConvergenceStatus::converged);
  CHECK_EQ(projected.value().iterations, 7);
  CHECK(projected.value().surface.has_value());
  CHECK(projected.value().surface->kind ==
        SurfaceDistributionKind::inviscid);
  CHECK(!projected.value().diagnostics.empty());
  CHECK_EQ(projected.value().diagnostics[0].code,
           std::string{"solver.diagnostic"});
}

TEST(unit, LibraryDataModelL2ProjectOperatingPointViscous) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isOk());
  CHECK(projected.value().viscous);
  CHECK(projected.value().dragCoefficientPressure.has_value());
  CHECK_NEAR(projected.value().dragCoefficientPressure.value(), 0.004,
             0.0, 0.0);
  CHECK(projected.value().surface.has_value());
  CHECK(projected.value().surface->kind ==
        SurfaceDistributionKind::viscous);
}

TEST(unit, LibraryDataModelL2ProjectOperatingPointUnconverged) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);
  session.result.lastResult.status = AnalysisStatus::unconverged;
  session.result.lastResult.converged = false;
  session.result.lastResult.diagnostic.clear();

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isOk());
  CHECK(projected.value().convergence == ConvergenceStatus::unconverged);
  CHECK_EQ(projected.value().diagnostics.size(), std::size_t{1});
  CHECK_EQ(projected.value().diagnostics[0].code,
           std::string{"solver.nonConvergence"});
}

TEST(unit, LibraryDataModelL2ProjectOperatingPointFailed) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status = AnalysisStatus::missingGeometry;

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isErr());
  CHECK_EQ(projected.error().code,
           std::string{"analysis.missingGeometry"});
}

TEST(unit, LibraryDataModelL2ProjectActivePolar) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  session.polar.activeSlot = 0;
  session.polar.slots.resize(1);
  xfoil::core::polar::PolarPoint source;
  source.alpha = 2.0;
  source.cl = 0.5;
  source.cd = 0.02;
  source.cdPres = 0.01;
  source.cm = -0.05;
  source.cdWave = 0.001;
  source.cdVisc = 0.009;
  source.mach = 0.1;
  source.re = 1.0e6;
  source.hinge = 0.3;
  source.cpMin = -1.2;
  source.cdh = 0.002;
  source.cmDot = -0.003;
  session.polar.slots[0].points.push_back(source);

  const Result<SequenceResult> projected =
      xfoil::session::projectActivePolar(session);
  CHECK(projected.isOk());
  CHECK_EQ(projected.value().points.size(), std::size_t{1});
  CHECK(projected.value().stopReason == SequenceStopReason::completed);
  const PolarPoint& point = projected.value().points[0];
  CHECK_NEAR(point.alpha.value, 2.0, 0.0, 0.0);
  CHECK_NEAR(point.liftCoefficient, 0.5, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficient, 0.02, 0.0, 0.0);
  CHECK(point.dragCoefficientPressure.has_value());
  CHECK_NEAR(point.dragCoefficientPressure.value(), 0.01, 0.0, 0.0);
  CHECK_NEAR(point.momentCoefficient, -0.05, 0.0, 0.0);
  CHECK(point.convergence == ConvergenceStatus::converged);

  session.polar.slots[0].points.clear();
  const Result<SequenceResult> empty =
      xfoil::session::projectActivePolar(session);
  CHECK(empty.isOk());
  CHECK(empty.value().points.empty());
  CHECK(empty.value().stopReason == SequenceStopReason::notRun);
}

TEST(unit, LibraryDataModelL2ProjectSequenceConvergence) {
  std::vector<AnalysisResult> results(3);
  results[0].status = AnalysisStatus::converged;
  results[0].successful = true;
  results[0].alpha = xfoil::Degrees{1.0};
  results[0].liftCoefficient = 0.4;
  results[1].status = AnalysisStatus::unconverged;
  results[1].successful = true;
  results[1].alpha = xfoil::Degrees{2.0};
  results[1].liftCoefficient = 0.8;
  results[1].diagnostic = "stalled";
  results[2].status = AnalysisStatus::missingGeometry;
  results[2].successful = false;
  results[2].alpha = xfoil::Degrees{3.0};

  const SequenceResult sequence = xfoil::session::projectSequence(
      results, SequenceStopReason::failure, std::size_t{2});
  CHECK_EQ(sequence.points.size(), std::size_t{3});
  CHECK(sequence.stopReason == SequenceStopReason::failure);
  CHECK(sequence.stoppedAtIndex.has_value());
  CHECK_EQ(sequence.stoppedAtIndex.value(), std::size_t{2});
  CHECK(sequence.points[0].convergence == ConvergenceStatus::converged);
  CHECK(sequence.points[1].convergence ==
        ConvergenceStatus::unconverged);
  CHECK(sequence.points[2].convergence == ConvergenceStatus::failed);
  CHECK_EQ(sequence.points[1].diagnostics.size(), std::size_t{2});
  CHECK(sequence.points[1].diagnostics[0].severity ==
        DiagnosticSeverity::warning);
}

TEST(unit, LibraryDataModelL2ProjectionNonMutatingAndDeterministic) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);
  const std::size_t panelSize = session.paneling.panel.x.size();
  const double originalAlpha = session.result.lastResult.alpha.value;

  const Result<OperatingPointData> first =
      xfoil::session::projectLastOperatingPoint(session);
  const Result<OperatingPointData> second =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(first.isOk());
  CHECK(second.isOk());
  CHECK_EQ(first.value().iterations, second.value().iterations);
  CHECK_NEAR(first.value().liftCoefficient, second.value().liftCoefficient,
             0.0, 0.0);
  CHECK_EQ(first.value().surface->cp.size(),
           second.value().surface->cp.size());
  CHECK_EQ(session.paneling.panel.x.size(), panelSize);
  CHECK_NEAR(session.result.lastResult.alpha.value, originalAlpha, 0.0, 0.0);

  // A copied projection is independent of a later session mutation.
  OperatingPointData copy = first.value();
  session.viscous.cpv[0] = 42.0;
  CHECK_NEAR(copy.surface->cp[0], 1.1, 0.0, 0.0);
}

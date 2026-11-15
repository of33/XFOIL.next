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

// File: tests/unit/LibraryOneShotL4Tests.cpp
// Public one-shot request/analyze tests: target selection, request
// immutability, L3 parity, error order, non-convergence, cancellation,
// determinism, isolation and no-I/O behaviour.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::geometry::AirfoilGeometry;
using xfoil::AlphaTarget;
using xfoil::AnalysisConfig;
using xfoil::AnalysisRequest;
using xfoil::AnalysisTarget;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::Diagnostic;
using xfoil::LiftTarget;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::Result;
using xfoil::Session;
using xfoil::SurfaceDistribution;
using xfoil::SurfaceDistributionKind;

AirfoilGeometry nacaGeometry(const std::string& designation) {
  Session session;
  const Result<AirfoilGeometry> geometry = session.setNaca(designation);
  if (geometry.isErr()) {
    return AirfoilGeometry{};
  }
  return geometry.value();
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

AnalysisRequest makeRequest(const AirfoilGeometry& geometry,
                            const AnalysisConfig& config,
                            AnalysisTarget target,
                            const PanelingOptions& paneling = {}) {
  AnalysisRequest request;
  request.geometry = geometry;
  request.paneling = paneling;
  request.configuration = config;
  request.target = std::move(target);
  return request;
}

Result<OperatingPointData> directAnalysis(const AnalysisRequest& request) {
  Session session;
  const Result<void> geometry = session.setGeometry(request.geometry);
  if (geometry.isErr()) {
    return Result<OperatingPointData>::fail(geometry.error());
  }
  const Result<xfoil::SurfaceGeometry> panel =
      session.generatePanel(request.paneling);
  if (panel.isErr()) {
    return Result<OperatingPointData>::fail(panel.error());
  }
  const Result<void> configuration =
      session.configure(request.configuration);
  if (configuration.isErr()) {
    return Result<OperatingPointData>::fail(configuration.error());
  }
  if (const auto* alpha = std::get_if<AlphaTarget>(&request.target)) {
    return session.solveAlpha(alpha->alpha);
  }
  const auto* cl = std::get_if<LiftTarget>(&request.target);
  return session.solveCl(cl->cl);
}

void expectSameDistribution(const SurfaceDistribution& actual,
                            const SurfaceDistribution& expected) {
  CHECK(actual.kind == expected.kind);
  CHECK_EQ(actual.x.size(), expected.x.size());
  CHECK_EQ(actual.y.size(), expected.y.size());
  CHECK_EQ(actual.cp.size(), expected.cp.size());
  for (std::size_t index = 0; index < actual.x.size(); ++index) {
    CHECK_NEAR(actual.x[index], expected.x[index], 0.0, 0.0);
    CHECK_NEAR(actual.y[index], expected.y[index], 0.0, 0.0);
    CHECK_NEAR(actual.cp[index], expected.cp[index], 0.0, 0.0);
  }
}

void expectSameDiagnostics(const std::vector<Diagnostic>& actual,
                           const std::vector<Diagnostic>& expected) {
  CHECK_EQ(actual.size(), expected.size());
  const std::size_t count = actual.size() < expected.size()
                                ? actual.size()
                                : expected.size();
  for (std::size_t index = 0; index < count; ++index) {
    CHECK(actual[index].severity == expected[index].severity);
    CHECK_EQ(actual[index].code, expected[index].code);
    CHECK_EQ(actual[index].message, expected[index].message);
    CHECK_EQ(actual[index].iteration.has_value(),
             expected[index].iteration.has_value());
    if (actual[index].iteration.has_value() &&
        expected[index].iteration.has_value()) {
      CHECK_EQ(actual[index].iteration.value(),
               expected[index].iteration.value());
    }
  }
}

void expectSamePoint(const OperatingPointData& actual,
                     const OperatingPointData& expected) {
  CHECK_NEAR(actual.alpha.value, expected.alpha.value, 0.0, 0.0);
  CHECK_NEAR(actual.liftCoefficient, expected.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(actual.dragCoefficient, expected.dragCoefficient, 0.0, 0.0);
  CHECK_NEAR(actual.momentCoefficient, expected.momentCoefficient, 0.0,
             0.0);
  CHECK_NEAR(actual.centerOfPressureX, expected.centerOfPressureX, 0.0,
             0.0);
  CHECK(actual.convergence == expected.convergence);
  CHECK_EQ(actual.iterations, expected.iterations);
  CHECK_EQ(actual.viscous, expected.viscous);
  CHECK_EQ(actual.dragCoefficientPressure.has_value(),
           expected.dragCoefficientPressure.has_value());
  if (actual.dragCoefficientPressure.has_value() &&
      expected.dragCoefficientPressure.has_value()) {
    CHECK_NEAR(actual.dragCoefficientPressure.value(),
               expected.dragCoefficientPressure.value(), 0.0, 0.0);
  }
  CHECK_EQ(actual.surface.has_value(), expected.surface.has_value());
  if (actual.surface.has_value() && expected.surface.has_value()) {
    expectSameDistribution(actual.surface.value(), expected.surface.value());
  }
  expectSameDiagnostics(actual.diagnostics, expected.diagnostics);
}

void expectRequestUnchanged(const AnalysisRequest& before,
                            const AnalysisRequest& after) {
  CHECK_EQ(before.geometry.pointCount(), after.geometry.pointCount());
  const auto beforeCoordinates = before.geometry.coordinates();
  const auto afterCoordinates = after.geometry.coordinates();
  for (std::size_t index = 0; index < beforeCoordinates.size(); ++index) {
    CHECK_NEAR(beforeCoordinates[index].x, afterCoordinates[index].x, 0.0,
               0.0);
    CHECK_NEAR(beforeCoordinates[index].y, afterCoordinates[index].y, 0.0,
               0.0);
  }

  CHECK_EQ(before.paneling.panelCount, after.paneling.panelCount);
  CHECK_NEAR(before.paneling.bunchingParameter,
             after.paneling.bunchingParameter, 0.0, 0.0);
  CHECK_NEAR(before.paneling.teToLeDensityRatio,
             after.paneling.teToLeDensityRatio, 0.0, 0.0);
  CHECK_NEAR(before.paneling.refinementDensityRatio,
             after.paneling.refinementDensityRatio, 0.0, 0.0);
  CHECK_NEAR(before.paneling.upperRefinementStart,
             after.paneling.upperRefinementStart, 0.0, 0.0);
  CHECK_NEAR(before.paneling.upperRefinementEnd,
             after.paneling.upperRefinementEnd, 0.0, 0.0);
  CHECK_NEAR(before.paneling.lowerRefinementStart,
             after.paneling.lowerRefinementStart, 0.0, 0.0);
  CHECK_NEAR(before.paneling.lowerRefinementEnd,
             after.paneling.lowerRefinementEnd, 0.0, 0.0);

  CHECK_NEAR(before.configuration.reynoldsNumber,
             after.configuration.reynoldsNumber, 0.0, 0.0);
  CHECK_NEAR(before.configuration.machNumber,
             after.configuration.machNumber, 0.0, 0.0);
  CHECK_EQ(before.configuration.viscous, after.configuration.viscous);
  CHECK(before.configuration.reynoldsDependency ==
        after.configuration.reynoldsDependency);
  CHECK(before.configuration.machDependency ==
        after.configuration.machDependency);
  CHECK_NEAR(before.configuration.transition.nCritical,
             after.configuration.transition.nCritical, 0.0, 0.0);
  CHECK_NEAR(before.configuration.transition.topTripX,
             after.configuration.transition.topTripX, 0.0, 0.0);
  CHECK_NEAR(before.configuration.transition.bottomTripX,
             after.configuration.transition.bottomTripX, 0.0, 0.0);
  CHECK_EQ(before.configuration.maximumIterations,
           after.configuration.maximumIterations);
  CHECK_EQ(before.configuration.autoInitializeBoundaryLayer,
           after.configuration.autoInitializeBoundaryLayer);
  CHECK_NEAR(before.configuration.viscousAcceleration,
             after.configuration.viscousAcceleration, 0.0, 0.0);

  CHECK_EQ(before.target.index(), after.target.index());
  if (const auto* alpha = std::get_if<AlphaTarget>(&before.target)) {
    const auto* other = std::get_if<AlphaTarget>(&after.target);
    CHECK(other != nullptr);
    if (other != nullptr) {
      CHECK_NEAR(alpha->alpha.value, other->alpha.value, 0.0, 0.0);
    }
  } else {
    const auto* cl = std::get_if<LiftTarget>(&before.target);
    const auto* other = std::get_if<LiftTarget>(&after.target);
    CHECK(cl != nullptr);
    CHECK(other != nullptr);
    if (cl != nullptr && other != nullptr) {
      CHECK_NEAR(cl->cl, other->cl, 0.0, 0.0);
    }
  }
}

}  // namespace

TEST(unit, LibraryOneShotL4RequestDefaultsAndTargetSelection) {
  AnalysisRequest request;
  CHECK_EQ(request.geometry.pointCount(), std::size_t{0});
  CHECK_EQ(request.target.index(), std::size_t{0});
  const auto* alpha = std::get_if<AlphaTarget>(&request.target);
  CHECK(alpha != nullptr);
  if (alpha != nullptr) {
    CHECK_NEAR(alpha->alpha.value, 0.0, 0.0, 0.0);
  }

  request.target = LiftTarget{0.5};
  CHECK_EQ(request.target.index(), std::size_t{1});
  const auto* lift = std::get_if<LiftTarget>(&request.target);
  CHECK(lift != nullptr);
  if (lift != nullptr) {
    CHECK_NEAR(lift->cl, 0.5, 0.0, 0.0);
  }

  request.target = AlphaTarget{Degrees{4.0}};
  CHECK_EQ(request.target.index(), std::size_t{0});
  alpha = std::get_if<AlphaTarget>(&request.target);
  CHECK(alpha != nullptr);
  if (alpha != nullptr) {
    CHECK_NEAR(alpha->alpha.value, 4.0, 0.0, 0.0);
  }
}

TEST(unit, LibraryOneShotL4RequestIsNotMutated) {
  AnalysisRequest request = makeRequest(
      nacaGeometry("2412"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  const AnalysisRequest before = request;
  const Result<OperatingPointData> result = xfoil::analyze(request);
  CHECK(result.isOk());
  expectRequestUnchanged(before, request);
}

TEST(unit, LibraryOneShotL4InviscidAlphaMatchesSession) {
  const AnalysisRequest request = makeRequest(
      nacaGeometry("2412"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> oneShot = xfoil::analyze(request);
  const Result<OperatingPointData> direct = directAnalysis(request);
  CHECK(oneShot.isOk());
  CHECK(direct.isOk());
  if (oneShot.isOk() && direct.isOk()) {
    CHECK(oneShot.value().convergence == ConvergenceStatus::converged);
    CHECK(oneShot.value().surface.has_value());
    CHECK(oneShot.value().surface->kind ==
          SurfaceDistributionKind::inviscid);
    expectSamePoint(oneShot.value(), direct.value());
  }
}

TEST(unit, LibraryOneShotL4ViscousAlphaMatchesSession) {
  const AnalysisRequest request = makeRequest(
      nacaGeometry("2412"), viscousConfig(),
      AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> oneShot = xfoil::analyze(request);
  const Result<OperatingPointData> direct = directAnalysis(request);
  CHECK(oneShot.isOk());
  CHECK(direct.isOk());
  if (oneShot.isOk() && direct.isOk()) {
    CHECK(oneShot.value().convergence == ConvergenceStatus::converged);
    CHECK(oneShot.value().viscous);
    CHECK(oneShot.value().surface.has_value());
    CHECK(oneShot.value().surface->kind ==
          SurfaceDistributionKind::viscous);
    CHECK(oneShot.value().dragCoefficientPressure.has_value());
    expectSamePoint(oneShot.value(), direct.value());
  }
}

TEST(unit, LibraryOneShotL4LiftTargetMatchesSession) {
  const AnalysisRequest request = makeRequest(
      nacaGeometry("0012"), inviscidConfig(), LiftTarget{0.5});
  const Result<OperatingPointData> oneShot = xfoil::analyze(request);
  const Result<OperatingPointData> direct = directAnalysis(request);
  CHECK(oneShot.isOk());
  CHECK(direct.isOk());
  if (oneShot.isOk()) {
    CHECK(oneShot.value().convergence == ConvergenceStatus::converged);
    CHECK_NEAR(oneShot.value().liftCoefficient, 0.5, 1.0e-6, 1.0e-6);
  }
  if (oneShot.isOk() && direct.isOk()) {
    expectSamePoint(oneShot.value(), direct.value());
  }
}

TEST(unit, LibraryOneShotL4ErrorOrder) {
  AnalysisConfig invalidConfig;
  invalidConfig.reynoldsNumber = -1.0;
  PanelingOptions invalidPaneling;
  invalidPaneling.panelCount = 1;

  AnalysisRequest geometryFirst;
  geometryFirst.paneling = invalidPaneling;
  geometryFirst.configuration = invalidConfig;
  geometryFirst.target = AlphaTarget{Degrees{2.0}};
  const Result<OperatingPointData> geometryError =
      xfoil::analyze(geometryFirst);
  CHECK(geometryError.isErr());
  CHECK_EQ(geometryError.error().code, std::string{"geometry.empty"});

  AnalysisRequest panelingSecond = makeRequest(
      nacaGeometry("0012"), invalidConfig, AlphaTarget{Degrees{2.0}},
      invalidPaneling);
  const Result<OperatingPointData> panelingError =
      xfoil::analyze(panelingSecond);
  CHECK(panelingError.isErr());
  CHECK_EQ(panelingError.error().code, std::string{"paneling.panelCount"});

  AnalysisRequest configThird = makeRequest(
      nacaGeometry("0012"), invalidConfig, AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> configError =
      xfoil::analyze(configThird);
  CHECK(configError.isErr());
  CHECK_EQ(configError.error().code,
           std::string{"config.invalidReynoldsNumber"});

  AnalysisRequest badAlpha = makeRequest(
      nacaGeometry("0012"), inviscidConfig(),
      AlphaTarget{Degrees{std::nan("")}});
  const Result<OperatingPointData> alphaError = xfoil::analyze(badAlpha);
  CHECK(alphaError.isErr());
  CHECK_EQ(alphaError.error().code, std::string{"analysis.invalidAlpha"});

  AnalysisRequest badCl = makeRequest(
      nacaGeometry("0012"), inviscidConfig(), LiftTarget{std::nan("")});
  const Result<OperatingPointData> clError = xfoil::analyze(badCl);
  CHECK(clError.isErr());
  CHECK_EQ(clError.error().code,
           std::string{"analysis.invalidLiftCoefficient"});

  PanelingOptions overCapacity;
  overCapacity.panelCount = 596;
  AnalysisRequest capacity = makeRequest(
      nacaGeometry("0012"), inviscidConfig(), AlphaTarget{Degrees{2.0}},
      overCapacity);
  const Result<OperatingPointData> capacityError = xfoil::analyze(capacity);
  CHECK(capacityError.isErr());
  CHECK_EQ(capacityError.error().code, std::string{"paneling.panelCount"});
  CHECK(capacityError.error().category ==
        xfoil::ErrorCategory::capacityExceeded);
}

TEST(unit, LibraryOneShotL4NonConvergenceIsStructuredValue) {
  AnalysisConfig config = viscousConfig();
  config.maximumIterations = 1;
  const AnalysisRequest request = makeRequest(
      nacaGeometry("0012"), config, AlphaTarget{Degrees{12.0}});
  const Result<OperatingPointData> result = xfoil::analyze(request);
  CHECK(result.isOk());
  if (result.isOk()) {
    CHECK(result.value().convergence == ConvergenceStatus::unconverged);
    CHECK(result.value().viscous);
    CHECK(result.value().surface.has_value());
  }
}

TEST(unit, LibraryOneShotL4CancellationMatchesSession) {
  const AnalysisRequest request = makeRequest(
      nacaGeometry("0012"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  CancellationToken token;
  token.requestCancellation();
  const Result<OperatingPointData> oneShot = xfoil::analyze(request, &token);
  CHECK(oneShot.isErr());
  CHECK_EQ(oneShot.error().code, std::string{"analysis.cancelled"});
  CHECK(oneShot.error().category == xfoil::ErrorCategory::cancelled);

  Session directSession;
  CHECK(directSession.setGeometry(request.geometry).isOk());
  CHECK(directSession.generatePanel(request.paneling).isOk());
  CHECK(directSession.configure(request.configuration).isOk());
  const Result<OperatingPointData> direct =
      directSession.solveAlpha(Degrees{2.0}, &token);
  CHECK(direct.isErr());
  CHECK_EQ(direct.error().code, oneShot.error().code);
  CHECK(direct.error().category == oneShot.error().category);
}

TEST(unit, LibraryOneShotL4DeterminismAndIsolation) {
  const AnalysisRequest valid = makeRequest(
      nacaGeometry("2412"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> first = xfoil::analyze(valid);
  CHECK(first.isOk());

  AnalysisConfig invalidConfig;
  invalidConfig.machNumber = std::nan("");
  const AnalysisRequest invalid = makeRequest(
      nacaGeometry("0012"), invalidConfig, LiftTarget{0.3});
  const Result<OperatingPointData> failed = xfoil::analyze(invalid);
  CHECK(failed.isErr());

  const Result<OperatingPointData> second = xfoil::analyze(valid);
  CHECK(second.isOk());
  if (first.isOk() && second.isOk()) {
    expectSamePoint(first.value(), second.value());
  }

  const AnalysisRequest validAgain = makeRequest(
      nacaGeometry("2412"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> third = xfoil::analyze(validAgain);
  CHECK(third.isOk());
  if (first.isOk() && third.isOk()) {
    expectSamePoint(first.value(), third.value());
  }
}

TEST(unit, LibraryOneShotL4NoIoSideEffects) {
  testing::ScopedTempDirectory work("library-one-shot-l4-no-io", true);
  CHECK(std::filesystem::is_empty(work.path()));

  const AnalysisRequest request = makeRequest(
      nacaGeometry("0012"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
  std::streambuf* oldErr = std::cerr.rdbuf(err.rdbuf());
  std::optional<Result<OperatingPointData>> result;
  result = xfoil::analyze(request);
  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);

  CHECK(result.has_value());
  CHECK(result->isOk());
  CHECK(out.str().empty());
  CHECK(err.str().empty());
  CHECK(std::filesystem::is_empty(work.path()));
}

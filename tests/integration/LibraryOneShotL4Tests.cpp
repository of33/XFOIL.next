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

// File: tests/integration/LibraryOneShotL4Tests.cpp
// End-to-end one-shot workflows: L3 parity, XFoilSession panel parity,
// errors, isolation, determinism and no file/console I/O.

#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Analyze.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/XFoilSession.h"
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
using xfoil::LiftTarget;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::Result;
using xfoil::Session;
using xfoil::SurfaceDistributionKind;
using xfoil::SurfaceGeometry;

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

void expectSamePoint(const OperatingPointData& actual,
                     const OperatingPointData& expected) {
  CHECK_NEAR(actual.alpha.value, expected.alpha.value, 0.0, 0.0);
  CHECK_NEAR(actual.liftCoefficient, expected.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(actual.dragCoefficient, expected.dragCoefficient, 0.0, 0.0);
  CHECK_NEAR(actual.momentCoefficient, expected.momentCoefficient, 0.0,
             0.0);
  CHECK(actual.convergence == expected.convergence);
  CHECK_EQ(actual.iterations, expected.iterations);
  CHECK_EQ(actual.viscous, expected.viscous);
  CHECK_EQ(actual.surface.has_value(), expected.surface.has_value());
  if (actual.surface.has_value() && expected.surface.has_value()) {
    CHECK(actual.surface->kind == expected.surface->kind);
    CHECK_EQ(actual.surface->cp.size(), expected.surface->cp.size());
    const std::size_t count = actual.surface->cp.size() <
                                      expected.surface->cp.size()
                                  ? actual.surface->cp.size()
                                  : expected.surface->cp.size();
    for (std::size_t index = 0; index < count; ++index) {
      CHECK_NEAR(actual.surface->cp[index], expected.surface->cp[index],
                 0.0, 0.0);
    }
  }
}

void expectSameRequest(const AnalysisRequest& before,
                       const AnalysisRequest& after) {
  CHECK_EQ(before.geometry.pointCount(), after.geometry.pointCount());
  CHECK_EQ(before.paneling.panelCount, after.paneling.panelCount);
  CHECK_NEAR(before.configuration.reynoldsNumber,
             after.configuration.reynoldsNumber, 0.0, 0.0);
  CHECK_EQ(before.configuration.viscous, after.configuration.viscous);
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

TEST(integration, LibraryOneShotL4FullWorkflow) {
  const AirfoilGeometry geometry = nacaGeometry("2412");
  CHECK(geometry.pointCount() > 0);
  const AnalysisRequest request = makeRequest(
      geometry, inviscidConfig(), AlphaTarget{Degrees{2.0}});
  const AnalysisRequest before = request;

  const Result<OperatingPointData> oneShot = xfoil::analyze(request);
  CHECK(oneShot.isOk());
  expectSameRequest(before, request);

  Session directSession;
  CHECK(directSession.setGeometry(request.geometry).isOk());
  const Result<SurfaceGeometry> directPanel =
      directSession.generatePanel(request.paneling);
  CHECK(directPanel.isOk());
  CHECK(directSession.configure(request.configuration).isOk());
  const Result<OperatingPointData> direct =
      directSession.solveAlpha(Degrees{2.0});
  CHECK(direct.isOk());
  if (oneShot.isOk() && direct.isOk()) {
    CHECK(oneShot.value().convergence == ConvergenceStatus::converged);
    CHECK(oneShot.value().surface.has_value());
    CHECK(oneShot.value().surface->kind ==
          SurfaceDistributionKind::inviscid);
    CHECK_EQ(oneShot.value().surface->cp.size(),
             directPanel.value().points.size());
    expectSamePoint(oneShot.value(), direct.value());
  }

  // Same panel contract, compared through the existing XFoilSession:
  // feed the PANGEN panel back as an unpaneled geometry. XFoilSession
  // then uses ABCOPY on exactly those panel nodes.
  if (directPanel.isOk()) {
    AirfoilGeometry panelGeometry;
    const auto assigned =
        panelGeometry.assign("panel", directPanel.value().points);
    CHECK(assigned.success());
    xfoil::XFoilSession reference;
    CHECK(reference.setGeometry(panelGeometry).success());
    CHECK(reference.configure(request.configuration).success());
    const xfoil::AnalysisResult xfoilResult =
        reference.analyzeAtAlpha(Degrees{2.0});
    CHECK(xfoilResult.successful);
    CHECK(xfoilResult.converged);
    if (oneShot.isOk()) {
      CHECK_NEAR(oneShot.value().liftCoefficient,
                 xfoilResult.liftCoefficient, 0.0, 0.0);
      CHECK_NEAR(oneShot.value().dragCoefficient,
                 xfoilResult.dragCoefficient, 0.0, 0.0);
      CHECK_NEAR(oneShot.value().momentCoefficient,
                 xfoilResult.momentCoefficient, 0.0, 0.0);
      CHECK_EQ(oneShot.value().iterations, xfoilResult.iterations);
    }
  }

  const Result<OperatingPointData> repeat = xfoil::analyze(request);
  CHECK(repeat.isOk());
  if (oneShot.isOk() && repeat.isOk()) {
    expectSamePoint(oneShot.value(), repeat.value());
  }

  const AnalysisRequest viscousRequest = makeRequest(
      geometry, viscousConfig(), AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> viscous =
      xfoil::analyze(viscousRequest);
  CHECK(viscous.isOk());
  if (viscous.isOk()) {
    CHECK(viscous.value().convergence == ConvergenceStatus::converged);
    CHECK(viscous.value().viscous);
    CHECK(viscous.value().surface.has_value());
    CHECK(viscous.value().surface->kind ==
          SurfaceDistributionKind::viscous);
    CHECK(viscous.value().dragCoefficientPressure.has_value());
  }

  const AnalysisRequest clRequest = makeRequest(
      nacaGeometry("0012"), inviscidConfig(), LiftTarget{0.5});
  const Result<OperatingPointData> cl = xfoil::analyze(clRequest);
  CHECK(cl.isOk());
  if (cl.isOk()) {
    CHECK(cl.value().convergence == ConvergenceStatus::converged);
    CHECK_NEAR(cl.value().liftCoefficient, 0.5, 1.0e-6, 1.0e-6);
  }
}

TEST(integration, LibraryOneShotL4ErrorsIsolationAndCancellation) {
  const AnalysisRequest valid = makeRequest(
      nacaGeometry("2412"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  const Result<OperatingPointData> before = xfoil::analyze(valid);
  CHECK(before.isOk());

  AnalysisConfig invalidConfig;
  invalidConfig.reynoldsNumber = -1.0;
  const AnalysisRequest invalid = makeRequest(
      nacaGeometry("0012"), invalidConfig, LiftTarget{0.3});
  const Result<OperatingPointData> failed = xfoil::analyze(invalid);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"config.invalidReynoldsNumber"});

  const Result<OperatingPointData> after = xfoil::analyze(valid);
  CHECK(after.isOk());
  if (before.isOk() && after.isOk()) {
    expectSamePoint(before.value(), after.value());
  }

  AnalysisRequest geometryFirst;
  geometryFirst.configuration = invalidConfig;
  const Result<OperatingPointData> geometryError =
      xfoil::analyze(geometryFirst);
  CHECK(geometryError.isErr());
  CHECK_EQ(geometryError.error().code, std::string{"geometry.empty"});

  PanelingOptions invalidPaneling;
  invalidPaneling.panelCount = 1;
  const AnalysisRequest panelingSecond = makeRequest(
      nacaGeometry("0012"), invalidConfig, AlphaTarget{Degrees{2.0}},
      invalidPaneling);
  const Result<OperatingPointData> panelingError =
      xfoil::analyze(panelingSecond);
  CHECK(panelingError.isErr());
  CHECK_EQ(panelingError.error().code, std::string{"paneling.panelCount"});

  const AnalysisRequest badAlpha = makeRequest(
      nacaGeometry("0012"), inviscidConfig(),
      AlphaTarget{Degrees{std::nan("")}});
  const Result<OperatingPointData> alphaError = xfoil::analyze(badAlpha);
  CHECK(alphaError.isErr());
  CHECK_EQ(alphaError.error().code, std::string{"analysis.invalidAlpha"});

  const AnalysisRequest badCl = makeRequest(
      nacaGeometry("0012"), inviscidConfig(), LiftTarget{std::nan("")});
  const Result<OperatingPointData> clError = xfoil::analyze(badCl);
  CHECK(clError.isErr());
  CHECK_EQ(clError.error().code,
           std::string{"analysis.invalidLiftCoefficient"});

  PanelingOptions overCapacity;
  overCapacity.panelCount = 596;
  const AnalysisRequest capacity = makeRequest(
      nacaGeometry("0012"), inviscidConfig(), AlphaTarget{Degrees{2.0}},
      overCapacity);
  const Result<OperatingPointData> capacityError = xfoil::analyze(capacity);
  CHECK(capacityError.isErr());
  CHECK(capacityError.error().category ==
        xfoil::ErrorCategory::capacityExceeded);

  CancellationToken token;
  token.requestCancellation();
  const Result<OperatingPointData> cancelled =
      xfoil::analyze(valid, &token);
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});

  AnalysisConfig limited = viscousConfig();
  limited.maximumIterations = 1;
  const AnalysisRequest nonConverged = makeRequest(
      nacaGeometry("0012"), limited, AlphaTarget{Degrees{12.0}});
  const Result<OperatingPointData> partial =
      xfoil::analyze(nonConverged);
  CHECK(partial.isOk());
  if (partial.isOk()) {
    CHECK(partial.value().convergence == ConvergenceStatus::unconverged);
    CHECK(partial.value().surface.has_value());
  }
}

TEST(integration, LibraryOneShotL4NoIoSideEffects) {
  testing::ScopedTempDirectory work("library-one-shot-l4-integration", true);
  CHECK(std::filesystem::is_empty(work.path()));

  const AnalysisRequest request = makeRequest(
      nacaGeometry("0012"), inviscidConfig(),
      AlphaTarget{Degrees{2.0}});
  const AnalysisRequest invalid = makeRequest(
      nacaGeometry("0012"), inviscidConfig(),
      AlphaTarget{Degrees{std::nan("")}});

  std::ostringstream out;
  std::ostringstream err;
  std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
  std::streambuf* oldErr = std::cerr.rdbuf(err.rdbuf());

  std::optional<Result<OperatingPointData>> first;
  std::optional<Result<OperatingPointData>> failed;
  std::optional<Result<OperatingPointData>> second;
  first = xfoil::analyze(request);
  failed = xfoil::analyze(invalid);
  second = xfoil::analyze(request);

  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);

  CHECK(first.has_value());
  CHECK(first->isOk());
  CHECK(failed.has_value());
  CHECK(failed->isErr());
  CHECK(second.has_value());
  CHECK(second->isOk());
  CHECK(out.str().empty());
  CHECK(err.str().empty());
  CHECK(std::filesystem::is_empty(work.path()));
}

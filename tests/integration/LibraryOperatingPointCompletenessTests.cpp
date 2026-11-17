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

// File: tests/integration/LibraryOperatingPointCompletenessTests.cpp
// End-to-end operating-point CDF and optional owning Cf coverage.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/SkinFriction.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::AlphaTarget;
using xfoil::AnalysisConfig;
using xfoil::AnalysisRequest;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::LiftTarget;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::Result;
using xfoil::Session;
using xfoil::SkinFrictionDistribution;
using xfoil::SurfaceSide;

AnalysisConfig viscousConfig(int iterations = 50) {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = iterations;
  return config;
}

AnalysisConfig inviscidConfig() {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.maximumIterations = 40;
  return config;
}

PanelingOptions paneling() {
  PanelingOptions options;
  options.panelCount = std::size_t{160};
  return options;
}

void makeReady(Session& session, const AnalysisConfig& config) {
  CHECK(session.setNaca("0012").isOk());
  CHECK(session.generatePanel(paneling()).isOk());
  CHECK(session.configure(config).isOk());
}

xfoil::geometry::AirfoilGeometry nacaGeometry(const std::string& name) {
  Session session;
  const Result<xfoil::geometry::AirfoilGeometry> geometry =
      session.setNaca(name);
  CHECK(geometry.isOk());
  return geometry.isOk() ? geometry.value()
                         : xfoil::geometry::AirfoilGeometry{};
}

AnalysisRequest makeRequest(const xfoil::geometry::AirfoilGeometry& geometry,
                            const AnalysisConfig& config,
                            xfoil::AnalysisTarget target) {
  AnalysisRequest request;
  request.geometry = geometry;
  request.paneling = paneling();
  request.configuration = config;
  request.target = std::move(target);
  return request;
}

void expectSameDistribution(const SkinFrictionDistribution& actual,
                            const SkinFrictionDistribution& expected) {
  CHECK(actual.upper.side == SurfaceSide::upper);
  CHECK(actual.lower.side == SurfaceSide::lower);
  CHECK_EQ(actual.upper.x.size(), expected.upper.x.size());
  CHECK_EQ(actual.upper.y.size(), expected.upper.y.size());
  CHECK_EQ(actual.upper.s.size(), expected.upper.s.size());
  CHECK_EQ(actual.upper.cf.size(), expected.upper.cf.size());
  CHECK_EQ(actual.lower.x.size(), expected.lower.x.size());
  CHECK_EQ(actual.lower.y.size(), expected.lower.y.size());
  CHECK_EQ(actual.lower.s.size(), expected.lower.s.size());
  CHECK_EQ(actual.lower.cf.size(), expected.lower.cf.size());
  for (std::size_t index = 0; index < actual.upper.cf.size(); ++index) {
    CHECK_NEAR(actual.upper.x[index], expected.upper.x[index], 0.0, 0.0);
    CHECK_NEAR(actual.upper.y[index], expected.upper.y[index], 0.0, 0.0);
    CHECK_NEAR(actual.upper.s[index], expected.upper.s[index], 0.0, 0.0);
    CHECK_NEAR(actual.upper.cf[index], expected.upper.cf[index], 0.0, 0.0);
  }
  for (std::size_t index = 0; index < actual.lower.cf.size(); ++index) {
    CHECK_NEAR(actual.lower.x[index], expected.lower.x[index], 0.0, 0.0);
    CHECK_NEAR(actual.lower.y[index], expected.lower.y[index], 0.0, 0.0);
    CHECK_NEAR(actual.lower.s[index], expected.lower.s[index], 0.0, 0.0);
    CHECK_NEAR(actual.lower.cf[index], expected.lower.cf[index], 0.0, 0.0);
  }
}

void expectViscousCompleteness(const OperatingPointData& point,
                               const SkinFrictionDistribution& current) {
  CHECK(point.viscous);
  CHECK(point.dragCoefficientPressure.has_value());
  CHECK(point.dragCoefficientFriction.has_value());
  CHECK(point.skinFriction.has_value());
  CHECK(std::isfinite(point.dragCoefficientFriction.value()));
  CHECK(point.dragCoefficientFriction.value() >= 0.0);
  expectSameDistribution(point.skinFriction.value(), current);
}

}  // namespace

TEST(integration, LibraryOperatingPointCompletenessViscousAlpha) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<OperatingPointData> point =
      session.solveAlpha(Degrees{4.0});
  CHECK(point.isOk());
  const Result<SkinFrictionDistribution> current =
      session.currentSkinFriction();
  CHECK(current.isOk());
  expectViscousCompleteness(point.value(), current.value());
}

TEST(integration, LibraryOperatingPointCompletenessViscousCl) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<OperatingPointData> point = session.solveCl(0.5);
  CHECK(point.isOk());
  const Result<SkinFrictionDistribution> current =
      session.currentSkinFriction();
  CHECK(current.isOk());
  expectViscousCompleteness(point.value(), current.value());
}

TEST(integration, LibraryOperatingPointCompletenessOneShotOwnsCf) {
  const auto geometry = nacaGeometry("2412");
  const Result<OperatingPointData> oneShot = xfoil::analyze(
      makeRequest(geometry, viscousConfig(),
                  AlphaTarget{Degrees{4.0}}));
  CHECK(oneShot.isOk());
  CHECK(oneShot.value().viscous);
  CHECK(oneShot.value().dragCoefficientFriction.has_value());
  CHECK(oneShot.value().skinFriction.has_value());
  CHECK(!oneShot.value().skinFriction->upper.cf.empty());
  CHECK(!oneShot.value().skinFriction->lower.cf.empty());
  CHECK(std::isfinite(oneShot.value().dragCoefficientFriction.value()));
  CHECK_EQ(oneShot.value().skinFriction->upper.cf.size(),
           oneShot.value().skinFriction->upper.s.size());
  CHECK_EQ(oneShot.value().skinFriction->lower.cf.size(),
           oneShot.value().skinFriction->lower.s.size());
}

TEST(integration, LibraryOperatingPointCompletenessOneShotClOwnsCf) {
  const auto geometry = nacaGeometry("2412");
  const Result<OperatingPointData> oneShot = xfoil::analyze(
      makeRequest(geometry, viscousConfig(), LiftTarget{0.5}));
  CHECK(oneShot.isOk());
  CHECK(oneShot.value().dragCoefficientFriction.has_value());
  CHECK(oneShot.value().skinFriction.has_value());
}

TEST(integration, LibraryOperatingPointCompletenessInviscidHasNoCdfOrCf) {
  Session session;
  makeReady(session, inviscidConfig());
  const Result<OperatingPointData> point =
      session.solveAlpha(Degrees{3.0});
  CHECK(point.isOk());
  CHECK(!point.value().viscous);
  CHECK(!point.value().dragCoefficientPressure.has_value());
  CHECK(!point.value().dragCoefficientFriction.has_value());
  CHECK(!point.value().skinFriction.has_value());
  CHECK(session.currentSkinFriction().isErr());
}

TEST(integration, LibraryOperatingPointCompletenessUnconvergedKeepsState) {
  Session session;
  makeReady(session, viscousConfig(1));
  const Result<OperatingPointData> point =
      session.solveAlpha(Degrees{4.0});
  CHECK(point.isOk());
  CHECK(point.value().convergence == ConvergenceStatus::unconverged);
  CHECK(point.value().dragCoefficientFriction.has_value());
  CHECK(point.value().skinFriction.has_value());
  CHECK(session.currentSkinFriction().isOk());
}

TEST(integration, LibraryOperatingPointCompletenessFollowPointsStayOwned) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<OperatingPointData> first =
      session.solveAlpha(Degrees{4.0});
  CHECK(first.isOk());
  OperatingPointData owned = first.value();

  CHECK(session.configure(inviscidConfig()).isOk());
  const Result<OperatingPointData> inviscid =
      session.solveAlpha(Degrees{3.0});
  CHECK(inviscid.isOk());
  CHECK(!inviscid.value().dragCoefficientFriction.has_value());
  CHECK(!inviscid.value().skinFriction.has_value());
  CHECK(owned.dragCoefficientFriction.has_value());
  CHECK(owned.skinFriction.has_value());

  CHECK(session.configure(viscousConfig()).isOk());
  const Result<OperatingPointData> second =
      session.solveAlpha(Degrees{2.0});
  CHECK(second.isOk());
  CHECK(second.value().dragCoefficientFriction.has_value());
  CHECK(second.value().skinFriction.has_value());
  CHECK_NEAR(owned.alpha.value, 4.0, 0.0, 0.0);
  CHECK_NEAR(first.value().alpha.value, 4.0, 0.0, 0.0);
}

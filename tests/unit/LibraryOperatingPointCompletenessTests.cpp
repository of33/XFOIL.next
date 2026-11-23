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

// File: tests/unit/LibraryOperatingPointCompletenessTests.cpp
// Public operating-point drag/Cf completeness and projection contracts.

#include <cmath>
#include <cstddef>
#include <utility>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Result.h"
#include "xfoil/SkinFriction.h"
#include "xfoil/io/ExportFormats.h"
#include "xfoil/session/SessionProjections.h"
#include "xfoil/session/SessionState.h"

namespace {

using xfoil::AnalysisResult;
using xfoil::AnalysisStatus;
using xfoil::Diagnostic;
using xfoil::ErrorCategory;
using xfoil::OperatingPointData;
using xfoil::Result;
using xfoil::SkinFrictionDistribution;
using xfoil::SkinFrictionSide;
using xfoil::SurfaceSide;

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
  session.viscous.viscals.gam = {-0.1, 0.4, -0.2, 0.3};
  session.viscous.viscals.minf = 0.12;
  session.operating.qinf = 1.0;
  if (!viscous) {
    return;
  }
  session.viscous.viscals.cdp = 0.004;
  session.viscous.viscals.cdf = 0.006;
  session.viscous.viscals.mapping.valid = true;
  auto& top = session.viscous.viscals.mapping.top;
  top.iblte = 2;
  top.nbl = 2;
  top.ipan = {0, 1, 2};
  top.vti = {1, 1, 1};
  auto& bottom = session.viscous.viscals.mapping.bottom;
  bottom.iblte = 2;
  bottom.nbl = 2;
  bottom.ipan = {0, 2, 3};
  bottom.vti = {-1, -1, -1};
  session.viscous.viscals.arcs.top = {0.0, 0.1, 0.2};
  session.viscous.viscals.arcs.bottom = {0.0, 0.1, 0.2};
  session.viscous.viscals.tau.top = {0.0, 0.006, 0.007};
  session.viscous.viscals.tau.bottom = {0.0, 0.008, 0.009};
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

}  // namespace

TEST(unit, LibraryOperatingPointCompletenessDefaultsAndOwnership) {
  OperatingPointData point;
  CHECK(!point.dragCoefficientFriction.has_value());
  CHECK(!point.skinFriction.has_value());
  CHECK(!point.pressureExport.has_value());

  point.pressureExport.emplace();
  point.pressureExport->machNumber = 0.12;
  point.pressureExport->qinf = 1.0;
  point.pressureExport->x = {0.0, 1.0};
  point.pressureExport->gam = {0.2, -0.2};
  point.dragCoefficientFriction = 0.004;
  point.skinFriction.emplace();
  point.skinFriction->upper.side = SurfaceSide::upper;
  point.skinFriction->lower.side = SurfaceSide::lower;
  point.skinFriction->upper.x = {0.0, 1.0};
  point.skinFriction->upper.y = {0.0, 0.0};
  point.skinFriction->upper.s = {0.0, 1.0};
  point.skinFriction->upper.cf = {0.1, 0.2};

  OperatingPointData copy = point;
  CHECK(copy.dragCoefficientFriction.has_value());
  CHECK(copy.skinFriction.has_value());
  CHECK(copy.pressureExport.has_value());
  CHECK_EQ(copy.pressureExport->x.size(), std::size_t{2});
  CHECK_EQ(copy.skinFriction->upper.cf.size(), std::size_t{2});
  point.skinFriction->upper.cf[0] = 99.0;
  point.pressureExport->x[0] = 99.0;
  CHECK_NEAR(copy.skinFriction->upper.cf[0], 0.1, 0.0, 0.0);
  CHECK_NEAR(copy.pressureExport->x[0], 0.0, 0.0, 0.0);

  OperatingPointData moved = std::move(copy);
  CHECK(moved.dragCoefficientFriction.has_value());
  CHECK(moved.skinFriction.has_value());
  CHECK(moved.pressureExport.has_value());
}

TEST(unit, LibraryOperatingPointCompletenessViscousProjection) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);

  const Result<SkinFrictionDistribution> expectedCf =
      xfoil::session::projectSkinFriction(session);
  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(expectedCf.isOk());
  CHECK(projected.isOk());
  CHECK(projected.value().dragCoefficientFriction.has_value());
  CHECK(projected.value().skinFriction.has_value());
  CHECK(projected.value().pressureExport.has_value());
  CHECK_EQ(projected.value().pressureExport->x.size(), std::size_t{4});
  CHECK_EQ(projected.value().pressureExport->gam.size(), std::size_t{4});
  CHECK_NEAR(projected.value().pressureExport->machNumber, 0.12, 0.0, 0.0);
  CHECK_NEAR(projected.value().pressureExport->qinf, 1.0, 0.0, 0.0);
  const auto cpText = xfoil::io::pressureDumpText(
      xfoil::io::PressureDumpInput{4,
          xfoil::io::DumpDelimiter::kBlank,
          projected.value().pressureExport->machNumber,
          projected.value().pressureExport->qinf,
          projected.value().pressureExport->x,
          projected.value().pressureExport->gam});
  CHECK(cpText.find("#      x          Cp") == 0);
  CHECK_EQ(cpText.find('\n', cpText.find('\n') + 1) != std::string::npos,
           true);
  CHECK_NEAR(projected.value().dragCoefficientFriction.value(),
             session.viscous.viscals.cdf, 0.0, 0.0);
  expectSameDistribution(projected.value().skinFriction.value(),
                         expectedCf.value());

  OperatingPointData owned = projected.value();
  session.viscous.viscals.cdf = 0.02;
  session.viscous.viscals.tau.top[1] = 0.02;
  CHECK_NEAR(owned.dragCoefficientFriction.value(), 0.006, 0.0, 0.0);
  CHECK_NEAR(owned.skinFriction->upper.cf[0], expectedCf.value().upper.cf[0],
             0.0, 0.0);
}

TEST(unit, LibraryOperatingPointCompletenessInviscidHasNoDragDetails) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, false);

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isOk());
  CHECK(!projected.value().viscous);
  CHECK(!projected.value().dragCoefficientPressure.has_value());
  CHECK(!projected.value().dragCoefficientFriction.has_value());
  CHECK(!projected.value().skinFriction.has_value());
  CHECK(projected.value().pressureExport.has_value());
}

TEST(unit, LibraryOperatingPointCompletenessRejectsInvalidCdf) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);
  session.viscous.viscals.cdf = std::nan("");

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isErr());
  CHECK(projected.error().category == ErrorCategory::internal);
  CHECK_EQ(projected.error().code, std::string{"analysis.inconsistentState"});
}

TEST(unit, LibraryOperatingPointCompletenessRejectsInvalidCfState) {
  xfoil::session::SessionComposition session = makeSessionWithPanel();
  setSuccessfulPoint(session, true);
  session.viscous.viscals.tau.top[1] = std::nan("");

  const Result<OperatingPointData> projected =
      xfoil::session::projectLastOperatingPoint(session);
  CHECK(projected.isErr());
  CHECK(projected.error().category == ErrorCategory::internal);
  CHECK_EQ(projected.error().code,
           std::string{"skinFriction.inconsistentState"});
}

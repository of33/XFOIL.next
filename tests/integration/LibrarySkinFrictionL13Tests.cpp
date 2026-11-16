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

// File: tests/integration/LibrarySkinFrictionL13Tests.cpp
// Integration coverage of the owning Cf projection, BLDUMP parity,
// cancellation/reuse and isolated sessions.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/SkinFriction.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/paneling/PanelParameters.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionProjections.h"
#include "xfoil/session/SessionState.h"

namespace {

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

void makeReady(Session& session, const char* designation,
               const AnalysisConfig& config, int panels = 160) {
  CHECK(session.setNaca(designation).isOk());
  PanelingOptions options;
  options.panelCount = static_cast<std::size_t>(panels);
  CHECK(session.generatePanel(options).isOk());
  CHECK(session.configure(config).isOk());
}

Result<SkinFrictionDistribution> solveAndProject(
    Session& session, double alpha = 4.0) {
  const Result<OperatingPointData> point =
      session.solveAlpha(Degrees{alpha});
  CHECK(point.isOk());
  return session.currentSkinFriction();
}

bool prepareInternal(xfoil::session::SessionComposition& composition,
                     int naca, int panels, int iterations, double alpha) {
  const auto nacaResult = xfoil::core::geometry::nacaAirfoil(naca, false, 200);
  if (!nacaResult.isOk()) {
    return false;
  }
  std::vector<xfoil::Point2D> points;
  points.reserve(nacaResult.value().buffer.x.size());
  for (std::size_t index = 0; index < nacaResult.value().buffer.x.size();
       ++index) {
    points.push_back(xfoil::Point2D{nacaResult.value().buffer.x[index],
                                    nacaResult.value().buffer.y[index]});
  }
  xfoil::geometry::AirfoilGeometry geometry;
  if (!geometry.assign("internal", points).success()) {
    return false;
  }
  if (!xfoil::session::setGeometry(composition, geometry).success()) {
    return false;
  }
  xfoil::core::paneling::PanelingParameters parameters;
  parameters.panelCount = panels;
  if (!xfoil::session::setPanelingParameters(composition, parameters).ok) {
    return false;
  }
  if (!xfoil::session::applyPaneling(composition).ok) {
    return false;
  }
  if (!xfoil::session::configure(composition, viscousConfig(iterations))
           .success()) {
    return false;
  }
  return xfoil::session::runSpecAlpha(composition, Degrees{alpha}, nullptr)
      .successful;
}

void checkDistribution(const SkinFrictionDistribution& distribution) {
  CHECK(distribution.upper.side == SurfaceSide::upper);
  CHECK(distribution.lower.side == SurfaceSide::lower);
  CHECK(!distribution.upper.s.empty());
  CHECK(!distribution.lower.s.empty());
  CHECK_EQ(distribution.upper.x.size(), distribution.upper.s.size());
  CHECK_EQ(distribution.upper.y.size(), distribution.upper.s.size());
  CHECK_EQ(distribution.upper.cf.size(), distribution.upper.s.size());
  CHECK_EQ(distribution.lower.x.size(), distribution.lower.s.size());
  CHECK_EQ(distribution.lower.y.size(), distribution.lower.s.size());
  CHECK_EQ(distribution.lower.cf.size(), distribution.lower.s.size());
  for (const auto* side : {&distribution.upper, &distribution.lower}) {
    for (std::size_t index = 0; index < side->cf.size(); ++index) {
      CHECK(std::isfinite(side->cf[index]));
      if (index > 0) {
        CHECK(side->s[index] > side->s[index - 1]);
      }
    }
  }
}

}  // namespace

TEST(integration, LibrarySkinFrictionL13Naca0012Converged) {
  Session session;
  makeReady(session, "0012", viscousConfig());
  const Result<SkinFrictionDistribution> cf =
      solveAndProject(session, 4.0);
  CHECK(cf.isOk());
  checkDistribution(cf.value());
  CHECK(cf.value().upper.cf.size() != cf.value().lower.cf.size());
  CHECK(cf.value().upper.cf.front() > 0.0);
  CHECK(cf.value().upper.cf.back() > 0.0);
}

TEST(integration, LibrarySkinFrictionL13Naca2412MatchesBlDump) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 2412, 160, 50, 4.0));
  const auto projected = xfoil::session::projectSkinFriction(composition);
  CHECK(projected.isOk());
  checkDistribution(projected.value());

  const auto dump = xfoil::session::currentBoundaryLayerDumpInput(composition);
  CHECK_EQ(projected.value().upper.cf.size(),
           static_cast<std::size_t>(composition.viscous.viscals.mapping.top.iblte));
  CHECK(projected.value().lower.cf.size() <
        static_cast<std::size_t>(composition.viscous.viscals.mapping.bottom.nbl));

  xfoil::core::export_data::DumpFlowState flow;
  flow.qinf = dump.qinf;
  for (std::size_t index = 0; index < projected.value().upper.cf.size();
       ++index) {
    xfoil::core::export_data::BoundaryLayerDumpInput input;
    input.viscousStation = true;
    input.tau = dump.tau[0][index + 2];
    const auto row =
        xfoil::core::export_data::boundaryLayerDumpRow(input, flow);
    CHECK_NEAR(projected.value().upper.cf[index], row.cf, 0.0, 0.0);
  }
  for (std::size_t index = 0; index < projected.value().lower.cf.size();
       ++index) {
    xfoil::core::export_data::BoundaryLayerDumpInput input;
    input.viscousStation = true;
    input.tau = dump.tau[1][index + 2];
    const auto row =
        xfoil::core::export_data::boundaryLayerDumpRow(input, flow);
    CHECK_NEAR(projected.value().lower.cf[index], row.cf, 0.0, 0.0);
  }
}

TEST(integration, LibrarySkinFrictionL13UnconvergedAndInviscid) {
  Session unconverged;
  makeReady(unconverged, "0012", viscousConfig(1));
  const Result<OperatingPointData> point =
      unconverged.solveAlpha(Degrees{4.0});
  CHECK(point.isOk());
  CHECK(point.value().convergence == ConvergenceStatus::unconverged);
  const Result<SkinFrictionDistribution> lastState =
      unconverged.currentSkinFriction();
  CHECK(lastState.isOk());
  checkDistribution(lastState.value());

  Session inviscid;
  makeReady(inviscid, "0012", [] {
    AnalysisConfig config = viscousConfig();
    config.viscous = false;
    return config;
  }());
  CHECK(inviscid.solveAlpha(Degrees{2.0}).isOk());
  const Result<SkinFrictionDistribution> rejected =
      inviscid.currentSkinFriction();
  CHECK(rejected.isErr());
  CHECK_EQ(rejected.error().code, std::string{"skinFriction.notViscous"});
}

TEST(integration, LibrarySkinFrictionL13CancellationAndCallbackFailure) {
  Session session;
  makeReady(session, "0012", viscousConfig());
  CHECK(solveAndProject(session, 3.0).isOk());

  CancellationToken token;
  const ProgressCallback cancel = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration) {
      token.requestCancellation();
    }
  };
  const Result<OperatingPointData> cancelled =
      session.solveAlpha(Degrees{8.0}, &token, cancel);
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});
  const Result<SkinFrictionDistribution> stale =
      session.currentSkinFriction();
  CHECK(stale.isErr());
  CHECK_EQ(stale.error().code,
           std::string{"skinFriction.noValidViscousState"});
  token.reset();
  CHECK(solveAndProject(session, 5.0).isOk());

  const ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration) {
      throw 1;
    }
  };
  const Result<OperatingPointData> failed =
      session.solveAlpha(Degrees{9.0}, nullptr, throwing);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"progress.callbackFailure"});
  const Result<SkinFrictionDistribution> failedState =
      session.currentSkinFriction();
  CHECK(failedState.isErr());
  CHECK_EQ(failedState.error().code,
           std::string{"skinFriction.noValidViscousState"});
  CHECK(solveAndProject(session, 6.0).isOk());
}

TEST(integration, LibrarySkinFrictionL13OwningAndIsolation) {
  Session first;
  Session second;
  makeReady(first, "0012", viscousConfig());
  makeReady(second, "0012", viscousConfig());
  const Result<SkinFrictionDistribution> a = solveAndProject(first, 4.0);
  const Result<SkinFrictionDistribution> b = solveAndProject(second, 4.0);
  CHECK(a.isOk());
  CHECK(b.isOk());
  SkinFrictionDistribution owned = a.value();
  first.reset();
  second.reset();
  CHECK(!owned.upper.cf.empty());
  CHECK_EQ(owned.upper.cf.size(), a.value().upper.cf.size());
  CHECK(first.currentSkinFriction().isErr());
  CHECK(second.currentSkinFriction().isErr());

  Session repeat;
  makeReady(repeat, "0012", viscousConfig());
  const Result<SkinFrictionDistribution> c = solveAndProject(repeat, 4.0);
  CHECK(c.isOk());
  CHECK_EQ(c.value().upper.cf.size(), owned.upper.cf.size());
  for (std::size_t index = 0; index < owned.upper.cf.size(); ++index) {
    CHECK_NEAR(owned.upper.cf[index], c.value().upper.cf[index], 0.0, 0.0);
  }
}

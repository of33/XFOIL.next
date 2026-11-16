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

// File: tests/unit/LibrarySkinFrictionL13Tests.cpp
// Public owning skin-friction value and projection contracts.

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisResult.h"
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
using xfoil::AnalysisStatus;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::PanelingOptions;
using xfoil::ProgressCallback;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::Result;
using xfoil::Session;
using xfoil::SkinFrictionDistribution;
using xfoil::SkinFrictionSide;
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

void makeReady(Session& session, const AnalysisConfig& config,
               int panels = 160) {
  CHECK(session.setNaca("0012").isOk());
  PanelingOptions options;
  options.panelCount = static_cast<std::size_t>(panels);
  CHECK(session.generatePanel(options).isOk());
  CHECK(session.configure(config).isOk());
}

Result<SkinFrictionDistribution> solveAndProject(
    Session& session, double alpha = 4.0) {
  const Result<xfoil::OperatingPointData> point =
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
  AnalysisConfig config = viscousConfig(iterations);
  if (!xfoil::session::configure(composition, config).success()) {
    return false;
  }
  return xfoil::session::runSpecAlpha(composition, Degrees{alpha}, nullptr)
      .successful;
}

void checkSide(const SkinFrictionSide& side, SurfaceSide expected) {
  CHECK(side.side == expected);
  CHECK_EQ(side.x.size(), side.s.size());
  CHECK_EQ(side.y.size(), side.s.size());
  CHECK_EQ(side.cf.size(), side.s.size());
  CHECK(!side.s.empty());
  for (std::size_t index = 0; index < side.s.size(); ++index) {
    CHECK(std::isfinite(side.x[index]));
    CHECK(std::isfinite(side.y[index]));
    CHECK(std::isfinite(side.s[index]));
    CHECK(std::isfinite(side.cf[index]));
    if (index > 0) {
      CHECK(side.s[index] > side.s[index - 1]);
    }
  }
}

}  // namespace

TEST(unit, LibrarySkinFrictionL13ValueSemantics) {
  SkinFrictionSide defaultSide;
  CHECK(defaultSide.side == SurfaceSide::upper);
  CHECK(defaultSide.x.empty());
  SkinFrictionDistribution distribution;
  CHECK(distribution.upper.side == SurfaceSide::upper);
  CHECK(distribution.lower.side == SurfaceSide::lower);

  distribution.upper.x = {0.0, 1.0};
  distribution.upper.y = {0.0, 0.1};
  distribution.upper.s = {0.0, 1.0};
  distribution.upper.cf = {-0.1, 0.2};
  distribution.lower.x = {1.0};
  distribution.lower.y = {0.0};
  distribution.lower.s = {0.0};
  distribution.lower.cf = {0.3};

  SkinFrictionDistribution copied = distribution;
  SkinFrictionDistribution moved = std::move(distribution);
  CHECK_EQ(copied.upper.cf.size(), std::size_t{2});
  CHECK_EQ(copied.lower.cf.size(), std::size_t{1});
  CHECK_EQ(moved.upper.cf.size(), std::size_t{2});
  CHECK_EQ(moved.lower.cf.size(), std::size_t{1});
  moved.upper.cf[0] = 99.0;
  CHECK_NEAR(copied.upper.cf[0], -0.1, 0.0, 0.0);
}

TEST(unit, LibrarySkinFrictionL13NoAnalyzedPoint) {
  Session fresh;
  const Result<SkinFrictionDistribution> noPoint =
      fresh.currentSkinFriction();
  CHECK(noPoint.isErr());
  CHECK_EQ(noPoint.error().code, std::string{"skinFriction.noAnalyzedPoint"});

  CHECK(fresh.setNaca("0012").isOk());
  CHECK(fresh.generatePanel(PanelingOptions{}).isOk());
  CHECK(fresh.configure(viscousConfig()).isOk());
  const Result<SkinFrictionDistribution> noRun =
      fresh.currentSkinFriction();
  CHECK(noRun.isErr());
  CHECK_EQ(noRun.error().code, std::string{"skinFriction.noAnalyzedPoint"});
}

TEST(unit, LibrarySkinFrictionL13InviscidPointRejected) {
  Session session;
  makeReady(session, inviscidConfig());
  const Result<xfoil::OperatingPointData> point =
      session.solveAlpha(Degrees{2.0});
  CHECK(point.isOk());
  const Result<SkinFrictionDistribution> cf =
      session.currentSkinFriction();
  CHECK(cf.isErr());
  CHECK_EQ(cf.error().code, std::string{"skinFriction.notViscous"});
}

TEST(unit, LibrarySkinFrictionL13ConvergedViscousPoint) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<SkinFrictionDistribution> cf =
      solveAndProject(session, 4.0);
  CHECK(cf.isOk());
  checkSide(cf.value().upper, SurfaceSide::upper);
  checkSide(cf.value().lower, SurfaceSide::lower);
  CHECK(cf.value().upper.s.size() != cf.value().lower.s.size());
}

TEST(unit, LibrarySkinFrictionL13UnconvergedPointDeliversState) {
  Session session;
  makeReady(session, viscousConfig(1));
  const Result<xfoil::OperatingPointData> point =
      session.solveAlpha(Degrees{4.0});
  CHECK(point.isOk());
  CHECK(point.value().convergence == ConvergenceStatus::unconverged);
  const Result<SkinFrictionDistribution> cf =
      session.currentSkinFriction();
  CHECK(cf.isOk());
  checkSide(cf.value().upper, SurfaceSide::upper);
}

TEST(unit, LibrarySkinFrictionL13CancelledBeforeValidState) {
  Session session;
  makeReady(session, viscousConfig());
  CancellationToken token;
  token.requestCancellation();
  const Result<xfoil::OperatingPointData> point =
      session.solveAlpha(Degrees{4.0}, &token);
  CHECK(point.isErr());
  CHECK_EQ(point.error().code, std::string{"analysis.cancelled"});
  const Result<SkinFrictionDistribution> cf =
      session.currentSkinFriction();
  CHECK(cf.isErr());
  CHECK_EQ(cf.error().code,
           std::string{"skinFriction.noValidViscousState"});
}

TEST(unit, LibrarySkinFrictionL13CancelledAfterValidPoint) {
  Session session;
  makeReady(session, viscousConfig());
  CHECK(solveAndProject(session, 3.0).isOk());

  CancellationToken token;
  const ProgressCallback cancel = [&token](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration) {
      token.requestCancellation();
    }
  };
  const Result<xfoil::OperatingPointData> cancelled =
      session.solveAlpha(Degrees{8.0}, &token, cancel);
  CHECK(cancelled.isErr());
  CHECK_EQ(cancelled.error().code, std::string{"analysis.cancelled"});
  const Result<SkinFrictionDistribution> stale =
      session.currentSkinFriction();
  CHECK(stale.isErr());
  CHECK_EQ(stale.error().code,
           std::string{"skinFriction.noValidViscousState"});

  token.reset();
  const Result<SkinFrictionDistribution> recovered =
      solveAndProject(session, 5.0);
  CHECK(recovered.isOk());
}

TEST(unit, LibrarySkinFrictionL13CallbackFailureAndReuse) {
  Session session;
  makeReady(session, viscousConfig());
  CHECK(solveAndProject(session, 3.0).isOk());

  const ProgressCallback throwing = [](const ProgressEvent& event) {
    if (event.phase == ProgressPhase::viscousIteration) {
      throw 1;
    }
  };
  const Result<xfoil::OperatingPointData> failed =
      session.solveAlpha(Degrees{8.0}, nullptr, throwing);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code,
           std::string{"progress.callbackFailure"});
  const Result<SkinFrictionDistribution> stale =
      session.currentSkinFriction();
  CHECK(stale.isErr());
  CHECK_EQ(stale.error().code,
           std::string{"skinFriction.noValidViscousState"});

  const Result<SkinFrictionDistribution> recovered =
      solveAndProject(session, 5.0);
  CHECK(recovered.isOk());
}

TEST(unit, LibrarySkinFrictionL13OwningAfterReset) {
  Session session;
  makeReady(session, viscousConfig());
  const Result<SkinFrictionDistribution> cf =
      solveAndProject(session, 4.0);
  CHECK(cf.isOk());
  SkinFrictionDistribution owned = cf.value();
  session.reset();
  CHECK_EQ(owned.upper.cf.size(), cf.value().upper.cf.size());
  CHECK(!owned.lower.s.empty());
  const Result<SkinFrictionDistribution> afterReset =
      session.currentSkinFriction();
  CHECK(afterReset.isErr());
  CHECK_EQ(afterReset.error().code,
           std::string{"skinFriction.noAnalyzedPoint"});
}

TEST(unit, LibrarySkinFrictionL13ProjectionDoesNotMutate) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  const auto tauTop = composition.viscous.viscals.tau.top;
  const auto tauBottom = composition.viscous.viscals.tau.bottom;
  const auto ipanTop = composition.viscous.viscals.mapping.top.ipan;
  const double qinf = composition.operating.qinf;
  const auto first = xfoil::session::projectSkinFriction(composition);
  const auto second = xfoil::session::projectSkinFriction(composition);
  CHECK(first.isOk());
  CHECK(second.isOk());
  CHECK_EQ(first.value().upper.cf.size(), second.value().upper.cf.size());
  CHECK(composition.viscous.viscals.tau.top == tauTop);
  CHECK(composition.viscous.viscals.tau.bottom == tauBottom);
  CHECK(composition.viscous.viscals.mapping.top.ipan == ipanTop);
  CHECK_NEAR(composition.operating.qinf, qinf, 0.0, 0.0);
}

TEST(unit, LibrarySkinFrictionL13StructuredMappingAndNoWake) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  const auto& viscals = composition.viscous.viscals;
  const auto& panel = composition.paneling.panel;
  const auto cf = xfoil::session::projectSkinFriction(composition);
  CHECK(cf.isOk());
  CHECK_EQ(cf.value().upper.cf.size(),
           static_cast<std::size_t>(viscals.mapping.top.iblte));
  CHECK_EQ(cf.value().lower.cf.size(),
           static_cast<std::size_t>(viscals.mapping.bottom.iblte));
  CHECK(cf.value().lower.cf.size() <
        static_cast<std::size_t>(viscals.mapping.bottom.nbl));
  for (std::size_t index = 0; index < cf.value().upper.cf.size(); ++index) {
    const std::size_t station = index + 1;
    const auto panelIndex = static_cast<std::size_t>(
        viscals.mapping.top.ipan[station]);
    CHECK_NEAR(cf.value().upper.x[index], panel.x[panelIndex], 0.0, 0.0);
    CHECK_NEAR(cf.value().upper.y[index], panel.y[panelIndex], 0.0, 0.0);
    CHECK_NEAR(cf.value().upper.s[index], viscals.arcs.top[station], 0.0,
               0.0);
    CHECK_NEAR(cf.value().upper.cf[index],
               xfoil::core::export_data::skinFrictionCoefficient(
                   viscals.tau.top[station], composition.operating.qinf),
               0.0, 0.0);
  }
}

TEST(unit, LibrarySkinFrictionL13SignAndFormulaPreserved) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  composition.viscous.viscals.tau.top[1] = -0.0123;
  const auto cf = xfoil::session::projectSkinFriction(composition);
  CHECK(cf.isOk());
  CHECK_NEAR(cf.value().upper.cf[0],
             -0.0123 / (0.5 * composition.operating.qinf *
                        composition.operating.qinf),
             0.0, 0.0);
  CHECK(cf.value().upper.cf[0] < 0.0);
}

TEST(unit, LibrarySkinFrictionL13InconsistentMapping) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  composition.viscous.viscals.mapping.top.ipan[1] =
      static_cast<int>(composition.paneling.panel.x.size());
  const auto cf = xfoil::session::projectSkinFriction(composition);
  CHECK(cf.isErr());
  CHECK_EQ(cf.error().code,
           std::string{"skinFriction.inconsistentState"});
}

TEST(unit, LibrarySkinFrictionL13InconsistentArrayLengths) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  composition.viscous.viscals.tau.top.resize(2);
  const auto cf = xfoil::session::projectSkinFriction(composition);
  CHECK(cf.isErr());
  CHECK_EQ(cf.error().code,
           std::string{"skinFriction.inconsistentState"});
}

TEST(unit, LibrarySkinFrictionL13ZeroQinfRejected) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  composition.operating.qinf = 0.0;
  const auto cf = xfoil::session::projectSkinFriction(composition);
  CHECK(cf.isErr());
  CHECK_EQ(cf.error().code,
           std::string{"skinFriction.inconsistentState"});
}

TEST(unit, LibrarySkinFrictionL13NonFiniteTauRejected) {
  xfoil::session::SessionComposition composition;
  CHECK(prepareInternal(composition, 12, 160, 50, 4.0));
  composition.viscous.viscals.tau.top[1] =
      std::numeric_limits<double>::quiet_NaN();
  const auto cf = xfoil::session::projectSkinFriction(composition);
  CHECK(cf.isErr());
  CHECK_EQ(cf.error().code,
           std::string{"skinFriction.inconsistentState"});
}

TEST(unit, LibrarySkinFrictionL13SessionsRemainIndependent) {
  Session first;
  Session second;
  makeReady(first, viscousConfig());
  makeReady(second, viscousConfig());
  const Result<SkinFrictionDistribution> a = solveAndProject(first, 4.0);
  const Result<SkinFrictionDistribution> b = solveAndProject(second, 4.0);
  CHECK(a.isOk());
  CHECK(b.isOk());
  CHECK_EQ(a.value().upper.cf.size(), b.value().upper.cf.size());
  for (std::size_t index = 0; index < a.value().upper.cf.size(); ++index) {
    CHECK_NEAR(a.value().upper.cf[index], b.value().upper.cf[index], 0.0,
               0.0);
  }
}

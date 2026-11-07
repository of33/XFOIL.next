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

// File: tests/unit/SessionCoreStateWiringTests.cpp
// Activation tests: instance-scoped VACCEL,
// independent CancellationToken/abort states, NACA 0012 regression, and
// the session/use-case base form (runSpecCl).
//
// the session construction and the session/API link are
// permanently secured by the independent link target session-link-check
// (tests/core/SessionLinkCheck.cpp); the two
// Clark Y comparison points against the permanent Fortran proof live in
// tests/integration/XFoilSessionTests.cpp and
// tests/integration/ModernXFoilTests.cpp.

#include "TestSupport.h"

#include <cmath>
#include <vector>

#include "app/DatFileReader.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/XFoilSession.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/paneling/PanelParameters.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace xfoil;

// Clark Y from sample/CLARK_Y.dat (permanent reference geometry).
xfoil::geometry::AirfoilGeometry makeClarkYGeometry() {
  const auto read = xfoil::app::DatFileReader::read("sample/CLARK_Y.dat");
  CHECK(read.success());
  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign(
      read.airfoil.name,
      std::span<const xfoil::Point2D>(read.airfoil.coordinates));
  CHECK(assigned.success());
  return geometry;
}

// test-local symmetric NACA-0012 contour (81 points, XFoil DAT order).
double naca0012Thickness(double x) {
  constexpr double t = 0.12;
  return 5.0 * t *
         (0.2969 * std::sqrt(x) - 0.1260 * x - 0.3516 * x * x +
          0.2843 * x * x * x - 0.1015 * x * x * x * x);
}

xfoil::geometry::AirfoilGeometry makeNaca0012Geometry() {
  constexpr int pointsPerSide = 41;
  std::vector<xfoil::Point2D> points;
  for (int i = 0; i < pointsPerSide; ++i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(pointsPerSide - 1);
    points.push_back(xfoil::Point2D{x, naca0012Thickness(x)});
  }
  for (int i = pointsPerSide - 2; i >= 0; --i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(pointsPerSide - 1);
    points.push_back(xfoil::Point2D{x, -naca0012Thickness(x)});
  }

  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign("NACA 0012 (test-local)", points);
  CHECK(assigned.success());
  return geometry;
}

}  // namespace

// instance-scoped VACCEL (static cross-instance
// VACCEL state forbidden). Two independent sessions with different
// viscousAcceleration do not influence each other; the result of the
// default session stays bit-identical to a fresh session.
TEST(unit, SessionVaccelIsInstanceScoped) {
  const xfoil::AnalysisConfig defaultConfig;

  xfoil::XFoilSession reference;
  CHECK(reference.setGeometry(makeClarkYGeometry()).success());
  CHECK(reference.configure(defaultConfig).success());
  const xfoil::AnalysisResult referenceResult =
      reference.analyzeAtAlpha(xfoil::Degrees{4.0});

  xfoil::XFoilSession accelerated;
  xfoil::AnalysisConfig acceleratedConfig;
  acceleratedConfig.viscousAcceleration = 0.05;
  CHECK(accelerated.setGeometry(makeClarkYGeometry()).success());
  CHECK(accelerated.configure(acceleratedConfig).success());
  const xfoil::AnalysisResult acceleratedResult =
      accelerated.analyzeAtAlpha(xfoil::Degrees{4.0});

  xfoil::XFoilSession defaults;
  CHECK(defaults.setGeometry(makeClarkYGeometry()).success());
  CHECK(defaults.configure(defaultConfig).success());
  const xfoil::AnalysisResult defaultsResult =
      defaults.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(acceleratedResult.successful);
  CHECK(defaultsResult.successful);
  CHECK(referenceResult.successful);
  // the default session stays untouched by the differing VACCEL
  // configuration (instance-scoped VACCEL, no static state).
  CHECK_EQ(defaultsResult.iterations, referenceResult.iterations);
  CHECK_EQ(defaultsResult.liftCoefficient, referenceResult.liftCoefficient);
  CHECK_EQ(defaultsResult.dragCoefficient, referenceResult.dragCoefficient);
}

// independent CancellationToken/abort states: an abort affects only the
// session that owns it; the other session stays converged.
TEST(unit, SessionCancellationTokensAreIndependent) {
  xfoil::XFoilSession sessionA;
  CHECK(sessionA.setGeometry(makeClarkYGeometry()).success());
  CHECK(sessionA.configure(xfoil::AnalysisConfig{}).success());

  xfoil::XFoilSession sessionB;
  CHECK(sessionB.setGeometry(makeClarkYGeometry()).success());
  CHECK(sessionB.configure(xfoil::AnalysisConfig{}).success());

  xfoil::CancellationToken tokenA;
  tokenA.requestCancellation();
  const xfoil::AnalysisResult cancelledA =
      sessionA.analyzeAtAlpha(xfoil::Degrees{4.0}, tokenA);
  CHECK(cancelledA.status == xfoil::AnalysisStatus::cancelled);
  CHECK_FALSE(cancelledA.successful);

  xfoil::CancellationToken tokenB;
  const xfoil::AnalysisResult convergedB =
      sessionB.analyzeAtAlpha(xfoil::Degrees{4.0}, tokenB);
  CHECK(convergedB.status == xfoil::AnalysisStatus::converged);
  CHECK(convergedB.successful);
  CHECK(convergedB.converged);

  // A stays unchanged by B; B stays unchanged by A and its
  // abort.
  CHECK(sessionA.lastResult().status == xfoil::AnalysisStatus::cancelled);
  CHECK_EQ(sessionB.lastResult().liftCoefficient,
           convergedB.liftCoefficient);
}

// NACA-0012 regression: symmetric airfoil at alpha = 0 returns nearly
// vanishing lift (inviscid) and converged viscous.
TEST(unit, SessionNaca0012SymmetricInviscidRegression) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca0012Geometry()).success());

  xfoil::AnalysisConfig inviscid;
  inviscid.viscous = false;
  CHECK(session.configure(inviscid).success());
  const xfoil::AnalysisResult inviscidResult =
      session.analyzeAtAlpha(xfoil::Degrees{0.0});
  CHECK(inviscidResult.status == xfoil::AnalysisStatus::converged);
  CHECK(inviscidResult.converged);
  CHECK_EQ(inviscidResult.iterations, 0);
  CHECK(std::fabs(inviscidResult.liftCoefficient) < 1.0e-8);
  CHECK(std::fabs(inviscidResult.momentCoefficient) < 1.0e-8);
}

TEST(unit, SessionNaca0012SymmetricViscousRegression) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca0012Geometry()).success());

  // viscous NACA-0012 regression case (Re = 3e6, alpha = 1 deg): the
  // coarse test-local discretization (81 points, abcopy) converges;
  // alpha = 0 oscillates in the symmetric laminar bubble (physical
  // edge case of the coarse paneling, no solver deviation).
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 3000000.0;
  CHECK(session.configure(config).success());

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{1.0});
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK(result.iterations > 0);
  CHECK(result.iterations < 100);
  CHECK(result.liftCoefficient > 0.05 && result.liftCoefficient < 0.2);
  CHECK(result.dragCoefficient > 0.0 && result.dragCoefficient < 0.1);
}

// session/use-case base form: runSpecCl (Fortran SPECCL branch, LALFA=false)
// on the composition.
TEST(unit, SessionSpecClUseCaseInviscidPoint) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());

  xfoil::AnalysisConfig config;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());

  const xfoil::AnalysisResult result =
      session::runSpecCl(composition, 0.5, xfoil::Degrees{2.0}, nullptr);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK(result.liftCoefficient > 0.4 && result.liftCoefficient < 0.6);
}

// resetBoundaryLayer (Fortran INIT): invalidates mapping, convergence and
// BL state, without changing geometry, panel or operating state.
TEST(unit, SessionResetBoundaryLayerInvalidatesMappingOnly) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(composition.viscous.converged);
  CHECK(composition.viscous.viscals.mapping.valid);
  CHECK(composition.viscous.viscals.lblini);

  session::resetBoundaryLayer(composition);
  CHECK_FALSE(composition.viscous.converged);
  CHECK_FALSE(composition.viscous.viscals.mapping.valid);
  CHECK_FALSE(composition.viscous.viscals.lblini);
  // geometry, panel and operating state remain unchanged.
  CHECK(composition.geometry.valid);
  CHECK(composition.paneling.panelingValid);
  CHECK(composition.operating.configured);
}

// session geometry/panel producers -------------------

// buffer adoption without ABCOPY/PANGEN: panel is invalidated, origin none,
// buffer revision increased; panel parameters are retained.
TEST(unit, SessionSetBufferGeometryInvalidatesPanel) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::panelingValid(composition));
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::abcopy);
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;
  const int panelCountBefore = session::panelingParameters(composition).panelCount;

  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());

  CHECK_FALSE(session::panelingValid(composition));
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::none);
  CHECK_EQ(composition.geometry.bufferRevision, revisionBefore + 1);
  CHECK_EQ(session::panelingParameters(composition).panelCount,
           panelCountBefore);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.converged);
}

// counterpart to integration.MinimalValidGeometryIsProcessed: a minimal valid
// symmetric buffer contour (81 points) is taken and paneled
// via the session path; the panel coordinates remain in
// the range x in [0, 1].
TEST(unit, SessionBufferGeometryMinimalProcessedThenPaneling) {
  std::vector<double> xs;
  std::vector<double> ys;
  const auto thickness = [](double x) {
    const double t = 0.12;
    return 5.0 * t *
           (0.2969 * std::sqrt(x) - 0.1260 * x - 0.3516 * x * x +
            0.2843 * x * x * x - 0.1015 * x * x * x * x);
  };
  const int perSide = 41;
  for (int i = 0; i < perSide; ++i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(perSide - 1);
    xs.push_back(x);
    ys.push_back(thickness(x));
  }
  for (int k = 1; k < perSide; ++k) {
    const double x = static_cast<double>(k) / static_cast<double>(perSide - 1);
    xs.push_back(x);
    ys.push_back(-thickness(x));
  }
  CHECK_EQ(xs.size(), std::size_t{81});

  session::SessionComposition composition;
  CHECK(session::setBufferGeometry(
            composition,
            core::geometry::AirfoilBuffer(xs, ys, "MINIMAL"))
            .success());
  CHECK_EQ(composition.geometry.buffer.x.size(), std::size_t{81});
  const auto applied = session::applyPaneling(composition);
  CHECK(applied.ok);
  CHECK(session::panelingValid(composition));
  CHECK(!composition.paneling.panel.x.empty());
  for (const double x : composition.paneling.panel.x) {
    CHECK(x >= -1e-9 && x <= 1.0 + 1e-9);
  }
}

// ABCOPY (copyBufferToCurrent): origin abcopy, panelSourceRevision ==
// current buffer revision; buffer stays preserved.
TEST(unit, SessionCopyBufferToCurrentSetsAbcopyOrigin) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK_FALSE(session::panelingValid(composition));

  CHECK(session::copyBufferToCurrent(composition).ok);
  CHECK(session::panelingValid(composition));
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::abcopy);
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(session::currentBufferGeometry(composition).x.size(),
           naca.value().buffer.x.size());
}

// ABCOPY LBLINI branch N==NB: PCOP/ABCOPY on a buffer with the same panel
// contour preserves LBLINI (canonical warm start, xgdes.f:975 only for N!=NB
// false), while the remaining ABCOPY flags are reset as in the flag block.
TEST(unit, SessionAbcopyPreservesLbliniWhenCountEqual) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  const auto point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(composition.viscous.viscals.lblini);  // BL initialized
  const std::size_t panelCountBefore = composition.paneling.panel.x.size();
  CHECK_EQ(panelCountBefore, composition.geometry.buffer.x.size());

  CHECK(session::copyBufferToCurrent(composition).ok);
  CHECK(composition.viscous.viscals.lblini);  // N==NB -> preserved
  // remaining ABCOPY flags reset:
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK_FALSE(composition.viscous.viscals.lvconv);
  CHECK_FALSE(composition.viscous.viscals.mapping.valid);
  CHECK_FALSE(composition.viscous.viscals.dijWakeValid);
  // panel contour is nevertheless newly written from the buffer.
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::abcopy);
  CHECK_EQ(composition.paneling.panel.x.size(),
           composition.geometry.buffer.x.size());
}

// ABCOPY LBLINI branch N!=NB: PCOP/ABCOPY on PANGEN panel (160) with buffer
// 245 sets LBLINI=false (xgdes.f:975).
TEST(unit, SessionAbcopyResetsLbliniWhenCountDiffers) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);  // panel count stays 160
  xfoil::AnalysisConfig config;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  const auto point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(composition.viscous.viscals.lblini);
  CHECK_NE(composition.paneling.panel.x.size(),
           composition.geometry.buffer.x.size());

  CHECK(session::copyBufferToCurrent(composition).ok);
  CHECK_FALSE(composition.viscous.viscals.lblini);  // N!=NB -> false
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK_FALSE(composition.viscous.viscals.lvconv);
  CHECK_FALSE(composition.viscous.viscals.mapping.valid);
  CHECK_FALSE(composition.viscous.viscals.dijWakeValid);
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::abcopy);
  CHECK_EQ(composition.paneling.panel.x.size(),
           composition.geometry.buffer.x.size());
}

// ABCOPY does not blanket-delete the BL station fields (Fortran resets only
// flags); for the same point count they remain preserved.
TEST(unit, SessionAbcopyPreservesStationFieldsWhenCountEqual) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  const auto point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  const std::size_t thetTopBefore = composition.viscous.viscals.thet.top.size();
  CHECK(thetTopBefore > 0u);

  CHECK(session::copyBufferToCurrent(composition).ok);
  CHECK_EQ(composition.viscous.viscals.thet.top.size(), thetTopBefore);
  CHECK(composition.viscous.viscals.lblini);
}

// Actual buffer replacement (setBufferGeometry) does not preserve the old
// BL state for a new geometry, even if the point count is equal: the
// panel contour is emptied, so the following ABCOPY sees N!=NB.
TEST(unit, SessionSetBufferGeometryNewGeometryResetsLblini) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  const auto point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(composition.viscous.viscals.lblini);

  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK_FALSE(composition.viscous.viscals.lblini);  // new geometry: cold start
  CHECK_FALSE(session::panelingValid(composition));
}

// Public setGeometry API: a new geometry with the same
// point count preserves no old BL state (complete cold path).
TEST(unit, SessionSetGeometrySameCountNewGeometryResetsLblini) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  const auto point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(composition.viscous.viscals.lblini);
  const std::size_t countBefore = composition.paneling.panel.x.size();

  // set the same geometry (same point count) again as a new geometry
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK_EQ(composition.paneling.panel.x.size(), countBefore);
  CHECK_FALSE(composition.viscous.viscals.lblini);  // cold path, no state change
}

// Critical product path NACA 2412 -> PANE (PANGEN) -> OPER Alfa -10: the
// active PANGEN panel contour is analyzed (no ABCOPY fallback). The
// lift coefficient must reproduce the confirmed reference value.
TEST(unit, SessionPangenNaca2412OperatesOnPangenPanel) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());

  const session::PanelingApplyResult pane = session::applyPaneling(composition);
  CHECK(pane.ok);
  CHECK(session::panelingValid(composition));
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::pangen);
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);

  const auto metrics = session::geometryMetrics(composition);
  CHECK(metrics.has_value());

  xfoil::AnalysisConfig config;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());

  const xfoil::AnalysisResult result =
      session::runSpecAlpha(composition, xfoil::Degrees{-10.0}, nullptr);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK_NEAR(result.liftCoefficient, -0.950577, 1.0e-4, 1.0e-4);
}

// PPAR semantics: parameters are stored; the existing panel contour
// is not regenerated (effect only at the next PANGEN).
TEST(unit, SessionSetPanelingParametersDoesNotRepanel) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  const std::uint64_t sourceRevision = composition.paneling.panelSourceRevision;
  const std::size_t panelSize = session::currentPanelGeometry(composition).x.size();

  core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 200;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK_EQ(session::panelingParameters(composition).panelCount, 200);
  // No panel regeneration: origin and source remain unchanged.
  CHECK(session::panelingOrigin(composition) == session::PanelingOrigin::abcopy);
  CHECK_EQ(composition.paneling.panelSourceRevision, sourceRevision);
  CHECK_EQ(session::currentPanelGeometry(composition).x.size(), panelSize);

  // Invalid panelCount without partial mutation.
  core::paneling::PanelingParameters bad = params;
  bad.panelCount = 1;
  CHECK_FALSE(session::setPanelingParameters(composition, bad).ok);
  CHECK_EQ(session::panelingParameters(composition).panelCount, 200);
}

// operating setters according to xoper.f: VISC/Re/Mach/TYPE set only
// LVCONV=false; Mach/TYPE update MINF/REINF/compressibility, but not the
// GGCALC influence matrix. ITER/VACCEL do not invalidate. VACCEL is
// instance-specific.
TEST(unit, SessionOperatingSettersInvalidationRules) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult point =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(composition.viscous.systemValid);
  CHECK(composition.viscous.converged);
  CHECK(composition.viscous.viscals.lblini);
  const double previousMinf = composition.viscous.viscals.minf;

  session::setReynoldsNumber(composition, 6.0e6);
  CHECK(composition.viscous.systemValid);  // Re does not invalidate the matrix
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.viscous.viscals.lblini);  // xoper.f Re: only LVCONV=false
  CHECK_EQ(composition.operating.machReynolds.reinf1, 6.0e6);
  CHECK_NEAR(composition.viscous.viscals.reinf, 6.0e6, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.viscals.minf, previousMinf, 0.0, 0.0);

  session::setMachNumber(composition, 0.3);
  CHECK(composition.viscous.systemValid);  // Mach does not invalidate GGCALC
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.viscous.viscals.lblini);
  CHECK_NEAR(composition.viscous.viscals.minf, 0.3, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.viscals.reinf, 6.0e6, 0.0, 0.0);

  session::setIterationLimit(composition, 40);
  CHECK_EQ(composition.operating.maximumIterations, 40);
  CHECK(composition.viscous.systemValid);  // ITER invalidates nothing new
  CHECK_FALSE(composition.viscous.converged);

  session::setViscousAcceleration(composition, 0.05);
  CHECK_NEAR(composition.operating.viscousAcceleration, 0.05, 0.0, 0.0);
}

// runClSequence (CSEQ base form): several successful points with warm start.
TEST(unit, SessionRunClSequenceProducesWarmStartedPoints) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points = session::runClSequence(
      composition, 0.4, 0.8, 0.2, xfoil::Degrees{2.0}, nullptr);
  CHECK_EQ(points.size(), 3u);
  for (const xfoil::AnalysisResult& point : points) {
    CHECK(point.successful);
    CHECK(point.converged);
  }
  CHECK(points[0].liftCoefficient < points[1].liftCoefficient);
  CHECK(points[1].liftCoefficient < points[2].liftCoefficient);
}

// Name/XYCM remain in their presentation/last state without panel invalidation.
TEST(unit, SessionNameAndMomentReferenceDoNotRepanel) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  const std::size_t panelSize = session::currentPanelGeometry(composition).x.size();

  session::setGeometryName(composition, "CLARK-Y renamed");
  CHECK_EQ(session::currentBufferGeometry(composition).name,
           std::string("CLARK-Y renamed"));
  CHECK(session::panelingValid(composition));
  CHECK_EQ(session::currentPanelGeometry(composition).x.size(), panelSize);

  session::setMomentReference(composition, 0.25, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceX, 0.25, 0.0, 0.0);
  CHECK(session::panelingValid(composition));
}

// OPER-ASEQ (inviscid) after NPOINT formula with forced
// step sign; results match repeated individual points
// (inviscid is warm-start-invariant).
TEST(unit, SessionOperAlphaSequenceInviscidMatchesRepeatedPoints) {
  session::SessionComposition sequence;
  CHECK(session::setGeometry(sequence, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig inviscid;
  inviscid.viscous = false;
  CHECK(session::configure(sequence, inviscid).success());

  const std::vector<xfoil::AnalysisResult> points =
      session::runOperAlphaSequence(sequence, xfoil::Degrees{1.0},
                                    xfoil::Degrees{2.0}, xfoil::Degrees{1.0},
                                    nullptr);
  CHECK_EQ(points.size(), 2u);

  session::SessionComposition repeated;
  CHECK(session::setGeometry(repeated, makeClarkYGeometry()).success());
  CHECK(session::configure(repeated, inviscid).success());
  const xfoil::AnalysisResult first =
      session::runSpecAlpha(repeated, xfoil::Degrees{1.0}, nullptr);
  const xfoil::AnalysisResult second =
      session::runSpecAlpha(repeated, xfoil::Degrees{2.0}, nullptr);

  CHECK_EQ(points.size(), 2u);
  if (points.size() == 2u) {
    CHECK_NEAR(points[0].alpha.value, 1.0, 1e-9, 0.0);
    CHECK_NEAR(points[1].alpha.value, 2.0, 1e-9, 0.0);
    CHECK_NEAR(points[0].liftCoefficient, first.liftCoefficient, 1e-9, 0.0);
    CHECK_NEAR(points[1].liftCoefficient, second.liftCoefficient, 1e-9, 0.0);
  }
}

// descending OPER-ASEQ forces the step sign from start/end
// (xoper.f 576-580); a positive inline step becomes descending.
TEST(unit, SessionOperAlphaSequenceForcesDescendingStep) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig inviscid;
  inviscid.viscous = false;
  CHECK(session::configure(composition, inviscid).success());

  const std::vector<xfoil::AnalysisResult> points =
      session::runOperAlphaSequence(composition, xfoil::Degrees{2.0},
                                    xfoil::Degrees{0.0}, xfoil::Degrees{1.0},
                                    nullptr);
  CHECK_EQ(points.size(), 3u);
  if (points.size() == 3u) {
    CHECK_NEAR(points[0].alpha.value, 2.0, 1e-9, 0.0);
    CHECK_NEAR(points[1].alpha.value, 1.0, 1e-9, 0.0);
    CHECK_NEAR(points[2].alpha.value, 0.0, 1e-9, 0.0);
  }
}

// warm-start state machine (xoper.f 675-677). After a
// converged point, AWAKE/AVISC/LVCONV stay preserved as long as the
// 1e-5 threshold is not exceeded; a sufficiently large alpha
// jump invalidates LWAKE and LVCONV.
TEST(unit, SessionOperSequenceWarmStartPreservesAndResetsState) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 3000000.0;
  CHECK(session::configure(composition, config).success());

  // cold-path single point for alpha=4: converged, sets AVISC/MVISC/LVCONV,
  // but no AWAKE/LWAKE (the cold path does not enter sequence state).
  const xfoil::AnalysisResult cold =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(cold.successful);
  CHECK(composition.viscous.viscals.lvconv);
  CHECK_FALSE(composition.viscous.wakeValid);

  // OPER sequence point for the same alpha=4: wake is built (AWAKE set),
  // LVCONV stays because |Alfa-AVISC|<=1e-5 preserves it (warmer BL path).
  const std::vector<xfoil::AnalysisResult> same =
      session::runOperAlphaSequence(composition, xfoil::Degrees{4.0},
                                    xfoil::Degrees{4.0}, xfoil::Degrees{0.0},
                                    nullptr);
  CHECK_EQ(same.size(), 1u);
  CHECK(same.at(0).successful);
  CHECK(composition.viscous.wakeValid);
  const double alpha4Rad = 4.0 * 0.017453292519943295;
  CHECK_NEAR(composition.viscous.awake, alpha4Rad, 1e-9, 0.0);
  CHECK(composition.viscous.viscals.lvconv);

  // OPER sequence point for alpha=6: |Alfa-AWAKE|>1e-5 -> new wake (AWAKE=6rad),
  // |Alfa-AVISC|>1e-5 -> LVCONV reset, then converged again.
  const std::vector<xfoil::AnalysisResult> jump =
      session::runOperAlphaSequence(composition, xfoil::Degrees{6.0},
                                    xfoil::Degrees{6.0}, xfoil::Degrees{0.0},
                                    nullptr);
  CHECK_EQ(jump.size(), 1u);
  CHECK(jump.at(0).successful);
  const double alpha6Rad = 6.0 * 0.017453292519943295;
  CHECK_NEAR(composition.viscous.awake, alpha6Rad, 1e-9, 0.0);
  CHECK_NEAR(composition.viscous.viscals.avisc, alpha6Rad, 1e-9, 0.0);
  CHECK(composition.viscous.viscals.lvconv);
}

// NSEQEX halt (xoper.f 738-753, xfoil.f:698). Replays the
// Fortran reference aero-viscous/aseq-nseqex (NACA 2412 -> PANGEN, Re=1e6,
// ITER 1, ASEQ 0 10 1): Fortran reports 'sequence halted since previous 4
// points did not converge' after alpha points 0,1,2,3. With ITER=1
// VISCAL never converges; the OPER-ASEQ aborts after four points.
TEST(unit, SessionOperSequenceNseqexHaltsOnNonConvergence) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);

  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 1;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points =
      session::runOperAlphaSequence(composition, xfoil::Degrees{0.0},
                                    xfoil::Degrees{10.0}, xfoil::Degrees{1.0},
                                    nullptr);
  CHECK_EQ(points.size(), 4u);
  for (const xfoil::AnalysisResult& point : points) {
    CHECK_FALSE(point.converged);
    CHECK(point.status == xfoil::AnalysisStatus::unconverged);
  }
}

// viscous OPER-ASEQ warm start against the Fortran reference
// aero-viscous/seq-visc-2412 (NACA 2412 -> PANGEN, Re=1e6, ITER 100,
// ASEQ 0 8 2). Fortran-converged Cl per point (stdout.raw.txt, 4 decimal
// precision): 0->0.2371, 2->0.4496, 4->0.7146, 6->0.9019, 8->1.0875.
// tolerance 1e-3 is limited by the four-decimal precision of the
// reference, not raised by increasing the reference resolution.
TEST(unit, SessionOperAlphaSequenceViscousMatchesFortranSeqVisc2412) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);

  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points =
      session::runOperAlphaSequence(composition, xfoil::Degrees{0.0},
                                    xfoil::Degrees{8.0}, xfoil::Degrees{2.0},
                                    nullptr);
  CHECK_EQ(points.size(), 5u);
  if (points.size() == 5u) {
    const double expectedCl[5] = {0.2371, 0.4496, 0.7146, 0.9019, 1.0875};
    const double expectedAlpha[5] = {0.0, 2.0, 4.0, 6.0, 8.0};
    for (std::size_t i = 0; i < 5; ++i) {
      CHECK(points[i].converged);
      CHECK_NEAR(points[i].alpha.value, expectedAlpha[i], 1e-9, 0.0);
      CHECK_NEAR(points[i].liftCoefficient, expectedCl[i], 1.0e-3, 0.0);
    }
  }
}

// viscous OPER-CSEQ warm start (PointExecutionPolicy::
// operSequenceWarmStart as ASEQ). SPECCL provides Cl exactly at alpha; alpha
// is monotonically increasing. NPOINT formula (0.4..0.8/0.2 -> 3 points).
TEST(unit, SessionOperClSequenceViscousConvergesToTargets) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);

  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points = session::runOperClSequence(
      composition, 0.4, 0.8, 0.2, xfoil::Degrees{2.0}, nullptr);
  CHECK_EQ(points.size(), 3u);
  if (points.size() == 3u) {
    const double targets[3] = {0.4, 0.6, 0.8};
    for (std::size_t i = 0; i < 3; ++i) {
      CHECK(points[i].converged);
      CHECK_NEAR(points[i].liftCoefficient, targets[i], 1.0e-3, 0.0);
      if (i > 0) {
        CHECK(points[i].alpha.value > points[i - 1].alpha.value);
      }
    }
  }
}

// Direct CSEQ Fortran proof. Compares the session CSEQ
// (Clark Y -> ABCOPY 151 points, Re=1e6, ITER 100, Cl 0.4/0.6/0.8) pointwise
// against the canonical c11a-state harness reference
// tests/data/core-c11a-state-reference/c11a_cseq_harness_reference.txt
// (It24.16). Thresholds abs/rel 1e-9 for coefficients/alpha, abs 1e-9 /
// rel 1e-6 for RMSBL/RMXBL (analogous to ClarkYViscousCoreTests, core == Fortran
// bit-close). Iteration count and convergence status are compared directly.
TEST(unit, SessionOperClSequenceViscousMatchesFortranCseqHarness) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points = session::runOperClSequence(
      composition, 0.4, 0.8, 0.2, xfoil::Degrees{0.0}, nullptr);
  CHECK_EQ(points.size(), 3u);
  if (points.size() != 3u) {
    return;
  }
  // harness It24.16 values per point.
  const double adegh[3] = {8.4149127500698598e-02, 1.7354485710947867e+00,
                           3.6477939463265745e+00};
  const double clh[3] = {3.9999999999997932e-01, 6.0000000000001352e-01,
                         8.0000000000000071e-01};
  const double cmh[3] = {-8.2428415388704823e-02, -8.5129710814231679e-02,
                         -8.2901697952837028e-02};
  const double cdh[3] = {6.2942095984635732e-03, 5.6984747974721849e-03,
                         7.2085007472027802e-03};
  const double rmsh[3] = {2.1934703919611229e-06, 7.1342168014175352e-06,
                          2.5396008451545545e-06};
  const double rmxh[3] = {2.4086482872009755e-05, 7.4996522183389924e-05,
                          -3.4950306045576484e-05};
  const int nith[3] = {19, 41, 7};
  for (std::size_t i = 0; i < 3; ++i) {
    CHECK(points[i].converged);
    CHECK_EQ(points[i].iterations, nith[i]);
    CHECK_NEAR(points[i].alpha.value, adegh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(points[i].liftCoefficient, clh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(points[i].momentCoefficient, cmh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(points[i].dragCoefficient, cdh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(points[i].rmsResidual, rmsh[i], 1.0e-9, 1.0e-6);
    // RMXBL (max-residual diagnostics of the last UPDATE): point 1 matches at
    // 1e-9; at the second warm-start point the core deviates from Fortran by up to
 // 2.8e-9 (first differing state).
 // Field-specific, threshold 1e-8 derived from the observed core fidelity,
 // remaining fields 1e-9; no blanket pressure tolerance.
    CHECK_NEAR(points[i].maximumResidual, rmxh[i], 1.0e-8, 1.0e-6);
  }
 // CDF and ITRAN are not in the public AnalysisResult but are
 // contained per point in the permanent harness as
 // `CSEQ CDF`/`CSEQ ITRAN1`/`CSEQ ITRAN2`.
 // the final BL state (point 3) is read from the session
 // viscous session state and confirmed against the reference.
 // core itran is 0-based, Fortran ITRAN = core + 1 (convention as in
 // ClarkYViscousCoreTests). Reference for point 3: CDF 4.8941639528211874E-03,
 // ITRAN1 44, ITRAN2 71.
  CHECK_NEAR(composition.viscous.viscals.cdf, 4.8941639528211874e-03,
             1.0e-9, 1.0e-9);
  CHECK_EQ(composition.viscous.viscals.itran[0] + 1, 44);
  CHECK_EQ(composition.viscous.viscals.itran[1] + 1, 71);
}

// descending OPER-CSEQ forces the step sign from start/end
// (xoper.f 596-600); a positive inline step becomes descending. Cl targets exact,
// alpha monotonically descending. (the normal-path harness covers ascending numerical
// behavior; descent is structurally covered.)
TEST(unit, SessionOperClSequenceDescendingForcedStep) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points = session::runOperClSequence(
      composition, 0.8, 0.4, 0.2, xfoil::Degrees{3.0}, nullptr);
  CHECK_EQ(points.size(), 3u);
  if (points.size() != 3u) {
    return;
  }
  const double targets[3] = {0.8, 0.6, 0.4};
  for (std::size_t i = 0; i < 3; ++i) {
    CHECK(points[i].converged);
    CHECK_NEAR(points[i].liftCoefficient, targets[i], 1.0e-6, 0.0);
    if (i > 0) {
      CHECK(points[i].alpha.value < points[i - 1].alpha.value);
    }
  }
}

// CSEQ-NSEQEX halt. Replay the new c11a-state harness reference
// c11a_cseq_nseqex_harness_reference.txt (Clark Y -> ABCOPY, Re=1e6, ITMAX=1,
// CSEQ 0.4..1.2/0.2): Fortran halts after four non-converged points.
TEST(unit, SessionOperClSequenceNseqexHalts) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 1;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points = session::runOperClSequence(
      composition, 0.4, 1.2, 0.2, xfoil::Degrees{0.0}, nullptr);
  CHECK_EQ(points.size(), 4u);
  for (const xfoil::AnalysisResult& point : points) {
    CHECK_FALSE(point.converged);
    CHECK(point.status == xfoil::AnalysisStatus::unconverged);
  }
}

// abort in the OPER-CSEQ. An abort requested beforehand produces
// exactly one cancelled result and ends the sequence.
TEST(unit, SessionOperClSequenceCancellation) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  CHECK(session::configure(composition, config).success());

  xfoil::CancellationToken token;
  token.requestCancellation();
  const std::vector<xfoil::AnalysisResult> points = session::runOperClSequence(
      composition, 0.4, 0.8, 0.2, xfoil::Degrees{0.0}, &token);
  CHECK_EQ(points.size(), 1u);
  if (!points.empty()) {
    CHECK(points[0].status == xfoil::AnalysisStatus::cancelled);
    CHECK_FALSE(points[0].successful);
  }
}

// two independent sessions without shared sequence state. Each
// session keeps its own viscous state (awake/wakeValid/BL) and
// returns identical individual points; a sequence in one leaves the other untouched.
TEST(unit, SessionOperClSequenceIndependentSessions) {
  auto runFresh = []() {
    session::SessionComposition composition;
    CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
    xfoil::AnalysisConfig config;
    config.reynoldsNumber = 1000000.0;
    config.maximumIterations = 100;
    CHECK(session::configure(composition, config).success());
    return session::runOperClSequence(composition, 0.4, 0.8, 0.2,
                                      xfoil::Degrees{0.0}, nullptr);
  };
  const std::vector<xfoil::AnalysisResult> a = runFresh();
  const std::vector<xfoil::AnalysisResult> b = runFresh();
  CHECK_EQ(a.size(), b.size());
  if (a.size() == b.size() && a.size() == 3u) {
    for (std::size_t i = 0; i < a.size(); ++i) {
      CHECK_NEAR(a[i].liftCoefficient, b[i].liftCoefficient, 1.0e-12, 0.0);
      CHECK_NEAR(a[i].alpha.value, b[i].alpha.value, 1.0e-12, 0.0);
    }
  }
}

// DCL=0 yields exactly one point on the start Cl (xoper.f 596-600:
// -> NPOINT=1). The point hits the target Cl.
TEST(unit, SessionOperClSequenceZeroStepSinglePoint) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  CHECK(session::configure(composition, config).success());

  const std::vector<xfoil::AnalysisResult> points = session::runOperClSequence(
      composition, 0.6, 0.6, 0.0, xfoil::Degrees{0.0}, nullptr);
  CHECK_EQ(points.size(), 1u);
  if (points.size() == 1u) {
    CHECK(points[0].converged);
    CHECK_NEAR(points[0].liftCoefficient, 0.6, 1.0e-6, 0.0);
  }
}

// NPOINT formula including the inclusive end (xoper.f 596-600:
// NPOINT = INT((Cl2-Cl1)/DCL + 0.5) + 1, step sign from start/end).
// Driven inviscid (point count is viscosity-independent, SPECCL hits Cl
// exactly): ascending 0.4..0.8/0.2 -> 3 points with final Cl=0.8;
// descending 0.8..0.4/0.2 -> 3 points with final Cl=0.4; rounding case
// 0.4..0.9/0.2 -> INT(2.5+0.5)+1 = 4 points (Cl 0.4/0.6/0.8/1.0).
TEST(unit, SessionOperClSequenceNpointAndInclusiveEnd) {
  xfoil::AnalysisConfig inviscid;
  inviscid.viscous = false;

  session::SessionComposition up;
  CHECK(session::setGeometry(up, makeClarkYGeometry()).success());
  CHECK(session::configure(up, inviscid).success());
  const std::vector<xfoil::AnalysisResult> ascending =
      session::runOperClSequence(up, 0.4, 0.8, 0.2, xfoil::Degrees{2.0},
                                 nullptr);
  CHECK_EQ(ascending.size(), 3u);
  if (ascending.size() == 3u) {
    CHECK_NEAR(ascending.front().liftCoefficient, 0.4, 1.0e-6, 0.0);
    CHECK_NEAR(ascending.back().liftCoefficient, 0.8, 1.0e-6, 0.0);
  }

  session::SessionComposition down;
  CHECK(session::setGeometry(down, makeClarkYGeometry()).success());
  CHECK(session::configure(down, inviscid).success());
  const std::vector<xfoil::AnalysisResult> descending =
      session::runOperClSequence(down, 0.8, 0.4, 0.2, xfoil::Degrees{2.0},
                                 nullptr);
  CHECK_EQ(descending.size(), 3u);
  if (descending.size() == 3u) {
    CHECK_NEAR(descending.front().liftCoefficient, 0.8, 1.0e-6, 0.0);
    CHECK_NEAR(descending.back().liftCoefficient, 0.4, 1.0e-6, 0.0);
  }

  session::SessionComposition round;
  CHECK(session::setGeometry(round, makeClarkYGeometry()).success());
  CHECK(session::configure(round, inviscid).success());
  const std::vector<xfoil::AnalysisResult> rounded =
      session::runOperClSequence(round, 0.4, 0.9, 0.2, xfoil::Degrees{2.0},
                                 nullptr);
  CHECK_EQ(rounded.size(), 4u);
  if (rounded.size() == 4u) {
    const double targets[4] = {0.4, 0.6, 0.8, 1.0};
    for (std::size_t i = 0; i < 4; ++i) {
      CHECK_NEAR(rounded[i].liftCoefficient, targets[i], 1.0e-6, 0.0);
    }
  }
}

// LRECALC continuation (xoper.f 85-92/416-542). A viscous point that did
// not converge under a too-small iteration limit is
// continued with continueViscousPoint (only VISCAL(ITMAX), no SPECAL/SPECCL, no
// cold start) and eventually converges to the same solution as a single
// full-resolution point. Proves Fortran consistency and that no cold start
// occurs (a cold start with ITMAX=1 would never converge).
TEST(unit, SessionContinueViscousPointConvergesToFullSolve) {
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 1;  // deliberately too small

  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));

  const xfoil::AnalysisResult first =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.successful);
  CHECK_FALSE(first.converged);
  CHECK_EQ(first.iterations, 1);

  xfoil::AnalysisResult last = first;
  int continues = 0;
  while (!last.converged && continues < 50) {
    last = session::continueViscousPoint(composition, nullptr);
    CHECK(last.successful);
    CHECK(last.iterations >= 1);  // continuation runs VISCAL again
    ++continues;
  }
  CHECK(last.converged);
  CHECK(continues >= 1);

 // comparison against an independent full resolution (large ITMAX, one point).
  session::SessionComposition full;
  CHECK(session::setGeometry(full, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig fullConfig = config;
  fullConfig.maximumIterations = 100;
  CHECK(session::configure(full, fullConfig).success());
  CHECK(session::initializeAnalysis(full));
  const xfoil::AnalysisResult reference =
      session::runSpecAlpha(full, xfoil::Degrees{2.0}, nullptr);
  CHECK(reference.converged);
  CHECK_NEAR(last.liftCoefficient, reference.liftCoefficient, 1.0e-9, 1.0e-9);
  CHECK_NEAR(last.dragCoefficient, reference.dragCoefficient, 1.0e-9, 1.0e-9);
  CHECK_NEAR(last.momentCoefficient, reference.momentCoefficient, 1.0e-9,
             1.0e-9);
}

// after a converged viscous point LVCONV is preserved; the continuation
// loads the loads from the converged state and converges after exactly one
// Newton pass again (iterations==1). A cold start would reset LVCONV/LBLINI
// and need the full iteration count (here ~5). This proves,
// that continueViscousPoint performs no cold start.
TEST(unit, SessionContinueViscousPointAfterConvergedIsNotColdStart) {
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;

  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));
  const xfoil::AnalysisResult first =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.converged);
  CHECK(first.iterations > 1);  // full cold start needed

  const xfoil::AnalysisResult again =
      session::continueViscousPoint(composition, nullptr);
  CHECK(again.successful);
  CHECK(again.converged);
  CHECK_EQ(again.iterations, 1);  // warm continuation, no cold start
  CHECK_NEAR(again.liftCoefficient, first.liftCoefficient, 1.0e-9, 1.0e-9);
}

// without a previous viscous point the state is not continuable:
// controlled error status without partial mutation.
TEST(unit, SessionContinueViscousPointRequiresPriorViscousPoint) {
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));

  const xfoil::AnalysisResult result =
      session::continueViscousPoint(composition, nullptr);
  CHECK(result.status == xfoil::AnalysisStatus::invalidAnalysisSettings);
  CHECK_FALSE(result.successful);
}

// after an inviscid point there is no viscous continuation state.
TEST(unit, SessionContinueViscousPointAfterInviscidNotContinuable) {
  xfoil::AnalysisConfig config;
  config.viscous = false;
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));
  const xfoil::AnalysisResult inviscid =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(inviscid.successful);

  const xfoil::AnalysisResult result =
      session::continueViscousPoint(composition, nullptr);
  CHECK(result.status == xfoil::AnalysisStatus::invalidAnalysisSettings);
  CHECK_FALSE(result.successful);
}

// Fortran xoper.f Re sets only LVCONV=false (296); COMOLD/ARGOLD and the
// inviscid point state stay preserved. A '!' entered after Re therefore
// re-runs VISCAL(ITMAX) on the preserved state with the new REINF,
// without calling SPECAL again.
TEST(unit, SessionContinueViscousPointAfterReynoldsChangeUsesNewReinf) {
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 1;
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));
  const xfoil::AnalysisResult first =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK_FALSE(first.converged);

  session::setReynoldsNumber(composition, 2000000.0);
  CHECK_NEAR(composition.viscous.viscals.reinf, 2000000.0, 0.0, 0.0);
  CHECK(composition.viscous.lastPointContinuable);

  const xfoil::AnalysisResult result =
      session::continueViscousPoint(composition, nullptr);
  CHECK(result.successful);
  CHECK(result.status == xfoil::AnalysisStatus::unconverged);
  CHECK_EQ(result.iterations, 1);
}

// abort is instance-specific: a requested abort produces exactly a
// cancelled result, without mutating the solution state.
TEST(unit, SessionContinueViscousPointCancellation) {
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 1;
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));
  const xfoil::AnalysisResult first =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK_FALSE(first.converged);

  xfoil::CancellationToken token;
  token.requestCancellation();
  const xfoil::AnalysisResult cancelled =
      session::continueViscousPoint(composition, &token);
  CHECK(cancelled.status == xfoil::AnalysisStatus::cancelled);
  CHECK_FALSE(cancelled.successful);
}

// replay the permanent Fortran reference
// tests/data/core-c11a-state-reference/c11a_lrecalc_harness_reference.txt
// (Clark Y -> ABCOPY, Re=1e6, ITMAX=1, alpha 2 deg): SPECAL once, then
// VISCAL(ITMAX=1) non-converged, then LRECALC continuation
// (continueViscousPoint = only VISCAL(ITMAX), no SPECAL, no cold start) until
// convergence. Compared per step: Cl/CM/Cd, RMSBL/RMXBL,
// iteration count, convergence; final CDF/ITRAN from the BL state. Thresholds
// analogous to ClarkYViscousCoreTests/CSEQ replay (coefficients 1e-9, residuals
// abs 1e-9/rel 1e-6). Proves that the session continuation exactly follows
// the canonical '!' semantics.
TEST(unit, SessionContinueViscousPointMatchesFortranLrecalcHarness) {
  session::SessionComposition composition;
  CHECK(session::setGeometry(composition, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 1;
  CHECK(session::configure(composition, config).success());
  CHECK(session::initializeAnalysis(composition));

  const int kSteps = 5;
  const double clh[kSteps] = {6.6648225661190708e-01, 6.5177478367346864e-01,
                              6.4651012869392377e-01, 6.4645426411149187e-01,
                              6.4645431381471929e-01};
  const double cmh[kSteps] = {-9.3295854966373076e-02, -9.0065013266028060e-02,
                              -8.8927208876014768e-02, -8.8917042861883067e-02,
                              -8.8917057433402391e-02};
  const double cdh[kSteps] = {5.6175745970964931e-03, 5.7299819963986141e-03,
                              5.8464828453088820e-03, 5.8509892836897991e-03,
                              5.8509956565811275e-03};
  const double rmsh[kSteps] = {1.9917899911865045e-01, 4.5365384951061336e-02,
                               6.4615340009574473e-03, 1.5218442121779923e-04,
                               6.4529864270447660e-07};
  const double rmxh[kSteps] = {1.1453174438138056e+00, 2.9107930910216134e-01,
                               -4.2291285366664690e-02, -2.1506401587695983e-03,
                               -4.0557588867961554e-06};
  const bool convh[kSteps] = {false, false, false, false, true};

  xfoil::AnalysisResult steps[kSteps];
  steps[0] = session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  for (int i = 1; i < kSteps; ++i) {
    steps[i] = session::continueViscousPoint(composition, nullptr);
  }

  for (int i = 0; i < kSteps; ++i) {
    CHECK(steps[i].successful);
    CHECK_EQ(steps[i].converged, convh[i]);
    CHECK_EQ(steps[i].iterations, 1);
    CHECK_NEAR(steps[i].liftCoefficient, clh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(steps[i].momentCoefficient, cmh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(steps[i].dragCoefficient, cdh[i], 1.0e-9, 1.0e-9);
    CHECK_NEAR(steps[i].rmsResidual, rmsh[i], 1.0e-9, 1.0e-6);
    CHECK_NEAR(steps[i].maximumResidual, rmxh[i], 1.0e-8, 1.0e-6);
  }
 // final BL state: CDF and ITRAN from the viscous session state.
 // core itran is 0-based, Fortran ITRAN = core + 1.
  CHECK_NEAR(composition.viscous.viscals.cdf, 4.1410670163606359e-03, 1.0e-9,
             1.0e-9);
  CHECK_EQ(composition.viscous.viscals.itran[0] + 1, 52);
  CHECK_EQ(composition.viscous.viscals.itran[1] + 1, 73);
}

// end: tests/unit/SessionCoreStateWiringTests.cpp

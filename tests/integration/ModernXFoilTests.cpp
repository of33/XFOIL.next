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

// File: tests/integration/ModernXFoilTests.cpp
// integration tests for the facade class xfoil::XFoil (
// : the facade delegates internally to the
// session/use-case layer and the Fortran-verified core path).
// the same safe geometry helpers as the session tests
// (tests/integration/XFoilSessionTests.cpp). The CLARK-Y reference point is the permanent
// Fortran proof (tests/core/ClarkYViscousCoreTests.cpp, c11a_harness.f
// with the canonical build flags -fdefault-real-8 -fdefault-double-8):
// alpha = 4 -> 12 iterations, Cl 8.2243225894774308E-01,
// CM -8.2790137654465551E-02, CD 1.7288688307967329E-02. Thresholds derived
// directly from the Fortran confirmation (abs/rel 1e-9).

#include "TestSupport.h"

#include "app/DatFileReader.h"
#include "xfoil/Limits.h"
#include "xfoil/XFoil.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

// permanent Fortran confirmation (c11a_harness.f, VISCAL(100) FINAL block).
constexpr double kFortranClAbs = 1e-9;
constexpr double kFortranClRel = 1e-9;
constexpr double kFortranCdAbs = 1e-9;
constexpr double kFortranCdRel = 1e-9;

double naca2412Thickness(double x) {
  constexpr double t = 0.12;
  return 5.0 * t *
         (0.2969 * std::sqrt(x) - 0.1260 * x - 0.3516 * x * x +
          0.2843 * x * x * x - 0.1015 * x * x * x * x);
}

double naca2412Camber(double x) {
  constexpr double m = 0.02;
  constexpr double p = 0.4;
  if (x < p) {
    return m / (p * p) * (2.0 * p * x - x * x);
  }
  return m / ((1.0 - p) * (1.0 - p)) *
         (1.0 - 2.0 * p + 2.0 * p * x - x * x);
}

// Test-local NACA 2412 contour in XFoil DAT order, 81 points
// (identical to the session tests).
xfoil::geometry::AirfoilGeometry makeNaca2412Geometry() {
  constexpr int pointsPerSide = 41;
  std::vector<xfoil::Point2D> points;
  for (int i = 0; i < pointsPerSide; ++i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(pointsPerSide - 1);
    points.push_back(xfoil::Point2D{x, naca2412Camber(x) + naca2412Thickness(x)});
  }
  for (int i = pointsPerSide - 2; i >= 0; --i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(pointsPerSide - 1);
    points.push_back(xfoil::Point2D{x, naca2412Camber(x) - naca2412Thickness(x)});
  }

  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign("NACA 2412 (test-local)", points);
  CHECK(assigned.success());
  return geometry;
}

// CLARK-Y from sample/CLARK_Y.dat (identical to the session tests).
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

xfoil::AnalysisConfig makeInviscidConfig() {
  xfoil::AnalysisConfig config;
  config.viscous = false;
  return config;
}

}  // namespace

TEST(integration, ModernXFoilAcceptsGeometry) {
  xfoil::XFoil foil;
  const auto geometrySet = foil.setGeometry(makeNaca2412Geometry());
  CHECK(geometrySet.success());
  CHECK(foil.hasGeometry());
  CHECK_EQ(foil.geometry().pointCount(), std::size_t{81});
  CHECK(!foil.geometry().name().empty());
}

TEST(integration, ModernXFoilAcceptsConfiguration) {
  xfoil::XFoil foil;
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 200000.0;
  const auto configured = foil.configure(config);
  CHECK(configured.success());
  CHECK(foil.hasConfiguration());
  CHECK_EQ(foil.config().reynoldsNumber, 200000.0);
}

TEST(integration, ModernXFoilAnalyzesInviscidPoint) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(makeInviscidConfig()).success());

  const xfoil::AnalysisResult result =
      foil.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK_EQ(result.iterations, 0);
  CHECK(result.liftCoefficient > 0.5);
  CHECK(result.momentCoefficient < 0.0);
  CHECK(std::isfinite(result.centerOfPressureX));
}

TEST(integration, ModernXFoilAnalyzesViscousPoint) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult result =
      foil.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK(result.iterations > 0);
  CHECK(result.iterations < 100);
  CHECK(result.liftCoefficient > 0.5 && result.liftCoefficient < 1.0);
  CHECK(result.dragCoefficient > 0.0 && result.dragCoefficient < 0.1);
  CHECK(result.momentCoefficient < 0.0);
}

TEST(integration, ModernXFoilCalculatesPositivePolar) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = foil.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{2.0}, xfoil::Degrees{0.5});

  CHECK_EQ(polar.size(), std::size_t{4});
  for (std::size_t i = 0; i < polar.size(); ++i) {
    CHECK(polar[i].status == xfoil::AnalysisStatus::converged);
    CHECK(polar[i].successful);
    CHECK_NEAR(
        polar[i].alpha.value, static_cast<double>(i) * 0.5, 1e-12, 0.0);
  }
}

TEST(integration, ModernXFoilCalculatesNegativePolar) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = foil.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{-1.0}, xfoil::Degrees{-0.5});

  CHECK_EQ(polar.size(), std::size_t{2});
  CHECK(polar[0].status == xfoil::AnalysisStatus::converged);
  CHECK(polar[1].status == xfoil::AnalysisStatus::converged);
  CHECK_NEAR(polar[0].alpha.value, 0.0, 1e-12, 0.0);
  CHECK_NEAR(polar[1].alpha.value, -0.5, 1e-12, 0.0);
}

TEST(integration, ModernXFoilUsesExclusivePolarEnd) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = foil.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{1.0}, xfoil::Degrees{0.5});

  CHECK_EQ(polar.size(), std::size_t{2});
  CHECK_NEAR(polar[0].alpha.value, 0.0, 1e-12, 0.0);
  CHECK_NEAR(polar[1].alpha.value, 0.5, 1e-12, 0.0);
  for (const xfoil::AnalysisResult& point : polar) {
    CHECK(point.alpha.value != 1.0);
  }
}

TEST(integration, ModernXFoilHonorsCancellation) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(makeInviscidConfig()).success());

  xfoil::CancellationToken token;
  token.requestCancellation();

  const xfoil::AnalysisResult result =
      foil.analyzeAtAlpha(xfoil::Degrees{4.0}, token);
  CHECK(result.status == xfoil::AnalysisStatus::cancelled);
  CHECK_FALSE(result.successful);
  CHECK_FALSE(result.converged);
  CHECK_EQ(result.iterations, 0);

  const std::vector<xfoil::AnalysisResult> polar = foil.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{2.0}, xfoil::Degrees{0.5}, token);
  CHECK_EQ(polar.size(), std::size_t{1});
  CHECK(polar[0].status == xfoil::AnalysisStatus::cancelled);
  CHECK(foil.lastResult().status == xfoil::AnalysisStatus::cancelled);
}

TEST(integration, ModernXFoilResultIsSnapshot) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foil.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult first =
      foil.analyzeAtAlpha(xfoil::Degrees{4.0});

  const xfoil::AnalysisStatus firstStatus = first.status;
  const bool firstSuccessful = first.successful;
  const bool firstConverged = first.converged;
  const int firstIterations = first.iterations;
  const double firstCl = first.liftCoefficient;
  const double firstCd = first.dragCoefficient;
  const std::string firstDiagnostic = first.diagnostic;

  const xfoil::AnalysisResult second =
      foil.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(first.status == firstStatus);
  CHECK(first.successful == firstSuccessful);
  CHECK(first.converged == firstConverged);
  CHECK_EQ(first.iterations, firstIterations);
  CHECK_EQ(first.liftCoefficient, firstCl);
  CHECK_EQ(first.dragCoefficient, firstCd);
  CHECK(first.diagnostic == firstDiagnostic);

  CHECK(foil.lastResult().status == second.status);
  CHECK_EQ(foil.lastResult().iterations, second.iterations);
}

TEST(integration, ModernXFoilMoveConstructionPreservesUsability) {
  std::unique_ptr<xfoil::XFoil> target;

  {
    xfoil::XFoil source;
    CHECK(source.setGeometry(makeNaca2412Geometry()).success());
    CHECK(source.configure(makeInviscidConfig()).success());

    const xfoil::AnalysisResult beforeMove =
        source.analyzeAtAlpha(xfoil::Degrees{4.0});
    CHECK(beforeMove.status == xfoil::AnalysisStatus::converged);
    CHECK(beforeMove.successful);

    target = std::make_unique<xfoil::XFoil>(std::move(source));
  }

  // the source object is destroyed; the facade must remain analyzable.
  const xfoil::AnalysisResult afterMove =
      target->analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(afterMove.status == xfoil::AnalysisStatus::converged);
  CHECK(afterMove.successful);
  CHECK(afterMove.converged);
  CHECK(std::isfinite(afterMove.liftCoefficient));
}

TEST(integration, ModernXFoilMoveAssignmentPreservesUsability) {
  xfoil::XFoil target;
  CHECK(target.setGeometry(makeClarkYGeometry()).success());
  CHECK(target.configure(makeInviscidConfig()).success());
  const xfoil::AnalysisResult targetBeforeAssignment =
      target.analyzeAtAlpha(xfoil::Degrees{2.0});
  CHECK(targetBeforeAssignment.status == xfoil::AnalysisStatus::converged);

  {
    xfoil::XFoil source;
    CHECK(source.setGeometry(makeNaca2412Geometry()).success());
    CHECK(source.configure(makeInviscidConfig()).success());

    const xfoil::AnalysisResult sourceBeforeMove =
        source.analyzeAtAlpha(xfoil::Degrees{4.0});
    CHECK(sourceBeforeMove.status == xfoil::AnalysisStatus::converged);

    target = std::move(source);
  }

  const xfoil::AnalysisResult afterMoveAssignment =
      target.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(afterMoveAssignment.status == xfoil::AnalysisStatus::converged);
  CHECK(afterMoveAssignment.successful);
  CHECK(afterMoveAssignment.converged);
  CHECK(std::isfinite(afterMoveAssignment.liftCoefficient));
}

TEST(integration, TwoModernXFoilInstancesAreSequentiallyIndependent) {
  xfoil::XFoil foilA;
  CHECK(foilA.setGeometry(makeNaca2412Geometry()).success());
  CHECK(foilA.configure(makeInviscidConfig()).success());
  const xfoil::AnalysisResult resultA =
      foilA.analyzeAtAlpha(xfoil::Degrees{4.0});

  xfoil::XFoil foilB;
  CHECK(foilB.setGeometry(makeClarkYGeometry()).success());
  CHECK(foilB.configure(makeInviscidConfig()).success());
  const xfoil::AnalysisResult resultB =
      foilB.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(resultA.status == xfoil::AnalysisStatus::converged);
  CHECK(resultB.status == xfoil::AnalysisStatus::converged);
  CHECK_NE(resultA.liftCoefficient, resultB.liftCoefficient);

  CHECK_EQ(foilA.geometry().pointCount(), std::size_t{81});
  CHECK_NE(foilB.geometry().pointCount(), std::size_t{81});

  // separate lastResult values: no mutual overwriting.
  CHECK_EQ(foilA.lastResult().liftCoefficient, resultA.liftCoefficient);
  CHECK_EQ(foilB.lastResult().liftCoefficient, resultB.liftCoefficient);
}

// Two modern instances after a run on the same profile keep
// separate diagnostics and separate cancellation tokens; no
// mutual overwriting. Parallelism is explicitly not claimed here.
TEST(integration, TwoModernXFoilInstancesKeepSeparateDiagnosticsAndTokens) {
  xfoil::XFoil foilA;
  CHECK(foilA.setGeometry(makeNaca2412Geometry()).success());
  xfoil::AnalysisConfig configA;
  configA.maximumIterations = 1;  // forces a non-converged result with diagnostics
  CHECK(foilA.configure(configA).success());
  const xfoil::AnalysisResult resultA =
      foilA.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(resultA.status == xfoil::AnalysisStatus::unconverged);
  CHECK(!resultA.diagnostic.empty());

  xfoil::XFoil foilB;
  CHECK(foilB.setGeometry(makeClarkYGeometry()).success());
  CHECK(foilB.configure(makeInviscidConfig()).success());
  const xfoil::AnalysisResult resultB =
      foilB.analyzeAtAlpha(xfoil::Degrees{0.0});
  CHECK(resultB.status == xfoil::AnalysisStatus::converged);

  // separate diagnostics: the analysis of B does not overwrite A.
  CHECK(foilA.lastResult().status == xfoil::AnalysisStatus::unconverged);
  CHECK_EQ(foilA.lastResult().diagnostic, resultA.diagnostic);
  CHECK(resultB.diagnostic != resultA.diagnostic);

  // separate cancellation token: a token depends only on its own instance.
  xfoil::CancellationToken tokenA;
  tokenA.requestCancellation();
  const xfoil::AnalysisResult cancelledA =
      foilA.analyzeAtAlpha(xfoil::Degrees{4.0}, tokenA);
  CHECK(cancelledA.status == xfoil::AnalysisStatus::cancelled);

  xfoil::CancellationToken tokenB;
  const xfoil::AnalysisResult unaffectedB =
      foilB.analyzeAtAlpha(xfoil::Degrees{2.0}, tokenB);
  CHECK(unaffectedB.status == xfoil::AnalysisStatus::converged);
  CHECK(unaffectedB.successful);

  // A stays unchanged by B; B stays unchanged by A and by A's
  // cancellation request.
  CHECK(foilA.lastResult().status == xfoil::AnalysisStatus::cancelled);
  CHECK_EQ(foilB.lastResult().liftCoefficient,
           unaffectedB.liftCoefficient);
}

// CLARK-Y comparison point alpha = 4 against the permanent Fortran proof
// (c11a_harness.f VISCAL(100)): Convergence and iteration count exact (12),
// coefficients against the canonical Fortran full-path values (core == Fortran
// on 10 significant digits, thresholds abs/rel 1e-9). XCP has no
// canonical Fortran field and is only
// checked for plausibility.
TEST(integration, ModernXFoilAnalyzesClarkYViscousAtFourDegreesMatchingFortran) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeClarkYGeometry()).success());
  CHECK(foil.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult result =
      foil.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK_EQ(result.iterations, 12);

  CHECK_NEAR(result.liftCoefficient, 8.2243225894774308e-01, kFortranClAbs,
             kFortranClRel);
  CHECK_NEAR(result.dragCoefficient, 1.7288688307967329e-02, kFortranCdAbs,
             kFortranCdRel);
  CHECK_NEAR(result.momentCoefficient, -8.2790137654465551e-02, kFortranClAbs,
             kFortranClRel);
  CHECK(result.centerOfPressureX > 0.0 && result.centerOfPressureX < 1.0);
}

// Second permanent Fortran comparison point: alpha = 0.5 -> 8 iterations,
// CL 4.2784322458710400E-01, CM -9.0867797856485771E-02,
// CD 1.8639247478485541E-02 (c11a_harness.f VISCAL(100)).
TEST(integration, ModernXFoilAnalyzesClarkYViscousAtHalfDegreeMatchingFortran) {
  xfoil::XFoil foil;
  CHECK(foil.setGeometry(makeClarkYGeometry()).success());
  CHECK(foil.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult result =
      foil.analyzeAtAlpha(xfoil::Degrees{0.5});
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK_EQ(result.iterations, 8);

  CHECK_NEAR(result.liftCoefficient, 4.2784322458710400e-01, kFortranClAbs,
             kFortranClRel);
  CHECK_NEAR(result.dragCoefficient, 1.8639247478485541e-02, kFortranCdAbs,
             kFortranCdRel);
  CHECK_NEAR(result.momentCoefficient, -9.0867797856485771e-02, kFortranClAbs,
             kFortranClRel);
}

// End: tests/integration/ModernXFoilTests.cpp

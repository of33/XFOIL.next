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

// File: tests/integration/XFoilSessionTests.cpp
// integration tests for the XFoilSession API:
// the session uses the Fortran-verified core path).
// The session delegates internally to
// the session-/use-case layer (xfoil::session) and the core kernels.
//
// Geometries per session, no accesses to private solver data.
// Numerically compared CLARK-Y points come from sample/CLARK_Y.dat
// (DatFileReader); the NACA 2412 geometry is produced test-locally.
// the CLARK-Y reference point is the permanent Fortran proof
// (tests/core/ClarkYViscousCoreTests.cpp, c11a_harness.f with the canonical
// build flags -fdefault-real-8 -fdefault-double-8): alpha = 4 -> 12
// iterations, Cl 8.2243225894774308E-01, CM -8.2790137654465551E-02,
// Cd 1.7288688307967329E-02. Thresholds derived directly from the Fortran
// confirmation (core == Fortran to 10 significant digits; abs/rel 1e-9).

#include "TestSupport.h"

#include "app/DatFileReader.h"
#include "xfoil/Limits.h"
#include "xfoil/XFoilSession.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

static_assert(std::is_move_constructible_v<xfoil::XFoilSession>);
static_assert(std::is_move_assignable_v<xfoil::XFoilSession>);
static_assert(!std::is_copy_constructible_v<xfoil::XFoilSession>);
static_assert(!std::is_copy_assignable_v<xfoil::XFoilSession>);

namespace {

constexpr double kPi = 3.14159265358979323846;

// permanent Fortran confirmation (c11a_harness.f, VISCAL(100) FINAL block;
// core == Fortran on 10 significant digits).
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

// test-local NACA 2412 contour in XFoil DAT order (upper side TE -> LE,
// then lower side back to TE), 81 points.
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

// Closed circular contour with a given point count; for rejection
// tests only (it is rejected before every geometry derivation).
xfoil::geometry::AirfoilGeometry makeCircleGeometry(std::size_t pointCount) {
  std::vector<xfoil::Point2D> points;
  for (std::size_t i = 0; i < pointCount; ++i) {
    const double angle =
        2.0 * kPi * static_cast<double>(i) / static_cast<double>(pointCount);
    points.push_back(
        xfoil::Point2D{1.0 + std::cos(angle), std::sin(angle)});
  }

  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign("Circle", points);
  CHECK(assigned.success());
  return geometry;
}

// CLARK-Y from sample/CLARK_Y.dat (as permitted
// reference geometry for the session).
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

TEST(integration, SessionStartsWithoutGeometryOrConfiguration) {
  xfoil::XFoilSession session;
  CHECK_FALSE(session.hasGeometry());
  CHECK_FALSE(session.hasConfiguration());
  CHECK(session.geometry().empty());
  CHECK(session.lastResult().status ==
        xfoil::AnalysisStatus::missingGeometry);
  CHECK_FALSE(session.lastResult().successful);
}

TEST(integration, SessionRejectsAnalysisWithoutGeometry) {
  xfoil::XFoilSession session;
  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(result.status == xfoil::AnalysisStatus::missingGeometry);
  CHECK_FALSE(result.successful);
  CHECK_FALSE(result.converged);
  CHECK_EQ(result.iterations, 0);
  CHECK(session.lastResult().status ==
        xfoil::AnalysisStatus::missingGeometry);
}

TEST(integration, SessionRejectsAnalysisWithoutConfiguration) {
  xfoil::XFoilSession session;
  const auto geometrySet = session.setGeometry(makeNaca2412Geometry());
  CHECK(geometrySet.success());

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(result.status == xfoil::AnalysisStatus::missingConfiguration);
  CHECK_FALSE(result.successful);
}

TEST(integration, SessionAcceptsValidGeometry) {
  xfoil::XFoilSession session;
  const auto geometrySet = session.setGeometry(makeNaca2412Geometry());
  CHECK(geometrySet.success());
  CHECK(session.hasGeometry());
  CHECK_EQ(session.geometry().pointCount(), std::size_t{81});
  CHECK(!session.geometry().name().empty());
  CHECK(session.lastResult().status ==
      xfoil::AnalysisStatus::missingConfiguration);
}

TEST(integration, SessionRejectsGeometryAbovePointLimit) {
  xfoil::XFoilSession session;
  const auto geometrySet = session.setGeometry(
      makeCircleGeometry(xfoil::limits::maxInputCoordinates + 1));
  CHECK_FALSE(geometrySet.success());
  CHECK(geometrySet.error == xfoil::GeometrySetError::tooManyPoints);
  CHECK_EQ(
      geometrySet.message,
      std::string("Geometry exceeds the point limit"));
  CHECK_FALSE(session.hasGeometry());
  CHECK(session.geometry().empty());
}

TEST(integration, SessionPreservesGeometryAfterFailedReplacement) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());

  const auto emptyAttempt =
      session.setGeometry(xfoil::geometry::AirfoilGeometry{});
  CHECK_FALSE(emptyAttempt.success());
  CHECK(emptyAttempt.error == xfoil::GeometrySetError::emptyGeometry);
  CHECK(session.hasGeometry());
  CHECK_EQ(session.geometry().pointCount(), std::size_t{81});

  const auto tooLargeAttempt = session.setGeometry(
      makeCircleGeometry(xfoil::limits::maxInputCoordinates + 1));
  CHECK_FALSE(tooLargeAttempt.success());
  CHECK(tooLargeAttempt.error == xfoil::GeometrySetError::tooManyPoints);
  CHECK(session.hasGeometry());
  CHECK_EQ(session.geometry().pointCount(), std::size_t{81});
}

TEST(integration, SessionAcceptsValidConfiguration) {
  xfoil::XFoilSession session;
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 200000.0;
  const auto configured = session.configure(config);
  CHECK(configured.success());
  CHECK(session.hasConfiguration());
  CHECK_EQ(session.config().reynoldsNumber, 200000.0);
}

TEST(integration, SessionBecomesReadyWithoutAnalysis) {
  xfoil::XFoilSession session;

  CHECK(session.configure(makeInviscidConfig()).success());
  CHECK(session.hasConfiguration());
  CHECK_FALSE(session.hasGeometry());
  CHECK(session.lastResult().status ==
    xfoil::AnalysisStatus::missingGeometry);

  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.hasGeometry());
  CHECK(session.hasConfiguration());
  CHECK(session.lastResult().status ==
    xfoil::AnalysisStatus::notAnalyzed);
  CHECK_FALSE(session.lastResult().successful);
  CHECK_FALSE(session.lastResult().converged);
  CHECK_EQ(session.lastResult().iterations, 0);
}

TEST(integration, SessionConfigurationAfterGeometryMarksNotAnalyzed) {
  xfoil::XFoilSession session;

  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.hasGeometry());
  CHECK_FALSE(session.hasConfiguration());
  CHECK(session.lastResult().status ==
  xfoil::AnalysisStatus::missingConfiguration);

  CHECK(session.configure(makeInviscidConfig()).success());
  CHECK(session.hasGeometry());
  CHECK(session.hasConfiguration());
  CHECK(session.lastResult().status ==
  xfoil::AnalysisStatus::notAnalyzed);
  CHECK_FALSE(session.lastResult().successful);
  CHECK_FALSE(session.lastResult().converged);
  CHECK_EQ(session.lastResult().iterations, 0);
}

TEST(integration, SessionPreservesConfigurationAfterInvalidReplacement) {
  xfoil::XFoilSession session;
  xfoil::AnalysisConfig validConfig;
  validConfig.reynoldsNumber = 200000.0;
  CHECK(session.configure(validConfig).success());

  xfoil::AnalysisConfig invalidConfig;
  invalidConfig.machNumber = 1.5;
  const auto rejected = session.configure(invalidConfig);
  CHECK_FALSE(rejected.success());
  CHECK(rejected.error == xfoil::ConfigError::invalidMachNumber);
  CHECK_EQ(session.config().reynoldsNumber, 200000.0);
  CHECK_EQ(session.config().machNumber, 0.0);
  CHECK(session.hasConfiguration());
}

TEST(integration, SessionAnalyzesNaca2412InviscidAtFourDegrees) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(makeInviscidConfig()).success());

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK_EQ(result.iterations, 0);
  CHECK(result.liftCoefficient > 0.5);
  CHECK(result.momentCoefficient < 0.0);
  CHECK(std::isfinite(result.centerOfPressureX));
}

TEST(integration, SessionAnalyzesNaca2412ViscousAtFourDegrees) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});

  // the test-local geometry produces a different discretization than
 // the NACA4 core path; therefore only plausibility, no frozen values.
  CHECK(result.status == xfoil::AnalysisStatus::converged);
  CHECK(result.successful);
  CHECK(result.converged);
  CHECK(result.iterations > 0);
  CHECK(result.iterations < 100);
  CHECK(result.liftCoefficient > 0.5 && result.liftCoefficient < 1.0);
  CHECK(result.dragCoefficient > 0.0 && result.dragCoefficient < 0.1);
  CHECK(result.momentCoefficient < 0.0);
  CHECK(result.centerOfPressureX > 0.2 && result.centerOfPressureX < 0.6);
}

// comparison with the permanent Fortran proof (c11a_harness.f,
// VISCAL(100)): convergence and iteration count exact (12), coefficients
// against the canonical Fortran full-path values (core == Fortran on 10
// significant digits, thresholds abs/rel 1e-9). XCP has no canonical
// Fortran field and is only checked for plausibility.
TEST(integration, SessionAnalyzesClarkYViscousAtFourDegreesMatchingFortran) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeClarkYGeometry()).success());
  CHECK(session.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});
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
  CHECK(std::isfinite(result.rmsResidual));
}

// Second permanent Fortran comparison point: alpha = 0.5 -> 8 iterations,
// CL 4.2784322458710400E-01, CM -9.0867797856485771E-02,
// CD 1.8639247478485541E-02 (c11a_harness.f VISCAL(100)).
TEST(integration, SessionAnalyzesClarkYViscousAtHalfDegreeMatchingFortran) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeClarkYGeometry()).success());
  CHECK(session.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{0.5});
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

TEST(integration, SessionReturnsStableResultSnapshot) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(xfoil::AnalysisConfig{}).success());

  const xfoil::AnalysisResult first =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});

  const xfoil::AnalysisStatus firstStatus = first.status;
  const bool firstSuccessful = first.successful;
  const bool firstConverged = first.converged;
  const int firstIterations = first.iterations;
  const double firstCl = first.liftCoefficient;
  const double firstCd = first.dragCoefficient;
  const double firstCm = first.momentCoefficient;
  const double firstXcp = first.centerOfPressureX;
  const std::string firstDiagnostic = first.diagnostic;

  const xfoil::AnalysisResult second =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(first.status == firstStatus);
  CHECK(first.successful == firstSuccessful);
  CHECK(first.converged == firstConverged);
  CHECK_EQ(first.iterations, firstIterations);
  CHECK_EQ(first.liftCoefficient, firstCl);
  CHECK_EQ(first.dragCoefficient, firstCd);
  CHECK_EQ(first.momentCoefficient, firstCm);
  CHECK_EQ(first.centerOfPressureX, firstXcp);
  CHECK(first.diagnostic == firstDiagnostic);

  CHECK(session.lastResult().status == second.status);
  CHECK_EQ(session.lastResult().iterations, second.iterations);
}

TEST(integration, SessionDiagnosticsArePerAnalysisPoint) {
  xfoil::XFoilSession session;

  CHECK(session.setGeometry(makeNaca2412Geometry()).success());

  xfoil::AnalysisConfig config;
  config.maximumIterations = 1;
  CHECK(session.configure(config).success());

  const xfoil::AnalysisResult first =
  session.analyzeAtAlpha(xfoil::Degrees{4.0});

  const xfoil::AnalysisResult second =
  session.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(first.status == xfoil::AnalysisStatus::unconverged);
  CHECK(second.status == xfoil::AnalysisStatus::unconverged);
  CHECK(first.successful);
  CHECK(second.successful);

  // the session diagnostics carries the canonical VISCAL iteration lines
  // (xoper.f 2990-3003, one confirmation line per iteration).
  const std::string marker = "     a=";

      const auto containsExactlyOnce =
      [&marker](const std::string& text) {
        const std::size_t firstPosition = text.find(marker);
        if (firstPosition == std::string::npos) {
          return false;
        }

        const std::size_t secondPosition =
        text.find(marker, firstPosition + marker.size());

        return secondPosition == std::string::npos;
      };

      CHECK(containsExactlyOnce(first.diagnostic));
      CHECK(containsExactlyOnce(second.diagnostic));
}

TEST(integration, SessionCalculatesPositivePolar) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = session.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{2.0}, xfoil::Degrees{0.5});

  CHECK_EQ(polar.size(), std::size_t{4});
  for (std::size_t i = 0; i < polar.size(); ++i) {
    CHECK(polar[i].status == xfoil::AnalysisStatus::converged);
    CHECK(polar[i].successful);
    CHECK_NEAR(
        polar[i].alpha.value, static_cast<double>(i) * 0.5, 1e-12, 0.0);
  }
}

TEST(integration, SessionCalculatesNegativePolar) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = session.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{-1.0}, xfoil::Degrees{-0.5});

  CHECK_EQ(polar.size(), std::size_t{2});
  CHECK(polar[0].status == xfoil::AnalysisStatus::converged);
  CHECK(polar[1].status == xfoil::AnalysisStatus::converged);
  CHECK_NEAR(polar[0].alpha.value, 0.0, 1e-12, 0.0);
  CHECK_NEAR(polar[1].alpha.value, -0.5, 1e-12, 0.0);
}

TEST(integration, SessionUsesExclusivePolarEnd) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = session.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{1.0}, xfoil::Degrees{0.5});

  CHECK_EQ(polar.size(), std::size_t{2});
  CHECK_NEAR(polar[0].alpha.value, 0.0, 1e-12, 0.0);
  CHECK_NEAR(polar[1].alpha.value, 0.5, 1e-12, 0.0);
  for (const xfoil::AnalysisResult& point : polar) {
    CHECK(point.alpha.value != 1.0);
  }
}

TEST(integration, SessionRejectsZeroPolarStep) {
  xfoil::XFoilSession session;
  const std::vector<xfoil::AnalysisResult> polar = session.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{2.0}, xfoil::Degrees{0.0});

  CHECK_EQ(polar.size(), std::size_t{1});
  CHECK(polar[0].status == xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_FALSE(polar[0].successful);
  CHECK_EQ(polar[0].diagnostic, std::string("Invalid polar range"));
}

TEST(integration, SessionRejectsWrongStepDirection) {
  xfoil::XFoilSession session;

  const std::vector<xfoil::AnalysisResult> descending = session.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{2.0}, xfoil::Degrees{-0.5});
  CHECK_EQ(descending.size(), std::size_t{1});
  CHECK(descending[0].status == xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_EQ(descending[0].diagnostic, std::string("Invalid polar range"));

  const std::vector<xfoil::AnalysisResult> ascending = session.analyzePolar(
      xfoil::Degrees{2.0}, xfoil::Degrees{0.0}, xfoil::Degrees{0.5});
  CHECK_EQ(ascending.size(), std::size_t{1});
  CHECK(ascending[0].status == xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_EQ(ascending[0].diagnostic, std::string("Invalid polar range"));
}

TEST(integration, SessionLastResultTracksInvalidPolarRange) {
  xfoil::XFoilSession session;

  const std::vector<xfoil::AnalysisResult> polar =
  session.analyzePolar(
    xfoil::Degrees{0.0},
    xfoil::Degrees{1.0},
    xfoil::Degrees{0.0});

  CHECK_EQ(polar.size(), std::size_t{1});
  CHECK(polar[0].status ==
  xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_FALSE(polar[0].successful);
  CHECK_EQ(
    polar[0].diagnostic,
    std::string("Invalid polar range"));

  CHECK(session.lastResult().status ==
  xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_FALSE(session.lastResult().successful);
  CHECK_EQ(
    session.lastResult().diagnostic,
           std::string("Invalid polar range"));
}

// next-after-one step: 1.0 + 5e-324 is again 1.0; the loop
// must not stagnate; the polar logic depends on an error result.
TEST(integration, SessionStopsPolarOnStagnatingStep) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(makeInviscidConfig()).success());

  const std::vector<xfoil::AnalysisResult> polar = session.analyzePolar(
      xfoil::Degrees{1.0}, xfoil::Degrees{2.0}, xfoil::Degrees{5e-324});

  CHECK_EQ(polar.size(), std::size_t{2});
  CHECK(polar[0].status == xfoil::AnalysisStatus::converged);
  CHECK(polar[1].status == xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_EQ(polar[1].diagnostic, std::string("Polar sequence stalled"));
  CHECK(session.lastResult().status ==
  xfoil::AnalysisStatus::invalidConfiguration);
  CHECK_FALSE(session.lastResult().successful);
  CHECK_EQ(
    session.lastResult().diagnostic,
           std::string("Polar sequence stalled"));
}

TEST(integration, SessionHonorsPreRequestedCancellation) {
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeNaca2412Geometry()).success());
  CHECK(session.configure(makeInviscidConfig()).success());

  xfoil::CancellationToken token;
  token.requestCancellation();

  const xfoil::AnalysisResult result =
      session.analyzeAtAlpha(xfoil::Degrees{4.0}, token);
  CHECK(result.status == xfoil::AnalysisStatus::cancelled);
  CHECK_FALSE(result.successful);
  CHECK_FALSE(result.converged);
  CHECK_EQ(result.iterations, 0);

  const std::vector<xfoil::AnalysisResult> polar = session.analyzePolar(
      xfoil::Degrees{0.0}, xfoil::Degrees{2.0}, xfoil::Degrees{0.5}, token);
  CHECK_EQ(polar.size(), std::size_t{1});
  CHECK(polar[0].status == xfoil::AnalysisStatus::cancelled);
  CHECK(session.lastResult().status ==
    xfoil::AnalysisStatus::cancelled);
  CHECK_FALSE(session.lastResult().successful);
  CHECK_FALSE(session.lastResult().converged);

  // the previous single point used alpha 4. The unexecuted
  // polar point defaults to alpha 0. This check proves that
 // lastResult_ was actually updated by analyzePolar.
  CHECK_NEAR(
    session.lastResult().alpha.value,
             0.0,
             0.0,
             0.0);
}

TEST(integration, MovedSessionRebindsDiagnosticStreamBeforeAnalysis) {
  std::unique_ptr<xfoil::XFoilSession> target;

  {
    xfoil::XFoilSession source;

    CHECK(source.setGeometry(makeNaca2412Geometry()).success());
    CHECK(source.configure(makeInviscidConfig()).success());

    const xfoil::AnalysisResult beforeMove =
    source.analyzeAtAlpha(xfoil::Degrees{4.0});

    CHECK(beforeMove.status == xfoil::AnalysisStatus::converged);
    CHECK(beforeMove.successful);

    target =
    std::make_unique<xfoil::XFoilSession>(std::move(source));
  }

  // the source object is now destroyed. A stream pointer
  // that was not rebound would be invalid.
  const xfoil::AnalysisResult afterMove =
  target->analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(afterMove.status == xfoil::AnalysisStatus::converged);
  CHECK(afterMove.successful);
  CHECK(afterMove.converged);
  CHECK_EQ(afterMove.iterations, 0);
  CHECK(std::isfinite(afterMove.liftCoefficient));
  CHECK(std::isfinite(afterMove.momentCoefficient));
}

TEST(integration, MoveAssignedSessionCanAnalyzeAgain) {
  xfoil::XFoilSession target;

  CHECK(target.setGeometry(makeClarkYGeometry()).success());
  CHECK(target.configure(makeInviscidConfig()).success());

  const xfoil::AnalysisResult targetBeforeAssignment =
  target.analyzeAtAlpha(xfoil::Degrees{2.0});

  CHECK(targetBeforeAssignment.status ==
  xfoil::AnalysisStatus::converged);

  {
    xfoil::XFoilSession source;

    CHECK(source.setGeometry(makeNaca2412Geometry()).success());
    CHECK(source.configure(makeInviscidConfig()).success());

    const xfoil::AnalysisResult sourceBeforeMove =
    source.analyzeAtAlpha(xfoil::Degrees{4.0});

    CHECK(sourceBeforeMove.status ==
    xfoil::AnalysisStatus::converged);
    CHECK(sourceBeforeMove.successful);

    target = std::move(source);
  }

  // Here too the source is destroyed before the repeated analysis.
  const xfoil::AnalysisResult afterMoveAssignment =
  target.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(afterMoveAssignment.status ==
  xfoil::AnalysisStatus::converged);
  CHECK(afterMoveAssignment.successful);
  CHECK(afterMoveAssignment.converged);
  CHECK_EQ(afterMoveAssignment.iterations, 0);
  CHECK(std::isfinite(afterMoveAssignment.liftCoefficient));
  CHECK(std::isfinite(afterMoveAssignment.momentCoefficient));
}

TEST(integration, TwoSessionsDoNotShareGeometryOrResults) {
  xfoil::XFoilSession sessionA;
  CHECK(sessionA.setGeometry(makeNaca2412Geometry()).success());
  CHECK(sessionA.configure(makeInviscidConfig()).success());
  const xfoil::AnalysisResult resultA =
      sessionA.analyzeAtAlpha(xfoil::Degrees{4.0});

  xfoil::XFoilSession sessionB;
  CHECK(sessionB.setGeometry(makeClarkYGeometry()).success());
  CHECK(sessionB.configure(makeInviscidConfig()).success());
  const xfoil::AnalysisResult resultB =
      sessionB.analyzeAtAlpha(xfoil::Degrees{4.0});

  CHECK(resultA.status == xfoil::AnalysisStatus::converged);
  CHECK(resultB.status == xfoil::AnalysisStatus::converged);
  CHECK_NE(resultA.liftCoefficient, resultB.liftCoefficient);

  CHECK_EQ(sessionA.geometry().pointCount(), std::size_t{81});
  CHECK_NE(sessionB.geometry().pointCount(), std::size_t{81});

  // the results stay by value and independent of the other session.
  CHECK_EQ(sessionA.lastResult().liftCoefficient, resultA.liftCoefficient);
  CHECK_EQ(sessionB.lastResult().liftCoefficient, resultB.liftCoefficient);
}

// End: tests/integration/XFoilSessionTests.cpp

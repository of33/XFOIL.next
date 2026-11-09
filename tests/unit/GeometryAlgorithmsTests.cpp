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

// File: tests/unit/GeometryAlgorithmsTests.cpp
// unit tests for xfoil::geometry. All numerical
// test data are 1-based: index 0 is reserved and unused. For the
// leading-edge tests, small, clearly documented contours are generated
// locally; the NACA helpers of the regression tests are not used.

#include "TestSupport.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "xfoil/geometry/GeometryAlgorithms.h"
#include "xfoil/math/Spline.h"

namespace {

// 1-based vector: size pointCount + 1, index 0 reserved.
std::vector<double> storage(std::size_t pointCount, double value = 0.0) {
  return std::vector<double>(pointCount + 1, value);
}

// exact PI constant used for expected values.
constexpr double kPi = 3.141592654;

// closed contour with rounded leading edge: upper side from TE (x=1)
// back to LE (x=0), lower side mirrored back. Thickness
// t(x) = 0.1 * sqrt(x) * (1 - x): vertical tangent at x=0 (rounded
// LE), closed TE at x=1 with t(1) = 0. By the exact mirroring
// trailingEdgeY == 0 holds, and the leading-edge fixed point lies at
// parameter = s[n] / 2 with leadingEdgeX ~ 0 and leadingEdgeY ~ 0.
struct SymmetricContour {
  int pointCount{0};
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> s;
  std::vector<double> xp;
  std::vector<double> yp;
};

SymmetricContour buildRoundedLeadingEdgeContour(int pointsPerSide = 41) {
  SymmetricContour contour;
  const int n = 2 * pointsPerSide - 1;
  const auto count = static_cast<std::size_t>(n);
  contour.pointCount = n;
  contour.x = storage(count);
  contour.y = storage(count);

  // upper side: x from 1 down to 0.
  for (int i = 0; i < pointsPerSide; ++i) {
    const auto index = static_cast<std::size_t>(i) + 1;
    const double x = 1.0 - static_cast<double>(i) /
                          static_cast<double>(pointsPerSide - 1);
    contour.x[index] = x;
    contour.y[index] = 0.1 * std::sqrt(x) * (1.0 - x);
  }
  // Bottom side: the top side mirrored in reverse order. The
  // mirroring reaches to TE point 1, so the contour is closed
  // and exactly reverse-symmetric (p[n] = (x[1], -y[1])).
  for (int j = 1; j <= pointsPerSide - 1; ++j) {
    const auto index = static_cast<std::size_t>(pointsPerSide + j);
    const auto source = static_cast<std::size_t>(pointsPerSide - j);
    contour.x[index] = contour.x[source];
    contour.y[index] = -contour.y[source];
  }

  contour.s = storage(count);
  CHECK(xfoil::math::spline::calculateArcLength(
      contour.x, contour.y, contour.s, n));

  contour.xp = storage(count);
  contour.yp = storage(count);
  CHECK(xfoil::math::spline::segmentedSplineDerivatives(
      contour.x, contour.xp, contour.s, n));
  CHECK(xfoil::math::spline::segmentedSplineDerivatives(
      contour.y, contour.yp, contour.s, n));

  return contour;
}

}  // namespace

// ---
// Corner angle (cang)
// ---

TEST(unit, MaximumCornerAngleOfStraightPolylineIsZero) {
  const std::vector<double> x = {0.0, 0.0, 1.0, 2.0, 3.0};
  const std::vector<double> y = {0.0, 0.0, 1.0, 2.0, 3.0};

  const auto result = xfoil::geometry::findMaximumCornerAngle(x, y, 4);

  CHECK(result.success);
  CHECK_EQ(result.pointIndex, 1);
  CHECK_NEAR(result.angleDegrees, 0.0, 0.0, 0.0);
}

TEST(unit, MaximumCornerAngleFindsKnownCorner) {
  // right-angle kink at index 3: horizontal run, then vertical run.
  const std::vector<double> x = {0.0, 0.0, 1.0, 2.0, 2.0, 2.0};
  const std::vector<double> y = {0.0, 0.0, 0.0, 0.0, 1.0, 2.0};

  const auto result = xfoil::geometry::findMaximumCornerAngle(x, y, 5);

  CHECK(result.success);
  CHECK_EQ(result.pointIndex, 3);
  const double expected = std::asin(1.0) * (180.0 / kPi);
  CHECK_NEAR(result.angleDegrees, expected, 1e-12, 0.0);
}

TEST(unit, MaximumCornerAngleUsesAtan2Quadrant) {
  const std::vector<double> x = {0.0, 0.0, 1.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 0.0, 0.0, 1.0, 1.0};
  const auto result = xfoil::geometry::findMaximumCornerAngle(x, y, 4);
  CHECK(result.success);
  CHECK_EQ(result.pointIndex, 2);
  const double expected = std::atan2(1.0, -1.0) * (180.0 / kPi);
  CHECK_NEAR(result.angleDegrees, expected, 1e-12, 0.0);
}

TEST(unit, MaximumCornerAngleRejectsTooFewPoints) {
  const std::vector<double> x = {0.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 0.0, 1.0};

  const auto result = xfoil::geometry::findMaximumCornerAngle(x, y, 2);

  CHECK(!result.success);
  CHECK_EQ(result.pointIndex, 1);
  CHECK_NEAR(result.angleDegrees, 0.0, 0.0, 0.0);
}

TEST(unit, MaximumCornerAngleRejectsUnsafeDoubledBoundaryPoint) {
  // Duplicated endpoint at the boundary: the
  // formula would access
  // index i + 2 = n + 1 for i = n - 1; the API rejects this controlled.
  const std::vector<double> x = {0.0, 0.0, 1.0, 1.0};
  const std::vector<double> y = {0.0, 0.0, 0.0, 0.0};

  const auto result = xfoil::geometry::findMaximumCornerAngle(x, y, 3);

  CHECK(!result.success);
}

TEST(unit, MaximumCornerAngleRejectsUnsafeDoubledFirstSegment) {
  // Duplicated first segment: at i = 2 CANG would access
  // the reserved index i - 2 == 0 as a domain point.
  const std::vector<double> x = {0.0, 0.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 0.0, 0.0, 0.0};

  const auto result =
  xfoil::geometry::findMaximumCornerAngle(x, y, 3);

  CHECK_FALSE(result.success);
  CHECK_EQ(result.pointIndex, 1);
  CHECK_NEAR(result.angleDegrees, 0.0, 0.0, 0.0);
}

// ---
// point containment (inside)
// ---

TEST(unit, ContainsPointInsideSquare) {
  // closed contour of the unit square (1-based).
  const std::vector<double> x = {0.0, 0.0, 1.0, 1.0, 0.0};
  const std::vector<double> y = {0.0, 0.0, 0.0, 1.0, 1.0};

  CHECK(xfoil::geometry::containsPoint(x, y, 4, 0.5, 0.5));
}

TEST(unit, ContainsPointOutsideSquare) {
  const std::vector<double> x = {0.0, 0.0, 1.0, 1.0, 0.0};
  const std::vector<double> y = {0.0, 0.0, 0.0, 1.0, 1.0};

  CHECK_FALSE(xfoil::geometry::containsPoint(x, y, 4, 2.0, 2.0));
}

TEST(unit, ContainsPointRejectsTooFewPoints) {
  const std::vector<double> x = {0.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 0.0, 0.0};

  CHECK_FALSE(xfoil::geometry::containsPoint(x, y, 2, 0.5, 0.5));
}

// ---
// normals (ncalc)
// ---

TEST(unit, CalculateNormalsOnSmoothOpenCurve) {
  // Straight polyline x(s) = s, y(s) = 0: exact linear
  // spline derivatives (x' = 1, y' = 0) deliver the constant normal
  // (0, -1).
  const int n = 5;
  std::vector<double> x = storage(5);
  std::vector<double> y = storage(5);
  std::vector<double> s = storage(5);
  for (int i = 1; i <= n; ++i) {
    x[static_cast<std::size_t>(i)] = static_cast<double>(i - 1);
    s[static_cast<std::size_t>(i)] = static_cast<double>(i - 1);
  }
  std::vector<double> normalX = storage(5);
  std::vector<double> normalY = storage(5);

  CHECK(xfoil::geometry::calculateNormals(x, y, s, normalX, normalY, n));

  for (int i = 1; i <= n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    CHECK_NEAR(normalX[index], 0.0, 1e-9, 0.0);
    CHECK_NEAR(normalY[index], -1.0, 1e-9, 0.0);
  }
}

TEST(unit, CalculateNormalsProducesFiniteUnitVectors) {
  const SymmetricContour contour = buildRoundedLeadingEdgeContour(21);
  std::vector<double> normalX = storage(41);
  std::vector<double> normalY = storage(41);

  CHECK(xfoil::geometry::calculateNormals(
      contour.x, contour.y, contour.s, normalX, normalY,
      contour.pointCount));

  for (int i = 1; i <= contour.pointCount; ++i) {
    const auto index = static_cast<std::size_t>(i);
    CHECK(std::isfinite(normalX[index]));
    CHECK(std::isfinite(normalY[index]));
    const double length =
        std::sqrt(normalX[index] * normalX[index] +
                  normalY[index] * normalY[index]);
    CHECK_NEAR(length, 1.0, 1e-12, 0.0);
  }
}

TEST(unit, CalculateNormalsHandlesInnerDuplicateParameter) {
  // Interior duplicate parameter value for i = 3 and i = 4 (s[3] == s[4]):
  // the averaged normals of both nodes must be identical.
  const int n = 5;
  std::vector<double> x = {0.0, 0.0, 1.0, 2.0, 3.0, 4.0};
  std::vector<double> y = {0.0, 0.0, 0.0, 0.5, 1.0, 1.0};
  std::vector<double> s = {0.0, 0.0, 1.0, 2.0, 2.0, 3.0};
  std::vector<double> normalX = storage(5);
  std::vector<double> normalY = storage(5);

  CHECK(xfoil::geometry::calculateNormals(x, y, s, normalX, normalY, n));

  CHECK_EQ(normalX[3], normalX[4]);
  CHECK_EQ(normalY[3], normalY[4]);
  for (int i = 1; i <= n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    const double length =
        std::sqrt(normalX[index] * normalX[index] +
                  normalY[index] * normalY[index]);
    CHECK_NEAR(length, 1.0, 1e-9, 0.0);
  }
}

TEST(unit, CalculateNormalsRejectsDegenerateTangent) {
  // constant coordinates: all spline derivatives are exactly zero, the
  // tangent norm is 0 -> controlled failure instead of division
  // by zero.
  const int n = 5;
  std::vector<double> x = storage(5, 1.0);
  std::vector<double> y = storage(5, 2.0);
  std::vector<double> s = {0.0, 0.0, 1.0, 2.0, 3.0, 4.0};
  std::vector<double> normalX = storage(5);
  std::vector<double> normalY = storage(5);

  CHECK_FALSE(xfoil::geometry::calculateNormals(
      x, y, s, normalX, normalY, n));
}

TEST(unit, CalculateNormalsRejectsShortSpan) {
  // normalX span has only pointCount elements; required are
  // pointCount + 1 (index 0 reserved).
  const int n = 4;
  std::vector<double> x = {0.0, 0.0, 1.0, 2.0, 3.0};
  std::vector<double> y = storage(4);
  std::vector<double> s = {0.0, 0.0, 1.0, 2.0, 3.0};
  std::vector<double> normalX = storage(3);
  std::vector<double> normalY = storage(4);

  CHECK_FALSE(xfoil::geometry::calculateNormals(
      x, y, s, normalX, normalY, n));
}

// ---
// leading edge (lefind)
// ---

TEST(unit, FindLeadingEdgeOnSymmetricAirfoil) {
  const SymmetricContour contour = buildRoundedLeadingEdgeContour(41);

  const auto result = xfoil::geometry::findLeadingEdge(
      contour.x, contour.xp, contour.y, contour.yp, contour.s,
      contour.pointCount);

  CHECK(result.success);
  // By the exact mirroring the fixed point lies at s[n] / 2 with
  // leading-edge coordinates (0, 0).
  const double expectedParameter =
      0.5 * contour.s[static_cast<std::size_t>(contour.pointCount)];
  CHECK_NEAR(result.parameter, expectedParameter, 1e-9, 0.0);
  CHECK_NEAR(result.leadingEdgeX, 0.0, 1e-9, 0.0);
  CHECK_NEAR(result.leadingEdgeY, 0.0, 1e-9, 0.0);
}

TEST(unit, FindLeadingEdgeReturnsTrailingEdgeMidpoint) {
  const SymmetricContour contour = buildRoundedLeadingEdgeContour(41);
  const auto last = static_cast<std::size_t>(contour.pointCount);

  const auto result = xfoil::geometry::findLeadingEdge(
      contour.x, contour.xp, contour.y, contour.yp, contour.s,
      contour.pointCount);

  CHECK(result.success);
  CHECK_NEAR(result.trailingEdgeX,
             0.5 * (contour.x[1] + contour.x[last]), 0.0, 0.0);
  CHECK_NEAR(result.trailingEdgeY,
             0.5 * (contour.y[1] + contour.y[last]), 0.0, 0.0);
}

TEST(unit, FindLeadingEdgeRejectsTooFewPoints) {
  const SymmetricContour contour = buildRoundedLeadingEdgeContour(41);

  const auto result = xfoil::geometry::findLeadingEdge(
      contour.x, contour.xp, contour.y, contour.yp, contour.s, 3);

  CHECK(!result.success);
}

TEST(unit, FindLeadingEdgeSmallNGuardUsesMidpoint) {
  // 6.996 LEFIND small-N guard; N=4 uses the discrete midpoint index.
  constexpr int n = 4;
  constexpr double pi = 3.14159265358979323846;
  std::vector<double> x(n + 1, 0.0);
  std::vector<double> y(n + 1, 0.0);
  std::vector<double> s(n + 1, 0.0);
  for (int i = 1; i <= n; ++i) {
    const double t = 2.0 * pi * static_cast<double>(i - 1) /
                     static_cast<double>(n - 1);
    x[static_cast<std::size_t>(i)] = 0.5 + 0.5 * std::cos(t);
    y[static_cast<std::size_t>(i)] = 0.10 * std::sin(t);
  }
  CHECK(xfoil::math::spline::calculateArcLength(x, y, s, n));
  std::vector<double> xp(n + 1, 0.0);
  std::vector<double> yp(n + 1, 0.0);
  CHECK(xfoil::math::spline::segmentedSplineDerivatives(x, xp, s, n));
  CHECK(xfoil::math::spline::segmentedSplineDerivatives(y, yp, s, n));
  const auto result = xfoil::geometry::findLeadingEdge(x, xp, y, yp, s, n);
  CHECK(result.success);
  CHECK_NEAR(result.parameter, 8.415859839056992e-1, 1e-12, 0.0);
}

TEST(unit, FindLeadingEdgeRejectsMissingBracket) {
  // No point with a negative scalar product (x - xte, y - yte). dx in
  // i = 3..n-2: all averages lie to the right of xte and the steps
  // point right. The function rejects this in a controlled way, without using the
  // last loop index.
  const int n = 7;
  std::vector<double> x = {0.0, 0.0, 0.1, 5.0, 6.0, 7.0, 8.0, 0.9};
  std::vector<double> y = storage(7);
  std::vector<double> s = {0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::vector<double> xp = storage(7);
  std::vector<double> yp = storage(7);

  const auto result = xfoil::geometry::findLeadingEdge(
      x, xp, y, yp, s, n);

  CHECK(!result.success);
  CHECK_NEAR(result.parameter, 0.0, 0.0, 0.0);
  CHECK(std::isnan(result.leadingEdgeX));
  CHECK(std::isnan(result.leadingEdgeY));
  CHECK_NEAR(result.trailingEdgeX, 0.45, 0.0, 0.0);
  CHECK_NEAR(result.trailingEdgeY, 0.0, 0.0, 0.0);
}

TEST(unit, FindLeadingEdgeHandlesSharpLeadingEdge) {
  // Bracket for i = 4 with duplicate parameter value s[4] == s[3]: the
  // result carries the discrete start value,
  // the trailing-edge values and NaN as signal for "xle/yle never set".
  const int n = 6;
  std::vector<double> x = {0.0, 0.0, 4.0, 8.0, 9.0, 7.0, 10.0};
  std::vector<double> y = storage(6);
  std::vector<double> s = {0.0, 0.0, 1.0, 2.0, 2.0, 3.0, 4.0};
  std::vector<double> xp = storage(6);
  std::vector<double> yp = storage(6);

  const auto result = xfoil::geometry::findLeadingEdge(
      x, xp, y, yp, s, n);

  CHECK(!result.success);
  CHECK_NEAR(result.parameter, 2.0, 0.0, 0.0);
  CHECK(std::isnan(result.leadingEdgeX));
  CHECK(std::isnan(result.leadingEdgeY));
  CHECK_NEAR(result.trailingEdgeX, 5.0, 0.0, 0.0);
  CHECK_NEAR(result.trailingEdgeY, 0.0, 0.0, 0.0);
}

// ---
// Leftmost point (xlfind)
// ---

TEST(unit, FindLeftmostPointOnSmoothCurve) {
  // x-values from a right-opening parabola x(t) = 0.3 * (t - 3.5)^2,
  // symmetric around t = 3.5: the spline derivative dx/ds vanishes there.
  const int n = 8;
  std::vector<double> x = storage(8);
  std::vector<double> y = storage(8);
  std::vector<double> s = storage(8);
  for (int i = 1; i <= n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    const double t = static_cast<double>(i - 1);
    x[index] = 0.3 * (t - 3.5) * (t - 3.5);
    s[index] = t;
  }
  std::vector<double> xp = storage(8);
  std::vector<double> yp = storage(8);
  CHECK(xfoil::math::spline::segmentedSplineDerivatives(x, xp, s, n));

  const auto result = xfoil::geometry::findLeftmostPoint(
      x, xp, y, yp, s, n);

  CHECK(result.success);
  CHECK_NEAR(result.parameter, 3.5, 1e-6, 0.0);
}

TEST(unit, FindLeftmostPointRejectsMissingBracket) {
  // Monotonically decreasing x: no sign change x[i + 1] - x[i] > 0 in i = 3..n-2.
  const int n = 6;
  std::vector<double> x = {0.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0};
  std::vector<double> y = storage(6);
  std::vector<double> s = {0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
  std::vector<double> xp = storage(6);
  std::vector<double> yp = storage(6);

  const auto result = xfoil::geometry::findLeftmostPoint(
      x, xp, y, yp, s, n);

  CHECK(!result.success);
  CHECK_NEAR(result.parameter, 4.0, 0.0, 0.0);
}

TEST(unit, FindLeftmostPointUsesSharpEdgeFallback) {
  // first sign change for i = 3 with duplicate parameter value
  // |s[3] - s[2]| < 1e-6: sharp edge, discrete parameter value.
  const int n = 6;
  std::vector<double> x = {0.0, 5.0, 3.0, 1.0, 1.5, 3.0, 4.0};
  std::vector<double> y = storage(6);
  std::vector<double> s = {0.0, 0.0, 1.0, 1.0, 2.0, 3.0, 4.0};
  std::vector<double> xp = storage(6);
  std::vector<double> yp = storage(6);

  const auto result = xfoil::geometry::findLeftmostPoint(
      x, xp, y, yp, s, n);

  CHECK(result.success);
  CHECK_NEAR(result.parameter, 1.0, 0.0, 0.0);
}

// ---
// Sort and remove near duplicates (sortol)
// ---

TEST(unit, SortAndRemoveNearDuplicatesSortsPairsTogether) {
  const int n = 3;
  std::vector<double> first = {0.0, 3.0, 1.0, 2.0};
  std::vector<double> second = {0.0, 30.0, 10.0, 20.0};
  int pointCount = n;

  CHECK(xfoil::geometry::sortAndRemoveNearDuplicates(
      1e-9, pointCount, first, second));

  CHECK_EQ(pointCount, 3);
  CHECK_NEAR(first[1], 1.0, 0.0, 0.0);
  CHECK_NEAR(first[2], 2.0, 0.0, 0.0);
  CHECK_NEAR(first[3], 3.0, 0.0, 0.0);
  // the value pairs must move together.
  CHECK_NEAR(second[1], 10.0, 0.0, 0.0);
  CHECK_NEAR(second[2], 20.0, 0.0, 0.0);
  CHECK_NEAR(second[3], 30.0, 0.0, 0.0);
}

TEST(unit, SortAndRemoveNearDuplicatesRemovesClosePair) {
  const int n = 4;
  std::vector<double> first = {0.0, 1.0, 2.0, 2.0001, 5.0};
  std::vector<double> second = {0.0, 10.0, 20.0, 20.001, 50.0};
  int pointCount = n;

  CHECK(xfoil::geometry::sortAndRemoveNearDuplicates(
      0.01, pointCount, first, second));

  CHECK_EQ(pointCount, 3);
  CHECK_NEAR(first[1], 1.0, 0.0, 0.0);
  CHECK_NEAR(first[2], 2.0, 0.0, 0.0);
  CHECK_NEAR(first[3], 5.0, 0.0, 0.0);
  CHECK_NEAR(second[1], 10.0, 0.0, 0.0);
  CHECK_NEAR(second[2], 20.0, 0.0, 0.0);
  CHECK_NEAR(second[3], 50.0, 0.0, 0.0);
}

TEST(unit, SortAndRemoveNearDuplicatesRepeatsUntilStable) {
  // Chain of nearby points: 1, 1.001, 1.002 - duplicates are removed repeatedly
  // until no adjacent duplicates are found.
  const int n = 4;
  std::vector<double> first = {0.0, 1.0, 1.001, 1.002, 10.0};
  std::vector<double> second = storage(4);
  int pointCount = n;

  CHECK(xfoil::geometry::sortAndRemoveNearDuplicates(
      0.01, pointCount, first, second));

  CHECK_EQ(pointCount, 2);
  CHECK_NEAR(first[1], 1.0, 0.0, 0.0);
  CHECK_NEAR(first[2], 10.0, 0.0, 0.0);
}

TEST(unit, SortAndRemoveNearDuplicatesRejectsNegativeTolerance) {
  const int n = 3;
  std::vector<double> first = {0.0, 3.0, 1.0, 2.0};
  std::vector<double> second = {0.0, 30.0, 10.0, 20.0};
  int pointCount = n;

  CHECK_FALSE(xfoil::geometry::sortAndRemoveNearDuplicates(
      -1.0, pointCount, first, second));

  // Inputs remain unchanged.
  CHECK_EQ(pointCount, 3);
  CHECK_NEAR(first[1], 3.0, 0.0, 0.0);
  CHECK_NEAR(first[2], 1.0, 0.0, 0.0);
  CHECK_NEAR(first[3], 2.0, 0.0, 0.0);
}

TEST(unit, SortAndRemoveNearDuplicatesRejectsShortSpan) {
  // span size pointCount instead of pointCount + 1.
  const int n = 3;
  std::vector<double> first = {0.0, 3.0, 1.0, 2.0};
  std::vector<double> second = storage(2);
  int pointCount = n;

  CHECK_FALSE(xfoil::geometry::sortAndRemoveNearDuplicates(
      0.01, pointCount, first, second));
}

// End: tests/unit/GeometryAlgorithmsTests.cpp

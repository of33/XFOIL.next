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

// File: tests/unit/SplineTests.cpp
// tests for the stateless spline modules implementing the spline routines of fortran-src/spline.f.
// all test data use the documented 1-based
// span convention with reserved index 0; active indices are 1..n.

#include "TestSupport.h"

#include <cmath>
#include <span>
#include <vector>

#include "xfoil/math/Spline.h"

namespace spline = xfoil::math::spline;

namespace {

// Produces a vector with reserved index 0 (value 0.0) and fills the
// active indices 1..n with the values from values1Based (count n).
std::vector<double> makeOneBased(const double* values1Based, int n) {
  std::vector<double> result(static_cast<std::size_t>(n) + 1, 0.0);
  for (int i = 1; i <= n; i++) {
    result[static_cast<std::size_t>(i)] = values1Based[i - 1];
  }
  return result;
}

bool allFinite(std::span<const double> values) {
  for (const double value : values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

// linear reference data: parameter[i] = i, values[i] = 3*i - 7, i = 1..5.
const double kLinearParameter[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
const double kLinearValues[5] = {-4.0, -1.0, 2.0, 5.0, 8.0};

}  // namespace

TEST(unit, ArcLengthOfStraightPolyline) {
  const double xData[3] = {0.0, 1.0, 1.0};
  const double yData[3] = {0.0, 0.0, 2.0};
  auto x = makeOneBased(xData, 3);
  auto y = makeOneBased(yData, 3);
  std::vector<double> s(4, -1.0);

  CHECK(spline::calculateArcLength(x, y, s, 3));
  CHECK_NEAR(s[1], 0.0, 1e-15, 0.0);
  CHECK_NEAR(s[2], 1.0, 1e-15, 0.0);
  CHECK_NEAR(s[3], 3.0, 1e-15, 0.0);
}

TEST(unit, AveragedDerivativesOfLinearFunction) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::averagedDerivatives(values, derivatives, parameter, 5));
  for (int i = 1; i <= 5; i++) {
    CHECK_NEAR(derivatives[static_cast<std::size_t>(i)], 3.0, 1e-14, 0.0);
  }
}

TEST(unit, CubicSplineReproducesLinearFunction) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(values, derivatives, parameter, 5,
                                       -999.0, -999.0));
  for (int i = 1; i <= 5; i++) {
    CHECK_NEAR(derivatives[static_cast<std::size_t>(i)], 3.0, 1e-14, 0.0);
  }

  // Evaluation at several interior positions: x(s) = 3*s - 7.
  for (const double ss : {1.25, 2.0, 2.5, 3.5, 4.75}) {
    const double value = spline::evaluate(ss, values, derivatives, parameter, 5);
    CHECK_NEAR(value, 3.0 * ss - 7.0, 1e-12, 0.0);
  }
}

TEST(unit, SplineFirstDerivativeOfLinearFunction) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(values, derivatives, parameter, 5,
                                       -999.0, -999.0));
  for (const double ss : {1.25, 2.5, 3.5, 4.75}) {
    const double d =
        spline::firstDerivative(ss, values, derivatives, parameter, 5);
    CHECK_NEAR(d, 3.0, 1e-12, 0.0);
  }
}

TEST(unit, CubicSplineHonorsSpecifiedEndpointDerivatives) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(
    values, derivatives, parameter, 5, 3.0, 3.0));

  CHECK_NEAR(derivatives[1], 3.0, 1e-14, 0.0);
  CHECK_NEAR(derivatives[5], 3.0, 1e-14, 0.0);

  for (int i = 1; i <= 5; ++i) {
    CHECK_NEAR(
      derivatives[static_cast<std::size_t>(i)],
               3.0,
               1e-13,
               0.0);
  }
}

TEST(unit, CubicSplineSupportsNaturalEndpointConditions) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(
    values, derivatives, parameter, 5, 999.0, 999.0));

  for (int i = 1; i <= 5; ++i) {
    CHECK_NEAR(
      derivatives[static_cast<std::size_t>(i)],
               3.0,
               1e-13,
               0.0);
  }

  CHECK_NEAR(
    spline::secondDerivative(
      1.0, values, derivatives, parameter, 5),
      0.0,
      1e-12,
      0.0);

  CHECK_NEAR(
    spline::secondDerivative(
      5.0, values, derivatives, parameter, 5),
      0.0,
      1e-12,
      0.0);
}

TEST(unit, CubicSplineHandlesTwoPointFortranDerivativeCase) {
  const double parameterData[2] = {1.0, 3.0};
  const double valueData[2] = {2.0, 8.0};

  auto parameter = makeOneBased(parameterData, 2);
  auto values = makeOneBased(valueData, 2);
  std::vector<double> derivatives(3, 0.0);

  CHECK(spline::cubicSplineDerivatives(
    values, derivatives, parameter, 2, -999.0, -999.0));

  // slope of the line: (8 - 2) / (3 - 1) = 3.
  CHECK_NEAR(derivatives[1], 3.0, 1e-14, 0.0);
  CHECK_NEAR(derivatives[2], 3.0, 1e-14, 0.0);

  CHECK_NEAR(
    spline::evaluate(
      2.0, values, derivatives, parameter, 2),
      5.0,
      1e-14,
      0.0);
}

TEST(unit, SplineSecondDerivativeOfLinearFunction) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(values, derivatives, parameter, 5,
                                       -999.0, -999.0));
  for (const double ss : {1.25, 2.5, 3.5, 4.75}) {
    const double dd =
        spline::secondDerivative(ss, values, derivatives, parameter, 5);
    CHECK_NEAR(dd, 0.0, 1e-12, 0.0);
  }
}

TEST(unit, SegmentedSplineAcceptsInnerDuplicateParameter) {
  // interior duplicate parameter value (s[2] == s[3]) separates two segments.
  const double sData[4] = {1.0, 2.0, 2.0, 3.0};
  const double xData[4] = {1.0, 2.0, 2.0, 3.0};
  auto s = makeOneBased(sData, 4);
  auto x = makeOneBased(xData, 4);
  std::vector<double> xs(5, 0.0);

  CHECK(spline::segmentedSplineDerivatives(x, xs, s, 4));
  CHECK(allFinite(xs));

  // both segments contain a linear function with slope 1;
  // the boundary condition -999/-999 returns exactly that slope for two points.
  for (int i = 1; i <= 4; i++) {
    CHECK_NEAR(xs[static_cast<std::size_t>(i)], 1.0, 1e-12, 0.0);
  }

  // Comparable segspld variant with explicit end derivatives.
  std::vector<double> xsd(5, 0.0);
  CHECK(spline::segmentedSplineDerivativesWithEndpoints(x, xsd, s, 4, -999.0,
                                                        -999.0));
  for (int i = 1; i <= 4; i++) {
    CHECK_NEAR(xsd[static_cast<std::size_t>(i)], 1.0, 1e-12, 0.0);
  }
}

TEST(unit, SegmentedSplineUsesExplicitDerivativesOnlyAtGlobalEnds) {
  // XFOIL 6.996 SEGSPLD: explicit end derivatives apply only at the
  // global first/last point. The interior joint uses zero third derivative.
  const double sData[5] = {0.0, 1.0, 2.0, 2.0, 3.0};
  const double xData[5] = {0.0, 1.0, 1.0, 1.0, 2.0};
  auto s = makeOneBased(sData, 5);
  auto x = makeOneBased(xData, 5);
  std::vector<double> xs(6, 0.0);

  CHECK(spline::segmentedSplineDerivativesWithEndpoints(x, xs, s, 5, 2.0,
                                                        12.0));
  const double expected[5] = {2.0, 1.0 / 3.0, -1.0 / 3.0, -10.0, 12.0};
  for (int i = 1; i <= 5; i++) {
    CHECK_NEAR(xs[static_cast<std::size_t>(i)], expected[i - 1], 1e-12, 0.0);
  }

  // The exact joint selects the right segment's one-sided derivative.
  CHECK_NEAR(spline::firstDerivative(2.0, x, xs, s, 5), -10.0, 1e-12, 0.0);
  CHECK_NEAR(spline::firstDerivative(1.999999, x, xs, s, 5),
             -0.3333326666666666, 1e-6, 0.0);
  CHECK_NEAR(spline::firstDerivative(2.000001, x, xs, s, 5),
             -9.999977999999999, 1e-6, 0.0);
  CHECK_NEAR(spline::evaluate(2.0, x, xs, s, 5), 1.0, 1e-12, 0.0);
  CHECK_NEAR(spline::secondDerivative(2.0, x, xs, s, 5), 22.0, 1e-12, 0.0);

  // Non-segmented control: both explicit endpoint derivatives are used.
  const double sControlData[4] = {0.0, 1.0, 2.0, 3.0};
  const double xControlData[4] = {0.0, 1.0, 1.5, 2.5};
  auto sControl = makeOneBased(sControlData, 4);
  auto xControl = makeOneBased(xControlData, 4);
  std::vector<double> xsControl(5, 0.0);
  CHECK(spline::segmentedSplineDerivativesWithEndpoints(
      xControl, xsControl, sControl, 4, 2.0, 12.0));
  const double expectedControl[4] = {2.0, 7.0 / 6.0, -13.0 / 6.0, 12.0};
  for (int i = 1; i <= 4; i++) {
    CHECK_NEAR(xsControl[static_cast<std::size_t>(i)], expectedControl[i - 1],
               1e-12, 0.0);
  }
}

TEST(unit, SegmentedSplineWithEndpointsRejectsWithoutMutation) {
  // The bool API rejects invalid sizes and duplicated global ends before
  // solving any segment; the derivative span remains untouched.
  const double tinySData[1] = {0.0};
  const double tinyXData[1] = {1.0};
  auto tinyS = makeOneBased(tinySData, 1);
  auto tinyX = makeOneBased(tinyXData, 1);
  std::vector<double> tinyOut(2, 42.0);
  CHECK_FALSE(spline::segmentedSplineDerivativesWithEndpoints(
      tinyX, tinyOut, tinyS, 1, 2.0, 12.0));
  CHECK_EQ(tinyOut[1], 42.0);

  const double firstDupSData[3] = {0.0, 0.0, 1.0};
  const double firstDupXData[3] = {0.0, 1.0, 2.0};
  auto firstDupS = makeOneBased(firstDupSData, 3);
  auto firstDupX = makeOneBased(firstDupXData, 3);
  std::vector<double> firstDupOut(4, 7.0);
  CHECK_FALSE(spline::segmentedSplineDerivativesWithEndpoints(
      firstDupX, firstDupOut, firstDupS, 3, 2.0, 12.0));
  for (int i = 1; i <= 3; i++) {
    CHECK_EQ(firstDupOut[static_cast<std::size_t>(i)], 7.0);
  }

  const double lastDupSData[3] = {0.0, 1.0, 1.0};
  const double lastDupXData[3] = {0.0, 1.0, 2.0};
  auto lastDupS = makeOneBased(lastDupSData, 3);
  auto lastDupX = makeOneBased(lastDupXData, 3);
  std::vector<double> lastDupOut(4, 9.0);
  CHECK_FALSE(spline::segmentedSplineDerivativesWithEndpoints(
      lastDupX, lastDupOut, lastDupS, 3, 2.0, 12.0));
  for (int i = 1; i <= 3; i++) {
    CHECK_EQ(lastDupOut[static_cast<std::size_t>(i)], 9.0);
  }
}

TEST(unit, SegmentedSplineRejectsDuplicatedFirstPoint) {
  const double sData[4] = {1.0, 1.0, 2.0, 3.0};
  const double xData[4] = {0.0, 1.0, 2.0, 3.0};
  auto s = makeOneBased(sData, 4);
  auto x = makeOneBased(xData, 4);
  std::vector<double> xs(5, 0.0);

  CHECK_FALSE(spline::segmentedSplineDerivatives(x, xs, s, 4));
  CHECK_FALSE(spline::segmentedSplineDerivativesWithEndpoints(x, xs, s, 4,
                                                              -999.0, -999.0));
}

TEST(unit, SegmentedSplineRejectsDuplicatedLastPoint) {
  const double sData[4] = {1.0, 2.0, 3.0, 3.0};
  const double xData[4] = {0.0, 1.0, 2.0, 3.0};
  auto s = makeOneBased(sData, 4);
  auto x = makeOneBased(xData, 4);
  std::vector<double> xs(5, 0.0);

  CHECK_FALSE(spline::segmentedSplineDerivatives(x, xs, s, 4));
  CHECK_FALSE(spline::segmentedSplineDerivativesWithEndpoints(x, xs, s, 4,
                                                              -999.0, -999.0));
}

TEST(unit, SplineEvaluationChoosesExpectedIntervalAtKnot) {
  // Nonlinear data: the second derivative is exact at an interior node
  // only if the interval search
  // selects the upper interval [k, k + 1] with t = 0.
  const double sData[4] = {0.0, 1.0, 3.0, 6.0};
  const double xData[4] = {0.0, 1.0, 2.0, 3.0};
  auto s = makeOneBased(sData, 4);
  auto x = makeOneBased(xData, 4);
  std::vector<double> xs(5, 0.0);

  CHECK(spline::cubicSplineDerivatives(x, xs, s, 4, -999.0, -999.0));

  // Hand-computed solution of the 4x4 system (rows: [1 1 0 0], [2 6 1 0],
  // [0 3 10 2], [0 0 1 1], rhs [2, 7.5, 6.5, 2/3]) exactly as fractions /174:
  CHECK_NEAR(xs[1], 211.0 / 174.0, 1e-12, 0.0);
  CHECK_NEAR(xs[2], 137.0 / 174.0, 1e-12, 0.0);
  CHECK_NEAR(xs[3], 61.0 / 174.0, 1e-12, 0.0);
  CHECK_NEAR(xs[4], 55.0 / 174.0, 1e-12, 0.0);

  // value and first derivative exact at the interior node s[2] = 1:
  // hits the support value or xs[2] (upper interval bound, t = 0).
  CHECK_NEAR(spline::evaluate(1.0, x, xs, s, 4), 1.0, 1e-15, 0.0);
  CHECK_NEAR(spline::firstDerivative(1.0, x, xs, s, 4), xs[2], 1e-15, 0.0);

  // interval selection at the node is checked uniquely with constructed
  // (deliberately not spline-consistent) derivatives: a true spline
  // is C2-continuous, so both sides would be equal; with arbitrary
  // derivatives the second derivative differs exactly.
  const double xsData[4] = {1.0, 0.5, 1.0, 2.0};
  auto xd = makeOneBased(xsData, 4);

  // Selected, as in D2VAL, is the upper interval [2, 3] with t = 0:
  // ds = 2, cx1 = 2*0.5 - 2 + 1 = 0, cx2 = 2*1 - 2 + 1 = 1,
  // d2val = (-4*0 - 2*1)/4 = -0.5 (exactly representable).
  CHECK_NEAR(spline::secondDerivative(1.0, x, xd, s, 4), -0.5, 1e-15, 0.0);

  // the lower interval [1, 2] for t = 1 would yield a different value:
  // ds = 1, cx1 = 1*1 - 1 + 0 = 0, cx2 = 1*0.5 - 1 + 0 = -0.5,
  // d2val = (2*0 + 4*(-0.5))/1 = -2.
  CHECK(std::fabs(spline::secondDerivative(1.0, x, xd, s, 4) - (-2.0)) >
        1.0e-3);
}

TEST(unit, SplineInversionFindsLinearParameter) {
  auto values = makeOneBased(kLinearValues, 5);
  auto parameter = makeOneBased(kLinearParameter, 5);
  std::vector<double> derivatives(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(values, derivatives, parameter, 5,
                                       -999.0, -999.0));

  double si = 2.0;
  CHECK(spline::invert(si, 2.0, values, derivatives, parameter, 5));
  CHECK_NEAR(si, 3.0, 1e-12, 0.0);
}

TEST(unit, SplineInversionRestoresInitialGuessOnFailure) {
  // Constant-value airfoil: the Newton derivative is exactly 0. On
  // supported IEEE-754 platforms this yields non-finite
  // intermediate values; after 10 failed iterations the function must
  // restore the original starting value. Robust handling of
  // vanishing derivatives is not part of this extraction.
  const double valueData[3] = {1.0, 1.0, 1.0};
  const double sData[3] = {1.0, 2.0, 3.0};
  auto values = makeOneBased(valueData, 3);
  auto parameter = makeOneBased(sData, 3);
  std::vector<double> derivatives(4, 0.0);

  const double initialGuess = 2.5;
  double si = initialGuess;
  CHECK_FALSE(spline::invert(si, 7.0, values, derivatives, parameter, 3));
  CHECK_EQ(si, initialGuess);
}

TEST(unit, CurvatureOfStraightLineIsZero) {
  const double sData[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
  const double xData[5] = {-4.0, -1.0, 2.0, 5.0, 8.0};
  const double yData[5] = {2.0, 4.0, 6.0, 8.0, 10.0};
  auto s = makeOneBased(sData, 5);
  auto x = makeOneBased(xData, 5);
  auto y = makeOneBased(yData, 5);
  std::vector<double> xs(6, 0.0);
  std::vector<double> ys(6, 0.0);

  CHECK(spline::cubicSplineDerivatives(x, xs, s, 5, -999.0, -999.0));
  CHECK(spline::cubicSplineDerivatives(y, ys, s, 5, -999.0, -999.0));

  for (const double ss : {1.5, 2.5, 3.5, 4.5}) {
    const double crv = spline::curvature(ss, x, xs, y, ys, s, 5);
    CHECK_NEAR(crv, 0.0, 1e-12, 0.0);
  }
}

TEST(unit, TridiagonalSolverMatchesKnownSolution) {
  // Small 3x3 system with known solution (extraction
 // equivalence).
  std::vector<double> a = {0.0, 2.0, 2.0, 2.0};
  std::vector<double> b = {0.0, 0.0, -1.0, -1.0};
  std::vector<double> c = {0.0, -1.0, -1.0, 0.0};
  std::vector<double> d = {0.0, 1.0, -2.0, 3.0};
  const std::vector<double> bBefore = b;

  CHECK(spline::solveTridiagonalInPlace(a, b, c, d, 3));

  // Solves 2*x1 - x2 = 1, -x1 + 2*x2 - x3 = -2, -x2 + 2*x3 = 3
  // with the known solution (0.5, 0, 1.5).
  CHECK_NEAR(d[1], 0.5, 1e-12, 0.0);
  CHECK_NEAR(d[2], 0.0, 1e-12, 0.0);
  CHECK_NEAR(d[3], 1.5, 1e-12, 0.0);

  // the lower diagonal stays unchanged.
  for (std::size_t i = 0; i < bBefore.size(); i++) {
    CHECK_EQ(b[i], bBefore[i]);
  }
}

TEST(unit, SplineFunctionsRejectTooSmallSpans) {
  // pointCount = 4, but the spans contain only 4 instead of 5 elements.
  std::vector<double> values(4, 1.0);
  std::vector<double> derivatives(4, 0.0);
  std::vector<double> parameter(4, 0.0);

  CHECK_FALSE(spline::calculateArcLength(values, values, derivatives, 4));
  CHECK_FALSE(spline::averagedDerivatives(values, derivatives, parameter, 4));
  CHECK_FALSE(spline::cubicSplineDerivatives(values, derivatives, parameter, 4,
                                             -999.0, -999.0));
  CHECK_FALSE(spline::segmentedSplineDerivatives(values, derivatives,
                                                 parameter, 4));
  CHECK_FALSE(spline::segmentedSplineDerivativesWithEndpoints(
      values, derivatives, parameter, 4, -999.0, -999.0));

  double si = 1.0;
  CHECK_FALSE(
      spline::invert(si, 0.5, values, derivatives, parameter, 4));

  // pointCount < 2 is invalid for all bool functions.
  std::vector<double> okValues(3, 1.0);
  std::vector<double> okDerivatives(3, 0.0);
  std::vector<double> okParameter(3, 0.0);
  CHECK_FALSE(spline::calculateArcLength(okValues, okValues, okDerivatives, 1));
  CHECK_FALSE(spline::averagedDerivatives(okValues, okDerivatives, okParameter,
                                          1));
  CHECK_FALSE(spline::cubicSplineDerivatives(okValues, okDerivatives,
                                             okParameter, 1, -999.0, -999.0));

  // invalid sizes in the solver.
  std::vector<double> small(1, 0.0);
  CHECK_FALSE(spline::solveTridiagonalInPlace(small, small, small, small, 3));
  CHECK_FALSE(spline::solveTridiagonalInPlace(values, values, values, values,
                                              0));
}

TEST(unit, CubicSplineRejectsBeyondFortranWorkspace) {
  // pointCount > 600 (the module's fixed workspace limit) returns
  // false. Dynamic vectors in the test avoid huge stack allocation.
  const int n = 601;
  std::vector<double> values(static_cast<std::size_t>(n) + 1, 0.0);
  std::vector<double> derivatives(static_cast<std::size_t>(n) + 1, 0.0);
  std::vector<double> parameter(static_cast<std::size_t>(n) + 1, 0.0);

  for (int i = 1; i <= n; i++) {
    const double index = static_cast<double>(i);
    values[static_cast<std::size_t>(i)] = index;
    parameter[static_cast<std::size_t>(i)] = index;
  }

  CHECK_FALSE(spline::cubicSplineDerivatives(values, derivatives, parameter, n,
                                             -999.0, -999.0));

  // pointCount == 600 still lies within the valid workspace.
  CHECK(spline::cubicSplineDerivatives(values, derivatives, parameter, 600,
                                       -999.0, -999.0));
}

// end: tests/unit/SplineTests.cpp

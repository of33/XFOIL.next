// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

// File: src/core/math/Spline.cpp
// Stateless spline and curve functions. The algorithms
// follow the formulas and operation order of the SCALC, SPLINA, SPLIND,
// SEGSPL, SEGSPLD, SEVAL, DEVAL, D2VAL, CURV, SINVRT and TRISOL routines
// in fortran-src/spline.f;
// 1-based indexing with reserved index 0 is preserved
// (see the convention comment in Spline.h).

#include "xfoil/math/Spline.h"

#include "xfoil/math/LinearAlgebra.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>

namespace xfoil::math::spline {
namespace {

// Fixed workspace limit for the spline-coefficient arrays.
constexpr int kMaxSplinePoints = 600;

// 1-based element access on spans and fixed arrays: the domain
// indices are restricted to 1..pointCount by the size check,
// the conversion to size_type is deliberate and lossless here.
template <class Container>
[[nodiscard]] auto& atOneBased(Container& container, int index) noexcept {
  return container[static_cast<std::size_t>(index)];
}

// Exact adoption of the shared binary interval search of SEVAL, DEVAL,
// D2VAL and CURV in fortran-src/spline.f (partly goto-based there).
// Returns the upper index of the interval [i - 1, i]; the loop
// corresponds to those Fortran search loops in structured while form.
std::size_t findInterval(double parameterValue,
                         std::span<const double> parameter,
                         int pointCount) noexcept {
  int lower = 1;
  int upper = pointCount;

  while (upper - lower > 1) {
    const int middle = (upper + lower) / 2;
    if (parameterValue < atOneBased(parameter, middle)) {
      upper = middle;
    } else {
      lower = middle;
    }
  }

  return static_cast<std::size_t>(upper);
}

// Checks the 1-based span convention: index 0 reserved, active values
// in 1..pointCount, therefore at least pointCount + 1 elements and
// pointCount >= 2.
template <class T>
bool hasRequiredSize(std::span<T> values, int pointCount) noexcept {
  return pointCount >= 2 &&
         values.size() >= static_cast<std::size_t>(pointCount) + 1;
}

template <class... Spans>
bool allHaveRequiredSize(int pointCount, Spans... spans) noexcept {
  return (hasRequiredSize(spans, pointCount) && ...);
}

// Calls cubicSplineDerivatives on a segment. The subspan begins at the
  // original index segmentStart - 1 and contains segmentCount + 1
// elements, so that subspan[1] == values[segmentStart] (corresponds to
// the previous call to splind(x + iseg0 - 1, xs + iseg0 - 1,
// s + iseg0 - 1, ...)).
bool splineSegment(std::span<const double> values,
                   std::span<double> derivatives,
                   std::span<const double> parameter, int segmentStart,
                   int segmentCount, double firstEndpointDerivative,
                   double lastEndpointDerivative) {
  const auto offset = static_cast<std::size_t>(segmentStart - 1);
  const auto length = static_cast<std::size_t>(segmentCount) + 1;

  return cubicSplineDerivatives(values.subspan(offset, length),
                                derivatives.subspan(offset, length),
                                parameter.subspan(offset, length), segmentCount,
                                firstEndpointDerivative,
                                lastEndpointDerivative);
}

}  // namespace

bool calculateArcLength(std::span<const double> x, std::span<const double> y,
                        std::span<double> arcLength, int pointCount) noexcept {
  if (!allHaveRequiredSize(pointCount, x, y, arcLength)) {
    return false;
  }

  atOneBased(arcLength, 1) = 0.0;
  for (int i = 2; i <= pointCount; i++) {
    atOneBased(arcLength, i) =
        atOneBased(arcLength, i - 1) +
        std::sqrt((atOneBased(x, i) - atOneBased(x, i - 1)) *
                      (atOneBased(x, i) - atOneBased(x, i - 1)) +
                  (atOneBased(y, i) - atOneBased(y, i - 1)) *
                      (atOneBased(y, i) - atOneBased(y, i - 1)));
  }
  return true;
}

bool averagedDerivatives(std::span<const double> values,
                         std::span<double> derivatives,
                         std::span<const double> parameter,
                         int pointCount) noexcept {
  if (!allHaveRequiredSize(pointCount, values, derivatives, parameter)) {
    return false;
  }

  double xs1 = 0.0;
  double xs2 = 0.0;
  bool lend = true;

  for (int i = 1; i <= pointCount - 1; i++) {
    const double ds = atOneBased(parameter, i + 1) - atOneBased(parameter, i);
    if (std::fabs(ds) < 1.e-10) {  //=0.0
      atOneBased(derivatives, i) = xs1;
      lend = true;
    } else {
      const double dx = atOneBased(values, i + 1) - atOneBased(values, i);
      xs2 = dx / ds;
      if (lend) {
        atOneBased(derivatives, i) = xs2;
        lend = false;
      } else {
        atOneBased(derivatives, i) = 0.5 * (xs1 + xs2);
      }
    }
    xs1 = xs2;
  }
  atOneBased(derivatives, pointCount) = xs1;
  return true;
}

bool cubicSplineDerivatives(std::span<const double> values,
                            std::span<double> derivatives,
                            std::span<const double> parameter, int pointCount,
                            double firstEndpointDerivative,
                            double lastEndpointDerivative) {
  if (!allHaveRequiredSize(pointCount, values, derivatives, parameter)) {
    return false;
  }
  if (pointCount > kMaxSplinePoints) {
    return false;
  }

  std::array<double, 601> a{};
  std::array<double, 601> b{};
  std::array<double, 601> c{};

  double dsm = 0.0;
  double dsp = 0.0;

  for (int i = 2; i <= pointCount - 1; i++) {
    dsm = atOneBased(parameter, i) - atOneBased(parameter, i - 1);
    dsp = atOneBased(parameter, i + 1) - atOneBased(parameter, i);
    atOneBased(b, i) = dsp;
    atOneBased(a, i) = 2.0 * (dsm + dsp);
    atOneBased(c, i) = dsm;
    atOneBased(derivatives, i) =
        3.0 * ((atOneBased(values, i + 1) - atOneBased(values, i)) * dsm / dsp +
               (atOneBased(values, i) - atOneBased(values, i - 1)) * dsp / dsm);
  }

  if (firstEndpointDerivative >= 998.0) {
    // --- set zero second derivative end condition
    atOneBased(a, 1) = 2.0;
    atOneBased(c, 1) = 1.0;
    atOneBased(derivatives, 1) =
        3.0 * (values[2] - values[1]) / (parameter[2] - parameter[1]);
  } else {
    if (firstEndpointDerivative <= -998.0) {
      // --- set zero third derivative end condition
      atOneBased(a, 1) = 1.0;
      atOneBased(c, 1) = 1.0;
      atOneBased(derivatives, 1) =
          2.0 * (values[2] - values[1]) / (parameter[2] - parameter[1]);
    } else {
      // --- set specified first derivative end condition
      atOneBased(a, 1) = 1.0;
      atOneBased(c, 1) = 0.0;
      atOneBased(derivatives, 1) = firstEndpointDerivative;
    }
  }

  if (lastEndpointDerivative >= 998.0) {
    atOneBased(b, pointCount) = 1.0;
    atOneBased(a, pointCount) = 2.0;
    atOneBased(derivatives, pointCount) =
        3.0 * (atOneBased(values, pointCount) -
               atOneBased(values, pointCount - 1)) /
        (atOneBased(parameter, pointCount) -
         atOneBased(parameter, pointCount - 1));
  } else {
    if (lastEndpointDerivative <= -998.0) {
      atOneBased(b, pointCount) = 1.0;
      atOneBased(a, pointCount) = 1.0;
      atOneBased(derivatives, pointCount) =
          2.0 * (atOneBased(values, pointCount) -
                 atOneBased(values, pointCount - 1)) /
          (atOneBased(parameter, pointCount) -
           atOneBased(parameter, pointCount - 1));
    } else {
      atOneBased(a, pointCount) = 1.0;
      atOneBased(b, pointCount) = 0.0;
      atOneBased(derivatives, pointCount) = lastEndpointDerivative;
    }
  }

  if (pointCount == 2 && firstEndpointDerivative <= -998.0 &&
      lastEndpointDerivative <= -998.0) {
    atOneBased(b, pointCount) = 1.0;
    atOneBased(a, pointCount) = 2.0;
    atOneBased(derivatives, pointCount) =
        3.0 * (atOneBased(values, pointCount) -
               atOneBased(values, pointCount - 1)) /
        (atOneBased(parameter, pointCount) -
         atOneBased(parameter, pointCount - 1));
  }

  // --- solve for derivative array (diagonal a, lower b, upper c)
  return solveTridiagonalInPlace(std::span<double>(a.data(), a.size()),
                                 std::span<double>(b.data(), b.size()),
                                 std::span<double>(c.data(), c.size()),
                                 derivatives, pointCount);
}

bool segmentedSplineDerivatives(std::span<const double> values,
                                std::span<double> derivatives,
                                std::span<const double> parameter,
                                int pointCount) {
  return segmentedSplineDerivativesWithEndpoints(values, derivatives, parameter,
                                                 pointCount, -999.0, -999.0);
}

bool segmentedSplineDerivativesWithEndpoints(
    std::span<const double> values, std::span<double> derivatives,
    std::span<const double> parameter, int pointCount,
    double firstEndpointDerivative, double lastEndpointDerivative) {
  if (!allHaveRequiredSize(pointCount, values, derivatives, parameter)) {
    return false;
  }

  if (parameter[1] == parameter[2]) {
    return false;  // stop 'segspl: first input point duplicated'
  }
  if (atOneBased(parameter, pointCount) ==
      atOneBased(parameter, pointCount - 1)) {
    return false;  // stop 'segspl: last input point duplicated'
  }

  int iseg0 = 1;
  for (int iseg = 2; iseg <= pointCount - 2; iseg++) {
    if (atOneBased(parameter, iseg) == atOneBased(parameter, iseg + 1)) {
      const int nseg = iseg - iseg0 + 1;
      // XFOIL 6.996: explicit end derivatives apply only at the global
      // first/last point; internal segment joints use the zero-third
      // derivative condition.
      double firstSegmentDerivative = -999.0;
      double lastSegmentDerivative = -999.0;
      if (iseg0 == 1) {
        firstSegmentDerivative = firstEndpointDerivative;
      }
      if (iseg0 + nseg - 1 == pointCount) {
        lastSegmentDerivative = lastEndpointDerivative;
      }
      if (!splineSegment(values, derivatives, parameter, iseg0, nseg,
                         firstSegmentDerivative, lastSegmentDerivative)) {
        return false;
      }
      iseg0 = iseg + 1;
    }
  }
  const int nseg = pointCount - iseg0 + 1;

  double firstSegmentDerivative = -999.0;
  double lastSegmentDerivative = -999.0;
  if (iseg0 == 1) {
    firstSegmentDerivative = firstEndpointDerivative;
  }
  if (iseg0 + nseg - 1 == pointCount) {
    lastSegmentDerivative = lastEndpointDerivative;
  }
  if (!splineSegment(values, derivatives, parameter, iseg0, nseg,
                     firstSegmentDerivative, lastSegmentDerivative)) {
    return false;
  }

  return true;
}

double evaluate(double parameterValue, std::span<const double> values,
                std::span<const double> derivatives,
                std::span<const double> parameter,
                int pointCount) noexcept {
  assert(allHaveRequiredSize(pointCount, values, derivatives, parameter));

  const std::size_t i = findInterval(parameterValue, parameter, pointCount);

  const double ds = parameter[i] - parameter[i - 1];
  const double t = (parameterValue - parameter[i - 1]) / ds;
  const double cx1 = ds * derivatives[i - 1] - values[i] + values[i - 1];
  const double cx2 = ds * derivatives[i] - values[i] + values[i - 1];
  return t * values[i] + (1.0 - t) * values[i - 1] +
         (t - t * t) * ((1.0 - t) * cx1 - t * cx2);
}

double firstDerivative(double parameterValue, std::span<const double> values,
                       std::span<const double> derivatives,
                       std::span<const double> parameter,
                       int pointCount) noexcept {
  assert(allHaveRequiredSize(pointCount, values, derivatives, parameter));

  const std::size_t i = findInterval(parameterValue, parameter, pointCount);

  const double ds = parameter[i] - parameter[i - 1];
  const double t = (parameterValue - parameter[i - 1]) / ds;
  const double cx1 = ds * derivatives[i - 1] - values[i] + values[i - 1];
  const double cx2 = ds * derivatives[i] - values[i] + values[i - 1];
  double result =
      values[i] - values[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cx1 +
      t * (3.0 * t - 2.0) * cx2;
  result = result / ds;
  return result;
}

double secondDerivative(double parameterValue, std::span<const double> values,
                        std::span<const double> derivatives,
                        std::span<const double> parameter,
                        int pointCount) noexcept {
  assert(allHaveRequiredSize(pointCount, values, derivatives, parameter));

  const std::size_t i = findInterval(parameterValue, parameter, pointCount);

  const double ds = parameter[i] - parameter[i - 1];
  const double t = (parameterValue - parameter[i - 1]) / ds;
  const double cx1 = ds * derivatives[i - 1] - values[i] + values[i - 1];
  const double cx2 = ds * derivatives[i] - values[i] + values[i - 1];
  double result = (6.0 * t - 4.0) * cx1 + (6.0 * t - 2.0) * cx2;
  result = result / ds / ds;
  return result;
}

double curvature(double parameterValue, std::span<const double> x,
                 std::span<const double> xDerivatives,
                 std::span<const double> y,
                 std::span<const double> yDerivatives,
                 std::span<const double> parameter,
                 int pointCount) noexcept {
  assert(allHaveRequiredSize(pointCount, x, xDerivatives, y, yDerivatives,
                             parameter));

  const std::size_t i = findInterval(parameterValue, parameter, pointCount);

  const double ds = parameter[i] - parameter[i - 1];
  const double t = (parameterValue - parameter[i - 1]) / ds;

  const double cx1 = ds * xDerivatives[i - 1] - x[i] + x[i - 1];
  const double cx2 = ds * xDerivatives[i] - x[i] + x[i - 1];
  const double xd = x[i] - x[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cx1 +
                    t * (3.0 * t - 2.0) * cx2;
  const double xdd = (6.0 * t - 4.0) * cx1 + (6.0 * t - 2.0) * cx2;

  const double cy1 = ds * yDerivatives[i - 1] - y[i] + y[i - 1];
  const double cy2 = ds * yDerivatives[i] - y[i] + y[i - 1];
  const double yd = y[i] - y[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cy1 +
                    t * (3.0 * t - 2.0) * cy2;
  const double ydd = (6.0 * t - 4.0) * cy1 + (6.0 * t - 2.0) * cy2;

  double sd = std::sqrt(xd * xd + yd * yd);
  sd = std::max(sd, 0.001 * ds);

  return (xd * ydd - yd * xdd) / sd / sd / sd;
}

bool invert(double& parameterValue, double targetValue,
            std::span<const double> values,
            std::span<const double> derivatives,
            std::span<const double> parameter, int pointCount) noexcept {
  if (!allHaveRequiredSize(pointCount, values, derivatives, parameter)) {
    return false;
  }

  const double savedValue = parameterValue;

  for (int iter = 1; iter <= 10; iter++) {
    const double res = evaluate(parameterValue, values, derivatives, parameter,
                                pointCount) -
                       targetValue;
    const double resp = firstDerivative(parameterValue, values, derivatives,
                                        parameter, pointCount);
    const double ds = -res / resp;
    parameterValue = parameterValue + ds;
    if (std::fabs(ds / (atOneBased(parameter, pointCount) - parameter[1])) <
        1.0e-5) {
      return true;
    }
  }

  parameterValue = savedValue;
  return false;
}

// Compatibility wrapper: the canonical implementation of the
// tridiagonal solver lives in
// xfoil::math::linear_algebra::solveTridiagonalInPlace. This function remains
// callable unchanged; the formula exists only
// once (in the linear-algebra module). Behavior: false for invalid
// span sizes as before; additionally false for an exactly zero pivot
// (newly detected in the module; not observable on regular paths).
bool solveTridiagonalInPlace(std::span<double> diagonal,
                             std::span<double> lower, std::span<double> upper,
                             std::span<double> rightHandSide,
                             int equationCount) noexcept {
  return xfoil::math::linear_algebra::solveTridiagonalInPlace(
             diagonal, lower, upper, rightHandSide, equationCount) ==
         xfoil::math::linear_algebra::SolveStatus::success;
}

}  // namespace xfoil::math::spline

// End: src/core/math/Spline.cpp

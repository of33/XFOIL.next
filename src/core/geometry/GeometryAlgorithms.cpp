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

// File: src/core/geometry/GeometryAlgorithms.cpp
// Stateless geometry algorithms. See
// include/xfoil/geometry/GeometryAlgorithms.h for the API- and
// convention documentation.

#include "xfoil/geometry/GeometryAlgorithms.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "xfoil/math/Spline.h"

namespace xfoil::geometry {

namespace {

// 1-based element access on spans: the domain indices are
// restricted to the valid range by the size checks; the
// conversion to size_type is deliberate and lossless here
// (convention as in src/core/math/Spline.cpp).
template <class Container>
[[nodiscard]] auto& atOneBased(Container& container, int index) noexcept {
  return container[static_cast<std::size_t>(index)];
}

// Exact Pi value adopted unchanged by these kernels.
constexpr double kPi = 3.141592654;

[[nodiscard]]
bool hasUsableSpans(
    std::size_t firstSize,
    std::size_t secondSize,
    int pointCount) noexcept {
  if (pointCount < 0) {
    return false;
  }
  const auto required = static_cast<std::size_t>(pointCount) + 1;
  return firstSize >= required && secondSize >= required;
}

}  // namespace

CornerAngleResult findMaximumCornerAngle(
    std::span<const double> x,
    std::span<const double> y,
    int pointCount) noexcept {
  CornerAngleResult result;

  if (pointCount < 3 || !hasUsableSpans(x.size(), y.size(), pointCount)) {
    return result;
  }

  // Initial values and loop structure adopted unchanged.
  double angle = 0.0;
  int index = 1;

  // --- go over each point, calculating corner angle
  for (int i = 2; i <= pointCount - 1; i++) {
    double dx1 = atOneBased(x, i) - atOneBased(x, i - 1);
    double dy1 = atOneBased(y, i) - atOneBased(y, i - 1);
    double dx2 = atOneBased(x, i) - atOneBased(x, i + 1);
    double dy2 = atOneBased(y, i) - atOneBased(y, i + 1);

    // --- allow for doubled points
    if (dx1 == 0.0 && dy1 == 0.0) {
      // CANG (fortran-src/xgeom.f:1111) would read the reserved index 0 for i == 2.
      // The new API does not treat index 0 as a domain data point and
      // rejects this pathological edge case in a controlled way.
      if (i - 2 < 1) {
        return {};
      }

      dx1 = atOneBased(x, i) - atOneBased(x, i - 2);
      dy1 = atOneBased(y, i) - atOneBased(y, i - 2);
    }
    if (dx2 == 0.0 && dy2 == 0.0) {
      // CANG would access i + 2 here. This indexing
      // lies outside the valid range only for i == pointCount - 1;
      // instead of an out-of-bounds access, a controlled
      // success == false is provided.
      if (i + 2 > pointCount) {
        return {};
      }
      dx2 = atOneBased(x, i) - atOneBased(x, i + 2);
      dy2 = atOneBased(y, i) - atOneBased(y, i + 2);
    }

    const double crossp = (dx2 * dy1 - dy2 * dx1) /
                          std::sqrt((dx1 * dx1 + dy1 * dy1) *
                                    (dx2 * dx2 + dy2 * dy2));
    // The asin argument is not clamped.
    const double angleAtPoint = std::asin(crossp) * (180.0 / kPi);
    if (std::fabs(angleAtPoint) > std::fabs(angle)) {
      angle = angleAtPoint;
      index = i;
    }
  }

  result.success = true;
  result.pointIndex = index;
  result.angleDegrees = angle;
  return result;
}

bool containsPoint(
    std::span<const double> x,
    std::span<const double> y,
    int pointCount,
    double pointX,
    double pointY) noexcept {
  if (pointCount < 3 || !hasUsableSpans(x.size(), y.size(), pointCount)) {
    return false;
  }

  // --- integ/ybrate: subtended angle around the airfoil perimeter; yb
  // loop, cyclic successor and summation formula exactly as in INSIDE (fortran-src/xgdes.f:1852).
  double angle = 0.0;
  for (int i = 1; i <= pointCount; i++) {
    int ip = i + 1;
    if (i == pointCount) ip = 1;

    const double xb1 = atOneBased(x, i) - pointX;
    const double yb1 = atOneBased(y, i) - pointY;
    const double xb2 = atOneBased(x, ip) - pointX;
    const double yb2 = atOneBased(y, ip) - pointY;
    angle = angle + (xb1 * yb2 - yb1 * xb2) /
                    std::sqrt((xb1 * xb1 + yb1 * yb1) *
                              (xb2 * xb2 + yb2 * yb2));
  }

  // --- angle = 0 if xf,yf is outside, angle = +/- 2 pi if xf,yf is inside
  return (std::fabs(angle) > 1.0);
}

bool calculateNormals(
    std::span<const double> x,
    std::span<const double> y,
    std::span<const double> parameter,
    std::span<double> normalX,
    std::span<double> normalY,
    int pointCount) {
  // Guard: n <= 1 returns false.
  if (pointCount <= 1) {
    return false;
  }

  // Size checks happen inside segmentedSplineDerivatives
  // (all spans at least pointCount + 1). If the spline computation
  // returns false, false is returned immediately; no computation
  // continues with incomplete derivatives. The output may then
  // already be partially changed (documented in the header).
  if (!xfoil::math::spline::segmentedSplineDerivatives(
          x, normalX, parameter, pointCount)) {
    return false;
  }
  if (!xfoil::math::spline::segmentedSplineDerivatives(
          y, normalY, parameter, pointCount)) {
    return false;
  }

  for (int i = 1; i <= pointCount; i++) {
    const double sx = atOneBased(normalY, i);
    const double sy = -atOneBased(normalX, i);
    const double smod = std::sqrt(sx * sx + sy * sy);
    // New controlled error path: exactly zero-dimensional or non-finite
    // tangents are rejected instead of dividing by zero as in NCALC.
    // No substitute normal is produced.
    if (smod == 0.0 || !std::isfinite(smod)) {
      return false;
    }
    atOneBased(normalX, i) = sx / smod;
    atOneBased(normalY, i) = sy / smod;
  }

  // --- average normal vectors at corner points
  for (int i = 1; i <= pointCount - 1; i++) {
    if (atOneBased(parameter, i) == atOneBased(parameter, i + 1)) {
      const double sx = 0.5 * (atOneBased(normalX, i) + atOneBased(normalX, i + 1));
      const double sy = 0.5 * (atOneBased(normalY, i) + atOneBased(normalY, i + 1));
      const double smod = std::sqrt(sx * sx + sy * sy);
      if (smod == 0.0 || !std::isfinite(smod)) {
        return false;
      }
      atOneBased(normalX, i) = sx / smod;
      atOneBased(normalY, i) = sy / smod;
      atOneBased(normalX, i + 1) = sx / smod;
      atOneBased(normalY, i + 1) = sy / smod;
    }
  }

  return true;
}

LeadingEdgeResult findLeadingEdge(
    std::span<const double> x,
    std::span<const double> xDerivatives,
    std::span<const double> y,
    std::span<const double> yDerivatives,
    std::span<const double> parameter,
    int pointCount) noexcept {
  using Limits = std::numeric_limits<double>;

  LeadingEdgeResult result;
  // NaN signal: the Newton loop never ran; xle/yle remain unset at this
  // point. See the header documentation.
  result.leadingEdgeX = Limits::quiet_NaN();
  result.leadingEdgeY = Limits::quiet_NaN();

  if (pointCount < 5 ||
      !hasUsableSpans(x.size(), xDerivatives.size(), pointCount) ||
      !hasUsableSpans(y.size(), yDerivatives.size(), pointCount) ||
      parameter.size() < static_cast<std::size_t>(pointCount) + 1) {
    return result;
  }

  // --- convergence tolerance
  const double dseps = (atOneBased(parameter, pointCount) - atOneBased(parameter, 1)) * 0.00001;

  // --- set trailing edge point coordinates
  const double xte = 0.5 * (atOneBased(x, 1) + atOneBased(x, pointCount));
  const double yte = 0.5 * (atOneBased(y, 1) + atOneBased(y, pointCount));
  result.trailingEdgeX = xte;
  result.trailingEdgeY = yte;

  // --- get first guess for sle
  int i = 3;
  bool bracketFound = false;
  for (; i <= pointCount - 2; i++) {
    const double dxte = atOneBased(x, i) - xte;
    const double dyte = atOneBased(y, i) - yte;
    const double dx = atOneBased(x, i + 1) - atOneBased(x, i);
    const double dy = atOneBased(y, i + 1) - atOneBased(y, i);
    const double dotp = dxte * dx + dyte * dy;
    if (dotp < 0.0) {
      bracketFound = true;
      break;
    }
  }

  // No start index found: controlled failure without using
  // the last loop index (deviation from the Fortran LEFIND
  // fallthrough, which would have continued with s[n - 1]).
  if (!bracketFound) {
    return result;
  }

  double sle = atOneBased(parameter, i);

  // --- check for sharp le case
  if (atOneBased(parameter, i) == atOneBased(parameter, i - 1)) {
    result.parameter = sle;
    return result;
  }

  // --- Newton iteration to get exact sle value
  for (int iter = 1; iter <= 50; iter++) {
    const double xle = xfoil::math::spline::evaluate(
        sle, x, xDerivatives, parameter, pointCount);
    const double yle = xfoil::math::spline::evaluate(
        sle, y, yDerivatives, parameter, pointCount);
    const double dxds = xfoil::math::spline::firstDerivative(
        sle, x, xDerivatives, parameter, pointCount);
    const double dyds = xfoil::math::spline::firstDerivative(
        sle, y, yDerivatives, parameter, pointCount);
    const double dxdd = xfoil::math::spline::secondDerivative(
        sle, x, xDerivatives, parameter, pointCount);
    const double dydd = xfoil::math::spline::secondDerivative(
        sle, y, yDerivatives, parameter, pointCount);

    const double xchord = xle - xte;
    const double ychord = yle - yte;

    // --- drive dot product between chord line and le tangent to zero
    const double res = xchord * dxds + ychord * dyds;
    const double ress = dxds * dxds + dyds * dyds + xchord * dxdd +
                        ychord * dydd;

    // --- Newton delta for sle
    double dsle = -res / ress;

    dsle = std::max(dsle, -0.02 * std::fabs(xchord + ychord));
    dsle = std::min(dsle, 0.02 * std::fabs(xchord + ychord));
    sle = sle + dsle;

    // The returned values correspond to the last evaluation,
    // also for non-convergence.
    result.leadingEdgeX = xle;
    result.leadingEdgeY = yle;

    if (std::fabs(dsle) < dseps) {
      result.success = true;
      result.parameter = sle;
      return result;
    }
  }

  // Newton iteration did not converge: fall back to the
  // discrete initial value and still return true.
  // leadingEdgeX/Y further contain
  // the values from the last Newton evaluation.
  result.success = true;
  result.parameter = atOneBased(parameter, i);
  return result;
}

LeftmostPointResult findLeftmostPoint(
    std::span<const double> x,
    std::span<const double> xDerivatives,
    std::span<const double> y,
    std::span<const double> yDerivatives,
    std::span<const double> parameter,
    int pointCount) noexcept {
  LeftmostPointResult result;

  if (pointCount < 5 ||
      !hasUsableSpans(x.size(), xDerivatives.size(), pointCount) ||
      !hasUsableSpans(y.size(), yDerivatives.size(), pointCount) ||
      parameter.size() < static_cast<std::size_t>(pointCount) + 1) {
    return result;
  }

  const double dslen = atOneBased(parameter, pointCount) - atOneBased(parameter, 1);

  // --- convergence tolerance
  const double dseps = (atOneBased(parameter, pointCount) - atOneBased(parameter, 1)) * 0.00001;

  // --- get first guess for sle
  int i = 3;
  bool bracketFound = false;
  for (; i <= pointCount - 2; i++) {
    const double dx = atOneBased(x, i + 1) - atOneBased(x, i);
    if (dx > 0.0) {
      bracketFound = true;
      break;
    }
  }

  if (!bracketFound) {
    // No valid switch found: controlled failure without
    // uninitialized index access. The parameter corresponds to the value
    // the XLFIND fallthrough would have used (atOneBased(parameter, pointCount - 1)).
    result.success = false;
    result.parameter = atOneBased(parameter, pointCount - 1);
    return result;
  }

  double sle = atOneBased(parameter, i);

  // --- check for sharp le case
  if (std::fabs(atOneBased(parameter, i) - atOneBased(parameter, i - 1)) < 1.e-06) {
    result.success = true;
    result.parameter = sle;
    return result;
  }

  // --- Newton iteration to get exact sle value
  for (int iter = 1; iter <= 50; iter++) {
    const double dxds = xfoil::math::spline::firstDerivative(
        sle, x, xDerivatives, parameter, pointCount);
    const double dxdd = xfoil::math::spline::secondDerivative(
        sle, x, xDerivatives, parameter, pointCount);

    // --- drive dxds to zero
    const double res = dxds;
    const double ress = dxdd;

    // --- Newton delta for sle
    double dsle = -res / ress;

    dsle = std::max(dsle, -0.01 * std::fabs(dslen));
    dsle = std::min(dsle, 0.01 * std::fabs(dslen));
    sle += dsle;
    if (std::fabs(dsle) < dseps) {
      result.success = true;
      result.parameter = sle;
      return result;
    }
  }

  // Newton non-convergence: discrete fallback value,
  // success == false. The caller adopts the value
  // independently of the status.
  result.success = false;
  result.parameter = atOneBased(parameter, i);
  return result;
}

bool sortAndRemoveNearDuplicates(
    double tolerance,
    int& pointCount,
    std::span<double> first,
    std::span<double> second) noexcept {
  if (pointCount < 1 ||
      !hasUsableSpans(first.size(), second.size(), pointCount) ||
      !std::isfinite(tolerance) || tolerance < 0.0) {
    return false;
  }

  // --- sort arrays
  bool done = false;
  for (int ipass = 1; ipass <= 1234; ipass++) {
    done = true;
    for (int n = 1; n <= pointCount - 1; n++) {
      const int np = n + 1;

      if (atOneBased(first, np) < atOneBased(first, n)) {
        double temp = atOneBased(first, np);
        atOneBased(first, np) = atOneBased(first, n);
        atOneBased(first, n) = temp;
        temp = atOneBased(second, np);
        atOneBased(second, np) = atOneBased(second, n);
        atOneBased(second, n) = temp;
        done = false;
      }
    }
    if (done) break;
  }

  // --- search for near-duplicate pairs and eliminate extra points
  // --- modified 4/24/01 hhy to check list until all duplicates removed
  //	this cures a bug for sharp le foils where there were 3 le points in
  //	camber, thickness lists from getcam.
  done = false;
  while (!done) {
    const int kks = pointCount;
    done = true;
    for (int k = 1; k <= kks; k++) {
      if (k < pointCount) {  // go to 20
        const double dFirst = atOneBased(first, k) - atOneBased(first, k + 1);
        const double dSecond = atOneBased(second, k) - atOneBased(second, k + 1);
        const double dsq = dFirst * dFirst + dSecond * dSecond;
        if (dsq < tolerance * tolerance) {  // go to 20
          // --- eliminate extra point pairs
          pointCount = pointCount - 1;
          for (int kt = k + 1; kt <= pointCount; kt++) {
            atOneBased(first, kt) = atOneBased(first, kt + 1);
            atOneBased(second, kt) = atOneBased(second, kt + 1);
          }
          done = false;
        }
      }
    }
  }  // 20 continue

  return true;
}

}  // namespace xfoil::geometry

// End: src/core/geometry/GeometryAlgorithms.cpp

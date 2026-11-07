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

// File: include/xfoil/geometry/GeometryAlgorithms.h
// Stateless geometry algorithms implementing the CANG, LEFIND and
// XLFIND routines of fortran-src/xgeom.f, the INSIDE function of
// fortran-src/xgdes.f, the NCALC routine of fortran-src/xpanel.f and
// the SORTOL routine of fortran-src/sort.f. Algorithms, constants,
// and operation orders are adopted unchanged.
//
// 1-based span convention:
// all numerical spans contain a reserved index 0. Domain-specific
// active points lie at indices 1..pointCount. Every required
// span must therefore have at least pointCount + 1 elements. For an
// invalid count or too small a point count the functions report
// controlled failure (success == false or false, respectively); no access
// happens outside the passed spans.
//
// The API uses no raw pointers and stores no spans beyond the
// call. Errors are reported exclusively through the result status
// or the bool return value; there is no central
// error-message infrastructure and no output.

#pragma once

#include <span>

namespace xfoil::geometry {

struct CornerAngleResult {
  bool success{};
  int pointIndex{1};
  double angleDegrees{};
};

struct LeadingEdgeResult {
  bool success{};
  double parameter{};
  double leadingEdgeX{};
  double leadingEdgeY{};
  double trailingEdgeX{};
  double trailingEdgeY{};
};

struct LeftmostPointResult {
  bool success{};
  double parameter{};
};

// Implements CANG (fortran-src/xgeom.f:1111): largest bend angle (by magnitude) of the
// polyline x(1..pointCount), y(1..pointCount). Initial values:
// angle 0.0 for index 1, loop over i = 2..pointCount - 1,
// duplicate points are resolved via neighbors i-2 or i+2.
// Requires pointCount >= 3 and spans of at least
// pointCount + 1 elements. If a duplicate point lies so close to the edge that
// the formula would use an index outside the domain-specific active
// range 1..pointCount, the function returns controlled success == false.
// This affects in particular the accesses to the reserved
// index 0 for a duplicated first segment and to
// pointCount + 1 for a duplicated last segment.
[[nodiscard]]
CornerAngleResult findMaximumCornerAngle(
    std::span<const double> x,
    std::span<const double> y,
    int pointCount) noexcept;

// Implements the INSIDE function (fortran-src/xgdes.f:1852): true if the point (pointX, pointY) lies inside
// the closed contour x(1..pointCount), y(1..pointCount).
// Adopted formula: summed cross-product/sine terms
// over all segments with a cyclic successor, return value
// |angle| > 1.0. False for pointCount < 3 or too-short spans. Points
// exactly on a node or segment are not reliably
// classified by the formula; no new semantics are introduced for this.
[[nodiscard]]
bool containsPoint(
    std::span<const double> x,
    std::span<const double> y,
    int pointCount,
    double pointX,
    double pointY) noexcept;

// Implements NCALC (fortran-src/xpanel.f:51): normalized surface normals of the curve
// (x, y)(parameter 1..pointCount). The derivatives are computed with
// xfoil::math::spline::segmentedSplineDerivatives (x derivative
// into normalX, y derivative into normalY), then normalized exactly as in
// NCALC via (sx, sy) = (normalY[i], -normalX[i]) and averaged at
// interior duplicated parameter points.
//
// The function works in place in normalX/normalY; on an error the
// output may already be partially changed. If the
// spline computation returns false, false is returned immediately. New
// controlled error path (documented): if the norm is exactly 0.0 or
// not finite, false is returned instead of dividing by
// zero; no substitute normal is produced.
[[nodiscard]]
bool calculateNormals(
    std::span<const double> x,
    std::span<const double> y,
    std::span<const double> parameter,
    std::span<double> normalX,
    std::span<double> normalY,
    int pointCount);

// Implements LEFIND (fortran-src/xgeom.f:21): spline parameter of the leading-edge point,
// defined by (x - xte, y - yte) . (x', y') = 0 with the
// trailing-edge midpoint (xte, yte) = ((x[1] + x[n]) / 2,
// (y[1] + y[n]) / 2). Adopted: tolerance 0.00001 * (s[n] - s[1]),
// start-index search from 3 to pointCount - 2 via the first
// sign change of the dot product, at most 50 Newton iterations with
// unchanged residual/derivative formula and step limit
// +-0.02 * |xchord + ychord|, success for |dsle| < tolerance, fallback to
// the discrete initial value.
//
// Requires pointCount >= 5 and spans of at least
// pointCount + 1 elements; otherwise success == false. The trailing-edge values are
// set as soon as they are computable. If the search finds no valid
// start index, controlled success == false is returned, without
// using the last loop index. A sharp leading edge
// (parameter[i] == parameter[i - 1]) returns success == false with the
// discrete initial value. leadingEdgeX/leadingEdgeY contain the values
// from the last Newton evaluation (also for
// non-convergence); NaN if the Newton loop never ran
// (sharp leading edge or no start index). Callers can use
// std::isnan to detect that these values were not set.
[[nodiscard]]
LeadingEdgeResult findLeadingEdge(
    std::span<const double> x,
    std::span<const double> xDerivatives,
    std::span<const double> y,
    std::span<const double> yDerivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements XLFIND (fortran-src/xgeom.f:91): spline parameter of the leftmost point
// (x' = 0, vertical tangent). Adopted: parameterLength = s[n] - s[1],
// convergence tolerance 0.00001 * parameterLength, start search from index 3 to
// pointCount - 2 via the first x[i + 1] - x[i] > 0, sharp-LE tolerance
// 1.e-06 versus parameter[i - 1], at most 50 Newton iterations with
// step limit +-0.01 * |parameterLength|, and fallback to the
// discrete initial value. Success semantics: regular convergence and a detected
// sharp leading edge yield success == true (for a sharp LE with a
// discrete parameter value); Newton non-convergence returns success == false
// with the discrete fallback value; if no start index is found,
// success == false (no uninitialized index access), and the parameter
// corresponds to the XLFIND value parameter[pointCount - 1]. The
// y values and y derivatives are part of the signature, may be unused
// in the existing formula, and are kept for
// signature stability. Requires pointCount >= 5 and spans of
// at least pointCount + 1 elements; otherwise success == false with
// parameter == 0.0.
// The meaning of success stays compatible with findLeadingEdge above:
// regular Newton convergence and Newton non-convergence with the
// discrete fallback return true. A sharp leading edge, invalid inputs,
// or a missing start range return false. The field therefore does not
// distinguish Newton convergence from the non-convergence fallback.
[[nodiscard]]
LeftmostPointResult findLeftmostPoint(
    std::span<const double> x,
    std::span<const double> xDerivatives,
    std::span<const double> y,
    std::span<const double> yDerivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements SORTOL (fortran-src/sort.f:221): bubble sort of the value pairs (first, second)
// by first with synchronized swapping of both arrays (at most 1234
// passes), followed by repeated removal of nearby duplicates
// (squared distance < tol * tol) until none are found; pointCount
// is adapted by reference. The function removes only elements and
// does not increase the point count. Preconditions: pointCount >= 1, both
// spans at least pointCount + 1 elements, tolerance finite and not
// negative; otherwise false with unchanged inputs preferred.
[[nodiscard]]
bool sortAndRemoveNearDuplicates(
    double tolerance,
    int& pointCount,
    std::span<double> first,
    std::span<double> second) noexcept;

}  // namespace xfoil::geometry

// End: include/xfoil/geometry/GeometryAlgorithms.h

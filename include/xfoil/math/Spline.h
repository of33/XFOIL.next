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

// File: include/xfoil/math/Spline.h
// Stateless spline and curve functions implementing the SCALC, SPLINA,
// SPLIND, SEGSPL, SEGSPLD, SEVAL, DEVAL, D2VAL, CURV, SINVRT and TRISOL
// routines of fortran-src/spline.f. Algorithms, constants, and operation
// orders are adopted unchanged.
//
// 1-based span convention:
// all spans contain the reserved index 0. Domain-specific active values
// lie in the indices 1..pointCount. Every required span must therefore
// contain at least pointCount + 1 elements. For an invalid count
// or pointCount < 2 the bool functions return false; the scalar
// evaluation functions check this via assert in debug builds.

#pragma once

#include <span>

namespace xfoil::math::spline {

// Implements SCALC (fortran-src/spline.f:436): computes the arc-length array s (index 0
// reserved, s[1] = 0). Returns false for invalid span sizes.
[[nodiscard]]
bool calculateArcLength(
    std::span<const double> x,
    std::span<const double> y,
    std::span<double> arcLength,
    int pointCount) noexcept;

// Implements SPLINA (fortran-src/spline.f:141): averaging of the segment slopes. Returns
// false for invalid span sizes.
[[nodiscard]]
bool averagedDerivatives(
    std::span<const double> values,
    std::span<double> derivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements SPLIND (fortran-src/spline.f:63): cubic spline derivatives
// with specified or special boundary conditions (998/-998/999/-999
// convention). Returns false for invalid span sizes, pointCount < 2 or
// pointCount > 600 (the module's fixed workspace limit).
[[nodiscard]]
bool cubicSplineDerivatives(
    std::span<const double> values,
    std::span<double> derivatives,
    std::span<const double> parameter,
    int pointCount,
    double firstEndpointDerivative,
    double lastEndpointDerivative);

// Implements SEGSPL (fortran-src/spline.f:533): piecewise splines with -999.0/-999.0
// boundary conditions; segments are separated at identical consecutive
// parameter values. False for a duplicated first/last point or
// invalid span sizes.
[[nodiscard]]
bool segmentedSplineDerivatives(
    std::span<const double> values,
    std::span<double> derivatives,
    std::span<const double> parameter,
    int pointCount);

// Implements SEGSPLD (fortran-src/spline.f:562): as segmentedSplineDerivatives with
// specified end derivatives at the global first/last point. Internal segment
// joints use the zero-third-derivative condition as in XFOIL 6.996.
[[nodiscard]]
bool segmentedSplineDerivativesWithEndpoints(
    std::span<const double> values,
    std::span<double> derivatives,
    std::span<const double> parameter,
    int pointCount,
    double firstEndpointDerivative,
    double lastEndpointDerivative);

// Implements SEVAL (fortran-src/spline.f:218): evaluates value x(parameterValue).
[[nodiscard]]
double evaluate(
    double parameterValue,
    std::span<const double> values,
    std::span<const double> derivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements DEVAL (fortran-src/spline.f:245): first derivative dx/ds(parameterValue).
[[nodiscard]]
double firstDerivative(
    double parameterValue,
    std::span<const double> values,
    std::span<const double> derivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements D2VAL (fortran-src/spline.f:273): second derivative d2x/ds2(parameterValue).
[[nodiscard]]
double secondDerivative(
    double parameterValue,
    std::span<const double> values,
    std::span<const double> derivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements CURV (fortran-src/spline.f:302): curvature of the parameterized curve (x,y)(s).
[[nodiscard]]
double curvature(
    double parameterValue,
    std::span<const double> x,
    std::span<const double> xDerivatives,
    std::span<const double> y,
    std::span<const double> yDerivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Implements SINVRT (fortran-src/spline.f:404): Newton inversion s(xi) with si as initial value.
// Returns false for invalid span sizes. On failure (after
// 10 iterations) the original value of parameterValue is
// restored and false returned.
[[nodiscard]]
bool invert(
    double& parameterValue,
    double targetValue,
    std::span<const double> values,
    std::span<const double> derivatives,
    std::span<const double> parameter,
    int pointCount) noexcept;

// Forwarding wrapper onto
// xfoil::math::linear_algebra::solveTridiagonalInPlace. Implements the
// TRISOL tridiagonal solve (fortran-src/spline.f:185): diagonal, upper
// and rightHandSide are changed,
// lower stays unchanged. False for invalid span sizes and for an
// exactly zero pivot (checked in the linear-algebra module; no
// near-zero tolerance).
[[nodiscard]]
bool solveTridiagonalInPlace(
    std::span<double> diagonal,
    std::span<double> lower,
    std::span<double> upper,
    std::span<double> rightHandSide,
    int equationCount) noexcept;

}  // namespace xfoil::math::spline

// End: include/xfoil/math/Spline.h

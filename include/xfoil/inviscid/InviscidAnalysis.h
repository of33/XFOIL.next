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

// File: include/xfoil/inviscid/InviscidAnalysis.h
// Stateless inviscid compute kernels implementing the COMSET, CPCALC
// and CLCALC routines of fortran-src/xfoil.f and the QISET routine of
// fortran-src/xpanel.f.
// Formulas, constants, and operation orders are adopted
// unchanged.
//
// 1-based span convention:
// all numerical spans contain a reserved index 0. Domain-specific
// active values lie in the indices 1..pointCount. Every required
// span must therefore have at least pointCount + 1 elements. For an
// invalid count the functions report controlled invalidDimension or
// invalidInput; no access happens outside the passed
// spans.
//
// The API contains no raw pointers, stores no spans beyond the
// call, and allocates no heap memory. The header is
// independent of XFoil.h. Errors are reported exclusively via the
// result structures; there is no output.

#pragma once

#include <span>

namespace xfoil::inviscid {

enum class InviscidStatus {
  success,
  invalidDimension,
  invalidInput,
  compressibilityFailure
};

// Mirrors the outputs of COMSET (fortran-src/xfoil.f:1029): Karman-Tsien parameter
// tklam (karmanTsienParameter), its derivative w.r.t. minf^2 (tkl_msq),
// sonic pressure coefficient (cpstar) and speed of sound (qstar).
struct CompressibilityState {
  InviscidStatus status{InviscidStatus::invalidInput};
  double karmanTsienParameter{};
  double karmanTsienMachDerivative{};
  double sonicPressureCoefficient{};
  double sonicSpeed{};
};

// Mirrors the behavior of CPCALC (fortran-src/xfoil.f:1057): the pressure coefficient distribution
// is written; denominatorNonPositive signals at least one
// non-positive denominator (status compressibilityFailure).
struct PressureCoefficientResult {
  InviscidStatus status{InviscidStatus::invalidInput};
  bool denominatorNonPositive{};
};

// Freestream state for the inviscid sub-steps.
struct FreestreamState {
  double alphaRadians{};
  double speed{1.0};
  double mach{};
};

// Mirrors the outputs of QISET (fortran-src/xpanel.f:1597): cosa (cosineAlpha) and
// sina (sineAlpha); qinv and qinv_a are written to the output
// spans.
struct SurfaceVelocityResult {
  InviscidStatus status{InviscidStatus::invalidInput};
  double cosineAlpha{};
  double sineAlpha{};
};

// Mirrors the outputs of CLCALC (fortran-src/xfoil.f:1088): cl, cdp, cm, xcp, cl_alf
// and cl_msq.
struct PressureLoadResult {
  InviscidStatus status{InviscidStatus::invalidInput};
  double liftCoefficient{};
  double pressureDragCoefficient{};
  double momentCoefficient{};
  double centerOfPressureX{};
  double liftAlphaDerivative{};
  double liftMachSquaredDerivative{};
};

// Implements COMSET (fortran-src/xfoil.f:1029): sets the Karman-Tsien compressibility
// parameter from Mach number, freestream velocity, and gas constants.
// Adopted formulas (operation order unchanged):
// beta = sqrt(1 - Mach^2)
// beta_msq = -0.5 / beta
// tklam = Mach^2 / (1 + beta)^2
// tkl_msq = 1/(1+beta)^2 - 2*tklam/(1+beta)*beta_msq
// Case Mach == 0: cpstar = -999, qstar = 999 (constants unchanged).
// Input validation (new controlled error path invalidInput): all
// inputs finite, freeStreamSpeed != 0, gammaMinusOne != 0, and
// Mach^2 < 1. The Mach number is not clamped and the existing
// MRCL limit is not changed. On success status == success.
[[nodiscard]]
CompressibilityState calculateCompressibilityState(
    double mach,
    double freeStreamSpeed,
    double gamma,
    double gammaMinusOne) noexcept;

// Implements CPCALC (fortran-src/xfoil.f:1057): computes the compressible pressure coefficient from the
// velocity distribution. Adopted formula (operation order
// unchanged):
// beta = sqrt(1 - Mach^2)
// bfac = 0.5 * Mach^2 / (1 + beta)
// cpinc = 1 - (q/qinf)^2
// the = beta + bfac*cpinc
// cp = cpinc / the
// Index convention: index 0 reserved, active values 1..pointCount, both
// spans at least pointCount + 1 elements.
// Validation before the first mutation: pointCount >= 1 and sufficient
// span sizes, otherwise invalidDimension; freeStreamSpeed == 0, non-finite
// Mach number/free-stream velocity/velocities, or
// Mach^2 >= 1, otherwise invalidInput.
// Behavior for a non-positive denominator as in CPCALC: all
// points are still computed, denominatorNonPositive is set to true,
// and finally compressibilityFailure is returned (no exception).
[[nodiscard]]
PressureCoefficientResult calculatePressureCoefficients(
    std::span<const double> velocity,
    std::span<double> pressureCoefficient,
    int pointCount,
    double freeStreamSpeed,
    double mach) noexcept;

// Row-span representation of the unit vortex distributions qinvu: each
// row contains components [0] (reserved), [1], and [2]. This maps
// the two-dimensional storage without flattening. It
// owns no data and exposes no raw pointer in the API.
using UnitVelocityRows = std::span<std::span<const double>>;

// Implements QISET (fortran-src/xpanel.f:1597): overlays the two unit distributions into
// qinv and qinv_a. Adopted formula (operation order unchanged):
// cosine = cos(alphaRadians)
// sine = sin(alphaRadians)
// qinv[i] = cosine*unitVelocity[i][1] + sine*unitVelocity[i][2]
// qinv_a[i] = -sine*unitVelocity[i][1] + cosine*unitVelocity[i][2]
// Dimensions: pointCount >= 1, rows 0..pointCount present (at least
// pointCount + 1 rows), each active row at least three values (columns
// 1 and 2), output vectors at least pointCount + 1. Invalid
// dimensions yield invalidDimension; non-finite alphaRadians or
// unit values yield invalidInput; otherwise success. The return value contains
// cosineAlpha and sineAlpha (cosa, sina). For non-finite
// intermediate results during the computation, the outputs may be
// partially changed; for regular inputs the behavior stays unchanged.
[[nodiscard]]
SurfaceVelocityResult combineUnitVelocityDistributions(
    double alphaRadians,
    UnitVelocityRows unitVelocity,
    std::span<double> velocity,
    std::span<double> alphaDerivative,
    int pointCount) noexcept;

// Implements CLCALC (fortran-src/xfoil.f:1088): integrates the surface pressure
// coefficients of the contour 1..pointCount (cyclic successor of the
// last point is 1) into cl, cdp, cm, xcp and cl_alf and cl_msq. The
// summation/operation order is adopted unchanged: beta, beta_msq, bfac,
// bfac_msq, cginc,
// cpg1/cpg2 with Mach-squared and alpha derivatives, transformed
// segment differences (dx, dy), moment integration with /12.0 terms,
// pressure drag, xcp accumulation and final division of xcp by cl
// (case cl == 0: xcp = 0). No vector/cross-product helper functions, no
// loop fusion.
// Index convention: index 0 reserved, active points 1..pointCount.
// Validation before computation: pointCount >= 2 and sufficient span sizes
// otherwise invalidDimension; non-finite inputs, freeStreamSpeed == 0 or
// Mach^2 >= 1 otherwise invalidInput. Return value by value (no output parameter).
[[nodiscard]]
PressureLoadResult integratePressureLoads(
    std::span<const double> x,
    std::span<const double> y,
    std::span<const double> circulation,
    std::span<const double> circulationAlphaDerivative,
    int pointCount,
    double alphaRadians,
    double freeStreamSpeed,
    double mach,
    double referenceX,
    double referenceY) noexcept;

}  // namespace xfoil::inviscid

// End: include/xfoil/inviscid/InviscidAnalysis.h

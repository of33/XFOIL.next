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

// File: src/core/inviscid/InviscidAnalysis.cpp
// Stateless inviscid compute kernels.
//

#include "xfoil/inviscid/InviscidAnalysis.h"

#include <cmath>

namespace xfoil::inviscid {

CompressibilityState calculateCompressibilityState(
    double mach,
    double freeStreamSpeed,
    double gamma,
    double gammaMinusOne) noexcept {
  CompressibilityState result;

  // Input validation (documented error path invalidInput). The Mach
  // number is deliberately not clamped.
  if (!std::isfinite(mach) || !std::isfinite(freeStreamSpeed) ||
    !std::isfinite(gamma) || !std::isfinite(gammaMinusOne) ||
    freeStreamSpeed == 0.0 || gamma == 0.0 ||
    gammaMinusOne == 0.0 || !(mach * mach < 1.0)) {
      result.status = InviscidStatus::invalidInput;
      return result;
  }

  // --- set the Karman-Tsien parameter tklam (formula and order as in
  // COMSET, fortran-src/xfoil.f:1029)
  const double beta = std::sqrt(1.0 - mach * mach);
  const double beta_msq = -0.5 / beta;

  result.karmanTsienParameter =
      mach * mach / (1.0 + beta) / (1.0 + beta);
  result.karmanTsienMachDerivative =
      1.0 / (1.0 + beta) / (1.0 + beta) -
      2.0 * result.karmanTsienParameter / (1.0 + beta) * beta_msq;

  // --- set sonic pressure coefficient and speed
  if (mach == 0.0) {
    result.sonicPressureCoefficient = -999.0;
    result.sonicSpeed = 999.0;
  } else {
    result.sonicPressureCoefficient =
        2.0 / (gamma * mach * mach) *
        (std::pow(((1.0 + 0.5 * gammaMinusOne * mach * mach) /
                   (1.0 + 0.5 * gammaMinusOne)),
                  (gamma / gammaMinusOne)) -
         1.0);
    result.sonicSpeed =
        freeStreamSpeed / mach *
        std::sqrt((1.0 + 0.5 * gammaMinusOne * mach * mach) /
                  (1.0 + 0.5 * gammaMinusOne));
  }

  result.status = InviscidStatus::success;
  return result;
}

PressureCoefficientResult calculatePressureCoefficients(
    std::span<const double> velocity,
    std::span<double> pressureCoefficient,
    int pointCount,
    double freeStreamSpeed,
    double mach) noexcept {
  PressureCoefficientResult result;

  // Validation before the first Mutation.
  if (pointCount < 1) {
    result.status = InviscidStatus::invalidDimension;
    return result;
  }

  const auto required = static_cast<std::size_t>(pointCount) + 1;
  if (velocity.size() < required ||
      pressureCoefficient.size() < required) {
    result.status = InviscidStatus::invalidDimension;
    return result;
  }

  if (!std::isfinite(mach) || !std::isfinite(freeStreamSpeed) ||
      freeStreamSpeed == 0.0 || !(mach * mach < 1.0)) {
    result.status = InviscidStatus::invalidInput;
    return result;
  }

  for (int i = 1; i <= pointCount; ++i) {
    if (!std::isfinite(velocity[static_cast<std::size_t>(i)])) {
      result.status = InviscidStatus::invalidInput;
      return result;
    }
  }

  // Formula and operation order as in CPCALC (fortran-src/xfoil.f:1057).
  const double beta = std::sqrt(1.0 - mach * mach);
  const double bfac = 0.5 * mach * mach / (1.0 + beta);

  bool denneg = false;

  for (int i = 1; i <= pointCount; ++i) {
    const auto index = static_cast<std::size_t>(i);
    const double q = velocity[index];
    const double cpinc =
        1.0 - (q / freeStreamSpeed) * (q / freeStreamSpeed);
    const double den = beta + bfac * cpinc;
    pressureCoefficient[index] = cpinc / den;
    if (den <= 0.0) {
      denneg = true;
    }
  }

  result.denominatorNonPositive = denneg;
  result.status = denneg ? InviscidStatus::compressibilityFailure
                         : InviscidStatus::success;
  return result;
}

SurfaceVelocityResult combineUnitVelocityDistributions(
    double alphaRadians,
    UnitVelocityRows unitVelocity,
    std::span<double> velocity,
    std::span<double> alphaDerivative,
    int pointCount) noexcept {
  SurfaceVelocityResult result;

  // Validate dimensions before the first mutation.
  if (pointCount < 1) {
    result.status = InviscidStatus::invalidDimension;
    return result;
  }

  const auto required = static_cast<std::size_t>(pointCount) + 1;
  if (unitVelocity.size() < required || velocity.size() < required ||
      alphaDerivative.size() < required) {
    result.status = InviscidStatus::invalidDimension;
    return result;
  }

  // Active lines use columns 1 and 2 and must own at least three
  // values. Line index 0 is reserved.
  for (int i = 1; i <= pointCount; ++i) {
    if (unitVelocity[static_cast<std::size_t>(i)].size() < 3) {
      result.status = InviscidStatus::invalidDimension;
      return result;
    }
  }

  if (!std::isfinite(alphaRadians)) {
    result.status = InviscidStatus::invalidInput;
    return result;
  }

  for (int i = 1; i <= pointCount; ++i) {
    const auto& row = unitVelocity[static_cast<std::size_t>(i)];
    if (!std::isfinite(row[1]) || !std::isfinite(row[2])) {
      result.status = InviscidStatus::invalidInput;
      return result;
    }
  }

  // Formula and operation order as in QISET (fortran-src/xpanel.f:1597).
  const double cosine = std::cos(alphaRadians);
  const double sine = std::sin(alphaRadians);

  for (int i = 1; i <= pointCount; ++i) {
    const auto index = static_cast<std::size_t>(i);
    const auto& row = unitVelocity[index];
    velocity[index] = cosine * row[1] + sine * row[2];
    alphaDerivative[index] = -sine * row[1] + cosine * row[2];
  }

  result.cosineAlpha = cosine;
  result.sineAlpha = sine;
  result.status = InviscidStatus::success;
  return result;
}

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
    double referenceY) noexcept {
  PressureLoadResult result;

  // Validate dimensions before the first computation.
  if (pointCount < 2) {
    result.status = InviscidStatus::invalidDimension;
    return result;
  }

  const auto required = static_cast<std::size_t>(pointCount) + 1;
  if (x.size() < required || y.size() < required ||
      circulation.size() < required ||
      circulationAlphaDerivative.size() < required) {
    result.status = InviscidStatus::invalidDimension;
    return result;
  }

  // Active values and scalars must be finite.
  if (!std::isfinite(alphaRadians) || !std::isfinite(freeStreamSpeed) ||
      !std::isfinite(mach) || !std::isfinite(referenceX) ||
      !std::isfinite(referenceY) || freeStreamSpeed == 0.0 ||
      !(mach * mach < 1.0)) {
    result.status = InviscidStatus::invalidInput;
    return result;
  }

  for (int i = 1; i <= pointCount; ++i) {
    const auto index = static_cast<std::size_t>(i);
    if (!std::isfinite(x[index]) || !std::isfinite(y[index]) ||
        !std::isfinite(circulation[index]) ||
        !std::isfinite(circulationAlphaDerivative[index])) {
      result.status = InviscidStatus::invalidInput;
      return result;
    }
  }

  // Formula, summation order and operation order exactly as in
  // CLCALC (fortran-src/xfoil.f:1088).
  const double sa = std::sin(alphaRadians);
  const double ca = std::cos(alphaRadians);

  double xcp = 0.0;

  const double beta = std::sqrt(1.0 - mach * mach);
  const double beta_msq = -0.5 / beta;

  const double bfac = 0.5 * mach * mach / (1.0 + beta);
  const double bfac_msq =
      0.5 / (1.0 + beta) - bfac / (1.0 + beta) * beta_msq;

  double cl = 0.0;
  double cm = 0.0;
  double cdp = 0.0;
  double cl_alf = 0.0;
  double cl_msq = 0.0;

  int i = 1;
  const auto first = static_cast<std::size_t>(i);
  double cginc = 1.0 - (circulation[first] / freeStreamSpeed) *
                           (circulation[first] / freeStreamSpeed);
  double cpg1 = cginc / (beta + bfac * cginc);
  double cpg1_msq =
      -cpg1 / (beta + bfac * cginc) * (beta_msq + bfac_msq * cginc);

  double cpi_gam = -2.0 * circulation[first] / freeStreamSpeed / freeStreamSpeed;
  double cpc_cpi =
      (1.0 - bfac * cpg1) / (beta + bfac * cginc);
  double cpg1_alf = cpc_cpi * cpi_gam * circulationAlphaDerivative[first];

  for (i = 1; i <= pointCount; i++) {
    int ip = i + 1;
    if (i == pointCount) {
      ip = 1;
    }

    const auto ci = static_cast<std::size_t>(i);
    const auto cp = static_cast<std::size_t>(ip);

    cginc = 1.0 - (circulation[cp] / freeStreamSpeed) *
                      (circulation[cp] / freeStreamSpeed);
    const double cpg2 = cginc / (beta + bfac * cginc);
    const double cpg2_msq =
        -cpg2 / (beta + bfac * cginc) * (beta_msq + bfac_msq * cginc);

    cpi_gam = -2.0 * circulation[cp] / freeStreamSpeed / freeStreamSpeed;
    cpc_cpi = (1.0 - bfac * cpg2) / (beta + bfac * cginc);
    const double cpg2_alf =
        cpc_cpi * cpi_gam * circulationAlphaDerivative[cp];

    const double dx =
        (x[cp] - x[ci]) * ca + (y[cp] - y[ci]) * sa;
    const double dy =
        (y[cp] - y[ci]) * ca - (x[cp] - x[ci]) * sa;
    const double dg = cpg2 - cpg1;

    const double ax =
        (0.5 * (x[cp] + x[ci]) - referenceX) * ca +
        (0.5 * (y[cp] + y[ci]) - referenceY) * sa;
    const double ay =
        (0.5 * (y[cp] + y[ci]) - referenceY) * ca -
        (0.5 * (x[cp] + x[ci]) - referenceX) * sa;
    const double ag = 0.5 * (cpg2 + cpg1);

    const double dx_alf =
        -(x[cp] - x[ci]) * sa + (y[cp] - y[ci]) * ca;
    const double ag_alf = 0.5 * (cpg2_alf + cpg1_alf);
    const double ag_msq = 0.5 * (cpg2_msq + cpg1_msq);

    cl = cl + dx * ag;
    cdp = cdp - dy * ag;
    cm = cm - dx * (ag * ax + dg * dx / 12.0) -
         dy * (ag * ay + dg * dy / 12.0);

    xcp += dx * ag * (x[cp] + x[ci]) / 2.0;

    cl_alf = cl_alf + dx * ag_alf + ag * dx_alf;
    cl_msq = cl_msq + dx * ag_msq;

    cpg1 = cpg2;
    cpg1_alf = cpg2_alf;
    cpg1_msq = cpg2_msq;
  }

  if (std::fabs(cl) > 0.0) {
    xcp /= cl;
  } else {
    xcp = 0.0;
  }

  result.liftCoefficient = cl;
  result.pressureDragCoefficient = cdp;
  result.momentCoefficient = cm;
  result.centerOfPressureX = xcp;
  result.liftAlphaDerivative = cl_alf;
  result.liftMachSquaredDerivative = cl_msq;
  result.status = InviscidStatus::success;
  return result;
}

}  // namespace xfoil::inviscid

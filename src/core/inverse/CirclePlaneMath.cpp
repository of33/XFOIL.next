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

// File: src/core/inverse/CirclePlaneMath.cpp
// Stateless circle/Fourier kernels. The formulas are
// taken literally from the EIWSET, FTP, PIQSUM and CNFILT routines
// in fortran-src/xmdes.f;
// constants (PI), indexing and orderings are
// unchanged. No FFT library, no exceptions, no state.

#include "xfoil/inverse/CirclePlaneMath.h"

#include <cmath>

namespace xfoil::inverse {

namespace {

// Deliberately rounded PI value 3.141592654 (/22).
// Do not replace this constant with std::numbers::pi, because doing so
// could change the frozen reference values of the inverse paths.
constexpr double kPi = 3.141592654;

bool isComplexFinite(const std::complex<double>& value) noexcept {
  return std::isfinite(value.real()) && std::isfinite(value.imag());
}

bool dimensionsPlausible(int circlePointCount, int harmonicCount) noexcept {
  return circlePointCount >= 2 && harmonicCount >= 0 &&
         harmonicCount < circlePointCount;
}

bool exponentialsRowsUsable(const std::span<std::span<const std::complex<double>>>& exponentials,
                            std::size_t count,
                            std::size_t harmonics) noexcept {
  if (exponentials.size() < count + 1) {
    return false;
  }
  for (std::size_t ic = 1; ic <= count; ++ic) {
    if (exponentials[ic].size() < harmonics + 1) {
      return false;
    }
  }
  return true;
}

bool exponentialsRowsUsable(ComplexRows exponentials,
                            std::size_t count,
                            std::size_t harmonics) noexcept {
  if (exponentials.size() < count + 1) {
    return false;
  }
  for (std::size_t ic = 1; ic <= count; ++ic) {
    if (exponentials[ic].size() < harmonics + 1) {
      return false;
    }
  }
  return true;
}

}  // namespace

InverseMathStatus initializeUnitCircleExponentials(
    int circlePointCount,
    int harmonicCount,
    std::span<double> circleCoordinate,
    ComplexRows exponentials,
    double& coordinateIncrement) noexcept {
  if (!dimensionsPlausible(circlePointCount, harmonicCount)) {
    return InverseMathStatus::invalidDimension;
  }

  const auto count = static_cast<std::size_t>(circlePointCount);
  const auto harmonics = static_cast<std::size_t>(harmonicCount);

  if (circleCoordinate.size() < count + 1 ||
      !exponentialsRowsUsable(exponentials, count, harmonics)) {
    return InverseMathStatus::invalidDimension;
  }

  // --- dwc = 2 pi/(NC-1), unchanged from eiwset
  const double dwc =
      2.0 * kPi / static_cast<double>(circlePointCount - 1);

  for (int ic = 1; ic <= circlePointCount; ic++) {
    circleCoordinate[static_cast<std::size_t>(ic)] =
        dwc * static_cast<double>(ic - 1);
  }

  // --- set m = 0 numbers
  for (int ic = 1; ic <= circlePointCount; ic++) {
    exponentials[static_cast<std::size_t>(ic)][0] =
        std::complex<double>(1.0, 0.0);
  }

  // For harmonicCount == 0 only the active mean-value term m = 0 exists.
  // In this case column 1 must not be accessed.
  if (harmonicCount >= 1) {
    // --- set m = 1 numbers
    for (int ic = 1; ic <= circlePointCount; ++ic) {
      exponentials[static_cast<std::size_t>(ic)][1] =
      std::exp(std::complex<double>(
        0.0,
        circleCoordinate[static_cast<std::size_t>(ic)]));
    }

    // --- set m > 1 numbers by indexing appropriately from m = 1 numbers
    for (int m = 2; m <= harmonicCount; ++m) {
      for (int ic = 1; ic <= circlePointCount; ++ic) {
        int ic1 = m * (ic - 1);
        ic1 = ic1 % (circlePointCount - 1) + 1;

        exponentials[static_cast<std::size_t>(ic)]
        [static_cast<std::size_t>(m)] =
        exponentials[static_cast<std::size_t>(ic1)][1];
      }
    }
  }

  coordinateIncrement = dwc;
  return InverseMathStatus::success;
}

InverseMathStatus forwardTransform(
    int circlePointCount,
    int harmonicCount,
    double coordinateIncrement,
    std::span<const std::complex<double>> harmonicFunction,
    ConstComplexRows exponentials,
    std::span<std::complex<double>> coefficients) noexcept {
  if (!dimensionsPlausible(circlePointCount, harmonicCount)) {
    return InverseMathStatus::invalidDimension;
  }
  if (!std::isfinite(coordinateIncrement)) {
    return InverseMathStatus::invalidInput;
  }

  const auto count = static_cast<std::size_t>(circlePointCount);
  const auto harmonics = static_cast<std::size_t>(harmonicCount);

  if (harmonicFunction.size() < count + 1 ||
      coefficients.size() < harmonics + 1 ||
      !exponentialsRowsUsable(exponentials, count, harmonics)) {
    return InverseMathStatus::invalidDimension;
  }

  // --- finite active inputs
  for (std::size_t ic = 1; ic <= count; ++ic) {
    if (!isComplexFinite(harmonicFunction[ic])) {
      return InverseMathStatus::invalidInput;
    }
    for (std::size_t m = 0; m <= harmonics; ++m) {
      if (!isComplexFinite(exponentials[ic][m])) {
        return InverseMathStatus::invalidInput;
      }
    }
  }

  // --- trapezoidal integration, summation order unchanged from ftp
  for (int m = 0; m <= harmonicCount; m++) {
    const auto index = static_cast<std::size_t>(m);
    std::complex<double> zsum(0.0, 0.0);
    for (int ic = 2; ic <= circlePointCount - 1; ic++) {
      zsum = zsum + harmonicFunction[static_cast<std::size_t>(ic)] *
                        exponentials[static_cast<std::size_t>(ic)][index];
    }

    coefficients[index] =
        (0.5 * (harmonicFunction[1] * exponentials[1][index] +
                harmonicFunction[count] * exponentials[count][index]) +
         zsum) *
        coordinateIncrement / kPi;
  }
  coefficients[0] = 0.5 * coefficients[0];

  return InverseMathStatus::success;
}

InverseMathStatus inverseTransform(
    int circlePointCount,
    int harmonicCount,
    std::span<const std::complex<double>> coefficients,
    ConstComplexRows exponentials,
    std::span<std::complex<double>> harmonicFunction) noexcept {
  if (!dimensionsPlausible(circlePointCount, harmonicCount)) {
    return InverseMathStatus::invalidDimension;
  }

  const auto count = static_cast<std::size_t>(circlePointCount);
  const auto harmonics = static_cast<std::size_t>(harmonicCount);

  if (harmonicFunction.size() < count + 1 ||
      coefficients.size() < harmonics + 1 ||
      !exponentialsRowsUsable(exponentials, count, harmonics)) {
    return InverseMathStatus::invalidDimension;
  }

  // --- finite active inputs
  for (std::size_t m = 0; m <= harmonics; ++m) {
    if (!isComplexFinite(coefficients[m])) {
      return InverseMathStatus::invalidInput;
    }
  }
  for (std::size_t ic = 1; ic <= count; ++ic) {
    for (std::size_t m = 0; m <= harmonics; ++m) {
      if (!isComplexFinite(exponentials[ic][m])) {
        return InverseMathStatus::invalidInput;
      }
    }
  }

  // --- back-transformation, summation order unchanged from piqsum
  for (int ic = 1; ic <= circlePointCount; ic++) {
    const auto row = static_cast<std::size_t>(ic);
    std::complex<double> zsum(0.0, 0.0);
    for (int m = 0; m <= harmonicCount; m++) {
      const auto index = static_cast<std::size_t>(m);
      zsum = zsum + coefficients[index] * conjugate(exponentials[row][index]);
    }
    harmonicFunction[row] = zsum;
  }

  return InverseMathStatus::success;
}

InverseMathStatus applyModifiedHanningFilter(
    double filterExponent,
    int harmonicCount,
    std::span<std::complex<double>> coefficients) noexcept {
  if (harmonicCount < 0) {
    return InverseMathStatus::invalidDimension;
  }
  if (!std::isfinite(filterExponent)) {
    return InverseMathStatus::invalidInput;
  }

  const auto harmonics = static_cast<std::size_t>(harmonicCount);
  if (coefficients.size() < harmonics + 1) {
    return InverseMathStatus::invalidDimension;
  }

  // --- finite active inputs
  for (std::size_t m = 0; m <= harmonics; ++m) {
    if (!isComplexFinite(coefficients[m])) {
      return InverseMathStatus::invalidInput;
    }
  }

  // --- Early exit unchanged: ffilt <= 0.00001
  if (filterExponent <= 0.00001) {
    return InverseMathStatus::success;
  }

  // For harmonicCount == 0 only the mean term exists. It stays
  // unchanged; the frequency normalization by harmonicCount is not
  // defined here.
  if (harmonicCount == 0) {
    return InverseMathStatus::success;
  }

  for (int m = 0; m <= harmonicCount; m++) {
    const auto index = static_cast<std::size_t>(m);
    const double freq = static_cast<double>(m) / static_cast<double>(harmonicCount);
    const double cwt = 0.5 * (1.0 + std::cos(kPi * freq));
    double cwtx = cwt;
    if (filterExponent > 0.0) {
      cwtx = std::pow(cwt, filterExponent);
    }
    coefficients[index] = std::complex<double>(coefficients[index].real() * cwtx,
                                               coefficients[index].imag() * cwtx);
  }

  return InverseMathStatus::success;
}

}  // namespace xfoil::inverse

// End: src/core/inverse/CirclePlaneMath.cpp

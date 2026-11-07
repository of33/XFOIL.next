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

// file: include/xfoil/inverse/CirclePlaneMath.h
// Stateless circle/Fourier kernels for the inverse method,
// implementing the EIWSET (kernel part), FTP, PIQSUM and CNFILT
// routines of fortran-src/xmdes.f and the CONJG complex-conjugate
// operation used there. Formulas, constants (PI
// 3.141592654), modulo indexing, trapezoidal integration, halving of cn[0],
// summation order and the early-exit case of the filter formula are
// adopted literally unchanged. No FFT library.
//
// Index convention (1-based):
// - Circle indices are 1..circlePointCount; circle index 0 is reserved.
// - harmonic indices are 0..harmonicCount.
// - coefficient index 0 is active (mean term).
// - matrix row 0 of exponentials must be present as memory
// (reserved row); it is never read or written.
// - each active circle row 1..circlePointCount needs at least
// harmonicCount + 1 values.
// - circleCoordinate, harmonicFunction: Quantity at least
// circlePointCount + 1 (index 0 reserved, untouched).
// - coefficients: Quantity at least harmonicCount + 1.
//
// The API contains no Raw Pointer, stores no Spans via a
// call and allocates no heap memory. Validation happens before each
// mutation; no new mathematical tolerance is deliberately introduced.

#pragma once

#include <complex>
#include <span>

namespace xfoil::inverse {

enum class InverseMathStatus {
  success,
  invalidDimension,
  invalidInput
};

using ComplexRows =
    std::span<std::span<std::complex<double>>>;

using ConstComplexRows =
    std::span<std::span<const std::complex<double>>>;

// Implements the stateless kernel of EIWSET (fortran-src/xmdes.f:1607): sets the
// uniformly distributed circle coordinate increment wc (omega) with
// dwc = 2 * PI / (circlePointCount - 1) (PI 3.141592654) and the
// complex unit numbers exp(i m w) in exponentials[ic][m] for
// m = 0..harmonicCount. The lines m > 1 be by the unchanged
// Modulo indexing ic1 = (m * (ic - 1)) % (circlePointCount - 1) + 1 from
// the line m = 1 is formed. coordinateIncrement carries dwc.
[[nodiscard]]
InverseMathStatus initializeUnitCircleExponentials(
    int circlePointCount,
    int harmonicCount,
    std::span<double> circleCoordinate,
    ComplexRows exponentials,
    double& coordinateIncrement) noexcept;

// Implements FTP (fortran-src/xmdes.f:1586): slow Fourier transform of the harmonic
// function via trapezoidal integration. coefficients[0] is
// halved at the end. coordinateIncrement is the circle increment dwc.
[[nodiscard]]
InverseMathStatus forwardTransform(
    int circlePointCount,
    int harmonicCount,
    double coordinateIncrement,
    std::span<const std::complex<double>> harmonicFunction,
    ConstComplexRows exponentials,
    std::span<std::complex<double>> coefficients) noexcept;

// Implements PIQSUM (fortran-src/xmdes.f:1274): back-transforms the coefficients to the
// circle points with conjugated unit numbers, summation order
// m = 0..harmonicCount unchanged.
[[nodiscard]]
InverseMathStatus inverseTransform(
    int circlePointCount,
    int harmonicCount,
    std::span<const std::complex<double>> coefficients,
    ConstComplexRows exponentials,
    std::span<std::complex<double>> harmonicFunction) noexcept;

// Implements CNFILT (fortran-src/xmdes.f:1294): modified Hanning filter on the
// Coefficients 0..harmonicCount. Early exit unchanged for
// filterExponent <= 0.00001; otherwise freq = m / harmonicCount,
// cwt = 0.5 * (1 + cos(PI * freq)) with PI 3.141592654 and cwtx = pow(cwt,
// filterExponent) for filterExponent > 0.
[[nodiscard]]
InverseMathStatus applyModifiedHanningFilter(
    double filterExponent,
    int harmonicCount,
    std::span<std::complex<double>> coefficients) noexcept;

// Complex conjugation, mirroring the CONJG operation used in fortran-src/xmdes.f.
[[nodiscard]]
constexpr std::complex<double> conjugate(
    std::complex<double> value) noexcept {
  return {value.real(), -value.imag()};
}

}  // namespace xfoil::inverse

// End: include/xfoil/inverse/CirclePlaneMath.h

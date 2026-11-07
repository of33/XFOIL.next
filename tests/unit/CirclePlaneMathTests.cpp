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

// File: tests/unit/CirclePlaneMathTests.cpp
// unit tests for the stateless circle/Fourier modules.
// reference values are produced independently of the functions under test:
// directly from the analytic expressions (std::polar, std::sin/cos) with the
// documented PI value 3.141592654, not via the module's column table.

#include "TestSupport.h"

#include "xfoil/inverse/CirclePlaneMath.h"

#include <cmath>
#include <complex>
#include <span>
#include <vector>

namespace {

using Complex = std::complex<double>;
using Status = xfoil::inverse::InverseMathStatus;

// Deliberately rounded PI value 3.141592654.
constexpr double kPi = 3.141592654;

// circle matrix with separately stored rows (reserved row 0).
struct CircleFixture {
  int count;
  int harmonics;
  std::vector<std::vector<Complex>> storage;
  std::vector<std::span<Complex>> rows;
  std::vector<std::span<const Complex>> constRows;
  std::vector<double> coordinate;
  double increment{0.0};

  CircleFixture(int circlePointCount, int harmonicCount)
      : count(circlePointCount), harmonics(harmonicCount) {
    storage.assign(static_cast<std::size_t>(count) + 1,
                   std::vector<Complex>(
                       static_cast<std::size_t>(harmonics) + 1,
                       Complex(0.0, 0.0)));
    rows.resize(static_cast<std::size_t>(count) + 1);
    constRows.resize(static_cast<std::size_t>(count) + 1);
    for (std::size_t ic = 0; ic <= static_cast<std::size_t>(count); ++ic) {
      rows[ic] = std::span<Complex>(storage[ic]);
      constRows[ic] = std::span<const Complex>(storage[ic]);
    }
    coordinate.assign(static_cast<std::size_t>(count) + 1, 0.0);
  }

  xfoil::inverse::ComplexRows exponentials() {
    return xfoil::inverse::ComplexRows(rows);
  }

  xfoil::inverse::ConstComplexRows constExponentials() {
    return xfoil::inverse::ConstComplexRows(constRows);
  }
};

double expectedIncrement(int circlePointCount) {
  return 2.0 * kPi / static_cast<double>(circlePointCount - 1);
}

// Unexpected status codes are reported as hard errors.
void expectSuccess(Status status) {
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(Status::success));
}

}  // namespace

TEST(unit, CirclePlaneInitializationSetsCoordinates) {
  CircleFixture fixture(9, 3);
  fixture.coordinate[0] = -777.0;  // reserved index, must stay unchanged

  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  const double dwc = expectedIncrement(9);
  CHECK_NEAR(fixture.increment, dwc, 0.0, 1e-12);
  CHECK_EQ(fixture.coordinate[0], -777.0);  // index 0 stays reserved
  for (int ic = 1; ic <= 9; ++ic) {
    const double expected = dwc * static_cast<double>(ic - 1);
    CHECK_NEAR(fixture.coordinate[static_cast<std::size_t>(ic)], expected, 0.0,
               1e-12);
  }
}

TEST(unit, CirclePlaneInitializationSetsZeroAndFirstHarmonic) {
  CircleFixture fixture(9, 3);
  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  const double dwc = expectedIncrement(9);
  for (int ic = 1; ic <= 9; ++ic) {
    const auto row = fixture.storage[static_cast<std::size_t>(ic)];
    CHECK_EQ(row[0], Complex(1.0, 0.0));

    // Independent reference: exp(i w) via magnitude 1 and angle.
    const Complex expectedFirst = std::polar(1.0, dwc * static_cast<double>(ic - 1));
    CHECK_NEAR(row[1].real(), expectedFirst.real(), 1e-12, 0.0);
    CHECK_NEAR(row[1].imag(), expectedFirst.imag(), 1e-12, 0.0);
  }
}

TEST(unit, CirclePlaneInitializationBuildsHigherHarmonics) {
  CircleFixture fixture(9, 3);
  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  const double dwc = expectedIncrement(9);
  // Independent reference: exp(i m w_ic) via polar, the modulo angle
  // (m * (ic - 1)) mod (nc - 1) follows from the uniform spacing of the
  // tick marks and is evaluated here directly as a full angle.
  for (int ic = 1; ic <= 9; ++ic) {
    for (int m = 2; m <= 3; ++m) {
      const Complex expected = std::polar(
          1.0, dwc * static_cast<double>(m) * static_cast<double>(ic - 1));
      const Complex& actual =
          fixture.storage[static_cast<std::size_t>(ic)]
                         [static_cast<std::size_t>(m)];
      // Tolerance 1e-7: modulo-angle indexing with PI
      // (3.141592654) runs against the exact 2*pi period and leaves a
      // wrap error of this order of magnitude.
      CHECK_NEAR(actual.real(), expected.real(), 1e-7, 0.0);
      CHECK_NEAR(actual.imag(), expected.imag(), 1e-7, 0.0);
    }
  }
}

TEST(unit, CirclePlaneInitializationRejectsInvalidDimensions) {
  double increment = 0.0;
  std::vector<double> coordinate(4, 0.0);

  // circle indices < 2.
  {
    CircleFixture fixture(3, 1);
    CHECK_EQ(static_cast<int>(xfoil::inverse::initializeUnitCircleExponentials(
                 1, 0, fixture.coordinate, fixture.exponentials(), increment)),
             static_cast<int>(Status::invalidDimension));
  }
  // negative harmonic count.
  {
    CircleFixture fixture(3, 1);
    CHECK_EQ(static_cast<int>(xfoil::inverse::initializeUnitCircleExponentials(
                 3, -1, fixture.coordinate, fixture.exponentials(), increment)),
             static_cast<int>(Status::invalidDimension));
  }
  // harmonicCount >= circlePointCount.
  {
    CircleFixture fixture(4, 4);
    CHECK_EQ(static_cast<int>(xfoil::inverse::initializeUnitCircleExponentials(
                 4, 4, fixture.coordinate, fixture.exponentials(), increment)),
             static_cast<int>(Status::invalidDimension));
  }
  // coordinate span too small (index 0 + nc values required).
  {
    CircleFixture fixture(9, 3);
    std::vector<double> tooSmall(9, 0.0);
    CHECK_EQ(static_cast<int>(xfoil::inverse::initializeUnitCircleExponentials(
                 9, 3, tooSmall, fixture.exponentials(), increment)),
             static_cast<int>(Status::invalidDimension));
  }
  // matrix without the reserved row 0.
  {
    CircleFixture fixture(3, 1);
    std::vector<std::span<Complex>> rows(fixture.rows.begin() + 1,
                                         fixture.rows.end());
    CHECK_EQ(static_cast<int>(xfoil::inverse::initializeUnitCircleExponentials(
                 3, 1, fixture.coordinate,
                 xfoil::inverse::ComplexRows(rows), increment)),
             static_cast<int>(Status::invalidDimension));
  }
  // active row with too few harmonics.
  {
    CircleFixture fixture(9, 3);
    std::vector<std::vector<Complex>> narrow(
        10, std::vector<Complex>(3, Complex(0.0, 0.0)));
    std::vector<std::span<Complex>> rows(narrow.size());
    for (std::size_t ic = 0; ic < narrow.size(); ++ic) {
      rows[ic] = std::span<Complex>(narrow[ic]);
    }
    CHECK_EQ(static_cast<int>(xfoil::inverse::initializeUnitCircleExponentials(
                 9, 3, fixture.coordinate,
                 xfoil::inverse::ComplexRows(rows), increment)),
             static_cast<int>(Status::invalidDimension));
  }
}

TEST(unit, CirclePlaneSupportsZeroHarmonicCount) {
  CircleFixture fixture(3, 0);

  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
    3,
    0,
    fixture.coordinate,
    fixture.exponentials(),
                                                                 fixture.increment));

  for (int ic = 1; ic <= 3; ++ic) {
    CHECK_EQ(
      fixture.storage[static_cast<std::size_t>(ic)][0],
             Complex(1.0, 0.0));
  }

  const Complex constantValue(2.0, -0.5);
  std::vector<Complex> harmonic(4, constantValue);
  harmonic[0] = Complex(99.0, 99.0);

  std::vector<Complex> coefficients(1, Complex(0.0, 0.0));

  expectSuccess(xfoil::inverse::forwardTransform(
    3,
    0,
    fixture.increment,
    harmonic,
    fixture.constExponentials(),
                                                 coefficients));

  CHECK_NEAR(coefficients[0].real(), constantValue.real(), 1e-12, 0.0);
  CHECK_NEAR(coefficients[0].imag(), constantValue.imag(), 1e-12, 0.0);

  std::vector<Complex> restored(4, Complex(0.0, 0.0));

  expectSuccess(xfoil::inverse::inverseTransform(
    3,
    0,
    coefficients,
    fixture.constExponentials(),
                                                 restored));

  for (int ic = 1; ic <= 3; ++ic) {
    CHECK_NEAR(
      restored[static_cast<std::size_t>(ic)].real(),
               constantValue.real(),
               1e-12,
               0.0);
    CHECK_NEAR(
      restored[static_cast<std::size_t>(ic)].imag(),
               constantValue.imag(),
               1e-12,
               0.0);
  }

  const std::vector<Complex> beforeFilter = coefficients;

  expectSuccess(xfoil::inverse::applyModifiedHanningFilter(
    2.0, 0, coefficients));

  CHECK_EQ(coefficients[0], beforeFilter[0]);
}

TEST(unit, ForwardTransformOfZeroFunctionIsZero) {
  CircleFixture fixture(9, 3);
  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  std::vector<Complex> harmonic(10, Complex(0.0, 0.0));
  std::vector<Complex> coefficients(4, Complex(99.0, 99.0));

  expectSuccess(xfoil::inverse::forwardTransform(
      9, 3, fixture.increment, harmonic, fixture.constExponentials(),
      coefficients));

  for (int m = 0; m <= 3; ++m) {
    CHECK_EQ(coefficients[static_cast<std::size_t>(m)], Complex(0.0, 0.0));
  }
}

TEST(unit, ForwardTransformMatchesSmallReference) {
  // Input as in scinit: piq(w) = i q(w) with Re in its sine series
  // q(w) = sin(w) + 0.5 sin(2w). The trapezoid sum gives
  // independently evaluated cn[0] = 0, cn[1] = -1, cn[2] = -0.5, cn[3] = 0.
  CircleFixture fixture(9, 3);
  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  const double dwc = expectedIncrement(9);
  std::vector<Complex> harmonic(10, Complex(0.0, 0.0));
  for (int ic = 1; ic <= 9; ++ic) {
    const double w = dwc * static_cast<double>(ic - 1);
    harmonic[static_cast<std::size_t>(ic)] =
        Complex(0.0, std::sin(w) + 0.5 * std::sin(2.0 * w));
  }

  std::vector<Complex> coefficients(4, Complex(0.0, 0.0));
  expectSuccess(xfoil::inverse::forwardTransform(
      9, 3, fixture.increment, harmonic, fixture.constExponentials(),
      coefficients));

  const Complex expected[4] = {Complex(0.0, 0.0), Complex(-1.0, 0.0),
                               Complex(-0.5, 0.0), Complex(0.0, 0.0)};
  for (int m = 0; m <= 3; ++m) {
    const auto value = coefficients[static_cast<std::size_t>(m)];
    CHECK_NEAR(value.real(), expected[m].real(), 1e-8, 0.0);
    CHECK_NEAR(value.imag(), expected[m].imag(), 1e-8, 0.0);
  }
}

TEST(unit, InverseTransformMatchesSmallReference) {
  CircleFixture fixture(9, 3);
  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  std::vector<Complex> coefficients(4);
  for (int m = 0; m <= 3; ++m) {
    coefficients[static_cast<std::size_t>(m)] =
        Complex(static_cast<double>(m) + 1.0, -0.5 * static_cast<double>(m));
  }

  std::vector<Complex> harmonic(10, Complex(-55.0, -55.0));
  harmonic[0] = Complex(-55.0, -55.0);
  expectSuccess(xfoil::inverse::inverseTransform(
      9, 3, coefficients, fixture.constExponentials(), harmonic));

  CHECK_EQ(harmonic[0], Complex(-55.0, -55.0));  // index 0 reserved

  // Independent reference: hf(w) = sum_m cn[m] * conj(exp(i m w)) with
  // exp via std::polar and uniform angle steps.
  const double dwc = expectedIncrement(9);
  for (int ic = 1; ic <= 9; ++ic) {
    const double w = dwc * static_cast<double>(ic - 1);
    Complex expected(0.0, 0.0);
    for (int m = 0; m <= 3; ++m) {
      expected = expected + coefficients[static_cast<std::size_t>(m)] *
                                std::conj(std::polar(1.0, static_cast<double>(m) * w));
    }
    const auto value = harmonic[static_cast<std::size_t>(ic)];
    // Tolerance 1e-7: wrap conditionality of the modulo indexing for
    // PI 3.141592654 (see CirclePlaneInitializationBuildsHigherHarmonics).
    CHECK_NEAR(value.real(), expected.real(), 1e-7, 0.0);
    CHECK_NEAR(value.imag(), expected.imag(), 1e-7, 0.0);
  }
}

TEST(unit, TransformRoundTripPreservesRepresentableSignal) {
  // Representable signal in the sense of the FTP/PIQSUM pair: a pure
  // sine series q(w) = sin(w) + 0.5 sin(2w), passed as i q(w).
  // forward-backward preserves the imaginary part q(w) exactly; the
  // real part is the negative conjugate cosine part
  // -(cos(w) + 0.5 cos(2w)) - exactly the behavior used in scinit.
  CircleFixture fixture(9, 3);
  expectSuccess(xfoil::inverse::initializeUnitCircleExponentials(
      9, 3, fixture.coordinate, fixture.exponentials(), fixture.increment));

  const double dwc = expectedIncrement(9);
  std::vector<Complex> harmonic(10, Complex(0.0, 0.0));
  for (int ic = 1; ic <= 9; ++ic) {
    const double w = dwc * static_cast<double>(ic - 1);
    harmonic[static_cast<std::size_t>(ic)] =
        Complex(0.0, std::sin(w) + 0.5 * std::sin(2.0 * w));
  }

  std::vector<Complex> coefficients(4, Complex(0.0, 0.0));
  expectSuccess(xfoil::inverse::forwardTransform(
      9, 3, fixture.increment, harmonic, fixture.constExponentials(),
      coefficients));

  std::vector<Complex> restored(10, Complex(0.0, 0.0));
  expectSuccess(xfoil::inverse::inverseTransform(
      9, 3, coefficients, fixture.constExponentials(), restored));

  for (int ic = 1; ic <= 9; ++ic) {
    const double w = dwc * static_cast<double>(ic - 1);
    const double expectedImaginary =
        std::sin(w) + 0.5 * std::sin(2.0 * w);
    const double expectedReal =
        -(std::cos(w) + 0.5 * std::cos(2.0 * w));
    const auto value = restored[static_cast<std::size_t>(ic)];
    CHECK_NEAR(value.imag(), expectedImaginary, 1e-8, 0.0);
    CHECK_NEAR(value.real(), expectedReal, 1e-8, 0.0);
  }
}

TEST(unit, HanningFilterZeroExponentLeavesCoefficientsUnchanged) {
  const Complex original[3] = {Complex(1.0, 2.0), Complex(3.0, 4.0),
                               Complex(-5.0, 6.0)};

  // exponent 0 and the early-exit value 0.00001 leave the coefficients unchanged.
  const double exponents[2] = {0.0, 0.00001};
  for (const double exponent : exponents) {
    std::vector<Complex> coefficients(std::begin(original), std::end(original));
    expectSuccess(xfoil::inverse::applyModifiedHanningFilter(exponent, 2,
                                                             coefficients));
    for (int m = 0; m <= 2; ++m) {
      CHECK_EQ(coefficients[static_cast<std::size_t>(m)],
               original[m]);
    }
  }
}

TEST(unit, HanningFilterDampsHigherModes) {
  constexpr int kHarmonics = 4;
  std::vector<Complex> coefficients(kHarmonics + 1, Complex(1.0, 1.0));

  expectSuccess(xfoil::inverse::applyModifiedHanningFilter(2.0, kHarmonics,
                                                           coefficients));

  // Independent reference from the documented filter formula with
  // PI 3.141592654: cwt = 0.5 (1 + cos(PI m / mc)), cwtx = cwt^2.
  double previousMagnitude = 0.0;
  bool firstMode = true;
  for (int m = 0; m <= kHarmonics; ++m) {
    const double freq = static_cast<double>(m) / static_cast<double>(kHarmonics);
    const double cwt = 0.5 * (1.0 + std::cos(kPi * freq));
    const double cwtx = std::pow(cwt, 2.0);
    const auto value = coefficients[static_cast<std::size_t>(m)];
    CHECK_NEAR(value.real(), 1.0 * cwtx, 1e-12, 0.0);
    CHECK_NEAR(value.imag(), 1.0 * cwtx, 1e-12, 0.0);

    const double magnitude = std::abs(value);
    CHECK(firstMode || magnitude <= previousMagnitude + 1e-15);
    previousMagnitude = magnitude;
    firstMode = false;
  }
  CHECK(std::abs(coefficients[kHarmonics]) <
        std::abs(Complex(1.0, 1.0)));
}

TEST(unit, HanningFilterRejectsInvalidInput) {
  const double nanValue = std::nan("");

  // not a finite exponent.
  {
    std::vector<Complex> coefficients(3, Complex(1.0, 1.0));
    CHECK_EQ(static_cast<int>(xfoil::inverse::applyModifiedHanningFilter(
                 nanValue, 2, coefficients)),
             static_cast<int>(Status::invalidInput));
  }
  // negative harmonic count and too-small coefficient span.
  {
    std::vector<Complex> coefficients(3, Complex(1.0, 1.0));
    CHECK_EQ(static_cast<int>(xfoil::inverse::applyModifiedHanningFilter(
                 1.0, -1, coefficients)),
             static_cast<int>(Status::invalidDimension));
    CHECK_EQ(static_cast<int>(xfoil::inverse::applyModifiedHanningFilter(
                 1.0, 3, coefficients)),
             static_cast<int>(Status::invalidDimension));
  }
  // not all active coefficients are finite.
  {
    std::vector<Complex> coefficients(3, Complex(1.0, 1.0));
    coefficients[2] = Complex(0.0, nanValue);
    CHECK_EQ(static_cast<int>(xfoil::inverse::applyModifiedHanningFilter(
                 1.0, 2, coefficients)),
             static_cast<int>(Status::invalidInput));
  }
}

TEST(unit, ComplexConjugateMatchesReference) {
  const Complex samples[] = {Complex(3.0, 4.0),   Complex(-2.0, -5.0),
                             Complex(0.0, 0.0),   Complex(1.5, -0.25),
                             Complex(-1e300, 2.0), Complex(0.0, -7.0)};

  for (const Complex sample : samples) {
    const Complex result = xfoil::inverse::conjugate(sample);
    const Complex reference = std::conj(sample);
    CHECK_EQ(result.real(), reference.real());
    CHECK_EQ(result.imag(), reference.imag());
  }

  // Conjugation is involutory.
  static_assert(xfoil::inverse::conjugate(xfoil::inverse::conjugate(
                    Complex(2.5, -3.5))) == Complex(2.5, -3.5));
}

// End: tests/unit/CirclePlaneMathTests.cpp

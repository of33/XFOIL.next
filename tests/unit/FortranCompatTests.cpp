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

// File: tests/unit/FortranCompatTests.cpp
// tests for the Fortran compatibility functions mirroring the Fortran intrinsics SIGN and AINT and the ATANC routine.
// the tolerances cover rounding error only.

#include "TestSupport.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

#include "xfoil/core/math/FortranCompat.h"
#include "xfoil/math/FortranCompat.h"

using xfoil::math::continuousAtan2;
using xfoil::math::copyMagnitudeWithSign;
using xfoil::math::truncateTowardZero;

TEST(unit, CopyMagnitudeWithPositiveSign) {
  CHECK_NEAR(copyMagnitudeWithSign(-3.5, 2.0), 3.5, 1e-15, 0.0);
  CHECK_NEAR(copyMagnitudeWithSign(3.5, 2.0), 3.5, 1e-15, 0.0);
  CHECK_NEAR(copyMagnitudeWithSign(-7.25, 1.0e-300), 7.25, 0.0, 1e-15);
}

TEST(unit, CopyMagnitudeWithNegativeSign) {
  CHECK_NEAR(copyMagnitudeWithSign(-3.5, -2.0), -3.5, 1e-15, 0.0);
  CHECK_NEAR(copyMagnitudeWithSign(3.5, -2.0), -3.5, 1e-15, 0.0);
}

TEST(unit, CopyMagnitudeWithZeroSign) {
  // Sign source exactly 0.0: 0.0 >= 0.0 is true, so magnitude is positive.
  CHECK_NEAR(copyMagnitudeWithSign(-4.0, 0.0), 4.0, 1e-15, 0.0);
  CHECK_NEAR(copyMagnitudeWithSign(4.0, 0.0), 4.0, 1e-15, 0.0);
}

TEST(unit, CopyMagnitudeWithNegativeZeroSign) {
  // Consistent with the previous comparison -0.0 >= 0.0 (true): the
  // magnitude is positive. std::copysign was deliberately not used.
  const double negativeZero = std::copysign(0.0, -1.0);
  CHECK(std::signbit(negativeZero));
  CHECK_NEAR(copyMagnitudeWithSign(-4.0, negativeZero), 4.0, 1e-15, 0.0);
  CHECK_NEAR(copyMagnitudeWithSign(4.0, negativeZero), 4.0, 1e-15, 0.0);
}

TEST(unit, TruncateTowardZeroPositive) {
  CHECK_NEAR(truncateTowardZero(3.7), 3.0, 0.0, 0.0);
  CHECK_NEAR(truncateTowardZero(0.2), 0.0, 0.0, 0.0);
  CHECK_NEAR(truncateTowardZero(5.0), 5.0, 0.0, 0.0);
}

TEST(unit, TruncateTowardZeroNegative) {
  CHECK_NEAR(truncateTowardZero(-3.7), -3.0, 0.0, 0.0);
  CHECK_NEAR(truncateTowardZero(-0.2), 0.0, 0.0, 0.0);
  CHECK_NEAR(truncateTowardZero(-5.0), -5.0, 0.0, 0.0);
}

TEST(unit, ContinuousAtan2DoesNotJumpAtNegativeXAxis) {
  // Two neighboring points near the negative x axis, whose atan2 values
  // jump from slightly above +pi to slightly below -pi. continuousAtan2
  // must let the angle continue running continuously above +pi.
  const double previous = std::atan2(1.0e-9, -1.0);   // ~ +pi (slightly under it)
  const double jumped = std::atan2(-1.0e-9, -1.0);    // ~ -pi (jump)
  CHECK(jumped < 0.0);

  const double result = continuousAtan2(-1.0e-9, -1.0, previous);

  // No jump: the result lies slightly above +pi, not near -pi.
  CHECK(result > 3.14);
  CHECK(result < 3.15);
  CHECK_NEAR(std::fabs(result - previous), 2.0e-9, 1e-12, 0.0);

  // Counter-check without crossing: smaller angles stay unchanged.
  const double small = continuousAtan2(0.5, 2.0, 0.2);
  CHECK_NEAR(small, std::atan2(0.5, 2.0), 1e-12, 0.0);
}

// -XYWAKE correction: the integer power in SETEXP must match gfortran's
// `**` with an integer exponent bit for bit (repeated squaring, not
// n-fold sequential multiplication).
// reference: a throwaway Fortran probe built with the canonical flags
// -fdefault-real-8 -fdefault-double-8 (144/144 bit-identical for
// exponents 1..48), exercised via setExp: the SNEW progression for the
// scenario (NACA 2412, PANGEN, Re=1e6, alfa 2, NW=23,
// DS1=8.3846484558742773e-03, SMAX=WAKLEN*CHORD) is bit-identical to the
// Fortran reference.
TEST(unit, SetExpSnewBitIdenticalToFortran) {
  // Fortran: NW = N/12 + 10*INT(WAKLEN) = 160/12 + 10 = 23.
  std::array<double, 23> snew{};
  const double ds1 = 8.3846484558742773e-03;
  const double smax = 1.0000000245898315;  // WAKLEN*CHORD (CHORD from GEOPAR)
  const auto diag = xfoil::core::math::setExp(snew, ds1, smax);
  CHECK(diag == xfoil::core::math::Diagnostic::completed);
  // Fortran-verified bits (fr_xwake probe, SETEXP exit):
  const std::array<std::uint64_t, 23> expectedBits{
      0x0000000000000000, 0x3F812BF877406200, 0x3F925E12FD8B44B2,
      0x3F9D82E578D01610, 0x3FA51A825AD27FDB, 0x3FAC55E29D3923E4,
      0x3FB2498C0BBE1DB3, 0x3FB6FB05851F146A, 0x3FBC53D1F00F53CF,
      0x3FC1359F5D870C7F, 0x3FC4ADEC0E05D184, 0x3FC8A1EE61AA0F00,
      0x3FCD22E0D5AFC9C8, 0x3FD1223210FEE22C, 0x3FD40E6A61D0DAE6,
      0x3FD762D638806A7C, 0x3FDB2DF8903A5B60, 0x3FDF8059C2994791,
      0x3FE23667CA6F6E3C, 0x3FE50467A6FAEBDA, 0x3FE836659D7D7C81,
      0x3FEBDA4E9CEEB2A7, 0x3FF00000069A5C46};
  for (std::size_t i = 0; i < snew.size(); ++i) {
    CHECK_EQ(std::bit_cast<std::uint64_t>(snew[i]), expectedBits[i]);
  }
}

// End: tests/unit/FortranCompatTests.cpp

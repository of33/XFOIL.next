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

// File: tests/unit/PortableNumericContractTests.cpp
// PUB-W1: contract tests for the platform-specific numeric comparison
// classes. They pin (a) the ULP distance helper, (b) the platform
// selection (Linux and every other exact platform must never relax),
// and (c) the documented Windows budgets. The Windows assertions only
// execute on a Windows run; on Linux they document the contract.

#include "TestSupport.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

double nextUp(double v) {
  return std::nextafter(v, std::numeric_limits<double>::infinity());
}

}  // namespace

TEST(unit, PortableNumericExactPlatformFlag) {
  // Platform selection guard: on every non-Windows build the exact
  // contract must be active.
#ifndef _WIN32
  CHECK(testing::kPortableNumericExactPlatform);
#else
  CHECK(!testing::kPortableNumericExactPlatform);
#endif
}

TEST(unit, PortableUlpDistanceBasics) {
  using testing::portableUlpDistance;
  CHECK_EQ(portableUlpDistance(1.0, 1.0), std::uint64_t(0));
  CHECK_EQ(portableUlpDistance(nextUp(1.0), 1.0), std::uint64_t(1));
  CHECK_EQ(portableUlpDistance(std::nextafter(1.0, 0.0), 1.0),
           std::uint64_t(1));
  // +0.0 and -0.0 are one point on the value line.
  CHECK_EQ(portableUlpDistance(0.0, -0.0), std::uint64_t(0));
  // Opposite infinities and NaN mixtures are outside the ULP line.
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(portableUlpDistance(inf, -inf) ==
        std::numeric_limits<std::uint64_t>::max());
  CHECK(portableUlpDistance(nan, 1.0) ==
        std::numeric_limits<std::uint64_t>::max());
  // Bit-identical pairs always pass, including NaN.
  CHECK_EQ(portableUlpDistance(nan, nan), std::uint64_t(0));
}

TEST(unit, PortableContractExactPlatformNeverRelaxes) {
  using testing::PortableNumericClass;
  if (!testing::kPortableNumericExactPlatform) {
    return;  // Windows budgets are exercised by their own tests below.
  }
  // On the exact platform any bitwise difference fails every class.
  const double a = 1.0;
  const double b = nextUp(1.0);
  CHECK(!testing::withinPortableNumericContract(b, a,
                                                PortableNumericClass::kExactBits));
  CHECK(!testing::withinPortableNumericContract(b, a,
                                                PortableNumericClass::kReplayValue));
  CHECK(!testing::withinPortableNumericContract(b, a,
                                                PortableNumericClass::kUlpBudget128));
  CHECK(!testing::withinPortableNumericContract(b, a,
                                                PortableNumericClass::kUlpBudget1024));
  CHECK(!testing::withinPortableNumericContract(b, a,
                                                PortableNumericClass::kNearZero1e15));
  CHECK(!testing::withinPortableNumericContract(b, a,
                                                PortableNumericClass::kDijMatrixValue));
  // Bit-identical pairs pass; NaN cannot be smuggled through.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(testing::withinPortableNumericContract(nan, nan,
                                               PortableNumericClass::kExactBits));
  CHECK(!testing::withinPortableNumericContract(nan, 1.0,
                                                PortableNumericClass::kReplayValue));
  // Value classes keep the historic == semantics (-0.0 vs 0.0 passes,
  // bitwise classes keep the historic bitwise semantics).
  CHECK(testing::withinPortableNumericContract(0.0, -0.0,
                                               PortableNumericClass::kDijMatrixValue));
  CHECK(!testing::withinPortableNumericContract(0.0, -0.0,
                                                PortableNumericClass::kExactBits));
}

TEST(unit, PortableContractWindowsBudgetsAreDocumentedAndNarrow) {
  using testing::PortableNumericClass;
  if (testing::kPortableNumericExactPlatform) {
    return;  // executed only on the Windows side of the contract.
  }
  // Budgets must accept the recorded worst case with the documented
  // margin and must still reject real algorithmic deviations.
  const double replay = 0.1120646254094094;
  const double replayDev = std::nextafter(
      std::nextafter(replay, 0.0), 0.0);  // 1 ULP below
  CHECK(testing::withinPortableNumericContract(replayDev, replay,
                                               PortableNumericClass::kReplayValue));
  CHECK(!testing::withinPortableNumericContract(replay * (1.0 + 1e-9), replay,
                                                PortableNumericClass::kReplayValue));
  const double near = 1.0e-14;
  CHECK(testing::withinPortableNumericContract(near + 1.0e-16, near,
                                               PortableNumericClass::kNearZero1e15));
  CHECK(!testing::withinPortableNumericContract(near + 1.0e-12, near,
                                                PortableNumericClass::kNearZero1e15));
  CHECK(testing::withinPortableNumericContract(70.224, 70.224,
                                               PortableNumericClass::kDijMatrixValue));
  CHECK(!testing::withinPortableNumericContract(70.224, 70.0,
                                                PortableNumericClass::kDijMatrixValue));
  // The ULP budgets accept a few ULP and reject large distances.
  CHECK(testing::withinPortableNumericContract(
      std::nextafter(std::nextafter(1.0, 2.0), 2.0), 1.0,
      PortableNumericClass::kUlpBudget128));
  CHECK(!testing::withinPortableNumericContract(2.0, 1.0,
                                                PortableNumericClass::kUlpBudget128));
  CHECK(!testing::withinPortableNumericContract(2.0, 1.0,
                                                PortableNumericClass::kUlpBudget1024));
}

TEST(unit, PortableContractNeverAcceptsNaNDivergence) {
  using testing::PortableNumericClass;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!testing::withinPortableNumericContract(nan, 1.0,
                                                PortableNumericClass::kUlpBudget128));
  CHECK(!testing::withinPortableNumericContract(1.0, nan,
                                                PortableNumericClass::kDijMatrixValue));
  CHECK(!testing::withinPortableNumericContract(nan, 0.0,
                                                PortableNumericClass::kNearZero1e15));
}

// PUB-W3: the shared absolute/relative predicate and the three new
// measured reference classes.
TEST(unit, PortableContractReferenceClasses) {
  using testing::PortableNumericClass;
  // abs OR rel semantics, platform independent.
  CHECK(testing::withinAbsoluteRelativeContract(2.0, 2.0 + 5e-13, 1e-12, 1e-9));
  CHECK(testing::withinAbsoluteRelativeContract(1e9 + 500.0, 1e9, 1e-12, 1e-6));
  CHECK(!testing::withinAbsoluteRelativeContract(3.0, 2.0, 1e-12, 1e-9));
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  CHECK(!testing::withinAbsoluteRelativeContract(nan, 1.0, 1.0, 1.0));
  CHECK(!testing::withinAbsoluteRelativeContract(1.0, inf, 1.0, 1.0));
  // Near-zero values pass via the absolute branch although the ULP
  // distance explodes (the PUB-W3 VELS lesson).
  CHECK(testing::withinAbsoluteRelativeContract(1.0e-16, 2.0e-16, 1e-14, 2e-10));
  CHECK(!testing::withinAbsoluteRelativeContract(1.0e-13, 0.0, 1e-14, 2e-10));
#ifdef _WIN32
  CHECK(testing::withinPortableNumericContract(
      1.0 + 1e-14, 1.0, PortableNumericClass::kFmomReferenceValue));
  CHECK(testing::withinPortableNumericContract(
      1.0e-16, 2.0e-16, PortableNumericClass::kVelsReferenceValue));
  CHECK(!testing::withinPortableNumericContract(
      2.0, 1.0, PortableNumericClass::kCpiReferenceValue));
  CHECK(!testing::withinPortableNumericContract(
      nan, 1.0, PortableNumericClass::kVelsReferenceValue));
#else
  // Exact platform: any differing bit pair is rejected for the
  // bitwise-exact reference classes.
  CHECK(testing::withinPortableNumericContract(
      1.0, 1.0, PortableNumericClass::kVelsReferenceValue));
  const double onePlus = std::nextafter(1.0, 2.0);
  CHECK(!testing::withinPortableNumericContract(
      onePlus, 1.0, PortableNumericClass::kVelsReferenceValue));
  CHECK(!testing::withinPortableNumericContract(
      onePlus, 1.0, PortableNumericClass::kFmomReferenceValue));
  CHECK(!testing::withinPortableNumericContract(
      onePlus, 1.0, PortableNumericClass::kCpiReferenceValue));
#endif
}

// End: tests/unit/PortableNumericContractTests.cpp

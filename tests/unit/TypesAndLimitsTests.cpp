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

// File: tests/unit/TypesAndLimitsTests.cpp
// tests for the type-safe dimension constants (limits.h) and the
// base types (types.h). The constant values are pinned directly
// to the canonical Fortran dimensions.
// Note: these tests check the precise angle conversion
// (std::numbers::pi).
// the two conversions are intentionally kept separate.

#include "TestSupport.h"

#include <type_traits>

#include "xfoil/Limits.h"
#include "xfoil/Types.h"

// Compile-time checks.
static_assert(xfoil::limits::surfaceNodeDimension == 601);
static_assert(xfoil::limits::bufferNodeDimension == 2404);
static_assert(xfoil::limits::maxInputCoordinates == 2404);

TEST(unit, LimitsMatchCanonicalDimensions) {
  CHECK_EQ(xfoil::limits::surfaceNodeDimension, std::size_t{601});
  CHECK_EQ(xfoil::limits::halfSurfaceNodeDimension, std::size_t{300});
  CHECK_EQ(xfoil::limits::wakeNodeDimension, std::size_t{77});
  CHECK_EQ(xfoil::limits::qspecDistributionDimension, std::size_t{5});
  CHECK_EQ(xfoil::limits::sideDimension, std::size_t{2});
  CHECK_EQ(xfoil::limits::bufferNodeDimension, std::size_t{2404});
  CHECK_EQ(xfoil::limits::panelWakeNodeDimension, std::size_t{678});
  CHECK_EQ(xfoil::limits::boundaryLayerNodeDimension, std::size_t{427});
  CHECK_EQ(xfoil::limits::circlePlanePointDimension, std::size_t{257});
  CHECK_EQ(xfoil::limits::mappingCoefficientDimension, std::size_t{64});
  CHECK_EQ(xfoil::limits::quarterMappingCoefficientDimension, std::size_t{16});
  CHECK_EQ(xfoil::limits::midpointCount, std::size_t{100});
}

TEST(unit, NamespacedLimitsMatchCanonicalLimits) {
  // semantic Fortran dimensions (category A) with direct
  // value pins.
  CHECK_EQ(xfoil::limits::surfaceNodeDimension, std::size_t{601});
  CHECK_EQ(xfoil::limits::halfSurfaceNodeDimension, std::size_t{300});
  CHECK_EQ(xfoil::limits::wakeNodeDimension, std::size_t{77});
  CHECK_EQ(xfoil::limits::qspecDistributionDimension, std::size_t{5});
  CHECK_EQ(xfoil::limits::sideDimension, std::size_t{2});
  CHECK_EQ(xfoil::limits::bufferNodeDimension, std::size_t{2404});
  CHECK_EQ(xfoil::limits::panelWakeNodeDimension, std::size_t{678});
  CHECK_EQ(xfoil::limits::boundaryLayerNodeDimension, std::size_t{427});
  CHECK_EQ(xfoil::limits::circlePlanePointDimension, std::size_t{257});
  CHECK_EQ(xfoil::limits::mappingCoefficientDimension, std::size_t{64});
  CHECK_EQ(xfoil::limits::quarterMappingCoefficientDimension, std::size_t{16});
  CHECK_EQ(xfoil::limits::midpointCount, std::size_t{100});
  CHECK_EQ(xfoil::limits::maxInputCoordinates, std::size_t{2404});
}

TEST(unit, StorageConstantsMatchFachdimensionRelations) {
  // Physical storage constants (category B) and their relations to the
 // semantic domain dimensions.
  CHECK_EQ(xfoil::limits::surfaceNodeStorage,
           xfoil::limits::surfaceNodeDimension + 1);
  CHECK_EQ(xfoil::limits::surfaceNodeStorage, std::size_t{602});
  CHECK_EQ(xfoil::limits::panelWakeNodeStorage,
           xfoil::limits::panelWakeNodeDimension + 1);
  CHECK_EQ(xfoil::limits::panelWakeNodeStorage, std::size_t{679});
  CHECK_EQ(xfoil::limits::qspecDistributionStorage,
           xfoil::limits::qspecDistributionDimension + 1);
  CHECK_EQ(xfoil::limits::qspecDistributionStorage, std::size_t{6});
  CHECK_EQ(xfoil::limits::sideStorage, xfoil::limits::sideDimension + 1);
  CHECK_EQ(xfoil::limits::sideStorage, std::size_t{3});
  CHECK_EQ(xfoil::limits::camberNodeStorage,
           xfoil::limits::bufferNodeDimension + 1);
  CHECK_EQ(xfoil::limits::camberNodeStorage, std::size_t{2405});
  CHECK_EQ(xfoil::limits::bufferNodeStorage,
           xfoil::limits::bufferNodeDimension);
  CHECK_EQ(xfoil::limits::bufferNodeStorage, std::size_t{2404});
  // Wake/BL reserves stay larger than the semantic domains.
  CHECK(xfoil::limits::wakeNodeStorage >
        xfoil::limits::wakeNodeDimension + 1);
  CHECK_EQ(xfoil::limits::wakeNodeStorage, std::size_t{79});
  CHECK(xfoil::limits::boundaryLayerNodeStorage >
        xfoil::limits::boundaryLayerNodeDimension + 1);
  CHECK_EQ(xfoil::limits::boundaryLayerNodeStorage, std::size_t{446});
  CHECK_EQ(xfoil::limits::snewNodeStorage,
           5 * xfoil::limits::bufferNodeDimension + 1);
  CHECK_EQ(xfoil::limits::workNodeStorage,
           6 * xfoil::limits::surfaceNodeDimension + 1);
}

TEST(unit, BufferSemanticsAndReaderCapacity) {
  // Buffer semantics: AREAD/reader capacity = IBX = 2404,
  // maximum successful real-point count NB = 2403, GETCAM output index =
  // NB + 1 = 2404.
  CHECK_EQ(xfoil::limits::maxInputCoordinates,
           xfoil::limits::bufferNodeDimension);
  CHECK_EQ(xfoil::limits::maxInputCoordinates, std::size_t{2404});
  CHECK_EQ(xfoil::limits::bufferNodeStorage, std::size_t{2404});
  CHECK_EQ(xfoil::limits::bufferNodeStorage - 1, std::size_t{2403});
  // camberNodeStorage must safely hold the GETCAM output index 2404.
  CHECK(xfoil::limits::camberNodeStorage > 2404);
  CHECK_EQ(xfoil::limits::camberNodeStorage, std::size_t{2405});
}

TEST(unit, MixedInverseCapacityConstantsMatchCanonicalDerivation) {
  CHECK_EQ(xfoil::limits::maxRegularPanelNodes, std::size_t{596});
  CHECK_EQ(xfoil::limits::mixedInverseExtraUnknowns, std::size_t{5});
  CHECK_EQ(xfoil::limits::mixedInverseMaximumSystemIndex, std::size_t{601});
  CHECK_EQ(xfoil::limits::mixedInverseSystemStorage, std::size_t{602});

 // Derivation from the semantic domain dimension IQX.
  CHECK_EQ(xfoil::limits::maxRegularPanelNodes,
           xfoil::limits::surfaceNodeDimension - 5);
  CHECK_EQ(xfoil::limits::mixedInverseMaximumSystemIndex,
           xfoil::limits::maxRegularPanelNodes +
               xfoil::limits::mixedInverseExtraUnknowns);
  CHECK_EQ(xfoil::limits::mixedInverseSystemStorage,
           xfoil::limits::mixedInverseMaximumSystemIndex + 1);
  CHECK(xfoil::limits::mixedInverseSystemStorage >=
        xfoil::limits::surfaceNodeStorage);
}

// Compile-time limits of the mixed inverse system.
static_assert(xfoil::limits::maxRegularPanelNodes == 596);
static_assert(xfoil::limits::mixedInverseExtraUnknowns == 5);
static_assert(xfoil::limits::mixedInverseMaximumSystemIndex == 601);
static_assert(xfoil::limits::mixedInverseSystemStorage == 602);

TEST(unit, PointAndVectorDefaultInitializeToZero) {
  constexpr xfoil::Point2D point{};
  constexpr xfoil::Vector2D vector{};

  CHECK_EQ(point.x, 0.0);
  CHECK_EQ(point.y, 0.0);
  CHECK_EQ(vector.x, 0.0);
  CHECK_EQ(vector.y, 0.0);

  // the value types have no implicit conversions.
  static_assert(!std::is_convertible_v<double, xfoil::Degrees>);
  static_assert(!std::is_convertible_v<double, xfoil::Radians>);
  static_assert(!std::is_convertible_v<xfoil::Degrees, double>);
  static_assert(!std::is_convertible_v<xfoil::Radians, double>);
}

TEST(unit, DegreesRadiansRoundTrip) {
  const double degrees[] = {0.0, 90.0, -45.0};

  for (const double angle : degrees) {
    const xfoil::Radians radians = xfoil::toRadians(xfoil::Degrees{angle});
    const xfoil::Degrees back = xfoil::toDegrees(radians);
    CHECK_NEAR(back.value, angle, 1e-14, 0.0);
  }

  // 90 degrees correspond to exactly pi/2 (within the given tolerance).
  const xfoil::Radians quarterTurn =
      xfoil::toRadians(xfoil::Degrees{90.0});
  CHECK_NEAR(quarterTurn.value, 1.5707963267948966, 1e-14, 0.0);
}

TEST(unit, ToRadiansBitIdenticalToFortranDtorAdeg) {
  // Fortran: DTOR = PI/180 (xfoil.f INIT, rounded once) and
  // Alfa = DTOR*ADEG (xoper.f 424). The reference bits derive from the
  // canonical Fortran path; the evaluation order
  // (Degrees*(PI/180)) is binding, not (Degrees*PI)/180.
  const struct {
    double degrees;
    double expected;
  } cases[] = {
      {0.5, 0.008726646259971648},   {1.0, 0.017453292519943295},
      {1.5, 0.026179938779914945},   {2.0, 0.03490658503988659},
      {9.5, 0.16580627893946132},    {10.0, 0.17453292519943295},
      {13.5, 0.23561944901923448},   {14.5, 0.2530727415391778},
  };
  for (const auto& c : cases) {
    const double alfa = xfoil::toRadians(xfoil::Degrees{c.degrees}).value;
    CHECK_EQ(alfa, c.expected);
  }
}

// End: tests/unit/TypesAndLimitsTests.cpp

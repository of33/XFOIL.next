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

// File: tests/unit/AirfoilGeometryTests.cpp
// unit tests for the ownership-semantics value type
// xfoil::geometry::AirfoilGeometry.

#include "TestSupport.h"

#include <cstddef>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

#include "xfoil/Types.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

std::vector<xfoil::Point2D> makeTriangle() {
  return {
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{1.0, 0.0},
      xfoil::Point2D{0.0, 1.0},
  };
}

}  // namespace

TEST(unit, AirfoilGeometryStartsEmpty) {
  const xfoil::geometry::AirfoilGeometry geometry;
  CHECK(geometry.empty());
  CHECK_EQ(geometry.pointCount(), static_cast<std::size_t>(0));
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(0));
  CHECK(geometry.name().empty());
  CHECK_EQ(geometry.coordinates().size(), static_cast<std::size_t>(0));
}

TEST(unit, AirfoilGeometryAcceptsThreePoints) {
  xfoil::geometry::AirfoilGeometry geometry;
  const std::vector<xfoil::Point2D> points = makeTriangle();

  const auto result = geometry.assign("TRI", points);

  CHECK(result.success());
  CHECK_EQ(
      static_cast<int>(result.error),
      static_cast<int>(xfoil::geometry::GeometryError::none));
  CHECK(!geometry.empty());
  CHECK_EQ(geometry.pointCount(), static_cast<std::size_t>(3));
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(1));
  CHECK_EQ(geometry.name(), std::string_view("TRI"));
}

TEST(unit, AirfoilGeometryOwnsAssignedCoordinates) {
  xfoil::geometry::AirfoilGeometry geometry;
  std::vector<xfoil::Point2D> points = makeTriangle();

  const auto result = geometry.assign("OWN", points);
  CHECK(result.success());

 // Input changed after a successful assign; the stored
  // coordinates must remain unchanged (the vector owns its data).
  points[0].x = 99.0;
  points[1].y = -42.0;
  points.push_back(xfoil::Point2D{5.0, 5.0});

  CHECK_EQ(geometry.pointCount(), static_cast<std::size_t>(3));
  const auto stored = geometry.coordinates();
  CHECK_NEAR(stored[0].x, 0.0, 0.0, 0.0);
  CHECK_NEAR(stored[0].y, 0.0, 0.0, 0.0);
  CHECK_NEAR(stored[1].x, 1.0, 0.0, 0.0);
  CHECK_NEAR(stored[1].y, 0.0, 0.0, 0.0);
  CHECK_NEAR(stored[2].x, 0.0, 0.0, 0.0);
  CHECK_NEAR(stored[2].y, 1.0, 0.0, 0.0);
}

TEST(unit, AirfoilGeometryRejectsTooFewPointsWithoutChangingState) {
  xfoil::geometry::AirfoilGeometry geometry;
  const std::vector<xfoil::Point2D> points = makeTriangle();
  CHECK(geometry.assign("KEEP", points).success());

  const std::vector<xfoil::Point2D> tooFew = {
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{1.0, 1.0},
  };
  const auto result = geometry.assign("GONE", tooFew);

  CHECK(!result.success());
  CHECK_EQ(
      static_cast<int>(result.error),
      static_cast<int>(xfoil::geometry::GeometryError::tooFewPoints));
  CHECK_EQ(
      result.message,
      std::string_view("Airfoil geometry requires at least 3 points"));

  // Name, coordinates and revision remain unchanged.
  CHECK_EQ(geometry.name(), std::string_view("KEEP"));
  CHECK_EQ(geometry.pointCount(), static_cast<std::size_t>(3));
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(1));
}

TEST(unit, AirfoilGeometryRejectsNan) {
  xfoil::geometry::AirfoilGeometry geometry;
  const double nanValue = std::numeric_limits<double>::quiet_NaN();
  const std::vector<xfoil::Point2D> points = {
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{1.0, nanValue},
      xfoil::Point2D{0.0, 1.0},
  };

  const auto result = geometry.assign("NAN", points);

  CHECK(!result.success());
  CHECK_EQ(
      static_cast<int>(result.error),
      static_cast<int>(xfoil::geometry::GeometryError::nonFiniteCoordinate));
  CHECK_EQ(result.pointIndex, static_cast<std::size_t>(1));
  CHECK_EQ(
      result.message,
      std::string_view(
          "Airfoil geometry contains a non-finite coordinate"));
  CHECK(geometry.empty());
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(0));
}

TEST(unit, AirfoilGeometryRejectsInfinity) {
  xfoil::geometry::AirfoilGeometry geometry;
  const double infinity = std::numeric_limits<double>::infinity();
  const std::vector<xfoil::Point2D> points = {
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{1.0, 0.0},
      xfoil::Point2D{-infinity, 1.0},
  };

  const auto result = geometry.assign("INF", points);

  CHECK(!result.success());
  CHECK_EQ(
      static_cast<int>(result.error),
      static_cast<int>(xfoil::geometry::GeometryError::nonFiniteCoordinate));
  CHECK_EQ(result.pointIndex, static_cast<std::size_t>(2));
  CHECK(geometry.empty());
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(0));
}

TEST(unit, AirfoilGeometryRevisionIncrementsOnSuccessfulAssignment) {
  xfoil::geometry::AirfoilGeometry geometry;
  const std::vector<xfoil::Point2D> points = makeTriangle();
  const std::vector<xfoil::Point2D> other = {
      xfoil::Point2D{0.0, 0.0},
      xfoil::Point2D{2.0, 0.0},
      xfoil::Point2D{0.0, 2.0},
  };

  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(0));

  CHECK(geometry.assign("EINS", points).success());
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(1));

  const std::vector<xfoil::Point2D> tooFew = {
      xfoil::Point2D{7.0, 7.0},
  };
  CHECK(!geometry.assign("ERROR", tooFew).success());
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(1));
  CHECK_EQ(geometry.name(), std::string_view("EINS"));

  CHECK(geometry.assign("ZWEI", other).success());
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(2));
  CHECK_EQ(geometry.pointCount(), static_cast<std::size_t>(3));
}

TEST(unit, AirfoilGeometryDoesNotApplyOldPointLimit) {
  xfoil::geometry::AirfoilGeometry geometry;

  std::vector<xfoil::Point2D> points;
  points.reserve(700);
  for (std::size_t index = 0; index < 700; ++index) {
    const double position = static_cast<double>(index);
    points.push_back(xfoil::Point2D{position, position * 0.5});
  }

  const auto result = geometry.assign("VIELE", points);

  CHECK(result.success());
  CHECK_EQ(geometry.pointCount(), static_cast<std::size_t>(700));
  CHECK_EQ(geometry.revision(), static_cast<std::size_t>(1));
}

TEST(unit, AirfoilGeometryCanReassignFromItsOwnCoordinateView) {
  xfoil::geometry::AirfoilGeometry geometry;
  const std::vector<xfoil::Point2D> points = makeTriangle();

  CHECK(geometry.assign("FIRST", points).success());

  const auto ownCoordinates = geometry.coordinates();
  const auto result = geometry.assign("SECOND", ownCoordinates);

  CHECK(result.success());
  CHECK_EQ(geometry.name(), std::string_view("SECOND"));
  CHECK_EQ(geometry.pointCount(), std::size_t{3});
  CHECK_EQ(geometry.revision(), std::size_t{2});

  const auto stored = geometry.coordinates();
  CHECK_NEAR(stored[0].x, 0.0, 0.0, 0.0);
  CHECK_NEAR(stored[1].x, 1.0, 0.0, 0.0);
  CHECK_NEAR(stored[2].y, 1.0, 0.0, 0.0);
}

// End: tests/unit/AirfoilGeometryTests.cpp

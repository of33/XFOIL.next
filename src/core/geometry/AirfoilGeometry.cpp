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

// File: src/core/geometry/AirfoilGeometry.cpp
// Implementation of the ownership type for unpaneled profile coordinates.
// See include/xfoil/geometry/AirfoilGeometry.h.

#include "xfoil/geometry/AirfoilGeometry.h"

#include <cmath>

namespace xfoil::geometry {

namespace {

constexpr std::string_view kTooFewPointsMessage =
    "Airfoil geometry requires at least 3 points";
constexpr std::string_view kNonFiniteCoordinateMessage =
    "Airfoil geometry contains a non-finite coordinate";

}  // namespace

GeometryAssignmentResult AirfoilGeometry::assign(
    std::string_view name,
    std::span<const xfoil::Point2D> coordinates) {
  GeometryAssignmentResult result;

  if (coordinates.size() < 3) {
    result.error = GeometryError::tooFewPoints;
    result.message = kTooFewPointsMessage;
    return result;
  }

  for (std::size_t index = 0; index < coordinates.size(); ++index) {
    const xfoil::Point2D& point = coordinates[index];
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      result.error = GeometryError::nonFiniteCoordinate;
      result.pointIndex = index;
      result.message = kNonFiniteCoordinateMessage;
      return result;
    }
  }

  // Both potentially allocating operations complete before the visible
  // object state changes. If an allocation fails, the existing geometry
  // stays unchanged.
  std::string newName(name);
  std::vector<xfoil::Point2D> newCoordinates(
    coordinates.begin(), coordinates.end());

  name_.swap(newName);
  coordinates_.swap(newCoordinates);
  ++revision_;
  return result;
}

void AirfoilGeometry::setName(std::string_view name) {
 // Pure value assignment without revision change.
  std::string newName(name);
  name_.swap(newName);
}

std::string_view AirfoilGeometry::name() const noexcept {
  return name_;
}

std::span<const xfoil::Point2D> AirfoilGeometry::coordinates() const noexcept {
  return coordinates_;
}

std::size_t AirfoilGeometry::pointCount() const noexcept {
  return coordinates_.size();
}

std::size_t AirfoilGeometry::revision() const noexcept {
  return revision_;
}

bool AirfoilGeometry::empty() const noexcept {
  return coordinates_.empty();
}

}  // namespace xfoil::geometry

// End: src/core/geometry/AirfoilGeometry.cpp

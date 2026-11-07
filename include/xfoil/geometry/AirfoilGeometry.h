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

// File: include/xfoil/geometry/AirfoilGeometry.h
// Owning value type for unpaneled airfoil coordinates.
// The type hardcodes no Fortran buffer limits, stores no external views, and
// writes neither to stdout nor stderr. Normal validation errors are
// reported via GeometryAssignmentResult, not via exceptions.

#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "xfoil/Types.h"

namespace xfoil::geometry {

enum class GeometryError {
  none,
  tooFewPoints,
  nonFiniteCoordinate
};

struct GeometryAssignmentResult {
  GeometryError error{GeometryError::none};
  std::size_t pointIndex{};
  std::string message;

  [[nodiscard]]
  bool success() const noexcept {
    return error == GeometryError::none;
  }
};

class AirfoilGeometry {
 public:
  AirfoilGeometry() = default;

  [[nodiscard]]
  GeometryAssignmentResult assign(
      std::string_view name,
      std::span<const xfoil::Point2D> coordinates);

 // Value-semantic rename: only the name is replaced;
  // coordinates and revision counters remain unchanged. It serves the
  // synchronization of the CLI profile name with the geometry mirror.
  void setName(std::string_view name);

  [[nodiscard]]
  std::string_view name() const noexcept;

  [[nodiscard]]
  std::span<const xfoil::Point2D> coordinates() const noexcept;

  [[nodiscard]]
  std::size_t pointCount() const noexcept;

  [[nodiscard]]
  std::size_t revision() const noexcept;

  [[nodiscard]]
  bool empty() const noexcept;

 private:
  std::string name_;
  std::vector<xfoil::Point2D> coordinates_;
  std::size_t revision_{};
};

}  // namespace xfoil::geometry

// End: include/xfoil/geometry/AirfoilGeometry.h

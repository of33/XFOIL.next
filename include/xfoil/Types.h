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

// File: include/xfoil/Types.h
// Basic modern value types: points, vectors and angle.
// The angle conversions use the precise std::numbers::pi basis
// (modern API). The CLI numeric paths keep their own
// deliberately rounded PI constants, so that the golden results
// stay equal.

#pragma once

#include <numbers>

namespace xfoil {

struct Point2D {
  double x{};
  double y{};
};

struct Vector2D {
  double x{};
  double y{};
};

struct Degrees {
  double value{};
};

struct Radians {
  double value{};
};

[[nodiscard]]
constexpr Radians toRadians(Degrees degrees) noexcept {
  // Fortran: DTOR = PI/180 (xfoil.f INIT, rounded once) and
  // Alfa = DTOR*ADEG (xoper.f 424). The rounded DTOR must exist as
  // an intermediate value; (degrees*PI)/180 rounds differently
  // (1 ULP for e.g. 1.5 degrees).
  return Radians{
      degrees.value * (std::numbers::pi_v<double> / 180.0)
  };
}

[[nodiscard]]
constexpr Degrees toDegrees(Radians radians) noexcept {
  return Degrees{
      radians.value * 180.0 / std::numbers::pi_v<double>
  };
}

}  // namespace xfoil

// End: include/xfoil/Types.h

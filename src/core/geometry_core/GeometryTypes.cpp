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

// File: src/core/geometry_core/GeometryTypes.cpp
// Implementation of the geometry value types.

#include "xfoil/core/geometry/GeometryTypes.h"

#include <stdexcept>
#include <utility>

namespace xfoil::core::geometry {

AirfoilCoordinates::AirfoilCoordinates(std::vector<double> x_,
                                       std::vector<double> y_)
    : x(std::move(x_)), y(std::move(y_)) {
  if (x.size() != y.size()) {
    throw std::invalid_argument(
        "AirfoilCoordinates: x/y length inconsistency");
  }
}

AirfoilBuffer::AirfoilBuffer(std::vector<double> x_, std::vector<double> y_,
                             std::string name_)
    : x(std::move(x_)), y(std::move(y_)), name(std::move(name_)) {
  if (x.size() != y.size()) {
    throw std::invalid_argument(
        "AirfoilBuffer: x/y length inconsistency");
  }
}

}  // namespace xfoil::core::geometry

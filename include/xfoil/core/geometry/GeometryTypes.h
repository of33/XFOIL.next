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

// File: include/xfoil/core/geometry/GeometryTypes.h
// Minimal, separated geometry value types of the new
// 0-based cores.
//
// - AirfoilCoordinates: 0-based point contour (X,Y), x.size==y.size.
// - AirfoilBuffer: buffer contour with a name; separate from the later
// panel contour and from the active contour.
// - SplineProfile: spline-fitted contour incl. arc length, spline derivatives
// and spline LE position (Fortran X,XP,Y,YP,S,N,SLE).
//
// No hidden panel or solver semantics, no raw pointers, no
// dependency on app, CLI, or tests.

#pragma once

#include <string>
#include <vector>

namespace xfoil::core::geometry {

// 0-based point contour. The x/y length invariant is checked in the
// constructor; a violation throws std::invalid_argument without partial state.
struct AirfoilCoordinates {
  std::vector<double> x;
  std::vector<double> y;

  AirfoilCoordinates() = default;
  AirfoilCoordinates(std::vector<double> x_, std::vector<double> y_);
};

// Buffer contour with name (e.g. "NACA 0012"). Buffer contour and later
// panel contour remain separate types.
struct AirfoilBuffer {
  std::vector<double> x;
  std::vector<double> y;
  std::string name;

  AirfoilBuffer() = default;
  AirfoilBuffer(std::vector<double> x_, std::vector<double> y_,
                std::string name_);
};

// Spline-fitted contour: coordinates, arc length, spline derivatives and the
// computed spline LE position. The discrete LE coordinate and the computed
// spline LE position are deliberately separated quantities (see LOAD LE finding).
struct SplineProfile {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> s;
  std::vector<double> xp;
  std::vector<double> yp;
  double sle{0.0};
};

}  // namespace xfoil::core::geometry

// End: include/xfoil/core/geometry/GeometryTypes.h

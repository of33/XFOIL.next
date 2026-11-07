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

// File: src/core/paneling/PanelTypes.cpp
// Factory with length-invariant check of the panel contour.

#include "xfoil/core/paneling/PanelingTypes.h"

#include <stdexcept>
#include <string>

namespace xfoil::core::paneling {

// Fortran: X,XP,Y,YP,S,N,SLE,XLE,YLE,XTE,YTE,CHORD,SHARP,NX,NY,APANEL.
PanelGeometry PanelGeometry::make(std::vector<double> x,
                                  std::vector<double> y,
                                  std::vector<double> s,
                                  std::vector<double> xp,
                                  std::vector<double> yp,
                                  std::vector<double> xn,
                                  std::vector<double> yn,
                                  std::vector<double> apanel, double sle,
                                  double xle, double yle, double xte,
                                  double yte, double chord, bool sharpTe) {
  const std::size_t n = x.size();
  if (y.size() != n || s.size() != n || xp.size() != n || yp.size() != n ||
      xn.size() != n || yn.size() != n || apanel.size() != n) {
    throw std::invalid_argument(
        "xfoil::core::paneling::PanelGeometry::make: all lengths must be equal");
  }

  PanelGeometry panel;
  panel.x = std::move(x);
  panel.y = std::move(y);
  panel.s = std::move(s);
  panel.xp = std::move(xp);
  panel.yp = std::move(yp);
  panel.xn = std::move(xn);
  panel.yn = std::move(yn);
  panel.apanel = std::move(apanel);
  panel.sle = sle;
  panel.xle = xle;
  panel.yle = yle;
  panel.xte = xte;
  panel.yte = yte;
  panel.chord = chord;
  panel.sharpTe = sharpTe;
  return panel;
}

}  // namespace xfoil::core::paneling

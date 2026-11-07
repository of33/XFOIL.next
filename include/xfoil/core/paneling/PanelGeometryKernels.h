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

// File: include/xfoil/core/paneling/PanelGeometryKernels.h
// Panel geometry kernel of the new 0-based cores, directly
// ported from fortran-src/xpanel.f (APCALC, NCALC) and the
// geometry part of fortran-src/xfoil.f TECALC.
//
// Contained routines:
// APCALC -> apcalc NCALC -> ncalc TECALC -> teGeometry
//
// All routines are 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).
// Formulas, operation order, and special cases (degenerate panels,
// degenerate normals, sharp TE) are adopted exactly from the Fortran.
//
// TECALC is ported only in its geometry part (DSTE, ANTE, ASTE, SCS,
// SDS, SHARP); the SIGTE/GAMTE part needs a solved gamma
// distribution (solver state) and is reserved for later
// steps.
//
// The numerical kernel does not write to stdout. No raw-pointer API,
// no dependency on app, CLI, or tests.

#pragma once

#include <span>
#include <vector>

namespace xfoil::core::paneling {

// Fortran TECALC geometry part: initial group of the trailing-edge region
// without SIGTE/GAMTE (solver state).
struct TrailingEdgeGeometry {
  bool sharp{false};  // Fortran SHARP
  double dste{0.0};
  double ante{0.0};
  double aste{0.0};
  double scs{0.0};
  double sds{0.0};
};

// Safety contract: all routines check their span lengths before
// use. Invalid sizes throw std::invalid_argument.

// Fortran: SUBROUTINE TECALC (geometry part)
// Blunt or sharp trailing-edge geometry of the panel contour
// (DXT/DYT/DXS/DYS, ANTE, ASTE, DSTE, SCS, SDS, SHARP=DSTE<0.0001*CHORD).
// ch must be positive. N >= 1 (only nodes 0 and n-1 are used).
TrailingEdgeGeometry teGeometry(double x0, double y0, double xp0, double yp0,
                                double xn, double yn, double xpn, double ypn,
                                double chord);

// Fortran: SUBROUTINE NCALC(X,Y,S,N,XN,YN)
// Sets the contour normals (in-place): SEGSPL on x/y, normalization,
// then corner averaging for duplicated points. xn/yn are, as in
// Fortran, first used as SEGSPL work arrays and then
// overwritten with the normals. N <= 1 leaves xn/yn unchanged.
// All lengths must be equal.
void ncalc(std::span<const double> x, std::span<const double> y,
           std::span<const double> s, std::span<double> xn,
           std::span<double> yn);

// Fortran: SUBROUTINE APCALC (no arguments, COMMON-based)
// Computes the panel angle APANEL(1..N) from the panel normals and the
// TE segment geometry. The TE panel (index n-1) uses the value PI for a
// sharp trailing edge, otherwise the segment direction. Returns
// the angle list (n entries). All lengths must be equal.
// N >= 1.
std::vector<double> apcalc(std::span<const double> x,
                           std::span<const double> y,
                           std::span<const double> xn,
                           std::span<const double> yn, bool sharpTe);

}  // namespace xfoil::core::paneling

// End: include/xfoil/core/paneling/PanelGeometryKernels.h

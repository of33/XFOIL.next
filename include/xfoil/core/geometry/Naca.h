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

// File: include/xfoil/core/geometry/Naca.h
// NACA core, directly ported from fortran-src/naca.f and the
// NACA call context in fortran-src/xfoil.f.
//
// Contained routines:
// NACA4 -> naca4 NACA5 -> naca5 NACA -> nacaAirfoil
//
// All operations are 0-based (Fortran 1..NSIDE <-> C++ 0..NSIDE-1).
// Formulas, operation order, and point placement (only whether the LE side
// comes after the TE, then the lower side from point 2) are adopted exactly
// from the Fortran. No raw-pointer API, no dependency on app or CLI.
//
// NACA (xfoil.f) is modelled as a core operation, not as a CLI dialog:
// the dialog (ASKI) and PANGEN are not part of it. S01 deliberately
// retains the XFOIL 6.99 NSIDE contract of 123 (buffer point count
// NB = 245); the 6.996 NACA point-count change belongs to a later slice.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"

namespace xfoil::core::geometry {

// Fortran: SUBROUTINE NACA4(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
// Four-digit NACA airfoil. XX/YT/YC are the NSIDE-wide side arrays
// (0..NSIDE-1), XB/YB the buffer contour with NB = 2*NSIDE-1 points
// (0..2*NSIDE-2). ides must lie in 0..9999; NACA4 requires this
// limit from its caller NACA (digit access); directly invalid
// identifiers yield a controlled error. nside >= 2.
struct NacaResult {
  std::vector<double> xx;  // side array XX (Fortran 1..NSIDE)
  std::vector<double> yt;  // Thickness distribution YT
  std::vector<double> yc;  // Camber line YC
  std::vector<double> xb;  // Buffer contour X (Fortran 1..NB)
  std::vector<double> yb;  // Buffer contour Y
  std::string name;        // e.g. "NACA 0012" (Name(1:9) after padding)
};

// Fortran: SUBROUTINE NACA5(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
// Five-digit NACA airfoil. For an invalid identifier (N543 not
// 210..250) Fortran sets IDES=0 and returns no points; here
// the same case is returned as a controlled error with the Fortran
// message text (no partial mutation).
struct Naca5Result {
  std::vector<double> xx;
  std::vector<double> yt;
  std::vector<double> yc;
  std::vector<double> xb;
  std::vector<double> yb;
  std::string name;  // e.g. "NACA 23012" (Name(1:10) after padding)
};

// Result of the core operation NACA (xfoil.f): buffer contour with name plus
// the geometry parameters computed in the calling context (SCALC/SEGSPL/GEOPAR
// as in xfoil.f NACA; PANGEN follows).
struct NacaAirfoilResult {
  AirfoilBuffer buffer;
  GeometryMetrics metrics;
};

Result<NacaResult> naca4(int ides, std::size_t nside);

Result<Naca5Result> naca5(int ides, std::size_t nside);

// Fortran: SUBROUTINE NACA(IDES1) without dialog and without PANGEN.
// NSIDE = 123 (retained 6.99 contract in S01), NB = 245. ITYPE rules as in
// xfoil.f: IDES<=9999 -> NACA4, IDES<=25099 -> NACA5, otherwise
// 'This designation not implemented.' as a controlled error.
// IDES <= 0 corresponds to the Fortran dialog case and also returns
// a controlled error. The geometry parameters follow from the
// SCALC/SEGSPL/LEFIND/GEOPAR chain as in the Fortran calling context.
Result<NacaAirfoilResult> nacaAirfoil(int ides);

}  // namespace xfoil::core::geometry

// End: include/xfoil/core/geometry/Naca.h

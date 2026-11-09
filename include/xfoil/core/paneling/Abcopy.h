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

// File: include/xfoil/core/paneling/Abcopy.h
// ABCOPY of the new 0-based cores, directly ported from
// fortran-src/xgdes.f ABCOPY.
//
// ABCOPY sets the current panel contour directly from the buffer
// airfoil points (Fortran X(1..NB), Y(1..NB)), removes duplicated
// points exactly as in the Fortran, and computes the panel geometry
// (splines, normals, LE position, TE geometry, panel angle).
//
// Capacity limit (canonical): NB > IQX-5 = 596 is rejected in a
// controlled way; the current contour is then not set. The Fortran flap
// and solver flags are session state held outside the kernel.
//
// The numerical kernel does not write to stdout. No raw-pointer API,
// no dependency on app, CLI, or tests.

#pragma once

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::paneling {

// Fortran: SUBROUTINE ABCOPY(LCONF)
// Sets the current panel contour from the buffer contour. buffer must
// contain 2..596 points (smaller: 'ABCOPY: Buffer airfoil not
// available.'; larger: Fortran rejection without setting the contour).
// The LCONF parameter controls only the confirmation output in Fortran
// and is not present in the kernel (not represented).
Result<AbcopyResult> abcopy(const geometry::AirfoilBuffer& buffer);

}  // namespace xfoil::core::paneling

// End: include/xfoil/core/paneling/Abcopy.h

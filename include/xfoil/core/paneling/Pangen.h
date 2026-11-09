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

// File: include/xfoil/core/paneling/Pangen.h
// Paneling kernel of the new 0-based cores, directly
// ported from fortran-src/xfoil.f PANGEN.
//
// PANGEN produces the current panel contour from the buffer contour:
// default paneling (160 nodes) or PPAR-configured paneling,
// LE/TE refinement, Newton node distribution, corner node insertion,
// panel normals and panel angles.
//
// Formulas, operation order, and special cases (sharp LE/TE, blunt
// TE, corner node insertion, Newton continuation after 20 iterations) are
// adopted exactly from the Fortran. The Fortran STOP for too many panels
// (N > IQX-1 = 600) becomes a controlled error; the Newton limit
// is returned as structured diagnostics (limitReached) and the
// routine continues with the canonical Fortran behavior.
//
// TECALC is used only in its geometry part (SIGTE/GAMTE = solver state,
// later step). The numerical kernel does not write to stdout; the
// Fortran writes (sharp LE/TE, GAP, parameter output) are returned
// structured. No raw-pointer API, no dependency on
// app, CLI, or tests.

#pragma once

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::paneling {

// Fortran: SUBROUTINE PANGEN(SHOPAR)
// Produces the current panel contour from the buffer contour. buffer must
// contain at least 2 points (otherwise a controlled error as in the
// Fortran: 'PANGEN: Buffer airfoil not available.'). The details of the
// default and PPAR paneling are read from params.
// Substep instruction: routines that need a
// solved state are not contained here.
Result<PangenResult> pangen(const geometry::AirfoilBuffer& buffer,
                            const PanelingParameters& params);

}  // namespace xfoil::core::paneling

// End: include/xfoil/core/paneling/Pangen.h

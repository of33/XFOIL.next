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

// File: include/xfoil/core/inviscid/TrailingEdge.h
// Complete TECALC routine of the new 0-based cores,
// ported from fortran-src/xfoil.f TECALC.
//
// The geometry formulas (DXT/DYT/DXS/DYS, ANTE, ASTE, DSTE, SHARP, SCS/SDS)
// stay in the paneling::teGeometry kernel as the shared geometric
// basis (no duplicated formulas). It adds the SIGTE/GAMTE part:
// SIGTE = 0.5*(GAM(1) - GAM(N)) *SCS
// GAMTE = -.5*(GAM(1) - GAM(N)) *SDS
// SIGTE_A = 0.5*(GAM_A(1) - GAM_A(N))*SCS
// GAMTE_A = -.5*(GAM_A(1) - GAM_A(N))*SDS
//
// The Fortran routine status TECALC may only be set after a complete
// comparison on a completed status. No raw-pointer API,
// no dependency on app, CLI, or tests.

#pragma once

#include "xfoil/core/paneling/PanelGeometryKernels.h"
#include "xfoil/core/inviscid/InviscidTypes.h"

namespace xfoil::core::inviscid {

// Fortran: SUBROUTINE TECALC (complete).
// Computes the trailing-edge strengths from GAM/GAM_A on the basis of the
// geometry kernels teGeometry. chord must be positive.
TrailingEdgeState tecalcFull(double x0, double y0, double xp0, double yp0,
                             double xn, double yn, double xpn, double ypn,
                             double chord, double gam0, double gamN,
                             double gamA0, double gamAN);

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/TrailingEdge.h

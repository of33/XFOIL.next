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

// File: include/xfoil/core/boundary_layer/BoundaryLayerParameters.h
// BLPAR/BLPINI initialization of the new 0-based cores,
// ported from fortran-src/BLPAR.INC and fortran-src/xbl.f BLPINI.
//
// Canonical initial values (BLPINI, xbl.f:1578):
// SCCON = 5.6, GACON = 6.70, GBCON = 0.75, GCCON = 18.0, DLCON = 0.9,
// CTRCON = 1.8, CTRCEX = 3.3, DUXCON = 1.0,
// CTCON = 0.5/(GACON^2*GBCON), CFFAC = 1.0

#pragma once

#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: SUBROUTINE BLPINI (xbl.f:1578).
// Sets the canonical BLPAR constants (BLPAR.INC).
[[nodiscard]] BoundaryLayerParameters blparConstants();

// Fortran: SETBL data part (xbl.f:46-85).
// Sets the boundary-layer operating quantities from freestream and
// compressibility inputs. Inputs: minf (Fortran MINF), reinf
// (Fortran REINF), qinf (Fortran QINF), gamma/gamm1 (session constants),
// tklam/tkMsq (COMSET exit), hvrat (Fortran HVRAT,
// Sutherland ratio; never set canonically -> passed as 0.0),
// wgap (Fortran WGAP(1), TE gap), idamp (Fortran IDAMP, 0/1).
[[nodiscard]] BoundaryLayerProperties boundaryLayerProperties(
    double minf, double reinf, double qinf, double gamma, double gamm1,
    double tklam, double tkMsq, double hvrat, double wgap, int idamp);

}  // namespace xfoil::core::boundary_layer

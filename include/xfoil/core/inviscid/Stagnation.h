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

// File: include/xfoil/core/inviscid/Stagnation.h
// Stagnation-point and velocity kernels of the new 0-based cores,
// directly ported from fortran-src/xpanel.f (STFIND, QISET).
//
// Contained routines:
// STFIND -> stfind QISET -> qiset
//
// Formulas, operation order, and special cases (GAM sign change,
// tweak on node 1e-7, not found -> I = N/2) are adopted exactly from
// the Fortran. The STFIND message is returned structured.
// No raw-pointer API, no dependency on app, CLI, or tests.

#pragma once

#include <span>
#include <vector>

#include "xfoil/core/inviscid/InviscidTypes.h"

namespace xfoil::core::inviscid {

// Fortran: SUBROUTINE STFIND (no arguments, COMMON-based)
// Locates the stagnation point from the GAM sign change. s/s must
// have equal length. Not found -> is = n/2 (0-based) and
// found=false (structured message, canonical Fortran behavior).
StagnationPointResult stfind(std::span<const double> gam,
                             std::span<const double> s);

// Fortran: SUBROUTINE QISET (no arguments, COMMON-based)
// Sets the inviscid panel tangential velocities QINV and QINV_A from
// the alpha=0/90 base solutions QINVU. qinvu1/qinvu2 must have equal length
// (Fortran I=1..N+NW; 0-based 0..n+nw-1).
void qiset(std::span<const double> qinvu1, std::span<const double> qinvu2,
           double alfa, std::vector<double>& qinv,
           std::vector<double>& qinvA);

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/Stagnation.h

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

// File: include/xfoil/core/inviscid/Loads.h
// Load integration kernels of the new 0-based cores, directly
// ported from fortran-src/xfoil.f (CLCALC) and fortran-src/xoper.f
// (MHINGE, inviscid branch).
//
// Contained routines:
// CLCALC -> clCalc MHINGE -> mhinge
//
// Formulas, operation order, and special cases (TE segment IP=1,
// 1/12 term, FRAC interpolation) are taken exactly from the Fortran.
// The MHINGE LVISC branch (CPV, viscous coupling) is covered by the
// lvisc flag; the GETXYF dialog lives in the CLI (XOF/YOF arrive as input).
// No raw-pointer API, no dependency on app, CLI, or tests.

#pragma once

#include <span>
#include <vector>

#include "xfoil/core/inviscid/InviscidTypes.h"

namespace xfoil::core::inviscid {

// Fortran: SUBROUTINE CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF,XREF,YREF,
// CL,CM,CDP,CL_ALF,CL_MSQ)
// Integrates the surface pressures into Cl and CM and dCL/dAlpha and
// dCL/d(MINF^2). All lengths must be equal (x/y/gam/gamA).
LoadResult clCalc(std::span<const double> x, std::span<const double> y,
                  std::span<const double> gam, std::span<const double> gamA,
                  double alfa, double minf, double qinf, double xref,
                  double yref);

// Fortran: SUBROUTINE MHINGE (xoper.f, inviscid and LVISC branch)
// Integrates the surface pressures into the hinge moment about (xof, yof).
// The hinge position is expected as input (the Fortran GETXYF dialog
// lives in the CLI). For lvisc=true, CPV is integrated everywhere instead of
// CPI (Fortran IF(LVISC) branches); otherwise exclusively CPI.
// cpi and cpv must be at least n long (cpv needed only for lvisc).
HingeMomentResult mhinge(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s,
                         std::span<const double> cpi,
                         std::span<const double> cpv, bool lvisc,
                         double xof, double yof);

// SINVRT initial guesses for both canonical MHINGE paths:
// GETXYF path (LFLAP=false, xgdes.f 1887-1888):
// tops = S(1) + (X(1)-XOF), bots = S(N) - (X(N)-XOF)
// LFLAP=true path (xoper.f 2367-2368):
// tops = XOF, bots = S(N) - XOF
// The estimates are canonically different; SINVRT can converge to
// different ULP ranges depending on the start value, so the kernel
// runs both paths explicitly.
struct HingeArcGuess {
  double tops{0.0};
  double bots{0.0};
};
HingeMomentResult mhinge(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s,
                         std::span<const double> cpi,
                         std::span<const double> cpv, bool lvisc,
                         double xof, double yof,
                         const HingeArcGuess& guess);

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/Loads.h

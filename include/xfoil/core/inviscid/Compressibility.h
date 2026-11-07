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

// File: include/xfoil/core/inviscid/Compressibility.h
// Compressibility kernels of the new 0-based cores,
// directly ported from fortran-src/xfoil.f (MRCL, COMSET, CPCALC).
//
// Contained routines:
// MRCL -> MRCL COMSET -> comset CPCALC -> cpCalc
//
// Formulas, operation order, and limits (MINF >= 0.99 -> 0.99,
// RRAT > 100 -> REINF1*100) are adopted exactly from the Fortran. The
// invalid RETYP/MATYP correction of the Fortran (message + set to 1,
// session state) is modeled as a controlled rejection; the
// valid Fortran path stays unchanged.
//
// The numerical kernel does not write to stdout. No raw-pointer API,
// no dependency on app, CLI, or tests.

#pragma once

#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inviscid/InviscidTypes.h"

namespace xfoil::core::inviscid {

// Fortran gas constants (session constants, xfoil.f INIT).
inline constexpr double kGamma = 1.4;
inline constexpr double kGamm1 = kGamma - 1.0;

// Fortran: SUBROUTINE MRCL(CLS,M_CLS,R_CLS)
// Sets the Mach and Reynolds numbers from the unit-Cl values and the
// given CLS depending on machType/reType. Limits as in the
// Fortran: MINF >= 0.99 -> 0.99 (machLimited), RRAT > 100 ->
// REINF1*100 (reLimited), each as structured diagnostics.
// Error: machType/reType outside 1..3 is rejected in a controlled way
// (Fortran would set it to 1 with a message, session state).
Result<MachReynoldsResult> mrcl(const MachReynoldsSettings& settings,
                                double cls);

// Fortran: SUBROUTINE COMSET (no arguments, COMMON-based)
// Sets the Karman-Tsien parameter and sonic limits. minf == 0 ->
// cpstar = -999, qstar = 999 (canonical).
CompressibilityState comset(double minf, double qinf);

// Fortran: SUBROUTINE CPCALC(N,Q,QINF,MINF,CP)
// Sets the compressible Cp from the velocity. Returns
// the Cp list; the denominator state (Theta <= 0) is reported as a
// structured error state (Fortran writes a message and continues).
// cp is exactly q.size long.
Result<std::vector<double>> cpCalc(std::span<const double> q, double qinf,
                                   double minf);

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/Compressibility.h

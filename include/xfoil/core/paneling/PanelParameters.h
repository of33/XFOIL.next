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

// File: include/xfoil/core/paneling/PanelParameters.h
// GETPAN parameter semantics of the new 0-based cores,
// ported from fortran-src/xfoil.f GETPAN.
//
// Only the parameter branching is ported ('N' with clamp on IQX-6,
// 'P', 'T', 'R', 'XT', 'XB'). Menu, prompt, and plot code remain in the
// CLI (later step); the GETPAN command therefore stays CLI-blocked.
//
// Fortran diagnostics (clamp message of the 1200 formats) are returned
// structured; the numerical kernel does not write to stdout.
// No raw-pointer API, no dependency on app, CLI, or tests.

#pragma once

#include <span>

#include "xfoil/core/Result.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::paneling {

// Fortran: GETPAN parameter branching
// Sets a paneling parameter from values (without menu/prompt).
// child==panelCount: 1 value; Fortran clamp NPAN > IQX-6 (364) -> 364 +
// clamped=true. child in {bunchingParam, teToLeDensityRatio,
// refinementDensityRatio}: 1 value. child==upperRefinementRange/
// lowerRefinementRange: exactly 2 values (XSREF1/2 or XPREF1/2).
// Error: wrong number of values or panelCount < 2 is rejected in a
// controlled way (Fortran would ask again via dialog).
Result<ParameterChangeResult> applyPanelParameter(
    const PanelingParameters& current, PanelParameterKind kind,
    std::span<const double> values);

}  // namespace xfoil::core::paneling

// End: include/xfoil/core/paneling/PanelParameters.h

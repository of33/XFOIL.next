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

// File: include/xfoil/core/math/Diagnostic.h
// Structured, non-fatal diagnostic state of limited/iterative
// routines of the new 0-based cores.
//
// The numerical kernel does not write to stdout. Canonical
// Fortran diagnostic states (pass limit reached, Newton non-convergence,
// inversion failed, pass limit without tolerance satisfaction) are
// returned structured instead. The canonical numerical
// result and the operation order remain unchanged while doing so.
//
// No raw-pointer API, no dependency on app, CLI, or tests.

#pragma once

namespace xfoil::core::math {

// Non-fatal diagnostic state of limited/iterative routines.
enum class Diagnostic {
  // Regularly completed: tolerance satisfied or within the limits.
  completed,
  // Pass/iteration limit reached without regular completion. The routine
  // continues with the canonical behavior (numerical result unchanged).
  limitReached,
};

}  // namespace xfoil::core::math

// End: include/xfoil/core/math/Diagnostic.h

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

// File: include/xfoil/math/FortranCompat.h
// Fortran compatibility functions mirroring the Fortran intrinsics
// SIGN and AINT and the ATANC routine of fortran-src/xutils.f.
// No dependency on the facade header, no templates, no implicit
// global state.

#pragma once

namespace xfoil::math {

// Mirrors the Fortran intrinsic SIGN(a, b): the magnitude of magnitude with
// the sign of signSource (positive when signSource >= 0.0). For
// signSource == -0.0 it returns the positive magnitude because
// -0.0 >= 0.0 is true. std::copysign is deliberately not used because its
// behavior for -0.0 would differ from this comparison.
[[nodiscard]]
double copyMagnitudeWithSign(double magnitude, double signSource) noexcept;

// Mirrors the Fortran intrinsic AINT(number): truncation toward
// zero via conversion to int. The int conversion is deliberate
// (risk register R29); values outside the
// representable int range are not guarded.
[[nodiscard]]
double truncateTowardZero(double value) noexcept;

// Implements the ATANC routine (fortran-src/xutils.f:68): atan2 with
// a branch-cut correction relative to previousAngle, so that the
// position-angle sequence does not jump by +/- 2 pi when crossing the
// negative x axis.
[[nodiscard]]
double continuousAtan2(double y, double x, double previousAngle) noexcept;

}  // namespace xfoil::math

// End: include/xfoil/math/FortranCompat.h

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

// File: src/core/math/FortranCompat.cpp
// Fortran compatibility functions mirroring the Fortran intrinsics
// SIGN and AINT and the ATANC routine of fortran-src/xutils.f.
// Computation order and constants
// are adopted unchanged.

#include "xfoil/math/FortranCompat.h"

#include <cmath>

namespace {

// PI magnitude used by the SIGN term of the branch-cut correction.
constexpr double kPi = 3.141592654;

// TPI constant of the ATANC branch-cut correction (fortran-src/xutils.f:99).
constexpr double twoPi = 6.2831853071795864769;

}  // namespace

namespace xfoil::math {

double copyMagnitudeWithSign(double magnitude, double signSource) noexcept {
  if (signSource >= 0.0) {
    return std::fabs(magnitude);
  }
  return -std::fabs(magnitude);
}

double truncateTowardZero(double value) noexcept {
  if (value >= 0.0) {
    return static_cast<double>(static_cast<int>(value));
  }
  return static_cast<double>(-static_cast<int>(-value));
}

double continuousAtan2(double y, double x, double previousAngle) noexcept {
  // --- set new position angle, ignoring branch cut in atan2 function for now
  const double newAngle = std::atan2(y, x);
  const double delta = newAngle - previousAngle;

  // --- angle change cannot exceed +/- pi, so get rid of any multiples of 2 pi
  const double correction =
      delta -
      twoPi * static_cast<int>(
          (delta + copyMagnitudeWithSign(kPi, delta)) / twoPi);

  // --- set correct new angle
  return previousAngle + correction;
}

}  // namespace xfoil::math

// End: src/core/math/FortranCompat.cpp

// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
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

// File: include/xfoil/SkinFriction.h
// Owning, in-memory skin-friction distribution for the current valid
// viscous point. The arrays use the existing boundary-layer stations
// (airfoil stations only), not the Cp/panel-node discretization.

#pragma once

#include <vector>

namespace xfoil {

// Airfoil side. The values name the physical surface, not a container
// position.
enum class SurfaceSide {
  upper,
  lower
};

// One side of the boundary-layer station distribution. x/y are the
// panel-node coordinates at the mapped station, s is the existing
// boundary-layer arc coordinate. cf is the existing BLDUMP coefficient
// tau/(0.5*qinf^2), where qinf is the dimensionless freestream speed
// scale (not dynamic pressure) and tau is the normalized wall-shear value.
// All vectors have the same length. s increases from the leading-edge region
// toward the trailing edge; x is not required to be monotonic.
struct SkinFrictionSide {
  SkinFrictionSide() = default;
  explicit SkinFrictionSide(SurfaceSide value) noexcept : side(value) {}

  SurfaceSide side{SurfaceSide::upper};
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> s;
  std::vector<double> cf;
};

// Owning upper/lower skin-friction result. The upper and lower station
// counts may differ. Wake stations are never included.
struct SkinFrictionDistribution {
  SkinFrictionSide upper{};
  SkinFrictionSide lower{SurfaceSide::lower};
};

}  // namespace xfoil

// End: include/xfoil/SkinFriction.h

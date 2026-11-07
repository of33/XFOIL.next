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

// file: include/xfoil/Limits.h
// Type-safe dimension constants. Separates the semantic
// Fortran dimensions (fortran-src/XFOIL.INC) from the physical
// C++ element counts (storage).

#pragma once

#include <cstddef>

namespace xfoil::limits {

// ---
// Semantic Fortran dimensions (canonical, XFOIL.INC). These values
// correspond exactly to the Fortran PARAMETERs and are used in domain
// loops, formulas and validations.
// ---
inline constexpr std::size_t surfaceNodeDimension = 370;  // IQX
inline constexpr std::size_t halfSurfaceNodeDimension =
    surfaceNodeDimension / 2;  // IQX2 = 185
inline constexpr std::size_t bufferNodeDimension = 1480;  // IBX
inline constexpr std::size_t panelWakeNodeDimension = 418;  // IZX
inline constexpr std::size_t wakeNodeDimension = 48;  // IWX
inline constexpr std::size_t boundaryLayerNodeDimension = 283;  // IVX
inline constexpr std::size_t qspecDistributionDimension = 5;  // IPX
inline constexpr std::size_t sideDimension = 2;  // ISX
inline constexpr std::size_t circlePlanePointDimension = 257;  // ICX
inline constexpr std::size_t mappingCoefficientDimension = 64;  // IMX
inline constexpr std::size_t quarterMappingCoefficientDimension = 16;  // IMX4

// ---
// Physical C++ element counts (storage). The geometry is stored
// internally 1-based from index 1; index 0 stays padding. The wake/BL
// reserves are deliberately larger than the semantic domain sizes.
// ---
inline constexpr std::size_t surfaceNodeStorage =
    surfaceNodeDimension + 1;  // 371
inline constexpr std::size_t bufferNodeStorage =
    bufferNodeDimension;  // 1480, indices 0..1479
inline constexpr std::size_t panelWakeNodeStorage =
    panelWakeNodeDimension + 1;  // 419
inline constexpr std::size_t wakeNodeStorage = 50;  // Reserve (> IWX+1=49)
inline constexpr std::size_t boundaryLayerNodeStorage = 302;  // Reserve
inline constexpr std::size_t qspecDistributionStorage =
    qspecDistributionDimension + 1;  // 6
inline constexpr std::size_t sideStorage = sideDimension + 1;  // 3
inline constexpr std::size_t camberNodeStorage =
    bufferNodeDimension + 1;  // 1481, holds GETCAM index 1480
inline constexpr std::size_t snewNodeStorage =
    5 * bufferNodeDimension + 1;  // 7401
inline constexpr std::size_t workNodeStorage =
    6 * surfaceNodeDimension + 1;  // 2221

// ICX/IMX/IMX4: physical storage = semantic dimension (arrays use
// [X+1] for 1-based indices 1..X).
inline constexpr std::size_t circlePlanePointStorage =
    circlePlanePointDimension;  // 257
inline constexpr std::size_t mappingCoefficientStorage =
    mappingCoefficientDimension;  // 64
inline constexpr std::size_t quarterMappingCoefficientStorage =
    quarterMappingCoefficientDimension;  // 16

inline constexpr std::size_t midpointCount = 100;

// AREAD/reader-loop capacity (NMAX), matching Fortran IBX = 4*IQX.
// Not the maximum successful
// point count: 1480 real coordinates without EOF/separator pair are rejected as
// capacity overflow. Maximum successful real point count
// NB = maxInputCoordinates - 1 = 1479.
inline constexpr std::size_t maxInputCoordinates =
    bufferNodeDimension;  // 1480 (capacity, not accepted point count)
// OVER-NMAX of the overlay contour (xgdes.f 2291: 2*IQX = 740).
inline constexpr std::size_t overlayCoordinateCapacity =
    2 * surfaceNodeDimension;  // 740

// Named domain path constants. Canonical target values;
// the productive paths enable them only in the respective successor groups
// (NACA/GETPAN/ABCOPY/PANGEN).
inline constexpr std::size_t nacaSideLimit =
    surfaceNodeDimension / 3;  // 123
inline constexpr std::size_t getpanPanelLimit =
    surfaceNodeDimension - 6;  // 364
inline constexpr std::size_t abcopyPanelLimit =
    surfaceNodeDimension - 5;  // 365
inline constexpr std::size_t pangenPanelLimit =
    surfaceNodeDimension - 1;  // 369

// Inverse capacity limits. Derived from the
// semantic domain dimension IQX: the Mixed-inverse system needs
// active indices 1..n+5 with the additional unknowns psio (n+1) and
// qdof0..qdof3 (n+2..n+5), solved as GAUSS(IQX, N+5, ...) in
// fortran-src/xqdes.f:1454. The largest regular panel count is
// IQX - 5 = 365.
inline constexpr std::size_t maxRegularPanelNodes =
    surfaceNodeDimension - 5;  // 365
inline constexpr std::size_t mixedInverseExtraUnknowns = 5;
inline constexpr std::size_t mixedInverseMaximumSystemIndex =
    maxRegularPanelNodes + mixedInverseExtraUnknowns;  // 370
inline constexpr std::size_t mixedInverseSystemStorage =
    mixedInverseMaximumSystemIndex + 1;  // 371

static_assert(mixedInverseSystemStorage >= surfaceNodeStorage);

}  // namespace xfoil::limits

// End: include/xfoil/Limits.h

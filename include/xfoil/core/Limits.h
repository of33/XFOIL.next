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

// File: include/xfoil/core/Limits.h
// Canonical domain capacities of the new 0-based XFOIL core.
// All values are derived directly from the canonical
// Fortran includes (fortran-src/XFOIL.INC, fortran-src/CIRCLE.INC).
//
// 0-based semantics: a Fortran array A(IQX) with indices 1..IQX
// corresponds in C++ to a[i] with indices 0..IQX-1. The element count stays
// identical. A value denotes either a domain capacity or a
// physical element count, never both implicitly. No
// blanket +1 index-0 padding is kept. Path-specific boundaries such as
// IQX-6, IQX-5 or IQX-1 are introduced only in the respective domain
// steps themselves.

#pragma once

#include <cstddef>

namespace xfoil::core {

// XFOIL.INC: PARAMETER (IQX=601, IPX=5, ISX=2)
// Surface system dimension IQX, among others for GAM(IQX).
inline constexpr std::size_t surfaceSystemCapacity = 601;
// Number of QSPEC distributions.
inline constexpr std::size_t qspecDistributionCapacity = 5;
// Number of airfoil sides (suction and pressure side).
inline constexpr std::size_t sideCount = 2;

// XFOIL.INC: PARAMETER (IWX=IQX/8+2) -> 601/8+2 = 77
// Wake-panel capacity.
inline constexpr std::size_t wakeCapacity = 77;

// XFOIL.INC: PARAMETER (IBX=4*IQX) -> 4*601 = 2404
// Buffer contour capacity.
inline constexpr std::size_t bufferCapacity = 2404;

// XFOIL.INC: PARAMETER (IZX=IQX+IWX) -> 601+77 = 678
// Panel + wake capacity (current contour incl. wake).
inline constexpr std::size_t panelAndWakeCapacity = 678;

// XFOIL.INC: PARAMETER (IVX=IQX/2+IWX+50) -> 300+77+50 = 427
// Boundary-layer capacity per airfoil side (incl. wake).
inline constexpr std::size_t boundaryLayerCapacity = 427;

// XFOIL.INC: PARAMETER (NAX=800, NPX=20, NFX=128)
// Polar-point capacity.
inline constexpr std::size_t polarPointCapacity = 800;
// Number of polars and reference polars (NPX).
inline constexpr std::size_t polarSetCapacity = 20;
// Reference polar-point capacity.
inline constexpr std::size_t referencePointCapacity = 128;

// XFOIL.INC: PARAMETER (NTX=2*IBX) -> 2*2404 = 4808
// Camber/thickness table capacity.
inline constexpr std::size_t camberTableCapacity = 4808;

// spline.f: PARAMETER (NMAX=2500) in SPLINE and SPLIND.
// Spline coefficient workspace limit (valid point count <= capacity).
inline constexpr std::size_t splineWorkspaceCapacity = 2500;

// xsolve.f: PARAMETER (NVX=515) in LUDCMP.
// LU scaling workspace limit (valid equation count <= capacity).
inline constexpr std::size_t linearSolverWorkspaceCapacity = 515;

// CIRCLE.INC: PARAMETER (ICX=257)
// Circle-plane point capacity (2^n + 1).
inline constexpr std::size_t circlePointCapacity = 257;

// CIRCLE.INC: PARAMETER (IMX=(ICX-1)/4) -> (257-1)/4 = 64
// Mapping-coefficient capacity Cn.
inline constexpr std::size_t mappingCoefficientCapacity = 64;

// ---
// Canonical dimension relations (directly from XFOIL.INC / CIRCLE.INC).
// ---
static_assert(wakeCapacity == surfaceSystemCapacity / 8 + 2);
static_assert(bufferCapacity == 4 * surfaceSystemCapacity);
static_assert(panelAndWakeCapacity == surfaceSystemCapacity + wakeCapacity);
static_assert(
    boundaryLayerCapacity == surfaceSystemCapacity / 2 + wakeCapacity + 50);
static_assert(camberTableCapacity == 2 * bufferCapacity);
static_assert(circlePointCapacity == 257);
static_assert(mappingCoefficientCapacity == (circlePointCapacity - 1) / 4);
static_assert(sideCount == 2);
static_assert(qspecDistributionCapacity == 5);

}  // namespace xfoil::core

// End: include/xfoil/core/Limits.h

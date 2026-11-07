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

// File: include/xfoil/core/boundary_layer/BoundaryLayerMapping.h
// Panel/boundary-layer mapping kernels of the new 0-based cores,
// ported from fortran-src/xpanel.f IBLPAN and XICALC and
// the station-mapping part of fortran-src/xbl.f XIFSET.
//
// Contained operations:
// IBLPAN -> iblpan (panel/BL assignment)
// XICALC -> xicalc (BL arc lengths and wake gap)
// XIFSET -> xifset (forced transition)
//
// Canonical sources:
// xpanel.f IBLPAN: top I=Is..1 (backward, VTI=+1), bottom I=Is+1..N
// (VTI=-1), wake I=N+IW (VTI=-1), IBLMAX, IBLMAX>IVX -> STOP.
// xpanel.f XICALC: XSSI(IBL,IS)=MAX(SST-S(I), XEPS) or S(I)-SST,
// wake DXSSI, TELRAT=2.50, CROSP/DWDXTE limited, WGAP.
// xbl.f XIFSET: XIFORC from XSTRIP via SINVRT, MIN with XSSI(IBLTE).
//
// The input is the panel geometry and the stagnation/wake states.
//
// No raw-pointer API, no global state.

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran IPAN(IVX,IS) is a fixed COMMON field: IBLPAN overwrites
// only the canonical written range (1..IBLTE(IS) and the
// wake-pointer loop on IS=1); entries beyond it retain earlier
// COMMON state (first use: zero from INITCOM). PLXADD (xpol.f)
// reads IPAN up to NSIDE(IS), where
// NSIDE(1) = IBLTE(1) + (NBL(2)-IBLTE(2)) can exceed the active
// NBL(1) range. preserveIpanTail implements exactly this contract
// capacity-wise: grow-only, the new active range overwrites its
// entries, and the tail of old points stays preserved.
inline void preserveIpanTail(const std::vector<int>& previous,
                             std::vector<int>& fresh) {
  if (previous.size() <= fresh.size()) {
    return;
  }
  fresh.insert(fresh.end(), previous.begin() +
                                static_cast<std::ptrdiff_t>(fresh.size()),
               previous.end());
}

// IPAN(IVX,IS) and the remaining CR15 station fields share the same
// fixed COMMON capacity. ensureIpanCapacity keeps the IPAN view on
// the capacity of the station fields: grow-only, new positions are
// zero (INITCOM state never wrote those entries), and the
// written range stays unchanged.
inline void ensureIpanCapacity(BoundaryLayerMapping& map,
                               std::size_t topCapacity,
                               std::size_t bottomCapacity) {
  if (map.top.ipan.size() < topCapacity) {
    map.top.ipan.resize(topCapacity, 0);
  }
  if (map.bottom.ipan.size() < bottomCapacity) {
    map.bottom.ipan.resize(bottomCapacity, 0);
  }
}

// Fortran: SUBROUTINE IBLPAN.
// Sets the panel/BL assignment: upper side from the TE station to the
// stagnation point (backward, VTI=+1), lower side from the stagnation
// point to the TE (VTI=-1), then the wake (VTI=-1). iblte/nbl are
// 0-based station indices; ipan[station] is the 0-based panel index.
// Input: panel geometry; istStation (Fortran Is 1-based, passed
// 0-based); nw (wake point count); ivx (canonical BL capacity,
// Fortran IVX=IQX/2+IWX+50=283).
// If iblmax exceeds the capacity, the canonical STOP is returned as
// structured diagnostics (Fortran WRITEs not on stdout).
BoundaryLayerMapping iblpan(const paneling::PanelGeometry& panel,
                            std::size_t istStation, std::size_t nw,
                            std::size_t ivx);

// Fortran: SUBROUTINE XICALC.
// Sets the BL arc lengths (XSSI) per side and the wake-gap array.
// Input: panel from paneling, mapping from iblpan, sst (Fortran SST),
// ante (Fortran ANTE, TE gap from the TE geometry), wakeX/wakeY
// (Fortran X(N+IW)/Y(N+IW), 0-based, length nw, from XYWAKE).
WakeGap xicalc(const paneling::PanelGeometry& panel,
               const BoundaryLayerMapping& mapping, double sst, double ante,
               std::span<const double> wakeX, std::span<const double> wakeY,
               BoundaryLayerArcLengths& arcs);

// Fortran: SUBROUTINE XIFSET(IS).
// Sets the BL coordinate for forced transition (XIFORC) from the
// chord-based trip position (Fortran XSTRIP(IS), 0<=x/c<=1). Input:
// panel from paneling, mapping from iblpan, arcs from xicalc, sst (Fortran SST),
// xstrip (Fortran XSTRIP(IS)), side (SurfaceSide).
// xiforc <= 0 (stagnation point behind the trip) gives the canonical
// fix (XIFORC=XSSI(IBLTE)) with structured diagnostics returned.
Result<TransitionState> xifset(const paneling::PanelGeometry& panel,
                               const BoundaryLayerMapping& mapping,
                               const BoundaryLayerArcLengths& arcs, double sst,
                               double xstrip, SurfaceSide side);

}  // namespace xfoil::core::boundary_layer

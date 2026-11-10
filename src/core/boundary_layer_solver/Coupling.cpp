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

// File: src/core/boundary_layer_solver/Coupling.cpp
// Implementation of the coupling and clamping routines. Formula-faithful
// adoption from fortran-src/xpanel.f and
// xbl.f; constants, branch conditions and the order of
// computations are unchanged.

#include "xfoil/core/boundary_layer/Coupling.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "xfoil/core/boundary_layer/Viscal.h"
#include "xfoil/core/inviscid/Stagnation.h"

namespace xfoil::core::boundary_layer {

namespace {

// Fortran: station-loop mirroring. Returns the station span per page.
std::span<const double> sideSpan(const BoundaryLayerStationField& f,
                                 SurfaceSide is) {
  return is == SurfaceSide::kTop ? std::span<const double>(f.top)
                                 : std::span<const double>(f.bottom);
}

void setSideField(BoundaryLayerStationField& f, SurfaceSide is,
                  std::size_t n) {
  if (is == SurfaceSide::kTop) {
    f.top.assign(n, 0.0);
  } else {
    f.bottom.assign(n, 0.0);
  }
}

std::span<double> sideSpan(BoundaryLayerStationField& f, SurfaceSide is) {
  return is == SurfaceSide::kTop ? std::span<double>(f.top)
                                 : std::span<double>(f.bottom);
}

}  // namespace

void uicalc(const BoundaryLayerMapping& map, std::span<const double> qinv,
            std::span<const double> qinvA, BoundaryLayerStationField& uinv,
            BoundaryLayerStationField& uinvA) {
  const std::array<const BoundaryLayerSideMapping*, 2> sides{&map.top,
                                                             &map.bottom};
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    if (m.nbl < 1) {
      throw std::invalid_argument("uicalc: NBL < 1");
    }
    setSideField(uinv, side, static_cast<std::size_t>(m.nbl) + 1);
    setSideField(uinvA, side, static_cast<std::size_t>(m.nbl) + 1);
    auto uinvS = sideSpan(uinv, side);
    auto uinvAS = sideSpan(uinvA, side);
    // Fortran: UINV(1,IS)=0 ; UINV_A(1,IS)=0
    uinvS[0] = 0.0;
    uinvAS[0] = 0.0;
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      if (i >= qinv.size() || i >= qinvA.size()) {
        throw std::invalid_argument("uicalc: panel index outside QINV");
      }
      uinvS[k] = m.vti[k] * qinv[i];
      uinvAS[k] = m.vti[k] * qinvA[i];
    }
  }
}

void uecalc(const BoundaryLayerMapping& map, std::span<const double> qvis,
            BoundaryLayerStationField& uedg) {
  const std::array<const BoundaryLayerSideMapping*, 2> sides{&map.top,
                                                             &map.bottom};
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    if (m.nbl < 1) {
      throw std::invalid_argument("uecalc: NBL < 1");
    }
    setSideField(uedg, side, static_cast<std::size_t>(m.nbl) + 1);
    auto uedgS = sideSpan(uedg, side);
    // Fortran: UEDG(1,IS)=0
    uedgS[0] = 0.0;
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      if (i >= qvis.size()) {
        throw std::invalid_argument("uecalc: panel index outside QVIS");
      }
      uedgS[k] = m.vti[k] * qvis[i];
    }
  }
}

void qvfue(const BoundaryLayerMapping& map,
           const BoundaryLayerStationField& uedg, std::span<double> qvis) {
  const std::array<const BoundaryLayerSideMapping*, 2> sides{&map.top,
                                                             &map.bottom};
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    const auto uedgS = sideSpan(uedg, side);
    if (m.nbl < 1 || uedgS.size() < static_cast<std::size_t>(m.nbl) + 1) {
      throw std::invalid_argument("qvfue: field quantity does not match NBL");
    }
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      if (i >= qvis.size()) {
        throw std::invalid_argument("qvfue: panel index outside QVIS");
      }
      qvis[i] = m.vti[k] * uedgS[k];
    }
  }
}

void gamqv(std::span<const double> qvis, std::span<const double> qinvA,
           std::span<double> gam, std::span<double> gamA) {
  const std::size_t n = gam.size();
  if (qvis.size() < n || qinvA.size() < n || gamA.size() < n) {
    throw std::invalid_argument("gamqv: field quantities do not match");
  }
  // Fortran: GAM(I)=QVIS(I) ; GAM_A(I)=QINV_A(I) for I=1..N
  for (std::size_t i = 0; i < n; ++i) {
    gam[i] = qvis[i];
    gamA[i] = qinvA[i];
  }
}

void ueset(const BoundaryLayerMapping& map, std::span<const double> dij,
           int dijStride, const BoundaryLayerStationField& uinv,
           const BoundaryLayerStationField& mass,
           BoundaryLayerStationField& uedg) {
  const std::array<const BoundaryLayerSideMapping*, 2> sides{&map.top,
                                                             &map.bottom};
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    const auto uinvS = sideSpan(uinv, side);
    if (m.nbl < 1) {
      throw std::invalid_argument("ueset: NBL < 1");
    }
    setSideField(uedg, side, static_cast<std::size_t>(m.nbl) + 1);
    auto uedgS = sideSpan(uedg, side);
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      if (i >= static_cast<std::size_t>(dijStride)) {
        throw std::invalid_argument("ueset: panel index outside DIJ");
      }
      double dui = 0.0;
      for (int js = 0; js < 2; ++js) {
        const BoundaryLayerSideMapping& ms =
            *sides[static_cast<std::size_t>(js)];
        const auto massS =
            sideSpan(mass, js == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom);
        for (int jbl = 1; jbl <= ms.nbl; ++jbl) {
          const std::size_t kk = static_cast<std::size_t>(jbl);
          const std::size_t j = static_cast<std::size_t>(ms.ipan[kk]);
          const std::size_t idx =
              static_cast<std::size_t>(i) * static_cast<std::size_t>(dijStride) +
              j;
          if (idx >= dij.size()) {
            throw std::invalid_argument("ueset: DIJ index outside");
          }
          dui += -m.vti[k] * ms.vti[kk] * dij[idx] * massS[kk];
        }
      }
      uedgS[k] = uinvS[k] + dui;
    }
  }
}

void dsset(const BoundaryLayerMapping& map,
           const BoundaryLayerStationField& mass,
           const BoundaryLayerStationField& uedg,
           BoundaryLayerStationField& dstr) {
  const std::array<const BoundaryLayerSideMapping*, 2> sides{&map.top,
                                                             &map.bottom};
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    if (m.nbl < 1) {
      throw std::invalid_argument("dsset: NBL < 1");
    }
    setSideField(dstr, side, static_cast<std::size_t>(m.nbl) + 1);
    const auto massS = sideSpan(mass, side);
    const auto uedgS = sideSpan(uedg, side);
    auto dstrS = sideSpan(dstr, side);
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      if (uedgS[k] == 0.0) {
        throw std::invalid_argument("dsset: UEDG is zero");
      }
      dstrS[k] = massS[k] / uedgS[k];
    }
  }
}

StMoveResult stmove(const paneling::PanelGeometry& panel,
                    std::span<const double> wakeX,
                    std::span<const double> wakeY, double ante,
                    std::size_t nw, std::size_t ivx,
                    std::span<const double> qinv,
                    std::span<const double> qinvA, std::span<double> gam,
                    std::span<double> qvis, StMoveState& state) {
  StMoveResult res{};

  // Fortran: ISTOLD = Is ; CALL STFIND (GAM from panel+wake, s from panel).
  const std::size_t istold = state.ist;
  const inviscid::StagnationPointResult st =
      inviscid::stfind(gam, std::span<const double>(panel.s));
  state.ist = st.ist;
  state.sst = st.sst;
  // Fortran STFIND: message + fallback I=N/2, continuation (xpanel.f:1366).
  res.stagnationNotFound = !st.found;

  if (state.ist == istold) {
    // Fortran: recompute the arc-length array (CALL XICALC).
    static_cast<void>(xicalc(panel, state.mapping, state.sst, ante, wakeX,
                             wakeY, state.arcs));
  } else {
    res.moved = true;

    // Fortran: set new BL position -> panel position pointers (CALL IBLPAN).
    // IPAN COMMON contract as in Viscal.cpp: the new active range is set,
    // while the tail of the COMMON fields remains capacity-preserved.
    auto fresh = iblpan(panel, state.ist, nw, ivx);
    preserveIpanTail(state.mapping.top.ipan, fresh.top.ipan);
    preserveIpanTail(state.mapping.bottom.ipan, fresh.bottom.ipan);
    state.mapping = std::move(fresh);
    // Capacity sync of the PLXADD view (CR15 COMMON contract).
    syncPlxaddStationCapacity(state.mapping, {&state.ctau, &state.thet,
                                             &state.dstr});
    // Fortran: set new inviscid BL edge velocity UINV from QINV (CALL UICALC).
    uicalc(state.mapping, qinv, qinvA, state.uinv, state.uinvA);
    // Fortran: recompute the arc-length array (CALL XICALC).
    static_cast<void>(xicalc(panel, state.mapping, state.sst, ante, wakeX,
                             wakeY, state.arcs));
    // Fortran: set BL position -> system line pointers (CALL IBLSYS).
    state.isys =
        iblsys({state.mapping.top.nbl, state.mapping.bottom.nbl},
               static_cast<int>(ivx));

    const std::size_t idif =
        state.ist > istold ? state.ist - istold : istold - state.ist;
    res.idif = idif;
    const int idifI = static_cast<int>(idif);

    // Safety (cpp-safety deviation): the IDIF shifts read IBL up to
    // NBL+IDIF (DO 120/220: CTAU(IBL+IDIF,IS)) and write IBL up to
    // NBL_new. Fortran dimensions CTAU/THET/DSTR/UEDG/MASS as
    // COMMON/CR15 arrays (IVX,ISX) with IVX = IQX/2+IWX+50 >> NBL+IDIF;
    // entries beyond IBLTE(1)+NW are never written there by IBLPAN/UPDATE/
    // STMOVE and remain zero-initialized in the COMMON.
    // The tight C++ vectors are extended here with zero initialization to
    // the extent touched by the shift — value-identical to Fortran, with
    // no formula or control-flow change.
    const std::size_t shiftNeedTop =
        static_cast<std::size_t>(state.mapping.top.nbl) + idif + 1;
    const std::size_t shiftNeedBottom =
        static_cast<std::size_t>(state.mapping.bottom.nbl) + idif + 1;
    const auto growShiftFields = [shiftNeedTop,
                                  shiftNeedBottom](BoundaryLayerStationField& f) {
      if (f.top.size() < shiftNeedTop) {
        f.top.resize(shiftNeedTop, 0.0);
      }
      if (f.bottom.size() < shiftNeedBottom) {
        f.bottom.resize(shiftNeedBottom, 0.0);
      }
    };
    growShiftFields(state.ctau);
    growShiftFields(state.thet);
    growShiftFields(state.dstr);
    growShiftFields(state.uedg);
    growShiftFields(state.mass);

    if (state.ist > istold) {
      // Fortran: Is > ISTOLD -> top side downstream, bottom side upstream.
      // ITRAN(1) += IDIF ; ITRAN(2) -= IDIF (xpanel.f:1663-1664).
      state.itran[0] += idifI;
      state.itran[1] -= idifI;
      // Fortran: DO 110 IBL=NBL(1), IDIF+2, -1 (xpanel.f:1667) ->
      // 0-based [nbl, idif+1] step -1.
      for (int ibl = state.mapping.top.nbl; ibl >= idifI + 1; --ibl) {
        const std::size_t k = static_cast<std::size_t>(ibl);
        const std::size_t kk = k - idif;
        state.ctau.top[k] = state.ctau.top[kk];
        state.thet.top[k] = state.thet.top[kk];
        state.dstr.top[k] = state.dstr.top[kk];
        state.uedg.top[k] = state.uedg.top[kk];
      }
      // Fortran: DO 115 IBL=IDIF+1, 2, -1 (xpanel.f:1676) ->
      // 0-based [idif, 1] step -1.
      const double dudx =
          state.uedg.top[idif + 1] / state.arcs.top[idif + 1];
      for (int ibl = idifI; ibl >= 1; --ibl) {
        const std::size_t k = static_cast<std::size_t>(ibl);
        state.ctau.top[k] = state.ctau.top[idif + 1];
        state.thet.top[k] = state.thet.top[idif + 1];
        state.dstr.top[k] = state.dstr.top[idif + 1];
        state.uedg.top[k] = dudx * state.arcs.top[k];
      }
      // Fortran: DO 120 IBL=2, NBL(2) (xpanel.f:1684) ->
      // 0-based [1, nbl_bottom].
      for (int ibl = 1; ibl <= state.mapping.bottom.nbl; ++ibl) {
        const std::size_t k = static_cast<std::size_t>(ibl);
        const std::size_t kk = k + idif;
        state.ctau.bottom[k] = state.ctau.bottom[kk];
        state.thet.bottom[k] = state.thet.bottom[kk];
        state.dstr.bottom[k] = state.dstr.bottom[kk];
        state.uedg.bottom[k] = state.uedg.bottom[kk];
      }
    } else {
      // Fortran: Is < ISTOLD -> bottom side downstream, top side upstream.
      // ITRAN(1) -= IDIF ; ITRAN(2) += IDIF (xpanel.f:1695-1696).
      state.itran[0] -= idifI;
      state.itran[1] += idifI;
      // Fortran: DO 210 IBL=NBL(2), IDIF+2, -1 (xpanel.f:1699) ->
      // 0-based [nbl, idif+1] step -1.
      for (int ibl = state.mapping.bottom.nbl; ibl >= idifI + 1; --ibl) {
        const std::size_t k = static_cast<std::size_t>(ibl);
        const std::size_t kk = k - idif;
        state.ctau.bottom[k] = state.ctau.bottom[kk];
        state.thet.bottom[k] = state.thet.bottom[kk];
        state.dstr.bottom[k] = state.dstr.bottom[kk];
        state.uedg.bottom[k] = state.uedg.bottom[kk];
      }
      // Fortran: DO 215 IBL=IDIF+1, 2, -1 (xpanel.f:1713) ->
      // 0-based [idif, 1] step -1.
      const double dudx =
          state.uedg.bottom[idif + 1] / state.arcs.bottom[idif + 1];
      for (int ibl = idifI; ibl >= 1; --ibl) {
        const std::size_t k = static_cast<std::size_t>(ibl);
        state.ctau.bottom[k] = state.ctau.bottom[idif + 1];
        state.thet.bottom[k] = state.thet.bottom[idif + 1];
        state.dstr.bottom[k] = state.dstr.bottom[idif + 1];
        state.uedg.bottom[k] = dudx * state.arcs.bottom[k];
      }
      // Fortran: DO 220 IBL=2, NBL(1) (xpanel.f:1724) ->
      // 0-based [1, nbl_top].
      for (int ibl = 1; ibl <= state.mapping.top.nbl; ++ibl) {
        const std::size_t k = static_cast<std::size_t>(ibl);
        const std::size_t kk = k + idif;
        state.ctau.top[k] = state.ctau.top[kk];
        state.thet.top[k] = state.thet.top[kk];
        state.dstr.top[k] = state.dstr.top[kk];
        state.uedg.top[k] = state.uedg.top[kk];
      }
    }
  }

  // Fortran: tweak Ue so it's not zero (UEPS = 1.0E-7, xpanel.f:1732-1743).
  constexpr double kUeps = 1.0e-7;
  for (int is = 0; is < 2; ++is) {
    const BoundaryLayerSideMapping& m =
        is == 0 ? state.mapping.top : state.mapping.bottom;
    auto& uedgS = is == 0 ? state.uedg.top : state.uedg.bottom;
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      if (uedgS[k] <= kUeps) {
        uedgS[k] = kUeps;
        qvis[i] = m.vti[k] * kUeps;
        gam[i] = m.vti[k] * kUeps;
      }
    }
  }

  // Fortran: set new mass array (MASS = DSTR*UEDG, xpanel.f:1747-1752).
  for (int is = 0; is < 2; ++is) {
    const BoundaryLayerSideMapping& m =
        is == 0 ? state.mapping.top : state.mapping.bottom;
    auto& dstrS = is == 0 ? state.dstr.top : state.dstr.bottom;
    auto& uedgS = is == 0 ? state.uedg.top : state.uedg.bottom;
    auto& massS = is == 0 ? state.mass.top : state.mass.bottom;
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      massS[k] = dstrS[k] * uedgS[k];
    }
  }

  return res;
}

CdCalcResult cdcalc(double alfa, double qinf, double tklam, bool lvisc,
                    bool lblini, const BoundaryLayerMapping& map,
                    const BoundaryLayerStationField& thet,
                    const BoundaryLayerStationField& dstr,
                    const BoundaryLayerStationField& uedg,
                    const BoundaryLayerStationField& tau,
                    std::span<const double> x, std::span<const double> y) {
  CdCalcResult res{};
  const double sa = std::sin(alfa);
  const double ca = std::cos(alfa);

  // Fortran: Squire-Young wake extrapolation (xfoil.f:1178-1194).
  if (lvisc && lblini) {
    const std::size_t nb = static_cast<std::size_t>(map.bottom.nbl);
    const double thwake = thet.bottom[nb];
    const double urat = uedg.bottom[nb] / qinf;
    const double uewake =
        uedg.bottom[nb] * (1.0 - tklam) / (1.0 - tklam * urat * urat);
    const double shwake = dstr.bottom[nb] / thet.bottom[nb];
    res.cd = 2.0 * thwake * std::pow(uewake / qinf, 0.5 * (5.0 + shwake));
  } else {
    res.cd = 0.0;
  }

  // Fortran: friction drag and lift (xfoil.f:1197-1205).
  // DO IBL=3, IBLTE(IS) -> 0-based [2, iblte].
  res.cdf = 0.0;
  res.clf = 0.0;
  for (int is = 0; is < 2; ++is) {
    const BoundaryLayerSideMapping& m =
        is == 0 ? map.top : map.bottom;
    const auto& tauS = is == 0 ? tau.top : tau.bottom;
    for (int ibl = 2; ibl <= m.iblte; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      const std::size_t im = static_cast<std::size_t>(m.ipan[k - 1]);
      if (i >= x.size() || im >= x.size() || i >= y.size() || im >= y.size()) {
        throw std::invalid_argument("cdcalc: panel index outside X/Y");
      }
      const double dx = (x[i] - x[im]) * ca + (y[i] - y[im]) * sa;
      const double dy = -(x[i] - x[im]) * sa + (y[i] - y[im]) * ca;
      res.cdf += 0.5 * (tauS[k] + tauS[k - 1]) * dx * 2.0 / (qinf * qinf);
      res.clf += 0.5 * (tauS[k] + tauS[k - 1]) * dy * 2.0 / (qinf * qinf);
    }
  }

  return res;
}

CpMinResult fcpmin(bool lvisc, std::span<const double> x,
                   std::span<const double> cpi,
                   std::span<const double> cpv) {
  CpMinResult res{};
  const std::size_t n = x.size();
  if (cpi.size() < n || cpv.size() < n) {
    throw std::invalid_argument("fcpmin: CPI/CPV too short");
  }
  // Fortran: DO I=2, N+NW -> 0-based [1, n).
  res.xcpmni = x[0];
  res.xcpmnv = x[0];
  res.cpmni = cpi[0];
  res.cpmnv = cpv[0];
  for (std::size_t i = 1; i < n; ++i) {
    if (cpi[i] < res.cpmni) {
      res.xcpmni = x[i];
      res.cpmni = cpi[i];
    }
    if (cpv[i] < res.cpmnv) {
      res.xcpmnv = x[i];
      res.cpmnv = cpv[i];
    }
  }
  // Fortran: LVISC -> CPMN = CPMNV, otherwise CPMN = CPMNI (xoper.f:1791-1798).
  if (lvisc) {
    res.cpmin = res.cpmnv;
  } else {
    res.cpmin = res.cpmni;
    res.cpmnv = res.cpmni;
    res.xcpmnv = res.xcpmni;
  }
  return res;
}

double dslim(double dstr, double thet, double msq, double hklim) {
  // Fortran: H = DSTR/THET ; CALL HKIN(H,MSQ,HK,HK_H,HK_M)
  const double h = dstr / thet;
  const KinematicShapeValue hk = hkin(h, msq);
  // Fortran: DH = MAX(0.0, HKLIM-HK)/HK_H ; DSTR = DSTR + DH*THET
  const double dh = std::max(0.0, hklim - hk.value) / hk.shapeDerivative;
  return dstr + dh * thet;
}

}  // namespace xfoil::core::boundary_layer

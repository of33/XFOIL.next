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

// File: src/core/boundary_layer_core/BoundaryLayerMapping.cpp
// IBLPAN/XICALC/XIFSET kernels of the new 0-based core.
//
// Formulas and operation order exactly as in Fortran:
// - IBLPAN (xpanel.f:1395): IBL is incremented before the assignment
// (IBL=IBL+1; IPAN(IBL,IS)=I), i.e. IPAN(1,IS) stays unset and the
// entries lie at the 1-based positions 2..NBL(IS). 0-based
// Mirroring: ipan[k] = IPAN(k+1,IS); ipan[0] stays 0 (unused).
// top I=Is..1 (backward), VTI=+1; IBLTE(1)=NBL(1)=Is+1 (1-based);
// bottom I=Is+1..N, VTI=-1; IBLTE(2)=N-Is+1; wake: IPAN(IBLTE(2)+IW,2)=
// N+IW, VTI=-1; NBL(2)=IBLTE(2)+NW; upper wake pointers (only plot);
// IBLMAX=MAX(IBLTE(1),IBLTE(2))+NW; IBLMAX>IVX -> Message + STOP.
// - XICALC (xpanel.f:1455): XEPS=XFEPS*(S(N)-S(1)) with XFEPS=1.0E-7;
// XSSI(1,IS)=0; top XSSI(IBL,1)=MAX(SST-S(I), XEPS); bottom XSSI(IBL,2)=
// MAX(S(I)-SST, XEPS); IBL1=IBLTE(1)+1: XSSI(IBL1,1)=XSSI(IBL1-1,1);
// wake: DXSSI=sqrt((X(I)-X(I-1))^2+(Y(I)-Y(I-1))^2), XSSI+=DXSSI;
// TELRAT=2.50; CROSP=(XP(1)*YP(N)-YP(1)*XP(N))/sqrt(...); DWDXTE=
// CROSP/sqrt(1-CROSP^2) limited to +/-3.0/TELRAT; AA=3.0+TELRAT*DWDXTE;
// BB=-2.0-TELRAT*DWDXTE; SHARP -> WGAP=0; otherwise WGAP(IW)=ANTE*(AA+BB*ZN)*
// ZN^2 with ZN=1.0-(XSSI(IBL,IS)-XSSI(IBLTE(IS),IS))/(TELRAT*ANTE).
// - XIFSET (xbl.f:1196): XSTRIP>=1.0 -> XIFORC=XSSI(IBLTE(IS),IS); otherwise
// CHX/CHY/CHSQ, W1/W2 chord-based, STR=SLE+(S(1)-SLE)*XSTRIP (top) or.
// SLE+(S(N)-SLE)*XSTRIP (bottom), SINVRT, XIFORC=MIN(SST-STR or.
// STR-SST, XSSI(IBLTE(IS),IS)); XIFORC<0 -> Message + XIFORC=XSSI.

#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::boundary_layer {

namespace {

// Fortran: IVX capacity (XFOIL.INC parameter IVX=IQX/2+IWX+50).
constexpr std::size_t kDefaultBlCapacity = 283;

}  // namespace

// Fortran: SUBROUTINE IBLPAN.
BoundaryLayerMapping iblpan(const paneling::PanelGeometry& panel,
                            std::size_t istStation, std::size_t nw,
                            std::size_t ivx) {
  const std::size_t n = panel.x.size();
  const std::size_t ivxLimit = ivx == 0 ? kDefaultBlCapacity : ivx;
  if (istStation >= n) {
    throw std::invalid_argument(
        "xfoil::core::boundary_layer::iblpan: Stagnation point index outside "
        "the Contour");
  }

  BoundaryLayerMapping mapping;

  // Fortran: top surface first (IS=1). IBL is incremented before the
  // assignment: IPAN(2,1)=Is, ..., IPAN(Is+1,1)=1; IPAN(1,1) stays
  // unset. 0-based mirroring: ipan[k] = IPAN(k+1,IS).
  std::vector<int>& ipanTop = mapping.top.ipan;
  std::vector<double>& vtiTop = mapping.top.vti;
  ipanTop.push_back(0);  // Fortran IPAN(1,IS) is unset
  vtiTop.push_back(0.0);
  for (std::size_t i = istStation + 1; i-- > 0;) {
    ipanTop.push_back(static_cast<int>(i));
    vtiTop.push_back(1.0);
  }
  // Fortran: IBLTE(1)=NBL(1)=Is+1 (1-based) -> 0-based index
  // istStation+1 (TE entry).
  mapping.top.iblte = static_cast<int>(istStation) + 1;
  mapping.top.nbl = static_cast<int>(istStation) + 1;

  // Fortran: bottom surface next (IS=2). IPAN(2,2)=Is+1, ..., IPAN(N-Is,2)=
  // N; 0-based: ipan[k]=i for i=istStation+1..n-1.
  std::vector<int>& ipanBottom = mapping.bottom.ipan;
  std::vector<double>& vtiBottom = mapping.bottom.vti;
  ipanBottom.push_back(0);  // Fortran IPAN(1,IS) is unset
  vtiBottom.push_back(0.0);
  for (std::size_t i = istStation + 1; i < n; ++i) {
    ipanBottom.push_back(static_cast<int>(i));
    vtiBottom.push_back(-1.0);
  }
  // Fortran: IBLTE(2) = number of lower airfoil entries (1-based).
  mapping.bottom.iblte = static_cast<int>(n - 1 - istStation);

  // Fortran: wake. IPAN(IBLTE(2)+IW,2)=N+IW; 0-based:
  // ipan[iblte2+1+iw]=n+iw (0-based panel) for iw=0..nw-1.
  for (std::size_t iw = 0; iw < nw; ++iw) {
    ipanBottom.push_back(static_cast<int>(n + iw));
    vtiBottom.push_back(-1.0);
  }
  // Fortran: NBL(2)=IBLTE(2)+NW (1-based) -> 0-based last index.
  mapping.bottom.nbl = mapping.bottom.iblte + static_cast<int>(nw);

  // Fortran: upper wake pointers (for plotting only).
  // IPAN(IBLTE(1)+IW,1)=IPAN(IBLTE(2)+IW,2), VTI=+1; 0-based:
  // ipan[iblte1+iw]=ipan[iblte2+iw].
  const std::size_t iblte2 =
      static_cast<std::size_t>(mapping.bottom.iblte);
  for (std::size_t iw = 0; iw < nw; ++iw) {
    ipanTop.push_back(ipanBottom[iblte2 + 1 + iw]);
    vtiTop.push_back(1.0);
  }

  // Fortran: IBLMAX = MAX(IBLTE(1),IBLTE(2)) + NW (1-based); 0-based
  // last index = IBLMAX-1. Capacity: Fortran STOP if
  // IBLMAX > IVX, i.e. (iblmax+1) > ivxLimit.
  const int iblmaxOneBased =
      std::max(mapping.top.iblte, mapping.bottom.iblte) +
      static_cast<int>(nw) + 1;
  mapping.iblmax = iblmaxOneBased - 1;
  if (static_cast<std::size_t>(iblmaxOneBased) > ivxLimit) {
    throw std::invalid_argument(
        "xfoil::core::boundary_layer::iblpan: BL array overflow — reduce "
        "number of wake panels");
  }
  mapping.valid = true;
  return mapping;
}

// Fortran: SUBROUTINE XICALC.
WakeGap xicalc(const paneling::PanelGeometry& panel,
               const BoundaryLayerMapping& mapping, double sst, double ante,
               std::span<const double> wakeX, std::span<const double> wakeY,
               BoundaryLayerArcLengths& arcs) {
  const std::size_t n = panel.x.size();
  if (!mapping.valid || mapping.top.ipan.empty() ||
      mapping.bottom.ipan.empty()) {
    throw std::invalid_argument(
        "xfoil::core::boundary_layer::xicalc: Mapping is invalid "
        "(run iblpan first)");
  }

  // Fortran MAX(a,b) with NaN: gfortran returns the non-NaN operand
  // (canonically observed in the degenerate STFIND fallback regime:
  // MAX(NaN, XEPS) = XEPS, binary oper-state/oper-init-inviscid-then-bang).
  auto fortranMax = [](double a, double b) {
    if (std::isnan(a)) return b;
    if (std::isnan(b)) return a;
    return a < b ? b : a;
  };

  // Fortran: XFEPS = 1.0E-7 (DATA).
  constexpr double kXfeps = 1.0e-7;
  const double xeps = kXfeps * (panel.s[n - 1] - panel.s[0]);

  // Fortran: top surface (IS=1). XSSI(1,IS)=0.
  arcs.top.assign(mapping.top.ipan.size(), 0.0);
  arcs.top[0] = 0.0;
  for (int ibl = 1; ibl <= mapping.top.iblte; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    const std::size_t i = static_cast<std::size_t>(mapping.top.ipan[k]);
    arcs.top[k] = fortranMax(sst - panel.s[i], xeps);
  }

  // Fortran: bottom surface (IS=2). XSSI(1,IS)=0.
  arcs.bottom.assign(mapping.bottom.ipan.size(), 0.0);
  arcs.bottom[0] = 0.0;
  for (int ibl = 1; ibl <= mapping.bottom.iblte; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    const std::size_t i = static_cast<std::size_t>(mapping.bottom.ipan[k]);
    arcs.bottom[k] = fortranMax(panel.s[i] - sst, xeps);
  }

  // Fortran: wake. IBL1=IBLTE(1)+1: XSSI(IBL1,1)=XSSI(IBL1-1,1);
  // 0-based: arcs.top[iblte1+1] = arcs.top[iblte1].
  const int ibl1 = mapping.top.iblte + 1;
  arcs.top[static_cast<std::size_t>(ibl1)] =
      arcs.top[static_cast<std::size_t>(ibl1 - 1)];
  const int ibl2 = mapping.bottom.iblte + 1;
  arcs.bottom[static_cast<std::size_t>(ibl2)] =
      arcs.bottom[static_cast<std::size_t>(ibl2 - 1)];

  // Fortran: wake loop IBL=IBLTE(IS)+2..NBL(IS) with I=IPAN(IBL,IS)=N+IW
  // (1-based); DXSSI = X(I)-X(I-1) via the wake node. 0-based:
  // k=iblte2+2..nbl2, wake node index j=k-iblte2-1 in wakeX/wakeY.
  for (int ibl = mapping.bottom.iblte + 2; ibl <= mapping.bottom.nbl; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    const std::size_t j = k - static_cast<std::size_t>(mapping.bottom.iblte) -
                          1;
    if (j == 0 || j >= wakeX.size() || j >= wakeY.size()) {
      throw std::invalid_argument(
          "xfoil::core::boundary_layer::xicalc: wake point count is smaller than "
          "NBL-IBLTE");
    }
    const double dxssi = std::sqrt((wakeX[j] - wakeX[j - 1]) *
                                       (wakeX[j] - wakeX[j - 1]) +
                                   (wakeY[j] - wakeY[j - 1]) *
                                       (wakeY[j] - wakeY[j - 1]));
    const std::size_t k1 = static_cast<std::size_t>(
        mapping.top.iblte + ibl - mapping.bottom.iblte);
    const std::size_t k2 = k;
    arcs.top[k1] = arcs.top[k1 - 1] + dxssi;
    arcs.bottom[k2] = arcs.bottom[k2 - 1] + dxssi;
  }

  // Fortran: TE flap length to TE gap ratio.
  constexpr double kTelrat = 2.50;

  // Fortran: CROSP/DWDXTE (limited).
  const double crosP =
      (panel.xp[0] * panel.yp[n - 1] - panel.yp[0] * panel.xp[n - 1]) /
      std::sqrt((panel.xp[0] * panel.xp[0] + panel.yp[0] * panel.yp[0]) *
                (panel.xp[n - 1] * panel.xp[n - 1] +
                 panel.yp[n - 1] * panel.yp[n - 1]));
  double dwdxte = crosP / std::sqrt(1.0 - crosP * crosP);
  dwdxte = std::max(dwdxte, -3.0 / kTelrat);
  dwdxte = std::min(dwdxte, 3.0 / kTelrat);
  const double aa = 3.0 + kTelrat * dwdxte;
  const double bb = -2.0 - kTelrat * dwdxte;

  WakeGap wake;
  wake.dwdxte = dwdxte;
  const std::size_t nw = static_cast<std::size_t>(mapping.bottom.nbl -
                                                  mapping.bottom.iblte);
  wake.wgap.assign(nw, 0.0);
  if (panel.sharpTe) {
    // Fortran: IF(SHARP) THEN WGAP(IW) = 0.
    for (std::size_t iw = 0; iw < nw; ++iw) {
      wake.wgap[iw] = 0.0;
    }
  } else {
    // Fortran: WGAP(IW) = ANTE*(AA+BB*ZN)*ZN^2 with
    // ZN = 1.0-(XSSI(IBL,IS)-XSSI(IBLTE(IS),IS))/(TELRAT*ANTE) for
    // IW=1..NW with IBL = IBLTE(IS)+IW (1-based) — also the wake-
    // Stations IBLTE+1..NBL. 0-based: wgap[iw] belongs to the station
    // iblteB+iw+1.
    const std::size_t iblteB = static_cast<std::size_t>(mapping.bottom.iblte);
    for (std::size_t iw = 0; iw < nw; ++iw) {
      const std::size_t ibl = iblteB + iw + 1;
      const double zn =
          1.0 - (arcs.bottom[ibl] - arcs.bottom[iblteB]) /
                    (kTelrat * ante);
      wake.wgap[iw] = 0.0;
      if (zn >= 0.0) {
        wake.wgap[iw] = ante * (aa + bb * zn) * zn * zn;
      }
    }
  }
  (void)kDefaultBlCapacity;
  return wake;
}

// Fortran: SUBROUTINE XIFSET(IS).
Result<TransitionState> xifset(const paneling::PanelGeometry& panel,
                               const BoundaryLayerMapping& mapping,
                               const BoundaryLayerArcLengths& arcs, double sst,
                               double xstrip, SurfaceSide side) {
  const std::size_t n = panel.x.size();
  if (!mapping.valid) {
    throw std::invalid_argument(
        "xfoil::core::boundary_layer::xifset: Mapping is invalid "
        "(run iblpan first)");
  }
  const BoundaryLayerSideMapping& sm =
      side == SurfaceSide::kTop ? mapping.top : mapping.bottom;
  TransitionState ts;
  ts.tforce = true;

  // Fortran: IF(XSTRIP(IS).GE.1.0) THEN XIFORC = XSSI(IBLTE(IS),IS); RETURN.
  const std::size_t iblteS = static_cast<std::size_t>(sm.iblte);
  if (xstrip >= 1.0) {
    // Fortran: XIFORC = XSSI(IBLTE(IS),IS) — strict pages separate.
    ts.xiforc = (side == SurfaceSide::kTop) ? arcs.top[iblteS]
                                            : arcs.bottom[iblteS];
    return Result<TransitionState>::ok(ts);
  }

  // Fortran: CHX = XTE - XLE; CHY = YTE - YLE; CHSQ = CHX^2 + CHY^2.
  const double chx = panel.xte - panel.xle;
  const double chy = panel.yte - panel.yle;
  const double chsq = chx * chx + chy * chy;

  // Fortran: chord-based x/c, y/c. W1(I)=((X-XLE)*CHX+(Y-YLE)*CHY)/CHSQ;
  // W2(I)=((Y-YLE)*CHX-(X-XLE)*CHY)/CHSQ; SPLIND(W1,W3,S,N,-999,-999);
  // SPLIND(W2,W4,S,N,-999,-999).
  std::vector<double> w1(n);
  std::vector<double> w2(n);
  for (std::size_t i = 0; i < n; ++i) {
    w1[i] = ((panel.x[i] - panel.xle) * chx + (panel.y[i] - panel.yle) * chy) /
            chsq;
    w2[i] = ((panel.y[i] - panel.yle) * chx - (panel.x[i] - panel.xle) * chy) /
            chsq;
  }
  std::vector<double> w3(n);
  std::vector<double> w4(n);
  math::splind(w1, panel.s, w3, -999.0, -999.0);
  math::splind(w2, panel.s, w4, -999.0, -999.0);

  // Fortran: STR = SLE + (S(1)-SLE)*XSTRIP (top) or. SLE+(S(N)-SLE)*XSTRIP
  // (bottom); SINVRT(STR, XSTRIP, W1, W3, S, N);
  // XIFORC = MIN(SST-STR or. STR-SST, XSSI(IBLTE(IS),IS)).
  double str = 0.0;
  double xssiTe = 0.0;
  if (side == SurfaceSide::kTop) {
    str = panel.sle + (panel.s[0] - panel.sle) * xstrip;
    const math::Diagnostic diag =
        math::sinvrt(str, xstrip, w1, w3, panel.s);
    static_cast<void>(diag);
    ts.xiforc = std::min(sst - str, arcs.top[iblteS]);
    xssiTe = arcs.top[iblteS];
  } else {
    str = panel.sle + (panel.s[n - 1] - panel.sle) * xstrip;
    const math::Diagnostic diag =
        math::sinvrt(str, xstrip, w1, w3, panel.s);
    static_cast<void>(diag);
    ts.xiforc = std::min(str - sst, arcs.bottom[iblteS]);
    xssiTe = arcs.bottom[iblteS];
  }

  // Fortran: IF(XIFORC .LT. 0.0) THEN message + XIFORC=XSSI(IBLTE(IS),IS)
  // (the caller continues with the corrected value).
  if (ts.xiforc < 0.0) {
    ts.xiforc = xssiTe;
    ts.pastTrip = true;
  }
  return Result<TransitionState>::ok(ts);
}

}  // namespace xfoil::core::boundary_layer

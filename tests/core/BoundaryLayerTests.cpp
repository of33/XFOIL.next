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

// File: tests/core/BoundaryLayerTests.cpp
// Unit tests for the new 0-based core: boundary-layer data model and state
// mapping (iblpan, xicalc, xifset, blpar, blprv, blkin, SETBL data part).
//
//
// the tests use only xfoil::core::boundary_layer and no app, CLI, or
// 1-based code.
//
// reference comparison: the expected values come from the focused Fortran
// harness (tests/fortran-reference/harnesses/boundary-layer-data), which
// calls the canonical routines unchanged (BLPINI from xbl.f, IBLPAN/XICALC
// from xpanel.f, XIFSET from xbl.f, BLPRV/BLKIN from xblsys.f).
// Fortran REAL is single precision; the new core computes in double.
// Formula and operation order are identical.

#include "TestSupport.h"

#include <cstddef>
#include <stdexcept>
#include <vector>

#include <cmath>

#include "xfoil/core/Result.h"
#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/Viscal.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverKernels.h"
#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/paneling/Pangen.h"

namespace {

using namespace xfoil::core;
using namespace xfoil::core::boundary_layer;

// test profile (pangen result, analogous to the tests).
paneling::PanelGeometry makeTestProfile() {
  // 10-node profile (upper side 0..4, lower side 4..9), sharp TE.
  return paneling::PanelGeometry::make(
      std::vector<double>{1.0, 0.9, 0.5, 0.1, 0.0, 0.0, 0.1, 0.5, 0.9, 1.0},
      std::vector<double>{0.0, 0.05, 0.08, 0.05, 0.0, 0.0, -0.05, -0.08,
                          -0.05, 0.0},
      std::vector<double>{0.0, 0.1, 0.55, 0.9, 1.0, 1.1, 1.55, 1.9, 2.1, 2.2},
      std::vector<double>{-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0,
                          -1.0, -1.0},
      std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      std::vector<double>{0.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0,
                          -1.0, -1.0},
      std::vector<double>{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
      0.0, 0.0, 0.0, 1.0, 0.0, 1.0, true);
}

constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;

// Fortran: BLPINI (xbl.f:1578). Reference: boundary-layer_reference.txt
// BLPINI1.
TEST(unit, BlParConstants) {
  const BoundaryLayerParameters p = blparConstants();
  CHECK_NEAR(p.sccon, 5.6, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.gacon, 6.70, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.gbcon, 0.75, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.gccon, 18.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.dlcon, 0.9, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ctrcon, 1.8, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ctrcex, 3.3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.duxcon, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ctcon, 0.5 / (6.70 * 6.70 * 0.75), kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.cffac, 1.0, kFortranAbsTol, kFortranRelTol);
}

// Fortran: SETBL data part (xbl.f:46-85). Reference:
// boundary-layer_reference.txt SETBL1 (MINF=0, REINF=3e6, GAMMA=1.4).
TEST(unit, BlPropertiesIncompressible) {
  const BoundaryLayerProperties r = boundaryLayerProperties(
      0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0, 0);
  CHECK_NEAR(r.gambl, 1.4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.gm1bl, 0.4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.qinfbl, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.rstbl, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.rstblMs, 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.hstinv, 0.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: HSTINV_MS = GM1BL*(1/QINFBL)^2/(1+0.5*GM1BL*MINF^2)
  // -0.5*GM1BL*HSTINV/(1+0.5*GM1BL*MINF^2) = 0.4 for MINF=0.
  CHECK_NEAR(r.hstinvMs, 0.4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.herat, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.heratMs, -0.5 * 1.0 * 0.4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.reybl, 3.0e6, 1.0e-3, kFortranRelTol);
  // Fortran: REYBL_Re = SQRT(HERAT^3)*(1+HVRAT)/(HERAT+HVRAT) = 1.
  CHECK_NEAR(r.reyblRe, 1.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: REYBL_MS = REYBL*(1.5/HERAT-1/(HERAT+HVRAT))*HERAT_MS.
  CHECK_NEAR(r.reyblMs, 3.0e6 * 0.5 * (-0.2), 1.0e-3, kFortranRelTol);
  CHECK_NEAR(r.dwte, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK(r.idampv == 0);
}

// Fortran: SETBL data part (xbl.f:46-85), compressible. Reference:
// boundary-layer_reference.txt SETBL2 (MINF=0.3, REINF=3e6, GAMMA=1.4).
TEST(unit, BlPropertiesCompressible) {
  const double minf = 0.3;
  const double gamm1 = 0.4;
  const double t0 = 1.0 + 0.5 * gamm1 * minf * minf;
  const double qinfbl = 1.0;
  const double rstbl = std::pow(t0, 1.0 / gamm1);
  const double hstinv = gamm1 * minf * minf / t0;
  const double herat = 1.0 - 0.5 * qinfbl * qinfbl * hstinv;
  const BoundaryLayerProperties r = boundaryLayerProperties(
      minf, 3.0e6, qinfbl, 1.4, gamm1, 0.0, 0.0, 0.0, 0.0, 1);
  CHECK_NEAR(r.rstbl, rstbl, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.hstinv, hstinv, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.herat, herat, kFortranAbsTol, kFortranRelTol);
  // Fortran: REYBL = REINF*SQRT(HERAT^3)*(1+HVRAT)/(HERAT+HVRAT);
  // HVRAT=0 -> (1)/(HERAT).
  CHECK_NEAR(r.reybl, 3.0e6 * std::sqrt(herat * herat * herat) / herat,
             1.0e-3, kFortranRelTol);
  CHECK(r.idampv == 1);
}

// Fortran: IBLPAN (xpanel.f:1395). Reference: boundary-layer_reference.txt
// IBLPAN1. 10-node profile, stagnation point at 0-based index 4, NW=2.
TEST(unit, BlIblpanMapping) {
  const paneling::PanelGeometry pg = makeTestProfile();
  const BoundaryLayerMapping m = iblpan(pg, 4, 2, 0);
  CHECK(m.valid);
  // Fortran mirroring: ipan[k] = IPAN(k+1,IS); IPAN(1,IS) stays unset
  // (IBL is incremented before the assignment). 0-based: top stations
  // [4,3,2,1,0] at indices 1..5, TE entry (node 0) at index 5.
  CHECK(m.top.iblte == 5);
  CHECK(m.top.nbl == 5);
  // Fortran: upper wake pointers (plot only): IPAN(IBLTE(1)+IW,1)=
  // IPAN(IBLTE(2)+IW,2) -> wake entries at indices 6,7.
  CHECK(m.top.ipan.size() == 8);
  CHECK(m.top.ipan[0] == 0);
  CHECK(m.top.ipan[1] == 4);
  CHECK(m.top.ipan[2] == 3);
  CHECK(m.top.ipan[3] == 2);
  CHECK(m.top.ipan[4] == 1);
  CHECK(m.top.ipan[5] == 0);
  CHECK(m.top.ipan[6] == 10);
  CHECK(m.top.ipan[7] == 11);
  CHECK_NEAR(m.top.vti[1], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.top.vti[5], 1.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: lower side I=Is+1..N-1 (0-based 5..9), VTI=-1.
  CHECK(m.bottom.ipan.size() == 8);
  CHECK(m.bottom.ipan[0] == 0);
  CHECK(m.bottom.ipan[1] == 5);
  CHECK(m.bottom.ipan[2] == 6);
  CHECK(m.bottom.ipan[3] == 7);
  CHECK(m.bottom.ipan[4] == 8);
  CHECK(m.bottom.ipan[5] == 9);
  CHECK(m.bottom.ipan[6] == 10);
  CHECK(m.bottom.ipan[7] == 11);
  CHECK_NEAR(m.bottom.vti[1], -1.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: Wake IBLTE(2)+IW (1-based 7,8), VTI=-1.
  CHECK(m.bottom.iblte == 5);
  CHECK(m.bottom.nbl == 7);
  CHECK(m.bottom.ipan[6] == 10);
  CHECK(m.bottom.ipan[7] == 11);
  // Fortran: IBLMAX = MAX(IBLTE(1), IBLTE(2)) + NW = 8 (1-based);
  // 0-based last index 7.
  CHECK(m.iblmax == 7);
}

// Fortran: XICALC (xpanel.f:1455). Reference: boundary-layer_reference.txt
// XICALC1. XEPS = 1.0E-7*(S(N)-S(1)).
TEST(unit, BlXicalcArcLengths) {
  const paneling::PanelGeometry pg = makeTestProfile();
  const BoundaryLayerMapping m = iblpan(pg, 4, 2, 0);
  BoundaryLayerArcLengths arcs;
  const std::vector<double> wakeX{1.05, 1.15};
  const std::vector<double> wakeY{0.0, 0.0};
  const WakeGap wg = xicalc(pg, m, pg.s[4], 0.0, wakeX, wakeY, arcs);
  // Fortran mirroring: arcs[k] = XSSI(k+1,IS); size = NBL(IS)+1.
  CHECK(arcs.top.size() == 8);
  CHECK(arcs.bottom.size() == 8);
  CHECK_NEAR(arcs.top[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.bottom[0], 0.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: XSSI(IBL,1)=MAX(SST-S(I), XEPS); XSSI(IBL,2)=MAX(S(I)-SST, XEPS).
  const double xeps = 1.0e-7 * (pg.s[9] - pg.s[0]);
  for (int ibl = 1; ibl <= m.top.iblte; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    const std::size_t i = static_cast<std::size_t>(m.top.ipan[k]);
    const double expect = std::max(pg.s[4] - pg.s[i], xeps);
    CHECK_NEAR(arcs.top[k], expect, kFortranAbsTol, kFortranRelTol);
  }
  for (int ibl = 1; ibl <= m.bottom.iblte; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    const std::size_t i = static_cast<std::size_t>(m.bottom.ipan[k]);
    const double expect = std::max(pg.s[i] - pg.s[4], xeps);
    CHECK_NEAR(arcs.bottom[k], expect, kFortranAbsTol, kFortranRelTol);
  }
  // Fortran: wake loop XSSI(IBL,IS) = XSSI(IBL-1,IS) + DXSSI via the
  // wake node (j=k-IBLTE-1 in wakeX/wakeY).
  for (int ibl = m.bottom.iblte + 2; ibl <= m.bottom.nbl; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    const std::size_t j = k - static_cast<std::size_t>(m.bottom.iblte) - 1;
    const double dxssi = std::sqrt((wakeX[j] - wakeX[j - 1]) *
                                       (wakeX[j] - wakeX[j - 1]) +
                                   (wakeY[j] - wakeY[j - 1]) *
                                       (wakeY[j] - wakeY[j - 1]));
    CHECK_NEAR(arcs.bottom[k], arcs.bottom[k - 1] + dxssi, kFortranAbsTol,
               kFortranRelTol);
  }
  // Fortran: DWDXTE = CROSP/sqrt(1-CROSP^2), limited to +/-3.0/TELRAT.
  const double crosP = (pg.xp[0] * pg.yp[9] - pg.yp[0] * pg.xp[9]) /
                       std::sqrt((pg.xp[0] * pg.xp[0] + pg.yp[0] * pg.yp[0]) *
                                 (pg.xp[9] * pg.xp[9] + pg.yp[9] * pg.yp[9]));
  const double dwdxte = std::min(
      3.0 / 2.50,
      std::max(-3.0 / 2.50, crosP / std::sqrt(1.0 - crosP * crosP)));
  CHECK_NEAR(wg.dwdxte, dwdxte, kFortranAbsTol, kFortranRelTol);
  // Fortran: WGAP(IW) = 0 for SHARP (test profile).
  for (const double w : wg.wgap) {
    CHECK_NEAR(w, 0.0, kFortranAbsTol, kFortranRelTol);
  }
}

// Fortran: XIFSET (xbl.f:1196), XSTRIP>=1.0. Reference:
// boundary-layer_reference.txt XIFSET1 (XSTRIP=1.0).
TEST(unit, BlXifsetNoStrip) {
  const paneling::PanelGeometry pg = makeTestProfile();
  const BoundaryLayerMapping m = iblpan(pg, 4, 2, 0);
  BoundaryLayerArcLengths arcs;
  static_cast<void>(
      xicalc(pg, m, pg.s[4], 0.0, std::vector<double>{1.05, 1.15},
             std::vector<double>{0.0, 0.0}, arcs));
  const Result<TransitionState> ts = xifset(pg, m, arcs, pg.s[4], 1.0,
                                            SurfaceSide::kTop);
  CHECK(ts.isOk());
  CHECK(ts.value().tforce);
  CHECK_NEAR(ts.value().xiforc,
             arcs.top[static_cast<std::size_t>(m.top.iblte)], kFortranAbsTol,
             kFortranRelTol);
}

// Fortran: BLPRV (xblsys.f:701). Reference: boundary-layer_reference.txt
// BLPRV1 (incompressible, UEI=1).
TEST(unit, BlPrvIncompressible) {
  const BoundaryLayerProperties props = boundaryLayerProperties(
      0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0, 0);
  const BoundaryLayerPrimary p = blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0,
                                       props);
  CHECK_NEAR(p.x, 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ampl, 0.1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.s, 0.01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.t, 0.02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.d, 0.03, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.dw, 0.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: U2 = UEI*(1-TKBL)/(1-TKBL*(UEI/QINFBL)^2), TKBL=0 ->
  // U2 = UEI.
  CHECK_NEAR(p.ue, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ueEi, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ueMs, 0.0, kFortranAbsTol, kFortranRelTol);
}

// Fortran: BLPRV (xblsys.f:701), compressible (TKBL>0). Reference:
// boundary-layer_reference.txt BLPRV2 (MINF=0.3, UEI=1).
TEST(unit, BlPrvCompressible) {
  const double minf = 0.3;
  const double gamm1 = 0.4;
  const double t0 = 1.0 + 0.5 * gamm1 * minf * minf;
  const double qinf = 1.0;
  const double tklam = gamm1 * minf * minf / t0;
  const double tkMsq = gamm1 / t0;
  const BoundaryLayerProperties props = boundaryLayerProperties(
      minf, 3.0e6, qinf, 1.4, gamm1, tklam, tkMsq, 0.0, 0.0, 0);
  const double uei = 1.0;
  const double ueQ = uei / qinf;
  const double den = 1.0 - tklam * ueQ * ueQ;
  const double expect = uei * (1.0 - tklam) / den;
  const BoundaryLayerPrimary p = blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, uei,
                                       props);
  CHECK_NEAR(p.ue, expect, kFortranAbsTol, kFortranRelTol);
  const double expectEi = (1.0 + tklam * (2.0 * p.ue * uei - 1.0)) / den;
  CHECK_NEAR(p.ueEi, expectEi, kFortranAbsTol, kFortranRelTol);
  const double expectMs =
      (p.ue * ueQ * ueQ - uei) * tkMsq / den;
  CHECK_NEAR(p.ueMs, expectMs, kFortranAbsTol, kFortranRelTol);
}

// Fortran: BLKIN (xblsys.f:725), incompressible (HSTINV=0). Reference:
// boundary-layer_reference.txt BLKIN1 (incompressible, UEI=1).
TEST(unit, BlKinIncompressible) {
  const BoundaryLayerProperties props = boundaryLayerProperties(
      0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0, 0);
  const BoundaryLayerPrimary p = blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0,
                                       props);
  const BoundaryLayerSecondary s = blkin(p, props);
  // Fortran: M2 = U2^2*HSTINV/(GM1BL*(1-0.5*U2^2*HSTINV)) = 0 (HSTINV=0).
  CHECK_NEAR(s.m, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.tr, 1.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: R2 = RSTBL*TR2^(-1/GM1BL) = 1 (RSTBL=1, TR2=1).
  CHECK_NEAR(s.r, 1.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: H2 = D2/T2.
  CHECK_NEAR(s.h, p.d / p.t, kFortranAbsTol, kFortranRelTol);
  // Fortran: HERAT = 1-0.5*U2^2*HSTINV = 1.
  CHECK_NEAR(s.herat, 1.0, kFortranAbsTol, kFortranRelTol);
  // Fortran: V2 = SQRT(HERAT^3)*(1+HVRAT)/(HERAT+HVRAT)/REYBL = 1/REYBL.
  CHECK_NEAR(s.v, 1.0 / 3.0e6, 1.0e-9, kFortranRelTol);
  // Fortran: HKIN (Whitfield): HK = (H-0.29*MSQ)/(1+0.113*MSQ) = H.
  CHECK_NEAR(s.hk, s.h, kFortranAbsTol, kFortranRelTol);
  // Fortran: RT2 = R2*U2*T2/V2 = 3e6*T2.
  CHECK_NEAR(s.rt, 1.0 * p.ue * p.t / s.v, 1.0e-3, kFortranRelTol);
  CHECK_NEAR(s.rt, 3.0e6 * p.t, 1.0e-3, kFortranRelTol);
}

// Fortran: BLKIN (xblsys.f:725), compressible (MINF=0.3). Reference:
// boundary-layer_reference.txt BLKIN2 (MINF=0.3, UEI=1).
TEST(unit, BlKinCompressible) {
  const double minf = 0.3;
  const double gamm1 = 0.4;
  const double t0 = 1.0 + 0.5 * gamm1 * minf * minf;
  const double qinf = 1.0;
  const double tklam = gamm1 * minf * minf / t0;
  const double tkMsq = gamm1 / t0;
  const BoundaryLayerProperties props = boundaryLayerProperties(
      minf, 3.0e6, qinf, 1.4, gamm1, tklam, tkMsq, 0.0, 0.0, 0);
  const BoundaryLayerPrimary p = blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0,
                                       props);
  const BoundaryLayerSecondary s = blkin(p, props);
  const double u2h = 0.5 * p.ue * p.ue * props.hstinv;
  const double mden = gamm1 * (1.0 - u2h);
  const double expectM = p.ue * p.ue * props.hstinv / mden;
  CHECK_NEAR(s.m, expectM, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.tr, 1.0 + 0.5 * gamm1 * s.m, kFortranAbsTol, kFortranRelTol);
  const double expectR = props.rstbl *
                         std::pow(s.tr, -1.0 / gamm1);
  CHECK_NEAR(s.r, expectR, kFortranAbsTol, kFortranRelTol);
  const double expectH = p.d / p.t;
  CHECK_NEAR(s.h, expectH, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.herat, 1.0 - u2h, kFortranAbsTol, kFortranRelTol);
  const double expectV = std::pow(s.herat, 1.5) * (1.0 + props.hvrat) /
                         (s.herat + props.hvrat) / props.reybl;
  CHECK_NEAR(s.v, expectV, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(s.rt, s.r * p.ue * p.t / s.v, 1.0e-3, kFortranRelTol);
}

// Fortran: IBLPAN capacity. An IVX that is too small returns the canonical
// diagnostics (STOP) as structured diagnostics.
TEST(unit, BlIblpanCapacity) {
  const paneling::PanelGeometry pg = makeTestProfile();
  // Fortran: IBLMAX = MAX(IBLTE(1),IBLTE(2)) + NW = 8 (1-based);
  // IBLMAX>IVX -> STOP. IVX=8 is exactly enough, IVX=5 throws.
  const BoundaryLayerMapping m = iblpan(pg, 4, 2, 8);
  CHECK(m.valid);
  CHECK(m.iblmax == 7);
  bool threw = false;
  try {
    const BoundaryLayerMapping ignored = iblpan(pg, 4, 2, 5);
    static_cast<void>(ignored);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}


// Fortran: XIFSET, XIFORC<0 (stagnation point behind the trip): canonical
// fix XIFORC=XSSI(IBLTE(IS),IS); callers continue (no abort).
TEST(unit, BlXifsetPastTrip) {
  const paneling::PanelGeometry pg = makeTestProfile();
  const BoundaryLayerMapping m = iblpan(pg, 4, 2, 0);
  BoundaryLayerArcLengths arcs;
  static_cast<void>(
      xicalc(pg, m, pg.s[4], 0.0, std::vector<double>{1.05, 1.15},
             std::vector<double>{0.0, 0.0}, arcs));
  // Synthetic SST < STR: STR = SLE*(1-XSTRIP) = 0.9 > 0.5 -> XIFORC<0.
  const Result<TransitionState> ts = xifset(pg, m, arcs, 0.5, 0.1,
                                            SurfaceSide::kTop);
  CHECK(ts.isOk());
  CHECK(ts.value().pastTrip);
  CHECK_NEAR(ts.value().xiforc,
             arcs.top[static_cast<std::size_t>(m.top.iblte)], kFortranAbsTol,
             kFortranRelTol);
}


// NACA 0012 + PANGEN + GGCALC + SPECAL(ALFA=0) + STFIND + XYWAKE
// (canonical chain, as in the Fortran harness).
struct BlPipeline {
  paneling::PanelGeometry panel;
  inviscid::StagnationPointResult st;
  std::size_t nw{0};
  std::vector<double> wakeX;
  std::vector<double> wakeY;
  bool ok{false};
};

BlPipeline makeBlPipeline0012() {
  BlPipeline out;
  const inviscid::MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<geometry::NacaResult> n = geometry::naca4(12, 123);
  if (!n.isOk()) {
    return out;
  }
  const geometry::AirfoilBuffer buffer(n.value().xb, n.value().yb,
                                       n.value().name);
  const Result<paneling::PangenResult> pg =
      paneling::pangen(buffer, paneling::PanelingParameters{});
  if (pg.isErr()) {
    return out;
  }
  out.panel = pg.value().panel;
  const std::vector<double> sig0(out.panel.s.size(), 0.0);
  const Result<inviscid::InfluenceSystem> gs =
      inviscid::ggCalc(out.panel, sig0, 0.0, 1.0);
  if (gs.isErr()) {
    return out;
  }
  const Result<inviscid::SpecPointResult> sp =
      inviscid::specAlpha(gs.value(), out.panel, settings, 0.0,
                          1.0, 0.25, 0.0);
  if (sp.isErr()) {
    return out;
  }
  out.st = inviscid::stfind(sp.value().gam, out.panel.s);
  const Result<inviscid::WakeState> wk =
      inviscid::xywake(out.panel, sp.value().gam, sp.value().gamA,
                       gs.value().gamu1, gs.value().gamu2, 0.0, 1.0, 1.0);
  if (wk.isErr()) {
    return out;
  }
  out.nw = wk.value().nw;
  out.wakeX = wk.value().x;
  out.wakeY = wk.value().y;
  out.ok = true;
  return out;
}

// reference: boundary_layer_reference.txt BLPINI (REAL(4) outputs).
TEST(unit, BlParDirectFortran) {
  const BoundaryLayerParameters p = blparConstants();
  CHECK_NEAR(p.sccon, 5.59999990, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.gacon, 6.69999981, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.gbcon, 0.75, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.gccon, 18.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.dlcon, 0.899999976, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ctrcon, 1.79999995, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ctrcex, 3.29999995, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.duxcon, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ctcon, 1.48511194e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.cffac, 1.0, kFortranAbsTol, kFortranRelTol);
}

// reference: boundary_layer_reference.txt SETBL3 (MINF=0.3, REINF=3e6,
// QINF=1, HVRAT=0).
TEST(unit, BlPropertiesDirectFortran) {
  const double minf = 0.3;
  const double beta = std::sqrt(1.0 - minf * minf);
  const double tklam = minf * minf / ((1.0 + beta) * (1.0 + beta));
  const double betaMsq = -0.5 / beta;
  const double tkMsq =
      1.0 / ((1.0 + beta) * (1.0 + beta)) -
      2.0 * tklam / (1.0 + beta) * betaMsq;
  const BoundaryLayerProperties r = boundaryLayerProperties(
      minf, 3.0e6, 1.0, 1.4, 0.4, tklam, tkMsq, 0.0, 0.0, 0);
  CHECK_NEAR(r.rstbl, 1.04560935, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.hstinv, 3.53634544e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.herat, 9.82318282e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.reybl, 2.97335900e6, 1.0e-3, kFortranRelTol);
  CHECK_NEAR(r.reyblMs, -2.92078438e5, 1.0e-3, kFortranRelTol);
  CHECK_NEAR(r.reyblRe, 9.91119742e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.tkbl, 2.35733017e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.tkblMs, 2.74572611e-1, kFortranAbsTol, kFortranRelTol);
}

// reference: boundary_layer_reference.txt IBLPAN12 (integer mapping values,
// exact equality).
TEST(unit, BlIblpanDirectFortranNaca0012) {
  const BlPipeline pl = makeBlPipeline0012();
  CHECK(pl.ok);
  const BoundaryLayerMapping m = iblpan(pl.panel, pl.st.ist, pl.nw, 283);
  CHECK(m.valid);
  CHECK_EQ(pl.panel.x.size(), std::size_t{160});
  CHECK_EQ(pl.st.ist, std::size_t{79});
  CHECK_EQ(pl.nw, std::size_t{23});
  // Fortran IBLTE1=IBLTE2=81, NBL1=81, NBL2=104 (1-based).
  CHECK_EQ(m.top.iblte, 80);
  CHECK_EQ(m.bottom.iblte, 80);
  CHECK_EQ(m.top.nbl, 80);
  CHECK_EQ(m.bottom.nbl, 103);
  // Fortran IPAN(1,1)=IPAN(1,2)=0 (unset), IPAN(2,1)=Is=80 (1-based).
  CHECK_EQ(m.top.ipan[0], 0);
  CHECK_EQ(m.top.ipan[1], 79);
  CHECK_EQ(m.bottom.ipan[0], 0);
  CHECK_EQ(m.bottom.ipan[1], 80);
  CHECK_EQ(m.top.ipan[static_cast<std::size_t>(m.top.iblte)], 0);
  CHECK_EQ(m.bottom.ipan[static_cast<std::size_t>(m.bottom.iblte)], 159);
  CHECK_EQ(m.bottom.ipan[static_cast<std::size_t>(m.bottom.iblte) + 1], 160);
  CHECK_EQ(m.bottom.ipan[static_cast<std::size_t>(m.bottom.iblte) + 2], 161);
  CHECK_NEAR(m.top.vti[1], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.bottom.vti[1], -1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.bottom.vti[static_cast<std::size_t>(m.bottom.iblte)], -1.0,
             kFortranAbsTol, kFortranRelTol);
}

// reference: boundary_layer_reference.txt XICALC12/XIFSET1-3.
TEST(unit, BlXicalcDirectFortranNaca0012) {
  const BlPipeline pl = makeBlPipeline0012();
  CHECK(pl.ok);
  const BoundaryLayerMapping m = iblpan(pl.panel, pl.st.ist, pl.nw, 283);
  BoundaryLayerArcLengths arcs;
  const WakeGap wg =
      xicalc(pl.panel, m, pl.st.sst, 0.0, pl.wakeX, pl.wakeY, arcs);
  // Fortran: 1-based maximum IBLTE(1)+NW = 104 or NBL(2) = 104.
  CHECK_EQ(arcs.top.size(), std::size_t{104});
  CHECK_EQ(arcs.bottom.size(), std::size_t{104});
  CHECK_NEAR(arcs.top[1], 9.05752182e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.top[80], 1.01962137, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.bottom[1], 9.05513763e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.bottom[80], 1.01962137, kFortranAbsTol, kFortranRelTol);
  // Fortran XSSI(82,1)/XSSI(83,1) and XSSI(82,2)/XSSI(83,2) (wake).
  CHECK_NEAR(arcs.top[81], 1.01962137, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.top[82], 1.02802253, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.bottom[81], 1.01962137, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.bottom[82], 1.02802253, kFortranAbsTol, kFortranRelTol);
  static_cast<void>(wg);
  // Fortran XIFSET1 (XSTRIP=1.0), XIFSET2 (IS=1, XSTRIP=0.3), XIFSET3
  // (IS=2, XSTRIP=0.3).
  const Result<TransitionState> t1 =
      xifset(pl.panel, m, arcs, pl.st.sst, 1.0, SurfaceSide::kTop);
  CHECK(t1.isOk());
  CHECK_FALSE(t1.value().pastTrip);
  CHECK_NEAR(t1.value().xiforc, 1.01962137, kFortranAbsTol, kFortranRelTol);
  const Result<TransitionState> t2 =
      xifset(pl.panel, m, arcs, pl.st.sst, 0.3, SurfaceSide::kTop);
  CHECK(t2.isOk());
  CHECK_NEAR(t2.value().xiforc, 3.16690445e-1, kFortranAbsTol, kFortranRelTol);
  const Result<TransitionState> t3 =
      xifset(pl.panel, m, arcs, pl.st.sst, 0.3, SurfaceSide::kBottom);
  CHECK(t3.isOk());
  CHECK_NEAR(t3.value().xiforc, 3.16690326e-1, kFortranAbsTol, kFortranRelTol);
}

// reference: boundary_layer_reference.txt BLPRV9/BLKIN9 (HK2 = HKIN evidence).
TEST(unit, BlPrvKinDirectFortran) {
  const double minf = 0.3;
  const double beta = std::sqrt(1.0 - minf * minf);
  const double tklam = minf * minf / ((1.0 + beta) * (1.0 + beta));
  const double betaMsq = -0.5 / beta;
  const double tkMsq =
      1.0 / ((1.0 + beta) * (1.0 + beta)) -
      2.0 * tklam / (1.0 + beta) * betaMsq;
  const BoundaryLayerProperties props = boundaryLayerProperties(
      minf, 3.0e6, 1.0, 1.4, 0.4, tklam, tkMsq, 0.0, 0.0, 0);
  const BoundaryLayerPrimary p =
      blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0, props);
  CHECK_NEAR(p.x, 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ampl, 0.1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.s, 0.01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.t, 0.02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.d, 0.03, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.dw, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ue, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ueEi, 1.04828477, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.ueMs, 0.0, kFortranAbsTol, kFortranRelTol);
  const BoundaryLayerSecondary s = blkin(p, props);
  CHECK_NEAR(s.m, 8.99999961e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.r, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.h, 1.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.herat, 9.82318282e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.v, 3.33333361e-7, 1.0e-9, kFortranRelTol);
  // Fortran HKIN (Whitfield) via BLKIN9 HK2.
  CHECK_NEAR(s.hk, 1.45906138, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.rt, 5.99999922e4, 1.0e-2, kFortranRelTol);
}

// Fortran: BLVAR clamps COMMON HK2 (xblsys.f:799-800). The clamped
// HK2 acts on all COMMON readers (BLDIF, BLMID, MRCHDU/MRCHUE
// slope equations). The port must expose the clamped value.
TEST(unit, BlVarHk2ClampExposed) {
  const BoundaryLayerProperties props = boundaryLayerProperties(
      0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0, 0);
  const BoundaryLayerPrimary p =
      blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0, props);
  BoundaryLayerSecondary s = blkin(p, props);
  s.hk = 1.03;
  BoundaryLayerParameters bp = blparConstants();
  bp.gbcon = 6.0;
  bp.gccon = 6.7;
  bp.ctcon = 1.8;
  BlvarInput in{p, s, bp};
  const auto tsTurb = blvar(StationType::kTurbulent, in);
  CHECK_NEAR(tsTurb.hk, 1.05000, 0.0, 0.0);
  const auto tsLam = blvar(StationType::kLaminar, in);
  CHECK_NEAR(tsLam.hk, 1.05000, 0.0, 0.0);
  s.hk = 1.00001;
  in.secondary = s;
  const auto tsWake = blvar(StationType::kWake, in);
  CHECK_NEAR(tsWake.hk, 1.00005, 0.0, 0.0);
  s.hk = 1.06;
  in.secondary = s;
  const auto tsHigh = blvar(StationType::kTurbulent, in);
  CHECK_NEAR(tsHigh.hk, 1.06, 0.0, 0.0);
}

// IPAN COMMON contract (IVX): IBLPAN overwrites only the active range; the
// tail keeps its capacity and is preserved (xpol.f PLXADD reads up to
// NSIDE(IS)). preserveIpanTail implements exactly this lifecycle, so only
// the active prefix changes.
TEST(unit, BlIpanTailShrinkKeepsTail) {
  const std::vector<int> previous = {5, 6, 7, 8, 9, 10};  // point 1
  std::vector<int> fresh = {1, 2, 3};                     // point 2, shorter
  preserveIpanTail(previous, fresh);
  CHECK_EQ(fresh.size(), 6u);
  CHECK_EQ(fresh[0], 1);
  CHECK_EQ(fresh[1], 2);
  CHECK_EQ(fresh[2], 3);
  CHECK_EQ(fresh[3], 8);  // retained COMMON tail
  CHECK_EQ(fresh[4], 9);
  CHECK_EQ(fresh[5], 10);
}

TEST(unit, BlIpanTailGrowOverwritesActiveOnly) {
  const std::vector<int> previous = {5, 6, 7};
  std::vector<int> fresh = {1, 2, 3, 4, 5};
  preserveIpanTail(previous, fresh);
  CHECK_EQ(fresh.size(), 5u);
  CHECK_EQ(fresh[3], 4);
  CHECK_EQ(fresh[4], 5);
}

TEST(unit, BlPlxaddStationCapacitySyncGrowOnly) {
  // IPAN and the PLXADD station fields share one COMMON capacity: grow-only,
  // zeros for never-written entries, and existing values are retained when
  // the active range grows.
  ViscalState state;
  state.mapping.top.ipan = {3, 4};
  state.mapping.bottom.ipan = {5};
  state.thet.top = {1.0, 2.0, 3.0, 4.0};
  state.thet.bottom = {5.0, 6.0};
  state.dstr.top = {7.0};
  state.ctau.bottom = {8.0, 9.0};
  state.tau.top = {10.0, 11.0, 12.0};
  syncPlxaddStationCapacity(state);
  CHECK_EQ(state.mapping.top.ipan.size(), 4u);
  CHECK_EQ(state.mapping.bottom.ipan.size(), 2u);
  CHECK_EQ(state.mapping.top.ipan[0], 3);
  CHECK_EQ(state.mapping.top.ipan[1], 4);
  CHECK_EQ(state.mapping.top.ipan[2], 0);  // COMMON zero, never written
  CHECK_EQ(state.mapping.top.ipan[3], 0);
  CHECK_EQ(state.dstr.top.size(), 4u);
  CHECK_NEAR(state.dstr.top[0], 7.0, 0.0, 0.0);
  CHECK_NEAR(state.dstr.top[1], 0.0, 0.0, 0.0);
  CHECK_NEAR(state.thet.top[3], 4.0, 0.0, 0.0);
  CHECK_NEAR(state.tau.top[2], 12.0, 0.0, 0.0);
  // Shrink-free: a repeated call changes nothing.
  syncPlxaddStationCapacity(state);
  CHECK_EQ(state.mapping.top.ipan.size(), 4u);
  CHECK_NEAR(state.thet.top[3], 4.0, 0.0, 0.0);
}

TEST(unit, BlIpanTailNewSessionAndBothSides) {
  // new session: no previous state, so the fresh IBLPAN range stays
  // unextended because INITCOM zeros lie below the written range; only the
  // filled entries are visible.
  const std::vector<int> empty;
  std::vector<int> fresh = {1, 2, 3};
  preserveIpanTail(empty, fresh);
  CHECK_EQ(fresh.size(), 3u);
  // Both sides independent.
  const std::vector<int> topPrevious = {1, 2, 3, 4};
  const std::vector<int> bottomPrevious = {5, 6};
  std::vector<int> topFresh = {7};
  std::vector<int> bottomFresh = {8, 9, 10};
  preserveIpanTail(topPrevious, topFresh);
  preserveIpanTail(bottomPrevious, bottomFresh);
  CHECK_EQ(topFresh.size(), 4u);
  CHECK_EQ(topFresh[1], 2);
  CHECK_EQ(bottomFresh.size(), 3u);
  CHECK_EQ(bottomFresh[2], 10);
}

}  // namespace

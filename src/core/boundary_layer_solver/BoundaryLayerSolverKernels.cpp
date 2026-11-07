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

// File: src/core/boundary_layer_solver/BoundaryLayerSolverKernels.cpp
// Implementation of the BLVAR/BLMID kernels. Formula-
// faithful adoption from fortran-src/xblsys.f; constants, branch
// conditions and the order of the computations are unchanged.

#include "xfoil/core/boundary_layer/BoundaryLayerSolverKernels.h"

#include <algorithm>
#include <cmath>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"

namespace xfoil::core::boundary_layer {

namespace {

// Fortran: xblsys.f:871-895 CQ2 block (shared for all ITYP branches
// of BLVAR). Returns CQ2 and the derivatives wrt HS2/US2/HK2/RT2/H2.
struct CqBlock {
  double cq{0.0};
  double cqHs{0.0};
  double cqUs{0.0};
  double cqHk{0.0};
  double cqRt{0.0};
  double cqH2{0.0};
};

CqBlock computeCq(double hs2, double us2, double hk2, double h2,
                  double hkRt2, double hkRt2Rt, double hkRt2Hk,
                  const BlvarInput& in) {
  // Fortran: HKB = HK2 - 1.0
  // USB = 1.0 - US2
  const double hkb = hk2 - 1.0;
  const double usb = 1.0 - us2;
  CqBlock out{};
  // Fortran binds ** more strongly than *: the power sub-expressions are
  // first computed as their own intermediate values and then multiplied
  // (canonical Fortran evaluation, xblsys.f:873-883; proven by the
  // direct CQASSOC Fortran comparison):
  // HKC**2 = HKC*HKC ; HK2**2 = HK2*HK2 ; HK2**3 = (HK2*HK2)*HK2.
  const double hkcSq = hkRt2 * hkRt2;
  const double hk2Sq = hk2 * hk2;
  const double hk2Cube = hk2Sq * hk2;
  // Fortran: CQ2 =
 // & SQRT (CTCON*HS2*HKB*HKC**2 / (USB*H2*HK2**2))
 // = SQRT(((CTCON*HS2)*HKB)*HKC2 / ((USB*H2)*HK22))
  out.cq = std::sqrt(in.blpar.ctcon * hs2 * hkb * hkcSq /
                     (usb * h2 * hk2Sq));
  // Fortran: CQ2_HS2 = CTCON *HKB*HKC**2 / (USB*H2*HK2**2) * 0.5/CQ2
  // (no HS2 factor; the d/dHS2 derivation of SQRT(K*HS2))
  out.cqHs = in.blpar.ctcon * hkb * hkcSq / (usb * h2 * hk2Sq) *
             0.5 / out.cq;
  // Fortran: CQ2_US2 = CTCON*HS2*HKB*HKC**2 / (USB*H2*HK2**2) / USB * 0.5/CQ2
  out.cqUs = in.blpar.ctcon * hs2 * hkb * hkcSq /
             (usb * h2 * hk2Sq) / usb * 0.5 / out.cq;
  // Fortran: CQ2_HK2 = CTCON*HS2 *HKC**2 / (USB*H2*HK2**2) * 0.5/CQ2
  // & - CTCON*HS2*HKB*HKC**2 / (USB*H2*HK2**3) * 2.0 * 0.5/CQ2
  // & + CTCON*HS2*HKB*HKC / (USB*H2*HK2**2) * 2.0 * 0.5/CQ2
  // & *HKC_HK2
  out.cqHk = in.blpar.ctcon * hs2 * hkcSq / (usb * h2 * hk2Sq) *
                 0.5 / out.cq -
             in.blpar.ctcon * hs2 * hkb * hkcSq /
                 (usb * h2 * hk2Cube) * 2.0 * 0.5 / out.cq +
             in.blpar.ctcon * hs2 * hkb * hkRt2 / (usb * h2 * hk2Sq) * 2.0 *
                 0.5 / out.cq * hkRt2Hk;
  // Fortran: CQ2_RT2 = CTCON*HS2*HKB*HKC / (USB*H2*HK2**2) * 2.0 * 0.5/CQ2
  // & *HKC_RT2
  out.cqRt = in.blpar.ctcon * hs2 * hkb * hkRt2 / (usb * h2 * hk2Sq) * 2.0 *
             0.5 / out.cq * hkRt2Rt;
  // Fortran: CQ2_H2 =-CTCON*HS2*HKB*HKC**2 / (USB*H2*HK2**2) / H2 * 0.5/CQ2
  out.cqH2 = -in.blpar.ctcon * hs2 * hkb * hkcSq /
                 (usb * h2 * hk2Sq) / h2 * 0.5 / out.cq;
  return out;
}

}  // namespace

// Fortran: xblsys.f:784 BLVAR(ITYP)
BoundaryLayerTurbulentSecondary blvar(StationType ityp, const BlvarInput& in) {
  const BoundaryLayerPrimary& p = in.primary;
  const BoundaryLayerSecondary& s = in.secondary;
  BoundaryLayerTurbulentSecondary out{};
  const bool wake = ityp == StationType::kWake;
  const bool laminar = ityp == StationType::kLaminar;
  // Fortran: IF(ITYP.EQ.3) HK2 = MAX(HK2,1.00005)
  // IF(ITYP.NE.3) HK2 = MAX(HK2,1.05000)
  double hk2 = s.hk;
  if (wake) {
    hk2 = std::max(hk2, 1.00005);
  } else {
    hk2 = std::max(hk2, 1.05000);
  }
  // Fortran: HK2 is COMMON; the clamp affects all subsequent
  // COMMON readers (BLDIF, BLMID, MRCHUE/MRCHDU hanging equations).
  out.hk = hk2;
  // Note: the HK2 clamp changes HK2 itself; the HK2 sensitivities
  // are taken only after the clamp for the subsequent quantities
  // used (canonical Operations order).
  const double hk2U = s.hkUe;
  const double hk2T = s.hkT;
  const double hk2D = s.hkD;
  const double hk2Ms = s.hkMs;
 // Fortran: CALL HCT (HK2, M2, HC2, HC2_HK2, HC2_M2)
  // HC2_U2 = HC2_HK2*HK2_U2 + HC2_M2*M2_U2
  // HC2_T2 = HC2_HK2*HK2_T2
  // HC2_D2 = HC2_HK2*HK2_D2
  // HC2_MS = HC2_HK2*HK2_MS + HC2_M2*M2_MS
  const CorrelationValue hc = hct(hk2, s.m);
  out.hc = hc.value;
  out.hcU = hc.shapeDerivative * hk2U + hc.machSquaredDerivative * s.mUe;
  out.hcT = hc.shapeDerivative * hk2T;
  out.hcD = hc.shapeDerivative * hk2D;
  out.hcMs = hc.shapeDerivative * hk2Ms + hc.machSquaredDerivative * s.mMs;
  // Fortran: IF(ITYP.EQ.1) THEN
 // CALL HSL (HK2, RT2, M2, HS2, ...)
  // ELSE
 // CALL HST (HK2, RT2, M2, HS2, ...)
  // ENDIF
  const CorrelationValue hsOut =
      laminar ? hsl(hk2, s.rt) : hst(hk2, s.rt, s.m);
  out.hs = hsOut.value;
  // Fortran: HS2_U2 = HS2_HK2*HK2_U2 + HS2_RT2*RT2_U2 + HS2_M2*M2_U2
  // HS2_T2 = HS2_HK2*HK2_T2 + HS2_RT2*RT2_T2
  // HS2_D2 = HS2_HK2*HK2_D2
  // HS2_MS = HS2_HK2*HK2_MS + HS2_RT2*RT2_MS + HS2_M2*M2_MS
  // HS2_Re = HS2_RT2*RT2_Re
  out.hsU = hsOut.shapeDerivative * hk2U + hsOut.reynoldsDerivative * s.rtUe +
            hsOut.machSquaredDerivative * s.mUe;
  out.hsT = hsOut.shapeDerivative * hk2T + hsOut.reynoldsDerivative * s.rtT;
  out.hsD = hsOut.shapeDerivative * hk2D;
  out.hsMs = hsOut.shapeDerivative * hk2Ms +
             hsOut.reynoldsDerivative * s.rtMs +
             hsOut.machSquaredDerivative * s.mMs;
  out.hsRe = hsOut.reynoldsDerivative * s.rtRe;
 // Fortran: US2 = 0.5*HS2* (1.0 - (HK2-1.0)/(GBCON*H2))
 // US2_HS2 = 0.5 * (1.0 - (HK2-1.0)/(GBCON*H2))
 // US2_HK2 = 0.5*HS2*(- 1.0 /(GBCON*H2))
  // US2_H2 = 0.5*HS2* (HK2-1.0)/(GBCON*H2**2)
  // (Fortran ** stronger as *: H2**2 first computed)
  const double us2Hs = 0.5 * (1.0 - (hk2 - 1.0) / (in.blpar.gbcon * s.h));
  const double us2Hk = 0.5 * out.hs * (-1.0 / (in.blpar.gbcon * s.h));
  const double us2H2 =
      0.5 * out.hs * (hk2 - 1.0) / (in.blpar.gbcon * (s.h * s.h));
  out.us = 0.5 * out.hs * (1.0 - (hk2 - 1.0) / (in.blpar.gbcon * s.h));
  // Fortran: US2_U2 = US2_HS2*HS2_U2 + US2_HK2*HK2_U2
  // US2_T2 = US2_HS2*HS2_T2 + US2_HK2*HK2_T2 + US2_H2*H2_T2
  // US2_D2 = US2_HS2*HS2_D2 + US2_HK2*HK2_D2 + US2_H2*H2_D2
  // US2_MS = US2_HS2*HS2_MS + US2_HK2*HK2_MS
  // US2_Re = US2_HS2*HS2_Re
  double us2U = us2Hs * out.hsU + us2Hk * hk2U;
  double us2T = us2Hs * out.hsT + us2Hk * hk2T + us2H2 * s.hT;
  double us2D = us2Hs * out.hsD + us2Hk * hk2D + us2H2 * s.hD;
  double us2Ms = us2Hs * out.hsMs + us2Hk * hk2Ms;
  double us2Re = us2Hs * out.hsRe;
  // Fortran: IF(ITYP.LE.2 .AND. US2.GT.0.95) THEN
  // US2 = 0.98 ; US2_U2 = 0. ; US2_T2 = 0. ; US2_D2 = 0.
  // US2_MS = 0.; US2_Re = 0.
  // ENDIF
  if (!wake && out.us > 0.95) {
    out.us = 0.98;
    us2U = 0.0;
    us2T = 0.0;
    us2D = 0.0;
    us2Ms = 0.0;
    us2Re = 0.0;
  }
  // Fortran: IF(ITYP.EQ.3 .AND. US2.GT.0.99995) THEN
  // US2 = 0.99995 ; US2_* = 0.
  // ENDIF
  if (wake && out.us > 0.99995) {
    out.us = 0.99995;
    us2U = 0.0;
    us2T = 0.0;
    us2D = 0.0;
    us2Ms = 0.0;
    us2Re = 0.0;
  }
  out.usU = us2U;
  out.usT = us2T;
  out.usD = us2D;
  out.usMs = us2Ms;
  out.usRe = us2Re;
  // Fortran: GCC = 0.0
  // HKC = HK2 - 1.0 ; HKC_HK2 = 1.0 ; HKC_RT2 = 0.0
  // IF(ITYP.EQ.2) THEN
  // GCC = GCCON
  // HKC = HK2 - 1.0 - GCC/RT2
  // HKC_HK2 = 1.0
  // HKC_RT2 = GCC/RT2**2
  // IF(HKC .LT. 0.01) THEN
  // HKC = 0.01 ; HKC_HK2 = 0.0 ; HKC_RT2 = 0.0
  // ENDIF
  // ENDIF
  double hkRt2 = hk2 - 1.0;
  double hkRt2Rt = 0.0;
  double hkRt2Hk = 1.0;  // Fortran HKC_HK2 (not-turbulent: 1.0)
  if (ityp == StationType::kTurbulent) {
    hkRt2 = hk2 - 1.0 - in.blpar.gccon / s.rt;
    hkRt2Rt = in.blpar.gccon / (s.rt * s.rt);
    hkRt2Hk = 1.0;
    if (hkRt2 < 0.01) {
      hkRt2 = 0.01;
      hkRt2Rt = 0.0;
      hkRt2Hk = 0.0;
    }
  }
  const CqBlock cq =
      computeCq(out.hs, out.us, hk2, s.h, hkRt2, hkRt2Rt, hkRt2Hk, in);
  out.cq = cq.cq;
  // Fortran: CQ2_U2 = CQ2_HS2*HS2_U2 + CQ2_US2*US2_U2 + CQ2_HK2*HK2_U2
  // CQ2_T2 = CQ2_HS2*HS2_T2 + CQ2_US2*US2_T2 + CQ2_HK2*HK2_T2
  // CQ2_D2 = CQ2_HS2*HS2_D2 + CQ2_US2*US2_D2 + CQ2_HK2*HK2_D2
  // CQ2_MS = CQ2_HS2*HS2_MS + CQ2_US2*US2_MS + CQ2_HK2*HK2_MS
  // CQ2_Re = CQ2_HS2*HS2_Re + CQ2_US2*US2_Re
  // CQ2_U2 = CQ2_U2 + CQ2_RT2*RT2_U2
  // CQ2_T2 = CQ2_T2 + CQ2_H2*H2_T2 + CQ2_RT2*RT2_T2
  // CQ2_D2 = CQ2_D2 + CQ2_H2*H2_D2
  // CQ2_MS = CQ2_MS + CQ2_RT2*RT2_MS
  // CQ2_Re = CQ2_Re + CQ2_RT2*RT2_Re
  out.cqU = cq.cqHs * out.hsU + cq.cqUs * out.usU + cq.cqHk * hk2U +
            cq.cqRt * s.rtUe;
  out.cqT = cq.cqHs * out.hsT + cq.cqUs * out.usT + cq.cqHk * hk2T +
            cq.cqH2 * s.hT + cq.cqRt * s.rtT;
  out.cqD = cq.cqHs * out.hsD + cq.cqUs * out.usD + cq.cqHk * hk2D +
            cq.cqH2 * s.hD;
  out.cqMs = cq.cqHs * out.hsMs + cq.cqUs * out.usMs + cq.cqHk * hk2Ms +
             cq.cqRt * s.rtMs;
  out.cqRe = cq.cqHs * out.hsRe + cq.cqUs * out.usRe + cq.cqRt * s.rtRe;
  // Fortran: skin-friction coefficient CF2
  if (wake) {
    // Fortran: CF2 = 0. ; CF2_HK2 = 0. ; CF2_RT2 = 0. ; CF2_M2 = 0.
    out.cf = 0.0;
  } else if (laminar) {
 // Fortran: CALL CFL (HK2, RT2, M2, CF2, CF2_HK2, CF2_RT2, CF2_M2)
    // CF2_MS = CF2_HK2*HK2_MS + CF2_RT2*RT2_MS + CF2_M2*M2_MS
    // (CFL: CF_MSQ = 0, also CF2_M2 = 0; xblsys.f:926/2368)
    const CorrelationValue cf = cfl(hk2, s.rt);
    out.cf = cf.value;
    out.cfU = cf.shapeDerivative * hk2U + cf.reynoldsDerivative * s.rtUe;
    out.cfT = cf.shapeDerivative * hk2T + cf.reynoldsDerivative * s.rtT;
    out.cfD = cf.shapeDerivative * hk2D;
    out.cfMs =
        cf.shapeDerivative * hk2Ms + cf.reynoldsDerivative * s.rtMs;
    out.cfRe = cf.reynoldsDerivative * s.rtRe;
  } else {
 // Fortran: CALL CFT (HK2, RT2, M2, CF2, ...)
 // CALL CFL (HK2, RT2, M2, CF2L, ...)
    // IF(CF2L.GT.CF2) THEN
    // C------- laminar Cf is greater than turbulent Cf -- use laminar
    // CF2 = CF2L ; CF2_HK2 = CF2L_HK2
    // CF2_RT2 = CF2L_RT2 ; CF2_M2 = CF2L_M2
    // ENDIF
    const double cffac = in.blpar.cffac;
    const CorrelationValue cfTurb = cft(hk2, s.rt, s.m, cffac);
    const CorrelationValue cfLam = cfl(hk2, s.rt);
    const CorrelationValue& cf = (cfLam.value > cfTurb.value) ? cfLam : cfTurb;
    // Fortran: CF2_MS = CF2_HK2*HK2_MS + CF2_RT2*RT2_MS + CF2_M2*M2_MS
    // (xblsys.f:926)
    out.cf = cf.value;
    out.cfU = cf.shapeDerivative * hk2U + cf.reynoldsDerivative * s.rtUe +
              cf.machSquaredDerivative * s.mUe;
    out.cfT = cf.shapeDerivative * hk2T + cf.reynoldsDerivative * s.rtT;
    out.cfD = cf.shapeDerivative * hk2D;
    out.cfMs = cf.shapeDerivative * hk2Ms + cf.reynoldsDerivative * s.rtMs +
               cf.machSquaredDerivative * s.mMs;
    out.cfRe = cf.reynoldsDerivative * s.rtRe;
  }
  // Fortran: Dissipation function 2 CD / H*
  if (laminar) {
 // Fortran: CALL DIL (HK2, RT2, DI2, DI2_HK2, DI2_RT2)
    // DI2_U2 = DI2_HK2*HK2_U2 + DI2_RT2*RT2_U2
    // DI2_T2 = DI2_HK2*HK2_T2 + DI2_RT2*RT2_T2
    // DI2_D2 = DI2_HK2*HK2_D2
    // DI2_S2 = 0.
    // DI2_MS = DI2_HK2*HK2_MS + DI2_RT2*RT2_MS
    // DI2_Re = DI2_RT2*RT2_Re
    const CorrelationValue dilOut = dil(hk2, s.rt);
    out.di = dilOut.value;
    out.diU = dilOut.shapeDerivative * hk2U +
              dilOut.reynoldsDerivative * s.rtUe;
    out.diT = dilOut.shapeDerivative * hk2T +
              dilOut.reynoldsDerivative * s.rtT;
    out.diD = dilOut.shapeDerivative * hk2D;
    out.diS = 0.0;
    out.diMs = dilOut.shapeDerivative * hk2Ms +
               dilOut.reynoldsDerivative * s.rtMs;
    out.diRe = dilOut.reynoldsDerivative * s.rtRe;
  } else if (ityp == StationType::kTurbulent) {
    // Fortran: turbulent wall dissipation
    // CALL CFT(HK2, RT2, M2, CF2T, CF2T_HK2, CF2T_RT2, CF2T_M2)
 // DI2 = (0.5*CF2T*US2) * 2.0/HS2
 // DI2_HS2 = - (0.5*CF2T*US2) * 2.0/HS2**2
 // DI2_US2 = (0.5*CF2T) * 2.0/HS2
 // DI2_CF2T = (0.5 *US2) * 2.0/HS2
    // DI2_S2 = 0.0
    // DI2_U2 = DI2_HS2*HS2_U2 + DI2_US2*US2_U2 + DI2_CF2T*CF2T_U2
    // DI2_T2 = DI2_HS2*HS2_T2 + DI2_US2*US2_T2 + DI2_CF2T*CF2T_T2
    // DI2_D2 = DI2_HS2*HS2_D2 + DI2_US2*US2_D2 + DI2_CF2T*CF2T_D2
    // DI2_MS = DI2_HS2*HS2_MS + DI2_US2*US2_MS + DI2_CF2T*CF2T_MS
    // DI2_Re = DI2_HS2*HS2_Re + DI2_US2*US2_Re + DI2_CF2T*CF2T_Re
    const double cffac = in.blpar.cffac;
    const CorrelationValue cf2t = cft(hk2, s.rt, s.m, cffac);
    const double cf2tU = cf2t.shapeDerivative * hk2U +
                         cf2t.reynoldsDerivative * s.rtUe +
                         cf2t.machSquaredDerivative * s.mUe;
    const double cf2tT =
        cf2t.shapeDerivative * hk2T + cf2t.reynoldsDerivative * s.rtT;
    const double cf2tD = cf2t.shapeDerivative * hk2D;
    // Fortran: CF2T_MS = CF2T_HK2*HK2_MS + CF2T_RT2*RT2_MS
    // + CF2T_M2*M2_MS (xblsys.f:952)
    const double cf2tMs = cf2t.shapeDerivative * hk2Ms +
                          cf2t.reynoldsDerivative * s.rtMs +
                          cf2t.machSquaredDerivative * s.mMs;
    const double cf2tRe = cf2t.reynoldsDerivative * s.rtRe;
    const double di2Hs = -(0.5 * cf2t.value * out.us) * 2.0 / (out.hs * out.hs);
    const double di2Us = (0.5 * cf2t.value) * 2.0 / out.hs;
    const double di2Cf = (0.5 * out.us) * 2.0 / out.hs;
    out.di = (0.5 * cf2t.value * out.us) * 2.0 / out.hs;
    out.diS = 0.0;
    out.diU = di2Hs * out.hsU + di2Us * out.usU + di2Cf * cf2tU;
    out.diT = di2Hs * out.hsT + di2Us * out.usT + di2Cf * cf2tT;
    out.diD = di2Hs * out.hsD + di2Us * out.usD + di2Cf * cf2tD;
    out.diMs = di2Hs * out.hsMs + di2Us * out.usMs + di2Cf * cf2tMs;
    out.diRe = di2Hs * out.hsRe + di2Us * out.usRe + di2Cf * cf2tRe;
    // Fortran: set minimum Hk for wake layer to still exist
    // GRT = LOG(RT2) ; HMIN = 1.0 + 2.1/GRT
    // HM_RT2 = -(2.1/GRT**2) / RT2
    const double grt = std::log(s.rt);
    const double hmin = 1.0 + 2.1 / grt;
    const double hmRt = -(2.1 / (grt * grt)) / s.rt;
    // Fortran: FL = (HK2-1.0)/(HMIN-1.0)
    // FL_HK2 = 1.0/(HMIN-1.0)
 // FL_RT2 = (-FL/(HMIN-1.0)) * HM_RT2
    const double fl = (hk2 - 1.0) / (hmin - 1.0);
    const double flHk = 1.0 / (hmin - 1.0);
    const double flRt = (-fl / (hmin - 1.0)) * hmRt;
    // Fortran: TFL = TANH(FL)
    // DFAC = 0.5 + 0.5* TFL
    // DF_FL = 0.5*(1.0 - TFL**2)
    // DF_HK2 = DF_FL*FL_HK2 ; DF_RT2 = DF_FL*FL_RT2
    const double tfl = std::tanh(fl);
    const double dfac = 0.5 + 0.5 * tfl;
    const double dfFl = 0.5 * (1.0 - tfl * tfl);
    const double dfHk = dfFl * flHk;
    const double dfRt = dfFl * flRt;
    // Fortran: DI2_S2 = DI2_S2*DFAC
    // DI2_U2 = DI2_U2*DFAC + DI2*(DF_HK2*HK2_U2 + DF_RT2*RT2_U2)
    // DI2_T2 = DI2_T2*DFAC + DI2*(DF_HK2*HK2_T2 + DF_RT2*RT2_T2)
 // DI2_D2 = DI2_D2*DFAC + DI2*(DF_HK2*HK2_D2)
    // DI2_MS = DI2_MS*DFAC + DI2*(DF_HK2*HK2_MS + DF_RT2*RT2_MS)
 // DI2_Re = DI2_Re*DFAC + DI2* (DF_RT2*RT2_Re)
    // DI2 = DI2 *DFAC
    out.diS = out.diS * dfac;
    out.diU = out.diU * dfac + out.di * (dfHk * hk2U + dfRt * s.rtUe);
    out.diT = out.diT * dfac + out.di * (dfHk * hk2T + dfRt * s.rtT);
    out.diD = out.diD * dfac + out.di * (dfHk * hk2D);
    out.diMs = out.diMs * dfac + out.di * (dfHk * hk2Ms + dfRt * s.rtMs);
    out.diRe = out.diRe * dfac + out.di * (dfRt * s.rtRe);
    out.di = out.di * dfac;
  } else {
    // Fortran: zero wall contribution for wake
    // DI2 = 0. ; DI2_S2 = 0. ; DI2_U2 = 0. ; DI2_T2 = 0.
    // DI2_D2 = 0.; DI2_MS = 0.; DI2_Re = 0.
    out.di = 0.0;
    out.diS = 0.0;
    out.diU = 0.0;
    out.diT = 0.0;
    out.diD = 0.0;
    out.diMs = 0.0;
    out.diRe = 0.0;
  }
  // Fortran: Add on turbulent outer layer contribution
  // IF(ITYP.NE.1) THEN
  if (!laminar) {
    // Fortran: DD = S2**2 * (0.995-US2) * 2.0/HS2
    // DD_HS2 = -S2**2 * (0.995-US2) * 2.0/HS2**2
    // DD_US2 = -S2**2 * 2.0/HS2
    // DD_S2 = S2*2.0* (0.995-US2) * 2.0/HS2
    const double dd = p.s * p.s * (0.995 - out.us) * 2.0 / out.hs;
    const double ddHs = -p.s * p.s * (0.995 - out.us) * 2.0 / (out.hs * out.hs);
    const double ddUs = -p.s * p.s * 2.0 / out.hs;
    const double ddS = p.s * 2.0 * (0.995 - out.us) * 2.0 / out.hs;
    // Fortran: DI2 = DI2 + DD
    // DI2_S2 = DD_S2
    // DI2_U2 = DI2_U2 + DD_HS2*HS2_U2 + DD_US2*US2_U2
    // DI2_T2 = DI2_T2 + DD_HS2*HS2_T2 + DD_US2*US2_T2
    // DI2_D2 = DI2_D2 + DD_HS2*HS2_D2 + DD_US2*US2_D2
    // DI2_MS = DI2_MS + DD_HS2*HS2_MS + DD_US2*US2_MS
    // DI2_Re = DI2_Re + DD_HS2*HS2_Re + DD_US2*US2_Re
    out.di = out.di + dd;
    out.diS = ddS;
    out.diU = out.diU + ddHs * out.hsU + ddUs * out.usU;
    out.diT = out.diT + ddHs * out.hsT + ddUs * out.usT;
    out.diD = out.diD + ddHs * out.hsD + ddUs * out.usD;
    out.diMs = out.diMs + ddHs * out.hsMs + ddUs * out.usMs;
    out.diRe = out.diRe + ddHs * out.hsRe + ddUs * out.usRe;
    // Fortran: add laminar stress contribution to outer layer CD
    // DD = 0.15*(0.995-US2)**2 / RT2 * 2.0/HS2
    // DD_US2 = -0.15*(0.995-US2)*2. / RT2 * 2.0/HS2
    // DD_HS2 = -DD/HS2 ; DD_RT2 = -DD/RT2
    // (Fortran ** stronger as *: (0.995-US2)**2 first computed)
    const double usComp = (0.995 - out.us) * (0.995 - out.us);
    const double dd2 = 0.15 * usComp / s.rt * 2.0 / out.hs;
    const double dd2Us = -0.15 * (0.995 - out.us) * 2.0 / s.rt * 2.0 / out.hs;
    const double dd2Hs = -dd2 / out.hs;
    const double dd2Rt = -dd2 / s.rt;
    // Fortran: DI2 = DI2 + DD
    // DI2_U2 = DI2_U2 + DD_HS2*HS2_U2 + DD_US2*US2_U2 + DD_RT2*RT2_U2
    // DI2_T2 = DI2_T2 + DD_HS2*HS2_T2 + DD_US2*US2_T2 + DD_RT2*RT2_T2
    // DI2_D2 = DI2_D2 + DD_HS2*HS2_D2 + DD_US2*US2_D2
    // DI2_MS = DI2_MS + DD_HS2*HS2_MS + DD_US2*US2_MS + DD_RT2*RT2_MS
    // DI2_Re = DI2_Re + DD_HS2*HS2_Re + DD_US2*US2_Re + DD_RT2*RT2_Re
    out.di = out.di + dd2;
    out.diU = out.diU + dd2Hs * out.hsU + dd2Us * out.usU + dd2Rt * s.rtUe;
    out.diT = out.diT + dd2Hs * out.hsT + dd2Us * out.usT + dd2Rt * s.rtT;
    out.diD = out.diD + dd2Hs * out.hsD + dd2Us * out.usD;
    out.diMs = out.diMs + dd2Hs * out.hsMs + dd2Us * out.usMs +
               dd2Rt * s.rtMs;
    out.diRe = out.diRe + dd2Hs * out.hsRe + dd2Us * out.usRe +
               dd2Rt * s.rtRe;
    // Fortran: IF(ITYP.EQ.2) THEN
 // CALL DIL (HK2, RT2, DI2L, DI2L_HK2, DI2L_RT2)
    // IF(DI2L.GT.DI2) THEN
    // DI2 = DI2L ; DI2_S2 = 0.
    // DI2_U2 = DI2L_HK2*HK2_U2 + DI2L_RT2*RT2_U2
    // DI2_T2 = DI2L_HK2*HK2_T2 + DI2L_RT2*RT2_T2
    // DI2_D2 = DI2L_HK2*HK2_D2
    // DI2_MS = DI2L_HK2*HK2_MS + DI2L_RT2*RT2_MS
    // DI2_Re = DI2L_RT2*RT2_Re
    // ENDIF
    // ENDIF
    if (ityp == StationType::kTurbulent) {
      const CorrelationValue dilOut = dil(hk2, s.rt);
      if (dilOut.value > out.di) {
        out.di = dilOut.value;
        out.diS = 0.0;
        out.diU = dilOut.shapeDerivative * hk2U +
                  dilOut.reynoldsDerivative * s.rtUe;
        out.diT = dilOut.shapeDerivative * hk2T +
                  dilOut.reynoldsDerivative * s.rtT;
        out.diD = dilOut.shapeDerivative * hk2D;
        out.diMs = dilOut.shapeDerivative * hk2Ms +
                   dilOut.reynoldsDerivative * s.rtMs;
        out.diRe = dilOut.reynoldsDerivative * s.rtRe;
      }
    }
  }
  // Fortran: IF(ITYP.EQ.3) THEN
  // C------ laminar wake CD
 // CALL DILW (HK2, RT2, DI2L, DI2L_HK2, DI2L_RT2)
  // IF(DI2L .GT. DI2) THEN
  // DI2 = DI2L ; DI2_S2 = 0.
  // DI2_U2 = DI2L_HK2*HK2_U2 + DI2L_RT2*RT2_U2
  // DI2_T2 = DI2L_HK2*HK2_T2 + DI2L_RT2*RT2_T2
  // DI2_D2 = DI2L_HK2*HK2_D2
  // DI2_MS = DI2L_HK2*HK2_MS + DI2L_RT2*RT2_MS
  // DI2_Re = DI2L_RT2*RT2_Re
  // ENDIF
  // ENDIF
  if (wake) {
    const CorrelationValue dilwOut = dilw(hk2, s.rt);
    if (dilwOut.value > out.di) {
      out.di = dilwOut.value;
      out.diS = 0.0;
      out.diU = dilwOut.shapeDerivative * hk2U +
                dilwOut.reynoldsDerivative * s.rtUe;
      out.diT = dilwOut.shapeDerivative * hk2T +
                dilwOut.reynoldsDerivative * s.rtT;
      out.diD = dilwOut.shapeDerivative * hk2D;
      out.diMs = dilwOut.shapeDerivative * hk2Ms +
                 dilwOut.reynoldsDerivative * s.rtMs;
      out.diRe = dilwOut.reynoldsDerivative * s.rtRe;
    }
  }
  // Fortran: IF(ITYP.EQ.3) THEN
  // C----- double dissipation for the wake (two wake halves)
  // DI2 = DI2*2.0 ; DI2_S2 = DI2_S2*2.0 ; DI2_U2 = DI2_U2*2.0
  // DI2_T2 = DI2_T2*2.0 ; DI2_D2 = DI2_D2*2.0
  // DI2_MS = DI2_MS*2.0; DI2_Re = DI2_Re*2.0
  // ENDIF
  if (wake) {
    out.di = out.di * 2.0;
    out.diS = out.diS * 2.0;
    out.diU = out.diU * 2.0;
    out.diT = out.diT * 2.0;
    out.diD = out.diD * 2.0;
    out.diMs = out.diMs * 2.0;
    out.diRe = out.diRe * 2.0;
  }
 // Fortran: DE2 = (3.15 + 1.72/(HK2-1.0))*T2 + D2
 // DE2_HK2 = (- 1.72/(HK2-1.0)**2)*T2
  const double de2Hk = -1.72 / ((hk2 - 1.0) * (hk2 - 1.0)) * p.t;
  out.de = (3.15 + 1.72 / (hk2 - 1.0)) * p.t + p.d;
  // Fortran: DE2_U2 = DE2_HK2*HK2_U2
  // DE2_T2 = DE2_HK2*HK2_T2 + (3.15 + 1.72/(HK2-1.0))
  // DE2_D2 = DE2_HK2*HK2_D2 + 1.0
  // DE2_MS = DE2_HK2*HK2_MS
  out.deU = de2Hk * hk2U;
  out.deT = de2Hk * hk2T + (3.15 + 1.72 / (hk2 - 1.0));
  out.deD = de2Hk * hk2D + 1.0;
  out.deMs = de2Hk * hk2Ms;
  // Fortran: HDMAX = 12.0
  // IF(DE2 .GT. HDMAX*T2) THEN
  // DE2 = HDMAX*T2 ; DE2_U2 = 0.0 ; DE2_T2 = HDMAX
  // DE2_D2 = 0.0 ; DE2_MS = 0.0
  // ENDIF
  const double hdmax = 12.0;
  if (out.de > hdmax * p.t) {
    out.de = hdmax * p.t;
    out.deU = 0.0;
    out.deT = hdmax;
    out.deD = 0.0;
    out.deMs = 0.0;
  }
  return out;
}

// Fortran: xblsys.f:1124 BLMID(ITYP)
MidpointSkinFriction blmid(StationType ityp, const BlmidInput& in) {
  BlmidStation first = in.first;
  const BlmidStation& second = in.second;
  // Fortran: set similarity variables if not defined
  // IF(SIMI) THEN
  // HK1 = HK2 ; HK1_T1 = HK2_T2 ; HK1_D1 = HK2_D2
  // HK1_U1 = HK2_U2 ; HK1_MS = HK2_MS
  // RT1 = RT2 ; RT1_T1 = RT2_T1 ; RT1_U1 = RT2_U2
  // RT1_MS = RT2_MS; RT1_Re = RT2_Re
  // M1 = M2 ; M1_U1 = M2_U2 ; M1_MS = M2_MS
  // ENDIF
  if (in.simi) {
    first.hk = second.hk;
    first.hkT = second.hkT;
    first.hkD = second.hkD;
    first.hkU = second.hkU;
    first.hkMs = second.hkMs;
    first.rt = second.rt;
    first.rtT = second.rtT;
    first.rtU = second.rtU;
    first.rtMs = second.rtMs;
    first.rtRe = second.rtRe;
    first.m = second.m;
    first.mU = second.mU;
    first.mMs = second.mMs;
  }
  MidpointSkinFriction out{};
  // Fortran: HKA = 0.5*(HK1 + HK2)
  // RTA = 0.5*(RT1 + RT2)
 // MA = 0.5*(M1 + M2)
  const double hka = 0.5 * (first.hk + second.hk);
  const double rta = 0.5 * (first.rt + second.rt);
  const double ma = 0.5 * (first.m + second.m);
  // Fortran: midpoint skin friction coefficient (zero in wake)
  CorrelationValue cfOut{};
  if (ityp == StationType::kWake) {
    // Fortran: CFM = 0. ; CFM_HKA = 0. ; CFM_RTA = 0. ; CFM_MA = 0.
    // CFM_MS = 0.
    out.cf = 0.0;
    return out;
  }
  if (ityp == StationType::kLaminar) {
 // Fortran: CALL CFL (HKA, RTA, MA, CFM, CFM_HKA, CFM_RTA, CFM_MA)
    cfOut = cfl(hka, rta);
  } else {
 // Fortran: CALL CFT (HKA, RTA, MA, CFM, ...)
 // CALL CFL (HKA, RTA, MA, CFML, ...)
    // IF(CFML.GT.CFM) THEN
    // CFM = CFML ; CFM_HKA = CFML_HKA
    // CFM_RTA = CFML_RTA ; CFM_MA = CFML_MA
    // ENDIF
    const double cffac = in.blpar.cffac;
    const CorrelationValue cfTurb = cft(hka, rta, ma, cffac);
    const CorrelationValue cfLam = cfl(hka, rta);
    cfOut = (cfLam.value > cfTurb.value) ? cfLam : cfTurb;
  }
  // Fortran: CFM_U1 = 0.5*(CFM_HKA*HK1_U1 + CFM_MA*M1_U1 + CFM_RTA*RT1_U1)
  // CFM_T1 = 0.5*(CFM_HKA*HK1_T1 + CFM_RTA*RT1_T1)
 // CFM_D1 = 0.5*(CFM_HKA*HK1_D1)
  // CFM_U2 = 0.5*(CFM_HKA*HK2_U2 + CFM_MA*M2_U2 + CFM_RTA*RT2_U2)
  // CFM_T2 = 0.5*(CFM_HKA*HK2_T2 + CFM_RTA*RT2_T2)
 // CFM_D2 = 0.5*(CFM_HKA*HK2_D2)
  // CFM_MS = 0.5*(CFM_HKA*HK1_MS + CFM_MA*M1_MS + CFM_RTA*RT1_MS
  // & + CFM_HKA*HK2_MS + CFM_MA*M2_MS + CFM_RTA*RT2_MS)
 // CFM_Re = 0.5* (CFM_RTA*RT1_Re
  // & + CFM_RTA*RT2_Re)
  const double cfHk = cfOut.shapeDerivative;
  const double cfRt = cfOut.reynoldsDerivative;
  const double cfM = cfOut.machSquaredDerivative;
  out.cf = cfOut.value;
  out.cfU1 = 0.5 * (cfHk * first.hkU + cfM * first.mU + cfRt * first.rtU);
  out.cfT1 = 0.5 * (cfHk * first.hkT + cfRt * first.rtT);
  out.cfD1 = 0.5 * (cfHk * first.hkD);
  out.cfU2 = 0.5 * (cfHk * second.hkU + cfM * second.mU + cfRt * second.rtU);
  out.cfT2 = 0.5 * (cfHk * second.hkT + cfRt * second.rtT);
  out.cfD2 = 0.5 * (cfHk * second.hkD);
  out.cfMs = 0.5 * (cfHk * first.hkMs + cfM * first.mMs + cfRt * first.rtMs +
                    cfHk * second.hkMs + cfM * second.mMs +
                    cfRt * second.rtMs);
  out.cfRe = 0.5 * (cfRt * first.rtRe + cfRt * second.rtRe);
  return out;
}

namespace {

// BLKIN properties for the interpolated point in TRCHEK2
// (canonical SETBL data: HSTINV/GM1BL/RSTBL/HVRAT/REYBL).
BoundaryLayerProperties trchekProps(const TrchekInput& in) {
  BoundaryLayerProperties props{};
  props.gm1bl = in.gm1bl;
  props.rstbl = in.rstbl;
  props.rstblMs = in.rstblMs;
  props.hstinv = in.hstinv;
  props.hstinvMs = in.hstinvMs;
  props.hvrat = in.hvrat;
  props.reybl = in.reybl;
  props.reyblMs = in.reyblMs;
  props.reyblRe = in.reyblRe;
  return props;
}

// BLKIN at an interpolated point (X/T/D/U from the interpolation).
BoundaryLayerSecondary blkinAt(double x, double t, double d, double ue,
                               const BoundaryLayerProperties& props) {
  const BoundaryLayerPrimary interp{x, 0.0, 0.0, t, d, 0.0, ue, 0.0, 0.0};
  return blkin(interp, props);
}

}  // namespace

// Fortran: xblsys.f:231 TRCHEK2
TrchekResult trchek2(const TrchekInput& in) {
  // Fortran: DATA DAEPS / 5.0E-5 /
  const double daeps = 5.0e-5;
  const TrchekStation& s1 = in.first;
  const TrchekStation& s2 = in.second;
  const BoundaryLayerProperties props = trchekProps(in);
  TrchekResult out{};
  // Fortran: save variables and sensitivities at IBL ("2") for future
  // restoration — in C++ the routine works on local copies (no
  // in-place mutation).
  const double x1 = s1.x;
  const double t1 = s1.t;
  const double d1 = s1.d;
  const double u1 = s1.ue;
  const double x2 = s2.x;
  const double t2 = s2.t;
  const double d2 = s2.d;
  const double u2 = s2.ue;
  double ampl1 = s1.ampl;
  double ampl2 = s2.ampl;
  // Fortran: calculate average amplification rate AX over X1..X2 interval
  const AverageAmplificationValue ax0 =
      axset(s1.hk, t1, s1.rt, ampl1, s2.hk, s2.t, s2.rt, ampl2, in.acrit,
            in.idampv);
  // Fortran: set initial guess for iterate N2 (AMPL2) at X2
  ampl2 = ampl1 + ax0.value * (x2 - x1);
  // Fortran: solve implicit system for amplification AMPL2 (30 Iterations)
  bool converged = false;
  bool punchedOut = false;
  // WF state of the last iteration (for the sensitivity block).
  double wf1 = 0.0;
  double wf2 = 0.0;
  double wf1A1 = 0.0;
  double wf2A1 = 0.0;
  double wf1A2 = 0.0;
  double wf2A2 = 0.0;
  double wf1X1 = 0.0;
  double wf2X1 = 0.0;
  double wf1X2 = 0.0;
  double wf2X2 = 0.0;
  double wf1Xf = 0.0;
  double wf2Xf = 0.0;
  double amplt = 0.0;
  double ampltA2 = 0.0;
  // Interpolated "T" variables of the last iteration (for the
  // sensitivity block).
  double hkt = 0.0;
  double hktTt = 0.0;
  double hktDt = 0.0;
  double hktUt = 0.0;
  double hktMs = 0.0;
  double rtt = 0.0;
  double rttTt = 0.0;
  double rttUt = 0.0;
  double rttMs = 0.0;
  double rttRe = 0.0;
  // Second AXSET of the last iteration (X1..XT).
  AverageAmplificationValue axLast{};
  // Fortran label 100 writes the values of the last iteration
  // (XT, AMPLT, AX, second amplification); for the structured TRCHEK2 error record.
  double lastXt = 0.0;
  double lastAmplt = 0.0;
  double lastAx = 0.0;
  double lastDa2 = 0.0;
  for (int itam = 1; itam <= 30; ++itam) {
    // Fortran: define weighting factors WF1,WF2 for defining "T" quantities
    double sfa = 0.0;
    double sfaA1 = 0.0;
    double sfaA2 = 0.0;
    if (ampl2 <= in.acrit) {
      // Fortran: there is no transition yet, "T" is the same as "2"
      amplt = ampl2;
      ampltA2 = 1.0;
      sfa = 1.0;
      sfaA1 = 0.0;
      sfaA2 = 0.0;
    } else {
      // Fortran: a transition occurs in X1..X2; "T" is set from N1, N2
      amplt = in.acrit;
      ampltA2 = 0.0;
      sfa = (amplt - ampl1) / (ampl2 - ampl1);
      sfaA1 = (sfa - 1.0) / (ampl2 - ampl1);
      sfaA2 = -sfa / (ampl2 - ampl1);
    }
    double sfx = 0.0;
    double sfxX1 = 0.0;
    double sfxX2 = 0.0;
    double sfxXf = 0.0;
    if (in.xiforc < x2) {
      sfx = (in.xiforc - x1) / (x2 - x1);
      sfxX1 = (sfx - 1.0) / (x2 - x1);
      sfxX2 = -sfx / (x2 - x1);
      sfxXf = 1.0 / (x2 - x1);
    } else {
      sfx = 1.0;
      sfxX1 = 0.0;
      sfxX2 = 0.0;
      sfxXf = 0.0;
    }
    // Fortran: set weighting factor from free or forced transition
    if (sfa < sfx) {
      wf2 = sfa;
      wf2A1 = sfaA1;
      wf2A2 = sfaA2;
      wf2X1 = 0.0;
      wf2X2 = 0.0;
      wf2Xf = 0.0;
    } else {
      wf2 = sfx;
      wf2A1 = 0.0;
      wf2A2 = 0.0;
      wf2X1 = sfxX1;
      wf2X2 = sfxX2;
      wf2Xf = sfxXf;
    }
    // Fortran: WF1 = 1.0 - WF2
    // WF1_A1 = - WF2_A1
    // WF1_A2 = - WF2_A2
    // WF1_X1 = - WF2_X1
    // WF1_X2 = - WF2_X2
    // WF1_XF = - WF2_XF
    wf1 = 1.0 - wf2;
    wf1A1 = -wf2A1;
    wf1A2 = -wf2A2;
    wf1X1 = -wf2X1;
    wf1X2 = -wf2X2;
    wf1Xf = -wf2Xf;
    // Fortran: interpolate BL variables to XT
    // XT = X1*WF1 + X2*WF2 ; TT = T1*WF1 + T2*WF2
    // DT = D1*WF1 + D2*WF2 ; UT = U1*WF1 + U2*WF2
    const double xt = x1 * wf1 + x2 * wf2;
    const double tt = t1 * wf1 + t2 * wf2;
    const double dt = d1 * wf1 + d2 * wf2;
    const double ut = u1 * wf1 + u2 * wf2;
    // Fortran: XT_A2 = X1*WF1_A2 + X2*WF2_A2
    // TT_A2 = T1*WF1_A2 + T2*WF2_A2
    // DT_A2 = D1*WF1_A2 + D2*WF2_A2
    // UT_A2 = U1*WF1_A2 + U2*WF2_A2
    const double ttA2 = t1 * wf1A2 + t2 * wf2A2;
    const double dtA2 = d1 * wf1A2 + d2 * wf2A2;
    const double utA2 = u1 * wf1A2 + u2 * wf2A2;
    // Fortran: temporarily set "2" variables from "T" for BLKIN
    // X2 = XT ; T2 = TT ; D2 = DT ; U2 = UT ; CALL BLKIN
    const BoundaryLayerSecondary interpSec = blkinAt(xt, tt, dt, ut, props);
    // Fortran: calculate laminar secondary "T" variables HKT, RTT
    // (state of the last iteration for the sensitivity block)
    hkt = interpSec.hk;
    hktTt = interpSec.hkT;
    hktDt = interpSec.hkD;
    hktUt = interpSec.hkUe;
    hktMs = interpSec.hkMs;
    rtt = interpSec.rt;
    rttTt = interpSec.rtT;
    rttUt = interpSec.rtUe;
    rttMs = interpSec.rtMs;
    rttRe = interpSec.rtRe;
    // Fortran: restore clobbered "2" variables, except for AMPL2
    // (in C++ local copies; x2/t2/d2/u2 remain unchanged)
    // Fortran: calculate amplification rate AX over current X1-XT interval
 // CALL AXSET (HK1, T1, RT1, AMPL1, HKT, TT, RTT, AMPLT, ...)
    axLast = axset(s1.hk, t1, s1.rt, ampl1, hkt, tt, rtt, amplt, in.acrit,
                   in.idampv);
    // Fortran: punch out early if there is no amplification here
    // IF(AX .LE. 0.0) GO TO 101
    if (axLast.value <= 0.0) {
      punchedOut = true;
      break;
    }
    // Fortran: set sensitivity of AX(A2)
    // AX_A2 = (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_A2
 // & + (AX_HKT*HKT_DT)*DT_A2
    // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_A2
    // & + AX_AT *AMPLT_A2
    const double axA2 =
        (axLast.shape2Derivative * hktTt + axLast.momentum2Derivative +
         axLast.reynolds2Derivative * rttTt) * ttA2 +
        (axLast.shape2Derivative * hktDt) * dtA2 +
        (axLast.shape2Derivative * hktUt +
         axLast.reynolds2Derivative * rttUt) * utA2 +
        axLast.amplification2Derivative * ampltA2;
    // Fortran: residual for implicit AMPL2 definition
    // RES = AMPL2 - AMPL1 - AX *(X2-X1)
    // RES_A2 = 1.0 - AX_A2*(X2-X1)
    const double res = ampl2 - ampl1 - axLast.value * (x2 - x1);
    const double resA2 = 1.0 - axA2 * (x2 - x1);
    const double da2 = -res / resA2;
    lastXt = xt;
    lastAmplt = amplt;
    lastAx = axLast.value;
    lastDa2 = da2;
    double rlx = 1.0;
    const double dxt = (x1 * wf1A2 + x2 * wf2A2) * da2;
    // Fortran: IF(RLX*ABS(DXT/(X2-X1)) .GT. 0.05) RLX = 0.05*ABS((X2-X1)/DXT)
 // IF(RLX*ABS(Since2) .GT. 1.0) RLX = 1.0 *ABS (1.0 /Since2)
    if (rlx * std::fabs(dxt / (x2 - x1)) > 0.05) {
      rlx = 0.05 * std::fabs((x2 - x1) / dxt);
    }
    if (rlx * std::fabs(da2) > 1.0) {
      rlx = 1.0 * std::fabs(1.0 / da2);
    }
    // Fortran: check if converged
    // IF(ABS(Since2) .LT. DAEPS) GO TO 101
    if (std::fabs(da2) < daeps) {
      converged = true;
      break;
    }
    // Fortran: limited Newton step so AMPL2 doesn't step across AMCRIT
    if ((ampl2 > in.acrit && ampl2 + rlx * da2 < in.acrit) ||
        (ampl2 < in.acrit && ampl2 + rlx * da2 > in.acrit)) {
      ampl2 = in.acrit;
    } else {
      ampl2 = ampl2 + rlx * da2;
    }
  }
  out.ampl = ampl2;
  out.converged = converged;
  out.nonAmplified = punchedOut;
  if (!converged && !punchedOut) {
    out.n2ConvergenceFailed = true;
    out.failX1 = x1;
    out.failXt = lastXt;
    out.failX2 = x2;
    out.failAmpl1 = ampl1;
    out.failAmplt = lastAmplt;
    out.failAmpl2 = ampl2;
    out.failAx = lastAx;
    out.failDa2 = lastDa2;
  }
  out.xt = x1 * wf1 + x2 * wf2;
  // Fortran: test for free or forced transition
  // TRFREE = AMPL2 .GE. AMCRIT
  // TRFORC = XIFORC.GT.X1 .AND. XIFORC.LE.X2
  // TRAN = TRFORC .OR. TRFREE
  out.trfree = ampl2 >= in.acrit;
  out.trforc = in.xiforc > x1 && in.xiforc <= x2;
  out.tran = out.trforc || out.trfree;
  if (!out.tran) {
    return out;
  }
  // Fortran: resolve if both forced and free transition
  // IF(TRFREE .AND. TRFORC) THEN
  // TRFORC = XIFORC .LT. XT ; TRFREE = XIFORC .GE. XT
  // ENDIF
  if (out.trfree && out.trforc) {
    out.trforc = in.xiforc < out.xt;
    out.trfree = in.xiforc >= out.xt;
  }
  if (out.trforc) {
    // Fortran: if forced transition, then XT is prescribed,
    // no sense calculating the sensitivities, since we know them...
    out.xt = in.xiforc;
    out.xtA1 = 0.0;
    out.xtX1 = 0.0;
    out.xtT1 = 0.0;
    out.xtD1 = 0.0;
    out.xtU1 = 0.0;
    out.xtX2 = 0.0;
    out.xtT2 = 0.0;
    out.xtD2 = 0.0;
    out.xtU2 = 0.0;
    out.xtMs = 0.0;
    out.xtRe = 0.0;
    out.xtXf = 1.0;
    return out;
  }
  // Fortran: free transition ... set sensitivities of XT
 // --- XT (X1 X2 A1 A2 XF), TT (T1 T2 A1 A2 X1 X2 XF), DT(...
  // XT_X1 = WF1 ; TT_T1 = WF1 ; DT_D1 = WF1 ; UT_U1 = WF1
  // XT_X2 = WF2 ; TT_T2 = WF2 ; DT_D2 = WF2 ; UT_U2 = WF2
  // XT_A1 = X1*WF1_A1 + X2*WF2_A1 ; TT_A1 = T1*WF1_A1 + T2*WF2_A1
  // DT_A1 = D1*WF1_A1 + D2*WF2_A1 ; UT_A1 = U1*WF1_A1 + U2*WF2_A1
  // XT_X1 = X1*WF1_X1 + X2*WF2_X1 + XT_X1
  // TT_X1 = T1*WF1_X1 + T2*WF2_X1 ; DT_X1 = D1*WF1_X1 + D2*WF2_X1
  // UT_X1 = U1*WF1_X1 + U2*WF2_X1
  // XT_X2 = X1*WF1_X2 + X2*WF2_X2 + XT_X2
  // TT_X2 = T1*WF1_X2 + T2*WF2_X2 ; DT_X2 = D1*WF1_X2 + D2*WF2_X2
  // UT_X2 = U1*WF1_X2 + U2*WF2_X2
  // XT_XF = X1*WF1_XF + X2*WF2_XF ; TT_XF = T1*WF1_XF + T2*WF2_XF
  // DT_XF = D1*WF1_XF + D2*WF2_XF ; UT_XF = U1*WF1_XF + U2*WF2_XF
  const double ttT1 = wf1;
  const double ttT2 = wf2;
  const double dtD1 = wf1;
  const double dtD2 = wf2;
  const double utU1 = wf1;
  const double utU2 = wf2;
  out.xtX1 = wf1;
  out.xtX2 = wf2;
  out.xtA1 = x1 * wf1A1 + x2 * wf2A1;
  const double ttA1 = t1 * wf1A1 + t2 * wf2A1;
  const double dtA1 = d1 * wf1A1 + d2 * wf2A1;
  const double utA1 = u1 * wf1A1 + u2 * wf2A1;
  const double ttX1 = t1 * wf1X1 + t2 * wf2X1;
  const double dtX1 = d1 * wf1X1 + d2 * wf2X1;
  const double utX1 = u1 * wf1X1 + u2 * wf2X1;
  const double ttX2 = t1 * wf1X2 + t2 * wf2X2;
  const double dtX2 = d1 * wf1X2 + d2 * wf2X2;
  const double utX2 = u1 * wf1X2 + u2 * wf2X2;
  // Note: AX_XF/TT_XF/DT_XF/UT_XF (xblsys.f:502-505, 535-537) sum to the
  // canonical result XT_XF (= 0.0, line 577) and are therefore
  // omitted.
  out.xtX1 = x1 * wf1X1 + x2 * wf2X1 + out.xtX1;
  out.xtX2 = x1 * wf1X2 + x2 * wf2X2 + out.xtX2;
  out.xtXf = x1 * wf1Xf + x2 * wf2Xf;
  // Fortran: at this point, AX = AX (HK1, T1, RT1, A1, HKT, TT, RTT, AT)
 // --- set sensitivities of AX (T1 D1 U1 A1 T2 D2 U2 A2 MS Re)
  // AX_T1 = AX_HK1*HK1_T1 + AX_T1 + AX_RT1*RT1_T1
  // & + (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_T1
  // AX_D1 = AX_HK1*HK1_D1
 // & + (AX_HKT*HKT_DT)*DT_D1
  // AX_U1 = AX_HK1*HK1_U1 + AX_RT1*RT1_U1
  // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_U1
  // AX_A1 = AX_A1
  // & + (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_A1
 // & + (AX_HKT*HKT_DT)*DT_A1
  // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_A1
  // AX_X1 = (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_X1
 // & + (AX_HKT*HKT_DT)*DT_X1
  // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_X1
  // AX_T2 = (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_T2
 // AX_D2 = (AX_HKT*HKT_DT)*DT_D2
  // AX_U2 = (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_U2
  // AX_A2 = AX_AT *AMPLT_A2
  // & + (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_A2
 // & + (AX_HKT*HKT_DT)*DT_A2
  // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_A2
  // AX_X2 = (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_X2
 // & + (AX_HKT*HKT_DT)*DT_X2
  // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_X2
  // AX_XF = (AX_HKT*HKT_TT + AX_TT + AX_RTT*RTT_TT)*TT_XF
 // & + (AX_HKT*HKT_DT)*DT_XF
  // & + (AX_HKT*HKT_UT + AX_RTT*RTT_UT)*UT_XF
  // AX_MS = AX_HKT*HKT_MS + AX_RTT*RTT_MS
  // & + AX_HK1*HK1_MS + AX_RT1*RT1_MS
  // AX_Re = AX_RTT*RTT_Re
  // & + AX_RT1*RT1_Re
  const double axHk1 = axLast.shape1Derivative;
  const double axT1 = axLast.momentum1Derivative;
  const double axRt1 = axLast.reynolds1Derivative;
  const double axA1v = axLast.amplification1Derivative;
  const double axHkt = axLast.shape2Derivative;
  const double axTt = axLast.momentum2Derivative;
  const double axRtt = axLast.reynolds2Derivative;
  const double axAt = axLast.amplification2Derivative;
  const double hktFac = axHkt * hktTt + axTt + axRtt * rttTt;
  const double axT1f = axHk1 * s1.hkT + axT1 + axRt1 * s1.rtT +
                       hktFac * ttT1;
  const double axD1f = axHk1 * s1.hkD + (axHkt * hktDt) * dtD1;
  const double axU1f = axHk1 * s1.hkU + axRt1 * s1.rtU +
                       (axHkt * hktUt + axRtt * rttUt) * utU1;
  const double axA1f = axA1v + hktFac * ttA1 + (axHkt * hktDt) * dtA1 +
                       (axHkt * hktUt + axRtt * rttUt) * utA1;
  const double axX1f = hktFac * ttX1 + (axHkt * hktDt) * dtX1 +
                       (axHkt * hktUt + axRtt * rttUt) * utX1;
  const double axT2f = hktFac * ttT2;
  const double axD2f = (axHkt * hktDt) * dtD2;
  const double axU2f = (axHkt * hktUt + axRtt * rttUt) * utU2;
  const double axA2f = axAt * ampltA2 + hktFac * (t1 * wf1A2 + t2 * wf2A2) +
                       (axHkt * hktDt) * (d1 * wf1A2 + d2 * wf2A2) +
                       (axHkt * hktUt + axRtt * rttUt) *
                           (u1 * wf1A2 + u2 * wf2A2);
  const double axX2f = hktFac * ttX2 + (axHkt * hktDt) * dtX2 +
                       (axHkt * hktUt + axRtt * rttUt) * utX2;
  const double axMsf = axHkt * hktMs + axRtt * rttMs + axHk1 * s1.hkMs +
                       axRt1 * s1.rtMs;
  const double axRef = axRtt * rttRe + axRt1 * s1.rtRe;
  // Fortran: set sensitivities of residual RES
  // Z_AX = - (X2-X1)
  // Z_A1 = Z_AX*AX_A1 - 1.0 ; Z_T1 = Z_AX*AX_T1
  // Z_D1 = Z_AX*AX_D1 ; Z_U1 = Z_AX*AX_U1 ; Z_X1 = Z_AX*AX_X1 + AX
  // Z_A2 = Z_AX*AX_A2 + 1.0 ; Z_T2 = Z_AX*AX_T2
  // Z_D2 = Z_AX*AX_D2 ; Z_U2 = Z_AX*AX_U2 ; Z_X2 = Z_AX*AX_X2 - AX
  // Z_XF = Z_AX*AX_XF; Z_MS = Z_AX*AX_MS; Z_Re = Z_AX*AX_Re
  const double zAx = -(x2 - x1);
  const double zA1 = zAx * axA1f - 1.0;
  const double zT1 = zAx * axT1f;
  const double zD1 = zAx * axD1f;
  const double zU1 = zAx * axU1f;
  const double zX1 = zAx * axX1f + axLast.value;
  const double zA2 = zAx * axA2f + 1.0;
  const double zT2 = zAx * axT2f;
  const double zD2 = zAx * axD2f;
  const double zU2 = zAx * axU2f;
  const double zX2 = zAx * axX2f - axLast.value;
  // Fortran: set sensitivities of XT, with RES being stationary for the
  // A2 constraint
  // XT_A1 = XT_A1 - (XT_A2/Z_A2)*Z_A1
  // XT_T1 = - (XT_A2/Z_A2)*Z_T1
  // XT_D1 = - (XT_A2/Z_A2)*Z_D1
  // XT_U1 = - (XT_A2/Z_A2)*Z_U1
  // XT_X1 = XT_X1 - (XT_A2/Z_A2)*Z_X1
  // XT_T2 = - (XT_A2/Z_A2)*Z_T2
  // XT_D2 = - (XT_A2/Z_A2)*Z_D2
  // XT_U2 = - (XT_A2/Z_A2)*Z_U2
  // XT_X2 = XT_X2 - (XT_A2/Z_A2)*Z_X2
  // XT_MS = - (XT_A2/Z_A2)*Z_MS
  // XT_Re = - (XT_A2/Z_A2)*Z_Re
  // XT_XF = 0.0
  // XT_A2 = X1*WF1_A2 + X2*WF2_A2 (from the last Newton iteration)
  const double xtA2 = x1 * wf1A2 + x2 * wf2A2;
  const double corr = (xtA2 / zA2);
  out.xtA1 = out.xtA1 - corr * zA1;
  out.xtT1 = -corr * zT1;
  out.xtD1 = -corr * zD1;
  out.xtU1 = -corr * zU1;
  out.xtX1 = out.xtX1 - corr * zX1;
  out.xtT2 = -corr * zT2;
  out.xtD2 = -corr * zD2;
  out.xtU2 = -corr * zU2;
  out.xtX2 = out.xtX2 - corr * zX2;
  // Fortran: Z_MS = Z_AX*AX_MS; Z_Re = Z_AX*AX_Re (xblsys.f:562-563);
  // the earlier direct AX_MS/AX_Re use without the Z_AX factor was
  // corrected test-backed (TRCHKT XT_MS reference).
  out.xtMs = -corr * (zAx * axMsf);
  out.xtRe = -corr * (zAx * axRef);
  out.xtXf = 0.0;
  return out;
}


// Fortran: xblsys.f:1552 BLDIF(ITYP)
BldifSystem bldif(const BldifInput& in) {
  const BldifStation& st1 = in.first;
  const BldifStation& st2 = in.second;
  const MidpointSkinFriction& mid = in.mid;
  BldifSystem out{};
  // Fortran: logarithmic differences
  double xlog = 0.0;
  double ulog = 0.0;
  double tlog = 0.0;
  double hlog = 0.0;
  double ddlog = 0.0;
  if (in.similarity) {
    // Fortran: similarity logarithmic differences (prescribed)
    xlog = 1.0;
    ulog = in.bule;
    tlog = 0.5 * (1.0 - in.bule);
    hlog = 0.0;
    ddlog = 0.0;
  } else {
    // Fortran: usual logarithmic differences
    xlog = std::log(st2.x / st1.x);
    ulog = std::log(st2.ue / st1.ue);
    tlog = std::log(st2.t / st1.t);
    hlog = std::log(st2.hs / st1.hs);
    ddlog = 1.0;
  }
  // Fortran: zero the system
  for (int k = 0; k < 4; ++k) {
    out.vsrez[k] = 0.0;
    out.vsr[k] = 0.0;
    out.vsm[k] = 0.0;
    out.vsx[k] = 0.0;
    for (int l = 0; l < 5; ++l) {
      out.vs1[k][l] = 0.0;
      out.vs2[k][l] = 0.0;
    }
  }
  // Fortran: set triggering constant for local upwinding
  // HUPWT = 1.0 ; HDCON = 5.0*HUPWT/HK2**2 (wake: HUPWT/HK2**2)
  // HD_HK1 = 0.0 ; HD_HK2 = -HDCON*2.0/HK2
  const double hupwt = 1.0;
  const double hdcon =
      (in.ityp == StationType::kWake) ? hupwt / (st2.hk * st2.hk)
                                      : 5.0 * hupwt / (st2.hk * st2.hk);
  const double hdHk1 = 0.0;
  const double hdHk2 = -hdcon * 2.0 / st2.hk;
  // Fortran: local upwinding is based on local change in log(Hk-1)
  // ARG = ABS((HK2-1.0)/(HK1-1.0)) ; HL = LOG(ARG)
  // HL_HK1 = -1.0/(HK1-1.0) ; HL_HK2 = 1.0/(HK2-1.0)
  // HLSQ = MIN(HL**2, 15.0) ; EHH = EXP(-HLSQ*HDCON)
  // UPW = 1.0 - 0.5*EHH ; UPW_HL = EHH*HL*HDCON
  // UPW_HD = 0.5*EHH*HLSQ
  // UPW_HK1 = UPW_HL*HL_HK1 + UPW_HD*HD_HK1
  // UPW_HK2 = UPW_HL*HL_HK2 + UPW_HD*HD_HK2
  const double arg = std::fabs((st2.hk - 1.0) / (st1.hk - 1.0));
  const double hl = std::log(arg);
  const double hlHk1 = -1.0 / (st1.hk - 1.0);
  const double hlHk2 = 1.0 / (st2.hk - 1.0);
  const double hlsq = std::min(hl * hl, 15.0);
  const double ehh = std::exp(-hlsq * hdcon);
  const double upw = 1.0 - 0.5 * ehh;
  const double upwHl = ehh * hl * hdcon;
  const double upwHd = 0.5 * ehh * hlsq;
  const double upwHk1 = upwHl * hlHk1 + upwHd * hdHk1;
  const double upwHk2 = upwHl * hlHk2 + upwHd * hdHk2;
  // Fortran: UPW_U1 = UPW_HK1*HK1_U1 ; UPW_T1 = UPW_HK1*HK1_T1
  // UPW_D1 = UPW_HK1*HK1_D1 ; UPW_U2 = UPW_HK2*HK2_U2
  // UPW_T2 = UPW_HK2*HK2_T2 ; UPW_D2 = UPW_HK2*HK2_D2
  // UPW_MS = UPW_HK1*HK1_MS + UPW_HK2*HK2_MS
  const double upwU1 = upwHk1 * st1.hkU;
  const double upwT1 = upwHk1 * st1.hkT;
  const double upwD1 = upwHk1 * st1.hkD;
  const double upwU2 = upwHk2 * st2.hkU;
  const double upwT2 = upwHk2 * st2.hkT;
  const double upwD2 = upwHk2 * st2.hkD;
  const double upwMs = upwHk1 * st1.hkMs + upwHk2 * st2.hkMs;
  // --- Line 1
  if (in.similarity) {
    // Fortran: LE point --> set zero amplification factor
    out.vs2[0][0] = 1.0;
    out.vsr[0] = 0.0;
    out.vsrez[0] = -in.ampl2;
  } else if (in.ityp == StationType::kLaminar) {
    // Fortran: laminar part --> set amplification equation
    const AverageAmplificationValue ax =
        axset(st1.hk, st1.t, st1.rt, st1.ampl, st2.hk, st2.t, st2.rt,
              st2.ampl, in.acrit, in.idampv);
    const double rezc =
        st2.ampl - st1.ampl - ax.value * (st2.x - st1.x);
    const double zAx = -(st2.x - st1.x);
    out.vs1[0][0] = zAx * ax.amplification1Derivative - 1.0;
    out.vs1[0][1] = zAx * (ax.shape1Derivative * st1.hkT +
                           ax.momentum1Derivative +
                           ax.reynolds1Derivative * st1.rtT);
    out.vs1[0][2] = zAx * (ax.shape1Derivative * st1.hkD);
    out.vs1[0][3] = zAx * (ax.shape1Derivative * st1.hkU +
                           ax.reynolds1Derivative * st1.rtU);
    out.vs1[0][4] = ax.value;
    out.vs2[0][0] = zAx * ax.amplification2Derivative + 1.0;
    out.vs2[0][1] = zAx * (ax.shape2Derivative * st2.hkT +
                           ax.momentum2Derivative +
                           ax.reynolds2Derivative * st2.rtT);
    out.vs2[0][2] = zAx * (ax.shape2Derivative * st2.hkD);
    out.vs2[0][3] = zAx * (ax.shape2Derivative * st2.hkU +
                           ax.reynolds2Derivative * st2.rtU);
    out.vs2[0][4] = -ax.value;
    out.vsm[0] = zAx * (ax.shape1Derivative * st1.hkMs +
                        ax.reynolds1Derivative * st1.rtMs +
                        ax.shape2Derivative * st2.hkMs +
                        ax.reynolds2Derivative * st2.rtMs);
    out.vsr[0] = zAx * (ax.reynolds1Derivative * st1.rtRe +
                        ax.reynolds2Derivative * st2.rtRe);
    out.vsx[0] = 0.0;
    out.vsrez[0] = -rezc;
  } else {
    // Fortran: turbulent part --> set shear lag equation
    const double sa = (1.0 - upw) * st1.s + upw * st2.s;
    const double cqa = (1.0 - upw) * st1.cq + upw * st2.cq;
    const double cfa = (1.0 - upw) * st1.cf + upw * st2.cf;
    const double hka = (1.0 - upw) * st1.hk + upw * st2.hk;
    const double usa = 0.5 * (st1.us + st2.us);
    const double rta = 0.5 * (st1.rt + st2.rt);
    const double dea = 0.5 * (st1.de + st2.de);
    const double da = 0.5 * (st1.d + st2.d);
    // Fortran: IF(ITYP.EQ.3) THEN ALD = DLCON ELSE ALD = 1.0
    const double ald = (in.ityp == StationType::kWake)
                           ? in.blpar.dlcon
                           : 1.0;
    // Fortran: GCC/HKC (ITYP=2 with clamp, otherwise 0)
    double hkc = 0.0;
    double hkcHka = 0.0;
    double hkcRta = 0.0;
    if (in.ityp == StationType::kTurbulent) {
      hkc = hka - 1.0 - in.blpar.gccon / rta;
      hkcHka = 1.0;
      hkcRta = in.blpar.gccon / (rta * rta);
      if (hkc < 0.01) {
        hkc = 0.01;
        hkcHka = 0.0;
        hkcRta = 0.0;
      }
    } else {
      hkc = hka - 1.0;
      hkcHka = 1.0;
      hkcRta = 0.0;
    }
    // Fortran: HR = HKC/(GACON*ALD*HKA) and derivatives
    const double gacon = in.blpar.gacon;
    const double hr = hkc / (gacon * ald * hka);
    const double hrHka = hkcHka / (gacon * ald * hka) - hr / hka;
    const double hrRta = hkcRta / (gacon * ald * hka);
    // Fortran: UQ = (0.5*CFA - HR**2)/(GBCON*DA) and derivatives
    const double gbcon = in.blpar.gbcon;
    const double uq = (0.5 * cfa - hr * hr) / (gbcon * da);
    const double uqHka = -2.0 * hr * hrHka / (gbcon * da);
    const double uqRta = -2.0 * hr * hrRta / (gbcon * da);
    const double uqCfa = 0.5 / (gbcon * da);
    const double uqDa = -uq / da;
    const double uqUpw =
        uqCfa * (st2.cf - st1.cf) + uqHka * (st2.hk - st1.hk);
    const double uqT1 = (1.0 - upw) * (uqCfa * st1.cfT + uqHka * st1.hkT) +
                        uqUpw * upwT1;
    const double uqD1 = (1.0 - upw) * (uqCfa * st1.cfD + uqHka * st1.hkD) +
                        uqUpw * upwD1;
    const double uqU1 = (1.0 - upw) * (uqCfa * st1.cfU + uqHka * st1.hkU) +
                        uqUpw * upwU1;
    const double uqT2 = upw * (uqCfa * st2.cfT + uqHka * st2.hkT) +
                        uqUpw * upwT2;
    const double uqD2 = upw * (uqCfa * st2.cfD + uqHka * st2.hkD) +
                        uqUpw * upwD2;
    const double uqU2 = upw * (uqCfa * st2.cfU + uqHka * st2.hkU) +
                        uqUpw * upwU2;
    const double uqMs = (1.0 - upw) * (uqCfa * st1.cfMs + uqHka * st1.hkMs) +
                        uqUpw * upwMs +
                        upw * (uqCfa * st2.cfMs + uqHka * st2.hkMs);
    const double uqRe =
        (1.0 - upw) * uqCfa * st1.cfRe + upw * uqCfa * st2.cfRe;
    // Fortran: UQ_T1 += 0.5*UQ_RTA*RT1_T1 ; UQ_D1 += 0.5*UQ_Since ; ...
    const double uqT1f = uqT1 + 0.5 * uqRta * st1.rtT;
    const double uqD1f = uqD1 + 0.5 * uqDa;
    const double uqU1f = uqU1 + 0.5 * uqRta * st1.rtU;
    const double uqT2f = uqT2 + 0.5 * uqRta * st2.rtT;
    const double uqD2f = uqD2 + 0.5 * uqDa;
    const double uqU2f = uqU2 + 0.5 * uqRta * st2.rtU;
    const double uqMsf =
        uqMs + 0.5 * uqRta * st1.rtMs + 0.5 * uqRta * st2.rtMs;
    const double uqRef =
        uqRe + 0.5 * uqRta * st1.rtRe + 0.5 * uqRta * st2.rtRe;
    static_cast<void>(uqT1f);
    static_cast<void>(uqD1f);
    static_cast<void>(uqU1f);
    static_cast<void>(uqT2f);
    static_cast<void>(uqD2f);
    static_cast<void>(uqU2f);
    static_cast<void>(uqMsf);
    static_cast<void>(uqRef);
    // Fortran: SCC = SCCON*1.333/(1.0+USA) ; SCC_USA = -SCC/(1.0+USA)
    const double scc = in.blpar.sccon * 1.333 / (1.0 + usa);
    const double sccUsa = -scc / (1.0 + usa);
    // Note: SCC_US1/SCC_US2 (xblsys.f:1760-1761) are computed in the
    // canonical code but do not enter VS1/VS2 (VS1(1,2..4) uses the
    // Z_US1/Z_US2 values); dead Fortran code at this point.
    static_cast<void>(sccUsa * 0.5);
    static_cast<void>(sccUsa * 0.5);
    // Fortran: SLOG = LOG(S2/S1) ; DXI = X2-X1
    const double slog = std::log(st2.s / st1.s);
    const double dxi = st2.x - st1.x;
    // Fortran: REZC = SCC*(CQA-SA*ALD)*DXI - DEA*2.0*SLOG
    // & + DEA*2.0*(UQ*DXI - ULOG)*DUXCON
    const double duxcon = in.blpar.duxcon;
    const double rezc = scc * (cqa - sa * ald) * dxi - dea * 2.0 * slog +
                        dea * 2.0 * (uq * dxi - ulog) * duxcon;
    // Fortran: Z chains
    const double zCfa = dea * 2.0 * uqCfa * dxi * duxcon;
    const double zHka = dea * 2.0 * uqHka * dxi * duxcon;
    const double zDa = dea * 2.0 * uqDa * dxi * duxcon;
    const double zSl = -dea * 2.0;
    const double zUl = -dea * 2.0 * duxcon;
    const double zDxi = scc * (cqa - sa * ald) + dea * 2.0 * uq * duxcon;
    const double zUsa = sccUsa * (cqa - sa * ald) * dxi;
    const double zCqa = scc * dxi;
    const double zSa = -scc * dxi * ald;
    const double zDea = 2.0 * ((uq * dxi - ulog) * duxcon - slog);
    const double zUpw = zCqa * (st2.cq - st1.cq) + zSa * (st2.s - st1.s) +
                        zCfa * (st2.cf - st1.cf) + zHka * (st2.hk - st1.hk);
    const double zDe1 = 0.5 * zDea;
    const double zDe2 = 0.5 * zDea;
    const double zUs1 = 0.5 * zUsa;
    const double zUs2 = 0.5 * zUsa;
    const double zD1 = 0.5 * zDa;
    const double zD2 = 0.5 * zDa;
    const double zU1 = -zUl / st1.ue;
    const double zU2 = zUl / st2.ue;
    const double zX1 = -zDxi;
    const double zX2 = zDxi;
    const double zS1 = (1.0 - upw) * zSa - zSl / st1.s;
    const double zS2 = upw * zSa + zSl / st2.s;
    const double zCq1 = (1.0 - upw) * zCqa;
    const double zCq2 = upw * zCqa;
    const double zCf1 = (1.0 - upw) * zCfa;
    const double zCf2 = upw * zCfa;
    const double zHk1 = (1.0 - upw) * zHka;
    const double zHk2 = upw * zHka;
    // Fortran: VS1(1,1) = Z_S1 ; VS1(1,2..4) ; VS1(1,5) = Z_X1 ; VS2 analogous
    out.vs1[0][0] = zS1;
    out.vs1[0][1] = zUpw * upwT1 + zDe1 * st1.deT + zUs1 * st1.usT;
    out.vs1[0][2] = zD1 + zUpw * upwD1 + zDe1 * st1.deD + zUs1 * st1.usD;
    out.vs1[0][3] = zU1 + zUpw * upwU1 + zDe1 * st1.deU + zUs1 * st1.usU;
    out.vs1[0][4] = zX1;
    out.vs2[0][0] = zS2;
    out.vs2[0][1] = zUpw * upwT2 + zDe2 * st2.deT + zUs2 * st2.usT;
    out.vs2[0][2] = zD2 + zUpw * upwD2 + zDe2 * st2.deD + zUs2 * st2.usD;
    out.vs2[0][3] = zU2 + zUpw * upwU2 + zDe2 * st2.deU + zUs2 * st2.usU;
    out.vs2[0][4] = zX2;
    out.vsm[0] = zUpw * upwMs + zDe1 * st1.deMs + zUs1 * st1.usMs +
                 zDe2 * st2.deMs + zUs2 * st2.usMs;
    // Fortran: VS1(1,2) += Z_CQ1*CQ1_T1 + Z_CF1*CF1_T1 + Z_HK1*HK1_T1 etc.
    out.vs1[0][1] = out.vs1[0][1] + zCq1 * st1.cqT + zCf1 * st1.cfT +
                    zHk1 * st1.hkT;
    out.vs1[0][2] = out.vs1[0][2] + zCq1 * st1.cqD + zCf1 * st1.cfD +
                    zHk1 * st1.hkD;
    out.vs1[0][3] = out.vs1[0][3] + zCq1 * st1.cqU + zCf1 * st1.cfU +
                    zHk1 * st1.hkU;
    out.vs2[0][1] = out.vs2[0][1] + zCq2 * st2.cqT + zCf2 * st2.cfT +
                    zHk2 * st2.hkT;
    out.vs2[0][2] = out.vs2[0][2] + zCq2 * st2.cqD + zCf2 * st2.cfD +
                    zHk2 * st2.hkD;
    out.vs2[0][3] = out.vs2[0][3] + zCq2 * st2.cqU + zCf2 * st2.cfU +
                    zHk2 * st2.hkU;
    out.vsm[0] = out.vsm[0] + zCq1 * st1.cqMs + zCf1 * st1.cfMs +
                 zHk1 * st1.hkMs + zCq2 * st2.cqMs + zCf2 * st2.cfMs +
                 zHk2 * st2.hkMs;
    out.vsr[0] = zCq1 * st1.cqRe + zCf1 * st1.cfRe + zCq2 * st2.cqRe +
                 zCf2 * st2.cfRe;
    out.vsx[0] = 0.0;
    out.vsrez[0] = -rezc;
  }
  // --- Line 2: Momentum equation
  // Fortran: HA = 0.5*(H1+H2) ; MA = 0.5*(M1+M2) ; XA ; TA
  // HWA = 0.5*(DW1/T1 + DW2/T2)
  const double ha = 0.5 * (st1.h + st2.h);
  const double ma = 0.5 * (st1.m + st2.m);
  const double xa = 0.5 * (st1.x + st2.x);
  const double ta = 0.5 * (st1.t + st2.t);
  const double hwa = 0.5 * (st1.dw / st1.t + st2.dw / st2.t);
  // Fortran: CFX = 0.50*CFM*XA/TA + 0.25*(CF1*X1/T1 + CF2*X2/T2)
  const double cfx =
      0.50 * mid.cf * xa / ta +
      0.25 * (st1.cf * st1.x / st1.t + st2.cf * st2.x / st2.t);
  // Fortran: CFX_XA/TA/X1/X2/T1/T2/CF1/CF2/CFM
  const double cfxXa = 0.50 * mid.cf / ta;
  const double cfxTa = -0.50 * mid.cf * xa / (ta * ta);
  const double cfxX1 = 0.25 * st1.cf / st1.t + cfxXa * 0.5;
  const double cfxX2 = 0.25 * st2.cf / st2.t + cfxXa * 0.5;
  const double cfxT1 = -0.25 * st1.cf * st1.x / (st1.t * st1.t) + cfxTa * 0.5;
  const double cfxT2 = -0.25 * st2.cf * st2.x / (st2.t * st2.t) + cfxTa * 0.5;
  const double cfxCf1 = 0.25 * st1.x / st1.t;
  const double cfxCf2 = 0.25 * st2.x / st2.t;
  const double cfxCfm = 0.50 * xa / ta;
  // Fortran: BTMP = HA + 2.0 - MA + HWA
  // REZT = TLOG + BTMP*ULOG - XLOG*0.5*CFX
  const double btmp = ha + 2.0 - ma + hwa;
  const double rezt = tlog + btmp * ulog - xlog * 0.5 * cfx;
  // Fortran: Z_CFX = -XLOG*0.5 ; Z_HA = ULOG ; Z_HWA = ULOG ; Z_MA = -ULOG
  // Z_XL = -DDLOG*0.5*CFX ; Z_UL = DDLOG*BTMP ; Z_TL = DDLOG
  const double zCfx = -xlog * 0.5;
  const double zHa = ulog;
  const double zHwa = ulog;
  const double zMa = -ulog;
  const double zXl = -ddlog * 0.5 * cfx;
  const double zUl = ddlog * btmp;
  const double zTl = ddlog;
  // Fortran: Z_CFM = Z_CFX*CFX_CFM ; Z_CF1 = Z_CFX*CFX_CF1 ; Z_CF2 = ...
  const double zCfm = zCfx * cfxCfm;
  const double zCf1 = zCfx * cfxCf1;
  const double zCf2 = zCfx * cfxCf2;
  // Fortran: Z_T1/Z_T2/Z_X1/Z_X2/Z_U1/Z_U2
  const double zT1 = -zTl / st1.t + zCfx * cfxT1 +
                     zHwa * 0.5 * (-st1.dw / (st1.t * st1.t));
  const double zT2 = zTl / st2.t + zCfx * cfxT2 +
                     zHwa * 0.5 * (-st2.dw / (st2.t * st2.t));
  const double zX1 = -zXl / st1.x + zCfx * cfxX1;
  const double zX2 = zXl / st2.x + zCfx * cfxX2;
  const double zU1 = -zUl / st1.ue;
  const double zU2 = zUl / st2.ue;
  // Fortran: VS1(2,2)/VS1(2,3)/VS1(2,4)/VS1(2,5) ; VS2 analogous
  out.vs1[1][1] = 0.5 * zHa * st1.hT + zCfm * mid.cfT1 + zCf1 * st1.cfT + zT1;
  out.vs1[1][2] = 0.5 * zHa * st1.hD + zCfm * mid.cfD1 + zCf1 * st1.cfD;
  out.vs1[1][3] = 0.5 * zMa * st1.mU + zCfm * mid.cfU1 + zCf1 * st1.cfU + zU1;
  out.vs1[1][4] = zX1;
  out.vs2[1][1] = 0.5 * zHa * st2.hT + zCfm * mid.cfT2 + zCf2 * st2.cfT + zT2;
  out.vs2[1][2] = 0.5 * zHa * st2.hD + zCfm * mid.cfD2 + zCf2 * st2.cfD;
  out.vs2[1][3] = 0.5 * zMa * st2.mU + zCfm * mid.cfU2 + zCf2 * st2.cfU + zU2;
  out.vs2[1][4] = zX2;
  out.vsm[1] = 0.5 * zMa * st1.mMs + zCfm * mid.cfMs + zCf1 * st1.cfMs +
               0.5 * zMa * st2.mMs + zCf2 * st2.cfMs;
  out.vsr[1] = zCfm * mid.cfRe + zCf1 * st1.cfRe + zCf2 * st2.cfRe;
  out.vsx[1] = 0.0;
  out.vsrez[1] = -rezt;
  // --- Line 3: shape-factor equation
  // Fortran: XOT1 = X1/T1 ; XOT2 = X2/T2
  const double xot1 = st1.x / st1.t;
  const double xot2 = st2.x / st2.t;
  const double ha3 = 0.5 * (st1.h + st2.h);
  const double hsa = 0.5 * (st1.hs + st2.hs);
  const double hca = 0.5 * (st1.hc + st2.hc);
  const double hwa3 = 0.5 * (st1.dw / st1.t + st2.dw / st2.t);
  // Fortran: DIX = (1-UPW)*DI1*XOT1 + UPW*DI2*XOT2
  // CFX = (1-UPW)*CF1*XOT1 + UPW*CF2*XOT2
  const double dix = (1.0 - upw) * st1.di * xot1 + upw * st2.di * xot2;
  const double cfx3 = (1.0 - upw) * st1.cf * xot1 + upw * st2.cf * xot2;
  const double dixUpw = st2.di * xot2 - st1.di * xot1;
  const double cfxUpw = st2.cf * xot2 - st1.cf * xot1;
  // Fortran: BTMP = 2.0*HCA/HSA + 1.0 - HA - HWA
  // REZH = HLOG + BTMP*ULOG + XLOG*(0.5*CFX - DIX)
  const double btmp3 = 2.0 * hca / hsa + 1.0 - ha3 - hwa3;
  const double rezh = hlog + btmp3 * ulog + xlog * (0.5 * cfx3 - dix);
  // Fortran: Z_CFX = XLOG*0.5 ; Z_DIX = -XLOG ; Z_HCA = 2.0*ULOG/HSA
  // Z_HA = -ULOG ; Z_HWA = -ULOG
  // Z_XL = DDLOG*(0.5*CFX-DIX) ; Z_UL = DDLOG*BTMP ; Z_HL = DDLOG
  const double zCfx3 = xlog * 0.5;
  const double zDix = -xlog;
  const double zHca = 2.0 * ulog / hsa;
  const double zHa3 = -ulog;
  const double zHwa3 = -ulog;
  const double zXl3 = ddlog * (0.5 * cfx3 - dix);
  const double zUl3 = ddlog * btmp3;
  const double zHl = ddlog;
  // Fortran: Z_UPW = Z_CFX*CFX_UPW + Z_DIX*DIX_UPW
  const double zUpw3 = zCfx3 * cfxUpw + zDix * dixUpw;
  // Fortran: Z_HS1 = -HCA*ULOG/HSA**2 - Z_HL/HS1
  // Z_HS2 = -HCA*ULOG/HSA**2 + Z_HL/HS2
  const double zHs1 = -hca * ulog / (hsa * hsa) - zHl / st1.hs;
  const double zHs2 = -hca * ulog / (hsa * hsa) + zHl / st2.hs;
  // Fortran: Z_CF1/CF2/DI1/DI2
  const double zCf13 = (1.0 - upw) * zCfx3 * xot1;
  const double zCf23 = upw * zCfx3 * xot2;
  const double zDi1 = (1.0 - upw) * zDix * xot1;
  const double zDi2 = upw * zDix * xot2;
  // Fortran: Z_T1/T2/X1/X2/U1/U2
  const double zT13 = (1.0 - upw) * (zCfx3 * st1.cf + zDix * st1.di) *
                      (-xot1 / st1.t);
  const double zT23 = upw * (zCfx3 * st2.cf + zDix * st2.di) * (-xot2 / st2.t);
  const double zX13 = (1.0 - upw) * (zCfx3 * st1.cf + zDix * st1.di) / st1.t -
                      zXl3 / st1.x;
  const double zX23 = upw * (zCfx3 * st2.cf + zDix * st2.di) / st2.t +
                      zXl3 / st2.x;
  const double zU13 = -zUl3 / st1.ue;
  const double zU23 = zUl3 / st2.ue;
  // Fortran: Z_T1 += Z_HWA*0.5*(-DW1/T1**2) ; Z_T2 analogous
  const double zT13f = zT13 + zHwa3 * 0.5 * (-st1.dw / (st1.t * st1.t));
  const double zT23f = zT23 + zHwa3 * 0.5 * (-st2.dw / (st2.t * st2.t));
  // Fortran: VS1(3,1..5) ; VS2 analogous ; VSM(3)/VSR(3)
  out.vs1[2][0] = zDi1 * st1.diS;
  out.vs1[2][1] = zHs1 * st1.hsT + zCf13 * st1.cfT + zDi1 * st1.diT + zT13f;
  out.vs1[2][2] = zHs1 * st1.hsD + zCf13 * st1.cfD + zDi1 * st1.diD;
  out.vs1[2][3] = zHs1 * st1.hsU + zCf13 * st1.cfU + zDi1 * st1.diU + zU13;
  out.vs1[2][4] = zX13;
  out.vs2[2][0] = zDi2 * st2.diS;
  out.vs2[2][1] = zHs2 * st2.hsT + zCf23 * st2.cfT + zDi2 * st2.diT + zT23f;
  out.vs2[2][2] = zHs2 * st2.hsD + zCf23 * st2.cfD + zDi2 * st2.diD;
  out.vs2[2][3] = zHs2 * st2.hsU + zCf23 * st2.cfU + zDi2 * st2.diU + zU23;
  out.vs2[2][4] = zX23;
  out.vsm[2] = zHs1 * st1.hsMs + zCf13 * st1.cfMs + zDi1 * st1.diMs +
               zHs2 * st2.hsMs + zCf23 * st2.cfMs + zDi2 * st2.diMs;
  out.vsr[2] = zHs1 * st1.hsRe + zCf13 * st1.cfRe + zDi1 * st1.diRe +
               zHs2 * st2.hsRe + zCf23 * st2.cfRe + zDi2 * st2.diRe;
  // Fortran: VS1(3,2) += 0.5*(Z_HCA*HC1_T1+Z_HA*H1_T1) + Z_UPW*UPW_T1 etc.
  out.vs1[2][1] = out.vs1[2][1] + 0.5 * (zHca * st1.hcT + zHa3 * st1.hT) +
                  zUpw3 * upwT1;
  out.vs1[2][2] = out.vs1[2][2] + 0.5 * (zHca * st1.hcD + zHa3 * st1.hD) +
                  zUpw3 * upwD1;
  out.vs1[2][3] = out.vs1[2][3] + 0.5 * (zHca * st1.hcU) + zUpw3 * upwU1;
  out.vs2[2][1] = out.vs2[2][1] + 0.5 * (zHca * st2.hcT + zHa3 * st2.hT) +
                  zUpw3 * upwT2;
  out.vs2[2][2] = out.vs2[2][2] + 0.5 * (zHca * st2.hcD + zHa3 * st2.hD) +
                  zUpw3 * upwD2;
  out.vs2[2][3] = out.vs2[2][3] + 0.5 * (zHca * st2.hcU) + zUpw3 * upwU2;
  out.vsm[2] = out.vsm[2] + 0.5 * (zHca * st1.hcMs) + zUpw3 * upwMs +
               0.5 * (zHca * st2.hcMs);
  out.vsx[2] = 0.0;
  out.vsrez[2] = -rezh;
  return out;
}

namespace {

// Fortran: BLKIN exit or. BLDIF station -> BLMID inputs
  // (HK/RT/M per station incl. sensitivities).
BlmidStation blmidStationOf(const BoundaryLayerSecondary& sec) {
  BlmidStation m{};
  m.hk = sec.hk;
  m.hkU = sec.hkUe;
  m.hkT = sec.hkT;
  m.hkD = sec.hkD;
  m.hkMs = sec.hkMs;
  m.rt = sec.rt;
  m.rtU = sec.rtUe;
  m.rtT = sec.rtT;
  m.rtMs = sec.rtMs;
  m.rtRe = sec.rtRe;
  m.m = sec.m;
  m.mU = sec.mUe;
  m.mMs = sec.mMs;
  return m;
}

BlmidStation blmidStationOf(const BldifStation& st) {
  BlmidStation m{};
  m.hk = st.hk;
  m.hkU = st.hkU;
  m.hkT = st.hkT;
  m.hkD = st.hkD;
  m.hkMs = st.hkMs;
  m.rt = st.rt;
  m.rtU = st.rtU;
  m.rtT = st.rtT;
  m.rtMs = st.rtMs;
  m.rtRe = st.rtRe;
  m.m = st.m;
  m.mU = st.mU;
  m.mMs = st.mMs;
  return m;
}

// Fortran: BLPRV/BLKIN/BLVAR exit -> complete BLDIF
// station state.
BldifStation bldifStationOf(const BoundaryLayerPrimary& prim,
                            const BoundaryLayerSecondary& sec,
                            const BoundaryLayerTurbulentSecondary& ts) {
  BldifStation st{};
  st.x = prim.x;
  st.ue = prim.ue;
  st.t = prim.t;
  st.d = prim.d;
  st.s = prim.s;
  st.ampl = prim.ampl;
  st.dw = prim.dw;
  st.m = sec.m;
  st.mU = sec.mUe;
  st.mMs = sec.mMs;
  st.h = sec.h;
  st.hT = sec.hT;
  st.hD = sec.hD;
  st.hk = sec.hk;
  st.hkU = sec.hkUe;
  st.hkT = sec.hkT;
  st.hkD = sec.hkD;
  st.hkMs = sec.hkMs;
  st.rt = sec.rt;
  st.rtU = sec.rtUe;
  st.rtT = sec.rtT;
  st.rtMs = sec.rtMs;
  st.rtRe = sec.rtRe;
  st.hs = ts.hs;
  st.hsU = ts.hsU;
  st.hsT = ts.hsT;
  st.hsD = ts.hsD;
  st.hsMs = ts.hsMs;
  st.hsRe = ts.hsRe;
  st.hc = ts.hc;
  st.hcU = ts.hcU;
  st.hcT = ts.hcT;
  st.hcD = ts.hcD;
  st.hcMs = ts.hcMs;
  st.cf = ts.cf;
  st.cfU = ts.cfU;
  st.cfT = ts.cfT;
  st.cfD = ts.cfD;
  st.cfMs = ts.cfMs;
  st.cfRe = ts.cfRe;
  st.di = ts.di;
  st.diU = ts.diU;
  st.diT = ts.diT;
  st.diD = ts.diD;
  st.diS = ts.diS;
  st.diMs = ts.diMs;
  st.diRe = ts.diRe;
  st.us = ts.us;
  st.usU = ts.usU;
  st.usT = ts.usT;
  st.usD = ts.usD;
  st.usMs = ts.usMs;
  st.cq = ts.cq;
  st.cqU = ts.cqU;
  st.cqT = ts.cqT;
  st.cqD = ts.cqD;
  st.cqMs = ts.cqMs;
  st.cqRe = ts.cqRe;
  st.de = ts.de;
  st.deU = ts.deU;
  st.deT = ts.deT;
  st.deD = ts.deD;
  st.deMs = ts.deMs;
  return st;
}

}  // namespace

BldifSystem trdif(const TrdifInput& in) {
  const BldifStation& c1 = in.first;
  const BldifStation& c2 = in.second;

  // Fortran: C1SAV/C2SAV (xblsys.f:1209-1212) — in C++ Working copies;
  // COM1/COM2 be not changed.
  const BldifStation c1sav = c1;
  const BldifStation c2sav = c2;

  // Fortran: weight factors of the linear interpolation to the
  // transition point (xblsys.f:1215-1243).
  const double dx12 = c2sav.x - c1sav.x;
  const double wf2 = (in.xt - c1sav.x) / dx12;
  const double wf2Xt = 1.0 / dx12;
  const double wf2A1 = wf2Xt * in.xtA1;
  const double wf2X1 = wf2Xt * in.xtX1 + (wf2 - 1.0) / dx12;
  const double wf2X2 = wf2Xt * in.xtX2 - wf2 / dx12;
  const double wf2T1 = wf2Xt * in.xtT1;
  const double wf2T2 = wf2Xt * in.xtT2;
  const double wf2D1 = wf2Xt * in.xtD1;
  const double wf2D2 = wf2Xt * in.xtD2;
  const double wf2U1 = wf2Xt * in.xtU1;
  const double wf2U2 = wf2Xt * in.xtU2;
  const double wf2Ms = wf2Xt * in.xtMs;
  const double wf2Re = wf2Xt * in.xtRe;
  const double wf2Xf = wf2Xt * in.xtXf;
  const double wf1 = 1.0 - wf2;
  const double wf1A1 = -wf2A1;
  const double wf1X1 = -wf2X1;
  const double wf1X2 = -wf2X2;
  const double wf1T1 = -wf2T1;
  const double wf1T2 = -wf2T2;
  const double wf1D1 = -wf2D1;
  const double wf1D2 = -wf2D2;
  const double wf1U1 = -wf2U1;
  const double wf1U2 = -wf2U2;
  const double wf1Ms = -wf2Ms;
  const double wf1Re = -wf2Re;
  const double wf1Xf = -wf2Xf;

  // Fortran: interpolation of the primary variables to the transition
  // point (xblsys.f:1249-1289).
  const double tt = c1sav.t * wf1 + c2sav.t * wf2;
  const double ttA1 = c1sav.t * wf1A1 + c2sav.t * wf2A1;
  const double ttX1 = c1sav.t * wf1X1 + c2sav.t * wf2X1;
  const double ttX2 = c1sav.t * wf1X2 + c2sav.t * wf2X2;
  const double ttT1 = c1sav.t * wf1T1 + c2sav.t * wf2T1 + wf1;
  const double ttT2 = c1sav.t * wf1T2 + c2sav.t * wf2T2 + wf2;
  const double ttD1 = c1sav.t * wf1D1 + c2sav.t * wf2D1;
  const double ttD2 = c1sav.t * wf1D2 + c2sav.t * wf2D2;
  const double ttU1 = c1sav.t * wf1U1 + c2sav.t * wf2U1;
  const double ttU2 = c1sav.t * wf1U2 + c2sav.t * wf2U2;
  const double ttMs = c1sav.t * wf1Ms + c2sav.t * wf2Ms;
  const double ttRe = c1sav.t * wf1Re + c2sav.t * wf2Re;
  const double ttXf = c1sav.t * wf1Xf + c2sav.t * wf2Xf;

  const double dt = c1sav.d * wf1 + c2sav.d * wf2;
  const double dtA1 = c1sav.d * wf1A1 + c2sav.d * wf2A1;
  const double dtX1 = c1sav.d * wf1X1 + c2sav.d * wf2X1;
  const double dtX2 = c1sav.d * wf1X2 + c2sav.d * wf2X2;
  const double dtT1 = c1sav.d * wf1T1 + c2sav.d * wf2T1;
  const double dtT2 = c1sav.d * wf1T2 + c2sav.d * wf2T2;
  const double dtD1 = c1sav.d * wf1D1 + c2sav.d * wf2D1 + wf1;
  const double dtD2 = c1sav.d * wf1D2 + c2sav.d * wf2D2 + wf2;
  const double dtU1 = c1sav.d * wf1U1 + c2sav.d * wf2U1;
  const double dtU2 = c1sav.d * wf1U2 + c2sav.d * wf2U2;
  const double dtMs = c1sav.d * wf1Ms + c2sav.d * wf2Ms;
  const double dtRe = c1sav.d * wf1Re + c2sav.d * wf2Re;
  const double dtXf = c1sav.d * wf1Xf + c2sav.d * wf2Xf;

  const double ut = c1sav.ue * wf1 + c2sav.ue * wf2;
  const double utA1 = c1sav.ue * wf1A1 + c2sav.ue * wf2A1;
  const double utX1 = c1sav.ue * wf1X1 + c2sav.ue * wf2X1;
  const double utX2 = c1sav.ue * wf1X2 + c2sav.ue * wf2X2;
  const double utT1 = c1sav.ue * wf1T1 + c2sav.ue * wf2T1;
  const double utT2 = c1sav.ue * wf1T2 + c2sav.ue * wf2T2;
  const double utD1 = c1sav.ue * wf1D1 + c2sav.ue * wf2D1;
  const double utD2 = c1sav.ue * wf1D2 + c2sav.ue * wf2D2;
  const double utU1 = c1sav.ue * wf1U1 + c2sav.ue * wf2U1 + wf1;
  const double utU2 = c1sav.ue * wf1U2 + c2sav.ue * wf2U2 + wf2;
  const double utMs = c1sav.ue * wf1Ms + c2sav.ue * wf2Ms;
  const double utRe = c1sav.ue * wf1Re + c2sav.ue * wf2Re;
  const double utXf = c1sav.ue * wf1Xf + c2sav.ue * wf2Xf;

  // Fortran: primary "T" variables at the transition point (xblsys.f:1291-
  // 1298) — X2=XT, T2=TT, D2=DT, U2=UT, AMPL2=AMCRIT, S2=0. In C++ this is
  // a separate T state (COM2 stays unchanged). U2_UEI/U2_MS do not enter
  // TRDIF and remain 0.
  BoundaryLayerPrimary tPrim{};
  tPrim.x = in.xt;
  tPrim.t = tt;
  tPrim.d = dt;
  tPrim.ue = ut;
  tPrim.s = 0.0;
  tPrim.ampl = in.amcrit;
  tPrim.dw = c2sav.dw;  // Fortran DW2 unchanged
  const BoundaryLayerSecondary tSec = blkin(tPrim, in.props);
  const BlvarInput tVin{tPrim, tSec, in.blpar};

  // Fortran: laminar secondary "T" variables (xblsys.f:1301-1302).
  const BoundaryLayerTurbulentSecondary tTurbLam =
      blvar(StationType::kLaminar, tVin);

  // Fortran: X1-XT medium point-CFM (xblsys.f:1305).
  BlmidInput mi1{};
  mi1.first = blmidStationOf(c1sav);
  mi1.second = blmidStationOf(tSec);
  mi1.simi = false;
  mi1.blpar = in.blpar;
  const MidpointSkinFriction mid1 = blmid(StationType::kLaminar, mi1);

  // Fortran: Newton system for X1..XT (xblsys.f:1311).
  BldifInput bi1{};
  bi1.first = c1sav;
  bi1.second = bldifStationOf(tPrim, tSec, tTurbLam);
  bi1.mid = mid1;
  bi1.ityp = StationType::kLaminar;
  bi1.similarity = false;
  bi1.ampl2 = in.amcrit;
  bi1.acrit = in.amcrit;
  bi1.idampv = in.idampv;
  bi1.blpar = in.blpar;
  const BldifSystem sysLam = bldif(bi1);

  // Fortran: Conversion the Residual sensitivities wrt "T"-
  // Variables in "1"/"2"-Variables (xblsys.f:1318-1380). Line K=1
  // stays empty (Amplification equation in the transition interval unnecessary).
  double blrez[4]{};
  double blm[4]{};
  double blr[4]{};
  double blx[4]{};
  double bl1[4][5]{};
  double bl2[4][5]{};
  for (int k = 2; k <= 3; ++k) {
    const int i = k - 1;  // Fortran 1-based -> C++ 0-based
    blrez[i] = sysLam.vsrez[i];
    blm[i] = sysLam.vsm[i]
           + sysLam.vs2[i][1] * ttMs + sysLam.vs2[i][2] * dtMs
           + sysLam.vs2[i][3] * utMs + sysLam.vs2[i][4] * in.xtMs;
    blr[i] = sysLam.vsr[i]
           + sysLam.vs2[i][1] * ttRe + sysLam.vs2[i][2] * dtRe
           + sysLam.vs2[i][3] * utRe + sysLam.vs2[i][4] * in.xtRe;
    blx[i] = sysLam.vsx[i]
           + sysLam.vs2[i][1] * ttXf + sysLam.vs2[i][2] * dtXf
           + sysLam.vs2[i][3] * utXf + sysLam.vs2[i][4] * in.xtXf;
    bl1[i][0] = sysLam.vs1[i][0]
              + sysLam.vs2[i][1] * ttA1 + sysLam.vs2[i][2] * dtA1
              + sysLam.vs2[i][3] * utA1 + sysLam.vs2[i][4] * in.xtA1;
    bl1[i][1] = sysLam.vs1[i][1]
              + sysLam.vs2[i][1] * ttT1 + sysLam.vs2[i][2] * dtT1
              + sysLam.vs2[i][3] * utT1 + sysLam.vs2[i][4] * in.xtT1;
    bl1[i][2] = sysLam.vs1[i][2]
              + sysLam.vs2[i][1] * ttD1 + sysLam.vs2[i][2] * dtD1
              + sysLam.vs2[i][3] * utD1 + sysLam.vs2[i][4] * in.xtD1;
    bl1[i][3] = sysLam.vs1[i][3]
              + sysLam.vs2[i][1] * ttU1 + sysLam.vs2[i][2] * dtU1
              + sysLam.vs2[i][3] * utU1 + sysLam.vs2[i][4] * in.xtU1;
    bl1[i][4] = sysLam.vs1[i][4]
              + sysLam.vs2[i][1] * ttX1 + sysLam.vs2[i][2] * dtX1
              + sysLam.vs2[i][3] * utX1 + sysLam.vs2[i][4] * in.xtX1;
    bl2[i][0] = 0.0;
    bl2[i][1] = sysLam.vs2[i][1] * ttT2 + sysLam.vs2[i][2] * dtT2
              + sysLam.vs2[i][3] * utT2 + sysLam.vs2[i][4] * in.xtT2;
    bl2[i][2] = sysLam.vs2[i][1] * ttD2 + sysLam.vs2[i][2] * dtD2
              + sysLam.vs2[i][3] * utD2 + sysLam.vs2[i][4] * in.xtD2;
    bl2[i][3] = sysLam.vs2[i][1] * ttU2 + sysLam.vs2[i][2] * dtU2
              + sysLam.vs2[i][3] * utU2 + sysLam.vs2[i][4] * in.xtU2;
    bl2[i][4] = sysLam.vs2[i][1] * ttX2 + sysLam.vs2[i][2] * dtX2
              + sysLam.vs2[i][3] * utX2 + sysLam.vs2[i][4] * in.xtX2;
  }

  // Fortran: equilibrium shear CQT at the transition point (xblsys.f:1386)
  // — BLVAR(2) with the T state (S2=0, AMPL2=AMCRIT).
  const BoundaryLayerTurbulentSecondary tTurbEq =
      blvar(StationType::kTurbulent, tVin);

  // Fortran: CTR (xblsys.f:1391-1392).
  const double hk2T = tSec.hk;
  const double ctr =
      in.blpar.ctrcon * std::exp(-in.blpar.ctrcex / (hk2T - 1.0));
  const double ctrHk2 =
      ctr * in.blpar.ctrcex / ((hk2T - 1.0) * (hk2T - 1.0));

  // Fortran: ST at the transition point (xblsys.f:1401-1406). CQ2_* are
  // "CQT_*" (wrt TT/DT/UT).
  const double st = ctr * tTurbEq.cq;
  const double stTt = ctr * tTurbEq.cqT + tTurbEq.cq * ctrHk2 * tSec.hkT;
  const double stDt = ctr * tTurbEq.cqD + tTurbEq.cq * ctrHk2 * tSec.hkD;
  const double stUt = ctr * tTurbEq.cqU + tTurbEq.cq * ctrHk2 * tSec.hkUe;
  double stMs = ctr * tTurbEq.cqMs + tTurbEq.cq * ctrHk2 * tSec.hkMs;
  const double stReT = ctr * tTurbEq.cqRe;

  // Fortran: ST sensitivities wrt the "1"/"2"-Variables
  // (xblsys.f:1409-1420). ST_MS/ST_Re accumulate (Fortran idiom).
  const double stA1 = stTt * ttA1 + stDt * dtA1 + stUt * utA1;
  const double stX1 = stTt * ttX1 + stDt * dtX1 + stUt * utX1;
  const double stX2 = stTt * ttX2 + stDt * dtX2 + stUt * utX2;
  const double stT1 = stTt * ttT1 + stDt * dtT1 + stUt * utT1;
  const double stT2 = stTt * ttT2 + stDt * dtT2 + stUt * utT2;
  const double stD1 = stTt * ttD1 + stDt * dtD1 + stUt * utD1;
  const double stD2 = stTt * ttD2 + stDt * dtD2 + stUt * utD2;
  const double stU1 = stTt * ttU1 + stDt * dtU1 + stUt * utU1;
  const double stU2 = stTt * ttU2 + stDt * dtU2 + stUt * utU2;
  stMs = stTt * ttMs + stDt * dtMs + stUt * utMs + stMs;
  const double stRe = stTt * ttRe + stDt * dtRe + stUt * utRe + stReT;
  const double stXf = stTt * ttXf + stDt * dtXf + stUt * utXf;

  // Fortran: turbulent secondary "T"-Variables with proper CTI
  // (xblsys.f:1422-1426) — AMPL2=0, S2=ST.
  tPrim.s = st;
  tPrim.ampl = 0.0;
  const BlvarInput tVin2{tPrim, tSec, in.blpar};
  const BoundaryLayerTurbulentSecondary tTurbTur =
      blvar(StationType::kTurbulent, tVin2);

  // Fortran: COM1=COM2 ("1" is T state), COM2=C2SAV (xblsys.f:1430-
  // 1433).
  const BldifStation tStation = bldifStationOf(tPrim, tSec, tTurbTur);

  // Fortran: XT-X2 medium point-CFM (xblsys.f:1436).
  BlmidInput mi2{};
  mi2.first = blmidStationOf(tStation);
  mi2.second = blmidStationOf(c2sav);
  mi2.simi = false;
  mi2.blpar = in.blpar;
  const MidpointSkinFriction mid2 = blmid(StationType::kTurbulent, mi2);

  // Fortran: Newton system for XT..X2 (xblsys.f:1439).
  BldifInput bi2{};
  bi2.first = tStation;
  bi2.second = c2sav;
  bi2.mid = mid2;
  bi2.ityp = StationType::kTurbulent;
  bi2.similarity = false;
  bi2.blpar = in.blpar;
  const BldifSystem sysTur = bldif(bi2);

  // Fortran: Conversion the Residual sensitivities wrt "T"-
  // Variables in "1"/"2"-Variables (xblsys.f:1443-1516).
  double btrez[4]{};
  double btm[4]{};
  double btr[4]{};
  double btx[4]{};
  double bt1[4][5]{};
  double bt2[4][5]{};
  for (int k = 1; k <= 3; ++k) {
    const int i = k - 1;
    btrez[i] = sysTur.vsrez[i];
    btm[i] = sysTur.vsm[i]
           + sysTur.vs1[i][0] * stMs
           + sysTur.vs1[i][1] * ttMs + sysTur.vs1[i][2] * dtMs
           + sysTur.vs1[i][3] * utMs + sysTur.vs1[i][4] * in.xtMs;
    btr[i] = sysTur.vsr[i]
           + sysTur.vs1[i][0] * stRe
           + sysTur.vs1[i][1] * ttRe + sysTur.vs1[i][2] * dtRe
           + sysTur.vs1[i][3] * utRe + sysTur.vs1[i][4] * in.xtRe;
    btx[i] = sysTur.vsx[i]
           + sysTur.vs1[i][0] * stXf
           + sysTur.vs1[i][1] * ttXf + sysTur.vs1[i][2] * dtXf
           + sysTur.vs1[i][3] * utXf + sysTur.vs1[i][4] * in.xtXf;
    bt1[i][0] = sysTur.vs1[i][0] * stA1
              + sysTur.vs1[i][1] * ttA1 + sysTur.vs1[i][2] * dtA1
              + sysTur.vs1[i][3] * utA1 + sysTur.vs1[i][4] * in.xtA1;
    bt1[i][1] = sysTur.vs1[i][0] * stT1
              + sysTur.vs1[i][1] * ttT1 + sysTur.vs1[i][2] * dtT1
              + sysTur.vs1[i][3] * utT1 + sysTur.vs1[i][4] * in.xtT1;
    bt1[i][2] = sysTur.vs1[i][0] * stD1
              + sysTur.vs1[i][1] * ttD1 + sysTur.vs1[i][2] * dtD1
              + sysTur.vs1[i][3] * utD1 + sysTur.vs1[i][4] * in.xtD1;
    bt1[i][3] = sysTur.vs1[i][0] * stU1
              + sysTur.vs1[i][1] * ttU1 + sysTur.vs1[i][2] * dtU1
              + sysTur.vs1[i][3] * utU1 + sysTur.vs1[i][4] * in.xtU1;
    bt1[i][4] = sysTur.vs1[i][0] * stX1
              + sysTur.vs1[i][1] * ttX1 + sysTur.vs1[i][2] * dtX1
              + sysTur.vs1[i][3] * utX1 + sysTur.vs1[i][4] * in.xtX1;
    bt2[i][0] = sysTur.vs2[i][0];
    bt2[i][1] = sysTur.vs2[i][1]
              + sysTur.vs1[i][0] * stT2
              + sysTur.vs1[i][1] * ttT2 + sysTur.vs1[i][2] * dtT2
              + sysTur.vs1[i][3] * utT2 + sysTur.vs1[i][4] * in.xtT2;
    bt2[i][2] = sysTur.vs2[i][2]
              + sysTur.vs1[i][0] * stD2
              + sysTur.vs1[i][1] * ttD2 + sysTur.vs1[i][2] * dtD2
              + sysTur.vs1[i][3] * utD2 + sysTur.vs1[i][4] * in.xtD2;
    bt2[i][3] = sysTur.vs2[i][3]
              + sysTur.vs1[i][0] * stU2
              + sysTur.vs1[i][1] * ttU2 + sysTur.vs1[i][2] * dtU2
              + sysTur.vs1[i][3] * utU2 + sysTur.vs1[i][4] * in.xtU2;
    bt2[i][4] = sysTur.vs2[i][4]
              + sysTur.vs1[i][0] * stX2
              + sysTur.vs1[i][1] * ttX2 + sysTur.vs1[i][2] * dtX2
              + sysTur.vs1[i][3] * utX2 + sysTur.vs1[i][4] * in.xtX2;
  }

  // Fortran: addition of the laminar and turbulent parts to the
  // final system (xblsys.f:1520-1539).
  BldifSystem out{};
  out.vsrez[0] = btrez[0];
  out.vsrez[1] = blrez[1] + btrez[1];
  out.vsrez[2] = blrez[2] + btrez[2];
  out.vsm[0] = btm[0];
  out.vsm[1] = blm[1] + btm[1];
  out.vsm[2] = blm[2] + btm[2];
  out.vsr[0] = btr[0];
  out.vsr[1] = blr[1] + btr[1];
  out.vsr[2] = blr[2] + btr[2];
  out.vsx[0] = btx[0];
  out.vsx[1] = blx[1] + btx[1];
  out.vsx[2] = blx[2] + btx[2];
  for (int l = 0; l < 5; ++l) {
    out.vs1[0][l] = bt1[0][l];
    out.vs2[0][l] = bt2[0][l];
    out.vs1[1][l] = bl1[1][l] + bt1[1][l];
    out.vs2[1][l] = bl2[1][l] + bt2[1][l];
    out.vs1[2][l] = bl1[2][l] + bt1[2][l];
    out.vs2[2][l] = bl2[2][l] + bt2[2][l];
  }
  return out;
}

}  // namespace xfoil::core::boundary_layer

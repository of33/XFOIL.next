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

// File: src/core/boundary_layer_solver/BoundaryLayerCorrelations.cpp
// Implementation of the stateless boundary-layer correlation and
// station kernels. Each function is a
// formula-faithful adoption of the respective Fortran routine from
// fortran-src/xblsys.f; constants, branch conditions and the
// order of computations are unchanged.

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"

#include <cmath>

namespace xfoil::core::boundary_layer {

// Fortran: xblsys.f:2276 HKIN(H, MSQ, HK, HK_H, HK_MSQ)
KinematicShapeValue hkin(double h, double msq) {
  KinematicShapeValue out{};
  // Fortran: HK = (H - 0.29*MSQ)/(1.0 + 0.113*MSQ)
  out.value = (h - 0.29 * msq) / (1.0 + 0.113 * msq);
  // Fortran: HK_H = 1.0 /(1.0 + 0.113*MSQ)
  out.shapeDerivative = 1.0 / (1.0 + 0.113 * msq);
  // Fortran: HK_MSQ = (-.29 - 0.113*HK)/(1.0 + 0.113*MSQ)
  out.machSquaredDerivative =
      (-0.29 - 0.113 * out.value) / (1.0 + 0.113 * msq);
  return out;
}

// Fortran: xblsys.f:2290 DIL(HK, RT, DI, DI_HK, DI_RT)
CorrelationValue dil(double hk, double rt) {
  CorrelationValue out{};
  if (hk < 4.0) {
 // Fortran: DI = (0.00205 * (4.0-HK)**5.5 + 0.207) / RT
    out.value = (0.00205 * std::pow(4.0 - hk, 5.5) + 0.207) / rt;
 // Fortran: DI_HK = (-.00205*5.5*(4.0-HK)**4.5) / RT
    out.shapeDerivative = (-0.00205 * 5.5 * std::pow(4.0 - hk, 4.5)) / rt;
  } else {
    // Fortran binds ** more strongly than *: HKB**2 = HKB*HKB is computed
    // first (canonical Fortran evaluation, xblsys.f:2298-2301; confirmed by
    // the direct DIASSOC Fortran comparison).
    const double hkbSq = (hk - 4.0) * (hk - 4.0);
    const double den = 1.0 + 0.02 * hkbSq;
 // Fortran: DI = (-.0016 * HKB**2 / DEN + 0.207) / RT
    out.value = (-0.0016 * hkbSq / den + 0.207) / rt;
 // Fortran: DI_HK = (-.0016*2.0*HKB*(1.0/DEN - 0.02*HKB**2/DEN**2)) / RT
    out.shapeDerivative =
        (-0.0016 * 2.0 * (hk - 4.0) *
         (1.0 / den - 0.02 * hkbSq / (den * den))) /
        rt;
  }
  // Fortran: DI_RT = -DI/RT
  out.reynoldsDerivative = -out.value / rt;
  out.machSquaredDerivative = 0.0;
  return out;
}

// Fortran: xblsys.f:2308 DILW(HK, RT, DI, DI_HK, DI_RT)
CorrelationValue dilw(double hk, double rt) {
  // Fortran: MSQ = 0.
 // CALL HSL (HK, RT, MSQ, HS, HS_HK, HS_RT, HS_MSQ)
  const CorrelationValue hsOut = hsl(hk, rt);
  const double hs = hsOut.value;
  const double hsHk = hsOut.shapeDerivative;
  const double hsRt = hsOut.reynoldsDerivative;
  // Fortran: RCD = 1.10 * (1.0 - 1.0/HK)**2 / HK
  // (Fortran ** binds more strongly than *: the power is computed first)
  const double invHkFac = 1.0 - 1.0 / hk;
  const double rcd = 1.10 * (invHkFac * invHkFac) / hk;
  // Fortran: RCD_HK = -1.10 * (1.0 - 1.0/HK)*2.0 / HK**3
  // & - RCD/HK
  const double rcdHk =
      -1.10 * (1.0 - 1.0 / hk) * 2.0 / (hk * hk * hk) - rcd / hk;
  CorrelationValue out{};
  // Fortran: DI = 2.0*RCD /(HS*RT)
  out.value = 2.0 * rcd / (hs * rt);
  // Fortran: DI_HK = 2.0*RCD_HK/(HS*RT) - (DI/HS)*HS_HK
  out.shapeDerivative =
      2.0 * rcdHk / (hs * rt) - (out.value / hs) * hsHk;
  // Fortran: DI_RT = -DI/RT - (DI/HS)*HS_RT
  out.reynoldsDerivative = -out.value / rt - (out.value / hs) * hsRt;
  out.machSquaredDerivative = 0.0;
  return out;
}

// Fortran: xblsys.f:2327 HSL(HK, RT, MSQ, HS, HS_HK, HS_RT, HS_MSQ)
// Note: RT is unused in the Fortran routine
// (canonical HS_RT=HS_MSQ=0); it remains in the signature.
CorrelationValue hsl(double hk, [[maybe_unused]] double rt) {
  CorrelationValue out{};
  if (hk < 4.35) {
    const double tmp = hk - 4.35;
    // Fortran binds ** more strongly than *: TMP**2/TMP**3/(TMP*HK)**2 are
    // computed first (canonical Fortran evaluation, xblsys.f:2334-2344).
    const double tmpSq = tmp * tmp;              // Fortran TMP**2
    const double tmpCube = tmpSq * tmp;          // Fortran TMP**3
    const double tmpHkSq = (tmp * hk) * (tmp * hk);  // Fortran (TMP*HK)**2
    // Fortran: HS = 0.0111*TMP**2/(HK+1.0)
    // & - 0.0278*TMP**3/(HK+1.0) + 1.528
    // & - 0.0002*(TMP*HK)**2
    out.value = 0.0111 * tmpSq / (hk + 1.0) -
                0.0278 * tmpCube / (hk + 1.0) + 1.528 -
                0.0002 * tmpHkSq;
    // Fortran: HS_HK = 0.0111*(2.0*TMP - TMP**2/(HK+1.0))/(HK+1.0)
    // & - 0.0278*(3.0*TMP**2 - TMP**3/(HK+1.0))/(HK+1.0)
    // & - 0.0002*2.0*TMP*HK * (TMP + HK)
    out.shapeDerivative =
        0.0111 * (2.0 * tmp - tmpSq / (hk + 1.0)) / (hk + 1.0) -
        0.0278 * (3.0 * tmpSq - tmpCube / (hk + 1.0)) / (hk + 1.0) -
        0.0002 * 2.0 * tmp * hk * (tmp + hk);
  } else {
    const double hs2 = 0.015;
    // Fortran binds ** more strongly than *: (HK-4.35)**2 is computed
    // first (canonical Fortran evaluation, xblsys.f:2347-2350).
    const double hkShiftSq = (hk - 4.35) * (hk - 4.35);
    // Fortran: HS = HS2* (HK-4.35)**2/HK + 1.528
    out.value = hs2 * hkShiftSq / hk + 1.528;
    // Fortran: HS_HK = HS2*2.0*(HK-4.35) /HK
    // & - HS2* (HK-4.35)**2/HK**2
    out.shapeDerivative = hs2 * 2.0 * (hk - 4.35) / hk -
                          hs2 * hkShiftSq / (hk * hk);
  }
  // Fortran: HS_RT = 0.
  // HS_MSQ = 0.
  out.reynoldsDerivative = 0.0;
  out.machSquaredDerivative = 0.0;
  return out;
}

// Fortran: xblsys.f:2354 CFL(HK, RT, MSQ, CF, CF_HK, CF_RT, CF_MSQ)
CorrelationValue cfl(double hk, double rt) {
  CorrelationValue out{};
  if (hk < 5.5) {
    // Fortran: TMP = (5.5-HK)**3 / (HK+1.0)
    const double tmp = (5.5 - hk) * (5.5 - hk) * (5.5 - hk) / (hk + 1.0);
 // Fortran: CF = (0.0727*TMP - 0.07)/RT
    out.value = (0.0727 * tmp - 0.07) / rt;
 // Fortran: CF_HK = (-.0727*TMP*3.0/(5.5-HK) - 0.0727*TMP/(HK+1.0))/RT
    out.shapeDerivative =
        (-0.0727 * tmp * 3.0 / (5.5 - hk) - 0.0727 * tmp / (hk + 1.0)) / rt;
  } else {
    // Fortran: TMP = 1.0 - 1.0/(HK-4.5)
    const double tmp = 1.0 - 1.0 / (hk - 4.5);
 // Fortran: CF = (0.015*TMP**2 - 0.07) / RT
    // (Fortran ** binds more strongly than *: TMP**2 first)
    out.value = (0.015 * (tmp * tmp) - 0.07) / rt;
 // Fortran: CF_HK = (0.015*TMP*2.0/(HK-4.5)**2) / RT
    out.shapeDerivative =
        (0.015 * tmp * 2.0 / ((hk - 4.5) * (hk - 4.5))) / rt;
  }
  // Fortran: CF_RT = -CF/RT
  out.reynoldsDerivative = -out.value / rt;
  // Fortran: CF_MSQ = 0.0
  out.machSquaredDerivative = 0.0;
  return out;
}

// Fortran: xblsys.f:2375 DIT(HS, US, CF, ST, DI, DI_HS, DI_US, DI_CF, DI_ST)
TurbulentDissipationValue dit(double hs, double us, double cf, double st) {
  TurbulentDissipationValue out{};
 // Fortran: DI = (0.5*CF*US + ST*ST*(1.0-US)) * 2.0/HS
  out.value = (0.5 * cf * us + st * st * (1.0 - us)) * 2.0 / hs;
 // Fortran: DI_HS = - (0.5*CF*US + ST*ST*(1.0-US)) * 2.0/HS**2
  out.energyShapeDerivative =
      -(0.5 * cf * us + st * st * (1.0 - us)) * 2.0 / (hs * hs);
 // Fortran: DI_US = (0.5*CF - ST*ST) * 2.0/HS
  out.slipVelocityDerivative = (0.5 * cf - st * st) * 2.0 / hs;
 // Fortran: DI_CF = (0.5 *US) * 2.0/HS
  out.skinFrictionDerivative = (0.5 * us) * 2.0 / hs;
 // Fortran: DI_ST = (2.0*ST*(1.0-US)) * 2.0/HS
  out.shearCoefficientDerivative = (2.0 * st * (1.0 - us)) * 2.0 / hs;
  return out;
}

// Fortran: xblsys.f:2388 HST(HK, RT, MSQ, HS, HS_HK, HS_RT, HS_MSQ)
CorrelationValue hst(double hk, double rt, double msq) {
  // Fortran: DATA HSMIN, DHSINF / 1.500, 0.015 /
  const double hsmin = 1.500;
  const double dhsinf = 0.015;
  double ho = 0.0;
  double hoRt = 0.0;
  // Fortran: IF(RT.GT.400.0) THEN
  // HO = 3.0 + 400.0/RT
  // HO_RT = - 400.0/RT**2
  // ELSE
  // HO = 4.0
  // HO_RT = 0.
  // ENDIF
  if (rt > 400.0) {
    ho = 3.0 + 400.0 / rt;
    hoRt = -400.0 / (rt * rt);
  } else {
    ho = 4.0;
    hoRt = 0.0;
  }
  double rtz = 0.0;
  double rtzRt = 0.0;
  // Fortran: IF(RT.GT.200.0) THEN
  // RTZ = RT
  // RTZ_RT = 1.
  // ELSE
  // RTZ = 200.0
  // RTZ_RT = 0.
  // ENDIF
  if (rt > 200.0) {
    rtz = rt;
    rtzRt = 1.0;
  } else {
    rtz = 200.0;
    rtzRt = 0.0;
  }
  CorrelationValue out{};
  if (hk < ho) {
 // Fortran: HR = (HO - HK)/(HO-1.0)
    // HR_HK = - 1.0/(HO-1.0)
    // HR_RT = (1.0 - HR)/(HO-1.0) * HO_RT
    const double hr = (ho - hk) / (ho - 1.0);
    const double hrHk = -1.0 / (ho - 1.0);
    const double hrRt = (1.0 - hr) / (ho - 1.0) * hoRt;
    // Fortran binds ** more strongly than *: HR**2 is computed first
    // (canonical Fortran evaluation, xblsys.f:2401-2413).
    const double hrSq = hr * hr;  // Fortran HR**2
    // Fortran: HS = (2.0-HSMIN-4.0/RTZ)*HR**2 * 1.5/(HK+0.5) + HSMIN
    // & + 4.0/RTZ
    out.value = (2.0 - hsmin - 4.0 / rtz) * hrSq * 1.5 / (hk + 0.5) +
                hsmin + 4.0 / rtz;
    // Fortran: HS_HK =-(2.0-HSMIN-4.0/RTZ)*HR**2 * 1.5/(HK+0.5)**2
    // & + (2.0-HSMIN-4.0/RTZ)*HR*2.0 * 1.5/(HK+0.5) * HR_HK
    out.shapeDerivative =
        -(2.0 - hsmin - 4.0 / rtz) * hrSq * 1.5 / ((hk + 0.5) * (hk + 0.5)) +
        (2.0 - hsmin - 4.0 / rtz) * hr * 2.0 * 1.5 / (hk + 0.5) * hrHk;
    // Fortran: HS_RT = (2.0-HSMIN-4.0/RTZ)*HR*2.0 * 1.5/(HK+0.5) * HR_RT
    // & + (HR**2 * 1.5/(HK+0.5) - 1.0)*4.0/RTZ**2 * RTZ_RT
    out.reynoldsDerivative =
        (2.0 - hsmin - 4.0 / rtz) * hr * 2.0 * 1.5 / (hk + 0.5) * hrRt +
        (hrSq * 1.5 / (hk + 0.5) - 1.0) * 4.0 / (rtz * rtz) * rtzRt;
  } else {
    // Fortran: GRT = LOG(RTZ)
    // HDIF = HK - HO
    // RTMP = HK - HO + 4.0/GRT
    const double grt = std::log(rtz);
    const double hdif = hk - ho;
    const double rtmp = hk - ho + 4.0 / grt;
    // Fortran: HTMP = 0.007*GRT/RTMP**2 + DHSINF/HK
    // HTMP_HK = -.014*GRT/RTMP**3 - DHSINF/HK**2
    // HTMP_RT = -.014*GRT/RTMP**3 * (-HO_RT - 4.0/GRT**2/RTZ * RTZ_RT)
    // & + 0.007 /RTMP**2 / RTZ * RTZ_RT
    const double htmp = 0.007 * grt / (rtmp * rtmp) + dhsinf / hk;
    const double htmpHk =
        -0.014 * grt / (rtmp * rtmp * rtmp) - dhsinf / (hk * hk);
    const double htmpRt =
        -0.014 * grt / (rtmp * rtmp * rtmp) *
            (-hoRt - 4.0 / (grt * grt) / rtz * rtzRt) +
        0.007 / (rtmp * rtmp) / rtz * rtzRt;
    // Fortran: HS = HDIF**2 * HTMP + HSMIN + 4.0/RTZ
    out.value = hdif * hdif * htmp + hsmin + 4.0 / rtz;
    // Fortran: HS_HK = HDIF*2.0* HTMP
    // & + HDIF**2 * HTMP_HK
    out.shapeDerivative = hdif * 2.0 * htmp + hdif * hdif * htmpHk;
    // Fortran: HS_RT = HDIF**2 * HTMP_RT - 4.0/RTZ**2 * RTZ_RT
    // & + HDIF*2.0* HTMP * (-HO_RT)
    out.reynoldsDerivative =
        hdif * hdif * htmpRt - 4.0 / (rtz * rtz) * rtzRt +
        hdif * 2.0 * htmp * (-hoRt);
  }
  // Fortran: FM = 1.0 + 0.014*MSQ
 // HS = (HS + 0.028*MSQ) / FM
 // HS_HK = (HS_HK) / FM
 // HS_RT = (HS_RT) / FM
  // HS_MSQ = 0.028/FM - 0.014*HS/FM
  const double fm = 1.0 + 0.014 * msq;
  out.value = (out.value + 0.028 * msq) / fm;
  out.shapeDerivative = out.shapeDerivative / fm;
  out.reynoldsDerivative = out.reynoldsDerivative / fm;
  out.machSquaredDerivative = 0.028 / fm - 0.014 * out.value / fm;
  return out;
}

// Fortran: xblsys.f:2483 CFT(HK, RT, MSQ, CF, CF_HK, CF_RT, CF_MSQ)
CorrelationValue cft(double hk, double rt, double msq, double cffac) {
  // Fortran: DATA GAM /1.4/
  // GM1 = GAM - 1.0
  const double gam = 1.4;
  const double gm1 = gam - 1.0;
  // Fortran: FC = SQRT(1.0 + 0.5*GM1*MSQ)
  const double fc = std::sqrt(1.0 + 0.5 * gm1 * msq);
  // Fortran: GRT = LOG(RT/FC)
  // GRT = MAX(GRT,3.0)
  double grt = std::log(rt / fc);
  grt = std::max(grt, 3.0);
  // Fortran: GEX = -1.74 - 0.31*HK
  const double gex = -1.74 - 0.31 * hk;
  // Fortran: ARG = -1.33*HK
 // ARG = MAX(-20.0, ARG)
  double arg = -1.33 * hk;
  arg = std::max(-20.0, arg);
  // Fortran: THK = TANH(4.0 - HK/0.875)
  const double thk = std::tanh(4.0 - hk / 0.875);
  // Fortran: CFO = CFFAC * 0.3*EXP(ARG) * (GRT/2.3026)**GEX
  const double cfo = cffac * 0.3 * std::exp(arg) *
                     std::pow(grt / 2.3026, gex);
  CorrelationValue out{};
 // Fortran: CF = (CFO + 1.1E-4*(THK-1.0)) / FC
  out.value = (cfo + 1.1e-4 * (thk - 1.0)) / fc;
  // Fortran: CF_HK = (-1.33*CFO - 0.31*LOG(GRT/2.3026)*CFO
 // & - 1.1E-4*(1.0-THK**2) / 0.875) / FC
  out.shapeDerivative =
      (-1.33 * cfo - 0.31 * std::log(grt / 2.3026) * cfo -
       1.1e-4 * (1.0 - thk * thk) / 0.875) /
      fc;
  // Fortran: CF_RT = GEX*CFO/(FC*GRT) / RT
  out.reynoldsDerivative = gex * cfo / (fc * grt) / rt;
  // Fortran: CF_MSQ = GEX*CFO/(FC*GRT) * (-0.25*GM1/FC**2) - 0.25*GM1*CF/FC**2
  out.machSquaredDerivative = gex * cfo / (fc * grt) * (-0.25 * gm1 / (fc * fc)) -
                              0.25 * gm1 * out.value / (fc * fc);
  return out;
}

// Fortran: xblsys.f:2514 HCT(HK, MSQ, HC, HC_HK, HC_MSQ)
CorrelationValue hct(double hk, double msq) {
  CorrelationValue out{};
  // Fortran: HC = MSQ * (0.064/(HK-0.8) + 0.251)
  out.value = msq * (0.064 / (hk - 0.8) + 0.251);
 // Fortran: HC_HK = MSQ * (-.064/(HK-0.8)**2)
  out.shapeDerivative = msq * (-0.064 / ((hk - 0.8) * (hk - 0.8)));
  // Fortran: HC_MSQ = 0.064/(HK-0.8) + 0.251
  out.machSquaredDerivative = 0.064 / (hk - 0.8) + 0.251;
  out.reynoldsDerivative = 0.0;
  return out;
}

// Fortran: xblsys.f:1981 DAMPL(HK, TH, RT, AX, AX_HK, AX_TH, AX_RT)
AmplificationRateValue dampl(double hk, double th, double rt) {
  // Fortran: DATA DGR / 0.08 /
  const double dgr = 0.08;
  // Fortran: HMI = 1.0/(HK - 1.0)
  // HMI_HK = -HMI**2
  const double hmi = 1.0 / (hk - 1.0);
  const double hmiHk = -hmi * hmi;
  // Fortran: AA = 2.492*HMI**0.43
  // AA_HK = (AA/HMI)*0.43 * HMI_HK
  const double aa = 2.492 * std::pow(hmi, 0.43);
  const double aaHk = (aa / hmi) * 0.43 * hmiHk;
  // Fortran: BB = TANH(14.0*HMI - 9.24)
  // BB_HK = (1.0 - BB*BB) * 14.0 * HMI_HK
  const double bb = std::tanh(14.0 * hmi - 9.24);
  const double bbHk = (1.0 - bb * bb) * 14.0 * hmiHk;
  // Fortran: GRCRIT = AA + 0.7*(BB + 1.0)
  // GRC_HK = AA_HK + 0.7* BB_HK
  const double grcrit = aa + 0.7 * (bb + 1.0);
  const double grcHk = aaHk + 0.7 * bbHk;
  // Fortran: GR = LOG10(RT)
  // GR_RT = 1.0 / (2.3025851*RT)
  const double gr = std::log10(rt);
  const double grRt = 1.0 / (2.3025851 * rt);
  AmplificationRateValue out{};
  if (gr < grcrit - dgr) {
    // Fortran: AX = 0.
    // AX_HK = 0.
    // AX_TH = 0.
    // AX_RT = 0.
    out.value = 0.0;
    out.shapeDerivative = 0.0;
    out.momentumThicknessDerivative = 0.0;
    out.reynoldsDerivative = 0.0;
  } else {
    // Fortran: RNORM = (GR - (GRCRIT-DGR)) / (2.0*DGR)
    // RN_HK = - GRC_HK / (2.0*DGR)
    // RN_RT = GR_RT / (2.0*DGR)
    const double rnorm = (gr - (grcrit - dgr)) / (2.0 * dgr);
    const double rnHk = -grcHk / (2.0 * dgr);
    const double rnRt = grRt / (2.0 * dgr);
    // Fortran binds ** more strongly than *: RNORM**2/RNORM**3 are
    // computed first (canonical Fortran evaluation, xblsys.f:2032-2040).
    const double rnSq = rnorm * rnorm;      // Fortran RNORM**2
    const double rnCube = rnSq * rnorm;     // Fortran RNORM**3
    double rfac = 0.0;
    double rfacHk = 0.0;
    double rfacRt = 0.0;
    // Fortran: IF(RNORM .GE. 1.0) THEN
    // RFAC = 1.0
    // RFAC_HK = 0.
    // RFAC_RT = 0.
    // ELSE
    // RFAC = 3.0*RNORM**2 - 2.0*RNORM**3
    // RFAC_RN = 6.0*RNORM - 6.0*RNORM**2
    // RFAC_HK = RFAC_RN*RN_HK
    // RFAC_RT = RFAC_RN*RN_RT
    // ENDIF
    if (rnorm >= 1.0) {
      rfac = 1.0;
      rfacHk = 0.0;
      rfacRt = 0.0;
    } else {
      rfac = 3.0 * rnSq - 2.0 * rnCube;
      const double rfacRn = 6.0 * rnorm - 6.0 * rnSq;
      rfacHk = rfacRn * rnHk;
      rfacRt = rfacRn * rnRt;
    }
    // Fortran: ARG = 3.87*HMI - 2.52
    // ARG_HK = 3.87*HMI_HK
    const double arg = 3.87 * hmi - 2.52;
    const double argHk = 3.87 * hmiHk;
    // Fortran: EX = EXP(-ARG**2)
    // EX_HK = EX * (-2.0*ARG*ARG_HK)
    const double ex = std::exp(-(arg * arg));
    const double exHk = ex * (-2.0 * arg * argHk);
    // Fortran: DADR = 0.028*(HK-1.0) - 0.0345*EX
    // DADR_HK = 0.028 - 0.0345*EX_HK
    const double dadr = 0.028 * (hk - 1.0) - 0.0345 * ex;
    const double dadrHk = 0.028 - 0.0345 * exHk;
    // Fortran: AF = -0.05 + 2.7*HMI - 5.5*HMI**2 + 3.0*HMI**3
    // AF_HMI = 2.7 - 11.0*HMI + 9.0*HMI**2
    // AF_HK = AF_HMI*HMI_HK
    // (Fortran ** stronger as *: HMI**2/HMI**3 first computed)
    const double hmiSq = hmi * hmi;      // Fortran HMI**2
    const double hmiCube = hmiSq * hmi;  // Fortran HMI**3
    const double af =
        -0.05 + 2.7 * hmi - 5.5 * hmiSq + 3.0 * hmiCube;
    const double afHmi = 2.7 - 11.0 * hmi + 9.0 * hmiSq;
    const double afHk = afHmi * hmiHk;
 // Fortran: AX = (AF *DADR/TH) * RFAC
    out.value = (af * dadr / th) * rfac;
    // Fortran: AX_HK = (AF_HK*DADR/TH + AF*DADR_HK/TH) * RFAC
 // & + (AF *DADR/TH) * RFAC_HK
    out.shapeDerivative = (afHk * dadr / th + af * dadrHk / th) * rfac +
                          (af * dadr / th) * rfacHk;
    // Fortran: AX_TH = -AX/TH
    out.momentumThicknessDerivative = -out.value / th;
 // Fortran: AX_RT = (AF *DADR/TH) * RFAC_RT
    out.reynoldsDerivative = (af * dadr / th) * rfacRt;
  }
  return out;
}

// Fortran: xblsys.f:2099 DAMPL2(HK, TH, RT, AX, AX_HK, AX_TH, AX_RT)
AmplificationRateValue dampl2(double hk, double th, double rt) {
  // Fortran: DATA DGR / 0.08 /
  // DATA HK1, HK2 / 3.5, 4.0 /
  const double dgr = 0.08;
  const double hk1 = 3.5;
  const double hk2 = 4.0;
  // Fortran: HMI = 1.0/(HK - 1.0)
  // HMI_HK = -HMI**2
  const double hmi = 1.0 / (hk - 1.0);
  const double hmiHk = -hmi * hmi;
  // Fortran: AA = 2.492*HMI**0.43
  // AA_HK = (AA/HMI)*0.43 * HMI_HK
  const double aa = 2.492 * std::pow(hmi, 0.43);
  const double aaHk = (aa / hmi) * 0.43 * hmiHk;
  // Fortran: BB = TANH(14.0*HMI - 9.24)
  // BB_HK = (1.0 - BB*BB) * 14.0 * HMI_HK
  const double bb = std::tanh(14.0 * hmi - 9.24);
  const double bbHk = (1.0 - bb * bb) * 14.0 * hmiHk;
  // Fortran: GRC = AA + 0.7*(BB + 1.0)
  // GRC_HK = AA_HK + 0.7* BB_HK
  const double grc = aa + 0.7 * (bb + 1.0);
  const double grcHk = aaHk + 0.7 * bbHk;
  // Fortran: GR = LOG10(RT)
  // GR_RT = 1.0 / (2.3025851*RT)
  const double gr = std::log10(rt);
  const double grRt = 1.0 / (2.3025851 * rt);
  AmplificationRateValue out{};
  if (gr < grc - dgr) {
    // Fortran: AX = 0.
    // AX_HK = 0.
    // AX_TH = 0.
    // AX_RT = 0.
    out.value = 0.0;
    out.shapeDerivative = 0.0;
    out.momentumThicknessDerivative = 0.0;
    out.reynoldsDerivative = 0.0;
    return out;
  }
  // Fortran: RNORM = (GR - (GRC-DGR)) / (2.0*DGR)
  // RN_HK = - GRC_HK / (2.0*DGR)
  // RN_RT = GR_RT / (2.0*DGR)
    const double rnorm = (gr - (grc - dgr)) / (2.0 * dgr);
    const double rnHk = -grcHk / (2.0 * dgr);
    const double rnRt = grRt / (2.0 * dgr);
    // Fortran binds ** more strongly than *: RNORM**2/RNORM**3 are
    // computed first (canonical Fortran evaluation, xblsys.f:2129-2137).
    const double rnSq = rnorm * rnorm;      // Fortran RNORM**2
    const double rnCube = rnSq * rnorm;     // Fortran RNORM**3
    double rfac = 0.0;
    double rfacHk = 0.0;
    double rfacRt = 0.0;
    // Fortran: IF(RNORM .GE. 1.0) THEN
    // RFAC = 1.0
    // RFAC_HK = 0.
    // RFAC_RT = 0.
    // ELSE
    // RFAC = 3.0*RNORM**2 - 2.0*RNORM**3
    // RFAC_RN = 6.0*RNORM - 6.0*RNORM**2
    // RFAC_HK = RFAC_RN*RN_HK
    // RFAC_RT = RFAC_RN*RN_RT
    // ENDIF
    if (rnorm >= 1.0) {
      rfac = 1.0;
      rfacHk = 0.0;
      rfacRt = 0.0;
    } else {
      rfac = 3.0 * rnSq - 2.0 * rnCube;
      const double rfacRn = 6.0 * rnorm - 6.0 * rnSq;
      rfacHk = rfacRn * rnHk;
      rfacRt = rfacRn * rnRt;
    }
    // Fortran: ARG = 3.87*HMI - 2.52
    // ARG_HK = 3.87*HMI_HK
    const double arg = 3.87 * hmi - 2.52;
    const double argHk = 3.87 * hmiHk;
    // Fortran: EX = EXP(-ARG**2)
    // EX_HK = EX * (-2.0*ARG*ARG_HK)
    const double ex = std::exp(-(arg * arg));
    const double exHk = ex * (-2.0 * arg * argHk);
    // Fortran: DADR = 0.028*(HK-1.0) - 0.0345*EX
    // DADR_HK = 0.028 - 0.0345*EX_HK
    const double dadr = 0.028 * (hk - 1.0) - 0.0345 * ex;
    const double dadrHk = 0.028 - 0.0345 * exHk;
    // Fortran: BRG = -20.0*HMI
    // AF = -0.05 + 2.7*HMI - 5.5*HMI**2 + 3.0*HMI**3 + 0.1*EXP(BRG)
    // AF_HMI = 2.7 - 11.0*HMI + 9.0*HMI**2 - 2.0*EXP(BRG)
    // AF_HK = AF_HMI*HMI_HK
    // (Fortran ** stronger as *: HMI**2/HMI**3 first computed)
    const double hmiSq = hmi * hmi;      // Fortran HMI**2
    const double hmiCube = hmiSq * hmi;  // Fortran HMI**3
    const double brg = -20.0 * hmi;
    const double af = -0.05 + 2.7 * hmi - 5.5 * hmiSq + 3.0 * hmiCube +
                      0.1 * std::exp(brg);
    const double afHmi =
        2.7 - 11.0 * hmi + 9.0 * hmiSq - 2.0 * std::exp(brg);
    const double afHk = afHmi * hmiHk;
 // Fortran: AX = (AF *DADR/TH) * RFAC
  out.value = (af * dadr / th) * rfac;
  // Fortran: AX_HK = (AF_HK*DADR/TH + AF*DADR_HK/TH) * RFAC
 // & + (AF *DADR/TH) * RFAC_HK
  out.shapeDerivative = (afHk * dadr / th + af * dadrHk / th) * rfac +
                        (af * dadr / th) * rfacHk;
  // Fortran: AX_TH = -AX/TH
  out.momentumThicknessDerivative = -out.value / th;
 // Fortran: AX_RT = (AF *DADR/TH) * RFAC_RT
  out.reynoldsDerivative = (af * dadr / th) * rfacRt;
  // Fortran: IF(HK .LT. HK1) RETURN
  if (hk < hk1) {
    return out;
  }
  // Fortran: HNORM = (HK - HK1) / (HK2 - HK1)
  // HN_HK = 1.0 / (HK2 - HK1)
  const double hnorm = (hk - hk1) / (hk2 - hk1);
  const double hnHk = 1.0 / (hk2 - hk1);
  // Fortran: IF(HNORM .GE. 1.0) THEN
  // HFAC = 1.0
  // HF_HK = 0.
  // ELSE
  // HFAC = 3.0*HNORM**2 - 2.0*HNORM**3
  // HF_HK = (6.0*HNORM - 6.0*HNORM**2)*HN_HK
  // ENDIF
  double hfac = 0.0;
  double hfHk = 0.0;
  if (hnorm >= 1.0) {
    hfac = 1.0;
    hfHk = 0.0;
  } else {
    // Fortran ** stronger as *: HNORM**2/HNORM**3 first computed.
    const double hnSq = hnorm * hnorm;     // Fortran HNORM**2
    const double hnCube = hnSq * hnorm;    // Fortran HNORM**3
    hfac = 3.0 * hnSq - 2.0 * hnCube;
    hfHk = (6.0 * hnorm - 6.0 * hnSq) * hnHk;
  }
  // Fortran: AX1 = AX
  // AX1_HK = AX_HK
  // AX1_TH = AX_TH
  // AX1_RT = AX_RT
  const double ax1 = out.value;
  const double ax1Hk = out.shapeDerivative;
  const double ax1Th = out.momentumThicknessDerivative;
  const double ax1Rt = out.reynoldsDerivative;
  // Fortran: GR0 = 0.30 + 0.35 * EXP(-0.15*(HK-5.0))
  // GR0_HK = - 0.35 * EXP(-0.15*(HK-5.0)) * 0.15
  const double gr0 = 0.30 + 0.35 * std::exp(-0.15 * (hk - 5.0));
  const double gr0Hk = -0.35 * std::exp(-0.15 * (hk - 5.0)) * 0.15;
  // Fortran: TNR = TANH(1.2*(GR - GR0))
  // TNR_RT = (1.0 - TNR**2)*1.2*GR_RT
  // TNR_HK = -(1.0 - TNR**2)*1.2*GR0_HK
  const double tnr = std::tanh(1.2 * (gr - gr0));
  const double tnrRt = (1.0 - tnr * tnr) * 1.2 * grRt;
  const double tnrHk = -(1.0 - tnr * tnr) * 1.2 * gr0Hk;
  // Fortran: AX2 = (0.086*TNR - 0.25/(HK-1.0)**1.5) / TH
  // AX2_HK = (0.086*TNR_HK + 1.5*0.25/(HK-1.0)**2.5) / TH
 // AX2_RT = (0.086*TNR_RT) / TH
  // AX2_TH = -AX2/TH
  double ax2 = (0.086 * tnr - 0.25 / std::pow(hk - 1.0, 1.5)) / th;
  double ax2Hk = (0.086 * tnrHk + 1.5 * 0.25 / std::pow(hk - 1.0, 2.5)) / th;
  double ax2Rt = (0.086 * tnrRt) / th;
  double ax2Th = -ax2 / th;
  // Fortran: IF(AX2 .LT. 0.0) THEN
  // AX2 = 0.0
  // AX2_HK = 0.
  // AX2_RT = 0.
  // AX2_TH = 0.
  // ENDIF
  if (ax2 < 0.0) {
    ax2 = 0.0;
    ax2Hk = 0.0;
    ax2Rt = 0.0;
    ax2Th = 0.0;
  }
  // Fortran: AX = HFAC*AX2 + (1.0 - HFAC)*AX1
  out.value = hfac * ax2 + (1.0 - hfac) * ax1;
  // Fortran: AX_HK = HFAC*AX2_HK + (1.0 - HFAC)*AX1_HK + HF_HK*(AX2-AX1)
  out.shapeDerivative =
      hfac * ax2Hk + (1.0 - hfac) * ax1Hk + hfHk * (ax2 - ax1);
  // Fortran: AX_RT = HFAC*AX2_RT + (1.0 - HFAC)*AX1_RT
  out.reynoldsDerivative = hfac * ax2Rt + (1.0 - hfac) * ax1Rt;
  // Fortran: AX_TH = HFAC*AX2_TH + (1.0 - HFAC)*AX1_TH
  out.momentumThicknessDerivative = hfac * ax2Th + (1.0 - hfac) * ax1Th;
  return out;
}

// Fortran: xblsys.f:35 AXSET(HK1, T1, RT1, A1, HK2, T2, RT2, A2, ACRIT,
// IDAMPV, AX, AX_HK1, AX_T1, AX_RT1, AX_A1, AX_HK2, AX_T2, AX_RT2, AX_A2)
AverageAmplificationValue axset(double hk1, double t1, double rt1, double a1,
                                double hk2, double t2, double rt2, double a2,
                                double acrit, int idampv) {
  // Fortran: 2nd-order
  // Fortran: IF(IDAMPV.EQ.0) THEN
 // CALL DAMPL (HK1, T1, RT1, AX1, AX1_HK1, AX1_T1, AX1_RT1)
 // CALL DAMPL (HK2, T2, RT2, AX2, AX2_HK2, AX2_T2, AX2_RT2)
  // ELSE
 // CALL DAMPL2 (HK1, T1, RT1, AX1, AX1_HK1, AX1_T1, AX1_RT1)
 // CALL DAMPL2 (HK2, T2, RT2, AX2, AX2_HK2, AX2_T2, AX2_RT2)
  // ENDIF
  const AmplificationRateValue first =
      (idampv == 0) ? dampl(hk1, t1, rt1) : dampl2(hk1, t1, rt1);
  const AmplificationRateValue second =
      (idampv == 0) ? dampl(hk2, t2, rt2) : dampl2(hk2, t2, rt2);
  const double ax1 = first.value;
  const double ax1Hk1 = first.shapeDerivative;
  const double ax1T1 = first.momentumThicknessDerivative;
  const double ax1Rt1 = first.reynoldsDerivative;
  const double ax2 = second.value;
  const double ax2Hk2 = second.shapeDerivative;
  const double ax2T2 = second.momentumThicknessDerivative;
  const double ax2Rt2 = second.reynoldsDerivative;
  // Fortran: rms-average version (seems a little better on coarse grids)
  // AXSQ = 0.5*(AX1**2 + AX2**2)
  const double axsq = 0.5 * (ax1 * ax1 + ax2 * ax2);
  double axa = 0.0;
  double axaAx1 = 0.0;
  double axaAx2 = 0.0;
  // Fortran: IF(AXSQ .LE. 0.0) THEN
  // AXA = 0.0 ; AXA_AX1 = 0.0 ; AXA_AX2 = 0.0
  // ELSE
  // AXA = SQRT(AXSQ)
  // AXA_AX1 = 0.5*AX1/AXA
  // AXA_AX2 = 0.5*AX2/AXA
  // ENDIF
  if (axsq <= 0.0) {
    axa = 0.0;
    axaAx1 = 0.0;
    axaAx2 = 0.0;
  } else {
    axa = std::sqrt(axsq);
    axaAx1 = 0.5 * ax1 / axa;
    axaAx2 = 0.5 * ax2 / axa;
  }
  // Fortran: small additional term to ensure dN/dx > 0 near N = Ncrit
 // ARG = MIN (20.0*(ACRIT-0.5*(A1+A2)) , 20.0)
  const double arg = std::min(20.0 * (acrit - 0.5 * (a1 + a2)), 20.0);
  double exn = 0.0;
  double exnA1 = 0.0;
  double exnA2 = 0.0;
  // Fortran: IF(ARG.LE.0.0) THEN
  // EXN = 1.0
  // EXN_A1 = 0.
  // EXN_A2 = 0.
  // ELSE
  // EXN = EXP(-ARG)
  // EXN_A1 = 20.0*0.5*EXN
  // EXN_A2 = 20.0*0.5*EXN
  // ENDIF
  if (arg <= 0.0) {
    exn = 1.0;
    exnA1 = 0.0;
    exnA2 = 0.0;
  } else {
    exn = std::exp(-arg);
    exnA1 = 20.0 * 0.5 * exn;
    exnA2 = 20.0 * 0.5 * exn;
  }
  // Fortran: DAX = EXN * 0.002/(T1+T2)
  // DAX_A1 = EXN_A1 * 0.002/(T1+T2)
  // DAX_A2 = EXN_A2 * 0.002/(T1+T2)
  // DAX_T1 = -DAX/(T1+T2)
  // DAX_T2 = -DAX/(T1+T2)
  const double dax = exn * 0.002 / (t1 + t2);
  const double daxA1 = exnA1 * 0.002 / (t1 + t2);
  const double daxA2 = exnA2 * 0.002 / (t1 + t2);
  const double daxT1 = -dax / (t1 + t2);
  const double daxT2 = -dax / (t1 + t2);
  AverageAmplificationValue out{};
  // Fortran: AX = AXA + DAX
  out.value = axa + dax;
  // Fortran: AX_HK1 = AXA_AX1*AX1_HK1
  out.shape1Derivative = axaAx1 * ax1Hk1;
  // Fortran: AX_T1 = AXA_AX1*AX1_T1 + DAX_T1
  out.momentum1Derivative = axaAx1 * ax1T1 + daxT1;
  // Fortran: AX_RT1 = AXA_AX1*AX1_RT1
  out.reynolds1Derivative = axaAx1 * ax1Rt1;
  // Fortran: AX_A1 = DAX_A1
  out.amplification1Derivative = daxA1;
  // Fortran: AX_HK2 = AXA_AX2*AX2_HK2
  out.shape2Derivative = axaAx2 * ax2Hk2;
  // Fortran: AX_T2 = AXA_AX2*AX2_T2 + DAX_T2
  out.momentum2Derivative = axaAx2 * ax2T2 + daxT2;
  // Fortran: AX_RT2 = AXA_AX2*AX2_RT2
  out.reynolds2Derivative = axaAx2 * ax2Rt2;
  // Fortran: AX_A2 = DAX_A2
  out.amplification2Derivative = daxA2;
  return out;
}

}  // namespace xfoil::core::boundary_layer

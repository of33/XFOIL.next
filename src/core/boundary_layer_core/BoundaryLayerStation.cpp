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

// File: src/core/boundary_layer_core/BoundaryLayerStation.cpp
// BLPRV/BLKIN kernels of the new 0-based core.
//
// Formulas exactly as in Fortran:
// - BLPRV (xblsys.f:701): X2=XSI; AMPL2=AMI; S2=CTI; T2=THI; D2=DSI-DSWAKI;
// DW2=DSWAKI; U2=UEI*(1-TKBL)/(1-TKBL*(UEI/QINFBL)^2);
// U2_UEI=(1+TKBL*(2*U2*UEI/QINFBL^2-1))/(1-TKBL*(UEI/QINFBL)^2);
// U2_MS=(U2*(UEI/QINFBL)^2-UEI)*TKBL_MS/(1-TKBL*(UEI/QINFBL)^2).
// - BLKIN (xblsys.f:725): M2=U2^2*HSTINV/(GM1BL*(1-0.5*U2^2*HSTINV));
// TR2=1+0.5*GM1BL*M2; M2_U2=2*M2*TR2/U2;
// M2_MS=U2^2*TR2/(GM1BL*(1-0.5*U2^2*HSTINV))*HSTINV_MS;
// R2=RSTBL*TR2^(-1/GM1BL); R2_U2=-R2/TR2*0.5*M2_U2;
// R2_MS=-R2/TR2*0.5*M2_MS+RSTBL_MS*TR2^(-1/GM1BL); H2=D2/T2;
// HERAT=1-0.5*U2^2*HSTINV; HE_U2=-U2*HSTINV; HE_MS=-0.5*U2^2*HSTINV_MS;
// V2=SQRT(HERAT^3)*(1+HVRAT)/(HERAT+HVRAT)/REYBL;
// V2_HE=V2*(1.5/HERAT-1.0/(HERAT+HVRAT)); V2_U2=V2_HE*HE_U2;
// V2_MS=-V2/REYBL*REYBL_MS+V2_HE*HE_MS; V2_Re=-V2/REYBL*REYBL_Re;
// HK2/HK2_H2/HK2_M2 from HKIN (xblsys.f:2276, Whitfield);
// RT2=R2*U2*T2/V2; RT2_U2=RT2*(1/U2+R2_U2/R2-V2_U2/V2); RT2_T2=RT2/T2;
// RT2_MS=RT2*(R2_MS/R2-V2_MS/V2); RT2_Re=RT2*(-V2_Re/V2).

#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"

#include <cmath>

namespace xfoil::core::boundary_layer {

BoundaryLayerPrimary blprv(double xsi, double ami, double cti, double thi,
                           double dsi, double dswaki, double uei,
                           const BoundaryLayerProperties& props) {
  BoundaryLayerPrimary r;
  r.x = xsi;
  r.ampl = ami;
  r.s = cti;
  r.t = thi;
  r.d = dsi - dswaki;
  r.dw = dswaki;

  // Fortran: U2/U2_UEI/U2_MS.
  const double tkbl = props.tkbl;
  const double tkblMs = props.tkblMs;
  const double qinfbl = props.qinfbl;
  const double ueQ = uei / qinfbl;
  const double ueQ2 = ueQ * ueQ;
  const double den = 1.0 - tkbl * ueQ2;
  r.ue = uei * (1.0 - tkbl) / den;
  r.ueEi = (1.0 + tkbl * (2.0 * r.ue * uei / (qinfbl * qinfbl) - 1.0)) / den;
  r.ueMs = (r.ue * ueQ2 - uei) * tkblMs / den;
  return r;
}

BoundaryLayerSecondary blkin(const BoundaryLayerPrimary& prim,
                             const BoundaryLayerProperties& props) {
  BoundaryLayerSecondary r;
  const double gm1bl = props.gm1bl;
  const double hstinv = props.hstinv;
  const double hstinvMs = props.hstinvMs;
  const double rstbl = props.rstbl;
  const double rstblMs = props.rstblMs;
  const double hvrat = props.hvrat;
  const double reybl = props.reybl;
  const double reyblMs = props.reyblMs;
  const double reyblRe = props.reyblRe;

  // Fortran: edge Mach number ** 2.
  const double u2 = prim.ue;
  const double u2sq = u2 * u2;
  const double u2h = 0.5 * u2sq * hstinv;
  const double mden = gm1bl * (1.0 - u2h);
  r.m = u2sq * hstinv / mden;
  r.tr = 1.0 + 0.5 * gm1bl * r.m;
  r.mUe = 2.0 * r.m * r.tr / u2;
  r.mMs = u2sq * r.tr / mden * hstinvMs;

  // Fortran: edge static density (isentropic relation).
  const double trPow = std::pow(r.tr, -1.0 / gm1bl);
  r.r = rstbl * trPow;
  r.rUe = -r.r / r.tr * 0.5 * r.mUe;
  r.rMs = -r.r / r.tr * 0.5 * r.mMs + rstblMs * trPow;

  // Fortran: shape parameter.
  r.h = prim.d / prim.t;
  r.hD = 1.0 / prim.t;
  r.hT = -r.h / prim.t;

  // Fortran: edge static/stagnation enthalpy.
  // HERAT = 1.0 - 0.5*U2*U2*HSTINV ; HE_U2 = -U2*HSTINV
  // HE_MS = -0.5*U2*U2*HSTINV_MS (xblsys.f:752-754; the earlier
  // equivalent rearrangement with division by HSTINV yielded 0/0 = NaN
  // for HSTINV=0 and was corrected test-backed to the Fortran
  // formula).
  r.herat = 1.0 - u2h;
  r.heUe = -u2 * hstinv;
  r.heMs = -0.5 * u2 * u2 * hstinvMs;

  // Fortran: molecular viscosity.
  r.v = std::sqrt(r.herat * r.herat * r.herat) * (1.0 + hvrat) /
        (r.herat + hvrat) / reybl;
  const double vHe = r.v * (1.5 / r.herat - 1.0 / (r.herat + hvrat));
  r.vUe = vHe * r.heUe;
  r.vMs = -r.v / reybl * reyblMs + vHe * r.heMs;
  r.vRe = -r.v / reybl * reyblRe;

  // Fortran: HKIN (Whitfield) — kinematic shape factor.
  const double mSq = r.m;
  const double hkinDen = 1.0 + 0.113 * mSq;
  r.hk = (r.h - 0.29 * mSq) / hkinDen;
  const double hkH = 1.0 / hkinDen;
  const double hkM = (-0.29 - 0.113 * r.hk) / hkinDen;
  r.hkUe = hkM * r.mUe;
  r.hkT = hkH * r.hT;
  r.hkD = hkH * r.hD;
  r.hkMs = hkM * r.mMs;

  // Fortran: momentum thickness Reynolds number.
  r.rt = r.r * u2 * prim.t / r.v;
  r.rtUe = r.rt * (1.0 / u2 + r.rUe / r.r - r.vUe / r.v);
  r.rtT = r.rt / prim.t;
  r.rtMs = r.rt * (r.rMs / r.r - r.vMs / r.v);
  r.rtRe = r.rt * (-r.vRe / r.v);
  return r;
}

}  // namespace xfoil::core::boundary_layer

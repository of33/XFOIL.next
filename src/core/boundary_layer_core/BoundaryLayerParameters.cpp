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

// File: src/core/boundary_layer_core/BoundaryLayerParameters.cpp
// BLPINI/SETBL data part of the new 0-based core.
//
// Formulas exactly as in Fortran:
// - BLPINI (xbl.f:1578): SCCON=5.6, GACON=6.70, GBCON=0.75, GCCON=18.0,
// DLCON=0.9, CTRCON=1.8, CTRCEX=3.3, DUXCON=1.0,
// CTCON=0.5/(GACON^2*GBCON), CFFAC=1.0.
// - SETBL (xbl.f:46-85): GAMBL=GAMMA; GM1BL=GAMM1; QINFBL=QINF; TKBL=TKLAM;
// TKBL_MS=TK_MSQ; RSTBL=(1+0.5*GM1BL*MINF^2)^(1/GM1BL);
// RSTBL_MS=0.5*RSTBL/(1+0.5*GM1BL*MINF^2);
// HSTINV=GM1BL*(MINF/QINFBL)^2/(1+0.5*GM1BL*MINF^2);
// HSTINV_MS=GM1BL*(1/QINFBL)^2/(1+0.5*GM1BL*MINF^2)
// -0.5*GM1BL*HSTINV/(1+0.5*GM1BL*MINF^2);
// HERAT=1.0-0.5*QINFBL^2*HSTINV; HERAT_MS=-0.5*QINFBL^2*HSTINV_MS;
// REYBL=REINF*SQRT(HERAT^3)*(1+HVRAT)/(HERAT+HVRAT);
// REYBL_Re=SQRT(HERAT^3)*(1+HVRAT)/(HERAT+HVRAT);
// REYBL_MS=REYBL*(1.5/HERAT-1.0/(HERAT+HVRAT))*HERAT_MS;
// DWTE=WGAP(1); IDAMPV=IDAMP.

#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"

#include <cmath>

namespace xfoil::core::boundary_layer {

BoundaryLayerParameters blparConstants() {
  BoundaryLayerParameters p;
  p.sccon = 5.6;
  p.gacon = 6.70;
  p.gbcon = 0.75;
  p.gccon = 18.0;
  p.dlcon = 0.9;
  p.ctrcon = 1.8;
  p.ctrcex = 3.3;
  p.duxcon = 1.0;
  p.ctcon = 0.5 / (p.gacon * p.gacon * p.gbcon);
  p.cffac = 1.0;
  return p;
}

BoundaryLayerProperties boundaryLayerProperties(double minf, double reinf,
                                                double qinf, double gamma,
                                                double gamm1, double tklam,
                                                double tkMsq, double hvrat,
                                                double wgap, int idamp) {
  BoundaryLayerProperties r;
  r.gambl = gamma;
  r.gm1bl = gamm1;
  r.qinfbl = qinf;
  r.tkbl = tklam;
  r.tkblMs = tkMsq;

  // Fortran: RSTBL/RSTBL_MS.
  const double t0 = 1.0 + 0.5 * r.gm1bl * minf * minf;
  r.rstbl = std::pow(t0, 1.0 / r.gm1bl);
  r.rstblMs = 0.5 * r.rstbl / t0;

  // Fortran: HSTINV/HSTINV_MS.
  r.hstinv = r.gm1bl * (minf / r.qinfbl) * (minf / r.qinfbl) / t0;
  r.hstinvMs =
      r.gm1bl * (1.0 / r.qinfbl) * (1.0 / r.qinfbl) / t0 -
      0.5 * r.gm1bl * r.hstinv / t0;

  // Fortran: HERAT/HERAT_MS.
  r.herat = 1.0 - 0.5 * r.qinfbl * r.qinfbl * r.hstinv;
  r.heratMs = -0.5 * r.qinfbl * r.qinfbl * r.hstinvMs;
  r.hvrat = hvrat;

  // Fortran: REYBL/REYBL_Re/REYBL_MS.
  r.reybl = reinf * std::sqrt(r.herat * r.herat * r.herat) *
            (1.0 + hvrat) / (r.herat + hvrat);
  r.reyblRe = std::sqrt(r.herat * r.herat * r.herat) * (1.0 + hvrat) /
              (r.herat + hvrat);
  r.reyblMs =
      r.reybl * (1.5 / r.herat - 1.0 / (r.herat + hvrat)) * r.heratMs;

  // Fortran: DWTE = WGAP(1); IDAMPV = IDAMP.
  r.dwte = wgap;
  r.idampv = idamp;
  return r;
}

}  // namespace xfoil::core::boundary_layer

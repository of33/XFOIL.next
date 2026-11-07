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

// File: include/xfoil/core/boundary_layer/BoundaryLayerCorrelations.h
// Stateless boundary-layer correlation and station kernels of the new
// 0-based cores, ported directly from
// fortran-src/xblsys.f (1981-2523).
//
// Contained kernels:
// hkin HKIN (xblsys.f:2276) kinematic shape factor (Whitfield)
// dil DIL (xblsys.f:2290) laminar dissipation function
// dilw DILW (xblsys.f:2308) laminar wake dissipation function
// hsl HSL (xblsys.f:2327) laminar HS correlation
// cfl CFL (xblsys.f:2354) laminar Cf correlation
// dit DIT (xblsys.f:2375) turbulent dissipation function
// hst HST (xblsys.f:2388) turbulent HS correlation
// cft CFT (xblsys.f:2483) turbulent Cf correlation
// hct HCT (xblsys.f:2514) density shape factor (Whitfield)
// dampl DAMPL (xblsys.f:1981) amplification rate (e^n transition method)
// dampl2 DAMPL2 (xblsys.f:2099) amplification rate (modified e^n)
//
// BLPRV/BLKIN(BoundaryLayerStation.h) remain the primary/
// secondary station kernels and were revalidated.
//
// Formulas, constants, case distinctions, and operation order
// are formula-faithful to the Fortran; internal computation in
// double (Fortran REAL). No raw pointers, no global state.
//
// No dependency on app, CLI, or tests.

#pragma once

#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: CFL/CFT/HSL/HST/DIL/DILW exit — value plus sensitivities.
// Non-applicable sensitivities are 0 (canonical: CF_MSQ=0 for CFL,
// HS_RT=HS_MSQ=0 for HSL, DIL/MSQ=0).
struct CorrelationValue {
  double value{0.0};                  // Fortran X (e.g. CF/HS/DI)
  double shapeDerivative{0.0};        // Fortran X_HK
  double reynoldsDerivative{0.0};     // Fortran X_RT
  double machSquaredDerivative{0.0};  // Fortran X_MSQ
};

// Fortran: DAMPL/DAMPL2 exit — spatial amplification rate AX and
// Sensitivities (x = dN/dx).
struct AmplificationRateValue {
  double value{0.0};                       // Fortran AX
  double shapeDerivative{0.0};             // Fortran AX_HK
  double momentumThicknessDerivative{0.0}; // Fortran AX_TH
  double reynoldsDerivative{0.0};          // Fortran AX_RT
};

// Fortran: AXSET exit — averaged amplification over interval 1..2
// and its sensitivities (xblsys.f:35-144).
struct AverageAmplificationValue {
  double value{0.0};                       // Fortran AX
  double shape1Derivative{0.0};            // Fortran AX_HK1
  double momentum1Derivative{0.0};         // Fortran AX_T1
  double reynolds1Derivative{0.0};         // Fortran AX_RT1
  double amplification1Derivative{0.0};    // Fortran AX_A1
  double shape2Derivative{0.0};            // Fortran AX_HK2
  double momentum2Derivative{0.0};         // Fortran AX_T2
  double reynolds2Derivative{0.0};         // Fortran AX_RT2
  double amplification2Derivative{0.0};    // Fortran AX_A2
};

// Fortran: HKIN exit — kinematic shape factor and sensitivities.
struct KinematicShapeValue {
  double value{0.0};                  // Fortran HK
  double shapeDerivative{0.0};        // Fortran HK_H
  double machSquaredDerivative{0.0};  // Fortran HK_MSQ
};

// Fortran: DIT exit — turbulent dissipation function and
// sensitivities (xblsys.f:2375; in the canonical BLVAR branch ITYP=2,
// performed inline with the DFAC correction).
struct TurbulentDissipationValue {
  double value{0.0};                   // Fortran DI
  double energyShapeDerivative{0.0};   // Fortran DI_HS
  double slipVelocityDerivative{0.0};  // Fortran DI_US
  double skinFrictionDerivative{0.0};  // Fortran DI_CF
  double shearCoefficientDerivative{0.0};  // Fortran DI_ST
};

// Fortran: SUBROUTINE HKIN(H, MSQ, HK, HK_H, HK_MSQ) — kinematic
// shape factor (Whitfield). Inputs: h (shape factor), msq (edge Mach number^2).
[[nodiscard]] KinematicShapeValue hkin(double h, double msq);

// Fortran: SUBROUTINE DIL(HK, RT, DI, DI_HK, DI_RT) — laminar
// dissipation function (Falkner-Skan).
[[nodiscard]] CorrelationValue dil(double hk, double rt);

// Fortran: SUBROUTINE DILW(HK, RT, DI, DI_HK, DI_RT) — laminar
// wake dissipation function (calls HSL with MSQ=0).
[[nodiscard]] CorrelationValue dilw(double hk, double rt);

// Fortran: SUBROUTINE HSL(HK, RT, MSQ, HS, HS_HK, HS_RT, HS_MSQ) —
// laminar HS correlation (canonical HS_RT=HS_MSQ=0; MSQ input unused).
[[nodiscard]] CorrelationValue hsl(double hk, double rt);

// Fortran: SUBROUTINE CFL(HK, RT, MSQ, CF, CF_HK, CF_RT, CF_MSQ) —
// laminar Cf correlation (Falkner-Skan; canonical CF_MSQ=0).
[[nodiscard]] CorrelationValue cfl(double hk, double rt);

// Fortran: SUBROUTINE DIT(HS, US, CF, ST, DI, ...) — turbulent
// dissipation function (2 CD/H*).
[[nodiscard]] TurbulentDissipationValue dit(double hs, double us, double cf,
                                            double st);

// Fortran: SUBROUTINE HST(HK, RT, MSQ, HS, HS_HK, HS_RT, HS_MSQ) —
// turbulent HS correlation (including the Whitfield compressibility correction).
[[nodiscard]] CorrelationValue hst(double hk, double rt, double msq);

// Fortran: SUBROUTINE CFT(HK, RT, MSQ, CF, CF_HK, CF_RT, CF_MSQ) —
// turbulent Cf correlation (Coles; CFFAC from BLPAR, canonical 1.0).
[[nodiscard]] CorrelationValue cft(double hk, double rt, double msq,
                                   double cffac);

// Fortran: SUBROUTINE HCT(HK, MSQ, HC, HC_HK, HC_MSQ) — density shape
// factor (Whitfield).
[[nodiscard]] CorrelationValue hct(double hk, double msq);

// Fortran: SUBROUTINE DAMPL(HK, TH, RT, AX, AX_HK, AX_TH, AX_RT) —
// Amplification rate of the envelope e^n method (Drela/Giles 1987,
// version March 1991, DGR=0.08).
[[nodiscard]] AmplificationRateValue dampl(double hk, double th, double rt);

// Fortran: SUBROUTINE DAMPL2(HK, TH, RT, AX, AX_HK, AX_TH, AX_RT) —
// amplification rate of the modified e^n method (Nov 1996; local
// Constants DGR=0.08, HK1=3.5, HK2=4.0).
[[nodiscard]] AmplificationRateValue dampl2(double hk, double th, double rt);

// Fortran: SUBROUTINE AXSET(HK1, T1, RT1, A1, HK2, T2, RT2, A2, ACRIT,
// IDAMPV, AX, AX_HK1,...) — averaged amplification over interval
// 1..2 (xblsys.f:35; IDAMPV=0 -> DAMPL, otherwise DAMPL2).
[[nodiscard]] AverageAmplificationValue axset(
    double hk1, double t1, double rt1, double a1, double hk2, double t2,
    double rt2, double a2, double acrit, int idampv);

}  // namespace xfoil::core::boundary_layer

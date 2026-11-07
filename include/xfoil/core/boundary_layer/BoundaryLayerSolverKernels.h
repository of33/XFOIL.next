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

// File: include/xfoil/core/boundary_layer/BoundaryLayerSolverKernels.h
// Viscous solver kernels of the new 0-based cores,
// directly ported from fortran-src/xblsys.f BLVAR (784-1123), BLMID
// (1124-1192), TRDIF (1195-1549) and BLDIF (1552-1977).
//
// Contained kernels:
// blvar BLVAR (xblsys.f:784) all secondary "2" variables from the
// primary "2" variables (ITYP=1 laminar,
// 2 turbulent, 3 turbulent wake)
// blmid BLMID (xblsys.f:1124) midpoint Cf
//
// The primary and turbulence-independent secondary "2" variables
// (BLPRV/BLKIN) remain in BoundaryLayerStation.h. BLVAR/BLMID
// add the turbulent secondary quantities HS/HC/US/CQ/CF/DI/DE.
//
// Formulas, constants, case distinctions, and operation order
// are adopted faithfully to the Fortran; internal computation in
// double (Fortran REAL). No raw pointers, no global state.
//
// No dependency on app, CLI, or tests.

#pragma once

#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"

namespace xfoil::core::boundary_layer {

// Fortran: ITYP argument of BLVAR/BLMID (xblsys.f:791-793).
enum class StationType {
  kLaminar = 1,     // Fortran ITYP=1 : laminar
  kTurbulent = 2,   // Fortran ITYP=2 : turbulent
  kWake = 3,        // Fortran ITYP=3 : turbulent wake
};

// Fortran: BLVAR exit — turbulent/secondary "2" variables
// (V_VAR2 fields HS2/HC2/US2/CQ2/CF2/DI2/DE2) and their sensitivities
// with respect to U2/T2/D2/MSQ (X_MS) and, for HS/US/CQ/CF/DI, with
// respect to Re (X_Re); DI additionally with respect to S2 (DI2_S2).
struct BoundaryLayerTurbulentSecondary {
  double hk{0.0};                       // Fortran HK2 after the
                                        // BLVAR clamp MAX(HK2,1.05)
                                        // or MAX(HK2,1.00005)
                                        // (xblsys.f:799-800, COMMON
                                        // side effect on BLDIF/BLMID)
  double hs{0.0};                       // Fortran HS2 (KE shape factor H*)
  double hc{0.0};                       // Fortran HC2 (density shape factor H**)
  double us{0.0};                       // Fortran US2 (normalized slip
                                        // velocity, clamped)
  double cq{0.0};                       // Fortran CQ2 (sqrt((Ctau)EQ))
  double cf{0.0};                       // Fortran CF2 (skin friction coefficient)
  double di{0.0};                       // Fortran DI2 (dissipation function
                                        // 2 CD/H*)
  double diS{0.0};                      // Fortran DI2_S2
  double de{0.0};                       // Fortran DE2 (BL thickness delta)
  double hsU{0.0};                      // Fortran HS2_U2
  double hsT{0.0};                      // Fortran HS2_T2
  double hsD{0.0};                      // Fortran HS2_D2
  double hsMs{0.0};                     // Fortran HS2_MS
  double hsRe{0.0};                     // Fortran HS2_Re
  double hcU{0.0};                      // Fortran HC2_U2
  double hcT{0.0};                      // Fortran HC2_T2
  double hcD{0.0};                      // Fortran HC2_D2
  double hcMs{0.0};                     // Fortran HC2_MS
  double usU{0.0};                      // Fortran US2_U2
  double usT{0.0};                      // Fortran US2_T2
  double usD{0.0};                      // Fortran US2_D2
  double usMs{0.0};                     // Fortran US2_MS
  double usRe{0.0};                     // Fortran US2_Re
  double cqU{0.0};                      // Fortran CQ2_U2
  double cqT{0.0};                      // Fortran CQ2_T2
  double cqD{0.0};                      // Fortran CQ2_D2
  double cqMs{0.0};                     // Fortran CQ2_MS
  double cqRe{0.0};                     // Fortran CQ2_Re
  double cfU{0.0};                      // Fortran CF2_U2
  double cfT{0.0};                      // Fortran CF2_T2
  double cfD{0.0};                      // Fortran CF2_D2
  double cfMs{0.0};                     // Fortran CF2_MS
  double cfRe{0.0};                     // Fortran CF2_Re
  double diU{0.0};                      // Fortran DI2_U2
  double diT{0.0};                      // Fortran DI2_T2
  double diD{0.0};                      // Fortran DI2_D2
  double diMs{0.0};                     // Fortran DI2_MS
  double diRe{0.0};                     // Fortran DI2_Re
  double deU{0.0};                      // Fortran DE2_U2
  double deT{0.0};                      // Fortran DE2_T2
  double deD{0.0};                      // Fortran DE2_D2
  double deMs{0.0};                     // Fortran DE2_MS
};

// Fortran: BLMID exit — midpoint skin-friction coefficient CFM and
// sensitivities with respect to the "1" and "2" stations (HK/RT/M per
// station) and MSQ/Re.
struct MidpointSkinFriction {
  double cf{0.0};          // Fortran CFM
  double cfU1{0.0};        // Fortran CFM_U1
  double cfT1{0.0};        // Fortran CFM_T1
  double cfD1{0.0};        // Fortran CFM_D1
  double cfU2{0.0};        // Fortran CFM_U2
  double cfT2{0.0};        // Fortran CFM_T2
  double cfD2{0.0};        // Fortran CFM_D2
  double cfMs{0.0};        // Fortran CFM_MS
  double cfRe{0.0};        // Fortran CFM_Re
};

// Inputs of blvar: the primary and turbulence-independent
// secondary "2" variables (BLPRV/BLKIN exit) and the BLPAR set passed
// by the caller (canonical defaults via
// blparConstants only for session initialization).
struct BlvarInput {
  BoundaryLayerPrimary primary;
  BoundaryLayerSecondary secondary;
  // complete BLPAR set (COMMON/BLPAR) instead of
  // individual constants; the kernel reads only these values.
  BoundaryLayerParameters blpar;
};

// Fortran: SUBROUTINE BLVAR(ITYP) (xblsys.f:784-1123).
// Computes all secondary "2" variables from the primary "2"
// variables (ITYP=1 laminar, 2 turbulent, 3 turbulent wake).
[[nodiscard]] BoundaryLayerTurbulentSecondary blvar(
    StationType ityp, const BlvarInput& in);

// Fortran: BLMID inputs — HK/RT/M per station including their
// sensitivities (from the respective station state).
struct BlmidStation {
  double hk{0.0};      // Fortran HK1/HK2
  double hkU{0.0};     // Fortran HK1_U1 / HK2_U2
  double hkT{0.0};     // Fortran HK1_T1 / HK2_T2
  double hkD{0.0};     // Fortran HK1_D1 / HK2_D2
  double hkMs{0.0};    // Fortran HK1_MS / HK2_MS
  double rt{0.0};      // Fortran RT1/RT2
  double rtU{0.0};     // Fortran RT1_U1 / RT2_U2
  double rtT{0.0};     // Fortran RT1_T1 / RT2_T2
  double rtMs{0.0};    // Fortran RT1_MS / RT2_MS
  double rtRe{0.0};    // Fortran RT1_Re / RT2_Re
  double m{0.0};       // Fortran M1/M2
  double mU{0.0};      // Fortran M1_U1 / M2_U2
  double mMs{0.0};     // Fortran M1_MS / M2_MS
};

struct BlmidInput {
  BlmidStation first;   // Fortran "1" station
  BlmidStation second;  // Fortran "2" station
  bool simi{false};     // Fortran SIMI ("1" adopted from "2")
  // BLPAR set (CFFAC for CFM) passed explicitly by the caller.
  BoundaryLayerParameters blpar;
};

// Fortran: SUBROUTINE BLMID(ITYP) (xblsys.f:1124-1192).
// Computes the midpoint skin-friction coefficient CFM.
[[nodiscard]] MidpointSkinFriction blmid(StationType ityp,
                                         const BlmidInput& in);

// Fortran: TRCHEK2 inputs — the two-station state (V_VAR1/V_VAR2
// part set) and the control constants (V_VAR/V_INT).
struct TrchekStation {
  double x{0.0};        // Fortran X1/X2
  double ue{0.0};       // Fortran U1/U2 (edge-compressible)
  double t{0.0};        // Fortran T1/T2 (momentum thickness)
  double d{0.0};        // Fortran D1/D2 (displacement thickness)
  double s{0.0};        // Fortran S1/S2 (sqrt(max skin-friction coefficient))
  double ampl{0.0};     // Fortran AMPL1/AMPL2 (log amplification factor)
  double hk{0.0};       // Fortran HK1/HK2 (kinematic shape factor)
  double hkT{0.0};      // Fortran HK1_T1 / HK2_T2
  double hkD{0.0};      // Fortran HK1_D1 / HK2_D2
  double hkU{0.0};      // Fortran HK1_U1 / HK2_U2
  double hkMs{0.0};     // Fortran HK1_MS / HK2_MS
  double rt{0.0};       // Fortran RT1/RT2 (Reynolds number based on momentum thickness)
  double rtT{0.0};      // Fortran RT1_T1 / RT2_T2
  double rtU{0.0};      // Fortran RT1_U1 / RT2_U2
  double rtMs{0.0};     // Fortran RT1_MS / RT2_MS
  double rtRe{0.0};     // Fortran RT1_Re / RT2_Re
};

struct TrchekInput {
  TrchekStation first;    // Fortran "1" station (COM1)
  TrchekStation second;   // Fortran "2" station (COM2)
  double xiforc{0.0};     // Fortran XIFORC
  double acrit{0.0};      // Fortran AMCRIT
  int idampv{0};          // Fortran IDAMPV
  double hstinv{0.0};     // Fortran HSTINV (BLKIN property)
  double gm1bl{0.0};      // Fortran GM1BL
  double rstbl{0.0};      // Fortran RSTBL
  double rstblMs{0.0};    // Fortran RSTBL_MS
  double hstinvMs{0.0};   // Fortran HSTINV_MS
  double reybl{0.0};      // Fortran REYBL
  double reyblMs{0.0};    // Fortran REYBL_MS
  double reyblRe{0.0};    // Fortran REYBL_Re
  double hvrat{0.0};      // Fortran HVRAT
};

// Fortran: TRCHEK2 exit — transition state and XT sensitivities
// (V_VARA fields XT/XT_A1/.../XT_XF) and flags (V_INT).
struct TrchekResult {
  double ampl{0.0};         // Fortran AMPL2 (updated)
  bool tran{false};         // Fortran TRAN
  bool trfree{false};       // Fortran TRFREE
  bool trforc{false};       // Fortran TRFORC
  bool converged{false};    // Newton converged (otherwise warning; canonical
                            // stdout WRITE as structured state)
  bool nonAmplified{false}; // Fortran AX .LE. 0 (punch out, line 390)
  // Fortran label 100 (xblsys.f:423-425): WRITE 'TRCHEK2: N2 convergence
  // failed.' + format 6700 with X1, XT, X2, AMPL1, AMPLT, AMPL2, AX, DA2 from
  // the last iteration. Only set if the 30-iteration Newton run finished
  // without convergence and without punch-out.
  bool n2ConvergenceFailed{false};
  double failX1{0.0};
  double failXt{0.0};
  double failX2{0.0};
  double failAmpl1{0.0};
  double failAmplt{0.0};
  double failAmpl2{0.0};
  double failAx{0.0};
  double failDa2{0.0};
  double xt{0.0};           // Fortran XT
  double xtA1{0.0};         // Fortran XT_A1
  double xtT1{0.0};         // Fortran XT_T1
  double xtD1{0.0};         // Fortran XT_D1
  double xtU1{0.0};         // Fortran XT_U1
  double xtX1{0.0};         // Fortran XT_X1
  double xtT2{0.0};         // Fortran XT_T2
  double xtD2{0.0};         // Fortran XT_D2
  double xtU2{0.0};         // Fortran XT_U2
  double xtX2{0.0};         // Fortran XT_X2
  double xtMs{0.0};         // Fortran XT_MS
  double xtRe{0.0};         // Fortran XT_Re
  double xtXf{0.0};         // Fortran XT_XF
};

// Fortran: SUBROUTINE TRCHEK2 (xblsys.f:231-580).
// Checks whether transition occurs in the interval X1..X2 (implicit
// amplification equation, DAEPS=5.0e-5); sets XT and its
// sensitivities or AMPL2.
[[nodiscard]] TrchekResult trchek2(const TrchekInput& in);

// Fortran: BLDIF inputs — the two-station state (V_VAR1/V_VAR2
// part set) and the interval argument.
struct BldifStation {
  double x{0.0};        // Fortran X1/X2
  double ue{0.0};       // Fortran U1/U2 (edge-compressible)
  double t{0.0};        // Fortran T1/T2 (momentum thickness)
  double d{0.0};        // Fortran D1/D2 (displacement thickness)
  double s{0.0};        // Fortran S1/S2 (sqrt(max skin-friction coefficient))
  double ampl{0.0};     // Fortran AMPL1/AMPL2 (laminar)
  double dw{0.0};       // Fortran DW1/DW2 (wake displacement)
  double m{0.0};        // Fortran M1/M2 (edge Mach number^2)
  double mU{0.0};       // Fortran M1_U1 / M2_U2
  double mMs{0.0};      // Fortran M1_MS / M2_MS
  double h{0.0};        // Fortran H1/H2 (shape factor)
  double hT{0.0};       // Fortran H1_T1 / H2_T2
  double hD{0.0};       // Fortran H1_D1 / H2_D2
  double hk{0.0};       // Fortran HK1/HK2 (kinematic shape factor)
  double hkU{0.0};      // Fortran HK1_U1 / HK2_U2
  double hkT{0.0};      // Fortran HK1_T1 / HK2_T2
  double hkD{0.0};      // Fortran HK1_D1 / HK2_D2
  double hkMs{0.0};     // Fortran HK1_MS / HK2_MS
  double hs{0.0};       // Fortran HS1/HS2 (kinetic energy shape factor)
  double hsU{0.0};      // Fortran HS1_U1 / HS2_U2
  double hsT{0.0};      // Fortran HS1_T1 / HS2_T2
  double hsD{0.0};      // Fortran HS1_D1 / HS2_D2
  double hsMs{0.0};     // Fortran HS1_MS / HS2_MS
  double hsRe{0.0};     // Fortran HS1_Re / HS2_Re
  double hc{0.0};       // Fortran HC1/HC2 (density shape factor)
  double hcU{0.0};      // Fortran HC1_U1 / HC2_U2
  double hcT{0.0};      // Fortran HC1_T1 / HC2_T2
  double hcD{0.0};      // Fortran HC1_D1 / HC2_D2
  double hcMs{0.0};     // Fortran HC1_MS / HC2_MS
  double rt{0.0};       // Fortran RT1/RT2
  double rtU{0.0};      // Fortran RT1_U1 / RT2_U2
  double rtT{0.0};      // Fortran RT1_T1 / RT2_T2
  double rtMs{0.0};     // Fortran RT1_MS / RT2_MS
  double rtRe{0.0};     // Fortran RT1_Re / RT2_Re
  double cf{0.0};       // Fortran CF1/CF2 (skin-friction coefficient)
  double cfU{0.0};      // Fortran CF1_U1 / CF2_U2
  double cfT{0.0};      // Fortran CF1_T1 / CF2_T2
  double cfD{0.0};      // Fortran CF1_D1 / CF2_D2
  double cfMs{0.0};     // Fortran CF1_MS / CF2_MS
  double cfRe{0.0};     // Fortran CF1_Re / CF2_Re
  double di{0.0};       // Fortran DI1/DI2 (Dissipation function)
  double diU{0.0};      // Fortran DI1_U1 / DI2_U2
  double diT{0.0};      // Fortran DI1_T1 / DI2_T2
  double diD{0.0};      // Fortran DI1_D1 / DI2_D2
  double diS{0.0};      // Fortran DI1_S1 / DI2_S2
  double diMs{0.0};     // Fortran DI1_MS / DI2_MS
  double diRe{0.0};     // Fortran DI1_Re / DI2_Re
  double us{0.0};       // Fortran US1/US2 (slip velocity)
  double usU{0.0};      // Fortran US1_U1 / US2_U2
  double usT{0.0};      // Fortran US1_T1 / US2_T2
  double usD{0.0};      // Fortran US1_D1 / US2_D2
  double usMs{0.0};     // Fortran US1_MS / US2_MS
  double cq{0.0};       // Fortran CQ1/CQ2 (sqrt((Ctau)EQ))
  double cqU{0.0};      // Fortran CQ1_U1 / CQ2_U2
  double cqT{0.0};      // Fortran CQ1_T1 / CQ2_T2
  double cqD{0.0};      // Fortran CQ1_D1 / CQ2_D2
  double cqMs{0.0};     // Fortran CQ1_MS / CQ2_MS
  double cqRe{0.0};     // Fortran CQ1_Re / CQ2_Re
  double de{0.0};       // Fortran DE1/DE2 (BL thickness)
  double deU{0.0};      // Fortran DE1_U1 / DE2_U2
  double deT{0.0};      // Fortran DE1_T1 / DE2_T2
  double deD{0.0};      // Fortran DE1_D1 / DE2_D2
  double deMs{0.0};     // Fortran DE1_MS / DE2_MS
};

struct BldifInput {
  BldifStation first;    // Fortran "1" station (COM1)
  BldifStation second;   // Fortran "2" station (COM2)
  MidpointSkinFriction mid;  // Fortran CFM/CFM_* (BLMID exit)
  StationType ityp{StationType::kLaminar};  // Fortran ITYP (0 -> kSimilarity)
  bool similarity{false};  // Fortran ITYP=0 (similarity station)
  double bule{1.0};        // Fortran BULE (x/u du/dx at the stagnation point)
  double ampl2{0.0};       // Fortran AMPL2 (line 1, ITYP=0)
  double acrit{0.0};       // Fortran AMCRIT (AXSET, ITYP=1)
  int idampv{0};           // Fortran IDAMPV (AXSET, ITYP=1)
  // BLPAR set (DLCON/GCCON/GACON/GBCON/SCCON/DUXCON) passed explicitly
  // by the caller; no default reference in the kernel.
  BoundaryLayerParameters blpar;
};

// Fortran: BLDIF exit — the Newton system of the intervals:
// VS1/VS2 (4x5, Fortran VS1(K,L)/VS2(K,L)) and VSREZ/VSR/VSM/VSX (4).
struct BldifSystem {
  double vs1[4][5]{};    // Fortran VS1(K,L)
  double vs2[4][5]{};    // Fortran VS2(K,L)
  double vsrez[4]{};     // Fortran VSREZ(K)
  double vsr[4]{};       // Fortran VSR(K)
  double vsm[4]{};       // Fortran VSM(K)
  double vsx[4]{};       // Fortran VSX(K)
};

// Fortran: SUBROUTINE BLDIF(ITYP) (xblsys.f:1552-1977).
// Sets the Newton-system coefficients and residuals of the intervals
// (ITYP=0 similarity station, 1 laminar, 2 turbulent, 3 wake).
[[nodiscard]] BldifSystem bldif(const BldifInput& in);

// Fortran: TRDIF inputs — the two-station state (COM1/COM2 as
// complete BLDIF stations), the transition state (XT and its
// sensitivities, TRCHEK2 exit), the control constants (AMCRIT/
// IDAMPV) and the BLPAR/BLKIN properties for the internal
// BLKIN/BLVAR/BLMID/BLDIF calls of TRDIF.
struct TrdifInput {
  BldifStation first;    // Fortran COM1
  BldifStation second;   // Fortran COM2
  double xt{0.0};        // Fortran XT (TRCHEK2)
  double xtA1{0.0};      // Fortran XT_A1
  double xtT1{0.0};      // Fortran XT_T1
  double xtD1{0.0};      // Fortran XT_D1
  double xtU1{0.0};      // Fortran XT_U1
  double xtX1{0.0};      // Fortran XT_X1
  double xtT2{0.0};      // Fortran XT_T2
  double xtD2{0.0};      // Fortran XT_D2
  double xtU2{0.0};      // Fortran XT_U2
  double xtX2{0.0};      // Fortran XT_X2
  double xtMs{0.0};      // Fortran XT_MS
  double xtRe{0.0};      // Fortran XT_Re
  double xtXf{0.0};      // Fortran XT_XF
  double amcrit{0.0};    // Fortran AMCRIT
  int idampv{0};         // Fortran IDAMPV
  BoundaryLayerProperties props;  // BLPRV/BLKIN properties
  BoundaryLayerParameters blpar;  // BLPAR constants
};

// Fortran: SUBROUTINE TRDIF (xblsys.f:1195-1549).
// Sets up the Newton system of the transition interval: the laminar
// part X1<xi<XT and the turbulent part XT<xi<X2 are summed. The
// internal T state (X2=XT, T2=TT, D2=DT, U2=UT) is built via
// blkin/blvar; COM1/COM2 are not changed.
[[nodiscard]] BldifSystem trdif(const TrdifInput& in);

}  // namespace xfoil::core::boundary_layer

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

// File: include/xfoil/core/boundary_layer/BoundaryLayerStation.h
// Boundary-layer station variables of the new 0-based cores,
// ported from fortran-src/xblsys.f BLPRV and BLKIN.
//
// Contained types:
// BoundaryLayerPrimary primary "2" variables (BLPRV)
// BoundaryLayerSecondary turbulence-independent secondary "2" variables
// (BLKIN)
//
// Canonical sources:
// BLPRV (xblsys.f:701) X2/AMPL2/S2/T2/D2/DW2/U2/U2_UEI/U2_MS
// BLKIN (xblsys.f:725) M2/TR2/R2/H2/HERAT/V2/HK2/RT2
// HKIN (xblsys.f:2276) HK (Whitfield)
//
// No raw-pointer API, no global state.

#pragma once

#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: BLPRV exit (primary "2"-Variables).
struct BoundaryLayerPrimary {
  double x{0.0};         // Fortran X2 = XSI (BL arc length)
  double ampl{0.0};      // Fortran AMPL2 = AMI (laminar log amplification factor)
  double s{0.0};         // Fortran S2 = CTI (sqrt(max skin-friction coefficient))
  double t{0.0};         // Fortran T2 = THI (kinetic energy thickness)
  double d{0.0};         // Fortran D2 = DSI-DSWAKI (displacement thickness)
  double dw{0.0};        // Fortran DW2 = DSWAKI (wake displacement)
  double ue{0.0};        // Fortran U2 (edge-compressible velocity)
  double ueEi{0.0};      // Fortran U2_UEI (dU2/dUEI)
  double ueMs{0.0};      // Fortran U2_MS (dU2/dHSTINV)
};

// Fortran: BLKIN exit (turbulence-independent secondary "2"-Variables).
struct BoundaryLayerSecondary {
  double m{0.0};         // Fortran M2 (edge Mach number^2)
  double mUe{0.0};       // Fortran M2_U2
  double mMs{0.0};       // Fortran M2_MS
  double tr{0.0};        // Fortran TR2 (T/T0)
  double r{0.0};         // Fortran R2 (edge density)
  double rUe{0.0};       // Fortran R2_U2
  double rMs{0.0};       // Fortran R2_MS
  double h{0.0};         // Fortran H2 (shape factor)
  double hD{0.0};        // Fortran H2_D2
  double hT{0.0};        // Fortran H2_T2
  double herat{0.0};     // Fortran HERAT (edge/stagnation enthalpy)
  double heUe{0.0};      // Fortran HE_U2
  double heMs{0.0};      // Fortran HE_MS
  double v{0.0};         // Fortran V2 (molecular viscosity)
  double vUe{0.0};       // Fortran V2_U2
  double vMs{0.0};       // Fortran V2_MS
  double vRe{0.0};       // Fortran V2_Re
  double hk{0.0};        // Fortran HK2 (kinematic shape factor)
  double hkUe{0.0};      // Fortran HK2_U2
  double hkT{0.0};       // Fortran HK2_T2
  double hkD{0.0};       // Fortran HK2_D2
  double hkMs{0.0};      // Fortran HK2_MS
  double rt{0.0};        // Fortran RT2 (Reynolds number based on momentum thickness)
  double rtUe{0.0};      // Fortran RT2_U2
  double rtT{0.0};       // Fortran RT2_T2
  double rtMs{0.0};      // Fortran RT2_MS
  double rtRe{0.0};      // Fortran RT2_Re
};

// Fortran: SUBROUTINE BLPRV(XSI,AMI,CTI,THI,DSI,DSWAKI,UEI).
// Sets the primary "2" variables from the parameter list. Inputs:
// xsi/ami/cti/thi/dsi/dswaki/uei (station inputs), props
// (BoundaryLayerProperties from the SETBL data-part factory).
[[nodiscard]] BoundaryLayerPrimary blprv(double xsi, double ami, double cti,
                                         double thi, double dsi, double dswaki,
                                         double uei,
                                         const BoundaryLayerProperties& props);

// Fortran: SUBROUTINE BLKIN (uses the primary "2"-Variables).
// Sets the turbulence-independent secondary "2" variables from the
// primary "2" variables. Input: prim (BLPRV exit), props.
[[nodiscard]] BoundaryLayerSecondary blkin(
    const BoundaryLayerPrimary& prim, const BoundaryLayerProperties& props);

}  // namespace xfoil::core::boundary_layer

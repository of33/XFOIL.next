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

// File: include/xfoil/core/boundary_layer/BoundaryLayerTypes.h
// Boundary-layer data model of the new 0-based cores,
// ported from fortran-src/XBL.INC, BLPAR.INC, XFOIL.INC (CR15/CI05/Cl02)
// and the data parts of fortran-src/xbl.f SETBL.
//
// Contained types:
// SurfaceSide explicit airfoil side (0-based internal)
// BoundaryLayerParameters BLPAR constants (BLPAR.INC, BLPINI)
// BoundaryLayerProperties operating quantities from the SETBL data part
// TransitionState forced transition (XIFSET)
// BoundaryLayerMapping panel/BL assignment (IBLPAN)
//
// Canonical sources:
// BLPAR.INC SCCON/GACON/GBCON/GCCON/DLCON/CTRCON/CTRCEX/DUXCON/CTCON/
// CFFAC
// BLPINI SCCON=5.6, GACON=6.70, GBCON=0.75, GCCON=18.0, DLCON=0.9,
// CTRCON=1.8, CTRCEX=3.3, DUXCON=1.0, CTCON=0.5/(GACON^2*GBCON),
// CFFAC=1.0
// SETBL GAMBL/GM1BL/QINFBL/TKBL/TKBL_MS/RSTBL/RSTBL_MS/HSTINV/
// HSTINV_MS/HERAT/HERAT_MS/REYBL/REYBL_MS/REYBL_Re/DWTE/IDAMPV
//
// Invariants:
// - upper and lower side are separate (SurfaceSide),
// - the TE station per side is the last profile station,
// - the wake begins after the airfoil TE station,
// - the panel index per BL station is valid,
// - xi (XSSI) is monotonic as in Fortran (small at the stagnation point),
// - the transition index lies in the valid station range,
// - geometry/panel changes invalidate the mapping and BL state.
//
// No raw-pointer API, no global state, no dependency on
// app, CLI, or tests.

#pragma once

#include <cstddef>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::boundary_layer {

// Fortran IS 1/2 (COMMON ISX=2) -> C++ 0-based. Explicit type, no
// magic index.
enum class SurfaceSide {
  kTop = 0,     // Fortran IS=1 (upper airfoil side)
  kBottom = 1,  // Fortran IS=2 (lower airfoil side)
};

// Fortran: BLPAR.INC constants (set by BLPINI).
struct BoundaryLayerParameters {
  double sccon{0.0};   // shear coefficient lag constant
  double gacon{0.0};   // G-beta locus constant
  double gbcon{0.0};   // G = GACON*sqrt(1+GBCON*beta) + GCCON/[H*Rtheta*sqrt(Cf/2)]
  double gccon{0.0};   // wall term
  double dlcon{0.0};   // wall/wake dissipation length ratio Lo/L
  double ctrcon{0.0};  // transition Reynolds exponent
  double ctrcex{0.0};  // transition Reynolds prefactor exponent
  double duxcon{0.0};  // dUe/dui coupling constant
  double ctcon{0.0};   // Ctau weighting coefficient
  double cffac{0.0};   // Cf scaling factor
};

// Fortran: SETBL data part (xbl.f:46-85) — operating quantities for the
// boundary layer. Inputs are MINF/REINF/QINF/GAMMA/GAMM1 (from MRCL/
// COMSET), tklam/tkMsq (COMSET exit), hvrat (Sutherland ratio,
// never set canonically -> 0.0), wgap (TE gap, Fortran WGAP(1)).
struct BoundaryLayerProperties {
  double gambl{0.0};     // Fortran GAMBL = GAMMA
  double gm1bl{0.0};     // Fortran GM1BL = GAMM1
  double qinfbl{0.0};    // Fortran QINFBL = QINF
  double tkbl{0.0};      // Fortran TKBL = TKLAM
  double tkblMs{0.0};    // Fortran TKBL_MS = TK_MSQ
  double rstbl{0.0};     // Fortran RSTBL (Stagnation point density)
  double rstblMs{0.0};   // Fortran RSTBL_MS
  double hstinv{0.0};    // Fortran HSTINV (1/enthalpy)
  double hstinvMs{0.0};  // Fortran HSTINV_MS
  double hvrat{0.0};     // Fortran HVRAT (Sutherland ratio)
  double herat{0.0};     // Fortran HERAT
  double heratMs{0.0};   // Fortran HERAT_MS
  double reybl{0.0};     // Fortran REYBL
  double reyblMs{0.0};   // Fortran REYBL_MS
  double reyblRe{0.0};   // Fortran REYBL_Re
  double dwte{0.0};      // Fortran DWTE = WGAP(1) (TE gap)
  int idampv{0};         // Fortran IDAMPV = IDAMP (0/1)
};

// Fortran: XIFSET exit (forced transition) and trip state.
struct TransitionState {
  double xiforc{0.0};         // Fortran XIFORC (BL coordinate of the forced
                              // transition)
  bool tforce{false};         // Fortran TFORCE(IS) (transition enforced)
  bool trforc{false};         // Fortran TRFORC
  bool trfree{false};         // Fortran TRFREE
  bool pastTrip{false};       // Fortran 'stagnation point is past trip':
                              // XIFORC < 0 -> XIFORC=XSSI(IBLTE) (fix,
                              // caller continues)
};

// Fortran: IBLPAN exit — panel/BL assignment per side.
// ipan[station] is the 0-based panel index (Fortran IPAN(IBL,IS) 1-based).
struct BoundaryLayerSideMapping {
  int iblte{0};                 // Fortran IBLTE(IS) (TE station, 0-based)
  int nbl{0};                   // Fortran NBL(IS) (max station, 0-based)
  std::vector<int> ipan;        // panel index per station (0-based)
  std::vector<double> vti;      // +/-1 conversion factor (Fortran VTI)
};

// Fortran: IBLPAN combined output (both sides + IBLMAX).
struct BoundaryLayerMapping {
  BoundaryLayerSideMapping top;     // Fortran IS=1
  BoundaryLayerSideMapping bottom;  // Fortran IS=2
  int iblmax{0};                    // Fortran IBLMAX
  bool valid{false};                // Fortran LIPAN
};

// Fortran: XICALC exit — BL arc lengths (XSSI) per side and wake.
struct BoundaryLayerArcLengths {
  std::vector<double> top;     // Fortran XSSI(IBL,1), 0-based
  std::vector<double> bottom;  // Fortran XSSI(IBL,2), 0-based
};

// Fortran: XICALC exit — wake-gap array (Fortran WGAP(IW)).
struct WakeGap {
  std::vector<double> wgap;  // Fortran WGAP(IW), 0-based
  double dwdxte{0.0};        // Fortran DWDXTE (limited)
};

// Safety contract: all routines check their span lengths before
// use. Invalid sizes throw std::invalid_argument.

}  // namespace xfoil::core::boundary_layer

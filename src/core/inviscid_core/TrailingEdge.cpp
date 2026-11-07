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

// File: src/core/inviscid_core/TrailingEdge.cpp
// Complete TECALC routine of the new 0-based cores,
// ported from fortran-src/xfoil.f TECALC.
//
// The geometry part stays in the paneling::teGeometry kernel (shared
// geometric basis, no duplicated formulas). added the
// SIGTE/GAMTE part exact from Fortran:
// SIGTE = 0.5*(GAM(1) - GAM(N)) *SCS
// GAMTE = -.5*(GAM(1) - GAM(N)) *SDS
// SIGTE_A = 0.5*(GAM_A(1) - GAM_A(N))*SCS
// GAMTE_A = -.5*(GAM_A(1) - GAM_A(N))*SDS

#include "xfoil/core/inviscid/TrailingEdge.h"

namespace xfoil::core::inviscid {

// Fortran: SUBROUTINE TECALC (complete).
TrailingEdgeState tecalcFull(double x0, double y0, double xp0, double yp0,
                             double xn, double yn, double xpn, double ypn,
                             double chord, double gam0, double gamN,
                             double gamA0, double gamAN) {
 // Fortran: Geometry share (Kernel teGeometry).
  const paneling::TrailingEdgeGeometry geometry =
      paneling::teGeometry(x0, y0, xp0, yp0, xn, yn, xpn, ypn, chord);

  TrailingEdgeState state;
  state.sharp = geometry.sharp;
  state.dste = geometry.dste;
  state.ante = geometry.ante;
  state.aste = geometry.aste;
  state.scs = geometry.scs;
  state.sds = geometry.sds;

  // Fortran: TE panel source and vortex strengths.
  state.sigte = 0.5 * (gam0 - gamN) * state.scs;
  state.gamte = -0.5 * (gam0 - gamN) * state.sds;
  state.sigteA = 0.5 * (gamA0 - gamAN) * state.scs;
  state.gamteA = -0.5 * (gamA0 - gamAN) * state.sds;

  return state;
}

}  // namespace xfoil::core::inviscid

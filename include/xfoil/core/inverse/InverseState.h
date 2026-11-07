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

// File: include/xfoil/core/inverse/InverseState.h
// Complete MDES forward state and the kernels MAPGAM and QSPCIR
// operating on it for the new 0-based cores, ported from
// fortran-src/xmdes.f.
//
// FullInverseState combines the circle-mapping state (CircleMappingState),
// the panel geometry of the current contour (X,XP,Y,YP,S,N, LE/TE), and the
// mapped surface velocity (QGAMM/SSPEC/XSPOC/YSPOC/SSPLE) as
// separate, value-semantic parts. No Fortran EQUIVALENCE,
// no global states, no raw pointers, no 1-based containers.
//
// Internal computations use double; the canonical Fortran values are
// REAL(4). MAPGAM/QSPCIR call the already validated kernels QCCALC, SEVAL,
// and SPLQSP and do not change their formulas.

#pragma once

#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inverse/CircleGeometry.h"
#include "xfoil/core/inverse/QSpecKernels.h"

namespace xfoil::core::inverse {

// Fortran COMMON/XFOIL.INC/: panel geometry of the current contour.
struct PanelGeometry {
  std::vector<double> x;    // X(1..N)
  std::vector<double> xp;   // XP(1..N) = dX/dS
  std::vector<double> y;    // Y(1..N)
  std::vector<double> yp;   // YP(1..N) = dY/dS
  std::vector<double> s;    // S(1..N)
  int n{0};                 // N
  double sle{0.0};          // SLE
  double xle{0.0};          // XLE
  double yle{0.0};          // YLE
  double xte{0.0};          // XTE
  double yte{0.0};          // YTE
};

// Fortran COMMON/XFOIL.INC/: complete MDES state (forward chain).
struct FullInverseState {
  CircleMappingState circle;
  PanelGeometry panel;
  double minf{0.0};   // MINF
  double qinf{1.0};   // QINF

  std::vector<double> qgamm;  // QGAMM(1..NSP)
  std::vector<double> sspec;  // SSPEC(1..NSP)
  std::vector<double> xspoc;  // XSPOC(1..NSP)
  std::vector<double> yspoc;  // YSPOC(1..NSP)
  double ssple{0.0};          // SSPLE
  int nsp{0};                 // NSP
};

// Fortran QCCALC output (ALG/CLG/CMG or ALQSP/CLQSP/CMQSP):
// IAC=1: ALG given -> Cl, CM are computed (ALG unchanged).
// IAC=2: Cl given -> ALG (Newton) and CM are computed.
struct MappedGammaResult {
  double alfa{0.0};  // ALG/Alfa (IAC=2: updated)
  double cl{0.0};    // CLG/CL (IAC=1: computed)
  double cm{0.0};    // CMG/CM (always computed)
};

// Fortran: SUBROUTINE MAPGAM(IAC,ALG,CLG,CMG)
// IAC=1: ALG given, IAC=2: CLG given. Sets QGAMM/SSPEC from
// QCCALC and XSPOC/YSPOC/SSPLE from the panel geometry. Returns the
// values ALG/CLG/CMG updated by QCCALC (Fortran in/out).
[[nodiscard]] Result<MappedGammaResult> mapGam(FullInverseState& state, int iac,
                                               double alg, double clg,
                                               double cmg);

// Fortran: SUBROUTINE QSPCIR (a QSPEC entry KQSP).
// Fills d.q from QCCALC (ISPEC=IACQSP, AL/Cl/CM from d), uses d.s =
// SSPEC (set by MAPGAM) and calls SPLQSP. Writes the values ALQSP/
// CLQSP/CMQSP updated by QCCALC back into d.al/d.cl/d.cm.
Status qSpecCirc(const FullInverseState& state, QSpecDistribution& d,
                 int iacqsp);

// Fortran QDES (xqdes.f:79-89): numerical capture of the current Q. Takes
// over the panel surface velocities gam = GAM(1..N) and normalizes the
// arc length (SSPEC = S/S(N)), the chord-projected coordinates
// (XSPOC/YSPOC), and SSPLE. SPECAL (analysis) returns gam.
[[nodiscard]] Status captureCurrentSpeed(FullInverseState& state,
                                         std::span<const double> gam);

// Fortran MDES numerical initialization path (xmdes.f:85-106, before the
// menu loop): SCINIT -> MAPGAM(1,ALFA) -> CNCALC(QGAMM) -> QSPCIR.
// EIWSET(NC1) corresponds to a grid previously set via setCircleGrid.
// The menu/plot/prompt part remains outside.
[[nodiscard]] Status mdesInitializeFullInverse(FullInverseState& state,
                                               QSpecDistribution& qspec,
                                               double alfa, double& clgam,
                                               double& cmgam);

// Fortran QDES numerical initialization path (xqdes.f:79-102, before the
// menu loop): current Q capture -> GAMQSP(1). The menu/plot/prompt
// part remains outside. iq1/iq2 are 0-based including segment boundaries.
[[nodiscard]] Status qdesInitializeMixedInverse(FullInverseState& state,
                                                QSpecDistribution& qspec,
                                                std::span<const double> gam,
                                                double alfa, double cl,
                                                double cm, bool liqset,
                                                int& iq1, int& iq2);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/InverseState.h

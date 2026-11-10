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

// File: include/xfoil/core/boundary_layer/Update.h
// UPDATE of the new 0-based cores, directly
// ported from fortran-src/xbl.f UPDATE (1253-1560).
//
// Formulas, constants, case distinctions, and operation order
// are adopted faithfully to the Fortran; internal computation in
// double (Fortran REAL). No raw pointers, no global state; the
// Fortran EQUIVALENCE (VA,UNEW)/(VB,QNEW) is not reproduced as a
// memory overlay.
//
// No dependency on app, CLI, or tests.

#pragma once

#include <span>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: UPDATE inputs (xbl.f:1253-1560) — the coupling state
// (mapping/DIJ/UINV/MASS), the Newton solution (VDEL residuals and
// Re sensitivities per system row), the panel geometry, and the
// BL state (in/out).
struct UpdateInput {
  BoundaryLayerMapping mapping;      // IPAN/VTI/NBL/IBLTE
  std::span<const double> dij;       // DIJ (flat, row-major)
  int dijStride{0};                  // DIJ row stride
  BoundaryLayerStationField uinv;    // Fortran UINV
  BoundaryLayerStationField uinvA;   // Fortran UINV_A
  BoundaryLayerStationField mass;    // Fortran MASS
  std::span<const double> vdelRes;   // VDEL(3,1,JV) per 0-based line
  std::span<const double> vdelRe;    // VDEL(3,2,JV) per 0-based line
  std::array<std::vector<int>, 2> isys;  // Fortran ISYS (0-based lines)
  std::span<const double> x;         // Fortran X (Panel geometry)
  std::span<const double> y;         // Fortran Y
  double alfa{0.0};                  // Fortran ALFA (in/out)
  double minf{0.0};                  // Fortran MINF
  double qinf{1.0};                  // Fortran QINF
  double gamm1{0.4};                 // Fortran GAMM1
  double dtor{0.0};                  // Fortran DTOR
  double minfCl{0.0};                // Fortran MINF_CL
  bool lalfa{false};                 // Fortran LALFA
  bool matyp1{true};                 // Fortran MATYP=1
  double cl{0.0};                    // Fortran Cl (in/out for LALFA)
  double clspec{0.0};                // Fortran CLSPEC
  std::span<const double> wgap;      // Fortran WGAP
  int itranTop{0};                   // Fortran ITRAN(1) (0-based)
  int itranBottom{0};                // Fortran ITRAN(2) (0-based)
  // BL state (in/out)
  BoundaryLayerStationField ctau;    // Fortran CTAU
  BoundaryLayerStationField thet;    // Fortran THET
  BoundaryLayerStationField dstr;    // Fortran DSTR
  BoundaryLayerStationField uedg;    // Fortran UEDG
  BoundaryLayerStationField tau;     // Fortran TAU (wake equivalence only)
  BoundaryLayerStationField dis;     // Fortran DIS
  BoundaryLayerStationField ctq;     // Fortran CTQ
  BoundaryLayerStationField delt;    // Fortran DELT
  BoundaryLayerStationField tstr;    // Fortran TSTR
  BoundaryLayerStationField dssr;    // Fortran DSSR
  BoundaryLayerStationField mdef;    // Fortran MDEF
  BoundaryLayerStationField pdef;    // Fortran PDEF
  BoundaryLayerStationField kdef;    // Fortran KDEF
};

// Fortran: UPDATE exit — relaxation, DAC, and the RMS/max
// changes (VMXBL as character, IMXBL/ISMXBL 0-based).
struct UpdateResult {
  double rlx{0.0};    // Fortran RLX
  double dac{0.0};    // Fortran DAC
  double rmsbl{0.0};  // Fortran RMSBL
  double rmxbl{0.0};  // Fortran RMXBL
  char vmxbl{'\0'};   // Fortran VMXBL ('n'/'C'/'T'/'D'/'U'); initial COMMON value NUL
  int imxbl{-1};      // Fortran IMXBL (0-based; initial COMMON value 0 -> -1)
  int ismxbl{-1};     // Fortran ISMXBL (0-based; initial COMMON value 0 -> -1)
};

// Fortran: SUBROUTINE UPDATE (xbl.f:1253-1560).
// Adds the Newton deltas to the BL variables, checks for excessive
// changes (under-relaxation), computes RMSBL/RMXBL, and sets the
// wake equating.
[[nodiscard]] UpdateResult update(UpdateInput& in);

}  // namespace xfoil::core::boundary_layer

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

// File: include/xfoil/core/boundary_layer/NewtonSystem.h
// Newton system and block solver of the new 0-based cores,
// directly ported from fortran-src/xbl.f IBLSYS (519-539),
// xblsys.f BLSYS (583-661) and TESYS (664-698) and xsolve.f BLSOLV
// (283-488).
//
// Contained kernels:
// iblsys IBLSYS (xbl.f:519) BL position -> system row
// blsys BLSYS (xblsys.f:583) interval Newton system
// tesys TESYS (xblsys.f:664) dummy system TE -> first wake point
// blsolv BLSOLV (xsolve.f:283) block Thomas solver with mass vectors
//
// Formulas, constants, case distinctions, and operation order
// are adopted faithfully to the Fortran; internal computation in
// double (Fortran REAL). No raw pointers, no global state,
// no EQUIVALENCE memory overlay.
//
// No dependency on app, CLI, or tests.

#pragma once

#include <array>
#include <vector>

#include "xfoil/core/boundary_layer/BoundaryLayerSolverKernels.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"

namespace xfoil::core::boundary_layer {

// Fortran: ISYS/NSYS (IBLSYS, xbl.f:519-539) — system row per BL station.
// isys[is][ibl] is the 0-based system row of the 0-based station ibl
// (Fortran IBL=ibl+1); stations 1..nbl carry lines 0..nbl-1 per
// side, and the stagnation point station ibl=0 stays unset (0, Fortran
// ISYS(1,IS) is not set). nbl is the 0-based maximum station index
// (Fortran NBL(IS)-1); nsys is the number of lines.
struct BlsysMapping {
  std::array<std::vector<int>, 2> isys;  // [page][station] -> Line
  int nsys{0};                           // Fortran NSYS
};

// Fortran: SUBROUTINE IBLSYS (xbl.f:519-539).
// Sets the system row per BL station (stations 1..NBL-1, 0-based).
// The canonical STOP '*** IBLSYS: BL system array overflow. ***'
// (NSYS > 2*IVX) is mapped to std::invalid_argument
// (C++ safety deviation, as in IBLPAN).
[[nodiscard]] BlsysMapping iblsys(const std::array<int, 2>& nbl,
                                  int ivxLimit);

// Fortran: TESYS inputs (xblsys.f:664-698) — the TE-averaged
// quantities and the COM2 state (wake station) for the BLVAR(3) call.
struct TesysInput {
  double cte{0.0};  // Fortran CTE (averaged thrust coefficient at the TE)
  double tte{0.0};  // Fortran TTE (averaged momentum thickness)
  double dte{0.0};  // Fortran DTE (averaged displacement including ANTE)
  BlvarInput wake;  // Fortran COM2 state (BLVAR(3) input)
};

// Fortran: TESYS exit — the dummy system and the secondary wake
// quantities updated by the BLVAR(3) call (COM2).
struct TesysResult {
  BldifSystem system;
  BoundaryLayerTurbulentSecondary wakeSecondary;  // BLVAR(3)-Exit
};

// Fortran: SUBROUTINE TESYS(CTE,TTE,DTE) (xblsys.f:664-698).
// Sets the dummy BL system between the airfoil TE point and the
// first wake point (infinitesimally behind the TE).
[[nodiscard]] TesysResult tesys(const TesysInput& in);

// Fortran: BLSYS inputs (xblsys.f:583-661) — the two-station state
// (COM1 complete, COM2 as BLPRV/BLKIN exit for the internal
// BLVAR call), the interval flags, and the edge-compressibility
// sensitivities (U1_UEI/U1_MS/U2_UEI/U2_MS from BLPRV).
struct BlsysInput {
  BldifStation first;               // Fortran COM1 (complete)
  BoundaryLayerPrimary primary2;    // Fortran COM2 (BLPRV exit)
  BoundaryLayerSecondary secondary2;  // Fortran COM2 (BLKIN exit)
  bool wake{false};                 // Fortran WAKE
  bool turb{false};                 // Fortran TURB
  bool tran{false};                 // Fortran TRAN
  bool simi{false};                 // Fortran SIMI
  double ueEi1{1.0};                // Fortran U1_UEI
  double ueMs1{0.0};                // Fortran U1_MS
  double ueEi2{1.0};                // Fortran U2_UEI
  double ueMs2{0.0};                // Fortran U2_MS
  double amcrit{0.0};               // Fortran AMCRIT
  int idampv{0};                    // Fortran IDAMPV
  double bule{1.0};                 // Fortran BULE (SIMI station)
  // Fortran XT/XT_* (TRCHEK2 exit, TRAN interval)
  double xt{0.0};
  double xtA1{0.0};
  double xtT1{0.0};
  double xtD1{0.0};
  double xtU1{0.0};
  double xtX1{0.0};
  double xtT2{0.0};
  double xtD2{0.0};
  double xtU2{0.0};
  double xtX2{0.0};
  double xtMs{0.0};
  double xtRe{0.0};
  double xtXf{0.0};
  BoundaryLayerProperties props;    // BLPRV/BLKIN properties
  BoundaryLayerParameters blpar;    // BLPAR constants
};

// Fortran: SUBROUTINE BLSYS (xblsys.f:583-661).
// Sets up the interval Newton system: BLVAR/BLMID depending on
// WAKE/TURB/TRAN, TRDIF/BLDIF selection, SIMI handling, and the
// conversion to incompressible Uei and Mach.
[[nodiscard]] BldifSystem blsys(const BlsysInput& in);

// Fortran: BLSYS with the post-BLVAR state (xblsys.f:605-614): the
// BLVAR(ITYP) state of the last BLSYS assembly is returned via tsOut
// (Fortran keeps it in COM2 while COM1=COM2 is fixed,
// xbl.f:848-850 — on the converged marching path BLVAR is not
// called again).
[[nodiscard]] BldifSystem blsys(const BlsysInput& in,
                                BoundaryLayerTurbulentSecondary& tsOut);

// Fortran: VA/VB/VZ/VM/VDEL workspace (XFOIL.INC COMMON/VMAT/) for the
// block Newton solver. VA/VB/VDEL are (3x2) blocks per system row,
// VM the mass-influence vectors (3 x nsys per line), and VZ the
// far-off-diagonal TE block.
struct BlsysWorkspace {
  int nsys{0};
  std::vector<std::array<std::array<double, 2>, 3>> va;    // VA(3,2,NSYS)
  std::vector<std::array<std::array<double, 2>, 3>> vb;    // VB(3,2,NSYS)
  std::vector<std::array<std::array<double, 2>, 3>> vdel;  // VDEL(3,2,NSYS)
  std::array<std::array<double, 2>, 3> vz{};               // VZ(3,2)
  std::vector<std::vector<std::array<double, 3>>> vm;      // VM(3,L,IV)
};

// Fortran: SUBROUTINE BLSOLV (xsolve.f:283-488).
// Block-Thomas solver for the coupled BL Newton system
// (in-place mutation of the workspaces, as in the Fortran).
void blsolv(BlsysWorkspace& ws, double vaccel, double arcSpan, int ivte1,
            int ivz);

}  // namespace xfoil::core::boundary_layer

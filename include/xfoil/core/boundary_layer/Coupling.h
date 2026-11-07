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

// File: include/xfoil/core/boundary_layer/Coupling.h
// Viscous-inviscid coupling routines of the new 0-based cores,
// directly ported from fortran-src/xpanel.f UICALC (1542),
// UECALC (1562), QVFUE (1580), GAMQV (1616), UESET (1758), DSSET (1786)
// and xbl.f DSLIM (1564).
//
// Contained kernels:
// uicalc UICALC (xpanel.f:1542) UINV/UINV_A from QINV/QINV_A
// uecalc UECALC (xpanel.f:1562) UEDG from QVIS
// qvfue QVFUE (xpanel.f:1580) QVIS from UEDG
// gamqv GAMQV (xpanel.f:1616) GAM/GAM_A from QVIS/QINV_A
// ueset UESET (xpanel.f:1758) UEDG from UINV + source term
// dsset DSSET (xpanel.f:1786) DSTR from MASS/UEDG
// dslim DSLIM (xbl.f:1564) DSTR clamp (HK <= HKLIM)
//
// Formulas, constants, and operation order are formula-faithful to
// the Fortran; internal computation in double (Fortran REAL).
// No raw pointers, no global state.
//
// No dependency on app, CLI, or tests.

#pragma once

#include <array>
#include <span>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"

namespace xfoil::core::boundary_layer {

// Fortran: SUBROUTINE UICALC (xpanel.f:1542-1559).
// Sets UINV/UINV_A from the panel tangential velocity QINV/QINV_A
// (UINV(IBL,IS) = VTI(IBL,IS)*QINV(IPAN(IBL,IS)); UINV(1,IS)=0).
void uicalc(const BoundaryLayerMapping& map, std::span<const double> qinv,
            std::span<const double> qinvA, BoundaryLayerStationField& uinv,
            BoundaryLayerStationField& uinvA);

// Fortran: SUBROUTINE UECALC (xpanel.f:1562-1577).
// Sets UEDG from the viscous panel tangential velocity QVIS
// (UEDG(1,IS)=0).
void uecalc(const BoundaryLayerMapping& map, std::span<const double> qvis,
            BoundaryLayerStationField& uedg);

// Fortran: SUBROUTINE QVFUE (xpanel.f:1580-1594).
// Sets QVIS from UEDG (QVIS(I) = VTI(IBL,IS)*UEDG(IBL,IS)).
void qvfue(const BoundaryLayerMapping& map,
           const BoundaryLayerStationField& uedg, std::span<double> qvis);

// Fortran: SUBROUTINE GAMQV (xpanel.f:1616-1625).
// Sets GAM/GAM_A from QVIS/QINV_A (GAM(I)=QVIS(I), GAM_A(I)=QINV_A(I)).
void gamqv(std::span<const double> qvis, std::span<const double> qinvA,
           std::span<double> gam, std::span<double> gamA);

// Fortran: SUBROUTINE UESET (xpanel.f:1758-1783).
// Sets UEDG from UINV plus the full source influence
// (DUI = sum(-VTI(IBL,IS)*VTI(JBL,JS)*DIJ(I,J)*MASS(JBL,JS))).
void ueset(const BoundaryLayerMapping& map, std::span<const double> dij,
           int dijStride, const BoundaryLayerStationField& uinv,
           const BoundaryLayerStationField& mass,
           BoundaryLayerStationField& uedg);

// Fortran: SUBROUTINE DSSET (xpanel.f:1786-1796).
// Sets DSTR from MASS/UEDG.
void dsset(const BoundaryLayerMapping& map,
           const BoundaryLayerStationField& mass,
           const BoundaryLayerStationField& uedg,
           BoundaryLayerStationField& dstr);

// Fortran: SUBROUTINE STMOVE (xpanel.f:1628-1755).
// Shifts the stagnation point position onto a new panel and remaps the
// BL state. Session-orchestrated routine: STFIND -> (IBLPAN, UICALC,
// XICALC, IBLSYS) -> shift the BL station arrays -> UEPS tweak ->
// mass system rebuild. Read-only inputs (panel, wakeX/Y, qinv/qinvA) are
// passed as parameters; the mutable BL state lives in StMoveState.
struct StMoveState {
  BoundaryLayerMapping mapping;          // Fortran IPAN/VTI/IBLTE/NBL (IBLPAN)
  BoundaryLayerArcLengths arcs;          // Fortran XSSI (XICALC)
  BoundaryLayerStationField uinv;        // Fortran UINV (UICALC)
  BoundaryLayerStationField uinvA;       // Fortran UINV_A
  BoundaryLayerStationField ctau;        // Fortran CTAU
  BoundaryLayerStationField thet;        // Fortran THET
  BoundaryLayerStationField dstr;        // Fortran DSTR
  BoundaryLayerStationField uedg;        // Fortran UEDG
  BoundaryLayerStationField mass;        // Fortran MASS
  std::array<int, 2> itran{0, 0};        // Fortran ITRAN (0-based)
  std::size_t ist{0};                    // Fortran Is (0-based)
  double sst{0.0};                       // Fortran SST
  BlsysMapping isys;                     // Fortran ISYS/NSYS (IBLSYS)
};

struct StMoveResult {
  bool moved{false};   // true: ISTOLD!= Is (reset branch executed)
  std::size_t idif{0};  // Fortran IDIF
  // true: STFIND found no stagnation point (xpanel.f:1366 message; Fortran
  // backs off to I=N/2 and continues the iteration).
  bool stagnationNotFound{false};
};

StMoveResult stmove(const paneling::PanelGeometry& panel,
                    std::span<const double> wakeX,
                    std::span<const double> wakeY, double ante,
                    std::size_t nw, std::size_t ivx,
                    std::span<const double> qinv,
                    std::span<const double> qinvA, std::span<double> gam,
                    std::span<double> qvis, StMoveState& state);

// Fortran: SUBROUTINE CDCALC (xfoil.f:1172-1208).
// Sets Cd (Squire-Young wake extrapolation) and CDF (skin-friction drag)
// from the BL wake state. CDP comes from CLCALC (inviscid
// pressure distribution) and is not touched here. All station fields
// (thet/dstr/uedg/tau) and x/y must match the mapping.
struct CdCalcResult {
  double cd{0.0};   // Fortran CD
  double cdf{0.0};  // Fortran CDF
};

CdCalcResult cdcalc(double alfa, double qinf, double tklam, bool lvisc,
                    bool lblini, const BoundaryLayerMapping& map,
                    const BoundaryLayerStationField& thet,
                    const BoundaryLayerStationField& dstr,
                    const BoundaryLayerStationField& uedg,
                    const BoundaryLayerStationField& tau,
                    std::span<const double> x, std::span<const double> y);

// Fortran: SUBROUTINE FCPMIN (xoper.f:1768-1801).
// Finds the minimum Cp on the pressure distribution (cavitation work).
// CPMN = CPMNV for LVISC, otherwise CPMNI. x/cpi/cpv have the same length (N+NW).
struct CpMinResult {
  double cpmin{0.0};   // Fortran CPMN
  double cpmni{0.0};   // Fortran CPMNI
  double cpmnv{0.0};   // Fortran CPMNV
  double xcpmni{0.0};  // Fortran XCPMNI
  double xcpmnv{0.0};  // Fortran XCPMNV
};

CpMinResult fcpmin(bool lvisc, std::span<const double> x,
                   std::span<const double> cpi, std::span<const double> cpv);

// Fortran: SUBROUTINE DSLIM(DSTR,THET,UEDG,MSQ,HKLIM) (xbl.f:1564-1574).
// Clamps the displacement thickness at HK <= HKLIM
// (DH = MAX(0, HKLIM-HK)/HK_H ; DSTR += DH*THET).
[[nodiscard]] double dslim(double dstr, double thet, double msq,
                           double hklim);

}  // namespace xfoil::core::boundary_layer

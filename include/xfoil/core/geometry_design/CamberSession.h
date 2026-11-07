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

// File: include/xfoil/core/geometry_design/CamberSession.h
// CAMB session state of the new 0-based cores, ported
// from the numerical sections of SUBROUTINE CAMB (fortran-src/xtcam.f).
//
// The CAMB menu part (ASKC, cursor MODC/MODP, plot PLTINI/PLOTG/PLOTC,
// file RDAC/WRTT/WRTC, GOFINI/AXISADJ) remains outside; this kernel models
// only the numerical state transitions:
// CAMB init (100 block, LCLEAR) -> initCamberSession
// SETC -> setAddedCamberFromCamberline
// INPC (explicit keyboard points) -> inputAddedCamberPoints
// INPP (explicit keyboard points) -> inputAddedLoadingPoints
// CLR -> clearAddedCamber
// SCAL -> scaleAddedCamber
// ADD -> applyAddedCamber
//
// The COMMON state (CR19: XCADD/YCADD/YCADDP, XPADD/YPADD/YPADDP,
// XCAM/YCAM/YCAMP/PCAM/PCAMP, CI19: NCAM; CAMB-local sizes SBL/XBL/YBL/
// XBCH/YBCH/SBCH, LOGICAL LDCPLOT) is modelled as a value type; no
// global kernel state. YBCH is canonically 0 (CAMB 95).
//
// All indices 0-based (Fortran X(1..N) <-> C++ x[0..n-1]).

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"

namespace xfoil::core::geometry_design {

// NTX = 2*IBX = 8*IQX = 2960 (XFOIL.INC PARAMETER). NCAM = MIN(201, NTX).
inline constexpr std::size_t kNtx = 2960;
inline constexpr std::size_t kNcamMax = 201;

// Input point for INPC (x/c, y/c) or INPP (x/c, DCp), each
// normalized to 0..1 with the canonical clamp at the ends.
struct CamberInputPoint {
  double x{0.0};
  double y{0.0};
};

// Integrals of the added loading (CAMB 514-522: CLX, CMX via XCMREF).
struct LoadingIntegrals {
  double clx{0.0};
  double cmx{0.0};
};

// CAMB session state (CR19 + CAMB-local). All vectors are length-equal to ncam or
// ncadd/npadd; ycamp/pcamp/ycadddp/ypadddp are the SEGSPL derivatives.
struct CamberSession {
  std::size_t ncam{0};
  std::vector<double> xcam;
  std::vector<double> ycam;
  std::vector<double> ycamp;
  std::vector<double> pcam;
  std::vector<double> pcamp;

  std::size_t ncadd{0};
  std::vector<double> xcadd;
  std::vector<double> ycadd;
  std::vector<double> ycadddp;

  std::size_t npadd{0};
  std::vector<double> xpadd;
  std::vector<double> ypadd;
  std::vector<double> ypadddp;

  // CAMB geometry reference (100 block).
  double sbl{0.0};
  double xbl{0.0};
  double ybl{0.0};
  double xbch{0.0};
  double ybch{0.0};  // canonical 0.0 (CAMB 95)
  double sbch{0.0};

  bool ldCplot{false};
  double xcmref{0.25};  // COMMON CR12 XCMREF, MAIN default 0.25

  // XLFIND diagnostics of the 100 block.
  geometry::LeStatus xlStatus{geometry::LeStatus::completed};
};

// Fortran: CAMB 100 block with LCLEAR=.TRUE. (init after call/CLR/ADD).
// Buffer NB >= 3 (XLFIND contract). NCADD=NPADD=2 (zero camber/loading),
// SEGSPL of both user arrays, interpolation onto the dense XCAM grid
// (NCAM = MIN(201, NTX)).
CamberSession initCamberSession(const GeometryDesignBuffer& buffer,
                                double xcmref = 0.25);

// Fortran: CAMB 'SETC' (378-394). Added camber = current camber line.
void setAddedCamberFromCamberline(CamberSession& session,
                                  const GeometryDesignBuffer& buffer);

// Fortran: CAMB 'INPC' (397-443). Explicit points instead of keyboard READ;
// x/c clamp (<=0 -> 0, >=1 -> 1), mapping onto camber-line coordinates,
// SORTDUP/FIXDUP, SEGSPL, interpolation. An empty point list leads, as in
// Fortran, to SEGSPL with N=0; the core throws a controlled
// std::invalid_argument (C++ safety deviation).
void inputAddedCamberPoints(CamberSession& session,
                            std::span<const CamberInputPoint> points);

// Fortran: CAMB 'INPP' (446-526). Appends points to the existing
// added loading (NPADD grows), SORTDUP/FIXDUP, SEGSPL, CPCAM,
// CLX/CMX; LDCPLOT=true.
LoadingIntegrals inputAddedLoadingPoints(
    CamberSession& session, std::span<const CamberInputPoint> points);

// Fortran: CAMB 'RDAC' (xtcam.f 330-372) after a complete file read.
// The file reader (xfoil::io) returns the x/c,y/c pairs that were read;
// this kernel performs only the canonical preprocessing:
// - NCADD <= 1 or x span 0 -> zero camber (XCADD = XCAM(1),
// XCAM(NCAM); YCADD = 0) with RdacCamberStatus::noPointsFound;
// - otherwise normalization to x/c 0..1 (XCORG/XCSCL), re-origination in
// camber-line coordinates (consecutive lines, XCADD is updated before the
// YCADD line), SEGSPL, interpolation onto XCAM;
// - LDCPLOT = .FALSE.
// SORTDUP/FIXDUP are not executed, as in Fortran.
enum class RdacCamberStatus { applied, noPointsFound };
RdacCamberStatus readAddedCamber(CamberSession& session,
                                 std::span<const CamberInputPoint> points);

// Fortran: CAMB 100 block with LCLEAR=.FALSE. (TFAC/TSET/HIGH after GO TO 100,
// xtcam.f 85-106): geometry reference (SBL/XBL/YBL/XBCH/SBCH) and the dense
// XCAM grid are recomputed from the buffer; added camber/loading
// (XCADD/YCADD/XPADD/YPADD) and the dense arrays YCAM/YCAMP/PCAM/PCAMP
// remain canonically unchanged (Fortran leads them in the LCLEAR branch).
void refreshCamberSession(const GeometryDesignBuffer& buffer,
                          CamberSession& session);

// Fortran: CAMB 'CLR' (591-595): LCLEAR=.TRUE. -> back to the 100 block.
CamberSession clearAddedCamber(const GeometryDesignBuffer& buffer,
                               double xcmref = 0.25);

// Fortran: CAMB 'SCAL' (598-640): scales YCADD/YPADD/YCADDP/YPADDP
// (1..NCADD) and YCAM/YCAMP/PCAM/PCAMP (1..NCAM); CLX/CMX recomputed.
LoadingIntegrals scaleAddedCamber(CamberSession& session, double scal);

// Fortran: CAMB 'ADD' (643-683): accumulates the added camber on the
// camber line of the buffer contour (SOPPS/SEVAL, YB update, SCALC/SEGSPL x2,
// GEOPAR), afterwards LCLEAR=.TRUE. (session re-initialized).
// SBCH == 0 (degenerate contour) leads to division by zero in Fortran;
// the core aborts in a controlled way with invalidState (C++ safety deviation).
struct AddedCamberResult {
  // SOPPS failures of the ADD point loop (xtcam.f 650, one canonical
  // 'SOPPS: Opposite-point location failed' line per failure).
  std::size_t oppositePointFailures{0};
};
Result<AddedCamberResult> applyAddedCamber(CamberSession& session,
                                           GeometryDesignBuffer& buffer);

}  // namespace xfoil::core::geometry_design

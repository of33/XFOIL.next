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

// File: include/xfoil/core/geometry_design/GeometryDesignOps.h
// GDES buffer operations of the new 0-based cores,
// ported from the numerical sections of SUBROUTINE GDES
// (fortran-src/xgdes.f) and SUBROUTINE ABCOPY.
//
// The GDES menu-loop part (ASKC, cursor MODI/CORN/ADDP/MOVP/DELP/
// DIST/WIND, plot PLTINI/PLOTG/PLTAIR/PLNEWP, file OVER, Name/NINC)
// stays; this kernel models the numerical buffer operations:
// GSET -> setBufferFromAirfoil eXec -> executeGeometryBuffer
// SYMM -> setSymmetry ADEG/ARAD -> rotateBuffer
// Tran -> translateBuffer Scal -> scaleBuffer
// LINS -> linearYScale DERO -> derotateBuffer
// UNIT -> normalizeBuffer NMOV -> moveSurfaceNormals
// SINT -> splineArcLengthRespline CADD -> addCornerPoints
//
// Canonical invariant: these operations change only the
// buffer; only executeGeometryBuffer (ABCOPY,) carries it into the
// current contour. LGSAME handling follows the Fortran exactly (for ADEG/
// ARAD/SINT LGSAME stays unchanged).
//
// 0-based: Fortran XB(1..NB) <-> C++ x[0..nb-1].

#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "xfoil/core/Limits.h"
#include "xfoil/core/Result.h"
#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"
#include "xfoil/core/math/Diagnostic.h"
#include "xfoil/core/paneling/Abcopy.h"

namespace xfoil::core::geometry_design {

// IBX = 4*IQX = 2404 (XFOIL.INC): capacity of the buffer contour.
inline constexpr std::size_t kIbx = xfoil::core::bufferCapacity;

// Fortran: GDES 'GSET' (xgdes.f 175-194). Buffer := current contour
// (NB=N, XB/YB copy, LGSAME=true, SCALC/SEGSPL x2, GEOPAR; for LGSYM
// additionally ZERCAM). x/y equal length, NB >= 3 (ZERCAM/GEOPAR path).
// Return value: metrics of GSET's own GEOPAR output (including
// SOPPS counter) and, if ZERCAM ran (LGSYM active), its
// sub-results; otherwise nullopt.
struct BufferSetResult {
  geometry::GeometryMetrics metricsAfterGeopar;
  std::optional<ZerCamberResult> zercam;
};
Result<BufferSetResult> setBufferFromAirfoil(GeometryDesignBuffer& buffer,
                                             std::span<const double> x,
                                             std::span<const double> y);

// Fortran: GDES 'eXec' -> ABCOPY(.TRUE.) (xgdes.f 197-202).
// Copies the buffer contour to the current panel contour.
Result<paneling::AbcopyResult> executeGeometryBuffer(
    const GeometryDesignBuffer& buffer);

// Fortran: GDES 'SYMM' (xgdes.f 204-211). Sets LGSYM; for activation
// additionally ZERCAM. Return value: ZERCAM sub-results for activation,
// otherwise nullopt.
Result<std::optional<ZerCamberResult>> setSymmetry(
    GeometryDesignBuffer& buffer, bool enabled);

// Fortran: GDES 'ADEG'/'ARAD' (xgdes.f 215-242). Rotation around the
// origin (ROTATE), afterwards SEGSPL x2 (canonically without SCALC) and
// APX1BA/2BA/1BT/2BT -= ARAD. LGSAME stays unchanged.
void rotateBuffer(GeometryDesignBuffer& buffer, double arad);

// Fortran: GDES 'Tran' (xgdes.f 250-274). Translation, LGSAME=false.
void translateBuffer(GeometryDesignBuffer& buffer, double delx, double dely);

// Fortran: GDES 'Scal' (xgdes.f 277-334). Scaling around the origin;
// for XXFAC*YYFAC < 0 reverse the point order (to preserve the
// orientation), afterwards SCALC/SEGSPL x2/GEOPAR,
// LGSAME=false.
void scaleBuffer(GeometryDesignBuffer& buffer, double xxfac, double yyfac);

// Fortran: GDES 'LINS' (xgdes.f 337-386). Linearly varying
// y scaling between x/c positions XOC1/XOC2 (LEFIND/SEVAL for
// LE/TE). The core rejects |XOC1-XOC2| < 1.E-5 in a controlled way
// (Fortran: repeated prompt). A degenerate chord (XTE == XLE) is rejected as
// invalidState (C++ safety deviation). Afterwards
// SCALC/SEGSPL x2/GEOPAR, LGSAME=false.
Status linearYScale(GeometryDesignBuffer& buffer, double xoc1, double yfac1,
                    double xoc2, double yfac2);

// Fortran: GDES 'DERO' (xgdes.f 389-414). Derotation (chord line
// horizontal): LEFIND, ARAD = ATAN2(YTE-YLE, XTE-XLE), ROTATE,
// SCALC/SEGSPL x2/GEOPAR, LGSAME=false. Return value: ARAD (Fortran
// WRITE 1080, structured).
Result<double> derotateBuffer(GeometryDesignBuffer& buffer);

// Fortran: GDES 'UNIT' (xgdes.f 763-781). NORM (unit chord),
// LGSAME=false, SCALC/SEGSPL x2/GEOPAR.
Status normalizeBuffer(GeometryDesignBuffer& buffer);

// Fortran: GDES 'NMOV' (xgdes.f 723-756). Moves all points in
// surface-normal direction (ENX=YBP, ENY=-XBP, normalized).
// ENS == 0 is rejected as invalidState (cpp-safety deviation).
// Afterwards SCALC/SEGSPL x2/GEOPAR, LGSAME=false.
Status moveSurfaceNormals(GeometryDesignBuffer& buffer, double deln);

// Fortran: GDES 'SINT' (xgdes.f 934-940). SPLNXY: arc-length
// respline of the buffer contour (SB/XBP/YBP are set anew; XB/YB
// unchanged). LGSAME stays unchanged. Return value: SPLNXY
// diagnostics (limitReached after 10 passes).
math::Diagnostic splineArcLengthRespline(GeometryDesignBuffer& buffer);

// Fortran: GDES 'CADD' (xgdes.f 503-598). Corner refinement:
// CANG (default ATOL = 0.5*AMAX), optional uniform S (ISPL=1),
// AREFINE in the window [XRF1,XRF2], NB=NNEW, SCALC/SEGSPL x2/GEOPAR,
// LGSAME=false, final CANG. ISPL outside {1,2} is
// rejected in a controlled way (Fortran: repeated query). ndim = IBX.
struct CornerAddResult {
  std::size_t nbAdded{0};
  math::Diagnostic refineStatus{math::Diagnostic::completed};
  double maxAngleAfter{0.0};
};

Result<CornerAddResult> addCornerPoints(GeometryDesignBuffer& buffer,
                                        double atol, int ispl, double xrf1,
                                        double xrf2);

// Fortran: GDES 'CADD' with canonical defaults (ATOL=0.5*AMAX,
// ISPL=1, XRF = BBox +/- 10 %).
Result<CornerAddResult> addCornerPoints(GeometryDesignBuffer& buffer);

// Fortran: SUBROUTINE TGAP(RINPUT,NINPUT) (xgdes.f 1218-1308).
// Sets the TE gap of the buffer contour (LEFIND, TE-gap unit vector,
// exponential blend factor via x/c with DOC in [0,1]; DOC==0 only
// edge points). Afterwards SCALC/SEGSPL x2/GEOPAR, LGSAME=false. NB >= 3.
Status setTrailingEdgeGap(GeometryDesignBuffer& buffer, double gapnew,
                          double doc);

// Fortran: SUBROUTINE LERAD(RINPUT,NINPUT) (xgdes.f 1312-1373).
// LE radius scaling (LERSCL, DOC >= 0.001), afterwards respline/GEOPAR,
// LGSAME=false; return value: new LE radius 1/CVMAX from the
// mid range (canonical index window NB/4..3NB/4). CVMAX == 0
// is rejected as invalidState (C++ safety deviation).
Result<double> scaleLeadingEdgeRadius(GeometryDesignBuffer& buffer,
                                      double rfac, double doc);

// Fortran: SUBROUTINE SCLXY (xgdes.f 1377-1429; dead code in the source tree,
// revalidated). Scaling around LE, TE, or a
// given point (prompt selection dropped, origin explicit).
// Afterwards SCALC/SEGSPL x2/GEOPAR, LGSAME=false.
enum class ScaleOrigin { le, te, point };
void scaleAboutOrigin(GeometryDesignBuffer& buffer, ScaleOrigin origin,
                      double xorg, double yorg, double scl);

// Fortran: LOGICAL FUNCTION INSIDE(X,Y,N,XF,YF) (xgdes.f 1852-1876).
// Point-in-contour via the enclosed angle sum (|ANGLE| > 1).
// Coincidence of the point with a contour point returns NaN/Inf in
// Fortran (INSIDE = .FALSE.); the core returns controlled false in this
// case (C++ safety deviation). N >= 1.
bool insideContour(std::span<const double> x, std::span<const double> y,
                   double xf, double yf);

// Fortran: SUBROUTINE GETXYF(...) (xgdes.f 1880-1908), numerical
// path only. SINVRT at the hinge x position returns TOPS/BOTS and the
// associated y values. yf == 999.0 corresponds to the canonical
// y/t path; the relative value yrel is passed explicitly (prompt
// remains).
struct FlapHingeResult {
  double tops{0.0};
  double bots{0.0};
  double topy{0.0};
  double boty{0.0};
  double yf{0.0};
  math::Diagnostic topStatus{math::Diagnostic::completed};
  math::Diagnostic botStatus{math::Diagnostic::completed};
};

FlapHingeResult flapHinge(const GeometryDesignBuffer& buffer, double xf,
                          double yf, double yrel = 0.0);

// GETXYF numerics (xgdes.f 1887-1905) on an arbitrary contour
// (span facet; the buffer wrapper flapHinge delegates to it).
// Needed for the OPER FMOM path on the current contour.
FlapHingeResult flapHingeOnContour(std::span<const double> x,
                                   std::span<const double> xp,
                                   std::span<const double> y,
                                   std::span<const double> yp,
                                   std::span<const double> s, double xf,
                                   double yf, double yrel = 0.0);

// Fortran: SUBROUTINE FLAP(RINPUT,NINPUT) (xgdes.f 1433-1849).
// Flap deflection of the buffer contour: GETXYF/INSIDE, SSS break points,
// rotation around the hinge, point deletion/insertion at the
// break locations, SCHECK(STOL=0.2), SCALC/SEGSPL x2/GEOPAR;
// LGSAME=false, LBFLAP=true; LGSYM is canonically disabled.
// RDEF == 0 corresponds to the canonical early return (no change).
// Plot parts omitted. NB >= 3.
struct FlapResult {
  double xbf{0.0};
  double ybf{0.0};
  double tops{0.0};
  double bots{0.0};
  double atop{0.0};
  double abot{0.0};
  double st1{0.0};
  double st2{0.0};
  double sb1{0.0};
  double sb2{0.0};
  double xt1{0.0};
  double yt1{0.0};
  double xt2{0.0};
  double yt2{0.0};
  double xb1{0.0};
  double yb1{0.0};
  double xb2{0.0};
  double yb2{0.0};
  std::size_t nbBefore{0};
  std::size_t nbAfter{0};
  bool inside{false};
  bool symmetryDisabled{false};
  math::Diagnostic sssTopStatus{math::Diagnostic::completed};
  math::Diagnostic sssBotStatus{math::Diagnostic::completed};
};

Result<FlapResult> deflectFlap(GeometryDesignBuffer& buffer, double xbf,
                               double ybf, double ddef);

}  // namespace xfoil::core::geometry_design

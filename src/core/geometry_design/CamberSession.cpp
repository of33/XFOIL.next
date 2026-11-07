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

// File: src/core/geometry_design/CamberSession.cpp
// Numerical CAMB session kernel, ported from SUBROUTINE CAMB
// (fortran-src/xtcam.f, 100-/200 block and SETC/INPC/INPP/CLR/SCAL/ADD).

#include "xfoil/core/geometry_design/CamberSession.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry_design/CamberThickness.h"
#include "xfoil/core/math/Sort.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry_design {
namespace {

bool consistent(const GeometryDesignBuffer& b) {
  return b.x.size() == b.y.size() && b.x.size() == b.s.size() &&
         b.x.size() == b.xp.size() && b.x.size() == b.yp.size();
}

// CAMB 126-129: Interpolation the User splines on the density XCAM grid.
void interpolateCamber(CamberSession& s) {
  for (std::size_t k = 0; k < s.ncam; ++k) {
    s.ycam[k] = math::seval(s.xcam[k], s.ycadd, s.ycadddp, s.xcadd);
    s.ycamp[k] = math::deval(s.xcam[k], s.ycadd, s.ycadddp, s.xcadd);
  }
}

void interpolateLoading(CamberSession& s) {
  for (std::size_t k = 0; k < s.ncam; ++k) {
    s.pcam[k] = math::seval(s.xcam[k], s.ypadd, s.ypadddp, s.xpadd);
    s.pcamp[k] = math::deval(s.xcam[k], s.ypadd, s.ypadddp, s.xpadd);
  }
}

// CAMB 514-522/623-632: CLX/CMX the Added Loading (Trapezoid via XCAM).
LoadingIntegrals loadingIntegrals(const CamberSession& s) {
  LoadingIntegrals out;
  for (std::size_t k = 0; k + 1 < s.ncam; ++k) {
    const double dx = s.xcam[k + 1] - s.xcam[k];
    const double xa = 0.5 * (s.xcam[k + 1] + s.xcam[k]);
    const double pa = 0.5 * (s.pcam[k + 1] + s.pcam[k]);
    out.clx = out.clx + pa * dx;
    out.cmx = out.cmx + pa * dx * (s.xcmref - xa);
  }
  return out;
}

// CAMB 85-106: geometry reference and dense XCAM grid from the buffer.
// Executed from the 100 block in both LCLEAR branches.
void updateCamberGeometryReference(const GeometryDesignBuffer& buffer,
                                   CamberSession& s) {
  const std::size_t n = buffer.x.size();
  const geometry::LeadingEdgeResult xl =
      geometry::xlfind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  s.sbl = xl.sle;
  s.xlStatus = xl.status;
  s.xbl = math::seval(s.sbl, buffer.x, buffer.xp, buffer.s);
  s.ybl = math::seval(s.sbl, buffer.y, buffer.yp, buffer.s);
  const double xbr = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
  s.xbch = xbr - s.xbl;
  s.ybch = 0.0;
  s.sbch = std::sqrt(s.xbch * s.xbch + s.ybch * s.ybch);

  const ThicknessCamberDistribution dist = getCamberThickness(
      buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  s.ncam = std::min(kNcamMax, kNtx);
  s.xcam.assign(s.ncam, 0.0);
  for (std::size_t k = 0; k < s.ncam; ++k) {
    s.xcam[k] = dist.xcm.front() +
                (dist.xcm.back() - dist.xcm.front()) *
                    static_cast<double>(k) /
                    static_cast<double>(s.ncam - 1);
  }
}

}  // namespace

CamberSession initCamberSession(const GeometryDesignBuffer& buffer,
                                double xcmref) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "initCamberSession: consistent lengths and NB >= 3 required");
  }
  CamberSession s;
  s.xcmref = xcmref;

  // CAMB 85-106: leftmost point, "chord line" axis (YBCH canonical 0),
  // current distributions, dense XCAM grid.
  updateCamberGeometryReference(buffer, s);

  // CAMB 108-131 (LCLEAR): Added camber/loading on Zero set.
  s.ncadd = 2;
  s.xcadd = {s.xcam[0], s.xcam[s.ncam - 1]};
  s.ycadd = {0.0, 0.0};
  s.ycadddp.assign(s.ncadd, 0.0);
  math::segspl(s.ycadd, s.xcadd, s.ycadddp);
  s.npadd = 2;
  s.xpadd = {s.xcam[0], s.xcam[s.ncam - 1]};
  s.ypadd = {0.0, 0.0};
  s.ypadddp.assign(s.npadd, 0.0);
  math::segspl(s.ypadd, s.xpadd, s.ypadddp);
  s.ycam.assign(s.ncam, 0.0);
  s.ycamp.assign(s.ncam, 0.0);
  s.pcam.assign(s.ncam, 0.0);
  s.pcamp.assign(s.ncam, 0.0);
  interpolateCamber(s);
  interpolateLoading(s);
  return s;
}

void refreshCamberSession(const GeometryDesignBuffer& buffer,
                          CamberSession& session) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "refreshCamberSession: consistent lengths and NB >= 3 required");
  }
  // CAMB 85-106 with LCLEAR=.FALSE.: only the geometry reference and XCAM
  // grid are renewed; added arrays and density arrays remain unchanged.
  updateCamberGeometryReference(buffer, session);
}

void setAddedCamberFromCamberline(CamberSession& session,
                                  const GeometryDesignBuffer& buffer) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "setAddedCamberFromCamberline: consistent lengths and "
        "NB >= 3 required");
  }
  // CAMB 380-392: Added camber = current Camberline.
  const ThicknessCamberDistribution dist = getCamberThickness(
      buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  session.ncadd = dist.ncm;
  session.xcadd = dist.xcm;
  session.ycadd = dist.ycm;
  session.ycadddp.assign(session.ncadd, 0.0);
  math::segspl(session.ycadd, session.xcadd, session.ycadddp);
  interpolateCamber(session);
  session.ldCplot = false;
}

void inputAddedCamberPoints(CamberSession& session,
                            std::span<const CamberInputPoint> points) {
  // cpp-safety deviation: Fortran calls SEGSPL with NCADD=0 (out-of-bounds
  // read); the core rejects this in a controlled way before the state changes.
  if (points.size() < 2) {
    throw std::invalid_argument(
        "inputAddedCamberPoints: at least 2 points required");
  }
  // CAMB 408-420: NCADD=0, Points explicit (instead of Keyboard-READ),
  // x/c clamp, Mapping on Camberline coordinates.
  session.ncadd = 0;
  session.xcadd.clear();
  session.ycadd.clear();
  session.xcadd.reserve(points.size());
  session.ycadd.reserve(points.size());
  for (const CamberInputPoint& p : points) {
    double xx = p.x;
    if (xx <= 0.0) {
      xx = 0.0;
    } else if (xx >= 1.0) {
      xx = 1.0;
    }
    session.xcadd.push_back(session.xcam[0] + xx * session.xbch -
                            p.y * session.ybch);
    session.ycadd.push_back(xx * session.ybch + p.y * session.xbch);
  }
  session.ncadd = session.xcadd.size();
  // CAMB 431-435: SORTDUP/FIXDUP, SEGSPL, Interpolation.
  static_cast<void>(math::sortDup(session.xcadd, session.ycadd));
  math::fixDup(session.ncadd, session.xcadd, session.ycadd);
  session.ycadddp.assign(session.ncadd, 0.0);
  math::segspl(session.ycadd, session.xcadd, session.ycadddp);
  interpolateCamber(session);
  session.ldCplot = false;
}

LoadingIntegrals inputAddedLoadingPoints(
    CamberSession& session, std::span<const CamberInputPoint> points) {
  // CAMB 471-482: NPADD append on existing Added loading (NPAPPEND grows).
  for (const CamberInputPoint& p : points) {
    double xx = p.x;
    if (xx <= 0.0) {
      xx = 0.0;
    } else if (xx >= 1.0) {
      xx = 1.0;
    }
    session.xpadd.push_back(session.xcam[0] + xx * session.xbch);
    session.ypadd.push_back(p.y);
  }
  session.npadd = session.xpadd.size();
  if (session.npadd < 2) {
    throw std::invalid_argument(
        "inputAddedLoadingPoints: at least 2 points required");
  }
  // CAMB 495-511: SORTDUP/FIXDUP, SEGSPL, Interpolation, CPCAM.
  static_cast<void>(math::sortDup(session.xpadd, session.ypadd));
  math::fixDup(session.npadd, session.xpadd, session.ypadd);
  session.ypadddp.assign(session.npadd, 0.0);
  math::segspl(session.ypadd, session.xpadd, session.ypadddp);
  interpolateLoading(session);
  const CamberLoadingResult r =
      camberFromLoading(session.xcam, session.pcam, session.pcamp);
  session.ycam = r.y;
  session.ycamp = r.dydx;
  session.ldCplot = true;
  // CAMB 513-523: CLX/CMX.
  return loadingIntegrals(session);
}

RdacCamberStatus readAddedCamber(CamberSession& session,
                                 std::span<const CamberInputPoint> points) {
  // CAMB 342-349: NCADD <= 1 or x span 0 -> zero camber, message,
  // GO TO 100. Fortran does not set YCADDP anew in this case (stale); the core
  // initializes it to 0 (cpp-safety deviation, unobservable:
  // YCADDP is only read after a renewed SEGSPL in SETC/INPC/RDAC).
  if (points.size() <= 1 ||
      (points.back().x - points.front().x) == 0.0) {
    session.ncadd = 2;
    session.xcadd = {session.xcam[0], session.xcam[session.ncam - 1]};
    session.ycadd = {0.0, 0.0};
    session.ycadddp.assign(session.ncadd, 0.0);
    return RdacCamberStatus::noPointsFound;
  }

  // CAMB 351-357: Normalization on x/c 0..1.
  const double xcorg = points.front().x;
  const double xcscl = points.back().x - xcorg;
  session.ncadd = points.size();
  session.xcadd.assign(session.ncadd, 0.0);
  session.ycadd.assign(session.ncadd, 0.0);
  for (std::size_t k = 0; k < session.ncadd; ++k) {
    session.xcadd[k] = (points[k].x - xcorg) / xcscl;
    session.ycadd[k] = points[k].y / xcscl;
  }

  // CAMB 359-361: Re-origination in Camberline coordinates. Fortran
  // updated XCADD before the YCADD line; the YCADD line used
  // the already updated XCADD values (the order is part of the
  // contract).
  for (std::size_t k = 0; k < session.ncadd; ++k) {
    session.xcadd[k] = session.xcam[0] + session.xcadd[k] * session.xbch -
                       session.ycadd[k] * session.ybch;
    session.ycadd[k] = session.xcadd[k] * session.ybch +
                       session.ycadd[k] * session.xbch;
  }

  // CAMB 363-368: SEGSPL, Interpolation on XCAM; no SORTDUP/FIXDUP.
  session.ycadddp.assign(session.ncadd, 0.0);
  math::segspl(session.ycadd, session.xcadd, session.ycadddp);
  interpolateCamber(session);
  session.ldCplot = false;
  return RdacCamberStatus::applied;
}

CamberSession clearAddedCamber(const GeometryDesignBuffer& buffer,
                               double xcmref) {
  // CAMB 591-595: LCLEAR=.TRUE. -> 100 block again.
  return initCamberSession(buffer, xcmref);
}

LoadingIntegrals scaleAddedCamber(CamberSession& session, double scal) {
  // CAMB 608-621: Fortran scales both user-array pairs in a
  // loop DO I = 1, NCADD. YPADD/YPADDP are also scaled only up to NCADD;
  // entries NCADD+1..NPADD remain unscaled (observable
  // at the next INPP: SEGSPL/SEVAL via YPADD, CLX/CMX, format 1110).
  // Entries NPADD+1..NCADD lie in Fortran in the NTX padding and are
  // never read by SEGSPL/SEVAL; the core does not scale them (no
  // padding, no observation).
  const std::size_t userBound = std::min(session.ncadd, session.npadd);
  for (std::size_t i = 0; i < session.ncadd; ++i) {
    session.ycadd[i] = session.ycadd[i] * scal;
    session.ycadddp[i] = session.ycadddp[i] * scal;
  }
  for (std::size_t i = 0; i < userBound; ++i) {
    session.ypadd[i] = session.ypadd[i] * scal;
    session.ypadddp[i] = session.ypadddp[i] * scal;
  }
  for (std::size_t i = 0; i < session.ncam; ++i) {
    session.ycam[i] = session.ycam[i] * scal;
    session.ycamp[i] = session.ycamp[i] * scal;
    session.pcam[i] = session.pcam[i] * scal;
    session.pcamp[i] = session.pcamp[i] * scal;
  }
  // CAMB 623-633: CLX/CMX (Fortran prints only for!= 0).
  return loadingIntegrals(session);
}

Result<AddedCamberResult> applyAddedCamber(CamberSession& session,
                                           GeometryDesignBuffer& buffer) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    return Result<AddedCamberResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "applyAddedCamber: consistent lengths and "
                    "NB >= 3 required"));
  }
  // cpp-safety deviation: Fortran divides by zero for SBCH == 0.
  if (session.sbch == 0.0) {
    return Result<AddedCamberResult>::fail(
        Error::make(ErrorCode::invalidState,
                    "applyAddedCamber: degenerate chord line (SBCH == 0)"));
  }

  // CAMB 645-664: accumulate added camber on each contour side.
  math::segspl(session.ycam, session.xcam, session.ycamp);
  AddedCamberResult result;
  std::vector<double> w1(n);
  for (std::size_t i = 0; i < n; ++i) {
    const geometry::OppositePointResult opp =
        geometry::sopps(buffer.s[i], buffer.x, buffer.xp, buffer.y, buffer.yp,
                        buffer.s, session.sbl);
    if (opp.status != math::Diagnostic::completed) {
      ++result.oppositePointFailures;
    }
    const double xbopp = math::seval(opp.sopp, buffer.x, buffer.xp, buffer.s);
    const double ybopp = math::seval(opp.sopp, buffer.y, buffer.yp, buffer.s);
    const double oldcam =
        0.5 * (buffer.y[i] + ybopp) * session.xbch / session.sbch -
        0.5 * (buffer.x[i] + xbopp) * session.ybch / session.sbch;
    const double cam =
        oldcam + math::seval(buffer.x[i], session.ycam, session.ycamp,
                             session.xcam);
    w1[i] = cam + 0.5 * (buffer.y[i] - ybopp);
  }
  for (std::size_t i = 0; i < n; ++i) {
    buffer.y[i] = w1[i];
  }

  // CAMB 670-683: Respline, GEOPAR, afterwards LCLEAR=.TRUE. (Session new).
  math::scalc(buffer.x, buffer.y, buffer.s);
  math::segspl(buffer.x, buffer.s, buffer.xp);
  math::segspl(buffer.y, buffer.s, buffer.yp);
  buffer.metrics =
      geometry::geopar(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  session = initCamberSession(buffer, session.xcmref);
  return Result<AddedCamberResult>::ok(result);
}

}  // namespace xfoil::core::geometry_design

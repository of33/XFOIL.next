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

// File: src/core/geometry_design/GeometryDesignBuffer.cpp
// Port of THKCAM and ZERCAM from fortran-src/xtcam.f and of the
// buffer state type. 0-based.

#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/geometry_design/CamberThickness.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry_design {

GeometryDesignBuffer::GeometryDesignBuffer(std::vector<double> x_,
                                           std::vector<double> y_,
                                           std::vector<double> s_,
                                           std::vector<double> xp_,
                                           std::vector<double> yp_)
    : x(std::move(x_)),
      y(std::move(y_)),
      s(std::move(s_)),
      xp(std::move(xp_)),
      yp(std::move(yp_)) {
  if (x.size() != y.size() || x.size() != s.size() || x.size() != xp.size() ||
      x.size() != yp.size()) {
    throw std::invalid_argument(
        "GeometryDesignBuffer: x/y/s/xp/yp must be equal in length");
  }
}

namespace {

bool consistentSizes(const GeometryDesignBuffer& buffer) {
  const std::size_t n = buffer.x.size();
  return n == buffer.y.size() && n == buffer.s.size() &&
         n == buffer.xp.size() && n == buffer.yp.size();
}

}  // namespace

Result<std::size_t> transformThicknessCamber(GeometryDesignBuffer& buffer,
                                             double tfac, double cfac) {
  const std::size_t n = buffer.x.size();
  if (!consistentSizes(buffer) || n < 3) {
    return Result<std::size_t>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "transformThicknessCamber: consistent lengths and "
                    "NB >= 3 required"));
  }

  // Fortran 1034: LEFIND (status is ignored numerically as in Fortran).
  const geometry::LeadingEdgeResult le =
      geometry::lefind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  const double sble = le.sle;

  // Fortran 1045-1052: chord unit vector.
  const double xle = math::seval(sble, buffer.x, buffer.xp, buffer.s);
  const double yle = math::seval(sble, buffer.y, buffer.yp, buffer.s);
  const double xte = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
  const double yte = 0.5 * (buffer.y[0] + buffer.y[n - 1]);
  const double chord =
      std::sqrt((xte - xle) * (xte - xle) + (yte - yle) * (yte - yle));
  // cpp-safety deviation: Fortran divides by zero for CHORD == 0
  // (NaN/Inf); the core rejects this before each mutation.
  if (chord == 0.0) {
    return Result<std::size_t>::fail(Error::make(
        ErrorCode::invalidState,
        "transformThicknessCamber: degenerate chord (CHORD == 0)"));
  }
  const double dxc = (xte - xle) / chord;
  const double dyc = (yte - yle) / chord;

  // Fortran 1055-1070: for each point, the opposite point and the
  // new coordinates in W1/W2.
  std::size_t oppositePointFailures = 0;
  std::vector<double> w1(n);
  std::vector<double> w2(n);
  for (std::size_t i = 0; i < n; ++i) {
    const geometry::OppositePointResult opp = geometry::sopps(
        buffer.s[i], buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s, sble);
    if (opp.status != math::Diagnostic::completed) {
      // Fortran: WRITE(*,*) 'SOPPS: Opposite-point location failed.
      // Continuing...' (the kernel only counts; the CLI text follows later).
      ++oppositePointFailures;
    }
    const double sbopp = opp.sopp;
    const double xbopp = math::seval(sbopp, buffer.x, buffer.xp, buffer.s);
    const double ybopp = math::seval(sbopp, buffer.y, buffer.yp, buffer.s);

    const double xcavg = 0.5 * (buffer.x[i] + xbopp) * dxc +
                         0.5 * (buffer.y[i] + ybopp) * dyc;
    const double ycavg = cfac * (0.5 * (buffer.y[i] + ybopp) * dxc -
                                 0.5 * (buffer.x[i] + xbopp) * dyc);
    const double xcdel = 0.5 * (buffer.x[i] - xbopp) * dxc +
                         0.5 * (buffer.y[i] - ybopp) * dyc;
    const double ycdel = tfac * (0.5 * (buffer.y[i] - ybopp) * dxc -
                                 0.5 * (buffer.x[i] - xbopp) * dyc);
    w1[i] = (xcavg + xcdel) * dxc - (ycavg + ycdel) * dyc;
    w2[i] = (ycavg + ycdel) * dxc + (xcavg + xcdel) * dyc;
  }

  // Fortran 1072-1086: copy back, LGSAME=false, respline, GEOPAR.
  buffer.x = std::move(w1);
  buffer.y = std::move(w2);
  buffer.lgSame = false;
  math::scalc(buffer.x, buffer.y, buffer.s);
  math::segspl(buffer.x, buffer.s, buffer.xp);
  math::segspl(buffer.y, buffer.s, buffer.yp);
  buffer.metrics =
      geometry::geopar(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  return Result<std::size_t>::ok(oppositePointFailures);
}

Result<ZerCamberResult> zerCamber(GeometryDesignBuffer& buffer) {
  const std::size_t n0 = buffer.x.size();
  if (!consistentSizes(buffer) || n0 < 3) {
    return Result<ZerCamberResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "zerCamber: consistent lengths and NB >= 3 required"));
  }

  // Fortran 856-858: TFAC=1, CFAC=0, THKCAM.
  const Result<std::size_t> thk =
      transformThicknessCamber(buffer, 1.0, 0.0);
  if (thk.isErr()) {
    return Result<ZerCamberResult>::fail(thk.error());
  }
  ZerCamberResult result;
  result.thkcamOppositePointFailures = thk.value();
  // Fortran 863-867: GEOPAR in THKCAM (metrics of the first printout).
  result.metricsAfterThkcam = buffer.metrics;

  // Fortran 861: YSYM with capacity 2*IQX, ISIDE=1 (upper side as source).
  // Core capacity: 2*NB (NNEW = 2*NSIDE-1 <= 2*NB-1).
  std::size_t n = buffer.x.size();
  const std::size_t cap = 2 * n;
  buffer.x.resize(cap);
  buffer.xp.resize(cap);
  buffer.y.resize(cap);
  buffer.yp.resize(cap);
  buffer.s.resize(cap);
  std::vector<double> xnew(cap);
  std::vector<double> ynew(cap);
  const Status ys =
      geometry::ysym(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s, n, 1,
                     xnew, ynew);
  buffer.x.resize(n);
  buffer.xp.resize(n);
  buffer.y.resize(n);
  buffer.yp.resize(n);
  buffer.s.resize(n);
  if (ys.isErr()) {
    return Result<ZerCamberResult>::fail(ys.error());
  }

  // Fortran 863-867: GEOPAR again.
  buffer.metrics =
      geometry::geopar(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  return Result<ZerCamberResult>::ok(std::move(result));
}

HighPointResult moveHighPoints(GeometryDesignBuffer& buffer, double thpnt,
                               double chpnt) {
  const std::size_t n = buffer.x.size();
  if (!consistentSizes(buffer) || n < 3) {
    throw std::invalid_argument(
        "moveHighPoints: consistent lengths and NB >= 3 required");
  }
  HighPointResult result;

  // Fortran 1105-1115: chord-line angle (warning for |AROT| > 1 degree).
  constexpr double kDtor = 0.017453292519943295;  // COMMON CR11 DTOR = PI/180
  const double xle =
      math::seval(buffer.metrics.sle, buffer.x, buffer.xp, buffer.s);
  const double yle =
      math::seval(buffer.metrics.sle, buffer.y, buffer.yp, buffer.s);
  const double xte = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
  const double yte = 0.5 * (buffer.y[0] + buffer.y[n - 1]);
  result.chordlineAngleDeg =
      std::atan2(yle - yte, xte - xle) / kDtor;

  // Fortran 1119-1121: leftmost point.
  const geometry::LeadingEdgeResult xl =
      geometry::xlfind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  const double sbl = xl.sle;
  const double xbl = math::seval(sbl, buffer.x, buffer.xp, buffer.s);
  const double ybl = math::seval(sbl, buffer.y, buffer.yp, buffer.s);

  // Fortran 1126-1131: current Distributions and Maxima.
  ThicknessCamberDistribution dist =
      getCamberThickness(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  result.oppositePointFailuresBefore = dist.oppositePointFailures;
  std::vector<double> ycmp(dist.ncm, 0.0);
  std::vector<double> ytkp(dist.ntk, 0.0);
  const MaxPointResult cam = getMaxPoint(dist.xcm, dist.ycm, ycmp);
  const MaxPointResult thk = getMaxPoint(dist.xtk, dist.ytk, ytkp);
  result.camberMaxBefore = cam.max;
  result.camberStatus = cam.status;
  result.thickMaxBefore = thk.max;
  result.thickStatus = thk.status;

  // Fortran 1163-1164: <= 0 keeps the current maximum location.
  if (thpnt <= 0.0) thpnt = thk.max.x;
  if (chpnt <= 0.0) chpnt = cam.max.x;

  // Fortran 1174-1185: cubic X mapping of the thickness distribution.
  if (thpnt > 0.0) {
    const std::array<double, 3> xfn{dist.xtk.front(), thk.max.x,
                                    dist.xtk.back()};
    const std::array<double, 3> yfn{dist.xtk.front(), thpnt, dist.xtk.back()};
    std::array<double, 3> yfnp{};
    math::splina(yfn, xfn, yfnp);
    for (std::size_t i = 0; i < dist.ntk; ++i) {
      dist.xtk[i] = math::seval(dist.xtk[i], yfn, yfnp, xfn);
    }
  }
  // Fortran 1188-1199: cubic X mapping of the camber distribution.
  if (chpnt > 0.0) {
    const std::array<double, 3> xfn{dist.xcm.front(), cam.max.x,
                                    dist.xcm.back()};
    const std::array<double, 3> yfn{dist.xcm.front(), chpnt, dist.xcm.back()};
    std::array<double, 3> yfnp{};
    math::splina(yfn, xfn, yfnp);
    for (std::size_t i = 0; i < dist.ncm; ++i) {
      dist.xcm[i] = math::seval(dist.xcm[i], yfn, yfnp, xfn);
    }
  }

  // Fortran 1210-1211: Y(X)-Splines the new Distributions.
  std::vector<double> ytkp2(dist.ntk);
  std::vector<double> ycmp2(dist.ncm);
  math::segspl(dist.ytk, dist.xtk, ytkp2);
  math::segspl(dist.ycm, dist.xcm, ycmp2);

  // Fortran 1215-1228: new YB from Thickness/Camber on the original
  // XB points, Sign via SB against via SBL, plus YBL offset.
  for (std::size_t i = 0; i < n; ++i) {
    const double ycc = math::seval(buffer.x[i], dist.ycm, ycmp2, dist.xcm);
    const double ytt = math::seval(buffer.x[i], dist.ytk, ytkp2, dist.xtk);
    if (buffer.s[i] <= sbl) {
      buffer.y[i] = ycc + ytt;
    } else {
      buffer.y[i] = ycc - ytt;
    }
    buffer.y[i] = buffer.y[i] + ybl;
  }
  static_cast<void>(xbl);

  // Fortran 1230-1240: LGSAME=false, respline, GEOPAR.
  buffer.lgSame = false;
  math::scalc(buffer.x, buffer.y, buffer.s);
  math::segspl(buffer.x, buffer.s, buffer.xp);
  math::segspl(buffer.y, buffer.s, buffer.yp);
  buffer.metrics =
      geometry::geopar(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  return result;
}

}  // namespace xfoil::core::geometry_design

// End: src/core/geometry_design/GeometryDesignBuffer.cpp

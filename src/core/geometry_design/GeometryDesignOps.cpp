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

// File: src/core/geometry_design/GeometryDesignOps.cpp
// GDES buffer operations, ported from SUBROUTINE GDES
// (fortran-src/xgdes.f).

#include "xfoil/core/geometry_design/GeometryDesignOps.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry_design {
namespace {

bool consistent(const GeometryDesignBuffer& b) {
  return b.x.size() == b.y.size() && b.x.size() == b.s.size() &&
         b.x.size() == b.xp.size() && b.x.size() == b.yp.size();
}

Status validate(const GeometryDesignBuffer& b, const char* what) {
  if (!consistent(b) || b.x.size() < 3) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    std::string(what) +
                                        ": consistent lengths and "
                                        "NB >= 3 required"));
  }
  return Status::ok();
}

// GDES 182-190: SCALC, SEGSPL x2, GEOPAR.
void resplineAndPar(GeometryDesignBuffer& b) {
  math::scalc(b.x, b.y, b.s);
  math::segspl(b.x, b.s, b.xp);
  math::segspl(b.y, b.s, b.yp);
  b.metrics = geometry::geopar(b.x, b.xp, b.y, b.yp, b.s);
}

}  // namespace

Result<BufferSetResult> setBufferFromAirfoil(GeometryDesignBuffer& buffer,
                                             std::span<const double> x,
                                             std::span<const double> y) {
  if (x.size() != y.size() || x.size() < 3) {
    return Result<BufferSetResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "setBufferFromAirfoil: x/y equal long and "
                    "NB >= 3 required"));
  }
  // GDES 176-181: NB=N, XB/YB copy, LGSAME=true.
  buffer.x.assign(x.begin(), x.end());
  buffer.y.assign(y.begin(), y.end());
  buffer.s.assign(x.size(), 0.0);
  buffer.xp.assign(x.size(), 0.0);
  buffer.yp.assign(x.size(), 0.0);
  buffer.lgSame = true;
  resplineAndPar(buffer);
  BufferSetResult result;
  // GDES 188: GEOPAR printout (metrics including the secured SOPPS counter).
  result.metricsAfterGeopar = buffer.metrics;
  // GDES 194: for LGSYM additionally ZERCAM.
  if (buffer.lgSym) {
    auto zc = zerCamber(buffer);
    if (zc.isErr()) {
      return Result<BufferSetResult>::fail(zc.error());
    }
    result.zercam = std::move(zc.value());
  }
  return Result<BufferSetResult>::ok(std::move(result));
}

Result<paneling::AbcopyResult> executeGeometryBuffer(
    const GeometryDesignBuffer& buffer) {
  if (!consistent(buffer)) {
    return Result<paneling::AbcopyResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "executeGeometryBuffer: consistent lengths "
                    "required"));
  }
  // GDES 199: ABCOPY(.TRUE.); LCONF controls only the confirmation output.
  const geometry::AirfoilBuffer ab(buffer.x, buffer.y, std::string());
  return paneling::abcopy(ab);
}

Result<std::optional<ZerCamberResult>> setSymmetry(
    GeometryDesignBuffer& buffer, bool enabled) {
  // GDES 205-211: toggle LGSYM; on activation ZERCAM.
  buffer.lgSym = enabled;
  if (enabled) {
    auto zc = zerCamber(buffer);
    if (zc.isErr()) {
      return Result<std::optional<ZerCamberResult>>::fail(zc.error());
    }
    return Result<std::optional<ZerCamberResult>>::ok(
        std::optional<ZerCamberResult>(std::move(zc.value())));
  }
  return Result<std::optional<ZerCamberResult>>::ok(
      std::optional<ZerCamberResult>{});
}

void rotateBuffer(GeometryDesignBuffer& buffer, double arad) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "rotateBuffer: consistent lengths and NB >= 3 required");
  }
  // GDES 234-242: ROTATE, SEGSPL x2 (canonical without SCALC), APX angle.
  geometry::rotate(buffer.x, buffer.y, arad);
  math::segspl(buffer.x, buffer.s, buffer.xp);
  math::segspl(buffer.y, buffer.s, buffer.yp);
  buffer.metrics.apx1a = buffer.metrics.apx1a - arad;
  buffer.metrics.apx2a = buffer.metrics.apx2a - arad;
  buffer.metrics.apx1t = buffer.metrics.apx1t - arad;
  buffer.metrics.apx2t = buffer.metrics.apx2t - arad;
}

void translateBuffer(GeometryDesignBuffer& buffer, double delx, double dely) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "translateBuffer: consistent lengths and NB >= 3 required");
  }
  // GDES 265-274: XB/YB shift, LGSAME=false (no respline).
  for (std::size_t i = 0; i < n; ++i) {
    buffer.x[i] = buffer.x[i] + delx;
    buffer.y[i] = buffer.y[i] + dely;
  }
  buffer.lgSame = false;
}

void scaleBuffer(GeometryDesignBuffer& buffer, double xxfac, double yyfac) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "scaleBuffer: consistent lengths and NB >= 3 required");
  }
  // GDES 302-305: scaling around the origin.
  for (std::size_t i = 0; i < n; ++i) {
    buffer.x[i] = buffer.x[i] * xxfac;
    buffer.y[i] = buffer.y[i] * yyfac;
  }
  // GDES 308-317: invert point order on sign change.
  if (xxfac * yyfac < 0.0) {
    for (std::size_t i = 0; i < n / 2; ++i) {
      const std::size_t j = n - 1 - i;
      std::swap(buffer.x[i], buffer.x[j]);
      std::swap(buffer.y[i], buffer.y[j]);
    }
  }
  // GDES 320-334: respline, GEOPAR, LGSAME=false.
  resplineAndPar(buffer);
  buffer.lgSame = false;
}

Status linearYScale(GeometryDesignBuffer& buffer, double xoc1, double yfac1,
                    double xoc2, double yfac2) {
  Status st = validate(buffer, "linearYScale");
  if (!st.isOk()) return st;
  const std::size_t n = buffer.x.size();
  // GDES 352-356: XOC1/XOC2 must be different.
  if (std::abs(xoc1 - xoc2) < 1.0E-5) {
    return Status::fail(
        Error::make(ErrorCode::invalidArgument,
                    "linearYScale: x/c-Positionen 1 and 2 must "
                    "different be"));
  }
  // GDES 358-362: LE/TE positions.
  const geometry::LeadingEdgeResult le =
      geometry::lefind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  const double xle = math::seval(le.sle, buffer.x, buffer.xp, buffer.s);
  const double xte = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
  // cpp-safety deviation: Fortran divides by zero for XTE == XLE.
  if (xte == xle) {
    return Status::fail(
        Error::make(ErrorCode::invalidState,
                    "linearYScale: degenerate chord (XTE == XLE)"));
  }
  // GDES 363-369: y-scaling per point.
  for (std::size_t i = 0; i < n; ++i) {
    const double xoc = (buffer.x[i] - xle) / (xte - xle);
    const double fr1 = (xoc2 - xoc) / (xoc2 - xoc1);
    const double fr2 = (xoc - xoc1) / (xoc2 - xoc1);
    const double yyfac = fr1 * yfac1 + fr2 * yfac2;
    buffer.y[i] = buffer.y[i] * yyfac;
  }
  // GDES 372-386: respline, GEOPAR, LGSAME=false.
  resplineAndPar(buffer);
  buffer.lgSame = false;
  return Status::ok();
}

Result<double> derotateBuffer(GeometryDesignBuffer& buffer) {
  Status st = validate(buffer, "derotateBuffer");
  if (!st.isOk()) {
    return Result<double>::fail(st.error());
  }
  const std::size_t n = buffer.x.size();
  // GDES 390-398: LEFIND, ARAD = ATAN2(YTE-YLE, XTE-XLE), ROTATE.
  const geometry::LeadingEdgeResult le =
      geometry::lefind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  const double xle = math::seval(le.sle, buffer.x, buffer.xp, buffer.s);
  const double yle = math::seval(le.sle, buffer.y, buffer.yp, buffer.s);
  const double xte = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
  const double yte = 0.5 * (buffer.y[0] + buffer.y[n - 1]);
  const double arad = std::atan2(yte - yle, xte - xle);
  geometry::rotate(buffer.x, buffer.y, arad);
  // GDES 401-414: respline, GEOPAR, LGSAME=false.
  resplineAndPar(buffer);
  buffer.lgSame = false;
  return Result<double>::ok(arad);
}

Status normalizeBuffer(GeometryDesignBuffer& buffer) {
  Status st = validate(buffer, "normalizeBuffer");
  if (!st.isOk()) return st;
  // GDES 764-776: NORM, LGSAME=false, SCALC/SEGSPL x2, GEOPAR.
  geometry::norm(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  buffer.lgSame = false;
  resplineAndPar(buffer);
  return Status::ok();
}

Status moveSurfaceNormals(GeometryDesignBuffer& buffer, double deln) {
  Status st = validate(buffer, "moveSurfaceNormals");
  if (!st.isOk()) return st;
  const std::size_t n = buffer.x.size();
  // GDES 731-739: surface normal shift.
  for (std::size_t i = 0; i < n; ++i) {
    const double enx = buffer.yp[i];
    const double eny = -buffer.xp[i];
    const double ens = std::sqrt(enx * enx + eny * eny);
    // cpp-safety deviation: Fortran divides by zero for ENS == 0.
    if (ens == 0.0) {
      return Status::fail(
          Error::make(ErrorCode::invalidState,
                      "moveSurfaceNormals: degenerate normal length"));
    }
    buffer.x[i] = buffer.x[i] + enx / ens * deln;
    buffer.y[i] = buffer.y[i] + eny / ens * deln;
  }
  // GDES 742-756: respline, GEOPAR, LGSAME=false.
  resplineAndPar(buffer);
  buffer.lgSame = false;
  return Status::ok();
}

math::Diagnostic splineArcLengthRespline(GeometryDesignBuffer& buffer) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 2) {
    throw std::invalid_argument(
        "splineArcLengthRespline: consistent lengths and NB >= 2 "
        "required");
  }
  // GDES 935: SPLNXY sets SB/XBP/YBP anew (XB/YB unchanged);
  // LGSAME stays unchanged.
  return math::splnxy(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
}

Result<CornerAddResult> addCornerPoints(GeometryDesignBuffer& buffer,
                                        double atol, int ispl, double xrf1,
                                        double xrf2) {
  Status st = validate(buffer, "addCornerPoints");
  if (!st.isOk()) {
    return Result<CornerAddResult>::fail(st.error());
  }
  // GDES 548-549: ISPL outside {1,2} leads to the repeated read-only query
  // only in the prompt branch. In the inline branch (xgdes.f 527-537),
  // ISPL = IINPUT(2) is adopted; a failed GETINT returns 0. Only
  // ISPL == 1 sets the uniform S (GDES 558-559); all other values
  // retain the current arc-length S (GDES 557 IF(ISPL.EQ.1)).
  const std::size_t n = buffer.x.size();
  // GDES 558-569: uniform S for ISPL=1 (identical points retain S).
  if (ispl == 1) {
    buffer.s[0] = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
      if (buffer.x[i] == buffer.x[i - 1] && buffer.y[i] == buffer.y[i - 1]) {
        buffer.s[i] = buffer.s[i - 1];
      } else {
        buffer.s[i] = buffer.s[i - 1] + 1.0;
      }
    }
    math::segspl(buffer.x, buffer.s, buffer.xp);
    math::segspl(buffer.y, buffer.s, buffer.yp);
  }
  // GDES 571-592: AREFINE, NB=NNEW, respline, GEOPAR, LGSAME=false.
  const geometry::RefineResult ref =
      geometry::arefine(buffer.x, buffer.y, buffer.s, buffer.xp, buffer.yp,
                        atol, kIbx, xrf1, xrf2);
  CornerAddResult out;
  out.nbAdded = ref.nnew - n;
  out.refineStatus = ref.status;
  buffer.x = ref.x;
  buffer.y = ref.y;
  buffer.s.assign(ref.nnew, 0.0);
  buffer.xp.assign(ref.nnew, 0.0);
  buffer.yp.assign(ref.nnew, 0.0);
  buffer.lgSame = false;
  resplineAndPar(buffer);
  // GDES 598: final CANG (display drops, AMAX structured).
  out.maxAngleAfter = geometry::cang(buffer.x, buffer.y).maxAngle;
  return Result<CornerAddResult>::ok(out);
}

Result<CornerAddResult> addCornerPoints(GeometryDesignBuffer& buffer) {
  Status st = validate(buffer, "addCornerPoints");
  if (!st.isOk()) {
    return Result<CornerAddResult>::fail(st.error());
  }
  // GDES 504-518: Defaults ATOL=0.5*AMAX, ISPL=1, XRF = BBox +/- 10 %.
  const double amax = geometry::cang(buffer.x, buffer.y).maxAngle;
  double xmin = buffer.x[0];
  double xmax = buffer.x[0];
  for (const double xi : buffer.x) {
    xmin = std::min(xmin, xi);
    xmax = std::max(xmax, xi);
  }
  return addCornerPoints(buffer, 0.5 * amax, 1, xmin - 0.1 * (xmax - xmin),
                         xmax + 0.1 * (xmax - xmin));
}

Status setTrailingEdgeGap(GeometryDesignBuffer& buffer, double gapnew,
                          double doc) {
  Status st = validate(buffer, "setTrailingEdgeGap");
  if (!st.isOk()) return st;
  const std::size_t n = buffer.x.size();
  // TGAP 1226-1231: LE/TE positions, chord length squared.
  const geometry::LeadingEdgeResult le =
      geometry::lefind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  const double xble = math::seval(le.sle, buffer.x, buffer.xp, buffer.s);
  const double yble = math::seval(le.sle, buffer.y, buffer.yp, buffer.s);
  const double xbte = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
  const double ybte = 0.5 * (buffer.y[0] + buffer.y[n - 1]);
  const double chbsq = (xbte - xble) * (xbte - xble) +
                       (ybte - yble) * (ybte - yble);
  // TGAP 1233-1244: current column and unit vector.
  const double dxn = buffer.x[0] - buffer.x[n - 1];
  const double dyn = buffer.y[0] - buffer.y[n - 1];
  const double gap = std::sqrt(dxn * dxn + dyn * dyn);
  double dxu;
  double dyu;
  if (gap > 0.0) {
    dxu = dxn / gap;
    dyu = dyn / gap;
  } else {
    dxu = -0.5 * (buffer.yp[n - 1] - buffer.yp[0]);
    dyu = 0.5 * (buffer.xp[n - 1] - buffer.xp[0]);
  }
  // TGAP 1262-1264: blend region and delta.
  const double docc = std::min(std::max(doc, 0.0), 1.0);
  const double dgap = gapnew - gap;
  // TGAP 1267-1289: per point x/c and exponential thickness factor.
  for (std::size_t i = 0; i < n; ++i) {
    const double xoc = ((buffer.x[i] - xble) * (xbte - xble) +
                        (buffer.y[i] - yble) * (ybte - yble)) /
                       chbsq;
    double tfac;
    if (docc == 0.0) {
      tfac = 0.0;
      if (i == 0 || i == n - 1) tfac = 1.0;
    } else {
      const double arg = std::min((1.0 - xoc) * (1.0 / docc - 1.0), 15.0);
      tfac = std::exp(-arg);
    }
    if (buffer.s[i] <= le.sle) {
      buffer.x[i] = buffer.x[i] + 0.5 * dgap * xoc * tfac * dxu;
      buffer.y[i] = buffer.y[i] + 0.5 * dgap * xoc * tfac * dyu;
    } else {
      buffer.x[i] = buffer.x[i] - 0.5 * dgap * xoc * tfac * dxu;
      buffer.y[i] = buffer.y[i] - 0.5 * dgap * xoc * tfac * dyu;
    }
  }
  // TGAP 1290-1300: LGSAME=false, respline, GEOPAR.
  buffer.lgSame = false;
  resplineAndPar(buffer);
  return Status::ok();
}

Result<double> scaleLeadingEdgeRadius(GeometryDesignBuffer& buffer,
                                      double rfac, double doc) {
  Status st = validate(buffer, "scaleLeadingEdgeRadius");
  if (!st.isOk()) {
    return Result<double>::fail(st.error());
  }
  // LERAD 1334-1341: DOC lower bound, LERSCL, copy back.
  const double docc = std::max(doc, 0.001);
  const geometry::AirfoilCoordinates nc =
      geometry::lerscl(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s,
                       docc, rfac);
  buffer.x = nc.x;
  buffer.y = nc.y;
  buffer.lgSame = false;
  // LERAD 1345-1353: Respline, GEOPAR.
  resplineAndPar(buffer);
  // LERAD 1356-1365: new LE radius from the Mid range.
  const std::size_t n = buffer.x.size();
  double cvmax = 0.0;
  for (std::size_t i = n / 4; i <= (3 * n) / 4; ++i) {
    const double cv = math::curv(buffer.s[i], buffer.x, buffer.xp, buffer.y,
                                 buffer.yp, buffer.s);
    cvmax = std::max(std::abs(cv), cvmax);
  }
  // cpp-safety-deviation: Fortran returns 1/0 = Inf for CVMAX == 0.
  if (cvmax == 0.0) {
    return Result<double>::fail(
        Error::make(ErrorCode::invalidState,
                    "scaleLeadingEdgeRadius: CVMAX == 0 (no radius)"));
  }
  return Result<double>::ok(1.0 / cvmax);
}

void scaleAboutOrigin(GeometryDesignBuffer& buffer, ScaleOrigin origin,
                      double xorg, double yorg, double scl) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 3) {
    throw std::invalid_argument(
        "scaleAboutOrigin: consistent lengths and NB >= 3 required");
  }
  // SCLXY 1384-1407: Origin LE/TE/Point.
  if (origin == ScaleOrigin::le) {
    const geometry::LeadingEdgeResult le =
        geometry::lefind(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
    xorg = math::seval(le.sle, buffer.x, buffer.xp, buffer.s);
    yorg = math::seval(le.sle, buffer.y, buffer.yp, buffer.s);
  } else if (origin == ScaleOrigin::te) {
    xorg = 0.5 * (buffer.x[0] + buffer.x[n - 1]);
    yorg = 0.5 * (buffer.y[0] + buffer.y[n - 1]);
  }
  // SCLXY 1412-1426: scaling, LGSAME=false, respline, GEOPAR.
  for (std::size_t i = 0; i < n; ++i) {
    buffer.x[i] = scl * (buffer.x[i] - xorg) + xorg;
    buffer.y[i] = scl * (buffer.y[i] - yorg) + yorg;
  }
  buffer.lgSame = false;
  resplineAndPar(buffer);
}

bool insideContour(std::span<const double> x, std::span<const double> y,
                   double xf, double yf) {
  const std::size_t n = x.size();
  if (n != y.size() || n < 1) {
    throw std::invalid_argument(
        "insideContour: x/y equal long and N >= 1 required");
  }
  // INSIDE 1860-1873: enclosed angle sum.
  double angle = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t ip = (i == n - 1) ? 0 : i + 1;
    const double xb1 = x[i] - xf;
    const double yb1 = y[i] - yf;
    const double xb2 = x[ip] - xf;
    const double yb2 = y[ip] - yf;
    const double r1 = xb1 * xb1 + yb1 * yb1;
    const double r2 = xb2 * xb2 + yb2 * yb2;
    // cpp-safety-deviation: Fortran preserves NaN/Inf for coincidence
    // so that INSIDE =.FALSE.; the core returns false in a controlled manner.
    if (r1 == 0.0 || r2 == 0.0) return false;
    angle = angle + (xb1 * yb2 - yb1 * xb2) / std::sqrt(r1 * r2);
  }
  return std::abs(angle) > 1.0;
}

FlapHingeResult flapHinge(const GeometryDesignBuffer& buffer, double xf,
                          double yf, double yrel) {
  const std::size_t n = buffer.x.size();
  if (!consistent(buffer) || n < 2) {
    throw std::invalid_argument(
        "flapHinge: consistent lengths and NB >= 2 required");
  }
  return flapHingeOnContour(buffer.x, buffer.xp, buffer.y, buffer.yp,
                            buffer.s, xf, yf, yrel);
}

FlapHingeResult flapHingeOnContour(std::span<const double> x,
                                   std::span<const double> xp,
                                   std::span<const double> y,
                                   std::span<const double> yp,
                                   std::span<const double> s, double xf,
                                   double yf, double yrel) {
  const std::size_t n = x.size();
  if (n != xp.size() || n != y.size() || n != yp.size() || n != s.size() ||
      n < 2) {
    throw std::invalid_argument(
        "flapHingeOnContour: consistent lengths and n >= 2 required");
  }
  FlapHingeResult r;
  // GETXYF 1887-1892: SINVRT on the hinge x-position.
  r.tops = s[0] + (x[0] - xf);
  r.bots = s[n - 1] - (x[n - 1] - xf);
  r.topStatus = math::sinvrt(r.tops, xf, x, xp, s);
  r.botStatus = math::sinvrt(r.bots, xf, x, xp, s);
  r.topy = math::seval(r.tops, y, yp, s);
  r.boty = math::seval(r.bots, y, yp, s);
  // GETXYF 1902-1905: y/t path (yrel explicit instead of a prompt).
  if (yf == 999.0) {
    r.yf = r.topy * yrel + r.boty * (1.0 - yrel);
  } else {
    r.yf = yf;
  }
  return r;
}

Result<FlapResult> deflectFlap(GeometryDesignBuffer& buffer, double xbf,
                               double ybf, double ddef) {
  Status st = validate(buffer, "deflectFlap");
  if (!st.isOk()) {
    return Result<FlapResult>::fail(st.error());
  }
  FlapResult fr;
  fr.xbf = xbf;
  fr.ybf = ybf;
  fr.nbBefore = buffer.x.size();

  // FLAP 1457-1458: hinge location and inside/outside decision.
  const FlapHingeResult hg = flapHinge(buffer, xbf, ybf);
  fr.tops = hg.tops;
  fr.bots = hg.bots;
  fr.inside = insideContour(buffer.x, buffer.y, xbf, ybf);

  // FLAP 1469-1470: RDEF == 0 -> canonical early return.
  const double pi = 3.14159265358979323846;
  const double rdef = ddef * pi / 180.0;
  if (rdef == 0.0) {
    fr.nbAfter = buffer.x.size();
    return Result<FlapResult>::ok(fr);
  }

  // FLAP 1473-1491: angle extremes ATOP/ABOT.
  if (fr.inside) {
    fr.atop = std::max(0.0, -rdef);
    fr.abot = std::max(0.0, rdef);
  } else {
    const double chx =
        math::deval(fr.bots, buffer.x, buffer.xp, buffer.s) -
        math::deval(fr.tops, buffer.x, buffer.xp, buffer.s);
    const double chy =
        math::deval(fr.bots, buffer.y, buffer.yp, buffer.s) -
        math::deval(fr.tops, buffer.y, buffer.yp, buffer.s);
    const double fvx = math::seval(fr.bots, buffer.x, buffer.xp, buffer.s) +
                       math::seval(fr.tops, buffer.x, buffer.xp, buffer.s);
    const double fvy = math::seval(fr.bots, buffer.y, buffer.yp, buffer.s) +
                       math::seval(fr.tops, buffer.y, buffer.yp, buffer.s);
    const double crsp =
        chx * (ybf - 0.5 * fvy) - chy * (xbf - 0.5 * fvx);
    if (crsp > 0.0) {
      fr.atop = std::max(0.0, rdef);
      fr.abot = std::max(0.0, rdef);
    } else {
      fr.atop = std::max(0.0, -rdef);
      fr.abot = std::max(0.0, -rdef);
    }
  }

  // FLAP 1494-1505: break points (SSS) and coordinates.
  const geometry::BreakPointResult bt =
      geometry::sss(fr.tops, xbf, ybf, fr.atop, 1, buffer.x, buffer.xp,
                    buffer.y, buffer.yp, buffer.s);
  fr.st1 = bt.s1;
  fr.st2 = bt.s2;
  fr.sssTopStatus = bt.status;
  const geometry::BreakPointResult bb =
      geometry::sss(fr.bots, xbf, ybf, fr.abot, 2, buffer.x, buffer.xp,
                    buffer.y, buffer.yp, buffer.s);
  fr.sb1 = bb.s1;
  fr.sb2 = bb.s2;
  fr.sssBotStatus = bb.status;
  fr.xt1 = math::seval(fr.st1, buffer.x, buffer.xp, buffer.s);
  fr.yt1 = math::seval(fr.st1, buffer.y, buffer.yp, buffer.s);
  fr.xt2 = math::seval(fr.st2, buffer.x, buffer.xp, buffer.s);
  fr.yt2 = math::seval(fr.st2, buffer.y, buffer.yp, buffer.s);
  fr.xb1 = math::seval(fr.sb1, buffer.x, buffer.xp, buffer.s);
  fr.yb1 = math::seval(fr.sb1, buffer.y, buffer.yp, buffer.s);
  fr.xb2 = math::seval(fr.sb2, buffer.x, buffer.xp, buffer.s);
  fr.yb2 = math::seval(fr.sb2, buffer.y, buffer.yp, buffer.s);

  // FLAP 1514-1519: points adjacent to the break locations (0-based).
  std::size_t n = buffer.x.size();
  std::size_t it1 = 0;
  std::size_t it2 = 0;
  std::size_t ib1 = 0;
  std::size_t ib2 = 0;
  for (std::size_t i = 0; i + 1 < n; ++i) {
    if (buffer.s[i] <= fr.st1 && buffer.s[i + 1] > fr.st1) it1 = i + 1;
    if (buffer.s[i] < fr.st2 && buffer.s[i + 1] >= fr.st2) it2 = i;
    if (buffer.s[i] <= fr.sb1 && buffer.s[i + 1] > fr.sb1) ib1 = i;
    if (buffer.s[i] < fr.sb2 && buffer.s[i + 1] >= fr.sb2) ib2 = i + 1;
  }
  const double dsavg = (buffer.s[n - 1] - buffer.s[0]) / double(n - 1);
  constexpr double sfrac = 0.33333;

  // FLAP 1526-1586: new/moved neighboring points at the break locations.
  double xt1new = 0.0;
  double yt1new = 0.0;
  double xt2new = 0.0;
  double yt2new = 0.0;
  double xb1new = 0.0;
  double yb1new = 0.0;
  double xb2new = 0.0;
  double yb2new = 0.0;
  bool lt1new = false;
  bool lt2new = false;
  bool lb1new = false;
  bool lb2new = false;
  if (fr.atop != 0.0) {
    const double st1p = fr.st1 + sfrac * (buffer.s[it1] - fr.st1);
    const double st1q = fr.st1 + sfrac * (buffer.s[it1 + 1] - fr.st1);
    if (buffer.s[it1] < st1q) {
      xt1new = math::seval(st1q, buffer.x, buffer.xp, buffer.s);
      yt1new = math::seval(st1q, buffer.y, buffer.yp, buffer.s);
      lt1new = false;
    } else {
      xt1new = math::seval(st1p, buffer.x, buffer.xp, buffer.s);
      yt1new = math::seval(st1p, buffer.y, buffer.yp, buffer.s);
      lt1new = true;
    }
    const double st2p = fr.st2 + sfrac * (buffer.s[it2] - fr.st2);
    const std::size_t it2q = (it2 >= 1) ? it2 - 1 : 0;
    const double st2q = fr.st2 + sfrac * (buffer.s[it2q] - fr.st2);
    if (buffer.s[it2] > st2q) {
      xt2new = math::seval(st2q, buffer.x, buffer.xp, buffer.s);
      yt2new = math::seval(st2q, buffer.y, buffer.yp, buffer.s);
      lt2new = false;
    } else {
      xt2new = math::seval(st2p, buffer.x, buffer.xp, buffer.s);
      yt2new = math::seval(st2p, buffer.y, buffer.yp, buffer.s);
      lt2new = true;
    }
  }
  if (fr.abot != 0.0) {
    const double sb1p = fr.sb1 + sfrac * (buffer.s[ib1] - fr.sb1);
    const double sb1q = fr.sb1 + sfrac * (buffer.s[ib1 - 1] - fr.sb1);
    if (buffer.s[ib1] > sb1q) {
      xb1new = math::seval(sb1q, buffer.x, buffer.xp, buffer.s);
      yb1new = math::seval(sb1q, buffer.y, buffer.yp, buffer.s);
      lb1new = false;
    } else {
      xb1new = math::seval(sb1p, buffer.x, buffer.xp, buffer.s);
      yb1new = math::seval(sb1p, buffer.y, buffer.yp, buffer.s);
      lb1new = true;
    }
    const double sb2p = fr.sb2 + sfrac * (buffer.s[ib2] - fr.sb2);
    const std::size_t ib2q = std::min(ib2 + 1, n - 1);
    const double sb2q = fr.sb2 + sfrac * (buffer.s[ib2q] - fr.sb2);
    if (buffer.s[ib2] < sb2q) {
      xb2new = math::seval(sb2q, buffer.x, buffer.xp, buffer.s);
      yb2new = math::seval(sb2q, buffer.y, buffer.yp, buffer.s);
      lb2new = false;
    } else {
      xb2new = math::seval(sb2p, buffer.x, buffer.xp, buffer.s);
      yb2new = math::seval(sb2p, buffer.y, buffer.yp, buffer.s);
      lb2new = true;
    }
  }

  // FLAP 1591-1603: rotation of the flap points around the hinge.
  const double sind = std::sin(rdef);
  const double cosd = std::cos(rdef);
  for (std::size_t i = 0; i < n; ++i) {
    if (i >= it1 && i <= ib1) continue;
    const double xbar = buffer.x[i] - xbf;
    const double ybar = buffer.y[i] - ybf;
    buffer.x[i] = xbf + xbar * cosd + ybar * sind;
    buffer.y[i] = ybf - xbar * sind + ybar * cosd;
  }

  // FLAP 1605-1629: deleted points on the upper and lower surfaces that disappeared.
  if (it1 > it2 + 1) {
    const std::size_t idif = it1 - it2 - 1;
    n -= idif;
    it1 -= idif;
    ib1 -= idif;
    ib2 -= idif;
    for (std::size_t i = it2 + 1; i < n; ++i) {
      buffer.s[i] = buffer.s[i + idif];
      buffer.x[i] = buffer.x[i + idif];
      buffer.y[i] = buffer.y[i + idif];
    }
    buffer.s.resize(n);
    buffer.x.resize(n);
    buffer.y.resize(n);
  }
  if (ib2 > ib1 + 1) {
    const std::size_t idif = ib2 - ib1 - 1;
    n -= idif;
    ib2 -= idif;
    for (std::size_t i = ib1 + 1; i < n; ++i) {
      buffer.s[i] = buffer.s[i + idif];
      buffer.x[i] = buffer.x[i + idif];
      buffer.y[i] = buffer.y[i + idif];
    }
    buffer.s.resize(n);
    buffer.x.resize(n);
    buffer.y.resize(n);
  }

  // FLAP 1632-1706: new points on the upper surface.
  if (fr.atop == 0.0) {
    const double dsnew = std::abs(rdef) *
                         std::sqrt((fr.xt1 - xbf) * (fr.xt1 - xbf) +
                                   (fr.yt1 - ybf) * (fr.yt1 - ybf));
    const std::size_t npadd =
        std::size_t(1.5 * dsnew / dsavg + 1.0);
    if (npadd != 0) {
      n += npadd;
      buffer.s.resize(n);
      buffer.x.resize(n);
      buffer.y.resize(n);
      it1 += npadd;
      ib1 += npadd;
      ib2 += npadd;
      for (std::size_t i = n; i-- > it1;) {
        buffer.x[i] = buffer.x[i - npadd];
        buffer.y[i] = buffer.y[i - npadd];
      }
      const double dang = rdef / double(npadd);
      const double xbar = fr.xt1 - xbf;
      const double ybar = fr.yt1 - ybf;
      for (std::size_t ip = 1; ip <= npadd; ++ip) {
        const double ang = dang * (double(ip) - 0.5);
        const double ca = std::cos(ang);
        const double sa = std::sin(ang);
        buffer.x[it1 - ip] = xbf + xbar * ca + ybar * sa;
        buffer.y[it1 - ip] = ybf - xbar * sa + ybar * ca;
      }
    }
  } else {
    std::size_t npadd = 1;
    if (lt2new) npadd += 1;
    if (lt1new) npadd += 1;
    n += npadd;
    buffer.s.resize(n);
    buffer.x.resize(n);
    buffer.y.resize(n);
    it1 += npadd;
    ib1 += npadd;
    ib2 += npadd;
    for (std::size_t i = n; i-- > it1;) {
      buffer.x[i] = buffer.x[i - npadd];
      buffer.y[i] = buffer.y[i - npadd];
    }
    if (lt1new) {
      buffer.x[it1 - 1] = xt1new;
      buffer.y[it1 - 1] = yt1new;
      buffer.x[it1 - 2] = fr.xt1;
      buffer.y[it1 - 2] = fr.yt1;
    } else {
      buffer.x[it1] = xt1new;
      buffer.y[it1] = yt1new;
      buffer.x[it1 - 1] = fr.xt1;
      buffer.y[it1 - 1] = fr.yt1;
    }
    const double xbar = xt2new - xbf;
    const double ybar = yt2new - ybf;
    if (lt2new) {
      buffer.x[it2 + 1] = xbf + xbar * cosd + ybar * sind;
      buffer.y[it2 + 1] = ybf - xbar * sind + ybar * cosd;
    } else {
      buffer.x[it2] = xbf + xbar * cosd + ybar * sind;
      buffer.y[it2] = ybf - xbar * sind + ybar * cosd;
    }
  }

  // FLAP 1709-1779: new points on the lower surface.
  if (fr.abot == 0.0) {
    const double dsnew = std::abs(rdef) *
                         std::sqrt((fr.xb1 - xbf) * (fr.xb1 - xbf) +
                                   (fr.yb1 - ybf) * (fr.yb1 - ybf));
    const std::size_t npadd =
        std::size_t(1.5 * dsnew / dsavg + 1.0);
    if (npadd != 0) {
      n += npadd;
      buffer.s.resize(n);
      buffer.x.resize(n);
      buffer.y.resize(n);
      ib2 += npadd;
      for (std::size_t i = n; i-- > ib2;) {
        buffer.x[i] = buffer.x[i - npadd];
        buffer.y[i] = buffer.y[i - npadd];
      }
      const double dang = rdef / double(npadd);
      const double xbar = fr.xb1 - xbf;
      const double ybar = fr.yb1 - ybf;
      for (std::size_t ip = 1; ip <= npadd; ++ip) {
        const double ang = dang * (double(ip) - 0.5);
        const double ca = std::cos(ang);
        const double sa = std::sin(ang);
        buffer.x[ib1 + ip] = xbf + xbar * ca + ybar * sa;
        buffer.y[ib1 + ip] = ybf - xbar * sa + ybar * ca;
      }
    }
  } else {
    std::size_t npadd = 1;
    if (lb2new) npadd += 1;
    if (lb1new) npadd += 1;
    n += npadd;
    buffer.s.resize(n);
    buffer.x.resize(n);
    buffer.y.resize(n);
    ib2 += npadd;
    for (std::size_t i = n; i-- > ib2;) {
      buffer.x[i] = buffer.x[i - npadd];
      buffer.y[i] = buffer.y[i - npadd];
    }
    if (lb1new) {
      buffer.x[ib1 + 1] = xb1new;
      buffer.y[ib1 + 1] = yb1new;
      buffer.x[ib1 + 2] = fr.xb1;
      buffer.y[ib1 + 2] = fr.yb1;
    } else {
      buffer.x[ib1] = xb1new;
      buffer.y[ib1] = yb1new;
      buffer.x[ib1 + 1] = fr.xb1;
      buffer.y[ib1 + 1] = fr.yb1;
    }
    const double xbar = xb2new - xbf;
    const double ybar = yb2new - ybf;
    if (lb2new) {
      buffer.x[ib2 - 1] = xbf + xbar * cosd + ybar * sind;
      buffer.y[ib2 - 1] = ybf - xbar * sind + ybar * cosd;
    } else {
      buffer.x[ib2] = xbf + xbar * cosd + ybar * sind;
      buffer.y[ib2] = ybf - xbar * sind + ybar * cosd;
    }
  }

  // FLAP 1781-1805: SCHECK, Respline, GEOPAR, Flags.
  buffer.lgSame = false;
  std::size_t nn = n;
  static_cast<void>(geometry::scheck(buffer.x, buffer.y, nn, 0.2));
  buffer.x.resize(nn);
  buffer.y.resize(nn);
  buffer.s.resize(nn);
  buffer.xp.resize(nn);
  buffer.yp.resize(nn);
  resplineAndPar(buffer);
  buffer.lbFlap = true;
  if (buffer.lgSym) {
    buffer.lgSym = false;
    fr.symmetryDisabled = true;
  }
  fr.nbAfter = buffer.x.size();
  return Result<FlapResult>::ok(fr);
}

}  // namespace xfoil::core::geometry_design

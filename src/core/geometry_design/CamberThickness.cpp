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

// File: src/core/geometry_design/CamberThickness.cpp
// Port of GETCAM and GETMAX from fortran-src/xtcam.f.
// 0-based: Fortran X(1..N) <-> C++ x[0..n-1].

#include "xfoil/core/geometry_design/CamberThickness.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/Sort.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry_design {

ThicknessCamberDistribution getCamberThickness(std::span<const double> x,
                                               std::span<const double> xp,
                                               std::span<const double> y,
                                               std::span<const double> yp,
                                               std::span<const double> s) {
  const std::size_t n = s.size();
  if (n < 3 || x.size() < n || xp.size() < n || y.size() < n ||
      yp.size() < n) {
    throw std::invalid_argument(
        "getCamberThickness: valid lengths required (n >= 3)");
  }

  // Fortran 1256-1258: XLFIND + SEVAL.
  const geometry::LeadingEdgeResult le = geometry::xlfind(x, xp, y, yp, s);
  const double sl = le.sle;
  const double xl = math::seval(sl, x, xp, s);
  const double yl = math::seval(sl, y, yp, s);

  // Fortran 1261-1285: for each point, determine the opposite point
  // and the new thickness/camber.
  std::vector<double> xcm(n + 1);
  std::vector<double> ycm(n + 1);
  std::vector<double> xtk(n + 1);
  std::vector<double> ytk(n + 1);
  std::size_t oppositePointFailures = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const geometry::OppositePointResult opp =
        geometry::sopps(s[i], x, xp, y, yp, s, sl);
    if (opp.status != math::Diagnostic::completed) {
      // Fortran: WRITE(*,*) 'SOPPS: Opposite-point location failed.
      // Continuing...' (the kernel only counts; the CLI text follows later).
      ++oppositePointFailures;
    }
    const double sopp = opp.sopp;
    double xopp = math::seval(sopp, x, xp, s);
    double yopp = math::seval(sopp, y, yp, s);
    // Fortran 1267-1273: edge points use the discrete endpoints.
    if (i == 0) {
      xopp = x[n - 1];
      yopp = y[n - 1];
    } else if (i == n - 1) {
      xopp = x[0];
      yopp = y[0];
    }
    xcm[i] = 0.5 * (x[i] + xopp);
    ycm[i] = 0.5 * (y[i] + yopp);
    xtk[i] = 0.5 * (x[i] + xopp);
    ytk[i] = std::abs(0.5 * (y[i] - yopp));
  }

  // Fortran 1288: TOL = 1.0E-5*(S(N)-S(1)).
  const double tol = 1.0e-5 * (s[n - 1] - s[0]);

  // Fortran 1292-1302: camber sort, LE point append,
  // re-origination YCM(I) -= YCM(1) (local YOF, no COMMON).
  std::size_t ncm = n + 1;
  xcm[n] = xl;
  ycm[n] = yl;
  math::sortWithTolerance(tol, ncm, xcm, ycm);
  const double yof = ycm[0];
  for (std::size_t i = 0; i < ncm; ++i) {
    ycm[i] -= yof;
  }

  // Fortran 1305-1308: Thickness sort, LE point (YTK=0) append.
  std::size_t ntk = n + 1;
  xtk[n] = xl;
  ytk[n] = 0.0;
  math::sortWithTolerance(tol, ntk, xtk, ytk);

  xcm.resize(ncm);
  ycm.resize(ncm);
  xtk.resize(ntk);
  ytk.resize(ntk);
  return ThicknessCamberDistribution{
      std::move(xcm), std::move(ycm), std::move(xtk), std::move(ytk),
      ncm,            ntk,            oppositePointFailures};
}

MaxPointResult getMaxPoint(std::span<const double> x,
                           std::span<const double> y,
                           std::span<double> yp) {
  const std::size_t n = x.size();
  if (n < 2 || y.size() < n || yp.size() != n) {
    throw std::invalid_argument(
        "getMaxPoint: valid lengths required (n >= 2, yp == n)");
  }

  // Fortran 1321-1324.
  const double xlen = x[n - 1] - x[0];
  const double xtol = xlen * 1.0e-5;
  math::segspl(y, x, yp);

  // Fortran 1327-1335: approximates Maximum.
  // cpp-safety-deviation: ddx is on 0.0 initialized (Fortran:
  // uninitialized if no point satisfies |Y(I)| > |YMAX0|) and the
  // source index i+1 is clamped to n-1 (Fortran reads X(N+1) from the
  // larger-sized COMMON array).
  double ymax0 = y[0];
  double xmax0 = x[0];
  double ddx = 0.0;
  for (std::size_t i = 1; i < n; ++i) {
    if (std::abs(y[i]) > std::abs(ymax0)) {
      ymax0 = y[i];
      xmax0 = 0.5 * (x[i - 1] + x[i]);
      const std::size_t hi = (i + 1 < n) ? i + 1 : n - 1;
      ddx = 0.5 * std::abs(x[hi] - x[i - 1]);
    }
  }
  double xmax = xmax0;

  // Fortran 1339-1352: Newton loop (max. 10), Fallback on
  // XMAX0/YMAX0 for Failure.
  for (int iter = 0; iter < 10; ++iter) {
    const double ymax = math::seval(xmax, y, yp, x);
    const double res = math::deval(xmax, y, yp, x);
    const double resp = math::d2val(xmax, y, yp, x);
    if (std::abs(xlen * resp) < 1.0e-6) {
      return MaxPointResult{MaxPoint{xmax, ymax}, math::Diagnostic::completed};
    }
    double dx = -res / resp;
    dx = std::copysign(std::min(0.5 * ddx, std::abs(dx)), dx);
    xmax += dx;
    if (std::abs(dx) < xtol) {
      return MaxPointResult{MaxPoint{xmax, ymax}, math::Diagnostic::completed};
    }
  }
  return MaxPointResult{MaxPoint{xmax0, ymax0}, math::Diagnostic::limitReached};
}

CamberLoadingResult camberFromLoading(std::span<const double> x,
                                      std::span<const double> p,
                                      std::span<const double> dpdx) {
  const std::size_t n = x.size();
  if (n < 2 || p.size() != n || dpdx.size() != n) {
    throw std::invalid_argument(
        "camberFromLoading: n >= 2 and equal lange x/p/dpdx required");
  }

  // Fortran 1373-1379: QOPI = 1/(4 pi), boundary values.
  constexpr double qopi = 7.9577471545948E-02;
  const double p0 = p[0];
  const double p1 = p[n - 1];
  const double x0 = x[0];
  const double x1 = x[n - 1];

  // Fortran 1382-1416: Cauchy integral with singularity removal.
  std::vector<double> dydx(n);
  std::vector<double> y(n);
  for (std::size_t i = 0; i < n; ++i) {
    dydx[i] = 0.0;
    double yp1 = (0 == i) ? dpdx[0] : (p[0] - p[i]) / (x[0] - x[i]);
    for (std::size_t j = 1; j < n; ++j) {
      const double yp2 =
          (j == i) ? dpdx[j] : (p[j] - p[i]) / (x[j] - x[i]);
      dydx[i] += 0.5 * (yp1 + yp2) * (x[j] - x[j - 1]);
      yp1 = yp2;
    }
    dydx[i] = qopi * dydx[i];
    if (i != 0) {
      dydx[i] = dydx[i] - qopi * (p[i] - p0) * std::log(x[i] - x0);
    }
    if (i != n - 1) {
      dydx[i] = dydx[i] + qopi * (p[i] - p1) * std::log(x1 - x[i]);
    }
  }

  // Fortran 1419-1423: regular share integrated from the LE.
  y[0] = 0.0;
  for (std::size_t i = 1; i < n; ++i) {
    y[i] = y[i - 1] + 0.5 * (dydx[i] + dydx[i - 1]) * (x[i] - x[i - 1]);
  }

  // Fortran 1427-1432: singular share YSING (MAX clamp 1.E-6).
  for (std::size_t i = 0; i < n; ++i) {
    const double xt = x[i];
    const double ysing =
        qopi * p1 * ((xt - x1) * std::log(std::max((x1 - xt) / (x1 - x0), 1.E-6)) - xt) -
        qopi * p0 * ((xt - x0) * std::log(std::max((xt - x0) / (x1 - x0), 1.E-6)) - xt);
    y[i] = y[i] + ysing;
  }

  // Fortran 1435-1441: offset and angle of attack, so that y(0)=y(1)=0.
  const double y0 = y[0];
  const double y1 = y[n - 1];
  for (std::size_t i = 0; i < n; ++i) {
    y[i] = y[i] - y0 * (x1 - x[i]) / (x1 - x0) - y1 * (x[i] - x0) / (x1 - x0);
  }
  return CamberLoadingResult{std::move(y), std::move(dydx)};
}

}  // namespace xfoil::core::geometry_design

// End: src/core/geometry_design/CamberThickness.cpp

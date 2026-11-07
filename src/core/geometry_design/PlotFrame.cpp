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

// File: src/core/geometry_design/PlotFrame.cpp
// Numerical Plot limits GOFINI/GOFSET + SCALIT.

#include "xfoil/core/geometry_design/PlotFrame.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstddef>
#include <stdexcept>

#include "xfoil/core/geometry_design/CamberThickness.h"

namespace xfoil::core::geometry_design {
namespace {

// Fortran INT: Rounding against Zero.
double fInt(double v) {
  return std::trunc(v);
}

}  // namespace

double scaleIter(std::span<const double> y, double yoff) {
  if (y.empty()) {
    throw std::invalid_argument("scaleIter: II >= 1 required");
  }
  // SCALIT 285-291: Absolute maximum.
  double ymax = std::abs(y[0] - yoff);
  for (std::size_t i = 1; i < y.size(); ++i) {
    ymax = std::max(ymax, std::abs(y[i] - yoff));
  }
  if (ymax == 0.0) ymax = 1.0E-8;
  const double ylog = std::log10(ymax);
  // SCALIT 294-306: next 1/2/5 x Power of ten.
  const double ylog1 = std::trunc(ylog + 100.0) - 99.0;
  const double ag2 = std::log10(2.0);
  const double ag5 = std::log10(5.0);
  double ylog2 = ylog1 + ag2;
  if (ylog2 - 1.0 > ylog) ylog2 = ylog2 - 1.0;
  double ylog5 = ylog1 + ag5;
  if (ylog5 - 1.0 > ylog) ylog5 = ylog5 - 1.0;
  const double gmin = std::min(ylog1, std::min(ylog2, ylog5));
  // SCALIT 308: Scale factor.
  return std::pow(10.0, -gmin);
}

GeometryPlotFrame gofIni(const GeometryDesignBuffer& buffer, bool lgGrid,
                         bool lgCplot, const PlotDefaults& def,
                         double ypMin, double ypMax) {
  const std::size_t n = buffer.x.size();
  if (buffer.x.size() != buffer.y.size() || n < 1) {
    throw std::invalid_argument(
        "gofIni: x/y equal long and NB >= 1 required");
  }
  GeometryPlotFrame f;
  f.xpmin = ypMin;
  f.xpmax = ypMax;

  // GOFINI 1045-1054: Bounding box.
  double xbmin = buffer.x[0];
  double ybmin = buffer.y[0];
  double xbmax = buffer.x[0];
  double ybmax = buffer.y[0];
  for (std::size_t i = 0; i < n; ++i) {
    xbmin = std::min(xbmin, buffer.x[i]);
    ybmin = std::min(ybmin, buffer.y[i]);
    xbmax = std::max(xbmax, buffer.x[i]);
    ybmax = std::max(ybmax, buffer.y[i]);
  }

  // GOFINI 1057-1072: GETCAM boundaries.
  const ThicknessCamberDistribution d = getCamberThickness(
      buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  double cmmin = 0.0;
  double cmmax = 0.0;
  for (std::size_t i = 0; i < d.ncm; ++i) {
    cmmin = std::min(cmmin, d.ycm[i]);
    cmmax = std::max(cmmax, d.ycm[i]);
  }
  double tkmin = 0.0;
  double tkmax = 0.0;
  for (std::size_t i = 0; i < d.ntk; ++i) {
    tkmin = std::min(tkmin, d.ytk[i]);
    tkmax = std::max(tkmax, d.ytk[i]);
  }

  // GOFINI 1074-1083: SCALIT and grid increment.
  double xr = xbmax - xbmin;
  double yr = ybmax - ybmin;
  const std::array<double, 1> xsIn{0.95 * xr};
  const std::array<double, 1> ysIn{0.95 * yr};
  f.xsf = scaleIter(xsIn, 0.0);
  f.ysf = scaleIter(ysIn, 0.0);
  f.dxyg = 0.1 / std::min(f.xsf, f.ysf);

  // GOFINI 1092-1095: "nice" grid limits.
  f.xgmax = f.dxyg * (fInt(xbmax / f.dxyg + 1001.01) - 1000.0);
  f.xgmin = f.dxyg * (fInt(xbmin / f.dxyg - 1001.01) + 1000.0);
  f.ygmax = f.dxyg * (fInt(ybmax / f.dxyg + 1001.01) - 1000.0);
  f.ygmin = f.dxyg * (fInt(ybmin / f.dxyg - 1001.01) + 1000.0);

  // GOFINI 1098-1105: thickness/camber plot frame.
  f.dxyc = f.dxyg;
  f.xcmin = f.xgmin;
  f.xcmax = f.xgmax;
  f.ycmin = std::min(cmmin, -tkmax);
  f.ycmax = std::max(cmmax, tkmax);
  f.ycmax = f.dxyc * (fInt(f.ycmax / f.dxyc + 1000.25) - 999.0);
  f.ycmin = f.dxyc * (fInt(f.ycmin / f.dxyc - 1000.25) + 999.0);
  f.ycmax = std::max(f.ycmax, f.ycmin + f.dxyc);

  // GOFINI 1108-1125: Scaling.
  if (lgGrid) {
    xr = f.xgmax - f.xgmin;
    yr = f.ygmax - f.ygmin;
  } else {
    xr = xbmax - xbmin;
    yr = ybmax - ybmin;
  }
  if (lgCplot) {
    yr = yr + (f.ycmax - f.ycmin);
  }
  const double range = std::max(xr, yr);
  const double sf = std::min(1.0 / xr, def.plotar / yr);
  f.xsf = sf;
  f.ysf = sf;
  f.chg = std::max(0.75 * def.ch * range * sf, 0.0075);

  // GOFINI 1131-1139: Offsets.
  if (lgGrid) {
    f.xoff = f.xgmin - 0.05 * range - 3.0 * f.chg / sf;
    f.yoff = f.ygmin - 0.05 * range - 2.0 * f.chg / sf;
  } else {
    f.xoff = xbmin - 0.05 * range;
    f.yoff = ybmin - 0.05 * range;
  }

  // GOFINI 1142-1158: dCp axis (AXISADJ, an external libPlt routine,
  // is not implemented here) and offsets.
  const double par = (def.ypPage - 2.0 * def.ymArg) /
                     (def.xpPage - 2.0 * def.xmArg);
  const double dprange = ypMax - ypMin;
  const double dyplt = std::max(0.1, par - def.plotar);
  f.ysfp = 0.8 * dyplt / dprange;
  f.ysfp = f.ysfp / f.ysf;
  f.dyoffc = -f.ygmax + f.ycmin - 2.2 * f.chg / f.ysf;
  f.dyoffp = -f.ycmax + f.dyoffc + ypMin * f.ysfp - 2.2 * f.chg / f.ysf;
  return f;
}

GridOverlay gofSet(const GeometryDesignBuffer& buffer, double xoff,
                   double yoff, double xsf, double ysf, double chg,
                   double size, double xwind, double ywind) {
  const std::size_t n = buffer.x.size();
  if (buffer.x.size() != buffer.y.size() || n < 1) {
    throw std::invalid_argument(
        "gofSet: x/y equal long and NB >= 1 required");
  }
  GridOverlay g;
  // GOFSET 1172-1181: extent.
  double xbmin = buffer.x[0];
  double ybmin = buffer.y[0];
  double xbmax = buffer.x[0];
  double ybmax = buffer.y[0];
  for (std::size_t i = 0; i < n; ++i) {
    xbmin = std::min(xbmin, buffer.x[i]);
    ybmin = std::min(ybmin, buffer.y[i]);
    xbmax = std::max(xbmax, buffer.x[i]);
    ybmax = std::max(ybmax, buffer.y[i]);
  }
  // GOFSET 1183-1195: corner points and clamping.
  const double range = std::max((xwind / size) / xsf, (ywind / size) / ysf);
  g.xg1 = xoff + 0.1 * range + 4.0 * chg / xsf;
  g.yg1 = yoff + 0.1 * range + 2.0 * chg / ysf;
  g.xg2 = xoff - 0.1 * range + (xwind / size) / xsf;
  g.yg2 = yoff - 0.1 * range + (ywind / size) / ysf;
  g.xg1 = std::max(g.xg1, xbmin);
  g.xg2 = std::min(g.xg2, xbmax);
  g.yg1 = std::max(g.yg1, ybmin);
  g.yg2 = std::min(g.yg2, ybmax);
  // GOFSET 1198-1211: SCALIT and grid limits.
  const std::array<double, 1> gxIn{0.95 * (g.xg2 - g.xg1)};
  const std::array<double, 1> gyIn{0.95 * (g.yg2 - g.yg1)};
  g.gxsf = scaleIter(gxIn, 0.0);
  g.gysf = scaleIter(gyIn, 0.0);
  g.gsf = g.gxsf;
  g.dxyg = 0.1 / g.gsf;
  g.xgmax = g.dxyg * (fInt(g.xg2 / g.dxyg + 1001.01) - 1000.0);
  g.xgmin = g.dxyg * (fInt(g.xg1 / g.dxyg - 1001.01) + 1000.0);
  g.ygmax = g.dxyg * (fInt(g.yg2 / g.dxyg + 1001.01) - 1000.0);
  g.ygmin = g.dxyg * (fInt(g.yg1 / g.dxyg - 1001.01) + 1000.0);
  return g;
}

}  // namespace xfoil::core::geometry_design

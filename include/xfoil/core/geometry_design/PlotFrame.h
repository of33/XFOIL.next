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

// File: include/xfoil/core/geometry_design/PlotFrame.h
// Numerical Plot limits of GOFINI/GOFSET, ported from
// fortran-src/xgdes.f; SCALIT from fortran-src/plutil.f.
//
// Only the numerical scaling and offset
// computations are ported. The AXISADJ axis adjustment (external libPlt
// routine) is not implemented here: YPMIN/YPMAX/DXYP are passed through as inputs
// (plot path). No drawing takes place.
//
// No raw-pointer API, no global states; all COMMON parameters
// (CH, PLOTAR, XPAGE, YPAGE, XMARG, YMARG, LGGRID, LPLCAM, XWIND, YWIND,
// SIZE) are passed explicitly.

#pragma once

#include <span>

#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"

namespace xfoil::core::geometry_design {

// Fortran: SUBROUTINE SCALIT(II,Y,YOFF,YSF) (plutil.f 277-310).
// "Nice" scale factor 10**-GMIN from the absolute maximum of
// Y-YOFF (YMAX == 0 -> 1.E-8 as in Fortran). II >= 1.
double scaleIter(std::span<const double> y, double yoff);

// MAIN/INIT defaults of the plot parameters (xfoil.f 559-560, 737-742).
struct PlotDefaults {
  double ch{0.015};      // CH
  double plotar{0.55};   // PLOTAR
  double xpPage{11.0};   // XPAGE
  double ypPage{8.5};    // YPAGE
  double xmArg{0.0};     // XMARG
  double ymArg{0.0};     // YMARG
};

// GOFINI initial group (COMMON CR18, AXISADJ fields passed through).
struct GeometryPlotFrame {
  double xsf{0.0};
  double ysf{0.0};
  double dxyg{0.0};
  double xgmin{0.0};
  double xgmax{0.0};
  double ygmin{0.0};
  double ygmax{0.0};
  double dxyc{0.0};
  double xcmin{0.0};
  double xcmax{0.0};
  double ycmin{0.0};
  double ycmax{0.0};
  double chg{0.0};
  double xoff{0.0};
  double yoff{0.0};
  double xpmin{0.0};  // AXISADJ not implemented (input pass-through)
  double xpmax{0.0};
  double dyoffc{0.0};
  double ysfp{0.0};
  double dyoffp{0.0};
};

// Fortran: SUBROUTINE GOFINI (xgdes.f 1038-1161) without AXISADJ part.
// Bounding box, GETCAM boundaries, SCALIT, "nice" grid limits,
// scaling/offsets. NB >= 1.
GeometryPlotFrame gofIni(const GeometryDesignBuffer& buffer, bool lgGrid,
                         bool lgCplot, const PlotDefaults& def,
                         double ypMin, double ypMax);

// GOFSET initial group (grid overlay).
struct GridOverlay {
  double xg1{0.0};
  double yg1{0.0};
  double xg2{0.0};
  double yg2{0.0};
  double gxsf{0.0};
  double gysf{0.0};
  double gsf{0.0};
  double dxyg{0.0};
  double xgmin{0.0};
  double xgmax{0.0};
  double ygmin{0.0};
  double ygmax{0.0};
};

// Fortran: SUBROUTINE GOFSET (xgdes.f 1165-1214).
// Grid overlay from the current XOFF/YOFF/XSF/YSF/CHG and window.
GridOverlay gofSet(const GeometryDesignBuffer& buffer, double xoff,
                   double yoff, double xsf, double ysf, double chg,
                   double size, double xwind, double ywind);

}  // namespace xfoil::core::geometry_design

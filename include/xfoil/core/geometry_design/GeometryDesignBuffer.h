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

// File: include/xfoil/core/geometry_design/GeometryDesignBuffer.h
// Buffer contour of the geometry design of the new 0-based cores,
// directly ported from fortran-src/xgdes.f and fortran-src/xtcam.f.
//
// The GeometryDesignBuffer corresponds to the Fortran buffer state
// (COMMON XB,YB,SB,XBP,YBP,NB,LGSAME,LGSYM,XBF,YBF,LBFLAP plus the buffer
// geometry parameters SBLE..CAMBRB maintained by GEOPAR). It is a
// pure value type without global state.
//
// Contained routines:
// THKCAM -> transformThicknessCamber
// ZERCAM -> zerCamber
//
// Canonical invariant (GDES): design operations change only
// the buffer; transfer into the current contour happens only via
// EXEC (ABCOPY, revalidated).
//
// 0-based: Fortran XB(1..NB) <-> C++ x[0..nb-1].
//
// C++ safety deviation THKCAM: Fortran keeps NaN/Inf from division by zero
// for CHORD == 0 (degenerate chord). The core rejects this case before
// each mutation with Status::fail(invalidState); the input state
// stays unchanged.
//
// No raw-pointer API, no dependency on app, CLI, or tests.

#pragma once

#include <cstddef>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry_design/CamberThickness.h"
#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::geometry_design {

// Fortran buffer state: contour (XB,YB,SB,XBP,YBP), active point count NB
// (= x.size), flags LGSAME/LGSYM, flap hinge XBF/YBF/LBFLAP, and the
// GEOPAR buffer parameters (SBLE,CHORDB,...,THICKB,CAMBRB).
struct GeometryDesignBuffer {
  std::vector<double> x;   // Fortran XB
  std::vector<double> y;   // Fortran YB
  std::vector<double> s;   // Fortran SB
  std::vector<double> xp;  // Fortran XBP
  std::vector<double> yp;  // Fortran YBP
  geometry::GeometryMetrics metrics;
  bool lgSame{false};  // Fortran LGSAME
  bool lgSym{false};   // Fortran LGSYM
  double xbf{0.0};     // Fortran XBF (flap hinge)
  double ybf{0.0};     // Fortran YBF
  bool lbFlap{false};  // Fortran LBFLAP

  GeometryDesignBuffer() = default;
  // Checks x/y/s/xp/yp equal lengths; a violation throws
  // std::invalid_argument without partial state.
  GeometryDesignBuffer(std::vector<double> x_, std::vector<double> y_,
                       std::vector<double> s_, std::vector<double> xp_,
                       std::vector<double> yp_);

  [[nodiscard]] std::size_t size() const noexcept { return x.size(); }
};

// Safety contract: all operations check their lengths before use;
// controlled errors leave the buffer unchanged.

// Fortran: SUBROUTINE THKCAM(TFAC,CFAC)
// Scales thickness (TFAC) and camber (CFAC) of the buffer contour relative to the
// chord line (LEFIND, SOPPS per point, W1/W2 scratch buffers before
// copy-back, afterwards SCALC/SEGSPL x2/GEOPAR; LGSAME=false).
// NB >= 3 (LEFIND contract). CHORD == 0 -> Status::fail (see above).
// Return value: count of SOPPS failures in its own
// opposite-point loop (Fortran WRITE 'SOPPS: Opposite-point location
// failed. Continuing...' per failure; the kernel itself performs no writes).
Result<std::size_t> transformThicknessCamber(GeometryDesignBuffer& buffer,
                                             double tfac, double cfac);

// Fortran: SUBROUTINE ZERCAM
// Sets the buffer camber to zero: TFAC=1, CFAC=0, THKCAM,
// then exact mirroring (YSYM ISIDE=1, with that side as
// source) and GEOPAR again. NB can change through YSYM
// (canonical behavior). Plot/stdout parts omitted.
// The metrics of the first GEOPAR (after THKCAM) are preserved as
// metricsAfterThkcam; the buffer holds those of the second.
struct ZerCamberResult {
  std::size_t thkcamOppositePointFailures{0};
  geometry::GeometryMetrics metricsAfterThkcam;
};
Result<ZerCamberResult> zerCamber(GeometryDesignBuffer& buffer);

// Fortran: SUBROUTINE HIPNT(RINPUT,NINPUT)
// Moves the thickness/camber maximum points of the buffer contour to new
// x positions (cubic X mapping function SPLINA/SEVAL, afterwards YB from
// new Y(X) splines of thickness/camber, SCALC/SEGSPL x2/GEOPAR;
// LGSAME=false). thpnt/chpnt <= 0 correspond to the canonical behavior
// "keep current maximum location" (THPNT=TXMAX, CHPNT=CXMAX).
// NB >= 3 (XLFIND contract). The Fortran warning for |AROT| > 1 degree
// is returned structured as chordlineAngleDeg.
struct HighPointResult {
  double chordlineAngleDeg{0.0};  // Fortran AROT
  MaxPoint camberMaxBefore;
  MaxPoint thickMaxBefore;
  math::Diagnostic camberStatus{math::Diagnostic::completed};
  math::Diagnostic thickStatus{math::Diagnostic::completed};
  // SOPPS failures of the GETCAM call in question (Fortran WRITE per
  // failure before the 1010-format output); the kernel itself performs no writes.
  std::size_t oppositePointFailuresBefore{0};
};

HighPointResult moveHighPoints(GeometryDesignBuffer& buffer, double thpnt,
                               double chpnt);

}  // namespace xfoil::core::geometry_design

// End: include/xfoil/core/geometry_design/GeometryDesignBuffer.h

// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
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

// File: tests/core/PanelingTests.cpp
// Unit tests for the new 0-based core: paneling and panel-geometry kernels
// (pangen, abcopy, panel parameters, NCALC, APCALC, TECALC geometry share).
//
// the tests use only xfoil::core::paneling and no app, CLI, or
// 1-based code.
//
// reference comparison: the expected values come from the focused
// Fortran harness (tests/fortran-reference/harnesses/paneling), which calls
// the canonical routines unchanged (PANGEN/TECALC from xfoil.f,
// ABCOPY from xgdes.f, NCALC/APCALC from xpanel.f, extracted byte-faithfully).
// Fortran REAL is single precision; the new core computes in double.
// Formula and operation order are identical; the deviations lie in the
// single-to-double range or in the documented amplified regions (SLE
// position, ATAN2 near TE).
//
// tolerance notes:
// - kFortranRelTol/kFortranAbsTol: base for all smooth values.
// - kLeRelTol (1e-4): the SLE = 1/CURV-based LE position amplifies the
// single-precision rounding.
// - kAngleAbsTol (5e-3 rad): APANEL ATAN2 values (amplified panel angles;
// Fortran PI is single precision).
// the tests cover standard, nonstandard, refinement, and capacity-limit cases.

#include "TestSupport.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/Abcopy.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"
#include "xfoil/core/paneling/PanelParameters.h"
#include "xfoil/core/paneling/PanelingTypes.h"

namespace {

xfoil::core::paneling::PanelingParameters xfoil699Paneling() {
  xfoil::core::paneling::PanelingParameters p;
  p.panelCount = 160;
  p.teToLeDensityRatio = 0.15;
  return p;
}


using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::math;
using namespace xfoil::core::paneling;

// Relative tolerance for the single-precision (Fortran) to double (C++) comparison.
constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;

// amplified values (SLE = LE position with a 1/CURV share).
constexpr double kLeRelTol = 1.0e-4;
constexpr double kLeAbsTol = 1.0e-7;

// APANEL ATAN2 angle (radians; Fortran PI is single precision).
constexpr double kAngleAbsTol = 5.0e-3;
constexpr double kAngleRelTol = 1.0e-3;

// canonical NACA buffer point count (Fortran NACA: NSIDE=IQX/3=123).
constexpr std::size_t kNside = 123;
constexpr std::size_t kBufferCount = 245;

// canonical NACA 2412 buffer.
AirfoilBuffer makeBuffer2412() {
  const Result<NacaResult> n = naca4(2412, kNside);
  if (!n.isOk()) {
    return AirfoilBuffer();
  }
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

// 365-point contour (Fortran NACA extraction: NB=365, the old IQX-5).
AirfoilBuffer makeBuffer365() {
  const Result<NacaResult> n = naca4(2412, 183);
  if (!n.isOk()) {
    return AirfoilBuffer();
  }
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

// 599-point contour for the new PANGEN upper boundary (NPAN=600).
AirfoilBuffer makeBuffer599() {
  const Result<NacaResult> n = naca4(2412, 300);
  if (!n.isOk()) {
    return AirfoilBuffer();
  }
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

// NACA-2412 buffer with duplicated point 62 (Fortran 1-based; 0-based
// index 61), as in harness test PANECORN.
AirfoilBuffer makeBufferDuplicated() {
  AirfoilBuffer buffer = makeBuffer2412();
  if (buffer.x.size() != kBufferCount) {
    return buffer;
  }
  buffer.x[61] = buffer.x[60];
  buffer.y[61] = buffer.y[60];
  return buffer;
}

// Clark-Y buffer (151 points from the canonical reference basis
// tests/fortran-reference/cases/geometry/clark-y-load/input/CLARK_Y.dat).
constexpr std::array<std::pair<double, double>, 151> kClarkYPoints{{
  {1.00000, 0.00060},
  {0.99344, 0.00216},
  {0.98165, 0.00494},
  {0.96782, 0.00820},
  {0.95254, 0.01174},
  {0.93656, 0.01540},
  {0.92034, 0.01904},
  {0.90394, 0.02264},
  {0.88745, 0.02620},
  {0.87096, 0.02969},
  {0.85444, 0.03311},
  {0.83791, 0.03647},
  {0.82138, 0.03975},
  {0.80484, 0.04296},
  {0.78829, 0.04609},
  {0.77175, 0.04914},
  {0.75520, 0.05211},
  {0.73864, 0.05500},
  {0.72207, 0.05782},
  {0.70549, 0.06054},
  {0.68890, 0.06319},
  {0.67234, 0.06575},
  {0.65579, 0.06822},
  {0.63925, 0.07058},
  {0.62277, 0.07284},
  {0.60630, 0.07497},
  {0.58980, 0.07700},
  {0.57336, 0.07890},
  {0.55689, 0.08068},
  {0.54041, 0.08233},
  {0.52391, 0.08386},
  {0.50740, 0.08528},
  {0.49093, 0.08658},
  {0.47448, 0.08773},
  {0.45804, 0.08876},
  {0.44164, 0.08964},
  {0.42531, 0.09037},
  {0.40900, 0.09093},
  {0.39270, 0.09133},
  {0.37637, 0.09156},
  {0.36001, 0.09163},
  {0.34360, 0.09155},
  {0.32714, 0.09133},
  {0.31066, 0.09097},
  {0.29417, 0.09050},
  {0.27783, 0.08991},
  {0.26173, 0.08917},
  {0.24588, 0.08824},
  {0.23032, 0.08707},
  {0.21499, 0.08564},
  {0.19990, 0.08391},
  {0.18499, 0.08184},
  {0.17016, 0.07943},
  {0.15538, 0.07667},
  {0.14076, 0.07361},
  {0.12636, 0.07023},
  {0.11227, 0.06655},
  {0.09860, 0.06257},
  {0.08537, 0.05829},
  {0.07278, 0.05382},
  {0.06126, 0.04929},
  {0.05105, 0.04478},
  {0.04218, 0.04032},
  {0.03460, 0.03598},
  {0.02824, 0.03180},
  {0.02293, 0.02781},
  {0.01848, 0.02403},
  {0.01473, 0.02054},
  {0.01155, 0.01741},
  {0.00885, 0.01465},
  {0.00660, 0.01217},
  {0.00472, 0.00989},
  {0.00318, 0.00774},
  {0.00194, 0.00569},
  {0.00100, 0.00372},
  {0.00035, 0.00181},
  {-0.00000, -0.00003},
  {-0.00004, -0.00185},
  {0.00022, -0.00368},
  {0.00082, -0.00552},
  {0.00173, -0.00735},
  {0.00293, -0.00918},
  {0.00443, -0.01100},
  {0.00623, -0.01280},
  {0.00837, -0.01457},
  {0.01086, -0.01630},
  {0.01379, -0.01790},
  {0.01726, -0.01936},
  {0.02137, -0.02066},
  {0.02632, -0.02186},
  {0.03239, -0.02309},
  {0.03973, -0.02447},
  {0.04842, -0.02584},
  {0.05871, -0.02701},
  {0.07083, -0.02794},
  {0.08476, -0.02870},
  {0.09979, -0.02937},
  {0.11534, -0.02985},
  {0.13129, -0.03015},
  {0.14744, -0.03027},
  {0.16378, -0.03023},
  {0.18028, -0.03005},
  {0.19687, -0.02974},
  {0.21359, -0.02932},
  {0.23047, -0.02883},
  {0.24741, -0.02827},
  {0.26443, -0.02766},
  {0.28157, -0.02701},
  {0.29876, -0.02636},
  {0.31597, -0.02571},
  {0.33319, -0.02507},
  {0.35042, -0.02444},
  {0.36766, -0.02381},
  {0.38492, -0.02318},
  {0.40218, -0.02255},
  {0.41944, -0.02192},
  {0.43669, -0.02129},
  {0.45393, -0.02066},
  {0.47116, -0.02003},
  {0.48838, -0.01939},
  {0.50557, -0.01876},
  {0.52274, -0.01813},
  {0.53990, -0.01749},
  {0.55706, -0.01687},
  {0.57423, -0.01623},
  {0.59142, -0.01560},
  {0.60864, -0.01498},
  {0.62583, -0.01434},
  {0.64299, -0.01371},
  {0.66015, -0.01308},
  {0.67729, -0.01245},
  {0.69444, -0.01182},
  {0.71158, -0.01119},
  {0.72873, -0.01056},
  {0.74587, -0.00993},
  {0.76302, -0.00930},
  {0.78018, -0.00867},
  {0.79735, -0.00804},
  {0.81454, -0.00741},
  {0.83176, -0.00678},
  {0.84894, -0.00615},
  {0.86610, -0.00552},
  {0.88324, -0.00489},
  {0.90035, -0.00426},
  {0.91741, -0.00364},
  {0.93432, -0.00301},
  {0.95093, -0.00240},
  {0.96680, -0.00182},
  {0.98113, -0.00129},
  {0.99325, -0.00085},
  {1.00000, -0.00060},
}};

AirfoilBuffer makeBufferClarkY() {
  std::vector<double> x;
  std::vector<double> y;
  x.reserve(kClarkYPoints.size());
  y.reserve(kClarkYPoints.size());
  for (const auto& point : kClarkYPoints) {
    x.push_back(point.first);
    y.push_back(point.second);
  }
  return AirfoilBuffer(std::move(x), std::move(y), "CLARK Y");
}

// NACA-2412 buffer with duplicated points (every second point from point
// 2; first and last remain unduplicated), as in harness test
// pangen_limit_reference.txt.
AirfoilBuffer makeBufferManyDoubled() {
  AirfoilBuffer buffer = makeBuffer2412();
  if (buffer.x.size() != kBufferCount) {
    return buffer;
  }
  for (std::size_t i = 1; i + 2 < buffer.x.size(); i += 2) {
    buffer.x[i + 1] = buffer.x[i];
    buffer.y[i + 1] = buffer.y[i];
  }
  return buffer;
}

// reference: paneling_reference.txt PANE160.
TEST(unit, PanelingPangenStandard) {
  const Result<PangenResult> r = pangen(makeBuffer2412(),
                                        xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{160});
  CHECK_EQ(p.y.size(), std::size_t{160});
  CHECK_EQ(p.s.size(), std::size_t{160});
  CHECK_EQ(p.xp.size(), std::size_t{160});
  CHECK_EQ(p.yp.size(), std::size_t{160});
  CHECK_EQ(p.xn.size(), std::size_t{160});
  CHECK_EQ(p.yn.size(), std::size_t{160});
  CHECK_EQ(p.apanel.size(), std::size_t{160});
  CHECK_FALSE(r.value().sharpLeadingEdge);
  CHECK_EQ(r.value().cornersInserted, 0);
  // reference: PANE160 (Fortran single precision).
  CHECK_NEAR(p.sle, 1.0267851352691650391, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xle, -2.4523615138605237e-7, kFortranAbsTol, kFortranRelTol);
  // YLE lies near zero (SEVAL at SLE) -> amplification; the absolute
  // tolerance is decisive.
  CHECK_NEAR(p.yle, 7.749226256237307e-6, 1.0e-5, kFortranRelTol);
  CHECK_EQ(p.xte, 1.0);
  CHECK_EQ(p.yte, 0.0);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_FALSE(p.sharpTe);
  // TE gap (blunt, GAP).
  CHECK_NEAR(r.value().teGap, 2.52e-3, 1.0e-4, 1.0e-2);
}

// reference: paneling_reference.txt PANE160 (node values).
TEST(unit, PanelingPangenStandardNodeValues) {
  const Result<PangenResult> r = pangen(makeBuffer2412(),
                                        xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  // first node (TE start point; XP/YP/NX/NY at the panel end are
  // amplified splines -> kLeRelTol).
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(p.s[0], 0.0);
  CHECK_NEAR(p.xp[0], -0.97924637794494628906, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.yp[0], 0.20267958939075469971, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xn[0], 0.20267935097217559814, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.yn[0], 0.97924518585205078125, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.apanel[0], -1.7737764120101928711, kAngleAbsTol,
             kAngleRelTol);
  // last node (TE end point; panel n-1 = TE panel).
  CHECK_EQ(p.x[159], 1.0);
  CHECK_NEAR(p.y[159], -1.2600078480318189e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.s[159], 2.0412497520446777344, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.xp[159], 0.99731415510177612305, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.yp[159], 0.073441803455352783203, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xn[159], 0.073440738022327423096, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.yn[159], -0.99729967117309570312, kLeAbsTol, kLeRelTol);
  // TE panel (blunt): ATAN2(-SX,SY)+PI with SX=0, SY=2.52e-3 -> PI.
  CHECK_NEAR(p.apanel[159], 3.1415927410125732422, kFortranAbsTol,
             kFortranRelTol);
}

// reference: paneling_reference.txt PANE200 (nonstandard, CVPAR=1.5,
// CTERAT=0.2).
TEST(unit, PanelingPangenNonstandard) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 200;
  params.bunchingParam = 1.5;
  params.teToLeDensityRatio = 0.2;
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{200});
  CHECK_NEAR(p.sle, 1.0267930030822753906, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.apanel[0], -1.7742148637771606445, kAngleAbsTol,
             kAngleRelTol);
  CHECK_NEAR(p.x[1], 0.99505239725112915039, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 2.2805577609688043594e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.apanel[1], -1.7726472616195678711, kAngleAbsTol,
             kAngleRelTol);
}

// reference: paneling_reference.txt PANE364 (effective GETPAN clamp value,
// defaults).
TEST(unit, PanelingPangen364) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 364;
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{364});
  CHECK_NEAR(p.sle, 1.0267974138259887695, kLeAbsTol, kLeRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.99657589197158813477, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 1.9670450128614902496e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: paneling_reference.txt PANECORN (duplicated point ->
// corner node insertion).
TEST(unit, PanelingPangenCornerInsertion) {
  const Result<PangenResult> r = pangen(makeBufferDuplicated(),
                                        xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(r.value().cornersInserted, 1);
  CHECK_EQ(p.x.size(), std::size_t{161});
  CHECK_EQ(p.y.size(), std::size_t{161});
  CHECK_NEAR(p.sle, 1.0267846584320068359, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.98986285924911499023, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 3.3436061348766088486e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: paneling_reference.txt PCOP (ABCOPY on a fresh buffer).
TEST(unit, PanelingAbcopyStandard) {
  const Result<AbcopyResult> r = abcopy(makeBuffer2412());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), kBufferCount);
  CHECK_EQ(p.y.size(), kBufferCount);
  CHECK_EQ(p.s.size(), kBufferCount);
  CHECK_EQ(p.apanel.size(), kBufferCount);
  // reference: PCOP (Fortran single precision).
  CHECK_NEAR(p.sle, 1.0267891883850097656, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.99815386533737182617, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 1.6416175058111548424e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.xn[0], 0.20266988873481750488, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.apanel[0], -1.7746328115463256836, kAngleAbsTol,
             kAngleRelTol);
  CHECK_NEAR(p.x[1], 0.99815386533737182617, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 1.6416175058111548424e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: paneling_reference.txt PCOPDUP (duplicated end point is
// removed).
TEST(unit, PanelingAbcopyDuplicateStrip) {
  AirfoilBuffer buffer = makeBuffer2412();
  buffer.x.push_back(buffer.x.back());
  buffer.y.push_back(buffer.y.back());
  CHECK_EQ(buffer.x.size(), kBufferCount + 1);
  const Result<AbcopyResult> r = abcopy(buffer);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), kBufferCount);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.x[1], 0.99815386533737182617, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[2], 0.99480420351028442383, kFortranAbsTol, kFortranRelTol);
}

// reference: paneling_reference.txt ABC365 (old 6.99 IQX-5 = 365, accepted at
// the boundary).
TEST(unit, PanelingAbcopy365AtLimit) {
  const Result<AbcopyResult> r = abcopy(makeBuffer365());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{365});
  // reference: ABC365 (Fortran single precision).
  CHECK_NEAR(p.xle, -3.241851231905457098e-13, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
}

// New ABCOPY upper boundary: NB = IQX-5 = 596 is accepted; NB = 597 is
// rejected without setting the current contour.
TEST(unit, PanelingAbcopy597AboveLimit) {
  AirfoilBuffer buffer = makeBuffer599();
  buffer.x.resize(597);
  buffer.y.resize(597);
  CHECK_EQ(buffer.x.size(), std::size_t{597});
  const Result<AbcopyResult> r = abcopy(buffer);
  CHECK(r.isErr());
  CHECK(r.error().code == ErrorCode::capacityExceeded);
}

// Fortran: NB <= 1 -> 'ABCOPY: Buffer airfoil not available.'
TEST(unit, PanelingAbcopyBufferTooSmall) {
  const Result<AbcopyResult> r1 = abcopy(AirfoilBuffer());
  CHECK(r1.isErr());
  CHECK(r1.error().code == ErrorCode::invalidArgument);

  AirfoilBuffer single;
  single.x = {0.5};
  single.y = {0.0};
  single.name = "Single";
  const Result<AbcopyResult> r2 = abcopy(single);
  CHECK(r2.isErr());
  CHECK(r2.error().code == ErrorCode::invalidArgument);
}

// Fortran: NB < 2 -> 'PANGEN: Buffer airfoil not available.'
TEST(unit, PanelingPangenBufferTooSmall) {
  const Result<PangenResult> r = pangen(AirfoilBuffer(),
                                        xfoil699Paneling());
  CHECK(r.isErr());
  CHECK(r.error().code == ErrorCode::invalidArgument);

  AirfoilBuffer single;
  single.x = {0.5};
  single.y = {0.0};
  single.name = "Single";
  const Result<PangenResult> r2 = pangen(single, xfoil699Paneling());
  CHECK(r2.isErr());
  CHECK(r2.error().code == ErrorCode::invalidArgument);
}

// Fortran: GETPAN clamp NPAN > IQX-6 -> NPAN = 595 + message.
TEST(unit, PanelingApplyPanelParameterClamp) {
  const std::array<double, 1> above{2000.0};
  const Result<ParameterChangeResult> rAbove = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::panelCount, above);
  CHECK(rAbove.isOk());
  CHECK(rAbove.value().clamped);
  CHECK_EQ(rAbove.value().parameters.panelCount, 595);

  const std::array<double, 1> atClamp{596.0};
  const Result<ParameterChangeResult> rAt = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::panelCount, atClamp);
  CHECK(rAt.isOk());
  CHECK(rAt.value().clamped);
  CHECK_EQ(rAt.value().parameters.panelCount, 595);

  const std::array<double, 1> within{595.0};
  const Result<ParameterChangeResult> rWithin = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::panelCount, within);
  CHECK(rWithin.isOk());
  CHECK_FALSE(rWithin.value().clamped);
  CHECK_EQ(rWithin.value().parameters.panelCount, 595);
}

// Fortran: GETPAN parameter branching P/T/R/XT/XB.
TEST(unit, PanelingApplyPanelParameterVariants) {
  const std::array<double, 1> one{1.5};
  const Result<ParameterChangeResult> rP = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::bunchingParam, one);
  CHECK(rP.isOk());
  CHECK_FALSE(rP.value().clamped);
  CHECK_EQ(rP.value().parameters.bunchingParam, 1.5);

  const Result<ParameterChangeResult> rT = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::teToLeDensityRatio, one);
  CHECK(rT.isOk());
  CHECK_EQ(rT.value().parameters.teToLeDensityRatio, 1.5);

  const Result<ParameterChangeResult> rR = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::refinementDensityRatio, one);
  CHECK(rR.isOk());
  CHECK_EQ(rR.value().parameters.refinementDensityRatio, 1.5);

  const std::array<double, 2> two{0.2, 0.8};
  const Result<ParameterChangeResult> rXT = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::upperRefinementRange, two);
  CHECK(rXT.isOk());
  CHECK_EQ(rXT.value().parameters.upperRefinementRange[0], 0.2);
  CHECK_EQ(rXT.value().parameters.upperRefinementRange[1], 0.8);

  const Result<ParameterChangeResult> rXB = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::lowerRefinementRange, two);
  CHECK(rXB.isOk());
  CHECK_EQ(rXB.value().parameters.lowerRefinementRange[0], 0.2);
  CHECK_EQ(rXB.value().parameters.lowerRefinementRange[1], 0.8);

  // defaults remain unchanged, if not set.
  CHECK_EQ(rXB.value().parameters.panelCount, 160);
  CHECK_EQ(rXB.value().parameters.bunchingParam, 1.0);
}

// controlled rejection: wrong number of values and panelCount < 2.
TEST(unit, PanelingApplyPanelParameterInvalid) {
  const std::array<double, 2> two{1.0, 2.0};
  CHECK(applyPanelParameter(xfoil699Paneling(),
                            PanelParameterKind::panelCount, two).isErr());

  const std::array<double, 1> one{1.0};
  CHECK(applyPanelParameter(xfoil699Paneling(),
                            PanelParameterKind::upperRefinementRange, one)
            .isErr());
  CHECK(applyPanelParameter(xfoil699Paneling(),
                            PanelParameterKind::lowerRefinementRange, one)
            .isErr());

  const std::array<double, 1> small{1.0};
  CHECK(applyPanelParameter(xfoil699Paneling(),
                            PanelParameterKind::panelCount, small).isErr());
}

// Fortran INIT defaults.
TEST(unit, PanelingParametersDefaults) {
  const PanelingParameters params;
  CHECK_EQ(params.panelCount, 240);
  CHECK_EQ(params.bunchingParam, 1.0);
  CHECK_EQ(params.teToLeDensityRatio, 0.3);
  CHECK_EQ(params.refinementDensityRatio, 0.2);
  CHECK_EQ(params.upperRefinementRange[0], 1.0);
  CHECK_EQ(params.upperRefinementRange[1], 1.0);
  CHECK_EQ(params.lowerRefinementRange[0], 1.0);
  CHECK_EQ(params.lowerRefinementRange[1], 1.0);
}

// reference: paneling_reference.txt NCALC4 (blunt contour).
TEST(unit, PanelingNcalcApCalcBlunt) {
  std::vector<double> x{1.0, 0.5, 0.0, 0.95};
  std::vector<double> y{0.0, 0.1, 0.0, -0.02};
  std::vector<double> s(4);
  std::vector<double> xn(4);
  std::vector<double> yn(4);
  scalc(x, y, s);
  ncalc(x, y, s, xn, yn);
  // reference: NCALC4 (Fortran single precision).
  CHECK_NEAR(xn[0], 0.48285666108131408691, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(yn[0], 0.87569940090179443359, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(xn[1], -0.052326235920190811157, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(yn[1], 0.99862998723983764648, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(xn[2], -0.39279615879058837891, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(yn[2], 0.91962558031082153320, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(xn[3], 0.068189531564712524414, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(yn[3], -0.99767231941223144531, kFortranAbsTol,
             kFortranRelTol);

  const std::vector<double> apanel = apcalc(x, y, xn, yn, false);
  CHECK_EQ(apanel.size(), std::size_t{4});
  CHECK_NEAR(apanel[0], -1.7681918144226074219, kAngleAbsTol, kAngleRelTol);
  CHECK_NEAR(apanel[1], -1.3734008073806762695, kAngleAbsTol,
             kAngleRelTol);
  CHECK_NEAR(apanel[2], 1.5497467517852783203, kAngleAbsTol, kAngleRelTol);
  CHECK_NEAR(apanel[3], 1.9513027667999267578, kAngleAbsTol, kAngleRelTol);
}

// reference: paneling_reference.txt NCALC4S (sharp TE, APANEL(N)=PI).
TEST(unit, PanelingNcalcApCalcSharp) {
  std::vector<double> x{1.0, 0.5, 0.0, 1.0};
  std::vector<double> y{0.0, 0.1, 0.0, 0.0};
  std::vector<double> s(4);
  std::vector<double> xn(4);
  std::vector<double> yn(4);
  scalc(x, y, s);
  ncalc(x, y, s, xn, yn);
  // reference: NCALC4S (Fortran single precision).
  CHECK_NEAR(xn[0], 0.48081079125404357910, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(yn[0], 0.87682437896728515625, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(xn[1], -0.053360614925622940063, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(yn[1], 0.99857527017593383789, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(xn[2], -0.37695470452308654785, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(yn[2], 0.92623168230056762695, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(xn[3], 0.083251997828483581543, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(yn[3], -0.99652856588363647461, kFortranAbsTol,
             kFortranRelTol);

  const std::vector<double> apanel = apcalc(x, y, xn, yn, true);
  CHECK_EQ(apanel.size(), std::size_t{4});
  CHECK_NEAR(apanel[0], -1.7681918144226074219, kAngleAbsTol, kAngleRelTol);
  CHECK_NEAR(apanel[1], -1.3734008073806762695, kAngleAbsTol,
             kAngleRelTol);
  // sharp TE panel: PI (Fortran single-precision PI).
  CHECK_NEAR(apanel[2], 1.5707963705062866211, kAngleAbsTol, kAngleRelTol);
  CHECK_NEAR(apanel[3], 3.1415927410125732422, kFortranAbsTol,
             kFortranRelTol);
}

// Fortran: NCALC N <= 1 -> return without change.
TEST(unit, PanelingNcalcNoop) {
  std::vector<double> x{0.5};
  std::vector<double> y{0.1};
  std::vector<double> s{0.0};
  std::vector<double> xn{7.0};
  std::vector<double> yn{9.0};
  ncalc(x, y, s, xn, yn);
  CHECK_EQ(xn[0], 7.0);
  CHECK_EQ(yn[0], 9.0);
}

// Fortran: corner averaging for duplicate points (S(I)==S(I+1)).
TEST(unit, PanelingNcalcCornerAveraging) {
  std::vector<double> x{0.0, 1.0, 1.0, 2.0};
  std::vector<double> y{0.0, 0.0, 1.0, 2.0};
  std::vector<double> s{0.0, 1.0, 1.0, 2.0};
  std::vector<double> xn(4);
  std::vector<double> yn(4);
  ncalc(x, y, s, xn, yn);
  // corner averaging: the normals at the corner (indices 1/2) are exactly
  // equal and normalized.
  CHECK_EQ(xn[1], xn[2]);
  CHECK_EQ(yn[1], yn[2]);
  CHECK_NEAR(xn[1] * xn[1] + yn[1] * yn[1], 1.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(xn[0] * xn[0] + yn[0] * yn[0], 1.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(xn[3] * xn[3] + yn[3] * yn[3], 1.0, 1.0e-12, 1.0e-12);
}

// Fortran: APCALC degenerate panel (SX==SY==0) -> ATAN2(-NY,-NX).
TEST(unit, PanelingApCalcDegeneratePanel) {
  std::vector<double> x{0.0, 1.0, 1.0};
  std::vector<double> y{0.0, 0.0, 0.0};
  std::vector<double> xn{0.0, -1.0, -1.0};
  std::vector<double> yn{1.0, 0.0, 0.0};
  const std::vector<double> apanel = apcalc(x, y, xn, yn, false);
  CHECK_EQ(apanel.size(), std::size_t{3});
  // panel 0: SX=1, SY=0 -> ATAN2(1,0) = PI/2.
  CHECK_NEAR(apanel[0], 1.5707963267948966, 1.0e-9, 1.0e-9);
  // panel 1 (degenerate): ATAN2(-NY,-NX) = ATAN2(0,1) = 0.
  CHECK_NEAR(apanel[1], 0.0, 1.0e-9, 1.0e-9);
  // TE panel (blunt): SX=X(1)-X(N)=-1, SY=0 -> ATAN2(1,0)+PI = 3*PI/2.
  CHECK_NEAR(apanel[2], 4.7123889803846897, 1.0e-9, 1.0e-9);
}

// Fortran: TECALC geometry share (sharp and blunt).
TEST(unit, PanelingTeGeometry) {
  // sharp: X(0)==X(N), Y(0)==Y(N) -> DSTE = 0 < 0.0001*CHORD.
  const TrailingEdgeGeometry sharp = teGeometry(1.0, 0.0, -1.0, 0.0,
                                                1.0, 0.0, -1.0, 0.0, 1.0);
  CHECK(sharp.sharp);
  CHECK_EQ(sharp.dste, 0.0);
  CHECK_EQ(sharp.scs, 1.0);
  CHECK_EQ(sharp.sds, 0.0);

  // blunt: DXT = 1-0.95 = 0.05, DYT = 0-(-0.02) = 0.02.
  const TrailingEdgeGeometry blunt = teGeometry(1.0, 0.0, -1.0, 0.0,
                                                0.95, -0.02, 1.0, 0.5, 1.0);
  CHECK_FALSE(blunt.sharp);
  const double dste = 0.053851648071345040313;
  CHECK_NEAR(blunt.dste, dste, 1.0e-12, 1.0e-12);
  // ANTE = DXS*DYTE - DYS*DXTE with DXS=0.5*(-(-1)+1)=1, DYS=0.5*(-0+0.5)=0.25.
  const double ante = 1.0 * 0.02 - 0.25 * 0.05;
  CHECK_NEAR(blunt.ante, ante, 1.0e-12, 1.0e-12);
  const double aste = 1.0 * 0.05 + 0.25 * 0.02;
  CHECK_NEAR(blunt.aste, aste, 1.0e-12, 1.0e-12);
  CHECK_NEAR(blunt.scs, ante / dste, 1.0e-12, 1.0e-12);
  CHECK_NEAR(blunt.sds, aste / dste, 1.0e-12, 1.0e-12);
}

// Fortran: TECALC geometry share on the 4-point test contour (reference
// xfoil_geom_reference.txt: DSTE = 5.385...e-2).
TEST(unit, PanelingTeGeometryHarnessContour) {
  const TrailingEdgeGeometry te = teGeometry(1.0, 0.0, -1.0, 0.0,
                                             0.95, -0.02, 1.0, 0.5, 1.0);
  CHECK_FALSE(te.sharp);
  // reference: xfoil_geom_reference.txt TECALC open DSTE.
  CHECK_NEAR(te.dste, 5.3851648071345040313e-2, 1.0e-9, 1.0e-9);
  CHECK_NEAR(te.ante, 7.5e-3, 1.0e-9, 1.0e-9);
  CHECK_NEAR(te.aste, 5.5e-2, 1.0e-9, 1.0e-9);
}

// Fortran: chord <= 0 is rejected in a controlled way.
TEST(unit, PanelingTeGeometryInvalidChord) {
  bool threw = false;
  try {
    teGeometry(1.0, 0.0, -1.0, 0.0, 0.95, -0.02, 1.0, 0.5, 0.0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// PanelGeometry::make checks the length invariant.
TEST(unit, PanelingGeometryMakeInvariant) {
  const PanelGeometry panel = PanelGeometry::make(
      std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, 0.0},
      std::vector<double>{0.0, 1.0}, std::vector<double>{-1.0, -1.0},
      std::vector<double>{0.0, 0.0}, std::vector<double>{0.0, -1.0},
      std::vector<double>{1.0, 0.0}, std::vector<double>{0.0, 0.0},
      0.0, 0.0, 0.0, 1.0, 0.0, 1.0, false);
  CHECK_EQ(panel.x.size(), std::size_t{2});
  CHECK_EQ(panel.apanel.size(), std::size_t{2});

  bool threw = false;
  try {
    const PanelGeometry ignored = PanelGeometry::make(
        std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, 0.0},
        std::vector<double>{0.0}, std::vector<double>{-1.0, -1.0},
        std::vector<double>{0.0, 0.0}, std::vector<double>{0.0, -1.0},
        std::vector<double>{1.0, 0.0}, std::vector<double>{0.0, 0.0},
        0.0, 0.0, 0.0, 1.0, 0.0, 1.0, false);
    static_cast<void>(ignored);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// capacity limits (canonical, XFOIL.INC IQX=601).
TEST(unit, PanelingCapacityLimits) {
  CHECK_EQ(kMaxPanelingPanelCount, 595);
  CHECK_EQ(kMaxAbcopyNodeCount, 596);
  CHECK_EQ(kMaxPanelNodeCount, 600);
}

// pangen accepts the former 6.99 boundary values (370, 371) and the new
// canonical upper boundary 600.
TEST(unit, PanelingPangenIqxBoundaryAccepted) {
  for (const int n : {370, 371}) {
    PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
    params.panelCount = n;
    const Result<PangenResult> r = pangen(makeBuffer2412(), params);
    CHECK(r.isOk());
    CHECK_EQ(r.value().panel.x.size(), static_cast<std::size_t>(n));
  }
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 600;
  const Result<PangenResult> r = pangen(makeBuffer599(), params);
  CHECK(r.isOk());
  CHECK_EQ(r.value().panel.x.size(), std::size_t{600});
}

// pangen rejects panelCount > 600 (canonical PANGEN guard IQX-1 = 600;
// the first disallowed cases are 601 and 602).
TEST(unit, PanelingPangenAboveCapacity) {
  for (const int n : {601, 602}) {
    PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
    params.panelCount = n;
    const Result<PangenResult> r = pangen(makeBuffer599(), params);
    CHECK(r.isErr());
    CHECK(r.error().code == ErrorCode::capacityExceeded);
  }
}

// direct PANGEN call for NPAN=365 (a 6.99 boundary; still valid with the
// new 6.996 IQX contract). IQX-6 is only the GETPAN/PPAR input boundary;
// the pure PANGEN kernel handles 365 directly (Fortran driver
// verification, NACA 2412, defaults).
TEST(unit, PanelingPangen365Direct) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 365;
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{365});
  // Fortran driver reference (extracted PANGEN, NPAN=365, defaults).
  CHECK_NEAR(p.sle, 1.0267976522445678711, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xle, 1.6370904631912708282e-11, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(p.yle, -4.5131017145649821032e-8, 2.0e-8, kFortranRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.99658566713333129883, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 1.9650342874228954315e-3, kFortranAbsTol,
             kFortranRelTol);
}

// direct PANGEN call for NPAN=369 (old 6.99 protection boundary,
// still inside the new IQX-1 = 600 limit; Fortran driver verification).
TEST(unit, PanelingPangen369Direct) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 369;
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{369});
  // Fortran driver reference (extracted PANGEN, NPAN=369, defaults).
  CHECK_NEAR(p.sle, 1.0267977714538574219, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xle, -1.1027623258996754885e-11, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(p.yle, -2.9684519731176806090e-8, 2.0e-8, kFortranRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.99662417173385620117, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 1.9571050070226192474e-3, kFortranAbsTol,
             kFortranRelTol);
}

// the GETPAN/PPAR input boundary (IQX-6 = 595) does not apply to the
// direct PANGEN call: 596..600 are all accepted.
TEST(unit, PanelingSeparatedLimits) {
  // direct PANGEN call: 596..600 all permitted.
  for (const int panelCount : {596, 597, 598, 599, 600}) {
    PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
    params.panelCount = panelCount;
    const Result<PangenResult> r = pangen(makeBuffer599(), params);
    CHECK(r.isOk());
    CHECK_EQ(r.value().panel.x.size(),
             static_cast<std::size_t>(panelCount));
  }
  // GETPAN/PPAR input boundary: 596 -> clamp to 595.
  const std::array<double, 1> at596{596.0};
  const Result<ParameterChangeResult> par = applyPanelParameter(
      xfoil699Paneling(), PanelParameterKind::panelCount, at596);
  CHECK(par.isOk());
  CHECK(par.value().clamped);
  CHECK_EQ(par.value().parameters.panelCount, 595);
}

// PangenResult structure: diagnostics empty on regular completion.
TEST(unit, PanelingPangenDiagnostics) {
  const Result<PangenResult> r = pangen(makeBuffer2412(),
                                        xfoil699Paneling());
  CHECK(r.isOk());
  CHECK(r.value().diagnostics.empty());
  // panel size ratios (user information) are finite and positive.
  CHECK(r.value().dsminRatio > 0.0);
  CHECK(r.value().dsmaxRatio >= r.value().dsminRatio);
}

// reference: paneling_reference.txt PANE12 (NACA 0012, standard parameters).
TEST(unit, PanelingPangenNaca0012) {
  const Result<NacaResult> n = naca4(12, kNside);
  CHECK(n.isOk());
  AirfoilBuffer buffer(n.value().xb, n.value().yb, n.value().name);
  const Result<PangenResult> r = pangen(buffer, xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{160});
  CHECK_NEAR(p.sle, 1.0196212530136108398, kLeAbsTol, kLeRelTol);
  // XLE/YLE lie near zero (SEVAL at SLE) -> absolute tolerance.
  CHECK_NEAR(p.xle, -5.1299139158800244331e-8, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(p.yle, 1.4551915228366851807e-11, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.apanel[0], -1.7094925642013549805, kAngleAbsTol,
             kAngleRelTol);
  CHECK_EQ(p.x[159], 1.0);
  CHECK_NEAR(p.y[159], -1.2600078480318189e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.apanel[159], 3.1415927410125732422, kFortranAbsTol,
             kFortranRelTol);
}

// reference: paneling_reference.txt PANECLY (Clark Y, standard parameters).
TEST(unit, PanelingPangenClarkY) {
  const Result<PangenResult> r = pangen(makeBufferClarkY(),
                                        xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{160});
  CHECK_NEAR(p.sle, 1.0335021018981933594, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xle, -6.0916063375771045685e-5, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(p.yle, -1.2064420152455568314e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(p.chord, 1.0000616312026977539, 1.0e-6, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 6.0000002849847078323e-4, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(p.apanel[0], -1.8039957284927368164, kAngleAbsTol,
             kAngleRelTol);
  CHECK_NEAR(p.x[1], 0.99171018600463867188, kFortranAbsTol, kFortranRelTol);
}

// reference: paneling_reference.txt PANEREF (refinement ranges
// XSREF/XPREF = 0.2..0.4, NPAN=200).
TEST(unit, PanelingPangenRefinementZones) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 200;
  params.upperRefinementRange = {0.2, 0.4};
  params.lowerRefinementRange = {0.2, 0.4};
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{200});
  CHECK_NEAR(p.sle, 1.02678680419921875, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.99159669876098632812, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.y[1], 2.9893021564930677414e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: paneling_reference.txt PANESLE (duplicated LE point ->
// corner node insertion in the LE range).
TEST(unit, PanelingPangenSharpLeRegion) {
  const Result<NacaResult> n = naca4(2412, kNside);
  CHECK(n.isOk());
  AirfoilBuffer buffer(n.value().xb, n.value().yb, n.value().name);
  buffer.x[kNside + 1] = buffer.x[kNside];
  buffer.y[kNside + 1] = buffer.y[kNside];
  const Result<PangenResult> r = pangen(buffer, xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  CHECK_EQ(p.x.size(), std::size_t{161});
  CHECK_NEAR(p.sle, 1.0267913341522216797, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.xle, 1.8350692698732018471e-7, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(p.yle, -5.4658676162944175303e-6, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(p.chord, 0.99999982118606567383, 1.0e-6, kFortranRelTol);
  CHECK_EQ(p.x[0], 1.0);
  CHECK_NEAR(p.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.x[1], 0.99181747436523437500, kFortranAbsTol, kFortranRelTol);
}

// protection case IQX-1 = 600: corner insertion must not exceed the
// capacity; controlled error without partial state.
TEST(unit, PanelingPangenIqx1Protection) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 600;
  const Result<PangenResult> r = pangen(makeBufferManyDoubled(), params);
  CHECK(r.isErr());
  CHECK(r.error().code == ErrorCode::capacityExceeded);
}

// canonical PSAV-reference comparison: panel-coord/pane-default-psav
// (PANE + PSAV of the original implementation, F9.6/E12.4 display rounding).
TEST(unit, PanelingPsaReferenceStandard) {
  const Result<PangenResult> r = pangen(makeBuffer2412(),
                                        xfoil699Paneling());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  // PSAV display rounding (F9.6/E12.4).
  constexpr double kDisplayTol = 5.0e-7;
  CHECK_EQ(p.x.size(), std::size_t{160});
  CHECK_NEAR(p.x[0], 1.000000, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[0], 1.260000e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[1], 0.9917995, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[1], 2.947775e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[2], 0.9806745, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[2], 5.207166e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[158], 0.9801923, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[158], -2.706883e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[159], 1.000000, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[159], -1.260000e-3, kDisplayTol, 1.0);
}

// canonical PSAV-reference comparison: panel-coord/ppar-n200-psav
// (PPAR N 200 + PSAV, N with defaults).
TEST(unit, PanelingPsaReferencePpar200) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 200;
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  constexpr double kDisplayTol = 5.0e-7;
  CHECK_EQ(p.x.size(), std::size_t{200});
  CHECK_NEAR(p.x[0], 1.000000, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[0], 1.260000e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[1], 0.9935584, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[1], 2.587371e-3, kDisplayTol, 1.0);
}

// canonical PSAV-reference comparison: panel-coord/ppar-n364-at-clamp and
// ppar-n365-above-clamp (historical 6.99 clamp values).
TEST(unit, PanelingPsaReferencePpar364Clamp) {
  PanelingParameters params;
  params.teToLeDensityRatio = 0.15;
  params.panelCount = 364;
  const Result<PangenResult> r = pangen(makeBuffer2412(), params);
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  constexpr double kDisplayTol = 5.0e-7;
  CHECK_EQ(p.x.size(), std::size_t{364});
  CHECK_NEAR(p.x[1], 0.9965759, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[1], 1.967040e-3, kDisplayTol, 1.0);
}

// canonical PSAV-reference comparison: panel-coord/pcop-psav
// (PCOP + PSAV, N = NB = 245).
TEST(unit, PanelingPsaReferencePcop) {
  const Result<AbcopyResult> r = abcopy(makeBuffer2412());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  constexpr double kDisplayTol = 5.0e-7;
  CHECK_EQ(p.x.size(), std::size_t{245});
  CHECK_NEAR(p.x[0], 1.000000, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[0], 1.260000e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[1], 0.9981539, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[1], 1.641618e-3, kDisplayTol, 1.0);
}

// canonical PSAV-reference comparison: panel-coord/abcopy-365-at-limit
// (365-point contour is adopted).
TEST(unit, PanelingPsaReferenceAbcopy365) {
  const Result<AbcopyResult> r = abcopy(makeBuffer365());
  CHECK(r.isOk());
  const PanelGeometry& p = r.value().panel;
  constexpr double kDisplayTol = 5.0e-7;
  CHECK_EQ(p.x.size(), std::size_t{365});
  CHECK_NEAR(p.x[0], 1.000000, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[0], 1.253700e-3, kDisplayTol, 1.0);
  CHECK_NEAR(p.x[1], 0.9999255, kDisplayTol, 1.0);
  CHECK_NEAR(p.y[1], 1.271600e-3, kDisplayTol, 1.0);
}

}  // namespace

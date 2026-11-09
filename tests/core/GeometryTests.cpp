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

// File: tests/core/GeometryTests.cpp
// unit tests for the new 0-based core: geometry and NACA kernels
// (NACA4/NACA5/NACA, LE pipeline, geometry parameters, transformations,
// interpolation/refinement). Both nominal and degenerate symmetric cases are
// covered.
//
// the tests use only xfoil::core::geometry and no app, CLI, or
// 1-based code.
//
// reference comparison: the expected values come from the focused Fortran
// harnesses (tests/fortran-reference/harnesses/geometry), which call the
// canonical routines unchanged. Fortran REAL is single precision; the new
// core computes in double. Formula and operation order are identical; the
// deviations lie in the single-to-double range or in the documented
// degenerate symmetric cases (curvature/ATAN2 amplification near LE and TE).
//
// tolerance notes (measured):
// - kFortranRelTol/kFortranAbsTol: base for all smooth values.
// - kLeRelTol (1e-4): RADLE = 1/CURV and ANGTE = ATAN2 difference at the
// sharp TE panel ends amplify single-precision rounding.
// - kAngleAbsTol (5e-3 degrees): CANG angle in degrees (Fortran display F9.3
// rounds to 3 decimal places; measured deviation 1.7e-4 relative).
// - kDisplayAbsTol (1e-3): CLIS gives x/y/s/theta/curv with F10.5/F11.3/
// F12.3 (display rounding).
// - degenerate symmetric 0012: CAMBR/XCAMBR/YC/YCT/APX are noise values
// (curvature/camber ~ 0); compared with absolute tolerances; the location
// of the maxima is not comparable.

#include "TestSupport.h"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/math/Spline.h"

namespace {

using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::math;

// relative tolerance for the single-precision (Fortran) to double (C++) comparison.
constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;

// Amplified values (1/CURV, ATAN2 at sharp panel ends).
constexpr double kLeRelTol = 1.0e-4;
constexpr double kLeAbsTol = 1.0e-7;

// CANG angle in degrees (display rounding plus measured deviation).
constexpr double kAngleAbsTol = 5.0e-3;
constexpr double kAngleRelTol = 1.0e-3;

// CLIS display rounding (F10.5/F11.3/F12.3).
constexpr double kDisplayAbsTol = 1.0e-3;

// canonical NACA buffer point count (Fortran NACA: NSIDE=IQX/3=123).
constexpr std::size_t kNside = 123;
constexpr std::size_t kBufferCount = 245;

// Builds the splined contour of a NACA buffer (SCALC/SEGSPL/LEFIND as
// in the xfoil.f calling context).
SplineProfile makeProfile(int ides) {
  const NacaResult n = naca4(ides, kNside).value();
  SplineProfile p;
  p.x = n.xb;
  p.y = n.yb;
  p.s.resize(n.xb.size());
  p.xp.resize(n.xb.size());
  p.yp.resize(n.xb.size());
  scalc(p.x, p.y, p.s);
  segspl(p.x, p.s, p.xp);
  segspl(p.y, p.s, p.yp);
  p.sle = lefind(p.x, p.xp, p.y, p.yp, p.s).sle;
  return p;
}

}  // namespace

// ---
// NACA4/NACA5
// ---

TEST(unit, GeometryNaca4N0012Buffer) {
  const Result<NacaResult> r = naca4(12, kNside);
  CHECK(r.isOk());
  const NacaResult& n = r.value();
  CHECK_EQ(n.xb.size(), kBufferCount);
  CHECK_EQ(n.yb.size(), kBufferCount);
  CHECK_EQ(n.xx.size(), kNside);
  CHECK_EQ(n.yt.size(), kNside);
  CHECK_EQ(n.yc.size(), kNside);
  CHECK(n.name == "NACA 0012");
  // reference: naca_reference.txt (Fortran single precision).
  CHECK_EQ(n.xb[0], 1.0);
  CHECK_NEAR(n.yb[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(n.xb[1], 0.99815386533737183, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(n.yb[1], 1.5187292592599988e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(n.xb[kBufferCount - 1], 1.0);
  CHECK_NEAR(n.yb[kBufferCount - 1], -1.2600078480318189e-3, kFortranAbsTol,
             kFortranRelTol);
  // point layout: upper side first (LE->TE), then lower side from point 2.
  CHECK_NEAR(n.xb[1], n.xb[kBufferCount - 2], 1.0e-12, 1.0e-12);
  CHECK_NEAR(n.yb[1], -n.yb[kBufferCount - 2], kFortranAbsTol,
             kFortranRelTol);
}

TEST(unit, GeometryNaca4N2412SideArrays) {
  const Result<NacaResult> r = naca4(2412, kNside);
  CHECK(r.isOk());
  const NacaResult& n = r.value();
  CHECK_EQ(n.xb.size(), kBufferCount);
  CHECK(n.name == "NACA 2412");
  // reference: XX/YT/YC side arrays from naca_reference.txt.
  CHECK_EQ(n.xx[0], 0.0);
  CHECK_EQ(n.yt[0], 0.0);
  CHECK_EQ(n.yc[0], 0.0);
  CHECK_NEAR(n.xx[1], 1.2558698654174805e-4, 1.0e-7, 1.0);
  CHECK_NEAR(n.yt[1], 1.9868388772010803e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(n.yc[1], 1.2556724868773017e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(n.xx[2], 5.0109624862670898e-4, 1.0e-7, 1.0);
  CHECK_NEAR(n.yt[2], 3.9497599937021732e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(n.yc[2], 5.0078229833161458e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(n.xx[kNside - 1], 1.0);
  CHECK_NEAR(n.yt[kNside - 1], 1.26e-3, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(n.yc[kNside - 1], 0.0, kFortranAbsTol, kFortranRelTol);
  // TE gap: YB(1) = YC+YT, YB(NB) = YC-YT
  CHECK_NEAR(n.xb[0], 1.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(n.yb[0], 1.26e-3, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(n.yb[kBufferCount - 1], -1.26e-3, 1.0e-9, kFortranRelTol);
}

TEST(unit, GeometryNaca5N23012Buffer) {
  const Result<Naca5Result> r = naca5(23012, kNside);
  CHECK(r.isOk());
  const Naca5Result& n = r.value();
  CHECK_EQ(n.xb.size(), kBufferCount);
  CHECK_EQ(n.yb.size(), kBufferCount);
  CHECK_EQ(n.xx.size(), kNside);
  CHECK(n.name == "NACA 23012");
  CHECK_EQ(n.xb[0], 1.0);
  CHECK_NEAR(n.yb[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(n.xb[1], 0.99815386533737183, 1.0e-9, kFortranRelTol);
  CHECK_NEAR(n.yb[1], 1.5594990691170096e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(n.xb[2], 0.99480420351028442, 1.0e-9, kFortranRelTol);
}

TEST(unit, GeometryNaca61Points) {
  const Result<NacaResult> r4 = naca4(1234, 61);
  CHECK(r4.isOk());
  CHECK_EQ(r4.value().xb.size(), std::size_t{121});
  CHECK_EQ(r4.value().xx.size(), std::size_t{61});
  CHECK(r4.value().name == "NACA 1234");
  const Result<Naca5Result> r5 = naca5(24012, 61);
  CHECK(r5.isOk());
  CHECK_EQ(r5.value().xb.size(), std::size_t{121});
  CHECK(r5.value().name == "NACA 24012");
}

TEST(unit, GeometryNaca5IllegalDesignationFails) {
  const Result<Naca5Result> r = naca5(2110, kNside);
  CHECK(r.isErr());
  const std::string& msg = r.error().message;
  CHECK(msg.find("Illegal 5-digit designation") != std::string::npos);
}

TEST(unit, GeometryNaca4OutOfRangeFails) {
  CHECK(naca4(12345, kNside).isErr());
  CHECK(naca4(-1, kNside).isErr());
}

TEST(unit, GeometryNacaTooSmallNsideFails) {
  CHECK(naca4(12, 1).isErr());
  CHECK(naca5(23012, 1).isErr());
}

TEST(unit, GeometryNaca4FirstLastZeroBased) {
  // 0-based first/last indices: XX(1)=0, XX(NSIDE)=1
  const Result<NacaResult> r = naca4(12, kNside);
  const NacaResult& n = r.value();
  CHECK_EQ(n.xx.front(), 0.0);
  CHECK_EQ(n.xx.back(), 1.0);
  // buffer: XB(1) = XB(NB) = 1.0 (TE point bundling)
  CHECK_EQ(n.xb.front(), 1.0);
  CHECK_EQ(n.xb.back(), 1.0);
}

// ---
// SCALC/SEGSPL/LEFIND/XLFIND/NSFIND/SOPPS/SEVAL (LE-pipeline)
// ---

TEST(unit, GeometryScalc0012ArcLengths) {
  const SplineProfile p = makeProfile(12);
  CHECK_EQ(p.s.size(), kBufferCount);
  CHECK_EQ(p.s[0], 0.0);
  // reference: le_pipeline_reference.txt SB-values (0012).
  CHECK_NEAR(p.s[1], 1.8641754286363721e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.s[121], 1.0176333189010620, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.s[122], 1.0196241140365601, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(p.s[244], 2.0392489433288574, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometrySegspl0012Derivatives) {
  const SplineProfile p = makeProfile(12);
  // reference: le_pipeline_reference.txt XBP/YPB-values (0012).
  CHECK_NEAR(p.xp[0], -0.99030160903930664, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(p.yp[0], 0.13893543183803558, kLeAbsTol, kLeRelTol);
}

TEST(unit, GeometryLefindN0012) {
  const SplineProfile p = makeProfile(12);
  const LeadingEdgeResult le = lefind(p.x, p.xp, p.y, p.yp, p.s);
  CHECK(le.status == LeStatus::completed);
  // reference: LEFIND 0012 SBLE = 0.10196241140365600586E+01
  CHECK_NEAR(le.sle, 1.0196241140365601, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryXlfindN0012) {
  const SplineProfile p = makeProfile(12);
  const LeadingEdgeResult xl = xlfind(p.x, p.xp, p.y, p.yp, p.s);
  CHECK(xl.status == LeStatus::completed);
  CHECK_NEAR(xl.sle, 1.0196241140365601, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryNsfindN0012) {
  const SplineProfile p = makeProfile(12);
  const LeadingEdgeResult ns = nsfind(p.x, p.xp, p.y, p.yp, p.s);
  CHECK(ns.status == LeStatus::completed);
  CHECK_NEAR(ns.sle, 1.0196241140365601, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryLefindN2412) {
  const SplineProfile p = makeProfile(2412);
  const LeadingEdgeResult le = lefind(p.x, p.xp, p.y, p.yp, p.s);
  CHECK(le.status == LeStatus::completed);
  // reference: LEFIND 2412 SBLE = 0.10267891883850097656E+01
  CHECK_NEAR(le.sle, 1.0267891883850098, kFortranAbsTol, kFortranRelTol);
  const LeadingEdgeResult xl = xlfind(p.x, p.xp, p.y, p.yp, p.s);
  CHECK_NEAR(xl.sle, 1.0267891883850098, kFortranAbsTol, kFortranRelTol);
  const LeadingEdgeResult ns = nsfind(p.x, p.xp, p.y, p.yp, p.s);
  // reference: NSFIND 2412 SBLE = 0.10285856723785400391E+01
  CHECK_NEAR(ns.sle, 1.0285856723785400, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometrySoppsN0012) {
  const SplineProfile p = makeProfile(12);
  const OppositePointResult op1 = sopps(p.s[0], p.x, p.xp, p.y, p.yp, p.s,
                                        p.sle);
  CHECK(op1.status == Diagnostic::completed);
  // reference: SOPPS 0012 SI=SB(1) SOPP = 0.20392489433288574219E+01
  CHECK_NEAR(op1.sopp, 2.0392489433288574, kFortranAbsTol, kFortranRelTol);
  const OppositePointResult op2 = sopps(p.s[121], p.x, p.xp, p.y, p.yp, p.s,
                                        p.sle);
  // reference: SOPPS 0012 SI=SB(NB/2) SOPP = 0.10216149091720581055E+01
  CHECK_NEAR(op2.sopp, 1.0216149091720581, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometrySoppsN2412) {
  const SplineProfile p = makeProfile(2412);
  const OppositePointResult op1 = sopps(p.s[0], p.x, p.xp, p.y, p.yp, p.s,
                                        p.sle);
  CHECK(op1.status == Diagnostic::completed);
  // reference: SOPPS 2412 SI=SB(1) SOPP = 0.20412559509277343750E+01
  CHECK_NEAR(op1.sopp, 2.0412559509277344, kFortranAbsTol, kFortranRelTol);
  const OppositePointResult op2 = sopps(p.s[121], p.x, p.xp, p.y, p.yp, p.s,
                                        p.sle);
  // reference: SOPPS 2412 SI=SB(NB/2) SOPP = 0.10288316011428833008E+01.
  // SOPPS Newton amplifies single-precision rounding (6.0e-5).
  CHECK_NEAR(op2.sopp, 1.0288316011428833, kLeAbsTol, kLeRelTol);
}

TEST(unit, GeometrySevalSleN0012) {
  const SplineProfile p = makeProfile(12);
  // reference: SEVAL0012 X@SLE = 0, Y@SLE = 0
  CHECK_NEAR(seval(p.sle, p.x, p.xp, p.s), 0.0, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(seval(p.sle, p.y, p.yp, p.s), 0.0, kFortranAbsTol,
             kFortranRelTol);
  // reference: DEVAL0012/D2VAL0012/CURV0012 @SLE
  CHECK_NEAR(deval(p.sle, p.x, p.xp, p.s), 7.3095995922756174e-9, kLeAbsTol,
             kLeRelTol);
  CHECK_NEAR(deval(p.sle, p.y, p.yp, p.s), -1.0006933212280273, kLeAbsTol,
             kLeRelTol);
  CHECK_NEAR(d2val(p.sle, p.x, p.xp, p.s), 63.453460693359375, kLeAbsTol,
             kLeRelTol);
  // degenerate symmetric case: D2VAL(Y) at the spline LE position is
  // noise (Fortran -2.3498872178606689E-04, C++ ~0).
  CHECK_NEAR(d2val(p.sle, p.y, p.yp, p.s), 0.0, 1.0e-3, 1.0);
  CHECK_NEAR(curv(p.sle, p.x, p.xp, p.y, p.yp, p.s), 63.365562438964844,
             kLeAbsTol, kLeRelTol);
}

TEST(unit, GeometrySevalNodesN0012) {
  const SplineProfile p = makeProfile(12);
  // reference: SEVAL0012 at nodes S(1), S(NB/2)=S(122) (1-based,
  // 0-based 121), S(NB)
  CHECK_NEAR(seval(p.s[0], p.x, p.xp, p.s), 1.0, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(seval(p.s[0], p.y, p.yp, p.s), 1.2600078480318189e-3,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(p.s[121], p.x, p.xp, p.s), 1.2558698654174805e-4,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(p.s[121], p.y, p.yp, p.s), 1.9868388772010803e-3,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(p.s[244], p.x, p.xp, p.s), 1.0, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(seval(p.s[244], p.y, p.yp, p.s), -1.2600078480318189e-3,
             kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometrySevalSleN2412) {
  const SplineProfile p = makeProfile(2412);
  // Note: the harness overwrites SBLE via LEFIND/XLFIND/NSFIND;
  // the SEVAL@SLE values refer to the NSFIND result.
  const LeadingEdgeResult ns = nsfind(p.x, p.xp, p.y, p.yp, p.s);
  // reference: SEVAL2412 X@SLE = 0.10345539340050891042E-03
  // SEVAL2412 Y@SLE = -0.17937220400199294090E-02
  CHECK_NEAR(seval(ns.sle, p.x, p.xp, p.s), 1.0345539340050891e-4,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(ns.sle, p.y, p.yp, p.s), -1.7937220400199294e-3,
             kFortranAbsTol, kFortranRelTol);
  // reference: SEVAL2412 at S(NB/2)=S(122) (1-based, 0-based 121).
  CHECK_NEAR(seval(p.s[121], p.x, p.xp, p.s), 1.2558698654174805e-4,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(p.s[121], p.y, p.yp, p.s), 1.9993956666439772e-3,
             kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryLefindTooFewPointsThrows) {
  const std::vector<double> x = {0.0, 1.0};
  const std::vector<double> xp(2, 0.0);
  const std::vector<double> s = {0.0, 1.0};
  bool threw = false;
  try {
    lefind(x, xp, x, xp, s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, GeometryNsfindOverflowThrows) {
  const std::vector<double> x(501, 0.0);
  const std::vector<double> xp(501, 0.0);
  const std::vector<double> s(501, 0.0);
  bool threw = false;
  try {
    nsfind(x, xp, x, xp, s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// ---
// GEOPAR/AECALC/TCCALC/NORM
// ---

TEST(unit, GeometryGeoparN0012Metrics) {
  const SplineProfile p = makeProfile(12);
  const GeometryMetrics m = geopar(p.x, p.xp, p.y, p.yp, p.s);
  // reference: geopar_reference.txt GEOPAR0012 (Fortran single precision).
  CHECK_NEAR(m.sle, 1.0196241140365601, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.area, 8.2203105092048645e-2, kFortranAbsTol, kFortranRelTol);
  // RADLE = 1/CURV amplifies single-precision rounding (1.9e-5).
  CHECK_NEAR(m.radle, 1.5781443566083908e-2, kLeAbsTol, kLeRelTol);
  // ANGTE at the sharp TE panel ends amplifies as well (9.6e-5).
  CHECK_NEAR(m.angte, 0.27877265214920044, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.ei11a, 6.8077351897954941e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei22a, 4.5377984642982483e-3, kFortranAbsTol, kFortranRelTol);
  // degenerate symmetric case: APX1A is exactly 0.
  CHECK_NEAR(m.apx1a, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.apx2a, -1.5707963705062866, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei11t, 3.9852261543273926e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei22t, 0.17558625340461731, kFortranAbsTol, kFortranRelTol);
  // APX1T is noise in the degenerate symmetric case
  // (Fortran 2.3841857910156250E-06, C++ exactly 0).
  CHECK_NEAR(m.apx1t, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.apx2t, -1.5707966251296997, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.thick, 0.12003208696842194, kFortranAbsTol, kFortranRelTol);
  // CAMBR is ~0 in the symmetric case (Fortran -6.05e-8, C++ -5.9e-17).
  CHECK_NEAR(m.cambr, 0.0, 1.0e-6, 1.0);
}

TEST(unit, GeometryGeoparN2412Metrics) {
  const SplineProfile p = makeProfile(2412);
  const GeometryMetrics m = geopar(p.x, p.xp, p.y, p.yp, p.s);
  // reference: geopar_reference.txt GEOPAR2412.
  CHECK_NEAR(m.sle, 1.0267891883850098, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.area, 8.2203090190887451e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.radle, 1.5784906223416328e-2, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.angte, 0.27757826447486877, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.ei11a, 6.9929519668221474e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei22a, 4.5378385111689568e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.apx1a, 2.1371841430664063e-3, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.apx2a, -1.5686591863632202, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei11t, 4.0592476725578308e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei22t, 0.17588233947753906, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.apx1t, -3.4139156341552734e-3, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.apx2t, 1.5673824548721313, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.thick, 0.12003209441900253, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.cambr, 1.9999433308839798e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryGeoparN23012Metrics) {
  const Result<Naca5Result> r5 = naca5(23012, kNside);
  const Naca5Result& n = r5.value();
  SplineProfile p;
  p.x = n.xb;
  p.y = n.yb;
  p.s.resize(n.xb.size());
  p.xp.resize(n.xb.size());
  p.yp.resize(n.xb.size());
  scalc(p.x, p.y, p.s);
  segspl(p.x, p.s, p.xp);
  segspl(p.y, p.s, p.yp);
  const GeometryMetrics m = geopar(p.x, p.xp, p.y, p.yp, p.s);
  // reference: geopar_reference.txt GEOPAR23012.
  CHECK_NEAR(m.sle, 1.0294321775436401, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.area, 8.2203090190887451e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.radle, 1.5813730657100677e-2, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.angte, 0.27864119410514832, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.ei11a, 6.8540684788826141e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei22a, 4.5390967279672623e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.apx1a, -1.7096281051635742e-2, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.apx2a, 1.5537002086639404, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei11t, 4.0081590414047241e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.ei22t, 0.17611472308635712, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.apx1t, -1.5810251235961914e-2, kLeAbsTol, kLeRelTol);
  CHECK_NEAR(m.apx2t, 1.5549860000610352, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.thick, 0.12003209441900253, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.cambr, 1.8381724134087563e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryAeCalcN0012Direct) {
  const Result<NacaResult> r = naca4(12, kNside);
  const NacaResult& n = r.value();
  const std::vector<double> t(n.xb.size(), 1.0);
  const SectionProperties a1 = aeCalc(n.xb, n.yb, t, 1);
  // reference: geopar_reference.txt AECALC0012 T1.
  CHECK_NEAR(a1.area, 8.2203105092048645e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a1.xcen, 0.42045703530311584, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a1.ei11, 6.8077351897954941e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a1.ei22, 4.5377984642982483e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a1.apx1, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a1.apx2, -1.5707963705062866, kFortranAbsTol, kFortranRelTol);
  const SectionProperties a2 = aeCalc(n.xb, n.yb, t, 2);
  // reference: geopar_reference.txt AECALC0012 T2.
  CHECK_NEAR(a2.area, 2.0417690277099610, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a2.xcen, 0.49356991052627563, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a2.ei11, 3.9852261543273926e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a2.ei22, 0.17558625340461731, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryAeCalcZeroAreaAndInvalidItype) {
  // AINT == 0 (degenerate line) -> null values, APX2 = pi/2.
  const std::vector<double> x = {0.0, 0.5, 1.0};
  const std::vector<double> y = {0.0, 0.0, 0.0};
  const std::vector<double> t(3, 1.0);
  const SectionProperties a = aeCalc(x, y, t, 1);
  CHECK_NEAR(a.area, 0.0, 0.0, 1.0);
  CHECK_NEAR(a.apx2, 1.5707963267948966, 1.0e-12, 1.0e-12);
  bool threw = false;
  try {
    aeCalc(x, y, t, 3);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, GeometryTccalcN0012Direct) {
  const SplineProfile p = makeProfile(12);
  const ThicknessCamberResult tc = tccalc(p.x, p.xp, p.y, p.yp, p.s);
  // reference: geopar_reference.txt TCCALC0012.
  CHECK_NEAR(tc.thick, 0.12003208696842194, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(tc.thickX, 0.29749578237533569, kFortranAbsTol, kFortranRelTol);
  // CAMBR is ~0 in the symmetric case (Fortran -6.05e-8).
  CHECK_NEAR(tc.cambr, 0.0, 1.0e-6, 1.0);
  // XCAMBR is a noise location in the symmetric case (Fortran 1.753e-2,
  // C++ 1.478e-2); comparable only at display tolerance.
  CHECK_NEAR(tc.cambrX, 1.7534343525767326e-2, 5.0e-3, 1.0);
}

TEST(unit, GeometryNormN2412) {
  const Result<NacaResult> r = naca4(2412, kNside);
  const NacaResult& n = r.value();
  std::vector<double> x = n.xb;
  std::vector<double> y = n.yb;
  std::vector<double> s(n.xb.size(), 0.0);
  std::vector<double> xp(n.xb.size(), 0.0);
  std::vector<double> yp(n.xb.size(), 0.0);
  norm(x, xp, y, yp, s);
  // reference: geopar_reference.txt NORM2412 (unit chord).
  CHECK_NEAR(x[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(s[0], 0.0);
  // reference: geopar_reference.txt NORM2412, node I=50 (1-based,
  // 0-based 49).
  CHECK_NEAR(x[49], 0.5170018076896667, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(y[49], 7.1075633168220520e-2, kFortranAbsTol, kFortranRelTol);
}

// ---
// YSYM/LERSCL/SSS/CANG/CLIS/ROTATE/TECALC
// ---

TEST(unit, GeometryYsymN2412Iside1) {
  const Result<NacaResult> r = naca4(2412, kNside);
  const NacaResult& n = r.value();
  // capacity 250 as in the Fortran-harness (XB(250)).
  constexpr std::size_t cap = 250;
  std::vector<double> x(cap, 0.0);
  std::vector<double> y(cap, 0.0);
  std::vector<double> s(cap, 0.0);
  std::vector<double> xp(cap, 0.0);
  std::vector<double> yp(cap, 0.0);
  for (std::size_t i = 0; i < kBufferCount; ++i) {
    x[i] = n.xb[i];
    y[i] = n.yb[i];
  }
  std::size_t nn = kBufferCount;
  scalc(std::span<const double>(x.data(), nn),
        std::span<const double>(y.data(), nn),
        std::span<double>(s.data(), nn));
  segspl(std::span<const double>(x.data(), nn),
         std::span<const double>(s.data(), nn),
         std::span<double>(xp.data(), nn));
  segspl(std::span<const double>(y.data(), nn),
         std::span<const double>(s.data(), nn),
         std::span<double>(yp.data(), nn));
  std::vector<double> xnew(500, 0.0);
  std::vector<double> ynew(500, 0.0);
  const Status st = ysym(x, xp, y, yp, s, nn, 1, xnew, ynew);
  CHECK(st.isOk());
  CHECK_EQ(nn, kBufferCount);
  // reference: transform_reference.txt YSYM2412 ISIDE=1 (symmetric
  // airfoil stays unchanged). The chord-axis back-transport deviates in
  // double by the DXC^2+DYC^2 rounding (Fortran single rounds to exactly 1.0).
  CHECK_NEAR(x[0], 1.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(x[1], 0.99815386533737183, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryYsymN2412Iside2) {
  const Result<NacaResult> r = naca4(2412, kNside);
  const NacaResult& n = r.value();
  // capacity 250 as in the Fortran-harness (XB(250)).
  constexpr std::size_t cap = 250;
  std::vector<double> x(cap, 0.0);
  std::vector<double> y(cap, 0.0);
  std::vector<double> s(cap, 0.0);
  std::vector<double> xp(cap, 0.0);
  std::vector<double> yp(cap, 0.0);
  for (std::size_t i = 0; i < kBufferCount; ++i) {
    x[i] = n.xb[i];
    y[i] = n.yb[i];
  }
  std::size_t nn = kBufferCount;
  scalc(std::span<const double>(x.data(), nn),
        std::span<const double>(y.data(), nn),
        std::span<double>(s.data(), nn));
  segspl(std::span<const double>(x.data(), nn),
         std::span<const double>(s.data(), nn),
         std::span<double>(xp.data(), nn));
  segspl(std::span<const double>(y.data(), nn),
         std::span<const double>(s.data(), nn),
         std::span<double>(yp.data(), nn));
  std::vector<double> xnew(500, 0.0);
  std::vector<double> ynew(500, 0.0);
  const Status st = ysym(x, xp, y, yp, s, nn, 2, xnew, ynew);
  CHECK(st.isOk());
  CHECK_EQ(nn, kBufferCount);
  // reference: transform_reference.txt YSYM2412 ISIDE=2. The chord-axis
  // back-transport deviates in double by the DXC^2+DYC^2 rounding.
  CHECK_NEAR(x[0], 1.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(y[1], 1.3958410127088428e-3, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryYsymOverflowNoPartialMutation) {
  // Small synthetic contour: capacity 10, ISIDE=1 returns
  // NNEW > NX (Fortran STOP 'YSYM: Array overflow on passed arrays.').
  const std::vector<double> x0 = {1.0, 0.8, 0.5, 0.2, 0.05,
                                  0.0, 0.05, 0.2, 0.5, 0.85};
  const std::vector<double> y0 = {0.004,  0.055,  0.075, 0.05,   0.018,
                                  0.0,    -0.016, -0.045, -0.07, -0.03};
  std::vector<double> x = x0;
  std::vector<double> y = y0;
  std::vector<double> s(10, 0.0);
  std::vector<double> xp(10, 0.0);
  std::vector<double> yp(10, 0.0);
  scalc(x, y, s);
  segspl(x, s, xp);
  segspl(y, s, yp);
  std::size_t nn = 10;
  std::vector<double> xnew(10, 0.0);
  std::vector<double> ynew(10, 0.0);
  const Status st = ysym(x, xp, y, yp, s, nn, 1, xnew, ynew);
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::capacityExceeded);
  // No partial mutation: contour unchanged.
  CHECK(x == x0);
  CHECK(y == y0);
  CHECK_EQ(nn, std::size_t{10});
}

TEST(unit, GeometryLersclN2412) {
  const SplineProfile p = makeProfile(2412);
  const AirfoilCoordinates lc = lerscl(p.x, p.xp, p.y, p.yp, p.s, 0.1, 2.0);
  CHECK_EQ(lc.x.size(), kBufferCount);
  CHECK_EQ(lc.y.size(), kBufferCount);
  // reference: transform_reference.txt LERSCL2412 DOC=0.1 RFAC=2.0.
  CHECK_NEAR(lc.x[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(lc.y[0], 1.2600315967574716e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(lc.y[1], 1.6416466096416116e-3, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometrySssN2412) {
  const SplineProfile p = makeProfile(2412);
  const double ss = p.s[19];
  // reference: transform_reference.txt SSS2412. The delta > 0.01 branch
  // reaches the iteration limit and returns S1=S2=SS.
  const BreakPointResult bp1 =
      sss(ss, 0.7, 0.0, 0.1, 1, p.x, p.xp, p.y, p.yp, p.s);
  CHECK(bp1.status == Diagnostic::limitReached);
  CHECK_NEAR(bp1.s1, ss, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(bp1.s2, ss, kFortranAbsTol, kFortranRelTol);
  const BreakPointResult bp2 =
      sss(ss, 0.7, 0.0, 0.0, 1, p.x, p.xp, p.y, p.yp, p.s);
  CHECK(bp2.status == Diagnostic::completed);
  // reference: SSS2412 DEL=0.0 ISIDE=1 S1 = S2 = 0.29749619960784912109
  CHECK_NEAR(bp2.s1, 0.29749619960784912, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(bp2.s2, bp2.s1, 0.0, 1.0);
}

TEST(unit, GeometryNormUsesChordVectorAndShiftsLeToOrigin) {
  // Rotated/translated ellipse. 6.996 normalizes by the LE-TE chord
  // vector, not by the x extents of the 6.99 NORM.
  constexpr std::size_t n = 41;
  constexpr double pi = 3.14159265358979323846;
  constexpr double angle = 20.0 * pi / 180.0;
  const double ca = std::cos(angle);
  const double sa = std::sin(angle);
  std::vector<double> x(n);
  std::vector<double> y(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = 2.0 * pi * static_cast<double>(i) /
                     static_cast<double>(n - 1);
    const double x0 = 0.5 + 0.5 * std::cos(t);
    const double y0 = 0.10 * std::sin(t);
    x[i] = 3.0 + x0 * ca - y0 * sa;
    y[i] = -2.0 + x0 * sa + y0 * ca;
  }
  std::vector<double> xp(n, 0.0);
  std::vector<double> yp(n, 0.0);
  std::vector<double> s(n, 0.0);
  scalc(x, y, s);
  segspl(x, s, xp);
  segspl(y, s, yp);
  const LeadingEdgeResult le = lefind(x, xp, y, yp, s);
  CHECK(le.status == LeStatus::completed);
  CHECK_NEAR(le.sle, 1.049423950339961, kFortranAbsTol, kFortranRelTol);

  norm(x, xp, y, yp, s);
  // reference: external H-S03 harness, 6.996 NORM.
  CHECK_NEAR(x[0], 9.396926207859084e-1, 1.0e-12, 0.0);
  CHECK_NEAR(y[0], 3.420201433256688e-1, 1.0e-12, 0.0);
  CHECK_NEAR(x[20], 0.0, 1.0e-12, 0.0);
  CHECK_NEAR(y[20], 0.0, 1.0e-12, 0.0);
  CHECK_NEAR(s[20], 1.049423950339961, 1.0e-12, 0.0);

  const GeometryMetrics metrics = geopar(x, xp, y, yp, s);
  CHECK_NEAR(metrics.chord, 1.0, 1.0e-12, 0.0);
  CHECK_NEAR(metrics.area, 1.564344650402310e-1, 1.0e-6, 1.0e-5);
}

TEST(unit, GeometryLefindSmallNGuard) {
  // 6.996 IF(N.LT.5) I=N/2; N=4 uses the discrete midpoint index.
  constexpr std::size_t n = 4;
  constexpr double pi = 3.14159265358979323846;
  std::vector<double> x(n);
  std::vector<double> y(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = 2.0 * pi * static_cast<double>(i) /
                     static_cast<double>(n - 1);
    x[i] = 0.5 + 0.5 * std::cos(t);
    y[i] = 0.10 * std::sin(t);
  }
  std::vector<double> xp(n, 0.0);
  std::vector<double> yp(n, 0.0);
  std::vector<double> s(n, 0.0);
  scalc(x, y, s);
  segspl(x, s, xp);
  segspl(y, s, yp);
  const LeadingEdgeResult le = lefind(x, xp, y, yp, s);
  CHECK(le.status == LeStatus::completed);
  CHECK_NEAR(le.sle, 8.415859839056992e-1, 1.0e-12, 0.0);
}

TEST(unit, GeometryXlfindZeroSecondDerivativeFallsBack) {
  const std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
  const std::vector<double> y(6, 0.0);
  std::vector<double> xp(6, 0.0);
  std::vector<double> yp(6, 0.0);
  std::vector<double> s(6, 0.0);
  scalc(x, y, s);
  segspl(x, s, xp);
  segspl(y, s, yp);
  const LeadingEdgeResult xl = xlfind(x, xp, y, yp, s);
  CHECK(xl.status == LeStatus::limitReached);
  CHECK_NEAR(xl.sle, s[2], 0.0, 0.0);
}

TEST(unit, GeometryCangUsesAtan2Quadrant) {
  // 6.996 CANG uses ATAN2(CRS,DOT), preserving angles beyond +-90 deg.
  const std::vector<double> x = {0.0, 1.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 0.0, 1.0, 1.0};
  const CornerAngleReport r = cang(x, y);
  CHECK_EQ(r.maxIndex, std::size_t{1});
  CHECK_NEAR(r.maxAngle, 135.0000023028518, 1.0e-9, 0.0);
}

TEST(unit, GeometryCangN0012) {
  const Result<NacaResult> r = naca4(12, kNside);
  const NacaResult& n = r.value();
  const CornerAngleReport ca = cang(n.xb, n.yb);
  CHECK_EQ(ca.angles.size(), kBufferCount - 2);
  // reference: transform_reference.txt CANG0012 IPRINT=0 AMAX = 7.23364639
  // (Fortran IMAX = 123 1-based -> 0-based 122).
  CHECK_EQ(ca.maxIndex, std::size_t{122});
  CHECK_NEAR(ca.maxAngle, 7.2336463928222656, kAngleAbsTol, kAngleRelTol);
}

TEST(unit, GeometryClisN0012) {
  const SplineProfile p = makeProfile(12);
  const CurvatureReport cr = clis(p.x, p.xp, p.y, p.yp, p.s);
  CHECK_EQ(cr.rows.size(), kBufferCount);
  // reference: transform_reference.txt CLIS0012 (maximum curvature =
  // 63.654). For the symmetric 0012 airfoil the maximum index is a
  // degenerate tie pair (0-based 121/123, Fortran 1-based
  // 122); only the value is compared.
  CHECK_NEAR(cr.maxCurvature, 63.654, kDisplayAbsTol, kFortranRelTol);
  CHECK(cr.maxIndex == 121 || cr.maxIndex == 123);
}

TEST(unit, GeometryRotateN2412AndBack) {
  const Result<NacaResult> r = naca4(2412, kNside);
  const NacaResult& n = r.value();
  std::vector<double> x = n.xb;
  std::vector<double> y = n.yb;
  std::vector<double> xref = x;
  std::vector<double> yref = y;
  rotate(x, y, 0.25);
  // reference: xfoil_geom_reference.txt ROT+ X(1) = 0.96922415494918823
  CHECK_NEAR(x[0], 0.96922415494918823, kFortranAbsTol, kFortranRelTol);
  rotate(x, y, -0.25);
  // back-rotation returns the initial contour (apart from float residue).
  CHECK_NEAR(x[0], xref[0], 1.0e-14, 1.0e-14);
  CHECK_NEAR(y[0], yref[0], 1.0e-14, 1.0e-14);
  CHECK_NEAR(x[122], xref[122], 1.0e-14, 1.0e-14);
  CHECK_NEAR(y[244], yref[244], 1.0e-14, 1.0e-14);
}

TEST(unit, GeometryRotateZeroAngleIdentity) {
  const Result<NacaResult> r = naca4(2412, kNside);
  const NacaResult& n = r.value();
  std::vector<double> x = n.xb;
  std::vector<double> y = n.yb;
  rotate(x, y, 0.0);
  for (std::size_t i = 0; i < x.size(); ++i) {
    CHECK_EQ(x[i], n.xb[i]);
    CHECK_EQ(y[i], n.yb[i]);
  }
}

TEST(unit, GeometryTecalcOpenTe) {
  TrailingEdgeInputs in;
  in.x0 = 1.0;
  in.y0 = 0.0;
  in.xp0 = -1.0;
  in.yp0 = 0.0;
  in.xn = 0.95;
  in.yn = -0.02;
  in.xpn = 1.0;
  in.ypn = 0.5;
  in.gam0 = 0.2;
  in.gamn = 0.3;
  in.gama0 = 0.5;
  in.gaman = 0.4;
  in.chord = 1.0;
  const TecalcResult te = tecalc(in);
  CHECK(!te.sharp);
  // reference: xfoil_geom_reference.txt TECALC open.
  CHECK_NEAR(te.dste, 5.3851660341024399e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(te.ante, 7.4999965727329254e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(te.aste, 5.5000010877847672e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(te.sigte, -6.9635710678994656e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(te.gamte, 5.1066219806671143e-2, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(te.sigteA, 6.9635701365768909e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(te.gamteA, -5.1066212356090546e-2, kFortranAbsTol,
             kFortranRelTol);
}

TEST(unit, GeometryTecalcSharpTe) {
  TrailingEdgeInputs in;
  in.x0 = 1.0;
  in.y0 = 0.0;
  in.xp0 = -1.0;
  in.yp0 = 0.0;
  in.xn = 1.0;
  in.yn = 0.0;
  in.xpn = -1.0;
  in.ypn = 0.0;
  in.gam0 = 0.2;
  in.gamn = 0.3;
  in.gama0 = 0.5;
  in.gaman = 0.4;
  in.chord = 1.0;
  const TecalcResult te = tecalc(in);
  CHECK(te.sharp);
  // reference: xfoil_geom_reference.txt TECALC sharp.
  CHECK_NEAR(te.dste, 0.0, 0.0, 1.0);
  CHECK_NEAR(te.ante, 0.0, 0.0, 1.0);
  CHECK_NEAR(te.aste, 0.0, 0.0, 1.0);
  CHECK_NEAR(te.scs, 1.0, 0.0, 1.0);
  CHECK_NEAR(te.sds, 0.0, 0.0, 1.0);
  CHECK_NEAR(te.sigte, -5.0000044703483582e-2, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(te.gamte, 0.0, 0.0, 1.0);
}

// ---
// INTER/INTERX/IJSECT/AREFINE/SCHECK/HALF
// ---

TEST(unit, GeometryInterN0012N2412) {
  const SplineProfile p0 = makeProfile(12);
  const SplineProfile p1 = makeProfile(2412);
  const AirfoilCoordinates iv = inter(p0, p1, 0.5);
  CHECK_EQ(iv.x.size(), kBufferCount);
  CHECK_EQ(iv.y.size(), kBufferCount);
  // reference: interp_reference.txt INTER0012-2412 FRAC=0.5.
  CHECK_NEAR(iv.x[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv.x[1], 0.99815773963928223, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv.y[1], 1.5793793136253953e-3, kFortranAbsTol, kFortranRelTol);
  const AirfoilCoordinates iv25 = inter(p0, p1, 0.25);
  // reference: interp_reference.txt INTER0012-2412 FRAC=0.25.
  CHECK_NEAR(iv25.x[1], 0.99815577268600464, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv25.y[1], 1.5490542864426970e-3, kFortranAbsTol,
             kFortranRelTol);
}

TEST(unit, GeometryInterXN0012N2412) {
  const SplineProfile p0 = makeProfile(12);
  const SplineProfile p1 = makeProfile(2412);
  const AirfoilCoordinates iv = interX(p0, p1, 0.5);
  CHECK_EQ(iv.x.size(), kBufferCount);
  CHECK_EQ(iv.y.size(), kBufferCount);
  // reference: interp_reference.txt INTERX0012-2412 FRAC=0.5.
  CHECK_NEAR(iv.x[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv.x[1], 0.99815386533737183, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(iv.y[1], 1.5801771078258753e-3, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryInterTooFewPointsThrows) {
  SplineProfile p0;
  p0.x = {0.0};
  p0.y = {0.0};
  p0.s = {0.0};
  p0.xp = {0.0};
  p0.yp = {0.0};
  SplineProfile p1 = p0;
  bool threw = false;
  try {
    inter(p0, p1, 0.5);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
  threw = false;
  try {
    interX(p0, p1, 0.5);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, GeometryIjsectN0012) {
  const Result<NacaResult> r = naca4(12, kNside);
  const NacaResult& n = r.value();
  const SectionBending ij = ijsect(n.xb, n.yb);
  // reference: interp_reference.txt IJSECT0012.
  CHECK_NEAR(ij.area, 8.2203105092048645e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.slen, 2.0417687892913818, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.xmin, 0.0, 0.0, 1.0);
  CHECK_NEAR(ij.xmax, 1.0, 0.0, 1.0);
  CHECK_NEAR(ij.ymin, -6.0016043484210968e-2, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(ij.ymax, 6.0016043484210968e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.xc, 0.42044183611869812, kFortranAbsTol, kFortranRelTol);
  // YC is ~0 in the symmetric case (Fortran 3.42e-10).
  CHECK_NEAR(ij.yc, 0.0, 1.0e-6, 1.0);
  CHECK_NEAR(ij.xct, 0.49356991052627563, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.yct, 0.0, 1.0e-6, 1.0);
  CHECK_NEAR(ij.aixx, 6.8077191826887429e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.aiyy, 4.5373877510428430e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.aixxt, 3.9852312766015530e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(ij.aiyyt, 0.17558631300926208, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.aj, 2.7230879641138017e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ij.ajt, 1.3238228857517242e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryArefineN2412S61) {
  const Result<NacaResult> r = naca4(2412, 61);
  const NacaResult& n = r.value();
  std::vector<double> s(n.xb.size());
  std::vector<double> xp(n.xb.size());
  std::vector<double> yp(n.xb.size());
  scalc(n.xb, n.yb, s);
  segspl(n.xb, s, xp);
  segspl(n.yb, s, yp);
  const RefineResult ar =
      arefine(n.xb, n.yb, s, xp, yp, 5.0, 500, -1.0, 2.0);
  CHECK(ar.status == Diagnostic::completed);
  // reference: interp_reference.txt AREFINE2412-61 ATOL=5 NNEW = 145.
  CHECK_EQ(ar.nnew, std::size_t{145});
  CHECK_EQ(ar.x.size(), std::size_t{145});
  CHECK_EQ(ar.y.size(), std::size_t{145});
  CHECK_NEAR(ar.x[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ar.y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ar.x[1], 0.99467462301254272, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(ar.y[1], 2.3582079447805882e-3, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryArefineOverflowNoPartialMutation) {
  const Result<NacaResult> r = naca4(2412, 61);
  const NacaResult& n = r.value();
  std::vector<double> s(n.xb.size());
  std::vector<double> xp(n.xb.size());
  std::vector<double> yp(n.xb.size());
  scalc(n.xb, n.yb, s);
  segspl(n.xb, s, xp);
  segspl(n.yb, s, yp);
  const RefineResult ar = arefine(n.xb, n.yb, s, xp, yp, 5.0, 10, -1.0, 2.0);
  // reference: interp_reference.txt AREFINE overflow NDIM=10 NNEW = 0.
  CHECK(ar.status == Diagnostic::limitReached);
  CHECK_EQ(ar.nnew, std::size_t{0});
  CHECK(ar.x.empty());
  CHECK(ar.y.empty());
}

TEST(unit, GeometryScheckMicroSegment) {
  std::vector<double> x = {0.0, 0.4, 0.401, 0.8, 1.0, 0.8, 0.4, 0.0};
  std::vector<double> y = {0.0, 0.05, 0.055, 0.02, 0.0, -0.02, -0.04, -0.03};
  std::size_t n = 8;
  const ScheckResult sc = scheck(x, y, n, 0.05);
  CHECK(sc.changed);
  CHECK(!sc.badStol);
  CHECK_EQ(sc.removedIndices.size(), std::size_t{1});
  // reference: interp_reference.txt SCHECK (Fortran 'segment removed at 2'
  // -> 0-based 1); N = 7.
  CHECK_EQ(sc.removedIndices[0], std::size_t{1});
  CHECK_EQ(n, std::size_t{7});
  CHECK_NEAR(x[1], 0.40049999952316284, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(y[1], 5.2500002086162567e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(x[6], 0.0, 0.0, 1.0);
  CHECK_NEAR(y[6], -2.9999993294477463e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryScheckBadStolNoMutation) {
  std::vector<double> x = {0.0, 0.4, 0.401, 0.8, 1.0, 0.8, 0.4, 0.0};
  std::vector<double> y = {0.0, 0.05, 0.055, 0.02, 0.0, -0.02, -0.04, -0.03};
  std::vector<double> xref = x;
  std::vector<double> yref = y;
  std::size_t n = 8;
  const ScheckResult sc = scheck(x, y, n, 0.5);
  // reference: SCHECK bad STOL -> no change.
  CHECK(!sc.changed);
  CHECK(sc.badStol);
  CHECK_EQ(n, std::size_t{8});
  CHECK(x == xref);
  CHECK(y == yref);
}

TEST(unit, GeometryHalfN2412) {
  const Result<NacaResult> r = naca4(2412, kNside);
  const NacaResult& n = r.value();
  std::vector<double> x = n.xb;
  std::vector<double> y = n.yb;
  std::vector<double> s(n.xb.size(), 0.0);
  scalc(x, y, s);
  std::size_t nn = kBufferCount;
  half(s, x, y, nn);
  // reference: interp_reference.txt HALF2412 NNEW = 123.
  CHECK_EQ(nn, std::size_t{123});
  CHECK_NEAR(x[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(y[0], 1.2600078480318189e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(x[1], 0.99480420351028442, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(y[1], 2.3315800353884697e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(x[122], n.xb[244], 1.0e-12, 1.0e-12);
  CHECK_NEAR(y[122], n.yb[244], kFortranAbsTol, kFortranRelTol);
}

// ---
// NACA core operation (nacaAirfoil)
// ---

TEST(unit, GeometryNacaAirfoilN0012) {
  const Result<NacaAirfoilResult> r = nacaAirfoil(12);
  CHECK(r.isOk());
  const NacaAirfoilResult& out = r.value();
  CHECK_EQ(out.buffer.x.size(), kBufferCount);
  CHECK(out.buffer.name == "NACA 0012");
  // geometry parameter as in the xfoil.f-NACA-call context (SCALC/SEGSPL/
  // GEOPAR).
  CHECK_NEAR(out.metrics.sle, 1.0196241140365601, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(out.metrics.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(out.metrics.thick, 0.12003208696842194, kFortranAbsTol,
             kFortranRelTol);
}

TEST(unit, GeometryNacaAirfoilN23012) {
  const Result<NacaAirfoilResult> r = nacaAirfoil(23012);
  CHECK(r.isOk());
  const NacaAirfoilResult& out = r.value();
  CHECK_EQ(out.buffer.x.size(), kBufferCount);
  CHECK(out.buffer.name == "NACA 23012");
  CHECK_NEAR(out.metrics.cambr, 1.8381724134087563e-2, kFortranAbsTol,
             kFortranRelTol);
}

TEST(unit, GeometryNacaAirfoilInvalidDesignations) {
  // ITYPE==0 (IDES > 25099) and IDES <= 0 (Fortran dialog).
  CHECK(nacaAirfoil(99999).isErr());
  CHECK(nacaAirfoil(0).isErr());
  CHECK(nacaAirfoil(-1).isErr());
  // Illegal NACA5 identifier (ITYPE=5, N543=219) propagates.
  CHECK(nacaAirfoil(21999).isErr());
}

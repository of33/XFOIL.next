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

// File: tests/core/GeometryDesignTests.cpp
// Unit tests for the new 0-based core: geometry-design kernels
// (GETCAM/GETMAX, later CAMB/GDES subpaths).
//
// the tests check both smooth and noise-dominated (symmetric 0012) outputs.
//
// the tests use only xfoil::core and no app, CLI, or 1-based code.
//
// reference comparison: the expected values come from the focused Fortran
// harness tests/fortran-reference/harnesses/geometry-design/
// camber_thickness_harness.f (reference data: tests/data/
// core-geometry-design-reference/camber_thickness_reference.txt), which
// calls the canonical routines unchanged. Fortran REAL is single
// precision; the new core computes in double. Formula and operation order
// are identical; the deviations lie in the single-to-double range.
//
// Tolerance notes:
// - kFortranRelTol/kFortranAbsTol: base for all smooth values.
// - kNoiseAbsTol (1e-5): GETMAX of the camber of a symmetric airfoil
// (0012) returns noise values (camber ~ 0); comparable only in absolute terms.

#include "TestSupport.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry_design/CamberThickness.h"
#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"
#include "xfoil/core/geometry_design/CamberSession.h"
#include "xfoil/core/geometry_design/GeometryDesignOps.h"
#include "xfoil/core/geometry_design/PlotFrame.h"
#include "xfoil/core/geometry_design/ThicknessCamberOps.h"
#include "xfoil/core/paneling/Abcopy.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/math/Spline.h"

namespace {

using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::geometry_design;
using namespace xfoil::core::math;

constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;
constexpr double kNoiseAbsTol = 1.0e-5;

constexpr std::size_t kNside = 123;

// Builds the splined contour as in the harnesses: SCALC + SEGSPL x2.
SplineProfile splinedNaca4(int ides) {
  const Result<NacaResult> r = naca4(ides, kNside);
  if (r.isErr()) {
    throw std::runtime_error("naca4 failed");
  }
  SplineProfile p;
  p.x = r.value().xb;
  p.y = r.value().yb;
  p.s.resize(p.x.size());
  p.xp.resize(p.x.size());
  p.yp.resize(p.x.size());
  scalc(p.x, p.y, p.s);
  segspl(p.x, p.s, p.xp);
  segspl(p.y, p.s, p.yp);
  return p;
}

// Builds the design buffer as the harnesses do: SCALC + SEGSPL x2 + GEOPAR.
GeometryDesignBuffer designBuffer(int ides) {
  SplineProfile p = splinedNaca4(ides);
  GeometryDesignBuffer b(std::move(p.x), std::move(p.y), std::move(p.s),
                         std::move(p.xp), std::move(p.yp));
  b.metrics = geopar(b.x, b.xp, b.y, b.yp, b.s);
  return b;
}

}  // namespace

TEST(unit, GeometryDesignGetCam0012CountsAndEndpoints) {
  const SplineProfile p = splinedNaca4(12);
  const ThicknessCamberDistribution d =
      getCamberThickness(p.x, p.xp, p.y, p.yp, p.s);
  // reference: camber_thickness_reference.txt GETCAM0012.
  CHECK_EQ(d.ncm, 123u);
  CHECK_EQ(d.ntk, 123u);
  CHECK_EQ(d.xcm.size(), d.ncm);
  CHECK_EQ(d.ytk.size(), d.ntk);
  CHECK_NEAR(d.xcm[0], 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(d.ycm[0], 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(d.xcm[1], 1.255870e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.ycm[1], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.xtk[1], 1.255870e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.ytk[1], 1.986839e-3, kFortranAbsTol, kFortranRelTol);
  // Re-origination: camber starts at Y=0 (Fortran YCM(I) -= YCM(1)).
  CHECK_NEAR(d.ycm[0], 0.0, 1.0e-12, 1.0e-12);
  // Blunt TE: thickness at the last point = TE thickness (reference TK 123).
  CHECK_NEAR(d.xtk[d.ntk - 1], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.ytk[d.ntk - 1], 0.1260008e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignGetCam2412CountsAndEndpoints) {
  const SplineProfile p = splinedNaca4(2412);
  const ThicknessCamberDistribution d =
      getCamberThickness(p.x, p.xp, p.y, p.yp, p.s);
  // reference: camber_thickness_reference.txt GETCAM2412.
  CHECK_EQ(d.ncm, 123u);
  CHECK_EQ(d.ntk, 124u);
  CHECK_NEAR(d.xcm[1], 1.255276e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.ycm[1], 1.208333e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.xtk[1], 1.255276e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.ytk[1], 1.986365e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d.ycm[0], 0.0, 1.0e-12, 1.0e-12);
}

TEST(unit, GeometryDesignGetMaxThickness0012) {
  const SplineProfile p = splinedNaca4(12);
  const ThicknessCamberDistribution d =
      getCamberThickness(p.x, p.xp, p.y, p.yp, p.s);
  std::vector<double> ytkp(d.ntk, 0.0);
  const MaxPointResult m = getMaxPoint(d.xtk, d.ytk, ytkp);
  CHECK(m.status == math::Diagnostic::completed);
  // reference: GETMAX0012 THK.
  CHECK_NEAR(m.max.x, 0.2998269e0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.max.y, 0.6001728e-1, kFortranAbsTol, kFortranRelTol);
  // SEGSPL side effect: yp is written as a spline coefficient.
  bool ypWritten = false;
  for (std::size_t i = 0; i < ytkp.size(); ++i) {
    if (ytkp[i] != 0.0) {
      ypWritten = true;
      break;
    }
  }
  CHECK(ypWritten);
}

TEST(unit, GeometryDesignGetMaxCamber2412) {
  const SplineProfile p = splinedNaca4(2412);
  const ThicknessCamberDistribution d =
      getCamberThickness(p.x, p.xp, p.y, p.yp, p.s);
  std::vector<double> ycmp(d.ncm, 0.0);
  const MaxPointResult m = getMaxPoint(d.xcm, d.ycm, ycmp);
  CHECK(m.status == math::Diagnostic::completed);
  // reference: GETMAX2412 CAM.
  CHECK_NEAR(m.max.x, 0.4009294e0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(m.max.y, 0.1999980e-1, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignGetMaxCamber0012Noise) {
  const SplineProfile p = splinedNaca4(12);
  const ThicknessCamberDistribution d =
      getCamberThickness(p.x, p.xp, p.y, p.yp, p.s);
  std::vector<double> ycmp(d.ncm, 0.0);
  const MaxPointResult m = getMaxPoint(d.xcm, d.ycm, ycmp);
  CHECK(m.status == math::Diagnostic::completed);
  // symmetric airfoil: the camber maximum is a noise value; only
  // absolutely comparable (reference: GETMAX0012 CAM YMAX = -4.53e-8).
  CHECK_NEAR(m.max.y, -0.4533207e-7, kNoiseAbsTol, 0.0);
}

TEST(unit, GeometryDesignGetCamRejectsTooFewPoints) {
  const SplineProfile p = splinedNaca4(12);
  bool threw = false;
  try {
    getCamberThickness(p.x, p.xp, p.y, p.yp,
                       std::span<const double>(p.s.data(), 2));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, GeometryDesignGetMaxRejectsWorkspaceMismatch) {
  const SplineProfile p = splinedNaca4(12);
  const ThicknessCamberDistribution d =
      getCamberThickness(p.x, p.xp, p.y, p.yp, p.s);
  std::vector<double> ytkp(d.ntk - 1, 0.0);
  bool threw = false;
  try {
    getMaxPoint(d.xtk, d.ytk, ytkp);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, GeometryDesignThkcam2412) {
  GeometryDesignBuffer b = designBuffer(2412);
  const Result<std::size_t> st = transformThicknessCamber(b, 0.5, 2.0);
  CHECK(st.isOk());
  CHECK_EQ(b.size(), 245u);
  CHECK_FALSE(b.lgSame);
  // reference: thkcam_reference.txt THK.
  CHECK_NEAR(b.metrics.sle, 0.1016988e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.chord, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.6001613e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.3998502e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.6300039e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.6924869e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.6300039e-3, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignZerCamber2412) {
  // harness sequence: THKCAM(0.5,2.0) -> TCSET(0.15,0.03) -> ZERCAM.
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(transformThicknessCamber(b, 0.5, 2.0).isOk());
  CHECK(setThicknessCamberTargets(b, 0.15, 0.03, false).isOk());
  const Result<ZerCamberResult> st = zerCamber(b);
  CHECK(st.isOk());
  CHECK_EQ(b.size(), 245u);
  // reference: thkcam_reference.txt ZERC.
  CHECK_NEAR(b.metrics.thick, 0.1500377e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.3352761e-7, 1.0e-5, 0.0);
  CHECK_NEAR(b.y[0], 0.1561905e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.7331472e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[122], -0.2903966e-4, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1587205e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignSetTargets2412) {
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(transformThicknessCamber(b, 0.5, 2.0).isOk());
  const Result<SetTargetsResult> r =
      setThicknessCamberTargets(b, 0.15, 0.03, false);
  CHECK(r.isOk());
  const SetTargetsResult& res = r.value();
  CHECK_FALSE(res.abortedQuestionable);
  CHECK(res.after.has_value());
  // reference: thkcam_reference.txt TCSE.
  CHECK_NEAR(res.factors.tfac, 2.499275e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(res.factors.cfac, 7.504400e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1499969e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.3001270e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1568773e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1580337e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignSetTargetsQuestionableAborts) {
  // symmetric airfoil: camber maximum is noise -> CFAC enormous;
  // without confirmation the buffer must remain unchanged.
  GeometryDesignBuffer b = designBuffer(12);
  const std::vector<double> before = b.y;
  const Result<SetTargetsResult> r =
      setThicknessCamberTargets(b, 0.15, 0.03, false);
  CHECK(r.isOk());
  CHECK(r.value().abortedQuestionable);
  CHECK_FALSE(r.value().after.has_value());
  CHECK(b.y == before);
}

TEST(unit, GeometryDesignScaleSymmetric2412AfterZerCamber) {
  // harness sequence: THKCAM -> TCSET -> ZERCAM -> TCSCAL(1.5,0.5).
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(transformThicknessCamber(b, 0.5, 2.0).isOk());
  CHECK(setThicknessCamberTargets(b, 0.15, 0.03, false).isOk());
  CHECK(zerCamber(b).isOk());
  const Result<ScaleResult> r = scaleThicknessCamber(b, 1.5, 0.5);
  CHECK(r.isOk());
  // reference: thkcam_reference.txt TCSC.
  CHECK_NEAR(b.metrics.thick, 0.2250566e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.2363702e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.2359963e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignReportSymmetricCamberNewtonFallback) {
  // reference thkcam_reference.txt ZERD: thickness maximum exact; the
  // camber maximum of the symmetric buffer is a noise value
  // (CYMAX = 1.60E-5). Fortran-specific Newton fallbacks on this
  // noise are precision-dependent and are not compared.
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(transformThicknessCamber(b, 0.5, 2.0).isOk());
  CHECK(setThicknessCamberTargets(b, 0.15, 0.03, false).isOk());
  CHECK(zerCamber(b).isOk());
  const ThicknessCamberReport rep = reportThicknessCamber(b);
  CHECK_NEAR(rep.thickMax.y, 0.7502034e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(rep.thickMax.x, 0.2997764e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(rep.camberMax.y, 0.0, 1.0e-4, 0.0);
}

TEST(unit, GeometryDesignCpcamSineLoading) {
  // reference: camber_thickness_reference.txt CPCAM (P = sin(pi x), 51 points).
  const double pi = std::acos(-1.0);
  std::vector<double> x(51), p(51), dpdx(51);
  for (std::size_t i = 0; i < 51; ++i) {
    x[i] = static_cast<double>(i) / 50.0;
    p[i] = std::sin(pi * x[i]);
    dpdx[i] = pi * std::cos(pi * x[i]);
  }
  const CamberLoadingResult r = camberFromLoading(x, p, dpdx);
  CHECK_NEAR(r.y[0], 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(r.y[12], 0.4123792e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.y[25], 0.6273874e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.y[37], 0.4421122e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.y[50], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.dydx[25], -0.2235174e-7, kNoiseAbsTol, 0.0);
}

TEST(unit, GeometryDesignCpcamConstantLoading) {
  // reference: camber_thickness_reference.txt CPCAMC (P = 1, 41 points).
  std::vector<double> x(41), p(41, 1.0), dpdx(41, 0.0);
  for (std::size_t i = 0; i < 41; ++i) {
    x[i] = static_cast<double>(i) / 40.0;
  }
  const CamberLoadingResult r = camberFromLoading(x, p, dpdx);
  CHECK_NEAR(r.y[0], 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(r.y[10], 0.4474921e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.y[20], 0.5515889e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.y[30], 0.4474920e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.y[40], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.dydx[20], 0.0, kNoiseAbsTol, 0.0);
}

TEST(unit, GeometryDesignHipnt2412MoveHighPoints) {
  // reference: hipnt_reference.txt HIP1 (HIPNT(0.4, 0.35)).
  GeometryDesignBuffer b = designBuffer(2412);
  const HighPointResult r = moveHighPoints(b, 0.4, 0.35);
  CHECK_NEAR(r.chordlineAngleDeg, 0.0, 1.0e-3, 0.0);
  CHECK_EQ(b.size(), 245u);
  CHECK_FALSE(b.lgSame);
  CHECK_NEAR(b.metrics.sle, 0.1025227e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1200307e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.1998769e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1260008e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.7989395e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1260008e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignHipnt2412KeepHighPoints) {
  // reference: hipnt_reference.txt HIP2 (HIPNT(0.0, 0.0) after HIP1).
  GeometryDesignBuffer b = designBuffer(2412);
  moveHighPoints(b, 0.4, 0.35);
  const HighPointResult r = moveHighPoints(b, 0.0, 0.0);
  CHECK_NEAR(r.thickMaxBefore.x, 0.4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.camberMaxBefore.x, 0.350357e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.sle, 0.1025210e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1200306e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.1997731e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[122], 0.1801731e-4, 1.0e-6, kFortranRelTol);
}

TEST(unit, GeometryDesignCamberSessionInit2412) {
  // reference: camb_session_reference.txt INI.
  GeometryDesignBuffer b = designBuffer(2412);
  const CamberSession s = initCamberSession(b);
  CHECK_EQ(s.ncam, 201u);
  CHECK_EQ(s.ncadd, 2u);
  CHECK_EQ(s.npadd, 2u);
  CHECK_NEAR(s.sbl, 0.1026789e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.xbch, 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.sbch, 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.xcam[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.xcam[100], 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.xcam[200], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[100], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.pcam[100], 0.0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignCamberSessionSetc2412) {
  // reference: camb_session_reference.txt SET.
  GeometryDesignBuffer b = designBuffer(2412);
  CamberSession s = initCamberSession(b);
  setAddedCamberFromCamberline(s, b);
  CHECK_EQ(s.ncadd, 123u);
  CHECK_NEAR(s.xcadd[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycadd[122], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[50], 0.1718750e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[100], 0.1944445e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[150], 0.1319444e-1, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignCamberSessionInpc2412) {
  // reference: camb_session_reference.txt CLR + NPC.
  GeometryDesignBuffer b = designBuffer(2412);
  CamberSession s = initCamberSession(b);
  setAddedCamberFromCamberline(s, b);
  s = clearAddedCamber(b);
  CHECK_EQ(s.ncadd, 2u);
  CHECK_NEAR(s.ycam[100], 0.0, kFortranAbsTol, kFortranRelTol);
  const CamberInputPoint pts[5] = {{0.0, 0.0}, {0.2, 0.02}, {0.5, 0.05},
                                   {0.8, 0.02}, {1.0, 0.0}};
  inputAddedCamberPoints(s, pts);
  CHECK_EQ(s.ncadd, 5u);
  CHECK_NEAR(s.xcadd[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycadd[2], 0.5000000e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[50], 0.2668651e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[100], 0.5000000e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[150], 0.2668651e-1, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignCamberSessionInppScalAdd2412) {
  // reference: camb_session_reference.txt NPP + SCL + ADD.
  GeometryDesignBuffer b = designBuffer(2412);
  CamberSession s = initCamberSession(b);
  const CamberInputPoint pts[5] = {{0.0, 0.0}, {0.2, 0.02}, {0.5, 0.05},
                                   {0.8, 0.02}, {1.0, 0.0}};
  inputAddedCamberPoints(s, pts);
  const CamberInputPoint lpts[3] = {{0.1, 1.0}, {0.5, -0.5}, {0.9, 0.0}};
  const LoadingIntegrals npp = inputAddedLoadingPoints(s, lpts);
  CHECK_EQ(s.npadd, 5u);
  CHECK_NEAR(s.ycam[50], 0.2586272e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[100], -0.2154706e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.ycam[150], -0.2192570e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(npp.clx, 0.1152447e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(npp.cmx, 0.8751953e-1, kFortranAbsTol, kFortranRelTol);
  const LoadingIntegrals scl = scaleAddedCamber(s, 2.0);
  CHECK_NEAR(s.ycam[100], -0.4309412e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(s.pcam[100], -0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(scl.clx, 0.2304894e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(scl.cmx, 0.1750391e+0, kFortranAbsTol, kFortranRelTol);
  const auto st = applyAddedCamber(s, b);
  CHECK(st.isOk());
  CHECK_EQ(st.value().oppositePointFailures, 0u);
  CHECK_NEAR(b.metrics.sle, 0.1074834e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1200321e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.7472526e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1260008e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.8109291e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[122], 0.0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1260008e-2, kFortranAbsTol, kFortranRelTol);
  // ADD sets LCLEAR=.TRUE.: the session is re-initialized.
  CHECK_EQ(s.ncadd, 2u);
  CHECK_NEAR(s.ycam[100], 0.0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignGdesSetBuffer2412) {
  // reference: gdes_ops_reference.txt GSE.
  const SplineProfile p = splinedNaca4(2412);
  GeometryDesignBuffer b;
  CHECK(setBufferFromAirfoil(b, p.x, p.y).isOk());
  CHECK(b.lgSame);
  CHECK_EQ(b.size(), 245u);
  CHECK_NEAR(b.metrics.sle, 0.1026789e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1200321e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.1999943e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1260008e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.7862878e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1260008e-2, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignGdesExecuteBuffer2412) {
 // EXEC carries the buffer through ABCOPY (revalidated here).
  const GeometryDesignBuffer b = designBuffer(2412);
  const Result<paneling::AbcopyResult> r = executeGeometryBuffer(b);
  CHECK(r.isOk());
}

TEST(unit, GeometryDesignGdesOps2412Sequence) {
  // reference: gdes_ops_reference.txt TRA..SYM (Operations sequence).
  const double pi = std::acos(-1.0);
  GeometryDesignBuffer b = designBuffer(2412);
  translateBuffer(b, 0.1, 0.05);
  CHECK_FALSE(b.lgSame);
  CHECK_NEAR(b.x[0], 0.1100000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.5126001e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], 0.4873999e-1, kFortranAbsTol, kFortranRelTol);
  scaleBuffer(b, 2.0, 2.0);
  CHECK_NEAR(b.metrics.chord, 0.2000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.2400642e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1025200e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], 0.9747998e-1, kFortranAbsTol, kFortranRelTol);
  scaleBuffer(b, 1.0, -1.0);
  CHECK_NEAR(b.x[0], 0.2200000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.9747998e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1025200e+0, kFortranAbsTol, kFortranRelTol);
  CHECK(linearYScale(b, 0.0, 1.0, 1.0, 0.5).isOk());
  CHECK_NEAR(b.metrics.thick, 0.2082317e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.4873999e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], -0.1826502e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.5126001e-1, kFortranAbsTol, kFortranRelTol);
  const Result<double> der = derotateBuffer(b);
  CHECK(der.isOk());
  CHECK_NEAR(der.value(), 0.2539675e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.2082317e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.1045911e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1071103e+0, kFortranAbsTol, kFortranRelTol);
  rotateBuffer(b, 5.0 * pi / 180.0);
  CHECK_NEAR(b.metrics.apx1a, -0.8629062e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.apx2t, -0.1651693e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.2957661e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.2982701e+0, kFortranAbsTol, kFortranRelTol);
  CHECK(normalizeBuffer(b).isOk());
  CHECK_NEAR(b.metrics.chord, 0.9999999e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1040828e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.8652993e-1, kFortranAbsTol, kFortranRelTol);
  CHECK(moveSurfaceNormals(b, 0.01).isOk());
  CHECK_NEAR(b.metrics.thick, 0.1240909e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.7660808e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.8050935e-2, kFortranAbsTol, kFortranRelTol);
  static_cast<void>(splineArcLengthRespline(b));
  CHECK_NEAR(b.s[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.s[244], 0.2204722e+2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.xp[0], -0.3524623e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.yp[61], 0.1415332e-1, kFortranAbsTol, kFortranRelTol);
  const Result<CornerAddResult> cad = addCornerPoints(b);
  CHECK(cad.isOk());
  CHECK_EQ(b.size(), 281u);
  CHECK_EQ(cad.value().nbAdded, 36u);
  CHECK_NEAR(b.metrics.thick, 0.1240909e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, -0.1592057e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.7660808e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[b.size() - 1], -0.9778030e-1, kFortranAbsTol, kFortranRelTol);
  CHECK(setSymmetry(b, true).isOk());
  CHECK(b.lgSym);
  CHECK_EQ(b.size(), 285u);
  CHECK_NEAR(b.metrics.thick, 0.1240755e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.2607703e-7, 1.0e-5, 0.0);
  CHECK_NEAR(b.y[0], -0.7657649e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[b.size() - 1], -0.9771511e-1, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignGdesShape2412Sequence) {
  // reference: gdes_shape_reference.txt TGA..FEX (TGAP/LERAD/SCLXY/
  // INSIDE/GETXYF/FLAP/EXEC).
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(setTrailingEdgeGap(b, 0.02, 0.3).isOk());
  CHECK_FALSE(b.lgSame);
  CHECK_EQ(b.size(), 245u);
  CHECK_NEAR(b.metrics.thick, 0.1210763e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.1999944e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.x[0], 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1000000e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.x[244], 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[244], -0.1000000e-1, kFortranAbsTol, kFortranRelTol);
  const Result<double> ler = scaleLeadingEdgeRadius(b, 1.5, 0.15);
  CHECK(ler.isOk());
  // Fortran display 'new LE radius = 0.02237' (F7.5).
  CHECK_NEAR(ler.value(), 0.02237, 1.0e-5, 0.0);
  CHECK_EQ(b.size(), 245u);
  CHECK_NEAR(b.metrics.thick, 0.1249657e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1000286e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.8046735e-1, kFortranAbsTol, kFortranRelTol);
  scaleAboutOrigin(b, ScaleOrigin::te, 0.0, 0.0, 1.2);
  CHECK_NEAR(b.metrics.chord, 0.1200000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.thick, 0.1499588e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], 0.1200343e-1, kFortranAbsTol, kFortranRelTol);
  CHECK(insideContour(b.x, b.y, 0.7, 0.0));
  const FlapHingeResult hg = flapHinge(b, 0.7, 0.0);
  CHECK_NEAR(hg.tops, 0.3035648e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(hg.bots, 0.2156970e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(hg.yf, 0.0, kFortranAbsTol, kFortranRelTol);
  const Result<FlapResult> flp = deflectFlap(b, 0.7, 0.0, 10.0);
  CHECK(flp.isOk());
  CHECK_EQ(b.size(), 249u);
  CHECK(b.lbFlap);
  CHECK_FALSE(b.lgSym);
  CHECK_NEAR(b.metrics.thick, 0.1503139e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.cambr, 0.5453574e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.metrics.sle, 0.1244451e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[0], -0.4027339e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[61], 0.9562000e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[124], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b.y[b.size() - 1], -0.6391554e-1, kFortranAbsTol,
             kFortranRelTol);
  const Result<paneling::AbcopyResult> fex = executeGeometryBuffer(b);
  CHECK(fex.isOk());
  CHECK_EQ(fex.value().panel.x.size(), 249u);
}

TEST(unit, GeometryDesignScalItCases) {
  // reference: gofini_reference.txt SCA.
  const std::vector<double> ys{0.95, 0.2, -0.7};
  CHECK_NEAR(scaleIter(ys, 0.0), 0.1000000e+1, kFortranAbsTol,
             kFortranRelTol);
  const std::vector<double> one{0.95};
  CHECK_NEAR(scaleIter(one, 0.0), 0.1000000e+1, kFortranAbsTol,
             kFortranRelTol);
  // YMAX == 0 -> canonical fallback 1.E-8.
  const std::vector<double> zero{0.0};
  CHECK_NEAR(scaleIter(zero, 0.0), 5.0e7, 1.0, 1.0e-5);
}

TEST(unit, GeometryDesignGofIni2412) {
  // reference: gofini_reference.txt GFI (LGGRID=F, LPLCAM=F,
  // YPMIN/YPMAX=-0.4/0.4, MAIN defaults).
  const GeometryDesignBuffer b = designBuffer(2412);
  const GeometryPlotFrame f = gofIni(b, false, false, PlotDefaults{}, -0.4,
                                     0.4);
  CHECK_NEAR(f.xsf, 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.ysf, 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.dxyg, 0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.xgmin, -0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.xgmax, 0.1100000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.ygmin, -0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.ygmax, 0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.ycmin, -0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.ycmax, 0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.chg, 0.1125000e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.xoff, -0.5000000e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.yoff, -0.9243442e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.dyoffc, -0.2247500e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.ysfp, 0.2227272e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(f.dyoffp, -0.4385909e+0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignGofSet2412) {
  // reference: gofini_reference.txt GFS (XOFF/YOFF/XSF/YSF/CHG from GFI,
  // SIZE=10, XWIND=9, YWIND=6).
  const GeometryDesignBuffer b = designBuffer(2412);
  const GridOverlay g = gofSet(b, -0.5000000e-1, -0.9243442e-1,
                               0.1000000e+1, 0.1000000e+1, 0.1125000e-1,
                               10.0, 9.0, 6.0);
  CHECK_NEAR(g.xg1, 0.8499999e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.yg1, 0.2006557e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.xg2, 0.7600000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.yg2, 0.7922405e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.gxsf, 0.1000000e+1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.gysf, 0.1000000e+2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.dxyg, 0.1000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.xgmin, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.xgmax, 0.8000000e+0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.ygmin, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.ygmax, 0.1000000e+0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, GeometryDesignExecEditWithoutExecKeepsPanel) {
  // A: an edit without EXEC changes only the buffer; a panel contour
  // previously produced from the same buffer stays value-equal.
  GeometryDesignBuffer b = designBuffer(2412);
  const GeometryDesignBuffer before = b;
  const Result<paneling::AbcopyResult> p0 = executeGeometryBuffer(b);
  CHECK(p0.isOk());
  translateBuffer(b, 0.1, 0.05);
  CHECK(b.x != before.x);
  CHECK(b.y != before.y);
  const Result<paneling::AbcopyResult> p1 = executeGeometryBuffer(before);
  CHECK(p1.isOk());
  CHECK(p1.value().panel.x == p0.value().panel.x);
  CHECK(p1.value().panel.y == p0.value().panel.y);
  CHECK(p1.value().panel.s == p0.value().panel.s);
}

TEST(unit, GeometryDesignExecTsetExecMatchesBuffer) {
  // B (gdes-tset-exec): TSET + EXEC propagates exactly the buffer.
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(setThicknessCamberTargets(b, 0.15, 0.03, false).isOk());
  const GeometryDesignBuffer snapshot = b;
  const Result<paneling::AbcopyResult> r = executeGeometryBuffer(b);
  CHECK(r.isOk());
  CHECK_EQ(r.value().panel.x.size(), b.size());
  CHECK(r.value().panel.x == b.x);
  CHECK(r.value().panel.y == b.y);
  // EXEC introduces no additional edit operation: buffer unchanged.
  CHECK(b.x == snapshot.x);
  CHECK(b.y == snapshot.y);
  CHECK(b.s == snapshot.s);
}

TEST(unit, GeometryDesignExecRotateAndDeroExecMatchBuffer) {
  // B (gdes-rotate-exec, gdes-dero-exec): rotation/derotation + EXEC.
  const double pi = std::acos(-1.0);
  GeometryDesignBuffer b = designBuffer(2412);
  rotateBuffer(b, 5.0 * pi / 180.0);
  const Result<paneling::AbcopyResult> r1 = executeGeometryBuffer(b);
  CHECK(r1.isOk());
  CHECK(r1.value().panel.x == b.x);
  CHECK(r1.value().panel.y == b.y);
  const Result<double> der = derotateBuffer(b);
  CHECK(der.isOk());
  const GeometryDesignBuffer snapshot = b;
  const Result<paneling::AbcopyResult> r2 = executeGeometryBuffer(b);
  CHECK(r2.isOk());
  CHECK(r2.value().panel.x == b.x);
  CHECK(r2.value().panel.y == b.y);
  CHECK(b.x == snapshot.x);
  CHECK(b.y == snapshot.y);
}

TEST(unit, GeometryDesignExecFlapExecMatchesBuffer) {
  // B (gdes-flap-exec): FLAP + EXEC propagates exactly the buffer.
  GeometryDesignBuffer b = designBuffer(2412);
  CHECK(deflectFlap(b, 0.7, 0.0, 10.0).isOk());
  const GeometryDesignBuffer snapshot = b;
  const Result<paneling::AbcopyResult> r = executeGeometryBuffer(b);
  CHECK(r.isOk());
  CHECK_EQ(r.value().panel.x.size(), b.size());
  CHECK(r.value().panel.x == b.x);
  CHECK(r.value().panel.y == b.y);
  CHECK(b.x == snapshot.x);
  CHECK(b.y == snapshot.y);
}

TEST(unit, GeometryDesignExecInvalidationIsStructuredOnly) {
  // C: EXEC returns the new panel contour as a value (structured
 // invalidation); it mutates no dependent count/BL states.
  GeometryDesignBuffer b = designBuffer(2412);
  translateBuffer(b, 0.1, 0.0);
  const GeometryDesignBuffer snapshot = b;
  const Result<paneling::AbcopyResult> r = executeGeometryBuffer(b);
  CHECK(r.isOk());
  CHECK_EQ(r.value().panel.x.size(), b.size());
  CHECK(b.x == snapshot.x);
  CHECK(b.y == snapshot.y);
  CHECK(b.s == snapshot.s);
  CHECK(b.xp == snapshot.xp);
  CHECK(b.yp == snapshot.yp);
  CHECK(b.metrics.chord == snapshot.metrics.chord);
}

TEST(unit, GeometryDesignExecCapacityErrorKeepsContour) {
  // D: capacity error (NB > IQX-5 = 596) -> EXEC fails, buffer and the
  // previously produced panel contour remain unchanged.
  GeometryDesignBuffer b = designBuffer(2412);
  const Result<paneling::AbcopyResult> p0 = executeGeometryBuffer(b);
  CHECK(p0.isOk());
  // Extend the buffer to the first disallowed node count (circle, uniform).
  const std::size_t n =
      static_cast<std::size_t>(paneling::kMaxAbcopyNodeCount) + 1;
  GeometryDesignBuffer big;
  big.x.resize(n);
  big.y.resize(n);
  big.s.resize(n);
  big.xp.resize(n);
  big.yp.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double a = 2.0 * 3.14159265358979323846 * double(i) / double(n);
    big.x[i] = std::cos(a);
    big.y[i] = std::sin(a);
  }
  const GeometryDesignBuffer snapshot = big;
  const Result<paneling::AbcopyResult> r = executeGeometryBuffer(big);
  CHECK(r.isErr());
  CHECK(big.x == snapshot.x);
  CHECK(big.y == snapshot.y);
  CHECK(big.s == snapshot.s);
  const Result<paneling::AbcopyResult> p1 = executeGeometryBuffer(b);
  CHECK(p1.isOk());
  CHECK(p1.value().panel.x == p0.value().panel.x);
}

TEST(unit, GeometryDesignThkcamDegenerateChordKeepsBuffer) {
  // D: THKCAM for degenerate chord (CHORD == 0) -> controlled error
  // without partial mutation.
  GeometryDesignBuffer b;
  b.x = {0.0, 0.0, 0.0};
  b.y = {0.0, 0.1, 0.2};
  b.s = {0.0, 0.1, 0.2};
  b.xp = {0.0, 0.0, 0.0};
  b.yp = {1.0, 1.0, 1.0};
  const GeometryDesignBuffer snapshot = b;
  const Result<std::size_t> st = transformThicknessCamber(b, 2.0, 1.0);
  CHECK(st.isErr());
  CHECK(b.x == snapshot.x);
  CHECK(b.y == snapshot.y);
  CHECK(b.s == snapshot.s);
  CHECK(b.xp == snapshot.xp);
  CHECK(b.yp == snapshot.yp);
}

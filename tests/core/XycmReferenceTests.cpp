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

// File: tests/core/XycmReferenceTests.cpp
// precision pass for the XYCM moment-reference transformation.
//
// Proof of the complete canonical effect of XCMREF/YCMREF on CM directly
// against It24.16 Fortran (double precision, -fdefault-real-8) and, on the
// same force state, against an analytical identity.
//
// canonical CLCALC formula (fortran-src/xfoil.f:1130-1165); CM is linear in
// XREF/YREF (the DG*DX/12 and DG*DY/12 terms are reference independent):
// AX = (Xm-XREF)*cosA + (Ym-YREF)*sinA
// AY = (Ym-YREF)*cosA - (Xm-XREF)*sinA
// CM = sum -DX*(AG*AX + DG*DX/12) - DY*(AG*AY + DG*DY/12)
// from which it follows exactly (cosA=cos(alfa), sinA=sin(alfa)):
// dCM/dXREF = Cl*cosA + CDP*sinA
// dCM/dYREF = Cl*sinA - CDP*cosA
// hence the exact reference-point transformation
// CM(XR,YR) = CM(XR0,YR0)
// + (XR-XR0)*(Cl*cosA + CDP*sinA)
// + (YR-YR0)*(Cl*sinA - CDP*cosA).
// the simplified assumption dCM = Cl*dXR is incomplete (it omits the
// cos(angle-of-attack) and CDP*sin shares) and caused the earlier ~2e-5
// deviation; that deviation is not solver or integration noise.
//
// reference: tests/data/core-xycm-reference/xycm_reference.txt (It24.16,
// NACA 2412, PANGEN, GGCALC, SPECAL for alfa=4 degrees, afterwards CLCALC with
// explicit XREF/YREF on the same GAM). The values are embedded as literals
// (convention as in InviscidTests.cpp).

#include "TestSupport.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/core/paneling/PanelingTypes.h"

using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::inviscid;
using namespace xfoil::core::paneling;

namespace {

// canonical MRCL settings of the harness (MATYP=1/RETYP=1, MINF1=0,
// REINF1=3e6), identical to InviscidTests.cpp.
MachReynoldsSettings harnessSettings() {
  MachReynoldsSettings settings;
  settings.machType = 1;
  settings.reType = 1;
  settings.minf1 = 0.0;
  settings.reinf1 = 3.0e6;
  return settings;
}

struct SystemBundle {
  PanelGeometry panel;
  InfluenceSystem system;
};

PanelingParameters xfoil699Paneling() {
  PanelingParameters p;
  p.panelCount = 160;
  p.teToLeDensityRatio = 0.15;
  return p;
}

Result<SystemBundle> buildSystem(const AirfoilBuffer& buffer) {
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  if (pg.isErr()) {
    return Result<SystemBundle>::fail(pg.error());
  }
  const PanelGeometry panel = pg.value().panel;
  std::vector<double> sig0(panel.s.size(), 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  if (gs.isErr()) {
    return Result<SystemBundle>::fail(gs.error());
  }
  return Result<SystemBundle>::ok(SystemBundle{panel, std::move(gs.value())});
}

AirfoilBuffer makeBuffer2412() {
  const Result<NacaResult> n = naca4(2412, 123);
  if (!n.isOk()) {
    return AirfoilBuffer();
  }
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

// reference values from xycm_reference.txt (It24.16, double precision).
struct RefPoint {
  double xref;
  double yref;
  double cl;
  double cdp;
  double cm;
};

// Alfa = 4 degrees (radians), MINF = 0, QINF = 1, N = 160.
constexpr double kAlfa = 0.69813170079773181897e-01;
constexpr double kQinf = 1.0;

// Cl and CDP are reference independent and identical in all four blocks.
constexpr double kRefCl = 0.73757352558247768481;
constexpr double kRefCdp = -0.10977267908221254901e-02;

constexpr RefPoint kRefs[] = {
    {0.0, 0.0, kRefCl, kRefCdp, -0.24555188803562483235},
    {0.25, 0.0, kRefCl, kRefCdp, -0.61626823037021875862e-01},
    {0.25, 0.1, kRefCl, kRefCdp, -0.56372264931503171526e-01},
    {-0.5, 0.1, kRefCl, kRefCdp, -0.60814745992731189528},
};

}  // namespace

// A. Direct numerical Fortran comparison: core clCalc against It24.16 at
// several moment references (CL, CDP, CM).
TEST(unit, XycmReferenceMatchesFortran) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412());
  CHECK(b.isOk());

  // force state for alfa=4 degrees (gam/gamA/alfa/minf from the SPECAL path).
  const Result<SpecPointResult> spec =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), kAlfa,
                kQinf, 0.25, 0.0);
  CHECK(spec.isOk());
  CHECK(spec.value().converged);

  for (const RefPoint& ref : kRefs) {
    const LoadResult r =
        clCalc(b.value().panel.x, b.value().panel.y, spec.value().gam,
               spec.value().gamA, spec.value().alfa, spec.value().minf, kQinf,
               ref.xref, ref.yref);
    // Fortran vs core observed: diff = 0.0 (bit-identical; the E30.20
    // reference values are reproduced exactly by the double core). Threshold
    // 1e-12 provides a small safety margin against toolchain libm rounding,
    // about four orders of magnitude below the smallest CM quantity and far
    // below the 2e-5 earlier misinterpreted as noise rather than an error.
    CHECK_NEAR(r.cl, ref.cl, 1e-12, 1e-12);
    CHECK_NEAR(r.cdp, ref.cdp, 1e-12, 1e-12);
    CHECK_NEAR(r.cm, ref.cm, 1e-12, 1e-12);
  }
}

// B. Analytical transformation check for the same force state: the
// exact canonical reference-point transformation (no second solver run).
TEST(unit, XycmReferenceExactTransformation) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412());
  CHECK(b.isOk());

  const Result<SpecPointResult> spec =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), kAlfa,
                kQinf, 0.25, 0.0);
  CHECK(spec.isOk());

  const std::vector<double>& x = b.value().panel.x;
  const std::vector<double>& y = b.value().panel.y;
  const std::vector<double>& gam = spec.value().gam;
  const std::vector<double>& gamA = spec.value().gamA;
  const double alfa = spec.value().alfa;
  const double minf = spec.value().minf;

  // base force state for reference (0,0).
  const LoadResult base =
      clCalc(x, y, gam, gamA, alfa, minf, kQinf, 0.0, 0.0);
  const double ca = std::cos(alfa);
  const double sa = std::sin(alfa);

  for (const RefPoint& ref : kRefs) {
    const LoadResult r =
        clCalc(x, y, gam, gamA, alfa, minf, kQinf, ref.xref, ref.yref);
    // Cl/CDP are reference independent: identical to the base state.
    CHECK_NEAR(r.cl, base.cl, 0.0, 0.0);
    CHECK_NEAR(r.cdp, base.cdp, 0.0, 0.0);
    // exact canonical transformation.
    const double predicted =
        base.cm + (ref.xref - 0.0) * (base.cl * ca + base.cdp * sa) +
        (ref.yref - 0.0) * (base.cl * sa - base.cdp * ca);
    // observed: max |dCM| = 3.3e-16 (machine precision, identical
    // force state). Threshold 1e-12 provides a safe distance at double level.
    CHECK_NEAR(r.cm, predicted, 1e-12, 1e-12);
  }
}

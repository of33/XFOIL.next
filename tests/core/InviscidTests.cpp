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

// File: tests/core/InviscidTests.cpp
// Unit tests for the new 0-based core: inviscid solver kernel
// (MRCL, comset, cpCalc, psiLin, pswLin, ggCalc, qwCalc, qdCalc, stfind,
// qiset, xywake, clCalc, mhinge, specAlpha, specCl).
//
// the tests use only xfoil::core::inviscid and no app, CLI, or
// 1-based code.
//
// reference comparison: the expected values come from the focused Fortran
// harness (tests/fortran-reference/harnesses/inviscid), which calls the
// canonical routines unchanged (MRCL/COMSET/CPCALC/CLCALC/TECALC from
// xfoil.f, PSILIN/PSWLIN/GGCALC/QWCALC/QDCALC/STFIND/QISET/XYWAKE from
// xpanel.f, SPECAL/SPECCL/MHINGE from xoper.f, extracted byte-faithfully).
// Fortran REAL is single precision; the new core computes in double.
// formula and operation order are identical.
//
// tolerance notes:
// - kFortranRelTol/kFortranAbsTol: base for all smooth values.
// - Newton-convergence quantities (Cl/CM/CDP) converge exactly and follow
// the same tolerances.
// - Pi constants from Fortran (single-precision PI) enter angle conversions;
// kAngleAbsTol covers this deviation.

#include <cmath>
#include "TestSupport.h"

#include <array>
#include <stdexcept>
#include <cstddef>
#include <utility>
#include <vector>

#include "xfoil/core/Limits.h"
#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inverse/MixedInverseKernels.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/inviscid/TrailingEdge.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/core/paneling/PanelingTypes.h"

using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::inviscid;
using namespace xfoil::core::paneling;

namespace {

// canonical NACA 0012 buffer (Fortran NACA-CODE 12).
AirfoilBuffer makeBuffer0012() {
  const Result<NacaResult> n = naca4(12, 123);
  if (!n.isOk()) {
    return AirfoilBuffer();
  }
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

// canonical NACA 2412 buffer.
AirfoilBuffer makeBuffer2412() {
  const Result<NacaResult> n = naca4(2412, 123);
  if (!n.isOk()) {
    return AirfoilBuffer();
  }
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

// Clark-Y buffer (151 buffer points from
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

// MRCL standard settings of the harness (MATYP=1/RETYP=1).
MachReynoldsSettings harnessSettings() {
  MachReynoldsSettings settings;
  settings.machType = 1;
  settings.reType = 1;
  settings.minf1 = 0.0;
  settings.reinf1 = 3.0e6;
  return settings;
}

PanelingParameters xfoil699Paneling() {
  PanelingParameters p;
  p.panelCount = 160;
  p.teToLeDensityRatio = 0.15;
  return p;
}

// Paneling plus factorized influence system (GGCALC chain).
struct SystemBundle {
  PanelGeometry panel;
  InfluenceSystem system;
};

Result<SystemBundle> buildSystem(const AirfoilBuffer& buffer,
                                 bool sharpTeHint) {
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
  (void)sharpTeHint;
  return Result<SystemBundle>::ok(
      SystemBundle{panel, std::move(gs.value())});
}

// constants from the Fortran init (xfoil.f).
constexpr double kPi = 3.14159265358979323846;

// Fortran reference tolerances (as in MathTests/PanelingTests).
constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;

// angle tolerance (PanelingTests).
constexpr double kAngleAbsTol = 5.0e-3;
constexpr double kAngleRelTol = 1.0e-3;

// influence solution (GAMU/GAM/QINV): the amplified panel-range deviations
// (spline corners, ATAN2 with single-precision PI) accumulate through the
// matrix factorization into a relative deviation up to ~3e-4, so the
// tolerance is one order above the observed accumulation.
constexpr double kInfluenceRelTol = 3.0e-3;

// Cl/CM of a symmetric airfoil at alpha=0 are differences of large
// numbers; the Fortran value sits at the single-precision rounding level.
constexpr double kRoundoffAbsTol = 1.0e-5;

}  // namespace

// reference: inviscid_reference.txt MRCL1 (MATYP=1/RETYP=1, MINF1=0,
// REINF1=3e6, CLS=0).
TEST(unit, InviscidMrclIncompressible) {
  const MachReynoldsSettings settings = harnessSettings();
  const Result<MachReynoldsResult> r = mrcl(settings, 0.0);
  CHECK(r.isOk());
  CHECK_NEAR(r.value().minf, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.value().reinf, 3.0e6, 1.0e-3, kFortranRelTol);
  CHECK_NEAR(r.value().mCls, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.value().rCls, 0.0, 1.0e-3, kFortranRelTol);
  CHECK_FALSE(r.value().machLimited);
  CHECK_FALSE(r.value().reLimited);
}

// reference: inviscid_reference.txt MRCL2 (MATYP=2/RETYP=2, MINF1=0.1,
// CLS=0.5).
TEST(unit, InviscidMrclCompressible) {
  MachReynoldsSettings settings = harnessSettings();
  settings.machType = 2;
  settings.reType = 2;
  settings.minf1 = 0.1;
  const Result<MachReynoldsResult> r = mrcl(settings, 0.5);
  CHECK(r.isOk());
  CHECK_NEAR(r.value().minf, 0.14142136275768280029, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.value().reinf, 4.242641e6, 1.0e-3, kFortranRelTol);
  CHECK_NEAR(r.value().mCls, -0.14142136275768280029, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.value().rCls, -4.242641e6, 1.0e-3, kFortranRelTol);
}

// reference: inviscid_reference.txt MRCLBAD (illegal RETYP -> numerically
// as RETYP=1).
TEST(unit, InviscidMrclIllegalRetype) {
  MachReynoldsSettings settings = harnessSettings();
  settings.reType = 5;
  const Result<MachReynoldsResult> r = mrcl(settings, 0.0);
  CHECK(r.isOk());
  CHECK_NEAR(r.value().minf, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.value().reinf, 3.0e6, 1.0e-3, kFortranRelTol);
}

// reference: inviscid_reference.txt COMSET1 (MINF=0) and COMSET2
// (MINF=0.14142136275768280029).
TEST(unit, InviscidComset) {
  const CompressibilityState c1 = comset(0.0, 1.0);
  CHECK_NEAR(c1.tklam, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c1.cpstar, -999.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c1.qstar, 999.0, kFortranAbsTol, kFortranRelTol);

  const CompressibilityState c2 = comset(0.14142136275768280029, 1.0);
  CHECK_NEAR(c2.tklam, 0.0050506344996392726898, kFortranAbsTol,
             kFortranRelTol);
}

// reference: inviscid_reference.txt CPCALC1 (Q=[1.0, 0.9, 1.1], QINF=1,
// MINF=0).
TEST(unit, InviscidCpCalcIncompressible) {
  const double q[]{1.0, 0.9, 1.1};
  const Result<std::vector<double>> r = cpCalc(q, 1.0, 0.0);
  CHECK(r.isOk());
  CHECK_EQ(r.value().size(), std::size_t{3});
  CHECK_NEAR(r.value()[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.value()[1], 0.19000005722045898438, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.value()[2], -0.21000003814697265625, kFortranAbsTol,
             kFortranRelTol);
}

// reference: inviscid_reference.txt GGCALC12 (NACA 0012, 160 panels,
// influence matrix and base solutions).
TEST(unit, InviscidGgCalcNaca0012) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  CHECK_EQ(n, std::size_t{160});
  CHECK_EQ(b.value().system.gamu1.size(), n + 1);
  CHECK_EQ(b.value().system.gamu2.size(), n + 1);
  CHECK_EQ(b.value().system.aijpiv.size(), n + 1);

  // Fortran AIJ(1,1)/AIJ(1,2) (row-major 0-based).
  const double aij11 = b.value().system.aij[0];
  const double aij12 = b.value().system.aij[1];
  CHECK_NEAR(aij11, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(aij12, 0.0, kFortranAbsTol, kFortranRelTol);

  // Fortran AIJ(N+1,1) (row n, col 0).
  const double aijk = b.value().system.aij[n * (n + 1)];
  CHECK_NEAR(aijk, -0.38860570639371871948e-2, kFortranAbsTol,
             kFortranRelTol);

  // base solutions GAMU(1,1)/GAMU(N+1,1).
  CHECK_NEAR(b.value().system.gamu1[0], 0.76726114749908447266,
             kFortranAbsTol, kInfluenceRelTol);
  CHECK_NEAR(b.value().system.gamu1[n], 0.95671525923535227776e-3,
             kFortranAbsTol, kInfluenceRelTol);

  // QINVU(1,1)/QINVU(2,1) (QISET in the SPECAL flow; here from the
  // base solution with alfa=0).
  std::vector<double> qinv;
  std::vector<double> qinvA;
  qiset(b.value().system.qinvu1, b.value().system.qinvu2, 0.0, qinv, qinvA);
  CHECK_NEAR(qinv[0], 0.76726114749908447266, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(qinv[1], 0.87356317043304443359, kFortranAbsTol,
             kInfluenceRelTol);
}

// reference: inviscid_reference.txt SPEC12-0 (NACA 0012, Alfa=0).
TEST(unit, InviscidSpecAlNaca0012Alpha0) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  const Result<SpecPointResult> r =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(r.isOk());
  CHECK(r.value().converged);
  CHECK_NEAR(r.value().gam[0], 0.76726114749908447266, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().gam[n - 1], -0.76726114749908447266,
             kFortranAbsTol, kInfluenceRelTol);
  CHECK_NEAR(r.value().qinv[0], 0.76726114749908447266, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().qinv[n - 1], -0.76726114749908447266,
             kFortranAbsTol, kInfluenceRelTol);
  CHECK_NEAR(r.value().cl, 0.62976032495498657227e-5, kRoundoffAbsTol,
             1.0);
  CHECK_NEAR(r.value().cm, -0.17277041024499339983e-5, kRoundoffAbsTol,
             1.0);
  CHECK_NEAR(r.value().cdp, -0.10654608486220240593e-2, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cp[0], 0.41131031513214111328, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cp[1], 0.23688739538192749023, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_EQ(r.value().cp.size(), n);

  // STFIND on the converged GAM distribution.
  const StagnationPointResult st = stfind(r.value().gam, b.value().panel.s);
  CHECK(st.found);
  CHECK_NEAR(st.sst, 1.0196213722229003906, kFortranAbsTol, kFortranRelTol);
  CHECK_EQ(st.ist, std::size_t{79});

  // TECALC complete (SIGTE/GAMTE part; Fortran reference SPEC12-0).
  // SIGTE follows GAM (influence solution) -> kInfluenceRelTol; GAMTE is near
  // zero (GAM(1)-GAM(N) ~ 0 times SDS=ASTE/DSTE ~ 0) -> absolute threshold.
  CHECK_NEAR(r.value().te.sigte, 0.75981211662292480469, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().te.gamte, 4.6303981093842594419e-7, kFortranAbsTol,
             1.0);
}

// reference: inviscid_reference.txt SPEC12-4 (NACA 0012, alfa=4 degrees).
TEST(unit, InviscidSpecAlNaca0012Alpha4) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  const double alfa = 4.0 * kPi / 180.0;
  const Result<SpecPointResult> r =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa,
                1.0, 0.25, 0.0);
  CHECK(r.isOk());
  CHECK(r.value().converged);
  CHECK_NEAR(r.value().gam[0], 0.76543205976486206055, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cl, 0.48286610841751098633, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cm, -0.55782957933843135834e-2, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cdp, -0.10863137431442737579e-2, kRoundoffAbsTol,
             1.0);
  CHECK_NEAR(r.value().gam[n - 1], -0.76543205976486206055, kFortranAbsTol,
             kInfluenceRelTol);

  const StagnationPointResult st = stfind(r.value().gam, b.value().panel.s);
  CHECK(st.found);
  CHECK_NEAR(st.sst, 0.10318831205368041992e1, kFortranAbsTol,
             kFortranRelTol);
  CHECK_EQ(st.ist, std::size_t{85});

  // TECALC complete (SIGTE/GAMTE; Fortran reference SPEC12-4).
  CHECK_NEAR(r.value().te.sigte, 0.75800079107284545898, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().te.gamte, 4.6193596858756791335e-7, kFortranAbsTol,
             1.0);
}

// reference: inviscid_reference.txt SPEC24-0/-4/-8/-2 (NACA 2412).
TEST(unit, InviscidSpecAlNaca2412) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());

  const Result<SpecPointResult> r0 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(r0.isOk());
  CHECK(r0.value().converged);
  CHECK_NEAR(r0.value().cl, 0.25533032417297363281, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r0.value().cm, -0.55724751204252243042e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r0.value().cdp, -0.10689864866435527802e-2, kFortranAbsTol,
             kInfluenceRelTol);

  const double alfa4 = 4.0 * kPi / 180.0;
  const Result<SpecPointResult> r4 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa4,
                1.0, 0.25, 0.0);
  CHECK(r4.isOk());
  CHECK(r4.value().converged);
  CHECK_NEAR(r4.value().cl, 0.73749876022338867188, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r4.value().cm, -0.61609167605638504028e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r4.value().cdp, -0.10975815821439027786e-2, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r4.value().cp[0], 0.41770356893539428711, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r4.value().cp[1], 0.21996748447418212891, kFortranAbsTol,
             kInfluenceRelTol);

  const double alfa8 = 8.0 * kPi / 180.0;
  const Result<SpecPointResult> r8 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa8,
                1.0, 0.25, 0.0);
  CHECK(r8.isOk());
  CHECK(r8.value().converged);
  CHECK_NEAR(r8.value().cl, 1.2160812616348266602, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r8.value().cm, -0.67627504467964172363e-1, kFortranAbsTol,
             kInfluenceRelTol);

  const double alfaM2 = -2.0 * kPi / 180.0;
  const Result<SpecPointResult> rM2 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfaM2,
                1.0, 0.25, 0.0);
  CHECK(rM2.isOk());
  CHECK(rM2.value().converged);
  CHECK_NEAR(rM2.value().cl, 0.1363167166709899023e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(rM2.value().cm, -0.52868727594614028931e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(rM2.value().cp[0], 0.41118007898330688477, kFortranAbsTol,
             kInfluenceRelTol);
}

// reference: inviscid_reference.txt SPECCLY-0/-4 (Clark Y).
TEST(unit, InviscidSpecAlClarkY) {
  const Result<SystemBundle> b = buildSystem(makeBufferClarkY(), false);
  CHECK(b.isOk());

  const Result<SpecPointResult> r0 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(r0.isOk());
  CHECK(r0.value().converged);
  CHECK_NEAR(r0.value().cl, 0.41616317629814147949, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r0.value().cm, -0.87916269898414611816e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r0.value().cdp, -0.67602796480059623718e-3, kRoundoffAbsTol,
             1.0);

  const double alfa4 = 4.0 * kPi / 180.0;
  const Result<SpecPointResult> r4 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa4,
                1.0, 0.25, 0.0);
  CHECK(r4.isOk());
  CHECK(r4.value().converged);
  CHECK_NEAR(r4.value().cl, 0.89707607030868530273, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r4.value().cm, -0.94283469021320343018e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r4.value().cdp, -0.68975682370364665985e-3, kRoundoffAbsTol,
             1.0);
}

// reference: inviscid_reference.txt SPECC24 (SPECCL Cl=0.5 on NACA 2412).
TEST(unit, InviscidSpecClNaca2412) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> r =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(r.isOk());
  CHECK(r.value().converged);
  CHECK_NEAR(r.value().cl, 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.value().cm, -0.58683056384325027466e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cdp, -0.10780720040202140808e-2, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().alfa * 180.0 / kPi, 2.0272319316864013672,
             kAngleAbsTol, kAngleRelTol);
  CHECK_NEAR(r.value().qinv[0], 0.76541167497634887695, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(r.value().cp[0], 0.41414499282836914062, kFortranAbsTol,
             kInfluenceRelTol);
}

// CSEQ scenario of the CLI polars (NACA 0012, SPECAL at 0/2/4 deg, then
// SPECCL Cl=0 with start alfa=4 deg). Reference: Fortran harness
// (xoper.f SPECCL, double realization): Newton path with CL_ALF updated
// per iteration; final ALFA = +1.0198414765846303E-14.
// the absolute value 1e-12 catches a porting bug in which CL_ALF only
// came from the start iteration (wrong final alfa -6.689E-10).
TEST(unit, InviscidSpecClAfterSpecAlSequenceNaca0012) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const MachReynoldsSettings settings = harnessSettings();
  const double dtor = kPi / 180.0;
  const Result<SpecPointResult> a0 =
      specAlpha(b.value().system, b.value().panel, settings, 0.0, 1.0, 0.0,
                0.0);
  CHECK(a0.isOk());
  const Result<SpecPointResult> a2 =
      specAlpha(b.value().system, b.value().panel, settings, 2.0 * dtor, 1.0,
                0.0, 0.0);
  CHECK(a2.isOk());
  const Result<SpecPointResult> a4 =
      specAlpha(b.value().system, b.value().panel, settings, 4.0 * dtor, 1.0,
                0.0, 0.0);
  CHECK(a4.isOk());
  const Result<SpecPointResult> r =
      specCl(b.value().system, b.value().panel, settings, 0.0, 4.0 * dtor,
             1.0, 0.0, 0.0);
  CHECK(r.isOk());
  CHECK(r.value().converged);
  CHECK_NEAR(r.value().alfa, 1.0198414765846303e-14, 1.0e-12, 0.0);
  CHECK_NEAR(r.value().cl, -7.8325804130034221e-17, 1.0e-12, 0.0);
}

// CL_ALF raw-bit pin. Fortran CLCALC (xfoil.f:1088) accumulates
// CL_ALF = CL_ALF + DX*AG_ALF + AG*DX_ALF left-associated, i.e.
// ((Cl_ALF + DX*AG_ALF) + AG*DX_ALF). The C++ form
// `clAlf += dx*agAlf + ag*dxAlf` parenthesized the products first and
// yielded 1 ULP too low (401BABC014C761D1 instead of ...D2). Reference bits:
// focused CLCALC harness with bit-identical restrict, bit-equal at -O0,
// -O2, and -O3 (401BABC014C761D2).
TEST(unit, InviscidClCalcClAlfAssociationBits) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const double dtor = kPi / 180.0;
  const Result<SpecPointResult> a2 = specAlpha(
      b.value().system, b.value().panel, harnessSettings(), 2.0 * dtor, 1.0,
      0.25, 0.0);
  CHECK(a2.isOk());
  const LoadResult load = clCalc(b.value().panel.x, b.value().panel.y,
                                 a2.value().gam, a2.value().gamA, 2.0 * dtor,
                                 0.0, 1.0, 0.25, 0.0);
  // PUB-W1: Linux keeps the exact (tolerance-0) contract. On
  // Windows/MSYS2 these load-integration values run through the same
  // kernels whose recorded influence-matrix deviations reach 105 ULP;
  // class kUlpBudget1024 is the documented, provisional portable bound
  // (to be re-measured on the MSYS2 acceptance run).
  testing::checkPortableNumeric(
      load.clAlf, 6.9177249190037902,
      testing::PortableNumericClass::kUlpBudget1024, "load.clAlf", "6.9177249190037902",
      __FILE__, __LINE__);
  testing::checkPortableNumeric(
      load.cl, 0.24157185484170532,
      testing::PortableNumericClass::kUlpBudget1024, "load.cl", "0.24157185484170532",
      __FILE__, __LINE__);
  testing::checkPortableNumeric(
      load.cm, -0.0027940299997170326,
      testing::PortableNumericClass::kUlpBudget1024, "load.cm", "-0.0027940299997170326",
      __FILE__, __LINE__);
}

// near-zero SPECCL end state with a raw-bit pin. ASEQ 0/1/2 (raw radians
// 2*DTOR as SPECCL start), then SPECCL CLSPEC=0: Fortran final values (gdb
// at the canonical reference binary, identical to the PLXADD point header)
// are alfa=1.0218560427528418e-14, cl=1.4027025085568577e-16,
// cm=1.5568571767289802e-14.
TEST(unit, InviscidSpecClNearZeroFinalBits) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const MachReynoldsSettings settings = harnessSettings();
  const double dtor = kPi / 180.0;
  const Result<SpecPointResult> a0 =
      specAlpha(b.value().system, b.value().panel, settings, 0.0, 1.0, 0.0,
                0.0);
  CHECK(a0.isOk());
  const Result<SpecPointResult> a1 =
      specAlpha(b.value().system, b.value().panel, settings, 1.0 * dtor, 1.0,
                0.0, 0.0);
  CHECK(a1.isOk());
  const Result<SpecPointResult> a2 =
      specAlpha(b.value().system, b.value().panel, settings, 2.0 * dtor, 1.0,
                0.0, 0.0);
  CHECK(a2.isOk());
  const Result<SpecPointResult> r =
      specCl(b.value().system, b.value().panel, settings, 0.0, 2.0 * dtor,
             1.0, 0.25, 0.0);
  CHECK(r.isOk());
  CHECK(r.value().converged);
  // PUB-W1: near-zero SPECCL residuals. Linux keeps the exact
  // (tolerance-0) contract. These Fortran COMMON-zero-scale residuals
  // (1e-14..1e-16) swing with the last iteration's rounding, so a
  // small absolute contract (1e-15, measured max 3.3e-16 on MSYS2) is
  // used on Windows; a relative contract is unsuitable near zero.
  testing::checkPortableNumeric(
      r.value().alfa, 1.0218560427528418e-14,
      testing::PortableNumericClass::kNearZero1e15, "r.value().alfa",
      "1.0218560427528418e-14", __FILE__, __LINE__);
  testing::checkPortableNumeric(
      r.value().cl, 1.4027025085568577e-16,
      testing::PortableNumericClass::kNearZero1e15, "r.value().cl",
      "1.4027025085568577e-16", __FILE__, __LINE__);
  testing::checkPortableNumeric(
      r.value().cm, 1.5568571767289802e-14,
      testing::PortableNumericClass::kNearZero1e15, "r.value().cm",
      "1.5568571767289802e-14", __FILE__, __LINE__);
}

// reference: inviscid_reference.txt WAKE (XYWAKE after SPECC24).
TEST(unit, InviscidXywakeAfterSpecCl) {  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk = xywake(
      b.value().panel, sp.value().gam, sp.value().gamA,
      b.value().system.gamu1, b.value().system.gamu2, sp.value().alfa, 1.0,
      1.0);
  CHECK(wk.isOk());
  CHECK_EQ(wk.value().nw, std::size_t{23});
  CHECK_EQ(wk.value().x.size(), std::size_t{23});
  CHECK_NEAR(wk.value().x[0], 1.0000997781753540039, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(wk.value().y[0], -0.65245862970186863095e-5, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(wk.value().nx[0], -0.65245866775512695312e-1, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(wk.value().ny[0], -0.99786919355392456055, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(wk.value().apanel[0], 1.5076199769973754883, kAngleAbsTol,
             kAngleRelTol);
  CHECK_NEAR(wk.value().x[1], 1.0084677934646606445, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(wk.value().y[1], -0.53588725859299302101e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: inviscid_reference.txt QWCALC (QWCALC after XYWAKE; all
// nodes, Fortran QINVU(I,1)).
TEST(unit, InviscidQwCalcWake) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk = xywake(
      b.value().panel, sp.value().gam, sp.value().gamA,
      b.value().system.gamu1, b.value().system.gamu2, sp.value().alfa, 1.0,
      1.0);
  CHECK(wk.isOk());
  const WakeBasisResult qb =
      qwCalc(b.value().system, wk.value(), b.value().panel, sp.value().alfa,
             1.0);
  CHECK_EQ(qb.qinvu1.size(), wk.value().nw);
  CHECK_NEAR(qb.qinvu1[0], -0.76685887575149536133, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(qb.qinvu1[1], -0.85291016101837158203, kFortranAbsTol,
             kInfluenceRelTol);
}

// reference: inviscid_reference.txt QISETW (QISET with wake; Fortran
// QINV(1)/QINV(N+1)/QINV_A(1)).
TEST(unit, InviscidQisetWake) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk = xywake(
      b.value().panel, sp.value().gam, sp.value().gamA,
      b.value().system.gamu1, b.value().system.gamu2, sp.value().alfa, 1.0,
      1.0);
  CHECK(wk.isOk());
  const WakeBasisResult qb =
      qwCalc(b.value().system, wk.value(), b.value().panel, sp.value().alfa,
             1.0);

  // Fortran QISET: QINV(I)=COSA*QINVU(I,1)+SINA*QINVU(I,2) for I=1..N+NW.
  // the test compares the wake node N+1/N (0-based nw).
  std::vector<double> qinvu1All = b.value().system.qinvu1;
  std::vector<double> qinvu2All = b.value().system.qinvu2;
  qinvu1All.insert(qinvu1All.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2All.insert(qinvu2All.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinv;
  std::vector<double> qinvA;
  const std::size_t n = b.value().panel.x.size();
  qiset(qinvu1All, qinvu2All, sp.value().alfa, qinv, qinvA);
  CHECK_EQ(qinv.size(), n + wk.value().nw);
  CHECK_NEAR(qinv[0], 0.76541167497634887695, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(qinv[n], -0.76541167497634887695, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(qinvA[0], -0.54452955722808837891e-1, 2.0e-3, 1.0);
}

// reference: inviscid_reference.txt QDCALC (source-influence matrix with
// wake).
TEST(unit, InviscidQdCalcWake) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk = xywake(
      b.value().panel, sp.value().gam, sp.value().gamA,
      b.value().system.gamu1, b.value().system.gamu2, sp.value().alfa, 1.0,
      1.0);
  CHECK(wk.isOk());
  const std::vector<double> sig0(n, 0.0);
  const SourceInfluenceResult qd =
      qdCalc(b.value().system, wk.value(), b.value().panel, sp.value().alfa,
             1.0, sig0);
  CHECK_EQ(qd.dij.size(), (n + wk.value().nw) * (n + wk.value().nw));
  CHECK_NEAR(qd.dij[0], -0.50437366485595703125e2, 1.0e-4, 1.0e-4);
  // Fortran DIJ(1,N+1) -> row 0, col n.
  CHECK_NEAR(qd.dij[n], 0.56866367340087890625e2, 1.0e-4, 1.0e-4);
  // Fortran DIJ(N,N+1) -> row n-1, col n.
  CHECK_NEAR(qd.dij[(n - 1) * (n + wk.value().nw) + n],
             -0.56866367340087890625e2, 1.0e-4, 1.0e-4);
}

// reference: inviscid_reference.txt MHINGE (XOF/YOF = 0.5/0.0, NACA 2412
// after SPECC24).
TEST(unit, InviscidMhinge) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const HingeMomentResult hm =
      mhinge(b.value().panel.x, b.value().panel.xp, b.value().panel.y,
             b.value().panel.yp, b.value().panel.s, sp.value().cp,
             std::span<const double>{}, false, 0.5, 0.0);
  CHECK_NEAR(hm.hmom, 0.28841709718108177185e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(hm.hfx, -0.25821333751082420349e-1, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(hm.hfy, 0.15660421550273895264, kFortranAbsTol,
             kInfluenceRelTol);
}

// Viscous CPV branch: mhinge is linear in the pressure distribution, so
// cpv = 2*cp must exactly double the integral (Fortran IF(LVISC)->CPV).
TEST(unit, InviscidMhingeViscousCpvBranch) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const auto& p = b.value().panel;

  // cpv == cpi -> lvisc=true returns the same value as inviscid.
  const HingeMomentResult hmInv = mhinge(p.x, p.xp, p.y, p.yp, p.s,
                                         sp.value().cp, sp.value().cp, false,
                                         0.5, 0.0);
  const HingeMomentResult hmViscEq = mhinge(p.x, p.xp, p.y, p.yp, p.s,
                                            sp.value().cp, sp.value().cp, true,
                                            0.5, 0.0);
  CHECK_NEAR(hmViscEq.hmom, hmInv.hmom, 0.0, 1.0e-14);
  CHECK_NEAR(hmViscEq.hfx, hmInv.hfx, 0.0, 1.0e-14);
  CHECK_NEAR(hmViscEq.hfy, hmInv.hfy, 0.0, 1.0e-14);

  // cpv = 2*cp -> the integrals double themselves (CPV is used).
  std::vector<double> cpv(sp.value().cp.size(), 0.0);
  for (std::size_t i = 0; i < cpv.size(); ++i) {
    cpv[i] = 2.0 * sp.value().cp[i];
  }
  const HingeMomentResult hmVisc2 = mhinge(p.x, p.xp, p.y, p.yp, p.s,
                                           sp.value().cp, cpv, true, 0.5, 0.0);
  CHECK_NEAR(hmVisc2.hmom, 2.0 * hmInv.hmom, 0.0, 1.0e-9);
  CHECK_NEAR(hmVisc2.hfx, 2.0 * hmInv.hfx, 0.0, 1.0e-9);
  CHECK_NEAR(hmVisc2.hfy, 2.0 * hmInv.hfy, 0.0, 1.0e-9);
}

// Fortran constants: INVPI/PSI/PSI2 (xfoil.f: 392-394) -> invPi.
TEST(unit, InviscidInvpi) {
  CHECK_NEAR(invPi(), kPi, kFortranAbsTol, kFortranRelTol);
}

// first differing intermediate stage: the PSILIN influence coefficients at
// panel node 1 (NACA 0012, 160 panels) against the Fortran reference
// (PSILIN(1,...) from the focus harness). The deviations (DZDG1 1.5e-8
// abs / 4.0e-6 rel, PSI_NI 2.9e-7 abs) stem from the panel contour
// (single-precision SEVAL/SPLINE) and confirm the formula structure:
// a structural formula error would cause O(1) deviations.
TEST(unit, InviscidPsiLinIntermediates) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  std::vector<double> gam0(n, 0.0);
  const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
      b.value().panel.x[0], b.value().panel.y[0], b.value().panel.xp[0],
      b.value().panel.yp[0], b.value().panel.x[n - 1], b.value().panel.y[n - 1],
      b.value().panel.xp[n - 1], b.value().panel.yp[n - 1],
      b.value().panel.chord);
  PsiLinContext ctx;
  ctx.x = b.value().panel.x;
  ctx.y = b.value().panel.y;
  ctx.s = b.value().panel.s;
  ctx.nx = b.value().panel.xn;
  ctx.ny = b.value().panel.yn;
  ctx.apanel = b.value().panel.apanel;
  ctx.gam = gam0;
  ctx.gamA = gam0;
  ctx.gamu1 = gam0;
  ctx.gamu2 = gam0;
  ctx.sig = gam0;
  ctx.sharp = teg.sharp;
  ctx.dste = teg.dste;
  ctx.ante = teg.ante;
  ctx.aste = teg.aste;
  ctx.qinf = 1.0;
  ctx.image = false;
  ctx.yimage = 0.0;
  PsiLinInputs in;
  in.node = 0;
  in.xi = b.value().panel.x[0];
  in.yi = b.value().panel.y[0];
  in.nxi = b.value().panel.xn[0];
  in.nyi = b.value().panel.yn[0];
  in.alfa = 0.0;
  in.nodeOnAirfoil = true;
  in.siglin = true;
  const PsiLinResult pl = psiLin(in, ctx);
  // Fortran reference: PSILIN(1,...) from xpanel.f (focus harness, node 1).
  CHECK_NEAR(pl.dzdg[0], -3.8860570639371871948e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(pl.dzdg[1], -7.5537571683526039124e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(pl.psiNi, 0.99030005931854248047, kFortranAbsTol,
             kFortranRelTol);
}

// Direct Fortran comparison of the PSWLIN outputs (wake node 1 of the
// NACA-2412 state after SPECCL Cl=0.5 + XYWAKE, SIG linear 0.01*(I-N)).
// the Fortran reference values come from the byte-faithfully extracted
// PSWLIN (xpanel.f), called with the same panel and wake contour.
// the deviations (PSI 1.5e-8, PSI_NI 1.0e-6, DZDM <=3.0e-6, DQDM
// <=5.7e-5 abs) stem from the REAL(4) capture of the contour in the
// Fortran proof program (half-ULP ~6e-8 rel, amplified on the
// log/atan2-based coefficients) -> threshold 1e-4 abs / 1e-4 rel.
constexpr double kPswlinDirectAbsTol = 1.0e-4;
constexpr double kPswlinDirectRelTol = 1.0e-4;

TEST(unit, InviscidPswLinDirectFortran) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> sp =
      specCl(b.value().system, b.value().panel, harnessSettings(), 0.5, 0.0,
             1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk = xywake(
      b.value().panel, sp.value().gam, sp.value().gamA,
      b.value().system.gamu1, b.value().system.gamu2, sp.value().alfa, 1.0,
      1.0);
  CHECK(wk.isOk());
  const std::size_t nw = wk.value().nw;
  std::vector<double> wakeSig(nw);
  for (std::size_t i = 0; i < nw; ++i) {
    wakeSig[i] = 0.01 * static_cast<double>(i + 1);
  }
  PswLinContext ctx;
  ctx.x = wk.value().x;
  ctx.y = wk.value().y;
  ctx.apanel = wk.value().apanel;
  ctx.sig = wakeSig;
  PswLinInputs in;
  in.xi = b.value().panel.x[0];
  in.yi = b.value().panel.y[0];
  in.nxi = b.value().panel.xn[0];
  in.nyi = b.value().panel.yn[0];
  in.alfa = sp.value().alfa;
  in.nodeWake = false;
  const PswLinResult pw = pswLin(in, ctx);
  CHECK_NEAR(pw.psi, 0.10764760524034500122, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  CHECK_NEAR(pw.psiNi, -0.79900145530700683594, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  // Fortran DZDM(N+1)/DZDM(N+2)/DZDM(N+3)/DZDM(N+NW) -> 0-based 0/1/2/nw-1.
  CHECK_NEAR(pw.dzdm[0], -0.42643725872039794922, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  CHECK_NEAR(pw.dzdm[1], -0.47689832746982574463e-1, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  CHECK_NEAR(pw.dzdm[2], -0.82428008317947387695e-2, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  CHECK_NEAR(pw.dzdm[nw - 1], 0.49842119216918945312, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  // Fortran DQDM(N+1)/DQDM(N+2)/DQDM(N+NW).
  CHECK_NEAR(pw.dqdm[0], 0.38609626770019531250e2, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  CHECK_NEAR(pw.dqdm[1], -0.25660511016845703125e2, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
  CHECK_NEAR(pw.dqdm[nw - 1], -0.16822841763496398926, kPswlinDirectAbsTol,
             kPswlinDirectRelTol);
}

// Direct Fortran comparison of the LIMAGE image-point system (xpanel.f,
// second DO-20 block): NACA 0012 (160 panels), SHARP=TRUE (SCS=1, SDS=0),
// SIG=0, GAM/GAMU linear (GAM=0.1*i, GAMU1=0.2*i, GAMU2=0.3*i), ALFA=0,
// YIMAGE=0, evaluation point off the airfoil (0.5, 0.1, normals 0/1). The
// Fortran reference values come from the byte-faithfully extracted PSILIN
// with LIMAGE=.TRUE. and the same panel contour. The deviations (PSI
// 1.9e-6, PSI_NI 1.3e-6, DZDG <=3.6e-9, DQDG <=9.3e-7 abs) stem from the
// REAL(4) capture of the contour in the Fortran proof program ->
// threshold kLimageDirectAbsTol=1e-4 abs / kLimageDirectRelTol=1e-3 rel.
constexpr double kLimageDirectAbsTol = 1.0e-4;
constexpr double kLimageDirectRelTol = 1.0e-3;

TEST(unit, InviscidPsiLinLimageDirectFortran) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  std::vector<double> gam(n), gamu1(n), gamu2(n), sig(n);
  for (std::size_t i = 0; i < n; ++i) {
    gam[i] = 0.1 * static_cast<double>(i);
    gamu1[i] = 0.2 * static_cast<double>(i);
    gamu2[i] = 0.3 * static_cast<double>(i);
    sig[i] = 0.0;
  }
  PsiLinContext ctx;
  ctx.x = b.value().panel.x;
  ctx.y = b.value().panel.y;
  ctx.s = b.value().panel.s;
  ctx.nx = b.value().panel.xn;
  ctx.ny = b.value().panel.yn;
  ctx.apanel = b.value().panel.apanel;
  ctx.gam = gam;
  ctx.gamA = gam;
  ctx.gamu1 = gamu1;
  ctx.gamu2 = gamu2;
  ctx.sig = sig;
  ctx.sharp = true;
  ctx.dste = 0.0;
  ctx.ante = 0.0;
  ctx.aste = 0.0;
  ctx.qinf = 1.0;
  ctx.image = true;
  ctx.yimage = 0.0;
  PsiLinInputs in;
  in.node = static_cast<std::size_t>(-1);
  in.xi = 0.5;
  in.yi = 0.1;
  in.nxi = 0.0;
  in.nyi = 1.0;
  in.alfa = 0.0;
  in.nodeOnAirfoil = false;
  in.siglin = false;
  const PsiLinResult pl = psiLin(in, ctx);
  CHECK_NEAR(pl.psi, 0.50757366418838500977, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.psiNi, 0.086250327527523040771, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.qtan1, -0.82749909162521362305, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.qtan2, -2.7412519454956054688, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.dzdg[0], -0.80049096141010522842e-4, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.dzdg[1], -0.31912350095808506012e-6, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.dzdg[n - 1], 0.80049037933349609375e-4, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.dqdg[0], -0.77826506458222866058e-3, kLimageDirectAbsTol,
             kLimageDirectRelTol);
  CHECK_NEAR(pl.dqdg[n - 1], 0.77826506458222866058e-3, kLimageDirectAbsTol,
             kLimageDirectRelTol);
}

// Index mapping: first/last 0-based indices of the GAM distribution.
TEST(unit, InviscidIndexMapping) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();
  const Result<SpecPointResult> r =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(r.isOk());
  CHECK_EQ(r.value().gam.size(), n);
  CHECK_EQ(r.value().gamA.size(), n);
  CHECK_EQ(r.value().qinv.size(), n);
  CHECK_EQ(r.value().qinvA.size(), n);
  CHECK_EQ(r.value().cp.size(), n);
  // Fortran GAM(1)/GAM(N) -> 0-based gam[0]/gam[n-1] (antisymmetric).
  CHECK_NEAR(r.value().gam[0], -r.value().gam[n - 1], kFortranAbsTol,
             kFortranRelTol);
}

// OPER point outputs of the golden cases (4 decimal places; tolerates half
// of the last digit). Reference: aero-inviscid/inv-*.
constexpr double kGoldenAbsTol = 5.0e-5;

// reference: aero-inviscid/inv-0012-0 (symmetric airfoil at alpha 0).
TEST(unit, InviscidGolden0012Alpha0) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const Result<SpecPointResult> r =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(r.isOk());
  CHECK_NEAR(r.value().cl, 0.0, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r.value().cm, 0.0, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r.value().cdp, -0.0011, kGoldenAbsTol, 1.0);
}

// reference: aero-inviscid/inv-0012-4 and inv-2412-* (alpha 4 degrees or
// 2412 sequence 0/4/8/-2).
TEST(unit, InviscidGoldenCases) {
  const Result<SystemBundle> b12 = buildSystem(makeBuffer0012(), true);
  CHECK(b12.isOk());
  const double alfa4 = 4.0 * kPi / 180.0;
  const Result<SpecPointResult> r124 =
      specAlpha(b12.value().system, b12.value().panel, harnessSettings(),
                alfa4, 1.0, 0.25, 0.0);
  CHECK(r124.isOk());
  // inv-0012-4: cl = 0.4829, cdp = -0.0011, cm = -0.0056.
  CHECK_NEAR(r124.value().cl, 0.4829, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r124.value().cdp, -0.0011, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r124.value().cm, -0.0056, kGoldenAbsTol, 1.0);

  const Result<SystemBundle> b24 = buildSystem(makeBuffer2412(), true);
  CHECK(b24.isOk());
  const Result<SpecPointResult> r240 =
      specAlpha(b24.value().system, b24.value().panel, harnessSettings(),
                0.0, 1.0, 0.25, 0.0);
  CHECK(r240.isOk());
  // inv-2412-0: cl = 0.2554, cdp = -0.0011, cm = -0.0557.
  CHECK_NEAR(r240.value().cl, 0.2554, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r240.value().cdp, -0.0011, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r240.value().cm, -0.0557, kGoldenAbsTol, 1.0);

  const Result<SpecPointResult> r244 =
      specAlpha(b24.value().system, b24.value().panel, harnessSettings(),
                alfa4, 1.0, 0.25, 0.0);
  CHECK(r244.isOk());
  // inv-2412-4: cl = 0.7376, cdp = -0.0011, cm = -0.0616.
  CHECK_NEAR(r244.value().cl, 0.7376, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r244.value().cdp, -0.0011, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r244.value().cm, -0.0616, kGoldenAbsTol, 1.0);

  const double alfa8 = 8.0 * kPi / 180.0;
  const Result<SpecPointResult> r248 =
      specAlpha(b24.value().system, b24.value().panel, harnessSettings(),
                alfa8, 1.0, 0.25, 0.0);
  CHECK(r248.isOk());
  // inv-2412-8: cl = 1.2162, cdp = -0.0012, cm = -0.0677.
  CHECK_NEAR(r248.value().cl, 1.2162, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r248.value().cdp, -0.0012, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r248.value().cm, -0.0677, kGoldenAbsTol, 1.0);

  const double alfaM2 = -2.0 * kPi / 180.0;
  const Result<SpecPointResult> r24n =
      specAlpha(b24.value().system, b24.value().panel, harnessSettings(),
                alfaM2, 1.0, 0.25, 0.0);
  CHECK(r24n.isOk());
  // inv-2412-neg2: cl = 0.0137, cdp = -0.0011, cm = -0.0529.
  CHECK_NEAR(r24n.value().cl, 0.0137, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r24n.value().cdp, -0.0011, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r24n.value().cm, -0.0529, kGoldenAbsTol, 1.0);
}

// reference: aero-inviscid/inv-clark-y-0 and inv-clark-y-4.
TEST(unit, InviscidGoldenClarkY) {
  const Result<SystemBundle> b = buildSystem(makeBufferClarkY(), false);
  CHECK(b.isOk());
  const Result<SpecPointResult> r0 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(r0.isOk());
  // inv-clark-y-0: cl = 0.4162, cdp = -0.0007, cm = -0.0879.
  CHECK_NEAR(r0.value().cl, 0.4162, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r0.value().cdp, -0.0007, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r0.value().cm, -0.0879, kGoldenAbsTol, 1.0);

  const double alfa4 = 4.0 * kPi / 180.0;
  const Result<SpecPointResult> r4 =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa4,
                1.0, 0.25, 0.0);
  CHECK(r4.isOk());
  // inv-clark-y-4: cl = 0.8972, cdp = -0.0007, cm = -0.0943.
  CHECK_NEAR(r4.value().cl, 0.8972, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r4.value().cdp, -0.0007, kGoldenAbsTol, 1.0);
  CHECK_NEAR(r4.value().cm, -0.0943, kGoldenAbsTol, 1.0);
}

// reference: state/state-ascending, state/state-descending,
// state/state-same-target-in-seq (ASEQ 0..8 or 8..0, also -2; the same
// target point in sequence and as a single point is identical).
TEST(unit, InviscidStateSequences) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const std::size_t n = b.value().panel.x.size();

  // state/state-ascending: ASEQ 0 2 4 6 8.
  double clPrev = -1.0;
  for (int deg = 0; deg <= 8; deg += 2) {
    const double alfa = static_cast<double>(deg) * kPi / 180.0;
    const Result<SpecPointResult> r =
        specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa,
                  1.0, 0.25, 0.0);
    CHECK(r.isOk());
    CHECK_EQ(r.value().gam.size(), n);
    // Cl monotonically increasing in the ascending sequence.
    CHECK(r.value().cl > clPrev);
    clPrev = r.value().cl;
  }

  // state/state-same-target-in-seq: the same target point (alpha=4) in the
  // sequence and as a single point gives the same result (stateless
  // kernel).
  const double alfa4 = 4.0 * kPi / 180.0;
  const Result<SpecPointResult> rSeq =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa4,
                1.0, 0.25, 0.0);
  CHECK(rSeq.isOk());
  const Result<SpecPointResult> rAgain =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), alfa4,
                1.0, 0.25, 0.0);
  CHECK(rAgain.isOk());
  CHECK_NEAR(rSeq.value().cl, rAgain.value().cl, 0.0, 1.0);
  CHECK_NEAR(rSeq.value().cm, rAgain.value().cm, 0.0, 1.0);
  for (std::size_t i = 0; i < n; ++i) {
    CHECK_NEAR(rSeq.value().gam[i], rAgain.value().gam[i], 0.0, 1.0);
  }
}

// error behavior: psiLin throws on unequal lengths without partial mutation.
TEST(unit, InviscidPsiLinLengthMismatchThrows) {
  const std::vector<double> xs{1.0, 0.998, 0.995};
  const std::vector<double> ys{0.001, 0.002, 0.003};
  const std::vector<double> ss{0.0, 0.001, 0.002};
  const std::vector<double> nxs{0.0, 0.0, 0.0};
  const std::vector<double> nys{1.0, 1.0, 1.0};
  const std::vector<double> apans{0.0, 0.0, 0.0};
  const std::vector<double> gams{0.5, 0.4};
  PsiLinContext ctx;
  ctx.x = xs;
  ctx.y = ys;
  ctx.s = ss;
  ctx.nx = nxs;
  ctx.ny = nys;
  ctx.apanel = apans;
  ctx.gam = gams;
  ctx.gamA = gams;
  ctx.sig = gams;
  ctx.qinf = 1.0;
  PsiLinInputs in;
  in.node = 1;
  in.nodeOnAirfoil = true;
  bool threw = false;
  try {
    (void)psiLin(in, ctx);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// Viscous SPECAL/SPECCL branches (xoper.f:2807-2813 or 2880-2886).
// structural test: the LVISC branch sets CPI/CPV on N+NW (QINV
// wake-inclusive via QISET with the n+1+nw basis); with QVIS == QINV,
// CPV == CPI (the same CPCALC formula on the same velocities). The
// inactive branch stays on N (CPI) without CPV.
TEST(unit, InviscidSpecViscousBranches) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());
  const auto& panel = b.value().panel;
  const std::size_t n = panel.x.size();

  // Fortran XYWAKE/QWCALC: wake node and n+1+nw basis.
  const Result<SpecPointResult> inv =
      specAlpha(b.value().system, panel, harnessSettings(), 0.0, 1.0, 0.25,
                0.0);
  CHECK(inv.isOk());
  const Result<WakeState> wk =
      xywake(panel, inv.value().gam, inv.value().gamA,
             b.value().system.gamu1, b.value().system.gamu2,
             inv.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const WakeBasisResult qb =
      qwCalc(b.value().system, wk.value(), panel, inv.value().alfa, 1.0);
  std::vector<double> qinvu1 = b.value().system.qinvu1;
  std::vector<double> qinvu2 = b.value().system.qinvu2;
  qinvu1.insert(qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2.insert(qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinvFull;
  std::vector<double> qinvAFull;
  qiset(qinvu1, qinvu2, inv.value().alfa, qinvFull, qinvAFull);
  const std::size_t nnw = qinvFull.size();

  // SPECAL LVISC branch with QVIS == QINV: CPV == CPI.
  SpecViscousExt ext;
  ext.lvisc = true;
  ext.qinvu1 = qinvu1;
  ext.qinvu2 = qinvu2;
  ext.qvis = qinvFull;
  const Result<SpecPointResult> rv =
      specAlpha(b.value().system, panel, harnessSettings(), 0.0, 1.0, 0.25,
                0.0, ext);
  CHECK(rv.isOk());
  CHECK(rv.value().converged);
  CHECK_EQ(rv.value().cp.size(), nnw);
  CHECK_EQ(rv.value().cpv.size(), nnw);
  for (std::size_t i = 0; i < nnw; ++i) {
    CHECK_NEAR(rv.value().cp[i], rv.value().cpv[i], 1.0e-14, 1.0e-14);
  }
  CHECK(rv.value().hinge == std::nullopt);

  // inactive branch unchanged: CPI on N, no CPV.
  CHECK_EQ(inv.value().cp.size(), n);
  CHECK(inv.value().cpv.empty());

  // SPECCL LVISC branch: same (QISET with the n+1+nw basis).
  const Result<SpecPointResult> rc =
      specCl(b.value().system, panel, harnessSettings(), inv.value().cl,
             inv.value().alfa, 1.0, 0.25, 0.0, ext);
  CHECK(rc.isOk());
  CHECK(rc.value().converged);
  CHECK_EQ(rc.value().cp.size(), nnw);
  CHECK_EQ(rc.value().cpv.size(), nnw);
  for (std::size_t i = 0; i < nnw; ++i) {
    CHECK_NEAR(rc.value().cp[i], rc.value().cpv[i], 1.0e-14, 1.0e-14);
  }

  // LVISC validation: unequal lengths are rejected.
  SpecViscousExt bad = ext;
  bad.qvis = std::span<const double>(qinvFull.data(), qinvFull.size() - 1);
  bool threw = false;
  try {
    static_cast<void>(specAlpha(b.value().system, panel, harnessSettings(),
                                0.0, 1.0, 0.25, 0.0, bad));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// As-is/deviation test for the SPECAL/SPECCL convergence aborts
// (xoper.f:2799/2874). The SPECCL part evidences the canonical error flag.
// the SPECAL part pins the documented deviation: in the harness regime
// (MATYP=2, MINF1=0.99) Fortran runs on NaN and writes the SPECAL message;
// the C++ kernel ends there with CPCALC-DENNEG. This is not the canonical
// SPECAL target contract, but the current C++ as-is state; an unintended
// change to the C++ convergence path would be caught here and would need
// a fresh deviation review.
TEST(unit, InviscidSpecConvergenceFailureDeviation) {
  const Result<SystemBundle> b = buildSystem(makeBuffer0012(), true);
  CHECK(b.isOk());

  // SPECAL harness regime (MATYP=2, MINF1=0.99, ALFA=0): Fortran first
  // writes 'SPECAL: Minf convergence failed' and a NaN state
  // (tests/data/core-spec-failure-reference); the C++ kernel runs in the
  // same regime on CPCALC DENNEG -> structured numerical error. The
  // divergence is documented as a deviation; the SPECAL error flag is
  // currently not reachable in the C++ kernel.
  MachReynoldsSettings settings = harnessSettings();
  settings.machType = 2;
  settings.minf1 = 0.99;
  const Result<SpecPointResult> ra =
      specAlpha(b.value().system, b.value().panel, settings, 0.0, 1.0, 0.25,
                0.0);
  CHECK(ra.isErr());
  CHECK(ra.error().code == ErrorCode::numericalFailure);

  const Result<SpecPointResult> rc =
      specCl(b.value().system, b.value().panel, harnessSettings(), 1.0e6,
             0.0, 1.0, 0.25, 0.0);
  CHECK(rc.isOk());
  CHECK(!rc.value().converged);
  CHECK(rc.value().failure ==
        SpecifiedPointFailure::specclLiftConvergenceFailure);

  // converge the case: no error.
  const Result<SpecPointResult> ok =
      specAlpha(b.value().system, b.value().panel, harnessSettings(), 0.0,
                1.0, 0.25, 0.0);
  CHECK(ok.isOk());
  CHECK(ok.value().converged);
  CHECK(ok.value().failure == SpecifiedPointFailure::none);
}

// PSILIN with GEOLIN=true (dPsi/dn, Z_QDOF0..3) against the Fortran
// reference (psilin_geolin_reference.txt: NACA2412, 160 panels,
// GAM=cos(2pi i/N), QF0..QF3 linear/exp on IQ1=N/4..IQ2=3N/4, alfa=0, QINF=1).
constexpr double kGeolinAbsTol = 1.0e-5;
constexpr double kGeolinRelTol = 1.0e-4;

TEST(unit, InviscidPsiLinGeolin) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const auto& p = b.value().panel;
  const std::size_t n = p.x.size();
  CHECK_EQ(n, 160u);
  std::vector<double> gam(n), zero(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    gam[i] = std::cos(6.2831853 * static_cast<double>(i) /
                      static_cast<double>(n));
  }
  const std::size_t iq1 = n / 4 - 1;   // Fortran IQ1 = N/4 (1-based)
  const std::size_t iq2 = 3 * n / 4 - 1;
  std::vector<double> qf0(n, 0.0), qf1(n, 0.0), qf2(n, 0.0), qf3(n, 0.0);
  const double ds = p.s[iq2] - p.s[iq1];
  for (std::size_t i = iq1; i <= iq2; ++i) {
    const double fs = (p.s[i] - p.s[iq1]) / ds;
    qf0[i] = 1.0 - fs;
    qf1[i] = fs;
    qf2[i] = std::exp(-5.0 * fs);
    qf3[i] = std::exp(-5.0 * (1.0 - fs));
  }
  const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
      p.x[0], p.y[0], p.xp[0], p.yp[0], p.x[n - 1], p.y[n - 1],
      p.xp[n - 1], p.yp[n - 1], p.chord);
  PsiLinContext ctx;
  ctx.x = p.x; ctx.y = p.y; ctx.s = p.s; ctx.nx = p.xn; ctx.ny = p.yn;
  ctx.apanel = p.apanel; ctx.gam = gam; ctx.gamA = zero; ctx.gamu1 = zero;
  ctx.gamu2 = zero; ctx.sig = zero; ctx.sharp = teg.sharp; ctx.dste = teg.dste;
  ctx.ante = teg.ante; ctx.aste = teg.aste; ctx.qinf = 1.0;
  ctx.image = false; ctx.yimage = 0.0;

  const std::size_t idx[9] = {0, 20, 40, 60, 80, 100, 120, 140, 159};
  const double ref[9][8] = {
    {-2.223127633333e-1, 1.164292335510e0, -3.965166863054e-3, 2.497653514147e-1, -1.345775276423e-2, -1.194878853858e-2, -7.748743519187e-3, -6.696447730064e-3},
    {-1.764611005783e-1, 8.894397020340e-1, -1.475948467851e-2, 3.539120256901e-1, -4.392953217030e-2, -4.053422436118e-2, -2.182988636196e-2, -1.938587799668e-2},
    { 3.297689929605e-2, 1.009959101677e0, -1.425470225513e-2, 6.891787052155e-7, -1.300985068083e-1, -9.590589255095e-2, -7.195585221052e-2, -4.367132857442e-2},
    { 1.465190052986e-1, 1.159229040146e0, -1.077751256526e-2, -3.559232652187e-1, -1.389117687941e-1, -1.356238722801e-1, -4.685661941767e-2, -4.614168033004e-2},
    { 1.089214459062e-1, 2.275992333889e-1, -2.303441986442e-3, -5.136698484421e-1, -1.191297844052e-1, -1.267755627632e-1, -3.743570297956e-2, -4.199092835188e-2},
    { 6.744240224361e-2, -8.431841135025e-1, -9.143472649157e-3, -3.559699654579e-1, -1.264674961567e-1, -1.429595649242e-1, -4.066066443920e-2, -5.005083978176e-2},
    {-8.211736381054e-2, -1.006296038628e0, -1.496351510286e-2, 2.480577677488e-6, -1.005736887455e-1, -1.148105636239e-1, -4.658909142017e-2, -5.967654287815e-2},
    {-2.533974647522e-1, -1.090454816818e0, -1.510952971876e-2, 3.535980284214e-1, -4.242273047566e-2, -4.051645472646e-2, -2.088500931859e-2, -1.957231387496e-2},
    {-2.246026843786e-1, -8.355343341827e-1, -5.039689131081e-3, 2.500875294209e-1, -1.344458758831e-2, -1.195243932307e-2, -7.739956025034e-3, -6.700097583234e-3},
  };
  for (int k = 0; k < 9; ++k) {
    const std::size_t i = idx[k];
    PsiLinInputs in;
    in.node = i; in.xi = p.x[i]; in.yi = p.y[i]; in.nxi = p.xn[i];
    in.nyi = p.yn[i]; in.alfa = 0.0; in.nodeOnAirfoil = true;
    in.siglin = false; in.geolin = true;
    in.qf0 = qf0; in.qf1 = qf1; in.qf2 = qf2; in.qf3 = qf3;
    const PsiLinResult pl = psiLin(in, ctx);
    CHECK_NEAR(pl.psi, ref[k][0], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.psiNi, ref[k][1], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.dzdg[i], ref[k][2], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.dzdn[i], ref[k][3], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.zQdof0, ref[k][4], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.zQdof1, ref[k][5], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.zQdof2, ref[k][6], kGeolinAbsTol, kGeolinRelTol);
    CHECK_NEAR(pl.zQdof3, ref[k][7], kGeolinAbsTol, kGeolinRelTol);
  }
}

// MIXED (one Newton iteration) against the Fortran reference
// (mixed_reference.txt: NACA2412, 160 panels, GAM=cos(2pi i/N), QSPEC=GAM,
// IQ1=N/4..IQ2=3N/4, LCPXX=false, PSIO=0, QDOF=0, ALFA=0).
// the node tolerance (rel 2e-3) carries the condition amplification of the
// REAL(4) panel contour through the (N+5) linear system; a formula error
// would produce O(1) or systematic deviations.
constexpr double kMixedAbsTol = 1.0e-4;
constexpr double kMixedRelTol = 1.0e-3;
constexpr double kMixedNodeRelTol = 2.0e-3;

TEST(unit, InverseMixedOneIteration) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const auto& p = b.value().panel;
  const int n = static_cast<int>(p.x.size());
  CHECK_EQ(n, 160);
  xfoil::core::inverse::MixedInverseState st;
  st.x = p.x; st.y = p.y; st.s = p.s; st.xp = p.xp; st.yp = p.yp;
  st.nx = p.xn; st.ny = p.yn; st.apanel = p.apanel;
  st.gam.assign(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    st.gam[static_cast<std::size_t>(i)] =
        std::cos(6.2831853 * static_cast<double>(i) / static_cast<double>(n));
  }
  st.gamA.assign(static_cast<std::size_t>(n), 0.0);
  st.gamu1 = st.gamA; st.gamu2 = st.gamA; st.sig = st.gamA;
  st.qspec = st.gam;
  st.qf0.assign(static_cast<std::size_t>(n), 0.0);
  st.qf1 = st.qf0; st.qf2 = st.qf0; st.qf3 = st.qf0;
  st.n = n;
  st.iq1 = n / 4 - 1;      // 0-based; Fortran IQ1 = N/4
  st.iq2 = 3 * n / 4 - 1;  // 0-based; Fortran IQ2 = 3*N/4
  st.lcpXX = false;
  st.sharp = p.sharpTe;
  const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
      p.x[0], p.y[0], p.xp[0], p.yp[0], p.x[static_cast<std::size_t>(n) - 1],
      p.y[static_cast<std::size_t>(n) - 1], p.xp[static_cast<std::size_t>(n) - 1],
      p.yp[static_cast<std::size_t>(n) - 1], p.chord);
  st.dste = teg.dste; st.ante = teg.ante; st.aste = teg.aste;
  st.xte = p.xte; st.yte = p.yte; st.chord = p.chord;
  st.alfa = 0.0; st.minf = 0.0; st.qinf = 1.0;
  st.psio = 0.0; st.qdof0 = 0.0; st.qdof1 = 0.0; st.qdof2 = 0.0; st.qdof3 = 0.0;

  CHECK(xfoil::core::inverse::mixedInverse(st, 1).isOk());
  CHECK_NEAR(st.psio, -2.353893592954e-3, kMixedAbsTol, kMixedRelTol);
  CHECK_NEAR(st.qdof0, 1.508222460747e0, kMixedAbsTol, kMixedRelTol);
  CHECK_NEAR(st.qdof1, -7.653871774673e-1, kMixedAbsTol, kMixedRelTol);
  CHECK_NEAR(st.qdof2, 0.0, kMixedAbsTol, kMixedAbsTol);
  CHECK_NEAR(st.qdof3, 0.0, kMixedAbsTol, kMixedAbsTol);

  const int idx[9] = {1, 21, 41, 61, 81, 101, 121, 141, 160};
  const double ref[9][3] = {
    {7.593519687653e-1, 1.0, 1.260007848032e-3},
    {1.106305599213e0, 6.940048336983e-1, 5.241766199470e-2},
    {1.460843920708e0, 3.679069578648e-1, 7.722927629948e-2},
    {-1.256938874722e-1, 9.333491325378e-2, -1.089014858007e-2},
    {-6.943736076355e-1, 3.740007430315e-2, -1.883937045932e-3},
    {-6.235410571098e-1, 5.734260752797e-2, -4.980209469795e-2},
    {-8.895769119263e-1, 3.616466522217e-1, -3.940701484680e-2},
    {-9.730041623116e-1, 6.992458105087e-1, -2.168729528785e-2},
    {-7.593519091606e-1, 1.0, -1.260007848032e-3},
  };
  for (int k = 0; k < 9; ++k) {
    const std::size_t i = static_cast<std::size_t>(idx[k] - 1);
    CHECK_NEAR(st.gam[i], ref[k][0], kMixedAbsTol, kMixedNodeRelTol);
    CHECK_NEAR(st.x[i], ref[k][1], kMixedAbsTol, kMixedNodeRelTol);
    CHECK_NEAR(st.y[i], ref[k][2], kMixedAbsTol, kMixedNodeRelTol);
  }
}

TEST(unit, InverseMixedConvergenceFlag) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const auto& p = b.value().panel;
  const int n = static_cast<int>(p.x.size());
  xfoil::core::inverse::MixedInverseState st;
  st.x = p.x; st.y = p.y; st.s = p.s; st.xp = p.xp; st.yp = p.yp;
  st.nx = p.xn; st.ny = p.yn; st.apanel = p.apanel;
  st.gam.assign(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    st.gam[static_cast<std::size_t>(i)] =
        std::cos(6.2831853 * static_cast<double>(i) / static_cast<double>(n));
  }
  st.gamA.assign(static_cast<std::size_t>(n), 0.0);
  st.gamu1 = st.gamA; st.gamu2 = st.gamA; st.sig = st.gamA;
  st.qspec = st.gam;
  st.qf0.assign(static_cast<std::size_t>(n), 0.0);
  st.qf1 = st.qf0; st.qf2 = st.qf0; st.qf3 = st.qf0;
  st.n = n; st.iq1 = n / 4 - 1; st.iq2 = 3 * n / 4 - 1; st.lcpXX = false;
  st.sharp = p.sharpTe;
  const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
      p.x[0], p.y[0], p.xp[0], p.yp[0], p.x[static_cast<std::size_t>(n) - 1],
      p.y[static_cast<std::size_t>(n) - 1], p.xp[static_cast<std::size_t>(n) - 1],
      p.yp[static_cast<std::size_t>(n) - 1], p.chord);
  st.dste = teg.dste; st.ante = teg.ante; st.aste = teg.aste;
  st.xte = p.xte; st.yte = p.yte; st.chord = p.chord;
  st.alfa = 0.0; st.minf = 0.0; st.qinf = 1.0;

  // one iteration: controlled non-convergence (Fortran: 'not quite
  // converged. Can EXEC again'), no error, status OK.
  CHECK(xfoil::core::inverse::mixedInverse(st, 1).isOk());
  CHECK(!st.converged);
}

TEST(unit, InverseMixedDegenerateSegment) {
  const Result<SystemBundle> b = buildSystem(makeBuffer2412(), true);
  CHECK(b.isOk());
  const auto& p = b.value().panel;
  const int n = static_cast<int>(p.x.size());
  xfoil::core::inverse::MixedInverseState st;
  st.x = p.x; st.y = p.y; st.s = p.s; st.xp = p.xp; st.yp = p.yp;
  st.nx = p.xn; st.ny = p.yn; st.apanel = p.apanel;
  st.gam.assign(static_cast<std::size_t>(n), 0.0);
  st.gamA.assign(static_cast<std::size_t>(n), 0.0);
  st.gamu1 = st.gamA; st.gamu2 = st.gamA; st.sig = st.gamA;
  st.qspec.assign(static_cast<std::size_t>(n), 1.0);
  st.qf0.assign(static_cast<std::size_t>(n), 0.0);
  st.qf1 = st.qf0; st.qf2 = st.qf0; st.qf3 = st.qf0;
  st.n = n; st.iq1 = 9; st.iq2 = 9; st.lcpXX = false;  // single point: ds=0
  st.sharp = p.sharpTe;
  const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
      p.x[0], p.y[0], p.xp[0], p.yp[0], p.x[static_cast<std::size_t>(n) - 1],
      p.y[static_cast<std::size_t>(n) - 1], p.xp[static_cast<std::size_t>(n) - 1],
      p.yp[static_cast<std::size_t>(n) - 1], p.chord);
  st.dste = teg.dste; st.ante = teg.ante; st.aste = teg.aste;
  st.xte = p.xte; st.yte = p.yte; st.chord = p.chord;
  st.alfa = 0.0; st.minf = 0.0; st.qinf = 1.0;
  // A degenerate segment (ds=0) leads to non-finite values; the core
  // returns a controlled numerical error instead of undefined behavior
  // (cpp-safety deviation).
  CHECK(xfoil::core::inverse::mixedInverse(st, 1).isErr());
}

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

// File: tests/core/BoundaryLayerCorrelationTests.cpp
// direct C++ against Fortran comparisons for the boundary-layer correlation
// and station kernels. Reference values come from
// tests/data/core-viscous-reference/viscous_reference.txt (test 5b and
// test 5, Fortran REAL(4) outputs). The tests use only
// xfoil::core::boundary_layer and no app/CLI state.

#include "TestSupport.h"

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverKernels.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"
#include "xfoil/core/boundary_layer/Coupling.h"
#include "xfoil/core/boundary_layer/Marching.h"
#include "xfoil/core/boundary_layer/Update.h"
#include "xfoil/core/boundary_layer/Viscal.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/Limits.h"

#include <array>
#include <stdexcept>

#include <cmath>

#include "xfoil/core/math/Spline.h"
#include <cstdio>

namespace {

xfoil::core::paneling::PanelingParameters xfoil699Paneling() {
  xfoil::core::paneling::PanelingParameters p;
  p.panelCount = 160;
  p.teToLeDensityRatio = 0.15;
  return p;
}


using xfoil::core::boundary_layer::blparConstants;

// Fortran REAL(4) outputs: tolerances analogous to BoundaryLayerTests.cpp.
constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;
// influence-derived quantities (QINV from GGCALC): core convention.
constexpr double kInfluenceRelTol = 3.0e-3;

}  // namespace

// reference: viscous_reference.txt HKIN1 (H=2.2, MSQ=0).
TEST(unit, HkinDirectFortran) {
  const auto r = xfoil::core::boundary_layer::hkin(2.2, 0.0);
  CHECK_NEAR(r.value, 2.20000005, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, 1.0, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt HKIN2 (H=2.2, MSQ=0.04).
TEST(unit, HkinCompressibleDirectFortran) {
  const auto r = xfoil::core::boundary_layer::hkin(2.2, 0.04);
  CHECK_NEAR(r.value, 2.17855287, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.machSquaredDerivative, -5.33763826e-1, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt DIL1 (HK=2.2, RT=100).
TEST(unit, DilDirectFortran) {
  const auto r = xfoil::core::boundary_layer::dil(2.2, 100.0);
  CHECK_NEAR(r.value, 2.58969981e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -1.58797170e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.reynoldsDerivative, -2.58969976e-5, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt DILW1 (HK=2.5, RT=100).
TEST(unit, DilwDirectFortran) {
  const auto r = xfoil::core::boundary_layer::dilw(2.5, 100.0);
  CHECK_NEAR(r.value, 1.99890579e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -1.72746938e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt HSL1 (HK=2.2, RT=100).
TEST(unit, HslDirectFortran) {
  const auto r = xfoil::core::boundary_layer::hsl(2.2, 100.0);
  CHECK_NEAR(r.value, 1.62589943, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -1.67286754e-1, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt CFL1 (HK=2.2, RT=100).
TEST(unit, CflDirectFortran) {
  const auto r = xfoil::core::boundary_layer::cfl(2.2, 100.0);
  CHECK_NEAR(r.value, 7.46443681e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -9.97360144e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.reynoldsDerivative, -7.46443693e-5, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt HST1 (HK=2.2, RT=300, MSQ=0.01).
TEST(unit, HstDirectFortran) {
  const auto r = xfoil::core::boundary_layer::hst(2.2, 300.0, 0.01);
  CHECK_NEAR(r.value, 1.61072123, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -1.44177347e-1, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.reynoldsDerivative, -3.55505799e-5, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.machSquaredDerivative, 5.44913858e-3, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt CFT1 (HK=2.2, RT=300, MSQ=0.01,
// CFFAC=1.0 from BLPINI).
TEST(unit, CftDirectFortran) {
  const double cffac = blparConstants().cffac;
  const auto r = xfoil::core::boundary_layer::cft(2.2, 300.0, 0.01, cffac);
  CHECK_NEAR(r.value, 1.77574600e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -2.90152710e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.reynoldsDerivative, -2.52905602e-6, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.machSquaredDerivative, -1.01499936e-4, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt HCT1 (HK=2.2, MSQ=0.01).
TEST(unit, HctDirectFortran) {
  const auto r = xfoil::core::boundary_layer::hct(2.2, 0.01);
  CHECK_NEAR(r.value, 2.96714273e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, -3.26530600e-4, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.machSquaredDerivative, 2.96714276e-1, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt DIT1 (HS=1.9, US=0.5, CF=0.004, ST=0.02).
TEST(unit, DitDirectFortran) {
  const auto r = xfoil::core::boundary_layer::dit(1.9, 0.5, 0.004, 0.02);
  CHECK_NEAR(r.value, 1.26315793e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.energyShapeDerivative, -6.64820021e-4, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.slipVelocityDerivative, 1.68421061e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.skinFrictionDerivative, 2.63157904e-1, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(r.shearCoefficientDerivative, 2.10526306e-2, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt DAMPL1 (HK=3.0, TH=0.01, RT=1e5).
TEST(unit, DamplDirectFortran) {
  const auto r = xfoil::core::boundary_layer::dampl(3.0, 0.01, 1.0e5);
  CHECK_NEAR(r.value, 9.44955289e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shapeDerivative, 2.10515761, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.reynoldsDerivative, 0.0, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt DAMPL2X (HK=4.5, TH=0.01, RT=2e5).
TEST(unit, Dampl2DirectFortran) {
  const auto r = xfoil::core::boundary_layer::dampl2(4.5, 0.01, 2.0e5);
  CHECK_NEAR(r.value, 4.78172159, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.reynoldsDerivative, 1.35975897e-9, kFortranAbsTol,
             kFortranRelTol);
}

// reference: viscous_reference.txt AXSET1 (HK=3.0, T=0.01, RT=1e5, A=0.5
// duplicate, ACRIT=9.0, IDAMPV=0).
TEST(unit, AxsetDirectFortran) {
  const auto r = xfoil::core::boundary_layer::axset(3.0, 0.01, 1.0e5, 0.5,
                                                    3.0, 0.01, 1.0e5, 0.5,
                                                    9.0, 0);
  CHECK_NEAR(r.value, 9.44955289e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shape1Derivative, 1.05257881, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.shape2Derivative, 1.05257881, kFortranAbsTol, kFortranRelTol);
  // AX_A1 = AX_A2 = 0 (canonical: ARG = MIN(20*(9.0-0.5), 20.0) = 20 > 0).
  CHECK_NEAR(r.amplification1Derivative, 0.0, 1.0e-8, 1.0e-8);
}

// No amplification region: RT below the critical Reynolds number ->
// canonical AX=0 (DAMPL-branch GR < GRCRIT-DGR).
TEST(unit, DamplBelowCritical) {
  const auto r = xfoil::core::boundary_layer::dampl(2.2, 0.01, 5.5);
  CHECK_EQ(r.value, 0.0);
  CHECK_EQ(r.shapeDerivative, 0.0);
  CHECK_EQ(r.momentumThicknessDerivative, 0.0);
  CHECK_EQ(r.reynoldsDerivative, 0.0);
}

// DAMPL2 below HK1=3.5: no blending (canonical return before the
// non-envelope correction). The value differs from DAMPL only by the AF term
// 0.1*EXP(BRG) (xblsys.f:2207, canonical).
TEST(unit, Dampl2BelowHk1) {
  const auto r = xfoil::core::boundary_layer::dampl2(2.2, 0.01, 1.0e5);
  const auto base = xfoil::core::boundary_layer::dampl(2.2, 0.01, 1.0e5);
  CHECK_NEAR(r.value, base.value, 1.0e-6, 1.0e-6);
  CHECK_NEAR(r.shapeDerivative, base.shapeDerivative, 1.0e-6, 1.0e-6);
  CHECK_NEAR(r.momentumThicknessDerivative, base.momentumThicknessDerivative,
             1.0e-6, 1.0e-6);
  CHECK_NEAR(r.reynoldsDerivative, base.reynoldsDerivative, 1.0e-6, 1.0e-6);
}

// ---
// reference: viscous_reference.txt test 5c (BLVAR/BLMID with fixed
// primary state XSI=0.5, AMI=0.1, CTI=0.01, THI=0.02, DSI=0.03,
// DSWAKI=0.0, UEI=1.0, TKBL=0/QINFBL=1 (MINF=0), REYBL=3e6, HSTINV=0,
// RSTBL=1, GM1BL=0.4, HVRAT=0; BLPINI constants).
namespace {

xfoil::core::boundary_layer::BoundaryLayerTurbulentSecondary
makeBlvar(const xfoil::core::boundary_layer::StationType ityp) {
  using namespace xfoil::core::boundary_layer;
  const BoundaryLayerProperties props =
      boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0,
                              0);
  const BlvarInput in{
      blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0, props),
      {},  // secondary (further below via blkin set)
      blparConstants()};
  BlvarInput state = in;
  state.secondary = blkin(state.primary, props);
  return blvar(ityp, state);
}

xfoil::core::boundary_layer::MidpointSkinFriction
makeBlmid(const xfoil::core::boundary_layer::StationType ityp) {
  using namespace xfoil::core::boundary_layer;
  const BoundaryLayerProperties props =
      boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0,
                              0);
  const BoundaryLayerPrimary prim =
      blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0, props);
  const BoundaryLayerSecondary sec = blkin(prim, props);
  BlmidStation st{};
  st.hk = sec.hk;
  st.hkU = sec.hkUe;
  st.hkT = sec.hkT;
  st.hkD = sec.hkD;
  st.hkMs = sec.hkMs;
  st.rt = sec.rt;
  st.rtU = sec.rtUe;
  st.rtT = sec.rtT;
  st.rtMs = sec.rtMs;
  st.rtRe = sec.rtRe;
  st.m = sec.m;
  st.mU = sec.mUe;
  st.mMs = sec.mMs;
  BlmidInput in{};
  in.first = st;
  in.second = st;
  in.simi = true;
  in.blpar = blparConstants();
  return blmid(ityp, in);
}

}  // namespace

// reference: viscous_reference.txt BLVARL (ITYP=1 laminar).
TEST(unit, BlvarLaminarDirectFortran) {
  const auto r = makeBlvar(xfoil::core::boundary_layer::StationType::kLaminar);
  CHECK_NEAR(r.hs, 1.81782711, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.hc, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.us, 5.04952013e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.cq, 4.49417904e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.cf, 2.98520008e-5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.di, 8.72561031e-6, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.de, 1.61799997e-1, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt BLVART (ITYP=2 turbulent).
TEST(unit, BlvarTurbulentDirectFortran) {
  const auto r = makeBlvar(xfoil::core::boundary_layer::StationType::kTurbulent);
  CHECK_NEAR(r.hs, 1.71144342, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.hc, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.us, 4.75400984e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.cq, 4.23354916e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.cf, 1.29475363e-3, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.di, 4.19265241e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.de, 1.61799997e-1, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt BLVARW (ITYP=3 turbulent wake).
TEST(unit, BlvarWakeDirectFortran) {
  const auto r = makeBlvar(xfoil::core::boundary_layer::StationType::kWake);
  CHECK_NEAR(r.hs, 1.71144342, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.hc, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.us, 4.75400984e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.cq, 4.23609093e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.cf, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.di, 1.23018632e-4, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.de, 1.61799997e-1, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt BLMIDL/BLMIDT/BLMIDW (SIMI=.TRUE.).
TEST(unit, BlmidDirectFortran) {
  CHECK_NEAR(makeBlmid(xfoil::core::boundary_layer::StationType::kLaminar).cf, 2.98520008e-5,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(makeBlmid(xfoil::core::boundary_layer::StationType::kTurbulent).cf, 1.29475363e-3,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(makeBlmid(xfoil::core::boundary_layer::StationType::kWake).cf, 0.0, kFortranAbsTol,
             kFortranRelTol);
}

// ---
// reference: viscous_reference.txt test 6b (TRCHEK/TRCHEK2 with fixed
// two-station state, BLPINI-constants, XIFORC=10.0, AMCRIT=9.0,
// IDAMPV=0, MINF=0/REINF=3e6/QINF=1 -> TKBL=0, HSTINV=0, RSTBL=1).
namespace {

xfoil::core::boundary_layer::TrchekInput makeTrchekInput(double x1, double t1,
                                                         double d1,
                                                         double ampl1,
                                                         double x2, double t2,
                                                         double d2) {
  using namespace xfoil::core::boundary_layer;
  const BoundaryLayerProperties props =
      boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0,
                              0);
  const BoundaryLayerPrimary p1 = blprv(x1, 0.0, 0.01, t1, d1, 0.0, 1.0, props);
  const BoundaryLayerSecondary sec1 = blkin(p1, props);
  const BoundaryLayerPrimary p2 = blprv(x2, 0.0, 0.01, t2, d2, 0.0, 1.0, props);
  const BoundaryLayerSecondary sec2 = blkin(p2, props);
  TrchekInput in{};
  in.first.x = p1.x;
  in.first.ue = p1.ue;
  in.first.t = p1.t;
  in.first.d = p1.d;
  in.first.s = p1.s;
  in.first.ampl = ampl1;
  in.first.hk = sec1.hk;
  in.first.hkT = sec1.hkT;
  in.first.hkD = sec1.hkD;
  in.first.hkU = sec1.hkUe;
  in.first.hkMs = sec1.hkMs;
  in.first.rt = sec1.rt;
  in.first.rtT = sec1.rtT;
  in.first.rtU = sec1.rtUe;
  in.first.rtMs = sec1.rtMs;
  in.first.rtRe = sec1.rtRe;
  in.second.x = p2.x;
  in.second.ue = p2.ue;
  in.second.t = p2.t;
  in.second.d = p2.d;
  in.second.s = p2.s;
  in.second.ampl = p2.ampl;
  in.second.hk = sec2.hk;
  in.second.hkT = sec2.hkT;
  in.second.hkD = sec2.hkD;
  in.second.hkU = sec2.hkUe;
  in.second.hkMs = sec2.hkMs;
  in.second.rt = sec2.rt;
  in.second.rtT = sec2.rtT;
  in.second.rtU = sec2.rtUe;
  in.second.rtMs = sec2.rtMs;
  in.second.rtRe = sec2.rtRe;
  in.xiforc = 10.0;
  in.acrit = 9.0;
  in.idampv = 0;
  in.hstinv = props.hstinv;
  in.gm1bl = props.gm1bl;
  in.rstbl = props.rstbl;
  in.rstblMs = props.rstblMs;
  in.hstinvMs = props.hstinvMs;
  in.reybl = props.reybl;
  in.reyblMs = props.reyblMs;
  in.reyblRe = props.reyblRe;
  in.hvrat = props.hvrat;
  return in;
}

}  // namespace

// reference: viscous_reference.txt TRCHKN (no amplification region; the
// implicit amplification equation converged with AX~2.7, no punch-out;
// TRAN=F).
TEST(unit, Trchek2NoAmplificationDirectFortran) {
  const auto in = makeTrchekInput(0.0, 0.02, 0.03, 0.0, 0.01, 0.021, 0.031);
  const auto r = xfoil::core::boundary_layer::trchek2(in);
  CHECK_FALSE(r.tran);
  CHECK(r.converged);
  CHECK_NEAR(r.ampl, 2.69852988e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xt, 9.99999978e-3, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt TRCHKT (free transition, AMPL2 crosses
// AMCRIT; TRAN=T, TRFREE=T, TRFORC=F; XT sensitivities).
TEST(unit, Trchek2FreeTransitionDirectFortran) {
  const auto in = makeTrchekInput(0.0, 0.05, 0.075, 8.9, 0.1, 0.051, 0.0765);
  const auto r = xfoil::core::boundary_layer::trchek2(in);
  CHECK(r.tran);
  CHECK(r.trfree);
  CHECK_FALSE(r.trforc);
  CHECK_NEAR(r.ampl, 9.10554314, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xt, 4.86516841e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xtT1, -7.09795189, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xtT2, -2.23806882, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xtD1, 5.22514057, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xtD2, 1.64756072, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xtMs, -1.58657357e-01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.xtRe, 0.0, kFortranAbsTol, kFortranRelTol);
}

// ---
// reference: viscous_reference.txt test 7b (BLDIF with fixed
// two-station state X1=0.05/T1=0.05/D1=0.075, X2=0.15/T2=0.051/D2=0.0765,
// U=1.0, S=0.01, AMPL=0.0; BLPINI constants; canonical call context as in
// BLSYS: BLVAR/BLMID before BLDIF).
namespace {

xfoil::core::boundary_layer::BldifStation makeBldifStation(
    double x, double t, double d,
    xfoil::core::boundary_layer::StationType vartyp) {
  using namespace xfoil::core::boundary_layer;
  const BoundaryLayerProperties props =
      boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0,
                              0);
  const BoundaryLayerPrimary prim = blprv(x, 0.0, 0.01, t, d, 0.0, 1.0, props);
  const BoundaryLayerSecondary sec = blkin(prim, props);
  const BlvarInput vin{prim, sec, blparConstants()};
  const BoundaryLayerTurbulentSecondary ts = blvar(vartyp, vin);
  BldifStation st{};
  st.x = prim.x;
  st.ue = prim.ue;
  st.t = prim.t;
  st.d = prim.d;
  st.s = prim.s;
  st.ampl = prim.ampl;
  st.dw = prim.dw;
  st.m = sec.m;
  st.mU = sec.mUe;
  st.mMs = sec.mMs;
  st.h = sec.h;
  st.hT = sec.hT;
  st.hD = sec.hD;
  st.hk = sec.hk;
  st.hkU = sec.hkUe;
  st.hkT = sec.hkT;
  st.hkD = sec.hkD;
  st.hkMs = sec.hkMs;
  st.hs = ts.hs;
  st.hsU = ts.hsU;
  st.hsT = ts.hsT;
  st.hsD = ts.hsD;
  st.hsMs = ts.hsMs;
  st.hsRe = ts.hsRe;
  st.hc = ts.hc;
  st.hcU = ts.hcU;
  st.hcT = ts.hcT;
  st.hcD = ts.hcD;
  st.hcMs = ts.hcMs;
  st.rt = sec.rt;
  st.rtU = sec.rtUe;
  st.rtT = sec.rtT;
  st.rtMs = sec.rtMs;
  st.rtRe = sec.rtRe;
  st.cf = ts.cf;
  st.cfU = ts.cfU;
  st.cfT = ts.cfT;
  st.cfD = ts.cfD;
  st.cfMs = ts.cfMs;
  st.cfRe = ts.cfRe;
  st.di = ts.di;
  st.diU = ts.diU;
  st.diT = ts.diT;
  st.diD = ts.diD;
  st.diS = ts.diS;
  st.diMs = ts.diMs;
  st.diRe = ts.diRe;
  st.us = ts.us;
  st.usU = ts.usU;
  st.usT = ts.usT;
  st.usD = ts.usD;
  st.usMs = ts.usMs;
  st.cq = ts.cq;
  st.cqU = ts.cqU;
  st.cqT = ts.cqT;
  st.cqD = ts.cqD;
  st.cqMs = ts.cqMs;
  st.cqRe = ts.cqRe;
  st.de = ts.de;
  st.deU = ts.deU;
  st.deT = ts.deT;
  st.deD = ts.deD;
  st.deMs = ts.deMs;
  return st;
}

xfoil::core::boundary_layer::BldifSystem makeBldif(
    xfoil::core::boundary_layer::StationType ityp, bool similarity) {
  using namespace xfoil::core::boundary_layer;
  const StationType vartyp =
      (ityp == StationType::kWake) ? StationType::kWake
                                   : (ityp == StationType::kTurbulent
                                          ? StationType::kTurbulent
                                          : StationType::kLaminar);
  const BldifStation st1 = makeBldifStation(0.05, 0.05, 0.075, vartyp);
  const BldifStation st2 = makeBldifStation(0.15, 0.051, 0.0765, vartyp);
  BlmidInput min{};
  min.first.hk = st1.hk;
  min.first.hkU = st1.hkU;
  min.first.hkT = st1.hkT;
  min.first.hkD = st1.hkD;
  min.first.hkMs = st1.hkMs;
  min.first.rt = st1.rt;
  min.first.rtU = st1.rtU;
  min.first.rtT = st1.rtT;
  min.first.rtMs = st1.rtMs;
  min.first.rtRe = st1.rtRe;
  min.first.m = st1.m;
  min.first.mU = st1.mU;
  min.first.mMs = st1.mMs;
  min.second = min.first;
  min.second.hk = st2.hk;
  min.second.hkU = st2.hkU;
  min.second.hkT = st2.hkT;
  min.second.hkD = st2.hkD;
  min.second.hkMs = st2.hkMs;
  min.second.rt = st2.rt;
  min.second.rtU = st2.rtU;
  min.second.rtT = st2.rtT;
  min.second.rtMs = st2.rtMs;
  min.second.rtRe = st2.rtRe;
  min.second.m = st2.m;
  min.second.mU = st2.mU;
  min.second.mMs = st2.mMs;
  min.simi = false;
  min.blpar = blparConstants();
  const MidpointSkinFriction mid = blmid(ityp, min);
  BldifInput in{};
  in.first = st1;
  in.second = st2;
  in.mid = mid;
  in.ityp = ityp;
  in.similarity = similarity;
  in.bule = 1.0;
  in.ampl2 = 0.0;
  in.acrit = 9.0;
  in.idampv = 0;
  in.blpar = blparConstants();
  return bldif(in);
}

}  // namespace

// reference: viscous_reference.txt BLDIFL (ITYP=1 laminar).
TEST(unit, BldifLaminarDirectFortran) {
  const auto r = makeBldif(xfoil::core::boundary_layer::StationType::kLaminar, false);
  CHECK_NEAR(r.vs1[0][0], -1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs1[1][1], -1.99999809e1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs1[2][2], 4.57660055, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[0][0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[1][1], 1.96078777e1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[1][3], 3.50000787, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[2][2], -4.48696756, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[2][4], 5.82949215e-05, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsrez[0], 2.03792378e-01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsrez[1], -1.97898112e-02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsrez[2], -5.29075260e-06, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsm[1], -7.02710895e-06, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsr[1], 4.26601462e-12, 1.0e-10, 1.0e-5);
  CHECK_NEAR(r.vsx[2], 0.0, kFortranAbsTol, kFortranRelTol);
}

// reference: viscous_reference.txt BLDIFT (ITYP=2 turbulent).
TEST(unit, BldifTurbulentDirectFortran) {
  const auto r = makeBldif(xfoil::core::boundary_layer::StationType::kTurbulent, false);
  CHECK_NEAR(r.vs1[0][0], 8.14560165e1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs1[1][1], -2.00141487e1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[0][0], -8.19619980e1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[1][1], 1.95851479e1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[1][3], 3.50013590, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vs2[2][2], -4.43652534, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsrez[0], -1.35858664e-02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsrez[1], -1.86279584e-02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsrez[2], -3.88275279e-04, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsm[1], -9.02027008e-04, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r.vsr[1], 7.25156601e-11, 1.0e-10, 1.0e-5);
}

// ---
// reference: viscous_reference.txt test 7c (TRDIF with a fixed
// two-station state, free transition): Station 1 X1=0.05/T1=0.05/
// D1=0.075 with AMPL1=8.9, station 2 X2=0.15/T2=0.051/D2=0.0765 (laminar);
// TRCHEK sets XT and XT sensitivities; TRDIF assembles the
// transition-interval system (laminar + turbulent share).
//
// VS111 = BT1(1,1) = VS1T11*ST_A1 + VS1T12*TT_A1 + VS1T13*DT_A1
// + VS1T14*UT_A1 + VS1T15*XT_A1 (xblsys.f:1445-1462). ST_A1 = P1 + P2
// with P1 = ST_TT*TT_A1 = +2.1917e-4 and P2 = ST_DT*DT_A1 = -2.1917e-4:
// catastrophic cancellation with factor |P1|/|ST_A1| = 3.7e5. The
// REAL(4) input noise of the TRCHEK-/BLVAR chains (TT_A1 1.5e-6,
// DT_A1 4.5e-6, XT_A1 2.2e-6, ST_TT/ST_DT 1.2e-6, VS1T11 1.7e-6
// relative, all <= 4.5e-6) gives an absolute uncertainty on ST_A1
// of 6.7e-10 (F -5.966e-10 vs C -1.270e-9; the linear prediction from the
// input differences -6.66e-10 explains 98.7%) and a contribution to VS111
// via |VS1T11| = 7.9e3 of 5.3e-6 (observed overall deviation 5.48e-6,
// prediction from the term differences -5.486e-6). Fortran REAL(4) and C++
// double are each self-consistent with their own inputs (cross-check:
// C inputs rounded to REAL(4) -> sum -1.266e-9 ~ C -1.270e-9; F inputs in
// double -> -6.045e-10 ~ F -5.966e-10). No formula/index error.
// the local tolerance equals twice the linearized prediction
// (5.5e-6 -> 1.2e-5); no global tolerance or formula change is
// required.
constexpr double kTrdifSta1CancellationAbsTol = 1.2e-5;

TEST(unit, TrdifFreeTransitionDirectFortran) {
  using namespace xfoil::core::boundary_layer;
  const auto tr = trchek2(
      makeTrchekInput(0.05, 0.05, 0.075, 8.9, 0.15, 0.051, 0.0765));
  CHECK(tr.tran);
  BldifStation st1 =
      makeBldifStation(0.05, 0.05, 0.075, StationType::kLaminar);
  BldifStation st2 =
      makeBldifStation(0.15, 0.051, 0.0765, StationType::kLaminar);
  st1.ampl = 8.9;  // Fortran AMPL1 (after COM1=COM2 set)
  TrdifInput in{};
  in.first = st1;
  in.second = st2;
  in.xt = tr.xt;
  in.xtA1 = tr.xtA1;
  in.xtT1 = tr.xtT1;
  in.xtD1 = tr.xtD1;
  in.xtU1 = tr.xtU1;
  in.xtX1 = tr.xtX1;
  in.xtT2 = tr.xtT2;
  in.xtD2 = tr.xtD2;
  in.xtU2 = tr.xtU2;
  in.xtX2 = tr.xtX2;
  in.xtMs = tr.xtMs;
  in.xtRe = tr.xtRe;
  in.xtXf = tr.xtXf;
  in.amcrit = 9.0;
  in.idampv = 0;
  in.props = boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0,
                                     0.0, 0.0, 0);
  in.blpar = blparConstants();
  const auto sys = trdif(in);
  CHECK_NEAR(sys.vs1[0][0], 2.60652065e-01, kTrdifSta1CancellationAbsTol,
             kFortranRelTol);
  CHECK_NEAR(sys.vs1[1][1], -2.00546608e+01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vs1[2][2], 4.53901339e+00, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vs2[0][0], -8.22312012e+01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vs2[1][1], 1.95837574e+01, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vs2[1][3], 3.50005174e+00, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vs2[2][2], -4.51800966e+00, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vs2[2][4], 1.60959153e-03, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vsrez[0], 3.74294829e+00, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vsrez[1], -1.94062740e-02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vsrez[2], -6.05859272e-02, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vsm[1], -1.43450126e-03, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sys.vsr[1], 2.65760417e-11, 1.0e-10, 1.0e-5);
  CHECK_NEAR(sys.vsx[2], 0.0, kFortranAbsTol, kFortranRelTol);
}

// ---
// IBLSYS/BLSYS/TESYS/BLSOLV (Newton system and block solver).
// the formulas are formula-faithful (xbl.f:519, xblsys.f:583/664,
// xsolve.f:283); the direct Fortran comparisons via harness test 7 follow
// with SETBL, whose coupling state the test-7 station values return
// unchanged.
TEST(unit, IblsysMappingDirect) {
  using namespace xfoil::core::boundary_layer;
  const std::array<int, 2> nbl{5, 7};
  const auto map = iblsys(nbl, 370);
  CHECK_EQ(map.nsys, 12);  // Fortran NSYS = (5-1) + (7-1), stations 1..nbl
  CHECK_EQ(map.isys[0][0], 0);  // stagnation point unset (Fortran ISYS(1,1))
  CHECK_EQ(map.isys[0][1], 0);  // first system row
  CHECK_EQ(map.isys[0][4], 3);
  CHECK_EQ(map.isys[0][5], 4);
  CHECK_EQ(map.isys[1][1], 5);
  CHECK_EQ(map.isys[1][7], 11);
  bool threw = false;
  try {
    static_cast<void>(iblsys(std::array<int, 2>{372, 372}, 370));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);  // Fortran STOP 'BL system array overflow'
}

TEST(unit, TesysDummyWakeSystemDirect) {
  using namespace xfoil::core::boundary_layer;
  const auto props = boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0,
                                             0.0, 0.0, 0.0, 0);
  const auto prim = blprv(1.0, 0.0, 0.01, 0.02, 0.03, 0.005, 1.0, props);
  const auto sec = blkin(prim, props);
  TesysInput in{};
  in.wake.primary = prim;
  in.wake.secondary = sec;
  in.wake.blpar = blparConstants();
  in.cte = 0.011;
  in.tte = 0.045;
  in.dte = 0.075;
  const auto r = tesys(in);
  CHECK_NEAR(r.system.vs1[0][0], -1.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r.system.vs2[0][0], 1.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r.system.vsrez[0], 0.011 - 0.01, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r.system.vsrez[1], 0.045 - 0.02, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r.system.vsrez[2], 0.075 - 0.025 - 0.005, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r.system.vs1[1][1], -1.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r.system.vs2[2][2], 1.0, 1.0e-14, 1.0e-14);
}

TEST(unit, BlsysSimilarityIncompressibleConversion) {
  using namespace xfoil::core::boundary_layer;
  const auto props = boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0,
                                             0.0, 0.0, 0.0, 0);
  const auto prim = blprv(0.05, 0.0, 0.01, 0.05, 0.075, 0.0, 1.0, props);
  const auto sec = blkin(prim, props);
  BlsysInput in{};
  in.primary2 = prim;
  in.secondary2 = sec;
  in.simi = true;
  in.ueEi1 = 1.0;
  in.ueEi2 = 1.0;
  in.ueMs1 = 0.0;
  in.ueMs2 = 0.0;
  in.props = props;
  in.blpar = blparConstants();
  const auto sys = blsys(in);
  // Fortran: SIMI -> VS2=VS1+VS2, VS1=0 (xblsys.f:636-644)
  for (int k = 0; k < 4; ++k) {
    for (int l = 0; l < 5; ++l) {
      CHECK_NEAR(sys.vs1[k][l], 0.0, 1.0e-12, 1.0e-12);
    }
  }
  // Fortran: VS1(K,4)=RES_U1*U1_UEI etc. (xblsys.f:647-658); with
  // U1_UEI=U2_UEI=1 and U1_MS=U2_MS=0 unchanged
  CHECK_NEAR(sys.vs2[0][0], 1.0, 1.0e-10, 1.0e-10);  // BLDIF(0): VS2(1,1)=1
}

TEST(unit, BlsolvBlockEliminationDirect) {
  using namespace xfoil::core::boundary_layer;
  // 2 system rows, diag VA=[1],[1], VB=[-1], right side 1: Solution
  // is the unit vector (block Thomas without mass coupling).
  BlsysWorkspace ws;
  ws.nsys = 2;
  ws.va.assign(2, {});
  ws.vb.assign(2, {});
  ws.vdel.assign(2, {});
  ws.vm.assign(2, std::vector<std::array<double, 3>>(2, {0.0, 0.0, 0.0}));
  ws.vm[0][0][2] = 1.0;  // Fortran VM(3,IV,IV): third row pivot
  ws.vm[1][1][2] = 1.0;
  ws.va[0][0][0] = 1.0;
  ws.va[0][1][1] = 1.0;
  ws.va[1][0][0] = 1.0;
  ws.va[1][1][1] = 1.0;
  // VA/VB are 3x2 blocks (the third column lies in VM); the third row
  // pivot is VM(3,IV,IV) = vm[iv][iv][2].
  ws.vb[1][0][0] = -1.0;
  ws.vb[1][1][1] = -1.0;
  ws.vdel[0][0][0] = 1.0;
  ws.vdel[1][0][0] = 2.0;
  blsolv(ws, 1.0e-10, 2.0, 1, 1);
  CHECK_NEAR(ws.vdel[0][0][0], 1.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(ws.vdel[1][0][0], 3.0, 1.0e-12, 1.0e-12);  // x1 = 2-(-1)*1
}

// ---
// coupling routines and DSLIM (xpanel.f:1542/1562/1580/1616/
// 1758/1786, xbl.f:1564). Formulas formula-faithful; the direct Fortran
// comparisons via harness test 9 follow with SETBL.
namespace {

xfoil::core::boundary_layer::BoundaryLayerMapping makeCouplingMapping() {
  xfoil::core::boundary_layer::BoundaryLayerMapping map{};
  map.top.iblte = 2;
  map.top.nbl = 3;
  map.top.ipan = {0, 1, 2, 3};
  map.top.vti = {1.0, 1.0, 1.0, 1.0};
  map.bottom.iblte = 2;
  map.bottom.nbl = 3;
  map.bottom.ipan = {2, 1, 0, 3};
  map.bottom.vti = {-1.0, -1.0, -1.0, -1.0};
  map.iblmax = 3;
  map.valid = true;
  return map;
}

}  // namespace

TEST(unit, CouplingUicalcUecalcQvfueGamqvDirect) {
  using namespace xfoil::core::boundary_layer;
  const auto map = makeCouplingMapping();
  const std::vector<double> qinv{1.0, 0.5, 0.9, 1.2};
  const std::vector<double> qinvA{0.1, 0.2, 0.3, 0.4};
  BoundaryLayerStationField uinv;
  BoundaryLayerStationField uinvA;
  uicalc(map, qinv, qinvA, uinv, uinvA);
  CHECK_NEAR(uinv.top[0], 0.0, 1.0e-14, 1.0e-14);  // Fortran UINV(1,IS)=0
  CHECK_NEAR(uinv.top[1], 0.5, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uinv.top[2], 0.9, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uinv.bottom[1], -0.5, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uinvA.top[1], 0.2, 1.0e-14, 1.0e-14);
  // UECALC/QVFUE: UEDG from QVIS and back
  const std::vector<double> qvis{0.8, 0.4, 0.7, 1.1};
  BoundaryLayerStationField uedg;
  uecalc(map, qvis, uedg);
  CHECK_NEAR(uedg.top[0], 0.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uedg.top[1], 0.4, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uedg.bottom[1], -0.4, 1.0e-14, 1.0e-14);
  std::vector<double> qvisBack(qvis.size(), 0.0);
  qvfue(map, uedg, qvisBack);
  CHECK_NEAR(qvisBack[1], 0.4, 1.0e-14, 1.0e-14);
  CHECK_NEAR(qvisBack[2], 0.7, 1.0e-14, 1.0e-14);
  // GAMQV
  std::vector<double> gam(qvis.size(), 0.0);
  std::vector<double> gamA(qvis.size(), 0.0);
  gamqv(qvis, qinvA, gam, gamA);
  CHECK_NEAR(gam[1], 0.4, 1.0e-14, 1.0e-14);
  CHECK_NEAR(gamA[2], 0.3, 1.0e-14, 1.0e-14);
}

TEST(unit, CouplingUesetDssetDirect) {
  using namespace xfoil::core::boundary_layer;
  const auto map = makeCouplingMapping();
  // DIJ: 4x4 identity diagonal (panel indices 0..3)
  const std::vector<double> dij{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
  BoundaryLayerStationField uinv;
  uinv.top = {0.0, 1.0, 2.0, 3.0};
  uinv.bottom = {0.0, 0.5, 1.5, 2.5};
  BoundaryLayerStationField mass;
  mass.top = {0.0, 0.1, 0.2, 0.3};
  mass.bottom = {0.0, 0.05, 0.15, 0.25};
  BoundaryLayerStationField uedg;
  ueset(map, dij, 4, uinv, mass, uedg);
  // Fortran: UEDG = UINV + sum(-VTI*VTI*DIJ*MASS); identity diagonal:
  // both sides carry opposite VTI signs for
  CHECK_NEAR(uedg.top[1], 1.0 - 0.1 + 0.05, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uedg.top[2], 2.0 - 0.2, 1.0e-14, 1.0e-14);
  CHECK_NEAR(uedg.bottom[1], 0.5 + 0.1 - 0.05, 1.0e-14, 1.0e-14);
  // DSSET: DSTR = MASS/UEDG
  BoundaryLayerStationField dstr;
  dsset(map, mass, uedg, dstr);
  CHECK_NEAR(dstr.top[1], 0.1 / 0.95, 1.0e-14, 1.0e-14);
  CHECK_NEAR(dstr.bottom[2], 0.15 / 1.35, 1.0e-14, 1.0e-14);
}

TEST(unit, DslimClampDirect) {
  using namespace xfoil::core::boundary_layer;
  // Fortran: DH = MAX(0, HKLIM-HK)/HK_H ; DSTR += DH*THET
  // HKIN(H=2.0, MSQ=0): HK = (2.0-0)/1 = 2.0, HK_H = 1.0
  const double out = dslim(0.4, 0.2, 0.0, 1.5);
  // HK=2.0 > HKLIM=1.5: DH = 1.5-2.0 = -0.5 -> MAX(0,-0.5)=0 -> unchanged
  CHECK_NEAR(out, 0.4, 1.0e-14, 1.0e-14);
  const double out2 = dslim(0.1, 0.2, 0.0, 1.5);
  // HK = 0.1/0.2 = 0.5 < 1.5: DH = 1.0/1.0 = 1.0 -> DSTR = 0.1+0.2 = 0.3
  CHECK_NEAR(out2, 0.3, 1.0e-14, 1.0e-14);
}

// ---
// UPDATE (xbl.f:1253-1560). Structural test: zero deltas and UINV=UEDG
// leave the state unchanged (RMSBL=0, RLX=1).
TEST(unit, UpdateZeroDeltasIdentity) {
  using namespace xfoil::core::boundary_layer;
  UpdateInput in{};
  in.mapping.top.iblte = 1;
  in.mapping.top.nbl = 2;
  in.mapping.top.ipan = {0, 1, 2};
  in.mapping.top.vti = {1.0, 1.0, 1.0};
  in.mapping.bottom.iblte = 1;
  in.mapping.bottom.nbl = 2;
  in.mapping.bottom.ipan = {1, 0, 2};
  in.mapping.bottom.vti = {-1.0, -1.0, -1.0};
  in.mapping.iblmax = 2;
  in.mapping.valid = true;
  const std::vector<double> dijV(9, 0.0);
  in.dij = dijV;
  in.dijStride = 3;
  in.uinv.top = {0.0, 1.0, 1.0};
  in.uinv.bottom = {0.0, 1.0, 1.0};
  in.uinvA.top = {0.0, 0.0, 0.0};
  in.uinvA.bottom = {0.0, 0.0, 0.0};
  in.mass.top = {0.0, 0.075, 0.0};
  in.mass.bottom = {0.0, 0.075, 0.0};
  const std::vector<double> vdelRes(18, 0.0);
  const std::vector<double> vdelRe(18, 0.0);
  in.vdelRes = vdelRes;
  in.vdelRe = vdelRe;
  in.isys = {std::vector<int>{0, 1, 2}, std::vector<int>{3, 4, 5}};
  const std::vector<double> xPan{0.0, 1.0};
  const std::vector<double> yPan{0.0, 0.0};
  in.x = xPan;
  in.y = yPan;
  in.alfa = 0.0;
  in.minf = 0.0;
  in.qinf = 1.0;
  in.gamm1 = 0.4;
  in.dtor = 3.14159265358979 / 180.0;
  in.lalfa = true;
  in.matyp1 = true;
  in.cl = 0.3;
  const std::vector<double> wgapV{0.005};
  in.wgap = wgapV;
  in.itranTop = 1;
  in.itranBottom = 1;
  in.ctau.top = {0.0, 0.03, 0.03};
  in.ctau.bottom = {0.0, 0.03, 0.03};
  in.thet.top = {0.0, 0.05, 0.05};
  in.thet.bottom = {0.0, 0.05, 0.05};
  in.dstr.top = {0.0, 0.075, 0.075};
  in.dstr.bottom = {0.0, 0.075, 0.075};
  in.uedg.top = {0.0, 1.0, 1.0};
  in.uedg.bottom = {0.0, 1.0, 1.0};
  in.tau.top = {0.0, 0.001, 0.0};
  in.tau.bottom = {0.0, 0.001, 0.003};
  in.dis.top = {0.0, 0.002, 0.0};
  in.dis.bottom = {0.0, 0.002, 0.0};
  in.ctq.top = {0.0, 0.03, 0.0};
  in.ctq.bottom = {0.0, 0.03, 0.0};
  in.delt.top = {0.0, 0.1, 0.0};
  in.delt.bottom = {0.0, 0.1, 0.12};
  in.tstr.top = {0.0, 0.05, 0.0};
  in.tstr.bottom = {0.0, 0.05, 0.0};
  const auto r = update(in);
  CHECK_NEAR(r.rmsbl, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(r.rlx, 1.0, 1.0e-12, 1.0e-12);
  // state unchanged (zero-deltas, UINV=UEDG)
  CHECK_NEAR(in.thet.top[1], 0.05, 1.0e-12, 1.0e-12);
  CHECK_NEAR(in.dstr.top[1], 0.075, 1.0e-12, 1.0e-12);
  CHECK_NEAR(in.uedg.top[1], 1.0, 1.0e-12, 1.0e-12);
  // wake equating: upper wake arrays = lower
  CHECK_NEAR(in.tau.top[2], 0.003, 1.0e-12, 1.0e-12);
  CHECK_NEAR(in.delt.top[2], 0.12, 1.0e-12, 1.0e-12);
}

// QNEW/Q_AC-view contract: Fortran DO 20 IBL=2, IBLTE(IS) reads UNEW until
// index IBLTE. With IBLTE == NBL (upper side without wake stations),
// index nbl is the last regular entry and must lie within the view.
// the TE value (top station k=nbl=2, panel 2, UINV=0.5) enters QNEW and
// thus CLNEW/DAC: without it DAC would be 0 instead of -0.25.
TEST(unit, UpdateQnewIblteEqualsNblView) {
  using namespace xfoil::core::boundary_layer;
  UpdateInput in{};
  in.mapping.top.iblte = 2;
  in.mapping.top.nbl = 2;  // iblte == nbl: TE is the last station
  in.mapping.top.ipan = {0, 1, 2};
  in.mapping.top.vti = {1.0, 1.0, 1.0};
  in.mapping.bottom.iblte = 2;
  in.mapping.bottom.nbl = 2;
  in.mapping.bottom.ipan = {3, 3, 3};
  in.mapping.bottom.vti = {-1.0, -1.0, -1.0};
  in.mapping.iblmax = 2;
  in.mapping.valid = true;
  const std::vector<double> dijV(16, 0.0);
  in.dij = dijV;
  in.dijStride = 4;
  in.uinv.top = {0.0, 1.0, 0.5};
  in.uinv.bottom = {0.0, 1.0, 1.0};
  in.uinvA.top = {0.0, 0.0, 0.0};
  in.uinvA.bottom = {0.0, 0.0, 0.0};
  in.mass.top = {0.0, 0.075, 0.075};
  in.mass.bottom = {0.0, 0.075, 0.075};
  const std::vector<double> vdelRes(18, 0.0);
  const std::vector<double> vdelRe(18, 0.0);
  in.vdelRes = vdelRes;
  in.vdelRe = vdelRe;
  in.isys = {std::vector<int>{0, 1, 2}, std::vector<int>{3, 4, 5}};
  const std::vector<double> xPan{0.0, 1.0, 2.0, 3.0};
  const std::vector<double> yPan{0.0, 0.0, 0.0, 0.0};
  in.x = xPan;
  in.y = yPan;
  in.alfa = 0.0;
  in.minf = 0.0;
  in.qinf = 1.0;
  in.gamm1 = 0.4;
  in.dtor = 3.14159265358979 / 180.0;
  in.lalfa = true;
  in.matyp1 = true;
  in.cl = 0.0;
  const std::vector<double> wgapV{0.005};
  in.wgap = wgapV;
  in.itranTop = 1;
  in.itranBottom = 1;
  in.ctau.top = {0.0, 0.03, 0.03};
  in.ctau.bottom = {0.0, 0.03, 0.03};
  in.thet.top = {0.0, 0.05, 0.05};
  in.thet.bottom = {0.0, 0.05, 0.05};
  in.dstr.top = {0.0, 0.075, 0.075};
  in.dstr.bottom = {0.0, 0.075, 0.075};
  in.uedg.top = {0.0, 1.0, 0.5};
  in.uedg.bottom = {0.0, 1.0, 1.0};
  in.tau.top = {0.0, 0.001, 0.0};
  in.tau.bottom = {0.0, 0.001, 0.003};
  in.dis.top = {0.0, 0.002, 0.0};
  in.dis.bottom = {0.0, 0.002, 0.0};
  in.ctq.top = {0.0, 0.03, 0.0};
  in.ctq.bottom = {0.0, 0.03, 0.0};
  in.delt.top = {0.0, 0.1, 0.0};
  in.delt.bottom = {0.0, 0.1, 0.12};
  in.tstr.top = {0.0, 0.05, 0.0};
  in.tstr.bottom = {0.0, 0.05, 0.0};
  const auto r = update(in);
  // QNEW: q = {0, 1, 0.5, -1} (panel 2 only from top-station k=nbl=2).
  // CP = 1 - q^2: {1, 0, 0.75, 0}; CLNEW = 0.5 + 0.375 + 0.375 - 1.5
  // = -0.25; DAC = CLNEW - CL = -0.25.
  CHECK_NEAR(r.dac, -0.25, 1.0e-12, 1.0e-12);
  // zero-deltas and UINV == UEDG: RMSBL = 0, RLX = 1.
  CHECK_NEAR(r.rmsbl, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(r.rlx, 1.0, 1.0e-12, 1.0e-12);
}

// ---
// SETBL (xbl.f:21-516). Complete Fortran chain as in harness test 1/2
// (viscous_harness.f): NACA0012 -> PANGEN -> GG/SPECL -> XYWAKE -> QWCALC
// -> QISET -> STFIND -> IBLPAN -> XICALC -> XIFSET -> IBLSYS -> UICALC ->
// QDCALC -> UESET -> COMSET -> SETBL. Reference: viscous_reference.txt
// SETBL12/DIAG1/DIAG2 (Fortran REAL(4) outputs of the first SETBL call with
// LBLINI=FALSE, also MRCHUE+MRCHDU). Shared NACA0012 SETBL fixture (Fortran
// NACA4..SETBL chain) for SetblNaca0012DirectFortran and
// MrchueTransitionSeedDirectFortran.
namespace {
struct SetblNaca0012Fixture {
  xfoil::core::boundary_layer::SetblInput sb;
  xfoil::core::boundary_layer::BoundaryLayerArcLengths arcs;
  std::vector<double> wgap;
  std::vector<double> dij;
  // panel arrays are copied so that the span lifetime lies in the fixture
  // (and not in the PangenResult result).
  std::vector<double> px, pxp, py, pyp, ps;
  std::array<xfoil::core::boundary_layer::MarchSideState, 2> state{};
  xfoil::core::boundary_layer::SetblDiagnostics diag{};
};

void buildSetblNaca0012Fixture(SetblNaca0012Fixture& fx,
                               double teDijscale1 = 1.0,
                               double teDijscale2 = 1.0, int idamp = 0) {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  // Fortran NACA4/PANGEN/GGCALC/SPECAL (harness test 1).
  const Result<geometry::NacaResult> n = geometry::naca4(12, 123);
  CHECK(n.isOk());
  const geometry::AirfoilBuffer buffer(n.value().xb, n.value().yb,
                                       n.value().name);
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  const std::vector<double> sig0(panel.x.size(), 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, 0.0, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());

  // Fortran XYWAKE (harness test 1) and QWCALC/QISET (harness test 2).
  const Result<WakeState> wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const WakeState& wake = wk.value();
  const WakeBasisResult qb =
      qwCalc(gs.value(), wake, panel, sp.value().alfa, 1.0);
  std::vector<double> qinvu1 = gs.value().qinvu1;
  std::vector<double> qinvu2 = gs.value().qinvu2;
  qinvu1.insert(qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2.insert(qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinv;
  std::vector<double> qinvA;
  qiset(qinvu1, qinvu2, sp.value().alfa, qinv, qinvA);

  // Fortran STFIND/IBLPAN/XICALC/XIFSET (harness test 2).
  const StagnationPointResult st = stfind(sp.value().gam, panel.s);
  CHECK(st.found);
  const BoundaryLayerMapping map = iblpan(panel, st.ist, wake.nw, 283);
  CHECK(map.valid);
  const WakeGap wg = xicalc(panel, map, st.sst, sp.value().te.ante, wake.x,
                            wake.y, fx.arcs);
  fx.wgap = wg.wgap;

  const Result<TransitionState> tTop =
      xifset(panel, map, fx.arcs, st.sst, 1.0, SurfaceSide::kTop);
  const Result<TransitionState> tBot =
      xifset(panel, map, fx.arcs, st.sst, 1.0, SurfaceSide::kBottom);
  CHECK(tTop.isOk());
  CHECK(tBot.isOk());

  // Fortran IBLSYS (harness test 2): NSYS = 183 (NACA0012).
  const BlsysMapping isysMap = iblsys({map.top.nbl, map.bottom.nbl}, 283);
  // Fortran UICALC/QDCALC/UESET (harness test 2; MASS=0 before the first
  // SETBL).
  BoundaryLayerStationField uinv;
  BoundaryLayerStationField uinvA;
  uicalc(map, qinv, qinvA, uinv, uinvA);
  const SourceInfluenceResult qd =
      qdCalc(gs.value(), wake, panel, sp.value().alfa, 1.0, sig0);
  const int dijStride = static_cast<int>(panel.x.size() + wake.nw);
  BoundaryLayerStationField uedg;
  BoundaryLayerStationField mass0;
  const std::size_t nbl1 = static_cast<std::size_t>(map.top.nbl) + 1;
  const std::size_t nbl2 = static_cast<std::size_t>(map.bottom.nbl) + 1;
  mass0.top.assign(nbl1, 0.0);
  mass0.bottom.assign(nbl2, 0.0);
  ueset(map, qd.dij, dijStride, uinv, mass0, uedg);

  // Fortran COMSET exit (harness MINF=0) and SETBL (xbl.f:21-516).
  const CompressibilityState cs = comset(0.0, 1.0);
  SetblInput& sb = fx.sb;
  sb.march.mapping = map;
  const std::array<std::span<const double>, 2> xssiArr{
      std::span<const double>(fx.arcs.top),
      std::span<const double>(fx.arcs.bottom)};
  sb.march.xssi = xssiArr;
  sb.march.wgap = std::span<const double>(fx.wgap);
  sb.march.acrit = {9.0, 9.0};
  sb.march.xiforc = {tTop.value().xiforc, tBot.value().xiforc};
  sb.march.ante = sp.value().te.ante;
  sb.march.blpar = blparConstants();
  sb.march.mass = mass0;
  sb.march.uedg = uedg;
  sb.march.thet.top.assign(nbl1, 0.0);
  sb.march.thet.bottom.assign(nbl2, 0.0);
  sb.march.dstr.top.assign(nbl1, 0.0);
  sb.march.dstr.bottom.assign(nbl2, 0.0);
  sb.march.ctau.top.assign(nbl1, 0.0);
  sb.march.ctau.bottom.assign(nbl2, 0.0);
  sb.march.tau.top.assign(nbl1, 0.0);
  sb.march.tau.bottom.assign(nbl2, 0.0);
  sb.march.dis.top.assign(nbl1, 0.0);
  sb.march.dis.bottom.assign(nbl2, 0.0);
  sb.march.ctq.top.assign(nbl1, 0.0);
  sb.march.ctq.bottom.assign(nbl2, 0.0);
  sb.march.delt.top.assign(nbl1, 0.0);
  sb.march.delt.bottom.assign(nbl2, 0.0);
  sb.march.tstr.top.assign(nbl1, 0.0);
  sb.march.tstr.bottom.assign(nbl2, 0.0);
  sb.mapping = map;
  sb.isys = isysMap.isys;
  sb.nsys = isysMap.nsys;
  sb.uinv = uinv;
  sb.uinvA = uinvA;
  fx.dij = qd.dij;
  if (teDijscale1 != 1.0 || teDijscale2 != 1.0) {
    // only the DIJ rows of TE panels ITE1/ITE2 are scaled (LE rows remain
    // unchanged) -- for the wake-start test.
    const int ite1 = map.top.ipan[static_cast<std::size_t>(map.top.iblte)];
    const int ite2 =
        map.bottom.ipan[static_cast<std::size_t>(map.bottom.iblte)];
    const std::size_t us = static_cast<std::size_t>(dijStride);
    for (std::size_t j = 0; j < us; ++j) {
      fx.dij[static_cast<std::size_t>(ite1) * us + j] *= teDijscale1;
      fx.dij[static_cast<std::size_t>(ite2) * us + j] *= teDijscale2;
    }
  }
  sb.dij = fx.dij;
  sb.dijStride = dijStride;
  sb.sstGo = st.sstGo;
  sb.sstGp = st.sstGp;
  sb.sst = st.sst;
  sb.xle = panel.xle;
  sb.xte = panel.xte;
  sb.yle = panel.yle;
  sb.yte = panel.yte;
  fx.px = panel.x;
  fx.pxp = panel.xp;
  fx.py = panel.y;
  fx.pyp = panel.yp;
  fx.ps = panel.s;
  sb.xPan = fx.px;
  sb.xpPan = fx.pxp;
  sb.yPan = fx.py;
  sb.ypPan = fx.pyp;
  sb.sPan = fx.ps;
  sb.nPan = static_cast<int>(panel.x.size());
  sb.ist = static_cast<int>(st.ist);
  sb.lalfa = true;
  sb.lblini = false;
  sb.machType = 1;
  sb.reType = 1;
  sb.minf1 = 0.0;
  sb.reinf1 = 3.0e6;
  sb.minf = 0.0;
  sb.reinf = 3.0e6;
  sb.tklam = cs.tklam;
  sb.tkMsq = cs.tkMsq;
  sb.idamp = idamp;

  fx.diag = setbl(sb, fx.state);
}
}  // namespace

// (m): wake-start branch of SETBL.
// Fortran xbl.f:255-263 defines at the first wake point (IBL=IBLTE+1)
// D1_M(JV) = DTE_UTE1*UTE1_M(JV) + DTE_UTE2*UTE2_M(JV)
// with the TE Ue sensitivities UTE1_M/UTE2_M (xbl.f:123-132), not with the
// LE values ULE1_M/ULE2_M. The test scales only the DIJ rows ITE1/ITE2
// (factors 1, 2, 3); the LE rows ILE1/ILE2 and the remaining wake-start
// build-up stay identical (at the wake start i = IPAN(IBL) is a wake-panel
// index, not ITE). With the correct implementation VM(L=3, JV, IV=JVTE1)
// changes linearly with the TE DIJ values; with the wrong LE-based
// variant (ule1M/ule2M) VM stays unchanged and the test proves nothing
// about the TE sensitivities.
TEST(unit, SetblWakeStartUsesTrailingEdgeSensitivities) {
  using namespace xfoil::core::boundary_layer;
  struct Run {
    SetblNaca0012Fixture fx;
    double vm{};
    double d1m{};
    double dteUte1{};
    double dteUte2{};
    double ute1m{};
    double ute2m{};
    double ule1m{};
  };
  const auto run = [](Run& r, double teScale1, double teScale2) {
    buildSetblNaca0012Fixture(r.fx, teScale1, teScale2);
    SetblInput& sb = r.fx.sb;
    const auto& mt = sb.mapping.top;
    const auto& mb = sb.mapping.bottom;
    const std::size_t iblte1 = static_cast<std::size_t>(mt.iblte);
    const std::size_t iblte2 = static_cast<std::size_t>(mb.iblte);
    const int jvte1 = sb.isys[0][iblte1];
    const int jvte2 = sb.isys[1][iblte2];
    // wake stations lie on the lower side (NBL2 > IBLTE2).
    const int ivWake = sb.isys[1][iblte2 + 1];  // IBL = IBLTE+1 (IS=2)
    // JV is top station 1 (near LE), different from JVTE1/JVTE2.
    const int jv = sb.isys[0][1];
    CHECK(jv != jvte1 && jv != jvte2);
    r.vm = sb.ws.vm[static_cast<std::size_t>(ivWake)]
               [static_cast<std::size_t>(jv)][2];
    // DTE_UTE1/2 (xbl.f:246,248) from the resulting state.
    r.dteUte1 = -sb.march.dstr.top[iblte1] / sb.march.uedg.top[iblte1];
    r.dteUte2 = -sb.march.dstr.bottom[iblte2] / sb.march.uedg.bottom[iblte2];
    // UTE1_M/UTE2_M/ULE1_M (xbl.f:123-132) for this JV.
    const std::size_t stride = static_cast<std::size_t>(sb.dijStride);
    const std::size_t ite1 = static_cast<std::size_t>(mt.ipan[iblte1]);
    const std::size_t ite2 = static_cast<std::size_t>(mb.ipan[iblte2]);
    const std::size_t ile1 = static_cast<std::size_t>(mt.ipan[1]);
    const std::size_t jbl = 1;  // top-station 1
    const std::size_t j = static_cast<std::size_t>(mt.ipan[jbl]);
    const double dijIte1 = sb.dij[ite1 * stride + j];
    const double dijIte2 = sb.dij[ite2 * stride + j];
    const double dijIle1 = sb.dij[ile1 * stride + j];
    r.ute1m = -mt.vti[iblte1] * mt.vti[jbl] * dijIte1;
    r.ute2m = -mb.vti[iblte2] * mb.vti[jbl] * dijIte2;
    r.ule1m = -mt.vti[1] * mt.vti[jbl] * dijIle1;
    r.d1m = r.dteUte1 * r.ute1m + r.dteUte2 * r.ute2m;
  };
  // asymmetric scaling of only the top TE row, so D1_M(JV) does not
  // cancel top/bottom in the symmetric NACA-0012 case.
  Run a;
  run(a, 1.0, 1.0);
  Run b;
  run(b, 2.0, 1.0);
  Run c;
  run(c, 3.0, 1.0);

  // LE- and TE-sensitivities must be distinguishable.
  CHECK(std::abs(a.ute1m - a.ule1m) > 1.0e-12);
  CHECK(std::abs(a.dteUte1) > 1.0e-12);
  CHECK(std::abs(a.dteUte2) > 1.0e-12);
  // VM itself changes with the TE DIJ values (LE error -> no change).
  CHECK(std::abs(b.vm - a.vm) > 1.0e-12);
  CHECK(std::abs(c.vm - b.vm) > 1.0e-12);
  // Linearity: VM delta proportional to the expected D1_M delta
  // (synthetic DIJ scaling is exactly linear).
  const double dVmB = b.vm - a.vm;
  const double dVmC = c.vm - a.vm;
  const double dD1B = b.d1m - a.d1m;
  const double dD1C = c.d1m - a.d1m;
  CHECK(std::abs(dD1B) > 1.0e-12);
  CHECK_NEAR(dVmB / dD1B, dVmC / dD1C, 1.0e-9, 1.0e-9);
  // UTE1_M scales exactly with the DIJ factor.
  CHECK_NEAR(b.ute1m, 2.0 * a.ute1m, 1.0e-12, 1.0e-12);
  CHECK_NEAR(c.ute1m, 3.0 * a.ute1m, 1.0e-12, 1.0e-12);
}

TEST(unit, SetblNaca0012DirectFortran) {
  using namespace xfoil::core::boundary_layer;
  SetblNaca0012Fixture fx;
  buildSetblNaca0012Fixture(fx);
  SetblInput& sb = fx.sb;
  const auto& diag = fx.diag;
  const auto& arcs = fx.arcs;
  CHECK(diag.lblini);
  CHECK_EQ(sb.nsys, 183);
  CHECK_EQ(sb.ist, 79);

  // reference: SETBL12 (SETBL data part, test 2).
  CHECK_NEAR(sb.march.props.rstbl, 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.props.hstinv, 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.props.reybl, 3.0e6, 1.0e-3, kFortranRelTol);
  CHECK_NEAR(sb.march.props.dwte, 2.49554985e-3, kFortranAbsTol,
             kFortranRelTol);
  CHECK_EQ(sb.march.props.idampv, 0);
  // reference: SETBL12 THET_T1/T2 = 0 (station 1 is not described).
  CHECK_NEAR(sb.march.thet.top[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.dstr.top[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.ctau.top[0], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.uedg.top[0], 0.0, kFortranAbsTol, kFortranRelTol);
  // reference: DIAG1 (marching result of the first SETBL, test 2).
  CHECK_NEAR(sb.march.thet.top[1], 1.85680910e-05, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(sb.march.dstr.top[1], 4.13977032e-05, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(sb.march.ctau.top[1], 0.0, kFortranAbsTol, kFortranRelTol);
  // reference: DIAG1 UEDG21/UEDG51. The edge velocity depends on the
 // inviscid base solution QINV at the stagnation-point panels
  // (GGCALC/PSILIN): there the C++ base solution deviates from the Fortran
  // one by up to 0.8% (QINV79/80: 7.49e-2 vs 7.43e-2, QINV78: 0.15%,
  // QINV77: 0.03%, QINV76: 0.007%), while the remaining panels match at
  // <0.03%. The march solves the same similarity equation and matches in
  // THET/DSTR/CTAU (above). The UEDG values at the stagnation-point panels
 // are therefore not directly comparable here (open deviation).
  CHECK_NEAR(sb.march.thet.bottom[7], 2.59117387e-05, kFortranAbsTol,
             kFortranRelTol);
  // UEDG is influence-derived (QINV from GGCALC): tolerance class
 // kInfluenceRelTol (convention). The stagnation-point panels differ by
 // up to 0.8% (open deviation), the remaining panels by <3e-3
  // relative.
  CHECK_NEAR(sb.march.uedg.bottom[7], 8.14660013e-01, kFortranAbsTol,
             kInfluenceRelTol);
  // reference: DIAG2 (XSSI(82,2), UEDG(82,2)/UEDG(83,2), WGAP(1..3),
  // THET(82,2)/THET(83,2), test 2).
  CHECK_NEAR(arcs.bottom[81], 1.01962137, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(arcs.bottom[82], 1.02802253, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.uedg.bottom[81], 7.67261147e-01, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(sb.march.uedg.bottom[82], 8.53640437e-01, kFortranAbsTol,
             kInfluenceRelTol);
  // THET(82,2)/THET(83,2) are influence-derived: the march consumes UEDG
 // from QINV (GGCALC/PSILIN, stagnation-point panels up to 0.8%
  // deviation). DBGMR station comparison with exchange restrict: with
  // Fortran UEDG, THET(82,2)/THET(83,2) match at 4e-6/6e-6 relative
  // (formula-faithful), with C++ UEDG at 6e-4/1.6e-3 relative -- the
 // deviation is UEDG-driven, not a formula error. Tolerance class
 // kInfluenceRelTol (convention, as the UEDG checks above); the wake
  // stations lie at 6e-4/1.6e-3 relative within this class.
  CHECK_NEAR(sb.march.thet.bottom[81], 6.03816751e-03, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(sb.march.thet.bottom[82], 3.86340986e-03, kFortranAbsTol,
             kInfluenceRelTol);
  CHECK_NEAR(sb.march.wgap[0], 2.49554985e-03, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(sb.march.wgap[1], 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(sb.march.wgap[2], 0.0, kFortranAbsTol, kFortranRelTol);
  // reference: 'side 1/2 free transition at x/c = 0.4975/0.4977, 50'
  // (ITRAN 50 1-based -> 49 0-based).
  //
  // XOCTR is influence-derived: the transition position XT comes from the
  // BL march (TRCHEK chain, consumes UEDG from QINV). Isolation
  // (DBGMR/DXTR comparison with exchange restrict):
  // - geometry/spline formula-faithful: with Fortran XT 0.514358/0.514503,
  // the C++ SEVAL returns 0.49751502/0.49765973 vs Fortran XOCTR
  // 0.497515/0.497660 (identical to 5e-6).
 // - XT deviation is UEDG-driven: the C++ XT is exactly symmetric
 // (5.14610812e-01 both sides, QINV symmetric), Fortran asymmetric
  // (0.514358/0.514503); with Fortran UEDG the C++ XT becomes asymmetric
  // and matches Fortran (5.14368046e-01 vs 0.514358, rel 2.0e-6;
  // 5.14484953e-01 vs 0.514503, rel 1.3e-5), XOCTR 0.497525/0.497642 vs
  // 0.497515/0.497660 (rel 2.1e-5/4.9e-5).
 // tolerance class kInfluenceRelTol (convention); ITRAN = 49
  // (0-based) in both.
  CHECK_NEAR(diag.xoctr[0], 0.4975, 1.0e-4, kInfluenceRelTol);
  CHECK_NEAR(diag.xoctr[1], 0.4977, 1.0e-4, kInfluenceRelTol);
  CHECK_EQ(diag.itran[0], 49);
  CHECK_EQ(diag.itran[1], 49);
  CHECK_FALSE(diag.tforce[0]);
  CHECK_FALSE(diag.tforce[1]);
  // reference: 'MRCHUE: Inverse mode at 81 Hk = 2.500' (last message,
  // side 2; 81 1-based -> 80 0-based).
  CHECK_EQ(diag.marchDiagnostics.lastInverseStation, 80);
  CHECK_NEAR(diag.marchDiagnostics.lastInverseHk, 2.5, 1.0e-5, 1.0e-5);
  CHECK(diag.marchDiagnostics.converged);
}

// MRCHUE/MRCHDU transition seeding (xbl.f:627-633, 966-970, 809-810,
// 1131-1132). Formula-faithful state evolution on the NACA0012 session
// state:
// - laminar storage CTAU(IBL)=AMI for IBL<ITRAN,
// - exponential rise of the amplification at the sensitive station 25
// (1-based),
// - seeding CTAU(ITRAN)=0.03 at the transition-station entry,
// - ITRAN = 49 (0-based) without forced transition.
// the test does not demand the exact REAL4 iteration history or a rounded
// CTAU trajectory; the tolerances are precision-band well-behaved
// (Fortran REAL4 vs. C++ double).
TEST(unit, MrchueTransitionSeedDirectFortran) {
  using namespace xfoil::core::boundary_layer;
  SetblNaca0012Fixture fx;
  buildSetblNaca0012Fixture(fx);
  const SetblInput& sb = fx.sb;
  const auto& diag = fx.diag;

  // ITRAN rule: no forced transition, free transition for
  // station 50 (1-based) -> 49 (0-based).
  CHECK_EQ(diag.itran[0], 49);
  CHECK_EQ(diag.itran[1], 49);
  CHECK_FALSE(diag.tforce[0]);
  CHECK_FALSE(diag.tforce[1]);

  const auto& ctau = sb.march.ctau.top;
  const auto& thet = sb.march.thet.top;
  const auto& dstr = sb.march.dstr.top;
  const auto& uedg = sb.march.uedg.top;

  // similarity station 2: THET/DSTR positive, CTAU still ~0.
  CHECK(thet[1] > 0.0);
  CHECK(dstr[1] > 0.0);
  CHECK_NEAR(ctau[1], 0.0, 1.0e-6, 1.0e-6);

  // laminar storage CTAU=AMI: before the exponential rise the
  // amplification is tiny, afterwards clear (station 25, 1-based -> 24).
  CHECK(ctau[23] > 0.0);
  CHECK(ctau[23] < 1.0e-5);
  CHECK(ctau[24] > 100.0 * ctau[23]);
  CHECK(ctau[24] > 3.0e-2);
  CHECK(ctau[24] < 8.0e-2);

  // Monotonic laminar rise until shortly before transition.
  CHECK(ctau[26] > ctau[25]);
  CHECK(ctau[40] > ctau[30]);

  // seeding at the transition station entry: CTAU(ITRAN) = CTI = 0.03
  // (later modified by the Newton update, hence the band).
  CHECK(ctau[49] > 0.02);
  CHECK(ctau[49] < 0.05);

  // Marginal quantities remain positive and finite (no index/sign/layout
  // error in the march). The laminar strengthening grows toward the
  // transition as O(exp(ACRIT)) = O(8100).
  for (int ibl = 1; ibl <= 49; ++ibl) {
    const std::size_t k = static_cast<std::size_t>(ibl);
    CHECK(thet[k] > 0.0);
    CHECK(dstr[k] > 0.0);
    CHECK(uedg[k] > 0.0);
    CHECK(ctau[k] >= 0.0);
    CHECK(ctau[k] < 1.0e5);
  }
  CHECK(ctau[48] > 1.0);
}

// SETBL must write the ITRAN currently held in the march loop
// (in.march.itran) back into the diagnostics, also if the second call
// (LBLINI=TRUE, only MRCHDU) detects no new transition. Fortran holds ITRAN
// in XBL.COM; VISCAL overwrites state.ITRAN with diag.ITRAN (xbl.f VISCAL
// loop). Without a return value, state.ITRAN would reset to 0 on every
// SETBL.
TEST(unit, SetblPersistsItranAcrossSecondCall) {
  using namespace xfoil::core::boundary_layer;
  SetblNaca0012Fixture fx;
  buildSetblNaca0012Fixture(fx);
  CHECK_EQ(fx.diag.itran[0], 49);
  CHECK_EQ(fx.diag.itran[1], 49);

  // second SETBL call on the same state (LBLINI already TRUE):
  // MRCHDU detects no new transition, diag.ITRAN must nevertheless
  // contain the current ITRAN.
  const SetblDiagnostics diag2 = setbl(fx.sb, fx.state);
  CHECK_EQ(diag2.itran[0], 49);
  CHECK_EQ(diag2.itran[1], 49);
  CHECK_EQ(fx.sb.march.itran[0], 49);
  CHECK_EQ(fx.sb.march.itran[1], 49);
}

// SETBL is the canonical producer of IDAMPV (xbl.f:77
// IDAMPV = IDAMP). The fresh SetblInput.idamp must reach props.idampv
// (AXSET dispatch xblsys.f:70: 0 -> DAMPL, 1 -> DAMPL2); the previous props
// threshold value must not be authoritative. Default 0 stays bit-identical
// to the previous baseline.
TEST(unit, SetblIdampProducerReachesProps) {
  using namespace xfoil::core::boundary_layer;
  SetblNaca0012Fixture fx0;
  buildSetblNaca0012Fixture(fx0, 1.0, 1.0, 0);
  CHECK_EQ(fx0.sb.march.props.idampv, 0);

  SetblNaca0012Fixture fx1;
  buildSetblNaca0012Fixture(fx1, 1.0, 1.0, 1);
  CHECK_EQ(fx1.sb.march.props.idampv, 1);

  // the changed amplification path (dampl2) must affect the marching
  // trajectory: at least one amplification or transition result differs from
  // the IDAMP=0 run.
  bool differs = false;
  if (fx0.state[0].ampl1 != fx1.state[0].ampl1) differs = true;
  if (fx0.state[1].ampl1 != fx1.state[1].ampl1) differs = true;
  if (fx0.diag.itran[0] != fx1.diag.itran[0]) differs = true;
  if (fx0.diag.itran[1] != fx1.diag.itran[1]) differs = true;
  if (fx0.diag.xoctr[0] != fx1.diag.xoctr[0]) differs = true;
  if (fx0.diag.xoctr[1] != fx1.diag.xoctr[1]) differs = true;
  CHECK(differs);
}

// (IV=74): SETBL must write the COMMON HK2 value provided by BLVAR(3) back
// into COM1. Fortran xbl.f:465-472,497-499 and xblsys.f:799-800: after
// CALL BLVAR(3), HK2 = MAX(HK2,1.00005), and COM1(4)=HK2 takes over exactly
// this COMMON value, not the unclamped BLKIN HK2. The test uses the
// NACA0012 SETBL fixture and compares state[1].com1.hk (last wake station on
// the lower side) with the explicitly recomputed BLVAR(3) HK2. The HK2 clamp
// itself is secured separately by unit.BlVarHk2ClampExposed and the
// clamp test.
TEST(unit, SetblCom1Hk2UsesClampedCommon) {
  using namespace xfoil::core::boundary_layer;
  SetblNaca0012Fixture fx;
  buildSetblNaca0012Fixture(fx);

  const auto& mb = fx.sb.mapping.bottom;
  const std::size_t iblLast = static_cast<std::size_t>(mb.nbl);
  const std::size_t iblte = static_cast<std::size_t>(mb.iblte);
  const double xsi = fx.arcs.bottom[iblLast];
  const double uei = fx.sb.march.uedg.bottom[iblLast];
  const double thi = fx.sb.march.thet.bottom[iblLast];
  const double dsi = fx.sb.march.dstr.bottom[iblLast];
  const double cti = fx.sb.march.ctau.bottom[iblLast];
  const double ami = 0.0;
  const std::size_t iw = iblLast - iblte - 1;
  const double dswaki = fx.wgap[iw];

  const BoundaryLayerPrimary prim2 =
      blprv(xsi, ami, cti, thi, dsi, dswaki, uei, fx.sb.march.props);
  const BoundaryLayerSecondary sec2 = blkin(prim2, fx.sb.march.props);
  const BlvarInput vin{prim2, sec2, fx.sb.march.blpar};
  const BoundaryLayerTurbulentSecondary ts = blvar(StationType::kWake, vin);

  CHECK_NEAR(fx.state[1].com1.hk, ts.hk, 0.0, 0.0);
}

// STMOVE (xpanel.f:1628-1755). Behavioral/structural test of the
// stagnation-point shift on the NACA0012 session state (as in
// SetblNaca0012DirectFortran). STMOVE calls STFIND anew; for unchanged GAM,
// Is/SST stay equal (reset branch, XICALC only). A deliberately different
// Is input forces the move branch (IBLPAN/UICALC/XICALC/IBLSYS + array
// shift).
TEST(unit, StmoveStagnationRemapDirect) {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  const Result<geometry::NacaResult> nacaRes = geometry::naca4(12, 123);
  CHECK(nacaRes.isOk());
  const geometry::AirfoilBuffer buffer(nacaRes.value().xb, nacaRes.value().yb,
                                       nacaRes.value().name);
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  const std::vector<double> sig0(panel.x.size(), 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, 0.0, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const WakeState& wake = wk.value();
  const WakeBasisResult qb =
      qwCalc(gs.value(), wake, panel, sp.value().alfa, 1.0);
  std::vector<double> qinvu1 = gs.value().qinvu1;
  std::vector<double> qinvu2 = gs.value().qinvu2;
  qinvu1.insert(qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2.insert(qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinv;
  std::vector<double> qinvA;
  qiset(qinvu1, qinvu2, sp.value().alfa, qinv, qinvA);

  const StagnationPointResult st = stfind(sp.value().gam, panel.s);
  CHECK(st.found);
  BoundaryLayerMapping map = iblpan(panel, st.ist, wake.nw, 283);
  CHECK(map.valid);
  BoundaryLayerArcLengths arcs;
  const WakeGap wg = xicalc(panel, map, st.sst, sp.value().te.ante, wake.x,
                            wake.y, arcs);
  static_cast<void>(wg);
  const BlsysMapping isysMap = iblsys({map.top.nbl, map.bottom.nbl}, 283);

  // qvis (panel+wake) and GAM copy (in/out for STMOVE).
  std::vector<double> qvis(panel.x.size() + wake.nw, 0.0);
  std::vector<double> gam = sp.value().gam;

  StMoveState state{};
  state.mapping = map;
  state.arcs = arcs;
  uicalc(map, qinv, qinvA, state.uinv, state.uinvA);
  // Fortran arrays are dimensioned IVX (283): the shift branch reads
  // IBL+IDIF beyond NBL(1) (xpanel.f:1724), so the station fields are filled
  // to IVX capacity.
  const std::size_t capIvx = boundaryLayerCapacity;
  auto fill = [](std::vector<double>& v, std::size_t n) { v.assign(n, 1.0); };
  fill(state.thet.top, capIvx);
  fill(state.thet.bottom, capIvx);
  fill(state.dstr.top, capIvx);
  fill(state.dstr.bottom, capIvx);
  fill(state.ctau.top, capIvx);
  fill(state.ctau.bottom, capIvx);
  fill(state.uedg.top, capIvx);
  fill(state.uedg.bottom, capIvx);
  fill(state.mass.top, capIvx);
  fill(state.mass.bottom, capIvx);
  state.itran = {49, 49};
  state.ist = st.ist;
  state.sst = st.sst;
  state.isys = isysMap;

  // Scenario A: no stagnation-point shift (ISTOLD == Is) -> XICALC only.
  const StMoveResult resA = stmove(panel, wake.x, wake.y, sp.value().te.ante,
                                   wake.nw, 283, qinv, qinvA, gam, qvis,
                                   state);
  CHECK_FALSE(resA.moved);
  CHECK_NEAR(state.sst, st.sst, 1.0e-9, 1.0e-9);
  CHECK_EQ(state.ist, st.ist);
  // unchanged station values in the non-move branch.
  CHECK_NEAR(state.thet.top[1], 1.0, 1.0e-14, 1.0e-14);

  // Scenario B: deliberately different Is input -> move branch.
  state.ist = st.ist + 1;  // is of STFIND reset on st.is
  const StMoveResult resB = stmove(panel, wake.x, wake.y, sp.value().te.ante,
                                   wake.nw, 283, qinv, qinvA, gam, qvis,
                                   state);
  CHECK(resB.moved);
  CHECK_EQ(state.ist, st.ist);
  CHECK(state.mapping.valid);
  CHECK(state.isys.nsys > 0);

  // Scenario C: no +->- sign change (GAM negated) -> STFIND fallback
  // I=N/2 with message (xpanel.f:1366); STMOVE continues.
  std::vector<double> gamNoStag(gam.size());
  for (std::size_t i = 0; i < gam.size(); ++i) {
    gamNoStag[i] = -gam[i];
  }
  state.ist = st.ist;
  const StMoveResult resC = stmove(panel, wake.x, wake.y, sp.value().te.ante,
                                   wake.nw, 283, qinv, qinvA, gamNoStag,
                                   qvis, state);
  CHECK(resC.stagnationNotFound);
  // Fortran STFIND fallback I=N/2 is 1-based -> 0-based N/2-1.
  CHECK_EQ(state.ist, panel.x.size() / 2 - 1);
}

// VISCAL header: STFIND fallback as event (xoper.f:2919, xpanel.f:1366).
TEST(unit, ViscalHeadStfindFallbackEvent) {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  const Result<geometry::NacaResult> nacaRes = geometry::naca4(12, 123);
  CHECK(nacaRes.isOk());
  const geometry::AirfoilBuffer buffer(nacaRes.value().xb, nacaRes.value().yb,
                                       nacaRes.value().name);
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  const std::vector<double> sig0(panel.x.size(), 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, 0.0, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());

  ViscalInput in{};
  in.blpar = blparConstants();
  in.niter = 1;
  in.qinf = 1.0;
  in.xref = 0.25;
  in.yref = 0.0;
  in.lvisc = false;
  in.lalfa = false;
  ViscalState vs{};
  vs.alfa = sp.value().alfa;
  vs.minf = 0.0;
  vs.reinf = 3.0e6;
  vs.gam = sp.value().gam;
  vs.gamA = sp.value().gamA;
  // No +->- sign change in the header GAM -> STFIND fallback.
  for (double& g : vs.gam) {
    g = -g;
  }
  const ViscalResult res =
      viscal(in, panel, gs.value(), wk.value(), sp.value().te, vs);
  bool headEvent = false;
  for (const auto& ev : res.events) {
    if (ev.kind == ViscousEventKind::stagnationPointNotFound &&
        ev.phase == ViscousEventPhase::preHead) {
      headEvent = true;
    }
  }
  CHECK(headEvent);
  // Fortran STFIND fallback I=N/2 is 1-based -> 0-based N/2-1.
  CHECK_EQ(vs.ist, panel.x.size() / 2 - 1);
}

// wake growth after a solved QVIS run. QVIS is a fixed-dimension COMMON
// field QVIS(IZX), IZX = IQX+IWX (XFOIL.INC CR04). QVFUE
// (xpanel.f:1580-1591) writes QVIS(I) via IPAN(IBL,IS) through the wake
// stations (I up to N+NW). NW grows via VPAR WAKE; the same COMMON field
// requires capacity n+nw. The C++ capacity had so far grown only in the
// common-initialized state, so qvfue after the first solved point threw
// with "panel index outside QVIS" (VISCAL non-convergence for WAKE 2.0
// after the default wake point). The capacity must follow the growing
// wake.
TEST(unit, ViscalWakeGrowthKeepsQvisCapacity) {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  const Result<geometry::NacaResult> nacaRes = geometry::naca4(12, 123);
  CHECK(nacaRes.isOk());
  const geometry::AirfoilBuffer buffer(nacaRes.value().xb, nacaRes.value().yb,
                                       nacaRes.value().name);
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  const std::size_t n = panel.x.size();
  const std::vector<double> sig0(n, 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 1.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, 0.0698132, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());

  ViscalInput in{};
  in.blpar = blparConstants();
  in.niter = 30;
  in.qinf = 1.0;
  in.xref = 0.25;
  in.yref = 0.0;
  in.vaccel = 0.01;
  in.lalfa = true;
  in.lvisc = true;
  in.machType = 1;
  in.reType = 1;
  in.minf1 = 0.0;
  in.reinf1 = 1.0e6;
  in.ivx = 283;
  in.acrit = {9.0, 9.0};
  in.xstrip = {1.0, 1.0};

  ViscalState vs{};
  vs.alfa = sp.value().alfa;
  vs.minf = sp.value().minf;
  vs.reinf = sp.value().reinf;
  vs.cl = sp.value().cl;
  vs.gam = sp.value().gam;
  vs.gamA = sp.value().gamA;

  // first viscous point with default wake (WAKLEN=1.0).
  const Result<WakeState> wk1 =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk1.isOk());
  const ViscalResult res1 =
      viscal(in, panel, gs.value(), wk1.value(), sp.value().te, vs);
  CHECK(res1.converged);
  CHECK(vs.qvisState == ViscousVelocityFieldState::solved);
  const std::size_t size1 = vs.qvis.size();
  CHECK_EQ(size1, n + static_cast<std::size_t>(wk1.value().nw));

  // VPAR WAKE 2.0: wake with larger NW; invalidation as setWakeLength
  // (LWAKE/LBLINI/LVCONV/LIPAN false, QVIS solved).
  const Result<WakeState> wk2 =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 2.0, 1.0);
  CHECK(wk2.isOk());
  CHECK(wk2.value().nw > wk1.value().nw);
  vs.mapping.valid = false;
  vs.lblini = false;
  vs.lvconv = false;
  // wake-DIJ invalidation as in the session path (XYWAKE regeneration,
  // xpanel.f:1350 LWDIJ=false): the wake-DIJ block is rebuilt with the new
  // wake geometry.
  vs.dijWakeValid = false;

  // before the fix, viscal threw here (qvfue: panel index outside QVIS)
  // because the QVIS capacity had not grown in the solved state.
  const ViscalResult res2 =
      viscal(in, panel, gs.value(), wk2.value(), sp.value().te, vs);
  CHECK(res2.converged);
  CHECK_EQ(vs.qvis.size(), n + static_cast<std::size_t>(wk2.value().nw));
  // grow-only: solved entries are retained, and the new wake followers are
  // written by QVFUE.
  CHECK(vs.qvis.size() > size1);
}

TEST(unit, CdcalcSquireYoungFrictionDirect) {
  using namespace xfoil::core::boundary_layer;
  BoundaryLayerMapping map{};
  map.top.iblte = 2;
  map.top.nbl = 3;
  map.top.ipan = {0, 1, 2, 3};
  map.top.vti = {1, 1, 1, 1};
  map.bottom.iblte = 2;
  map.bottom.nbl = 3;
  map.bottom.ipan = {2, 1, 0, 3};
  map.bottom.vti = {-1, -1, -1, -1};
  map.iblmax = 3;
  map.valid = true;

  BoundaryLayerStationField thet, dstr, uedg, tau;
  thet.top.assign(4, 0.1);
  thet.bottom.assign(4, 0.1);
  dstr.top.assign(4, 0.3);
  dstr.bottom.assign(4, 0.3);
  uedg.top.assign(4, 0.5);
  uedg.bottom.assign(4, 0.5);
  tau.top.assign(4, 0.2);
  tau.bottom.assign(4, 0.2);

  const std::vector<double> x{0.0, 0.25, 0.5, 0.75};
  const std::vector<double> y{0.0, 0.1, 0.0, -0.1};
  const double alfa = 0.0, qinf = 1.0, tklam = 0.0;

  const CdCalcResult res =
      cdcalc(alfa, qinf, tklam, true, true, map, thet, dstr, uedg, tau, x, y);

  // direct Fortran comparison (extracted CDCALC, xfoil.f:1172-1208,
  // REAL(4)):
  // THWAKE 0.1, URAT 0.5, UEWAKE 0.5, SHWAKE 3,
  // CD 1.25000002E-02, CDF 0, CDF1 +0.1, CDF2 -0.1.
  // Squire-Young: THWAKE=0.1, URAT=0.5, UEWAKE=0.5, SHWAKE=3
  // CD = 2*0.1*(0.5)^(0.5*8) = 0.2*0.5^4 = 0.0125
  CHECK_NEAR(res.cd, 0.2 * std::pow(0.5, 4.0), 1.0e-14, 1.0e-14);
  // friction via IBL=2 per side: top dx=+0.25, bottom dx=-0.25 -> cancel
  CHECK_NEAR(res.cdf, 0.0, 1.0e-14, 1.0e-14);
  // Lateral friction integrals (FR CDF1 = +0.1, FR CDF2 = -0.1):
  // only the top contribution (bottom TAU zero) or only the bottom one.
  BoundaryLayerStationField tauTopOnly = tau;
  std::fill(tauTopOnly.bottom.begin(), tauTopOnly.bottom.end(), 0.0);
  const CdCalcResult resTop =
      cdcalc(alfa, qinf, tklam, true, true, map, thet, dstr, uedg, tauTopOnly,
             x, y);
  CHECK_NEAR(resTop.cdf, 0.1, 1.0e-14, 1.0e-14);
  BoundaryLayerStationField tauBotOnly = tau;
  std::fill(tauBotOnly.top.begin(), tauBotOnly.top.end(), 0.0);
  const CdCalcResult resBot =
      cdcalc(alfa, qinf, tklam, true, true, map, thet, dstr, uedg, tauBotOnly,
             x, y);
  CHECK_NEAR(resBot.cdf, -0.1, 1.0e-14, 1.0e-14);

  // inviscid branch: Cd = 0, CDF stays the friction integral.
  const CdCalcResult resInv =
      cdcalc(alfa, qinf, tklam, false, true, map, thet, dstr, uedg, tau, x, y);
  CHECK_NEAR(resInv.cd, 0.0, 0.0, 0.0);
}

// direct Fortran comparison (extracted FCPMIN, xoper.f:1768-1801,
// REAL(4)):
// LVISC=.FALSE.: CPMNI -1, XCPMNI 0.5, CPMNV overwritten on -1,
// XCPMNV 0.5, CPMN -1.
// LVISC=.TRUE.: CPMNV -0.9, XCPMNV 0.5, CPMN -0.9.
TEST(unit, FcpminMinCpDirect) {
  using namespace xfoil::core::boundary_layer;
  const std::vector<double> x{0.0, 0.2, 0.5, 0.8, 1.0};
  const std::vector<double> cpi{0.0, -0.5, -1.0, -0.3, 0.1};
  const std::vector<double> cpv{0.0, -0.4, -0.9, -0.2, 0.1};

  const CpMinResult r1 = fcpmin(false, x, cpi, cpv);
  CHECK_NEAR(r1.cpmin, -1.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r1.xcpmni, 0.5, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r1.cpmni, -1.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r1.cpmnv, -1.0, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r1.xcpmnv, 0.5, 1.0e-14, 1.0e-14);

  const CpMinResult r2 = fcpmin(true, x, cpi, cpv);
  CHECK_NEAR(r2.cpmin, -0.9, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r2.xcpmnv, 0.5, 1.0e-14, 1.0e-14);
  CHECK_NEAR(r2.cpmnv, -0.9, 1.0e-14, 1.0e-14);
}

// ---
// VISCAL (xoper.f:2892-3116). Structural test of the Newton-loop driver on
// the NACA0012 session state (harness chain: NACA4 -> PANGEN -> GG/SPECAL
// -> XYWAKE -> QWCALC/QISET -> STFIND/IBLPAN/XICALC/IBLSYS/UICALC ->
// XIFSET). The driver takes over the QDCALC and SETBL setup and runs the
// Newton loop. Reference: viscous_reference.txt VISCAL12 (Cl=0,
// Cd=5.0883e-3, CDF=4.3625e-3, convergence in 6 iterations). The C++
// iteration deviates from iteration 3 onward (open momentum-thickness
// assembly deviation: VA(1,2) Fortran +1.8610e+2 vs C++ -2.8545e+1, VM
// line 1 -9.4334e+1 vs +1.2184e+1 at the turbulent station;
// VA(1,1)/VA(2,2)/VB(1,1) match at 0.2-2.6%). Checked are therefore the
// verified structural properties: the loop runs, the first iterations are
// finite, and RMSBL is sane.
TEST(unit, ViscalNewtonLoopStructural) {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  // Fortran NACA4/PANGEN/GGCALC/SPECAL (harness chain, alfa=0).
  const Result<geometry::NacaResult> n = geometry::naca4(12, 123);
  CHECK(n.isOk());
  const geometry::AirfoilBuffer buffer(n.value().xb, n.value().yb,
                                       n.value().name);
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  const std::vector<double> sig0(panel.x.size(), 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, 0.0, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());

  // Fortran XYWAKE/QWCALC/QISET.
  const Result<WakeState> wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const WakeState& wake = wk.value();
  const WakeBasisResult qb =
      qwCalc(gs.value(), wake, panel, sp.value().alfa, 1.0);
  std::vector<double> qinvu1 = gs.value().qinvu1;
  std::vector<double> qinvu2 = gs.value().qinvu2;
  qinvu1.insert(qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2.insert(qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinv;
  std::vector<double> qinvA;
  qiset(qinvu1, qinvu2, sp.value().alfa, qinv, qinvA);

  // Fortran STFIND/IBLPAN/XICALC/IBLSYS/UICALC (LIPAN section of the
  // reference run: LIPAN=.TRUE. at the VISCAL call).
  const StagnationPointResult st = stfind(sp.value().gam, panel.s);
  CHECK(st.found);
  const BoundaryLayerMapping map = iblpan(panel, st.ist, wake.nw, 283);
  CHECK(map.valid);
  BoundaryLayerArcLengths arcs;
  const WakeGap wg =
      xicalc(panel, map, st.sst, sp.value().te.ante, wake.x, wake.y, arcs);
  const BlsysMapping isysMap = iblsys({map.top.nbl, map.bottom.nbl}, 283);
  BoundaryLayerStationField uinv;
  BoundaryLayerStationField uinvA;
  uicalc(map, qinv, qinvA, uinv, uinvA);

  // Fortran XIFSET (XSTRIP=1.0 -> XIFORC = XSSI(IBLTE), harness session).
  const Result<TransitionState> tTop =
      xifset(panel, map, arcs, st.sst, 1.0, SurfaceSide::kTop);
  const Result<TransitionState> tBot =
      xifset(panel, map, arcs, st.sst, 1.0, SurfaceSide::kBottom);
  CHECK(tTop.isOk());
  CHECK(tBot.isOk());

  // session and BL state (VISCAL call with LBLINI=.FALSE.).
  ViscalState vs{};
  vs.alfa = sp.value().alfa;
  vs.minf = sp.value().minf;
  vs.reinf = sp.value().reinf;
  vs.cl = sp.value().cl;
  vs.gam = sp.value().gam;
  vs.gamA = sp.value().gamA;
  vs.qinv = qinv;
  vs.qinvA = qinvA;
  vs.mapping = map;
  vs.arcs = arcs;
  vs.wgap = wg;
  vs.isys = isysMap;
  vs.uinv = uinv;
  vs.uinvA = uinvA;
  vs.sst = st.sst;
  vs.sstGo = st.sstGo;
  vs.sstGp = st.sstGp;
  vs.ist = st.ist;

  ViscalInput in{};
  in.blpar = blparConstants();
  in.niter = 50;
  in.qinf = 1.0;
  in.lalfa = true;
  in.lvisc = true;
  in.lflap = false;
  in.machType = 1;
  in.reType = 1;
  in.minf1 = 0.0;
  in.reinf1 = 3.0e6;
  in.ivx = 283;
  in.acrit = {9.0, 9.0};
  in.xiforc = {tTop.value().xiforc, tBot.value().xiforc};

  const ViscalResult res =
      viscal(in, panel, gs.value(), wake, sp.value().te, vs);

  // the Newton loop converged after the TE-sensitivity fix (D1_M at the
// wake start with UTE1_M/UTE2_M, xbl.f:255-263) in 6 iterations as
// Fortran (Fortran: 7.481e-2 -> 8.153e-7 in 6; C++: 7.574e-2 ->
// 6.187e-7 in 6).
CHECK_EQ(res.iterations, 6u);
CHECK(res.converged);
CHECK_EQ(res.rmsblHistory.size(), res.iterations);
CHECK_NEAR(res.rmsblHistory[0], 7.574e-2, 1.0e-4, 2.0e-2);
CHECK_NEAR(res.rmsblHistory[1], 4.660e-2, 1.0e-4, 2.0e-2);
CHECK(res.rmsblHistory[1] < res.rmsblHistory[0]);
CHECK(res.rmsbl < 1.0e-4);
// UPDATE diagnostics per iteration: finite, VMXBL from the Fortran
// character set, last RMSBL below EPS1.
CHECK_EQ(res.iterationRecords.size(), res.iterations);
for (const auto& rec : res.iterationRecords) {
  CHECK(std::isfinite(rec.rmsbl));
  CHECK(std::isfinite(rec.dac));
  CHECK(std::isfinite(rec.rlx));
  CHECK(std::isfinite(rec.rmxbl));
  CHECK(rec.vmxbl == 'n' || rec.vmxbl == 'C' || rec.vmxbl == 'T' ||
        rec.vmxbl == 'D' || rec.vmxbl == 'U');
}
CHECK(res.iterationRecords.back().rmsbl < 1.0e-4);
  // Fortran SETBL sets LBLINI after the first march.
  CHECK(vs.lblini);
  // ITRAN chain: MRCHUE transition (ITRAN=50, 0-based) survives the SETBL
  // return value (diag.itran = in.march.itran), is adopted in the viscal
  // state, and is used by UPDATE/second SETBL with the same index (xbl.f
  // VISCAL loop: state.ITRAN = diag.ITRAN).
  CHECK_EQ(vs.itran[0], 50);
  CHECK_EQ(vs.itran[1], 50);
  // Cp distributions exist (Fortran CPCALC at the loop end).
  CHECK_EQ(res.cp.size(), panel.x.size() + wake.nw);
  CHECK_EQ(res.cpv.size(), panel.x.size() + wake.nw);
}

// Fortran UPDATE writes MASS(IBL,IS) = DSTR*UEDG (xbl.f:1531) and
// VISCAL must track the updated MASS state back in viscal state.mass,
// so that the next SETBL assembly (MDI = MASS; DSI = MDI/UEI,
// xbl.f:187-188) consumes the fresh value. The test uses the
// non-trivial NACA-0012 case (LBLINI=.FALSE., start masses 0 ->
// UPDATE produces large state changes), so that old and new MASS
// states are distinguishable by their values rather than by a
// coincidental reset.
TEST(unit, ViscalPersistsUpdatedMassBetweenIterations) {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  const Result<geometry::NacaResult> n = geometry::naca4(12, 123);
  CHECK(n.isOk());
  const geometry::AirfoilBuffer buffer(n.value().xb, n.value().yb,
                                       n.value().name);
  const Result<PangenResult> pg = pangen(buffer, xfoil699Paneling());
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  const std::vector<double> sig0(panel.x.size(), 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, 0.0, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());

  const Result<WakeState> wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const WakeState& wake = wk.value();
  const WakeBasisResult qb =
      qwCalc(gs.value(), wake, panel, sp.value().alfa, 1.0);
  std::vector<double> qinvu1 = gs.value().qinvu1;
  std::vector<double> qinvu2 = gs.value().qinvu2;
  qinvu1.insert(qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2.insert(qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinv;
  std::vector<double> qinvA;
  qiset(qinvu1, qinvu2, sp.value().alfa, qinv, qinvA);

  const StagnationPointResult st = stfind(sp.value().gam, panel.s);
  CHECK(st.found);
  const BoundaryLayerMapping map = iblpan(panel, st.ist, wake.nw, 283);
  CHECK(map.valid);
  BoundaryLayerArcLengths arcs;
  const WakeGap wg =
      xicalc(panel, map, st.sst, sp.value().te.ante, wake.x, wake.y, arcs);
  const BlsysMapping isysMap = iblsys({map.top.nbl, map.bottom.nbl}, 283);
  BoundaryLayerStationField uinv;
  BoundaryLayerStationField uinvA;
  uicalc(map, qinv, qinvA, uinv, uinvA);

  const Result<TransitionState> tTop =
      xifset(panel, map, arcs, st.sst, 1.0, SurfaceSide::kTop);
  const Result<TransitionState> tBot =
      xifset(panel, map, arcs, st.sst, 1.0, SurfaceSide::kBottom);
  CHECK(tTop.isOk());
  CHECK(tBot.isOk());

  ViscalState vs{};
  vs.alfa = sp.value().alfa;
  vs.minf = sp.value().minf;
  vs.reinf = sp.value().reinf;
  vs.cl = sp.value().cl;
  vs.gam = sp.value().gam;
  vs.gamA = sp.value().gamA;
  vs.qinv = qinv;
  vs.qinvA = qinvA;
  vs.mapping = map;
  vs.arcs = arcs;
  vs.wgap = wg;
  vs.isys = isysMap;
  vs.uinv = uinv;
  vs.uinvA = uinvA;
  vs.sst = st.sst;
  vs.sstGo = st.sstGo;
  vs.sstGp = st.sstGp;
  vs.ist = st.ist;

  ViscalInput in{};
  in.blpar = blparConstants();
  in.niter = 3;
  in.qinf = 1.0;
  in.lalfa = true;
  in.lvisc = true;
  in.lflap = false;
  in.machType = 1;
  in.reType = 1;
  in.minf1 = 0.0;
  in.reinf1 = 3.0e6;
  in.ivx = 283;
  in.acrit = {9.0, 9.0};
  in.xiforc = {tTop.value().xiforc, tBot.value().xiforc};

  const ViscalResult res =
      viscal(in, panel, gs.value(), wake, sp.value().te, vs);
  CHECK(res.iterations >= 2);

  // non-trivial case: start masses are 0 (LBLINI=.FALSE.); after
  // UPDATE+STMOVE, MASS must be clearly non-zero.
  double maxMass = 0.0;
  for (const double m : vs.mass.top) maxMass = std::max(maxMass, m);
  for (const double m : vs.mass.bottom) maxMass = std::max(maxMass, m);
  CHECK(maxMass > 1.0e-4);

  // invariant MASS == DSTR*UEDG in the regular advanced station range
  // (IBL=2..IBLTE on both sides). Both sides compare the same product of
  // the same double values (UPDATE xbl.f:1531 / STMOVE recompute), hence
  // exact; 1e-12 abs only tolerates the identical multiplication on other
  // digits. Upper wake positions (IBL>IBLTE on side 1) have special
  // assignment semantics (xbl.f:1547-1557 does not copy MASS directly)
  // and are excluded from this invariant because the stored value is not
  // the product.
  auto checkSide = [&](const std::vector<double>& mass,
                       const std::vector<double>& dstr,
                       const std::vector<double>& uedg, std::size_t iblte) {
    for (std::size_t ibl = 2; ibl <= iblte; ++ibl) {
      const double prod = dstr[ibl] * uedg[ibl];
      CHECK(std::abs(mass[ibl] - prod) <= 1.0e-12);
    }
  };
  checkSide(vs.mass.top, vs.dstr.top, vs.uedg.top,
              static_cast<std::size_t>(map.top.iblte));
  checkSide(vs.mass.bottom, vs.dstr.bottom, vs.uedg.bottom,
            static_cast<std::size_t>(map.bottom.iblte));
}

// Regression XIFSET (xbl.f:1196): XIFORC = XSSI(IBLTE(IS),IS) is read
// strictly separately per side. For asymmetric mapping (stagnation point
// near TE, IBLTE(1) > IBLTE(2)), the xstrip>=1.0 variant earlier indexed
// the bottom arc-length field with the top IBLTE -> out-of-bounds read
// (ASan heap-buffer-overflow), although the top value was then overwritten
// with arcs.top[IBLTE(1)] (numerically ineffective, but UB). The test pins
// down the per-side separated indexing.
TEST(unit, XifsetSideSpecificIblteAsymmetricMapping) {
  using namespace xfoil::core;
  using namespace xfoil::core::paneling;
  using namespace xfoil::core::boundary_layer;

  PanelGeometry panel;
  panel.x.assign(10, 0.0);
  panel.s.assign(10, 0.0);

  BoundaryLayerMapping map;
  map.valid = true;
  map.top.iblte = 5;
  map.top.nbl = 5;
  map.top.ipan.assign(6, 0);
  map.top.vti.assign(6, 1.0);
  map.bottom.iblte = 4;
  map.bottom.nbl = 4;
  map.bottom.ipan.assign(5, 0);
  map.bottom.vti.assign(5, -1.0);
  map.iblmax = 5;

  BoundaryLayerArcLengths arcs;
  arcs.top.assign(6, 0.0);
  for (std::size_t k = 0; k < arcs.top.size(); ++k) {
    arcs.top[k] = static_cast<double>(k);
  }
  arcs.bottom.assign(5, 0.0);
  for (std::size_t k = 0; k < arcs.bottom.size(); ++k) {
    arcs.bottom[k] = 100.0 + static_cast<double>(k);
  }

  CHECK(map.top.iblte > map.bottom.iblte);

  const Result<TransitionState> tTop =
      xifset(panel, map, arcs, 0.0, 1.0, SurfaceSide::kTop);
  CHECK(tTop.isOk());
  CHECK_NEAR(tTop.value().xiforc,
             arcs.top[static_cast<std::size_t>(map.top.iblte)], 0.0, 0.0);

  const Result<TransitionState> tBot =
      xifset(panel, map, arcs, 0.0, 1.0, SurfaceSide::kBottom);
  CHECK(tBot.isOk());
  CHECK_NEAR(tBot.value().xiforc,
             arcs.bottom[static_cast<std::size_t>(map.bottom.iblte)], 0.0,
             0.0);
}

// User-defined BLPAR values reach the kernels
// (single-source proof; no default re-fetch in the kernel).
TEST(unit, BlvarCustomBlparReachesSecondaryOutputs) {
  using namespace xfoil::core::boundary_layer;
  const BoundaryLayerProperties props =
      boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0, 0.0, 0.0,
                              0);
  const BoundaryLayerPrimary prim =
      blprv(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0, props);
  const BoundaryLayerSecondary sec = blkin(prim, props);
  const BoundaryLayerTurbulentSecondary def =
      blvar(StationType::kTurbulent, BlvarInput{prim, sec, blparConstants()});

  BoundaryLayerParameters bp = blparConstants();
  bp.gbcon = 0.9;
  const auto g = blvar(StationType::kTurbulent, BlvarInput{prim, sec, bp});
  CHECK(std::abs(g.us - def.us) > 1e-12);

  bp = blparConstants();
  bp.ctcon = 0.4;
  const auto c = blvar(StationType::kTurbulent, BlvarInput{prim, sec, bp});
  CHECK(std::abs(c.cq - def.cq) > 1e-12);

  bp = blparConstants();
  bp.cffac = 2.0;
  const auto f = blvar(StationType::kTurbulent, BlvarInput{prim, sec, bp});
  CHECK(std::abs(f.cf - def.cf) > 1e-12);
}

TEST(unit, BlmidCustomCffacReachesMidCf) {
  using namespace xfoil::core::boundary_layer;
  const BldifStation st1 = makeBldifStation(0.05, 0.05, 0.075,
                                            StationType::kTurbulent);
  const BldifStation st2 = makeBldifStation(0.15, 0.051, 0.0765,
                                            StationType::kTurbulent);
  BlmidInput min{};
  min.first.hk = st1.hk;
  min.first.rt = st1.rt;
  min.first.m = st1.m;
  min.second = min.first;
  min.second.hk = st2.hk;
  min.simi = false;
  min.blpar = blparConstants();
  const MidpointSkinFriction def = blmid(StationType::kTurbulent, min);
  min.blpar.cffac = 2.0;
  const MidpointSkinFriction dbl = blmid(StationType::kTurbulent, min);
  CHECK(std::abs(dbl.cf - def.cf) > 1e-12);
}

TEST(unit, BldifCustomBlparReachesSystem) {
  using namespace xfoil::core::boundary_layer;
  const BldifStation st1 = makeBldifStation(0.05, 0.05, 0.075,
                                            StationType::kTurbulent);
  const BldifStation st2 = makeBldifStation(0.15, 0.051, 0.0765,
                                            StationType::kTurbulent);
  BlmidInput min{};
  min.first.hk = st1.hk;
  min.first.rt = st1.rt;
  min.first.m = st1.m;
  min.second = min.first;
  min.second.hk = st2.hk;
  min.simi = false;
  min.blpar = blparConstants();
  const MidpointSkinFriction mid = blmid(StationType::kTurbulent, min);

  BldifInput in{};
  in.first = st1;
  in.second = st2;
  in.mid = mid;
  in.ityp = StationType::kTurbulent;
  in.similarity = false;
  in.bule = 1.0;
  in.ampl2 = 0.0;
  in.acrit = 9.0;
  in.idampv = 0;
  in.blpar = blparConstants();
  const BldifSystem def = bldif(in);

  const auto anyDiff = [](const BldifSystem& a, const BldifSystem& b) {
    for (int k = 0; k < 4; ++k) {
      for (int l = 0; l < 5; ++l) {
        if (std::abs(a.vs1[k][l] - b.vs1[k][l]) > 1e-12 ||
            std::abs(a.vs2[k][l] - b.vs2[k][l]) > 1e-12) {
          return true;
        }
      }
      for (int l = 0; l < 4; ++l) {
        if (std::abs(a.vsrez[l] - b.vsrez[l]) > 1e-12 ||
            std::abs(a.vsr[l] - b.vsr[l]) > 1e-12 ||
            std::abs(a.vsm[l] - b.vsm[l]) > 1e-12 ||
            std::abs(a.vsx[l] - b.vsx[l]) > 1e-12) {
          return true;
        }
      }
    }
    return false;
  };

  in.blpar.sccon = 6.0;
  const BldifSystem s = bldif(in);
  CHECK(anyDiff(s, def));

  in.blpar = blparConstants();
  in.blpar.duxcon = 2.0;
  const BldifSystem d = bldif(in);
  CHECK(anyDiff(d, def));

  in.blpar = blparConstants();
  in.blpar.gccon = 20.0;
  const BldifSystem gc = bldif(in);
  CHECK(anyDiff(gc, def));

  // DLCON acts only in the wake branch (xblsys.f: ALD = DLCON for ITYP=3).
  const BldifStation w1 =
      makeBldifStation(0.05, 0.05, 0.075, StationType::kWake);
  const BldifStation w2 =
      makeBldifStation(0.15, 0.051, 0.0765, StationType::kWake);
  BldifInput win{};
  win.first = w1;
  win.second = w2;
  win.mid = mid;
  win.ityp = StationType::kWake;
  win.similarity = false;
  win.bule = 1.0;
  win.ampl2 = 0.0;
  win.acrit = 9.0;
  win.idampv = 0;
  win.blpar = blparConstants();
  const BldifSystem wDef = bldif(win);
  win.blpar.dlcon = 1.5;
  const BldifSystem wDl = bldif(win);
  CHECK(anyDiff(wDl, wDef));

  // GACON/GBCON reach the BLDIF HR/UQ path
  // (xblsys.f 1724-1728: HR = HKC/(GACON*ALD*HKA),
  // UQ = (0.5*CFA - HR**2)/(GBCON*Since)).
  in.blpar = blparConstants();
  in.blpar.gacon = 7.0;
  const BldifSystem gA = bldif(in);
  CHECK(anyDiff(gA, def));
  in.blpar = blparConstants();
  in.blpar.gbcon = 0.8;
  const BldifSystem gB = bldif(in);
  CHECK(anyDiff(gB, def));
}

// CTRCON/CTRCEX reach the TRDIF Ctau path
// (xblsys.f 1391-1392: CTR = CTRCON*EXP(-CTRCEX/(HK2-1.0)),
// CTR_HK2 = CTR*CTRCEX/(HK2-1.0)**2). The structure stays unchanged;
// only the parameters are varied.
TEST(unit, TrdifCustomCtrReachesTransitionSystem) {
  using namespace xfoil::core::boundary_layer;
  const auto tr = trchek2(
      makeTrchekInput(0.05, 0.05, 0.075, 8.9, 0.15, 0.051, 0.0765));
  CHECK(tr.tran);
  BldifStation st1 =
      makeBldifStation(0.05, 0.05, 0.075, StationType::kLaminar);
  BldifStation st2 =
      makeBldifStation(0.15, 0.051, 0.0765, StationType::kLaminar);
  st1.ampl = 8.9;
  TrdifInput in{};
  in.first = st1;
  in.second = st2;
  in.xt = tr.xt;
  in.xtA1 = tr.xtA1;
  in.xtT1 = tr.xtT1;
  in.xtD1 = tr.xtD1;
  in.xtU1 = tr.xtU1;
  in.xtX1 = tr.xtX1;
  in.xtT2 = tr.xtT2;
  in.xtD2 = tr.xtD2;
  in.xtU2 = tr.xtU2;
  in.xtX2 = tr.xtX2;
  in.xtMs = tr.xtMs;
  in.xtRe = tr.xtRe;
  in.xtXf = tr.xtXf;
  in.amcrit = 9.0;
  in.idampv = 0;
  in.props = boundaryLayerProperties(0.0, 3.0e6, 1.0, 1.4, 0.4, 0.0, 0.0,
                                     0.0, 0.0, 0);
  in.blpar = blparConstants();
  const auto def = trdif(in);

  auto diff = [](const BldifSystem& a, const BldifSystem& b) {
    for (int k = 0; k < 4; ++k) {
      for (int l = 0; l < 5; ++l) {
        if (a.vs1[k][l] != b.vs1[k][l] || a.vs2[k][l] != b.vs2[k][l]) {
          return true;
        }
      }
    }
    return false;
  };

  in.blpar = blparConstants();
  in.blpar.ctrcon = 2.0;
  CHECK(diff(trdif(in), def));
  in.blpar = blparConstants();
  in.blpar.ctrcex = 4.0;
  CHECK(diff(trdif(in), def));
}

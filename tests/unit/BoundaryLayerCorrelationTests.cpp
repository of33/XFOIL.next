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

// file: tests/unit/BoundaryLayerCorrelationTests.cpp
// unit tests for the stateless boundary-layer correlations.
// reference values were characterized independently of the
// production formulas (a second implementation of the same
// formulas in double precision, separated from the production code); they
// were not produced by calling the new functions.
//
// finite-difference contract: central differences, step size
// relative to the tested quantity (factor 1e-5), tests only with
// clear distance to branch and clamp limits. Tolerance rationale: the
// error of the central difference consists of the truncation term O(step^2) and
// the rounding term O(eps*|f|/step); for step ~1e-5 the expected
// accuracy lies at 1e-8..1e-6 relative. Reference tests compare against the
// independent double implementation with relative tolerance 1e-10
// (platform libm scatter); finite-difference tests use relative
// tolerance 1e-5, and 1e-4 for the power/exponential terms of cft and axset.

#include "TestSupport.h"

#include "xfoil/boundary_layer/BoundaryLayerCorrelations.h"

#include <cmath>

namespace {

using namespace xfoil::boundary_layer;

double relativeStep(double value) {
  const double magnitude = std::fabs(value);
  return (magnitude > 1.0 ? magnitude : 1.0) * 1.0e-5;
}

}  // namespace

// ---
// Kinematic shape factor (hkin)
// ---

TEST(unit, KinematicShapeFactorMatchesReference) {
  const auto result = kinematicShapeFactor(2.3, 0.45);
  CHECK_NEAR(result.value, 2.0645191987438736, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, 0.9516106009420945, 1e-14, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, -0.4979689484303732, 1e-14, 1e-10);
}

TEST(unit, KinematicShapeFactorDerivativesMatchFiniteDifferences) {
  const double h = 2.3;
  const double msq = 0.45;
  const auto result = kinematicShapeFactor(h, msq);

  const double hh = relativeStep(h);
  const double centralH = (kinematicShapeFactor(h + hh, msq).value -
                           kinematicShapeFactor(h - hh, msq).value) /
                          (2.0 * hh);
  CHECK_NEAR(result.shapeDerivative, centralH, 1e-9, 1e-5);

  const double hm = relativeStep(msq);
  const double centralM = (kinematicShapeFactor(h, msq + hm).value -
                           kinematicShapeFactor(h, msq - hm).value) /
                          (2.0 * hm);
  CHECK_NEAR(result.machSquaredDerivative, centralM, 1e-9, 1e-5);
}

// ---
// Density shape factor (hct)
// ---

TEST(unit, DensityShapeFactorMatchesReference) {
  const auto result = densityShapeFactor(1.8, 0.5);
  CHECK_NEAR(result.value, 0.1575, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.032, 1e-14, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.315, 1e-14, 1e-10);
}

TEST(unit, DensityShapeFactorDerivativesMatchFiniteDifferences) {
  const double hk = 1.8;
  const double msq = 0.5;
  const auto result = densityShapeFactor(hk, msq);

  const double hh = relativeStep(hk);
  const double centralH = (densityShapeFactor(hk + hh, msq).value -
                           densityShapeFactor(hk - hh, msq).value) /
                          (2.0 * hh);
  CHECK_NEAR(result.shapeDerivative, centralH, 1e-9, 1e-5);

  const double hm = relativeStep(msq);
  const double centralM = (densityShapeFactor(hk, msq + hm).value -
                           densityShapeFactor(hk, msq - hm).value) /
                          (2.0 * hm);
  CHECK_NEAR(result.machSquaredDerivative, centralM, 1e-9, 1e-5);
}

// ---
// Laminar Energy-shape-factor (hsl)
// ---

TEST(unit, LaminarEnergyShapeFactorAttachedBranch) {
  // hk < 4.35: attached branch
  const auto result = laminarEnergyShapeFactor(3.2);
  CHECK_NEAR(result.value, 1.5388534426190477, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.03255107205215416, 1e-14, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, 0.0, 0.0, 0.0);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, LaminarEnergyShapeFactorSeparatedBranch) {
  // hk >= 4.35: separated branch
  const auto result = laminarEnergyShapeFactor(5.0);
  CHECK_NEAR(result.value, 1.5292675, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, 0.0036465000000000017, 1e-14, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, 0.0, 0.0, 0.0);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, LaminarEnergyShapeFactorDerivativeMatchesFiniteDifference) {
  const double hkLow = 3.2;
  const auto low = laminarEnergyShapeFactor(hkLow);
  const double h1 = relativeStep(hkLow);
  const double centralLow = (laminarEnergyShapeFactor(hkLow + h1).value -
                             laminarEnergyShapeFactor(hkLow - h1).value) /
                            (2.0 * h1);
  CHECK_NEAR(low.shapeDerivative, centralLow, 1e-9, 1e-5);

  const double hkHigh = 5.0;
  const auto high = laminarEnergyShapeFactor(hkHigh);
  const double h2 = relativeStep(hkHigh);
  const double centralHigh = (laminarEnergyShapeFactor(hkHigh + h2).value -
                              laminarEnergyShapeFactor(hkHigh - h2).value) /
                             (2.0 * h2);
  CHECK_NEAR(high.shapeDerivative, centralHigh, 1e-9, 1e-5);
}

// ---
// Turbulent Energy-shape-factor (hst)
// ---

TEST(unit, TurbulentEnergyShapeFactorAttachedBranch) {
  // rt > 400 and hk < ho = 3.4: attached branch
  const auto result = turbulentEnergyShapeFactor(2.5, 1000.0, 0.3);
  CHECK_NEAR(result.value, 1.5408036247759411, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.08875224058952401, 1e-14, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, -2.299716191993627e-05, 1e-16, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.0064018614351093635, 1e-16, 1e-10);
}

TEST(unit, TurbulentEnergyShapeFactorSeparatedBranch) {
  // rt > 400 and hk > ho = 3.4: separated branch
  const auto result = turbulentEnergyShapeFactor(4.5, 1000.0, 0.3);
  CHECK_NEAR(result.value, 1.5307575051314861, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, 0.019368842856221497, 1e-14, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, 9.176640877902676e-06, 1e-16, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.006541918868909771, 1e-16, 1e-10);
}

TEST(unit, TurbulentEnergyShapeFactorLowReynoldsClampBranch) {
  // rt <= 200: rtz clamped to 200 and ho = 4 (both clamps active)
  const auto result = turbulentEnergyShapeFactor(2.5, 150.0, 0.3);
  CHECK_NEAR(result.value, 1.5817566221868153, 1e-14, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.09958175662218681, 1e-14, 1e-10);
  // Inside the clamps rtz_rt and ho_rt are zero, so hs_rt is too.
  CHECK_NEAR(result.reynoldsDerivative, 0.0, 1e-17, 0.0);
  CHECK_NEAR(result.machSquaredDerivative, 0.005830917436152744, 1e-16, 1e-10);
}

TEST(unit, TurbulentEnergyShapeFactorDerivativesMatchFiniteDifferences) {
  // interior points with distance to branch and clamp limits: rt = 1000
  // (attached branch), rt = 300 (mid range 200 < rt <= 400), and
  // hk = 4.5 in the separated branch for rt = 1000.
  const double hk = 2.5;
  const double msq = 0.3;

  const double rtA = 1000.0;
  const auto a = turbulentEnergyShapeFactor(hk, rtA, msq);
  const double hHa = relativeStep(hk);
  CHECK_NEAR(a.shapeDerivative,
             (turbulentEnergyShapeFactor(hk + hHa, rtA, msq).value -
              turbulentEnergyShapeFactor(hk - hHa, rtA, msq).value) /
                 (2.0 * hHa),
             1e-9, 1e-5);
  const double hRa = relativeStep(rtA);
  CHECK_NEAR(a.reynoldsDerivative,
             (turbulentEnergyShapeFactor(hk, rtA + hRa, msq).value -
              turbulentEnergyShapeFactor(hk, rtA - hRa, msq).value) /
                 (2.0 * hRa),
             1e-9, 1e-5);
  const double hMa = relativeStep(msq);
  CHECK_NEAR(a.machSquaredDerivative,
             (turbulentEnergyShapeFactor(hk, rtA, msq + hMa).value -
              turbulentEnergyShapeFactor(hk, rtA, msq - hMa).value) /
                 (2.0 * hMa),
             1e-9, 1e-5);

  const double rtB = 300.0;
  const auto b = turbulentEnergyShapeFactor(hk, rtB, msq);
  const double hRb = relativeStep(rtB);
  CHECK_NEAR(b.reynoldsDerivative,
             (turbulentEnergyShapeFactor(hk, rtB + hRb, msq).value -
              turbulentEnergyShapeFactor(hk, rtB - hRb, msq).value) /
                 (2.0 * hRb),
             1e-9, 1e-5);

  const double hkSep = 4.5;
  const auto c = turbulentEnergyShapeFactor(hkSep, rtA, msq);
  const double hHc = relativeStep(hkSep);
  CHECK_NEAR(c.shapeDerivative,
             (turbulentEnergyShapeFactor(hkSep + hHc, rtA, msq).value -
              turbulentEnergyShapeFactor(hkSep - hHc, rtA, msq).value) /
                 (2.0 * hHc),
             1e-9, 1e-5);
}

// ---
// Skin friction (cfl, cft)
// ---

TEST(unit, LaminarSkinFrictionLowerShapeBranch) {
  // hk < 5.5
  const auto result = laminarSkinFriction(3.0, 300.0);
  CHECK_NEAR(result.value, 0.00071328125, 1e-15, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.0013725911458333335, 1e-15, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, -2.377604166666667e-06, 1e-17, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, LaminarSkinFrictionUpperShapeBranch) {
  // hk > 5.5
  const auto result = laminarSkinFriction(6.0, 300.0);
  CHECK_NEAR(result.value, -0.0002277777777777778, 1e-16, 1e-10);
  CHECK_NEAR(result.shapeDerivative, 1.4814814814814815e-05, 1e-17, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, 7.592592592592593e-07, 1e-18, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, LaminarSkinFrictionDerivativesMatchFiniteDifferences) {
  const double rt = 300.0;

  const double hkLow = 3.0;
  const auto low = laminarSkinFriction(hkLow, rt);
  const double h1 = relativeStep(hkLow);
  CHECK_NEAR(low.shapeDerivative,
             (laminarSkinFriction(hkLow + h1, rt).value -
              laminarSkinFriction(hkLow - h1, rt).value) /
                 (2.0 * h1),
             1e-9, 1e-5);
  const double r1 = relativeStep(rt);
  CHECK_NEAR(low.reynoldsDerivative,
             (laminarSkinFriction(hkLow, rt + r1).value -
              laminarSkinFriction(hkLow, rt - r1).value) /
                 (2.0 * r1),
             1e-9, 1e-5);

  const double hkHigh = 6.0;
  const auto high = laminarSkinFriction(hkHigh, rt);
  const double h2 = relativeStep(hkHigh);
  CHECK_NEAR(high.shapeDerivative,
             (laminarSkinFriction(hkHigh + h2, rt).value -
              laminarSkinFriction(hkHigh - h2, rt).value) /
                 (2.0 * h2),
             1e-9, 1e-5);
  const double r2 = relativeStep(rt);
  CHECK_NEAR(high.reynoldsDerivative,
             (laminarSkinFriction(hkHigh, rt + r2).value -
              laminarSkinFriction(hkHigh, rt - r2).value) /
                 (2.0 * r2),
             1e-9, 1e-5);
}

TEST(unit, TurbulentSkinFrictionMatchesReference) {
  const auto result = turbulentSkinFriction(1.8, 2000.0, 0.3);
  CHECK_NEAR(result.value, 0.0017205456107526485, 1e-16, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.0029401755856085293, 1e-16, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, -2.6174161806165696e-07, 1e-19, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, -0.00011293041270088062, 1e-17, 1e-10);
}

TEST(unit, TurbulentSkinFrictionDerivativesMatchFiniteDifferences) {
  // grt = log(rt/fc) ~ 7.5 lies clear via the Clamping for 3;
  // f_arg = -2.39 lies clear via the Clamping for -20.
  const double hk = 1.8;
  const double rt = 2000.0;
  const double msq = 0.3;
  const auto result = turbulentSkinFriction(hk, rt, msq);

  const double h = relativeStep(hk);
  CHECK_NEAR(result.shapeDerivative,
             (turbulentSkinFriction(hk + h, rt, msq).value -
              turbulentSkinFriction(hk - h, rt, msq).value) /
                 (2.0 * h),
             1e-9, 1e-4);
  const double r = relativeStep(rt);
  CHECK_NEAR(result.reynoldsDerivative,
             (turbulentSkinFriction(hk, rt + r, msq).value -
              turbulentSkinFriction(hk, rt - r, msq).value) /
                 (2.0 * r),
             1e-9, 1e-4);
  const double m = relativeStep(msq);
  CHECK_NEAR(result.machSquaredDerivative,
             (turbulentSkinFriction(hk, rt, msq + m).value -
              turbulentSkinFriction(hk, rt, msq - m).value) /
                 (2.0 * m),
             1e-9, 1e-4);
}

// ---
// Dissipation (dil, dilw)
// ---

TEST(unit, LaminarDissipationLowerShapeBranch) {
  // hk < 4.0
  const auto result = laminarDissipation(3.0, 350.0);
  CHECK_NEAR(result.value, 0.0005972857142857143, 1e-16, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -3.221428571428571e-05, 1e-17, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, -1.706530612244898e-06, 1e-18, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, LaminarDissipationUpperShapeBranch) {
  // hk > 4.0
  const auto result = laminarDissipation(5.0, 350.0);
  CHECK_NEAR(result.value, 0.0005869467787114846, 1e-16, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -8.787828857033009e-06, 1e-17, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, -1.6769907963185274e-06, 1e-18, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, LaminarDissipationDerivativesMatchFiniteDifferences) {
  const double rt = 350.0;

  const double hkLow = 3.0;
  const auto low = laminarDissipation(hkLow, rt);
  const double h1 = relativeStep(hkLow);
  CHECK_NEAR(low.shapeDerivative,
             (laminarDissipation(hkLow + h1, rt).value -
              laminarDissipation(hkLow - h1, rt).value) /
                 (2.0 * h1),
             1e-9, 1e-5);
  const double r1 = relativeStep(rt);
  CHECK_NEAR(low.reynoldsDerivative,
             (laminarDissipation(hkLow, rt + r1).value -
              laminarDissipation(hkLow, rt - r1).value) /
                 (2.0 * r1),
             1e-9, 1e-5);

  const double hkHigh = 5.0;
  const auto high = laminarDissipation(hkHigh, rt);
  const double h2 = relativeStep(hkHigh);
  CHECK_NEAR(high.shapeDerivative,
             (laminarDissipation(hkHigh + h2, rt).value -
              laminarDissipation(hkHigh - h2, rt).value) /
                 (2.0 * h2),
             1e-9, 1e-5);
}

TEST(unit, LaminarWakeDissipationMatchesReference) {
  const auto result = laminarWakeDissipation(2.5, 400.0);
  CHECK_NEAR(result.value, 0.0004997263865079969, 1e-17, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.0004318673176056153, 1e-17, 1e-10);
  CHECK_NEAR(result.reynoldsDerivative, -1.2493159662699921e-06, 1e-18, 1e-10);
  CHECK_NEAR(result.machSquaredDerivative, 0.0, 0.0, 0.0);
}

// ---
// Amplification (dampl, axset)
// ---

TEST(unit, AmplificationRateBelowCriticalReynoldsIsZero) {
  // hk = 2: grcrit = 3.8919; rt = 100 -> log10(rt) = 2 < grcrit - 0.08
  const auto result = amplificationRate(2.0, 0.05, 100.0);
  CHECK_NEAR(result.value, 0.0, 0.0, 0.0);
  CHECK_NEAR(result.shapeDerivative, 0.0, 0.0, 0.0);
  CHECK_NEAR(result.momentumThicknessDerivative, 0.0, 0.0, 0.0);
  CHECK_NEAR(result.thetaReynoldsDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, AmplificationRateRampBranch) {
  // log10(rt) = grcrit -> rnorm = 0.5 (centered on the common ramp)
  const auto result = amplificationRate(2.0, 0.05, 7796.457036773234);
  CHECK_NEAR(result.value, 0.03363610328991881, 1e-16, 1e-10);
  CHECK_NEAR(result.shapeDerivative, 0.4752589575553147, 1e-15, 1e-10);
  CHECK_NEAR(result.momentumThicknessDerivative, -0.6727220657983762, 1e-15, 1e-10);
  CHECK_NEAR(result.thetaReynoldsDerivative, 3.5131279674998004e-05, 1e-18, 1e-10);
}

TEST(unit, AmplificationRateFullBranch) {
  // log10(rt) = grcrit + 0.1 -> rnorm > 1: full envelope without ramp factor,
  // so the Reynolds derivative is exactly zero.
  const auto result = amplificationRate(2.0, 0.05, 9815.157885555278);
  CHECK_NEAR(result.value, 0.06727220657983754, 1e-16, 1e-10);
  CHECK_NEAR(result.shapeDerivative, -0.4047256774865202, 1e-15, 1e-10);
  CHECK_NEAR(result.momentumThicknessDerivative, -1.3454441315967507, 1e-15, 1e-10);
  CHECK_NEAR(result.thetaReynoldsDerivative, 0.0, 0.0, 0.0);
}

TEST(unit, AmplificationRateDerivativesMatchFiniteDifferences) {
  // ramp interior point (rnorm = 0.5): all three derivatives nonzero and
  // sufficiently far from the ramp and criticality limits.
  const double hk = 2.0;
  const double th = 0.05;
  const double rt = 7796.457036773234;
  const auto result = amplificationRate(hk, th, rt);

  const double h = relativeStep(hk);
  CHECK_NEAR(result.shapeDerivative,
             (amplificationRate(hk + h, th, rt).value -
              amplificationRate(hk - h, th, rt).value) /
                 (2.0 * h),
             1e-8, 1e-4);

  const double t = relativeStep(th);
  CHECK_NEAR(result.momentumThicknessDerivative,
             (amplificationRate(hk, th + t, rt).value -
              amplificationRate(hk, th - t, rt).value) /
                 (2.0 * t),
             1e-8, 1e-4);

  const double r = relativeStep(rt);
  CHECK_NEAR(result.thetaReynoldsDerivative,
             (amplificationRate(hk, th, rt + r).value -
              amplificationRate(hk, th, rt - r).value) /
                 (2.0 * r),
             1e-9, 1e-4);
}

TEST(unit, AverageAmplificationRateMatchesReference) {
  // station 1 on the ramp, station 2 in the full branch; exponential
  // extra correction active (acrit = 9, 0.5*(a1+a2) = 8.75 -> f_arg = 5).
  const auto result = averageAmplificationRate(
      /*firstKinematicShapeFactor=*/2.0,
      /*firstMomentumThickness=*/0.05,
      /*firstThetaReynolds=*/7796.457036773234,
      /*firstAmplification=*/8.0,
      /*secondKinematicShapeFactor=*/2.2,
      /*secondMomentumThickness=*/0.06,
      /*secondThetaReynolds=*/6212.334445334836,
      /*secondAmplification=*/9.5,
      /*criticalAmplification=*/9.0);
  CHECK_NEAR(result.value, 0.02955730379842593, 1e-16, 1e-10);
  CHECK_NEAR(result.firstShapeDerivative, 0.2715469739347842, 1e-15, 1e-10);
  CHECK_NEAR(result.firstMomentumThicknessDerivative, -0.385484458789528, 1e-15, 1e-10);
  CHECK_NEAR(result.firstThetaReynoldsDerivative, 2.007283089470644e-05, 1e-18, 1e-10);
  CHECK_NEAR(result.firstAmplificationDerivative, 0.001225081272560994, 1e-17, 1e-10);
  CHECK_NEAR(result.secondShapeDerivative, -0.028564693353483914, 1e-16, 1e-10);
  CHECK_NEAR(result.secondMomentumThicknessDerivative, -0.17138468098249215, 1e-16, 1e-10);
  // station 2 in the full branch: no Reynolds dependency.
  CHECK_NEAR(result.secondThetaReynoldsDerivative, 0.0, 0.0, 0.0);
  CHECK_NEAR(result.secondAmplificationDerivative, 0.001225081272560994, 1e-17, 1e-10);
}

TEST(unit, AverageAmplificationRateDerivativesMatchFiniteDifferences) {
  // the same interior point as in the reference test; all eight derivatives vs.
  // central differences. Tolerance 1e-4 relative due to power/exponential
  // terms and ramp sensitivity (see file header).
  const double hk1 = 2.0, t1 = 0.05, rt1 = 7796.457036773234, a1 = 8.0;
  const double hk2 = 2.2, t2 = 0.06, rt2 = 6212.334445334836, a2 = 9.5;
  const double acrit = 9.0;
  const auto result = averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2, rt2, a2,
                                               acrit);

  const double h1 = relativeStep(hk1);
  CHECK_NEAR(result.firstShapeDerivative,
             (averageAmplificationRate(hk1 + h1, t1, rt1, a1, hk2, t2, rt2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1 - h1, t1, rt1, a1, hk2, t2, rt2, a2,
                                       acrit)
                      .value) /
                 (2.0 * h1),
             1e-8, 1e-4);

  const double s1 = relativeStep(t1);
  CHECK_NEAR(result.firstMomentumThicknessDerivative,
             (averageAmplificationRate(hk1, t1 + s1, rt1, a1, hk2, t2, rt2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1 - s1, rt1, a1, hk2, t2, rt2, a2,
                                       acrit)
                      .value) /
                 (2.0 * s1),
             1e-8, 1e-4);

  const double r1 = relativeStep(rt1);
  CHECK_NEAR(result.firstThetaReynoldsDerivative,
             (averageAmplificationRate(hk1, t1, rt1 + r1, a1, hk2, t2, rt2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1, rt1 - r1, a1, hk2, t2, rt2, a2,
                                       acrit)
                      .value) /
                 (2.0 * r1),
             1e-9, 1e-4);

  const double b1 = relativeStep(a1);
  CHECK_NEAR(result.firstAmplificationDerivative,
             (averageAmplificationRate(hk1, t1, rt1, a1 + b1, hk2, t2, rt2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1, rt1, a1 - b1, hk2, t2, rt2, a2,
                                       acrit)
                      .value) /
                 (2.0 * b1),
             1e-8, 1e-4);

  const double h2 = relativeStep(hk2);
  CHECK_NEAR(result.secondShapeDerivative,
             (averageAmplificationRate(hk1, t1, rt1, a1, hk2 + h2, t2, rt2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1, rt1, a1, hk2 - h2, t2, rt2, a2,
                                       acrit)
                      .value) /
                 (2.0 * h2),
             1e-8, 1e-4);

  const double s2 = relativeStep(t2);
  CHECK_NEAR(result.secondMomentumThicknessDerivative,
             (averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2 + s2, rt2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2 - s2, rt2, a2,
                                       acrit)
                      .value) /
                 (2.0 * s2),
             1e-8, 1e-4);

  const double r2 = relativeStep(rt2);
  CHECK_NEAR(result.secondThetaReynoldsDerivative,
             (averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2, rt2 + r2, a2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2, rt2 - r2, a2,
                                       acrit)
                      .value) /
                 (2.0 * r2),
             1e-9, 1e-4);

  const double b2 = relativeStep(a2);
  CHECK_NEAR(result.secondAmplificationDerivative,
             (averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2, rt2, a2 + b2,
                                       acrit)
                      .value -
              averageAmplificationRate(hk1, t1, rt1, a1, hk2, t2, rt2, a2 - b2,
                                       acrit)
                      .value) /
                 (2.0 * b2),
             1e-8, 1e-4);
}

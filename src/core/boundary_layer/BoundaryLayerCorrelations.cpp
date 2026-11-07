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

// File: src/core/boundary_layer/BoundaryLayerCorrelations.cpp
// Implementation of the stateless boundary-layer correlations.
// Each function is a formula-faithful adoption of the respective
// Fortran routine: constants, branch conditions and
// order of computations are unchanged.

#include "xfoil/boundary_layer/BoundaryLayerCorrelations.h"

#include <algorithm>
#include <cmath>

namespace xfoil::boundary_layer {

CorrelationValue laminarSkinFriction(
    double kinematicShapeFactor,
    double thetaReynolds) noexcept {
 // --- laminar skin-friction function (cf) (Falkner-Skan)
  const double hk = kinematicShapeFactor;
  const double rt = thetaReynolds;
  CorrelationValue result{};
  double tmp;
  if (hk < 5.5) {
    tmp = (5.5 - hk) * (5.5 - hk) * (5.5 - hk) / (hk + 1.0);
    result.value = (0.0727 * tmp - 0.07) / rt;
    result.shapeDerivative =
        (-.0727 * tmp * 3.0 / (5.5 - hk) - 0.0727 * tmp / (hk + 1.0)) / rt;
  } else {
    tmp = 1.0 - 1.0 / (hk - 4.5);
    result.value = (0.015 * tmp * tmp - 0.07) / rt;
    result.shapeDerivative =
        (0.015 * tmp * 2.0 / (hk - 4.5) / (hk - 4.5)) / rt;
  }
  result.reynoldsDerivative = -result.value / rt;
  result.machSquaredDerivative = 0.0;
  return result;
}

CorrelationValue turbulentSkinFriction(
    double kinematicShapeFactor,
    double thetaReynolds,
    double machSquared) noexcept {
  const double hk = kinematicShapeFactor;
  const double rt = thetaReynolds;
  const double msq = machSquared;
  double gam, gm1, f_arg, fc, grt, gex, thk, cfo;
  gam = 1.4;

 // --- turbulent skin-friction function (cf) (Coles)
  gm1 = gam - 1.0;
  fc = sqrt(1.0 + 0.5 * gm1 * msq);
  grt = log(rt / fc);
  grt = std::max(grt, 3.0);

  gex = -1.74 - 0.31 * hk;

  f_arg = -1.33 * hk;
  f_arg = std::max(-20.0, f_arg);

  thk = tanh(4.0 - hk / 0.875);

  cfo = 0.3 * exp(f_arg) * pow((grt / 2.3026), gex);
  CorrelationValue result{};
  result.value = (cfo + 0.00011 * (thk - 1.0)) / fc;
  result.shapeDerivative =
      (-1.33 * cfo - 0.31 * log(grt / 2.3026) * cfo -
       0.00011 * (1.0 - thk * thk) / 0.875) /
      fc;
  result.reynoldsDerivative = gex * cfo / (fc * grt) / rt;
  result.machSquaredDerivative = gex * cfo / (fc * grt) * (-0.25 * gm1 / fc / fc) -
       0.25 * gm1 * (result.value) / fc / fc;
  return result;
}

CorrelationValue laminarDissipation(
    double kinematicShapeFactor,
    double thetaReynolds) noexcept {
  const double hk = kinematicShapeFactor;
  const double rt = thetaReynolds;
  CorrelationValue result{};
  if (hk < 4.0) {
    result.value = (0.00205 * pow((4.0 - hk), 5.5) + 0.207) / rt;
    result.shapeDerivative =
        (-.00205 * 5.5 * pow((4.0 - hk), 4.5)) / rt;
  } else {
    double hkb = hk - 4.0;
    double den = 1.0 + 0.02 * hkb * hkb;
    result.value = (-.0016 * hkb * hkb / den + 0.207) / rt;
    result.shapeDerivative =
        (-.0016 * 2.0 * hkb * (1.0 / den - 0.02 * hkb * hkb / den / den)) / rt;
  }
  result.reynoldsDerivative = -(result.value) / rt;
  result.machSquaredDerivative = 0.0;
  return result;
}

CorrelationValue laminarWakeDissipation(
    double kinematicShapeFactor,
    double thetaReynolds) noexcept {
  const double hk = kinematicShapeFactor;
  const double rt = thetaReynolds;

  // --- laminar hs correlation (unchanged to the first digit, as in dilw)
  const CorrelationValue hsResult = laminarEnergyShapeFactor(hk);
  const double hs = hsResult.value;
  const double hs_hk = hsResult.shapeDerivative;
  const double hs_rt = hsResult.reynoldsDerivative;

 // --- laminar wake dissipation function (2 cd/h*)
  double rcd = 1.10 * (1.0 - 1.0 / hk) * (1.0 - 1.0 / hk) / hk;
  double rcd_hk = -1.10 * (1.0 - 1.0 / hk) * 2.0 / hk / hk / hk - rcd / hk;

  CorrelationValue result{};
  result.value = 2.0 * rcd / (hs * rt);
  result.shapeDerivative = 2.0 * rcd_hk / (hs * rt) - ((result.value) / hs) * hs_hk;
  result.reynoldsDerivative = -(result.value) / rt - ((result.value) / hs) * hs_rt;
  result.machSquaredDerivative = 0.0;
  return result;
}

KinematicShapeFactorResult kinematicShapeFactor(
    double shapeFactor,
    double machSquared) noexcept {
  const double h = shapeFactor;
  const double msq = machSquared;
  // --- calculate kinematic shape parameter (assuming air)
 // (from Whitfield)
  KinematicShapeFactorResult result{};
  result.value = (h - 0.29 * msq) / (1.0 + 0.113 * msq);
  result.shapeDerivative = 1.0 / (1.0 + 0.113 * msq);
  result.machSquaredDerivative = (-.29 - 0.113 * (result.value)) / (1.0 + 0.113 * msq);
  return result;
}

DensityShapeFactorResult densityShapeFactor(
    double kinematicShapeFactor,
    double machSquared) noexcept {
  const double hk = kinematicShapeFactor;
  const double msq = machSquared;
  // --- density shape parameter (from whitfield)
  DensityShapeFactorResult result{};
  result.value = msq * (0.064 / (hk - 0.8) + 0.251);
  result.shapeDerivative = msq * (-.064 / (hk - 0.8) / (hk - 0.8));
  result.machSquaredDerivative = 0.064 / (hk - 0.8) + 0.251;
  return result;
}

CorrelationValue laminarEnergyShapeFactor(
    double kinematicShapeFactor) noexcept {
  const double hk = kinematicShapeFactor;
  // --- laminar hs correlation
  CorrelationValue result{};
  double tmp;
  if (hk < 4.35) {
    tmp = hk - 4.35;
    result.value = 0.0111 * tmp * tmp / (hk + 1.0) -
         0.0278 * tmp * tmp * tmp / (hk + 1.0) + 1.528 -
         0.0002 * (tmp * hk) * (tmp * hk);
    result.shapeDerivative =
        0.0111 * (2.0 * tmp - tmp * tmp / (hk + 1.0)) / (hk + 1.0) -
        0.0278 * (3.0 * tmp * tmp - tmp * tmp * tmp / (hk + 1.0)) / (hk + 1.0) -
        0.0002 * 2.0 * tmp * hk * (tmp + hk);
  } else {
    result.value = 0.015 * (hk - 4.35) * (hk - 4.35) / hk + 1.528;
    result.shapeDerivative = 0.015 * 2.0 * (hk - 4.35) / hk -
            0.015 * (hk - 4.35) * (hk - 4.35) / hk / hk;
  }

  result.reynoldsDerivative = 0.0;
  result.machSquaredDerivative = 0.0;
  return result;
}

CorrelationValue turbulentEnergyShapeFactor(
    double kinematicShapeFactor,
    double thetaReynolds,
    double machSquared) noexcept {
  const double hk = kinematicShapeFactor;
  const double rt = thetaReynolds;
  const double msq = machSquared;
  double hsmin, dhsinf, rtz, rtz_rt, ho, ho_rt, hr, hr_hk, hr_rt, fm;
  double grt, hdif, rtmp, htmp, htmp_hk, htmp_rt;

  // --- turbulent hs correlation

  hsmin = 1.5;
  dhsinf = 0.015;

  // --- ### 12/4/94
  // --- limited rtheta dependence for rtheta < 200

  if (rt > 400.0) {
    ho = 3.0 + 400.0 / rt;
    ho_rt = -400.0 / rt / rt;
  } else {
    ho = 4.0;
    ho_rt = 0.0;
  }

  if (rt > 200.0) {
    rtz = rt;
    rtz_rt = 1.0;
  } else {
    rtz = 200.0;
    rtz_rt = 0.0;
  }

  CorrelationValue result{};
  if (hk < ho) {
    // --- attached branch
    // --- new correlation 29 nov 91
    //- (from arctan(y+) and Schlichting profiles)
    hr = (ho - hk) / (ho - 1.0);
    hr_hk = -1.0 / (ho - 1.0);
    hr_rt = (1.0 - hr) / (ho - 1.0) * ho_rt;
    result.value = (2.0 - hsmin - 4.0 / rtz) * hr * hr * 1.5 / (hk + 0.5) + hsmin +
         4.0 / rtz;
    result.shapeDerivative =
        -(2.0 - hsmin - 4.0 / rtz) * hr * hr * 1.5 / (hk + 0.5) / (hk + 0.5) +
        (2.0 - hsmin - 4.0 / rtz) * hr * 2.0 * 1.5 / (hk + 0.5) * hr_hk;
    result.reynoldsDerivative =
        (2.0 - hsmin - 4.0 / rtz) * hr * 2.0 * 1.5 / (hk + 0.5) * hr_rt +
            (hr * hr * 1.5 / (hk + 0.5) - 1.0) * 4.0 / rtz / rtz * rtz_rt;
  } else {
    // --- separated branch
    grt = log(rtz);
    hdif = hk - ho;
    rtmp = hk - ho + 4.0 / grt;
    htmp = 0.007 * grt / rtmp / rtmp + dhsinf / hk;
    htmp_hk = -.014 * grt / rtmp / rtmp / rtmp - dhsinf / hk / hk;
    htmp_rt = -.014 * grt / rtmp / rtmp / rtmp *
                  (-ho_rt - 4.0 / grt / grt / rtz * rtz_rt) +
              0.007 / rtmp / rtmp / rtz * rtz_rt;
    result.value = hdif * hdif * htmp + hsmin + 4.0 / rtz;
    result.shapeDerivative = hdif * 2.0 * htmp + hdif * hdif * htmp_hk;
    result.reynoldsDerivative = hdif * hdif * htmp_rt - 4.0 / rtz / rtz * rtz_rt +
            hdif * 2.0 * htmp * (-ho_rt);
  }

  // --- whitfield's minor additional compressibility correction
  fm = 1.0 + 0.014 * msq;
  result.value = (result.value + 0.028 * msq) / fm;
  result.shapeDerivative = (result.shapeDerivative) / fm;
  result.reynoldsDerivative = (result.reynoldsDerivative) / fm;
  result.machSquaredDerivative = 0.028 / fm - 0.014 * (result.value) / fm;
  return result;
}

AmplificationRateResult amplificationRate(
    double kinematicShapeFactor,
    double momentumThickness,
    double thetaReynolds) noexcept {
  const double hk = kinematicShapeFactor;
  const double th = momentumThickness;
  const double rt = thetaReynolds;
  double dgr = 0.08;

  double hmi = 0.0, hmi_hk = 0.0, aa = 0.0, aa_hk = 0.0, bb = 0.0, bb_hk = 0.0,
         grcrit = 0.0, grc_hk = 0.0, gr = 0.0, gr_rt = 0.0;
  double rnorm = 0.0, rn_hk = 0.0, rn_rt = 0.0, rfac = 0.0, rfac_hk = 0.0,
         rfac_rt = 0.0;
  double rfac_rn = 0.0, arg_hk = 0.0, ex = 0.0, f_arg = 0.0, ex_hk = 0.0;
  double af = 0.0, af_hmi = 0.0, af_hk = 0.0, dadr = 0.0, dadr_hk = 0.0;

  hmi = 1.0 / (hk - 1.0);
  hmi_hk = -hmi * hmi;

  // --- log10(critical Rtheta)-h correlation for Falkner-Skan profiles
  aa = 2.492 * pow(hmi, 0.43);
  aa_hk = (aa / hmi) * 0.43 * hmi_hk;
  bb = tanh(14.0 * hmi - 9.24);
  bb_hk = (1.0 - bb * bb) * 14.0 * hmi_hk;
  grcrit = aa + 0.7 * (bb + 1.0);
  grc_hk = aa_hk + 0.7 * bb_hk;
  gr = log10(rt);
  gr_rt = 1.0 / (2.3025851 * rt);
  AmplificationRateResult result{};
  if (gr < grcrit - dgr) {
    // --- no amplification for rtheta < rcrit
    result.value = 0.0;
    result.shapeDerivative = 0.0;
    result.momentumThicknessDerivative = 0.0;
    result.thetaReynoldsDerivative = 0.0;
  } else {
    // --- set steep cubic ramp used to turn on ax smoothly as rtheta
    //- exceeds rcrit (previously, this was done discontinuously).
    //- the ramp goes between -dgr < log10(rtheta/rcrit) < dgr

    rnorm = (gr - (grcrit - dgr)) / (2.0 * dgr);
    rn_hk = -grc_hk / (2.0 * dgr);
    rn_rt = gr_rt / (2.0 * dgr);

    if (rnorm >= 1.0) {
      rfac = 1.0;
      rfac_hk = 0.0;
      rfac_rt = 0.0;
    } else {
      rfac = 3.0 * rnorm * rnorm - 2.0 * rnorm * rnorm * rnorm;
      rfac_rn = 6.0 * rnorm - 6.0 * rnorm * rnorm;

      rfac_hk = rfac_rn * rn_hk;
      rfac_rt = rfac_rn * rn_rt;
    }

    // --- amplification envelope slope correlation for Falkner-Skan
    f_arg = 3.87 * hmi - 2.52;
    arg_hk = 3.87 * hmi_hk;

    ex = exp(-f_arg * f_arg);
    ex_hk = ex * (-2.0 * f_arg * arg_hk);

    dadr = 0.028 * (hk - 1.0) - 0.0345 * ex;
    dadr_hk = 0.028 - 0.0345 * ex_hk;

    // --- new m(h) correlation 1 march 91
    af = -0.05 + 2.7 * hmi - 5.5 * hmi * hmi + 3.0 * hmi * hmi * hmi;
    af_hmi = 2.7 - 11.0 * hmi + 9.0 * hmi * hmi;
    af_hk = af_hmi * hmi_hk;

    result.value = (af * dadr / th) * rfac;
    result.shapeDerivative = (af_hk * dadr / th + af * dadr_hk / th) * rfac +
            (af * dadr / th) * rfac_hk;
    result.momentumThicknessDerivative = -(result.value) / th;
    result.thetaReynoldsDerivative = (af * dadr / th) * rfac_rt;
  }
  return result;
}

AverageAmplificationRateResult averageAmplificationRate(
    double firstKinematicShapeFactor,
    double firstMomentumThickness,
    double firstThetaReynolds,
    double firstAmplification,
    double secondKinematicShapeFactor,
    double secondMomentumThickness,
    double secondThetaReynolds,
    double secondAmplification,
    double criticalAmplification) noexcept {
  const double hk1 = firstKinematicShapeFactor;
  const double t1 = firstMomentumThickness;
  const double rt1 = firstThetaReynolds;
  const double a1 = firstAmplification;
  const double hk2 = secondKinematicShapeFactor;
  const double t2 = secondMomentumThickness;
  const double rt2 = secondThetaReynolds;
  const double a2 = secondAmplification;
  const double acrit = criticalAmplification;

  //
  //==========================
  // --- 2nd-order
  double ax1 = 0.0, ax2 = 0.0, ax1_hk1 = 0.0, ax1_t1 = 0.0, ax1_rt1 = 0.0;
  double ax2_hk2 = 0.0, ax2_t2 = 0.0, ax2_rt2 = 0.0, axsq = 0.0;
  double axa = 0.0, axa_ax1 = 0.0, axa_ax2 = 0.0;
  double exn = 0.0, exn_a1 = 0.0, exn_a2 = 0.0, dax = 0.0, dax_a1 = 0.0,
         dax_a2 = 0.0, dax_t1 = 0.0, dax_t2 = 0.0;
  double f_arg = 0.0;  // ex arg

  const AmplificationRateResult dampl1 = amplificationRate(hk1, t1, rt1);
  ax1 = dampl1.value;
  ax1_hk1 = dampl1.shapeDerivative;
  ax1_t1 = dampl1.momentumThicknessDerivative;
  ax1_rt1 = dampl1.thetaReynoldsDerivative;

  const AmplificationRateResult dampl2 = amplificationRate(hk2, t2, rt2);
  ax2 = dampl2.value;
  ax2_hk2 = dampl2.shapeDerivative;
  ax2_t2 = dampl2.momentumThicknessDerivative;
  ax2_rt2 = dampl2.thetaReynoldsDerivative;

  // --- rms-average version (seems a little better on coarse grids)
  axsq = 0.5 * (ax1 * ax1 + ax2 * ax2);
  if (axsq <= 0.0) {
    axa = 0.0;
    axa_ax1 = 0.0;
    axa_ax2 = 0.0;
  } else {
    axa = sqrt(axsq);
    axa_ax1 = 0.5 * ax1 / axa;
    axa_ax2 = 0.5 * ax2 / axa;
  }

  // --- small additional term to ensure dn/dx > 0 near n = ncrit
  f_arg = std::min(20.0 * (acrit - 0.5 * (a1 + a2)), 20.0);
  if (f_arg <= 0.0) {
    exn = 1.0;
    exn_a1 = 0.0;
    exn_a2 = 0.0;
  } else {
    exn = exp(-f_arg);
    exn_a1 = 20.0 * 0.5 * exn;
    exn_a2 = 20.0 * 0.5 * exn;
  }

  dax = exn * 0.002 / (t1 + t2);
  dax_a1 = exn_a1 * 0.002 / (t1 + t2);
  dax_a2 = exn_a2 * 0.002 / (t1 + t2);
  dax_t1 = -dax / (t1 + t2);
  dax_t2 = -dax / (t1 + t2);

  //==========================

  AverageAmplificationRateResult result{};
  result.value = axa + dax;
  result.firstShapeDerivative = axa_ax1 * ax1_hk1;
  result.firstMomentumThicknessDerivative = axa_ax1 * ax1_t1 + dax_t1;
  result.firstThetaReynoldsDerivative = axa_ax1 * ax1_rt1;
  result.firstAmplificationDerivative = dax_a1;

  result.secondShapeDerivative = axa_ax2 * ax2_hk2;
  result.secondMomentumThicknessDerivative = axa_ax2 * ax2_t2 + dax_t2;
  result.secondThetaReynoldsDerivative = axa_ax2 * ax2_rt2;
  result.secondAmplificationDerivative = dax_a2;
  return result;
}

}  // namespace xfoil::boundary_layer

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

// File: tests/unit/InviscidAnalysisTests.cpp
// unit tests of the stateless inviscid computation cores.
// the new functions are tested directly via the module header; the
// existing NACA and CLARK-Y regressions still check the
// XFoil wrapper. Reference values were obtained independently of the new
// functions, computed directly from the formulas of the original
// Fortran sources, and are documented as concrete numbers.

#include "TestSupport.h"
#include "xfoil/inviscid/InviscidAnalysis.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace {

using xfoil::inviscid::calculateCompressibilityState;
using xfoil::inviscid::calculatePressureCoefficients;
using xfoil::inviscid::combineUnitVelocityDistributions;
using xfoil::inviscid::CompressibilityState;
using xfoil::inviscid::InviscidStatus;
using xfoil::inviscid::integratePressureLoads;
using xfoil::inviscid::SurfaceVelocityResult;
using xfoil::inviscid::UnitVelocityRows;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Fill value for reserved and inactive output positions; it must never
// be changed by the functions.
constexpr double kSentinel = 12345.0;

// Builds local row storage over constant row spans (column 0
// reserved, columns 1 and 2 active).
template <std::size_t RowCapacity>
std::array<std::span<const double>, RowCapacity> makeRows(
    const std::array<std::array<double, 3>, RowCapacity>& storage) {
  std::array<std::span<const double>, RowCapacity> rows{};
  for (std::size_t row = 0; row < RowCapacity; ++row) {
    rows[row] = std::span<const double>(storage[row]);
  }
  return rows;
}

}  // namespace

//======================================================================
// Compressibility parameter (comset)
//======================================================================

TEST(unit, CompressibilityAtMachZeroMatchesFortranValues) {
  // Fortran values for minf == 0: beta = 1, tklam = 0/2/2 = 0,
  // tkl_msq = 1/2/2 - 0 = 0.25, cpstar = -999, qstar = 999.
  const CompressibilityState state =
      calculateCompressibilityState(0.0, 1.0, 1.4, 0.4);

  CHECK(state.status == InviscidStatus::success);
  CHECK_EQ(state.karmanTsienParameter, 0.0);
  CHECK_EQ(state.karmanTsienMachDerivative, 0.25);
  CHECK_EQ(state.sonicPressureCoefficient, -999.0);
  CHECK_EQ(state.sonicSpeed, 999.0);
}

TEST(unit, CompressibilityAtModerateMachMatchesReference) {
  // reference directly from the unchanged formulas,
  // Mach = 0.3, qinf = 1.0, gamma = 1.4, gamm1 = 0.4:
  // beta = sqrt(1 - 0.09) = sqrt(0.91)
  // tklam = 0.09/(1+beta)^2 = 0.02357330184565224
  // tkl_msq = 1/(1+beta)^2 - 2*tklam/(1+beta)*beta_msq = 0.2745726097362895
  // cpstar = 2/(gamma*M^2) * (((1+0.5*gamm1*M^2)/(1+0.5*gamm1))
  // ^(gamma/gamm1) - 1) = -6.9473153258860725
  // qstar = qinf/M * sqrt((1+0.5*gamm1*M^2)/(1+0.5*gamm1))
  // = 3.0701670843662443
  const CompressibilityState state =
      calculateCompressibilityState(0.3, 1.0, 1.4, 0.4);

  CHECK(state.status == InviscidStatus::success);
  CHECK_NEAR(state.karmanTsienParameter, 0.02357330184565224, 1e-12, 0.0);
  CHECK_NEAR(state.karmanTsienMachDerivative, 0.2745726097362895, 1e-12, 0.0);
  CHECK_NEAR(state.sonicPressureCoefficient, -6.9473153258860725, 1e-12, 0.0);
  CHECK_NEAR(state.sonicSpeed, 3.0701670843662443, 1e-12, 0.0);
}

TEST(unit, CompressibilityRejectsMachAtOrAboveOne) {
  // Mach^2 >= 1 is rejected in a controlled way (no clamping).
  CHECK(calculateCompressibilityState(1.0, 1.0, 1.4, 0.4).status ==
        InviscidStatus::invalidInput);
  CHECK(calculateCompressibilityState(1.2, 1.0, 1.4, 0.4).status ==
        InviscidStatus::invalidInput);
  CHECK(calculateCompressibilityState(-1.0, 1.0, 1.4, 0.4).status ==
        InviscidStatus::invalidInput);
}

TEST(unit, CompressibilityRejectsZeroFreestreamSpeed) {
  CHECK(calculateCompressibilityState(0.3, 0.0, 1.4, 0.4).status ==
        InviscidStatus::invalidInput);
}

TEST(unit, CompressibilityRejectsNonFiniteInput) {
  CHECK(calculateCompressibilityState(kNan, 1.0, 1.4, 0.4).status ==
        InviscidStatus::invalidInput);
  CHECK(calculateCompressibilityState(0.3, kInf, 1.4, 0.4).status ==
        InviscidStatus::invalidInput);
  CHECK(calculateCompressibilityState(0.3, 1.0, kNan, 0.4).status ==
        InviscidStatus::invalidInput);
  CHECK(calculateCompressibilityState(0.3, 1.0, 1.4, kInf).status ==
        InviscidStatus::invalidInput);
}

TEST(unit, CompressibilityRejectsZeroGammaMinusOne) {
  CHECK(calculateCompressibilityState(0.3, 1.0, 1.4, 0.0).status ==
        InviscidStatus::invalidInput);
}

TEST(unit, CompressibilityRejectsZeroGamma) {
  CHECK(calculateCompressibilityState(
    0.3, 1.0, 0.0, 0.4)
  .status == InviscidStatus::invalidInput);
}

//======================================================================
// Pressure coefficients (cpcalc)
//======================================================================

TEST(unit, PressureCoefficientsAtMachZeroMatchIncompressibleFormula) {
  // Mach = 0: beta = 1, bfac = 0, also cp = cpinc = 1 - (q/qinf)^2.
  std::vector<double> velocity = {kSentinel, 1.0, 0.5, 2.0};
  std::vector<double> pressureCoefficient(4, kSentinel);

  const auto result = calculatePressureCoefficients(
      velocity, pressureCoefficient, 3, 1.0, 0.0);

  CHECK(result.status == InviscidStatus::success);
  CHECK_FALSE(result.denominatorNonPositive);
  // index 0 is reserved and must not be written.
  CHECK_EQ(pressureCoefficient[0], kSentinel);
  CHECK_NEAR(pressureCoefficient[1], 0.0, 1e-12, 0.0);
  CHECK_NEAR(pressureCoefficient[2], 0.75, 1e-12, 0.0);
  CHECK_NEAR(pressureCoefficient[3], -3.0, 1e-12, 0.0);
}

TEST(unit, PressureCoefficientsAtModerateMachMatchReference) {
  // reference directly from the unchanged formula,
  // Mach = 0.5, qinf = 1.0: beta = sqrt(0.75), bfac = 0.125/(1+beta).
  // q = 1.2: cpinc = -0.44, the = 0.8365509926170152,
  // cp = -0.525969132644898
  // q = 0.8: cpinc = 0.36, the = 0.8901408311032396,
  // cp = 0.40443038609274473
  std::vector<double> velocity = {kSentinel, 1.2, 0.8};
  std::vector<double> pressureCoefficient(3, kSentinel);

  const auto result = calculatePressureCoefficients(
      velocity, pressureCoefficient, 2, 1.0, 0.5);

  CHECK(result.status == InviscidStatus::success);
  CHECK_FALSE(result.denominatorNonPositive);
  CHECK_EQ(pressureCoefficient[0], kSentinel);
  CHECK_NEAR(pressureCoefficient[1], -0.525969132644898, 1e-12, 0.0);
  CHECK_NEAR(pressureCoefficient[2], 0.40443038609274473, 1e-12, 0.0);
}

TEST(unit, PressureCoefficientsDetectNonPositiveDenominator) {
  // Mach = 0.9, qinf = 1.0: beta = sqrt(0.19), bfac = 0.405/(1+beta).
  // q = 2.0 returns cpinc = -3 and the = -0.4102752641148319 <= 0.
  // CPCALC keeps computing all points; reference from the
  // unchanged formula: cp(q=2.0) = 7.312163960146353.
  std::vector<double> velocity = {kSentinel, 1.0, 2.0};
  std::vector<double> pressureCoefficient(3, kSentinel);

  const auto result = calculatePressureCoefficients(
      velocity, pressureCoefficient, 2, 1.0, 0.9);

  CHECK(result.status == InviscidStatus::compressibilityFailure);
  CHECK(result.denominatorNonPositive);
  // all points were still computed.
  CHECK_EQ(pressureCoefficient[0], kSentinel);
  CHECK(std::isfinite(pressureCoefficient[1]));
  CHECK_NEAR(pressureCoefficient[2], 7.312163960146353, 1e-12, 0.0);
}

TEST(unit, PressureCoefficientsRejectShortSpan) {
  std::vector<double> velocity = {kSentinel, 1.0, 1.0};
  std::vector<double> pressureCoefficient(3, kSentinel);

  // pointCount < 1: invalidDimension.
  CHECK(calculatePressureCoefficients(
            velocity, pressureCoefficient, 0, 1.0, 0.5)
            .status == InviscidStatus::invalidDimension);

  // Input span too short (pointCount + 1 required).
  std::vector<double> shortInput = {kSentinel, 1.0};
  CHECK(calculatePressureCoefficients(
            shortInput, pressureCoefficient, 2, 1.0, 0.5)
            .status == InviscidStatus::invalidDimension);

  // output span too short; nothing is written.
  std::vector<double> shortOutput(2, kSentinel);
  CHECK(calculatePressureCoefficients(
            velocity, shortOutput, 2, 1.0, 0.5)
            .status == InviscidStatus::invalidDimension);
  CHECK_EQ(shortOutput[1], kSentinel);
}

TEST(unit, PressureCoefficientsRejectZeroFreestreamSpeed) {
  std::vector<double> velocity = {kSentinel, 1.0};
  std::vector<double> pressureCoefficient(2, kSentinel);

  const auto result = calculatePressureCoefficients(
      velocity, pressureCoefficient, 1, 0.0, 0.5);

  CHECK(result.status == InviscidStatus::invalidInput);
  CHECK_EQ(pressureCoefficient[1], kSentinel);
}

TEST(unit, PressureCoefficientsRejectNonFiniteVelocity) {
  std::vector<double> velocity = {kSentinel, 1.0, kNan};
  std::vector<double> pressureCoefficient(3, kSentinel);

  CHECK(calculatePressureCoefficients(
            velocity, pressureCoefficient, 2, 1.0, 0.5)
            .status == InviscidStatus::invalidInput);
  CHECK_EQ(pressureCoefficient[1], kSentinel);
  CHECK_EQ(pressureCoefficient[2], kSentinel);

  velocity[2] = kInf;
  CHECK(calculatePressureCoefficients(
            velocity, pressureCoefficient, 2, 1.0, 0.5)
            .status == InviscidStatus::invalidInput);

  // Non-finite scalars and invalid Mach number.
  std::vector<double> okVelocity = {kSentinel, 1.0};
  CHECK(calculatePressureCoefficients(
            okVelocity, pressureCoefficient, 1, kNan, 0.5)
            .status == InviscidStatus::invalidInput);
  CHECK(calculatePressureCoefficients(
            okVelocity, pressureCoefficient, 1, 1.0, kInf)
            .status == InviscidStatus::invalidInput);
  CHECK(calculatePressureCoefficients(
            okVelocity, pressureCoefficient, 1, 1.0, 1.0)
            .status == InviscidStatus::invalidInput);
}

//======================================================================
// Velocity superposition (qiset)
//======================================================================

TEST(unit, UnitVelocityCombinationAtZeroAngleUsesFirstDistribution) {
  // alpha = 0: cos = 1, sin = 0 exact -> qinv = column 1, qinv_a = column 2.
  const std::array<std::array<double, 3>, 3> storage = {{{{0.0, 0.0, 0.0}},
                                                         {{0.0, 1.0, 2.0}},
                                                         {{0.0, 3.0, -5.0}}}};
  auto rows = makeRows(storage);

  std::vector<double> velocity(3, kSentinel);
  std::vector<double> alphaDerivative(3, kSentinel);

  const SurfaceVelocityResult result = combineUnitVelocityDistributions(
      0.0, UnitVelocityRows(rows), velocity, alphaDerivative, 2);

  CHECK(result.status == InviscidStatus::success);
  CHECK_EQ(result.cosineAlpha, 1.0);
  CHECK_EQ(result.sineAlpha, 0.0);
  CHECK_EQ(velocity[0], kSentinel);
  CHECK_EQ(velocity[1], 1.0);
  CHECK_EQ(velocity[2], 3.0);
  CHECK_EQ(alphaDerivative[1], 2.0);
  CHECK_EQ(alphaDerivative[2], -5.0);
}

TEST(unit, UnitVelocityCombinationAtQuarterTurnUsesSecondDistribution) {
  // alpha = pi/2: sin = 1, cos ~ 6.1e-17 -> qinv ~ column 2,
  // qinv_a ~ -column 1 (Tolerance 1e-12).
  const std::array<std::array<double, 3>, 3> storage = {{{{0.0, 0.0, 0.0}},
                                                         {{0.0, 1.0, 2.0}},
                                                         {{0.0, 3.0, -5.0}}}};
  auto rows = makeRows(storage);

  std::vector<double> velocity(3, kSentinel);
  std::vector<double> alphaDerivative(3, kSentinel);

  const SurfaceVelocityResult result = combineUnitVelocityDistributions(
      std::numbers::pi_v<double> / 2.0, UnitVelocityRows(rows), velocity,
      alphaDerivative, 2);

  CHECK(result.status == InviscidStatus::success);
  CHECK_NEAR(result.sineAlpha, 1.0, 1e-15, 0.0);
  CHECK_NEAR(velocity[1], 2.0, 1e-12, 0.0);
  CHECK_NEAR(velocity[2], -5.0, 1e-12, 0.0);
  CHECK_NEAR(alphaDerivative[1], -1.0, 1e-12, 0.0);
  CHECK_NEAR(alphaDerivative[2], -3.0, 1e-12, 0.0);
}

TEST(unit, UnitVelocityCombinationCalculatesAlphaDerivative) {
  // alpha = 0.5 rad. Reference from the unchanged formula:
  // line (1, 2): v = 1.8364336390987788, a = 1.2757395851765425
  // line (-1, 0.5): v = -0.6378697925882713, a = 0.9182168195493894
  const std::array<std::array<double, 3>, 3> storage = {{{{0.0, 0.0, 0.0}},
                                                         {{0.0, 1.0, 2.0}},
                                                         {{0.0, -1.0, 0.5}}}};
  auto rows = makeRows(storage);

  std::vector<double> velocity(3, kSentinel);
  std::vector<double> alphaDerivative(3, kSentinel);

  const SurfaceVelocityResult result = combineUnitVelocityDistributions(
      0.5, UnitVelocityRows(rows), velocity, alphaDerivative, 2);

  CHECK(result.status == InviscidStatus::success);
  CHECK_NEAR(result.cosineAlpha, std::cos(0.5), 1e-15, 0.0);
  CHECK_NEAR(result.sineAlpha, std::sin(0.5), 1e-15, 0.0);
  CHECK_NEAR(velocity[1], 1.8364336390987788, 1e-12, 0.0);
  CHECK_NEAR(alphaDerivative[1], 1.2757395851765425, 1e-12, 0.0);
  CHECK_NEAR(velocity[2], -0.6378697925882713, 1e-12, 0.0);
  CHECK_NEAR(alphaDerivative[2], 0.9182168195493894, 1e-12, 0.0);
}

TEST(unit, UnitVelocityCombinationRejectsShortRow) {
  // active rows contain only two components: column 2 missing.
  std::vector<double> twoComponents = {0.0, 1.0};
  std::array<std::span<const double>, 3> shortRows{
      std::span<const double>(twoComponents),
      std::span<const double>(twoComponents),
      std::span<const double>(twoComponents)};

  std::vector<double> velocity(3, kSentinel);
  std::vector<double> alphaDerivative(3, kSentinel);

  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(shortRows), velocity, alphaDerivative, 2)
            .status == InviscidStatus::invalidDimension);
  CHECK_EQ(velocity[1], kSentinel);
}

TEST(unit, UnitVelocityCombinationRejectsShortOutput) {
  const std::array<std::array<double, 3>, 4> storage = {{{{0.0, 0.0, 0.0}},
                                                         {{0.0, 1.0, 2.0}},
                                                         {{0.0, 3.0, 4.0}},
                                                         {{0.0, 5.0, 6.0}}}};
  auto rows = makeRows(storage);

  std::vector<double> velocity(4, kSentinel);
  std::vector<double> alphaDerivative(4, kSentinel);

  // pointCount < 1.
  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(rows), velocity, alphaDerivative, 0)
            .status == InviscidStatus::invalidDimension);

  // output spans too short (pointCount + 1 required).
  std::vector<double> shortOutput(2, kSentinel);
  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(rows), shortOutput, alphaDerivative, 3)
            .status == InviscidStatus::invalidDimension);
  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(rows), velocity, shortOutput, 3)
            .status == InviscidStatus::invalidDimension);

  // too few rows (pointCount + 1 required).
  auto threeRows = rows;
  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(threeRows.data(), 3), velocity,
            alphaDerivative, 3)
            .status == InviscidStatus::invalidDimension);

  // Cross-check: with four rows and pointCount = 3 the dimension is
  // valid (index 0 reserved).
  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(rows), velocity, alphaDerivative, 3)
            .status == InviscidStatus::success);
}

TEST(unit, UnitVelocityCombinationRejectsNonFiniteInput) {
  const std::array<std::array<double, 3>, 3> storage = {{{{0.0, 0.0, 0.0}},
                                                         {{0.0, kNan, 2.0}},
                                                         {{0.0, 3.0, 4.0}}}};
  auto rows = makeRows(storage);

  std::vector<double> velocity(3, kSentinel);
  std::vector<double> alphaDerivative(3, kSentinel);

  CHECK(combineUnitVelocityDistributions(
            0.5, UnitVelocityRows(rows), velocity, alphaDerivative, 2)
            .status == InviscidStatus::invalidInput);
  CHECK_EQ(velocity[1], kSentinel);
  CHECK_EQ(alphaDerivative[1], kSentinel);

  // not a finite angle for valid distributions.
  const std::array<std::array<double, 3>, 3> okStorage = {{{{0.0, 0.0, 0.0}},
                                                           {{0.0, 1.0, 2.0}},
                                                           {{0.0, 3.0, 4.0}}}};
  auto okRows = makeRows(okStorage);
  CHECK(combineUnitVelocityDistributions(
            kInf, UnitVelocityRows(okRows), velocity, alphaDerivative, 2)
            .status == InviscidStatus::invalidInput);
}

//======================================================================
// Pressure load integration (clcalc)
//======================================================================

TEST(unit, PressureLoadsOfSymmetricDistributionHaveExpectedLift) {
  // Hand-computed case: square contour p1=(0,0), p2=(1,0), p3=(1,1),
  // p4=(0,1); alpha = 0, Mach = 0, qinf = 1, reference point (0,0).
  // Circulation gam = [.,0,0,1,1] is symmetric about the mid-chord line
  // x = 0.5 (gam[1]=gam[2], gam[3]=gam[4]); since Mach = 0 holds
  // cpg = 1 - gam^2: cpg = [.,1,1,0,0].
  // cl = sum dx*ag: only segment (1->2) with dx=1, ag=1 -> cl = +1.
  // cdp = -sum dy*ag: (2->3): -(1)(0.5); (4->1): -(-1)(0.5) -> 0.
  // cm: (1->2): -(1)(1*0.5) = -0.5; (2->3): -(1)(0.5*0.5 + (-1)/12) = -1/6;
  // (4->1): -(-1)(0.5*0.5 + (1)(-1)/12) = +1/6 -> cm = -1/2.
  // xcp accumulation: only (1->2): 1*1*(1+0)/2 = 0.5; /cl -> xcp = 0.5.
  // cl_msq (Mach=0: beta=1, bfac=0, beta_msq=-0.5, bfac_msq=0.25,
  // cpg_msq = -cpg*(beta_msq + bfac_msq*cpg) = 0.25 for cpg=1):
  // only (1->2): dx*ag_msq = 1*0.25 -> cl_msq = 0.25.
  // gam_a = 0, alpha = 0: cl_alf = sum ag*dx_alf = 0.5 - 0.5 = 0.
  std::vector<double> x = {kSentinel, 0.0, 1.0, 1.0, 0.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0, 1.0, 1.0};
  std::vector<double> gam = {kSentinel, 0.0, 0.0, 1.0, 1.0};
  std::vector<double> gamA(5, 0.0);

  const auto result = integratePressureLoads(
      x, y, gam, gamA, 4, 0.0, 1.0, 0.0, 0.0, 0.0);

  CHECK(result.status == InviscidStatus::success);
  CHECK_NEAR(result.liftCoefficient, 1.0, 1e-12, 0.0);
  CHECK_NEAR(result.pressureDragCoefficient, 0.0, 1e-12, 0.0);
  CHECK_NEAR(result.momentCoefficient, -0.5, 1e-12, 0.0);
  CHECK_NEAR(result.centerOfPressureX, 0.5, 1e-12, 0.0);
  CHECK_NEAR(result.liftAlphaDerivative, 0.0, 1e-12, 0.0);
  CHECK_NEAR(result.liftMachSquaredDerivative, 0.25, 1e-12, 0.0);
}

TEST(unit, PressureLoadsReturnZeroCenterForZeroLift) {
  // Triangle p1=(0,0), p2=(1,0), p3=(0,1), gam = [.,0,1,0], alpha = 0,
  // Mach = 0, qinf = 1: the CL contributions (+0.5 from (1->2) and -0.5 from
  // (2->3)) cancel exactly. Thus the branch
  // fabs(cl) > 0 == false applies and xcp is set to 0.
  std::vector<double> x = {kSentinel, 0.0, 1.0, 0.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0, 1.0};
  std::vector<double> gam = {kSentinel, 0.0, 1.0, 0.0};
  std::vector<double> gamA(4, 0.0);

  const auto result = integratePressureLoads(
      x, y, gam, gamA, 3, 0.0, 1.0, 0.0, 0.0, 0.0);

  CHECK(result.status == InviscidStatus::success);
  CHECK_EQ(result.liftCoefficient, 0.0);
  CHECK_EQ(result.centerOfPressureX, 0.0);
}

TEST(unit, PressureLoadsMatchHandCalculatedSmallContour) {
  // Hand-computed case (inputs and expected values):
  // contour: p1=(0,0), p2=(1,0), p3=(0,1), cyclic successor of
  // p3 is p1; gam = [.,1,1,0], gam_a = 0; alpha = 0, Mach = 0,
  // qinf = 1, reference point (0,0).
  // since Mach = 0: cpg = 1 - gam^2 -> cpg = [.,0,0,1]; beta = 1, bfac = 0,
  // beta_msq = -0.5, bfac_msq = 0.25,
  // cpg_msq = -cpg*(beta_msq + bfac_msq*cpg).
  // segment (1->2): ag = 0, dg = 0 -> no contributions.
  // segment (2->3): dx=-1, dy=1, ag=0.5, dg=1, ax=ay=0.5:
  // cl += -0.5; cdp -= 0.5; xcp += (-1)(0.5)(0.5) = -0.25;
  // cm -= (-1)(0.25-1/12) + (1)(0.25+1/12) -> cm = 1/6 - 1/3 = -1/6;
  // cl_msq += (-1)*0.5*(0.25+0) = -0.125.
  // segment (3->1): dx=0, dy=-1, ag=0.5, dg=-1, ax=0, ay=0.5:
  // cdp -= (-1)(0.5) -> +0.5 so cdp = 0;
  // cm -= (-1)(0.25+1/12) -> cm = -1/6 + 1/3 = +1/6.
  // Completion: cl = -0.5 != 0 -> xcp = (-0.25)/(-0.5) = 0.5.
  // expected: cl = -0.5, cdp = 0, cm = 1/6 (~0.16666666666667), xcp = 0.5,
  // cl_alf = 0 (dx_alf contributions +0.5 and -0.5 cancel),
  // cl_msq = -0.125.
  std::vector<double> x = {kSentinel, 0.0, 1.0, 0.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0, 1.0};
  std::vector<double> gam = {kSentinel, 1.0, 1.0, 0.0};
  std::vector<double> gamA(4, 0.0);

  const auto result = integratePressureLoads(
      x, y, gam, gamA, 3, 0.0, 1.0, 0.0, 0.0, 0.0);

  CHECK(result.status == InviscidStatus::success);
  CHECK_NEAR(result.liftCoefficient, -0.5, 1e-12, 0.0);
  CHECK_NEAR(result.pressureDragCoefficient, 0.0, 1e-12, 0.0);
  CHECK_NEAR(result.momentCoefficient, 0.1666666666666667, 1e-12, 0.0);
  CHECK_NEAR(result.centerOfPressureX, 0.5, 1e-12, 0.0);
  CHECK_NEAR(result.liftAlphaDerivative, 0.0, 1e-12, 0.0);
  CHECK_NEAR(result.liftMachSquaredDerivative, -0.125, 1e-12, 0.0);
}

TEST(unit, PressureLoadsRejectShortSpan) {
  std::vector<double> x = {kSentinel, 0.0, 1.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0};
  std::vector<double> gam = {kSentinel, 0.5, 0.5};
  std::vector<double> gamA(3, 0.0);

  // pointCount < 2.
  CHECK(integratePressureLoads(x, y, gam, gamA, 1, 0.0, 1.0, 0.0, 0.0, 0.0)
            .status == InviscidStatus::invalidDimension);

  // span too short (pointCount + 1 required).
  std::vector<double> shortX = {kSentinel, 0.0};
  CHECK(integratePressureLoads(shortX, y, gam, gamA, 2, 0.0, 1.0, 0.0, 0.0,
                               0.0)
            .status == InviscidStatus::invalidDimension);
}

TEST(unit, PressureLoadsRejectZeroFreestreamSpeed) {
  std::vector<double> x = {kSentinel, 0.0, 1.0, 0.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0, 1.0};
  std::vector<double> gam = {kSentinel, 0.5, 0.5, 0.5};
  std::vector<double> gamA(4, 0.0);

  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 0.0, 0.0, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);
}

TEST(unit, PressureLoadsRejectInvalidMach) {
  std::vector<double> x = {kSentinel, 0.0, 1.0, 0.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0, 1.0};
  std::vector<double> gam = {kSentinel, 0.5, 0.5, 0.5};
  std::vector<double> gamA(4, 0.0);

  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 1.0, 1.0, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);
  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 1.0, -1.5, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);
}

TEST(unit, PressureLoadsRejectNonFiniteInput) {
  std::vector<double> x = {kSentinel, 0.0, 1.0, 0.0};
  std::vector<double> y = {kSentinel, 0.0, 0.0, kNan};
  std::vector<double> gam = {kSentinel, 0.5, 0.5, 0.5};
  std::vector<double> gamA(4, 0.0);

  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 1.0, 0.0, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);

  y[3] = 1.0;
  gam[2] = kInf;
  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 1.0, 0.0, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);
  gam[2] = 0.5;

  gamA[1] = kNan;
  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 1.0, 0.0, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);
  gamA[1] = 0.0;

  CHECK(integratePressureLoads(x, y, gam, gamA, 3, kNan, 1.0, 0.0, 0.0, 0.0)
            .status == InviscidStatus::invalidInput);
  CHECK(integratePressureLoads(x, y, gam, gamA, 3, 0.0, 1.0, 0.0, kInf, 0.0)
            .status == InviscidStatus::invalidInput);
}

//======================================================================
// status values
//======================================================================

TEST(unit, InviscidStatusValuesAreDistinct) {
  // all status values are pairwise distinct; no numeric enum values
  // are preset.
  CHECK(InviscidStatus::success != InviscidStatus::invalidDimension);
  CHECK(InviscidStatus::success != InviscidStatus::invalidInput);
  CHECK(InviscidStatus::success != InviscidStatus::compressibilityFailure);
  CHECK(InviscidStatus::invalidDimension != InviscidStatus::invalidInput);
  CHECK(InviscidStatus::invalidDimension !=
        InviscidStatus::compressibilityFailure);
  CHECK(InviscidStatus::invalidInput !=
        InviscidStatus::compressibilityFailure);
}

// End: tests/unit/InviscidAnalysisTests.cpp

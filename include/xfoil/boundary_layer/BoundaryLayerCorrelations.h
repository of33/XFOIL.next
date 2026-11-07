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

// File: include/xfoil/boundary_layer/BoundaryLayerCorrelations.h
// Stateless boundary-layer correlations implementing the CFL, CFT,
// DIL, DILW, HKIN, HCT, HSL, HST, DAMPL and AXSET routines of
// fortran-src/xblsys.f. Constants,
// branch conditions and evaluation orders are
// adopted unchanged.
//
// API principle: independent of XFoil.h, no raw pointers, no state,
// result structures instead of long output reference lists, no exceptions,
// no changes to global values. The correlations assume
// physically sensible inputs; additional range, zero, or NaN/Inf
// validation is deliberately not introduced.

#pragma once

namespace xfoil::boundary_layer {

struct CorrelationValue {
  double value{};
  double shapeDerivative{};
  double reynoldsDerivative{};
  double machSquaredDerivative{};
};

struct KinematicShapeFactorResult {
  double value{};
  double shapeDerivative{};
  double machSquaredDerivative{};
};

struct DensityShapeFactorResult {
  double value{};
  double shapeDerivative{};
  double machSquaredDerivative{};
};

struct AmplificationRateResult {
  double value{};
  double shapeDerivative{};
  double momentumThicknessDerivative{};
  double thetaReynoldsDerivative{};
};

struct AverageAmplificationRateResult {
  double value{};

  double firstShapeDerivative{};
  double firstMomentumThicknessDerivative{};
  double firstThetaReynoldsDerivative{};
  double firstAmplificationDerivative{};

  double secondShapeDerivative{};
  double secondMomentumThicknessDerivative{};
  double secondThetaReynoldsDerivative{};
  double secondAmplificationDerivative{};
};

// Implements CFL (fortran-src/xblsys.f:2354): laminar skin friction (cf) after Falkner-Skan.
// shapeDerivative = d cf / d hk, reynoldsDerivative = d cf / d rt,
// machSquaredDerivative is zero by definition.
[[nodiscard]]
CorrelationValue laminarSkinFriction(
    double kinematicShapeFactor,
    double thetaReynolds) noexcept;

// Implements CFT (fortran-src/xblsys.f:2483): turbulent skin friction (cf) after Coles.
[[nodiscard]]
CorrelationValue turbulentSkinFriction(
    double kinematicShapeFactor,
    double thetaReynolds,
    double machSquared) noexcept;

// Implements DIL (fortran-src/xblsys.f:2290): laminar dissipation (2 cd/h*) after Falkner-Skan.
[[nodiscard]]
CorrelationValue laminarDissipation(
    double kinematicShapeFactor,
    double thetaReynolds) noexcept;

// Implements DILW (fortran-src/xblsys.f:2308): laminar wake dissipation; internally uses the
// laminar energy-shape correlation in unchanged order.
[[nodiscard]]
CorrelationValue laminarWakeDissipation(
    double kinematicShapeFactor,
    double thetaReynolds) noexcept;

// Implements HKIN (fortran-src/xblsys.f:2276): kinematic shape factor (per Whitfield).
[[nodiscard]]
KinematicShapeFactorResult kinematicShapeFactor(
    double shapeFactor,
    double machSquared) noexcept;

// Implements HCT (fortran-src/xblsys.f:2514): density shape factor (after Whitfield).
[[nodiscard]]
DensityShapeFactorResult densityShapeFactor(
    double kinematicShapeFactor,
    double machSquared) noexcept;

// Implements HSL (fortran-src/xblsys.f:2327): laminar energy shape factor. reynoldsDerivative
// and machSquaredDerivative are per definition 0.
[[nodiscard]]
CorrelationValue laminarEnergyShapeFactor(
    double kinematicShapeFactor) noexcept;

// Implements HST (fortran-src/xblsys.f:2388): turbulent energy shape factor.
[[nodiscard]]
CorrelationValue turbulentEnergyShapeFactor(
    double kinematicShapeFactor,
    double thetaReynolds,
    double machSquared) noexcept;

// Implements DAMPL (fortran-src/xblsys.f:1981): amplification envelope (ax) after Falkner-Skan.
[[nodiscard]]
AmplificationRateResult amplificationRate(
    double kinematicShapeFactor,
    double momentumThickness,
    double thetaReynolds) noexcept;

// Implements AXSET (fortran-src/xblsys.f:35): midpoint amplification ax over the interval
// 1..2 (RMS average plus exponential correction near ncrit).
[[nodiscard]]
AverageAmplificationRateResult averageAmplificationRate(
    double firstKinematicShapeFactor,
    double firstMomentumThickness,
    double firstThetaReynolds,
    double firstAmplification,
    double secondKinematicShapeFactor,
    double secondMomentumThickness,
    double secondThetaReynolds,
    double secondAmplification,
    double criticalAmplification) noexcept;

}  // namespace xfoil::boundary_layer

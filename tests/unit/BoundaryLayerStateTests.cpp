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

// File: tests/unit/BoundaryLayerStateTests.cpp
// unit tests for the value-based boundary-layer station type.
// Only public value properties are checked.

#include "TestSupport.h"

#include "xfoil/boundary_layer/BoundaryLayerState.h"

#include <type_traits>
#include <utility>

namespace {

using State = xfoil::boundary_layer::BoundaryLayerStationState;

}  // namespace

static_assert(std::is_default_constructible_v<State>);
static_assert(std::is_copy_constructible_v<State>);
static_assert(std::is_copy_assignable_v<State>);
static_assert(std::is_move_constructible_v<State>);
static_assert(std::is_move_assignable_v<State>);

TEST(unit, BoundaryLayerStationStateDefaultsToZero) {
  const State state;

  // Primary variables
  CHECK_EQ(state.x, 0.0);
  CHECK_EQ(state.edgeVelocity, 0.0);
  CHECK_EQ(state.momentumThickness, 0.0);
  CHECK_EQ(state.displacementThickness, 0.0);
  CHECK_EQ(state.shearCoefficient, 0.0);
  CHECK_EQ(state.amplification, 0.0);
  CHECK_EQ(state.edgeVelocityIncompressibleDerivative, 0.0);
  CHECK_EQ(state.edgeVelocityMachSquaredDerivative, 0.0);
  CHECK_EQ(state.wakeDisplacement, 0.0);

  // shape factors
  CHECK_EQ(state.shapeFactor, 0.0);
  CHECK_EQ(state.shapeFactorMomentumDerivative, 0.0);
  CHECK_EQ(state.shapeFactorDisplacementDerivative, 0.0);

  // Mach
  CHECK_EQ(state.machSquared, 0.0);
  CHECK_EQ(state.machSquaredVelocityDerivative, 0.0);
  CHECK_EQ(state.machSquaredMachSquaredDerivative, 0.0);

  // Density
  CHECK_EQ(state.density, 0.0);
  CHECK_EQ(state.densityVelocityDerivative, 0.0);
  CHECK_EQ(state.densityMachSquaredDerivative, 0.0);

  // Viscosity
  CHECK_EQ(state.viscosity, 0.0);
  CHECK_EQ(state.viscosityVelocityDerivative, 0.0);
  CHECK_EQ(state.viscosityMachSquaredDerivative, 0.0);
  CHECK_EQ(state.viscosityReynoldsDerivative, 0.0);

  // Kinematic and energy thickness shape factors
  CHECK_EQ(state.kinematicShapeFactor, 0.0);
  CHECK_EQ(state.energyShapeFactor, 0.0);
  CHECK_EQ(state.densityShapeFactor, 0.0);

  // Reynolds number
  CHECK_EQ(state.momentumThicknessReynoldsNumber, 0.0);
  CHECK_EQ(state.momentumThicknessReynoldsVelocityDerivative, 0.0);
  CHECK_EQ(state.momentumThicknessReynoldsMomentumDerivative, 0.0);
  CHECK_EQ(state.momentumThicknessReynoldsMachSquaredDerivative, 0.0);
  CHECK_EQ(state.momentumThicknessReynoldsReynoldsDerivative, 0.0);

  // Friction
  CHECK_EQ(state.skinFriction, 0.0);
  CHECK_EQ(state.skinFrictionVelocityDerivative, 0.0);
  CHECK_EQ(state.skinFrictionMomentumDerivative, 0.0);
  CHECK_EQ(state.skinFrictionDisplacementDerivative, 0.0);
  CHECK_EQ(state.skinFrictionMachSquaredDerivative, 0.0);
  CHECK_EQ(state.skinFrictionReynoldsDerivative, 0.0);

  // Dissipation
  CHECK_EQ(state.dissipation, 0.0);
  CHECK_EQ(state.dissipationVelocityDerivative, 0.0);
  CHECK_EQ(state.dissipationMomentumDerivative, 0.0);
  CHECK_EQ(state.dissipationDisplacementDerivative, 0.0);
  CHECK_EQ(state.dissipationShearDerivative, 0.0);
  CHECK_EQ(state.dissipationMachSquaredDerivative, 0.0);
  CHECK_EQ(state.dissipationReynoldsDerivative, 0.0);

  // Slip velocity
  CHECK_EQ(state.slipVelocity, 0.0);
  CHECK_EQ(state.slipVelocityVelocityDerivative, 0.0);
  CHECK_EQ(state.slipVelocityMomentumDerivative, 0.0);
  CHECK_EQ(state.slipVelocityDisplacementDerivative, 0.0);
  CHECK_EQ(state.slipVelocityMachSquaredDerivative, 0.0);
  CHECK_EQ(state.slipVelocityReynoldsDerivative, 0.0);

  // Equilibrium shear
  CHECK_EQ(state.equilibriumShear, 0.0);
  CHECK_EQ(state.equilibriumShearVelocityDerivative, 0.0);
  CHECK_EQ(state.equilibriumShearMomentumDerivative, 0.0);
  CHECK_EQ(state.equilibriumShearDisplacementDerivative, 0.0);
  CHECK_EQ(state.equilibriumShearMachSquaredDerivative, 0.0);
  CHECK_EQ(state.equilibriumShearReynoldsDerivative, 0.0);

  // thickness
  CHECK_EQ(state.boundaryLayerThickness, 0.0);
  CHECK_EQ(state.boundaryLayerThicknessVelocityDerivative, 0.0);
  CHECK_EQ(state.boundaryLayerThicknessMomentumDerivative, 0.0);
  CHECK_EQ(state.boundaryLayerThicknessDisplacementDerivative, 0.0);
  CHECK_EQ(state.boundaryLayerThicknessMachSquaredDerivative, 0.0);
}

TEST(unit, BoundaryLayerStationStateSupportsValueCopy) {
  State original;
  original.x = 1.5;
  original.edgeVelocity = 2.5;
  original.momentumThickness = 3.5;
  original.displacementThickness = 4.5;
  original.shearCoefficient = 5.5;
  original.amplification = 6.5;
  original.edgeVelocityIncompressibleDerivative = 7.5;
  original.edgeVelocityMachSquaredDerivative = 8.5;
  original.wakeDisplacement = 9.5;
  original.shapeFactor = 10.5;
  original.shapeFactorMomentumDerivative = 11.5;
  original.shapeFactorDisplacementDerivative = 12.5;
  original.machSquared = 13.5;
  original.machSquaredVelocityDerivative = 14.5;
  original.machSquaredMachSquaredDerivative = 15.5;
  original.density = 16.5;
  original.densityVelocityDerivative = 17.5;
  original.densityMachSquaredDerivative = 18.5;
  original.viscosity = 19.5;
  original.viscosityVelocityDerivative = 20.5;
  original.viscosityMachSquaredDerivative = 21.5;
  original.viscosityReynoldsDerivative = 22.5;
  original.kinematicShapeFactor = 23.5;
  original.kinematicShapeFactorVelocityDerivative = 24.5;
  original.kinematicShapeFactorMomentumDerivative = 25.5;
  original.kinematicShapeFactorDisplacementDerivative = 26.5;
  original.kinematicShapeFactorMachSquaredDerivative = 27.5;
  original.energyShapeFactor = 28.5;
  original.energyShapeFactorVelocityDerivative = 29.5;
  original.energyShapeFactorMomentumDerivative = 30.5;
  original.energyShapeFactorDisplacementDerivative = 31.5;
  original.energyShapeFactorMachSquaredDerivative = 32.5;
  original.energyShapeFactorReynoldsDerivative = 33.5;
  original.densityShapeFactor = 34.5;
  original.densityShapeFactorVelocityDerivative = 35.5;
  original.densityShapeFactorMomentumDerivative = 36.5;
  original.densityShapeFactorDisplacementDerivative = 37.5;
  original.densityShapeFactorMachSquaredDerivative = 38.5;
  original.momentumThicknessReynoldsNumber = 39.5;
  original.momentumThicknessReynoldsVelocityDerivative = 40.5;
  original.momentumThicknessReynoldsMomentumDerivative = 41.5;
  original.momentumThicknessReynoldsMachSquaredDerivative = 42.5;
  original.momentumThicknessReynoldsReynoldsDerivative = 43.5;
  original.skinFriction = 44.5;
  original.skinFrictionVelocityDerivative = 45.5;
  original.skinFrictionMomentumDerivative = 46.5;
  original.skinFrictionDisplacementDerivative = 47.5;
  original.skinFrictionMachSquaredDerivative = 48.5;
  original.skinFrictionReynoldsDerivative = 49.5;
  original.dissipation = 50.5;
  original.dissipationVelocityDerivative = 51.5;
  original.dissipationMomentumDerivative = 52.5;
  original.dissipationDisplacementDerivative = 53.5;
  original.dissipationShearDerivative = 54.5;
  original.dissipationMachSquaredDerivative = 55.5;
  original.dissipationReynoldsDerivative = 56.5;
  original.slipVelocity = 57.5;
  original.slipVelocityVelocityDerivative = 58.5;
  original.slipVelocityMomentumDerivative = 59.5;
  original.slipVelocityDisplacementDerivative = 60.5;
  original.slipVelocityMachSquaredDerivative = 61.5;
  original.slipVelocityReynoldsDerivative = 62.5;
  original.equilibriumShear = 63.5;
  original.equilibriumShearVelocityDerivative = 64.5;
  original.equilibriumShearMomentumDerivative = 65.5;
  original.equilibriumShearDisplacementDerivative = 66.5;
  original.equilibriumShearMachSquaredDerivative = 67.5;
  original.equilibriumShearReynoldsDerivative = 68.5;
  original.boundaryLayerThickness = 69.5;
  original.boundaryLayerThicknessVelocityDerivative = 70.5;
  original.boundaryLayerThicknessMomentumDerivative = 71.5;
  original.boundaryLayerThicknessDisplacementDerivative = 72.5;
  original.boundaryLayerThicknessMachSquaredDerivative = 73.5;

  const State copy(original);
  CHECK(copy == original);

  State assigned;
  assigned = original;
  CHECK(assigned == original);
}

TEST(unit, BoundaryLayerStationCopiesAreIndependent) {
  State original;
  original.x = 1.25;
  original.momentumThickness = 2.25;
  original.skinFriction = 3.25;
  original.dissipationShearDerivative = 4.25;
  original.boundaryLayerThicknessMachSquaredDerivative = 5.25;

  State copy(original);
  copy.x = -1.0;
  copy.momentumThickness = -2.0;
  copy.skinFriction = -3.0;
  copy.dissipationShearDerivative = -4.0;
  copy.boundaryLayerThicknessMachSquaredDerivative = -5.0;

  CHECK_EQ(original.x, 1.25);
  CHECK_EQ(original.momentumThickness, 2.25);
  CHECK_EQ(original.skinFriction, 3.25);
  CHECK_EQ(original.dissipationShearDerivative, 4.25);
  CHECK_EQ(original.boundaryLayerThicknessMachSquaredDerivative, 5.25);
  CHECK_FALSE(copy == original);
}

TEST(unit, BoundaryLayerStationStateHasExpectedValueSemantics) {
  static_assert(std::is_default_constructible_v<State>);
  static_assert(std::is_copy_constructible_v<State>);
  static_assert(std::is_copy_assignable_v<State>);
  static_assert(std::is_move_constructible_v<State>);
  static_assert(std::is_move_assignable_v<State>);

  State state;
  const State moved(std::move(state));
  CHECK(moved == State{});
}

// End: tests/unit/BoundaryLayerStateTests.cpp

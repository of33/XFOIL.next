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

#pragma once

namespace xfoil::boundary_layer {

struct BoundaryLayerStationState {
  double x{};
  double edgeVelocity{};
  double momentumThickness{};
  double displacementThickness{};
  // Thrust quantity or station-dependent
  // shear variable.
  double shearCoefficient{};

  // e^n amplification value for the
  // transition calculation.
  double amplification{};
  double edgeVelocityIncompressibleDerivative{};
  double edgeVelocityMachSquaredDerivative{};
  double wakeDisplacement{};

  double shapeFactor{};
  double shapeFactorMomentumDerivative{};
  double shapeFactorDisplacementDerivative{};

  double machSquared{};
  double machSquaredVelocityDerivative{};
  double machSquaredMachSquaredDerivative{};

  double density{};
  double densityVelocityDerivative{};
  double densityMachSquaredDerivative{};

  double viscosity{};
  double viscosityVelocityDerivative{};
  double viscosityMachSquaredDerivative{};
  double viscosityReynoldsDerivative{};

  double kinematicShapeFactor{};
  double kinematicShapeFactorVelocityDerivative{};
  double kinematicShapeFactorMomentumDerivative{};
  double kinematicShapeFactorDisplacementDerivative{};
  double kinematicShapeFactorMachSquaredDerivative{};

  double energyShapeFactor{};
  double energyShapeFactorVelocityDerivative{};
  double energyShapeFactorMomentumDerivative{};
  double energyShapeFactorDisplacementDerivative{};
  double energyShapeFactorMachSquaredDerivative{};
  double energyShapeFactorReynoldsDerivative{};

  double densityShapeFactor{};
  double densityShapeFactorVelocityDerivative{};
  double densityShapeFactorMomentumDerivative{};
  double densityShapeFactorDisplacementDerivative{};
  double densityShapeFactorMachSquaredDerivative{};

  double momentumThicknessReynoldsNumber{};
  double momentumThicknessReynoldsVelocityDerivative{};
  double momentumThicknessReynoldsMomentumDerivative{};
  double momentumThicknessReynoldsMachSquaredDerivative{};
  double momentumThicknessReynoldsReynoldsDerivative{};

  double skinFriction{};
  double skinFrictionVelocityDerivative{};
  double skinFrictionMomentumDerivative{};
  double skinFrictionDisplacementDerivative{};
  double skinFrictionMachSquaredDerivative{};
  double skinFrictionReynoldsDerivative{};

  double dissipation{};
  double dissipationVelocityDerivative{};
  double dissipationMomentumDerivative{};
  double dissipationDisplacementDerivative{};
  double dissipationShearDerivative{};
  double dissipationMachSquaredDerivative{};
  double dissipationReynoldsDerivative{};

  double slipVelocity{};
  double slipVelocityVelocityDerivative{};
  double slipVelocityMomentumDerivative{};
  double slipVelocityDisplacementDerivative{};
  double slipVelocityMachSquaredDerivative{};
  double slipVelocityReynoldsDerivative{};

  double equilibriumShear{};
  double equilibriumShearVelocityDerivative{};
  double equilibriumShearMomentumDerivative{};
  double equilibriumShearDisplacementDerivative{};
  double equilibriumShearMachSquaredDerivative{};
  double equilibriumShearReynoldsDerivative{};

  double boundaryLayerThickness{};
  double boundaryLayerThicknessVelocityDerivative{};
  double boundaryLayerThicknessMomentumDerivative{};
  double boundaryLayerThicknessDisplacementDerivative{};
  double boundaryLayerThicknessMachSquaredDerivative{};

  [[nodiscard]]
  bool operator==(
      const BoundaryLayerStationState&) const = default;
};

}  // namespace xfoil::boundary_layer

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

// File: include/xfoil/AnalysisData.h
// Public owning data model for panel geometry, surface distributions and
// a detailed operating point. All containers own their data; no view or
// pointer into session state is exposed. Units and ordering are
// documented per field.

#pragma once

#include <optional>
#include <vector>

#include "xfoil/Diagnostic.h"
#include "xfoil/SkinFriction.h"
#include "xfoil/Types.h"

namespace xfoil {

// Status of a single analysis point or a sequence point.
enum class ConvergenceStatus {
  notRun,
  converged,
  unconverged,
  cancelled,
  failed
};

// Current panel contour in panel-node order. Normalized chord
// coordinates. The trailing edge appears as the first and the last node
// (upper and lower TE); there is no extra duplicate closing point. The
// contour is implicitly closed by the last panel.
struct SurfaceGeometry {
  std::vector<Point2D> points;
};

// Source of a surface distribution.
enum class SurfaceDistributionKind {
  inviscid,
  viscous
};

// Cp distribution on the airfoil surface nodes only (no wake nodes), in
// the same order as SurfaceGeometry::points. x/y are normalized chord
// coordinates and cp is dimensionless. x, y and cp always have the same
// length.
struct SurfaceDistribution {
  SurfaceDistributionKind kind{SurfaceDistributionKind::inviscid};
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> cp;
};

// Detailed result of one analysis point. This is an additive detail type;
// the existing lightweight xfoil::AnalysisResult remains source-compatible.
// Angles are degrees, coefficients are dimensionless, x positions are
// normalized chord coordinates.
struct OperatingPointData {
  Degrees alpha{};
  double liftCoefficient{};
  // Total drag coefficient CD from the existing solver result.
  double dragCoefficient{};
  // Present only when the session state carries the pressure/form-drag
  // coefficient CDP for the last point (viscous VISCAL completion).
  std::optional<double> dragCoefficientPressure;
  double momentCoefficient{};
  double centerOfPressureX{};
  int iterations{};
  bool viscous{};
  ConvergenceStatus convergence{ConvergenceStatus::notRun};
  // Present when a valid surface distribution belongs to the point.
  std::optional<SurfaceDistribution> surface;
  // Solver diagnostics in emission order. Optional and may be empty.
  std::vector<Diagnostic> diagnostics;
  // Scalar friction-drag coefficient CDF from the canonical viscous state.
  // Present only for a successful viscous point with a valid final
  // VISCAL state; absent for inviscid points and failed/cancelled points.
  std::optional<double> dragCoefficientFriction;
  // Owning Cf distribution for the same valid viscous point. This is a
  // separate BL-station data set from the panel-node Cp distribution.
  // Present iff dragCoefficientFriction is present.
  std::optional<SkinFrictionDistribution> skinFriction;
};

}  // namespace xfoil

// End: include/xfoil/AnalysisData.h

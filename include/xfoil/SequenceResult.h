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

// File: include/xfoil/SequenceResult.h
// Public owning polar and sequence point model. Points are stored in the
// requested sequence order; partial sequences and non-converged points
// are represented explicitly. No file output and no hidden state.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "xfoil/AnalysisData.h"
#include "xfoil/Diagnostic.h"

namespace xfoil {

// Why a sequence stopped. A completed sequence reached its requested end;
// the other reasons describe a controlled stop.
enum class SequenceStopReason {
  notRun,
  completed,
  cancelled,
  nonConvergence,
  failure
};

// One polar or sequence point. alpha is in degrees, coefficients are
// dimensionless, machNumber and reynoldsNumber are the canonical
// point values. The optional polar-only columns are present when the
// point comes from a stored polar dataset; they are absent for a point
// built only from an xfoil::AnalysisResult.
struct PolarPoint {
  Degrees alpha{};
  double liftCoefficient{};
  // Total drag coefficient CD (XFOIL ICD column).
  double dragCoefficient{};
  // Optional pressure/form-drag coefficient CDP (XFOIL ICP column).
  std::optional<double> dragCoefficientPressure;
  double momentCoefficient{};

  std::optional<double> dragCoefficientWave;
  // XFOIL CDv column: total viscous drag coefficient, not the
  // friction-only dragCoefficientFriction value of OperatingPointData.
  std::optional<double> dragCoefficientViscous;
  std::optional<double> machNumber;
  std::optional<double> reynoldsNumber;
  std::optional<double> hingeMoment;
  std::optional<double> minimumPressureCoefficient;
  std::optional<double> hingeDragCoefficient;
  std::optional<double> momentCoefficientDerivative;

  ConvergenceStatus convergence{ConvergenceStatus::notRun};
  std::vector<Diagnostic> diagnostics;
};

// A partial or complete sequence. points are in the requested order;
// stoppedAtIndex is the 0-based index of the point that ended the
// sequence, if any. The result owns all point and diagnostic data.
struct SequenceResult {
  std::vector<PolarPoint> points;
  SequenceStopReason stopReason{SequenceStopReason::notRun};
  std::optional<std::size_t> stoppedAtIndex;
  std::vector<Diagnostic> diagnostics;
};

}  // namespace xfoil

// End: include/xfoil/SequenceResult.h

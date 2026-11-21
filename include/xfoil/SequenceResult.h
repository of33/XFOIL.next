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
// point values. Optional scalar columns are present only when the
// corresponding canonical value exists for the point. Live sequence
// points fill the scalar drag and iteration fields from the point's
// completed solver state; stored-polar points fill the stored columns.
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
  // Friction-only drag coefficient CDF from the canonical final VISCAL
  // state. This is not CDv and is never CD - CDp. Present for a successful
  // viscous live-sequence point; absent for inviscid, cancelled or failed
  // points and for stored polar points that do not carry CDF.
  std::optional<double> dragCoefficientFriction;
  // Executed analysis iteration count. Present for a successful live
  // sequence point (including inviscid point count 0); absent for failed or
  // cancelled points and for stored polar points that do not carry it.
  std::optional<std::size_t> iterations;
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

// One detailed sequence point. polar is the same compact scalar row that
// SequenceResult carries. operatingPoint is the owning full detail of the
// executed point: Cp surface data and, for a valid viscous point, Cf and
// CDF. It is absent when no consistent operating point was produced.
struct DetailedSequencePoint {
  PolarPoint polar;
  std::optional<OperatingPointData> operatingPoint;
};

// Detailed result of an alpha sequence. points follow the requested
// sequence order. The stop metadata is the same as SequenceResult; this
// type is deliberately more memory-intensive because every executed point
// owns its full Cp/Cf distributions. The compact SequenceResult remains
// available for scalar polar work.
struct DetailedSequenceResult {
  std::vector<DetailedSequencePoint> points;
  SequenceStopReason stopReason{SequenceStopReason::notRun};
  std::optional<std::size_t> stoppedAtIndex;
  std::vector<Diagnostic> diagnostics;
};

}  // namespace xfoil

// End: include/xfoil/SequenceResult.h

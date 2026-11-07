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

// File: include/xfoil/AnalysisResult.h
// Structured, copyable result of one analysis point.
// Self-contained value type without spans, pointers or references; it stays
// valid after further session calls. successful == true means the run took
// place (also for a non-converged point); converged == true only for status
// converged. Initialization, configuration, abort and
// Compressibility error set successful == false.

#pragma once

#include <string>

#include "xfoil/Types.h"

namespace xfoil {

enum class AnalysisStatus {
  notAnalyzed,
  converged,
  unconverged,
  cancelled,
  missingGeometry,
  missingConfiguration,
  invalidConfiguration,
  geometryTooLarge,
  geometryInitializationFailed,
  analysisInitializationFailed,
  invalidAnalysisSettings,
  compressibilityFailure,
  finalizationFailed
};

struct AnalysisResult {
  AnalysisStatus status{AnalysisStatus::missingGeometry};

  bool successful{};
  bool converged{};
  int iterations{};

  Degrees alpha{};

  double liftCoefficient{};
  double dragCoefficient{};
  double momentCoefficient{};
  double centerOfPressureX{};
  double rmsResidual{};
  double maximumResidual{};

  std::string diagnostic;
};

}  // namespace xfoil

// End: include/xfoil/AnalysisResult.h

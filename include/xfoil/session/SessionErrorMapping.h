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

// File: include/xfoil/session/SessionErrorMapping.h
// Internal mapping from existing session/analysis status values to the
// public L2 Error contract. Header-only and free of I/O.

#pragma once

#include <string>
#include <utility>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Result.h"
#include "xfoil/SessionTypes.h"

namespace xfoil::session {

[[nodiscard]]
inline Error analysisStatusError(AnalysisStatus status) {
  switch (status) {
    case AnalysisStatus::notAnalyzed:
      return Error::make(ErrorCategory::invalidState, "analysis.notAnalyzed",
                         "No analysis point has been run", "analysis");
    case AnalysisStatus::missingGeometry:
      return Error::make(ErrorCategory::invalidState,
                         "analysis.missingGeometry",
                         "No valid panel geometry is available", "analysis");
    case AnalysisStatus::missingConfiguration:
      return Error::make(ErrorCategory::invalidState,
                         "analysis.missingConfiguration",
                         "No validated analysis configuration is available",
                         "analysis");
    case AnalysisStatus::invalidConfiguration:
      return Error::make(ErrorCategory::invalidArgument,
                         "analysis.invalidConfiguration",
                         "The analysis configuration is invalid", "analysis");
    case AnalysisStatus::geometryTooLarge:
      return Error::make(ErrorCategory::capacityExceeded,
                         "analysis.geometryTooLarge",
                         "The geometry exceeds the supported capacity",
                         "analysis");
    case AnalysisStatus::geometryInitializationFailed:
      return Error::make(ErrorCategory::invalidState,
                         "analysis.geometryInitializationFailed",
                         "Geometry initialization failed", "analysis");
    case AnalysisStatus::analysisInitializationFailed:
      return Error::make(ErrorCategory::invalidState,
                         "analysis.analysisInitializationFailed",
                         "Analysis initialization failed", "analysis");
    case AnalysisStatus::invalidAnalysisSettings:
      return Error::make(ErrorCategory::invalidArgument,
                         "analysis.invalidAnalysisSettings",
                         "The analysis settings were rejected by the solver",
                         "analysis");
    case AnalysisStatus::compressibilityFailure:
      return Error::make(ErrorCategory::numericalFailure,
                         "analysis.compressibilityFailure",
                         "The compressibility calculation failed", "analysis");
    case AnalysisStatus::finalizationFailed:
      return Error::make(ErrorCategory::internal,
                         "analysis.finalizationFailed",
                         "Analysis finalization failed", "analysis");
    case AnalysisStatus::cancelled:
      return Error::make(ErrorCategory::cancelled, "analysis.cancelled",
                         "The analysis was cancelled", "analysis");
    case AnalysisStatus::progressCallbackFailure:
      return Error::make(ErrorCategory::invalidArgument,
                         "progress.callbackFailure",
                         "The progress callback failed", "progress");
    case AnalysisStatus::converged:
    case AnalysisStatus::unconverged:
      break;
  }
  return Error::make(ErrorCategory::internal, "analysis.unexpectedStatus",
                     "Unexpected successful analysis status", "analysis");
}

[[nodiscard]]
inline Error geometrySetError(const GeometrySetResult& result) {
  switch (result.error) {
    case GeometrySetError::none:
      return Error::make(ErrorCategory::internal, "geometry.none",
                         "No geometry error is present", "geometry");
    case GeometrySetError::emptyGeometry:
      return Error::make(ErrorCategory::invalidArgument, "geometry.empty",
                         result.message.empty()
                             ? std::string{"Geometry is empty or incomplete"}
                             : result.message,
                         "geometry");
    case GeometrySetError::tooManyPoints:
      return Error::make(ErrorCategory::capacityExceeded,
                         "geometry.tooManyPoints",
                         result.message.empty()
                             ? std::string{"Geometry exceeds the point limit"}
                             : result.message,
                         "geometry");
    case GeometrySetError::panelInitializationFailed:
      return Error::make(ErrorCategory::invalidState,
                         "geometry.panelInitializationFailed",
                         result.message.empty()
                             ? std::string{"Panel initialization failed"}
                             : result.message,
                         "geometry");
  }
  return Error::make(ErrorCategory::internal, "geometry.unexpectedError",
                     "Unexpected geometry error", "geometry");
}

[[nodiscard]]
inline Error configError(const ConfigValidationResult& result) {
  const std::string message =
      result.message.empty() ? std::string{"Invalid analysis configuration"}
                             : result.message;
  switch (result.error) {
    case ConfigError::none:
      return Error::make(ErrorCategory::internal, "config.none",
                         "No configuration error is present", "configuration");
    case ConfigError::nonFiniteValue:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.nonFiniteValue", message, "configuration");
    case ConfigError::invalidReynoldsNumber:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.invalidReynoldsNumber", message,
                         "configuration");
    case ConfigError::invalidMachNumber:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.invalidMachNumber", message,
                         "configuration");
    case ConfigError::invalidTransition:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.invalidTransition", message,
                         "configuration");
    case ConfigError::invalidIterationLimit:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.invalidIterationLimit", message,
                         "configuration");
    case ConfigError::invalidAcceleration:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.invalidAcceleration", message,
                         "configuration");
    case ConfigError::invalidDependency:
      return Error::make(ErrorCategory::invalidArgument,
                         "config.invalidDependency", message,
                         "configuration");
  }
  return Error::make(ErrorCategory::internal, "config.unexpectedError",
                     "Unexpected configuration error", "configuration");
}

}  // namespace xfoil::session

// End: include/xfoil/session/SessionErrorMapping.h

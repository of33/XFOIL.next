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

// File: src/core/AnalysisConfig.cpp
// Validation of the analysis configuration. The rules require all double
// values to be finite,
// reynoldsNumber >= 0 (for viscous > 0), machNumber in the range [0, 1),
// nCritical > 0, trip-X in the range [0, 1], maximumIterations > 0,
// viscousAcceleration >= 0, and both enums at a defined value.
// Invalid values are never corrected automatically.

#include "xfoil/AnalysisConfig.h"

#include <cmath>

namespace xfoil {

namespace {

constexpr double kTripLowerBound = 0.0;
constexpr double kTripUpperBound = 1.0;

}  // namespace

ConfigValidationResult validate(const AnalysisConfig& config) noexcept {
  ConfigValidationResult result;

  const bool allFinite =
    std::isfinite(config.reynoldsNumber) &&
    std::isfinite(config.machNumber) &&
    std::isfinite(config.transition.nCritical) &&
    std::isfinite(config.transition.topTripX) &&
    std::isfinite(config.transition.bottomTripX) &&
    std::isfinite(config.viscousAcceleration);

  if (!allFinite) {
    result.error = ConfigError::nonFiniteValue;
    result.message = "Analysis configuration contains a non-finite value";
    return result;
  }

  if (config.reynoldsNumber < 0.0) {
    result.error = ConfigError::invalidReynoldsNumber;
    result.message = "Reynolds number must not be negative";
    return result;
  }

  if (config.viscous && config.reynoldsNumber == 0.0) {
    result.error = ConfigError::invalidReynoldsNumber;
    result.message = "Viscous analysis requires a positive Reynolds number";
    return result;
  }

  if (config.machNumber < 0.0 || config.machNumber >= 1.0) {
    result.error = ConfigError::invalidMachNumber;
    result.message = "Mach number must be in the range [0, 1)";
    return result;
  }

  const bool transitionValid =
      config.transition.nCritical > 0.0 &&
      config.transition.topTripX >= kTripLowerBound &&
      config.transition.topTripX <= kTripUpperBound &&
      config.transition.bottomTripX >= kTripLowerBound &&
      config.transition.bottomTripX <= kTripUpperBound;

  if (!transitionValid) {
    result.error = ConfigError::invalidTransition;
    result.message = "Transition settings are invalid";
    return result;
  }

  if (config.maximumIterations <= 0) {
    result.error = ConfigError::invalidIterationLimit;
    result.message = "Maximum iteration count must be positive";
    return result;
  }

  if (config.viscousAcceleration < 0.0) {
    result.error = ConfigError::invalidAcceleration;
    result.message = "Viscous acceleration must not be negative";
    return result;
  }

  switch (config.reynoldsDependency) {
    case ReynoldsDependency::constant:
    case ReynoldsDependency::inverseSquareRootLift:
    case ReynoldsDependency::inverseLift:
      break;
    default:
      result.error = ConfigError::invalidDependency;
      result.message = "Analysis dependency type is invalid";
      return result;
  }

  switch (config.machDependency) {
    case MachDependency::constant:
    case MachDependency::inverseSquareRootLift:
    case MachDependency::inverseLift:
      break;
    default:
      result.error = ConfigError::invalidDependency;
      result.message = "Analysis dependency type is invalid";
      return result;
  }

  return result;
}

}  // namespace xfoil

// End: src/core/AnalysisConfig.cpp

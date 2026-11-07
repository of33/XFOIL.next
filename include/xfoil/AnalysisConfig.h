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

// File: include/xfoil/AnalysisConfig.h
// Analysis configuration that can be validated. Self-contained value type
// without dependency on XFoil.h. Invalid values are reported exclusively
// via ConfigValidationResult; no automatic correction takes place
// instead.

#pragma once

#include <string>

namespace xfoil {

enum class ReynoldsDependency {
  constant = 1,
  inverseSquareRootLift = 2,
  inverseLift = 3
};

enum class MachDependency {
  constant = 1,
  inverseSquareRootLift = 2,
  inverseLift = 3
};

struct TransitionConfig {
  double nCritical{9.0};
  double topTripX{1.0};
  double bottomTripX{1.0};
};

struct AnalysisConfig {
  double reynoldsNumber{100000.0};
  double machNumber{0.0};
  bool viscous{true};

  ReynoldsDependency reynoldsDependency{
      ReynoldsDependency::constant};

  MachDependency machDependency{
      MachDependency::constant};

  TransitionConfig transition;
  int maximumIterations{100};
  bool autoInitializeBoundaryLayer{true};
  double viscousAcceleration{0.01};
};

enum class ConfigError {
  none,
  nonFiniteValue,
  invalidReynoldsNumber,
  invalidMachNumber,
  invalidTransition,
  invalidIterationLimit,
  invalidAcceleration,
  invalidDependency
};

struct ConfigValidationResult {
  ConfigError error{ConfigError::none};
  std::string message;

  [[nodiscard]]
  bool success() const noexcept {
    return error == ConfigError::none;
  }
};

[[nodiscard]]
ConfigValidationResult validate(
    const AnalysisConfig& config) noexcept;

}  // namespace xfoil

// End: include/xfoil/AnalysisConfig.h

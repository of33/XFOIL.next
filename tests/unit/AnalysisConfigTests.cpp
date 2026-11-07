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

// File: tests/unit/AnalysisConfigTests.cpp
// unit tests for validation of the analysis configuration.
// the expected error messages are asserted verbatim.

#include "TestSupport.h"

#include "xfoil/AnalysisConfig.h"

#include <limits>
#include <string>

TEST(unit, DefaultAnalysisConfigIsValid) {
  const xfoil::AnalysisConfig config;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK(result.success());
  CHECK(result.message.empty());
}

TEST(unit, InviscidConfigAllowsZeroReynoldsNumber) {
  xfoil::AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 0.0;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK(result.success());
}

TEST(unit, ViscousConfigRejectsZeroReynoldsNumber) {
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 0.0;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidReynoldsNumber);
  CHECK_EQ(
      result.message,
      std::string("Viscous analysis requires a positive Reynolds number"));
}

TEST(unit, ConfigRejectsNegativeReynoldsNumber) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = -1.0;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidReynoldsNumber);
  CHECK_EQ(
      result.message,
      std::string("Reynolds number must not be negative"));
}

TEST(unit, ConfigRejectsMachBelowZero) {
  xfoil::AnalysisConfig config;
  config.machNumber = -0.5;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidMachNumber);
  CHECK_EQ(
      result.message,
      std::string("Mach number must be in the range [0, 1)"));
}

TEST(unit, ConfigRejectsMachAtOne) {
  xfoil::AnalysisConfig config;
  config.machNumber = 1.0;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidMachNumber);
}

TEST(unit, ConfigRejectsNonFiniteValue) {
  xfoil::AnalysisConfig infinityCase;
  infinityCase.reynoldsNumber = std::numeric_limits<double>::infinity();
  const xfoil::ConfigValidationResult infinityResult =
      xfoil::validate(infinityCase);
  CHECK_FALSE(infinityResult.success());
  CHECK(infinityResult.error == xfoil::ConfigError::nonFiniteValue);
  CHECK_EQ(
      infinityResult.message,
      std::string("Analysis configuration contains a non-finite value"));

  xfoil::AnalysisConfig nanCase;
  nanCase.transition.nCritical = std::numeric_limits<double>::quiet_NaN();
  const xfoil::ConfigValidationResult nanResult =
      xfoil::validate(nanCase);
  CHECK_FALSE(nanResult.success());
  CHECK(nanResult.error == xfoil::ConfigError::nonFiniteValue);

  xfoil::AnalysisConfig nanAcceleration;
  nanAcceleration.viscousAcceleration =
  std::numeric_limits<double>::quiet_NaN();

  const xfoil::ConfigValidationResult nanAccelerationResult =
  xfoil::validate(nanAcceleration);

  CHECK_FALSE(nanAccelerationResult.success());
  CHECK(nanAccelerationResult.error ==
  xfoil::ConfigError::nonFiniteValue);
  CHECK_EQ(
    nanAccelerationResult.message,
    std::string(
      "Analysis configuration contains a non-finite value"));

  xfoil::AnalysisConfig infiniteAcceleration;
  infiniteAcceleration.viscousAcceleration =
  std::numeric_limits<double>::infinity();

  const xfoil::ConfigValidationResult infiniteAccelerationResult =
  xfoil::validate(infiniteAcceleration);

  CHECK_FALSE(infiniteAccelerationResult.success());
  CHECK(infiniteAccelerationResult.error ==
  xfoil::ConfigError::nonFiniteValue);
}

TEST(unit, ConfigRejectsInvalidTransition) {
  xfoil::AnalysisConfig zeroCritical;
  zeroCritical.transition.nCritical = 0.0;
  const xfoil::ConfigValidationResult zeroResult =
      xfoil::validate(zeroCritical);
  CHECK_FALSE(zeroResult.success());
  CHECK(zeroResult.error == xfoil::ConfigError::invalidTransition);
  CHECK_EQ(
      zeroResult.message,
      std::string("Transition settings are invalid"));

  xfoil::AnalysisConfig negativeTrip;
  negativeTrip.transition.topTripX = -0.25;
  const xfoil::ConfigValidationResult negativeResult =
      xfoil::validate(negativeTrip);
  CHECK_FALSE(negativeResult.success());
  CHECK(negativeResult.error == xfoil::ConfigError::invalidTransition);

  xfoil::AnalysisConfig tooLargeTrip;
  tooLargeTrip.transition.bottomTripX = 1.25;
  const xfoil::ConfigValidationResult largeResult =
      xfoil::validate(tooLargeTrip);
  CHECK_FALSE(largeResult.success());
  CHECK(largeResult.error == xfoil::ConfigError::invalidTransition);
}

TEST(unit, ConfigRejectsNonPositiveIterationLimit) {
  xfoil::AnalysisConfig zeroLimit;
  zeroLimit.maximumIterations = 0;
  const xfoil::ConfigValidationResult zeroResult =
      xfoil::validate(zeroLimit);
  CHECK_FALSE(zeroResult.success());
  CHECK(zeroResult.error == xfoil::ConfigError::invalidIterationLimit);
  CHECK_EQ(
      zeroResult.message,
      std::string("Maximum iteration count must be positive"));

  xfoil::AnalysisConfig negativeLimit;
  negativeLimit.maximumIterations = -3;
  const xfoil::ConfigValidationResult negativeResult =
      xfoil::validate(negativeLimit);
  CHECK_FALSE(negativeResult.success());
  CHECK(negativeResult.error == xfoil::ConfigError::invalidIterationLimit);
}

TEST(unit, ConfigRejectsNegativeAcceleration) {
  xfoil::AnalysisConfig config;
  config.viscousAcceleration = -0.5;
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidAcceleration);
  CHECK_EQ(
      result.message,
      std::string("Viscous acceleration must not be negative"));
}

TEST(unit, ConfigRejectsInvalidReynoldsDependency) {
  xfoil::AnalysisConfig config;
  config.reynoldsDependency =
      static_cast<xfoil::ReynoldsDependency>(999);
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidDependency);
  CHECK_EQ(
      result.message,
      std::string("Analysis dependency type is invalid"));
}

TEST(unit, ConfigRejectsInvalidMachDependency) {
  xfoil::AnalysisConfig config;
  config.machDependency =
      static_cast<xfoil::MachDependency>(999);
  const xfoil::ConfigValidationResult result = xfoil::validate(config);
  CHECK_FALSE(result.success());
  CHECK(result.error == xfoil::ConfigError::invalidDependency);
  CHECK_EQ(
      result.message,
      std::string("Analysis dependency type is invalid"));
}

// End: tests/unit/AnalysisConfigTests.cpp

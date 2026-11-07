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

// File: tests/unit/NumericArgumentsTests.cpp
// unit tests for the numeric argument parser. The
// diagnostics are reported without taking absolute values:
// "Invalid numeric argument: <token>" and
// "numeric argument out of range: <token>".

#include "TestSupport.h"
#include "app/cli/NumericArguments.h"

#include <string>
#include <vector>

namespace {

using xfoil::app::cli::NumericArgumentError;
using xfoil::app::cli::parseIntegerArguments;
using xfoil::app::cli::parseRealArguments;

bool valuesEqual(const std::vector<double>& actual,
                 const std::vector<double>& expected) {
  if (actual.size() != expected.size()) {
    return false;
  }
  for (std::size_t index = 0; index < actual.size(); ++index) {
    const double difference = actual[index] - expected[index];
    const double magnitude =
        expected[index] < 0.0 ? -expected[index] : expected[index];
    if (difference > 1.0e-12 * magnitude ||
        difference < -1.0e-12 * magnitude) {
      return false;
    }
  }
  return true;
}

bool intValuesEqual(const std::vector<int>& actual,
                    const std::vector<int>& expected) {
  return actual == expected;
}

}  // namespace

TEST(unit, RealArgumentsAcceptEmptyInput) {
  const auto empty = parseRealArguments("");
  CHECK(empty.success());
  CHECK(empty.values.empty());

  const auto blanks = parseRealArguments("   ");
  CHECK(blanks.success());
  CHECK(blanks.values.empty());
}

TEST(unit, RealArgumentsParseWhitespaceSeparatedValues) {
  const auto result = parseRealArguments("1.0 2.5 -3");
  CHECK(result.success());
  CHECK(valuesEqual(result.values, std::vector<double>{1.0, 2.5, -3.0}));
}

TEST(unit, RealArgumentsParseCommaSeparatedValues) {
  const auto result = parseRealArguments("4,-2.5,0");
  CHECK(result.success());
  CHECK(valuesEqual(result.values, std::vector<double>{4.0, -2.5, 0.0}));

  const auto mixed = parseRealArguments("1 ,2, 3");
  CHECK(mixed.success());
  CHECK(valuesEqual(mixed.values, std::vector<double>{1.0, 2.0, 3.0}));
}

TEST(unit, RealArgumentsParseFortranDExponent) {
  const auto lower = parseRealArguments("1d3");
  CHECK(lower.success());
  CHECK(valuesEqual(lower.values, std::vector<double>{1000.0}));

  const auto upper = parseRealArguments("2.5D-2");
  CHECK(upper.success());
  CHECK(valuesEqual(upper.values, std::vector<double>{0.025}));

  const auto eNotation = parseRealArguments("1e3 1E3");
  CHECK(eNotation.success());
  CHECK(valuesEqual(eNotation.values, std::vector<double>{1000.0, 1000.0}));
}

TEST(unit, RealArgumentsPreserveSigns) {
  const auto result = parseRealArguments("+1.5 -2.5 +3 -4");
  CHECK(result.success());
  CHECK(valuesEqual(result.values,
                    std::vector<double>{1.5, -2.5, 3.0, -4.0}));

  const auto leadingDot = parseRealArguments("-.5 +.5");
  CHECK(leadingDot.success());
  CHECK(valuesEqual(leadingDot.values, std::vector<double>{-0.5, 0.5}));
}

TEST(unit, RealArgumentsRejectInvalidToken) {
  const auto invalid = parseRealArguments("1.0 abc 2.0");
  CHECK(!invalid.success());
  CHECK(invalid.error == NumericArgumentError::invalidToken);
  CHECK_EQ(invalid.diagnostic, std::string{"Invalid numeric argument: abc"});

  const auto nanToken = parseRealArguments("nan");
  CHECK(!nanToken.success());
  CHECK(nanToken.error == NumericArgumentError::invalidToken);

  const auto infToken = parseRealArguments("inf");
  CHECK(!infToken.success());
  CHECK(infToken.error == NumericArgumentError::invalidToken);

  const auto trailing = parseRealArguments("1.2x");
  CHECK(!trailing.success());
  CHECK_EQ(trailing.diagnostic, std::string{"Invalid numeric argument: 1.2x"});

  const auto partial = parseRealArguments("--3");
  CHECK(!partial.success());
  CHECK(partial.error == NumericArgumentError::invalidToken);
}

TEST(unit, RealArgumentsRejectNanAndInfinity) {
  const auto nan = parseRealArguments("NaN");
  CHECK(!nan.success());
  CHECK_EQ(nan.diagnostic, std::string{"Invalid numeric argument: NaN"});

  const auto infinity = parseRealArguments("Infinity");
  CHECK(!infinity.success());
  CHECK(infinity.error == NumericArgumentError::invalidToken);
}

TEST(unit, RealArgumentsLimitToFirst128Characters) {
  // FCPP013: GETFLT/GETINT parse only the first 128 characters; a value
  // that starts only at character 129 is ignored.
  const std::string input = "1 2 3" + std::string(123, ' ') + "4";
  CHECK_EQ(input.size(), std::size_t{129});
  const auto result = parseRealArguments(input);
  CHECK(result.success());
  CHECK(valuesEqual(result.values, std::vector<double>{1.0, 2.0, 3.0}));
}

TEST(unit, RealArgumentsTreatLeadingCommaAsSeparator) {
  // FCPP014/FCPP013: a leading separator comma (from ASKC: CARGS keeps
  // the comma) is treated as a separator and skipped, as in Fortran.
  const auto result = parseRealArguments(",2412");
  CHECK(result.success());
  CHECK(valuesEqual(result.values, std::vector<double>{2412.0}));
}

TEST(unit, RealArgumentsRejectOutOfRange) {
  const auto overflow = parseRealArguments("1e999");
  CHECK(!overflow.success());
  CHECK(overflow.error == NumericArgumentError::outOfRange);
  CHECK_EQ(overflow.diagnostic,
           std::string{"Numeric argument out of range: 1e999"});

  // FCPP013: pure digit strings are limited to 128 characters and
  // therefore stay within the double range (max. ~1e127 < ~1.8e308); only
  // an exponent can overflow.
  const std::string maxDigits(128, '9');
  const auto inRange = parseRealArguments(maxDigits);
  CHECK(inRange.success());
  CHECK_EQ(inRange.values.size(), std::size_t{1});
}

TEST(unit, IntegerArgumentsAcceptEmptyInput) {
  const auto empty = parseIntegerArguments("");
  CHECK(empty.success());
  CHECK(empty.values.empty());

  const auto blanks = parseIntegerArguments("  , ");
  CHECK(blanks.success());
  CHECK(blanks.values.empty());
}

TEST(unit, IntegerArgumentsParseMultipleValues) {
  const auto result = parseIntegerArguments("1 -2 +3");
  CHECK(result.success());
  CHECK(intValuesEqual(result.values, std::vector<int>{1, -2, 3}));

  const auto commas = parseIntegerArguments("10,20, 30");
  CHECK(commas.success());
  CHECK(intValuesEqual(commas.values, std::vector<int>{10, 20, 30}));
}

TEST(unit, IntegerArgumentsRejectFraction) {
  const auto result = parseIntegerArguments("1.5");
  CHECK(!result.success());
  CHECK(result.error == NumericArgumentError::invalidToken);
  CHECK_EQ(result.diagnostic, std::string{"Invalid numeric argument: 1.5"});

  const auto exponent = parseIntegerArguments("2e3");
  CHECK(!exponent.success());
  CHECK(exponent.error == NumericArgumentError::invalidToken);
}

TEST(unit, IntegerArgumentsRejectOutOfRange) {
  const auto result = parseIntegerArguments("99999999999999999999");
  CHECK(!result.success());
  CHECK(result.error == NumericArgumentError::outOfRange);
  CHECK_EQ(result.diagnostic,
           std::string{"Numeric argument out of range: 99999999999999999999"});

  const auto below = parseIntegerArguments("-99999999999999999999");
  CHECK(!below.success());
  CHECK(below.error == NumericArgumentError::outOfRange);
}

// End: tests/unit/NumericArgumentsTests.cpp

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

// file: tests/unit/InputCapacityS20Tests.cpp
// Focused S20 boundary coverage for the 256-character GETINT/GETFLT
// parse view.

#include "TestSupport.h"

#include "app/cli/NumericArguments.h"
#include "xfoil/io/TextFormat.h"

#include <string>
#include <vector>

namespace {

using xfoil::app::cli::parseIntegerArguments;
using xfoil::app::cli::parseRealArguments;

std::string paddedPrefix(std::size_t length, const std::string& suffix) {
  if (length <= suffix.size()) {
    return suffix;
  }
  return std::string(length - suffix.size(), ' ') + suffix;
}

}  // namespace

TEST(unit, InputCapacityS20RealArgumentsIncludeCharacter129) {
  const std::string input = "1" + std::string(200, ' ') + "2";
  const auto result = parseRealArguments(input);
  CHECK(result.success());
  CHECK_EQ(result.values.size(), std::size_t{2});
  CHECK_EQ(result.values[0], 1.0);
  CHECK_EQ(result.values[1], 2.0);
}

TEST(unit, InputCapacityS20RealArgumentsIncludeCharacter256) {
  const std::string input = paddedPrefix(256, "12");
  CHECK_EQ(input.size(), std::size_t{256});
  const auto result = parseRealArguments(input);
  CHECK(result.success());
  CHECK_EQ(result.values.size(), std::size_t{1});
  CHECK_EQ(result.values[0], 12.0);
}

TEST(unit, InputCapacityS20RealArgumentsIgnoreCharacter257) {
  const std::string input = std::string(256, ' ') + "9";
  CHECK_EQ(input.size(), std::size_t{257});
  const auto result = parseRealArguments(input);
  CHECK(result.success());
  CHECK(result.values.empty());
}

TEST(unit, InputCapacityS20RealArgumentsRecognizeRightEdgeToken) {
  const std::string input = paddedPrefix(255, "7");
  CHECK_EQ(input.size(), std::size_t{255});
  const auto result = parseRealArguments(input);
  CHECK(result.success());
  CHECK_EQ(result.values.size(), std::size_t{1});
  CHECK_EQ(result.values[0], 7.0);
}

TEST(unit, InputCapacityS20RealArgumentsKeepDExponentAfterOldLimit) {
  const std::string input = "1" + std::string(200, ' ') + "2.5D-2";
  const auto result = parseRealArguments(input);
  CHECK(result.success());
  CHECK_EQ(result.values.size(), std::size_t{2});
  CHECK_EQ(result.values[0], 1.0);
  CHECK_EQ(result.values[1], 0.025);
}

TEST(unit, InputCapacityS20IntegerArgumentsIncludeCharacter256) {
  const std::string input = paddedPrefix(256, "42");
  const auto result = parseIntegerArguments(input);
  CHECK(result.success());
  CHECK_EQ(result.values.size(), std::size_t{1});
  CHECK_EQ(result.values[0], 42);
}

TEST(unit, InputCapacityS20IntegerArgumentsIgnoreCharacter257) {
  const std::string input = std::string(256, ' ') + "9";
  const auto result = parseIntegerArguments(input);
  CHECK(result.success());
  CHECK(result.values.empty());
}

TEST(unit, InputCapacityS20PromptViewRetainsA80Limit) {
  const std::string atEighty = paddedPrefix(81, "2");
  const auto excluded = parseRealArguments(atEighty, 80);
  CHECK(excluded.success());
  CHECK(excluded.values.empty());

  const std::string belowEighty = paddedPrefix(79, "2");
  const auto included = parseRealArguments(belowEighty, 80);
  CHECK(included.success());
  CHECK_EQ(included.values.size(), std::size_t{1});
  CHECK_EQ(included.values[0], 2.0);
}

TEST(unit, InputCapacityS20TextFormatUses256CharacterView) {
  const std::string included = paddedPrefix(256, "12");
  const auto parsedIncluded = xfoil::io::parseFloatList(included, 0);
  CHECK(!parsedIncluded.error);
  CHECK_EQ(parsedIncluded.values.size(), std::size_t{1});
  CHECK_EQ(parsedIncluded.values[0], 12.0);

  const std::string excluded = std::string(256, ' ') + "9";
  const auto parsedExcluded = xfoil::io::parseFloatList(excluded, 0);
  CHECK(!parsedExcluded.error);
  CHECK(parsedExcluded.values.empty());
}

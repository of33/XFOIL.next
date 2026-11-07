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

// File: tests/integration/OperNameTests.cpp
// tests for NAME, NINC and NDEC in the OPER menu.

#include "TestSupport.h"

#include "app/InteractiveApplication.h"

#include <optional>
#include <sstream>
#include <string>

namespace {

struct NameRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

NameRunResult runNameSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  NameRunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input,
      output,
      errorOutput,
      options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
}

}  // namespace

TEST(integration, OperNameSetsCliAirfoilName) {
  const auto result = runNameSession(
      "OPER\n"
      "NAME TestFoil\n"
      "NAME\n"
      "\n"
      "\n"
      "QUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(
      result.outputText.find(
          "Enter airfoil name or <return> for default:  TestFoil") !=
      std::string::npos);
}

TEST(integration, OperNincIncrementsNameVersion) {
  const auto result = runNameSession(
      "OPER\n"
      "NAME TestFoil\n"
      "NINC\n"
      "\n"
      "NAME\n"
      "\n"
      "\n"
      "QUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(
      result.outputText.find(
          "Enter airfoil name or <return> for default:  "
          "TestFoil [1]") != std::string::npos);
}

TEST(integration, OperNdecDecrementsNameVersion) {
  const auto result = runNameSession(
      "OPER\n"
      "NAME TestFoil [2]\n"
      "NDEC\n"
      "\n"
      "NAME\n"
      "\n"
      "\n"
      "QUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(
      result.outputText.find(
          "Enter airfoil name or <return> for default:  "
          "TestFoil [1]") != std::string::npos);
}

TEST(integration, OperNameCommandsPreserveDefaultOnEmptyInput) {
  const auto result = runNameSession(
      "OPER\n"
      "NAME TestFoil\n"
      "NINC\n"
      "\n"
      "NAME\n"
      "\n"
      "NAME\n"
      "\n"
      "\n"
      "QUIT\n");

  CHECK_EQ(result.exitCode, 0);

  const std::string expected =
      "Enter airfoil name or <return> for default:  TestFoil [1]";

  const std::size_t first = result.outputText.find(expected);
  CHECK(first != std::string::npos);

  if (first != std::string::npos) {
    CHECK(
        result.outputText.find(
            expected,
            first + expected.size()) !=
        std::string::npos);
  }
}

// End: tests/integration/OperNameTests.cpp

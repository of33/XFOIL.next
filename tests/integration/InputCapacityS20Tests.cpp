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

// file: tests/integration/InputCapacityS20Tests.cpp
// Focused S20 integration coverage for the 256-character numeric parser.

#include "TestSupport.h"

#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

struct SessionResult {
  int exitCode{};
  std::string outputText;
};

SessionResult runSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;
  xfoil::app::cli::InteractiveStartupOptions options;
  SessionResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  return result;
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::size_t countOccurrences(const std::string& text,
                             const std::string& needle) {
  std::size_t count = 0;
  std::size_t position = text.find(needle);
  while (position != std::string::npos) {
    ++count;
    position = text.find(needle, position + needle.size());
  }
  return count;
}

std::string readText(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

}  // namespace

TEST(integration, InputCapacityS20InlineRealBeyondOldLimit) {
  const auto result = runSession(
      "NACA 2412\nOPER\nPACC\n\n\nASEQ 0" + std::string(130, ' ') +
      "1 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, " Point added to stored polar  1"),
           std::size_t{2});
}

TEST(integration, InputCapacityS20InlineIntegerBeyondOldLimit) {
  const auto result = runSession(
      "NACA 2412\nPPAR\nN" + std::string(130, ' ') + "160\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(contains(result.outputText,
                 "   Number of panel nodes       160\n"));
}

TEST(integration, InputCapacityS20InlineIgnoresCharacterBeyond256) {
  const auto result = runSession(
      "NACA 2412\nOPER\nPACC\n\n\nASEQ 0" + std::string(259, ' ') +
      "1 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, " Point added to stored polar  1"),
           std::size_t{0});
  CHECK(contains(result.outputText, "Enter last  alfa value (deg)"));
}

TEST(integration, InputCapacityS20PromptIntegerPath) {
  const auto result =
      runSession("NACA 2412\nOPER\nITER\n2\nITER\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText,
                            "Current iteration limit:          20"),
           std::size_t{1});
  CHECK(contains(result.outputText,
                 "Current iteration limit:           2"));
}

TEST(integration, InputCapacityS20PromptBlankKeepsDefault) {
  const auto result =
      runSession("NACA 2412\nOPER\nITER\n\nITER\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText,
                            "Current iteration limit:          20"),
           std::size_t{2});
}

TEST(integration, InputCapacityS20LongNameRegressionA32) {
  testing::ScopedTempDirectory work("s20-long-name", true);
  const std::string longName(200, 'A');
  const auto result = runSession(
      "NACA 2412\nNAME " + longName + "\nSAVE long-name.dat\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  const std::string saved = readText(work.makePath("long-name.dat"));
  CHECK(saved.rfind(std::string(48, 'A') + "\n", 0) == 0);
}

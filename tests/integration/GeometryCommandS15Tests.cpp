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

// File: tests/integration/GeometryCommandS15Tests.cpp
// End-to-end S15 command contract against the 6.996 oracle fixtures:
// NACA/NACS buffers and inviscid Cp files, GDES SQTE/CIRC/SYTB/XCEN,
// ADEG/A aliasing, EXEC commit and the excluded NSPL command.

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
  std::string errorText;
};

SessionResult runSession(const std::string& inputText) {
  SessionResult result;
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;
  xfoil::app::cli::InteractiveStartupOptions options;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
}

std::string readFileBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string fixture(const std::string& relative) {
  return readFileBytes(std::filesystem::path("tests/data/xfoil-6.996/s15") /
                       relative);
}

void checkFileEquals(const std::string& actualPath,
                     const std::string& expected) {
  const std::string actual = readFileBytes(actualPath);
  CHECK_EQ(actual, expected);
}

}  // namespace

TEST(integration, GeometryCommandS15NacaNacsOracleBuffersAndCp) {
  const char* const cases[] = {
      "6996-naca-0012-200",  "6996-nacs-0012-200",
      "6996-naca-2412-200",  "6996-nacs-2412-200",
      "6996-naca-23012-200", "6996-nacs-23012-200",
  };
  const char* const commands[] = {"NACA", "NACS", "NACA", "NACS", "NACA",
                                  "NACS"};
  const char* const designations[] = {"0012", "0012", "2412",
                                      "2412", "23012", "23012"};
  for (std::size_t i = 0; i < 6; ++i) {
    const std::string expectedBuffer =
        fixture(std::string("cases/") + cases[i] + "/buffer.dat");
    const std::string expectedCp =
        fixture(std::string("cases/") + cases[i] + "/cp.dat");
    testing::ScopedTempDirectory work("s15-naca-case", true);
    const auto result = runSession(
        std::string(commands[i]) + " " + designations[i] +
        "\nPCOP\nPSAV buffer.dat\nPANE\nOPER\nALFA 4\nCPWR cp.dat\nCPMN\n"
        "QUIT\n");
    CHECK_EQ(result.exitCode, 0);
    checkFileEquals("buffer.dat", expectedBuffer);
    checkFileEquals("cp.dat", expectedCp);
    CHECK(result.outputText.find("Buffer airfoil set using 399 points") !=
          std::string::npos);
    if (std::string(commands[i]) == "NACS") {
      CHECK(result.outputText.find("Sharp trailing edge") !=
            std::string::npos);
    } else {
      CHECK(result.outputText.find("Blunt trailing edge.  Gap =  0.00252") !=
            std::string::npos);
    }
  }
}

TEST(integration, GeometryCommandS15GdesOracleBuffers) {
  const std::string before = fixture("cases/6996-naca-2412-200/buffer.dat");
  struct Case {
    const char* name;
    const char* command;
  };
  const Case cases[] = {
      {"sqte", "SQTE"},
      {"circ", "CIRC"},
      {"circ", "CIRC 1"},  // inline argument intentionally ignored
      {"sytb", "SYTB 1.1 0.9"},
      {"adeg", "ADEG 5"},
      {"adeg", "A 5"},
      {"xcen", "XCEN 0.5 0.5"},
  };
  for (const Case& probe : cases) {
    const std::string expected =
        fixture(std::string("command-probes/") + probe.name + "/after.dat");
    testing::ScopedTempDirectory work("s15-gdes-case", true);
    const auto result = runSession(
        std::string("NACA 2412\nPCOP\nPSAV before.dat\nGDES\n") +
        probe.command + "\nEXEC\n\nPCOP\nPSAV after.dat\nQUIT\n");
    CHECK_EQ(result.exitCode, 0);
    checkFileEquals("before.dat", before);
    checkFileEquals("after.dat", expected);
  }
}

TEST(integration, GeometryCommandS15XcenTextAndNoMutation) {
  const std::string before =
      fixture("cases/6996-naca-2412-200/buffer.dat");
  const std::string after = fixture("command-probes/xcen/after.dat");
  testing::ScopedTempDirectory work("s15-xcen-text", true);
  const auto result = runSession(
      "NACA 2412\nPCOP\nPSAV before.dat\nGDES\nXCEN 0.5 0.5\nEXEC\n\n"
      "PCOP\nPSAV after.dat\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            " Top point  x,y =   0.500000       0.723847E-01") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " y/t point  x,y =   0.500000       0.194444E-01") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Bot point  x,y =   0.500000      -0.334958E-01") !=
        std::string::npos);
  checkFileEquals("before.dat", before);
  checkFileEquals("after.dat", after);
}

TEST(integration, GeometryCommandS15GdesPromptsAndExcludedNspl) {
  const std::string buffer =
      fixture("cases/6996-naca-2412-200/buffer.dat");
  testing::ScopedTempDirectory work("s15-gdes-prompts", true);
  const auto syb = runSession(
      "NACA 2412\nPCOP\nGDES\nSYTB 1.1\n0.9\nEXEC\n\nQUIT\n");
  CHECK_EQ(syb.exitCode, 0);
  CHECK(syb.outputText.find("Enter bottom y scale factor") !=
        std::string::npos);

  const auto xcen = runSession(
      "NACA 2412\nPCOP\nGDES\nXCEN\n0.5\n0.5\nEXEC\n\nQUIT\n");
  CHECK_EQ(xcen.exitCode, 0);
  CHECK(xcen.outputText.find("Enter center-point x/c:") !=
        std::string::npos);
  CHECK(xcen.outputText.find("Enter center-point y/t:") !=
        std::string::npos);

  const auto nspl = runSession(
      "NACA 2412\nPCOP\nPSAV before.dat\nGDES\nNSPL\nEXEC\n\n"
      "PCOP\nPSAV after.dat\nQUIT\n");
  CHECK_EQ(nspl.exitCode, 0);
  CHECK(nspl.outputText.find("Command NSPL not recognized") !=
        std::string::npos);
  checkFileEquals("before.dat", buffer);
  checkFileEquals("after.dat", buffer);
}

TEST(integration, GeometryCommandS15HiddenNacsAndGdesHelp) {
  const auto topHelp = runSession("?\nQUIT\n");
  CHECK_EQ(topHelp.exitCode, 0);
  CHECK(topHelp.outputText.find("NACA i  Set NACA 4,5-digit") !=
        std::string::npos);
  CHECK(topHelp.outputText.find("NACS") == std::string::npos);

  const auto gdesHelp = runSession("NACA 2412\nGDES\n?\nQUIT\n");
  CHECK_EQ(gdesHelp.exitCode, 0);
  CHECK(gdesHelp.outputText.find(
            "SYTB rr  Scale top and bottom y values separately") !=
        std::string::npos);
  CHECK(gdesHelp.outputText.find("SQTE     Make TE gap square") !=
        std::string::npos);
  CHECK(gdesHelp.outputText.find("CIRC i   Add TE circle") !=
        std::string::npos);
  CHECK(gdesHelp.outputText.find("NSPL") == std::string::npos);
}

// End: tests/integration/GeometryCommandS15Tests.cpp

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

// file: tests/integration/ConsoleTextS18Tests.cpp
// Focused S18 integration coverage for the productive console paths.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <sstream>
#include <string>

namespace {

struct TextSessionResult {
  int exitCode{};
  std::string outputText;
};

TextSessionResult runTextSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;
  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");
  TextSessionResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  return result;
}

}  // namespace

TEST(integration, ConsoleTextS18InviscidAlphaOmitsOfs) {
  const auto result = runTextSession("OPER\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
}

TEST(integration, ConsoleTextS18ViscousLoadsAppendClfAndCdp) {
  const auto result =
      runTextSession("OPER\nVISC 1e6\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find(
            "    CLf =  0.00004   CDp =  0.00075 CDp+CDf =  0.00489") !=
        std::string::npos);
}

TEST(integration, ConsoleTextS18QdesEntryOmitsOfs) {
  const auto result = runTextSession("NACA 2412\nQDES\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
}

TEST(integration, ConsoleTextS18OperHelpMatchesTextContract) {
  const auto result = runTextSession("OPER\n?\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            "   ASET i   Copy stored polar airfoil into current airfoil") !=
        std::string::npos);
  CHECK(result.outputText.find(
            "   FNEW rr  Set new flap hinge point\n"
            "\n"
            "   VELS rr  Calculate velocity components at a point") !=
        std::string::npos);
  CHECK(result.outputText.find(
            "   CPMN     Report minimum surface Cp\n"
            "\n"
            "   NAME s   Specify new airfoil name") != std::string::npos);
}

TEST(integration, ConsoleTextS18XtcamReportUsesWidenedFields) {
  const auto result =
      runTextSession("NACA 2412\nGDES\nTFAC 1.0 1.0\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            " Max thickness =   0.120035  at x =   0.2998") !=
        std::string::npos);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

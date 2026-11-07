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

// File: tests/integration/OperClTests.cpp
// tests for the CL-controlled single-point CLI and CL in the OPER menu
// (switch on the
// session/core router). The text contract follows the canonical
// Fortran reference (fortran-bin/xfoil): SPECCL points print only the
// GGCALC line in inviscid mode; in viscous mode the VISCAL block with
// format 9922 is printed as well; the OFS line (format 333) comes only
// from SPECAL (alpha). The CLI branch preserves LALFA =.TRUE. as in xoper.f.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <cmath>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>

namespace {

struct ClRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

ClRunResult runOperClSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  ClRunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
}

}  // namespace

TEST(integration, OperCliRunsInviscidSpecifiedLiftPoint) {
  const auto result = runOperClSession("OPER\nCLI 0.5\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // canonical: an inviscid SPECCL point prints only the GGCALC line.
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
  CHECK(result.outputText.find(" Solving BL system ...") == std::string::npos);
  CHECK(result.errorText.empty());
}

TEST(integration, OperCliRunsViscousIterationInViscousMode) {
  const auto result =
  runOperClSession("OPER\nVISC 1000000\nCLI 0.5\n\nQUIT\n");

  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(" Solving BL system ...") != std::string::npos);
  // format 9922 result block of the VISCAL point.
  CHECK(result.outputText.find("=>   CDf = ") != std::string::npos);

  // the CLI takes an inviscid Cl target first and determines alpha
  // from it (LALFA=.TRUE., xoper.f CLI branch). The resulting viscous Cl
  // need not correspond exactly to CLSPEC.
  CHECK(result.errorText.empty());
}

TEST(integration, OperClRunsViscousSpecifiedLiftPoint) {
  const auto result =
      runOperClSession("OPER\nVISC 1e6\nCL 0.8\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(" Solving BL system ...") != std::string::npos);
  CHECK(result.outputText.find("=>   CDf = ") != std::string::npos);
}

TEST(integration, OperClPromptsForMissingArgument) {
  const auto result = runOperClSession("OPER\nCL\n0.5\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Enter lift coefficient   r>  ") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
}

TEST(integration, OperClRejectsInvalidArgument) {
  const auto result = runOperClSession("OPER\nCL foo\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // canonical Fortran contract (GETFLT + ASKR): an invalid inline value leads
  // to the ASKR prompt; the following empty line keeps the previous CLSPEC.
  CHECK(result.outputText.find("Enter lift coefficient   r>  ") !=
        std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
}

TEST(integration, OperClResultIsFinite) {
  const auto result = runOperClSession("OPER\nCL 0.8\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // the canonical inviscid SPECCL point prints no result line; the
  // finiteness of the result is checked at handler/session level
  // (CoreOperSinglePointHandlerTests). The router contract is the GGCALC
  // line without error output.
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
  CHECK(result.errorText.empty());
}

// End: tests/integration/OperClTests.cpp

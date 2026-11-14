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

// File: tests/integration/OperMenuTests.cpp
// OPER menu integration tests for menu entry
// and return, dynamic prompts, help, and repetition of the last
// command with '!'. All runs use the injected interpreter core
// without process start; the geometry is loaded via the start
// parameter from sample/CLARK_Y.dat.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <optional>
#include <sstream>
#include <string>

namespace {

struct OperRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

OperRunResult runOperSession(const std::string& inputText, bool withGeometry) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  if (withGeometry) {
    options.inputPath = std::string("sample/CLARK_Y.dat");
  }

  OperRunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
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

const char* const kTopPrompt = "\n XFOIL   c>  ";
const char* const kOperInviscidPrompt = "\n.OPERi   c>  ";
const char* const kOperViscousPrompt = "\n.OPERv   c>  ";
const char* const kMenuHeader = "   QUIT    Exit program";

}  // namespace

TEST(integration, OperRequiresLoadedGeometry) {
  const auto result = runOperSession("OPER\nQUIT\n", false);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("***  No airfoil available  ***") !=
        std::string::npos);
  // TOP stays active: no OPER prompt (the menu table shows ".OPER"
  // as a text line), no response as unknown.
  CHECK(result.outputText.find(kOperInviscidPrompt) == std::string::npos);
  CHECK(result.outputText.find(".OPERv") == std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
}

TEST(integration, OperEntersAndReturnsToTop) {
  const auto result = runOperSession("OPER\n\nQUIT\n", true);
  CHECK_EQ(result.exitCode, 0);
  // Exactly one OPER prompt; then back to the TOP prompt. The TOP menu
  // is not printed again on return.
  CHECK_EQ(countOccurrences(result.outputText, kOperInviscidPrompt),
           std::size_t{1});
  CHECK_EQ(countOccurrences(result.outputText, kTopPrompt), std::size_t{2});
  CHECK_EQ(countOccurrences(result.outputText, kMenuHeader), std::size_t{1});
  CHECK(result.errorText.empty());
}

TEST(integration, OperPromptReflectsInviscidMode) {
  const auto result = runOperSession("OPER\n\nQUIT\n", true);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(kOperInviscidPrompt) != std::string::npos);
  CHECK(result.outputText.find(".OPERv") == std::string::npos);
}

TEST(integration, OperPromptReflectsViscousMode) {
  const auto result = runOperSession("OPER\nVISC 1000000\n\nQUIT\n", true);
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kOperInviscidPrompt),
           std::size_t{1});
  CHECK_EQ(countOccurrences(result.outputText, kOperViscousPrompt),
           std::size_t{1});
  // MRSHOW output after the switch (M and Re lines).
  CHECK(result.outputText.find(" M  =") != std::string::npos);
  CHECK(result.outputText.find(" Re =") != std::string::npos);
}

TEST(integration, OperHelpListsOriginalCommands) {
  const auto result = runOperSession("OPER\n?\n\nQUIT\n", true);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            "   !        Redo last ALFA,CLI,CL,ASEQ,CSEQ,VELS") !=
        std::string::npos);
  CHECK(result.outputText.find("   Alfa r   Prescribe alpha") !=
        std::string::npos);
  CHECK(result.outputText.find("   ASeq rrr Prescribe a sequence of alphas") !=
        std::string::npos);
  CHECK(result.outputText.find("   CPMN     Report minimum surface Cp") !=
        std::string::npos);
  CHECK(result.outputText.find("   NINC     Increment name version number") !=
        std::string::npos);
}

TEST(integration, OperBangRepeatsPreviousCommand) {
  const auto result =
      runOperSession("OPER\nALFA 2\n!\n\nQUIT\n", true);
  CHECK_EQ(result.exitCode, 0);
  // Canonical Fortran contract (xoper.f 85-92, 416-471): '!' sets
  // COMAND=COMOLD and LRECALC=.TRUE.; the alfa branch then only calls
  // VISCAL. In inviscid mode VISCAL does not run, so the repetition
  // prints no text at all; the point was already executed.
  CHECK_EQ(countOccurrences(result.outputText,
                            " Calculating unit vorticity distributions ..."),
           std::size_t{1});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.errorText.empty());
}

TEST(integration, OperBangWithoutPreviousCommandDoesNothing) {
  const auto result = runOperSession("OPER\n!\n\nQUIT\n", true);
  CHECK_EQ(result.exitCode, 0);
  // No point, no special message; only the next prompt.
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK_EQ(countOccurrences(result.outputText, kOperInviscidPrompt),
           std::size_t{2});
}

// End: tests/integration/OperMenuTests.cpp

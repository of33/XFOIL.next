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

// File: tests/integration/OperPlotBlockedTests.cpp
// Table-driven tests of the OPER plot commands (
//) and of the blocked OPER commands. Each
// known command is checked at least once; both classes remain
// distinguishable and change no numerical state.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

struct PlotBlockedRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

PlotBlockedRunResult runOperCommandsSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  PlotBlockedRunResult result;
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

// OPER plot commands per pass .-P3a:
// canonical ASKC-aliases (xoper.f 1181-1182, 1224-1225, 1472-1473,
// 1477-1478, 1720-1721) added.
const std::string_view kOperPlotCommands[] = {
    "Z",    "U",    "HARD", "SIZE", "CPMI", "UEMA", "UEMI", "SEQP",
    "PPLO", "APLO", "PPAX", "CREF", "CPX",  "UEX",  "GRID", "CPIO",
    "CPV",  "CPVI", "BL",   "BLC",  "BLWT", "ANNO", "VPLO", "FREF",
    "PP",   "AP",   "CP",   "UE",   "VP"};

std::string buildSession(const std::string_view* commands,
                         std::size_t count) {
  std::string input = "OPER\n";
  for (std::size_t i = 0; i < count; ++i) {
    input += std::string(commands[i]) + "\n";
  }
  input += "\nQUIT\n";
  return input;
}

const char* const kPlotUnavailable =
    "Plotting is not available in this build.";
const char* const kOperInviscidPrompt = "\n.OPERi   c>  ";

}  // namespace

TEST(integration, OperPlotCommandsAllReportPlotUnavailable) {
  const auto result = runOperCommandsSession(
      buildSession(kOperPlotCommands,
                   sizeof kOperPlotCommands / sizeof kOperPlotCommands[0]));
  CHECK_EQ(result.exitCode, 0);

  const std::size_t commandCount =
      sizeof kOperPlotCommands / sizeof kOperPlotCommands[0];
  // Each command reports the plot unavailability exactly.
  CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
           commandCount);
  // None of these lines counts as unknown; no analysis, no
  // extra prompt switch via the command sequence.
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK_EQ(countOccurrences(result.outputText, kOperInviscidPrompt),
           commandCount + 1);
}

// ASET/BLF/DAMP reach their handlers and never
// report the blocked message.
TEST(integration, OperPlotAliasCommandsAreProductive) {
  const auto result = runOperCommandsSession(
      "NACA 0012\nPANE\nOPER\nASET\nBLF\nDAMP\nDAMP\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  // ASET without polar slots: canonical Fortran message (xoper.f
  // 1253-1256).
  CHECK(result.outputText.find("No polar airfoils are stored") !=
        std::string::npos);
 // BLF without BL data: the released guard text.
  CHECK(result.outputText.find(" Boundary-layer data are not available.") !=
        std::string::npos);
  // DAMP toggles in both directions (xoper.f 1744-1751).
  CHECK(result.outputText.find("Modified amplification used") !=
        std::string::npos);
  CHECK(result.outputText.find("Original amplification used") !=
        std::string::npos);
}

// ASET/BLF/DAMP are productively reachable; prompt and
// report texts appear, the menu stays synchronous, and the
// sequence commands process the state.
TEST(integration, OperAliasHandlersAreProductive) {
  const auto result = runOperCommandsSession(
      "NACA 0012\nPANE\nOPER\nVISC 1e6\nALFA 2\nASET\nBLF\nDAMP\nALFA 2\n"
      "\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("No polar airfoils are stored") !=
        std::string::npos);
  CHECK(result.outputText.find("xBL, Fint") != std::string::npos);
  CHECK(result.outputText.find("Modified amplification") !=
        std::string::npos);
  // the following alfa point runs unchanged.
  CHECK(result.outputText.find("OFS:") != std::string::npos);
}

// FMOM/FNEW/VELS produce canonical reports/prompts;
// the menu stays synchronous.
TEST(integration, OperFlapVelocityCommandsAreProductive) {
  const auto result = runOperCommandsSession(
      "NACA 0012\nPANE\nOPER\nALFA 2\nFNEW 0.7 -0.05\nFMOM\n"
      "VELS 0.5 0.1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.outputText.find("Flap hinge x,y :  0.7000 -0.0500") !=
        std::string::npos);
  CHECK(result.outputText.find(" Hinge moment/span = 0.002416") !=
        std::string::npos);
  CHECK(result.outputText.find(" u/Uinf =   1.1284") != std::string::npos);
  // Inline values consume no prompts.
  CHECK(result.outputText.find("Enter flap hinge") == std::string::npos);
  CHECK(result.outputText.find("Enter x") == std::string::npos);
}

// the earlier unreachability pin has been replaced by the
// productive contract (OperAliasHandlersAreProductive); the
// canonical prompt texts are now reachable.
TEST(integration, OperAliasPromptPathsAreProductive) {
  // ASET with a polar slot (PACC accumulation returns the slot):
  // single-slot path without an index prompt, then the confirmation prompt.
  const auto result = runOperCommandsSession(
      "NACA 0012\nPANE\nOPER\nPACC\n\n\nALFA 2\nP\nASET\nY\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("Current airfoil will be overwritten.  "
                               "Proceed?  Y") != std::string::npos);
}

// the never-started OPER commands FMOM/FNEW/VELS
// remain recognized, but unexecuted (kOperNotStartedCommands).
// the state flags (IDAMP, CPI) are session-side only
// and do not reach the productive CLI path; no mutation, no
// prompt, no analysis.
//FMOM with a defined flap runs canonically via the
// GETXYF path (prompts + upper/lower surface text); VELS redo via
// '!' repeats with ARGOLD (xoper.f 1662-1663).
TEST(integration, OperFlapGetxyfAndVelsRedoAreProductive) {
  const auto result = runOperCommandsSession(
      "NACA 0012\nPANE\nOPER\nALFA 2\nFNEW\nFMOM\n0.7\n-0.05\n"
      "VELS 0.5 0.1\n!\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("Enter flap hinge x location") !=
        std::string::npos);
  CHECK(result.outputText.find("Top    surface") != std::string::npos);
  CHECK(result.outputText.find("Flap hinge x,y :  0.7000 -0.0500") !=
        std::string::npos);
  // VELS and the redo produce identical reports.
  CHECK_EQ(countOccurrences(result.outputText, " u/Uinf =   1.1284"),
           std::size_t{2});
}

// End: tests/integration/OperPlotBlockedTests.cpp

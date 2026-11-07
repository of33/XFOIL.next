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

// File: tests/integration/OperConfigTests.cpp
// tests for the OPER configuration commands VISC, Re, Mach, TYPE, ITER
// and INIT. All runs use the injected
// interpreter core; the geometry comes from sample/CLARK_Y.dat.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <optional>
#include <sstream>
#include <string>

namespace {

struct ConfigRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

ConfigRunResult runOperConfigSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  ConfigRunResult result;
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

const char* const kOperInviscidPrompt = "\n.OPERi   c>  ";
const char* const kOperViscousPrompt = "\n.OPERv   c>  ";

}  // namespace

TEST(integration, OperViscPromptsForReynoldsWhenMissing) {
  const auto result = runOperConfigSession("OPER\nVISC\n1e5\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Enter Reynolds number   r>  ") !=
        std::string::npos);
  CHECK(result.outputText.find(kOperViscousPrompt) != std::string::npos);
  // MRSHOW after the switch; G12.4 of 1e5 in decimal format.
  CHECK(result.outputText.find(" M  =    0.0000") != std::string::npos);
  CHECK(result.outputText.find(" Re =  0.1000E+06") != std::string::npos);
}

TEST(integration, OperViscStoresNegativeInlineReynoldsLikeFortran) {
  const auto result = runOperConfigSession("OPER\nVISC -5\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // Canonical Fortran contract (xoper.f VISC branch, oper-Re-negative):
  // Fortran does not validate the Reynolds number; -5 is stored, the
  // mode switches to the viscous state and MR shows Re = -5.000.
  CHECK(result.outputText.find("Re =  -5.000") != std::string::npos);
  CHECK(result.outputText.find(".OPERv") != std::string::npos);
  CHECK_EQ(countOccurrences(result.outputText, kOperViscousPrompt),
           std::size_t{1});
}

TEST(integration, OperViscTogglesBackToInviscid) {
  const auto result =
      runOperConfigSession("OPER\nVISC 100000\nVISC\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // Returning to inviscid mode prints no message.
  CHECK_EQ(countOccurrences(result.outputText, kOperViscousPrompt),
           std::size_t{1});
  CHECK_EQ(countOccurrences(result.outputText, kOperInviscidPrompt),
           std::size_t{2});
  CHECK_EQ(countOccurrences(result.outputText, " M  ="), std::size_t{1});
}

TEST(integration, OperReSetsValueWithoutRunningAnalysis) {
  const auto result = runOperConfigSession("OPER\nRE 200000\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // Re with an inline value still sets and starts no analysis.
  CHECK(result.outputText.find("alpha :") == std::string::npos);
  CHECK(result.outputText.find("Currently...") == std::string::npos);
}

TEST(integration, OperRePromptsWithCurrentlyWhenMissing) {
  const auto result = runOperConfigSession("OPER\nRE\n3e5\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(" Currently...") != std::string::npos);
  CHECK(result.outputText.find("Enter new Reynolds number   r>  ") !=
        std::string::npos);
  CHECK(result.outputText.find("alpha :") == std::string::npos);
}

TEST(integration, OperMachRejectsSupersonicAndAcceptsFollowUp) {
  const auto result =
      runOperConfigSession("OPER\nMACH 1.5\n0.3\nTYPE 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(" Supersonic freestream not allowed") !=
        std::string::npos);
  CHECK(result.outputText.find("Enter Mach number   r>  ") !=
        std::string::npos);
  // the adopted Mach appears only in the next MRSHOW (TYPE 1), as in
  // the original; the rejected value is replaced by the old one.
  CHECK(result.outputText.find(" M  =    0.3000") != std::string::npos);
}

TEST(integration, OperTypeSetsDependencySuffixes) {
  const auto result = runOperConfigSession("OPER\nTYPE 2\nTYPE 3\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // type 2 uses both quantities ' / sqrt(Cl)', type 3 uses Re ' / Cl'.
  CHECK_EQ(countOccurrences(result.outputText, " / sqrt(CL)"),
           std::size_t{2});
  CHECK(result.outputText.find(" / CL") != std::string::npos);
  CHECK(result.outputText.find("alpha :") == std::string::npos);
}

TEST(integration, OperTypePromptsWithTableWhenMissing) {
  const auto result = runOperConfigSession("OPER\nTYPE\n2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("parameters held constant") !=
        std::string::npos);
  CHECK(result.outputText.find(
            "Enter type of Mach,Re variation with CL   i>  ") !=
        std::string::npos);
}

TEST(integration, OperIterKeepsOldLimitOnInvalidInlineValue) {
  const auto result =
      runOperConfigSession("OPER\nITER 30\nITER 0\nITER\n5\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // Canonical Fortran contract (xoper.f 379-391): ITER sets ITMAX first
  // and checks afterwards; ITMAX<1 with NINPUT=0 returns to the loop head,
 // prints the (now 0) limit value list-directed and queries ASKI again
  // the answer 5 at the repeated prompt takes over 5.
  CHECK(result.outputText.find(" Current iteration limit:           0") !=
        std::string::npos);
  CHECK(result.outputText.find("Enter new iteration limit   i>  ") !=
        std::string::npos);
}

TEST(integration, OperInitTogglesBoundaryLayerMessages) {
  const auto result = runOperConfigSession("OPER\nINIT\nINIT\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText,
                            " BLs are assumed to be initialized"),
           std::size_t{1});
  CHECK_EQ(countOccurrences(result.outputText,
                            " BLs will be initialized on next point"),
           std::size_t{1});
}

// End: tests/integration/OperConfigTests.cpp

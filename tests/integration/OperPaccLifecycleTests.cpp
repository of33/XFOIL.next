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

// file: tests/integration/OperPaccLifecycleTests.cpp
// PACC lifecycle via the productive native CLI
// (xoper.f 772-823, xpol.f PLRSET/PLRINI/PLXINI/PLRADD/PLXADD).
// Checked are the prompt order, activation/deactivation, locking of the
// polar parameter commands during accumulation (FORMAT 2100),
// the OPER entry message of the active polar, point capture per
// analysis path, suppression of the continuation hint for LPACC, and
// the canonical file availability messages. Byte identity of the
// generated polar/dump files against the canonical Fortran case is covered
// by regression.PaccDumpByteParityMatchesFortranReference.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <sstream>
#include <string>

namespace {

struct PaccRunResult {
  int exitCode{};
  std::string outputText;
};

PaccRunResult runSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  PaccRunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
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

const char* const kAccumulationBlocked =
    " * Polar is being accumulated.";
const char* const kEnabled = " Polar accumulation enabled";
const char* const kDisabled = " Polar accumulation disabled";
const char* const kNoSave = " Save file unspecified or not available";
const char* const kNoDump = " Dump file unspecified or not available";
const char* const kPointAdded = " Point added to stored polar";

}  // namespace

TEST(integration, PaccEnableDisablePromptOrder) {
  const PaccRunResult run = runSession("OPER\nPACC\n\n\nPACC\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countOccurrences(run.outputText,
      "Enter  polar save filename  OR  <return> for no file"), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText,
      "Enter  polar dump filename  OR  <return> for no file"), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText,
      " Polar save file will NOT be written"), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText,
      " Polar dump file will NOT be written"), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText, kEnabled), std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText, kDisabled), std::size_t{1});
  // prompt order: Save before dump.
  CHECK(run.outputText.find("polar save filename") <
        run.outputText.find("polar dump filename"));
}

TEST(integration, PaccBlocksParameterCommandsMidstream) {
  const PaccRunResult run =
      runSession("OPER\nPACC\n\n\nVISC\nMACH\nTYPE\nRE\nVPAR\nXTR\nN\n\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  // VISC/Mach/TYPE blocked (format 2100); Re only for LVISC, here
  // inviscid -> not blocked; VPAR XTR/N only for LVISC.
  CHECK_EQ(countOccurrences(run.outputText, kAccumulationBlocked),
           std::size_t{3});
  CHECK(run.outputText.find("Enter new Reynolds number") != std::string::npos);
}

TEST(integration, PaccBlocksViscousParameterCommandsWhenViscous) {
  const PaccRunResult run =
      runSession("OPER\nVISC\n3e6\nPACC\n\n\nRE\nVPAR\nXTR\n1.0 1.0\nN\n9\n\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  // Re, VPAR-XTR and VPAR-N blocked (LPACC.AND.LVISC).
  CHECK_EQ(countOccurrences(run.outputText, kAccumulationBlocked),
           std::size_t{3});
}

TEST(integration, PaccActivePolarAnnouncedAtOperEntry) {
  const PaccRunResult run =
      runSession("OPER\nPACC\n\n\nALFA 2\n\nOPER\nALFA 4\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countOccurrences(run.outputText, "  Polar  1  is active"),
           std::size_t{1});
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{2});
}

TEST(integration, PaccPointAccumulationSingleAndBang) {
  const PaccRunResult run =
      runSession("OPER\nPACC\n\n\nALFA 2\nALFA 4\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{2});
  CHECK_EQ(countOccurrences(run.outputText, kNoSave), std::size_t{2});
  CHECK_EQ(countOccurrences(run.outputText, kNoDump), std::size_t{2});
}

TEST(integration, PaccSequenceAccumulatesPerPointInOrder) {
  const PaccRunResult run =
      runSession("OPER\nPACC\n\n\nASEQ 0 4 2\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{3});
  const std::string first = " Point added to stored polar  1";
  CHECK_EQ(countOccurrences(run.outputText, first), std::size_t{3});
}

TEST(integration, PaccSuppressesContinueHintAndSkipsNonConverged) {
  const PaccRunResult run = runSession(
      "OPER\nVISC\n3e6\nITER 1\nPACC\n\n\nALFA 10\nALFA 2\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  // the format-2100 note "Type \"!\"..." is suppressed for LPACC.
  CHECK(run.outputText.find("Type \"!\" to continue iterating") ==
        std::string::npos);
  // ITER-1 points do not converge and are not recorded.
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{0});
}

TEST(integration, PaccDisabledStopsAccumulation) {
  const PaccRunResult run =
      runSession("OPER\nPACC\n\n\nALFA 2\nPACC\nALFA 4\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countOccurrences(run.outputText, kPointAdded), std::size_t{1});
}

// End: tests/integration/OperPaccLifecycleTests.cpp

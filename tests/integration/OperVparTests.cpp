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

// File: tests/integration/OperVparTests.cpp
// tests for the VPAR sub-menu: entry
// and return, SHOW, XTR, N, VACC including the static
// VACCEL safeguard, INIT, REST, and the
// commands afterwards.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"
#include "app/cli/CommandInterpreter.h"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

struct VparRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

VparRunResult runOperVparSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  VparRunResult result;
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

const char* const kVparPrompt = "\n..VPAR   c>  ";
const char* const kOperInviscidPrompt = "\n.OPERi   c>  ";

}  // namespace

TEST(integration, VparEntersAndReturnsToOper) {
  const auto result = runOperVparSession("OPER\nVPAR\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kVparPrompt), std::size_t{1});
  // empty input returns to the OPER menu; from there another empty
  // input returns to TOP.
  CHECK_EQ(countOccurrences(result.outputText, kOperInviscidPrompt),
           std::size_t{2});
}

TEST(integration, InterpreterAppliesDefaultVaccelAndVaccUpdatesInstance) {
  // VACCEL is instance-specific (session composition); the proof runs
  // via the canonical VPAR display (format 1200, Vacc F8.4).
  const auto first = runOperVparSession("OPER\nVPAR\n\n\nQUIT\n");
  CHECK_EQ(first.exitCode, 0);
  // entry display before the first prompt: original default 0.0100.
  CHECK(first.outputText.find("Vacc      =  0.0100") != std::string::npos);

  const auto second =
      runOperVparSession("OPER\nVPAR\nVACC 0.37\nSHOW\n\n\nQUIT\n");
  CHECK_EQ(second.exitCode, 0);
  CHECK(second.outputText.find("Vacc      =  0.3700") != std::string::npos);
}

TEST(integration, VparXtrAcceptsInlineValues) {
      const auto result = runOperVparSession(
            "OPER\nVPAR\nXTR 0.5 0.6\n\n\nQUIT\n");

      CHECK_EQ(result.exitCode, 0);
      CHECK(result.errorText.empty());
      CHECK(
            result.outputText.find(
                  "Command is not available in this build yet: XTR") ==
                  std::string::npos);
      CHECK(
            result.outputText.find(
                  " XTR  command not recognized") ==
                  std::string::npos);
}

TEST(integration, VparXtrPromptsForBothValuesWhenMissing) {
      const auto result = runOperVparSession(
            "OPER\nVPAR\nXTR\n0.4\n0.7\n\n\nQUIT\n");

      CHECK_EQ(result.exitCode, 0);
      CHECK(
            result.outputText.find(
                  "Enter top    side Xtrip/c   r>  ") !=
                  std::string::npos);
      CHECK(
            result.outputText.find(
                  "Enter bottom side Xtrip/c   r>  ") !=
                  std::string::npos);
      CHECK(result.errorText.empty());
}

TEST(integration, VparNAcceptsCommonNCritical) {
      const auto result = runOperVparSession(
            "OPER\nVPAR\nN 12\n\n\nQUIT\n");

      CHECK_EQ(result.exitCode, 0);
      CHECK(result.errorText.empty());
      CHECK(
            result.outputText.find(
                  "Command is not available in this build yet: N") ==
                  std::string::npos);
      CHECK(
            result.outputText.find(
                  " N    command not recognized") ==
                  std::string::npos);
}

TEST(integration, VparVaccCommandAppliesInstanceValue) {
      // instance-specific contract: VACC acts on the running session
      // (format-1200 display), not on global state.
      const auto result = runOperVparSession(
            "OPER\nVPAR\nVACC 0.05\nSHOW\n\n\nQUIT\n");

      CHECK_EQ(result.exitCode, 0);
      CHECK(result.errorText.empty());
      CHECK(result.outputText.find("Vacc      =  0.0500") !=
            std::string::npos);
}

TEST(integration, VparVaccStoresNegativeValueLikeFortran) {
      // canonical Fortran contract (xoper.f VPAR VACC):
      // Fortran does not validate; the value is stored and shown in the
      // format-1200 display.
      const auto result = runOperVparSession(
            "OPER\nVPAR\nVACC -1\nSHOW\n\n\nQUIT\n");

      CHECK_EQ(result.exitCode, 0);
      CHECK(result.outputText.find("Vacc      = -1.0000") !=
            std::string::npos);
}

TEST(integration, VparInitTogglesBoundaryLayerInitialization) {
      const auto result = runOperVparSession(
            "OPER\nVPAR\nINIT\nINIT\n\n\nQUIT\n");

      CHECK_EQ(result.exitCode, 0);
      CHECK_EQ(
            countOccurrences(
                  result.outputText,
                  " BLs are assumed to be initialized"),
               std::size_t{1});
      CHECK_EQ(
            countOccurrences(
                  result.outputText,
                  " BLs will be initialized on next point"),
               std::size_t{1});
}

// None of the nine commands still runs through a
// blocked path; all are recognized and reached (no format 1000).
TEST(integration, VparCommandsAreProductive) {
  const auto result = runOperVparSession(
      "OPER\nVPAR\nNT 8.5\nNB 7.25\nWAKE 2.0\nW 0.5\n"
      "LAG 6.0 0.5 1.5\nGB 7.0 0.8\nCTR 2.0 4.0\nCFAC 2.0\nREST\n"
      "\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
}

TEST(integration, VparUnknownCommandIsDiagnosed) {
  const auto result = runOperVparSession("OPER\nVPAR\nFOOZ\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            " FOOZ command not recognized.  Type a \"?\" for list") !=
        std::string::npos);
}

TEST(integration, VparHelpListsOriginalCommands) {
  const auto result = runOperVparSession("OPER\nVPAR\n?\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("   <cr>    Return to OPER menu") !=
        std::string::npos);
  CHECK(result.outputText.find(
            "   N    r  Change critical amplification exponent Ncrit") !=
        std::string::npos);
  CHECK(result.outputText.find("   REST    restore BL calibration to baseline") !=
        std::string::npos);
}

// NT/NB are productive. Inline sets NT only ACRIT(1), NB only
// ACRIT(2); SHOW shows both separately; without an argument the
// canonical prompt appears; no blocked text.
TEST(integration, VparNtNbProductiveContract) {
  const auto inlined = runOperVparSession(
      "OPER\nVPAR\nNT 8.5\nNB 7.25\nSHOW\n\n\nQUIT\n");
  CHECK_EQ(inlined.exitCode, 0);
  CHECK(inlined.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(inlined.outputText.find("NcritT    =    8.50") !=
        std::string::npos);
  CHECK(inlined.outputText.find("NcritB    =    7.25") !=
        std::string::npos);
  CHECK(inlined.outputText.find("Enter top-surface") ==
        std::string::npos);
  const auto prompted = runOperVparSession(
      "OPER\nVPAR\nNT\n8.5\nNB\n7.25\nSHOW\n\n\nQUIT\n");
  CHECK(prompted.outputText.find("Enter top-surface") !=
        std::string::npos);
  CHECK(prompted.outputText.find("Enter bot-surface") !=
        std::string::npos);
  CHECK(prompted.outputText.find("NcritT    =    8.50") !=
        std::string::npos);
  CHECK(prompted.outputText.find("NcritB    =    7.25") !=
        std::string::npos);
}

// WAKE/W are productive and alias-equal; SHOW shows the
// last set WAKLEN; without an argument the prompt appears.
TEST(integration, VparWakeWProductiveContract) {
  const auto inlined = runOperVparSession(
      "OPER\nVPAR\nWAKE 2.0\nW 0.5\nSHOW\n\n\nQUIT\n");
  CHECK_EQ(inlined.exitCode, 0);
  CHECK(inlined.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(inlined.outputText.find("WakeL/c   =   0.500") !=
        std::string::npos);
  CHECK(inlined.outputText.find("Enter wake length/chord") ==
        std::string::npos);
  const auto prompted =
      runOperVparSession("OPER\nVPAR\nWAKE\n2.0\nSHOW\n\n\nQUIT\n");
  CHECK(prompted.outputText.find("Enter wake length/chord") !=
        std::string::npos);
  CHECK(prompted.outputText.find("WakeL/c   =   2.000") !=
        std::string::npos);
}

// Wake growth after a viscous point. NACA 0012,
// VISC 1e6, ALFA 4 (default wake WAKLEN=1.0), VPAR WAKE 2.0, ALFA 6:
// Fortran converged in 5 iterations (rms 0.3146E-04, Cd 0.00973);
// the QVIS capacity grows with the wake, so the run converges
// like Fortran.
TEST(integration, VparWakeGrowthAfterViscousPointConvergesLikeFortran) {
  const auto result = runOperVparSession(
      "NACA 0012\nPANE\nOPER\nVISC 1000000\nALFA 4\n"
      "VPAR\nWAKE 2.0\n\nALFA 6\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText,
                             " VISCAL:  Convergence failed"),
           std::size_t{0});
  CHECK(result.outputText.find(
            "   5   rms: 0.3146E-04   max: -.5670E-03   D at   32  1") !=
        std::string::npos);
  CHECK(result.outputText.find(
            "      Cm = -0.0043     CD =  0.00973   =>   CDf =  0.00578"
            "    CDp =  0.00395") != std::string::npos);
}

// Control case: wake shrinkage (WAKLEN 1.0 -> 0.5) after
// a viscous point also works (no throw, convergence).
TEST(integration, VparWakeShrinkAfterViscousPointConverges) {
  const auto result = runOperVparSession(
      "NACA 0012\nPANE\nOPER\nVISC 1000000\nALFA 4\n"
      "VPAR\nWAKE 0.5\n\nALFA 6\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText,
                             " VISCAL:  Convergence failed"),
           std::size_t{0});
}

// LAG/GB/CTR/CFAC are productive; the inline paths mutate
// the SHOW state canonically (KCt = CTCON from GB); '!' stays unknown.
TEST(integration, VparLagGbCtrCfacProductiveContract) {
  const auto result = runOperVparSession(
      "OPER\nVPAR\nLAG 6.0 0.5 1.5\nGB 7.0 0.8\nCTR 2.0 4.0\nCFAC 2.0\n"
      "SHOW\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("Klag  =  6.0000") != std::string::npos);
  CHECK(result.outputText.find("Uxwt  =    0.50") != std::string::npos);
  CHECK(result.outputText.find("Kdl =  1.5000") != std::string::npos);
  CHECK(result.outputText.find("A     =  7.0000") != std::string::npos);
  CHECK(result.outputText.find("B     =  0.8000") != std::string::npos);
  CHECK(result.outputText.find("KCt = 0.01276") != std::string::npos);
  CHECK(result.outputText.find("CtiniK=  2.0000") != std::string::npos);
  CHECK(result.outputText.find("CtiniX=  4.0000") != std::string::npos);
  // VPAR has no redo contract: '!' stays an unknown command
  // (format 1000: 1X,A4 — A4 pads '!' to four characters).
  const auto redo = runOperVparSession("OPER\nVPAR\n!\n\n\nQUIT\n");
  CHECK(redo.outputText.find(
            "!    command not recognized.  Type a \"?\" for list") !=
        std::string::npos);
}

//REST is productive. LAG mutates visibly, and REST restores the
// BLPINI defaults; no blocked message, no guard text.
TEST(integration, VparRestProductiveContract) {
  const auto result = runOperVparSession(
      "OPER\nVPAR\nLAG 6.0 0.5 1.5\nSHOW\nREST\nSHOW\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  CHECK(result.outputText.find("Polar is being accumulated") ==
        std::string::npos);
  CHECK(result.outputText.find("Klag  =  6.0000") != std::string::npos);
  CHECK(result.outputText.find("Klag  =  5.6000") != std::string::npos);
  // REST ignores extra arguments as in Fortran (the oracle
  // rest_args/rest_extrachars): 'REST 1.0' and 'RESTX' reset.
  const auto args = runOperVparSession(
      "OPER\nVPAR\nLAG 6.0 0.5 1.5\nREST 1.0\nRESTX\nSHOW\n\n\nQUIT\n");
  CHECK(args.outputText.find("not recognized") == std::string::npos);
  CHECK(args.outputText.find("Klag  =  5.6000") != std::string::npos);
  CHECK(args.outputText.find("Klag  =  6.0000") == std::string::npos);
}

//guard matrix via the productive router. LAG is blocked for
// LPACC.AND.LVISC (format 2100, no mutation); CFAC stays
// unguarded and mutates even during polar accumulation.
TEST(integration, VparGuardMatrixProductiveContract) {
  const auto guarded = runOperVparSession(
      "OPER\nVISC 1000000\nALFA 2\nPACC\n\n\nVPAR\nLAG 6.0 0.5 1.5\n"
      "CFAC 2.0\nSHOW\n\n\nQUIT\n");
  CHECK_EQ(guarded.exitCode, 0);
  CHECK(guarded.outputText.find(
            " * Polar is being accumulated.\n"
            " * Cannot change its parameters in midstream.\n") !=
        std::string::npos);
  CHECK(guarded.outputText.find("Klag  =  5.6000") != std::string::npos);
  CHECK(guarded.outputText.find("Klag  =  6.0000") == std::string::npos);
}

// End: tests/integration/OperVparTests.cpp

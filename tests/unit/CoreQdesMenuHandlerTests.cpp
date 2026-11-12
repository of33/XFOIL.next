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

// File: tests/unit/CoreQdesMenuHandlerTests.cpp
// unit tests of the QDES menu
// handlers handleQdesCore (xqdes.f 40-540). The text anchors derive
// from canonical Fortran transcripts (LC_ALL=C fortran-bin/xfoil <
// input, three times byte-identical). SPECAL-specific diagnostic text
// (OFS line, 'Calculating unit vorticity distributions ...') is
// core-internal and not part of the handlers. The test drives the
// QDES menu handler directly, and there is no
// plot backend.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreInverseDesignHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <sstream>
#include <string>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

using session::SessionComposition;

void setupPangenAirfoil(SessionComposition& s, int designation) {
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(s);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(s, params).ok);
  std::ostringstream out;
  testing::handleLegacyNacaCore(s, out, designation);
  CHECK(s.paneling.panelingValid);
}

void configureOper(SessionComposition& s) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(s, config).success());
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::size_t countOccurrences(const std::string& text,
                             const std::string& needle) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

const char* const kPrompt = "\n.QDES   c>  ";

// FORMAT 1150 + 1155 (entry NACA 2412, ALFA=0, Qspec rebuild).
// Extended: the GGCALC header (xpanel.f 1000, cache invalid) and
// OFS format 333 (xoper.f 2816) from the executed SPECAL run precede
// format 1150; oracle f_k1 (LC_ALL=C, 3x deterministic).
const char* const kEntryText =
    " Calculating unit vorticity distributions ...\n"
    "\n Current Q operating condition:"
    "\n alpha =    0.000 deg.      CL =   0.2554\n"
    "\n"
    "\n Qspec initialized to current Q.\n"
    "\n";

// format 1200 after the first SMOO (full range, canonical oracle).
const char* const kSmooText1 =
    "\n Q    :   CL =   0.255377    CM =  -0.055736"
    "\n Qspec:   CL =   0.255630    CM =  -0.055642\n";

// format 1200 after the second SMOO ('!' redo oracle).
const char* const kSmooText2 =
    "\n Q    :   CL =   0.255377    CM =  -0.055736"
    "\n Qspec:   CL =   0.255836    CM =  -0.055559\n";

}  // namespace

// entry without contour (xqdes.f 52-57): canonical message, no menu.
TEST(unit, QdesMenuNoAirfoilText) {
  SessionComposition s;
  std::istringstream in("\n");
  std::ostringstream out;
  const auto status = cli::handleQdesCore(s, in, out);
  CHECK(status == cli::QdesCoreStatus::returned);
  CHECK(out.str() == " \n ***  No airfoil available  ***\n");
}

// entry + <Return>: Format 1150/1155, Prompt, Exit without text.
TEST(unit, QdesMenuEntryAndReturnText) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("\n");
  std::ostringstream out;
  const auto status = cli::handleQdesCore(s, in, out);
  CHECK(status == cli::QdesCoreStatus::returned);
  CHECK(out.str() == std::string(kEntryText) + kPrompt);
  // LQSYM remains false after leaving the menu (xqdes.f 130-137).
  CHECK(!s.inverse.flags.lqSym);
}

// Help format 1050 byte-exact against the Transcript.
TEST(unit, QdesMenuHelpBytes) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("?\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text, "\n   <cr>   Return to Top Level"
                      "\n\n   QSET   Reset Qspec <== Q"
                      "\n\n   Modi   Modify Qspec"
                      "\n   MARK   Mark off target segment"
                      "\n   SMOO   Smooth Qspec inside target segment"
                      "\n   SLOP   Toggle modified-Qspec slope matching flag"
                      "\n\n   eXec i Execute mixed-inverse calculation"
                      "\n   REST   Restore geometry from buffer airfoil"
                      "\n   CPXX   CPxx endpoint constraint toggle"
                      "\n\n   Visc   Qvis overlay toggle"
                      "\n   REFL   Reflected Qspec overlay toggle"
                      "\n\n   Plot   Plot Qspec (line) and Q (symbols)"
                      "\n   Blow   Blowup plot region"
                      "\n   Rese   Reset plot scale and origin"
                      "\n   Wind     Plot window adjust via cursor and keys"
                      "\n\n   SIZE r Change absolute plot-object size"
                      "\n  .ANNO   Annotate plot"
                      "\n   HARD   Hardcopy current plot\n"));
}

// Unknown command format 1100 deletes the redo entry
// (a following '!': 'Previous .QDES command not valid').
TEST(unit, QdesMenuUnknownAndInvalidRedo) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("FOO\n!\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text,
                 " Command FOO  not recognized.  Type a \" ? \" for list.\n"));
  CHECK(contains(text, " Previous .QDES command not valid\n"));
}

// valid '!': redo via the separate QDES redo state; the second
// SMOO returns the canonical second oracle.
TEST(unit, QdesMenuRedoRepeatsSmoo) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("SMOO\n!\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text, kSmooText1));
  CHECK(contains(text, kSmooText2));
  CHECK_EQ(countOccurrences(text, kPrompt), 3u);
}

// QSET is text-free (only the GAMQSP kernel).
TEST(unit, QdesMenuQsetSilent) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("QSET\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  CHECK(out.str() == std::string(kEntryText) + kPrompt + kPrompt);
}

// SMOO without Marking: canonical full range, 1200 block.
TEST(unit, QdesMenuSmooOracleBlock) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("SMOO\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  CHECK(contains(out.str(), kSmooText1));
}

// VISC/V, REFL, SLOP, CPXX: canonical toggle text.
TEST(unit, QdesMenuToggleTexts) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("VISC\nV\nREFL\nREFL\nSLOP\nCPXX\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text, " Qspec & Qvis will be plotted\n"));
  CHECK(contains(text, " Only Qspec will be plotted\n"));
  CHECK(contains(text, " Reflected Qspec will be plotted\n"));
  CHECK(contains(text, " Reflected Qspec will not be plotted\n"));
  CHECK(contains(text,
                 " Modified Qspec piece will not be made tangent at endpoints\n"));
  CHECK(contains(text, " CPxx will be constrained\n"));
}

// EXEC without a target segment: canonical refusal, no mutation.
TEST(unit, QdesMenuExecMustMark) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("EXEC\nX\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  const std::string text = out.str();
  CHECK_EQ(countOccurrences(text,
                            " ***  Must mark off target segment first  ***\n"),
           2u);
  CHECK(!s.inverse.shared.target.liqset);
  CHECK(s.paneling.panelingValid);
}

// REST (geometry commit) stays recognized but unexecuted.
TEST(unit, QdesMenuRestCommits) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("REST\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
 // 6.996 LEFIND bracket finds the LE for the NACA buffer/panel
  // pair; no limit-reached warning at execution time.
  CHECK(out.str() == std::string(kEntryText) + kPrompt + kPrompt);
  CHECK(s.paneling.panelingValid);
}

// plot/cursor commands: exactly one unavailable message per command,
// no mutation; ANNO follows the canonical ELSE branch (LPLOT is
// always .FALSE. without a plot backend).
TEST(unit, QdesMenuPlotCursorBoundary) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in(
      "PLOT\nP\nBLOW\nB\nRESE\nR\nWIND\nW\nSIZE\nSIZE 2\nHARD\nMARK\n"
      "MODI\nM\nZ\nU\nANNO\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  const std::string text = out.str();
  CHECK_EQ(countOccurrences(text,
                            "Plotting is not available in this build.\n"),
           16u);
  CHECK_EQ(countOccurrences(text, " No active plot to annotate\n"), 1u);
  CHECK(!s.inverse.shared.target.liqset);
  CHECK(s.paneling.panelingValid);
}

// EOF in the menu: controlled abort with the redo state persisted.
TEST(unit, QdesMenuEndOfFile) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("SMOO");
  std::ostringstream out;
  CHECK(cli::handleQdesCore(s, in, out) == cli::QdesCoreStatus::endOfFile);
}

// Determinism: three times identical Transcript text.
TEST(unit, QdesMenuTextDeterministicThreeRuns) {
  auto run = [] {
    SessionComposition s;
    setupPangenAirfoil(s, 2412);
    configureOper(s);
    std::istringstream in("QSET\nSMOO\nVISC\nREFL\nSLOP\nCPXX\n?\nFOO\n!\n"
                          "EXEC\n\n");
    std::ostringstream out;
    (void)cli::handleQdesCore(s, in, out);
    return out.str();
  };
  const std::string a = run();
  const std::string b = run();
  const std::string c = run();
  CHECK(a == b);
  CHECK(b == c);
}
// QDES EXEC without a target segment: exactly one canonical refusal per
// command, no mutation (contract unchanged).
TEST(unit, QdesMenuExecGuardText) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("EXEC\nX\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
  CHECK_EQ(countOccurrences(out.str(),
                            " ***  Must mark off target segment first  ***\n"),
           2u);
  CHECK_EQ(s.geometry.buffer.x.size(), 245u);
}

// QDES REST: text-free commit; after EXEC+REST (target segment set)
// the contour is reset; in a handler without a segment the contour can
// only remain unchanged with an invalid buffer - here the text-free
// behavior and the panel validity are checked.
TEST(unit, QdesMenuRestTextlessCommit) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("REST\n\n");
  std::ostringstream out;
  (void)cli::handleQdesCore(s, in, out);
 // 6.996 LEFIND bracket finds the LE for the NACA buffer/panel
  // pair; no limit-reached warning at execution time.
  CHECK(out.str() == std::string(kEntryText) + kPrompt + kPrompt);
  CHECK(s.paneling.panelingValid);
}

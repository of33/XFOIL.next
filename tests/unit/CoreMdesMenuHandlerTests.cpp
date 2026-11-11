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

// file: tests/unit/CoreMdesMenuHandlerTests.cpp
// unit tests of the MDES menu
// handlers handleMdesCore (xmdes.f 30-720). The text anchors derive
// from canonical Fortran transcripts (LC_ALL=C fortran-bin/xfoil <
// input, three times byte-identical); SCINIT-specific iteration texts
// (xmdes.f 81-87 via SCINIT WRITE) are core-internal and anchored via
// the matrix reference data, not in the handler. The test drives the
// MDES menu handler directly, and there is no
// plot backend.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreInverseDesignHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

// canonical prompt (xmdes.f 122-126, userio.f 180).
const char* const kPrompt = "\n.MDES   c>  ";
const char* const kPromptSym = "\n.MDESs   c>  ";

// FORMAT 1150/1190 (Entry/INIT, NACA 2412, ALFA=0).
const char* const kInitText =
    "\n Current Q operating condition:   alpha =   0.000     CL =   0.2551\n"
    "\n Qspec initialized to current Q\n";

}  // namespace

// Entry without contour (xmdes.f 53-57): two WRITE(*,*), no menu.
TEST(unit, MdesMenuNoAirfoilText) {
  SessionComposition s;
  std::istringstream in("\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(out.str() == " \n ***  No airfoil available  ***\n");
}

// Entry + <Return> exit: label-1 text, prompt, exit without
// additional text; menu inactive, LQSYM=false (xmdes.f 139-147).
TEST(unit, MdesMenuEntryAndReturn) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text, kInitText));
  CHECK(contains(text, kPrompt));
  CHECK_EQ(countOccurrences(text, kPrompt), 1u);
  CHECK(!s.inverse.mdes.active);
  CHECK(!s.inverse.flags.lqSym);
}

// EOF at the prompt (canonical READ abort; controlled handling).
TEST(unit, MdesMenuEndOfFile) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("QSET");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::endOfFile);
}

// Help format 1050 (byte-exact against the transcript) and unknown
// command format 1100 with an A4 field; a following '!' reports an
// invalid predecessor (COMOLD='****', xmdes.f 129-131, 712).
TEST(unit, MdesMenuHelpUnknownAndInvalidRedo) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("?\nFOO\n!\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text, "\n   <cr>   Return to Top Level\n"
                       "   !      Redo previous command\n"
                       "\n   INIT   Re-initialize mapping\n"
                       "   QSET   Reset Qspec <== Q\n"
                       "   AQ r.. Show/select alpha(s) for Qspec\n"
                       "   CQ r.. Show/select  CL(s)   for Qspec\n"));
  CHECK(contains(text, "\n   PERT   Perturb one Cn and generate geometry\n"));
  CHECK(contains(text, " Command FOO  not recognized.  Type a \" ? \" for list.\n"));
  CHECK(contains(text, " Previous .MDES command not valid\n"));
}

// INIT command: calls only initializeMdesNumerics(forceMapping);
// 1150+1190 appear again; panel and geometry state remain unchanged.
TEST(unit, MdesMenuInitRerunsLabelOne) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  const std::size_t n = s.paneling.panel.x.size();
  std::istringstream in("INIT\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK_EQ(countOccurrences(out.str(), kInitText), 2u);
  CHECK_EQ(s.paneling.panel.x.size(), n);
  CHECK(s.inverse.validity.mapping ==
        session::MappingCoordinateValidity::initializedFromPanel);
}

// SYMM/S: message plus prompt variant .MDESs; a second S is disabled.
TEST(unit, MdesMenuSymmetryToggleTextAndPrompt) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("SYMM\nS\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text, " Qspec symmetry forcing enabled.\n"));
  CHECK(contains(text, " Qspec symmetry forcing disabled.\n"));
  CHECK(contains(text, kPromptSym));
  CHECK(!s.inverse.flags.lqSym);
}

// TGAP inline (NINPUT>=2) without text; TANG with NINPUT<2 uses the
// prompt mode (1180 + ASKR; canonical quirk: inline needs two
// arguments, but only RINPUT(1) is used).
TEST(unit, MdesMenuTeGapAndTeAngle) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("TGAP 0.001 0.002\nTANG 16.0 0\nTANG\n17.0\n\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK_NEAR(s.inverse.shared.circle.dzte.real(), 0.001, 0.0, 0.0);
  CHECK_NEAR(s.inverse.shared.circle.dzte.imag(), 0.002, 0.0, 0.0);
    CHECK(contains(text, "\n Current TE angle = 16.000 deg.\n"));
  CHECK(contains(text, "\nEnter new TE angle (deg)   r>  "));
  CHECK_NEAR(s.inverse.shared.circle.agte, 17.0 / 180.0, 0.0, 0.0);
}

// TANG prompt mode before any inline change: canonical
// initial value of the PANGEN contour NACA 2412 (15.905 deg, FORMAT 1180).
TEST(unit, MdesMenuTeAngleInitialValue) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("TANG\n\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(contains(out.str(), "\n Current TE angle = 15.905 deg.\n"));
  CHECK_NEAR(s.inverse.shared.circle.agte, 15.905 / 180.0, 5.0e-4, 5.0e-4);
}

// FILT text block against canonical transcript (NACA 2412,
// AQ 5 10, CQ 0.5, FILT):
// Current : alpha = 0.0000 CL = 0.255075 CM = -0.055687
// Qspec 1 : alpha = 1.9278 CL = 0.500000 CM = -0.009962
TEST(unit, MdesMenuFilterTextMatchesFortranTranscript) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("INIT\nQSET\nAQ 5 10\nCQ 0.5\nSYMM\nS\n"
                          "TGAP 0.001 0.002\nTANG 16.0 0\nFILT\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(contains(out.str(),
                 "\n Current :  alpha =   0.0000    CL =   0.255075"
                 "    CM =  -0.055687\n"
                 " Qspec 1 :  alpha =   1.9278    CL =   0.500000"
                 "    CM =  -0.009962\n"));
}

// AQ prompt mode: 1150 + 1161 requirement list + 1162 prompt; an empty
// line leaves the state unchanged (GO TO 500).
TEST(unit, MdesMenuAqPromptMode) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("AQ\n\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text, "\n Current Qspec alphas  =    0.000\n"));
  CHECK(contains(text, " New alphas or <return>:  "));
  CHECK_EQ(s.inverse.shared.nqsp, 1);
  CHECK_NEAR(s.inverse.shared.qspecs[0].al, 0.0, 0.0, 0.0);
}

// CQ prompt mode is analogous (1171/1172).
TEST(unit, MdesMenuCqPromptMode) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("CQ\n0.5\n\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(contains(out.str(), "\n Current Qspec CLs  =  0.2551\n"));
  CHECK(contains(out.str(), " New CLs or <return>:  "));
  CHECK_EQ(s.inverse.shared.iacqsp, 2);
  CHECK_NEAR(s.inverse.shared.qspecs[0].cl, 0.5, 0.0, 0.0);
}

// SLOP/VISC/REFL messages (INIT defaults: LQSLOP=.TRUE.,
// LQVDES/LQREFL=.FALSE.).
TEST(unit, MdesMenuStateCommandTexts) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("SLOP\nVISC\nREFL\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text,
                 " Modified Qspec piece will not be made tangent at endpoints\n"));
  CHECK(contains(text, " Qspec & Qvis will be plotted\n"));
  CHECK(contains(text, " Reflected Qspec will be plotted\n"));
  CHECK(s.inverse.flags.lqVdes);
  CHECK(s.inverse.flags.lqRefl);
}

// plot/cursor commands remain recognized but unavailable (READ/DUMP
// are tested below); EXEC/PERT are tested separately.
TEST(unit, MdesMenuPlotAndGuardBoundaries) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in(
      "PLOT\nSPEC\nSIZE 2\nMARK\nMODI\nBLOW\nWIND\nZ\nU\n"
      "READ q.dat\nDUMP cn.dat\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK_EQ(countOccurrences(text, "Plotting is not available in this build.\n"),
           9u);
  CHECK(!contains(text, "not recognized"));
  // No partial activation: Qspec/panel state unchanged.
  CHECK_EQ(s.inverse.shared.nqsp, 1);
  CHECK(s.paneling.panelingValid);
}

// '!' redo: a successful forward command is executed again;
// the MDES redo is separate from the GDES redo (contract).
TEST(unit, MdesMenuRedoUsesSeparateMdesSlot) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  s.design.menu.gdesRedo = std::make_pair(std::string("GSET"), std::string(""));
  std::istringstream in("VISC\n!\n\n");
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK_EQ(countOccurrences(text, " Qspec & Qvis will be plotted\n"), 1u);
  CHECK_EQ(countOccurrences(text, " Only Qspec will be plotted\n"), 1u);
  CHECK(!s.inverse.flags.lqVdes);
  CHECK(s.design.menu.gdesRedo.has_value());
  CHECK(s.inverse.mdes.redo.has_value());
  CHECK_EQ(s.inverse.mdes.redo->first, std::string("VISC"));
}

// Determinism: two identical handler runs deliver byte-identical
// Output.
TEST(unit, MdesMenuOutputIsDeterministic) {
  auto run = [] {
    SessionComposition s;
    setupPangenAirfoil(s, 2412);
    configureOper(s);
    std::istringstream in("INIT\nQSET\nAQ 5 10\nCQ 0.5\nSYMM\nS\n"
                          "TGAP 0.001 0.002\nTANG 16.0 0\nFILT\nSLOP\n"
                          "VISC\nREFL\nFOO\n!\n?\n\n");
    std::ostringstream out;
    const auto status = cli::handleMdesCore(s, in, out);
    CHECK(status == cli::MdesCoreStatus::returned);
    return out.str();
  };
  const std::string a = run();
  const std::string b = run();
  CHECK(a == b);
  CHECK_EQ(countOccurrences(a, " Command FOO  not recognized."), 1u);
  CHECK_EQ(countOccurrences(a, " Previous .MDES command not valid"), 1u);
}
// --- READ/DUMP handler branches ---------------------

namespace {

struct MdesFileCase {
  // RAII-owned, unique test directory per test run; dir is the path
  // copy used by the existing comparisons.
  std::unique_ptr<testing::ScopedTempDirectory> work;
  std::filesystem::path dir;
};

MdesFileCase makeMdesFileCase(const std::string& id) {
  MdesFileCase c;
  c.work = std::make_unique<testing::ScopedTempDirectory>("mdes-" + id);
  c.dir = c.work->path();
  return c;
}

std::string readWhole(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    return std::string{};
  }
  std::ostringstream b;
  b << f.rdbuf();
  return b.str();
}

const char* const kVovData =
    "0.0 0.30\n0.05 0.0\n0.1 0.85\n0.3 1.30\n0.6 1.20\n0.9 1.05\n1.0 1.00\n";

}  // namespace

// READ normal against the canonical CLI oracle (prompt on the open
// line, 1200/1210 block with Cl -0.098496 CM -0.011622).
TEST(unit, MdesMenuReadNormalMatchesOracle) {
  auto c = makeMdesFileCase("read_normal");
  {
    std::ofstream f(c.dir / "vov.dat", std::ios::binary);
    f << kVovData;
  }
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in((std::string("READ\n") + (c.dir / "vov.dat").string() + "\n\n").c_str());
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text, "\nEnter V/Vinf vs s data filename   s>  "));
  CHECK(contains(text,
                 "\n Current :  alpha =   0.0000    CL =   0.255075"
                 "    CM =  -0.055687\n"
                 " Qspec 1 :  alpha =   0.0000    CL =  -0.098496"
                 "    CM =  -0.011622\n"));
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::fileRead);
  std::filesystem::remove_all(c.dir);
}

// READ open error (ERR=98): message on the open prompt line, no
// mutation, follow-up report from the unchanged state.
TEST(unit, MdesMenuReadOpenErrorOracle) {
  auto c = makeMdesFileCase("read_openerr");
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in((std::string("READ\n") + (c.dir / "missing.dat").string() + "\n\n").c_str());
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  const std::string text = out.str();
  CHECK(contains(text, " GETVOV: File OPEN error.\n"));
  CHECK(contains(text,
                 " Qspec 1 :  alpha =   0.0000    CL =   0.255075"
                 "    CM =  -0.055687\n"));
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::currentQ);
  std::filesystem::remove_all(c.dir);
}

// READ parse error (ERR=99): message, no mutation (also no
// partial points), the follow-up still runs.
TEST(unit, MdesMenuReadParseErrorOracle) {
  auto c = makeMdesFileCase("read_parseerr");
  {
    std::ofstream f(c.dir / "bad.dat", std::ios::binary);
    f << "0.0 1.0\n0.5 0.0\n1.0 1.2\nxx\n";
  }
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in((std::string("READ\n") + (c.dir / "bad.dat").string() + "\n\n").c_str());
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(contains(out.str(), " GETVOV: File READ error.\n"));
  CHECK(contains(out.str(),
                 " Qspec 1 :  alpha =   0.0000    CL =   0.255075"
                 "    CM =  -0.055687\n"));
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::currentQ);
  std::filesystem::remove_all(c.dir);
}

// DUMP inline filename: canonical STATUS='UNKNOWN' overwrite
// without query; file bytes byte-exact against the Fortran oracle.
TEST(unit, MdesMenuDumpInlineBytesMatchOracle) {
  auto c = makeMdesFileCase("dump_inline");
  {
    std::ofstream pre(c.dir / "cn.out", std::ios::binary);
    pre << "PRE-EXISTING\n";
  }
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in((std::string("DUMP ") + (c.dir / "cn.out").string() + "\n\n").c_str());
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(!contains(out.str(), "Overwrite"));
  CHECK(readWhole(c.dir / "cn.out") ==
        readWhole("tests/data/mdes-file-reference/cn_after_init.txt"));
  std::filesystem::remove_all(c.dir);
}

// DUMP prompt mode + READ->DUMP bytes against the READ oracle.
TEST(unit, MdesMenuDumpPromptAfterReadMatchesOracle) {
  auto c = makeMdesFileCase("dump_prompt");
  {
    std::ofstream f(c.dir / "vov.dat", std::ios::binary);
    f << kVovData;
  }
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in(
      (std::string("READ\n") + (c.dir / "vov.dat").string() + "\nDUMP\n" +
       (c.dir / "cn.out").string() + "\n\n").c_str());
  std::ostringstream out;
  const auto status = cli::handleMdesCore(s, in, out);
  CHECK(status == cli::MdesCoreStatus::returned);
  CHECK(contains(out.str(), "\nEnter Cn output filename   s>  "));
  CHECK(readWhole(c.dir / "cn.out") ==
        readWhole("tests/data/mdes-file-reference/cn_after_read_vov2.txt"));
  std::filesystem::remove_all(c.dir);
}

// Repeated READ and independent sessions: third run identical.
TEST(unit, MdesMenuRepeatedReadDeterministic) {
  auto run = [] {
    auto c = makeMdesFileCase("read_repeat");
    {
      std::ofstream f(c.dir / "vov.dat", std::ios::binary);
      f << kVovData;
    }
    SessionComposition s;
    setupPangenAirfoil(s, 2412);
    configureOper(s);
    std::istringstream in(
        (std::string("READ\n") + (c.dir / "vov.dat").string() + "\nREAD\n" +
         (c.dir / "vov.dat").string() + "\n\n").c_str());
    std::ostringstream out;
    const auto status = cli::handleMdesCore(s, in, out);
    CHECK(status == cli::MdesCoreStatus::returned);
    std::filesystem::remove_all(c.dir);
    return out.str();
  };
  const std::string a = run();
  const std::string b = run();
  const std::string c2 = run();
  CHECK(a == b);
  CHECK(b == c2);
  CHECK_EQ(countOccurrences(a, " Qspec 1 :  alpha =   0.0000    CL =  -0.098496"), 2u);
}

// --- MDES EXEC/PERT and QDES EXEC/REST in the handler ------------

// MDES EXEC after INIT: canonical format-1300 block (oracle m1).
TEST(unit, MdesMenuExecNewBufferText) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("INIT\nEXEC\n\n");
  std::ostringstream out;
  (void)cli::handleMdesCore(s, in, out);
  CHECK(contains(out.str(),
                 "\n New buffer airfoil generated"
                 "\n Execute PANE at Top Level to set new current airfoil\n"));
  CHECK_EQ(s.geometry.buffer.x.size(), 257u);
}

// MDES PERT: table (first line 0.910192/-0.000054), prompt,
// valid input, then automatic EXEC block; an invalid index
// reports 'Max number of modes is' and prompts again (oracle m2).
TEST(unit, MdesMenuPertTableAndAutoExec) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("INIT\nPERT\n999 0.1 0.1\n1 0.001 0.0\n\n");
  std::ostringstream out;
  (void)cli::handleMdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text, "\n Current mapping coefficients...\n"
                      "       n    Re(Cn)      In the(Cn)\n"
                      "       1    0.910192   -0.000054\n"));
  CHECK(contains(text, " Max number of modes is         257\n"));
  CHECK(contains(text, "    Enter  n, delta(Cnr), delta(Cni):  "));
  CHECK(contains(text, "\n New buffer airfoil generated"));
  CHECK_EQ(s.geometry.buffer.x.size(), 257u);
}

// PERT Newton diagnostics (xmdes.f PERT 1713-1745): WRITE(*,*)
// ITERCN, DCNMAX per iteration before the EXEC block; the converged
// primary case (1 0.001 0.0) writes exactly two lines and no
// non-convergence text.
// PUB-W2 (Windows branch of the embedded-number text tests): the
// Newton iteration residual lines carry platform-dependent values
// (path-dependent residuals in full E-precision). Instead of a blind
// string loosening, the Windows contract verifies the unchanged text
// construction and the convergence semantics:
// - the lines keep the exact layout "  <iteration>   <mantissa>E<e-3>",
// - the numbering is scoped to the PERT prompt sections (see below),
//   forming consecutive runs that start at 1; a new block begins only
//   at a genuine restart (iteration 1),
// - every parsed residual is finite, positive and far below the
//   magnitude bound 1e-1 (observed values 1.0e-3 .. 4.3e-7),
// - for converged runs the final residual of the last block is
//   <= 1e-5 (observed 5.5e-10; the E-007 digit step is 1e-7, so the
//   bound is two full steps plus the documented order of the
//   measurement, not a wide tolerance).
// On Linux the byte-exact text contract is kept unchanged.
//
// PUB-W3/PUB-W4 prompt scoping (from the canonical transcript): the
// PERT Newton table is emitted inside a section that starts at the
// prompt line "    Enter  n, delta(Cnr), delta(Cni):"; iteration 1 is
// printed BEHIND the prompt on that very line, continuation
// iterations on the following lines, and the converged run ends with
// a genuine restart block (a second internal Newton pass, e.g.
// "1   5.48E-010"). Foreign iteration-shaped tables (the SCINIT/MAPGEN
// "max(dw)" blocks of INIT, coefficient tables, an EXEC follow-up) lie
// outside the prompt sections and must never be collected; the PUB-W3
// flat scan mixed them up and produced phantom blocks (single PERT:
// [2],[1]; double PERT: 3 blocks).
struct PertIterationLine {
  int iteration = 0;
  double value = 0.0;
};

// PUB-W4: prompt-section-scoped extractor. Sections begin at the PERT
// prompt; a line is an iteration line only if the WHOLE line (or, for
// the prompt line, the remainder behind "delta(Cni):") has the shape
// "<blanks><iteration><blanks><mantissa>E<sign><3 digits><blanks>".
const char* const kPertPrompt = "Enter  n, delta(Cnr), delta(Cni):";

bool parsePertIterationLine(const std::string& line,
                            std::string::size_type begin,
                            PertIterationLine& parsed) {
  const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
  std::string::size_type i = begin;
  while (i < line.size() && line[i] == ' ') {
    ++i;
  }
  const std::string::size_type digitsBegin = i;
  while (i < line.size() && isDigit(line[i])) {
    ++i;
  }
  if (i == digitsBegin || i >= line.size() || line[i] != ' ') {
    return false;
  }
  parsed.iteration = std::stoi(line.substr(digitsBegin, i - digitsBegin));
  while (i < line.size() && line[i] == ' ') {
    ++i;
  }
  const std::string::size_type valueBegin = i;
  const std::string::size_type mantissaEnd = valueBegin + 1;
  if (mantissaEnd >= line.size() || !isDigit(line[valueBegin]) ||
      line[mantissaEnd] != '.') {
    return false;
  }
  std::string::size_type j = mantissaEnd + 1;
  const std::string::size_type fractionBegin = j;
  while (j < line.size() && isDigit(line[j])) {
    ++j;
  }
  if (j == fractionBegin || j + 5 > line.size() || line[j] != 'E' ||
      (line[j + 1] != '+' && line[j + 1] != '-') || !isDigit(line[j + 2]) ||
      !isDigit(line[j + 3]) || !isDigit(line[j + 4])) {
    return false;
  }
  j += 5;
  while (j < line.size() && line[j] == ' ') {
    ++j;
  }
  if (j != line.size()) {
    return false;
  }
  parsed.value = std::strtod(line.substr(valueBegin, j - valueBegin).c_str(),
                             nullptr);
  return std::isfinite(parsed.value);
}

std::vector<std::vector<PertIterationLine>> extractPertIterationBlocks(
    const std::string& text, std::size_t& promptCount) {
  std::vector<std::vector<PertIterationLine>> blocks;
  promptCount = 0;
  std::istringstream stream(text);
  std::string line;
  bool inPertSection = false;
  while (std::getline(stream, line)) {
    PertIterationLine parsed;
    const std::string::size_type promptAt = line.find(kPertPrompt);
    if (promptAt != std::string::npos) {
      ++promptCount;
      inPertSection = true;
      const std::string::size_type after =
          promptAt + std::string(kPertPrompt).size();
      if (parsePertIterationLine(line, after, parsed)) {
        if (blocks.empty() || parsed.iteration == 1) {
          blocks.emplace_back();
        }
        blocks.back().push_back(parsed);
      }
      continue;
    }
    if (!inPertSection) {
      continue;
    }
    if (!parsePertIterationLine(line, 0, parsed)) {
      inPertSection = false;  // the section ended (table/banner/etc.)
      continue;
    }
    if (blocks.empty() || parsed.iteration == 1) {
      blocks.emplace_back();  // genuine restart opens a new block
    }
    blocks.back().push_back(parsed);
  }
  return blocks;
}

#ifdef _WIN32
void checkPortablePertIterationText(const std::string& text,
                                    bool expectConverged,
                                    std::size_t expectedPromptCount,
                                    std::size_t expectedBlockCount) {
  std::size_t promptCount = 0;
  const std::vector<std::vector<PertIterationLine>> blocks =
      extractPertIterationBlocks(text, promptCount);
  CHECK_EQ(promptCount, expectedPromptCount);
  CHECK(!blocks.empty());
  CHECK_EQ(blocks.size(), expectedBlockCount);
  for (const auto& block : blocks) {
    CHECK(!block.empty());
    CHECK_EQ(block.front().iteration, 1);  // blocks start at a restart
    int expectedIteration = 1;
    for (const auto& entry : block) {
      CHECK(entry.value > 0.0);
      CHECK(std::isfinite(entry.value));
      CHECK(entry.value <= 1e-1);  // magnitude bound, not tolerance
      CHECK_EQ(entry.iteration, expectedIteration);
      expectedIteration = entry.iteration + 1;
    }
  }
  if (expectConverged && !blocks.empty() && !blocks.back().empty()) {
    CHECK(blocks.back().back().value <= 1e-5);
  }
}
#endif  // _WIN32

// PUB-W3/PUB-W4: the prompt-section parser is platform independent.
// Fixtures cover: surrounding INIT-style iteration tables that must be
// ignored, one PERT section with the prompt-line iteration and a
// genuine restart block, two PERT sections, and a foreign table that
// must not be collected.
TEST(unit, MdesPertIterationBlockParser) {
  const std::string prompt = "    Enter  n, delta(Cnr), delta(Cni):";
  {
    const std::string text =
        "           1      max(dw) =   1.8671573587363710E-002\n"
        "           2      max(dw) =   8.2382643596907723E-003\n"
        "    1    0.910192   -0.000054\n"
        "    1   1.2345678901234567E-003\n"  // foreign, no prompt scope
        "    2   1.2345678901234567E-006\n"
        + prompt + "             1   1.0017816811997685E-003\n"
        "           2   4.2575723288811890E-007\n"
        "           1   5.4816390155947027E-010\n"
        " Max thickness =     0.120020  at x =   0.304\n";
    std::size_t prompts = 0;
    const std::vector<std::vector<PertIterationLine>> blocks =
        extractPertIterationBlocks(text, prompts);
    CHECK_EQ(prompts, std::size_t{1});
    CHECK_EQ(blocks.size(), std::size_t{2});  // [1,2] then restart [1]
    if (blocks.size() == 2) {
      CHECK_EQ(blocks[0].size(), std::size_t{2});
      CHECK_EQ(blocks[1].size(), std::size_t{1});
      CHECK_EQ(blocks[0][0].iteration, 1);
      CHECK_EQ(blocks[0][1].iteration, 2);
      CHECK_EQ(blocks[1][0].iteration, 1);
      CHECK(std::fabs(blocks[0][0].value - 1.0017816811997685e-3) <= 0.0);
      CHECK(blocks[1][0].value > 0.0);
    }
  }
  {
    // Two PERT sections: four blocks total, sections never merge.
    const std::string text =
        prompt + "             1   1.0017816811997685E-003\n"
        "           2   4.2575723288811890E-007\n"
        "           1   5.4816390155947027E-010\n"
        " Max thickness =     0.120020  at x =   0.304\n"
        " Current mapping coefficients...\n"
        "       1    0.910192   -0.000054\n"
        + prompt + "             1   1.0011505098921308E-003\n"
        "           2   4.2602982268161144E-007\n"
        "           1   5.5000000000000000E-010\n";
    std::size_t prompts = 0;
    const std::vector<std::vector<PertIterationLine>> blocks =
        extractPertIterationBlocks(text, prompts);
    CHECK_EQ(prompts, std::size_t{2});
    CHECK_EQ(blocks.size(), std::size_t{4});
  }
  {
    // Restart inside one section, and a prompt line without a trailing
    // iteration followed by continuation lines (both layouts supported).
    const std::string text =
        prompt + "\n"
        "           1   1.0000000000000000E-003\n"
        "           2   1.0000000000000000E-006\n"
        "           1   1.0000000000000000E-009\n"
        "           2   1.0000000000000000E-012\n"
        "some other output\n";
    std::size_t prompts = 0;
    const std::vector<std::vector<PertIterationLine>> blocks =
        extractPertIterationBlocks(text, prompts);
    CHECK_EQ(prompts, std::size_t{1});
    CHECK_EQ(blocks.size(), std::size_t{2});  // [1,2] restart [1,2]
  }
}

TEST(unit, MdesMenuPertNewtonIterationText) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("INIT\nPERT\n1 0.001 0.0\n\n");
  std::ostringstream out;
  (void)cli::handleMdesCore(s, in, out);
  const std::string text = out.str();
#ifdef _WIN32
  checkPortablePertIterationText(text, /*expectConverged=*/true,
                                 /*expectedPromptCount=*/1,
                                 /*expectedBlockCount=*/2);
#else
  CHECK(contains(text, "             1   1.0017816811997685E-003\n"));
  CHECK(contains(text, "           2   4.2575723288811890E-007\n"));
#endif
  CHECK(!contains(text, "TE gap,chord did not converge"));
}

// Double PERT: the second coefficient table must show the state
// written on by QSPCIR (canonical CN(1) = 0.910191 in the second
// table, format 1010).
TEST(unit, MdesMenuDoublePertSecondTable) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("INIT\nPERT\n1 0.001 0.0\nPERT\n1 0.001 0.0\n\n");
  std::ostringstream out;
  (void)cli::handleMdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text, "    1    0.910192   -0.000054"));
  CHECK(contains(text, "    1    0.910191   -0.000054"));
#ifdef _WIN32
  // PUB-W2: the two coefficient tables are byte-checked above (they
  // round identically on Windows); the embedded E-precision Newton
  // residual is verified structurally (see the helper contract).
  checkPortablePertIterationText(text, /*expectConverged=*/false,
                                 /*expectedPromptCount=*/2,
                                 /*expectedBlockCount=*/4);
#else
  CHECK(contains(text, "             1   1.0011505098921308E-003\n"));
#endif
}

// PERT non-convergence (1 50.0 0.0): exactly 10 iteration lines,
// then the list-directed ' TE gap,chord did not converge', followed by
// the unchanged EXEC path with its own MAPGEN block.
TEST(unit, MdesMenuPertNonConvergedText) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("INIT\nPERT\n1 50.0 0.0\n\n");
  std::ostringstream out;
  (void)cli::handleMdesCore(s, in, out);
  const std::string text = out.str();
  CHECK(contains(text, " TE gap,chord did not converge\n"));
  std::size_t pertWarnCount = 0;
  std::string::size_type pos = 0;
  while ((pos = text.find("TE gap,chord did not converge", pos)) !=
         std::string::npos) {
    ++pertWarnCount;
    ++pos;
  }
  CHECK_EQ(pertWarnCount, 1u);
  // PERT Newton writes exactly 10 iteration lines (ITERCN 1..10).
  CHECK(contains(text, "          10   "));
  CHECK(contains(text, " MAPGEN: Geometric constraints not fully converged\n"));
  CHECK(contains(text, "\n New buffer airfoil generated"));
}


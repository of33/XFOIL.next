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

// file: tests/unit/CoreGdesMenuHandlerTests.cpp
// byte-exact verification of the GDES menu handlers
// (handleGeometryDesignCore, xgdes.f 21-949) against
// the design-state Fortran references. The reference files are
// read-only.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;

namespace fs = std::filesystem;

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

std::vector<std::string> readLines(const fs::path& path) {
  std::vector<std::string> lines;
  std::ifstream in(path, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(line);
  }
  return lines;
}

// reference segment: from the GDES entry text up to just before the first
// TOP prompt after the GDES exit (which reads the next TOP line,
// e.g. PSAV before QUIT). If the reference ends within the GDES menus
// (canonical EOF abort), the segment reaches to the end of file.
std::string referenceGdesSegment(const std::string& reference) {
  const std::string start = "\n You are working with the buffer airfoil";
  const std::size_t begin = reference.find(start);
  if (begin == std::string::npos) {
    return {};
  }
  const std::size_t end = reference.find("\n XFOIL", begin);
  if (end == std::string::npos) {
    return reference.substr(begin);
  }
  if (end <= begin) {
    return {};
  }
  return reference.substr(begin, end - begin);
}

// Feeds the commands before GDES (NACA/PANE) through the existing core
// handlers and returns the remaining stream (including the GDES line).
// If the reference ends inside the GDES menus (canonical EOF abort, no
// TOP prompt after entry), the QUIT line is also passed to the menu
// (Fortran reads it as 'not recognized').
void runHandlerCase(const std::string& caseId, std::ostringstream& out,
                    std::string& actual) {
  const fs::path dir =
      fs::absolute(fs::path("tests/fortran-reference/cases") / caseId);
  const std::vector<std::string> lines = readLines(dir / "commands.in");
  const std::string reference = readFile(dir / "stdout.raw.txt");
  const std::size_t gdesBegin =
      reference.find("\n You are working with the buffer airfoil");
  const bool eofInsideMenu =
      gdesBegin != std::string::npos &&
      reference.find("\n XFOIL", gdesBegin) == std::string::npos;

  session::SessionComposition session;
  {
    xfoil::core::paneling::PanelingParameters params =
        session::panelingParameters(session);
    params.panelCount = 160;
    params.teToLeDensityRatio = 0.15;
    CHECK(session::setPanelingParameters(session, params).ok);
  }
  std::size_t gdesIndex = 0;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (lines[i] == "GDES") {
      gdesIndex = i;
      break;
    }
    if (lines[i].rfind("NACA ", 0) == 0) {
      app::cli::handleNacaCore(session, out,
                               std::stoi(lines[i].substr(5)));
    } else if (lines[i] == "PANE") {
      (void)app::cli::handlePaneCore(session, out);
    } else if (lines[i] == "NORM") {
      (void)app::cli::handleNormCore(session, out);
    } else if (lines[i].rfind("LOAD ", 0) == 0) {
      const std::string file = lines[i].substr(5);
      (void)app::cli::handleLoadCore(
          session, dir / "input" / file, "", out);
    }
  }
  // copy the case input files into a dedicated temporary directory
  // and run the menu there (the Fortran harness works in the case
  // directory; OVER stores the entered path in ONAME). The RAII
  // helper restores the process working directory even if a menu
  // handler throws, and it reliably removes the temporary directory
  // afterwards.
  const fs::path inputDir = dir / "input";
  const bool hasInput = fs::exists(inputDir);
  const std::string tag =
      "gdes-" + caseId.substr(caseId.find('/') + 1) +
      (hasInput ? "-in" : "-noinput");
  testing::ScopedTempDirectory work(tag, true);
  if (hasInput) {
    for (const auto& entry : fs::directory_iterator(inputDir)) {
      fs::copy_file(entry.path(),
                    work.makePath(entry.path().filename().string()),
                    fs::copy_options::overwrite_existing);
    }
  }
  std::ostringstream menuInput;
  for (std::size_t i = gdesIndex + 1; i < lines.size(); ++i) {
    if (lines[i] == "QUIT" && !eofInsideMenu) {
      break;
    }
    menuInput << lines[i] << "\n";
  }
  std::istringstream in(menuInput.str());
  std::ostringstream menuOut;
  (void)app::cli::handleGeometryDesignCore(session, in, menuOut);
  actual = menuOut.str();
}

void checkAgainstReference(const std::string& caseId) {
  std::ostringstream setupOut;
  std::string actual;
  runHandlerCase(caseId, setupOut, actual);
  const fs::path dir =
      fs::absolute(fs::path("tests/fortran-reference/cases") / caseId);
  const std::string expected = referenceGdesSegment(readFile(dir / "stdout.raw.txt"));
  if (actual != expected) {
    std::cerr << "\n=== " << caseId << " ===\n--- expected ---\n"
              << expected << "\n--- actual ---\n" << actual
              << "\n--- end ---\n";
  }
  CHECK(actual == expected);
}

TEST(unit, CoreGdesMenuHelpUnknownExitMatchesFortran) {
  checkAgainstReference("design-state/gdes-help-unknown-exit");
}

TEST(unit, CoreGdesMenuGsetMatchesFortran) {
  checkAgainstReference("design-state/gdes-gset");
}

TEST(unit, CoreGdesMenuSymmMatchesFortran) {
  checkAgainstReference("design-state/gdes-symm");
}

TEST(unit, CoreGdesMenuSymmPromptMatchesFortran) {
  checkAgainstReference("design-state/gdes-symm-prompt");
}

TEST(unit, CoreGdesMenuAdegAradMatchesFortran) {
  checkAgainstReference("design-state/gdes-adeg-arad");
}

TEST(unit, CoreGdesMenuTranMatchesFortran) {
  checkAgainstReference("design-state/gdes-tran");
}

TEST(unit, CoreGdesMenuScalNegMatchesFortran) {
  checkAgainstReference("design-state/gdes-scal-neg");
}

TEST(unit, CoreGdesMenuLinsMatchesFortran) {
  checkAgainstReference("design-state/gdes-lins");
}

TEST(unit, CoreGdesMenuLinsBadMatchesFortran) {
  checkAgainstReference("design-state/gdes-lins-bad");
}

TEST(unit, CoreGdesMenuTgapMatchesFortran) {
  checkAgainstReference("design-state/gdes-tgap");
}

TEST(unit, CoreGdesMenuTgapPromptMatchesFortran) {
  checkAgainstReference("design-state/gdes-tgap-prompt");
}

TEST(unit, CoreGdesMenuLeraMatchesFortran) {
  checkAgainstReference("design-state/gdes-lera");
}

TEST(unit, CoreGdesMenuHighMatchesFortran) {
  checkAgainstReference("design-state/gdes-high");
}

TEST(unit, CoreGdesMenuNmovMatchesFortran) {
  checkAgainstReference("design-state/gdes-nmov");
}

TEST(unit, CoreGdesMenuUnitMatchesFortran) {
  checkAgainstReference("design-state/gdes-unit");
}

TEST(unit, CoreGdesMenuClisMatchesFortran) {
  checkAgainstReference("design-state/gdes-clis");
}

TEST(unit, CoreGdesMenuCangMatchesFortran) {
  checkAgainstReference("geometry-edit/gdes-cang");
}

TEST(unit, CoreGdesMenuBendMatchesFortran) {
  checkAgainstReference("geometry-edit/gdes-bend");
}

TEST(unit, CoreGdesMenuTsetMatchesFortran) {
  checkAgainstReference("geometry-edit/gdes-tset");
}

TEST(unit, CoreGdesMenuTsetQuestionableNoMatchesFortran) {
  checkAgainstReference("design-state/gdes-tset-questionable");
}

// design-state/gdes-tset-questionable-yes: direct canonical
// comparison of the deterministic prefix up to and including the
// GEOPAR output after THKCAM (TSET report, prompts, questionable text,
// ASKL, THKCAM-GEOPAR metric line). The rest of the reference consists
// exclusively of SOPPS warning records from the second GETCAM phase.
// Their count on this overscaled wild geometry is not
// compiler-configuration invariant: the same canonical Fortran source
// returns 43 (-O0/-O1/-O2/-O3/-Os, -fdefault-real-8), 35
// (-O2 -march=x86-64-v3), or 86 (shipped fortran-bin/xfoil, unknown
// build flags) depending on the build configuration. The C++ core
// matches the -O0-DBL harness variant bit-exactly (CYMAX/TFAC/CFAC
// E24.16 identical, all numerical output lines identical, counter 43).
// the record count of the reference (86) is therefore not a portable
// contract; the geometry and metric output remains byte-exact
// compared.
TEST(unit, CoreGdesMenuTsetQuestionableYesDeterministicPrefixMatchesFortran) {
  const std::string caseId = "design-state/gdes-tset-questionable-yes";
  std::ostringstream setupOut;
  std::string actual;
  runHandlerCase(caseId, setupOut, actual);
  const fs::path dir =
      fs::absolute(fs::path("tests/fortran-reference/cases") / caseId);
  std::string expected =
      referenceGdesSegment(readFile(dir / "stdout.raw.txt"));
  const std::string marker = "\n Max thickness =     1.057482  at x =   0.994\n"
                             " Max camber    =    -0.573745  at x =   0.065\n";
  const std::size_t end = expected.find(marker);
  CHECK(end != std::string::npos);
  if (end != std::string::npos) {
    const std::size_t prefixEnd = end + marker.size();
    const std::string expectedPrefix = expected.substr(0, prefixEnd);
    CHECK(actual.size() >= prefixEnd);
    CHECK(actual.substr(0, prefixEnd) == expectedPrefix);
    // after the prefix follow only SOPPS warning records (count dependent
    // on the floating-point configuration) and then the identical exit tail.
    const std::string warning =
        " SOPPS: Opposite-point location failed. Continuing...\n";
    auto splitWarnings = [&](const std::string& s, std::size_t& count) {
      std::string rest = s;
      count = 0;
      while (rest.rfind(warning, 0) == 0) {
        ++count;
        rest.erase(0, warning.size());
      }
      return rest;
    };
    std::size_t expectedWarnings = 0;
    std::size_t actualWarnings = 0;
    const std::string expectedTail =
        splitWarnings(expected.substr(prefixEnd), expectedWarnings);
    const std::string actualTail =
        splitWarnings(actual.substr(prefixEnd), actualWarnings);
    CHECK(expectedWarnings > 0);
    CHECK(actualWarnings > 0);
    CHECK(actualTail == expectedTail);
  }
}

TEST(unit, CoreGdesMenuFlapMatchesFortran) {
  checkAgainstReference("geometry-edit/gdes-flap");
}

TEST(unit, CoreGdesMenuFlapPromptMatchesFortran) {
  checkAgainstReference("design-state/gdes-flap-prompt");
}

TEST(unit, CoreGdesMenuNameNincNdecMatchesFortran) {
  checkAgainstReference("design-state/gdes-name-ninc-ndec");
}

TEST(unit, CoreGdesMenuRedoMatchesFortran) {
  checkAgainstReference("design-state/gdes-redo");
}

TEST(unit, CoreGdesMenuCaddInlineMatchesFortran) {
  checkAgainstReference("design-state/gdes-cadd-inline");
}

TEST(unit, CoreGdesMenuCaddPromptMatchesFortran) {
  checkAgainstReference("design-state/gdes-cadd-prompt");
}

TEST(unit, CoreGdesMenuExitWarningMatchesFortran) {
  checkAgainstReference("design-state/gdes-exit-warning");
}

TEST(unit, CoreGdesMenuRotateDeroMatchesFortran) {
  checkAgainstReference("geometry-edit/gdes-rotate-dero");
}

// OVER/O file and ONAME semantics (xgdes.f 2262-2367).
TEST(unit, CoreGdesOverAsksPlainMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-asks-plain");
}

TEST(unit, CoreGdesOverInlineArgMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-inline-arg");
}

TEST(unit, CoreGdesOverOpenErrorMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-open-error");
}

TEST(unit, CoreGdesOverReadErrorMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-read-error");
}

TEST(unit, CoreGdesOverLabeledMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-labeled");
}

TEST(unit, CoreGdesOverLnormMatchesXfoil6996S03) {
  // S03 target fixture: 6.996 NORM/LEFIND finds the overlay LE and
  // normalizes the overlay contour. The canonical 6.99 reference stays
  // untouched under tests/fortran-reference.
  std::ostringstream setupOut;
  std::string actual;
  runHandlerCase("design-state/gdes-over-lnorm", setupOut, actual);
  const std::string expected = readFile(
      "tests/data/core-gdes-menu-reference/6.996-s03/"
      "gdes-over-lnorm.segment.txt");
  if (actual != expected) {
    std::cerr << "\n=== design-state/gdes-over-lnorm 6.996-s03 ===\n"
              << "--- expected ---\n" << expected
              << "\n--- actual ---\n" << actual << "\n--- end ---\n";
  }
  CHECK(actual == expected);
}

TEST(unit, CoreGdesOverMsesSingleMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-mses-single");
}

TEST(unit, CoreGdesOverMsesMultiMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-mses-multi");
}

TEST(unit, CoreGdesOverRedoMatchesFortran) {
  checkAgainstReference("design-state/gdes-over-redo");
}

TEST(unit, CoreGdesOverOverflowMatchesFortranExceptCapacityToken) {
  // The 6.99 reference reports the old 2*IQX=740 buffer cap. S01 raises
  // that cap to 1203; all other bytes remain the canonical reference.
  std::ostringstream setupOut;
  std::string actual;
  runHandlerCase("design-state/gdes-over-overflow", setupOut, actual);
  const fs::path dir = fs::absolute(
      fs::path("tests/fortran-reference/cases") /
      "design-state/gdes-over-overflow");
  std::string expected = referenceGdesSegment(
      readFile(dir / "stdout.raw.txt"));
  const std::string oldToken = "Maximum number of points:  740";
  const std::string newToken = "Maximum number of points: 1202";
  const std::size_t pos = expected.find(oldToken);
  CHECK(pos != std::string::npos);
  if (pos != std::string::npos) {
    expected.replace(pos, oldToken.size(), newToken);
  }
  CHECK(actual == expected);
}

// OVER changes neither buffer nor panel: SAVE after the GDES exit still
// writes the NACA-0012 panel contour (reference generated/out.dat).
TEST(unit, CoreGdesOverStateUnchangedMatchesFortran) {
  const fs::path dir = fs::absolute(
      fs::path("tests/fortran-reference/cases") /
      "design-state/gdes-over-state-unchanged");
  session::SessionComposition session;
  {
    xfoil::core::paneling::PanelingParameters params =
        session::panelingParameters(session);
    params.panelCount = 160;
    params.teToLeDensityRatio = 0.15;
    CHECK(session::setPanelingParameters(session, params).ok);
  }
  std::ostringstream setupOut;
  app::cli::handleNacaCore(session, setupOut, 12);
  testing::ScopedTempDirectory work("gdes-over-state-unchanged", true);
  fs::copy_file(dir / "input" / "ov_big.dat", work.makePath("ov_big.dat"),
                fs::copy_options::overwrite_existing);
  std::istringstream in("OVER ov_big.dat\n\n");
  std::ostringstream menuOut;
  (void)app::cli::handleGeometryDesignCore(session, in, menuOut);
  const fs::path target = work.makePath("save.dat");
  std::ostringstream saveOut;
  const auto status = app::cli::handleSaveCore(
      session, saveOut, target.string(), true, {}, false, {}, false);
  CHECK(status == app::cli::SaveCoreStatus::written);
  std::ifstream actualFile(target, std::ios::binary);
  std::ostringstream actual;
  actual << actualFile.rdbuf();
  const std::string expected = readFile(dir / "generated" / "out.dat");
  if (actual.str() != expected) {
    std::cerr << "\n=== gdes-over-state-unchanged SAVE ===\n";
  }
  CHECK(actual.str() == expected);
}

TEST(unit, CoreGdesExecSuccessMatchesFortran) {
  checkAgainstReference("design-state/gdes-exec-success");
}

TEST(unit, CoreGdesExecOldCapacityCaseNowAccepted) {
  // The 6.99 reference rejects the 399-point buffer at IQX-5=365. With S01
  // the same case is inside the new ABCOPY limit, so the old capacity
  // message must no longer appear.
  std::ostringstream setupOut;
  std::string actual;
  runHandlerCase("design-state/gdes-exec-capacity", setupOut, actual);
  CHECK(!actual.empty());
  CHECK(actual.find("Maximum number of panel nodes") == std::string::npos);
  CHECK(actual.find("Current airfoil cannot be set.") == std::string::npos);
}

}  // namespace

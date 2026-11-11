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

// file: tests/unit/CoreCamberMenuHandlerTests.cpp
// Byte-exact verification of the CAMB menu handlers
// (handleCamberDesignCore, xtcam.f) against the
// design-state Fortran references. The reference files are
// read-only.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
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

// reference segment: from the CAMB entry text 'Enabling camber,thickness
// plotting' (which appears on the same line as the GDES prompt) up to
// immediately before the first TOP prompt after the GDES exit. The
// LGSYM message 'Disabling symmetry enforcement.' precedes the entry
// text on the GDES prompt line and is checked separately.
std::string referenceCamberSegment(const std::string& reference) {
  std::size_t begin = reference.find("Disabling symmetry enforcement.");
  if (begin == std::string::npos) {
    begin = reference.find("Enabling camber,thickness plotting");
  }
  if (begin == std::string::npos) {
    return {};
  }
  // the GDES prompt after the CAMB exit bounds the segment to the CAMB
  // menu portion.
  std::size_t end = reference.find("\n.GDES   c>", begin);
  if (end == std::string::npos) {
    end = reference.find("\n XFOIL", begin);
  }
  if (end == std::string::npos) {
    return reference.substr(begin);
  }
  return reference.substr(begin, end - begin);
}

// Build the GDES setup run (NACA/PANE/SYMM/ADEG) through the existing
// core handlers, then run the CAMB handler on the menu lines following
// 'CAMB'.
void runCamberCase(const std::string& caseId, std::ostringstream& setupOut,
                   std::ostringstream& out,
                   app::cli::CamberDesignCoreStatus& status) {
  const fs::path dir = fs::path("tests/fortran-reference/cases") / caseId;
  const std::vector<std::string> lines = readLines(dir / "commands.in");

  session::SessionComposition session;
  std::size_t cambIndex = 0;
  bool designInitialized = false;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (lines[i] == "CAMB") {
      cambIndex = i;
      break;
    }
    if (lines[i].rfind("NACA ", 0) == 0) {
      testing::handleLegacyNacaCore(session, setupOut,
                               std::stoi(lines[i].substr(5)));
    } else if (lines[i] == "PANE") {
      (void)app::cli::handlePaneCore(session, setupOut);
    } else if (lines[i] == "GDES") {
      (void)session::initializeGeometryDesign(session);
      designInitialized = true;
    } else if (lines[i] == "SYMM") {
      (void)session::gdesToggleSymmetry(session);
    } else if (lines[i].rfind("ADEG ", 0) == 0) {
      const double deg = std::stod(lines[i].substr(5));
      (void)session::gdesRotate(session, deg * 3.14159265358979323846 / 180.0);
    }
  }
  if (!designInitialized) {
    (void)session::initializeGeometryDesign(session);
  }

  std::ostringstream menuInput;
  for (std::size_t i = cambIndex + 1; i < lines.size(); ++i) {
    if (lines[i] == "QUIT") {
      break;
    }
    menuInput << lines[i] << "\n";
  }
  std::istringstream in(menuInput.str());
  status = app::cli::handleCamberDesignCore(session, in, out);
}

void checkAgainstReference(const std::string& caseId) {
  std::ostringstream setupOut;
  std::ostringstream out;
  app::cli::CamberDesignCoreStatus status =
      app::cli::CamberDesignCoreStatus::returned;
  runCamberCase(caseId, setupOut, out, status);
  const fs::path dir = fs::path("tests/fortran-reference/cases") / caseId;
  const std::string expected =
      referenceCamberSegment(readFile(dir / "stdout.raw.txt"));
  std::string actual = out.str();
  // the reference entry starts on the GDES prompt line directly after
  // the prompt; the handler writes the entry text with a leading
  // WRITE(*,*) blank line and a space.
  if (actual.rfind(" " + expected, 0) == 0) {
    actual = actual.substr(1);
  }
  if (actual != expected) {
    std::cerr << "\n=== " << caseId << " ===\n--- expected ---\n"
              << expected << "\n--- actual ---\n" << actual
              << "\n--- end ---\n";
  }
  CHECK(actual == expected);
}

// file/use-case runs: RDAC reads from tests/fortran-reference/cases/
// <case>/input/, while WRTT/WRTC write into a temporary case directory.
// the reference file bytes live under generated/ of the case and are
// only read; there is no persistent /tmp dependency (the directory is
// removed at the end).
struct CamberFileCaseRun {
  std::string out;
  session::SessionComposition session;
  // RAII-owned, unique test directory per test run; workDir is only a
  // convenient path copy for the existing comparisons.
  std::unique_ptr<testing::ScopedTempDirectory> work;
  fs::path workDir;
  std::map<std::string, std::string> writtenFiles;  // Token -> path
};

CamberFileCaseRun runCamberFileCase(const std::string& caseId) {
  const fs::path dir = fs::absolute(
      fs::path("tests/fortran-reference/cases") / caseId);
  CamberFileCaseRun run;
  run.work = std::make_unique<testing::ScopedTempDirectory>(
      "camb-" + caseId.substr(caseId.find('/') + 1));
  run.workDir = run.work->path();

  // reference output token (generated/) and input token (input/).
  std::map<std::string, std::string> substitute;
  if (fs::exists(dir / "input")) {
    for (const auto& e : fs::directory_iterator(dir / "input")) {
      substitute[e.path().filename().string()] = fs::absolute(e.path()).string();
    }
  }
  if (fs::exists(dir / "generated")) {
    for (const auto& e : fs::directory_iterator(dir / "generated")) {
      const std::string token = e.path().filename().string();
      const fs::path target = run.workDir / token;
      // Prepare a reusable template (overwrite cases).
      if (fs::exists(dir / "input" / token)) {
        fs::copy_file(dir / "input" / token, target,
                      fs::copy_options::overwrite_existing);
      }
      substitute[token] = target.string();
      run.writtenFiles[token] = target.string();
    }
  }

  const std::vector<std::string> lines = readLines(dir / "commands.in");
  std::size_t cambIndex = 0;
  bool designInitialized = false;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (lines[i] == "CAMB") {
      cambIndex = i;
      break;
    }
    if (lines[i].rfind("NACA ", 0) == 0) {
      std::ostringstream setupOut;
      testing::handleLegacyNacaCore(run.session, setupOut,
                               std::stoi(lines[i].substr(5)));
    } else if (lines[i] == "GDES") {
      (void)session::initializeGeometryDesign(run.session);
      designInitialized = true;
    } else if (lines[i].rfind("NAME ", 0) == 0) {
      session::setGeometryName(run.session, lines[i].substr(5));
    }
  }
  if (!designInitialized) {
    (void)session::initializeGeometryDesign(run.session);
  }

  std::ostringstream menuInput;
  for (std::size_t i = cambIndex + 1; i < lines.size(); ++i) {
    if (lines[i] == "QUIT") {
      break;
    }
    std::string line = lines[i];
    auto exact = substitute.find(line);
    if (exact != substitute.end()) {
      line = exact->second;
    } else if (line.rfind("WRTT ", 0) == 0 || line.rfind("WRTC ", 0) == 0) {
      const std::size_t sp = line.find(' ');
      const std::string token = line.substr(sp + 1);
      auto sub = substitute.find(token);
      if (sub != substitute.end()) {
        line = line.substr(0, sp + 1) + sub->second;
      }
    }
    menuInput << line << "\n";
  }
  std::istringstream in(menuInput.str());
  std::ostringstream out;
  (void)app::cli::handleCamberDesignCore(run.session, in, out);
  run.out = out.str();
  return run;
}

void checkFileCaseAgainstReference(const std::string& caseId) {
  const fs::path dir = fs::path("tests/fortran-reference/cases") / caseId;
  CamberFileCaseRun run = runCamberFileCase(caseId);
  const std::string expected =
      referenceCamberSegment(readFile(dir / "stdout.raw.txt"));
  std::string actual = run.out;
  if (actual.rfind(" " + expected, 0) == 0) {
    actual = actual.substr(1);
  }
  if (actual != expected) {
    std::cerr << "\n=== " << caseId << " ===\n--- expected ---\n"
              << expected << "\n--- actual ---\n" << actual
              << "\n--- end ---\n";
  }
  CHECK(actual == expected);
  for (const auto& token : run.writtenFiles) {
    const fs::path ref = dir / "generated" / token.first;
    if (!fs::exists(ref)) {
      continue;  // unchanged file (overwrite rejection)
    }
    const std::string refBytes = readFile(ref);
    const std::string gotBytes =
        fs::exists(token.second) ? readFile(token.second) : std::string("<missing>");
    if (gotBytes != refBytes) {
      std::cerr << "\n=== " << caseId << " file " << token.first
                << " differs ===\n";
    }
    CHECK(gotBytes == refBytes);
  }
  fs::remove_all(run.workDir);
}

}  // namespace

TEST(unit, CoreCamberMenuEntryExitMatchesFortran) {
  checkAgainstReference("design-state/camb-entry-exit");
}

TEST(unit, CoreCamberMenuRotationWarningMatchesFortran) {
  // Byte-exact including the list-directed AROT line: the core now
  // derives the buffer metrics (SBLE) for the GDES entry canonically
  // from the buffer (SETGEOM chain), not from the panel.
  checkAgainstReference("design-state/camb-rotation-warning");
}

TEST(unit, CoreCamberMenuSymmetryDisableMatchesFortran) {
  checkAgainstReference("design-state/camb-symmetry-disable");
}

TEST(unit, CoreCamberMenuTfacMatchesFortran) {
  checkAgainstReference("design-state/camb-tfac");
}

TEST(unit, CoreCamberMenuTsetMatchesFortran) {
  checkAgainstReference("design-state/camb-tset");
}

TEST(unit, CoreCamberMenuHighMatchesFortran) {
  checkAgainstReference("design-state/camb-high");
}

TEST(unit, CoreCamberMenuSetcMatchesFortran) {
  checkAgainstReference("design-state/camb-setc");
}

TEST(unit, CoreCamberMenuInpcMatchesFortran) {
  checkAgainstReference("design-state/camb-inpc");
}

TEST(unit, CoreCamberMenuInppMatchesFortran) {
  checkAgainstReference("design-state/camb-inpp");
}

TEST(unit, CoreCamberMenuClrMatchesFortran) {
  checkAgainstReference("design-state/camb-clr");
}

TEST(unit, CoreCamberMenuScalLoadingMatchesFortran) {
  checkAgainstReference("design-state/camb-scal-loading");
}

TEST(unit, CoreCamberMenuScalCamberOnlyMatchesFortran) {
  checkAgainstReference("design-state/camb-scal-camber-only");
}

TEST(unit, CoreCamberMenuRedoScalMatchesFortran) {
  checkAgainstReference("design-state/camb-redo-scal");
}

TEST(unit, CoreCamberMenuRedoInvalidMatchesFortran) {
  checkAgainstReference("design-state/camb-redo-invalid");
}

TEST(unit, CoreCamberMenuAddMatchesFortran) {
  checkAgainstReference("design-state/camb-add");
}

TEST(unit, CoreCamberMenuDcplMatchesFortran) {
  checkAgainstReference("design-state/camb-dcpl");
}

TEST(unit, CoreCamberMenuSlopMatchesFortran) {
  checkAgainstReference("design-state/camb-slop");
}

TEST(unit, CoreCamberMenuUnknownMatchesFortran) {
  checkAgainstReference("design-state/camb-unknown");
}

TEST(unit, CoreCamberMenuUnavailableMatchesFortran) {
  checkAgainstReference("design-state/camb-unavailable");
}

TEST(unit, CoreCamberMenuFileMatchesFortran) {
  checkAgainstReference("design-state/camb-file-pending");
}

TEST(unit, CoreCamberMenuAddPanelUnchangedMatchesFortran) {
  checkAgainstReference("design-state/camb-add-panel-unchanged");
}

TEST(unit, CoreCamberMenuEndOfFileIsControlled) {
  session::SessionComposition session;
  std::ostringstream out;
  testing::handleLegacyNacaCore(session, out, 1212);
  (void)session::initializeGeometryDesign(session);
  std::istringstream in("");
  const auto status = app::cli::handleCamberDesignCore(session, in, out);
  CHECK(status == app::cli::CamberDesignCoreStatus::endOfFile);
}

TEST(unit, CoreCamberEntryExitLgparmRoundTrip) {
  session::SessionComposition session;
  std::ostringstream out;
  testing::handleLegacyNacaCore(session, out, 1212);
  (void)session::initializeGeometryDesign(session);
  session.design.lgParm = false;  // arbitrary initial value
  const auto entry = session::initializeCamberDesign(session);
  CHECK(entry.enablingCamberPlot);
  CHECK(session.design.lgParmSave == false);
  CHECK(session.design.lgParm == true);
  CHECK(session.design.lpLcam == true);
  CHECK(session.design.camber.has_value());
  CHECK(session.design.camber->ncam == 201u);
  CHECK(session.design.camber->ncadd == 2u);
  CHECK(session.design.camber->npadd == 2u);
  session::leaveCamberDesign(session);
  CHECK(session.design.lgParm == false);
  CHECK(session.design.lpLcam == true);
}

TEST(unit, CoreCamberAddMutatesBufferNotPanel) {
  session::SessionComposition session;
  std::ostringstream out;
  testing::handleLegacyNacaCore(session, out, 2412);
  (void)app::cli::handlePaneCore(session, out);
  const std::vector<double> panelBefore = session.paneling.panel.x;
  const std::vector<double> bufferBefore = session.geometry.buffer.y;
  (void)session::initializeGeometryDesign(session);
  (void)session::initializeCamberDesign(session);
  session::camberSetCamberline(session);
  const auto add = session::camberAdd(session);
  CHECK(add.isOk());
  CHECK(session.geometry.buffer.y != bufferBefore);
  CHECK(session.paneling.panel.x == panelBefore);
  CHECK(session.geometry.bufferRevision > 0u);
}

TEST(unit, CoreCamberSessionsAreIndependent) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  testing::handleLegacyNacaCore(a, out, 1212);
  testing::handleLegacyNacaCore(b, out, 2412);
  (void)session::initializeGeometryDesign(a);
  (void)session::initializeGeometryDesign(b);
  (void)session::initializeCamberDesign(a);
  (void)session::initializeCamberDesign(b);
  session::camberSetCamberline(a);
  CHECK(a.design.camber->ncadd > 2u);
  CHECK(b.design.camber->ncadd == 2u);
}

TEST(unit, CoreCamberMenuIsDeterministic) {
  std::ostringstream setup1;
  std::ostringstream setup2;
  std::ostringstream first;
  std::ostringstream second;
  app::cli::CamberDesignCoreStatus s1 =
      app::cli::CamberDesignCoreStatus::returned;
  app::cli::CamberDesignCoreStatus s2 =
      app::cli::CamberDesignCoreStatus::returned;
  runCamberCase("design-state/camb-inpp", setup1, first, s1);
  runCamberCase("design-state/camb-inpp", setup2, second, s2);
  CHECK(first.str() == second.str());
}

// ---: CAMB-file-I/O RDAC/WRTT/WRTC -----------------------------

TEST(unit, CoreCamberMenuRdacNormalMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-rdac-normal");
}

TEST(unit, CoreCamberMenuRdacOpenErrorMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-rdac-open-error");
}

TEST(unit, CoreCamberMenuRdacNoPointsMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-rdac-no-points");
}

TEST(unit, CoreCamberMenuRdacParseErrorMatchesFortran) {
  // Points parsed before the parse error are adopted canonically.
  checkFileCaseAgainstReference("design-state/camb-rdac-parse-error");
}

TEST(unit, CoreCamberMenuRdacXOffRangeMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-rdac-x-offrange");
}

TEST(unit, CoreCamberMenuWrttNormalMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-wrtt-normal");
}

TEST(unit, CoreCamberMenuWrtcNormalMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-wrtc-normal");
  // WRTC ends after the second value: no third Column.
  CamberFileCaseRun run = runCamberFileCase("design-state/camb-wrtc-normal");
  const fs::path file = run.workDir / "c.dat";
  // PUB-W2: the reader lives in its own scope so the file is closed
  // before the cleanup below (Windows rejects deleting open files).
  {
    std::ifstream in(file, std::ios::binary);
    std::string line;
    std::getline(in, line);  // Header
    std::getline(in, line);  // first data line
    CHECK(line.size() == 31);  // 1X + 2x G15.7
  }
  fs::remove_all(run.workDir);
}

TEST(unit, CoreCamberMenuWrtcFileIsReleasableImmediately) {
  // PUB-W2: the file produced by WRTC carries no open handle - it can
  // be renamed (and thus deleted) right after the operation returns,
  // without any retry.
  CamberFileCaseRun run = runCamberFileCase("design-state/camb-wrtc-normal");
  const fs::path file = run.workDir / "c.dat";
  std::error_code error;
  fs::rename(file, run.workDir / "c.dat.moved", error);
  CHECK(!error);
  CHECK(fs::exists(run.workDir / "c.dat.moved"));
  fs::remove_all(run.workDir);
}

TEST(unit, CoreCamberMenuWrttOverwriteRejectMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-wrtt-overwrite-reject");
}

TEST(unit, CoreCamberMenuWrttOverwriteAcceptMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-wrtt-overwrite-accept");
}

TEST(unit, CoreCamberMenuWrtcNameMatchesFortran) {
  checkFileCaseAgainstReference("design-state/camb-wrtc-name");
}

TEST(unit, CoreCamberMenuScalWrttWrtcMatchesFortran) {
  // SCAL scales only the added camber/loading; the buffer distribution
 // written by WRTT/WRTC stays identical (SCAL consumer check).
  checkFileCaseAgainstReference("design-state/camb-scal-wrtt-wrtc");
  CamberFileCaseRun run = runCamberFileCase("design-state/camb-scal-wrtt-wrtc");
  const std::string refT = readFile(
      "tests/fortran-reference/cases/design-state/camb-wrtt-normal/generated/t.dat");
  const std::string refC = readFile(
      "tests/fortran-reference/cases/design-state/camb-wrtc-normal/generated/c.dat");
  CHECK(readFile(run.workDir / "sc.dat") == refT);
  CHECK(readFile(run.workDir / "sc2.dat") == refC);
  fs::remove_all(run.workDir);
}

TEST(unit, CoreCamberMenuWrttWrtcDoNotMutateSession) {
  CamberFileCaseRun run = runCamberFileCase("design-state/camb-wrtt-normal");
  const auto& camber = *run.session.design.camber;
  CHECK(run.session.geometry.bufferRevision == 1);
  CHECK(camber.ncadd == 2 && camber.ycadd[0] == 0.0 && camber.ycadd[1] == 0.0);
  CHECK(camber.ncam == 201);
  fs::remove_all(run.workDir);
}

TEST(unit, CoreCamberMenuRdacMutatesCamberState) {
  // without an ADD consumer: the RDAC state remains as is.
  CamberFileCaseRun run = runCamberFileCase("design-state/camb-rdac-x-offrange");
  const auto& camber = *run.session.design.camber;
  CHECK(camber.ncadd == 2);
  CHECK(camber.ycadd[1] != 0.0);
  CHECK(run.session.design.ldCplot == false);
  fs::remove_all(run.workDir);
}

TEST(unit, CoreCamberMenuRdacNoPointsResetsCamber) {
  CamberFileCaseRun run = runCamberFileCase("design-state/camb-rdac-no-points");
  const auto& camber = *run.session.design.camber;
  CHECK(camber.ncadd == 2);
  CHECK(camber.ycadd[0] == 0.0 && camber.ycadd[1] == 0.0);
  fs::remove_all(run.workDir);
}

TEST(unit, CoreCamberMenuScalLoadingConsumerMatchesFortran) {
 // SCAL consumer classification (gate): for NCADD < NPADD, Fortran
  // scales YPADD only up to NCADD; the next INPP returns the canonical
  // integral values 0.768/-0.064 (oracle fortran-bin/xfoil).
  session::SessionComposition s;
  std::ostringstream setup;
  testing::handleLegacyNacaCore(s, setup, 12);
  (void)session::initializeGeometryDesign(s);
  const std::string menu =
      "INPP\n0.2 0.5\n0.8 0.5\n\n"
      "SCAL 2\n"
      "INPC\n0.3 0.1\n0.7 0.1\n\n"
      "SCAL 2\n"
      "INPP\n0.5 0.2\n\n"
      "\n";
  std::istringstream in(menu);
  std::ostringstream out;
  (void)app::cli::handleCamberDesignCore(s, in, out);
  CHECK(out.str().find(
            " Delta Cp loading gives delta CL =   0.768\n"
            "                        delta CM =  -0.064\n") !=
        std::string::npos);
}

TEST(unit, CoreCamberMenuFileCasesAreDeterministic) {
  CamberFileCaseRun a = runCamberFileCase("design-state/camb-rdac-normal");
  CamberFileCaseRun b = runCamberFileCase("design-state/camb-rdac-normal");
  CHECK(a.out == b.out);
  fs::remove_all(a.workDir);
  fs::remove_all(b.workDir);
}

// end: tests/unit/CoreCamberMenuHandlerTests.cpp

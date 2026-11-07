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

// file: tests/integration/NativeBatchTests.cpp
// execution matrix of the native batch mode.
// All cases start the product program as a real process
// (XFOIL_CLI_PATH fallback build/debug/xfoil.next) in a unique,
// empty temporary directory. Exit codes, stderr diagnostics,
// transcript/results file contents and produced artifacts are compared.
// Re-parsing stdout to derive exit codes does not take place.

#include "TestSupport.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string productProgramPath() {
  const char* configuredPath = std::getenv("XFOIL_CLI_PATH");
  return configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";
}

std::string absoluteProductProgramPath() {
  std::error_code error;
  const std::string absolute =
      std::filesystem::absolute(productProgramPath(), error).string();
  return error ? productProgramPath() : absolute;
}

bool writeFile(const std::filesystem::path& path, const std::string& text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
  return file.good();
}

bool copySample(const std::filesystem::path& directory,
                const std::string& name) {
  std::error_code error;
  std::filesystem::create_directory(directory / "sample", error);
  std::filesystem::copy_file("sample/" + name, directory / "sample" / name,
                             std::filesystem::copy_options::none, error);
  return !error;
}

bool fileExists(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(path, error);
}

std::string readFile(const std::filesystem::path& path) {
  std::string text;
  testing::loadTextFile(path.string(), text);
  return text;
}

struct BatchRun {
  bool executed{};
  int exitCode{};
  std::string stdoutText;
  std::string stderrText;
  std::string transcriptText;
  std::string resultsText;
};

// Starts the batch process with the temporary directory as the
// working directory (cd in the child; the test keeps the start directory,
// so that the relative reference paths sample/ and tests/ stay valid). jobText
// is written as job.xfb; extraArgs adds further CLI options.
BatchRun runBatch(const testing::ScopedTempDirectory& work,
                  const std::string& jobText,
                  const std::string& extraArgs = std::string{}) {
  BatchRun run;
  if (!writeFile(work.makePath("job.xfb"), jobText)) {
    testing::reportFailure(__FILE__, __LINE__, "job.xfb not writable");
    return run;
  }
  // PUB-W2: central portable process helper. extraArgs are
  // whitespace-separated simple tokens (flags and space-free paths).
  testing::ProcessInvocation invocation;
  invocation.program = absoluteProductProgramPath();
  invocation.workingDirectory = work.path().string();
  invocation.arguments = {"--batch", "job.xfb"};
  {
    std::istringstream argumentStream(extraArgs);
    std::string token;
    while (argumentStream >> token) {
      invocation.arguments.push_back(token);
    }
  }
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  const int exitCode = testing::runProcess(invocation);
  if (exitCode == -1) {
    testing::reportFailure(__FILE__, __LINE__, "batch process not runnable");
    return run;
  }
  run.executed = true;
  run.exitCode = exitCode;
  // Captured console output: canonicalize CRLF -> LF (PUB-W2). The
  // transcript/results files keep their byte contract.
  run.stdoutText =
      testing::canonicalizeCapturedText(readFile(work.makePath("stdout.txt")));
  run.stderrText =
      testing::canonicalizeCapturedText(readFile(work.makePath("stderr.txt")));
  run.transcriptText = readFile(work.makePath("transcript.txt"));
  run.resultsText = readFile(work.makePath("results.dat"));
  return run;
}

// Counts data lines (first column integer) in results.dat.
std::size_t countResultDataLines(const std::string& results) {
  std::size_t count = 0;
  std::size_t position = results.find('\n');
  while (position != std::string::npos) {
    const std::size_t next = results.find('\n', position + 1);
    const std::string line = results.substr(
        position + 1,
        next == std::string::npos ? std::string::npos : next - position - 1);
    if (!line.empty() && line[0] >= '0' && line[0] <= '9') {
      ++count;
    }
    if (next == std::string::npos) {
      break;
    }
    position = next;
  }
  return count;
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

const char* const kResultsHeader = "# XFOIL_BATCH_RESULTS 1\n";
const char* const kResultsColumns =
    "# index alpha cl cd cdp cm re mach ncrit_top ncrit_bottom"
    " xtr_top xtr_bottom converged iterations\n";

std::string minimalJob(const std::string& commands) {
  return "XFOIL-BATCH 1\n[commands]\n" + commands;
}

}  // namespace

// ---: execution matrix (execution) -----------------------------

TEST(batch, NacaInviscidPointRunsAndWritesOneResultLine) {
  const testing::ScopedTempDirectory work("naca-point");
  const auto run = runBatch(work, minimalJob("NACA 2412\nPANE\nOPER\nALFA 3\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(run.stderrText, std::string{});
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{1});
  CHECK(contains(run.resultsText, "1 3.000000"));
  CHECK(contains(run.resultsText, " 1 0\n"));  // converged, 0 iterations
}

TEST(batch, ViscousPointRuns) {
  const testing::ScopedTempDirectory work("viscous-point");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nPANE\nOPER\nVISC 100000\nALFA 3\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{1});
  CHECK(contains(run.resultsText, "100000.000000"));
}

TEST(batch, AseqShortSequenceWritesOneLinePerPoint) {
  const testing::ScopedTempDirectory work("aseq-short");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nPANE\nOPER\nVISC 100000\nASEQ 2 4 1\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{3});
}

TEST(batch, PaccPnamPsumSaveDumpRoundtrip) {
  const testing::ScopedTempDirectory work("pacc-roundtrip");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nPANE\nOPER\nPACC\npolar_save.txt\npolar_dump.txt\n"
                 "ALFA 3\nPACC\nPNAM\nTestpolar\nPSUM\nPWRT\npolar_out.txt\n"
                 "\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.makePath("polar_save.txt")));
  CHECK(fileExists(work.makePath("polar_dump.txt")));
  CHECK(fileExists(work.makePath("polar_out.txt")));
  CHECK(contains(run.transcriptText, "Polar accumulation enabled"));
  CHECK(contains(run.transcriptText, "Testpolar"));
}

TEST(batch, GdesCycleRunsCleanly) {
  const testing::ScopedTempDirectory work("gdes-cycle");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nPANE\nGDES\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.transcriptText, "Geometry design routine"));
}

TEST(batch, MdesQdesShortMenuPathRuns) {
  const testing::ScopedTempDirectory work("mdes-qdes-short");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nPANE\nMDES\n\nQDES\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
}

TEST(batch, VparSubmenuCommandsApply) {
  const testing::ScopedTempDirectory work("vpar-submenu");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nPANE\nOPER\nVPAR\nN 9\nXTR 1 1\nVACC 0.01\nREST\n"
                 "\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.transcriptText, "not recognized") == false);
}

TEST(batch, IntxHalfCommandsRun) {
  const testing::ScopedTempDirectory work("intx-half");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nPANE\nINTX\nHALF\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.transcriptText, "not recognized") == false);
}

TEST(batch, SaveLoadFileRoundtrip) {
  const testing::ScopedTempDirectory work("save-load");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nSAVE foil_save.dat\nLOAD foil_save.dat\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.makePath("foil_save.dat")));
  CHECK(contains(run.transcriptText, "Number of input coordinate points: 160"));
}

TEST(batch, PlotCommandDefaultReportsUnavailableAndExitsZero) {
  const testing::ScopedTempDirectory work("plot-default");
  const auto run = runBatch(work, minimalJob("NACA 2412\nPANE\nZ\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.transcriptText,
                 "Plotting is not available in this build."));
}

TEST(batch, PlotCommandDisallowedExitsFour) {
  const testing::ScopedTempDirectory work("plot-disallowed");
  const auto run = runBatch(
      work,
      "XFOIL-BATCH 1\n[job]\nallow-unavailable-plot-commands = false\n"
      "[commands]\nNACA 2412\nPANE\nZ\nQUIT\n");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 4);
}

TEST(batch, UnknownCommandDefaultStopsWithExitFour) {
  const testing::ScopedTempDirectory work("unknown-default");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nFOOBAR\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 4);
  CHECK(contains(run.transcriptText, "FOOB command not recognized"));
  // stop-on-error=true (default): QUIT is not executed.
  CHECK(contains(run.transcriptText, "FOOB") == true);
}

TEST(batch, UnknownCommandContinueStillExitsFour) {
  const testing::ScopedTempDirectory work("unknown-continue");
  const auto run = runBatch(
      work,
      "XFOIL-BATCH 1\n[job]\nstop-on-error = false\n[commands]\n"
      "NACA 2412\nFOOBAR\nPANE\nQUIT\n");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 4);
  // Continuation: PANE was still executed.
  CHECK(contains(run.transcriptText, "Paneling parameters used"));
}

TEST(batch, OperWithoutGeometryUsesCanonicalGuardNotSolverExit) {
  // the OPER entry checks the geometry before the menu (canonical
  // message, no menu entry); the job ends at the geometry guard
  // (exit 4). Exit 5 stays reserved for kernel solver errors.
  const testing::ScopedTempDirectory work("oper-guard");
  const auto run = runBatch(work, minimalJob("OPER\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 4);
  CHECK(contains(run.transcriptText, "***  No airfoil available  ***"));
}

TEST(batch, NonConvergenceDefaultExitsZero) {
  const testing::ScopedTempDirectory work("nonconv-default");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nPANE\nOPER\nVISC 100000\nITER 1\nALFA 12\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{1});
  CHECK(contains(run.resultsText, " 0 1\n"));  // converged=0, 1 iteration
}

TEST(batch, NonConvergenceStopExitsSix) {
  const testing::ScopedTempDirectory work("nonconv-stop");
  const auto run = runBatch(
      work,
      "XFOIL-BATCH 1\n[job]\nstop-on-nonconvergence = true\n[commands]\n"
      "NACA 2412\nPANE\nOPER\nVISC 100000\nITER 1\nALFA 12\n\nQUIT\n");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 6);
}

TEST(batch, QuitIgnoresFollowingCommands) {
  const testing::ScopedTempDirectory work("quit-ignores");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nQUIT\nFOOBAR\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.transcriptText, "not recognized") == false);
}

TEST(batch, EofWithoutQuitExitsZero) {
  const testing::ScopedTempDirectory work("eof-no-quit");
  const auto run = runBatch(work, minimalJob("NACA 2412\nPANE\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
}

TEST(batch, WdefRdefCanonicalRoundtrip) {
  const testing::ScopedTempDirectory work("wdef-rdef");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nWDEF test.def\nRDEF test.def\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.makePath("test.def")));
  const std::string def = readFile(work.makePath("test.def"));
  std::size_t lines = 0;
  for (const char symbol : def) {
    if (symbol == '\n') {
      ++lines;
    }
  }
  CHECK_EQ(lines, std::size_t{14});  // canonical 14-line contract
  CHECK(contains(run.transcriptText,
                 "Default parameters read in from file"));
}

TEST(batch, RdefEofExitsTwo) {
  const testing::ScopedTempDirectory work("rdef-eof");
  // valid but prematurely ended xfoil.def (7 of 14 lines):
  // canonical Fortran EOF abort (exit 2), not the ERR branch.
  CHECK(writeFile(work.makePath("part.def"),
                  "160       1.0000   0.1500   0.2000 | Npan    PPanel  "
                  "TErat  REFrat\n"
                  "    1.0000   1.0000   1.0000   1.0000 | XrefS1  XrefS2  "
                  "XrefP1 XrefP2\n"
                  "   10.0000   0.5500   0.0150   0.8000 | Size    plotAR  "
                  "CHsize ScrnFr\n"
                  "   11.0000   8.5000   0.0000   0.0000 | Xpage   Ypage   "
                  "Xmargn Ymargn\n"
                  "  T        T                          | Lcolor  Lcursor\n"
                  "    1.0000  -2.0000  -0.5000          | CPmax   CPmin   "
                  "CPdel\n"
                  "    0.0900   0.7000   0.0200          | XoffAir ScalAir "
                  "BLUwt\n"));
  const auto run = runBatch(work, minimalJob("RDEF part.def\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 2);
  CHECK(contains(run.transcriptText, "Fortran runtime error: End of file"));
}

TEST(batch, StartupGetdefSuppressedInBatch) {
  const testing::ScopedTempDirectory work("getdef-suppressed");
  const auto run = runBatch(work, minimalJob("QUIT\n"));
  CHECK(run.executed);
  // geometry error exit 4, but no startup GETDEF prompt and no
  // xfoil.def note in the batch stdout.
  CHECK_EQ(run.exitCode, 4);
  CHECK(contains(run.stdoutText, "Read settings") == false);
  CHECK(contains(run.stdoutText, "xfoil.def") == false);
}

TEST(batch, JobWithoutGeometryExitsFour) {
  const testing::ScopedTempDirectory work("no-geometry");
  const auto run = runBatch(work, minimalJob("QUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 4);
  CHECK_EQ(run.stderrText, "No airfoil geometry established.\n");
  // results.dat is still written: header without data lines.
  CHECK(contains(run.resultsText, kResultsHeader));
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{0});
}

TEST(batch, OnlyOperAttemptWithoutGeometryExitsFour) {
  const testing::ScopedTempDirectory work("oper-only-no-geometry");
  const auto run = runBatch(work, minimalJob("OPER\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 4);
}

TEST(batch, ProfileStartWithoutLoadEstablishesGeometry) {
  const testing::ScopedTempDirectory work("profile-start");
  CHECK(copySample(work.path(), "CLARK_Y.dat"));
  const auto run = runBatch(
      work, minimalJob("QUIT\n"),
      "--profile sample/CLARK_Y.dat");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.transcriptText, "Loading profile file"));
}

TEST(batch, ProfileThenLoadOverrideUsesLastGeometry) {
  const testing::ScopedTempDirectory work("profile-load-override");
  CHECK(copySample(work.path(), "CLARK_Y.dat"));
  CHECK(copySample(work.path(), "NACA2312.dat"));
  const auto run = runBatch(
      work, minimalJob("LOAD sample/NACA2312.dat\nQUIT\n"),
      "--profile sample/CLARK_Y.dat");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.resultsText, "source=\"command:LOAD\""));
}

// --- Prefix and output files ----------------------------

TEST(batch, PrefixAppliesToRunnerFiles) {
  const testing::ScopedTempDirectory work("prefix-runner");
  const auto run = runBatch(work, minimalJob("NACA 2412\nQUIT\n"),
                            "--prefix cand_");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.makePath("cand_transcript.txt")));
  CHECK(fileExists(work.makePath("cand_results.dat")));
  CHECK(!fileExists(work.makePath("transcript.txt")));
  CHECK(!fileExists(work.makePath("results.dat")));
}

TEST(batch, PrefixAppliesToNativeOutputFiles) {
  const testing::ScopedTempDirectory work("prefix-native");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nPANE\nSAVE foil.dat\nWDEF set.def\nOPER\nPACC\n"
                 "ps.txt\npd.txt\nALFA 3\nPACC\nPWRT\npw.txt\n\nQUIT\n"),
      "--prefix P_");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.makePath("P_foil.dat")));
  CHECK(fileExists(work.makePath("P_set.def")));
  CHECK(fileExists(work.makePath("P_ps.txt")));
  CHECK(fileExists(work.makePath("P_pd.txt")));
  CHECK(fileExists(work.makePath("P_pw.txt")));
  CHECK(!fileExists(work.makePath("foil.dat")));
}

TEST(batch, PrefixNotAppliedToInputPaths) {
  const testing::ScopedTempDirectory work("prefix-input");
  CHECK(copySample(work.path(), "CLARK_Y.dat"));
  const auto run = runBatch(
      work, minimalJob("LOAD sample/CLARK_Y.dat\nWDEF in.def\nRDEF in.def\nQUIT\n"),
      "--prefix X_");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  // Input paths stay un-prefixed: RDEF looks for the un-prefixed
  // in.def (not present -> canonical not-found message), while
  // WDEF writes the prefixed X_in.def as output.
  CHECK(fileExists(work.makePath("X_in.def")));
  CHECK(!fileExists(work.makePath("in.def")));
  CHECK(contains(readFile(work.makePath("X_transcript.txt")), "not found"));
}

TEST(batch, AbsolutePathGetsPrefixInSameDirectory) {
  const testing::ScopedTempDirectory work("prefix-absolute");
  const std::string absoluteOut =
      (work.path() / "abs_foil.dat").string();
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nSAVE " + absoluteOut + "\nQUIT\n"),
      "--prefix A_");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.path() / "A_abs_foil.dat"));
  CHECK(!fileExists(work.path() / "abs_foil.dat"));
}

TEST(batch, ExistingRunnerOutputFileExitsThreeBeforeRun) {
  const testing::ScopedTempDirectory work("existing-output");
  CHECK(writeFile(work.makePath("transcript.txt"), "PRE-EXISTING\n"));
  const auto run = runBatch(work, minimalJob("NACA 2412\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 3);
  CHECK_EQ(run.stderrText, "Batch output file exists: transcript.txt\n");
  // Precondition: no results.dat, the job was not started.
  CHECK(!fileExists(work.makePath("results.dat")));
  CHECK_EQ(readFile(work.makePath("transcript.txt")), "PRE-EXISTING\n");
}

TEST(batch, TwoSequentialJobsDifferentPrefixesStaySeparate) {
  const testing::ScopedTempDirectory work("two-prefixes");
  const auto first = runBatch(work, minimalJob("NACA 4415\nQUIT\n"),
                              "--prefix one_");
  CHECK(first.executed);
  CHECK_EQ(first.exitCode, 0);
  const auto second = runBatch(work, minimalJob("NACA 0012\nQUIT\n"),
                               "--prefix two_");
  CHECK(second.executed);
  CHECK_EQ(second.exitCode, 0);
  CHECK(fileExists(work.makePath("one_results.dat")));
  CHECK(fileExists(work.makePath("two_results.dat")));
  CHECK(contains(readFile(work.makePath("one_results.dat")),
                 "# geometry name=\"NACA 4415\""));
  CHECK(contains(readFile(work.makePath("two_results.dat")),
                 "# geometry name=\"NACA 0012\""));
}

// ---quiet and transcript --------------------------------

TEST(batch, QuietSuppressesStdoutKeepsTranscript) {
  const testing::ScopedTempDirectory work("quiet");
  const auto loud = runBatch(work, minimalJob("NACA 2412\nQUIT\n"));
  CHECK(loud.executed);
  CHECK_EQ(loud.exitCode, 0);
  CHECK_EQ(loud.stdoutText, loud.transcriptText);
  const testing::ScopedTempDirectory quietWork("quiet-second");
  const auto quiet = runBatch(quietWork, minimalJob("NACA 2412\nQUIT\n"),
                              "--quiet");
  CHECK(quiet.executed);
  CHECK_EQ(quiet.exitCode, 0);
  CHECK_EQ(quiet.stdoutText, std::string{});
  CHECK_EQ(quiet.transcriptText, loud.transcriptText);
}

// --- Result format (/19) --------------------------------------

TEST(batch, ResultsHeaderAndColumnsExact) {
  const testing::ScopedTempDirectory work("results-header");
  const auto run = runBatch(work, minimalJob("NACA 2412\nQUIT\n"));
  CHECK(run.executed);
  CHECK(contains(run.resultsText, kResultsHeader));
  CHECK(contains(run.resultsText, kResultsColumns));
}

TEST(batch, DuplicateAlphaProducesTwoLines) {
  const testing::ScopedTempDirectory work("dup-alpha");
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nPANE\nOPER\nALFA 3\nALFA 3\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{2});
}

TEST(batch, SettingsCommentOnViscChange) {
  const testing::ScopedTempDirectory work("settings-comment");
  const auto run = runBatch(
      work,
      minimalJob("NACA 2412\nPANE\nOPER\nALFA 3\nVISC 100000\nALFA 4\n\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(countResultDataLines(run.resultsText), std::size_t{2});
  CHECK(contains(run.resultsText, "# settings re=0.000000"));
  CHECK(contains(run.resultsText, "# settings re=100000.000000"));
}

TEST(batch, GeometryCommentOnLoadChange) {
  const testing::ScopedTempDirectory work("geometry-comment");
  CHECK(copySample(work.path(), "CLARK_Y.dat"));
  const auto run = runBatch(
      work, minimalJob("NACA 2412\nLOAD sample/CLARK_Y.dat\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(contains(run.resultsText, "# geometry name=\"NACA 2412\""));
  CHECK(contains(run.resultsText, "source=\"command:LOAD\""));
}

// --- paths and Parallelism --------------------------------

TEST(batch, WorkingDirectoryOptionResolvesRelativePaths) {
  const testing::ScopedTempDirectory work("working-dir");
  std::error_code error;
  std::filesystem::create_directory(work.path() / "sub", error);
  CHECK(!error);
  CHECK(copySample(work.path() / "sub", "CLARK_Y.dat"));
  CHECK(writeFile(work.makePath("sub/job.xfb"),
                  minimalJob("LOAD sample/CLARK_Y.dat\nQUIT\n")));
  testing::ProcessInvocation invocation;  // PUB-W2: portable helper
  invocation.program = absoluteProductProgramPath();
  invocation.workingDirectory = work.path().string();
  invocation.arguments = {"--batch", "sub/job.xfb"};
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  CHECK_EQ(testing::runProcess(invocation), 0);
  // default working directory = parent of the job file: artifacts land in sub/.
  CHECK(fileExists(work.path() / "sub" / "transcript.txt"));
  CHECK(fileExists(work.path() / "sub" / "results.dat"));
}

TEST(batch, ExplicitWorkingDirectoryOption) {
  const testing::ScopedTempDirectory work("explicit-working-dir");
  std::error_code error;
  std::filesystem::create_directory(work.path() / "wd", error);
  CHECK(!error);
  CHECK(copySample(work.path() / "wd", "CLARK_Y.dat"));
  const auto run = runBatch(
      work, minimalJob("LOAD sample/CLARK_Y.dat\nQUIT\n"),
      "--working-directory wd");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(fileExists(work.path() / "wd" / "transcript.txt"));
  CHECK(!fileExists(work.makePath("transcript.txt")));
}

TEST(batch, MissingWorkingDirectoryExitsThree) {
  const testing::ScopedTempDirectory work("missing-working-dir");
  const auto run = runBatch(work, minimalJob("NACA 2412\nQUIT\n"),
                            "--working-directory definitely-missing-dir");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 3);
  CHECK_EQ(run.stderrText, "Working directory does not exist: definitely-missing-dir\n");
}

TEST(batch, BatchStdinUsesProcessCwd) {
  const testing::ScopedTempDirectory work("stdin-job");
  CHECK(writeFile(work.makePath("stdin.xfb"), minimalJob("NACA 2412\nQUIT\n")));
  testing::ProcessInvocation invocation;  // PUB-W2: portable helper
  invocation.program = absoluteProductProgramPath();
  invocation.workingDirectory = work.path().string();
  invocation.arguments = {"--batch", "-"};
  invocation.stdinFile = "stdin.xfb";
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  CHECK_EQ(testing::runProcess(invocation), 0);
  CHECK(fileExists(work.makePath("transcript.txt")));
  CHECK(fileExists(work.makePath("results.dat")));
}

TEST(batch, NoArtifactsInRepositoryRootAfterRun) {
  // the golden reference run leaves no artifacts in the
  // process working directory (repository root) when the job
  // directory is a temporary directory.
  const testing::ScopedTempDirectory work("root-clean");
  const auto run = runBatch(work, minimalJob("NACA 2412\nQUIT\n"));
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(!fileExists(std::filesystem::path("transcript.txt")));
  CHECK(!fileExists(std::filesystem::path("results.dat")));
}

TEST(batch, ReferenceJobTripleDeterminism) {
  const testing::ScopedTempDirectory work("triple-determinism");
  const std::string job =
      "XFOIL-BATCH 1\n[commands]\nNACA 4415\nPANE\nOPER\nVISC 100000\n"
      "ASEQ 2 4 1\n\nQUIT\n";
  std::string firstTranscript;
  std::string firstResults;
  int firstExit = -1;
  for (int runIndex = 0; runIndex < 3; ++runIndex) {
    const testing::ScopedTempDirectory runWork("triple-run");
    const auto run = runBatch(runWork, job);
    CHECK(run.executed);
    if (runIndex == 0) {
      firstExit = run.exitCode;
      firstTranscript = run.transcriptText;
      firstResults = run.resultsText;
    } else {
      CHECK_EQ(run.exitCode, firstExit);
      CHECK_EQ(run.transcriptText, firstTranscript);
      CHECK_EQ(run.resultsText, firstResults);
    }
  }
  CHECK_EQ(firstExit, 0);
}

// End: tests/integration/NativeBatchTests.cpp

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

// file: tests/integration/DefaultsCommandsTests.cpp
// integration tests of the defaults commands TOP WDEF / TOP RDEF
// and of the startup GETDEF of xfoil.def (xfoil.f 50, 309-345,
// 867-1030). The in-session paths run with injected streams. Each
// test run keeps its own unique test working directory
//; the relative file names checked below
// build/test-data/ stay unchanged but are
// process-isolated. The startup path depends on the current
// working directory and is checked as a child process in the own
// test working directory. Expected messages follow the
// original formats (formats 1900/1600/1800/701/708) and are
// verified with gfortran.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"


#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

struct SessionResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

SessionResult runSession(const std::string& inputText) {
  SessionResult result;
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
}

void reportFailure(const std::string& message) {
  testing::reportFailure(__FILE__, __LINE__, message);
}

std::string dataPath(const std::string& fileName) {
  return "build/test-data/" + fileName;
}

// own, unique test working directory per test run. It contains
// a "build/test-data" subdirectory, so that the relative file
// names checked by the product contract stay unchanged, but the
// files do not lie in a build path shared by several test
// processes.
void prepareDefaultsWorkspace(const testing::ScopedTempDirectory& work) {
  std::error_code error;
  std::filesystem::create_directories(work.path() / "build" / "test-data",
                                      error);
  if (error) {
    reportFailure("Test working directory not preparable: " +
                  work.path().string());
  }
}

void writeDataFile(const std::string& fileName, const std::string& contents) {
  std::ofstream out(dataPath(fileName));
  out << contents;
}

std::string readDataFile(const std::string& fileName) {
  std::ifstream in(dataPath(fileName));
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void removeDataFile(const std::string& fileName) {
  std::error_code error;
  std::filesystem::remove(dataPath(fileName), error);
}

// Absolute path of the product program; must be resolved before a
// working-directory change, because XFOIL_CLI_PATH may be given
// as a relative path.
std::string programPathAbsolute() {
  const char* const configuredPath = std::getenv("XFOIL_CLI_PATH");
  const std::string program =
      configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";
  std::error_code error;
  const std::string absolute =
      std::filesystem::absolute(program, error).string();
  if (error) {
    reportFailure("Program path not determinable: " + program);
    return {};
  }
  return absolute;
}

// FNAME is CHARACTER*64 (XFOIL.INC 31); the GETDEF messages carry the
// file names in full length.
std::string paddedName(const std::string& name) {
  std::string padded = name;
  padded.resize(64, ' ');
  return padded;
}

// canonical 14-line xfoil.def with deviating product values
// (NPAN 120, MATYP 2/Mach 0.3, RETYP 3/Re 3e6, ACRIT 9/12, XTRIP 0.9/0.8).
const char* const kModifiedDefaults =
    "  120       1.0000   0.1500   0.2000 | Npan    PPanel  TErat  REFrat\n"
    "  1.0000   1.0000   1.0000   1.0000 | XrefS1  XrefS2  XrefP1 XrefP2\n"
    " 10.0000   0.5500   0.0150   0.8000 | Size    plotAR  CHsize ScrnFr\n"
    " 11.0000   8.5000   0.0000   0.0000 | Xpage   Ypage   Xmargn Ymargn\n"
    " T        T                          | Lcolor  Lcursor\n"
    " 1.0     -2.0   -0.5                 | CPmax   CPmin   CPdel\n"
    " 0.09    0.70   0.02                 | XoffAir ScalAir BLUwt\n"
    " 0.0     1.5    0.5                  | CLmin   CLmax   CLdel\n"
    " 0.0     0.04   0.01                 | CDmin   CDmax   CDdel\n"
    "-4.0    10.0    2.0                  | ALmin   ALmax   ALdel\n"
    "-0.25    0.0    0.05                 | CMmin   CMmax   CMdel\n"
    " 2       0.3000   0.0100             | MAtype  Mach    Vaccel\n"
    " 3       3.0000   9.0000  12.0000    | REtype  Re/10^6 Ncrit1  Ncrit2\n"
    " 0.9000   0.8000                     | Xtrip1  Xtrip2\n";

}  // namespace

TEST(integration, DefaultsStartupReportsMissingFile) {
  // Guaranteed empty test working directory: there can be no
  // xfoil.def present (precondition of the canonical
  // startup message FORMAT 1900 with the 9-character literal).
  const testing::ScopedTempDirectory work("defaults-startup-missing", true);
  const auto result = runSession("QUIT\n");
  CHECK(result.outputText.find("\n File  xfoil.def  not found\n") !=
        std::string::npos);
}

TEST(integration, DefaultsRdefMissingFileUsesPaddedName) {
  const testing::ScopedTempDirectory work("defaults-rdef-missing", true);
  prepareDefaultsWorkspace(work);
  const auto result = runSession("RDEF " + dataPath("missing.def") + "\nQUIT\n");
  CHECK(result.outputText.find(" File  " +
                               paddedName(dataPath("missing.def")) +
                               "  not found") != std::string::npos);
}

TEST(integration, DefaultsRdefAppliesAndEchoes) {
  const testing::ScopedTempDirectory work("defaults-rdef-echo", true);
  prepareDefaultsWorkspace(work);
  writeDataFile("p3-mod.def", kModifiedDefaults);
  const auto result =
      runSession("RDEF " + dataPath("p3-mod.def") + "\nQUIT\n");
  CHECK(result.outputText.find(" Default parameters read in from file  " +
                               paddedName(dataPath("p3-mod.def")) + ":") !=
        std::string::npos);
  CHECK(result.outputText.find("| Npan    PPanel  TErat  REFrat") !=
        std::string::npos);
  CHECK(result.outputText.find("Ncrit1 Ncrit2") != std::string::npos);
  removeDataFile("p3-mod.def");
}

TEST(integration, DefaultsRdefNumericEffectVisibleInPpar) {
  const testing::ScopedTempDirectory work("defaults-rdef-ppar", true);
  prepareDefaultsWorkspace(work);
  writeDataFile("p3-mod.def", kModifiedDefaults);
  const auto result =
      runSession("RDEF " + dataPath("p3-mod.def") +
                 "\nNACA 2412\nPPAR\n\nQUIT\n");
  CHECK(result.outputText.find("Number of panel nodes       120") !=
        std::string::npos);
  removeDataFile("p3-mod.def");
}

TEST(integration, DefaultsRdefBadDataKeepsEarlierLines) {
  const testing::ScopedTempDirectory work("defaults-rdef-bad", true);
  prepareDefaultsWorkspace(work);
  // canonical case: line 14 (XSTRIP) replaced by nonsense.
  std::string broken = kModifiedDefaults;
  const std::size_t lastNewline = broken.rfind('\n', broken.size() - 2);
  broken = broken.substr(0, lastNewline + 1) + "not-a-number\n";
  writeDataFile("p3-bad.def", broken);
  const auto result =
      runSession("RDEF " + dataPath("p3-bad.def") +
                 "\nNACA 2412\nPPAR\n\nQUIT\n");
  CHECK(result.outputText.find("  read error\n Settings may have been changed") !=
        std::string::npos);
  // Lines 1-13 remain effective: NPAN 120, ACRIT(2) 12, but XSTRIP(1)
  // stays at the required value 1.0.
  CHECK(result.outputText.find("Number of panel nodes       120") !=
        std::string::npos);
  removeDataFile("p3-bad.def");
}

TEST(integration, DefaultsRdefEndOfFileAbortsLikeFortranRuntimeError) {
  const testing::ScopedTempDirectory work("defaults-rdef-eof", true);
  prepareDefaultsWorkspace(work);
  std::string truncated = kModifiedDefaults;
  const std::size_t cut = truncated.find("Xpage");
  truncated = truncated.substr(0, truncated.rfind('\n', cut) + 1);
  writeDataFile("p3-short.def", truncated);
  const auto result =
      runSession("RDEF " + dataPath("p3-short.def") + "\nQUIT\n");
  CHECK(result.outputText.find(" Fortran runtime error: End of file") !=
        std::string::npos);
  CHECK_EQ(result.exitCode, 2);
  removeDataFile("p3-short.def");
}

TEST(integration, DefaultsWdefWritesAndRdefRestores) {
  const testing::ScopedTempDirectory work("defaults-wdef-round", true);
  prepareDefaultsWorkspace(work);
  const std::string file = dataPath("p3-round.def");
  removeDataFile("p3-round.def");
  const auto written = runSession("WDEF " + file + "\nQUIT\n");
  CHECK(written.outputText.find("  File  " + file + "  written") !=
        std::string::npos);
  const std::string contents = readDataFile("p3-round.def");
  CHECK(contents.find("| Npan    PPanel  TErat  REFrat") != std::string::npos);
  CHECK(contents.find("  160") != std::string::npos);

  writeDataFile("p3-mod.def", kModifiedDefaults);
  const auto modified =
      runSession("RDEF " + dataPath("p3-mod.def") + "\nRDEF " + file +
                 "\nNACA 2412\nPPAR\n\nQUIT\n");
  CHECK(modified.outputText.find("Number of panel nodes       160") !=
        std::string::npos);
  removeDataFile("p3-round.def");
  removeDataFile("p3-mod.def");
}

TEST(integration, DefaultsWdefOverwritePromptAcceptsBlank) {
  const testing::ScopedTempDirectory work("defaults-wdef-overwrite", true);
  prepareDefaultsWorkspace(work);
  const std::string file = dataPath("p3-over.def");
  writeDataFile("p3-over.def", "stale\n");
  const auto result = runSession("WDEF " + file + "\n\nQUIT\n");
  CHECK(result.outputText.find("  File  " + file +
                               "  exists.  Overwrite?  Y") !=
        std::string::npos);
  CHECK(result.outputText.find("  File  " + file + "  written") !=
        std::string::npos);
  CHECK(readDataFile("p3-over.def").find("| Npan") != std::string::npos);
  removeDataFile("p3-over.def");
}

TEST(integration, DefaultsWdefDeclineStillWritesLikeFortran) {
  const testing::ScopedTempDirectory work("defaults-wdef-decline", true);
  prepareDefaultsWorkspace(work);
  // canonical peculiarity (xfoil.f 319-327): the answer N/n reports
  // 'No action taken', but the sequence continues on OPEN STATUS='UNKNOWN'
  // error and still writes the file.
  const std::string file = dataPath("p3-decl.def");
  writeDataFile("p3-decl.def", "stale\n");
  const auto result = runSession("WDEF " + file + "\nN\nQUIT\n");
  CHECK(result.outputText.find("  File  " + file +
                               "  exists.  Overwrite?  Y") !=
        std::string::npos);
  CHECK(result.outputText.find(" No action taken") != std::string::npos);
  CHECK(result.outputText.find("  File  " + file + "  written") !=
        std::string::npos);
  CHECK(readDataFile("p3-decl.def").find("| Npan") != std::string::npos);
  removeDataFile("p3-decl.def");
}

TEST(integration, DefaultsStartupProcessAppliesFileInWorkDir) {
  // Resolve the program path absolutely before the working-directory change.
  const std::string programPath = programPathAbsolute();
  if (programPath.empty()) {
    return;
  }
  const testing::ScopedTempDirectory work("defaults-startup-y", true);
  {
    std::ofstream def(work.makePath("xfoil.def"));
    def << kModifiedDefaults;
  }
  {
    std::ofstream in(work.makePath("commands.in"));
    in << "\nQUIT\n";
  }
  testing::ProcessInvocation invocation;  // PUB-W2: portable helper
  invocation.program = programPath;
  invocation.stdinFile = "commands.in";
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  if (testing::runProcess(invocation) != 0) {
    reportFailure("Startup-Run with xfoil.def not successfully");
    return;
  }
  std::ifstream out(work.makePath("stdout.txt"));
  std::ostringstream buffer;
  buffer << out.rdbuf();
  const std::string text = buffer.str();
  CHECK(text.find("  Read settings from file  xfoil.def ?  Y") !=
        std::string::npos);
  CHECK(text.find(" Default parameters read in from file  xfoil.def:") !=
        std::string::npos);
  CHECK(text.find("| Npan    PPanel  TErat  REFrat") != std::string::npos);
}

TEST(integration, DefaultsStartupProcessDeclinesWithN) {
  const std::string programPath = programPathAbsolute();
  if (programPath.empty()) {
    return;
  }
  const testing::ScopedTempDirectory work("defaults-startup-n", true);
  {
    std::ofstream def(work.makePath("xfoil.def"));
    def << kModifiedDefaults;
  }
  {
    std::ofstream in(work.makePath("commands.in"));
    in << "N\nQUIT\n";
  }
  testing::ProcessInvocation invocation;  // PUB-W2: portable helper
  invocation.program = programPath;
  invocation.stdinFile = "commands.in";
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  if (testing::runProcess(invocation) != 0) {
    reportFailure("Startup-Run with N-Answer not successfully");
    return;
  }
  std::ifstream out(work.makePath("stdout.txt"));
  std::ostringstream buffer;
  buffer << out.rdbuf();
  const std::string text = buffer.str();
  CHECK(text.find("  Read settings from file  xfoil.def ?  Y") !=
        std::string::npos);
  CHECK(text.find("Default parameters read in") == std::string::npos);
}

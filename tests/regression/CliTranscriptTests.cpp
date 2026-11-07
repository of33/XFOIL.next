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

// file: tests/regression/CliTranscriptTests.cpp
// Transcript golden test of the interactive interpreter (steps 17b, 17c).
// Each case below tests/golden/cli/ runs as a real process with the
// frozen default input; default output,
// default error output and exit code are compared byte-exactly.
// Cases with optional argument entries additionally preserve a
// positional profile file path (OPER-/VPAR transcripts).

#include "TestSupport.h"
// loadTextFile now comes from TestSupport.h (CliOutputParser omitted).

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

const char* const kCliGoldenDir = "tests/golden/cli/";
const char* const kCliSampleFile = "sample/CLARK_Y.dat";

struct TranscriptCase {
  const char* name;
  const char* argument;  // empty: start without program arguments
};

const TranscriptCase kTranscriptCases[] = {
    {"startup-quit", ""},
    {"help-quit", ""},
    {"unknown-quit", ""},
    {"plot-unavailable-quit", ""},
    {"oper-help", "sample/CLARK_Y.dat"},
    {"oper-inviscid-alpha", "sample/CLARK_Y.dat"},
    {"oper-viscous-alpha", "sample/CLARK_Y.dat"},
    {"vpar-show", "sample/CLARK_Y.dat"},
    {"oper-sequence", "sample/CLARK_Y.dat"},
    {"file-load-clark-y-quit", ""},
    {"file-naca-save-load-quit", ""},
    {"file-norm-reve-deli-quit", ""},
    {"file-inte-pcop-quit", ""},
    {"file-name-ninc-xycm-quit", ""},
 // paneling and the PPAR menu.
    {"pane-loaded-quit", "sample/CLARK_Y.dat"},
    {"ppar-show-return", "sample/CLARK_Y.dat"},
    {"ppar-change-apply", "sample/CLARK_Y.dat"},
    {"ppar-plot-unavailable", "sample/CLARK_Y.dat"},
};

// transcripts with file outputs write relative to the
// working directory into build/regression-capture/t17d1-* (a fixed
// part of the golden default input, hence unchanged). The
// child process runs per run in its own, unique
// test working directory, which contains exactly this substructure;
// overwrite prompts therefore never depend on the state of earlier runs
// or on another test process, and the artifacts are removed by RAII
// automatically.
struct TranscriptOutcome {
  bool executed{};
  int exitCode{};
  std::string stdoutText;
  std::string stderrText;
};

std::string absoluteFromStartDirectory(const std::string& path) {
  std::error_code error;
  const std::string absolute = std::filesystem::absolute(path, error).string();
  return error ? path : absolute;
}

TranscriptOutcome runTranscript(const TranscriptCase& testCase) {
  TranscriptOutcome outcome;

  const char* const configuredPath = std::getenv("XFOIL_CLI_PATH");
  const std::string program =
      configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";

  // Resolve all reference paths absolutely before the working-directory
  // change; the goldens are only ever read.
  const std::string programPath = absoluteFromStartDirectory(program);
  const std::string stdinPath =
      absoluteFromStartDirectory(std::string(kCliGoldenDir) +
                                 testCase.name + ".stdin.txt");
  const std::string sampleFixture =
      absoluteFromStartDirectory(kCliSampleFile);

  const testing::ScopedTempDirectory work(
      std::string{"cli-transcript-"} + testCase.name, true);
  std::error_code error;
  std::filesystem::create_directories(work.path() / "build" /
                                          "regression-capture",
                                      error);
  std::filesystem::create_directories(work.path() / "sample", error);
  std::filesystem::copy_file(sampleFixture, work.path() / kCliSampleFile,
                             std::filesystem::copy_options::overwrite_existing,
                             error);

  // PUB-W2: central portable process helper. The run happens inside
  // the test working directory (the scoped directory already entered
  // it); the stdin path is absolute.
  testing::ProcessInvocation invocation;
  invocation.program = programPath;
  if (testCase.argument != nullptr && testCase.argument[0] != '\0') {
    invocation.arguments = {std::string(testCase.argument)};
  }
  invocation.stdinFile = stdinPath;
  invocation.stdoutFile = "cli-transcript-stdout.txt";
  invocation.stderrFile = "cli-transcript-stderr.txt";
  const int exitCode = testing::runProcess(invocation);
  if (exitCode == -1) {
    return outcome;
  }

  outcome.exitCode = exitCode;

  if (!testing::loadTextFile(
          work.makePath("cli-transcript-stdout.txt").string(),
          outcome.stdoutText) ||
      !testing::loadTextFile(
          work.makePath("cli-transcript-stderr.txt").string(),
          outcome.stderrText)) {
    return outcome;
  }
  // Captured console output: canonicalize CRLF -> LF before the golden
  // comparison (the goldens are LF text; PUB-W2).
  outcome.stdoutText = testing::canonicalizeCapturedText(outcome.stdoutText);
  outcome.stderrText = testing::canonicalizeCapturedText(outcome.stderrText);

  outcome.executed = true;
  return outcome;
}

void checkTranscriptCase(const TranscriptCase& testCase) {
  const std::string name{testCase.name};

  std::string expectedStdin;
  if (!testing::loadTextFile(
          std::string(kCliGoldenDir) + name + ".stdin.txt", expectedStdin)) {
    testing::reportFailure(__FILE__, __LINE__,
                           "Transcript-Input not readable: " + name);
    return;
  }

  const TranscriptOutcome outcome = runTranscript(testCase);
  CHECK(outcome.executed);
  if (!outcome.executed) {
    return;
  }

  std::string expectedStdout;
  if (testing::loadTextFile(
          std::string(kCliGoldenDir) + name + ".stdout.txt",
          expectedStdout)) {
    CHECK_EQ(outcome.stdoutText, expectedStdout);
  } else {
    testing::reportFailure(__FILE__, __LINE__,
                           "Transcript-Golden not readable: " + name +
                               ".stdout.txt");
  }

  std::string expectedStderr;
  if (testing::loadTextFile(
          std::string(kCliGoldenDir) + name + ".stderr.txt",
          expectedStderr)) {
    CHECK_EQ(outcome.stderrText, expectedStderr);
  } else {
    testing::reportFailure(__FILE__, __LINE__,
                           "Transcript-Golden not readable: " + name +
                               ".stderr.txt");
  }

  std::string expectedExitText;
  if (testing::loadTextFile(
          std::string(kCliGoldenDir) + name + ".exit.txt",
          expectedExitText)) {
    int expectedExitCode = -1;
    const bool parsed =
        (std::sscanf(expectedExitText.c_str(), "%d", &expectedExitCode) == 1);
    CHECK(parsed);
    if (parsed) {
      CHECK_EQ(outcome.exitCode, expectedExitCode);
    }
  } else {
    testing::reportFailure(__FILE__, __LINE__,
                           "Transcript-Golden not readable: " + name +
                               ".exit.txt");
  }
}

}  // namespace

TEST(regression, CliInterpreterTranscriptsMatchGolden) {
  for (const TranscriptCase& testCase : kTranscriptCases) {
    checkTranscriptCase(testCase);
  }
}

// End: tests/regression/CliTranscriptTests.cpp

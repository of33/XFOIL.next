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

// File: tests/integration/CommandInterpreterTests.cpp
// Interpreter tests with injected string streams without process start
// and real process-argument tests for the
// mode forwarding of the product program.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"
#include "app/ProductInfo.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

struct InterpreterRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

InterpreterRunResult runInterpreter(
    const std::string& inputText,
    const std::optional<std::string>& inputPath) {
  InterpreterRunResult result;

  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = inputPath;

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

std::string productProgramPath() {
  const char* configuredPath = std::getenv("XFOIL_CLI_PATH");
  return configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";
}

// Absolute program path, resolved before a working-directory change.
std::string absoluteProductProgramPath() {
  std::error_code error;
  const std::string absolute =
      std::filesystem::absolute(productProgramPath(), error).string();
  return error ? productProgramPath() : absolute;
}

struct ProcessRunResult {
  bool executed{};
  int exitCode{};
  std::string stdoutText;
  std::string stderrText;
};

// Starts the product program with exclusively internal fixed
// test arguments. This is no general process- or
// shell-escaping helper. The child process runs in its own,
// unique test working directory; capture files are therefore per
// run collision-free and the start directory of the child is
// controlled.
ProcessRunResult runProductProcess(
    const std::vector<std::string>& arguments) {
  ProcessRunResult result;

  const std::string programPath = absoluteProductProgramPath();
  const testing::ScopedTempDirectory work("cli-args-run", true);

  // PUB-W2: central portable process helper. The child runs inside the
  // test working directory (the scoped directory already entered it),
  // so no cd is needed; capture files are per-run and absolute-loaded.
  testing::ProcessInvocation invocation;
  invocation.program = programPath;
  invocation.arguments = arguments;
  invocation.stdoutFile = "cli-args-stdout.txt";
  invocation.stderrFile = "cli-args-stderr.txt";
  const int exitCode = testing::runProcess(invocation);
  if (exitCode == -1) {
    return result;
  }

  result.exitCode = exitCode;
  if (!testing::loadTextFile(work.makePath("cli-args-stdout.txt").string(),
                              result.stdoutText) ||
      !testing::loadTextFile(work.makePath("cli-args-stderr.txt").string(),
                              result.stderrText)) {
    return result;
  }
  // Captured console output: canonicalize CRLF -> LF (PUB-W2).
  result.stdoutText = testing::canonicalizeCapturedText(result.stdoutText);
  result.stderrText = testing::canonicalizeCapturedText(result.stderrText);

  result.executed = true;
  return result;
}

std::filesystem::path writeTemporaryProfile(
    const testing::ScopedTempDirectory& work) {
  const std::filesystem::path path = work.makePath("test-profile.dat");

  std::ofstream file(path);
  file << "TEST-PROFILE\n"
       << " 0.0   0.0\n"
       << " 0.5   0.1\n"
       << " 1.0   0.0\n";
  file.close();
  return path;
}

const char* const kPrompt = "\n XFOIL   c>  ";
const char* const kMenuHeader = "   QUIT    Exit program";
const char* const kPlotUnavailable =
    "Plotting is not available in this build.";

}  // namespace

TEST(integration, InterpreterQuitCommandExitsSuccessfully) {
  const auto result = runInterpreter("QUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(std::string{xfoil::app::product::name} +
                               " Version " +
                               std::string{xfoil::app::product::version}) !=
        std::string::npos);
  CHECK(result.outputText.find(kMenuHeader) != std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.errorText.empty());
}

TEST(integration, InterpreterShortQuitCommandExitsSuccessfully) {
  const auto result = runInterpreter("Q\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.errorText.empty());
}

TEST(integration, InterpreterEofExitsSuccessfully) {
  const auto result = runInterpreter("", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.errorText.empty());
  // the prompt was shown, then EOF arrived without an endless loop.
  CHECK_EQ(countOccurrences(result.outputText, kPrompt), std::size_t{1});
}

TEST(integration, InterpreterQuestionMarkPrintsMainMenu) {
  const auto result = runInterpreter("?\nQUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kMenuHeader), std::size_t{2});
  CHECK_EQ(countOccurrences(result.outputText, kPrompt), std::size_t{2});
}

TEST(integration, InterpreterEmptyLineReprompts) {
  const auto result = runInterpreter("\n\nQUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  // two empty inputs lead to two additional prompts before QUIT.
  CHECK_EQ(countOccurrences(result.outputText, kPrompt), std::size_t{3});
}

TEST(integration, InterpreterUnknownCommandIsDiagnosed) {
  // FCPP015: Fortran format 1050 (1X,A4,...); the command is printed in
  // four characters (A4), so "FOO" appears with trailing padding.
  const auto result = runInterpreter("foo bar\nQUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            " FOO  command not recognized.  Type a \"?\" for list") !=
        std::string::npos);
}

TEST(integration, UnknownTopCommandUsesFortranA4Field) {
  // FCPP015: Fortran FORMAT 1050 (1X,A4,' command not recognized...');
  // the command is printed with a four-character field width (A4). "zz"
  // (2 characters) is padded to 4 characters: "ZZ" + fill.
  const auto result = runInterpreter("zz\nQUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            " ZZ   command not recognized.  Type a \"?\" for list") !=
        std::string::npos);
}

TEST(integration, InterpreterRecognizesUnavailablePlotCommand) {
 // PLOP is routed (mixed text/flag command,
  // xfoil.f 302, plutil.f OPLSET) and is not blanket unavailable.
  // only pure plot commands (Z, U, BENP) keep the unavailable contract.
  const auto result = runInterpreter("z\nU\nBENP\nQUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
           std::size_t{3});
  CHECK(result.outputText.find("not recognized") == std::string::npos);
}

TEST(integration, InterpreterOpensPlopMenuProductively) {
  // TOP PLOP opens the PLOP handler
  // (handlePlotOptionsCore). The menu table appears; '0' returns
  // to TOP. No geometry or panel mutation.
  const auto result = runInterpreter("PLOP\n0\nQUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("raphics-enable flag") !=
        std::string::npos);
  CHECK(result.outputText.find(kPlotUnavailable) == std::string::npos);
}

TEST(integration, InterpreterDoesNotLoadClarkYWithoutArguments) {
  const auto result = runInterpreter("QUIT\n", std::nullopt);
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Loading profile file") ==
        std::string::npos);
  CHECK(result.outputText.find("CLARK") == std::string::npos);
}

TEST(integration, InterpreterLoadsPositionalDatFile) {
  const testing::ScopedTempDirectory work("InterpreterLoadsPositionalDatFile");
  const std::filesystem::path profilePath = writeTemporaryProfile(work);

  const auto result =
      runInterpreter("QUIT\n", std::optional<std::string>{profilePath.string()});
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            "Loading profile file: " + profilePath.string()) !=
        std::string::npos);
  CHECK(result.outputText.find("Foil name : TEST-PROFILE") !=
        std::string::npos);
  CHECK(result.outputText.find("Number of points : 3") !=
        std::string::npos);
}

TEST(integration, InterpreterUsesCentralPanelDefault) {
  // FCPP005: after loading a geometry the session keeps
  // NPAN = 160 from the central default (no CLI
  // compensation value). The PPAR menu shows that value.
  const testing::ScopedTempDirectory work(
      "InterpreterUsesCentralPanelDefault");
  const std::filesystem::path profilePath = writeTemporaryProfile(work);

  const auto result =
      runInterpreter("PPAR\n\nQUIT\n",
                     std::optional<std::string>{profilePath.string()});
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(
            "Number of panel nodes       160") != std::string::npos);
}

TEST(integration, InterpreterRejectsMissingPositionalDatFile) {
  const testing::ScopedTempDirectory work(
      "InterpreterRejectsMissingPositionalDatFile");
  const std::string missing = work.makePath("definitely-missing.dat").string();

  const auto result =
      runInterpreter("QUIT\n", std::optional<std::string>{missing});
  CHECK_EQ(result.exitCode, 1);
  CHECK(result.errorText.find("Failed to open dat file") !=
        std::string::npos);
  // No prompt, no menu table, no partially initialized session.
  CHECK(result.outputText.find(kPrompt) == std::string::npos);
  CHECK(result.outputText.find(kMenuHeader) == std::string::npos);
}

// --help is a supported option (exit 0); only a truly
// unknown option is rejected.
TEST(integration, InterpreterProgramRejectsUnknownOption) {
  const auto result = runProductProcess({"--definitely-not-on-option"});
  CHECK(result.executed);
  if (!result.executed) {
    return;
  }
  CHECK_EQ(result.exitCode, 2);
  CHECK_EQ(result.stderrText,
           std::string{"Unknown option: --definitely-not-on-option\n"});
}

TEST(integration, InterpreterProgramRejectsMultipleInputFiles) {
  const auto result = runProductProcess({"a.dat", "b.dat"});
  CHECK(result.executed);
  if (!result.executed) {
    return;
  }
  CHECK_EQ(result.exitCode, 2);
  CHECK_EQ(result.stderrText,
           std::string{"Only one airfoil input file may be specified\n"});
}

// End: tests/integration/CommandInterpreterTests.cpp

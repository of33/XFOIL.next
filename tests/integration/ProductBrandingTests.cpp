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

// file: tests/integration/ProductBrandingTests.cpp
// Permanent branding tests: the product banner, --version and the
// product metadata line of results.dat come exclusively from the
// central source app/ProductInfo.h. Each case starts the
// product program as a real process in a unique, empty
// temporary directory.

#include "TestSupport.h"
#include "app/ProductInfo.h"

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

std::string readFile(const std::filesystem::path& path) {
  std::string text;
  testing::loadTextFile(path.string(), text);
  return text;
}

struct ProcessRun {
  bool executed{};
  int exitCode{};
  std::string stdoutText;
  std::string stderrText;
};

// Runs the product program in the temporary directory as the
// working directory; stdout/stderr are redirected to stdout.txt or
// stderr.txt. extraArgs are the program arguments, stdinRedirect an
// optional redirect file.
ProcessRun runProduct(const testing::ScopedTempDirectory& work,
                      const std::string& extraArgs,
                      const std::string& stdinRedirect) {
  ProcessRun run;
  // PUB-W2: central portable process helper. extraArgs are
  // whitespace-separated simple tokens (flags and space-free paths).
  testing::ProcessInvocation invocation;
  invocation.program = absoluteProductProgramPath();
  invocation.workingDirectory = work.path().string();
  {
    std::istringstream argumentStream(extraArgs);
    std::string token;
    while (argumentStream >> token) {
      invocation.arguments.push_back(token);
    }
  }
  if (!stdinRedirect.empty()) {
    invocation.stdinFile = stdinRedirect;
  }
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  const int exitCode = testing::runProcess(invocation);
  if (exitCode == -1) {
    testing::reportFailure(__FILE__, __LINE__,
                           "product process not runnable");
    return run;
  }
  run.executed = true;
  run.exitCode = exitCode;
  // Captured console output: canonicalize CRLF -> LF (PUB-W2).
  run.stdoutText =
      testing::canonicalizeCapturedText(readFile(work.makePath("stdout.txt")));
  run.stderrText =
      testing::canonicalizeCapturedText(readFile(work.makePath("stderr.txt")));
  return run;
}

// the complete target banner, built exclusively from ProductInfo.
std::string expectedBanner() {
  return std::string{"\n ===================================================\n "} +
         std::string{xfoil::app::product::name} + " Version " +
         std::string{xfoil::app::product::version} +
         "\n Copyright (C) 2026 Olaf Frommann\n"
         "\n Based upon XFOIL Version 6.99\n"
         " Copyright (C) 2000   Mark Drela, Harold Youngren\n"
         "\n This software comes with ABSOLUTELY NO WARRANTY,\n"
         "   subject to the GNU General Public License.\n"
         "\n Caveat computor\n"
         " ===================================================\n";
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::string current;
  for (const char symbol : text) {
    if (symbol == '\n') {
      lines.push_back(current);
      current.clear();
    } else {
      current.push_back(symbol);
    }
  }
  if (!current.empty()) {
    lines.push_back(current);
  }
  return lines;
}

std::size_t countEntries(const std::filesystem::path& directory) {
  std::size_t count = 0;
  std::error_code error;
  for (const auto& entry :
       std::filesystem::directory_iterator(directory, error)) {
    static_cast<void>(entry);
    ++count;
  }
  return count;
}

}  // namespace

TEST(integration, VersionFlagUsesProductInfoAndCreatesNoArtifacts) {
  const testing::ScopedTempDirectory work("branding-version");
  const auto run = runProduct(work, "--version", "");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK_EQ(run.stdoutText,
           std::string{xfoil::app::product::name} + " " +
               std::string{xfoil::app::product::version} + "\n");
  CHECK_EQ(run.stderrText, std::string{});
  // No session, banner or file artifacts: apart from stdout.txt and
  // stderr.txt nothing is created in the working directory.
  CHECK_EQ(countEntries(work.path()), std::size_t{2});
}

TEST(integration, InteractiveBannerUsesProductInfoSource) {
  const testing::ScopedTempDirectory work("branding-banner");
  CHECK(writeFile(work.makePath("in.txt"), "QUIT\n"));
  const auto run = runProduct(work, "", "in.txt");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  CHECK(run.stdoutText.rfind(expectedBanner(), 0) == 0);
  CHECK_EQ(run.stderrText, std::string{});
}

TEST(integration, BatchResultsCarryProductMetadataLine) {
  const testing::ScopedTempDirectory work("branding-results");
  CHECK(writeFile(work.makePath("job.xfb"),
                   "XFOIL-BATCH 1\n[commands]\nNACA 2412\nPANE\nOPER\n"
                   "ALFA 3\n\nQUIT\n"));
  const auto run = runProduct(work, "--batch job.xfb", "");
  CHECK(run.executed);
  CHECK_EQ(run.exitCode, 0);
  const std::vector<std::string> lines =
      splitLines(readFile(work.makePath("results.dat")));
  CHECK(lines.size() >= 3);
  CHECK_EQ(lines.at(0), std::string{"# XFOIL_BATCH_RESULTS 1"});
  CHECK_EQ(lines.at(1), std::string{"# product "} +
                            std::string{xfoil::app::product::name} + " " +
                            std::string{xfoil::app::product::version});
  CHECK_EQ(lines.at(2),
           std::string{"# index alpha cl cd cdp cm re mach ncrit_top "
                       "ncrit_bottom xtr_top xtr_bottom converged iterations"});
}

TEST(integration, BatchTranscriptAndQuietTranscriptUseProductBanner) {
  const testing::ScopedTempDirectory normal("branding-transcript");
  CHECK(writeFile(normal.makePath("job.xfb"),
                   "XFOIL-BATCH 1\n[commands]\nNACA 2412\nQUIT\n"));
  const auto normalRun = runProduct(normal, "--batch job.xfb", "");
  CHECK(normalRun.executed);
  CHECK_EQ(normalRun.exitCode, 0);
  CHECK(readFile(normal.makePath("transcript.txt"))
            .rfind(expectedBanner(), 0) == 0);
  CHECK(normalRun.stdoutText.rfind(expectedBanner(), 0) == 0);

  const testing::ScopedTempDirectory quiet("branding-quiet-banner");
  CHECK(writeFile(quiet.makePath("job.xfb"),
                   "XFOIL-BATCH 1\n[commands]\nNACA 2412\nQUIT\n"));
  const auto quietRun = runProduct(quiet, "--batch job.xfb --quiet", "");
  CHECK(quietRun.executed);
  CHECK_EQ(quietRun.exitCode, 0);
  CHECK_EQ(quietRun.stdoutText, std::string{});
  CHECK(readFile(quiet.makePath("transcript.txt"))
            .rfind(expectedBanner(), 0) == 0);
}

// End: tests/integration/ProductBrandingTests.cpp

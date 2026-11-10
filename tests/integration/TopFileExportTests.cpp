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

// file: tests/integration/TopFileExportTests.cpp
// TOP file export: permanent check of MSAV (MSAVE,
// xfoil.f 1430-1544) and BEND (BENDUMP, xgeom.f 1295-1365) against the
// manifest-based Fortran references
// tests/fortran-reference/cases/top-file-export/*.

#include "TestSupport.h"

#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/session/SessionState.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

const char* const kCaseRoot = "tests/fortran-reference/cases/top-file-export";

std::string readBytes(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

// Runs the CLI in its own, unique, empty
// test working directory; the seed files of the reference cases
// are copied beforehand, and RAII removes the artifacts. No shared
// capture path across test processes.
class TopExportRun {
 public:
  explicit TopExportRun(const std::string& caseName)
      : work_("top-file-export-" + caseName) {
    std::error_code error;
    const std::filesystem::path casePath =
        std::filesystem::absolute(std::filesystem::path(kCaseRoot) / caseName,
                                  error);
    std::filesystem::copy(casePath / "commands.in",
                          work_.makePath("commands.in"),
                          std::filesystem::copy_options::overwrite_existing,
                          error);
    if (std::filesystem::exists(casePath / "input")) {
      for (const auto& entry :
           std::filesystem::directory_iterator(casePath / "input")) {
        std::filesystem::copy(entry.path(), work_.makePath(entry.path()
                                                               .filename()
                                                               .string()),
                              std::filesystem::copy_options::overwrite_existing,
                              error);
      }
    }

    // These cases pin the 6.99 byte artifacts. Keep the canonical corpus
    // unchanged and apply the old panel defaults through the existing RDEF
    // contract before the case commands.
    xfoil::io::DefaultsValues oldDefaults = xfoil::session::initDefaultsValues();
    oldDefaults.npan = 160;
    oldDefaults.cterat = 0.15;
    {
      std::ofstream defaults(work_.makePath("xfoil-6.99.def"),
                             std::ios::binary);
      defaults << xfoil::io::writeDefaults(oldDefaults, 2);
      std::ifstream original(work_.makePath("commands.in"), std::ios::binary);
      std::ofstream derived(work_.makePath("commands-with-old-defaults.in"),
                            std::ios::binary);
      derived << "RDEF xfoil-6.99.def\n" << original.rdbuf();
    }

    const char* const configuredPath = std::getenv("XFOIL_CLI_PATH");
    const std::string program = configuredPath != nullptr
                                    ? configuredPath
                                    : "build/debug/xfoil.next";
    // PUB-W2: central portable process helper (cmd.exe syntax, .exe
    // resolution, quoting, separate stdout/stderr capture).
    testing::ProcessInvocation invocation;
    invocation.program = program;
    invocation.workingDirectory = work_.path().string();
    invocation.stdinFile = "commands-with-old-defaults.in";
    invocation.stdoutFile = "stdout.txt";
    invocation.stderrFile = "stderr.txt";
    exitCode_ = testing::runProcess(invocation);
  }

  [[nodiscard]] int exitCode() const noexcept { return exitCode_; }

  [[nodiscard]] std::string read(const std::string& fileName) const {
    return readBytes(work_.makePath(fileName).string());
  }

  [[nodiscard]] bool exists(const std::string& fileName) const {
    std::error_code error;
    return std::filesystem::exists(work_.makePath(fileName), error);
  }

 private:
  testing::ScopedTempDirectory work_;
  int exitCode_{};
};

void compareFile(const TopExportRun& run, const std::string& caseName,
                 const std::string& artifact) {
  const std::string produced = run.read(artifact);
  const std::string expected =
      readBytes(std::string(kCaseRoot) + "/" + caseName + "/generated/" +
                artifact);
  if (expected.empty()) {
    testing::reportFailure(__FILE__, __LINE__,
                           "reference artifact not readable: " + artifact);
    return;
  }
  if (produced != expected) {
    testing::reportFailure(__FILE__, __LINE__,
                           "TOP export file not byte-identical: " + artifact);
  }
}

// Extract the BEND report block (from ' Area =' to ' J/t =')
// from a stdout text; bypasses the known startup/plot deviations.
std::string extractBendReport(const std::string& text) {
  const std::size_t begin = text.find(" Area =");
  const std::size_t end = text.find("      J/t   =");
  if (begin == std::string::npos || end == std::string::npos) {
    return {};
  }
  const std::size_t lineEnd = text.find('\n', end);
  return text.substr(begin, lineEnd - begin + 1);
}

void compareBendReport(const TopExportRun& run, const std::string& caseName) {
  // Captured console output: canonicalize CRLF -> LF (the reference is
  // LF text; PUB-W2).
  const std::string produced = extractBendReport(
      testing::canonicalizeCapturedText(run.read("stdout.txt")));
  const std::string expected = extractBendReport(
      readBytes(std::string(kCaseRoot) + "/" + caseName +
                "/stdout.raw.txt"));
  if (expected.empty()) {
    testing::reportFailure(__FILE__, __LINE__,
                           "BEND reference report not readable: " + caseName);
    return;
  }
  if (produced != expected) {
    testing::reportFailure(__FILE__, __LINE__,
                           "BEND report not byte-identical: " + caseName);
  }
}

}  // namespace

TEST(integration, MsaveOpenErrorMatchesFortranReference) {
  const TopExportRun run("msav-open-error");
  CHECK_EQ(run.exitCode(), 0);
  const std::string stdoutText = run.read("stdout.txt");
  CHECK(stdoutText.find(" Old file OPEN error.  Airfoil not saved.") !=
        std::string::npos);
  CHECK(!run.exists("does_not_exist.mses"));
}

TEST(integration, MsaveIsavRoundtripBytesMatchFortranReference) {
  const TopExportRun run("msav-isav-roundtrip");
  CHECK_EQ(run.exitCode(), 0);
  compareFile(run, "msav-isav-roundtrip", "single.mses");
}

TEST(integration, MsaveMultielementReplaceBytesMatchFortranReference) {
  const TopExportRun run("msav-multielement-replace");
  CHECK_EQ(run.exitCode(), 0);
  compareFile(run, "msav-multielement-replace", "multi.mses");
}

TEST(integration, MsaveMultielementReveReplaceBytesMatchFortranReference) {
  const TopExportRun run("msav-multielement-append-reve");
  CHECK_EQ(run.exitCode(), 0);
  compareFile(run, "msav-multielement-append-reve", "multi.mses");
}

TEST(integration, BendNaca2412ReportMatchesFortranReference) {
  const TopExportRun run("bend-naca2412");
  CHECK_EQ(run.exitCode(), 0);
  compareBendReport(run, "bend-naca2412");
}

TEST(integration, BendNaca0012ReportMatchesFortranReference) {
  const TopExportRun run("bend-naca0012");
  CHECK_EQ(run.exitCode(), 0);
  compareBendReport(run, "bend-naca0012");
}

TEST(integration, BendClarkYReportMatchesFortranReference) {
  const TopExportRun run("bend-clark-y");
  CHECK_EQ(run.exitCode(), 0);
  compareBendReport(run, "bend-clark-y");
}

// End: tests/integration/TopFileExportTests.cpp

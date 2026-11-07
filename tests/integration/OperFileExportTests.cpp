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

// file: tests/integration/OperFileExportTests.cpp
// permanent coverage of the productive OPER file exports
// CPWR (CPDUMP), DUMP (BLDUMP), DMP (BLDUMP2). The file bytes are
// compared against the manifest-based Fortran references
// tests/fortran-reference/cases/file-export/*.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

const char* const kCaseRoot = "tests/fortran-reference/cases/file-export";
const char* const kClarkYFixture =
    "tests/fortran-reference/cases/aero-viscous/visc-clark-y/input/CLARK_Y.dat";

std::string readBytes(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

// Subprocess run in its own, unique test working directory
//, so that the generated files do not contaminate the working
// directory, no capture path is shared by several test processes
// and the Fortran reference case (LOAD from input/) still works. The
// artifacts are removed by RAII.
class ExportRun {
 public:
  ExportRun(const std::string& tag, const std::string& commandsText)
      : work_("file-export-" + tag) {
    std::error_code error;
    {
      std::ofstream commands(work_.makePath("commands.in"), std::ios::binary);
      commands << commandsText;
    }
    std::filesystem::copy(std::filesystem::absolute(kClarkYFixture, error),
                          work_.makePath("CLARK_Y.dat"),
                          std::filesystem::copy_options::overwrite_existing,
                          error);

    const char* const configuredPath = std::getenv("XFOIL_CLI_PATH");
    const std::string program = configuredPath != nullptr
                                    ? configuredPath
                                    : "build/debug/xfoil.next";
    // PUB-W2: central portable process helper (cmd.exe syntax, .exe
    // resolution, quoting, separate stdout/stderr capture).
    testing::ProcessInvocation invocation;
    invocation.program = program;
    invocation.workingDirectory = work_.path().string();
    invocation.stdinFile = "commands.in";
    invocation.stdoutFile = "stdout.txt";
    invocation.stderrFile = "stderr.txt";
    exitCode_ = testing::runProcess(invocation);
  }

  [[nodiscard]] int exitCode() const noexcept { return exitCode_; }

  [[nodiscard]] std::string read(const std::string& fileName) const {
    return readBytes(work_.makePath(fileName).string());
  }

 private:
  testing::ScopedTempDirectory work_;
  int exitCode_{};
};

void compareArtifact(const ExportRun& run, const std::string& caseName,
                     const std::string& artifact,
                     const std::string& producedName) {
  const std::string produced = run.read(producedName);
  const std::string expected =
      readBytes(std::string(kCaseRoot) + "/" + caseName + "/generated/" +
                artifact);
  if (expected.empty()) {
    testing::reportFailure(__FILE__, __LINE__,
                           "reference artifact not readable: " + artifact);
    return;
  }
  if (produced != expected) {
    testing::reportFailure(
        __FILE__, __LINE__,
        "Export file not byte-identical: " + producedName + " (produces " +
            std::to_string(produced.size()) + " Bytes, Reference " +
            std::to_string(expected.size()) + " Bytes)");
  }
}

}  // namespace

TEST(integration, CpwrViscousFileMatchesFortranReference) {
  const ExportRun run(
      "cpwr-viscous",
      "LOAD CLARK_Y.dat\nOPER\nVISC\n3e6\nALFA 4\n"
      "CPWR produced.cp\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  compareArtifact(run, "cpwr-visc-clark-y", "cpwr-visc-clark-y.cp",
                  "produced.cp");
}

TEST(integration, DumpViscousFileMatchesFortranReference) {
  const ExportRun run(
      "dump-viscous",
      "LOAD CLARK_Y.dat\nOPER\nVISC\n3e6\nALFA 4\n"
      "DUMP produced.bl\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  compareArtifact(run, "dump-visc-clark-y", "dump-visc-clark-y.bl",
                  "produced.bl");
}

TEST(integration, DmpViscousFileMatchesFortranReference) {
  const ExportRun run(
      "dmp-viscous",
      "LOAD CLARK_Y.dat\nOPER\nVISC\n3e6\nALFA 4\n"
      "DMP produced.bl\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  compareArtifact(run, "dmp-visc-clark-y", "dmp-visc-clark-y.bl", "produced.bl");
}

TEST(integration, DumpPortableBlNameWorksAsExplicitPath) {
  // PUB-W1: 100.bl is the portable BL name for portable examples and
  // for the stored VISCAL artifacts; an explicit DUMP with that name
  // must write it unchanged (the product performs no name rewriting).
  const ExportRun run(
      "dump-portable-name",
      "LOAD CLARK_Y.dat\nOPER\nVISC\n3e6\nALFA 4\n"
      "DUMP 100.bl\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  compareArtifact(run, "dump-visc-clark-y", "dump-visc-clark-y.bl", "100.bl");
}

TEST(integration, CpwrDefaultPromptUsesLoadedFilePrefix) {
  const ExportRun run(
      "cpwr-default-prompt",
      "LOAD CLARK_Y.dat\nOPER\nALFA 2\nCPWR\n\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  const std::string stdoutText = run.read("stdout.txt");
  CHECK(stdoutText.find(" Enter filename:  CLARK_Y.cp") != std::string::npos);
  compareArtifact(run, "cpwr-inviscid-default-prompt", "CLARK_Y.cp",
                  "CLARK_Y.cp");
}

TEST(integration, CpwrOpenFailureAbortsLikeFortranRuntime) {
  // Fortran OPEN(...,STATUS='UNKNOWN') without an ERR branch leads to a
  // runtime abort with exit code 2; the C++ CLI mirrors the control flow
  // and exit code (the stderr backtrace stays a Fortran runtime artifact).
  const ExportRun run(
      "cpwr-open-failure",
      "LOAD CLARK_Y.dat\nOPER\nALFA 2\nCPWR /no_such_dir_xyz/out.cp\n"
      "\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 2);
}

TEST(integration, FileExportOverwritesWithoutPrompt) {
  const ExportRun run(
      "file-export-overwrite",
      "LOAD CLARK_Y.dat\nOPER\nVISC\n3e6\nALFA 4\n"
      "CPWR produced.cp\nCPWR produced.cp\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  const std::string stdoutText = run.read("stdout.txt");
  CHECK(stdoutText.find("overwrite") == std::string::npos);
  compareArtifact(run, "cpwr-visc-clark-y", "cpwr-visc-clark-y.cp",
                  "produced.cp");
}

// End: tests/integration/OperFileExportTests.cpp

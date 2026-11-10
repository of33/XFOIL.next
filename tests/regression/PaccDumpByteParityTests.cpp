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

// file: tests/regression/PaccDumpByteParityTests.cpp
// PLXADD multi-point proof: the canonical Fortran case
// polar/polar-visc-clark-y (PACC with save and dump files, ASEQ over
// several points with growing and shrinking NBL, NSIDE(1) >
// NBL(1)) is reproduced in production; the save (.pol) and dump
// file (.dump) must be byte-identical to the reference-generated
// artifacts. Reference: tests/fortran-reference/cases/polar/polar-visc-clark-y.
//
// No golden change: the reference artifacts are read unchanged
// and checked byte-by-byte without sha256.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <string>
#include <vector>

#include "TestSupport.h"
#ifdef _WIN32
#include "DumpParitySupport.h"
#endif

namespace {

const char* const kCaseDir =
    "tests/fortran-reference/cases/polar/polar-visc-clark-y";

bool readBytes(const std::string& path, std::string& out) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  out = buffer.str();
  return true;
}

void reportFailure(const std::string& message) {
  testing::reportFailure(__FILE__, __LINE__, message);
}

}  // namespace

TEST(regression, PaccDumpByteParityMatchesXfoil6996S09Target) {
  const char* configuredPath = std::getenv("XFOIL_CLI_PATH");
  const std::string program =
      configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";

 // Own, unique test directory per run; the
  // case artifacts lie collision-free and are removed by RAII.
  const testing::ScopedTempDirectory work("pacc-dump-parity");
  const std::string workDir = work.path().string();
  std::error_code error;

  // copy the input files of the referenced case into the working
  // directory (commands.in + input/*).
  std::filesystem::copy(std::filesystem::path(kCaseDir) / "commands.in",
                        std::filesystem::path(workDir) / "commands.in",
                        std::filesystem::copy_options::overwrite_existing,
                        error);
  if (error) {
    reportFailure("commands.in not copyable");
    return;
  }
  for (const auto& entry :
       std::filesystem::directory_iterator(std::filesystem::path(kCaseDir) /
                                          "input")) {
    std::filesystem::copy(entry.path(),
                          std::filesystem::path(workDir) / entry.path().filename(),
                          std::filesystem::copy_options::overwrite_existing,
                          error);
    if (error) {
      reportFailure("Input file not copyable: " +
                    entry.path().string());
      return;
    }
  }

  std::error_code pathError;
  const std::string programPath =
      std::filesystem::absolute(program, pathError).string();
  if (pathError) {
    reportFailure("Program path not determinable: " + program);
    return;
  }
  // PUB-W2: central portable process helper.
  testing::ProcessInvocation invocation;
  invocation.program = programPath;
  invocation.workingDirectory = workDir;
  invocation.stdinFile = "commands.in";
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  if (testing::runProcess(invocation) == -1) {
    reportFailure("CLI-Run not correct ended");
    return;
  }

  static const char* const kArtifacts[] = {"polar-visc-clark-y.pol",
                                           "polar-visc-clark-y.dump"};
  for (const char* artifact : kArtifacts) {
    std::string actual;
    std::string expected;
    if (!readBytes(std::string(workDir) + "/" + artifact, actual)) {
      reportFailure(std::string("generated artifact not readable: ") +
                    artifact);
      continue;
    }
    const std::string expectedPath =
        std::string(artifact) == "polar-visc-clark-y.dump"
            ? std::string(
                  "tests/data/xfoil-6.996/s09/polar-visc-clark-y.dump")
            : std::string(kCaseDir) + "/generated/" + artifact;
    if (!readBytes(expectedPath, expected)) {
      reportFailure(std::string("reference artifact not readable: ") +
                    artifact + " (" + expectedPath + ")");
      continue;
    }
#ifdef _WIN32
    // PUB-W2: PLXADD dumps store raw IEEE-754 records; on Windows the
    // records are compared through the decoded portable contract (see
    // DumpParitySupport.h). All other artifacts, in particular the
    // byte-contract .pol text files, stay byte-exact.
    if (std::string(artifact).size() > 5 &&
        std::string(artifact).compare(std::string(artifact).size() - 5, 5,
                                      ".dump") == 0) {
      const std::vector<std::uint8_t> actualBytes(actual.begin(),
                                                  actual.end());
      const std::vector<std::uint8_t> expectedBytes(expected.begin(),
                                                    expected.end());
      const std::string problems = testing::compareDumpsWindowsPortable(
          actualBytes, expectedBytes);
      if (!problems.empty()) {
        reportFailure(std::string("Dump portability contract violated: ") +
                      artifact + "\n      " + problems);
      }
      continue;
    }
#endif
    if (actual != expected) {
      reportFailure(std::string("Artifact not byte-identical: ") +
                    artifact + " (produces " + std::to_string(actual.size()) +
                    " Bytes, Reference " +
                    std::to_string(expected.size()) + " Bytes)");
    }
  }
}

// End: tests/regression/PaccDumpByteParityTests.cpp

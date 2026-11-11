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

// file: tests/regression/PaccInviscidDumpParityTests.cpp
// Inviscid PACC PLXADD dump proof: the canonical
// Fortran case polar/polar-inviscid-naca0012-aseq (NACA 0012, PANE,
// OPER, PACC with save and dump files, ASEQ 0 4 2) is reproduced in
// production. The save (.pol) and dump (.dump) files must be
// byte-identical to the reference-generated artifacts. Additionally
// the group expectation is checked: exactly one PLXADD point group
// (56+16+96+96 payload bytes, 4 records with markers = 296 bytes) per
// accepted point, and exactly one success message per point.
//
// No golden change: the reference artifacts are read unchanged
// and checked by byte comparison. The file-set assertion is only an
// additional assertion, not part of the contract.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <string>
#include <vector>

#include "TestSupport.h"
#include "DumpTargetSupport.h"
#include "LegacyDefaults.h"
#ifdef _WIN32
#include "DumpParitySupport.h"
#endif

namespace {

const char* const kCommandDir =
    "tests/fortran-reference/cases/polar/polar-inviscid-naca0012-aseq";
const char* const kReferenceDir =
    "tests/data/xfoil-6.996/s15/regression/polar-inviscid-naca0012-aseq";

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

int countOccurrences(const std::string& haystack, const std::string& needle) {
  int count = 0;
  std::string::size_type pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

}  // namespace

TEST(regression, PaccInviscidDumpByteParityMatchesFortranReference) {
  const char* configuredPath = std::getenv("XFOIL_CLI_PATH");
  const std::string program =
      configuredPath != nullptr ? configuredPath : "build/debug/xfoil.next";

 // Own, unique test directory per run; the
  // case artifacts lie collision-free and are removed by RAII.
  const testing::ScopedTempDirectory work("pacc-inviscid-dump-parity");
  const std::string workDir = work.path().string();
  std::error_code error;

  std::filesystem::copy(std::filesystem::path(kCommandDir) / "commands.in",
                        std::filesystem::path(workDir) / "commands.in",
                        std::filesystem::copy_options::overwrite_existing,
                        error);
  testing::writeCommandsWithXfoil699Defaults(
      workDir, std::filesystem::path(workDir) / "commands.in",
      std::filesystem::path(workDir) / "commands-with-old-defaults.in");
  if (error) {
    reportFailure("commands.in not copyable");
    return;
  }

  std::error_code pathError;
  const std::string programPath =
      std::filesystem::absolute(program, pathError).string();
  if (pathError) {
    reportFailure("Program path not determinable: " + program);
    return;
  }
  // PUB-W2: central portable process helper. LC_ALL=C is applied as a
  // POSIX environment prefix and via putenv around the cmd.exe call.
  testing::ProcessInvocation invocation;
  invocation.program = programPath;
  invocation.workingDirectory = workDir;
  invocation.stdinFile = "commands-with-old-defaults.in";
  invocation.stdoutFile = "stdout.txt";
  invocation.stderrFile = "stderr.txt";
  invocation.environment = {{"LC_ALL", "C"}};
  if (testing::runProcess(invocation) == -1) {
    reportFailure("CLI-Run not correct ended");
    return;
  }

  static const char* const kArtifacts[] = {"polar-inviscid-naca0012-aseq.pol",
                                           "polar-inviscid-naca0012-aseq.dump"};
  for (const char* artifact : kArtifacts) {
    std::string actual;
    std::string expected;
    if (!readBytes(std::string(workDir) + "/" + artifact, actual)) {
      reportFailure(std::string("generated artifact not readable: ") +
                    artifact);
      continue;
    }
    if (!readBytes(std::string(kReferenceDir) + "/generated/" + artifact,
                   expected)) {
      reportFailure(std::string("reference artifact not readable: ") +
                    artifact);
      continue;
    }
    if (std::string(artifact).size() > 5 &&
        std::string(artifact).compare(std::string(artifact).size() - 5, 5,
                                      ".dump") == 0) {
      expected = testing::withS12DumpHeaderVersion(std::move(expected));
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

  // additional structural assertions: header (2720 bytes incl.
  // marker) plus exactly three point groups of 296 bytes each (56+16+96+96
  // payload bytes plus 4 record-marker pairs).
  std::string dump;
  if (readBytes(std::string(workDir) + "/polar-inviscid-naca0012-aseq.dump",
                dump)) {
    const std::size_t expectedSize = 2720u + 3u * 296u;
    if (dump.size() != expectedSize) {
      reportFailure("Dump file size unexpected: " +
                    std::to_string(dump.size()) + " instead of " +
                    std::to_string(expectedSize));
    }
  }

  std::string stdoutText;
  if (readBytes(std::string(workDir) + "/stdout.txt", stdoutText)) {
    const int written =
        countOccurrences(stdoutText, " Point written to dump file ");
    if (written != 3) {
      reportFailure("Expected 3 dump success messages, found: " +
                    std::to_string(written));
    }
    const int added = countOccurrences(stdoutText, " Point added to stored polar");
    if (added != 3) {
      reportFailure("Expected 3 point samples, found: " +
                    std::to_string(added));
    }
  } else {
    reportFailure("stdout.txt not readable");
  }
}

// End: tests/regression/PaccInviscidDumpParityTests.cpp

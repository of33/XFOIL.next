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

// File: tests/unit/LicenseHeaderGateTests.cpp
// Persistent test of the license header gate. The tool's self-test
// covers the negative cases (missing header, wrong SPDX identifier,
// missing or not-permitted derived set, shebang outside line 1,
// injection into fortran-src, ignored data/golden/generated files) in
// isolated diagnostic copies; the second run checks the real
// working tree. The tool never writes.
//
// Portability contract (PUB-W1): the gate tool is launched through
// std::system() with the working directory of the test process. Only
// fixed, relative, shell-quoting-safe tokens appear in the command
// line (the repository path itself is never embedded), so the command
// is safe under POSIX /bin/sh and under the Windows command processor
// used by UCRT system(). Output is discarded through the platform null
// device (/dev/null, NUL). The process exit code is decoded per
// platform: POSIX via WIFEXITED/WEXITSTATUS on the wait status,
// MinGW/UCRT64 as the direct child exit code; a failed process start
// stays visible as -1.

#include "TestSupport.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace {

// Platform null sink for discarding stdout and stderr.
constexpr const char* kNullDevice =
#ifdef _WIN32
    "NUL";
#else
    "/dev/null";
#endif

// Decodes the result of std::system() into the child's exit code.
// -1 means: the process could not be started or did not terminate
// normally. On POSIX the value is a wait status; on Windows (MinGW/
// UCRT64) system() returns the child exit code directly.
int processExitCode(int status) {
  if (status == -1) {
    return -1;
  }
#ifdef _WIN32
  return status;
#else
  if (!WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
#endif
}

// Runs the given tool file (relative to the working directory) with
// the given arguments, discarding stdout and stderr. Returns the
// child exit code, -1 for a failed process start, and -2 when the
// tool file itself is missing (no failure reported: the caller
// decides how to treat it).
int toolExitCode(const std::string& toolPath, const std::string& arguments) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(toolPath, error)) {
    return -2;
  }
  const std::string command =
      "python3 " + toolPath + " " + arguments + " > " + kNullDevice + " 2>&1";
  const int status = std::system(command.c_str());
  return processExitCode(status);
}

// Runs the tool from the working tree. A missing tool is reported and
// yields -1.
int runTool(const std::string& arguments) {
  const int exitCode =
      toolExitCode("tools/check-license-headers.py", arguments);
  if (exitCode == -2) {
    testing::reportFailure(
        __FILE__, __LINE__,
        "Tool not found: tools/check-license-headers.py");
    return -1;
  }
  return exitCode;
}

}  // namespace

TEST(unit, LicenseHeaderGateSelfTestPasses) {
  CHECK_EQ(runTool("--self-test"), 0);
}

TEST(unit, LicenseHeaderGateAcceptsWorkingTree) {
  CHECK_EQ(runTool(""), 0);
}

TEST(unit, LicenseHeaderGateRejectsUnknownInvocation) {
  // An invocation the tool cannot honor must remain visible as a
  // nonzero exit code: it must never collapse into -1 or 0.
  const int exitCode =
      toolExitCode("tools/check-license-headers.py", "--no-such-mode");
  CHECK(exitCode != -2);
  CHECK(exitCode != -1);
  CHECK(exitCode != 0);
}

TEST(unit, LicenseHeaderGateMissingToolIsReportedAsMinusOne) {
  // A missing tool file is a controlled error signal (-2 inside the
  // helper, -1 for the reporting wrapper); it is never silently 0.
  CHECK_EQ(toolExitCode("tools/no-such-gate-tool.py", ""), -2);
}

TEST(unit, ProcessExitCodeDecodesPlatformStatus) {
  // Platform selection guard: the POSIX branch must decode a wait
  // status built as (exit code << 8); the Windows branch passes the
  // direct exit code through. Both must map a failed start (-1) to
  // -1.
  CHECK_EQ(processExitCode(-1), -1);
#ifndef _WIN32
  CHECK_EQ(processExitCode(0), 0);  // exited(0)
  CHECK_EQ(processExitCode(2 << 8), 2);
  CHECK_EQ(processExitCode(127 << 8), 127);
  // Not an exit status (killed by signal 9): stays -1.
  CHECK_EQ(processExitCode(9), -1);
#else
  CHECK_EQ(processExitCode(0), 0);
  CHECK_EQ(processExitCode(2), 2);
  CHECK_EQ(processExitCode(127), 127);
#endif
}

// End: tests/unit/LicenseHeaderGateTests.cpp

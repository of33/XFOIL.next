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

// File: tests/unit/PortableProcessCommandTests.cpp
// PUB-W2: contract tests for the central test-process helper. Both
// platform renderings are pure strings and are checked on every
// platform; actual Windows execution is part of the manual MSYS2
// acceptance.

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

// Used only by the POSIX smoke run (the Windows execution happens on
// the manual MSYS2 acceptance), hence the same platform guard.
#ifndef _WIN32
std::string readAll(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}
#endif  // !_WIN32

}  // namespace

TEST(unit, PortableProcessRenderingBasics) {
  testing::ProcessInvocation invocation;
  invocation.program = "/tmp/prog";
  invocation.arguments = {"arg one", "arg-two"};
  invocation.workingDirectory = "/tmp/dir with space";
  invocation.stdinFile = "in.txt";
  invocation.stdoutFile = "out.txt";
  invocation.stderrFile = "err.txt";
  invocation.environment = {{"LC_ALL", "C"}};

  const std::string posix = testing::renderPosixCommand(invocation);
  CHECK(posix.find("cd \"/tmp/dir with space\" && LC_ALL='C' ") == 0);
  CHECK(posix.find("\"/tmp/prog\"") != std::string::npos);
  CHECK(posix.find("'arg one'") != std::string::npos);
  CHECK(posix.find("< 'in.txt'") != std::string::npos);
  CHECK(posix.find("> 'out.txt'") != std::string::npos);
  CHECK(posix.find("2> 'err.txt'") != std::string::npos);

  const std::string windows = testing::renderWindowsCommand(invocation);
  // cmd.exe /c strips an outer quote pair, so the Windows rendering is
  // a parenthesised compound command (PUB-W2-WIN).
  CHECK(windows.size() > 3 && windows[0] == '(' && windows.back() == ')');
  CHECK(windows.find("( cd /d \"/tmp/dir with space\" && ") == 0);
  CHECK(windows.find("\"/tmp/prog\" \"arg one\" \"arg-two\"") !=
        std::string::npos);
  CHECK(windows.find("< \"in.txt\"") != std::string::npos);
  CHECK(windows.find("> \"out.txt\"") != std::string::npos);
  CHECK(windows.find("2> \"err.txt\"") != std::string::npos);
  // cmd.exe must never see a POSIX env-assignment prefix.
  CHECK(windows.find("LC_ALL=") == std::string::npos);
}

TEST(unit, PortableProcessCanonicalizeCapturedText) {
  CHECK_EQ(testing::canonicalizeCapturedText("a\r\nb\r\n"), std::string("a\nb\n"));
  CHECK_EQ(testing::canonicalizeCapturedText("a\nb\n"), std::string("a\nb\n"));
  // A lone CR is kept.
  CHECK_EQ(testing::canonicalizeCapturedText("a\rb"), std::string("a\rb"));
  CHECK_EQ(testing::canonicalizeCapturedText(""), std::string());
  CHECK_EQ(testing::canonicalizeCapturedText("\r\n"), std::string("\n"));
}

TEST(unit, PortableProcessResolveProgramKeepsExe) {
  testing::ScopedTempDirectory work("resolve-prog");
  const auto plain = work.makePath("tool.bin");
  {
    std::ofstream plainFile(plain, std::ios::binary);
    plainFile << "x";
  }
  std::string resolved;
  CHECK(testing::resolveProgram(plain.string(), resolved));
  CHECK_EQ(resolved, plain.string());  // suffix untouched

#ifdef _WIN32
  const auto exeBase = work.makePath("prod");
  {
    std::ofstream exeFile(exeBase.string() + ".exe", std::ios::binary);
    exeFile << "x";
  }
  CHECK(testing::resolveProgram(exeBase.string(), resolved));
  CHECK_EQ(resolved, exeBase.string() + ".exe");
#endif

  std::string missing;
  CHECK(!testing::resolveProgram(work.makePath("definitely-missing").string(),
                                 missing));
}

TEST(unit, PortableProcessRunMissingProgramIsControlled) {
  testing::ProcessInvocation invocation;
  invocation.program = "definitely-not-a-real-program-xfoil-test";
  CHECK_EQ(testing::runProcess(invocation), -1);
}

TEST(unit, PortableProcessRunSmokeRedirectsAndExitCode) {
  // POSIX smoke: an actual run through the renderer. The Windows smoke
  // execution happens on the manual MSYS2 acceptance.
#ifndef _WIN32
  testing::ScopedTempDirectory work("process-smoke");
  testing::ProcessInvocation invocation;
  invocation.program = "/bin/sh";
  invocation.arguments = {"-c", "echo outer-$XFOIL_TEST_VAR; "
                                "echo failure-detail 1>&2; exit 3"};
  invocation.workingDirectory = work.path().string();
  invocation.environment = {{"XFOIL_TEST_VAR", "value one"}};
  invocation.stdoutFile = "out.txt";
  invocation.stderrFile = "err.txt";
  const int exitCode = testing::runProcess(invocation);
  CHECK_EQ(exitCode, 3);
  CHECK_EQ(testing::canonicalizeCapturedText(
               readAll(work.makePath("out.txt"))),
           std::string("outer-value one\n"));
  CHECK_EQ(testing::canonicalizeCapturedText(
               readAll(work.makePath("err.txt"))),
           std::string("failure-detail\n"));
  // An empty stdout file is representable: run a silent command.
  invocation.arguments = {"-c", "exit 0"};
  invocation.environment.clear();
  CHECK_EQ(testing::runProcess(invocation), 0);
  CHECK_EQ(readAll(work.makePath("out.txt")), std::string());
#endif
}

// End: tests/unit/PortableProcessCommandTests.cpp

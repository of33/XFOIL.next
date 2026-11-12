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

// File: tests/integration/StateInvalidationS17aTests.cpp
// S17a end-to-end session invalidation after geometry and file commands.
// The tests compare the state reached through a mutation path with a fresh
// equivalent path and verify that query/export/failure paths preserve the
// current solution.

#include "TestSupport.h"

#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

const std::filesystem::path kClarkYPath = std::filesystem::absolute(
    "tests/fortran-reference/cases/aero-viscous/visc-clark-y/input/"
    "CLARK_Y.dat");
const std::filesystem::path kInte0Path = std::filesystem::absolute(
    "tests/data/core-c11a-cli-reference/inte_n0012.dat");
const std::filesystem::path kInte1Path = std::filesystem::absolute(
    "tests/data/core-c11a-cli-reference/inte_n2412.dat");

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
  result.exitCode =
      xfoil::app::runInteractiveApplication(input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
}

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

class S17aRun {
 public:
  S17aRun(const std::string& tag, const std::string& commands)
      : work_("s17a-" + tag, true), result_(runAfterSetup(commands)) {}

  [[nodiscard]] int exitCode() const noexcept { return result_.exitCode; }

  [[nodiscard]] const std::string& stdoutText() const noexcept {
    return result_.outputText;
  }

  [[nodiscard]] const std::string& stderrText() const noexcept {
    return result_.errorText;
  }

  [[nodiscard]] std::string read(const std::string& fileName) const {
    return readBytes(work_.makePath(fileName));
  }

  [[nodiscard]] bool exists(const std::string& fileName) const {
    return std::filesystem::exists(work_.makePath(fileName));
  }

 private:
  SessionResult runAfterSetup(const std::string& commands) {
    std::error_code error;
    std::filesystem::copy(kClarkYPath, work_.makePath("CLARK_Y.dat"),
                          std::filesystem::copy_options::overwrite_existing,
                          error);
    std::filesystem::copy(kInte0Path, work_.makePath("inte0.dat"),
                          std::filesystem::copy_options::overwrite_existing,
                          error);
    std::filesystem::copy(kInte1Path, work_.makePath("inte1.dat"),
                          std::filesystem::copy_options::overwrite_existing,
                          error);
    if (error) {
      testing::reportFailure(__FILE__, __LINE__,
                             "cannot stage S17a input files: " +
                                 error.message());
    }
    return runSession(commands);
  }

  testing::ScopedTempDirectory work_;
  SessionResult result_;
};

void checkSameFile(const S17aRun& left, const std::string& leftName,
                   const S17aRun& right, const std::string& rightName) {
  const std::string expected = left.read(leftName);
  CHECK(!expected.empty());
  CHECK_EQ(right.read(rightName), expected);
}

const char* const kLoadViscousPoint =
    "LOAD CLARK_Y.dat\nOPER\nVISC\n1e6\nALFA 4\n";

}  // namespace

TEST(integration, StateInvalidationS17aIntePreservesCurrentSolution) {
  const S17aRun run("inte-preserve",
                    "NACA 2412\nOPER\nVISC\n1e6\nALFA 4\nCPWR before.cp\n"
                    "\n\nINTE\nF\ninte0.dat\nF\ninte1.dat\n0.5\ninte05\n"
                    "\nOPER\nCPWR after.cp\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  checkSameFile(run, "before.cp", run, "after.cp");
}

TEST(integration, StateInvalidationS17aIntePcopMatchesFreshPath) {
  const S17aRun afterEdit(
      "inte-pcop-after-edit",
      "NACA 2412\nOPER\nVISC\n1e6\nALFA 4\n\n\n"
      "INTE\nF\ninte0.dat\nF\ninte1.dat\n0.5\ninte05\nPCOP\n"
      "OPER\nALFA 4\nCPWR result.cp\n\n\nQUIT\n");
  CHECK_EQ(afterEdit.exitCode(), 0);

  const S17aRun fresh(
      "inte-pcop-fresh",
      "INTE\nF\ninte0.dat\nF\ninte1.dat\n0.5\ninte05\nPCOP\n"
      "OPER\nVISC\n1e6\nALFA 4\nCPWR result.cp\n\n\nQUIT\n");
  CHECK_EQ(fresh.exitCode(), 0);
  checkSameFile(afterEdit, "result.cp", fresh, "result.cp");
}

TEST(integration, StateInvalidationS17aNacaAfterViscousMatchesFreshPath) {
  const S17aRun afterEdit(
      "naca-after-edit",
      std::string(kLoadViscousPoint) +
          "CPWR before.cp\n\n\nNACA 2412\nOPER\nALFA 4\n"
          "CPWR result.cp\n\n\nQUIT\n");
  CHECK_EQ(afterEdit.exitCode(), 0);

  const S17aRun fresh("naca-fresh",
                      "NACA 2412\nOPER\nVISC\n1e6\nALFA 4\n"
                      "CPWR result.cp\n\n\nQUIT\n");
  CHECK_EQ(fresh.exitCode(), 0);
  checkSameFile(afterEdit, "result.cp", fresh, "result.cp");
}

TEST(integration, StateInvalidationS17aLoadAfterViscousMatchesFreshPath) {
  const S17aRun afterLoad(
      "load-after-solution",
      std::string(kLoadViscousPoint) +
          "CPWR before.cp\n\n\nLOAD inte0.dat\nloaded file\n"
          "OPER\nALFA 4\nCPWR result.cp\n\n\nQUIT\n");
  CHECK_EQ(afterLoad.exitCode(), 0);

  const S17aRun fresh(
      "load-fresh",
      "LOAD inte0.dat\nloaded file\nOPER\nVISC\n1e6\nALFA 4\n"
      "CPWR result.cp\n\n\nQUIT\n");
  CHECK_EQ(fresh.exitCode(), 0);
  checkSameFile(afterLoad, "result.cp", fresh, "result.cp");
}

TEST(integration, StateInvalidationS17aGdesKeepsSolutionUntilExec) {
  const S17aRun run(
      "gdes-exec",
      std::string(kLoadViscousPoint) +
          "CPWR before.cp\n\n\nGDES\nSQTE\n\nOPER\nCPWR before-exec.cp\n"
          "\n\nGDES\nEXEC\n\nOPER\nALFA 4\nCPWR after-exec.cp\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  checkSameFile(run, "before.cp", run, "before-exec.cp");

  const S17aRun fresh(
      "gdes-fresh",
      "LOAD CLARK_Y.dat\nGDES\nSQTE\nEXEC\n\nOPER\nVISC\n1e6\nALFA 4\n"
      "CPWR after-exec.cp\n\n\nQUIT\n");
  CHECK_EQ(fresh.exitCode(), 0);
  checkSameFile(run, "after-exec.cp", fresh, "after-exec.cp");
}

TEST(integration, StateInvalidationS17aExportsDoNotInvalidateSolution) {
  const S17aRun run(
      "exports",
      std::string(kLoadViscousPoint) +
          "CPWR before.cp\nBLOU blou.bl\nDUMP dump.bl\nDMP dmp.bl\n"
          "CPWR after.cp\n\nSAVE saved.dat\n\nOPER\nCPWR after-save.cp\n"
          "\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  CHECK(run.exists("blou.bl"));
  CHECK(run.exists("dump.bl"));
  CHECK(run.exists("dmp.bl"));
  CHECK(run.exists("saved.dat"));
  checkSameFile(run, "before.cp", run, "after.cp");
  checkSameFile(run, "before.cp", run, "after-save.cp");
}

TEST(integration, StateInvalidationS17aFailedLoadKeepsPreviousState) {
  const S17aRun run(
      "failed-load",
      std::string(kLoadViscousPoint) +
          "CPWR before.cp\n\n\nLOAD missing-s17a.dat\n"
          "OPER\nCPWR after.cp\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  checkSameFile(run, "before.cp", run, "after.cp");
}

// End: tests/integration/StateInvalidationS17aTests.cpp

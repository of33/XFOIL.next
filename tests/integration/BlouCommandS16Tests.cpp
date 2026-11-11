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

// File: tests/integration/BlouCommandS16Tests.cpp
// End-to-end contract of the hidden 6.996 OPER command BLOU
// (BLDUMP3) against stock-6.996 oracle artifacts. The fixtures live
// under tests/data/xfoil-6.996/s16/.

#include "TestSupport.h"

#include "app/InteractiveApplication.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace {

const char* const kClarkYFixture =
    "tests/fortran-reference/cases/aero-viscous/visc-clark-y/input/CLARK_Y.dat";

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
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
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

const std::filesystem::path kFixtureRoot =
    std::filesystem::absolute("tests/data/xfoil-6.996/s16");

std::string fixture(const std::string& relative) {
  return readBytes(kFixtureRoot / relative);
}

class BlouRun {
 public:
  BlouRun(const std::string& tag, const std::string& commands,
          const std::string& preName = std::string{},
          const std::string& preText = std::string{})
      : source_(std::filesystem::absolute(kClarkYFixture)),
        work_("s16-blou-" + tag, true),
        result_(runAfterSetup(commands, preName, preText)) {}

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
  SessionResult runAfterSetup(const std::string& commands,
                              const std::string& preName,
                              const std::string& preText) {
    std::error_code error;
    std::filesystem::copy(source_, work_.makePath("CLARK_Y.dat"),
                          std::filesystem::copy_options::overwrite_existing,
                          error);
    if (error) {
      testing::reportFailure(__FILE__, __LINE__,
                             "cannot stage CLARK_Y.dat: " + error.message());
    }
    if (!preName.empty()) {
      std::ofstream pre(work_.makePath(preName), std::ios::binary);
      pre << preText;
    }
    return runSession(commands);
  }

  std::filesystem::path source_;
  testing::ScopedTempDirectory work_;
  SessionResult result_;
};

void checkFixture(const BlouRun& run, const std::string& producedName,
                  const std::string& fixtureName) {
  const std::string expected = fixture(fixtureName);
  if (expected.empty()) {
    testing::reportFailure(__FILE__, __LINE__,
                           "fixture not readable: " + fixtureName);
    return;
  }
  CHECK_EQ(run.read(producedName), expected);
}

const char* const kBlouPrefix =
    "LOAD CLARK_Y.dat\nOPER\nVISC\n1e6\nALFA 4\n";

}  // namespace

TEST(integration, BlouCommandS16InlineNameAndBytesMatchOracle) {
  const BlouRun run("inline", std::string(kBlouPrefix) +
                                  "BLOU produced.txt\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  checkFixture(run, "produced.txt", "blou-clark-y-re1e6-alfa4.bl");
  CHECK(!run.exists("CLARK_Y.bl"));
  CHECK(!run.exists("100.bl"));
}

TEST(integration, BlouCommandS16DefaultPromptUsesLoadedFilePrefix) {
  const BlouRun run("prompt-default", std::string(kBlouPrefix) +
                                          "BLOU\n\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  CHECK(run.stdoutText().find(" Enter filename:  CLARK_Y.bl") !=
        std::string::npos);
  checkFixture(run, "CLARK_Y.bl", "blou-clark-y-re1e6-alfa4.bl");
}

TEST(integration, BlouCommandS16NoPrefixPromptAcceptsName) {
  const BlouRun run(
      "prompt-no-prefix",
      "NACA 0012\nPANE\nOPER\nVISC\n1e6\nALFA 4\nBLOU\nnaca.bl\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  CHECK(run.stdoutText().find("Enter filename   s>  ") != std::string::npos);
  const std::string produced = run.read("naca.bl");
  CHECK(produced.rfind("#      x          y          s          b", 0) == 0);
  CHECK(produced.find('\n') != std::string::npos);
}

TEST(integration, BlouCommandS16NoViscousStateWritesCanonicalHeaderOnly) {
  const BlouRun run("no-stations",
                    "LOAD CLARK_Y.dat\nOPER\nBLOU no-stations.bl\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  checkFixture(run, "no-stations.bl", "blou-no-stations.bl");
}

TEST(integration, BlouCommandS16OpenFailureAbortsWithoutFile) {
  const BlouRun run("open-failure", std::string(kBlouPrefix) +
                                        "BLOU /no_such_dir_xyz/out.bl\n"
                                        "\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 2);
  CHECK(!run.exists("out.bl"));
}

TEST(integration, BlouCommandS16EofAtPromptEndsWithoutFile) {
  const BlouRun run("prompt-eof", std::string(kBlouPrefix) + "BLOU\n");
  CHECK_EQ(run.exitCode(), 0);
  CHECK(!run.exists("CLARK_Y.bl"));
}

TEST(integration, BlouCommandS16OverwritesExistingTargetWithoutPrompt) {
  const BlouRun run("overwrite", std::string(kBlouPrefix) +
                                     "BLOU produced.bl\n\n\nQUIT\n",
                    "produced.bl", "stale-content\n");
  CHECK_EQ(run.exitCode(), 0);
  CHECK(run.stdoutText().find("overwrite") == std::string::npos);
  checkFixture(run, "produced.bl", "blou-clark-y-re1e6-alfa4.bl");
}

TEST(integration, BlouCommandS16RepeatIsDeterministic) {
  const BlouRun run("repeat", std::string(kBlouPrefix) +
                                  "BLOU first.bl\nBLOU second.bl\n\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  const std::string first = run.read("first.bl");
  CHECK(!first.empty());
  CHECK_EQ(run.read("second.bl"), first);
  checkFixture(run, "first.bl", "blou-clark-y-re1e6-alfa4.bl");
}

TEST(integration, BlouCommandS16DoesNotMutateAnalysisState) {
  const BlouRun run("mutation",
                    std::string(kBlouPrefix) +
                        "CPWR before.cp\nBLOU trace.bl\nCPWR after.cp\n"
                        "\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  const std::string before = run.read("before.cp");
  CHECK(!before.empty());
  CHECK_EQ(run.read("after.cp"), before);
  checkFixture(run, "trace.bl", "blou-clark-y-re1e6-alfa4.bl");
}

TEST(integration, BlouCommandS16RemainsHiddenFromOperHelp) {
  const BlouRun run("help", "LOAD CLARK_Y.dat\nOPER\n?\n\nQUIT\n");
  CHECK_EQ(run.exitCode(), 0);
  CHECK(run.stdoutText().find("BLOU") == std::string::npos);
  CHECK(run.stdoutText().find("DUMP") != std::string::npos);
}

// End: tests/integration/BlouCommandS16Tests.cpp

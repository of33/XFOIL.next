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

// File: tests/integration/LibraryDemoL9Tests.cpp
// L9 end-to-end demo runs against the staged installation.

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "TestSupport.h"

namespace {

struct RunResult {
  int exitCode{-1};
  std::string standardOutput;
  std::string standardError;
  std::vector<std::string> filesInWorkingDirectory;
};

std::string environmentValue(const char* name) {
  const char* const value = std::getenv(name);
  return value == nullptr ? std::string{} : std::string{value};
}

bool hasText(const std::string& text, std::string_view needle) {
  return text.find(needle) != std::string::npos;
}

std::optional<std::size_t> stationCount(
    const std::string& text, std::string_view label) {
  const std::size_t position = text.find(label);
  if (position == std::string::npos) {
    return std::nullopt;
  }
  std::size_t begin = position + label.size();
  while (begin < text.size() && text[begin] == ' ') {
    ++begin;
  }
  std::size_t value = 0;
  const char* first = text.data() + begin;
  const char* last = text.data() + text.size();
  const auto parsed = std::from_chars(first, last, value);
  if (parsed.ec != std::errc{} || parsed.ptr == first) {
    return std::nullopt;
  }
  return value;
}

RunResult runDemo(
    const std::string& executable,
    const std::string& stage,
    const std::vector<std::string>& arguments,
    testing::ScopedTempDirectory& work,
    const std::string& tag) {
  RunResult result;
  const std::filesystem::path runDirectory = work.makePath("run-" + tag);
  std::filesystem::create_directories(runDirectory);
  const std::filesystem::path stdoutFile = work.makePath(tag + ".stdout");
  const std::filesystem::path stderrFile = work.makePath(tag + ".stderr");

  testing::ProcessInvocation invocation;
  invocation.program = executable;
  invocation.arguments = arguments;
  invocation.workingDirectory = runDirectory.string();
  invocation.stdoutFile = stdoutFile.string();
  invocation.stderrFile = stderrFile.string();
  if (!stage.empty()) {
#ifdef _WIN32
    const std::string binDirectory =
        (std::filesystem::path(stage) / "usr" / "bin").string();
    const std::string existingPath = environmentValue("PATH");
    invocation.environment.push_back(
        {"PATH", binDirectory + ";" + existingPath});
#else
    invocation.environment.push_back(
        {"LD_LIBRARY_PATH",
         (std::filesystem::path(stage) / "usr" / "lib").string()});
#endif
  }

  result.exitCode = testing::runProcess(invocation);
  testing::loadTextFile(stdoutFile.string(), result.standardOutput);
  testing::loadTextFile(stderrFile.string(), result.standardError);
  result.standardOutput =
      testing::canonicalizeCapturedText(result.standardOutput);
  result.standardError =
      testing::canonicalizeCapturedText(result.standardError);
  for (const auto& entry :
       std::filesystem::directory_iterator(runDirectory)) {
    result.filesInWorkingDirectory.push_back(
        entry.path().filename().string());
  }
  return result;
}

std::vector<std::string> baseArguments(
    const std::string& profile,
    std::string reynolds,
    std::string alphaStart,
    std::string alphaEnd,
    std::string alphaStep,
    std::string cl) {
  return {
      "--airfoil", profile,
      "--re", std::move(reynolds),
      "--alpha-start", std::move(alphaStart),
      "--alpha-end", std::move(alphaEnd),
      "--alpha-step", std::move(alphaStep),
      "--cl", std::move(cl)};
}

}  // namespace

TEST(integration, LibraryDemoL9InviscidStaticAndShared) {
  const std::string shared = environmentValue("XFOIL_DEMO_SHARED_PATH");
  const std::string staticDemo = environmentValue("XFOIL_DEMO_STATIC_PATH");
  const std::string profile = environmentValue("XFOIL_DEMO_PROFILE");
  const std::string stage = environmentValue("XFOIL_DEMO_STAGE");
  if (shared.empty() || staticDemo.empty() || profile.empty()) {
    return;
  }

  testing::ScopedTempDirectory work("l9-inviscid");
  const std::vector<std::string> arguments = baseArguments(
      profile, "0", "-2", "3", "1", "0.4");
  const RunResult sharedRun =
      runDemo(shared, stage, arguments, work, "shared");
  CHECK_EQ(sharedRun.exitCode, 0);
  CHECK(hasText(sharedRun.standardOutput, "XFOIL.next library demo"));
  CHECK(hasText(sharedRun.standardOutput, "Mode: inviscid"));
  CHECK(hasText(sharedRun.standardOutput, "Reynolds: 0"));
  CHECK(hasText(sharedRun.standardOutput, "Alpha sequence"));
  CHECK(hasText(sharedRun.standardOutput, "-2.0000"));
  CHECK(hasText(sharedRun.standardOutput, "2.0000"));
  CHECK_FALSE(hasText(sharedRun.standardOutput, "3.0000"));
  CHECK(hasText(sharedRun.standardOutput, "Specified CL"));
  CHECK(hasText(sharedRun.standardOutput, "target_CL"));
  CHECK(hasText(sharedRun.standardOutput, "Cf upper stations: n/a"));
  CHECK(hasText(sharedRun.standardOutput, "Cf lower stations: n/a"));
  CHECK(hasText(sharedRun.standardOutput, "Cp points:"));
  CHECK(sharedRun.filesInWorkingDirectory.empty());
  CHECK(sharedRun.standardError.empty());

  const RunResult staticRun =
      runDemo(staticDemo, stage, arguments, work, "static");
  CHECK_EQ(staticRun.exitCode, 0);
  CHECK(hasText(staticRun.standardOutput, "Mode: inviscid"));
  CHECK(hasText(staticRun.standardOutput, "Specified CL"));
  CHECK(hasText(staticRun.standardOutput, "Cf upper stations: n/a"));
  CHECK(hasText(staticRun.standardOutput, "Cf lower stations: n/a"));
  CHECK(staticRun.filesInWorkingDirectory.empty());
  CHECK(staticRun.standardError.empty());
}

TEST(integration, LibraryDemoL9ViscousAndOptionalValues) {
  const std::string shared = environmentValue("XFOIL_DEMO_SHARED_PATH");
  const std::string profile = environmentValue("XFOIL_DEMO_PROFILE");
  const std::string stage = environmentValue("XFOIL_DEMO_STAGE");
  if (shared.empty() || profile.empty()) {
    return;
  }

  testing::ScopedTempDirectory work("l9-viscous");
  const RunResult defaults = runDemo(
      shared, stage,
      baseArguments(profile, "1000000", "0", "4", "1", "0.5"),
      work, "viscous-defaults");
  CHECK(defaults.exitCode == 0 || defaults.exitCode == 6);
  CHECK(hasText(defaults.standardOutput, "Mode: viscous"));
  CHECK(hasText(defaults.standardOutput, "Reynolds: 1000000"));
  CHECK(hasText(defaults.standardOutput, "Specified CL"));
  CHECK(hasText(defaults.standardOutput, "CDF"));
  const std::optional<std::size_t> upper =
      stationCount(defaults.standardOutput, "Cf upper stations: ");
  const std::optional<std::size_t> lower =
      stationCount(defaults.standardOutput, "Cf lower stations: ");
  CHECK(upper.has_value());
  CHECK(lower.has_value());
  if (upper.has_value() && lower.has_value()) {
    CHECK(*upper > 0);
    CHECK(*lower > 0);
  }
  CHECK(hasText(defaults.standardOutput, "Cp points:"));
  CHECK(defaults.filesInWorkingDirectory.empty());

  std::vector<std::string> optional = baseArguments(
      profile, "2000000", "-1", "2", "0.5", "0.5");
  optional.push_back("--mach");
  optional.push_back("0.1");
  optional.push_back("--iterations");
  optional.push_back("150");
  optional.push_back("--panels");
  optional.push_back("200");
  const RunResult configured =
      runDemo(shared, stage, optional, work, "viscous-optional");
  CHECK(configured.exitCode == 0 || configured.exitCode == 6);
  CHECK(hasText(configured.standardOutput, "Mode: viscous"));
  CHECK(hasText(configured.standardOutput, "Mach: 0.1"));
  CHECK(hasText(configured.standardOutput, "Iterations: 150"));
  CHECK(hasText(configured.standardOutput, "Panels: 200"));
  CHECK(hasText(configured.standardOutput, "Specified CL"));
  CHECK(configured.filesInWorkingDirectory.empty());
}

TEST(integration, LibraryDemoL9NegativeSequence) {
  const std::string shared = environmentValue("XFOIL_DEMO_SHARED_PATH");
  const std::string profile = environmentValue("XFOIL_DEMO_PROFILE");
  const std::string stage = environmentValue("XFOIL_DEMO_STAGE");
  if (shared.empty() || profile.empty()) {
    return;
  }

  testing::ScopedTempDirectory work("l9-negative");
  const RunResult run = runDemo(
      shared, stage,
      baseArguments(profile, "0", "3", "-2", "-1", "-0.2"),
      work, "negative");
  CHECK_EQ(run.exitCode, 0);
  CHECK(hasText(run.standardOutput, "3.0000"));
  CHECK(hasText(run.standardOutput, "1.0000"));
  CHECK(hasText(run.standardOutput, "0.0000"));
  CHECK(hasText(run.standardOutput, "-1.0000"));
  CHECK_FALSE(hasText(run.standardOutput, "-2.0000"));
  CHECK(hasText(run.standardOutput, "Specified CL"));
  CHECK(run.filesInWorkingDirectory.empty());
}

TEST(integration, LibraryDemoL9ExitCodesAndNoOutputFiles) {
  const std::string shared = environmentValue("XFOIL_DEMO_SHARED_PATH");
  const std::string profile = environmentValue("XFOIL_DEMO_PROFILE");
  const std::string stage = environmentValue("XFOIL_DEMO_STAGE");
  if (shared.empty() || profile.empty()) {
    return;
  }

  testing::ScopedTempDirectory work("l9-errors");
  const RunResult unknown =
      runDemo(shared, stage, {"--bogus"}, work, "unknown");
  CHECK_EQ(unknown.exitCode, 2);
  CHECK(unknown.filesInWorkingDirectory.empty());

  const std::vector<std::string> missingProfile = {
      "--airfoil", work.makePath("missing-profile.dat").string(),
      "--re", "0", "--alpha-start", "0", "--alpha-end", "2",
      "--alpha-step", "1", "--cl", "0.2"};
  const RunResult missing =
      runDemo(shared, stage, missingProfile, work, "missing-profile");
  CHECK_EQ(missing.exitCode, 3);
  CHECK(missing.filesInWorkingDirectory.empty());

  std::vector<std::string> oversizedPanel = baseArguments(
      profile, "0", "0", "2", "1", "0.2");
  oversizedPanel.push_back("--panels");
  oversizedPanel.push_back("10000");
  const RunResult panelFailure =
      runDemo(shared, stage, oversizedPanel, work, "panel-failure");
  CHECK_EQ(panelFailure.exitCode, 4);
  CHECK(panelFailure.filesInWorkingDirectory.empty());

  std::vector<std::string> limited = baseArguments(
      profile, "1000000", "0", "2", "1", "0.5");
  limited.push_back("--iterations");
  limited.push_back("1");
  const RunResult limitedRun =
      runDemo(shared, stage, limited, work, "limited");
  CHECK_EQ(limitedRun.exitCode, 6);
  CHECK(hasText(limitedRun.standardOutput, "unconverged"));
  CHECK(hasText(limitedRun.standardOutput, "Specified CL"));
  CHECK(limitedRun.filesInWorkingDirectory.empty());
}

TEST(integration, LibraryDemoL9CrlfProfile) {
  const std::string shared = environmentValue("XFOIL_DEMO_SHARED_PATH");
  const std::string profile = environmentValue("XFOIL_DEMO_PROFILE");
  const std::string stage = environmentValue("XFOIL_DEMO_STAGE");
  if (shared.empty() || profile.empty()) {
    return;
  }

  testing::ScopedTempDirectory work("l9-crlf");
  std::string contents;
  CHECK(testing::loadTextFile(profile, contents));
  std::string crlf;
  crlf.reserve(contents.size() + 32);
  for (const char character : contents) {
    if (character == '\n') {
      crlf.push_back('\r');
    }
    crlf.push_back(character);
  }
  const std::filesystem::path crlfProfile = work.makePath("crlf.dat");
  {
    std::ofstream output(crlfProfile, std::ios::binary);
    CHECK(output.good());
    output << crlf;
  }

  const RunResult run = runDemo(
      shared, stage,
      baseArguments(crlfProfile.string(), "0", "-1", "2", "1", "0.2"),
      work, "crlf");
  CHECK_EQ(run.exitCode, 0);
  CHECK(hasText(run.standardOutput, "Airfoil: NACA 0012"));
  CHECK(hasText(run.standardOutput, "Mode: inviscid"));
  CHECK(run.filesInWorkingDirectory.empty());
}

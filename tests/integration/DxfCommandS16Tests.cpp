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

// File: tests/integration/DxfCommandS16Tests.cpp
// End-to-end hidden top-level DXF command contract: filename/prompt,
// overwrite, prefix, non-fatal path errors, EOF, hidden help and exact S14
// bytes.

#include "TestSupport.h"

#include "app/InteractiveApplication.h"
#include "app/cli/CommandInterpreter.h"
#include "xfoil/io/OutputPathPolicy.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace {

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

const std::filesystem::path kFixtureRoot = std::filesystem::absolute(
    std::filesystem::path("tests/data/xfoil-6.996/s14"));
const std::filesystem::path kClarkYPath =
    std::filesystem::absolute(std::filesystem::path("sample/CLARK_Y.dat"));

std::string fixture(const std::string& name) {
  return readBytes(kFixtureRoot / name);
}

void checkDxfBytes(const std::string& actual, std::size_t expectedSize) {
  const std::string expected = fixture("dxf-naca0012-240.dxf");
  CHECK_EQ(expected.size(), expectedSize);
  CHECK_EQ(actual, expected);
  CHECK(actual.find('\r') == std::string::npos);
  CHECK(!actual.empty());
  CHECK_EQ(actual.back(), 'F');
  CHECK_EQ(actual.rfind("EOF"), actual.size() - std::size_t{3});
  CHECK_EQ(actual.find("out.dxf"), std::string::npos);
}

class DxfCommandRun {
 public:
  explicit DxfCommandRun(const std::string& tag)
      : work_("s16-dxf-" + tag, true) {}

  SessionResult run(const std::string& commands) { return runSession(commands); }

  [[nodiscard]] std::string read(const std::string& name) const {
    return readBytes(work_.makePath(name));
  }

  [[nodiscard]] bool exists(const std::string& name) const {
    return std::filesystem::exists(work_.makePath(name));
  }

  [[nodiscard]] std::filesystem::path path(const std::string& name) const {
    return work_.makePath(name);
  }

 private:
  testing::ScopedTempDirectory work_;
};

void writeOldFile(const std::filesystem::path& path) {
  std::ofstream stream(path, std::ios::binary);
  stream << "OLD";
}

}  // namespace

TEST(integration, DxfCommandS16InlineWritesExactS14Bytes) {
  DxfCommandRun run("inline");
  const auto result =
      run.run("NACA 0012\nPANE\nDXF    out.dxf   \nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("DXF file not written.") ==
        std::string::npos);
  checkDxfBytes(run.read("out.dxf"), std::size_t{16013});
}

TEST(integration, DxfCommandS16PromptWritesSameBytes) {
  DxfCommandRun run("prompt");
  const auto result =
      run.run("NACA 0012\nPANE\nDXF\n  out.dxf  \nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("\nEnter output filename   s>  ") !=
        std::string::npos);
  checkDxfBytes(run.read("out.dxf"), std::size_t{16013});
}

TEST(integration, DxfCommandS16PromptHasNoLoadedPrefixDefault) {
  DxfCommandRun run("prefix-prompt");
  std::error_code error;
  std::filesystem::copy(kClarkYPath, run.path("CLARK_Y.dat"),
                        std::filesystem::copy_options::overwrite_existing,
                        error);
  CHECK(!error);
  const auto result =
      run.run("LOAD CLARK_Y.dat\nPANE\nDXF\nout.dxf\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("\nEnter output filename   s>  ") !=
        std::string::npos);
  CHECK(result.outputText.find("CLARK_Y.dxf") == std::string::npos);
  CHECK(!run.read("out.dxf").empty());
}

TEST(integration, DxfCommandS16KeepsMissingExtension) {
  DxfCommandRun run("noext");
  const auto result = run.run("NACA 0012\nPANE\nDXF noext\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(run.read("noext"), fixture("dxf-naca0012-240.dxf"));
  CHECK(!run.exists("noext.dxf"));
}

TEST(integration, DxfCommandS16OverwriteDefaultReplacesFile) {
  DxfCommandRun run("overwrite-default");
  writeOldFile(run.path("out.dxf"));
  const auto result = run.run("NACA 0012\nPANE\nDXF out.dxf\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Output file exists.  Overwrite?  Y") !=
        std::string::npos);
  CHECK_EQ(run.read("out.dxf"), fixture("dxf-naca0012-240.dxf"));
}

TEST(integration, DxfCommandS16DeclineKeepsExistingFile) {
  for (const std::string answer : {"N", "n"}) {
    DxfCommandRun run("decline");
    writeOldFile(run.path("out.dxf"));
    const auto result =
        run.run("NACA 0012\nPANE\nDXF out.dxf\n" + answer + "\nQUIT\n");
    CHECK_EQ(result.exitCode, 0);
    CHECK(result.outputText.find("DXF file not written.") !=
          std::string::npos);
    CHECK_EQ(run.read("out.dxf"), std::string("OLD"));
  }
}

TEST(integration, DxfCommandS16InvalidPathIsNonFatalAndContinues) {
  DxfCommandRun run("badpath");
  const auto result = run.run(
      "NACA 0012\nPANE\nDXF missing/out.dxf\nDXF good.dxf\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Bad filename.") != std::string::npos);
  CHECK(result.outputText.find("DXF file not written.") != std::string::npos);
  CHECK(!std::filesystem::exists(run.path("missing")));
  CHECK_EQ(run.read("good.dxf"), fixture("dxf-naca0012-240.dxf"));
}

TEST(integration, DxfCommandS16EofAtPromptWritesNoFile) {
  DxfCommandRun run("eof");
  const auto result = run.run("NACA 0012\nPANE\nDXF\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(!std::filesystem::exists(run.path("out.dxf")));
  CHECK(run.read("out.dxf").empty());
}

TEST(integration, DxfCommandS16OutputPathPolicyAppliedOnce) {
  DxfCommandRun run("policy");
  std::istringstream input("NACA 0012\nPANE\nDXF out.dxf\nQUIT\n");
  std::ostringstream output;
  std::ostringstream errorOutput;
  xfoil::app::cli::CommandInterpreter interpreter(input, output, errorOutput);
  xfoil::io::OutputPathPolicy policy;
  policy.prefix = "pre-";
  interpreter.setOutputPathPolicy(policy);
  const int exitCode =
      interpreter.run(xfoil::app::cli::InteractiveStartupOptions{});
  CHECK_EQ(exitCode, 0);
  CHECK_EQ(run.read("pre-out.dxf"), fixture("dxf-naca0012-240.dxf"));
  CHECK(!run.exists("out.dxf"));
  CHECK(!run.exists("pre-pre-out.dxf"));
}

TEST(integration, DxfCommandS16RepeatAndSessionStateUnchanged) {
  DxfCommandRun run("repeat");
  const auto result = run.run(
      "NACA 0012\nPANE\nPSAV before.dat\nDXF a.dxf\nDXF b.dxf\n"
      "PSAV after.dat\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(run.read("a.dxf"), fixture("dxf-naca0012-240.dxf"));
  CHECK_EQ(run.read("b.dxf"), fixture("dxf-naca0012-240.dxf"));
  CHECK_EQ(run.read("before.dat"), run.read("after.dat"));
}

TEST(integration, DxfCommandS16HiddenFromHelpAndSvgUnknown) {
  DxfCommandRun run("hidden-svg");
  const auto result =
      run.run("?\nNACA 0012\nPANE\nSVG out.svg\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("DXF") == std::string::npos);
  CHECK(result.outputText.find("SVG") != std::string::npos);
  CHECK(result.outputText.find("command not recognized") !=
        std::string::npos);
  CHECK(!run.exists("out.svg"));
}

// End: tests/integration/DxfCommandS16Tests.cpp

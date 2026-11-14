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

// file: tests/integration/OperPlotAliasTests.cpp
//-P3a: OPER plot alias classification pass. Parity between
// the canonical ASKC aliases and long forms (xoper.f 1181-1182,
// 1224-1225, 1472-1473, 1477-1478, 1720-1721), state-
// mutation freedom via output equivalence, menu synchronization, and
// delimitation against CPWR/CPIO/CPV/CPVI/P/alfa/V and genuinely
// unknown commands.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct AliasRunResult {
  int exitCode{};
  std::string outputText;
  // file names produced in the test working directory during the run
  // (excluding the copied start profile).
  std::vector<std::string> producedFiles;
};

AliasRunResult runSession(const std::string& inputText) {
  // Resolve the start profile absolutely before the working-directory
  // change and copy it into the test's own working directory: CPWR and
  // other file-writing commands create their files there exclusively,
  // not in the repository root directory.
  const std::filesystem::path startupProfile =
      std::filesystem::current_path() / "sample" / "CLARK_Y.dat";
  testing::ScopedTempDirectory work("OperPlotAlias-runSession", true);
  std::filesystem::copy_file(startupProfile, work.makePath("CLARK_Y.dat"),
                             std::filesystem::copy_options::overwrite_existing);

  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("CLARK_Y.dat");

  AliasRunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();

  std::error_code error;
  for (const auto& entry :
       std::filesystem::directory_iterator(work.path(), error)) {
    const std::string name = entry.path().filename().string();
    if (name != "CLARK_Y.dat") {
      result.producedFiles.push_back(name);
    }
  }
  return result;
}

bool produced(const AliasRunResult& result, const std::string& fileName) {
  return std::find(result.producedFiles.begin(), result.producedFiles.end(),
                   fileName) != result.producedFiles.end();
}

std::size_t countOccurrences(const std::string& text,
                             const std::string& needle) {
  std::size_t count = 0;
  std::size_t position = text.find(needle);
  while (position != std::string::npos) {
    ++count;
    position = text.find(needle, position + needle.size());
  }
  return count;
}

const char* const kPlotUnavailable =
    "Plotting is not available in this build.";

// Confirmed alias/long-form pairs from the Fortran dispatch.
const std::pair<std::string_view, std::string_view> kAliasPairs[] = {
    {"PP", "PPLO"}, {"AP", "APLO"}, {"CP", "CPX"},
    {"UE", "UEX"},  {"VP", "VPLO"},
};

}  // namespace

// 7.1: alias and long form are recognized and each report exactly once
// the plot unavailability; no unknown text, no blocked text.
TEST(integration, OperPlotAliasLangformParity) {
  for (const auto& [alias, langform] : kAliasPairs) {
    const std::string input =
        "OPER\n" + std::string(alias) + "\n" + std::string(langform) +
        "\n\nQUIT\n";
    const auto result = runSession(input);
    CHECK_EQ(result.exitCode, 0);
    CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
             std::size_t{2});
    CHECK(result.outputText.find("not recognized") == std::string::npos);
    CHECK(result.outputText.find("not available in this build yet") ==
          std::string::npos);
  }
}

// 7.2: state mutation freedom (observable): inserting a
// plot alias before a productive point chain leaves its output
// byte-identical; after removing the plot commands the output is
// identical to the reference without the alias.
TEST(integration, OperPlotAliasesLeaveSessionStateUnchanged) {
  const std::string reference =
      "OPER\nALFA 2\nPSUM\nCINC\n\nQUIT\n";
  const auto base = runSession(reference);
  CHECK_EQ(base.exitCode, 0);
  CHECK(base.outputText.find("OFS:") == std::string::npos);
  CHECK(base.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);

  for (const auto& [alias, langform] : kAliasPairs) {
    const std::string withAlias =
        "OPER\n" + std::string(alias) + "\n" + std::string(langform) +
        "\nALFA 2\nPSUM\nCINC\n\nQUIT\n";
    const auto result = runSession(withAlias);
    CHECK_EQ(result.exitCode, 0);
    CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
             std::size_t{2});
    // the production portion from the first point's output onward must be
    // byte-identical (the alias lines only add prompt/plot lines before).
    const char* const pointMarker =
        " Calculating unit vorticity distributions ...";
    const auto baseTail = base.outputText.substr(base.outputText.find(pointMarker));
    const auto aliasTail =
        result.outputText.substr(result.outputText.find(pointMarker));
    CHECK_EQ(aliasTail, baseTail);
  }
}

// 7.3: menu synchronization — the sequence command after a plot alias is
// processed correctly; the alias consumes no sequence line.
TEST(integration, OperPlotAliasKeepsMenuSynchronization) {
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nCP\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
           std::size_t{1});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
}

// 7.4 delimitation: a genuinely unknown command stays unknown.
TEST(integration, OperGenuinelyUnknownCommandStaysUnknown) {
  const auto result = runSession("OPER\nZZZZ\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("ZZZZ command not recognized") !=
        std::string::npos);
  CHECK(result.outputText.find(kPlotUnavailable) == std::string::npos);
}

// 7.4 delimitation: CPWR stays productive as file export, no plot text.
TEST(integration, OperCpwrRemainsProductiveNotPlot) {
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nALFA 2\nCPWR\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(kPlotUnavailable) == std::string::npos);
  CHECK(result.outputText.find("CPWR command not recognized") ==
        std::string::npos);
  CHECK(result.outputText.find("not available in this build yet") ==
        std::string::npos);
  // the PREFIX-dependent file name stays the domain-specific CLARK_Y.cp;
  // but it is created only in the test's own working directory.
  CHECK(produced(result, "CLARK_Y.cp"));
}

// 7.4 delimitation: CPIO/CPV/CPVI stay plot classification (one
// message per command), P/PACC and A/alfa stay productive.
TEST(integration, OperCpioCpvCpviAndProductiveAliasesUnchanged) {
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nCPIO\nCPV\nCPVI\nALFA 2\nA\n\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
           std::size_t{3});
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  // Alfa 2 and A run two points; the non-upstream OFS line is gone.
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

// 7.4 delimitation: the V/VISC alias stays productive (viscous prompt).
TEST(integration, OperViscAliasRemainsProductive) {
  const auto result = runSession(
      "NACA 0012\nPANE\nOPER\nVISC 1e6\nV\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(kPlotUnavailable) == std::string::npos);
  CHECK(result.outputText.find(".OPERv") != std::string::npos);
}

// 7.4 delimitation: TOP-menu plot commands (Z/U) stay unchanged.
TEST(integration, TopMenuPlotCommandsUnchanged) {
  const auto result = runSession("Z\nU\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
           std::size_t{2});
  CHECK(result.outputText.find("not recognized") == std::string::npos);
}

// 7.4 delimitation: the GDES plot boundary stays unchanged (Z in the
// GDES menu reports plot unavailability).
TEST(integration, GdesPlotBoundaryUnchanged) {
  const auto result = runSession("NACA 0012\nGDES\nZ\n\nQUIT\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find(kPlotUnavailable) != std::string::npos);
  CHECK(result.outputText.find("not recognized") == std::string::npos);
}

// Overall matrix: all long forms and aliases in one sequence; afterwards
// the menu state stays productively usable.
TEST(integration, OperPlotAliasFullMatrixSingleSequence) {
  std::string input = "NACA 0012\nPANE\nOPER\n";
  for (const auto& [alias, langform] : kAliasPairs) {
    input += std::string(alias) + "\n" + std::string(langform) + "\n";
  }
  input += "ALFA 2\n\nQUIT\n";
  const auto result = runSession(input);
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(countOccurrences(result.outputText, kPlotUnavailable),
           std::size_t{10});
  CHECK(result.outputText.find("not recognized") == std::string::npos);
  CHECK(result.outputText.find("OFS:") == std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
}

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

// File: tests/integration/OperSequenceTests.cpp
// tests for alpha and Cl sequences in the OPER menu (
// 16.3). All sequences run inviscid, so the single values are
// compared directly with repeated alfa/Cl commands.

#include "TestSupport.h"
#include "app/InteractiveApplication.h"

#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct SequenceRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

SequenceRunResult runOperSequenceSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  SequenceRunResult result;
  result.exitCode = xfoil::app::runInteractiveApplication(
      input, output, errorOutput, options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  return result;
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

SequenceRunResult runPaccSequenceSession(const std::string& commands) {
  return runOperSequenceSession("OPER\nPACC\n\n\n" + commands);
}

std::size_t collectPaccPointCount(const std::string& text) {
  return countOccurrences(text, " Point added to stored polar  1");
}

}  // namespace

TEST(integration, OperAseqRunsAscendingSequence) {
  const auto result =
      runPaccSequenceSession("ASEQ 0 2 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{3});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperAseqRunsDescendingSequence) {
  const auto result =
      runPaccSequenceSession("ASEQ 2 0 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{3});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperAseqMatchesRepeatedAlfaCommands) {
  const auto sequence =
      runPaccSequenceSession("ASEQ 1 2 1\n\nQUIT\n");
  const auto repeated =
      runPaccSequenceSession("ALFA 1\nALFA 2\n\nQUIT\n");
  CHECK_EQ(sequence.exitCode, 0);
  CHECK_EQ(repeated.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(sequence.outputText), std::size_t{2});
  CHECK_EQ(collectPaccPointCount(repeated.outputText), std::size_t{2});
}

TEST(integration, OperAseqZeroStepRunsSinglePointLikeFortran) {
  const auto result =
      runPaccSequenceSession("ASEQ 0 2 0\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{1});
}

TEST(integration, OperCseqRunsAscendingSequence) {
  const auto result =
      runPaccSequenceSession("CSEQ 0.4 0.8 0.2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{3});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperCseqMatchesRepeatedClCommands) {
  const auto sequence =
      runPaccSequenceSession("CSEQ 0.4 0.8 0.2\n\nQUIT\n");
  const auto repeated =
      runPaccSequenceSession("CL 0.4\nCL 0.6\nCL 0.8\n\nQUIT\n");
  CHECK_EQ(sequence.exitCode, 0);
  CHECK_EQ(repeated.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(sequence.outputText), std::size_t{3});
  CHECK_EQ(collectPaccPointCount(repeated.outputText), std::size_t{3});
}

TEST(integration, OperSequenceStagnatingStepRunsFortranPointCount) {
  const auto result = runPaccSequenceSession(
      "ASEQ 1e16 10000000000000004 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("too small") == std::string::npos);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{5});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperSequenceS17bBangAfterAseqRepeatsWholeSequence) {
  const auto result =
      runPaccSequenceSession("ASEQ 0 2 1\n!\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{6});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperSequenceS17bBangAfterCseqRepeatsWholeSequence) {
  const auto result =
      runPaccSequenceSession("CSEQ 0.4 0.8 0.2\n!\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(result.outputText), std::size_t{6});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperSequenceS17bBangAfterAliasesRepeatsWholeSequence) {
  const auto asResult =
      runPaccSequenceSession("AS 0 2 1\n!\n\nQUIT\n");
  const auto csResult =
      runPaccSequenceSession("CS 0.4 0.8 0.2\n!\n\nQUIT\n");
  CHECK_EQ(asResult.exitCode, 0);
  CHECK_EQ(csResult.exitCode, 0);
  CHECK_EQ(collectPaccPointCount(asResult.outputText), std::size_t{6});
  CHECK_EQ(collectPaccPointCount(csResult.outputText), std::size_t{6});
}

// end: tests/integration/OperSequenceTests.cpp

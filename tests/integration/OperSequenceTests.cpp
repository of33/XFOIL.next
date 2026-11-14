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
#include "app/cli/CommandInterpreter.h"

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
  std::vector<xfoil::app::cli::OperPointResult> points;
};

SequenceRunResult runOperSequenceSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  xfoil::app::cli::CommandInterpreter interpreter(input, output,
                                                   errorOutput);
  SequenceRunResult result;
  result.exitCode = interpreter.run(options);
  result.outputText = output.str();
  result.errorText = errorOutput.str();
  result.points = interpreter.lastRunSummary().pointResults;
  return result;
}

}  // namespace

TEST(integration, OperAseqRunsAscendingSequence) {
  const auto result = runOperSequenceSession("OPER\nASEQ 0 2 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  const auto& points = result.points;
  CHECK_EQ(points.size(), std::size_t{3});
  if (points.size() == 3) {
    CHECK_NEAR(points[0].alpha, 0.0, 1e-9, 0.0);
    CHECK_NEAR(points[1].alpha, 1.0, 1e-9, 0.0);
    CHECK_NEAR(points[2].alpha, 2.0, 1e-9, 0.0);
  }
}

TEST(integration, OperAseqRunsDescendingSequence) {
  // the original forces the step sign from start/end; a
  // positive inline step is negated for a descending sequence.
  const auto result = runOperSequenceSession("OPER\nASEQ 2 0 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  const auto& points = result.points;
  CHECK_EQ(points.size(), std::size_t{3});
  if (points.size() == 3) {
    CHECK_NEAR(points[0].alpha, 2.0, 1e-9, 0.0);
    CHECK_NEAR(points[1].alpha, 1.0, 1e-9, 0.0);
    CHECK_NEAR(points[2].alpha, 0.0, 1e-9, 0.0);
  }
}

TEST(integration, OperAseqMatchesRepeatedAlfaCommands) {
  const auto sequence = runOperSequenceSession("OPER\nASEQ 1 2 1\n\nQUIT\n");
  const auto repeated =
      runOperSequenceSession("OPER\nALFA 1\nALFA 2\n\nQUIT\n");
  CHECK_EQ(sequence.exitCode, 0);
  CHECK_EQ(repeated.exitCode, 0);

  const auto& sequencePoints = sequence.points;
  const auto& repeatedPoints = repeated.points;
  CHECK_EQ(sequencePoints.size(), std::size_t{2});
  CHECK_EQ(repeatedPoints.size(), std::size_t{2});
  if (sequencePoints.size() == repeatedPoints.size()) {
    for (std::size_t i = 0; i < sequencePoints.size(); ++i) {
      CHECK_NEAR(sequencePoints[i].alpha, repeatedPoints[i].alpha,
                 1e-9, 0.0);
      CHECK_NEAR(sequencePoints[i].cl, repeatedPoints[i].cl,
                 1e-6, 1e-4);
    }
  }
}

TEST(integration, OperAseqZeroStepRunsSinglePointLikeFortran) {
  // canonical Fortran contract (xoper.f ASEQ, oper-aseq-zero-step):
  // Fortran does not validate the step; for DAA=0 the
  // NPOINT formula returns a single point at the start value.
  const auto result = runOperSequenceSession("OPER\nASEQ 0 2 0\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  const auto& points = result.points;
  CHECK_EQ(points.size(), std::size_t{1});
  if (!points.empty()) {
    CHECK_NEAR(points[0].alpha, 0.0, 1e-9, 0.0);
  }
}

TEST(integration, OperCseqRunsAscendingSequence) {
  // point count per the original formula INT((END-START)/STEP + 0.5) + 1; for
  // 0.4..0.8/0.2 that is exactly three points.
  const auto result = runOperSequenceSession("OPER\nCSEQ 0.4 0.8 0.2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  const auto& points = result.points;
  CHECK_EQ(points.size(), std::size_t{3});
  if (points.size() == 3) {
    CHECK_NEAR(points[0].cl, 0.4, 1e-6, 0.0);
    CHECK_NEAR(points[1].cl, 0.6, 1e-6, 0.0);
    CHECK_NEAR(points[2].cl, 0.8, 1e-6, 0.0);
  }
}

TEST(integration, OperCseqMatchesRepeatedClCommands) {
  // The structured point records are the comparison surface; the
  // non-upstream OFS console line is not emitted in 1.0.1.
  const auto sequence = runOperSequenceSession("OPER\nCSEQ 0.4 0.8 0.2\n\nQUIT\n");
  const auto repeated =
      runOperSequenceSession("OPER\nCL 0.4\nCL 0.6\nCL 0.8\n\nQUIT\n");
  CHECK_EQ(sequence.exitCode, 0);
  CHECK_EQ(repeated.exitCode, 0);

  const auto& sequencePoints = sequence.points;
  CHECK_EQ(sequencePoints.size(), std::size_t{3});
  if (sequencePoints.size() == 3) {
    CHECK_NEAR(sequencePoints[0].cl, 0.4, 1e-4, 0.0);
    CHECK_NEAR(sequencePoints[1].cl, 0.6, 1e-4, 0.0);
    CHECK_NEAR(sequencePoints[2].cl, 0.8, 1e-4, 0.0);
  }
  CHECK_EQ(repeated.points.size(), std::size_t{3});
  CHECK(sequence.outputText.find("OFS:") == std::string::npos);
  CHECK(repeated.outputText.find("OFS:") == std::string::npos);
}

TEST(integration, OperSequenceStagnatingStepRunsFortranPointCount) {
  // canonical Fortran contract (xoper.f ASEQ-NPOINT): there is no
  // stagnation detection and no special message. The point count follows
  // the NPOINT formula; in the degenerate range around 1e16 degrees the
  // numeric values of the first points diverge because Fortran REAL is
  // computed in single precision, unlike the C++ core. The
  // structural contract (point count, no special message) is the test subject.
  const auto result = runOperSequenceSession(
      "OPER\nASEQ 1e16 10000000000000004 1\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("too small") == std::string::npos);
  CHECK_EQ(result.points.size(), std::size_t{5});
  CHECK(result.outputText.find("OFS:") == std::string::npos);
}

// end: tests/integration/OperSequenceTests.cpp

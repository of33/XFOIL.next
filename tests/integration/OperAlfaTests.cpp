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

// File: tests/integration/OperAlfaTests.cpp
// tests for the alpha single-point analysis in the OPER menu (
//). The reference comparison uses the same CLARK-Y geometry
// and the same analysis parameters via the modern XFoilSession facade.

#include "TestSupport.h"
#include "app/DatFileReader.h"
#include "app/InteractiveApplication.h"
#include "xfoil/XFoilSession.h"

#include <cstdio>
#include <optional>
#include <sstream>
#include <span>
#include <string>

namespace {

struct AlfaRunResult {
  int exitCode{};
  std::string outputText;
  std::string errorText;
};

AlfaRunResult runOperAlfaSession(const std::string& inputText) {
  std::istringstream input(inputText);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.inputPath = std::string("sample/CLARK_Y.dat");

  AlfaRunResult result;
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

// Reads the canonical OFS result line (xoper.f format 333) of the last
// point from the interpreter transcript.
bool parseResultLine(const std::string& text, double& alfa, double& cl,
                     double& cdp, double& cm) {
  const std::size_t position = text.rfind("OFS: alfa = ");
  if (position == std::string::npos) {
    return false;
  }
  const int matched = std::sscanf(text.c_str() + position,
                                  "OFS: alfa = %lf cl = %lf cdp = %lf cm = %lf",
                                  &alfa, &cl, &cdp, &cm);
  return matched == 4;
}

xfoil::geometry::AirfoilGeometry makeClarkYGeometry() {
  const auto read = xfoil::app::DatFileReader::read("sample/CLARK_Y.dat");
  CHECK(read.success());

  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign(
      read.airfoil.name,
      std::span<const xfoil::Point2D>(read.airfoil.coordinates));
  CHECK(assigned.success());
  return geometry;
}

}  // namespace

TEST(integration, OperAlfaRunsInviscidPoint) {
  const auto result = runOperAlfaSession("OPER\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("OFS: alfa =   2.0000") != std::string::npos);
  CHECK(result.outputText.find(
            " Calculating unit vorticity distributions ...") !=
        std::string::npos);
  // An inviscid point leaves no VISCAL block.
  CHECK(result.outputText.find(" Solving BL system ...") == std::string::npos);
  CHECK(result.outputText.find("CONVERGED") == std::string::npos);
  CHECK(result.errorText.empty());
}

TEST(integration, OperAlfaRunsViscousPoint) {
  const auto result =
      runOperAlfaSession("OPER\nVISC 1e6\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("OFS: alfa =   2.0000") != std::string::npos);
  CHECK(result.outputText.find(" Solving BL system ...") != std::string::npos);
  // format 9922 result block of the VISCAL point.
  CHECK(result.outputText.find("a =  2.000      CL = ") != std::string::npos);
  // No repetition on convergence.
  CHECK(result.outputText.find("continue iterating") ==
        std::string::npos);
}

TEST(integration, OperAlfaAcceptsInlineArgument) {
  const auto result = runOperAlfaSession("OPER\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Enter angle of attack") ==
        std::string::npos);
  CHECK(result.outputText.find("OFS: alfa =   2.0000") != std::string::npos);
}

TEST(integration, OperAlfaPromptsForMissingArgument) {
  const auto result = runOperAlfaSession("OPER\nALFA\n3\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  CHECK(result.outputText.find("Enter angle of attack (deg)   r>  ") !=
        std::string::npos);
  CHECK(result.outputText.find("OFS: alfa =   3.0000") != std::string::npos);
}

TEST(integration, OperAlfaRejectsInvalidArgument) {
  const auto result = runOperAlfaSession("OPER\nALFA xyz\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);
  // canonical Fortran contract (GETFLT + ASKR): an invalid inline value
  // leads to the ASKR prompt; the following empty line keeps the previous
  // alpha (0) and the point runs with that required value.
  CHECK(result.outputText.find("Enter angle of attack (deg)   r>  ") !=
        std::string::npos);
  CHECK(result.outputText.find("OFS: alfa =   0.0000") != std::string::npos);
  // prompt counting: OPER entry, prompt after the point, third prompt
  // before EOF (QUIT is not recognized in the OPER menu).
  CHECK_EQ(countOccurrences(result.outputText, "\n.OPERi   c>  "),
           std::size_t{3});
}

TEST(integration, OperAlfaMatchesModernSessionReference) {
  const auto result = runOperAlfaSession("OPER\nALFA 2\n\nQUIT\n");
  CHECK_EQ(result.exitCode, 0);

  double alfa = 0.0;
  double cl = 0.0;
  double cdp = 0.0;
  double cm = 0.0;
  CHECK(parseResultLine(result.outputText, alfa, cl, cdp, cm));
  CHECK_NEAR(alfa, 2.0, 1e-4, 0.0);

  // reference: same geometry and parameters via the modern facade.
  xfoil::XFoilSession session;
  CHECK(session.setGeometry(makeClarkYGeometry()).success());

  xfoil::AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 0.0;
  config.machNumber = 0.0;
  config.maximumIterations = 20;
  CHECK(session.configure(config).success());

  const xfoil::AnalysisResult reference =
      session.analyzeAtAlpha(xfoil::Degrees{2.0});
  CHECK(reference.successful);
  CHECK(reference.converged);

  // Step-03 tolerances; the CLI output returns six significant
  // digits, therefore an additional relative slack.
  // OFS (FORMAT 333) prints four decimal places; the tolerance matches
  // the rounding of the output precision, not the solver accuracy.
  CHECK_NEAR(cl, reference.liftCoefficient, 5e-5, 0.0);
  CHECK_NEAR(cm, reference.momentCoefficient, 5e-5, 0.0);
  static_cast<void>(cdp);
}

// End: tests/integration/OperAlfaTests.cpp

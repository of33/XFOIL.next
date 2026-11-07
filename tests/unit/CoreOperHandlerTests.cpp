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

// File: tests/unit/CoreOperHandlerTests.cpp
// Permanent tests of the OPER
// operating-state handlers VISC/Re/Mach/TYPE/ITER. The tests compare the
// handler output against the raw stdout references of the canonical
// Fortran processes and check state updates, invalidation,
// transactionality on abort, and absence of analysis and panel mutation.
// the production CLI route stays untouched.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "app/DatFileReader.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using namespace xfoil::app::cli;

xfoil::geometry::AirfoilGeometry makeNaca0012Geometry() {
  constexpr int pointsPerSide = 41;
  auto thickness = [](double x) {
    constexpr double t = 0.12;
    return 5.0 * t *
           (0.2969 * std::sqrt(x) - 0.1260 * x - 0.3516 * x * x +
            0.2843 * x * x * x - 0.1015 * x * x * x * x);
  };

  std::vector<xfoil::Point2D> points;
  for (int i = 0; i < pointsPerSide; ++i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(pointsPerSide - 1);
    points.push_back(xfoil::Point2D{x, thickness(x)});
  }
  for (int i = pointsPerSide - 2; i >= 0; --i) {
    const double x =
        1.0 - static_cast<double>(i) / static_cast<double>(pointsPerSide - 1);
    points.push_back(xfoil::Point2D{x, -thickness(x)});
  }

  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign("NACA 0012",
                                        std::span<const xfoil::Point2D>(points));
  CHECK(assigned.success());
  return geometry;
}

std::string readBinaryFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return std::string();
  }
  const std::streamoff size = in.tellg();
  if (size < 0) {
    return std::string();
  }
  std::string text(static_cast<std::size_t>(size), '\0');
  in.seekg(0, std::ios::beg);
  in.read(text.data(), size);
  text.resize(static_cast<std::size_t>(in.gcount()));
  return text;
}

std::string lineWith(const std::string& text, const std::string& needle) {
  const std::size_t start = text.find(needle);
  if (start == std::string::npos) {
    return std::string();
  }
  const std::size_t end = text.find('\n', start);
  if (end == std::string::npos) {
    return text.substr(start);
  }
  return text.substr(start, end - start);
}

void initializeFortranOperatingDefaults(session::SessionComposition& session) {
  session::setMachReynoldsTypes(session, 1, 1);
  session::setReynoldsBaseNumber(session, 0.0);
  session::setMachNumber(session, 0.0);
  session::setViscousMode(session, false);
  session::setIterationLimit(session, 20);
}

session::SessionComposition makeFortranOperSession() {
  session::SessionComposition session;
  CHECK(session::setGeometry(session, makeNaca0012Geometry()).success());
  CHECK(session::copyBufferToCurrent(session).ok);
  initializeFortranOperatingDefaults(session);
  return session;
}

// CLARK-Y from sample/CLARK_Y.dat (geometry identical to the
// c11a_Mach_postproc harness).
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

// point setup as in the c11a_Mach_postproc harness: Clark Y -> ABCOPY,
// Re=1e6, alpha 2 deg, Mach=0. maximumIterations=30 lets VISCAL converge
// at iteration 5 in a single call (the same point as the LRECALC harness
// step 5). viscous selects the viscous or inviscid point. After the
// return, session.viscous.viscals holds the resulting point state
// (GAM/QINV/QVIS/TAU/mapping), with Mach post-processed.
session::SessionComposition makeClarkYOperPointSession(bool viscous) {
  session::SessionComposition session;
  CHECK(session::setGeometry(session, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = viscous;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 30;
  CHECK(session::configure(session, config).success());
  CHECK(session::initializeAnalysis(session));
  const xfoil::AnalysisResult point =
      session::runSpecAlpha(session, xfoil::Degrees{2.0}, nullptr);
  CHECK(point.successful);
  return session;
}

// Index of the smallest value (0-based) for panel/wake placement of the
// Cp minimum.
std::size_t minIndex(const std::vector<double>& v) {
  std::size_t idx = 0;
  for (std::size_t i = 1; i < v.size(); ++i) {
    if (v[i] < v[idx]) {
      idx = i;
    }
  }
  return idx;
}

constexpr const char* kViscToggleStdout =
    "tests/fortran-reference/cases/oper-state/oper-visc-toggle/stdout.raw.txt";
constexpr const char* kViscInlineStdout =
    "tests/fortran-reference/cases/oper-state/oper-visc-inline/stdout.raw.txt";
constexpr const char* kRePromptStdout =
    "tests/fortran-reference/cases/oper-state/oper-re-prompt/stdout.raw.txt";
constexpr const char* kMachInlineStdout =
    "tests/fortran-reference/cases/oper-state/oper-mach-inline/stdout.raw.txt";
constexpr const char* kMachPromptStdout =
    "tests/fortran-reference/cases/oper-state/oper-mach-prompt/stdout.raw.txt";
constexpr const char* kMachSupersonicStdout =
    "tests/fortran-reference/cases/oper-state/oper-mach-supersonic/"
    "stdout.raw.txt";
constexpr const char* kType1Stdout =
    "tests/fortran-reference/cases/oper-state/oper-type-1/stdout.raw.txt";
constexpr const char* kType2Stdout =
    "tests/fortran-reference/cases/oper-state/oper-type-2/stdout.raw.txt";
constexpr const char* kType3Stdout =
    "tests/fortran-reference/cases/oper-state/oper-type-3/stdout.raw.txt";
constexpr const char* kTypeInvalidStdout =
    "tests/fortran-reference/cases/oper-state/oper-type-invalid/"
    "stdout.raw.txt";
constexpr const char* kIterPromptStdout =
    "tests/fortran-reference/cases/oper-state/oper-iter-prompt/stdout.raw.txt";
constexpr const char* kIterInvalidStdout =
    "tests/fortran-reference/cases/oper-state/oper-iter-invalid/"
    "stdout.raw.txt";

}  // namespace

TEST(unit, CoreOperViscToggleMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in("1000000\n");
  std::ostringstream out;

  const auto status = handleOperViscCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperViscCoreStatus::toggledOn));

  const std::string reference = readBinaryFile(kViscToggleStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " M  ="), lineWith(reference, " M  ="));
  CHECK_EQ(lineWith(text, " Re ="), lineWith(reference, " Re ="));
  CHECK(text.find("Enter Reynolds number   r>  ") != std::string::npos);

  CHECK(session.operating.viscous);
  CHECK_NEAR(session.operating.machReynolds.reinf1, 1000000.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.reinf, 0.0, 0.0, 0.0);
  CHECK_FALSE(session.viscous.converged);
  CHECK_FALSE(session.viscous.viscals.lvconv);

  std::ostringstream offOut;
  const auto off = handleOperViscCore(session, in, offOut, "");
  CHECK_EQ(static_cast<int>(off),
           static_cast<int>(OperViscCoreStatus::toggledOff));
  CHECK_EQ(offOut.str(), std::string());
  CHECK_FALSE(session.operating.viscous);
  CHECK_NEAR(session.operating.machReynolds.reinf1, 1000000.0, 0.0, 0.0);
}

TEST(unit, CoreOperViscInlineMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in;
  std::ostringstream out;

  const auto status = handleOperViscCore(session, in, out, "1000000");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperViscCoreStatus::toggledOn));

  const std::string reference = readBinaryFile(kViscInlineStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " M  ="), lineWith(reference, " M  ="));
  CHECK_EQ(lineWith(text, " Re ="), lineWith(reference, " Re ="));
  CHECK(session.operating.viscous);
  CHECK_NEAR(session.operating.machReynolds.reinf1, 1000000.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.reinf, 0.0, 0.0, 0.0);
}

TEST(unit, CoreOperReInlineUpdatesDerivedReynoldsWithoutText) {
  auto session = makeFortranOperSession();
  std::istringstream in;
  std::ostringstream out;

  const auto status = handleOperReCore(session, in, out, "1000000");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperReCoreStatus::applied));
  CHECK_EQ(out.str(), std::string());
  CHECK_NEAR(session.operating.machReynolds.reinf1, 1000000.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.reinf, 1000000.0, 0.0, 0.0);
  CHECK_FALSE(session.viscous.converged);
}

TEST(unit, CoreOperRePromptMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in("1000000\n");
  std::ostringstream out;

  const auto status = handleOperReCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperReCoreStatus::applied));

  const std::string reference = readBinaryFile(kRePromptStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Currently..."), lineWith(reference, " Currently..."));
  CHECK_EQ(lineWith(text, " Re ="), lineWith(reference, " Re ="));
  CHECK(text.find("Enter new Reynolds number   r>  ") != std::string::npos);
  CHECK_NEAR(session.operating.machReynolds.reinf1, 1000000.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.reinf, 1000000.0, 0.0, 0.0);
}

TEST(unit, CoreOperMachInlineMatchesFortranSonicCp) {
  auto session = makeFortranOperSession();
  std::istringstream in;
  std::ostringstream out;

  const auto status = handleOperMachCore(session, in, out, "0.3");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperMachCoreStatus::applied));

  const std::string reference = readBinaryFile(kMachInlineStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Sonic Cp ="), lineWith(reference, " Sonic Cp ="));
  CHECK_NEAR(session.operating.machReynolds.minf1, 0.3, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.minf, 0.3, 0.0, 0.0);
  CHECK_FALSE(session.viscous.converged);
}

TEST(unit, CoreOperMachPromptMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in("0.3\n");
  std::ostringstream out;

  const auto status = handleOperMachCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperMachCoreStatus::applied));

  const std::string reference = readBinaryFile(kMachPromptStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Currently..."), lineWith(reference, " Currently..."));
  CHECK_EQ(lineWith(text, " M  ="), lineWith(reference, " M  ="));
  CHECK_EQ(lineWith(text, " Sonic Cp ="), lineWith(reference, " Sonic Cp ="));
  CHECK(text.find("Enter Mach number   r>  ") != std::string::npos);
}

TEST(unit, CoreOperMachSupersonicMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in("0.3\n");
  std::ostringstream out;

  const auto status = handleOperMachCore(session, in, out, "1.0");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperMachCoreStatus::applied));

  const std::string reference = readBinaryFile(kMachSupersonicStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Supersonic freestream not allowed"),
           lineWith(reference, " Supersonic freestream not allowed"));
  CHECK_EQ(lineWith(text, " M  ="), lineWith(reference, " M  ="));
  CHECK_EQ(lineWith(text, " Sonic Cp ="), lineWith(reference, " Sonic Cp ="));
  CHECK_NEAR(session.operating.machReynolds.minf1, 0.3, 0.0, 0.0);
}

TEST(unit, CoreOperTypeEachSupportedTypeMatchesFortranRawLine) {
  struct Expected {
    int type;
    int machType;
    int reType;
    const char* stdoutPath;
  };
  const Expected cases[] = {
      {1, 1, 1, kType1Stdout},
      {2, 2, 2, kType2Stdout},
      {3, 1, 3, kType3Stdout},
  };

  for (const Expected& expected : cases) {
    auto session = makeFortranOperSession();
    std::istringstream in;
    std::ostringstream out;
    const std::string argument = std::to_string(expected.type);

    const auto status = handleOperTypeCore(session, in, out, argument);
    CHECK_EQ(static_cast<int>(status),
             static_cast<int>(OperTypeCoreStatus::applied));

    const std::string reference = readBinaryFile(expected.stdoutPath);
    const std::string text = out.str();
    CHECK_EQ(lineWith(text, " M  ="), lineWith(reference, " M  ="));
    CHECK_EQ(lineWith(text, " Re ="), lineWith(reference, " Re ="));
    CHECK_EQ(session.operating.machReynolds.machType, expected.machType);
    CHECK_EQ(session.operating.machReynolds.reType, expected.reType);
  }
}

TEST(unit, CoreOperTypeInvalidMatchesFortranTableAndPrompt) {
  auto session = makeFortranOperSession();
  std::istringstream in("2\n");
  std::ostringstream out;

  const auto status = handleOperTypeCore(session, in, out, "0");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperTypeCoreStatus::applied));

  const std::string reference = readBinaryFile(kTypeInvalidStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Type   parameters held constant"),
           lineWith(reference, " Type   parameters held constant"));
  CHECK_EQ(lineWith(text, " M  ="), lineWith(reference, " M  ="));
  CHECK_EQ(lineWith(text, " Re ="), lineWith(reference, " Re ="));
  CHECK(text.find("Enter type of Mach,Re variation with CL   i>  ") !=
        std::string::npos);
  CHECK_EQ(session.operating.machReynolds.machType, 2);
  CHECK_EQ(session.operating.machReynolds.reType, 2);
}

TEST(unit, CoreOperIterInlineStoresValueWithoutText) {
  auto session = makeFortranOperSession();
  std::istringstream in;
  std::ostringstream out;

  const auto status = handleOperIterCore(session, in, out, "30");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperIterCoreStatus::applied));
  CHECK_EQ(out.str(), std::string());
  CHECK_EQ(session.operating.maximumIterations, 30);
}

TEST(unit, CoreOperIterPromptMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in("30\n");
  std::ostringstream out;

  const auto status = handleOperIterCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperIterCoreStatus::applied));

  const std::string reference = readBinaryFile(kIterPromptStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Current iteration limit:"),
           lineWith(reference, " Current iteration limit:"));
  CHECK(text.find("Enter new iteration limit   i>  ") != std::string::npos);
  CHECK_EQ(session.operating.maximumIterations, 30);
}

TEST(unit, CoreOperIterInvalidMatchesFortranRawLine) {
  auto session = makeFortranOperSession();
  std::istringstream in("30\n");
  std::ostringstream out;

  const auto status = handleOperIterCore(session, in, out, "0");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(OperIterCoreStatus::applied));

  const std::string reference = readBinaryFile(kIterInvalidStdout);
  const std::string text = out.str();
  CHECK_EQ(lineWith(text, " Current iteration limit:"),
           lineWith(reference, " Current iteration limit:"));
  CHECK_EQ(session.operating.maximumIterations, 30);
}

TEST(unit, CoreOperInvalidInlineAbortsWithoutPartialMutation) {
  auto machSession = makeFortranOperSession();
  std::istringstream machIn;
  std::ostringstream machOut;
  const auto machStatus =
      handleOperMachCore(machSession, machIn, machOut, "1.0");
  CHECK_EQ(static_cast<int>(machStatus),
           static_cast<int>(OperMachCoreStatus::inputAborted));
  CHECK_NEAR(machSession.operating.machReynolds.minf1, 0.0, 0.0, 0.0);
  CHECK(machOut.str().find(" M  =    1.0000") != std::string::npos);

  auto iterSession = makeFortranOperSession();
  std::istringstream iterIn;
  std::ostringstream iterOut;
  const auto iterStatus =
      handleOperIterCore(iterSession, iterIn, iterOut, "0");
  CHECK_EQ(static_cast<int>(iterStatus),
           static_cast<int>(OperIterCoreStatus::inputAborted));
  CHECK_EQ(iterSession.operating.maximumIterations, 20);
  CHECK(iterOut.str().find(" Current iteration limit:           0") !=
        std::string::npos);

  auto typeSession = makeFortranOperSession();
  std::istringstream typeIn;
  std::ostringstream typeOut;
  const auto typeStatus =
      handleOperTypeCore(typeSession, typeIn, typeOut, "0");
  CHECK_EQ(static_cast<int>(typeStatus),
           static_cast<int>(OperTypeCoreStatus::inputAborted));
  CHECK_EQ(typeSession.operating.machReynolds.machType, 1);
  CHECK_EQ(typeSession.operating.machReynolds.reType, 1);
}

TEST(unit, CoreOperNegativeAndHugeValuesFollowFortranState) {
  auto reSession = makeFortranOperSession();
  std::istringstream reIn;
  std::ostringstream reOut;
  CHECK_EQ(static_cast<int>(handleOperReCore(reSession, reIn, reOut, "-1")),
           static_cast<int>(OperReCoreStatus::applied));
  CHECK_EQ(reOut.str(), std::string());
  CHECK_NEAR(reSession.operating.machReynolds.reinf1, -1.0, 0.0, 0.0);
  CHECK_NEAR(reSession.viscous.viscals.reinf, -1.0, 0.0, 0.0);

  auto machSession = makeFortranOperSession();
  std::istringstream machIn;
  std::ostringstream machOut;
  CHECK_EQ(
      static_cast<int>(handleOperMachCore(machSession, machIn, machOut, "-0.3")),
      static_cast<int>(OperMachCoreStatus::applied));
  CHECK(machOut.str().find(" Sonic Cp =") == std::string::npos);
  CHECK_NEAR(machSession.operating.machReynolds.minf1, -0.3, 0.0, 0.0);
  CHECK_NEAR(machSession.viscous.viscals.minf, -0.3, 0.0, 0.0);

  auto iterSession = makeFortranOperSession();
  std::istringstream iterIn;
  std::ostringstream iterOut;
  CHECK_EQ(static_cast<int>(handleOperIterCore(
               iterSession, iterIn, iterOut, "100000000000")),
           static_cast<int>(OperIterCoreStatus::applied));
  CHECK_EQ(iterSession.operating.maximumIterations, 1 << 30);

  auto typeSession = makeFortranOperSession();
  std::istringstream typeIn("2\n");
  std::ostringstream typeOut;
  CHECK_EQ(static_cast<int>(handleOperTypeCore(
               typeSession, typeIn, typeOut, "100000000000")),
           static_cast<int>(OperTypeCoreStatus::applied));
  CHECK_EQ(typeSession.operating.machReynolds.machType, 2);
  CHECK_EQ(typeSession.operating.machReynolds.reType, 2);
}

TEST(unit, CoreOperCommandsDoNotMutateGeometryPanelOrResult) {
  auto session = makeFortranOperSession();
  const std::size_t panelSize = session.paneling.panel.x.size();
  const std::uint64_t bufferRevision = session.geometry.bufferRevision;
  const std::uint64_t sourceRevision = session.paneling.panelSourceRevision;
  const auto origin = session.paneling.origin;
  const xfoil::AnalysisResult before = session.result.lastResult;

  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperViscCore(session, in, out, "1000000")),
           static_cast<int>(OperViscCoreStatus::toggledOn));
  CHECK_EQ(static_cast<int>(handleOperReCore(session, in, out, "2000000")),
           static_cast<int>(OperReCoreStatus::applied));
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  CHECK_EQ(static_cast<int>(handleOperTypeCore(session, in, out, "2")),
           static_cast<int>(OperTypeCoreStatus::applied));
  CHECK_EQ(static_cast<int>(handleOperIterCore(session, in, out, "30")),
           static_cast<int>(OperIterCoreStatus::applied));

  CHECK_EQ(session.paneling.panel.x.size(), panelSize);
  CHECK_EQ(session.geometry.bufferRevision, bufferRevision);
  CHECK_EQ(session.paneling.panelSourceRevision, sourceRevision);
  CHECK_EQ(static_cast<int>(session.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(static_cast<int>(session.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_FALSE(session.result.lastResult.successful);
  CHECK_EQ(session.result.lastResult.iterations, 0);
}

TEST(unit, CoreOperHandlersAreInstanceIndependent) {
  auto first = makeFortranOperSession();
  auto second = makeFortranOperSession();
  std::istringstream in;
  std::ostringstream out;

  CHECK_EQ(static_cast<int>(handleOperReCore(first, in, out, "1000000")),
           static_cast<int>(OperReCoreStatus::applied));
  CHECK_EQ(static_cast<int>(handleOperMachCore(first, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));

  CHECK_NEAR(first.operating.machReynolds.reinf1, 1000000.0, 0.0, 0.0);
  CHECK_NEAR(second.operating.machReynolds.reinf1, 0.0, 0.0, 0.0);
  CHECK_NEAR(first.operating.machReynolds.minf1, 0.3, 0.0, 0.0);
  CHECK_NEAR(second.operating.machReynolds.minf1, 0.0, 0.0, 0.0);
}

TEST(unit, CoreOperTextIsDeterministicThreeRuns) {
  auto run = [](std::ostringstream& out) {
    auto session = makeFortranOperSession();
    std::istringstream in;
    CHECK_EQ(
        static_cast<int>(handleOperViscCore(session, in, out, "1000000")),
        static_cast<int>(OperViscCoreStatus::toggledOn));
    CHECK_EQ(static_cast<int>(handleOperReCore(session, in, out, "2000000")),
             static_cast<int>(OperReCoreStatus::applied));
    CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
             static_cast<int>(OperMachCoreStatus::applied));
    CHECK_EQ(static_cast<int>(handleOperTypeCore(session, in, out, "2")),
             static_cast<int>(OperTypeCoreStatus::applied));
    CHECK_EQ(static_cast<int>(handleOperIterCore(session, in, out, "30")),
             static_cast<int>(OperIterCoreStatus::applied));
  };

  std::ostringstream first;
  std::ostringstream second;
  std::ostringstream third;
  run(first);
  run(second);
  run(third);
  CHECK_EQ(first.str(), second.str());
  CHECK_EQ(second.str(), third.str());
}

// Mach after an inviscid alpha point: immediate canonical post-processing
// (CLCALC/CDCALC on the retained GAM for the new MINF), no new solver run.
// Target values from tests/data/core-c11a-state-reference/
// c11a_Mach_postproc_harness_reference.txt (scenario B, It24.16).
TEST(unit, CoreOperMachAfterInviscidPointUpdatesLoadsMatchesFortran) {
  auto session = makeClarkYOperPointSession(/*viscous=*/false);
  const double clBefore = session.result.lastResult.liftCoefficient;
  CHECK_NEAR(clBefore, 6.5709867960858614e-01, 1.0e-9, 1.0e-9);

  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));

  CHECK_NEAR(session.result.lastResult.liftCoefficient,
             6.9882937318004035e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(session.result.lastResult.momentCoefficient,
             -9.5876212532565994e-02, 1.0e-9, 1.0e-9);
  // inviscid: CDCALC-ELSE branch -> Cd stays 0 (xfoil.f 1190-1194).
  CHECK_NEAR(session.result.lastResult.dragCoefficient, 0.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.cdp, -1.7335250771121464e-03, 1.0e-9,
             1.0e-9);
  CHECK_NEAR(session.viscous.viscals.minf, 0.3, 0.0, 0.0);
  CHECK_FALSE(session.viscous.converged);
}

// Mach after a converged viscous alpha point: Cl/CM/CDP (CLCALC) and Cd
// (CDCALC wake for the new TKLAM) are updated; CDF is preserved
// (TAU unchanged); LVCONV=false; BL state (ITRAN) unchanged.
// Target values: c11a_Mach_postproc_harness_reference.txt scenario C.
TEST(unit,
     CoreOperMachAfterViscousPointUpdatesLoadsWithoutTouchingCdfMatchesFortran) {
  auto session = makeClarkYOperPointSession(/*viscous=*/true);
  CHECK(session.viscous.converged);
  const std::array<int, 2> itranBefore = session.viscous.viscals.itran;
  const double cdfBefore = session.viscous.viscals.cdf;
  CHECK_NEAR(session.result.lastResult.liftCoefficient,
             6.4645431381471929e-01, 1.0e-9, 1.0e-9);

  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));

  CHECK_NEAR(session.result.lastResult.liftCoefficient,
             6.8756173455036773e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(session.result.lastResult.momentCoefficient,
             -9.3640220670966695e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(session.result.lastResult.dragCoefficient,
             5.8445220498272478e-03, 1.0e-9, 1.0e-9);
  CHECK_NEAR(session.viscous.viscals.cdp, -2.1433374867020619e-04, 1.0e-9,
             1.0e-9);
  // CDF unchanged (friction from the retained TAU; Mach does not re-solve the BL).
  CHECK_NEAR(session.viscous.viscals.cdf, cdfBefore, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.cdf, 4.1410670163606359e-03, 1.0e-9,
             1.0e-9);
  // LVCONV=false, but no new solver run: ITRAN is preserved.
  CHECK_FALSE(session.viscous.converged);
  CHECK_FALSE(session.viscous.viscals.lvconv);
  CHECK_EQ(session.viscous.viscals.itran[0], itranBefore[0]);
  CHECK_EQ(session.viscous.viscals.itran[1], itranBefore[1]);
}

// identical Mach value: post-processing runs again but the result stays
// unchanged (idempotence). Target values: scenario C C_AFTER == C_IDENT.
TEST(unit, CoreOperMachIdenticalValueIsIdempotentMatchesFortran) {
  auto session = makeClarkYOperPointSession(/*viscous=*/true);
  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const double clFirst = session.result.lastResult.liftCoefficient;
  const double cdFirst = session.result.lastResult.dragCoefficient;
  const double cmFirst = session.result.lastResult.momentCoefficient;
  const double cdpFirst = session.viscous.viscals.cdp;

  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  CHECK_NEAR(session.result.lastResult.liftCoefficient, clFirst, 0.0, 0.0);
  CHECK_NEAR(session.result.lastResult.dragCoefficient, cdFirst, 0.0, 0.0);
  CHECK_NEAR(session.result.lastResult.momentCoefficient, cmFirst, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.cdp, cdpFirst, 0.0, 0.0);
  CHECK_NEAR(session.result.lastResult.liftCoefficient,
             6.8756173455036773e-01, 1.0e-9, 1.0e-9);
}

// Mach without a previous analysis point: no invented result values.
// post-processing remains a no-op (GAM empty); lastResult stays
// unchanged and only MINF1/MINF and LVCONV are set. Harness scenario A
// shows the Fortran zero-GAM artifact (Cl ~ 0); the session does not
// report it as a result.
TEST(unit, CoreOperMachWithoutPriorPointDoesNotInventResult) {
  auto session = makeFortranOperSession();
  const xfoil::AnalysisResult before = session.result.lastResult;
  CHECK(session.viscous.viscals.gam.empty());

  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));

  CHECK_NEAR(session.operating.machReynolds.minf1, 0.3, 0.0, 0.0);
  CHECK_NEAR(session.viscous.viscals.minf, 0.3, 0.0, 0.0);
  CHECK(session.viscous.viscals.gam.empty());
  CHECK_EQ(static_cast<int>(session.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(session.result.lastResult.liftCoefficient, 0.0, 0.0, 0.0);
  CHECK_NEAR(session.result.lastResult.dragCoefficient, 0.0, 0.0, 0.0);
  CHECK_NEAR(session.result.lastResult.momentCoefficient, 0.0, 0.0, 0.0);
}

// Cp observability after Mach vs. canonical CPCALC+FCPMIN. Reference:
// tests/data/core-c11a-state-reference/c11a_mach_cp_harness_reference.txt
// (It24.16). Clark Y: N=151, NW=22 (viscous) or NW=0 (inviscid).
// Support indices 0=LE (CPI(1)), 74=middle (CPI(N/2)=CPI(75)), 150=TE (CPI(N)).
TEST(unit, CoreOperMachAfterInviscidPointDerivesCpMatchesFortran) {
  auto session = makeClarkYOperPointSession(/*viscous=*/false);
  CHECK_EQ(session.paneling.panel.x.size(), 151u);

  const session::CpObservation before = session::currentCpMinimum(session);
  CHECK(before.valid);
  CHECK_NEAR(before.cpInviscid[0], 4.4622410766803577e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.cpInviscid[74], 3.9113700429128007e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.cpInviscid[150], 4.4622410766803577e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.minimum.cpmni, -9.9608694841153800e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.minimum.xcpmni, 1.8498999999999999e-01, 1.0e-9, 1.0e-9);
  // FCPMIN inviscid: CPMNV=CPMNI, CPMN=CPMNI (xoper.f 1794-1797).
  CHECK_NEAR(before.minimum.cpmnv, -9.9608694841153800e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.minimum.cpmin, -9.9608694841153800e-01, 1.0e-9, 1.0e-9);
  CHECK(minIndex(before.cpInviscid) < session.paneling.panel.x.size());

  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));

  const session::CpObservation after = session::currentCpMinimum(session);
  CHECK(after.valid);
  CHECK_NEAR(after.cpInviscid[0], 4.6278442208506038e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.cpInviscid[74], 4.0618736614536405e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.cpInviscid[150], 4.6278442208506038e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.cpmni, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.xcpmni, 1.8498999999999999e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.cpmnv, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.cpmin, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.xcpmnv, after.minimum.xcpmni, 0.0, 0.0);
  CHECK(minIndex(after.cpInviscid) < session.paneling.panel.x.size());
}

TEST(unit, CoreOperMachAfterViscousPointDerivesCpMatchesFortran) {
  auto session = makeClarkYOperPointSession(/*viscous=*/true);
  CHECK_EQ(session.paneling.panel.x.size(), 151u);

  const session::CpObservation before = session::currentCpMinimum(session);
  CHECK(before.valid);
  CHECK_NEAR(before.cpInviscid[0], 4.4622410766803577e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.cpViscous[0], 1.8329717954350055e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.cpViscous[74], 4.1156693967828584e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.cpViscous[150], 1.8329717954350055e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.minimum.cpmni, -9.9608694841153800e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(before.minimum.cpmnv, -9.8360061107758145e-01, 1.0e-9, 1.0e-9);
  // LVISC: CPMN = CPMNV (xoper.f 1791-1792).
  CHECK_NEAR(before.minimum.cpmin, -9.8360061107758145e-01, 1.0e-9, 1.0e-9);
  CHECK(minIndex(before.cpViscous) < session.paneling.panel.x.size());

  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));

  const session::CpObservation after = session::currentCpMinimum(session);
  CHECK(after.valid);
  CHECK_NEAR(after.cpInviscid[0], 4.6278442208506038e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.cpViscous[0], 1.9130110118135177e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.cpViscous[74], 4.2719468129436416e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.cpViscous[150], 1.9130110118135177e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.cpmni, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.cpmnv, -1.0561740401367894e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(after.minimum.cpmin, -1.0561740401367894e+00, 1.0e-9, 1.0e-9);
  CHECK(minIndex(after.cpViscous) < session.paneling.panel.x.size());
  // Load consistency with the already confirmed Mach loads (C_AFTERMACH).
  CHECK_NEAR(session.result.lastResult.liftCoefficient,
             6.8756173455036773e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(session.result.lastResult.dragCoefficient,
             5.8445220498272478e-03, 1.0e-9, 1.0e-9);
}

TEST(unit, CoreOperMachIdenticalValueIsCpIdempotentMatchesFortran) {
  auto session = makeClarkYOperPointSession(/*viscous=*/true);
  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const session::CpObservation first = session::currentCpMinimum(session);
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const session::CpObservation second = session::currentCpMinimum(session);
  CHECK(second.valid);
  CHECK_NEAR(second.minimum.cpmni, first.minimum.cpmni, 0.0, 0.0);
  CHECK_NEAR(second.minimum.cpmnv, first.minimum.cpmnv, 0.0, 0.0);
  CHECK_NEAR(second.minimum.cpmin, first.minimum.cpmin, 0.0, 0.0);
  CHECK_NEAR(second.cpInviscid[0], first.cpInviscid[0], 0.0, 0.0);
  CHECK_NEAR(second.cpViscous[74], first.cpViscous[74], 0.0, 0.0);
  // C_IDENT == C_AFTER (reference).
  CHECK_NEAR(second.minimum.cpmni, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
}

// No valid point: controlled no current Cp-state. The
// Fortran COMMON artifact cpCalc(QINV=0)=1.0236 (harness A_AFTERMACH) is
// deliberate not invented.
TEST(unit, CoreOperCpWithoutPriorPointIsControlled) {
  auto session = makeFortranOperSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const session::CpObservation obs = session::currentCpMinimum(session);
  CHECK_FALSE(obs.valid);
  CHECK(obs.cpInviscid.empty());
  CHECK(obs.cpViscous.empty());
  CHECK_NEAR(obs.minimum.cpmin, 0.0, 0.0, 0.0);
}

// the pure Cp-read-only query mutates no session state.
TEST(unit, CoreOperCpQueryDoesNotMutateSessionState) {
  auto session = makeClarkYOperPointSession(/*viscous=*/true);
  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const std::vector<double> qinvBefore = session.viscous.viscals.qinv;
  const std::vector<double> qvisBefore = session.viscous.viscals.qvis;
  const std::vector<double> gamBefore = session.viscous.viscals.gam;
  const double minfBefore = session.viscous.viscals.minf;
  const double clBefore = session.result.lastResult.liftCoefficient;
  const std::size_t panelSize = session.paneling.panel.x.size();
  const std::size_t wakeSize = session.viscous.wake.x.size();

  const session::CpObservation obs = session::currentCpMinimum(session);
  CHECK(obs.valid);

  CHECK_EQ(session.viscous.viscals.qinv.size(), qinvBefore.size());
  CHECK(std::equal(session.viscous.viscals.qinv.begin(),
                   session.viscous.viscals.qinv.end(), qinvBefore.begin()));
  CHECK_EQ(session.viscous.viscals.qvis.size(), qvisBefore.size());
  CHECK(std::equal(session.viscous.viscals.qvis.begin(),
                   session.viscous.viscals.qvis.end(), qvisBefore.begin()));
  CHECK_EQ(session.viscous.viscals.gam.size(), gamBefore.size());
  CHECK(std::equal(session.viscous.viscals.gam.begin(),
                   session.viscous.viscals.gam.end(), gamBefore.begin()));
  CHECK_NEAR(session.viscous.viscals.minf, minfBefore, 0.0, 0.0);
  CHECK_NEAR(session.result.lastResult.liftCoefficient, clBefore, 0.0, 0.0);
  CHECK_EQ(session.paneling.panel.x.size(), panelSize);
  CHECK_EQ(session.viscous.wake.x.size(), wakeSize);
}

// non-graphical result consumer (CPMN text, xoper.f
// 1678-1686) keeps the derived values exactly; no plot implementation.
TEST(unit, CoreOperCpConsumerReceivesExactlyDerivedValues) {
  auto session = makeClarkYOperPointSession(/*viscous=*/true);
  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(session, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const session::CpObservation obs = session::currentCpMinimum(session);
  CHECK(obs.valid);
  // CPMN-output (LVISC): CPMNI/XCPMNI and CPMNV/XCPMNV.
  CHECK_NEAR(obs.minimum.cpmni, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(obs.minimum.xcpmni, 1.8498999999999999e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(obs.minimum.cpmnv, -1.0561740401367894e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(obs.minimum.xcpmnv, 1.8498999999999999e-01, 1.0e-9, 1.0e-9);
}

// Instance independence of the Cp derivation.
TEST(unit, CoreOperCpIndependentSessions) {
  auto mached = makeClarkYOperPointSession(/*viscous=*/true);
  auto untouched = makeClarkYOperPointSession(/*viscous=*/true);
  std::istringstream in;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(handleOperMachCore(mached, in, out, "0.3")),
           static_cast<int>(OperMachCoreStatus::applied));
  const session::CpObservation a = session::currentCpMinimum(mached);
  const session::CpObservation b = session::currentCpMinimum(untouched);
  CHECK(a.valid);
  CHECK(b.valid);
  CHECK_NEAR(a.minimum.cpmni, -1.0699120328271527e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(b.minimum.cpmni, -9.9608694841153800e-01, 1.0e-9, 1.0e-9);
}

// End: tests/unit/CoreOperHandlerTests.cpp

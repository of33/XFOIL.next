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

// File: tests/integration/LibraryCliParityL5Tests.cpp
// CLI/library parity: the CLI interpreter and the public in-memory APIs
// use the same SessionOps/Core paths. Comparisons use structured state,
// not CLI text parsing.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "app/AirfoilWriter.h"
#include "app/cli/CommandInterpreter.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli::testing {

// Read-only access used only by this test translation unit.
struct CommandInterpreterAccess {
  static const xfoil::session::SessionComposition& session(
      const xfoil::app::cli::CommandInterpreter& interpreter) {
    return interpreter.session_;
  }
};

}  // namespace xfoil::app::cli::testing

namespace {

using xfoil::geometry::AirfoilGeometry;
using xfoil::AlphaSequence;
using xfoil::AlphaTarget;
using xfoil::AnalysisConfig;
using xfoil::AnalysisRequest;
using xfoil::AnalysisStatus;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::LiftTarget;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::Point2D;
using xfoil::Result;
using xfoil::SequenceResult;
using xfoil::Session;
using xfoil::SurfaceDistributionKind;

struct CliSnapshot {
  int exitCode{};
  std::string outputText;
  std::string errorText;
  std::string bufferName;
  std::string geometryHeader;
  std::vector<Point2D> buffer;
  std::vector<Point2D> panel;
  xfoil::AnalysisResult lastResult;
  bool viscalRan{};
  double cdp{};
  std::vector<double> cpi;
  std::vector<double> cpv;
  std::vector<xfoil::app::cli::OperPointResult> points;
  bool panelingValid{};
};

CliSnapshot runCli(const std::string& script,
                   bool loadStartupDefaults = false) {
  std::istringstream input(script);
  std::ostringstream output;
  std::ostringstream errorOutput;

  xfoil::app::cli::InteractiveStartupOptions options;
  options.loadStartupDefaults = loadStartupDefaults;

  xfoil::app::cli::CommandInterpreter interpreter(input, output, errorOutput);
  CliSnapshot snapshot;
  snapshot.exitCode = interpreter.run(options);
  snapshot.outputText = output.str();
  snapshot.errorText = errorOutput.str();

  const auto& session =
      xfoil::app::cli::testing::CommandInterpreterAccess::session(interpreter);
  snapshot.bufferName = session.geometry.name;
  snapshot.geometryHeader = xfoil::session::geometryNameHeader(session);
  snapshot.panelingValid = session.paneling.panelingValid;
  snapshot.lastResult = session.result.lastResult;
  snapshot.viscalRan = session.result.lastDiagnostics.viscalRan;
  snapshot.cdp = session.viscous.viscals.cdp;
  snapshot.cpi = session.viscous.cpi;
  snapshot.cpv = session.viscous.cpv;
  const std::size_t bufferCount =
      std::min(session.geometry.buffer.x.size(),
               session.geometry.buffer.y.size());
  snapshot.buffer.reserve(bufferCount);
  for (std::size_t index = 0; index < bufferCount; ++index) {
    snapshot.buffer.push_back(Point2D{session.geometry.buffer.x[index],
                                     session.geometry.buffer.y[index]});
  }
  const std::size_t panelCount =
      std::min(session.paneling.panel.x.size(),
               session.paneling.panel.y.size());
  snapshot.panel.reserve(panelCount);
  for (std::size_t index = 0; index < panelCount; ++index) {
    snapshot.panel.push_back(Point2D{session.paneling.panel.x[index],
                                    session.paneling.panel.y[index]});
  }
  snapshot.points = interpreter.lastRunSummary().pointResults;
  return snapshot;
}

AirfoilGeometry cliGeometry(const CliSnapshot& cli) {
  AirfoilGeometry geometry;
  const auto assigned = geometry.assign(cli.bufferName, cli.buffer);
  CHECK(assigned.success());
  return geometry;
}

AnalysisConfig cliInviscidConfig() {
  AnalysisConfig config;
  config.reynoldsNumber = 0.0;
  config.machNumber = 0.0;
  config.viscous = false;
  config.maximumIterations = 20;
  config.autoInitializeBoundaryLayer = true;
  config.viscousAcceleration = 0.01;
  config.transition.nCritical = 9.0;
  config.transition.topTripX = 1.0;
  config.transition.bottomTripX = 1.0;
  return config;
}

AnalysisConfig cliViscousConfig(int maximumIterations = 20) {
  AnalysisConfig config = cliInviscidConfig();
  config.reynoldsNumber = 1.0e6;
  config.viscous = true;
  config.maximumIterations = maximumIterations;
  return config;
}

void expectBufferEqual(const std::vector<Point2D>& expected,
                       const AirfoilGeometry& actual) {
  CHECK_EQ(expected.size(), actual.pointCount());
  const auto coordinates = actual.coordinates();
  const std::size_t count =
      std::min(expected.size(), coordinates.size());
  for (std::size_t index = 0; index < count; ++index) {
    CHECK_NEAR(expected[index].x, coordinates[index].x, 0.0, 0.0);
    CHECK_NEAR(expected[index].y, coordinates[index].y, 0.0, 0.0);
  }
}

void expectPanelEqual(const CliSnapshot& cli, const Session& session) {
  const Result<xfoil::SurfaceGeometry> panel = session.currentPanel();
  CHECK(panel.isOk());
  if (panel.isErr()) {
    return;
  }
  CHECK_EQ(cli.panel.size(), panel.value().points.size());
  const std::size_t count =
      std::min(cli.panel.size(), panel.value().points.size());
  for (std::size_t index = 0; index < count; ++index) {
    CHECK_NEAR(cli.panel[index].x, panel.value().points[index].x, 0.0,
               0.0);
    CHECK_NEAR(cli.panel[index].y, panel.value().points[index].y, 0.0,
               0.0);
  }
}

void expectPointEqual(const CliSnapshot& cli,
                      const OperatingPointData& point) {
  CHECK(cli.lastResult.successful);
  CHECK_NEAR(cli.lastResult.alpha.value, point.alpha.value, 0.0, 0.0);
  CHECK_NEAR(cli.lastResult.liftCoefficient, point.liftCoefficient, 0.0,
             0.0);
  CHECK_NEAR(cli.lastResult.dragCoefficient, point.dragCoefficient, 0.0,
             0.0);
  CHECK_NEAR(cli.lastResult.momentCoefficient, point.momentCoefficient,
             0.0, 0.0);
  CHECK_NEAR(cli.lastResult.centerOfPressureX, point.centerOfPressureX,
             0.0, 0.0);
  CHECK_EQ(cli.lastResult.iterations, point.iterations);
  CHECK_EQ(cli.viscalRan, point.viscous);
  const ConvergenceStatus expectedConvergence =
      cli.lastResult.converged ? ConvergenceStatus::converged
                               : ConvergenceStatus::unconverged;
  CHECK(point.convergence == expectedConvergence);
  if (point.viscous) {
    CHECK(point.dragCoefficientPressure.has_value());
    if (point.dragCoefficientPressure.has_value()) {
      CHECK_NEAR(cli.cdp, point.dragCoefficientPressure.value(), 0.0, 0.0);
    }
  } else {
    CHECK(!point.dragCoefficientPressure.has_value());
  }
  CHECK(point.surface.has_value());
  if (!point.surface.has_value()) {
    return;
  }
  const std::vector<double>& cliCp = cli.viscalRan ? cli.cpv : cli.cpi;
  const auto& surface = point.surface.value();
  CHECK(surface.kind == (cli.viscalRan ? SurfaceDistributionKind::viscous
                                       : SurfaceDistributionKind::inviscid));
  CHECK_EQ(cliCp.size() >= surface.cp.size(), true);
  const std::size_t count =
      std::min(cliCp.size(), surface.cp.size());
  for (std::size_t index = 0; index < count; ++index) {
    CHECK_NEAR(cliCp[index], surface.cp[index], 0.0, 0.0);
  }
}

void expectSamePointData(const OperatingPointData& actual,
                         const OperatingPointData& expected) {
  CHECK_NEAR(actual.alpha.value, expected.alpha.value, 0.0, 0.0);
  CHECK_NEAR(actual.liftCoefficient, expected.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(actual.dragCoefficient, expected.dragCoefficient, 0.0, 0.0);
  CHECK_NEAR(actual.momentCoefficient, expected.momentCoefficient, 0.0,
             0.0);
  CHECK(actual.convergence == expected.convergence);
  CHECK_EQ(actual.iterations, expected.iterations);
  CHECK_EQ(actual.surface.has_value(), expected.surface.has_value());
  if (actual.surface.has_value() && expected.surface.has_value()) {
    CHECK_EQ(actual.surface->cp.size(), expected.surface->cp.size());
    const std::size_t count =
        std::min(actual.surface->cp.size(), expected.surface->cp.size());
    for (std::size_t index = 0; index < count; ++index) {
      CHECK_NEAR(actual.surface->cp[index], expected.surface->cp[index],
                 0.0, 0.0);
    }
  }
}

void expectSequenceEqual(const CliSnapshot& cli,
                         const SequenceResult& sequence) {
  CHECK_EQ(cli.points.size(), sequence.points.size());
  const std::size_t count =
      std::min(cli.points.size(), sequence.points.size());
  for (std::size_t index = 0; index < count; ++index) {
    CHECK_NEAR(cli.points[index].alpha, sequence.points[index].alpha.value,
               0.0, 0.0);
    CHECK_NEAR(cli.points[index].cl,
               sequence.points[index].liftCoefficient, 0.0, 0.0);
    CHECK_NEAR(cli.points[index].cd,
               sequence.points[index].dragCoefficient, 0.0, 0.0);
    CHECK_NEAR(cli.points[index].cm,
               sequence.points[index].momentCoefficient, 0.0, 0.0);
    CHECK(cli.points[index].converged ==
          (sequence.points[index].convergence ==
           ConvergenceStatus::converged));
  }
}

const std::vector<Point2D> kMemoryProfile = {
    {1.0, 0.0},    {0.9, 0.05},  {0.8, 0.07},  {0.7, 0.08},
    {0.6, 0.07},   {0.5, 0.06},  {0.4, 0.04},  {0.3, 0.02},
    {0.2, -0.01},  {0.1, -0.02}, {0.0, 0.0},   {0.1, -0.03},
    {0.3, -0.06},  {0.5, -0.08}, {0.7, -0.07}, {0.9, -0.04}};

void writeProfile(const std::filesystem::path& path) {
  std::ofstream out(path);
  out << "MEMORY PROFILE\n";
  for (const Point2D& point : kMemoryProfile) {
    out << point.x << " " << point.y << "\n";
  }
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

}  // namespace

TEST(integration, LibraryCliParityL5NacaPanelingInviscidAlpha) {
  const CliSnapshot cli =
      runCli("NACA 2412\nOPER\nALFA 4\n\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK(cli.panelingValid);
  CHECK(cli.lastResult.successful);

  Session nacaSession;
  const Result<AirfoilGeometry> naca = nacaSession.setNaca("2412");
  CHECK(naca.isOk());
  if (naca.isOk()) {
    expectBufferEqual(cli.buffer, naca.value());
  }

  Session library;
  CHECK(library.setGeometry(cliGeometry(cli)).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliInviscidConfig()).isOk());
  expectPanelEqual(cli, library);
  const Result<OperatingPointData> point =
      library.solveAlpha(Degrees{4.0});
  CHECK(point.isOk());
  if (point.isOk()) {
    expectPointEqual(cli, point.value());
  }

  AnalysisRequest request;
  request.geometry = cliGeometry(cli);
  request.paneling = PanelingOptions{};
  request.configuration = cliInviscidConfig();
  request.target = AlphaTarget{Degrees{4.0}};
  const Result<OperatingPointData> oneShot = xfoil::analyze(request);
  CHECK(oneShot.isOk());
  if (point.isOk() && oneShot.isOk()) {
    expectSamePointData(oneShot.value(), point.value());
  }
}

TEST(integration, LibraryCliParityL5NacsPaneling) {
  const CliSnapshot normal =
      runCli("NACA 0012\nOPER\nALFA 2\n\nQUIT\n");
  const CliSnapshot sharp =
      runCli("NACS 0012\nOPER\nALFA 2\n\nQUIT\n");
  CHECK_EQ(normal.exitCode, 0);
  CHECK_EQ(sharp.exitCode, 0);
  CHECK(normal.panelingValid);
  CHECK(sharp.panelingValid);
  CHECK(normal.outputText.find("not recognized") == std::string::npos);
  CHECK(sharp.outputText.find("not recognized") == std::string::npos);

  Session normalSession;
  Session sharpSession;
  const Result<AirfoilGeometry> normalGeometry =
      normalSession.setNaca("0012");
  const Result<AirfoilGeometry> sharpGeometry =
      sharpSession.setNacs("0012");
  CHECK(normalGeometry.isOk());
  CHECK(sharpGeometry.isOk());
  if (normalGeometry.isOk()) {
    expectBufferEqual(normal.buffer, normalGeometry.value());
  }
  if (sharpGeometry.isOk()) {
    expectBufferEqual(sharp.buffer, sharpGeometry.value());
  }
  CHECK(normal.buffer.front().y != sharp.buffer.front().y);
  CHECK(normal.buffer.back().y != sharp.buffer.back().y);

  Session library;
  CHECK(library.setGeometry(cliGeometry(sharp)).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliInviscidConfig()).isOk());
  expectPanelEqual(sharp, library);
  const Result<OperatingPointData> point =
      library.solveAlpha(Degrees{2.0});
  CHECK(point.isOk());
  if (point.isOk()) {
    expectPointEqual(sharp, point.value());
  }
}

TEST(integration, LibraryCliParityL5LoadVsMemoryGeometry) {
  const testing::ScopedTempDirectory work("l5-load-memory", true);
  writeProfile("profile.dat");

  const CliSnapshot cli =
      runCli("LOAD profile.dat\nPANE\nOPER\nALFA 2\n\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK(cli.panelingValid);
  CHECK(cli.lastResult.successful);
  CHECK_EQ(cli.buffer.size(), kMemoryProfile.size());
  for (std::size_t index = 0;
       index < std::min(cli.buffer.size(), kMemoryProfile.size());
       ++index) {
    CHECK_NEAR(cli.buffer[index].x, kMemoryProfile[index].x, 0.0, 0.0);
    CHECK_NEAR(cli.buffer[index].y, kMemoryProfile[index].y, 0.0, 0.0);
  }

  AirfoilGeometry geometry;
  const auto assigned = geometry.assign("MEMORY PROFILE", kMemoryProfile);
  CHECK(assigned.success());
  Session library;
  CHECK(library.setGeometry(geometry).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliInviscidConfig()).isOk());
  expectPanelEqual(cli, library);
  const Result<OperatingPointData> point =
      library.solveAlpha(Degrees{2.0});
  CHECK(point.isOk());
  if (point.isOk()) {
    expectPointEqual(cli, point.value());
  }
}

TEST(integration, LibraryCliParityL5ViscousAlpha) {
  const CliSnapshot cli =
      runCli("NACA 2412\nOPER\nVISC 1e6\nALFA 2\n\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK(cli.lastResult.successful);
  CHECK(cli.viscalRan);

  Session library;
  CHECK(library.setGeometry(cliGeometry(cli)).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliViscousConfig()).isOk());
  expectPanelEqual(cli, library);
  const Result<OperatingPointData> point =
      library.solveAlpha(Degrees{2.0});
  CHECK(point.isOk());
  if (point.isOk()) {
    CHECK(point.value().viscous);
    CHECK(point.value().dragCoefficientPressure.has_value());
    expectPointEqual(cli, point.value());
  }
}

TEST(integration, LibraryCliParityL5LiftTarget) {
  const CliSnapshot cli =
      runCli("NACA 0012\nOPER\nCL 0.5\n\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK(cli.lastResult.successful);

  Session library;
  CHECK(library.setGeometry(cliGeometry(cli)).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliInviscidConfig()).isOk());
  const Result<OperatingPointData> point = library.solveCl(0.5);
  CHECK(point.isOk());
  if (point.isOk()) {
    CHECK_NEAR(point.value().liftCoefficient, 0.5, 1.0e-6, 1.0e-6);
    expectPointEqual(cli, point.value());
  }
}

TEST(integration, LibraryCliParityL5AlphaSequences) {
  const CliSnapshot ascending =
      runCli("NACA 0012\nOPER\nASEQ 0 2 1\n\nQUIT\n");
  CHECK_EQ(ascending.exitCode, 0);
  CHECK_EQ(ascending.points.size(), std::size_t{3});

  Session ascendingLibrary;
  CHECK(ascendingLibrary.setGeometry(cliGeometry(ascending)).isOk());
  CHECK(ascendingLibrary.generatePanel(PanelingOptions{}).isOk());
  CHECK(ascendingLibrary.configure(cliInviscidConfig()).isOk());
  AlphaSequence ascendingSequence;
  ascendingSequence.start = Degrees{0.0};
  ascendingSequence.endExclusive = Degrees{3.0};
  ascendingSequence.step = Degrees{1.0};
  const Result<SequenceResult> ascendingResult =
      ascendingLibrary.solveAlphaSequence(ascendingSequence);
  CHECK(ascendingResult.isOk());
  if (ascendingResult.isOk()) {
    expectSequenceEqual(ascending, ascendingResult.value());
  }

  const CliSnapshot descending =
      runCli("NACA 0012\nOPER\nASEQ 2 0 1\n\nQUIT\n");
  CHECK_EQ(descending.exitCode, 0);
  CHECK_EQ(descending.points.size(), std::size_t{3});

  Session descendingLibrary;
  CHECK(descendingLibrary.setGeometry(cliGeometry(descending)).isOk());
  CHECK(descendingLibrary.generatePanel(PanelingOptions{}).isOk());
  CHECK(descendingLibrary.configure(cliInviscidConfig()).isOk());
  AlphaSequence descendingSequence;
  descendingSequence.start = Degrees{2.0};
  descendingSequence.endExclusive = Degrees{-1.0};
  descendingSequence.step = Degrees{-1.0};
  const Result<SequenceResult> descendingResult =
      descendingLibrary.solveAlphaSequence(descendingSequence);
  CHECK(descendingResult.isOk());
  if (descendingResult.isOk()) {
    expectSequenceEqual(descending, descendingResult.value());
  }
}

TEST(integration, LibraryCliParityL5SequenceNonConvergence) {
  // OPER ASEQ runs VISCAL(ITMAX+5) per point, the API/batch sequence
  // contract runs VISCAL(ITMAX). With ITMAX=1 and a high alpha both are
  // non-converged, but the partial coefficients are not a shared
  // converged quantity. Compare order, count and convergence status.
  const CliSnapshot cli = runCli(
      "NACA 0012\nOPER\nITER 1\nVISC 1e6\nASEQ 20 21 1\n\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK_EQ(cli.points.size(), std::size_t{2});
  for (const auto& point : cli.points) {
    CHECK(!point.converged);
  }

  Session library;
  CHECK(library.setGeometry(cliGeometry(cli)).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliViscousConfig(1)).isOk());
  AlphaSequence sequence;
  sequence.start = Degrees{20.0};
  sequence.endExclusive = Degrees{22.0};
  sequence.step = Degrees{1.0};
  const Result<SequenceResult> result =
      library.solveAlphaSequence(sequence);
  CHECK(result.isOk());
  if (result.isOk()) {
    CHECK_EQ(result.value().points.size(), cli.points.size());
    const std::size_t count =
        std::min(result.value().points.size(), cli.points.size());
    for (std::size_t index = 0; index < count; ++index) {
      CHECK(result.value().points[index].convergence ==
            ConvergenceStatus::unconverged);
      CHECK(!cli.points[index].converged);
      CHECK_NEAR(result.value().points[index].alpha.value,
                 cli.points[index].alpha, 0.0, 0.0);
    }
  }
}

TEST(integration, LibraryCliParityL5StateInvalidationAndIsolation) {
  const CliSnapshot cli = runCli(
      "NACA 2412\nOPER\nALFA 2\n\nNACA 0012\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK(cli.panelingValid);
  CHECK(!cli.lastResult.successful);

  Session library;
  CHECK(library.setNaca("2412").isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliInviscidConfig()).isOk());
  CHECK(library.solveAlpha(Degrees{2.0}).isOk());
  CHECK(library.setNaca("0012").isOk());
  CHECK(library.lastOperatingPoint().isErr());

  Session first;
  Session second;
  CHECK(first.setNaca("0012").isOk());
  CHECK(first.generatePanel(PanelingOptions{}).isOk());
  CHECK(first.configure(cliInviscidConfig()).isOk());
  CHECK(second.setNaca("2412").isOk());
  CHECK(second.generatePanel(PanelingOptions{}).isOk());
  CHECK(second.configure(cliInviscidConfig()).isOk());
  const Result<OperatingPointData> firstPoint =
      first.solveAlpha(Degrees{2.0});
  const Result<OperatingPointData> secondPoint =
      second.solveAlpha(Degrees{2.0});
  CHECK(firstPoint.isOk());
  CHECK(secondPoint.isOk());
  if (firstPoint.isOk() && secondPoint.isOk()) {
    CHECK(firstPoint.value().liftCoefficient !=
          secondPoint.value().liftCoefficient);
  }
  first.reset();
  CHECK(first.lastOperatingPoint().isErr());
  CHECK(second.lastOperatingPoint().isOk());
}

TEST(integration, LibraryCliParityL5FileAdapterAndLibraryNoIo) {
  const testing::ScopedTempDirectory work("l5-file-adapter", true);
  const CliSnapshot cli =
      runCli("NACA 2412\nSAVE saved.dat\nQUIT\n");
  CHECK_EQ(cli.exitCode, 0);
  CHECK(std::filesystem::exists("saved.dat"));

  xfoil::app::AirfoilWriteOptions options;
  options.format = xfoil::app::AirfoilOutputFormat::labeled;
  options.delimiter = xfoil::app::CoordinateDelimiter::blank;
  options.clockwise = false;
  options.name = cli.geometryHeader;
  options.overwriteExisting = true;
  const xfoil::app::AirfoilWriteResult expectedWritten =
      xfoil::app::writeAirfoil("expected.dat", cli.panel, options);
  CHECK(expectedWritten.success);
  CHECK_EQ(readFile("saved.dat"), readFile("expected.dat"));

  const std::vector<std::string> before = [] {
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
      names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
  }();

  Session library;
  CHECK(library.setGeometry(cliGeometry(cli)).isOk());
  CHECK(library.generatePanel(PanelingOptions{}).isOk());
  CHECK(library.configure(cliInviscidConfig()).isOk());
  CHECK(library.solveAlpha(Degrees{2.0}).isOk());

  const std::vector<std::string> after = [] {
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
      names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
  }();
  CHECK(before == after);
}

TEST(integration, LibraryCliParityL5HelpAndHiddenCommands) {
  const CliSnapshot help = runCli("?\nQUIT\n");
  CHECK_EQ(help.exitCode, 0);
  for (const std::string_view needle :
       {"OPER", "GDES", "MDES", "QDES", "NACA", "PANE", "PPAR", "QUIT"}) {
    CHECK(help.outputText.find(std::string{needle}) != std::string::npos);
  }

  const CliSnapshot hidden = runCli("NACS 0012\nQUIT\n");
  CHECK_EQ(hidden.exitCode, 0);
  CHECK(hidden.outputText.find("not recognized") == std::string::npos);
  CHECK(hidden.panelingValid);
  CHECK(!hidden.buffer.empty());
}

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

// File: library-demo/main.cpp
// Standalone embedded-library demo. The host application owns profile
// file I/O; the library receives only in-memory public values.

#include <cstddef>
#include <iostream>
#include <locale>
#include <string>

#include "DemoSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Types.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

int fail(const char* category, const std::string& message, int exitCode) {
  std::cerr << "xfoil-next-lib-demo: " << category << ": " << message << '\n';
  return exitCode;
}

template <class T>
int reportLibraryError(
    const char* category,
    const xfoil::Result<T>& result,
    int exitCode) {
  return fail(category, result.error().code + ": " + result.error().message,
              exitCode);
}

int reportVoidLibraryError(
    const char* category,
    const xfoil::Result<void>& result,
    int exitCode) {
  return fail(category, result.error().code + ": " + result.error().message,
              exitCode);
}

bool sequenceComplete(const xfoil::SequenceResult& sequence) {
  if (sequence.stopReason != xfoil::SequenceStopReason::completed) {
    return false;
  }
  for (const xfoil::PolarPoint& point : sequence.points) {
    if (point.convergence != xfoil::ConvergenceStatus::converged) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::cout.imbue(std::locale::classic());
  std::cerr.imbue(std::locale::classic());

  // Command-line parsing belongs to the host application, not to the
  // library. The demo owns option names, validation and help text.
  const xfoil_demo::ParseResult parsed =
      xfoil_demo::parseArguments(argc, argv);
  if (parsed.status == xfoil_demo::ParseStatus::help) {
    std::cout << xfoil_demo::helpText();
    return static_cast<int>(xfoil_demo::ExitCode::success);
  }
  if (parsed.status == xfoil_demo::ParseStatus::error) {
    std::cerr << "xfoil-next-lib-demo: " << parsed.message << '\n';
    std::cerr << "Try 'xfoil-next-lib-demo --help' for usage.\n";
    return static_cast<int>(xfoil_demo::ExitCode::usage);
  }

  // The host reads the profile file. The library receives only the
  // resulting name and coordinate values.
  const xfoil_demo::Options& options = parsed.options;
  const xfoil_demo::ProfileResult profileResult =
      xfoil_demo::readProfileFile(options.airfoilPath);
  if (profileResult.status != xfoil_demo::ProfileStatus::ok) {
    return fail("profile", profileResult.message,
                static_cast<int>(xfoil_demo::ExitCode::profile));
  }

  // Pass an owning public geometry value to the session. The library copies
  // the coordinates; the host keeps ownership of its profile buffers.
  const xfoil_demo::Profile& profile = profileResult.profile;
  xfoil::geometry::AirfoilGeometry geometry;
  const xfoil::geometry::GeometryAssignmentResult assignment =
      geometry.assign(profile.name, profile.points);
  if (!assignment.success()) {
    return fail("geometry", assignment.message,
                static_cast<int>(xfoil_demo::ExitCode::geometry));
  }

  xfoil::Session session;
  const auto geometryStatus = session.setGeometry(geometry);
  if (geometryStatus.isErr()) {
    return reportVoidLibraryError(
        "geometry", geometryStatus,
        static_cast<int>(xfoil_demo::ExitCode::geometry));
  }

  // Start from the public library defaults and override only values that the
  // user supplied explicitly.
  xfoil::PanelingOptions paneling;
  if (options.panelCount.has_value()) {
    paneling.panelCount = *options.panelCount;
  }
  const auto panelStatus = session.generatePanel(paneling);
  if (panelStatus.isErr()) {
    return reportLibraryError(
        "geometry", panelStatus,
        static_cast<int>(xfoil_demo::ExitCode::geometry));
  }

  // Re = 0 selects the inviscid path; Re > 0 selects viscous analysis.
  // Mach and iteration overrides are optional and otherwise keep the
  // AnalysisConfig{} defaults.
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = options.reynoldsNumber == 0.0
      ? 0.0
      : options.reynoldsNumber;
  config.viscous = xfoil_demo::usesViscousPath(options);
  if (options.machNumber.has_value()) {
    config.machNumber = *options.machNumber;
  }
  if (options.maximumIterations.has_value()) {
    config.maximumIterations = *options.maximumIterations;
  }
  const auto configStatus = session.configure(config);
  if (configStatus.isErr()) {
    return reportVoidLibraryError(
        "analysis", configStatus,
        static_cast<int>(xfoil_demo::ExitCode::analysis));
  }

  std::cout << xfoil_demo::formatHeader(profile, config, paneling, options);

  // The same session and configuration are used for the alpha sequence and
  // the following CL point. AlphaSequence::endExclusive is not executed.
  xfoil::AlphaSequence sequence;
  sequence.start = xfoil::Degrees{options.alphaStart};
  sequence.endExclusive = xfoil::Degrees{options.alphaEndExclusive};
  sequence.step = xfoil::Degrees{options.alphaStep};
  const auto sequenceResult = session.solveAlphaSequence(sequence);
  if (sequenceResult.isErr()) {
    return reportLibraryError(
        "analysis", sequenceResult,
        static_cast<int>(xfoil_demo::ExitCode::analysis));
  }

  // Results are read from structured public fields. The demo never parses
  // library console text; the library produces no such text.
  const xfoil::SequenceResult& sequenceValue = sequenceResult.value();
  std::cout << xfoil_demo::sequenceTableHeader();
  for (const xfoil::PolarPoint& point : sequenceValue.points) {
    std::cout << xfoil_demo::formatSequenceRow(point);
  }
  std::cout << '\n';

  const bool sequenceWasComplete = sequenceComplete(sequenceValue);

  // Reuse the same session for the specified-CL point so the demo shows a
  // continuous host workflow rather than two independent one-shot calls.
  const auto clResult = session.solveCl(options.targetLiftCoefficient);
  if (clResult.isErr()) {
    const int code = sequenceWasComplete
        ? static_cast<int>(xfoil_demo::ExitCode::analysis)
        : static_cast<int>(xfoil_demo::ExitCode::nonConverged);
    return reportLibraryError("analysis", clResult, code);
  }

  const xfoil::OperatingPointData& clValue = clResult.value();
  std::cout << xfoil_demo::clTableHeader();
  std::cout << xfoil_demo::formatClRow(
      options.targetLiftCoefficient, clValue);
  std::cout << '\n';
  const xfoil_demo::FrictionOutputResult friction =
      xfoil_demo::formatClFriction(clValue);
  if (friction.status == xfoil_demo::FrictionOutputStatus::inconsistent) {
    return fail("analysis", friction.message,
                static_cast<int>(xfoil_demo::ExitCode::analysis));
  }
  const std::size_t cpCount = clValue.surface.has_value()
      ? clValue.surface->cp.size()
      : std::size_t{0};
  std::cout << friction.text << '\n';
  std::cout << xfoil_demo::cpPointCount(cpCount) << '\n';

  const bool clConverged =
      clValue.convergence == xfoil::ConvergenceStatus::converged;
  if (!sequenceWasComplete || !clConverged) {
    return static_cast<int>(xfoil_demo::ExitCode::nonConverged);
  }
  return static_cast<int>(xfoil_demo::ExitCode::success);
}

// End: library-demo/main.cpp

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

// File: library-demo/DemoSupport.h
// Demo-local command-line, profile and output support. This header uses
// only installed public XFOIL.next headers and the C++ standard library.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Types.h"

namespace xfoil_demo {

// Process exit status. The values are part of the demo contract and are
// documented in library-demo/README.md.
enum class ExitCode : int {
  success = 0,
  usage = 2,
  profile = 3,
  geometry = 4,
  analysis = 5,
  nonConverged = 6
};

// Command-line request owned by the demo. Required fields are always set;
// optional analysis overrides are empty until explicitly supplied. Angles
// are in degrees and the alpha interval has an exclusive end.
struct Options {
  std::string airfoilPath;
  double reynoldsNumber{};
  double alphaStart{};
  double alphaEndExclusive{};
  double alphaStep{};
  double targetLiftCoefficient{};
  std::optional<double> machNumber;
  std::optional<int> maximumIterations;
  std::optional<std::size_t> panelCount;
};

enum class ParseStatus {
  ok,
  help,
  error
};

// Result of host-side argument parsing. On error, message is suitable for
// the command-line diagnostic; it is not a library error.
struct ParseResult {
  ParseStatus status{ParseStatus::error};
  Options options;
  std::string message;
};

// Parses argv without touching the library. The demo owns this policy.
[[nodiscard]]
ParseResult parseArguments(int argc, const char* const* argv);

// Returns the command-line help text, including the public library defaults.
[[nodiscard]]
std::string helpText();

// Airfoil coordinates owned by the demo before they are passed to the
// public geometry value type.
struct Profile {
  std::string name;
  std::vector<xfoil::Point2D> points;
};

enum class ProfileStatus {
  ok,
  fileError,
  parseError
};

// Result of reading or parsing a profile. The profile is demo-owned; the
// library never receives a filename or stream.
struct ProfileResult {
  ProfileStatus status{ProfileStatus::parseError};
  Profile profile;
  std::string message;
};

// Parses the minimal two-column profile text. fallbackName is used when the
// text has no name line.
[[nodiscard]]
ProfileResult parseProfileText(
    std::string_view text, std::string_view fallbackName);

// Host-side file read. This is deliberately outside the library boundary.
[[nodiscard]]
ProfileResult readProfileFile(const std::string& path);

// Re = 0 selects the inviscid path; Re > 0 selects the viscous path.
[[nodiscard]]
bool usesViscousPath(const Options& options) noexcept;

// Formats the effective configuration before any solver result is printed.
[[nodiscard]]
std::string formatHeader(
    const Profile& profile,
    const xfoil::AnalysisConfig& config,
    const xfoil::PanelingOptions& paneling,
    const Options& options);

[[nodiscard]]
std::string sequenceTableHeader();

// Formats one compact sequence row from public PolarPoint scalars.
[[nodiscard]]
std::string formatSequenceRow(const xfoil::PolarPoint& point);

[[nodiscard]]
std::string clTableHeader();

// Formats one specified-CL row. OperatingPointData is mapped into the same
// compact row layout; the demo does not recompute coefficients.
[[nodiscard]]
std::string formatClRow(
    double targetLiftCoefficient,
    const xfoil::OperatingPointData& point);

[[nodiscard]]
std::string statusText(xfoil::ConvergenceStatus status);

[[nodiscard]]
std::string cpPointCount(std::size_t count);

enum class FrictionOutputStatus {
  ok,
  inconsistent
};

// Result of formatting the optional CDF/Cf output. text is printed; message
// describes a controlled inconsistency and is not a library diagnostic.
struct FrictionOutputResult {
  FrictionOutputStatus status{FrictionOutputStatus::inconsistent};
  std::string text;
  std::string message;
};

// Checks CDF/Cf presence invariants and formats station counts. CDF is read
// from the owning result field; it is never reconstructed from CD and CDp.
[[nodiscard]]
FrictionOutputResult formatClFriction(
    const xfoil::OperatingPointData& point);

}  // namespace xfoil_demo

// End: library-demo/DemoSupport.h

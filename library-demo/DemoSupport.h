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

enum class ExitCode : int {
  success = 0,
  usage = 2,
  profile = 3,
  geometry = 4,
  analysis = 5,
  nonConverged = 6
};

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

struct ParseResult {
  ParseStatus status{ParseStatus::error};
  Options options;
  std::string message;
};

[[nodiscard]]
ParseResult parseArguments(int argc, const char* const* argv);

[[nodiscard]]
std::string helpText();

struct Profile {
  std::string name;
  std::vector<xfoil::Point2D> points;
};

enum class ProfileStatus {
  ok,
  fileError,
  parseError
};

struct ProfileResult {
  ProfileStatus status{ProfileStatus::parseError};
  Profile profile;
  std::string message;
};

[[nodiscard]]
ProfileResult parseProfileText(
    std::string_view text, std::string_view fallbackName);

[[nodiscard]]
ProfileResult readProfileFile(const std::string& path);

[[nodiscard]]
bool usesViscousPath(const Options& options) noexcept;

[[nodiscard]]
std::string formatHeader(
    const Profile& profile,
    const xfoil::AnalysisConfig& config,
    const xfoil::PanelingOptions& paneling,
    const Options& options);

[[nodiscard]]
std::string sequenceTableHeader();

[[nodiscard]]
std::string formatSequenceRow(const xfoil::PolarPoint& point);

[[nodiscard]]
std::string clTableHeader();

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

struct FrictionOutputResult {
  FrictionOutputStatus status{FrictionOutputStatus::inconsistent};
  std::string text;
  std::string message;
};

[[nodiscard]]
FrictionOutputResult formatClFriction(
    const xfoil::OperatingPointData& point);

}  // namespace xfoil_demo

// End: library-demo/DemoSupport.h

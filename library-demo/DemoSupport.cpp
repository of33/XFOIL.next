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

// File: library-demo/DemoSupport.cpp
// Demo-local argument parsing, profile parsing and stable output.

#include "DemoSupport.h"

#include <charconv>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <utility>

namespace xfoil_demo {
namespace {

std::string_view trim(std::string_view text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return text;
}

std::vector<std::string_view> tokens(std::string_view line) {
  std::vector<std::string_view> result;
  std::size_t position = 0;
  while (position < line.size()) {
    while (position < line.size() &&
           std::isspace(static_cast<unsigned char>(line[position])) != 0) {
      ++position;
    }
    const std::size_t begin = position;
    while (position < line.size() &&
           std::isspace(static_cast<unsigned char>(line[position])) == 0) {
      ++position;
    }
    if (position > begin) {
      result.push_back(line.substr(begin, position - begin));
    }
  }
  return result;
}

bool parseFiniteDouble(std::string_view text, double& value) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  if (text.front() == '+') {
    text.remove_prefix(1);
    if (text.empty()) {
      return false;
    }
  }
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto parsed =
      std::from_chars(begin, end, value, std::chars_format::general);
  return parsed.ec == std::errc{} && parsed.ptr == end &&
         std::isfinite(value);
}

bool isNonFiniteLiteral(std::string_view text) {
  text = trim(text);
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    text.remove_prefix(1);
  }
  std::string lowered;
  lowered.reserve(text.size());
  for (const char character : text) {
    lowered.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(character))));
  }
  return lowered == "nan" || lowered == "inf" || lowered == "infinity";
}

bool isNumericLike(std::string_view text) {
  double ignored = 0.0;
  return parseFiniteDouble(text, ignored) || isNonFiniteLiteral(text);
}

bool parsePositiveInteger(std::string_view text, int& value) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  long long parsedValue = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto parsed = std::from_chars(begin, end, parsedValue);
  if (parsed.ec != std::errc{} || parsed.ptr != end || parsedValue <= 0 ||
      parsedValue > std::numeric_limits<int>::max()) {
    return false;
  }
  value = static_cast<int>(parsedValue);
  return true;
}

bool parsePositiveSize(std::string_view text, std::size_t& value) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  unsigned long long parsedValue = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto parsed = std::from_chars(begin, end, parsedValue);
  if (parsed.ec != std::errc{} || parsed.ptr != end || parsedValue == 0 ||
      parsedValue > std::numeric_limits<std::size_t>::max()) {
    return false;
  }
  value = static_cast<std::size_t>(parsedValue);
  return true;
}

ParseResult parseError(std::string message) {
  ParseResult result;
  result.status = ParseStatus::error;
  result.message = std::move(message);
  return result;
}

std::string formatNumber(double value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setprecision(12) << std::defaultfloat << value;
  return stream.str();
}

std::string formatCoefficient(const std::optional<double>& value) {
  if (!value.has_value() || !std::isfinite(*value)) {
    return "n/a";
  }
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(6) << *value;
  return stream.str();
}

std::string formatCoefficient(double value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(6) << value;
  return stream.str();
}

std::string formatAlpha(double value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(4) << value;
  return stream.str();
}

std::string formatRow(
    std::string_view first,
    std::optional<std::string_view> second,
    std::optional<double> dragCoefficientFriction,
    std::string_view iterations,
    const xfoil::PolarPoint& point) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setw(10) << first;
  if (second.has_value()) {
    stream << ' ' << std::setw(10) << *second;
  }
  stream << ' '
         << std::setw(12) << formatCoefficient(point.liftCoefficient) << ' '
         << std::setw(12) << formatCoefficient(point.dragCoefficient) << ' '
         << std::setw(12)
         << formatCoefficient(point.dragCoefficientPressure) << ' '
         << std::setw(12)
         << formatCoefficient(dragCoefficientFriction) << ' '
         << std::setw(12) << formatCoefficient(point.momentCoefficient) << ' '
         << std::setw(10) << iterations << "  "
         << statusText(point.convergence) << '\n';
  return stream.str();
}

std::string fallbackProfileName(std::string_view path) {
  const std::filesystem::path filePath{std::string(path)};
  std::string name = filePath.stem().string();
  if (name.empty()) {
    name = "profile";
  }
  return name;
}

}  // namespace

ParseResult parseArguments(int argc, const char* const* argv) {
  std::optional<std::string> airfoilPath;
  std::optional<double> reynoldsNumber;
  std::optional<double> alphaStart;
  std::optional<double> alphaEndExclusive;
  std::optional<double> alphaStep;
  std::optional<double> targetLiftCoefficient;
  std::optional<double> machNumber;
  std::optional<int> maximumIterations;
  std::optional<std::size_t> panelCount;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--help") {
      ParseResult result;
      result.status = ParseStatus::help;
      return result;
    }

    auto readValue = [&](std::string_view option,
                         std::string_view& value) -> ParseResult {
      if (index + 1 >= argc) {
        return parseError("missing value for " + std::string(option));
      }
      ++index;
      value = argv[index];
      if (trim(value).empty()) {
        return parseError("empty value for " + std::string(option));
      }
      ParseResult success;
      success.status = ParseStatus::ok;
      return success;
    };

    if (argument == "--airfoil") {
      if (airfoilPath.has_value()) {
        return parseError("option may be given only once: --airfoil");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      airfoilPath = std::string(value);
      continue;
    }

    if (argument == "--re") {
      if (reynoldsNumber.has_value()) {
        return parseError("option may be given only once: --re");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      if (!parseFiniteDouble(value, reynoldsNumber.emplace())) {
        return parseError("--re requires a finite number");
      }
      continue;
    }

    if (argument == "--alpha-start") {
      if (alphaStart.has_value()) {
        return parseError("option may be given only once: --alpha-start");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      if (!parseFiniteDouble(value, alphaStart.emplace())) {
        return parseError("--alpha-start requires a finite number");
      }
      continue;
    }

    if (argument == "--alpha-end") {
      if (alphaEndExclusive.has_value()) {
        return parseError("option may be given only once: --alpha-end");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      if (!parseFiniteDouble(value, alphaEndExclusive.emplace())) {
        return parseError("--alpha-end requires a finite number");
      }
      continue;
    }

    if (argument == "--alpha-step") {
      if (alphaStep.has_value()) {
        return parseError("option may be given only once: --alpha-step");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      if (!parseFiniteDouble(value, alphaStep.emplace())) {
        return parseError("--alpha-step requires a finite number");
      }
      continue;
    }

    if (argument == "--cl") {
      if (targetLiftCoefficient.has_value()) {
        return parseError("option may be given only once: --cl");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      if (!parseFiniteDouble(value, targetLiftCoefficient.emplace())) {
        return parseError("--cl requires a finite number");
      }
      continue;
    }

    if (argument == "--mach") {
      if (machNumber.has_value()) {
        return parseError("option may be given only once: --mach");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      if (!parseFiniteDouble(value, machNumber.emplace())) {
        return parseError("--mach requires a finite number");
      }
      continue;
    }

    if (argument == "--iterations") {
      if (maximumIterations.has_value()) {
        return parseError("option may be given only once: --iterations");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      int parsed = 0;
      if (!parsePositiveInteger(value, parsed)) {
        return parseError("--iterations requires a positive integer");
      }
      maximumIterations = parsed;
      continue;
    }

    if (argument == "--panels") {
      if (panelCount.has_value()) {
        return parseError("option may be given only once: --panels");
      }
      std::string_view value;
      ParseResult read = readValue(argument, value);
      if (read.status == ParseStatus::error) {
        return read;
      }
      std::size_t parsed = 0;
      if (!parsePositiveSize(value, parsed)) {
        return parseError("--panels requires a positive integer");
      }
      panelCount = parsed;
      continue;
    }

    return parseError("unknown option: " + std::string(argument));
  }

  if (!airfoilPath.has_value()) {
    return parseError("missing required option: --airfoil");
  }
  if (!reynoldsNumber.has_value()) {
    return parseError("missing required option: --re");
  }
  if (!alphaStart.has_value()) {
    return parseError("missing required option: --alpha-start");
  }
  if (!alphaEndExclusive.has_value()) {
    return parseError("missing required option: --alpha-end");
  }
  if (!alphaStep.has_value()) {
    return parseError("missing required option: --alpha-step");
  }
  if (!targetLiftCoefficient.has_value()) {
    return parseError("missing required option: --cl");
  }
  if (*reynoldsNumber < 0.0) {
    return parseError("--re must be finite and non-negative");
  }
  if (*alphaStart == *alphaEndExclusive) {
    return parseError("--alpha-start and --alpha-end must differ");
  }
  if (*alphaStep == 0.0) {
    return parseError("--alpha-step must be non-zero");
  }
  if ((*alphaStart < *alphaEndExclusive && *alphaStep < 0.0) ||
      (*alphaStart > *alphaEndExclusive && *alphaStep > 0.0)) {
    return parseError(
        "--alpha-step direction does not match the alpha interval");
  }

  ParseResult result;
  result.status = ParseStatus::ok;
  result.options.airfoilPath = std::move(*airfoilPath);
  result.options.reynoldsNumber = *reynoldsNumber;
  result.options.alphaStart = *alphaStart;
  result.options.alphaEndExclusive = *alphaEndExclusive;
  result.options.alphaStep = *alphaStep;
  result.options.targetLiftCoefficient = *targetLiftCoefficient;
  result.options.machNumber = machNumber;
  result.options.maximumIterations = maximumIterations;
  result.options.panelCount = panelCount;
  return result;
}

std::string helpText() {
  const xfoil::AnalysisConfig defaults;
  const xfoil::PanelingOptions panelingDefaults;
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream
      << "Usage:\n"
      << "  xfoil-next-lib-demo\n"
      << "    --airfoil PATH\n"
      << "    --re VALUE\n"
      << "    --alpha-start DEG\n"
      << "    --alpha-end DEG\n"
      << "    --alpha-step DEG\n"
      << "    --cl VALUE\n"
      << "    [--mach VALUE]\n"
      << "    [--iterations COUNT]\n"
      << "    [--panels COUNT]\n"
      << "\n"
      << "Re = 0 selects inviscid analysis.\n"
      << "Re > 0 selects viscous analysis.\n"
      << "alpha-end is exclusive.\n"
      << "Optional values use the public library defaults when omitted:\n"
      << "  --mach default " << formatNumber(defaults.machNumber) << '\n'
      << "  --iterations default " << defaults.maximumIterations << '\n'
      << "  --panels default " << panelingDefaults.panelCount << '\n'
      << "The demo application reads the profile file; the XFOIL.next "
         "library does not.\n";
  return stream.str();
}

ProfileResult parseProfileText(
    std::string_view text, std::string_view fallbackName) {
  ProfileResult result;
  result.profile.name = std::string(fallbackName);

  bool firstContent = true;
  std::size_t lineNumber = 0;
  std::size_t position = 0;
  while (position < text.size()) {
    const std::size_t newline = text.find('\n', position);
    std::string_view line = text.substr(
        position,
        newline == std::string_view::npos
            ? std::string_view::npos
            : newline - position);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    ++lineNumber;
    position = newline == std::string_view::npos
        ? text.size()
        : newline + 1;

    const std::string_view content = trim(line);
    if (content.empty() || content.front() == '#') {
      continue;
    }

    const std::vector<std::string_view> values = tokens(content);
    if (firstContent) {
      firstContent = false;
      if (values.size() == 2 &&
          isNumericLike(values[0]) && isNumericLike(values[1])) {
        double x = 0.0;
        double y = 0.0;
        if (!parseFiniteDouble(values[0], x) ||
            !parseFiniteDouble(values[1], y)) {
          result.message =
              "line " + std::to_string(lineNumber) +
              ": expected exactly two finite numeric values";
          return result;
        }
        result.profile.points.push_back({x, y});
        continue;
      }

      bool allNumeric = !values.empty();
      for (const std::string_view value : values) {
        if (!isNumericLike(value)) {
          allNumeric = false;
          break;
        }
      }
      if (allNumeric) {
        result.message =
            "line " + std::to_string(lineNumber) +
            ": expected exactly two numeric columns";
        return result;
      }

      result.profile.name = std::string(content);
      continue;
    }

    if (values.size() != 2) {
      result.message =
          "line " + std::to_string(lineNumber) +
          ": expected exactly two numeric columns";
      return result;
    }
    double x = 0.0;
    double y = 0.0;
    if (!parseFiniteDouble(values[0], x) ||
        !parseFiniteDouble(values[1], y)) {
      result.message =
          "line " + std::to_string(lineNumber) +
          ": expected exactly two finite numeric values";
      return result;
    }
    result.profile.points.push_back({x, y});
  }

  if (result.profile.name.empty()) {
    result.profile.name = "profile";
  }
  if (result.profile.points.size() < 3) {
    result.message =
        "line 1: profile requires at least 3 coordinate points";
    return result;
  }

  result.status = ProfileStatus::ok;
  return result;
}

ProfileResult readProfileFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    ProfileResult result;
    result.status = ProfileStatus::fileError;
    result.message = "cannot open profile file: " + path;
    return result;
  }

  std::ostringstream content;
  content << input.rdbuf();
  if (input.bad()) {
    ProfileResult result;
    result.status = ProfileStatus::fileError;
    result.message = "cannot read profile file: " + path;
    return result;
  }

  ProfileResult result = parseProfileText(
      content.str(), fallbackProfileName(path));
  if (result.status != ProfileStatus::ok) {
    result.status = ProfileStatus::parseError;
    result.message = "profile parse error: " + result.message;
  }
  return result;
}

bool usesViscousPath(const Options& options) noexcept {
  return options.reynoldsNumber > 0.0;
}

std::string formatHeader(
    const Profile& profile,
    const xfoil::AnalysisConfig& config,
    const xfoil::PanelingOptions& paneling,
    const Options& options) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << "XFOIL.next library demo\n"
         << "Airfoil: " << profile.name << '\n'
         << "Mode: " << (config.viscous ? "viscous" : "inviscid") << '\n'
         << "Reynolds: " << formatNumber(config.reynoldsNumber) << '\n'
         << "Mach: " << formatNumber(config.machNumber) << '\n'
         << "Iterations: " << config.maximumIterations << '\n'
         << "Panels: " << paneling.panelCount << '\n'
         << "Alpha interval: [" << formatNumber(options.alphaStart) << ", "
         << formatNumber(options.alphaEndExclusive) << "), step "
         << formatNumber(options.alphaStep) << "\n\n";
  return stream.str();
}

std::string sequenceTableHeader() {
  return
      "Alpha sequence\n"
      "     alpha          CL          CD         CDp         CDF          CM  "
      "iterations  status\n";
}

std::string formatSequenceRow(const xfoil::PolarPoint& point) {
  return formatRow(formatAlpha(point.alpha.value), std::nullopt, std::nullopt,
                   "n/a", point);
}

std::string clTableHeader() {
  return
      "Specified CL\n"
      "target_CL      alpha          CL          CD         CDp         CDF          "
      "CM  iterations  status\n";
}

std::string formatClRow(
    double targetLiftCoefficient,
    const xfoil::OperatingPointData& point) {
  xfoil::PolarPoint polarPoint;
  polarPoint.alpha = point.alpha;
  polarPoint.liftCoefficient = point.liftCoefficient;
  polarPoint.dragCoefficient = point.dragCoefficient;
  polarPoint.dragCoefficientPressure = point.dragCoefficientPressure;
  polarPoint.momentCoefficient = point.momentCoefficient;
  polarPoint.convergence = point.convergence;
  return formatRow(formatAlpha(targetLiftCoefficient),
                   formatAlpha(point.alpha.value),
                   point.dragCoefficientFriction,
                   std::to_string(point.iterations), polarPoint);
}

std::string statusText(xfoil::ConvergenceStatus status) {
  switch (status) {
    case xfoil::ConvergenceStatus::notRun:
      return "not-run";
    case xfoil::ConvergenceStatus::converged:
      return "converged";
    case xfoil::ConvergenceStatus::unconverged:
      return "unconverged";
    case xfoil::ConvergenceStatus::cancelled:
      return "cancelled";
    case xfoil::ConvergenceStatus::failed:
      return "failed";
  }
  return "failed";
}

std::string cpPointCount(std::size_t count) {
  return "Cp points: " + std::to_string(count);
}

FrictionOutputResult formatClFriction(
    const xfoil::OperatingPointData& point) {
  const bool hasCdf = point.dragCoefficientFriction.has_value();
  const bool hasCf = point.skinFriction.has_value();
  if (hasCdf != hasCf) {
    return {FrictionOutputStatus::inconsistent, {},
            "inconsistent friction data: CDF and Cf presence differ"};
  }
  if (point.viscous != hasCf) {
    return {FrictionOutputStatus::inconsistent, {},
            "inconsistent friction data: viscous flag and Cf presence differ"};
  }
  if (!hasCf) {
    return {FrictionOutputStatus::ok,
            "Cf upper stations: n/a\nCf lower stations: n/a", {}};
  }

  const xfoil::SkinFrictionDistribution& distribution = *point.skinFriction;
  const xfoil::SkinFrictionSide& upper = distribution.upper;
  const xfoil::SkinFrictionSide& lower = distribution.lower;
  const bool upperConsistent =
      upper.x.size() == upper.y.size() &&
      upper.y.size() == upper.s.size() &&
      upper.s.size() == upper.cf.size();
  const bool lowerConsistent =
      lower.x.size() == lower.y.size() &&
      lower.y.size() == lower.s.size() &&
      lower.s.size() == lower.cf.size();
  if (!upperConsistent || !lowerConsistent) {
    return {FrictionOutputStatus::inconsistent, {},
            "inconsistent Cf station data: x/y/s/cf lengths differ"};
  }

  return {FrictionOutputStatus::ok,
          "Cf upper stations: " + std::to_string(upper.cf.size()) +
              "\nCf lower stations: " +
              std::to_string(lower.cf.size()),
          {}};
}

}  // namespace xfoil_demo

// End: library-demo/DemoSupport.cpp

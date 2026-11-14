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

// File: src/session/PublicSession.cpp
// Public in-memory session facade. The implementation delegates to the
// existing SessionOps, core kernels and L2 projections. It owns exactly
// one SessionComposition, performs no file or console I/O and never exits
// the host process.

#include "xfoil/Session.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <system_error>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionErrorMapping.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionProjections.h"
#include "xfoil/session/SessionState.h"

namespace xfoil {
namespace {

constexpr std::size_t kMaximumSequencePoints = 100000;
constexpr std::size_t kMaximumPanelCount = 595;
constexpr std::size_t kMinimumPanelCount = 2;
constexpr std::size_t kNacaSideCount = 200;

std::string_view trimSpaces(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t')) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t')) {
    --end;
  }
  return text.substr(begin, end - begin);
}

char upperAscii(char value) {
  if (value >= 'a' && value <= 'z') {
    return static_cast<char>(value - 'a' + 'A');
  }
  return value;
}

bool startsWithIgnoreCase(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (std::size_t index = 0; index < prefix.size(); ++index) {
    if (upperAscii(text[index]) != upperAscii(prefix[index])) {
      return false;
    }
  }
  return true;
}

Result<int> parseNacaDesignation(std::string_view designation) {
  std::string_view text = trimSpaces(designation);
  if (startsWithIgnoreCase(text, "NACS") ||
      startsWithIgnoreCase(text, "NACA")) {
    text = trimSpaces(text.substr(4));
  }
  if (text.empty()) {
    return Result<int>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "naca.invalidDesignation",
                    "Empty NACA designation", "naca"));
  }
  for (const char digit : text) {
    if (digit < '0' || digit > '9') {
      return Result<int>::fail(
          Error::make(ErrorCategory::invalidArgument,
                      "naca.invalidDesignation",
                      "NACA designation must contain decimal digits only",
                      "naca"));
    }
  }

  int value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end) {
    return Result<int>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "naca.invalidDesignation",
                    "NACA designation must contain decimal digits only",
                    "naca"));
  }
  return Result<int>::ok(value);
}

Result<geometry::AirfoilGeometry> makeNacaGeometry(
    std::string_view designation, bool sharpTrailingEdge) {
  const Result<int> parsed = parseNacaDesignation(designation);
  if (parsed.isErr()) {
    return Result<geometry::AirfoilGeometry>::fail(parsed.error());
  }

  const core::Result<core::geometry::NacaAirfoilResult> naca =
      core::geometry::nacaAirfoil(parsed.value(), sharpTrailingEdge,
                                  kNacaSideCount);
  if (!naca.isOk()) {
    return Result<geometry::AirfoilGeometry>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "naca.designationNotImplemented",
                    naca.error().message, "naca"));
  }

  std::vector<Point2D> points;
  points.reserve(naca.value().buffer.x.size());
  for (std::size_t index = 0; index < naca.value().buffer.x.size();
       ++index) {
    points.push_back(
        Point2D{naca.value().buffer.x[index], naca.value().buffer.y[index]});
  }

  geometry::AirfoilGeometry geometry;
  const geometry::GeometryAssignmentResult assigned =
      geometry.assign(naca.value().buffer.name, points);
  if (!assigned.success()) {
    return Result<geometry::AirfoilGeometry>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "naca.geometryAssignmentFailed", assigned.message,
                    "naca"));
  }
  return Result<geometry::AirfoilGeometry>::ok(std::move(geometry));
}

Error sequenceError(ErrorCategory category, std::string code,
                    std::string message) {
  return Error::make(category, std::move(code), std::move(message),
                     "sequence");
}

}  // namespace

static_assert(std::is_nothrow_move_assignable_v<session::SessionComposition>);

class Session::Impl {
 public:
  session::SessionComposition session;
};

Session::Session() : impl_(std::make_unique<Impl>()) {}

Session::~Session() = default;

Session::Session(Session&&) noexcept = default;

Session& Session::operator=(Session&&) noexcept = default;

Result<void> Session::setGeometry(
    const geometry::AirfoilGeometry& geometry) {
  const GeometrySetResult set =
      session::setGeometry(impl_->session, geometry);
  if (set.error != GeometrySetError::none) {
    return Result<void>::fail(session::geometrySetError(set));
  }
  return Result<void>::ok();
}

Result<geometry::AirfoilGeometry> Session::setNaca(
    std::string_view designation) {
  Result<geometry::AirfoilGeometry> geometry =
      makeNacaGeometry(designation, false);
  if (geometry.isErr()) {
    return geometry;
  }
  const Result<void> set = setGeometry(geometry.value());
  if (set.isErr()) {
    return Result<geometry::AirfoilGeometry>::fail(set.error());
  }
  return geometry;
}

Result<geometry::AirfoilGeometry> Session::setNacs(
    std::string_view designation) {
  Result<geometry::AirfoilGeometry> geometry =
      makeNacaGeometry(designation, true);
  if (geometry.isErr()) {
    return geometry;
  }
  const Result<void> set = setGeometry(geometry.value());
  if (set.isErr()) {
    return Result<geometry::AirfoilGeometry>::fail(set.error());
  }
  return geometry;
}

Result<SurfaceGeometry> Session::generatePanel(
    const PanelingOptions& options) {
  if (!std::isfinite(options.bunchingParameter) ||
      !std::isfinite(options.teToLeDensityRatio) ||
      !std::isfinite(options.refinementDensityRatio) ||
      !std::isfinite(options.upperRefinementStart) ||
      !std::isfinite(options.upperRefinementEnd) ||
      !std::isfinite(options.lowerRefinementStart) ||
      !std::isfinite(options.lowerRefinementEnd)) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "paneling.invalidParameters",
                    "Paneling parameters must be finite", "paneling"));
  }
  if (options.panelCount < kMinimumPanelCount) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "paneling.panelCount",
                    "Number of panel nodes must be at least 2", "paneling"));
  }
  if (options.panelCount > kMaximumPanelCount) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::capacityExceeded,
                    "paneling.panelCount",
                    "Panel count exceeds the supported maximum", "paneling"));
  }
  if (!impl_->session.geometry.valid) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidState,
                    "paneling.bufferUnavailable",
                    "No buffer airfoil is available", "paneling"));
  }

  const core::paneling::PanelingParameters previous =
      impl_->session.paneling.parameters;
  core::paneling::PanelingParameters parameters = previous;
  parameters.panelCount = static_cast<int>(options.panelCount);
  parameters.bunchingParam = options.bunchingParameter;
  parameters.teToLeDensityRatio = options.teToLeDensityRatio;
  parameters.refinementDensityRatio = options.refinementDensityRatio;
  parameters.upperRefinementRange = {options.upperRefinementStart,
                                     options.upperRefinementEnd};
  parameters.lowerRefinementRange = {options.lowerRefinementStart,
                                     options.lowerRefinementEnd};

  const session::SessionOperationStatus stored =
      session::setPanelingParameters(impl_->session, parameters);
  if (!stored.ok) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "paneling.invalidParameters", stored.message,
                    "paneling"));
  }

  const session::PanelingApplyResult applied =
      session::applyPaneling(impl_->session);
  if (!applied.ok) {
    impl_->session.paneling.parameters = previous;
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidState, "paneling.failed",
                    applied.message, "paneling"));
  }

  return projectCurrentPanel(impl_->session);
}

Result<void> Session::configure(const AnalysisConfig& config) {
  const ConfigValidationResult configured =
      session::configure(impl_->session, config);
  if (!configured.success()) {
    return Result<void>::fail(session::configError(configured));
  }
  return Result<void>::ok();
}

Result<OperatingPointData> Session::solveAlpha(
    Degrees alpha, const CancellationToken* cancellation) {
  if (!std::isfinite(alpha.value)) {
    return Result<OperatingPointData>::fail(
        Error::make(ErrorCategory::invalidArgument, "analysis.invalidAlpha",
                    "Alpha must be finite", "analysis"));
  }
  const AnalysisResult result =
      session::runSpecAlpha(impl_->session, alpha, cancellation);
  if (!result.successful) {
    return Result<OperatingPointData>::fail(
        session::analysisStatusError(result.status));
  }
  return projectLastOperatingPoint(impl_->session);
}

Result<OperatingPointData> Session::solveCl(
    double cl, const CancellationToken* cancellation) {
  if (!std::isfinite(cl)) {
    return Result<OperatingPointData>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "analysis.invalidLiftCoefficient",
                    "Lift coefficient must be finite", "analysis"));
  }
  const AnalysisResult result =
      session::runSpecCl(impl_->session, cl, Degrees{0.0}, cancellation);
  if (!result.successful) {
    return Result<OperatingPointData>::fail(
        session::analysisStatusError(result.status));
  }
  return projectLastOperatingPoint(impl_->session);
}

Result<SequenceResult> Session::solveAlphaSequence(
    const AlphaSequence& sequence, const CancellationToken* cancellation) {
  const double start = sequence.start.value;
  const double end = sequence.endExclusive.value;
  const double step = sequence.step.value;
  if (!std::isfinite(start) || !std::isfinite(end) ||
      !std::isfinite(step)) {
    return Result<SequenceResult>::fail(
        sequenceError(ErrorCategory::invalidArgument, "sequence.invalidRange",
                      "Sequence values must be finite"));
  }
  if (step == 0.0) {
    return Result<SequenceResult>::fail(
        sequenceError(ErrorCategory::invalidArgument, "sequence.zeroStep",
                      "Sequence step must be non-zero"));
  }
  if (!((step > 0.0 && start < end) || (step < 0.0 && start > end))) {
    return Result<SequenceResult>::fail(
        sequenceError(ErrorCategory::invalidArgument,
                      "sequence.direction",
                      "Sequence step direction does not match start and end"));
  }
  const long double span =
      (static_cast<long double>(end) - static_cast<long double>(start)) /
      static_cast<long double>(step);
  if (span <= 0.0L) {
    return Result<SequenceResult>::fail(
        sequenceError(ErrorCategory::invalidArgument, "sequence.invalidRange",
                      "Sequence range is empty"));
  }
  const long double expectedCount = std::ceil(span);
  if (expectedCount >
      static_cast<long double>(kMaximumSequencePoints)) {
    return Result<SequenceResult>::fail(
        sequenceError(ErrorCategory::capacityExceeded,
                      "sequence.pointLimit",
                      "Sequence exceeds the point limit"));
  }
  if (!impl_->session.geometry.valid ||
      !impl_->session.paneling.panelingValid) {
    return Result<SequenceResult>::fail(
        Error::make(ErrorCategory::invalidState, "analysis.missingGeometry",
                    "No valid panel geometry is available", "analysis"));
  }
  if (!impl_->session.operating.configured) {
    return Result<SequenceResult>::fail(
        Error::make(ErrorCategory::invalidState,
                    "analysis.missingConfiguration",
                    "No validated analysis configuration is available",
                    "analysis"));
  }

  const std::vector<AnalysisResult> results = session::runAlphaSequence(
      impl_->session, sequence.start, sequence.endExclusive, sequence.step,
      cancellation);
  if (results.empty()) {
    return Result<SequenceResult>::fail(
        sequenceError(ErrorCategory::internal, "sequence.empty",
                      "Sequence produced no result"));
  }

  SequenceStopReason stopReason = SequenceStopReason::completed;
  std::optional<std::size_t> stoppedAtIndex;
  std::size_t includedCount = results.size();
  for (std::size_t index = 0; index < results.size(); ++index) {
    if (results[index].status == AnalysisStatus::cancelled) {
      stopReason = SequenceStopReason::cancelled;
      stoppedAtIndex = index;
      includedCount = index + 1;
      break;
    }
    if (!results[index].successful) {
      if (index == 0) {
        return Result<SequenceResult>::fail(
            session::analysisStatusError(results[index].status));
      }
      if (results[index].status == AnalysisStatus::invalidConfiguration) {
        if (results.size() >= kMaximumSequencePoints) {
          return Result<SequenceResult>::fail(
              sequenceError(ErrorCategory::capacityExceeded,
                            "sequence.pointLimit",
                            "Sequence exceeds the point limit"));
        }
        return Result<SequenceResult>::fail(
            sequenceError(ErrorCategory::numericalFailure,
                          "sequence.stalled",
                          "Sequence step does not advance alpha"));
      }
      stopReason = SequenceStopReason::failure;
      stoppedAtIndex = index;
      includedCount = index + 1;
      break;
    }
  }

  const std::span<const AnalysisResult> included(results.data(),
                                                 includedCount);
  return Result<SequenceResult>::ok(
      session::projectSequence(included, stopReason, stoppedAtIndex));
}

Result<SurfaceGeometry> Session::currentPanel() const {
  return projectCurrentPanel(impl_->session);
}

Result<SurfaceDistribution> Session::currentSurfaceDistribution() const {
  return projectSurfaceDistribution(impl_->session);
}

Result<OperatingPointData> Session::lastOperatingPoint() const {
  return projectLastOperatingPoint(impl_->session);
}

Result<SequenceResult> Session::activePolar() const {
  return projectActivePolar(impl_->session);
}

void Session::reset() noexcept {
  impl_->session = session::SessionComposition{};
}

}  // namespace xfoil

// End: src/session/PublicSession.cpp

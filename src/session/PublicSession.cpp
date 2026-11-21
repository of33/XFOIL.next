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
#include "xfoil/session/ProgressObserver.h"
#include "xfoil/session/SessionErrorMapping.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionProjections.h"
#include "xfoil/session/SessionState.h"

namespace xfoil {

static Result<void> setGeometryImpl(
    session::SessionComposition& composition,
    const geometry::AirfoilGeometry& geometry,
    const ProgressCallback* progress);
static Result<geometry::AirfoilGeometry> setNacaImpl(
    session::SessionComposition& composition, std::string_view designation,
    bool sharpTrailingEdge, const ProgressCallback* progress);
static Result<SurfaceGeometry> generatePanelImpl(
    session::SessionComposition& composition, const PanelingOptions& options,
    const ProgressCallback* progress);
static Result<OperatingPointData> solveAlphaImpl(
    session::SessionComposition& composition, Degrees alpha,
    const CancellationToken* cancellation, const ProgressCallback* progress);
static Result<OperatingPointData> solveClImpl(
    session::SessionComposition& composition, double cl,
    const CancellationToken* cancellation, const ProgressCallback* progress);
static Result<SequenceResult> solveAlphaSequenceImpl(
    session::SessionComposition& composition, const AlphaSequence& sequence,
    const CancellationToken* cancellation, const ProgressCallback* progress);
static Result<DetailedSequenceResult> solveDetailedAlphaSequenceImpl(
    session::SessionComposition& composition, const AlphaSequence& sequence,
    const CancellationToken* cancellation, const ProgressCallback* progress);

struct AlphaSequenceRun {
  std::vector<session::SequencePointRecord> records;
  SequenceStopReason stopReason{SequenceStopReason::completed};
  std::optional<std::size_t> stoppedAtIndex;
};

static Result<AlphaSequenceRun> runAlphaSequenceImpl(
    session::SessionComposition& composition, const AlphaSequence& sequence,
    const CancellationToken* cancellation, const ProgressCallback* progress,
    bool captureOperatingPoint);

namespace {

class CallbackProgressObserver final : public session::ProgressObserver {
 public:
  explicit CallbackProgressObserver(const ProgressCallback* callback)
      : callback_(callback) {}

  [[nodiscard]]
  session::ProgressControl report(const ProgressEvent& event) noexcept override {
    if (callback_ == nullptr) {
      return session::ProgressControl::continueOperation;
    }
    try {
      (*callback_)(event);
    } catch (...) {
      return session::ProgressControl::callbackFailed;
    }
    return session::ProgressControl::continueOperation;
  }

 private:
  const ProgressCallback* callback_{nullptr};
};

session::ProgressObserver* observerFor(
    const ProgressCallback* callback, CallbackProgressObserver& observer) {
  return callback == nullptr ? nullptr : &observer;
}

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
  return setGeometryImpl(impl_->session, geometry, nullptr);
}

Result<void> Session::setGeometry(
    const geometry::AirfoilGeometry& geometry,
    const ProgressCallback& progress) {
  return setGeometryImpl(impl_->session, geometry, &progress);
}

static Result<void> setGeometryImpl(
    session::SessionComposition& composition,
    const geometry::AirfoilGeometry& geometry,
    const ProgressCallback* progress) {
  CallbackProgressObserver observer(progress);
  session::ProgressObserver* const progressObserver =
      observerFor(progress, observer);
  if (progressObserver != nullptr) {
    const session::ProgressControl control = progressObserver->report(
        session::makeProgressEvent(ProgressPhase::geometry, 0, 1));
    if (control == session::ProgressControl::callbackFailed) {
      return Result<void>::fail(session::progressCallbackFailureError());
    }
  }

  const GeometrySetResult set =
      session::setGeometry(composition, geometry);
  if (set.error != GeometrySetError::none) {
    return Result<void>::fail(session::geometrySetError(set));
  }

  if (progressObserver != nullptr) {
    const session::ProgressControl control = progressObserver->report(
        session::makeProgressEvent(ProgressPhase::geometry, 1, 1));
    if (control == session::ProgressControl::callbackFailed) {
      return Result<void>::fail(session::progressCallbackFailureError());
    }
  }
  return Result<void>::ok();
}

Result<geometry::AirfoilGeometry> Session::setNaca(
    std::string_view designation) {
  return setNacaImpl(impl_->session, designation, false, nullptr);
}

Result<geometry::AirfoilGeometry> Session::setNaca(
    std::string_view designation, const ProgressCallback& progress) {
  return setNacaImpl(impl_->session, designation, false, &progress);
}

Result<geometry::AirfoilGeometry> Session::setNacs(
    std::string_view designation) {
  return setNacaImpl(impl_->session, designation, true, nullptr);
}

Result<geometry::AirfoilGeometry> Session::setNacs(
    std::string_view designation, const ProgressCallback& progress) {
  return setNacaImpl(impl_->session, designation, true, &progress);
}

static Result<geometry::AirfoilGeometry> setNacaImpl(
    session::SessionComposition& composition, std::string_view designation,
    bool sharpTrailingEdge, const ProgressCallback* progress) {
  Result<geometry::AirfoilGeometry> geometry =
      makeNacaGeometry(designation, sharpTrailingEdge);
  if (geometry.isErr()) {
    return geometry;
  }
  const Result<void> set =
      setGeometryImpl(composition, geometry.value(), progress);
  if (set.isErr()) {
    return Result<geometry::AirfoilGeometry>::fail(set.error());
  }
  return geometry;
}

Result<SurfaceGeometry> Session::generatePanel(
    const PanelingOptions& options) {
  return generatePanelImpl(impl_->session, options, nullptr);
}

Result<SurfaceGeometry> Session::generatePanel(
    const PanelingOptions& options, const ProgressCallback& progress) {
  return generatePanelImpl(impl_->session, options, &progress);
}

static Result<SurfaceGeometry> generatePanelImpl(
    session::SessionComposition& composition, const PanelingOptions& options,
    const ProgressCallback* progress) {
  CallbackProgressObserver observer(progress);
  session::ProgressObserver* const progressObserver =
      observerFor(progress, observer);
  if (progressObserver != nullptr) {
    const session::ProgressControl control = progressObserver->report(
        session::makeProgressEvent(ProgressPhase::paneling, 0, 1));
    if (control == session::ProgressControl::callbackFailed) {
      return Result<SurfaceGeometry>::fail(
          session::progressCallbackFailureError());
    }
  }

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
  if (!composition.geometry.valid) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidState,
                    "paneling.bufferUnavailable",
                    "No buffer airfoil is available", "paneling"));
  }

  const core::paneling::PanelingParameters previous =
      composition.paneling.parameters;
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
      session::setPanelingParameters(composition, parameters);
  if (!stored.ok) {
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "paneling.invalidParameters", stored.message,
                    "paneling"));
  }

  const session::PanelingApplyResult applied =
      session::applyPaneling(composition);
  if (!applied.ok) {
    composition.paneling.parameters = previous;
    return Result<SurfaceGeometry>::fail(
        Error::make(ErrorCategory::invalidState, "paneling.failed",
                    applied.message, "paneling"));
  }

  const Result<SurfaceGeometry> current =
      projectCurrentPanel(composition);
  if (current.isErr()) {
    return current;
  }
  if (progressObserver != nullptr) {
    const session::ProgressControl control = progressObserver->report(
        session::makeProgressEvent(ProgressPhase::paneling, 1, 1));
    if (control == session::ProgressControl::callbackFailed) {
      return Result<SurfaceGeometry>::fail(
          session::progressCallbackFailureError());
    }
  }
  return current;
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
  return solveAlphaImpl(impl_->session, alpha, cancellation, nullptr);
}

Result<OperatingPointData> Session::solveAlpha(
    Degrees alpha, const CancellationToken* cancellation,
    const ProgressCallback& progress) {
  return solveAlphaImpl(impl_->session, alpha, cancellation, &progress);
}

static Result<OperatingPointData> solveAlphaImpl(
    session::SessionComposition& composition, Degrees alpha,
    const CancellationToken* cancellation, const ProgressCallback* progress) {
  if (!std::isfinite(alpha.value)) {
    return Result<OperatingPointData>::fail(
        Error::make(ErrorCategory::invalidArgument, "analysis.invalidAlpha",
                    "Alpha must be finite", "analysis"));
  }
  CallbackProgressObserver observer(progress);
  const AnalysisResult result = session::runSpecAlpha(
      composition, alpha, cancellation, observerFor(progress, observer));
  if (!result.successful) {
    return Result<OperatingPointData>::fail(
        session::analysisStatusError(result.status));
  }
  return projectLastOperatingPoint(composition);
}

Result<OperatingPointData> Session::solveCl(
    double cl, const CancellationToken* cancellation) {
  return solveClImpl(impl_->session, cl, cancellation, nullptr);
}

Result<OperatingPointData> Session::solveCl(
    double cl, const CancellationToken* cancellation,
    const ProgressCallback& progress) {
  return solveClImpl(impl_->session, cl, cancellation, &progress);
}

static Result<OperatingPointData> solveClImpl(
    session::SessionComposition& composition, double cl,
    const CancellationToken* cancellation, const ProgressCallback* progress) {
  if (!std::isfinite(cl)) {
    return Result<OperatingPointData>::fail(
        Error::make(ErrorCategory::invalidArgument,
                    "analysis.invalidLiftCoefficient",
                    "Lift coefficient must be finite", "analysis"));
  }
  CallbackProgressObserver observer(progress);
  const AnalysisResult result =
      session::runSpecCl(composition, cl, Degrees{0.0}, cancellation,
                         observerFor(progress, observer));
  if (!result.successful) {
    return Result<OperatingPointData>::fail(
        session::analysisStatusError(result.status));
  }
  return projectLastOperatingPoint(composition);
}

Result<SequenceResult> Session::solveAlphaSequence(
    const AlphaSequence& sequence, const CancellationToken* cancellation) {
  return solveAlphaSequenceImpl(impl_->session, sequence, cancellation, nullptr);
}

Result<SequenceResult> Session::solveAlphaSequence(
    const AlphaSequence& sequence, const CancellationToken* cancellation,
    const ProgressCallback& progress) {
  return solveAlphaSequenceImpl(impl_->session, sequence, cancellation, &progress);
}

Result<DetailedSequenceResult> Session::solveDetailedAlphaSequence(
    const AlphaSequence& sequence, const CancellationToken* cancellation) {
  return solveDetailedAlphaSequenceImpl(impl_->session, sequence, cancellation,
                                        nullptr);
}

Result<DetailedSequenceResult> Session::solveDetailedAlphaSequence(
    const AlphaSequence& sequence, const CancellationToken* cancellation,
    const ProgressCallback& progress) {
  return solveDetailedAlphaSequenceImpl(impl_->session, sequence, cancellation,
                                        &progress);
}

static Result<AlphaSequenceRun> runAlphaSequenceImpl(
    session::SessionComposition& composition, const AlphaSequence& sequence,
    const CancellationToken* cancellation, const ProgressCallback* progress,
    bool captureOperatingPoint) {
  const double start = sequence.start.value;
  const double end = sequence.endExclusive.value;
  const double step = sequence.step.value;
  if (!std::isfinite(start) || !std::isfinite(end) ||
      !std::isfinite(step)) {
    return Result<AlphaSequenceRun>::fail(
        sequenceError(ErrorCategory::invalidArgument, "sequence.invalidRange",
                      "Sequence values must be finite"));
  }
  if (step == 0.0) {
    return Result<AlphaSequenceRun>::fail(
        sequenceError(ErrorCategory::invalidArgument, "sequence.zeroStep",
                      "Sequence step must be non-zero"));
  }
  if (!((step > 0.0 && start < end) || (step < 0.0 && start > end))) {
    return Result<AlphaSequenceRun>::fail(
        sequenceError(ErrorCategory::invalidArgument,
                      "sequence.direction",
                      "Sequence step direction does not match start and end"));
  }
  const long double span =
      (static_cast<long double>(end) - static_cast<long double>(start)) /
      static_cast<long double>(step);
  if (span <= 0.0L) {
    return Result<AlphaSequenceRun>::fail(
        sequenceError(ErrorCategory::invalidArgument, "sequence.invalidRange",
                      "Sequence range is empty"));
  }
  const long double expectedCount = std::ceil(span);
  if (expectedCount >
      static_cast<long double>(kMaximumSequencePoints)) {
    return Result<AlphaSequenceRun>::fail(
        sequenceError(ErrorCategory::capacityExceeded,
                      "sequence.pointLimit",
                      "Sequence exceeds the point limit"));
  }
  if (!composition.geometry.valid ||
      !composition.paneling.panelingValid) {
    return Result<AlphaSequenceRun>::fail(
        Error::make(ErrorCategory::invalidState, "analysis.missingGeometry",
                    "No valid panel geometry is available", "analysis"));
  }
  if (!composition.operating.configured) {
    return Result<AlphaSequenceRun>::fail(
        Error::make(ErrorCategory::invalidState,
                    "analysis.missingConfiguration",
                    "No validated analysis configuration is available",
                    "analysis"));
  }

  CallbackProgressObserver observer(progress);
  std::vector<session::SequencePointRecord> results =
      session::runAlphaSequenceDetailed(
          composition, sequence.start, sequence.endExclusive, sequence.step,
          cancellation, observerFor(progress, observer),
          captureOperatingPoint);
  if (results.empty()) {
    return Result<AlphaSequenceRun>::fail(
        sequenceError(ErrorCategory::internal, "sequence.empty",
                      "Sequence produced no result"));
  }

  AlphaSequenceRun run;
  run.stopReason = SequenceStopReason::completed;
  std::size_t includedCount = results.size();
  for (std::size_t index = 0; index < results.size(); ++index) {
    if (results[index].result.status ==
        AnalysisStatus::progressCallbackFailure) {
      return Result<AlphaSequenceRun>::fail(
          session::progressCallbackFailureError());
    }
    if (results[index].result.status == AnalysisStatus::cancelled) {
      run.stopReason = SequenceStopReason::cancelled;
      run.stoppedAtIndex = index;
      includedCount = index + 1;
      break;
    }
    if (!results[index].result.successful) {
      if (index == 0) {
        return Result<AlphaSequenceRun>::fail(
            session::analysisStatusError(results[index].result.status));
      }
      if (results[index].result.status ==
          AnalysisStatus::invalidConfiguration) {
        if (results.size() >= kMaximumSequencePoints) {
          return Result<AlphaSequenceRun>::fail(
              sequenceError(ErrorCategory::capacityExceeded,
                            "sequence.pointLimit",
                            "Sequence exceeds the point limit"));
        }
        return Result<AlphaSequenceRun>::fail(
            sequenceError(ErrorCategory::numericalFailure,
                          "sequence.stalled",
                          "Sequence step does not advance alpha"));
      }
      run.stopReason = SequenceStopReason::failure;
      run.stoppedAtIndex = index;
      includedCount = index + 1;
      break;
    }
  }

  results.resize(includedCount);
  run.records = std::move(results);
  return Result<AlphaSequenceRun>::ok(std::move(run));
}

static Result<SequenceResult> solveAlphaSequenceImpl(
    session::SessionComposition& composition, const AlphaSequence& sequence,
    const CancellationToken* cancellation, const ProgressCallback* progress) {
  Result<AlphaSequenceRun> run = runAlphaSequenceImpl(
      composition, sequence, cancellation, progress, false);
  if (run.isErr()) {
    return Result<SequenceResult>::fail(run.error());
  }
  return Result<SequenceResult>::ok(
      session::projectSequence(run.value().records, run.value().stopReason,
                               run.value().stoppedAtIndex));
}

static Result<DetailedSequenceResult> solveDetailedAlphaSequenceImpl(
    session::SessionComposition& composition, const AlphaSequence& sequence,
    const CancellationToken* cancellation, const ProgressCallback* progress) {
  Result<AlphaSequenceRun> run = runAlphaSequenceImpl(
      composition, sequence, cancellation, progress, true);
  if (run.isErr()) {
    return Result<DetailedSequenceResult>::fail(run.error());
  }
  return Result<DetailedSequenceResult>::ok(session::projectDetailedSequence(
      run.value().records, run.value().stopReason,
      run.value().stoppedAtIndex));
}

Result<SurfaceGeometry> Session::currentPanel() const {
  return projectCurrentPanel(impl_->session);
}

Result<SurfaceDistribution> Session::currentSurfaceDistribution() const {
  return projectSurfaceDistribution(impl_->session);
}

Result<SkinFrictionDistribution> Session::currentSkinFriction() const {
  return projectSkinFriction(impl_->session);
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

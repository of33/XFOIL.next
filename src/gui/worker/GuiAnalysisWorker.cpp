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

// File: src/gui/worker/GuiAnalysisWorker.cpp
// The only GUI-side owner of a solver session. Setup is applied by short
// blocking calls before a job; analysis runs through public Session
// methods and reports owning values plus one terminal signal.

#include "worker/GuiAnalysisWorker.h"

#include <cmath>
#include <exception>
#include <string>
#include <utility>
#include <variant>

#include "xfoil/Session.h"

namespace xfoil::gui {
namespace {

Error workerUnavailableError() {
  return Error::make(ErrorCategory::invalidState,
                     "gui.workerUnavailable",
                     "The analysis worker session is not available",
                     "worker");
}

Error unexpectedWorkerError() {
  return Error::make(ErrorCategory::internal, "gui.workerException",
                     "The analysis worker caught an unexpected exception",
                     "worker");
}

Error setupRequestError(ErrorCategory category, std::string code,
                        std::string message) {
  return Error::make(category, std::move(code), std::move(message),
                     "setup");
}

Result<void> validateSetupSequence(double start, double endExclusive,
                                   double step) {
  if (!std::isfinite(start) || !std::isfinite(endExclusive) ||
      !std::isfinite(step)) {
    return Result<void>::fail(setupRequestError(
        ErrorCategory::invalidArgument, "sequence.invalidRange",
        "Sequence values must be finite"));
  }
  if (step == 0.0) {
    return Result<void>::fail(setupRequestError(
        ErrorCategory::invalidArgument, "sequence.zeroStep",
        "Sequence step must be non-zero"));
  }
  if (!((step > 0.0 && start < endExclusive) ||
        (step < 0.0 && start > endExclusive))) {
    return Result<void>::fail(setupRequestError(
        ErrorCategory::invalidArgument, "sequence.direction",
        "Sequence step direction does not match start and end"));
  }
  const long double span =
      (static_cast<long double>(endExclusive) -
       static_cast<long double>(start)) /
      static_cast<long double>(step);
  if (span <= 0.0L ||
      std::ceil(span) > 100000.0L) {
    return Result<void>::fail(setupRequestError(
        span <= 0.0L ? ErrorCategory::invalidArgument
                     : ErrorCategory::capacityExceeded,
        span <= 0.0L ? "sequence.invalidRange" : "sequence.pointLimit",
        span <= 0.0L ? "Sequence range is empty"
                     : "Sequence exceeds the point limit"));
  }
  return Result<void>::ok();
}

Result<void> validateSetupRequest(const AnalysisWorkflowRequest& request) {
  if (const auto* alpha = std::get_if<AlphaTarget>(&request)) {
    if (!std::isfinite(alpha->alpha.value)) {
      return Result<void>::fail(setupRequestError(
          ErrorCategory::invalidArgument, "target.nonFiniteAlpha",
          "Alpha target must be finite"));
    }
    return Result<void>::ok();
  }
  if (const auto* lift = std::get_if<LiftTarget>(&request)) {
    if (!std::isfinite(lift->cl)) {
      return Result<void>::fail(setupRequestError(
          ErrorCategory::invalidArgument, "target.nonFiniteLift",
          "Lift target must be finite"));
    }
    return Result<void>::ok();
  }
  if (const auto* sequence = std::get_if<AlphaSequence>(&request)) {
    return validateSetupSequence(sequence->start.value,
                                 sequence->endExclusive.value,
                                 sequence->step.value);
  }
  if (const auto* sequence = std::get_if<LiftSequence>(&request)) {
    return validateSetupSequence(sequence->start, sequence->endExclusive,
                                 sequence->step);
  }
  return Result<void>::fail(setupRequestError(
      ErrorCategory::invalidState, "gui.invalidSetupRequest",
      "The setup request has no supported analysis target"));
}

}  // namespace

GuiAnalysisWorker::GuiAnalysisWorker(QObject* parent) : QObject(parent) {}

GuiAnalysisWorker::~GuiAnalysisWorker() = default;

Result<void> GuiAnalysisWorker::initialize() {
  if (!session_) {
    session_ = std::make_unique<Session>();
  }
  return Result<void>::ok();
}

Result<void> GuiAnalysisWorker::requireSession() const {
  if (!session_) {
    return Result<void>::fail(workerUnavailableError());
  }
  return Result<void>::ok();
}

Result<void> GuiAnalysisWorker::setGeometry(
    const geometry::AirfoilGeometry& geometry) {
  if (const Result<void> session = requireSession(); session.isErr()) {
    return session;
  }
  return session_->setGeometry(geometry);
}

Result<geometry::AirfoilGeometry> GuiAnalysisWorker::setNaca(
    std::string_view designation, bool sharpTrailingEdge) {
  if (const Result<void> session = requireSession(); session.isErr()) {
    return Result<geometry::AirfoilGeometry>::fail(session.error());
  }
  if (sharpTrailingEdge) {
    return session_->setNacs(designation);
  }
  return session_->setNaca(designation);
}

Result<SurfaceGeometry> GuiAnalysisWorker::generatePanel(
    const PanelingOptions& options) {
  if (const Result<void> session = requireSession(); session.isErr()) {
    return Result<SurfaceGeometry>::fail(session.error());
  }
  return session_->generatePanel(options);
}

Result<void> GuiAnalysisWorker::setConfiguration(
    const AnalysisConfig& configuration) {
  if (const Result<void> session = requireSession(); session.isErr()) {
    return session;
  }
  return session_->configure(configuration);
}

Result<GuiAppliedSetup> GuiAnalysisWorker::applySetup(
    const AnalysisWorkflowSetup& setup) {
  if (const Result<void> session = requireSession(); session.isErr()) {
    return Result<GuiAppliedSetup>::fail(session.error());
  }

  auto candidate = std::make_unique<Session>();
  geometry::AirfoilGeometry geometry;
  if (const auto* memory =
          std::get_if<MemoryGeometrySetup>(&setup.geometry)) {
    const Result<void> assigned = candidate->setGeometry(memory->geometry);
    if (assigned.isErr()) {
      return Result<GuiAppliedSetup>::fail(assigned.error());
    }
    geometry = memory->geometry;
  } else if (const auto* naca =
                 std::get_if<NacaGeometrySetup>(&setup.geometry)) {
    const Result<geometry::AirfoilGeometry> generated =
        candidate->setNaca(naca->designation);
    if (generated.isErr()) {
      return Result<GuiAppliedSetup>::fail(generated.error());
    }
    geometry = generated.value();
  } else if (const auto* nacs =
                 std::get_if<NacsGeometrySetup>(&setup.geometry)) {
    const Result<geometry::AirfoilGeometry> generated =
        candidate->setNacs(nacs->designation);
    if (generated.isErr()) {
      return Result<GuiAppliedSetup>::fail(generated.error());
    }
    geometry = generated.value();
  } else {
    return Result<GuiAppliedSetup>::fail(setupRequestError(
        ErrorCategory::invalidArgument, "gui.geometrySource",
        "The setup has no supported geometry source"));
  }

  const Result<SurfaceGeometry> panel =
      candidate->generatePanel(setup.paneling);
  if (panel.isErr()) {
    return Result<GuiAppliedSetup>::fail(panel.error());
  }
  const Result<void> configured = candidate->configure(setup.configuration);
  if (configured.isErr()) {
    return Result<GuiAppliedSetup>::fail(configured.error());
  }
  const Result<void> request = validateSetupRequest(setup.request);
  if (request.isErr()) {
    return Result<GuiAppliedSetup>::fail(request.error());
  }

  session_ = std::move(candidate);
  return Result<GuiAppliedSetup>::ok(
      GuiAppliedSetup{std::move(geometry), panel.value()});
}

Result<void> GuiAnalysisWorker::resetSession() {
  session_ = std::make_unique<Session>();
  return Result<void>::ok();
}

void GuiAnalysisWorker::run(GuiAnalysisJob job) {
  const qulonglong jobId = job.jobId;
  const quint64 generationId = job.generationId;
  GuiAnalysisFinish finish = GuiAnalysisFinish::completed;
  Error outcome = Error::make(ErrorCategory::internal, "gui.analysisUnknown",
                              "The analysis did not produce an outcome",
                              "analysis");

  if (!session_) {
    outcome = workerUnavailableError();
    Q_EMIT finished(jobId, generationId, GuiAnalysisFinish::failed, outcome);
    return;
  }

  const CancellationToken* cancellation = job.cancellation.get();
  const ProgressCallback callback = job.progress;
  const ProgressCallback progress =
      [this, jobId, generationId, callback](const ProgressEvent& event) {
        Q_EMIT progressReady(jobId, generationId, event);
        if (callback) {
          callback(event);
        }
      };

  try {
    if (const auto* alpha = std::get_if<AlphaTarget>(&job.spec.target())) {
      const Result<OperatingPointData> result = session_->solveAlpha(
          alpha->alpha, cancellation, progress);
      if (result.isOk()) {
        Q_EMIT operatingPointReady(jobId, generationId, result.value());
      } else {
        outcome = result.error();
        finish = outcome.category == ErrorCategory::cancelled
                     ? GuiAnalysisFinish::cancelled
                     : GuiAnalysisFinish::failed;
      }
    } else if (const auto* lift =
                   std::get_if<LiftTarget>(&job.spec.target())) {
      const Result<OperatingPointData> result =
          session_->solveCl(lift->cl, cancellation, progress);
      if (result.isOk()) {
        Q_EMIT operatingPointReady(jobId, generationId, result.value());
      } else {
        outcome = result.error();
        finish = outcome.category == ErrorCategory::cancelled
                     ? GuiAnalysisFinish::cancelled
                     : GuiAnalysisFinish::failed;
      }
    } else if (const auto* sequence =
                   std::get_if<AlphaSequence>(&job.spec.target())) {
      const Result<DetailedSequenceResult> result =
          session_->solveDetailedAlphaSequence(*sequence, cancellation,
                                               progress);
      if (result.isOk()) {
        const DetailedSequenceResult& detailed = result.value();
        if (!detailed.points.empty()) {
          Q_EMIT sequenceReady(jobId, generationId, detailed);
        }
        if (detailed.stopReason == SequenceStopReason::cancelled) {
          outcome = Error::make(ErrorCategory::cancelled,
                                "analysis.cancelled",
                                "The analysis was cancelled", "analysis");
          finish = GuiAnalysisFinish::cancelled;
        } else if (detailed.stopReason == SequenceStopReason::failure ||
                   detailed.stopReason == SequenceStopReason::nonConvergence) {
          outcome = Error::make(ErrorCategory::numericalFailure,
                                "sequence.partialFailure",
                                "The alpha sequence stopped before completion",
                                "analysis");
          finish = GuiAnalysisFinish::failed;
        } else {
          outcome = Error{};
        }
      } else {
        outcome = result.error();
        finish = outcome.category == ErrorCategory::cancelled
                     ? GuiAnalysisFinish::cancelled
                     : GuiAnalysisFinish::failed;
      }
    } else if (const auto* sequence =
                   std::get_if<LiftSequence>(&job.spec.target())) {
      const Result<DetailedSequenceResult> result =
          session_->solveDetailedClSequence(*sequence, cancellation,
                                            progress);
      if (result.isOk()) {
        const DetailedSequenceResult& detailed = result.value();
        if (!detailed.points.empty()) {
          Q_EMIT sequenceReady(jobId, generationId, detailed);
        }
        if (detailed.stopReason == SequenceStopReason::cancelled) {
          outcome = Error::make(ErrorCategory::cancelled,
                                "analysis.cancelled",
                                "The analysis was cancelled", "analysis");
          finish = GuiAnalysisFinish::cancelled;
        } else if (detailed.stopReason == SequenceStopReason::failure ||
                   detailed.stopReason == SequenceStopReason::nonConvergence) {
          outcome = Error::make(ErrorCategory::numericalFailure,
                                "sequence.partialFailure",
                                "The CL sequence stopped before completion",
                                "analysis");
          finish = GuiAnalysisFinish::failed;
        } else {
          outcome = Error{};
        }
      } else {
        outcome = result.error();
        finish = outcome.category == ErrorCategory::cancelled
                     ? GuiAnalysisFinish::cancelled
                     : GuiAnalysisFinish::failed;
      }
    } else {
      outcome = Error::make(ErrorCategory::invalidState,
                            "gui.workerTarget",
                            "The analysis worker received no valid target",
                            "worker");
      finish = GuiAnalysisFinish::failed;
    }
  } catch (const std::exception& exception) {
    outcome = Error::make(ErrorCategory::internal, "gui.workerException",
                          exception.what(), "worker");
    finish = GuiAnalysisFinish::failed;
  } catch (...) {
    outcome = unexpectedWorkerError();
    finish = GuiAnalysisFinish::failed;
  }

  Q_EMIT finished(jobId, generationId, finish, outcome);
}

}  // namespace xfoil::gui

// End: src/gui/worker/GuiAnalysisWorker.cpp

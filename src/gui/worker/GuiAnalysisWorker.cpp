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

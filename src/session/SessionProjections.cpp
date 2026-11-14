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

// File: src/session/SessionProjections.cpp
// Pure projections from the existing session state into public owning
// value types. No solver calls, no session mutation, no file or console
// I/O, no aerodynamic recomputation.

#include "xfoil/session/SessionProjections.h"

#include <utility>
#include <vector>

namespace xfoil::session {
namespace {

Error makeError(ErrorCategory category, std::string code,
                std::string message, std::string context) {
  return Error::make(category, std::move(code), std::move(message),
                     std::move(context));
}

ConvergenceStatus convergenceFromStatus(AnalysisStatus status) noexcept {
  switch (status) {
    case AnalysisStatus::notAnalyzed:
      return ConvergenceStatus::notRun;
    case AnalysisStatus::converged:
      return ConvergenceStatus::converged;
    case AnalysisStatus::unconverged:
      return ConvergenceStatus::unconverged;
    case AnalysisStatus::cancelled:
      return ConvergenceStatus::cancelled;
    default:
      return ConvergenceStatus::failed;
  }
}

Error errorFromStatus(AnalysisStatus status) {
  switch (status) {
    case AnalysisStatus::notAnalyzed:
      return makeError(ErrorCategory::invalidState, "analysis.notAnalyzed",
                       "No analysis point has been run", "analysis");
    case AnalysisStatus::missingGeometry:
      return makeError(ErrorCategory::invalidState, "analysis.missingGeometry",
                       "No valid panel geometry is available", "analysis");
    case AnalysisStatus::missingConfiguration:
      return makeError(ErrorCategory::invalidState,
                       "analysis.missingConfiguration",
                       "No validated analysis configuration is available",
                       "analysis");
    case AnalysisStatus::invalidConfiguration:
      return makeError(ErrorCategory::invalidArgument,
                       "analysis.invalidConfiguration",
                       "The analysis configuration is invalid", "analysis");
    case AnalysisStatus::geometryTooLarge:
      return makeError(ErrorCategory::capacityExceeded,
                       "analysis.geometryTooLarge",
                       "The geometry exceeds the supported capacity",
                       "analysis");
    case AnalysisStatus::geometryInitializationFailed:
      return makeError(ErrorCategory::invalidState,
                       "analysis.geometryInitializationFailed",
                       "Geometry initialization failed", "analysis");
    case AnalysisStatus::analysisInitializationFailed:
      return makeError(ErrorCategory::invalidState,
                       "analysis.analysisInitializationFailed",
                       "Analysis initialization failed", "analysis");
    case AnalysisStatus::invalidAnalysisSettings:
      return makeError(ErrorCategory::invalidArgument,
                       "analysis.invalidAnalysisSettings",
                       "The analysis settings were rejected by the solver",
                       "analysis");
    case AnalysisStatus::compressibilityFailure:
      return makeError(ErrorCategory::numericalFailure,
                       "analysis.compressibilityFailure",
                       "The compressibility calculation failed", "analysis");
    case AnalysisStatus::finalizationFailed:
      return makeError(ErrorCategory::internal,
                       "analysis.finalizationFailed",
                       "Analysis finalization failed", "analysis");
    case AnalysisStatus::cancelled:
      return makeError(ErrorCategory::cancelled, "analysis.cancelled",
                       "The analysis was cancelled", "analysis");
    case AnalysisStatus::converged:
    case AnalysisStatus::unconverged:
      break;
  }
  return makeError(ErrorCategory::internal, "analysis.unexpectedStatus",
                   "Unexpected successful analysis status", "analysis");
}

std::vector<Diagnostic> diagnosticsFromResult(const AnalysisResult& result) {
  std::vector<Diagnostic> diagnostics;
  if (!result.diagnostic.empty()) {
    Diagnostic diagnostic;
    if (result.status == AnalysisStatus::converged) {
      diagnostic.severity = DiagnosticSeverity::info;
    } else if (result.status == AnalysisStatus::unconverged) {
      diagnostic.severity = DiagnosticSeverity::warning;
    } else {
      diagnostic.severity = DiagnosticSeverity::error;
    }
    diagnostic.code = "solver.diagnostic";
    diagnostic.message = result.diagnostic;
    diagnostics.push_back(std::move(diagnostic));
  }
  if (result.status == AnalysisStatus::unconverged) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::warning;
    diagnostic.code = "solver.nonConvergence";
    diagnostic.message = "Analysis did not converge";
    diagnostics.push_back(std::move(diagnostic));
  }
  return diagnostics;
}

}  // namespace

Result<SurfaceGeometry> projectCurrentPanel(
    const SessionComposition& session) {
  if (!session.geometry.valid) {
    return Result<SurfaceGeometry>::fail(
        makeError(ErrorCategory::invalidState, "geometry.unavailable",
                  "No valid airfoil geometry is available", "geometry"));
  }
  if (!session.paneling.panelingValid) {
    return Result<SurfaceGeometry>::fail(
        makeError(ErrorCategory::invalidState, "panel.unavailable",
                  "No valid panel contour is available", "paneling"));
  }
  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  if (panel.x.empty() || panel.x.size() != panel.y.size()) {
    return Result<SurfaceGeometry>::fail(
        makeError(ErrorCategory::invalidState, "panel.invalid",
                  "The panel contour has inconsistent dimensions",
                  "paneling"));
  }

  SurfaceGeometry geometry;
  geometry.points.reserve(panel.x.size());
  for (std::size_t index = 0; index < panel.x.size(); ++index) {
    geometry.points.push_back(Point2D{panel.x[index], panel.y[index]});
  }
  return Result<SurfaceGeometry>::ok(std::move(geometry));
}

Result<SurfaceDistribution> projectSurfaceDistribution(
    const SessionComposition& session) {
  const Result<SurfaceGeometry> panelResult = projectCurrentPanel(session);
  if (panelResult.isErr()) {
    return Result<SurfaceDistribution>::fail(panelResult.error());
  }
  if (!session.result.lastResult.successful) {
    return Result<SurfaceDistribution>::fail(
        makeError(ErrorCategory::invalidState, "surface.noAnalyzedPoint",
                  "No analyzed point is available for a surface distribution",
                  "surface"));
  }

  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  const std::size_t surfaceCount = panel.x.size();

  SurfaceDistribution distribution;
  distribution.x.reserve(surfaceCount);
  distribution.y.reserve(surfaceCount);
  distribution.cp.reserve(surfaceCount);
  for (std::size_t index = 0; index < surfaceCount; ++index) {
    distribution.x.push_back(panel.x[index]);
    distribution.y.push_back(panel.y[index]);
  }

  // The distribution follows the executed stage of the last point, not
  // the current VISC toggle.
  if (session.result.lastDiagnostics.viscalRan) {
    if (session.viscous.cpv.size() < surfaceCount) {
      return Result<SurfaceDistribution>::fail(
          makeError(ErrorCategory::invalidState,
                    "surface.viscousCpUnavailable",
                    "The viscous Cp distribution is incomplete", "surface"));
    }
    distribution.kind = SurfaceDistributionKind::viscous;
    distribution.cp.assign(session.viscous.cpv.begin(),
                           session.viscous.cpv.begin() +
                               static_cast<std::ptrdiff_t>(surfaceCount));
  } else {
    if (session.viscous.cpi.size() < surfaceCount) {
      return Result<SurfaceDistribution>::fail(
          makeError(ErrorCategory::invalidState,
                    "surface.inviscidCpUnavailable",
                    "The inviscid Cp distribution is incomplete", "surface"));
    }
    distribution.kind = SurfaceDistributionKind::inviscid;
    distribution.cp.assign(session.viscous.cpi.begin(),
                           session.viscous.cpi.begin() +
                               static_cast<std::ptrdiff_t>(surfaceCount));
  }
  return Result<SurfaceDistribution>::ok(std::move(distribution));
}

Result<OperatingPointData> projectLastOperatingPoint(
    const SessionComposition& session) {
  const AnalysisResult& result = session.result.lastResult;
  if (!result.successful) {
    return Result<OperatingPointData>::fail(errorFromStatus(result.status));
  }

  OperatingPointData point;
  point.alpha = result.alpha;
  point.liftCoefficient = result.liftCoefficient;
  point.dragCoefficient = result.dragCoefficient;
  point.momentCoefficient = result.momentCoefficient;
  point.centerOfPressureX = result.centerOfPressureX;
  point.iterations = result.iterations;
  point.viscous = session.result.lastDiagnostics.viscalRan;
  point.convergence = convergenceFromStatus(result.status);

  if (point.viscous) {
    point.dragCoefficientPressure = session.viscous.viscals.cdp;
  }

  const Result<SurfaceDistribution> surface =
      projectSurfaceDistribution(session);
  if (surface.isErr()) {
    return Result<OperatingPointData>::fail(surface.error());
  }
  point.surface = surface.value();
  point.diagnostics = diagnosticsFromResult(result);
  return Result<OperatingPointData>::ok(std::move(point));
}

Result<SequenceResult> projectActivePolar(
    const SessionComposition& session) {
  const int activeSlot = session.polar.activeSlot;
  if (activeSlot < 0 ||
      static_cast<std::size_t>(activeSlot) >= session.polar.slots.size()) {
    return Result<SequenceResult>::fail(
        makeError(ErrorCategory::invalidState, "polar.noActiveSlot",
                  "No active polar slot is available", "polar"));
  }

  const core::polar::PolarDataset& dataset =
      session.polar.slots[static_cast<std::size_t>(activeSlot)];
  SequenceResult result;
  result.points.reserve(dataset.points.size());
  for (const core::polar::PolarPoint& source : dataset.points) {
    PolarPoint point;
    point.alpha = Degrees{source.alpha};
    point.liftCoefficient = source.cl;
    point.dragCoefficient = source.cd;
    point.dragCoefficientPressure = source.cdPres;
    point.momentCoefficient = source.cm;
    point.dragCoefficientWave = source.cdWave;
    point.dragCoefficientViscous = source.cdVisc;
    point.machNumber = source.mach;
    point.reynoldsNumber = source.re;
    point.hingeMoment = source.hinge;
    point.minimumPressureCoefficient = source.cpMin;
    point.hingeDragCoefficient = source.cdh;
    point.momentCoefficientDerivative = source.cmDot;
    // Only accepted accumulation points are stored. A viscous
    // non-converged point is skipped and an inviscid point is accepted,
    // so the stored point is reported as converged.
    point.convergence = ConvergenceStatus::converged;
    result.points.push_back(std::move(point));
  }
  result.stopReason = result.points.empty() ? SequenceStopReason::notRun
                                            : SequenceStopReason::completed;
  return Result<SequenceResult>::ok(std::move(result));
}

SequenceResult projectSequence(std::span<const AnalysisResult> results,
                               SequenceStopReason stopReason,
                               std::optional<std::size_t> stoppedAtIndex) {
  SequenceResult sequence;
  sequence.stopReason = stopReason;
  sequence.stoppedAtIndex = stoppedAtIndex;
  sequence.points.reserve(results.size());
  for (const AnalysisResult& result : results) {
    PolarPoint point;
    point.alpha = result.alpha;
    point.liftCoefficient = result.liftCoefficient;
    point.dragCoefficient = result.dragCoefficient;
    point.momentCoefficient = result.momentCoefficient;
    point.convergence = convergenceFromStatus(result.status);
    point.diagnostics = diagnosticsFromResult(result);
    sequence.points.push_back(std::move(point));
  }
  if (results.empty() && sequence.stopReason == SequenceStopReason::completed) {
    sequence.stopReason = SequenceStopReason::notRun;
  }
  return sequence;
}

}  // namespace xfoil::session

// End: src/session/SessionProjections.cpp

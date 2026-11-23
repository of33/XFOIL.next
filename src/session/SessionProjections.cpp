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

#include "xfoil/session/SessionErrorMapping.h"

#include <cmath>
#include <utility>
#include <vector>

#include "xfoil/core/export_data/ExportData.h"

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

Result<SkinFrictionDistribution> projectSkinFriction(
    const SessionComposition& session) {
  const AnalysisResult& result = session.result.lastResult;
  if (!result.successful) {
    if (result.status == AnalysisStatus::cancelled ||
        result.status == AnalysisStatus::progressCallbackFailure) {
      return Result<SkinFrictionDistribution>::fail(
          makeError(ErrorCategory::invalidState,
                    "skinFriction.noValidViscousState",
                    "No valid viscous state belongs to the last point",
                    "skinFriction"));
    }
    if (result.status == AnalysisStatus::notAnalyzed ||
        result.status == AnalysisStatus::missingGeometry ||
        result.status == AnalysisStatus::missingConfiguration) {
      return Result<SkinFrictionDistribution>::fail(
          makeError(ErrorCategory::invalidState,
                    "skinFriction.noAnalyzedPoint",
                    "No analyzed point is available for skin friction",
                    "skinFriction"));
    }
    return Result<SkinFrictionDistribution>::fail(
        makeError(ErrorCategory::invalidState,
                  "skinFriction.noValidViscousState",
                  "No valid viscous state belongs to the last point",
                  "skinFriction"));
  }

  if (!session.result.lastDiagnostics.viscalRan) {
    return Result<SkinFrictionDistribution>::fail(
        makeError(ErrorCategory::invalidState, "skinFriction.notViscous",
                  "The last analyzed point is inviscid", "skinFriction"));
  }

  const auto& viscals = session.viscous.viscals;
  const auto& panel = session.paneling.panel;
  if (!session.paneling.panelingValid || !viscals.mapping.valid ||
      panel.x.empty() || panel.x.size() != panel.y.size()) {
    return Result<SkinFrictionDistribution>::fail(
        makeError(ErrorCategory::internal, "skinFriction.inconsistentState",
                  "The viscous mapping or panel geometry is inconsistent",
                  "skinFriction"));
  }

  const double qinf = session.operating.qinf;
  if (!std::isfinite(qinf) || qinf <= 0.0) {
    return Result<SkinFrictionDistribution>::fail(
        makeError(ErrorCategory::internal, "skinFriction.inconsistentState",
                  "The operating point has an invalid freestream velocity",
                  "skinFriction"));
  }

  auto projectSide = [&](const auto& mapping, const auto& arcs,
                         const auto& tau,
                         SurfaceSide side) -> Result<SkinFrictionSide> {
    if (mapping.iblte < 1 || mapping.nbl < mapping.iblte) {
      return Result<SkinFrictionSide>::fail(
          makeError(ErrorCategory::internal, "skinFriction.inconsistentState",
                    "The viscous station mapping is inconsistent",
                    "skinFriction"));
    }
    const std::size_t count = static_cast<std::size_t>(mapping.iblte);
    if (mapping.ipan.size() <= count || arcs.size() <= count ||
        tau.size() <= count) {
      return Result<SkinFrictionSide>::fail(
          makeError(ErrorCategory::internal, "skinFriction.inconsistentState",
                    "The viscous station arrays are incomplete",
                    "skinFriction"));
    }

    SkinFrictionSide projected;
    projected.side = side;
    projected.x.reserve(count);
    projected.y.reserve(count);
    projected.s.reserve(count);
    projected.cf.reserve(count);
    double previousS = 0.0;
    for (std::size_t station = 1; station <= count; ++station) {
      const int panelIndex = mapping.ipan[station];
      if (panelIndex < 0 ||
          static_cast<std::size_t>(panelIndex) >= panel.x.size()) {
        return Result<SkinFrictionSide>::fail(
            makeError(ErrorCategory::internal,
                      "skinFriction.inconsistentState",
                      "A boundary-layer station maps outside the panel",
                      "skinFriction"));
      }
      const std::size_t panelNode = static_cast<std::size_t>(panelIndex);
      const double s = arcs[station];
      const double cf = core::export_data::skinFrictionCoefficient(
          tau[station], qinf);
      if (!std::isfinite(arcs[station]) || !std::isfinite(tau[station]) ||
          !std::isfinite(panel.x[panelNode]) ||
          !std::isfinite(panel.y[panelNode]) || !std::isfinite(cf)) {
        return Result<SkinFrictionSide>::fail(
            makeError(ErrorCategory::internal,
                      "skinFriction.inconsistentState",
                      "The viscous station contains a non-finite value",
                      "skinFriction"));
      }
      if (station > 1 && !(s > previousS)) {
        return Result<SkinFrictionSide>::fail(
            makeError(ErrorCategory::internal,
                      "skinFriction.inconsistentState",
                      "The boundary-layer station coordinate is not increasing",
                      "skinFriction"));
      }
      previousS = s;
      projected.x.push_back(panel.x[panelNode]);
      projected.y.push_back(panel.y[panelNode]);
      projected.s.push_back(s);
      projected.cf.push_back(cf);
    }
    return Result<SkinFrictionSide>::ok(std::move(projected));
  };

  Result<SkinFrictionSide> upper = projectSide(
      viscals.mapping.top, viscals.arcs.top, viscals.tau.top,
      SurfaceSide::upper);
  if (upper.isErr()) {
    return Result<SkinFrictionDistribution>::fail(upper.error());
  }
  Result<SkinFrictionSide> lower = projectSide(
      viscals.mapping.bottom, viscals.arcs.bottom, viscals.tau.bottom,
      SurfaceSide::lower);
  if (lower.isErr()) {
    return Result<SkinFrictionDistribution>::fail(lower.error());
  }

  SkinFrictionDistribution distribution;
  distribution.upper = std::move(upper.value());
  distribution.lower = std::move(lower.value());
  distribution.upper.side = SurfaceSide::upper;
  distribution.lower.side = SurfaceSide::lower;
  return Result<SkinFrictionDistribution>::ok(std::move(distribution));
}

Result<OperatingPointData> projectLastOperatingPoint(
    const SessionComposition& session) {
  const AnalysisResult& result = session.result.lastResult;
  if (!result.successful) {
    return Result<OperatingPointData>::fail(
        analysisStatusError(result.status));
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
    const double cdf = session.viscous.viscals.cdf;
    if (!std::isfinite(cdf)) {
      return Result<OperatingPointData>::fail(makeError(
          ErrorCategory::internal, "analysis.inconsistentState",
          "The viscous operating point has an invalid friction-drag "
          "coefficient",
          "analysis"));
    }
    Result<SkinFrictionDistribution> cf = projectSkinFriction(session);
    if (cf.isErr()) {
      return Result<OperatingPointData>::fail(cf.error());
    }
    point.dragCoefficientPressure = session.viscous.viscals.cdp;
    point.dragCoefficientFriction = cdf;
    point.skinFriction = std::move(cf.value());
  }

  const Result<SurfaceDistribution> surface =
      projectSurfaceDistribution(session);
  if (surface.isErr()) {
    return Result<OperatingPointData>::fail(surface.error());
  }
  point.surface = surface.value();
  point.diagnostics = diagnosticsFromResult(result);

  const auto& panel = session.paneling.panel;
  const auto& gam = session.viscous.viscals.gam;
  if (!panel.x.empty() && gam.size() == panel.x.size()) {
    PressureExportData exportData;
    exportData.machNumber = session.viscous.viscals.minf;
    exportData.qinf = session.operating.qinf;
    exportData.x = panel.x;
    exportData.gam = gam;
    point.pressureExport = std::move(exportData);
  }
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

namespace {

PolarPoint makePolarPoint(const SequencePointRecord& record) {
  PolarPoint point;
  point.alpha = record.result.alpha;
  point.liftCoefficient = record.result.liftCoefficient;
  point.dragCoefficient = record.result.dragCoefficient;
  point.momentCoefficient = record.result.momentCoefficient;
  point.convergence = convergenceFromStatus(record.result.status);
  point.diagnostics = diagnosticsFromResult(record.result);
  point.dragCoefficientPressure = record.dragCoefficientPressure;
  point.dragCoefficientFriction = record.dragCoefficientFriction;
  point.dragCoefficientViscous = record.dragCoefficientViscous;
  point.iterations = record.iterations;
  return point;
}

SequenceStopReason normalizedStopReason(std::span<const SequencePointRecord> records,
                                        SequenceStopReason stopReason) {
  if (records.empty() && stopReason == SequenceStopReason::completed) {
    return SequenceStopReason::notRun;
  }
  return stopReason;
}

SequenceResult projectSequenceRecords(
    std::span<const SequencePointRecord> records,
    SequenceStopReason stopReason,
    std::optional<std::size_t> stoppedAtIndex) {
  SequenceResult sequence;
  sequence.stopReason = normalizedStopReason(records, stopReason);
  sequence.stoppedAtIndex = stoppedAtIndex;
  sequence.points.reserve(records.size());
  for (const SequencePointRecord& record : records) {
    sequence.points.push_back(makePolarPoint(record));
  }
  return sequence;
}

}  // namespace

SequenceResult projectSequence(std::span<const SequencePointRecord> records,
                               SequenceStopReason stopReason,
                               std::optional<std::size_t> stoppedAtIndex) {
  return projectSequenceRecords(records, stopReason, stoppedAtIndex);
}

SequenceResult projectSequence(std::span<const AnalysisResult> results,
                               SequenceStopReason stopReason,
                               std::optional<std::size_t> stoppedAtIndex) {
  std::vector<SequencePointRecord> records;
  records.reserve(results.size());
  for (const AnalysisResult& result : results) {
    SequencePointRecord record;
    record.result = result;
    records.push_back(std::move(record));
  }
  return projectSequenceRecords(records, stopReason, stoppedAtIndex);
}

DetailedSequenceResult projectDetailedSequence(
    std::span<const SequencePointRecord> records,
    SequenceStopReason stopReason,
    std::optional<std::size_t> stoppedAtIndex) {
  DetailedSequenceResult sequence;
  sequence.stopReason = normalizedStopReason(records, stopReason);
  sequence.stoppedAtIndex = stoppedAtIndex;
  sequence.points.reserve(records.size());
  for (const SequencePointRecord& record : records) {
    DetailedSequencePoint point;
    point.polar = makePolarPoint(record);
    if (record.operatingPoint.has_value()) {
      point.operatingPoint = record.operatingPoint;
    }
    sequence.points.push_back(std::move(point));
  }
  return sequence;
}

}  // namespace xfoil::session

// End: src/session/SessionProjections.cpp

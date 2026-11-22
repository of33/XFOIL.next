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

// File: tests/consumer/LibraryConsumer.cpp
// External consumer smoke: public headers only, memory-only inputs,
// one-shot and stateful Session analysis, no file output.

#include <iostream>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Progress.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/SkinFriction.h"
#include "xfoil/geometry/AirfoilGeometry.h"

int main() {
  std::vector<xfoil::Point2D> points = {
      {1.0, 0.0},   {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  xfoil::geometry::AirfoilGeometry geometry;
  if (!geometry.assign("consumer", points).success()) {
    return 1;
  }

  xfoil::AnalysisConfig configuration;
  configuration.reynoldsNumber = 0.0;
  configuration.machNumber = 0.0;
  configuration.viscous = false;
  configuration.maximumIterations = 20;

  xfoil::AnalysisRequest request;
  request.geometry = geometry;
  request.paneling = xfoil::PanelingOptions{};
  request.configuration = configuration;
  request.target = xfoil::AlphaTarget{xfoil::Degrees{4.0}};

  int progressEvents = 0;
  const xfoil::ProgressCallback progress =
      [&progressEvents](const xfoil::ProgressEvent&) { ++progressEvents; };

  const xfoil::Result<xfoil::OperatingPointData> oneShot =
      xfoil::analyze(request, nullptr, progress);
  if (oneShot.isErr()) {
    return 1;
  }
  const xfoil::OperatingPointData& point = oneShot.value();
  if (point.convergence != xfoil::ConvergenceStatus::converged) {
    return 1;
  }
  if (point.dragCoefficientFriction.has_value() ||
      point.skinFriction.has_value() ||
      !point.surface.has_value() ||
      point.surface->cp.size() == 0 ||
      point.surface->cp.size() != point.surface->x.size() ||
      point.surface->cp.size() != point.surface->y.size()) {
    return 1;
  }

  xfoil::Session session;
  if (session.setGeometry(geometry, progress).isErr() ||
      session.generatePanel(xfoil::PanelingOptions{}, progress).isErr() ||
      session.configure(configuration).isErr()) {
    return 1;
  }
  const xfoil::Result<xfoil::OperatingPointData> sessionPoint =
      session.solveAlpha(xfoil::Degrees{4.0}, nullptr, progress);
  if (sessionPoint.isErr() ||
      sessionPoint.value().liftCoefficient != point.liftCoefficient) {
    return 1;
  }

  if (progressEvents <= 0) {
    return 1;
  }

  xfoil::Session cfSession;
  xfoil::AnalysisConfig cfConfig;
  cfConfig.viscous = true;
  cfConfig.reynoldsNumber = 1.0e6;
  cfConfig.maximumIterations = 50;
  xfoil::PanelingOptions cfPanels;
  cfPanels.panelCount = 140;
  if (cfSession.setNaca("0012", progress).isErr() ||
      cfSession.generatePanel(cfPanels, progress).isErr() ||
      cfSession.configure(cfConfig).isErr() ||
      cfSession.solveAlpha(xfoil::Degrees{4.0}, nullptr, progress).isErr()) {
    return 1;
  }
  const xfoil::Result<xfoil::SkinFrictionDistribution> skinFriction =
      cfSession.currentSkinFriction();
  if (skinFriction.isErr()) {
    return 1;
  }
  const xfoil::SkinFrictionDistribution& cf = skinFriction.value();
  if (cf.upper.cf.empty() || cf.lower.cf.empty() ||
      cf.upper.side != xfoil::SurfaceSide::upper ||
      cf.lower.side != xfoil::SurfaceSide::lower ||
      cf.upper.x.size() != cf.upper.cf.size() ||
      cf.upper.y.size() != cf.upper.cf.size() ||
      cf.upper.s.size() != cf.upper.cf.size() ||
      cf.lower.x.size() != cf.lower.cf.size() ||
      cf.lower.y.size() != cf.lower.cf.size() ||
      cf.lower.s.size() != cf.lower.cf.size()) {
    return 1;
  }

  xfoil::AlphaSequence cfSequence;
  cfSequence.start = xfoil::Degrees{0.0};
  cfSequence.endExclusive = xfoil::Degrees{2.0};
  cfSequence.step = xfoil::Degrees{1.0};
  const xfoil::Result<xfoil::SequenceResult> sequence =
      cfSession.solveAlphaSequence(cfSequence, nullptr, progress);
  if (sequence.isErr() || sequence.value().points.empty()) {
    return 1;
  }
  for (const xfoil::PolarPoint& sequencePoint : sequence.value().points) {
    if (!sequencePoint.dragCoefficientPressure.has_value() ||
        !sequencePoint.dragCoefficientFriction.has_value() ||
        !sequencePoint.dragCoefficientViscous.has_value() ||
        !sequencePoint.iterations.has_value() ||
        *sequencePoint.dragCoefficientFriction < 0.0 ||
        *sequencePoint.dragCoefficientViscous != sequencePoint.dragCoefficient) {
      return 1;
    }
  }

  const xfoil::Result<xfoil::DetailedSequenceResult> detailed =
      cfSession.solveDetailedAlphaSequence(cfSequence, nullptr, progress);
  if (detailed.isErr() || detailed.value().points.empty()) {
    return 1;
  }
  for (const xfoil::DetailedSequencePoint& detailedPoint :
       detailed.value().points) {
    if (!detailedPoint.operatingPoint.has_value() ||
        !detailedPoint.operatingPoint->surface.has_value() ||
        !detailedPoint.operatingPoint->skinFriction.has_value() ||
        !detailedPoint.operatingPoint->dragCoefficientFriction.has_value()) {
      return 1;
    }
  }

  const xfoil::LiftSequence liftSequence{0.4, 0.8, 0.2};
  const xfoil::Result<xfoil::SequenceResult> liftCompact =
      cfSession.solveClSequence(liftSequence, nullptr, progress);
  if (liftCompact.isErr() || liftCompact.value().points.empty()) {
    return 1;
  }
  for (const xfoil::PolarPoint& point : liftCompact.value().points) {
    if (!point.dragCoefficientFriction.has_value() ||
        !point.iterations.has_value()) {
      return 1;
    }
  }
  const xfoil::Result<xfoil::DetailedSequenceResult> liftDetailed =
      cfSession.solveDetailedClSequence(liftSequence, nullptr, progress);
  if (liftDetailed.isErr() || liftDetailed.value().points.empty()) {
    return 1;
  }
  for (const xfoil::DetailedSequencePoint& point :
       liftDetailed.value().points) {
    if (!point.operatingPoint.has_value() ||
        !point.operatingPoint->surface.has_value() ||
        !point.operatingPoint->skinFriction.has_value()) {
      return 1;
    }
  }
  xfoil::CancellationToken liftToken;
  liftToken.requestCancellation();
  const xfoil::Result<xfoil::SequenceResult> liftCancelled =
      cfSession.solveClSequence(liftSequence, &liftToken);
  if (liftCancelled.isErr() ||
      liftCancelled.value().stopReason !=
          xfoil::SequenceStopReason::cancelled) {
    return 1;
  }

  std::cout << "LibraryConsumer PASS\n";
  return 0;
}

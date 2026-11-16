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
#include "xfoil/Session.h"
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
  if (!point.surface.has_value() ||
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
  std::cout << "LibraryConsumer PASS\n";
  return 0;
}

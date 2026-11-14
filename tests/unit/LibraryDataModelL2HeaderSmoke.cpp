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

// File: tests/unit/LibraryDataModelL2HeaderSmoke.cpp
// Header-only compile smoke for the L2 public data model. This
// translation unit includes only the new public headers (plus the test
// support header); it must not need an internal session, core or app
// header.

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Diagnostic.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"

TEST(unit, LibraryDataModelL2PublicHeadersCompile) {
  xfoil::Error error = xfoil::Error::make(
      xfoil::ErrorCategory::invalidArgument, "smoke.invalid", "message");
  xfoil::Result<int> ok = xfoil::Result<int>::ok(1);
  CHECK(ok.isOk());
  xfoil::Result<int> failed = xfoil::Result<int>::fail(error);
  CHECK(failed.isErr());
  CHECK_EQ(failed.error().code, std::string{"smoke.invalid"});

  xfoil::Result<void> status = xfoil::Result<void>::ok();
  CHECK(status.isOk());

  xfoil::Diagnostic diagnostic;
  diagnostic.severity = xfoil::DiagnosticSeverity::warning;
  diagnostic.code = "smoke.warning";
  diagnostic.message = "message";
  CHECK_EQ(diagnostic.code, std::string{"smoke.warning"});

  xfoil::SurfaceGeometry geometry;
  geometry.points.push_back(xfoil::Point2D{0.0, 0.0});
  geometry.points.push_back(xfoil::Point2D{1.0, 0.0});
  CHECK_EQ(geometry.points.size(), std::size_t{2});

  xfoil::SurfaceDistribution distribution;
  distribution.kind = xfoil::SurfaceDistributionKind::viscous;
  distribution.x = {0.0, 1.0};
  distribution.y = {0.0, 0.0};
  distribution.cp = {1.0, -1.0};
  CHECK_EQ(distribution.cp.size(), std::size_t{2});

  xfoil::OperatingPointData point;
  point.alpha = xfoil::Degrees{2.0};
  point.liftCoefficient = 0.5;
  point.convergence = xfoil::ConvergenceStatus::converged;
  point.surface = distribution;
  CHECK(point.surface.has_value());

  xfoil::PolarPoint polarPoint;
  polarPoint.alpha = xfoil::Degrees{2.0};
  polarPoint.liftCoefficient = 0.5;
  polarPoint.convergence = xfoil::ConvergenceStatus::converged;
  xfoil::SequenceResult sequence;
  sequence.points.push_back(polarPoint);
  sequence.stopReason = xfoil::SequenceStopReason::completed;
  CHECK_EQ(sequence.points.size(), std::size_t{1});
}

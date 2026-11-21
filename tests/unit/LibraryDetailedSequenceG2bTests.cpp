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

// File: tests/unit/LibraryDetailedSequenceG2bTests.cpp
// Public detailed-sequence value, ownership and projection contracts.

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/session/SequencePointRecord.h"
#include "xfoil/session/SessionProjections.h"

namespace {

using xfoil::AnalysisResult;
using xfoil::AnalysisStatus;
using xfoil::DetailedSequencePoint;
using xfoil::DetailedSequenceResult;
using xfoil::Diagnostic;
using xfoil::ErrorCategory;
using xfoil::OperatingPointData;
using xfoil::PolarPoint;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::SkinFrictionDistribution;
using xfoil::SurfaceDistribution;
using xfoil::SurfaceDistributionKind;
using xfoil::SurfaceSide;
using xfoil::session::SequencePointRecord;

OperatingPointData sampleOperatingPoint() {
  OperatingPointData point;
  point.alpha = xfoil::Degrees{2.0};
  point.liftCoefficient = 0.42;
  point.dragCoefficient = 0.012;
  point.momentCoefficient = -0.08;
  point.iterations = 7;
  point.viscous = true;
  point.convergence = xfoil::ConvergenceStatus::converged;
  SurfaceDistribution surface;
  surface.kind = SurfaceDistributionKind::viscous;
  surface.x = {0.0, 0.5, 1.0};
  surface.y = {0.0, 0.05, 0.0};
  surface.cp = {-0.4, -0.2, 0.1};
  point.surface = std::move(surface);
  SkinFrictionDistribution cf;
  cf.upper.side = SurfaceSide::upper;
  cf.upper.x = {0.0, 1.0};
  cf.upper.y = {0.0, 0.0};
  cf.upper.s = {0.0, 1.0};
  cf.upper.cf = {0.004, 0.003};
  cf.lower.side = SurfaceSide::lower;
  cf.lower.x = {0.0, 1.0};
  cf.lower.y = {0.0, 0.0};
  cf.lower.s = {0.0, 1.0};
  cf.lower.cf = {0.003, 0.002};
  point.dragCoefficientFriction = 0.0035;
  point.skinFriction = std::move(cf);
  return point;
}

SequencePointRecord successfulRecord(bool withDetail) {
  SequencePointRecord record;
  record.result.status = AnalysisStatus::converged;
  record.result.successful = true;
  record.result.converged = true;
  record.result.alpha = xfoil::Degrees{2.0};
  record.result.liftCoefficient = 0.42;
  record.result.dragCoefficient = 0.012;
  record.result.momentCoefficient = -0.08;
  record.result.iterations = 7;
  record.dragCoefficientPressure = 0.004;
  record.dragCoefficientFriction = 0.0035;
  record.dragCoefficientViscous = 0.012;
  record.iterations = std::size_t{7};
  if (withDetail) {
    record.operatingPoint = sampleOperatingPoint();
  }
  return record;
}

}  // namespace

TEST(unit, LibraryDetailedSequenceG2bDefaultsAndOwnership) {
  DetailedSequenceResult sequence;
  CHECK(sequence.points.empty());
  CHECK(sequence.stopReason == SequenceStopReason::notRun);
  CHECK(!sequence.stoppedAtIndex.has_value());
  CHECK(sequence.diagnostics.empty());

  DetailedSequencePoint point;
  CHECK(!point.operatingPoint.has_value());
  CHECK(point.polar.alpha.value == 0.0);

  point.operatingPoint = sampleOperatingPoint();
  const DetailedSequencePoint copy = point;
  CHECK(copy.operatingPoint.has_value());
  CHECK(copy.operatingPoint->surface.has_value());
  CHECK(copy.operatingPoint->skinFriction.has_value());

  point.operatingPoint->surface->cp[0] = 99.0;
  point.operatingPoint->skinFriction->upper.cf[0] = 99.0;
  CHECK_NEAR(copy.operatingPoint->surface->cp[0], -0.4, 0.0, 0.0);
  CHECK_NEAR(copy.operatingPoint->skinFriction->upper.cf[0], 0.004, 0.0,
             0.0);

  DetailedSequencePoint moved = std::move(copy);
  CHECK(moved.operatingPoint.has_value());
  CHECK(moved.operatingPoint->surface.has_value());
}

TEST(unit, LibraryDetailedSequenceG2bProjectionUsesSameScalars) {
  const std::vector<SequencePointRecord> records{
      successfulRecord(true)};
  const SequenceResult compact = xfoil::session::projectSequence(
      records, SequenceStopReason::completed, std::nullopt);
  const DetailedSequenceResult detailed =
      xfoil::session::projectDetailedSequence(
          records, SequenceStopReason::completed, std::nullopt);

  CHECK_EQ(compact.points.size(), std::size_t{1});
  CHECK_EQ(detailed.points.size(), std::size_t{1});
  const PolarPoint& left = compact.points[0];
  const PolarPoint& right = detailed.points[0].polar;
  CHECK_NEAR(right.alpha.value, left.alpha.value, 0.0, 0.0);
  CHECK_NEAR(right.liftCoefficient, left.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(right.dragCoefficient, left.dragCoefficient, 0.0, 0.0);
  CHECK_NEAR(right.momentCoefficient, left.momentCoefficient, 0.0, 0.0);
  CHECK_NEAR(right.dragCoefficientPressure.value(),
             left.dragCoefficientPressure.value(), 0.0, 0.0);
  CHECK_NEAR(right.dragCoefficientFriction.value(),
             left.dragCoefficientFriction.value(), 0.0, 0.0);
  CHECK_NEAR(right.dragCoefficientViscous.value(),
             left.dragCoefficientViscous.value(), 0.0, 0.0);
  CHECK_EQ(right.iterations.value(), left.iterations.value());
  CHECK(detailed.points[0].operatingPoint.has_value());
}

TEST(unit, LibraryDetailedSequenceG2bCancelledAndFailedHaveNoStaleDetail) {
  SequencePointRecord cancelled = successfulRecord(true);
  cancelled.result.status = AnalysisStatus::cancelled;
  cancelled.result.successful = false;
  cancelled.result.converged = false;
  cancelled.operatingPoint.reset();

  SequencePointRecord failed = successfulRecord(true);
  failed.result.status = AnalysisStatus::finalizationFailed;
  failed.result.successful = false;
  failed.result.converged = false;
  failed.operatingPoint.reset();

  const std::vector<SequencePointRecord> records{successfulRecord(true),
                                                 cancelled, failed};
  const DetailedSequenceResult detailed =
      xfoil::session::projectDetailedSequence(
          records, SequenceStopReason::failure, std::size_t{2});
  CHECK_EQ(detailed.points.size(), std::size_t{3});
  CHECK(detailed.points[0].operatingPoint.has_value());
  CHECK(!detailed.points[1].operatingPoint.has_value());
  CHECK(!detailed.points[2].operatingPoint.has_value());
  CHECK(detailed.points[1].polar.convergence ==
        xfoil::ConvergenceStatus::cancelled);
  CHECK(detailed.points[2].polar.convergence ==
        xfoil::ConvergenceStatus::failed);
  CHECK(detailed.stopReason == SequenceStopReason::failure);
  CHECK_EQ(detailed.stoppedAtIndex.value(), std::size_t{2});
}

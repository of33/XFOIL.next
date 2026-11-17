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

// File: tests/unit/LibrarySequencePointCompletenessTests.cpp
// Public sequence-point scalar completeness and projection contracts.

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/session/SequencePointRecord.h"
#include "xfoil/session/SessionProjections.h"
#include "xfoil/session/SessionState.h"

namespace {

using xfoil::AnalysisResult;
using xfoil::AnalysisStatus;
using xfoil::PolarPoint;
using xfoil::SequenceResult;
using xfoil::SequenceStopReason;
using xfoil::session::SequencePointRecord;

SequencePointRecord successfulRecord(double alpha, double cl, double cd) {
  SequencePointRecord record;
  record.result.status = AnalysisStatus::converged;
  record.result.successful = true;
  record.result.converged = true;
  record.result.alpha = xfoil::Degrees{alpha};
  record.result.liftCoefficient = cl;
  record.result.dragCoefficient = cd;
  record.result.momentCoefficient = -0.02;
  record.result.iterations = 6;
  record.dragCoefficientPressure = 0.002;
  record.dragCoefficientFriction = 0.004;
  record.dragCoefficientViscous = cd;
  record.iterations = std::size_t{6};
  return record;
}

template <class T>
concept HasSurfaceMember = requires(T value) { value.surface; };

template <class T>
concept HasSkinFrictionMember = requires(T value) { value.skinFriction; };

static_assert(!HasSurfaceMember<PolarPoint>);
static_assert(!HasSkinFrictionMember<PolarPoint>);

}  // namespace

TEST(unit, LibrarySequencePointCompletenessDefaultsAndOwnership) {
  PolarPoint point;
  CHECK(!point.dragCoefficientPressure.has_value());
  CHECK(!point.dragCoefficientFriction.has_value());
  CHECK(!point.dragCoefficientViscous.has_value());
  CHECK(!point.iterations.has_value());

  point.dragCoefficientPressure = 0.002;
  point.dragCoefficientFriction = 0.004;
  point.iterations = std::size_t{9};

  PolarPoint copy = point;
  CHECK_NEAR(copy.dragCoefficientFriction.value(), 0.004, 0.0, 0.0);
  CHECK_EQ(copy.iterations.value(), std::size_t{9});
  point.dragCoefficientFriction = 0.02;
  CHECK_NEAR(copy.dragCoefficientFriction.value(), 0.004, 0.0, 0.0);

  PolarPoint moved = std::move(copy);
  CHECK(moved.dragCoefficientFriction.has_value());
  CHECK_EQ(moved.iterations.value(), std::size_t{9});
}

TEST(unit, LibrarySequencePointCompletenessProjectionCarriesScalars) {
  std::vector<SequencePointRecord> records;
  records.push_back(successfulRecord(2.0, 0.5, 0.02));

  const SequenceResult sequence = xfoil::session::projectSequence(
      records, SequenceStopReason::completed, std::nullopt);
  CHECK_EQ(sequence.points.size(), std::size_t{1});
  const PolarPoint& point = sequence.points[0];
  CHECK_NEAR(point.alpha.value, 2.0, 0.0, 0.0);
  CHECK_NEAR(point.liftCoefficient, 0.5, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficient, 0.02, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficientPressure.value(), 0.002, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficientFriction.value(), 0.004, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficientViscous.value(), 0.02, 0.0, 0.0);
  CHECK_EQ(point.iterations.value(), std::size_t{6});
  CHECK(point.convergence == xfoil::ConvergenceStatus::converged);
}

TEST(unit, LibrarySequencePointCompletenessRetainsCdfNotDifference) {
  SequencePointRecord record;
  record.result.status = AnalysisStatus::converged;
  record.result.successful = true;
  record.result.converged = true;
  record.result.dragCoefficient = 0.020;
  record.dragCoefficientPressure = 0.005;
  record.dragCoefficientFriction = 0.004;
  record.dragCoefficientViscous = 0.123456;
  record.iterations = std::size_t{7};

  const std::vector<SequencePointRecord> records{record};
  const SequenceResult sequence =
      xfoil::session::projectSequence(records);
  const PolarPoint& point = sequence.points[0];
  CHECK_NEAR(point.dragCoefficientFriction.value(), 0.004, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficientViscous.value(), 0.123456, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficient, 0.020, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficientPressure.value(), 0.005, 0.0, 0.0);
  CHECK(std::fabs(point.dragCoefficientFriction.value() - 0.015) > 1.0e-12);
}

TEST(unit, LibrarySequencePointCompletenessFailedAndCancelledHaveNoStaleScalars) {
  SequencePointRecord failed = successfulRecord(1.0, 0.2, 0.01);
  failed.result.status = AnalysisStatus::finalizationFailed;
  failed.result.successful = false;
  failed.dragCoefficientPressure.reset();
  failed.dragCoefficientFriction.reset();
  failed.dragCoefficientViscous.reset();
  failed.iterations.reset();

  SequencePointRecord cancelled = failed;
  cancelled.result.status = AnalysisStatus::cancelled;

  const std::vector<SequencePointRecord> records{failed, cancelled};
  const SequenceResult sequence = xfoil::session::projectSequence(
      records, SequenceStopReason::failure, std::size_t{0});
  CHECK_EQ(sequence.points.size(), std::size_t{2});
  for (const PolarPoint& point : sequence.points) {
    CHECK(!point.dragCoefficientPressure.has_value());
    CHECK(!point.dragCoefficientFriction.has_value());
    CHECK(!point.dragCoefficientViscous.has_value());
    CHECK(!point.iterations.has_value());
  }
}

TEST(unit, LibrarySequencePointCompletenessLegacyProjectionLeavesScalarsEmpty) {
  std::vector<AnalysisResult> results(1);
  results[0].status = AnalysisStatus::converged;
  results[0].successful = true;
  results[0].converged = true;
  results[0].alpha = xfoil::Degrees{1.0};
  results[0].dragCoefficient = 0.01;
  results[0].iterations = 4;

  const SequenceResult sequence = xfoil::session::projectSequence(results);
  CHECK(sequence.points[0].dragCoefficientPressure == std::nullopt);
  CHECK(sequence.points[0].dragCoefficientFriction == std::nullopt);
  CHECK(sequence.points[0].dragCoefficientViscous == std::nullopt);
  CHECK(sequence.points[0].iterations == std::nullopt);
}

TEST(unit, LibrarySequencePointCompletenessActivePolarStoredLimits) {
  xfoil::session::SessionComposition session;
  session.polar.activeSlot = 0;
  session.polar.slots.resize(1);
  xfoil::core::polar::PolarPoint source;
  source.alpha = 2.0;
  source.cd = 0.02;
  source.cdPres = 0.003;
  source.cdVisc = 0.02;
  session.polar.slots[0].points.push_back(source);

  const xfoil::Result<SequenceResult> projected =
      xfoil::session::projectActivePolar(session);
  CHECK(projected.isOk());
  const PolarPoint& point = projected.value().points[0];
  CHECK_NEAR(point.dragCoefficientPressure.value(), 0.003, 0.0, 0.0);
  CHECK_NEAR(point.dragCoefficientViscous.value(), 0.02, 0.0, 0.0);
  CHECK(!point.dragCoefficientFriction.has_value());
  CHECK(!point.iterations.has_value());
}

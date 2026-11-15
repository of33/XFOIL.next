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

// File: tests/unit/LibraryIsolationL7Tests.cpp
// L7 logical session isolation and host-process error continuation.

#include <cmath>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/Types.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::ConvergenceStatus;
using xfoil::Degrees;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::SequenceStopReason;
using xfoil::Session;

AnalysisConfig inviscidConfig(double reynolds = 1.0e6,
                              double mach = 0.0) {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = reynolds;
  config.machNumber = mach;
  config.maximumIterations = 60;
  return config;
}

AnalysisConfig viscousConfig(double reynolds = 2.0e6,
                             double mach = 0.1) {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = reynolds;
  config.machNumber = mach;
  config.maximumIterations = 100;
  return config;
}

AlphaSequence sequence(double start, double endExclusive, double step) {
  AlphaSequence result;
  result.start = Degrees{start};
  result.endExclusive = Degrees{endExclusive};
  result.step = Degrees{step};
  return result;
}

PanelingOptions paneling(std::size_t count) {
  PanelingOptions options;
  options.panelCount = count;
  return options;
}

bool sameFloat(double left, double right) {
  return left == right;
}

bool samePoints(const std::vector<xfoil::Point2D>& left,
                const std::vector<xfoil::Point2D>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].x != right[index].x ||
        left[index].y != right[index].y) {
      return false;
    }
  }
  return true;
}

bool samePoint(const OperatingPointData& left,
               const OperatingPointData& right) {
  if (!sameFloat(left.liftCoefficient, right.liftCoefficient) ||
      !sameFloat(left.dragCoefficient, right.dragCoefficient) ||
      !sameFloat(left.momentCoefficient, right.momentCoefficient) ||
      left.convergence != right.convergence ||
      left.surface.has_value() != right.surface.has_value()) {
    return false;
  }
  if (!left.surface.has_value()) {
    return true;
  }
  return left.surface->x == right.surface->x &&
         left.surface->y == right.surface->y &&
         left.surface->cp == right.surface->cp;
}

struct PairSetup {
  Session low;
  Session high;
};

bool prepare(Session& session, const char* naca, std::size_t count,
             const AnalysisConfig& config) {
  return session.setNaca(naca).isOk() &&
         session.generatePanel(paneling(count)).isOk() &&
         session.configure(config).isOk();
}

}  // namespace

TEST(unit, LibraryIsolationL7OwnedResultsAndReset) {
  Session low;
  Session high;
  CHECK(prepare(low, "0012", 160, inviscidConfig()));
  CHECK(prepare(high, "2412", 220, viscousConfig()));

  const auto lowPoint = low.solveAlpha(Degrees{2.0});
  const auto highPoint = high.solveAlpha(Degrees{1.0});
  CHECK(lowPoint.isOk());
  CHECK(highPoint.isOk());
  CHECK(lowPoint.value().convergence == ConvergenceStatus::converged);
  CHECK(highPoint.value().convergence == ConvergenceStatus::converged);
  CHECK(lowPoint.value().surface.has_value());
  CHECK(highPoint.value().surface.has_value());
  CHECK_NE(lowPoint.value().liftCoefficient,
           highPoint.value().liftCoefficient);
  CHECK_NE(lowPoint.value().surface->cp.data(),
           highPoint.value().surface->cp.data());
  CHECK_NE(low.currentPanel().value().points.size(),
           high.currentPanel().value().points.size());

  const OperatingPointData highBefore = highPoint.value();
  const auto highPanelBefore = high.currentPanel().value();
  low.reset();
  CHECK(low.currentPanel().isErr());
  CHECK(low.lastOperatingPoint().isErr());
  CHECK(high.lastOperatingPoint().isOk());
  CHECK(samePoint(high.lastOperatingPoint().value(), highBefore));
  CHECK(samePoints(high.currentPanel().value().points,
                   highPanelBefore.points));

  const auto highAgain = high.solveAlpha(Degrees{1.0});
  CHECK(highAgain.isOk());
  CHECK(samePoint(highAgain.value(), highBefore));
}

TEST(unit, LibraryIsolationL7ErrorsCancellationAndDestruction) {
  Session source;
  Session observer;
  CHECK(prepare(source, "0012", 160, inviscidConfig()));
  CHECK(prepare(observer, "2412", 200, inviscidConfig()));
  const auto baseline = observer.solveAlpha(Degrees{2.0});
  CHECK(baseline.isOk());

  Session errors;
  CHECK(errors.solveAlpha(Degrees{1.0}).isErr());
  CHECK(errors.currentPanel().isErr());
  CHECK(errors.setNaca("0012").isOk());
  CHECK(errors.generatePanel(paneling(1)).isErr());
  AnalysisConfig invalid = inviscidConfig();
  invalid.reynoldsNumber = -1.0;
  CHECK(errors.configure(invalid).isErr());
  CHECK(errors.configure(inviscidConfig()).isOk());
  CHECK(errors.solveAlpha(Degrees{std::nan("")}).isErr());
  CHECK(errors.solveCl(std::nan("")).isErr());
  CHECK(errors.solveAlphaSequence(sequence(0.0, 2.0, 0.0)).isErr());
  CHECK(errors.solveAlphaSequence(sequence(2.0, 0.0, 1.0)).isErr());
  CHECK(observer.solveAlpha(Degrees{2.0}).isOk());
  CHECK(samePoint(observer.lastOperatingPoint().value(),
                  baseline.value()));

  CancellationToken token;
  token.requestCancellation();
  const auto cancelled =
      source.solveAlphaSequence(sequence(0.0, 3.0, 1.0), &token);
  CHECK(cancelled.isOk());
  CHECK(cancelled.value().stopReason == SequenceStopReason::cancelled);
  CHECK(observer.solveAlpha(Degrees{2.0}).isOk());
  CHECK(samePoint(observer.lastOperatingPoint().value(),
                  baseline.value()));

  {
    Session temporary;
    CHECK(prepare(temporary, "0012", 180, inviscidConfig()));
    CHECK(temporary.solveAlpha(Degrees{1.0}).isOk());
  }
  CHECK(observer.solveAlpha(Degrees{2.0}).isOk());
  CHECK(samePoint(observer.lastOperatingPoint().value(),
                  baseline.value()));
}

TEST(unit, LibraryIsolationL7RepeatedSessionsNoStaticState) {
  Session referenceLow;
  Session referenceHigh;
  CHECK(prepare(referenceLow, "0012", 160, inviscidConfig()));
  CHECK(prepare(referenceHigh, "2412", 220, viscousConfig()));
  const auto expectedLow = referenceLow.solveAlpha(Degrees{2.0});
  const auto expectedHigh = referenceHigh.solveAlpha(Degrees{1.0});
  CHECK(expectedLow.isOk());
  CHECK(expectedHigh.isOk());

  for (int index = 0; index < 6; ++index) {
    Session session;
    const bool low = (index % 2) == 0;
    CHECK(prepare(session, low ? "0012" : "2412", low ? 160 : 220,
                  low ? inviscidConfig() : viscousConfig()));
    const auto point = session.solveAlpha(
        Degrees{low ? 2.0 : 1.0});
    CHECK(point.isOk());
    CHECK(samePoint(point.value(),
                    (low ? expectedLow : expectedHigh).value()));
  }
}

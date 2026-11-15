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

// File: tests/integration/LibraryIsolationL7Tests.cpp
// L7 end-to-end session isolation with independent geometry, paneling,
// configuration, sequences, cancellation and destruction.

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
using xfoil::PanelingOptions;
using xfoil::SequenceStopReason;
using xfoil::Session;

AnalysisConfig inviscid() {
  AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 1.0e6;
  config.machNumber = 0.0;
  config.maximumIterations = 70;
  return config;
}

AnalysisConfig viscous() {
  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 2.0e6;
  config.machNumber = 0.1;
  config.maximumIterations = 100;
  return config;
}

PanelingOptions paneling(std::size_t count) {
  PanelingOptions options;
  options.panelCount = count;
  return options;
}

AlphaSequence sequence(double start, double endExclusive, double step) {
  AlphaSequence result;
  result.start = Degrees{start};
  result.endExclusive = Degrees{endExclusive};
  result.step = Degrees{step};
  return result;
}

bool samePoint(const xfoil::OperatingPointData& left,
               const xfoil::OperatingPointData& right) {
  return left.liftCoefficient == right.liftCoefficient &&
         left.dragCoefficient == right.dragCoefficient &&
         left.momentCoefficient == right.momentCoefficient &&
         left.surface.has_value() == right.surface.has_value() &&
         (!left.surface.has_value() ||
          (left.surface->x == right.surface->x &&
           left.surface->y == right.surface->y &&
           left.surface->cp == right.surface->cp));
}

}  // namespace

TEST(integration, LibraryIsolationL7IndependentWorkflows) {
  Session lower;
  Session upper;
  CHECK(lower.setNaca("0012").isOk());
  CHECK(upper.setNaca("2412").isOk());
  CHECK(lower.generatePanel(paneling(180)).isOk());
  CHECK(upper.generatePanel(paneling(240)).isOk());
  CHECK(lower.configure(inviscid()).isOk());
  CHECK(upper.configure(viscous()).isOk());

  const auto lowerPoint = lower.solveAlpha(Degrees{1.5});
  const auto upperPoint = upper.solveAlpha(Degrees{1.0});
  CHECK(lowerPoint.isOk());
  CHECK(upperPoint.isOk());
  CHECK(lowerPoint.value().surface.has_value());
  CHECK(upperPoint.value().surface.has_value());
  CHECK_NE(lowerPoint.value().liftCoefficient,
           upperPoint.value().liftCoefficient);
  CHECK(lowerPoint.value().surface->x != upperPoint.value().surface->x);

  const auto lowerSequence =
      lower.solveAlphaSequence(sequence(-1.0, 2.0, 1.0));
  const auto upperSequence =
      upper.solveAlphaSequence(sequence(-0.5, 1.5, 1.0));
  CHECK(lowerSequence.isOk());
  CHECK(upperSequence.isOk());
  CHECK_EQ(lowerSequence.value().points.size(), std::size_t{3});
  CHECK_EQ(upperSequence.value().points.size(), std::size_t{2});
  CHECK(lowerSequence.value().stopReason == SequenceStopReason::completed);
  CHECK(upperSequence.value().stopReason == SequenceStopReason::completed);

  const auto upperLast = upper.lastOperatingPoint().value();
  lower.reset();
  CHECK(lower.currentPanel().isErr());
  CHECK(upper.currentPanel().isOk());
  CHECK(samePoint(upper.lastOperatingPoint().value(), upperLast));
}

TEST(integration, LibraryIsolationL7FailureAndCancellationIsolation) {
  Session victim;
  Session witness;
  CHECK(victim.setNaca("0012").isOk());
  CHECK(witness.setNaca("2412").isOk());
  CHECK(victim.generatePanel(paneling(160)).isOk());
  CHECK(witness.generatePanel(paneling(220)).isOk());
  CHECK(victim.configure(inviscid()).isOk());
  CHECK(witness.configure(viscous()).isOk());
  const auto baseline = witness.solveAlpha(Degrees{1.0});
  CHECK(baseline.isOk());

  CHECK(victim.solveAlpha(Degrees{std::nan("")}).isErr());
  CHECK(victim.solveCl(std::nan("")).isErr());
  CHECK(victim.solveAlphaSequence(sequence(0.0, 2.0, 0.0)).isErr());

  CancellationToken token;
  token.requestCancellation();
  const auto cancelled =
      victim.solveAlphaSequence(sequence(0.0, 4.0, 1.0), &token);
  CHECK(cancelled.isOk());
  CHECK(cancelled.value().stopReason == SequenceStopReason::cancelled);
  CHECK(witness.solveAlpha(Degrees{1.0}).isOk());
  CHECK(samePoint(witness.lastOperatingPoint().value(),
                  baseline.value()));

  {
    Session destroyed;
    CHECK(destroyed.setNaca("4412").isOk());
    CHECK(destroyed.generatePanel(paneling(140)).isOk());
    CHECK(destroyed.configure(inviscid()).isOk());
    CHECK(destroyed.solveAlpha(Degrees{0.5}).isOk());
  }
  CHECK(witness.solveAlpha(Degrees{1.0}).isOk());
  CHECK(samePoint(witness.lastOperatingPoint().value(),
                  baseline.value()));
}

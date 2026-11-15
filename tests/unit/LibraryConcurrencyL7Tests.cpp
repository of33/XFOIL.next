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

// File: tests/unit/LibraryConcurrencyL7Tests.cpp
// Diagnostic parallel execution of independent public sessions. This is
// not a supported concurrency contract; the test exists to detect shared
// mutable state and to exercise the implementation under TSan.

#include <atomic>
#include <thread>
#include <vector>

#include "TestSupport.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/Types.h"

namespace {

using xfoil::AnalysisConfig;
using xfoil::AnalysisRequest;
using xfoil::Degrees;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::Session;

struct Case {
  const char* naca;
  std::size_t panels;
  bool viscous;
  double alpha;
};

const Case cases[] = {
    {"0012", 160, false, 2.0},
    {"2412", 220, true, 1.0},
    {"4412", 180, false, 1.5},
    {"0012", 240, true, 0.5},
};

AnalysisConfig configFor(const Case& item) {
  AnalysisConfig config;
  config.viscous = item.viscous;
  config.reynoldsNumber = item.viscous ? 2.0e6 : 1.0e6;
  config.machNumber = item.viscous ? 0.1 : 0.0;
  config.maximumIterations = 100;
  return config;
}

PanelingOptions panelingFor(const Case& item) {
  PanelingOptions options;
  options.panelCount = item.panels;
  return options;
}

bool samePoint(const OperatingPointData& left,
               const OperatingPointData& right);

OperatingPointData runCase(const Case& item) {
  Session session;
  const auto geometry = session.setNaca(item.naca);
  if (geometry.isErr() || session.generatePanel(panelingFor(item)).isErr() ||
      session.configure(configFor(item)).isErr()) {
    return {};
  }
  const auto point = session.solveAlpha(Degrees{item.alpha});
  if (point.isErr()) {
    return {};
  }

  AnalysisRequest request;
  request.geometry = geometry.value();
  request.paneling = panelingFor(item);
  request.configuration = configFor(item);
  request.target = xfoil::AlphaTarget{Degrees{item.alpha}};
  const auto oneShot = xfoil::analyze(request);
  if (oneShot.isErr() ||
      !samePoint(oneShot.value(), point.value())) {
    return {};
  }
  return point.value();
}

bool samePoint(const OperatingPointData& left,
               const OperatingPointData& right) {
  if (left.convergence != right.convergence ||
      left.liftCoefficient != right.liftCoefficient ||
      left.dragCoefficient != right.dragCoefficient ||
      left.momentCoefficient != right.momentCoefficient ||
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

}  // namespace

TEST(unit, LibraryConcurrencyL7IndependentSessions) {
  constexpr std::size_t caseCount =
      sizeof(cases) / sizeof(cases[0]);
  std::vector<OperatingPointData> baseline;
  for (const Case& item : cases) {
    baseline.push_back(runCase(item));
  }

  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (std::size_t threadIndex = 0; threadIndex < caseCount * 2;
       ++threadIndex) {
    const Case item = cases[threadIndex % caseCount];
    const OperatingPointData expected = baseline[threadIndex % caseCount];
    threads.emplace_back([&failures, item, expected]() {
      for (int repeat = 0; repeat < 2; ++repeat) {
        const OperatingPointData actual = runCase(item);
        if (!samePoint(actual, expected)) {
          failures.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  CHECK_EQ(failures.load(std::memory_order_relaxed), 0);
}

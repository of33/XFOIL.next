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

// File: tests/core/XcpDerivationTests.cpp
// XCP derivation regression: the XCP diagnostic quantity in the
// confirmed kernel clCalc (LoadResult.centerOfPressureX) is bit-identical
// to the public API path
// xfoil::inviscid::integratePressureLoads on 1-based spans and uses only
// terms already computed in the CLCALC loop (dx, AG, X).
//
// Provenance table:
// XCP source | current symbol | layer | index base
// Canonical CLCALC | xfoil.f CLCALC (1088-1172) | Fortran | 1-based, no XCP
// Public API path | xfoil::inviscid::integratePressureLoads | inviscid API | 1-based, index 0 reserved
// Session path | SessionOps centerOfPressureDiagnostic | session | 1-based
// Core path | core::inviscid::clCalc -> LoadResult | core | 0-based
// Each row names the layer that owns the XCP convention.
//
// Fortran/API evidence: Fortran CLCALC sets no XCP (xfoil.f:1088-1172);
// XCP semantics are part of the
// public API contract (AnalysisResult.centerOfPressureX). The
// loop term xcp += dx*AG*(X(IP)+X(I))/2 (/Cl, Cl==0 -> 0.0) uses only
// terms that are already computed and leaves the remaining CLCALC outputs
// unchanged (bit-identical, see below).

#include "TestSupport.h"

#include "xfoil/inviscid/InviscidAnalysis.h"
#include "xfoil/core/inviscid/Loads.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace {

// Synthetic contour + GAM (identical physical inputs for both paths).
struct SyntheticCase {
  std::vector<double> x0;
  std::vector<double> y0;
  std::vector<double> gam0;
  std::vector<double> gamA0;
  double alfa;
  double minf;
  double qinf;
  double xref;
  double yref;
};

SyntheticCase makeCase() {
  return {{0.0, 0.2, 0.5, 0.8, 1.0},
          {0.0, 0.08, 0.1, 0.05, 0.0},
          {0.3, 0.5, 0.6, 0.45, 0.2},
          {0.1, 0.15, 0.2, 0.14, 0.08},
          0.07,
          0.0,
          1.0,
          0.25,
          0.0};
}

}  // namespace

// the clCalc XCP is bit-identical to the public API path
// (integratePressureLoads) for identical physical inputs; Cl and CM remain
// unchanged.
TEST(unit, XcpCoreDerivationMatchesPreviousApiPath) {
  const SyntheticCase c = makeCase();
  const std::size_t n = c.x0.size();

  // path A: public API path (integratePressureLoads, 1-based).
  std::vector<double> x1(n + 1, 0.0);
  std::vector<double> y1(n + 1, 0.0);
  std::vector<double> g1(n + 1, 0.0);
  std::vector<double> ga1(n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    x1[i + 1] = c.x0[i];
    y1[i + 1] = c.y0[i];
    g1[i + 1] = c.gam0[i];
    ga1[i + 1] = c.gamA0[i];
  }
  const auto previousApi = xfoil::inviscid::integratePressureLoads(
      x1, y1, g1, ga1, static_cast<int>(n), c.alfa, c.qinf, c.minf, c.xref,
      c.yref);
  CHECK(previousApi.status == xfoil::inviscid::InviscidStatus::success);

 // path B: confirmed core path (0-based, LoadResult.centerOfPressureX).
  const auto core = xfoil::core::inviscid::clCalc(
      c.x0, c.y0, c.gam0, c.gamA0, c.alfa, c.minf, c.qinf, c.xref, c.yref);

  // bit-identical XCP and unchanged Cl/CM outputs.
  CHECK_EQ(core.centerOfPressureX, previousApi.centerOfPressureX);
  CHECK_EQ(core.cl, previousApi.liftCoefficient);
  CHECK_EQ(core.cm, previousApi.momentCoefficient);
  CHECK_EQ(core.cdp, previousApi.pressureDragCoefficient);
}

// the XCP loop term uses only quantities that are already
// computed (dx, AG, X) and leaves the remaining CLCALC outputs unchanged:
// a second clCalc call returns identical values (no state accumulates
// across calls).
TEST(unit, XcpCoreDerivationIsStatelessAndAdditive) {
  const SyntheticCase c = makeCase();
  const auto first = xfoil::core::inviscid::clCalc(
      c.x0, c.y0, c.gam0, c.gamA0, c.alfa, c.minf, c.qinf, c.xref, c.yref);
  const auto second = xfoil::core::inviscid::clCalc(
      c.x0, c.y0, c.gam0, c.gamA0, c.alfa, c.minf, c.qinf, c.xref, c.yref);

  CHECK_EQ(first.centerOfPressureX, second.centerOfPressureX);
  CHECK_EQ(first.cl, second.cl);
  CHECK_EQ(first.cm, second.cm);
  CHECK(first.centerOfPressureX > 0.0 && first.centerOfPressureX < 1.0);
}

// End: tests/core/XcpDerivationTests.cpp

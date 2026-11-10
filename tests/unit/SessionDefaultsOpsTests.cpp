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

// File: tests/unit/SessionDefaultsOpsTests.cpp
// unit tests for the central defaults application
// (session::applyDefaults / applyDefaultsPartial, GETDEF completion
// xfoil.f 905-947). Checks the canonical field assignment, the
// per-page ACRIT(1)/ACRIT(2) values, the INIT starting values, and the
// error-case partial assignment without MRCL/COMSET/IDEVRP.

#include "TestSupport.h"

#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

namespace {

using namespace xfoil;
using xfoil::session::SessionComposition;

io::DefaultsValues modifiedValues() {
  io::DefaultsValues v = session::initDefaultsValues();
  v.npan = 120;
  v.cvpar = 0.8;
  v.cterat = 0.25;
  v.ctrrat = 0.3;
  v.xsref1 = 0.15;
  v.xsref2 = 0.85;
  v.xpref1 = 0.15;
  v.xpref2 = 0.85;
  v.lcolor = false;
  v.matyp = 2;
  v.minf1 = 0.3;
  v.vaccel = 0.02;
  v.retyp = 3;
  v.rmill = 3.0;
  v.acrit = {9.0, 12.0};
  v.xtrip = {0.9, 0.8};
  return v;
}

}  // namespace

TEST(unit, SessionDefaultsInitialStateMatchesFortranInit) {
  SessionComposition session;
  const io::DefaultsValues& v = session.defaults.values;
  CHECK_EQ(v.npan, 240);
  CHECK_NEAR(v.cvpar, 1.0, 0.0, 0.0);
  CHECK_NEAR(v.cterat, 0.3, 0.0, 0.0);
  CHECK_NEAR(v.size, 10.0, 0.0, 0.0);
  CHECK_NEAR(v.plotar, 0.55, 0.0, 0.0);
  CHECK_NEAR(v.cpmax, 1.0, 0.0, 0.0);
  CHECK_NEAR(v.xofair, 0.09, 0.0, 0.0);
  CHECK_NEAR(v.clpol[1], 1.5, 0.0, 0.0);
  CHECK_NEAR(v.alpol[0], -4.0, 0.0, 0.0);
  CHECK(v.lcolor);
  CHECK_EQ(session.defaults.idevrp, 4);
  CHECK(!session.defaults.loaded);
  CHECK_EQ(session.operating.criticalReynoldsExponents[0], 9.0);
  CHECK_EQ(session.operating.criticalReynoldsExponents[1], 9.0);
}

TEST(unit, SessionDefaultsApplySetsProductFieldsAndSides) {
  SessionComposition session;
  const io::DefaultsValues v = modifiedValues();
  session::applyDefaults(session, v, 2, "mod.def");

  CHECK_EQ(session.paneling.parameters.panelCount, 120);
  CHECK_NEAR(session.paneling.parameters.bunchingParam, 0.8, 0.0, 0.0);
  CHECK_NEAR(session.paneling.parameters.teToLeDensityRatio, 0.25, 0.0, 0.0);
  CHECK_NEAR(session.paneling.parameters.refinementDensityRatio, 0.3, 0.0, 0.0);
  CHECK_NEAR(session.paneling.parameters.upperRefinementRange[0], 0.15, 0.0,
             0.0);
  CHECK_NEAR(session.paneling.parameters.lowerRefinementRange[1], 0.85, 0.0,
             0.0);

  CHECK_EQ(session.operating.machReynolds.machType, 2);
  CHECK_EQ(session.operating.machReynolds.reType, 3);
  CHECK_NEAR(session.operating.machReynolds.minf1, 0.3, 0.0, 0.0);
  CHECK_NEAR(session.operating.machReynolds.reinf1, 3.0e6, 0.0, 0.0);
  CHECK_NEAR(session.operating.viscousAcceleration, 0.02, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 12.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.topTripX, 0.9, 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, 0.8, 0.0, 0.0);

  CHECK(session.defaults.loaded);
  CHECK_EQ(session.defaults.sourceFile, "mod.def");
  CHECK_EQ(session.defaults.idevrp, 2);
  CHECK_NEAR(session.defaults.values.size, 10.0, 0.0, 0.0);
}

TEST(unit, SessionDefaultsApplyPartialStopsAtReadLine) {
  SessionComposition session;
  const io::DefaultsValues v = modifiedValues();
  // READ error at line 13: lines 1-12 remain effective; RETYP/RMILL/
  // ACRIT/XSTRIP and IDEVRP/MRCL do not.
  session::applyDefaultsPartial(session, v, 12);

  CHECK_EQ(session.paneling.parameters.panelCount, 120);
  CHECK_NEAR(session.paneling.parameters.bunchingParam, 0.8, 0.0, 0.0);
  CHECK_NEAR(session.paneling.parameters.upperRefinementRange[0], 0.15, 0.0,
             0.0);
  CHECK_EQ(session.operating.machReynolds.machType, 2);
  CHECK_NEAR(session.operating.machReynolds.minf1, 0.3, 0.0, 0.0);
  CHECK_NEAR(session.operating.viscousAcceleration, 0.02, 0.0, 0.0);
  CHECK_EQ(session.operating.machReynolds.reType, 1);
  CHECK_NEAR(session.operating.machReynolds.reinf1, 0.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.topTripX, 1.0, 0.0, 0.0);
  CHECK(!session.defaults.loaded);
  CHECK_EQ(session.defaults.idevrp, 4);
  CHECK_NEAR(session.defaults.values.npan, 120, 0.0, 0.0);
}

TEST(unit, SessionDefaultsCodecRoundtripKeepsPlotFields) {
  const io::DefaultsValues original = session::initDefaultsValues();
  const std::string text = io::writeDefaults(original, 4);
  const io::DefaultsParseResult parsed = io::parseDefaults(text);
  CHECK(parsed.complete);
  CHECK_EQ(parsed.linesRead, 14);
  CHECK_EQ(parsed.values.npan, original.npan);
  CHECK_NEAR(parsed.values.size, original.size, 0.0, 0.0);
  CHECK_NEAR(parsed.values.plotar, original.plotar, 0.0, 0.0);
  CHECK_NEAR(parsed.values.ypage, original.ypage, 0.0, 0.0);
  CHECK_NEAR(parsed.values.cmpol[0], original.cmpol[0], 0.0, 0.0);
  CHECK(parsed.values.lcolor == original.lcolor);
  CHECK_NEAR(parsed.values.acrit[1], original.acrit[1], 0.0, 0.0);
}

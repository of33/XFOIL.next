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

// File: tests/unit/OperFnewCoreTests.cpp
// Handler tests for OPER FNEW (xoper.f
// 1629-1641). FNEW writes no session state and no output; these tests
// verify the canonical sentinel assignment and the LFLAP reset.
// Byte-accurate FMOM sequence behavior is covered by
// OperFmomCoreTests.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <sstream>

namespace {

using namespace xfoil;

session::SessionComposition bareCompositionWithFlap() {
  session::SessionComposition composition;
  composition.design.flap.xof = 0.25;
  composition.design.flap.yof = 0.01;
  composition.design.flap.lFlap = true;
  return composition;
}

}  // namespace

// Two inline values: XOF/YOF are set, LFLAP=false, no output.
TEST(unit, OperFnewTwoValuesSetsSentinels) {
  auto composition = bareCompositionWithFlap();
  std::ostringstream out;
  app::cli::handleOperFnewCore(composition, "0.7 -0.05", true);
  CHECK_EQ(composition.design.flap.xof, 0.7);
  CHECK_EQ(composition.design.flap.yof, -0.05);
  CHECK(!composition.design.flap.lFlap);
  CHECK_EQ(out.str(), std::string{});
}

// One inline value: YOF=-999.
TEST(unit, OperFnewOneValueSetsYofSentinel) {
  auto composition = bareCompositionWithFlap();
  app::cli::handleOperFnewCore(composition, "0.7", true);
  CHECK_EQ(composition.design.flap.xof, 0.7);
  CHECK_EQ(composition.design.flap.yof, -999.0);
  CHECK(!composition.design.flap.lFlap);
}

// No argument: both sentinels.
TEST(unit, OperFnewNoArgumentSetsBothSentinels) {
  auto composition = bareCompositionWithFlap();
  app::cli::handleOperFnewCore(composition, "", false);
  CHECK_EQ(composition.design.flap.xof, -999.0);
  CHECK_EQ(composition.design.flap.yof, -999.0);
  CHECK(!composition.design.flap.lFlap);
}

// A non-parseable argument corresponds to GETFLT NINPUT=0: both
// sentinels (oracle fnew_nonparse).
TEST(unit, OperFnewNonParseableArgumentSetsBothSentinels) {
  auto composition = bareCompositionWithFlap();
  app::cli::handleOperFnewCore(composition, "abc", true);
  CHECK_EQ(composition.design.flap.xof, -999.0);
  CHECK_EQ(composition.design.flap.yof, -999.0);
  CHECK(!composition.design.flap.lFlap);
}

// extra values are ignored (oracle fnew_extra: FMOM uses
// 0.7/-0.05).
TEST(unit, OperFnewExtraValuesIgnored) {
  auto composition = bareCompositionWithFlap();
  app::cli::handleOperFnewCore(composition, "0.7 -0.05 0.9", true);
  CHECK_EQ(composition.design.flap.xof, 0.7);
  CHECK_EQ(composition.design.flap.yof, -0.05);
  CHECK(!composition.design.flap.lFlap);
}

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

// file: tests/core/XFoilSessionHeaderProbe.cpp
// header-independence compile probe: this file includes only the header
// under test and proves that xfoil/XFoilSession.h compiles on its own
// (GeometrySetResult from xfoil/SessionTypes.h, Composition from
// xfoil/session/SessionState.h).

#include "xfoil/XFoilSession.h"

#include "TestSupport.h"

TEST(unit, XFoilSessionHeaderIsSelfSufficient) {
  xfoil::XFoilSession session;
  CHECK_FALSE(session.hasGeometry());
  CHECK_FALSE(session.hasConfiguration());
  const xfoil::AnalysisResult rejected =
      session.analyzeAtAlpha(xfoil::Degrees{4.0});
  CHECK(rejected.status == xfoil::AnalysisStatus::missingGeometry);
}

// End: tests/core/XFoilSessionHeaderProbe.cpp

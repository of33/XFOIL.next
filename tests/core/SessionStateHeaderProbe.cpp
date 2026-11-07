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

// file: tests/core/SessionStateHeaderProbe.cpp
// header-independence compile probe for xfoil/session/SessionState.h.
// the file includes only the header under test and proves that it compiles
// without include-order dependence: all stored object types are included by
// value, and AnalysisConfig is not forward-declared, so the header is
// self-sufficient.

#include "xfoil/session/SessionState.h"

#include "TestSupport.h"

TEST(unit, SessionStateHeaderIsSelfSufficient) {
  xfoil::session::SessionComposition composition;
  CHECK_FALSE(composition.geometry.valid);
  CHECK_FALSE(composition.paneling.panelingValid);
  CHECK_FALSE(composition.operating.configured);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.geometry.name.empty());
  CHECK_FALSE(composition.result.lastResult.successful);
}

// End: tests/core/SessionStateHeaderProbe.cpp

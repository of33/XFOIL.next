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

// file: tests/core/SessionOpsHeaderProbe.cpp
// header-independence compile probe for xfoil/session/SessionOps.h.
// the file includes only the header under test and proves that it compiles
// without the public header xfoil/XFoilSession.h. GeometrySetResult comes
// from the neutral value-type header xfoil/SessionTypes.h, so the header is
// self-sufficient.

#include "xfoil/session/SessionOps.h"

#include "TestSupport.h"

TEST(unit, SessionOpsHeaderIsSelfSufficient) {
  xfoil::session::SessionComposition composition;
  CHECK_FALSE(composition.geometry.valid);
  // Use-case basics link without including the facade.
  xfoil::session::resetBoundaryLayer(composition);
  CHECK_FALSE(xfoil::session::initializeAnalysis(composition));
}

// End: tests/core/SessionOpsHeaderProbe.cpp

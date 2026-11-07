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

// File: tests/unit/CancellationTokenTests.cpp
// unit tests for the instance-scoped cancellation token.

#include "TestSupport.h"

#include "xfoil/CancellationToken.h"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<xfoil::CancellationToken>,
              "CancellationToken must not be copy constructible");
static_assert(!std::is_copy_assignable_v<xfoil::CancellationToken>,
              "CancellationToken must not be copy assignable");

TEST(unit, CancellationTokenStartsClear) {
  xfoil::CancellationToken token;
  CHECK_FALSE(token.isCancellationRequested());
}

TEST(unit, CancellationTokenCanBeRequested) {
  xfoil::CancellationToken token;
  token.requestCancellation();
  CHECK(token.isCancellationRequested());
}

TEST(unit, CancellationTokenCanBeReset) {
  xfoil::CancellationToken token;
  token.requestCancellation();
  CHECK(token.isCancellationRequested());
  token.reset();
  CHECK_FALSE(token.isCancellationRequested());
}

// End: tests/unit/CancellationTokenTests.cpp

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

// File: tests/unit/ModernXFoilApiTests.cpp
// unit tests of the facade class xfoil::XFoil.
// the facade is non-copyable but movable; it starts without
// geometry and without configuration. No solver computation in these tests.

#include "TestSupport.h"

#include "xfoil/XFoil.h"

#include <type_traits>

static_assert(!std::is_copy_constructible_v<xfoil::XFoil>);
static_assert(!std::is_copy_assignable_v<xfoil::XFoil>);
static_assert(std::is_move_constructible_v<xfoil::XFoil>);
static_assert(std::is_move_assignable_v<xfoil::XFoil>);

TEST(unit, ModernXFoilIsNotCopyConstructible) {
  constexpr bool copyConstructible =
      std::is_copy_constructible_v<xfoil::XFoil>;
  CHECK_FALSE(copyConstructible);
}

TEST(unit, ModernXFoilIsNotCopyAssignable) {
  constexpr bool copyAssignable =
      std::is_copy_assignable_v<xfoil::XFoil>;
  CHECK_FALSE(copyAssignable);
}

TEST(unit, ModernXFoilIsMoveConstructible) {
  constexpr bool moveConstructible =
      std::is_move_constructible_v<xfoil::XFoil>;
  CHECK(moveConstructible);
  static_assert(
      std::is_nothrow_move_constructible_v<xfoil::XFoil>,
      "move construction of xfoil::XFoil must be noexcept");
}

TEST(unit, ModernXFoilIsMoveAssignable) {
  constexpr bool moveAssignable =
      std::is_move_assignable_v<xfoil::XFoil>;
  CHECK(moveAssignable);
  static_assert(
      std::is_nothrow_move_assignable_v<xfoil::XFoil>,
      "move assignment of xfoil::XFoil must be noexcept");
}

TEST(unit, ModernXFoilStartsWithoutGeometry) {
  xfoil::XFoil foil;
  CHECK_FALSE(foil.hasGeometry());
  CHECK(foil.geometry().empty());
  CHECK(foil.lastResult().status ==
        xfoil::AnalysisStatus::missingGeometry);
  CHECK_FALSE(foil.lastResult().successful);
}

TEST(unit, ModernXFoilStartsWithoutConfiguration) {
  xfoil::XFoil foil;
  CHECK_FALSE(foil.hasConfiguration());
}

// End: tests/unit/ModernXFoilApiTests.cpp

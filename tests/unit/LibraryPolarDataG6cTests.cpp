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

// File: tests/unit/LibraryPolarDataG6cTests.cpp
// Public owning PolarData and in-memory PolarText projection tests.

#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "TestSupport.h"
#include "xfoil/PolarData.h"
#include "xfoil/PolarText.h"

namespace {

using xfoil::ConvergenceStatus;
using xfoil::PolarColumn;
using xfoil::PolarData;
using xfoil::PolarElementQuantity;
using xfoil::PolarSideColumn;
using xfoil::PolarTextCompatibility;

std::string readFixture(const char* name) {
  std::ifstream stream(std::string("tests/data/xfoil-6.996/s11/") + name,
                       std::ios::binary);
  CHECK(stream.good());
  return std::string(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
}

const char* kMinimalPolar =
    "  \n"
    "       XFOIL         Version 6.99\n"
    "  \n"
    " Calculated polar for: NACA 0012\n"
    "  \n"
    " 1 1 Reynolds number fixed          Mach number fixed\n"
    "  \n"
    " xtrf =   1.000 (top)        1.000 (bottom)\n"
    " Mach =   0.000     Re =     1.000 e 6     Ncrit =   9.000  9.000\n"
    "  \n"
    "   alpha    CL        CD       CDp       CM     Top_Xtr  Bot_Xtr\n"
    "  ------ -------- --------- --------- -------- -------- --------\n"
    "   0.000   0.1000   0.01000   0.00100  -0.1000   0.1100   0.2100\n"
    "   2.000   0.2000   0.02000   0.00200  -0.2000   0.1200   0.2200\n";

}  // namespace

TEST(unit, LibraryPolarDataG6cDecodeIsOwningAndConverged) {
  const auto decoded = xfoil::decodePolarText(kMinimalPolar);
  CHECK(decoded.isOk());
  if (decoded.isErr()) {
    return;
  }
  const PolarData& polar = decoded.value();
  CHECK_EQ(polar.metadata.name.find("NACA 0012") != std::string::npos, true);
  CHECK_EQ(polar.points.size(), 2u);
  CHECK_EQ(polar.sideData.size(), 2u);
  CHECK(polar.points[0].convergence == ConvergenceStatus::converged);
  CHECK_NEAR(polar.points[0].alpha.value, 0.0, 0.0, 0.0);
  CHECK_NEAR(polar.points[0].liftCoefficient, 0.1, 1.0e-12, 0.0);
  CHECK_NEAR(polar.points[1].dragCoefficient, 0.02, 1.0e-12, 0.0);
  CHECK_EQ(polar.points[1].dragCoefficientPressure.has_value(), true);
  CHECK_NEAR(*polar.points[1].dragCoefficientPressure, 0.002, 1.0e-12,
             0.0);
  CHECK_EQ(polar.layout.baseColumns.size(), 5u);
  CHECK(polar.layout.baseColumns[0] == PolarColumn::alpha);
  CHECK_EQ(polar.layout.sideColumns.size(), 1u);
  CHECK(polar.layout.sideColumns[0] == PolarSideColumn::transitionX);
  CHECK_EQ(polar.sideData[0].size(), 2u);
}

TEST(unit, LibraryPolarDataG6cRoundtripPreservesValuesAndLayout) {
  const auto first = xfoil::decodePolarText(readFixture("canonical-base.pol"));
  CHECK(first.isOk());
  if (first.isErr()) {
    return;
  }
  const auto encoded = xfoil::encodePolarText(first.value());
  CHECK(encoded.isOk());
  if (encoded.isErr()) {
    return;
  }
  const auto second = xfoil::decodePolarText(encoded.value());
  CHECK(second.isOk());
  if (second.isErr()) {
    return;
  }
  CHECK_EQ(second.value().metadata.name, first.value().metadata.name);
  CHECK_EQ(second.value().metadata.version, first.value().metadata.version);
  CHECK(second.value().metadata.reynoldsDependency ==
        first.value().metadata.reynoldsDependency);
  CHECK_EQ(second.value().points.size(), first.value().points.size());
  CHECK(second.value().layout.baseColumns ==
        first.value().layout.baseColumns);
  CHECK(second.value().layout.sideColumns ==
        first.value().layout.sideColumns);
  for (std::size_t i = 0; i < first.value().points.size(); ++i) {
    CHECK_NEAR(second.value().points[i].alpha.value,
               first.value().points[i].alpha.value, 1.0e-12, 0.0);
    CHECK_NEAR(second.value().points[i].liftCoefficient,
               first.value().points[i].liftCoefficient, 1.0e-12, 0.0);
    CHECK_NEAR(second.value().points[i].dragCoefficient,
               first.value().points[i].dragCoefficient, 1.0e-12, 0.0);
    CHECK_NEAR(second.value().sideData[i][0].transitionX,
               first.value().sideData[i][0].transitionX, 1.0e-12, 0.0);
  }
}

TEST(unit, LibraryPolarDataG6cElementRoundtripPreservesElementData) {
  const auto first = xfoil::decodePolarText(readFixture("canonical-elem4.pol"));
  CHECK(first.isOk());
  if (first.isErr()) {
    return;
  }
  CHECK_EQ(first.value().metadata.elementCount, 1u);
  CHECK_EQ(first.value().layout.elementSelection.count, 4u);
  CHECK_EQ(first.value().elementData.size(), first.value().points.size());
  CHECK_EQ(first.value().layout.elementSelection.columns[0].has_value(),
           true);
  CHECK(*first.value().layout.elementSelection.columns[0] ==
        PolarElementQuantity::cx);

  const auto encoded = xfoil::encodePolarText(first.value());
  CHECK(encoded.isOk());
  if (encoded.isErr()) {
    return;
  }
  const auto second = xfoil::decodePolarText(encoded.value());
  CHECK(second.isOk());
  if (second.isErr()) {
    return;
  }
  CHECK_EQ(second.value().layout.elementSelection.count, 4u);
  CHECK_EQ(second.value().elementData.size(), second.value().points.size());
  CHECK_NEAR(second.value().elementData[0][0].cx,
             first.value().elementData[0][0].cx, 1.0e-12, 0.0);
  CHECK_NEAR(second.value().elementData[0][3].cdVisc,
             first.value().elementData[0][3].cdVisc, 1.0e-12, 0.0);
}

TEST(unit, LibraryPolarDataG6cInvalidTextIsAtomic) {
  const auto decoded = xfoil::decodePolarText("not a PolarText file\n");
  CHECK(decoded.isErr());
  CHECK_EQ(decoded.error().code, std::string("polar.parse"));
}

TEST(unit, LibraryPolarDataG6cMissingSelectedValueIsRejected) {
  const auto decoded = xfoil::decodePolarText(kMinimalPolar);
  CHECK(decoded.isOk());
  if (decoded.isErr()) {
    return;
  }
  PolarData polar = decoded.value();
  polar.points[0].dragCoefficientPressure.reset();
  const auto encoded = xfoil::encodePolarText(polar);
  CHECK(encoded.isErr());
  CHECK_EQ(encoded.error().code, std::string("polar.missingColumnValue"));
}

TEST(unit, LibraryPolarDataG6cCapacityAndCompatibility) {
  PolarData polar;
  polar.metadata.elementCount = 1;
  polar.layout.baseColumns = {PolarColumn::alpha, PolarColumn::liftCoefficient,
                              PolarColumn::dragCoefficient,
                              PolarColumn::momentCoefficient};
  polar.points.reserve(xfoil::maxPolarTextPoints + 1);
  for (std::size_t i = 0; i < xfoil::maxPolarTextPoints + 1; ++i) {
    xfoil::PolarPoint point;
    point.alpha = xfoil::Degrees{static_cast<double>(i)};
    polar.points.push_back(point);
  }
  const auto encoded = xfoil::encodePolarText(polar);
  CHECK(encoded.isErr());
  CHECK_EQ(encoded.error().code, std::string("polar.pointLimit"));

  const auto broken = xfoil::decodePolarText(
      readFixture("broken-6996-base-v700.pol"),
      PolarTextCompatibility::xfoil6996BrokenSideOrder);
  CHECK(broken.isOk());
}

// End: tests/unit/LibraryPolarDataG6cTests.cpp

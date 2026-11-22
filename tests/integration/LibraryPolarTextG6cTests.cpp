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

// File: tests/integration/LibraryPolarTextG6cTests.cpp
// End-to-end public PolarText conversion with the accepted on-disk
// fixtures. The codec itself remains file- and path-free.

#include <fstream>
#include <sstream>
#include <string>

#include "TestSupport.h"
#include "xfoil/PolarData.h"
#include "xfoil/PolarText.h"

namespace {

std::string readFixture(const char* name) {
  std::ifstream stream(std::string("tests/data/xfoil-6.996/s11/") + name,
                       std::ios::binary);
  CHECK(stream.good());
  // No istreambuf_iterator: GCC -O3 (-Wnull-dereference) cannot prove
  // the buffer pointer of a failed stream to be non-zero.
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

void checkStableRoundtrip(const char* fixture) {
  const auto decoded = xfoil::decodePolarText(readFixture(fixture));
  CHECK(decoded.isOk());
  if (decoded.isErr()) {
    return;
  }
  const auto encoded1 = xfoil::encodePolarText(decoded.value());
  CHECK(encoded1.isOk());
  if (encoded1.isErr()) {
    return;
  }
  const auto decoded2 = xfoil::decodePolarText(encoded1.value());
  CHECK(decoded2.isOk());
  if (decoded2.isErr()) {
    return;
  }
  const auto encoded2 = xfoil::encodePolarText(decoded2.value());
  CHECK(encoded2.isOk());
  if (encoded2.isErr()) {
    return;
  }
  CHECK_EQ(encoded1.value(), encoded2.value());
  CHECK_EQ(decoded2.value().points.size(), decoded.value().points.size());
  CHECK(decoded2.value().layout.baseColumns ==
        decoded.value().layout.baseColumns);
}

}  // namespace

TEST(integration, LibraryPolarTextG6cCanonicalBaseAndElements) {
  checkStableRoundtrip("canonical-base.pol");
  checkStableRoundtrip("canonical-elem1.pol");
  checkStableRoundtrip("canonical-elem4.pol");
}

TEST(integration, LibraryPolarTextG6cInvalidFixtureIsAtomic) {
  const auto decoded =
      xfoil::decodePolarText(readFixture("truncated-row.pol"));
  CHECK(decoded.isErr());
  CHECK_EQ(decoded.error().code, std::string("polar.parse"));
}

// End: tests/integration/LibraryPolarTextG6cTests.cpp

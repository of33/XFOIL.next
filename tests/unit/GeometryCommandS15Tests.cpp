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

// File: tests/unit/GeometryCommandS15Tests.cpp
// S15 geometry command contracts: 6.996 NACA/NACS buffer size and the
// GDES SQTE/CIRC/SYTB/XCEN session operations. Canonical 6.996 oracle
// values live under tests/data/xfoil-6.996/s15/.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;

void buildNaca2412(session::SessionComposition& session) {
  std::ostringstream sink;
  app::cli::handleNacaCore(session, sink, 2412);
  CHECK(session.geometry.valid);
  CHECK_EQ(session.geometry.buffer.x.size(), std::size_t{399});
  CHECK(session::initializeGeometryDesign(session).status ==
        session::GeometryDesignInitStatus::initialized);
}

std::vector<double> panelX(const session::SessionComposition& session) {
  return session.paneling.panel.x;
}

double trailingEdgeGap(const core::geometry::AirfoilBuffer& buffer) {
  return std::abs(buffer.y.front() - buffer.y.back());
}

}  // namespace

TEST(unit, GeometryCommandS15NacaAndNacsBuffers) {
  const auto normal = core::geometry::nacaAirfoil(12);
  const auto sharp = core::geometry::nacaAirfoil(12, true);
  CHECK(normal.isOk());
  CHECK(sharp.isOk());
  if (!normal.isOk() || !sharp.isOk()) {
    return;
  }
  CHECK_EQ(normal.value().buffer.x.size(), std::size_t{399});
  CHECK_EQ(sharp.value().buffer.x.size(), std::size_t{399});
  CHECK(normal.value().buffer.name == "NACA 0012");
  CHECK(sharp.value().buffer.name == "NACA 0012");
  CHECK_NEAR(trailingEdgeGap(normal.value().buffer), 0.00252, 1.0e-12, 0.0);
  CHECK_NEAR(trailingEdgeGap(sharp.value().buffer), 0.0, 1.0e-15, 0.0);
}

TEST(unit, GeometryCommandS15GdesSquareTrailingEdge) {
  session::SessionComposition session;
  buildNaca2412(session);
  const auto beforePanel = panelX(session);
  const auto beforeBuffer = session.geometry.buffer;

  const auto result = session::gdesSquareTrailingEdge(session);
  CHECK(result.isOk());
  if (result.isOk()) {
    CHECK(result.value().converged);
  }
  CHECK_EQ(session.geometry.buffer.x.size(), std::size_t{399});
  CHECK_NEAR(trailingEdgeGap(session.geometry.buffer), 0.00250927,
             1.0e-8, 0.0);
  CHECK(panelX(session) == beforePanel);
  CHECK(session.geometry.buffer.y != beforeBuffer.y);
}

TEST(unit, GeometryCommandS15GdesTrailingEdgeCircleUsesNHcirc) {
  session::SessionComposition session;
  buildNaca2412(session);
  const auto beforePanel = panelX(session);

  const auto result = session::gdesAddTrailingEdgeCircle(session, 15);
  CHECK(result.isOk());
  if (result.isOk()) {
    CHECK_EQ(result.value().nHCirc, std::size_t{15});
  }
  CHECK_EQ(session.geometry.buffer.x.size(), std::size_t{429});
  CHECK_NEAR(trailingEdgeGap(session.geometry.buffer), 0.0, 1.0e-14, 0.0);
  CHECK(panelX(session) == beforePanel);
}

TEST(unit, GeometryCommandS15GdesScaleTopBottomY) {
  session::SessionComposition session;
  buildNaca2412(session);
  const auto beforePanel = panelX(session);
  const auto beforeX = session.geometry.buffer.x;
  const auto beforeY = session.geometry.buffer.y;

  const auto result = session::gdesScaleTopBottomY(session, 1.1, 0.9);
  CHECK(result.ok);
  CHECK_EQ(session.geometry.buffer.x.size(), std::size_t{399});
  CHECK(session.geometry.buffer.x == beforeX);
  CHECK(session.geometry.buffer.y != beforeY);
  CHECK(panelX(session) == beforePanel);
}

TEST(unit, GeometryCommandS15GdesCenterPointQuery) {
  session::SessionComposition session;
  buildNaca2412(session);
  const auto beforeBuffer = session.geometry.buffer;
  const auto beforePanel = panelX(session);

  const auto result = session::gdesCenterPointQuery(session, 0.5, 0.5);
  CHECK(result.ok);
  CHECK(result.topFound);
  CHECK(result.bottomFound);
  CHECK(result.topConverged);
  CHECK(result.bottomConverged);
  CHECK_NEAR(result.topX, 0.5, 1.0e-8, 0.0);
  CHECK_NEAR(result.topY, 0.0723847, 1.0e-7, 0.0);
  CHECK_NEAR(result.middleX, 0.5, 1.0e-8, 0.0);
  CHECK_NEAR(result.middleY, 0.0194444, 1.0e-7, 0.0);
  CHECK_NEAR(result.bottomX, 0.5, 1.0e-8, 0.0);
  CHECK_NEAR(result.bottomY, -0.0334958, 1.0e-7, 0.0);
  CHECK(session.geometry.buffer.x == beforeBuffer.x);
  CHECK(session.geometry.buffer.y == beforeBuffer.y);
  CHECK(panelX(session) == beforePanel);
}

// End: tests/unit/GeometryCommandS15Tests.cpp

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

// File: tests/unit/SessionIntxHalfStateTests.cpp
// INTX/HALF buffer commit according to the canonical contract
// (xfoil.f INTX 2566-2580, TOP-HALF 242-252). Proves: exclusively
// the buffer is changed; current panel contour, panel validity,
// panel revision, CPI/CPV, wake/BL/mapping, polar slots, flap state,
// inverse binding and airfoil name remain unchanged; buffer revision
// and buffer metrics are updated; an active GDES working buffer is
// refreshed as in Fortran (shared SB/XBP/YBP); error paths without any
// mutation; session independence.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <bit>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using xfoil::app::cli::handleOperAlphaCore;

void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(
      session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);
}

void configureInviscid(session::SessionComposition& composition) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());
}

bool bitsEqual(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::bit_cast<std::uint64_t>(a[i]) !=
        std::bit_cast<std::uint64_t>(b[i])) {
      return false;
    }
  }
  return true;
}

std::vector<double> contourX(int designation, std::size_t nside) {
  const auto n = core::geometry::naca4(designation, nside);
  CHECK(n.isOk());
  return n.value().xb;
}

std::vector<double> contourY(int designation, std::size_t nside) {
  const auto n = core::geometry::naca4(designation, nside);
  CHECK(n.isOk());
  return n.value().yb;
}

}  // namespace

// INTX: pure buffer mutation. panel contour, panel revision, CPI,
// airfoil name and polar slots remain bit- or value-identical.
TEST(unit, SessionIntxBufferOnlyKeepsPanelCpiAndName) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> panelY = composition.paneling.panel.y;
  const std::vector<double> panelS = composition.paneling.panel.s;
  const std::vector<double> cpi = composition.viscous.cpi;
  const std::uint64_t panelRevision = composition.paneling.panelRevision;
  const std::uint64_t panelSourceRevision =
      composition.paneling.panelSourceRevision;
  const std::string name = composition.geometry.name;
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;

  const session::BufferOnlyGeometryResult applied =
      session::applyIntxBufferInterpolation(
          composition, contourX(2412, 123), contourY(2412, 123),
          contourX(12, 123), contourY(12, 123), 0.5);
  CHECK(applied.ok);
  CHECK_EQ(applied.pointCount, std::size_t{245});
  CHECK_EQ(composition.geometry.buffer.x.size(), std::size_t{245});
  CHECK(composition.geometry.bufferRevision == revisionBefore + 1);

  // unchanged: panel contour (bit-identical), validity, revisions,
  // CPI, name, metrics field (panel metrics are not touched).
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK(bitsEqual(panelY, composition.paneling.panel.y));
  CHECK(bitsEqual(panelS, composition.paneling.panel.s));
  CHECK(composition.paneling.panelingValid);
  CHECK(composition.paneling.panelRevision == panelRevision);
  CHECK(composition.paneling.panelSourceRevision == panelSourceRevision);
  CHECK(bitsEqual(cpi, composition.viscous.cpi));
  CHECK_EQ(composition.geometry.name, name);
  CHECK(composition.geometry.metrics.has_value());

  // buffer metrics of the result (0.5 mix 2412/0012: thickness ~0.12,
  // camber ~0.01).
  CHECK(applied.bufferMetrics.thick > 0.11);
  CHECK(applied.bufferMetrics.thick < 0.13);
  CHECK(applied.bufferMetrics.cambr > 0.009);
  CHECK(applied.bufferMetrics.cambr < 0.011);
}

// INTX name contract: the name is set only after the buffer mutation
// (canonical ASKS order); canonically the name is a shared field
// (buffer and current name of the session).
TEST(unit, SessionIntxNameSetOnlyByExplicitOperation) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  const session::BufferOnlyGeometryResult applied =
      session::applyIntxBufferInterpolation(
          composition, contourX(2412, 123), contourY(2412, 123),
          contourX(12, 123), contourY(12, 123), 0.5);
  CHECK(applied.ok);
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  session::setGeometryName(composition, "MIXED");
  CHECK_EQ(composition.geometry.name, std::string("MIXED"));
  CHECK_EQ(composition.geometry.buffer.name, std::string("MIXED"));
}

// HALF: 245 -> 123 -> 62 only in the buffer; panel/CPI/name
// unchanged; no automatic paneling or invalidation.
TEST(unit, SessionHalfDecimatesBufferOnly) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> cpi = composition.viscous.cpi;
  const std::string name = composition.geometry.name;
  const std::uint64_t panelRevision = composition.paneling.panelRevision;
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;

  const session::BufferOnlyGeometryResult first =
      session::applyHalfBufferDecimation(composition);
  CHECK(first.ok);
  CHECK_EQ(first.pointCount, std::size_t{123});
  const session::BufferOnlyGeometryResult second =
      session::applyHalfBufferDecimation(composition);
  CHECK(second.ok);
  CHECK_EQ(second.pointCount, std::size_t{62});
  CHECK_EQ(composition.geometry.buffer.x.size(), std::size_t{62});
  CHECK(composition.geometry.bufferRevision == revisionBefore + 2);

  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK(composition.paneling.panelingValid);
  CHECK(composition.paneling.panelRevision == panelRevision);
  CHECK(bitsEqual(cpi, composition.viscous.cpi));
  CHECK_EQ(composition.geometry.name, name);
}

// HALF without valid buffer or with N < 2: no mutation.
TEST(unit, SessionHalfWithoutBufferLeavesStateUntouched) {
  session::SessionComposition composition;
  const session::BufferOnlyGeometryResult applied =
      session::applyHalfBufferDecimation(composition);
  CHECK(!applied.ok);
  CHECK(!composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
}

// INTX with too-short sources (LEFIND requires N >= 3): no mutation.
TEST(unit, SessionIntxTooShortSourcesNoMutation) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;
  const std::vector<double> bx = composition.geometry.buffer.x;
  const session::BufferOnlyGeometryResult applied =
      session::applyIntxBufferInterpolation(
          composition, {0.0, 1.0}, {0.0, 0.1}, contourX(2412, 123),
          contourY(2412, 123), 0.5);
  CHECK(!applied.ok);
  CHECK_EQ(composition.geometry.bufferRevision, revisionBefore);
  CHECK(bitsEqual(bx, composition.geometry.buffer.x));
}

// active GDES working buffer is adapted to the new buffer contour as
// in Fortran (shared SB/XBP/YBP).
TEST(unit, SessionIntxRefreshesActiveDesignWork) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  CHECK(session::initializeGeometryDesign(composition).status ==
        session::GeometryDesignInitStatus::initialized);
  CHECK(composition.design.work.has_value());
  const session::BufferOnlyGeometryResult applied =
      session::applyIntxBufferInterpolation(
          composition, contourX(2412, 61), contourY(2412, 61),
          contourX(12, 61), contourY(12, 61), 0.5);
  CHECK(applied.ok);
  CHECK_EQ(applied.pointCount, std::size_t{121});
  CHECK(composition.design.work.has_value());
  if (composition.design.work) {
    CHECK_EQ(composition.design.work->s.size(), std::size_t{121});
    CHECK_EQ(composition.design.work->xp.size(), std::size_t{121});
  }
}

// INTX and HALF in independent sessions: changing one session leaves
// the other untouched.
TEST(unit, SessionIntxHalfIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  buildNaca2412Paned(a);
  buildNaca2412Paned(b);
  const std::vector<double> bBuffer = b.geometry.buffer.x;

  CHECK(session::applyIntxBufferInterpolation(
            a, contourX(2412, 123), contourY(2412, 123), contourX(12, 123),
            contourY(12, 123), 0.5)
            .ok);
  CHECK(session::applyHalfBufferDecimation(a).ok);

  CHECK(bitsEqual(bBuffer, b.geometry.buffer.x));
  CHECK_EQ(b.geometry.bufferRevision, 1u);
}

// HALF after INTX: chain contract 245 -> 123 (INTX-N stays N0) and
// subsequent decimation.
TEST(unit, SessionHalfAfterIntxChains) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  CHECK(session::applyIntxBufferInterpolation(
            composition, contourX(2412, 61), contourY(2412, 61),
            contourX(12, 61), contourY(12, 61), 0.5)
            .ok);
  CHECK_EQ(composition.geometry.buffer.x.size(), std::size_t{121});
  const session::BufferOnlyGeometryResult half =
      session::applyHalfBufferDecimation(composition);
  CHECK(half.ok);
  CHECK_EQ(half.pointCount, std::size_t{61});
}

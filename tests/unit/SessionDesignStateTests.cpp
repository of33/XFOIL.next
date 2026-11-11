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

// File: tests/unit/SessionDesignStateTests.cpp
// unit tests for the design session state
// (GDES/CAMB entry/exit frame xgdes.f 40-84, xtcam.f 41/184) and
// the PLOP menu-state operations (OPLSET, plutil.f 21-176). Only session
// state is checked: no geometry, panel or analysis mutation, no plot
// backend.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <sstream>
#include <string>

namespace {

using namespace xfoil;
using xfoil::session::SessionComposition;

// Two-point buffer contour via the existing LOAD path (no
// panel generation, no current contour).
void loadBufferAirfoil(SessionComposition& session) {
  core::geometry::AirfoilBuffer buffer{{0.0, 1.0}, {0.0, 0.1}, "TEST"};
  const auto result = session::setBufferGeometry(session, std::move(buffer));
  CHECK(result.success());
}

TEST(unit, DesignInitWithoutAirfoilLeavesStateUnchanged) {
  SessionComposition session;
  const auto result = session::initializeGeometryDesign(session);
  CHECK(result.status == session::GeometryDesignInitStatus::noAirfoil);
  CHECK_FALSE(session.design.menu.active);
  CHECK_EQ(session.design.entryBufferRevision, 0u);
  CHECK_FALSE(session.design.menu.gdesRedo.has_value());
  CHECK_FALSE(session.design.menu.cambRedo.has_value());
}

TEST(unit, DesignInitWithBufferActivatesMenuWithoutGeometryChange) {
  SessionComposition session;
  loadBufferAirfoil(session);
  const std::uint64_t revisionBefore = session.geometry.bufferRevision;
  const auto result = session::initializeGeometryDesign(session);
  CHECK(result.status == session::GeometryDesignInitStatus::initialized);
  CHECK(session.design.menu.active);
  CHECK_FALSE(session.design.lpLcam);  // xgdes.f: LPLCAM=.FALSE.
  CHECK_EQ(session.design.entryBufferRevision, revisionBefore);
  // No geometry/panel/revision mutation.
  CHECK_EQ(session.geometry.bufferRevision, revisionBefore);
  CHECK(session.geometry.valid);
  CHECK_FALSE(session.design.menu.gdesRedo.has_value());
}

TEST(unit, DesignLeaveClearsMenuAndSymmetryOnly) {
  SessionComposition session;
  loadBufferAirfoil(session);
  (void)session::initializeGeometryDesign(session);
  session.design.lgSym = true;
  session.design.lgSame = true;
  const std::uint64_t revisionBefore = session.geometry.bufferRevision;

  const auto exit = session::leaveGeometryDesignWithoutCommit(session);
  CHECK(exit.lgsame);
  CHECK_FALSE(session.design.menu.active);
  CHECK_FALSE(session.design.lgSym);  // xgdes.f 72-84: LGSYM=.FALSE.
  CHECK(session.design.lgSame);       // remains set (plot warning comes later)
  CHECK_EQ(session.geometry.bufferRevision, revisionBefore);
}

TEST(unit, CamberPlotParameterSaveRestoreRoundTrip) {
  SessionComposition session;
  CHECK(session.design.lgParm);     // INIT value
  CHECK(session.design.lgParmSave);
  session::savePlotParameterForCamberEntry(session);
  session.design.lgParm = false;    // CAMB block lowers LGPAR
  session::restorePlotParameterAfterCamberExit(session);
  CHECK(session.design.lgParm);
}

TEST(unit, PlotToggleGraphicsFollowsIdevSemantics) {
  SessionComposition session;
  CHECK_EQ(session.design.plot.idev, 1);  // INIT xfoil.f
  session::togglePlotGraphics(session);
  CHECK_EQ(session.design.plot.idev, 0);
  session::togglePlotGraphics(session);
  CHECK_EQ(session.design.plot.idev, 1);
}

TEST(unit, PlotNumericOptionsWriteSingleDefaultsStorage) {
  SessionComposition session;
  session::setPlotSize(session, 12.0);
  session::setPlotAspect(session, 0.75);
  session::setPlotPage(session, 14.0, 9.0);
  session::setPlotMargins(session, 0.5, 1.0);
  session::setPlotFont(session, 0.02);
  session::setPlotScreenFraction(session, 0.5);
  CHECK_NEAR(session.defaults.values.size, 12.0, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.plotar, 0.75, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.xpage, 14.0, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.ypage, 9.0, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.xmarg, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.ymarg, 1.0, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.ch, 0.02, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.scrnfr, 0.5, 0.0, 0.0);
}

TEST(unit, PlotOrientationTogglesFlagAndSwapsPage) {
  SessionComposition session;
  CHECK(session.design.plot.lland);
  CHECK_NEAR(session.defaults.values.xpage, 11.0, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.ypage, 8.5, 0.0, 0.0);
  session::togglePlotOrientation(session);
  CHECK_FALSE(session.design.plot.lland);
  CHECK_NEAR(session.defaults.values.xpage, 8.5, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.ypage, 11.0, 0.0, 0.0);
}

TEST(unit, PlotColorTogglesIdevrpAndLcolor) {
  SessionComposition session;
  CHECK_EQ(session.defaults.idevrp, 4);  // INIT
  session::togglePlotColor(session);
  CHECK_EQ(session.defaults.idevrp, 2);
  CHECK_FALSE(session.defaults.values.lcolor);
  session::togglePlotColor(session);
  CHECK_EQ(session.defaults.idevrp, 4);
  CHECK(session.defaults.values.lcolor);
}

TEST(unit, PlotLineColorsClampToOneToTen) {
  SessionComposition session;
  CHECK_EQ(session.design.plot.icols[0], 8);  // INIT xfoil.f
  CHECK_EQ(session.design.plot.icols[1], 3);
  session::setPlotLineColors(session, 3, 4);
  CHECK_EQ(session.design.plot.icols[0], 3);
  CHECK_EQ(session.design.plot.icols[1], 4);
  session::setPlotLineColors(session, 0, 99);
  CHECK_EQ(session.design.plot.icols[0], 1);
  CHECK_EQ(session.design.plot.icols[1], 10);
}

TEST(unit, PlotBlowupToggleChangesLcursOnly) {
  SessionComposition session;
  CHECK(session.defaults.values.lcurs);
  session::togglePlotBlowupInput(session);
  CHECK_FALSE(session.defaults.values.lcurs);
}

TEST(unit, DesignStateDefaultsMatchFortranInit) {
  SessionComposition session;
  CHECK_FALSE(session.design.lgSame);
  CHECK_FALSE(session.design.lgSym);
  CHECK(session.design.lgSlop);
  CHECK(session.design.lcSlop);
  CHECK_FALSE(session.design.lpLcam);
  CHECK_FALSE(session.design.ldCplot);
  CHECK(session.design.lgParm);
  CHECK(session.design.lgGrid);
  CHECK(session.design.lgTick);
  CHECK_NEAR(session.design.gtick, 0.0005, 0.0, 0.0);
  CHECK(session.design.lgParmSave);
  CHECK(session.design.oname.empty());
  CHECK_FALSE(session.design.camber.has_value());
  CHECK_EQ(session.design.plot.idev, 1);
  CHECK_EQ(session.design.plot.ipslu, 0);
  CHECK(session.design.plot.lland);
  CHECK_EQ(session.design.plot.icols[0], 8);
  CHECK_EQ(session.design.plot.icols[1], 3);
}

// state contract: a direct GDES buffer command mutates only buffer geometry
// and the buffer revision; panel contour, panel provenance,
// panelSourceRevision, wake/system state and lastResult are preserved
// (xgdes.f: no ABCOPY/PANGEN outside eXec).
// No other session state is touched.
TEST(unit, GdesBufferOpPreservesPanelAndAnalysisState) {
  SessionComposition session;
  std::ostringstream out;
  testing::handleLegacyNacaCore(session, out, 12);
  (void)app::cli::handlePaneCore(session, out);
  CHECK(session.paneling.panelingValid);

  const auto panelBefore = session.paneling.panel;
  const std::uint64_t sourceRevisionBefore =
      session.paneling.panelSourceRevision;
  const bool wakeValidBefore = session.viscous.wakeValid;
  const double awakeBefore = session.viscous.awake;
  const bool systemValidBefore = session.viscous.systemValid;

  (void)session::initializeGeometryDesign(session);
  const auto op = session::gdesTranslate(session, 0.5, 0.0);
  CHECK(op.ok);

  // buffer mutation and revision increment.
  CHECK_EQ(session.geometry.bufferRevision, sourceRevisionBefore + 1);
  CHECK_FALSE(session.design.lgSame);
  CHECK_NEAR(session.geometry.buffer.x.front(),
             panelBefore.x.front() + 0.5, 1.0e-12, 0.0);

  // panel/analysis frame unchanged (OPER still consumes the old
  // contour, cf. the GDES/OPER state contract).
  CHECK(session.paneling.panelingValid);
  CHECK(session.paneling.panel.x == panelBefore.x);
  CHECK(session.paneling.panel.y == panelBefore.y);
  CHECK_EQ(session.paneling.panelSourceRevision, sourceRevisionBefore);
  CHECK_EQ(session.viscous.wakeValid, wakeValidBefore);
  CHECK_EQ(session.viscous.awake, awakeBefore);
  CHECK_EQ(session.viscous.systemValid, systemValidBefore);
}

}  // namespace

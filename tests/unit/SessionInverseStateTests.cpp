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

// File: tests/unit/SessionInverseStateTests.cpp
// unit tests for the inverse session state
// (shared MDES/QDES state XFOIL.INC CR07/CI01/Cl01 + CIRCLE.INC,
// separated menu/redo states xmdes.f 44/114-134, xqdes.f 42/112-130,
// validity/revision contract and invalidation rules xfoil.f
// 466-533/704, xgdes.f 1027, xfoil.f 2087-2088, xpol.f 172, xqdes.f 499).
// only session state is checked: no inverse
// numerics, no
// API/batch mutation.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/core/Limits.h"
#include "xfoil/core/inverse/CircleKernels.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <sstream>
#include <string>
#include <utility>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

using session::SessionComposition;

// Valid panel contour via the existing NACA path (buffer + PANGEN).
void setupPanelAirfoil(SessionComposition& session) {
  std::ostringstream out;
  cli::handleNacaCore(session, out, 2412);
  CHECK(session.paneling.panelingValid);
  CHECK(!session.paneling.panel.x.empty());
}

// inverse state manually marked as "initialized from the current
// panel contour" (the numerical SCINIT path itself is not run).
void markMappingInitialized(SessionComposition& session) {
  session.inverse.validity.grid = session::CircleGridValidity::initialized;
  session.inverse.validity.mapping =
      session::MappingCoordinateValidity::initializedFromPanel;
  session.inverse.provenance.sourcePanelRevision =
      session.paneling.panelRevision;
  session.inverse.provenance.sourceBufferRevision =
      session.paneling.panelSourceRevision;
  session.inverse.provenance.sourcePanelOrigin = session.paneling.origin;
}

void markQspecPresent(SessionComposition& session) {
  session.inverse.validity.qspec = session::QSpecValidity::present;
  session.inverse.validity.qspecOrigin = session::QSpecOrigin::menuEdits;
}

}  // namespace

// Initial state: INIT defaults xfoil.f 466-533/704 plus COMMON zeros.
TEST(unit, InverseStateDefaultsMatchFortranInit) {
  SessionComposition session;
  const auto& inv = session.inverse;
  CHECK_EQ(inv.shared.nc1, 257);  // INIT xfoil.f 530-533
  CHECK_EQ(inv.shared.nsp, 0);
  CHECK_EQ(inv.shared.nqsp, 0);
  CHECK_EQ(inv.shared.iacqsp, 0);
  // Fortran COMMON KQTARG=0 (xfoil.f COMMON init) shows on
  // QSPEC(:,0), which in COMMON/CR07 exactly aliases QGAMM(IBX)
  // (XFOIL.INC 77-80). Represented faithfully as -1 (no target column index).
  CHECK_EQ(inv.shared.kqtarg, -1);
  CHECK_NEAR(inv.shared.ffilt, 0.0, 0.0, 0.0);  // xfoil.f 704
  CHECK_NEAR(inv.shared.qdof0, 0.0, 0.0, 0.0);
  CHECK_NEAR(inv.shared.qdof1, 0.0, 0.0, 0.0);
  CHECK_NEAR(inv.shared.qdof2, 0.0, 0.0, 0.0);
  CHECK_NEAR(inv.shared.qdof3, 0.0, 0.0, 0.0);
  CHECK(inv.shared.qspecs.empty());
  CHECK(inv.shared.qgamm.empty());
  CHECK(inv.shared.sspec.empty());
  CHECK_EQ(inv.shared.circle.grid.nc, 0);  // EIWSET still not run
  // validity defaults.
  CHECK(inv.validity.grid == session::CircleGridValidity::notInitialized);
  CHECK(inv.validity.mapping ==
        session::MappingCoordinateValidity::neverInitialized);
  CHECK(inv.validity.qspec == session::QSpecValidity::absent);
  CHECK(inv.validity.qspecOrigin == session::QSpecOrigin::none);
  // COMMON flag defaults (INIT xfoil.f 466-502).
  CHECK_FALSE(inv.flags.lqSym);
  CHECK(inv.flags.lqSlop);
  CHECK_FALSE(inv.flags.lqVdes);
  CHECK_FALSE(inv.flags.lqRefl);
  CHECK_FALSE(inv.flags.lcpxx);
  // menu-/redo states inactive, redo invalid ('****').
  CHECK_FALSE(inv.mdes.active);
  CHECK_FALSE(inv.qdes.active);
  CHECK_FALSE(inv.mdes.redo.has_value());
  CHECK_FALSE(inv.qdes.redo.has_value());
  // Origin-/revision binding zero-initialized.
  CHECK_EQ(inv.provenance.sourcePanelRevision, 0u);
  CHECK_EQ(inv.provenance.sourceBufferRevision, 0u);
  CHECK(inv.provenance.sourcePanelOrigin == session::PanelingOrigin::none);
  // capacity limits are documented bounds, no reserved
  // cells: NC1 == ICX, IPX from limits.
  CHECK_EQ(inv.shared.nc1,
           static_cast<int>(core::inverse::kCircleMaxPoints));
  CHECK_EQ(core::qspecDistributionCapacity, std::size_t{5});
}

// Instance relatedness: two independent sessions, no static
// numerical fields.
TEST(unit, InverseStateInstancesIndependent) {
  SessionComposition first;
  SessionComposition second;
  setupPanelAirfoil(first);
  (void)session::enterMdesMenuState(first);
  first.inverse.shared.ffilt = 0.2;
  first.inverse.flags.lqVdes = true;
  first.inverse.mdes.redo = std::make_pair("QSET", "");
  CHECK(first.inverse.mdes.active);
  CHECK_FALSE(second.inverse.mdes.active);
  CHECK_FALSE(second.inverse.flags.lqVdes);
  CHECK_NEAR(second.inverse.shared.ffilt, 0.0, 0.0, 0.0);
  CHECK_FALSE(second.inverse.mdes.redo.has_value());
  CHECK_EQ(second.inverse.shared.nc1, 257);
}

// active length separate from capacity.
TEST(unit, InverseActiveLengthSeparateFromCapacity) {
  SessionComposition session;
  setupPanelAirfoil(session);
  (void)session::enterMdesMenuState(session);
  // NQSP (active column count) == size of the column list; capacity IPX.
  CHECK_EQ(session.inverse.shared.nqsp,
           static_cast<int>(session.inverse.shared.qspecs.size()));
  CHECK(session.inverse.shared.nqsp <=
        static_cast<int>(core::qspecDistributionCapacity));
  // NSP (active length) is separate from the contour capacity IQX.
  CHECK_EQ(session.inverse.shared.nsp, 0);
  CHECK(session.paneling.panel.x.size() <= core::surfaceSystemCapacity);
}

// MDES entry without contour: no partial mutation (xmdes.f 53-57).
TEST(unit, InverseMdesEntryWithoutAirfoilLeavesStateUnchanged) {
  SessionComposition session;
  const auto result = session::enterMdesMenuState(session);
  CHECK(result.status == session::InverseMenuEntryStatus::noAirfoil);
  CHECK_FALSE(session.inverse.mdes.active);
  CHECK_FALSE(session.inverse.qdes.active);
  CHECK(session.inverse.shared.qspecs.empty());
  CHECK_EQ(session.inverse.shared.nqsp, 0);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::absent);
}

// MDES entry frame with contour: NSP.NE.NC1 pre-check,
// NQSP==0 pre-check, menu/redo (xmdes.f 68-92 without numerics).
TEST(unit, InverseMdesEntrySetsFrameAndQspecDefaults) {
  SessionComposition session;
  setupPanelAirfoil(session);
  const std::uint64_t panelRevisionBefore = session.paneling.panelRevision;
  const auto result = session::enterMdesMenuState(session);
  CHECK(result.status == session::InverseMenuEntryStatus::entered);
  CHECK(session.inverse.mdes.active);
  CHECK_FALSE(session.inverse.qdes.active);
  CHECK_FALSE(session.inverse.mdes.redo.has_value());
  // NSP(0) != NC1(257): LQSPEC=.FALSE. and IQ1/IQ2 full range.
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::absent);
  CHECK_EQ(session.inverse.shared.target.iq1, 0);
  CHECK_EQ(session.inverse.shared.target.iq2, 256);
  CHECK_FALSE(session.inverse.shared.target.liqset);
  // NQSP==0: IACQSP=1, NQSP=1, ALQSP(1)=ALFA (INIT-ALFA 0.0).
  CHECK_EQ(session.inverse.shared.nqsp, 1);
  CHECK_EQ(session.inverse.shared.iacqsp, 1);
  CHECK_EQ(session.inverse.shared.qspecs.size(), 1u);
  CHECK_NEAR(session.inverse.shared.qspecs[0].al, 0.0, 0.0, 0.0);
  CHECK(session.inverse.shared.qspecs[0].q.empty());  // QSPCIR has not produced a column yet
  // No geometry/panel/revision transition, no analysis invalidation.
  CHECK_EQ(session.paneling.panelRevision, panelRevisionBefore);
  CHECK(session.paneling.panelingValid);
}

// MDES entry preserves target-Q if NSP == NC1 (xmdes.f 68-72
// does not apply), and preserves NQSP (xmdes.f 79-83 does not apply).
TEST(unit, InverseMdesEntryKeepsQspecWhenNspMatchesNc1) {
  SessionComposition session;
  setupPanelAirfoil(session);
  auto& inv = session.inverse;
  inv.shared.nsp = inv.shared.nc1;
  inv.shared.nqsp = 2;
  inv.shared.qspecs.assign(2, core::inverse::QSpecDistribution{});
  inv.shared.target.iq1 = 10;
  inv.shared.target.iq2 = 200;
  inv.shared.target.liqset = true;
  markQspecPresent(session);
  const auto result = session::enterMdesMenuState(session);
  CHECK(result.status == session::InverseMenuEntryStatus::entered);
  CHECK(inv.validity.qspec == session::QSpecValidity::present);
  CHECK(inv.validity.qspecOrigin == session::QSpecOrigin::menuEdits);
  CHECK_EQ(inv.shared.nqsp, 2);
  CHECK_EQ(inv.shared.target.iq1, 10);
  CHECK_EQ(inv.shared.target.iq2, 200);
  CHECK(inv.shared.target.liqset);  // MDES entry does not delete LIQSET
}

// QDES entry without contour: no partial mutation (xqdes.f 52-56).
TEST(unit, InverseQdesEntryWithoutAirfoilLeavesStateUnchanged) {
  SessionComposition session;
  const auto result = session::enterQdesMenuState(session);
  CHECK(result.status == session::InverseMenuEntryStatus::noAirfoil);
  CHECK_FALSE(session.inverse.qdes.active);
  CHECK_FALSE(session.inverse.qdes.redo.has_value());
}

// QDES entry frame: NSP.NE.N -> LQSPEC=.FALSE., LIQSET=.FALSE.
// (xqdes.f 68-71); menu/redo set; SPECAL/Q capture remain.
TEST(unit, InverseQdesEntryClearsQspecAndMarkOnNspMismatch) {
  SessionComposition session;
  setupPanelAirfoil(session);
  auto& inv = session.inverse;
  inv.shared.nsp = 7;  // != current panel point count
  inv.shared.target.iq1 = 3;
  inv.shared.target.iq2 = 9;
  inv.shared.target.liqset = true;
  markQspecPresent(session);
  const auto result = session::enterQdesMenuState(session);
  CHECK(result.status == session::InverseMenuEntryStatus::entered);
  CHECK(session.inverse.qdes.active);
  CHECK_FALSE(inv.qdes.redo.has_value());
  CHECK(inv.validity.qspec == session::QSpecValidity::absent);
  CHECK(inv.validity.qspecOrigin == session::QSpecOrigin::none);
  CHECK_FALSE(inv.shared.target.liqset);
  CHECK_EQ(inv.shared.target.iq1, 3);  // Indices remain (Fortran does not
  CHECK_EQ(inv.shared.target.iq2, 9);  // reset them here)
  CHECK(inv.shared.nsp == 7);          // NSP=N is the numerical share
}

// QDES entry preserves state if NSP == N.
TEST(unit, InverseQdesEntryKeepsStateWhenNspMatchesPanel) {
  SessionComposition session;
  setupPanelAirfoil(session);
  auto& inv = session.inverse;
  inv.shared.nsp = static_cast<int>(session.paneling.panel.x.size());
  inv.shared.target.liqset = true;
  markQspecPresent(session);
  const auto result = session::enterQdesMenuState(session);
  CHECK(result.status == session::InverseMenuEntryStatus::entered);
  CHECK(inv.validity.qspec == session::QSpecValidity::present);
  CHECK(inv.shared.target.liqset);
}

// redo states strictly separate: MDES redo is preserved and restored
// across QDES entry; design/OPER redo stays untouched.
TEST(unit, InverseMenuRedoSeparateBetweenMenusAndOtherDomains) {
  SessionComposition session;
  setupPanelAirfoil(session);
  session.design.menu.gdesRedo = std::make_pair("SCAL", "2 2");
  (void)session::enterMdesMenuState(session);
  session.inverse.mdes.redo = std::make_pair("QSET", "");
  session::leaveMdesMenuState(session);
  CHECK_FALSE(session.inverse.mdes.active);
  CHECK(session.inverse.mdes.redo.has_value());  // SAVE-local value stays
  (void)session::enterQdesMenuState(session);
  CHECK_FALSE(session.inverse.qdes.redo.has_value());  // own menu
  CHECK(session.inverse.mdes.redo.has_value());
  CHECK(session.design.menu.gdesRedo.has_value());     // GDES redo is separate
  (void)session::enterMdesMenuState(session);
  CHECK_FALSE(session.inverse.mdes.redo.has_value());  // entry resets it
  CHECK(session.inverse.qdes.active);                  // QDES stays active
}

// exit resets the shared LQSYM in both menus
// (xmdes.f 143, xqdes.f 136); numerical data stay preserved.
TEST(unit, InverseExitResetsSharedSymmetryFlagBothMenus) {
  SessionComposition session;
  setupPanelAirfoil(session);
  (void)session::enterMdesMenuState(session);
  session.inverse.flags.lqSym = true;   // MDES SYMM (xmdes.f 292)
  session::leaveMdesMenuState(session);
  CHECK_FALSE(session.inverse.flags.lqSym);
  session.inverse.flags.lqSym = true;
  (void)session::enterQdesMenuState(session);
  session::leaveQdesMenuState(session);
  CHECK_FALSE(session.inverse.flags.lqSym);
  CHECK_FALSE(session.inverse.qdes.active);
}

// ABCOPY/PCOP: only the mapping binding is invalidated; target-Q and target segment
// remain (xgdes.f 1027).
TEST(unit, InverseAbcopyInvalidatesMappingOnly) {
  SessionComposition session;
  setupPanelAirfoil(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  session.inverse.shared.target.iq1 = 5;
  session.inverse.shared.target.iq2 = 100;
  session.inverse.shared.target.liqset = true;
  const std::uint64_t panelRevisionBefore = session.paneling.panelRevision;
  const auto status = session::copyBufferToCurrent(session);
  CHECK(status.ok);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(session.inverse.shared.target.liqset);
  CHECK_EQ(session.inverse.shared.target.iq1, 5);
  CHECK_EQ(session.paneling.panelRevision, panelRevisionBefore + 1);
  CHECK(session.inverse.provenance.sourcePanelRevision !=
        session.paneling.panelRevision);  // Bound to the old contour
}

// PANGEN (PANE/PPAR apply): mapping invalidated and LQSPEC=.FALSE.
// (xfoil.f 2087-2088); menu/redo states remain.
TEST(unit, InversePangenInvalidatesMappingAndQspec) {
  SessionComposition session;
  setupPanelAirfoil(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  (void)session::enterMdesMenuState(session);
  session.inverse.mdes.redo = std::make_pair("SMOO", "");
  const std::uint64_t panelRevisionBefore = session.paneling.panelRevision;
  const auto paneled = session::applyPaneling(session);
  CHECK(paneled.ok);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::absent);
  CHECK(session.inverse.validity.qspecOrigin == session::QSpecOrigin::none);
  CHECK(session.inverse.mdes.active);            // menu stays active
  CHECK(session.inverse.mdes.redo.has_value());  // redo stays
  CHECK_EQ(session.paneling.panelRevision, panelRevisionBefore + 1);
}

// pure buffer change without EXEC (GDES command): inverse state
// stays fully preserved (current contour unchanged).
TEST(unit, InverseBufferOnlyChangeKeepsNumericState) {
  SessionComposition session;
  setupPanelAirfoil(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  const std::uint64_t panelRevisionBefore = session.paneling.panelRevision;
  const std::uint64_t bufferRevisionBefore = session.geometry.bufferRevision;
  (void)session::initializeGeometryDesign(session);
  const auto scaled = session::gdesScale(session, 2.0, 2.0);
  CHECK(scaled.ok);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::initializedFromPanel);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK_EQ(session.paneling.panelRevision, panelRevisionBefore);
  CHECK(session.geometry.bufferRevision > bufferRevisionBefore);
}

// GDES EXEC (ABCOPY commit): invalidation as with ABCOPY.
TEST(unit, InverseGdesExecInvalidatesLikeAbcopy) {
  SessionComposition session;
  setupPanelAirfoil(session);
  (void)session::initializeGeometryDesign(session);
  const auto scaled = session::gdesScale(session, 2.0, 2.0);
  CHECK(scaled.ok);
  markMappingInitialized(session);
  markQspecPresent(session);
  const auto exec = session::executeGeometryDesignBuffer(session);
  CHECK(exec.ok);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
}

// buffer adoption without a new contour (setBufferGeometry): binding
// invalidated, LQSPEC flag semantics stay (Fortran LOAD).
TEST(unit, InverseSetBufferGeometryInvalidatesBindingKeepsQspec) {
  SessionComposition session;
  setupPanelAirfoil(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  core::geometry::AirfoilBuffer buffer{{0.0, 1.0}, {0.0, 0.1}, "TEST"};
  const auto set = session::setBufferGeometry(session, std::move(buffer));
  CHECK(set.success());
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(session.inverse.validity.qspecOrigin ==
        session::QSpecOrigin::menuEdits);
}

// QDES EXEC invalidation contract (xqdes.f 499): mapping
// invalidated, target-Q stays (EXEC consumes it), menu stays.
TEST(unit, InverseMixedInverseExecInvalidatesMappingOnly) {
  SessionComposition session;
  setupPanelAirfoil(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  session.inverse.shared.nsp =
      static_cast<int>(session.paneling.panel.x.size());
  (void)session::enterQdesMenuState(session);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
  session::invalidateInverseStateAfterPanelChange(
      session, session::InversePanelChange::mixedInverseExec);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(session.inverse.qdes.active);
}

// APCOPY invalidation contract (xpol.f 172): mapping invalidated,
// target-Q stays.
TEST(unit, InverseApcopyKindInvalidatesMappingOnly) {
  SessionComposition session;
  setupPanelAirfoil(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  session::invalidateInverseStateAfterPanelChange(
      session, session::InversePanelChange::polarArchivedCopy);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session.inverse.validity.qspec == session::QSpecValidity::present);
}

// never initialized stays never initialized (no upgrade on
// "outdated" without a previous binding).
TEST(unit, InverseInvalidationNeverUpgradesNeverInitialized) {
  SessionComposition session;
  setupPanelAirfoil(session);
  session::invalidateInverseStateAfterPanelChange(
      session, session::InversePanelChange::bufferToCurrent);
  CHECK(session.inverse.validity.mapping ==
        session::MappingCoordinateValidity::neverInitialized);
}

// session reset: resetInverseDesignState provides the INIT defaults
// again, without changing other part states.
TEST(unit, InverseResetRestoresInitDefaultsOnlyForInverseState) {
  SessionComposition session;
  setupPanelAirfoil(session);
  const std::uint64_t panelRevisionBefore = session.paneling.panelRevision;
  (void)session::enterMdesMenuState(session);
  markMappingInitialized(session);
  markQspecPresent(session);
  session.inverse.flags.lqVdes = true;
  session.inverse.mdes.redo = std::make_pair("AQ", "5 10");
  session::resetInverseDesignState(session);
  const auto& inv = session.inverse;
  CHECK_FALSE(inv.mdes.active);
  CHECK_FALSE(inv.mdes.redo.has_value());
  CHECK(inv.validity.grid == session::CircleGridValidity::notInitialized);
  CHECK(inv.validity.mapping ==
        session::MappingCoordinateValidity::neverInitialized);
  CHECK(inv.validity.qspec == session::QSpecValidity::absent);
  CHECK_FALSE(inv.flags.lqVdes);
  CHECK(inv.flags.lqSlop);
  CHECK_EQ(inv.shared.nc1, 257);
  CHECK_EQ(inv.provenance.sourcePanelRevision, 0u);
  // other part states unchanged.
  CHECK(session.paneling.panelingValid);
  CHECK_EQ(session.paneling.panelRevision, panelRevisionBefore);
}

// panel revision is monotonic across all panel producers and does not
// increase for pure buffer changes.
TEST(unit, InversePanelRevisionMonotonicAcrossPanelProducers) {
  SessionComposition session;
  std::ostringstream out;
  cli::handleNacaCore(session, out, 2412);
  const std::uint64_t afterNaca = session.paneling.panelRevision;
  CHECK(afterNaca >= 2);  // setBufferGeometry + applyPaneling
  (void)session::copyBufferToCurrent(session);
  CHECK_EQ(session.paneling.panelRevision, afterNaca + 1);
  (void)session::applyPaneling(session);
  CHECK_EQ(session.paneling.panelRevision, afterNaca + 2);
  (void)session::initializeGeometryDesign(session);
  const auto moved = session::gdesTranslate(session, 1.0, 0.0);
  CHECK(moved.ok);
  CHECK_EQ(session.paneling.panelRevision, afterNaca + 2);  // buffer-only
}

// shared numerical data exactly once: both menu entries
// use the same column list; the QDES entry does not lay down a second
// copy of the target Q.
TEST(unit, InverseSharedNumericStoredOnceAcrossMenus) {
  SessionComposition session;
  setupPanelAirfoil(session);
  (void)session::enterMdesMenuState(session);
  CHECK_EQ(session.inverse.shared.qspecs.size(), 1u);
  session.inverse.shared.nsp =
      static_cast<int>(session.paneling.panel.x.size());
  (void)session::enterQdesMenuState(session);
  CHECK_EQ(session.inverse.shared.qspecs.size(), 1u);  // no copy
  CHECK(session.inverse.mdes.active);                  // both menus
  CHECK(session.inverse.qdes.active);                  // divide the same
  CHECK(&session.inverse.mdes.redo != &session.inverse.qdes.redo);
}

// entry/exit operations change no API/analysis contracts.
TEST(unit, InverseLifecycleOperationsDoNotTouchAnalysisApi) {
  SessionComposition session;
  setupPanelAirfoil(session);
  const auto statusBefore = session.result.lastResult.status;
  const int iterationsBefore = session.config.maximumIterations;
  (void)session::enterMdesMenuState(session);
  session::leaveMdesMenuState(session);
  (void)session::enterQdesMenuState(session);
  session::leaveQdesMenuState(session);
  session::invalidateInverseStateAfterPanelChange(
      session, session::InversePanelChange::bufferToCurrent);
  session::resetInverseDesignState(session);
  CHECK(session.result.lastResult.status == statusBefore);
  CHECK_EQ(session.config.maximumIterations, iterationsBefore);
  CHECK(session.paneling.panelingValid);
}

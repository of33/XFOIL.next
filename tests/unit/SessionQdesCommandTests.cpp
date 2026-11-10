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

// File: tests/unit/SessionQdesCommandTests.cpp
// tests the non-committing QDES session
// use cases and the shared MDES/QDES state. Fortran anchor:
// canonical CLI oracle (LC_ALL=C fortran-bin/xfoil, three times
// byte-identical). No CLI dispatch, no
// numerics recomputed in the test, no manual state repair.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreInverseDesignHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <sstream>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

using session::SessionComposition;

constexpr double kF6Abs = 5.0e-7 + 1.0e-12;

void setupPangenAirfoil(SessionComposition& s, int designation) {
  core::paneling::PanelingParameters params = session::panelingParameters(s);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(s, params).ok);
  std::ostringstream out;
  cli::handleNacaCore(s, out, designation);
  CHECK(s.paneling.panelingValid);
}

void configureOper(SessionComposition& s) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(s, config).success());
}

void enterMdes(SessionComposition& s) {
  const auto entry = session::enterMdesMenuState(s);
  CHECK(entry.status == session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeMdesNumerics(s).isOk());
}

}  // namespace

// QDES entry without previous MDES: SPECAL runs inviscid (no
// VISCAL), Q capture, GAMQSP(1) with the 1155 path. Anchor: CLGAM
// 0.255377 / CMGAM -0.055736 (FORMAT 1150 CL 0.2554).
TEST(unit, QdesEntrySpecalInviscidAnchors) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  const auto init = session::initializeQdesNumerics(s);
  CHECK(init.isOk());
  CHECK(init.qspecRebuilt);
  CHECK_NEAR(s.inverse.shared.clgam, 0.255377, kF6Abs, kF6Abs);
  CHECK_NEAR(s.inverse.shared.cmgam, -0.055736, kF6Abs, kF6Abs);
  CHECK_NEAR(s.inverse.shared.algam, 0.0, 0.0, 0.0);
  CHECK_EQ(s.inverse.shared.nsp, 160);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::currentQ);
  // SPECAL without VISCAL: no BL point run, configuration unchanged.
  CHECK(!s.result.lastDiagnostics.viscalRan);
  CHECK(s.operating.viscous);
  // No panel/buffer mutation.
  CHECK(s.paneling.panelingValid);
}

// QDES-entry without valid panel contour: noAirfoil without mutation.
TEST(unit, QdesEntryNoAirfoil) {
  SessionComposition s;
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::noAirfoil);
  const auto init = session::initializeQdesNumerics(s);
  CHECK(init.status == session::QdesNumericsInitStatus::noAirfoil);
  CHECK(s.inverse.shared.qspecs.empty());
}

// MDES -> QDES: NSP(=NC1) unequal N -> LQSPEC/LIQSET reset,
// GAMQSP rebuild (canonical 1155 path of the q4 oracle).
TEST(unit, QdesEntryAfterMdesRebuildsQspec) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  CHECK_EQ(s.inverse.shared.nsp, 257);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  const auto init = session::initializeQdesNumerics(s);
  CHECK(init.isOk());
  CHECK(init.qspecRebuilt);
  CHECK_EQ(s.inverse.shared.nsp, 160);
  CHECK_NEAR(s.inverse.shared.clgam, 0.255377, kF6Abs, kF6Abs);
}

// SMOO without MARK (canonical: IQ1/IQ2 = full range) against the
// 1200 oracle: Qspec CL 0.255630 CM -0.055642.
TEST(unit, QdesSmooFullRangeOracle) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
  CHECK(!s.inverse.shared.target.liqset);
  session::QdesForceReport r;
  CHECK(session::qdesSmoothTargetSegment(s, r) ==
        session::QdesCommandStatus::ok);
  CHECK_NEAR(r.cl, 0.255377, kF6Abs, kF6Abs);
  CHECK_NEAR(r.cm, -0.055736, kF6Abs, kF6Abs);
  CHECK_NEAR(r.clq, 0.255630, kF6Abs, kF6Abs);
  CHECK_NEAR(r.cmq, -0.055642, kF6Abs, kF6Abs);
  CHECK_NEAR(s.inverse.shared.qspecs[0].cl, 0.255630, kF6Abs, kF6Abs);
  CHECK_NEAR(s.inverse.shared.qspecs[0].cm, -0.055642, kF6Abs, kF6Abs);
  // No geometry/panel/analysis mutation.
  CHECK(s.paneling.panelingValid);
  CHECK(!s.result.lastDiagnostics.viscalRan);
}

// redo path of the '!' oracle: the second SMOO returns
// Qspec CL 0.255836 CM -0.055559.
TEST(unit, QdesSmooTwiceOracle) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
  session::QdesForceReport r;
  CHECK(session::qdesSmoothTargetSegment(s, r) ==
        session::QdesCommandStatus::ok);
  CHECK(session::qdesSmoothTargetSegment(s, r) ==
        session::QdesCommandStatus::ok);
  CHECK_NEAR(r.clq, 0.255836, kF6Abs, kF6Abs);
  CHECK_NEAR(r.cmq, -0.055559, kF6Abs, kF6Abs);
}

// SMOO with an already set target segment (direct session test, no
// invented CLI segment grammar): smoothing stays restricted to the
// marked range.
TEST(unit, QdesSmooPreparedTargetSegment) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
  const auto before = s.inverse.shared.qspecs[0].q;
  s.inverse.shared.target.liqset = true;
  s.inverse.shared.target.iq1 = 40;
  s.inverse.shared.target.iq2 = 60;
  session::QdesForceReport r;
  CHECK(session::qdesSmoothTargetSegment(s, r) ==
        session::QdesCommandStatus::ok);
  bool changedInside = false;
  for (int i = 40; i <= 60; ++i) {
    if (s.inverse.shared.qspecs[0].q[static_cast<std::size_t>(i)] !=
        before[static_cast<std::size_t>(i)]) {
      changedInside = true;
    }
  }
  CHECK(changedInside);
  CHECK_NEAR(s.inverse.shared.qspecs[0].q[0], before[0], 0.0, 0.0);
  CHECK_NEAR(s.inverse.shared.qspecs[0].q[159], before[159], 0.0, 0.0);
}

// QSET restores Qspec = current Q (QGAMM); QDOF and spline
// values are reset (SPLQSP kernel).
TEST(unit, QdesQsetRestoresCurrentQ) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
  session::QdesForceReport r;
  CHECK(session::qdesSmoothTargetSegment(s, r) ==
        session::QdesCommandStatus::ok);
  CHECK(session::qdesQspecReset(s) == session::QdesCommandStatus::ok);
  const auto& q = s.inverse.shared.qspecs[0].q;
  const auto& qg = s.inverse.shared.qgamm;
  CHECK_EQ(q.size(), qg.size());
  for (std::size_t i = 0; i < q.size(); ++i) {
    CHECK_NEAR(q[i], qg[i], 0.0, 0.0);
  }
  CHECK_NEAR(s.inverse.shared.qspecs[0].cl, s.inverse.shared.clgam, 0.0,
             0.0);
  CHECK_NEAR(s.inverse.shared.qspecs[0].cm, s.inverse.shared.cmgam, 0.0,
             0.0);
}

// QSET without inverse state: notInitialized without mutation.
TEST(unit, QdesQsetNotInitialized) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::qdesQspecReset(s) ==
        session::QdesCommandStatus::notInitialized);
  session::QdesForceReport report;
  CHECK(session::qdesSmoothTargetSegment(s, report) !=
        session::QdesCommandStatus::ok);
}

// VISC/REFL/SLOP/CPXX: pure flag toggles on the shared
// inverse session state; no analysis, no geometry.
TEST(unit, QdesFlagTogglesSharedState) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
  const auto geomBefore = s.paneling.panel.x;
  CHECK(session::mdesToggleQvisOverlay(s));
  CHECK(!session::mdesToggleQvisOverlay(s));
  CHECK(session::mdesToggleReflOverlay(s));
  CHECK(!session::mdesToggleReflOverlay(s));
  CHECK(!session::mdesToggleSlopeMatch(s));  // LQSLOP default true
  CHECK(session::mdesToggleSlopeMatch(s));
  CHECK(session::qdesToggleCpxxConstraint(s));
  CHECK(!session::qdesToggleCpxxConstraint(s));
  CHECK_EQ(s.paneling.panel.x.size(), geomBefore.size());
  for (std::size_t i = 0; i < geomBefore.size(); ++i) {
    CHECK_NEAR(s.paneling.panel.x[i], geomBefore[i], 0.0, 0.0);
  }
  CHECK(!s.result.lastDiagnostics.viscalRan);
}

// REST is a geometry commit (xqdes.f 508-530) and stays
// outside the session use cases, with no mutation. The
// QDES handler does not expose REST.
TEST(unit, QdesRestNotExecutedNoMutation) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("REST\n\n");
  std::ostringstream out;
  CHECK(cli::handleQdesCore(s, in, out) == cli::QdesCoreStatus::returned);
  CHECK(s.paneling.panelingValid);
  CHECK(!s.inverse.shared.target.liqset);
}

// shared state: QDES QSET/SMOO acts on the same
// QSPEC store that the MDES menu sees; redo states remain
// separate; LQSYM is false when either menu is left.
TEST(unit, QdesMdesSharedQspecAndSeparateRedo) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  CHECK(session::enterQdesMenuState(s).status ==
        session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
  session::QdesForceReport r;
  CHECK(session::qdesSmoothTargetSegment(s, r) ==
        session::QdesCommandStatus::ok);
  const double clq = s.inverse.shared.qspecs[0].cl;
  session::leaveQdesMenuState(s);
  CHECK(!s.inverse.flags.lqSym);
  // MDES sees the same column 1 (shared store, no copy).
  CHECK_NEAR(s.inverse.shared.qspecs[0].cl, clq, 0.0, 0.0);
  CHECK(!s.inverse.qdes.redo.has_value());
  CHECK(!s.inverse.mdes.redo.has_value());
}

// Independent sessions: identical inputs deliver identical
// final states.
TEST(unit, QdesIndependentSessionsIdentical) {
  auto run = [] {
    SessionComposition s;
    setupPangenAirfoil(s, 2412);
    configureOper(s);
    std::istringstream in("SMOO\nQSET\nSMOO\n\n");
    std::ostringstream out;
    (void)cli::handleQdesCore(s, in, out);
    return std::make_pair(out.str(), s.inverse.shared.qspecs[0].cl);
  };
  const auto a = run();
  const auto b = run();
  CHECK(a.first == b.first);
  CHECK_NEAR(a.second, b.second, 0.0, 0.0);
}

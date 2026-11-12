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

// File: tests/unit/SessionCommitTests.cpp
// tests the four separated commit contracts (MDES EXEC,
// MDES PERT+EXEC, QDES EXEC/MIXED, QDES REST) at session level.
// MDES EXEC/PERT text and CN anchors derive from canonical
// CLI oracles (LC_ALL=C fortran-bin/xfoil, three times byte-identical).
// the successful QDES EXEC path is cursor-bound (MARK) and not reachable
// via the CLI; it is checked as a direct session test with a prepared
// target segment (no segment grammar); the MIXED kernel itself
// is already anchored close to the Fortran bits (mixed_reference.txt).
// The test calls the session operations directly.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreInverseDesignHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace session = xfoil::session;
using session::SessionComposition;

void setupPangenAirfoil(SessionComposition& s, int designation) {
  core::paneling::PanelingParameters params = session::panelingParameters(s);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(s, params).ok);
  std::ostringstream out;
  testing::handleLegacyNacaCore(s, out, designation);
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

void enterQdes(SessionComposition& s) {
  const auto entry = session::enterQdesMenuState(s);
  CHECK(entry.status == session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeQdesNumerics(s).isOk());
}

}  // namespace

// MDES EXEC: MAPGEN commit into the buffer; panel contour and analysis
// remain unchanged; NB = NC = 257; LGSAME=false.
TEST(unit, MdesExecCommitsBufferOnly) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  const auto panelBefore = s.paneling.panel.x;
  const auto bufferBefore = s.geometry.buffer.x;
  const std::uint64_t revBefore = s.geometry.bufferRevision;
  const auto cnBefore = s.inverse.shared.circle.coeff.cn;
  const auto res = session::executeMdesDesign(s);
  CHECK(res.isOk());
  CHECK_EQ(res.bufferPointCount, 257u);
  CHECK_EQ(s.geometry.buffer.x.size(), 257u);
  CHECK_EQ(s.geometry.bufferRevision, revBefore + 1);
  CHECK(s.geometry.buffer.x != bufferBefore);
  // panel contour canonically unchanged (the PANE text indicates PANE).
  CHECK_EQ(s.paneling.panel.x.size(), panelBefore.size());
  for (std::size_t i = 0; i < panelBefore.size(); ++i) {
    CHECK_NEAR(s.paneling.panel.x[i], panelBefore[i], 0.0, 0.0);
  }
  CHECK(!s.design.lgSame);
  // MAPGEN mutates the shared circle state (CN(0) rotation offset).
  CHECK(s.inverse.shared.circle.coeff.cn != cnBefore);
}

// MDES EXEC without initialization: notInitialized without mutation.
TEST(unit, MdesExecNotInitialized) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  const auto panelBefore = s.paneling.panel.x;
  const auto res = session::executeMdesDesign(s);
  CHECK(res.status == session::MdesExecuteStatus::notInitialized);
  CHECK_EQ(s.paneling.panel.x.size(), panelBefore.size());
  CHECK_NEAR(s.paneling.panel.x[0], panelBefore[0], 0.0, 0.0);
}

// MDES PERT+EXEC: perturbation + forced EXEC (buffer commit).
// CN(1) = 0.910192.../-0.000054... (canonical PERT-table).
TEST(unit, MdesPertThenExec) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  const auto& cn = s.inverse.shared.circle.coeff.cn;
  CHECK_NEAR(cn[1].real(), 0.910192, 1.0e-6, 1.0e-6);
  CHECK_NEAR(cn[1].imag(), -0.000054, 1.0e-6, 1.0e-6);
  CHECK(session::mdesPerturbPrepareCoefficients(s).ok);
  const auto res = session::mdesPerturbCoefficientAndExecute(s, 1, 0.001, 0.0);
  CHECK(res.status == session::MdesPerturbStatus::ok);
  CHECK(res.coefficientPerturbed);
  CHECK(res.execute.isOk());
  CHECK_EQ(s.geometry.buffer.x.size(), 257u);
  // canonical: the TE-gap Newton (NCN=1) absorbs the CN(1)
  // perturbation completely; the buffer commit is decisive.
  CHECK(s.geometry.bufferRevision > 0);
  // PERT-specific Newton (xmdes.f PERT 1713-1745): two iterations,
  // convergence on the second pass (reference value class, absolute difference
  // to the canonical Fortran output ~2e-16).
  CHECK_EQ(res.newton.iterations.size(), 2u);
  CHECK(res.newton.converged);
  if (res.newton.iterations.size() == 2) {
    CHECK_EQ(res.newton.iterations[0].iteration, 1);
    CHECK_EQ(res.newton.iterations[1].iteration, 2);
    CHECK_NEAR(res.newton.iterations[0].dcnMax, 1.0017816811997685e-3,
               1.0e-12, 1.0e-12);
    CHECK(res.newton.iterations[1].dcnMax <= 5.0e-5);
  }
}

// Double PERT: xmdes.f 702-708 writes QSPCIR after every PERT
// (target-Q columns from the changed CN state) before the
// automatic EXEC. Without QSPCIR the second PERT table would be
// reproduced around ~1e-6 wrong (repeated MDES PERT redo
// amplification pass). Pin: QSPEC change after the first PERT,
// second Newton sequence with canonical initial value, CN table
// itself changes vs. the first table.
TEST(unit, MdesDoublePertRunsQspcir) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  CHECK(session::mdesPerturbPrepareCoefficients(s).ok);
  const auto cnFirst = s.inverse.shared.circle.coeff.cn[1];
  const auto qBefore = s.inverse.shared.qspecs[0].q;
  const auto r1 = session::mdesPerturbCoefficientAndExecute(s, 1, 0.001, 0.0);
  CHECK(r1.status == session::MdesPerturbStatus::ok);
  const auto& qAfter = s.inverse.shared.qspecs[0].q;
  bool qChanged = false;
  for (std::size_t i = 0; i < qBefore.size(); ++i) {
    if (qBefore[i] != qAfter[i]) {
      qChanged = true;
      break;
    }
  }
  CHECK(qChanged);  // QSPCIR updated the target-Q
  CHECK(session::mdesPerturbPrepareCoefficients(s).ok);
  const auto cnSecond = s.inverse.shared.circle.coeff.cn[1];
  CHECK(cnSecond != cnFirst);  // CNCALC via the updated QSPEC
  const auto r2 = session::mdesPerturbCoefficientAndExecute(s, 1, 0.001, 0.0);
  CHECK(r2.status == session::MdesPerturbStatus::ok);
  CHECK_EQ(r2.newton.iterations.size(), 2u);
  if (r2.newton.iterations.size() == 2) {
    CHECK_NEAR(r2.newton.iterations[0].dcnMax, 1.0011505098921308e-3,
               1.0e-12, 1.0e-12);
    CHECK(r2.newton.iterations[1].dcnMax <= 5.0e-5);
  }
  CHECK(r2.execute.isOk());
  CHECK_EQ(s.geometry.buffer.x.size(), 257u);
}

// PERT with domain non-convergence after 10 iterations
// (xmdes.f PERT 1741-1743): exactly 10 iteration records,
// non-convergence status, partial mutation remains, EXEC commit still
// nevertheless (Fortran runs after the warning text in the same EXEC path).
TEST(unit, MdesPertNonConvergenceStillCommits) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  CHECK(session::mdesPerturbPrepareCoefficients(s).ok);
  const auto res = session::mdesPerturbCoefficientAndExecute(s, 1, 50.0, 0.0);
  CHECK_EQ(res.newton.iterations.size(), 10u);
  CHECK(!res.newton.converged);
  CHECK(res.coefficientPerturbed);
  CHECK(res.execute.isOk());
  CHECK_EQ(s.geometry.buffer.x.size(), 257u);
  CHECK(s.geometry.bufferRevision > 0);
}

// PERT with invalid index: invalidIndex without any mutation.
TEST(unit, MdesPertInvalidIndexNoMutation) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  CHECK(session::mdesPerturbPrepareCoefficients(s).ok);
  const auto cnBefore = s.inverse.shared.circle.coeff.cn;
  const std::uint64_t revBefore = s.geometry.bufferRevision;
  CHECK(session::mdesPerturbCoefficientAndExecute(s, 0, 1.0, 0.0).status ==
        session::MdesPerturbStatus::invalidIndex);
  CHECK(session::mdesPerturbCoefficientAndExecute(s, 300, 1.0, 0.0).status ==
        session::MdesPerturbStatus::invalidIndex);
  CHECK(s.inverse.shared.circle.coeff.cn == cnBefore);
  CHECK_EQ(s.geometry.bufferRevision, revBefore);
}

// QDES EXEC without LIQSET: canonical refusal without any mutation.
TEST(unit, QdesExecGuardNoMutation) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterQdes(s);
  const auto panelBefore = s.paneling.panel.x;
  const auto bufferBefore = s.geometry.buffer.x;
  const std::uint64_t prevRev = s.geometry.bufferRevision;
  const std::uint64_t penRev = s.paneling.panelRevision;
  const auto res = session::executeQdesMixedDesign(s, 1);
  CHECK(res.status == session::QdesExecuteStatus::targetNotMarked);
  CHECK_EQ(s.paneling.panel.x.size(), panelBefore.size());
  CHECK_NEAR(s.paneling.panel.x[0], panelBefore[0], 0.0, 0.0);
  CHECK_EQ(s.geometry.buffer.x.size(), bufferBefore.size());
  CHECK_EQ(s.geometry.bufferRevision, prevRev);
  CHECK_EQ(s.paneling.panelRevision, penRev);
}

// QDES EXEC with a prepared target segment (direct session test):
// MIXED commit of the current contour, buffer = old contour, revisions,
// LSCINI stale, LQSPEC/LIQSET preserved, COMMON fields persisted.
TEST(unit, QdesExecPreparedSegmentCommit) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterQdes(s);
  const auto panelBefore = s.paneling.panel.x;
  const std::uint64_t bufRevBefore = s.geometry.bufferRevision;
  const std::uint64_t penRevBefore = s.paneling.panelRevision;
  // Choose the target segment beyond the stagnation point (canonical
  // stagnation point check xqdes.f 416-427).
  int stag = -1;
  for (int i = 0; i + 1 < 160; ++i) {
    if (s.inverse.shared.qgamm[static_cast<std::size_t>(i)] >= 0.0 &&
        s.inverse.shared.qgamm[static_cast<std::size_t>(i + 1)] < 0.0) {
      stag = i;
      break;
    }
  }
  CHECK(stag > 0);
  s.inverse.shared.target.liqset = true;
  s.inverse.shared.target.iq1 = stag + 2;
  s.inverse.shared.target.iq2 = stag + 42;
  const auto res = session::executeQdesMixedDesign(s, 1);
  if (!res.isOk()) {
    std::cerr << "QDES EXEC status=" << static_cast<int>(res.status)
              << " msg=" << res.message << "\n";
  }
  CHECK(res.isOk());
  CHECK_EQ(res.panelPointCount, 160u);
  // current contour changed (within the segments).
  bool changed = false;
  for (std::size_t i = 0; i < panelBefore.size(); ++i) {
    if (s.paneling.panel.x[i] != panelBefore[i]) {
      changed = true;
      break;
    }
  }
  CHECK(changed);
  // buffer = saved old contour (NB = N).
  CHECK_EQ(s.geometry.buffer.x.size(), 160u);
  for (std::size_t i = 0; i < panelBefore.size(); ++i) {
    CHECK_NEAR(s.geometry.buffer.x[i], panelBefore[i], 0.0, 0.0);
  }
  CHECK_EQ(s.geometry.bufferRevision, bufRevBefore + 1);
  CHECK_EQ(s.paneling.panelRevision, penRevBefore + 1);
  CHECK(s.paneling.panelSourceRevision == s.geometry.bufferRevision);
  // Invalidations: system/wake/BL new, LBLINI stays, LSCINI stale.
  CHECK(!s.viscous.systemValid);
  CHECK(!s.viscous.wakeValid);
  CHECK(s.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  // LQSPEC/LIQSET/IQ1/IQ2 are retained.
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(s.inverse.shared.target.liqset);
  CHECK_EQ(s.inverse.shared.target.iq1, stag + 2);
  CHECK_EQ(s.inverse.shared.target.iq2, stag + 42);
  // new inviscid state + COMMON persistence.
  CHECK_EQ(s.viscous.viscals.gam.size(), 160u);
  CHECK_EQ(s.inverse.shared.nsp, 160);
  CHECK(!s.design.lgSame);
}

// QDES EXEC with a target segment that contains the stagnation point:
// the canonical stagnation point check (xqdes.f 416-427) rejects with
// stagnationInTarget, without mutating contour, buffer or revisions
// (counterpart to
// integration.ExecQdesRejectsSegmentContainingStagnationPoint).
TEST(unit, QdesExecRejectsSegmentContainingStagnation) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterQdes(s);
  const auto panelBefore = s.paneling.panel.x;
  const auto bufferBefore = s.geometry.buffer.x;
  const std::uint64_t bufRevBefore = s.geometry.bufferRevision;
  const std::uint64_t penRevBefore = s.paneling.panelRevision;
  int stag = -1;
  for (int i = 0; i + 1 < 160; ++i) {
    if (s.inverse.shared.qgamm[static_cast<std::size_t>(i)] >= 0.0 &&
        s.inverse.shared.qgamm[static_cast<std::size_t>(i + 1)] < 0.0) {
      stag = i;
      break;
    }
  }
  CHECK(stag > 0);
  s.inverse.shared.target.liqset = true;
  s.inverse.shared.target.iq1 = stag - 2;
  s.inverse.shared.target.iq2 = stag + 4;
  const auto res = session::executeQdesMixedDesign(s, 1);
  CHECK(res.status == session::QdesExecuteStatus::stagnationInTarget);
  CHECK_EQ(s.paneling.panel.x.size(), panelBefore.size());
  for (std::size_t i = 0; i < panelBefore.size(); ++i) {
    CHECK_NEAR(s.paneling.panel.x[i], panelBefore[i], 0.0, 0.0);
  }
  CHECK_EQ(s.geometry.buffer.x.size(), bufferBefore.size());
  CHECK_EQ(s.geometry.bufferRevision, bufRevBefore);
  CHECK_EQ(s.paneling.panelRevision, penRevBefore);
}

// QDES EXEC -> REST: the old contour is exactly restored;
// LSCINI stays unchanged (REST does not reset it).
TEST(unit, QdesExecThenRest) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterQdes(s);
  const auto panelBefore = s.paneling.panel.x;
  int stag = -1;
  for (int i = 0; i + 1 < 160; ++i) {
    if (s.inverse.shared.qgamm[static_cast<std::size_t>(i)] >= 0.0 &&
        s.inverse.shared.qgamm[static_cast<std::size_t>(i + 1)] < 0.0) {
      stag = i;
      break;
    }
  }
  CHECK(stag > 0);
  s.inverse.shared.target.liqset = true;
  s.inverse.shared.target.iq1 = stag + 2;
  s.inverse.shared.target.iq2 = stag + 42;
  CHECK(session::executeQdesMixedDesign(s, 1).isOk());
  const auto mappingAfterExec = s.inverse.validity.mapping;
  const auto res = session::restoreQdesCurrentFromBuffer(s);
  CHECK(res.isOk());
  CHECK_EQ(res.pointCount, 160u);
  for (std::size_t i = 0; i < panelBefore.size(); ++i) {
    CHECK_NEAR(s.paneling.panel.x[i], panelBefore[i], 1.0e-12, 1.0e-12);
  }
  CHECK(s.design.lgSame);
  // LSCINI/LQSPEC/LIQSET unchanged.
  CHECK(s.inverse.validity.mapping == mappingAfterExec);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(s.inverse.shared.target.liqset);
  // wake/BL flags remain canonically preserved (only LGAMU/LQINU new).
}

// repeated REST is idempotent.
TEST(unit, QdesRestRepeatedIdempotent) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterQdes(s);
  int stag = -1;
  for (int i = 0; i + 1 < 160; ++i) {
    if (s.inverse.shared.qgamm[static_cast<std::size_t>(i)] >= 0.0 &&
        s.inverse.shared.qgamm[static_cast<std::size_t>(i + 1)] < 0.0) {
      stag = i;
      break;
    }
  }
  CHECK(stag > 0);
  s.inverse.shared.target.liqset = true;
  s.inverse.shared.target.iq1 = stag + 2;
  s.inverse.shared.target.iq2 = stag + 42;
  CHECK(session::executeQdesMixedDesign(s, 1).isOk());
  CHECK(session::restoreQdesCurrentFromBuffer(s).isOk());
  const auto panelAfterFirst = s.paneling.panel.x;
  CHECK(session::restoreQdesCurrentFromBuffer(s).isOk());
  for (std::size_t i = 0; i < panelAfterFirst.size(); ++i) {
    CHECK_NEAR(s.paneling.panel.x[i], panelAfterFirst[i], 0.0, 0.0);
  }
}

// REST with a buffer smaller than the contour: structured error without
// mutation (Fortran would read outdated XB values).
TEST(unit, QdesRestBufferTooSmall) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterQdes(s);
  // buffer with fewer points than the contour (NB < N): structured
  // error. setBufferGeometry canonically invalidates the panel contour;
  // REST still cannot construct a contour from stale values.
  core::geometry::AirfoilBuffer tiny({0.1, 0.2, 0.3}, {0.1, 0.2, 0.3}, "t");
  CHECK(session::setBufferGeometry(s, tiny).success());
  const auto res = session::restoreQdesCurrentFromBuffer(s);
  CHECK(res.status == session::QdesRestoreStatus::bufferTooSmall);
  CHECK(!s.paneling.panelingValid || s.paneling.panel.x.size() != 3u);
}

// Independent sessions with a prepared segment: identical
// commit geometry.
TEST(unit, QdesExecIndependentSessionsIdentical) {
  auto run = [] {
    SessionComposition s;
    setupPangenAirfoil(s, 2412);
    configureOper(s);
    enterQdes(s);
    int stag = -1;
    for (int i = 0; i + 1 < 160; ++i) {
      if (s.inverse.shared.qgamm[static_cast<std::size_t>(i)] >= 0.0 &&
          s.inverse.shared.qgamm[static_cast<std::size_t>(i + 1)] < 0.0) {
        stag = i;
        break;
      }
    }
    s.inverse.shared.target.liqset = true;
    s.inverse.shared.target.iq1 = stag + 2;
    s.inverse.shared.target.iq2 = stag + 42;
    (void)session::executeQdesMixedDesign(s, 1);
    return s.paneling.panel.x;
  };
  const auto a = run();
  const auto b = run();
  const auto c = run();
  CHECK_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    CHECK_NEAR(a[i], b[i], 0.0, 0.0);
    CHECK_NEAR(b[i], c[i], 0.0, 0.0);
  }
}

// diagnostics observability (structured records).

// SCINIT diagnostics: TE-gap values + IPASS/max(dw) at the canonical
// execution time; a repeated entry (cache hit) without
// new records.
TEST(unit, ScinitDiagnosticsPopulatedAndCacheHitSilent) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  const auto first = session::initializeMdesNumerics(s);
  CHECK(first.isOk());
  CHECK(first.scinitRan);
  CHECK_EQ(first.scinit.iterations.size(), 10u);
  CHECK(first.scinit.converged);
  CHECK_EQ(first.scinit.iterations.front().pass, 1);
  CHECK(first.scinit.iterations.back().maxDelta < 5.0e-7);
  CHECK_NEAR(first.scinit.dyte, 0.0025, 1e-3, 1e-3);
  const auto second = session::initializeMdesNumerics(s);
  CHECK(second.isOk());
  CHECK(!second.scinitRan);
  CHECK(second.scinit.iterations.empty());
}

// QDES entry: GGCALC only for a real run, OFS from the real snapshot,
// no VISCAL event; second entry (cache valid) without GGCALC.
TEST(unit, QdesEntryEventsFromRealPointPath) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  const auto first = session::initializeQdesNumerics(s);
  CHECK(first.isOk());
  CHECK(first.specalSnapshot.valid);
  CHECK_NEAR(first.specalSnapshot.cl, 0.2554, 5e-4, 5e-4);
  int gg = 0;
  int ofs = 0;
  for (const auto& ev : first.pointEvents) {
    if (ev.kind == session::OperPointEventKind::influenceSystemCalculation) {
      ++gg;
    }
    if (ev.kind == session::OperPointEventKind::specifiedPointOutput) {
      ++ofs;
    }
    CHECK(ev.kind != session::OperPointEventKind::viscous);
  }
  CHECK_EQ(gg, 1);
  CHECK_EQ(ofs, 1);
  // GGCALC before OFS (chronological).
  CHECK(first.pointEvents.front().kind ==
        session::OperPointEventKind::influenceSystemCalculation);
}

// MDES EXEC: MAPGEN records of the same commits + GEOPAR metrics
// (oracle: 0.120020/0.304, 0.019997/0.402).
TEST(unit, MdesExecMapgenAndGeoparRecords) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::initializeMdesNumerics(s).isOk());
  const auto exec = session::executeMdesDesign(s);
  CHECK(exec.isOk());
  CHECK_EQ(exec.mapgen.iterations.size(), 1u);
  CHECK(exec.mapgen.converged);
  CHECK(exec.metricsValid);
  CHECK_NEAR(exec.metrics.thick, 0.120020, 5e-6, 5e-6);
  CHECK_NEAR(exec.metrics.cambr, 0.019997, 5e-6, 5e-6);
}

// QDES REST: the 6.996 LEFIND bracket finds the LE for the NACA
// buffer/panel pair, so the limitReached event stays clear.
TEST(unit, QdesRestLefindEventFlag) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  const auto rest = session::restoreQdesCurrentFromBuffer(s);
  CHECK(rest.isOk());
  CHECK_FALSE(rest.leadingEdgeNotFound);
  CHECK(s.design.lgSame);
}

// S19: 6.996 MDES SMOO without a prior QDES run always smooths
// QSPEC(:,1) (xmdes.f 445: KQSP=1), even though KQTARG is still 0 and
// therefore aliases QGAMM in COMMON/CR07. QGAMM remains unchanged.
// Exact 6.996 CLI DUMP bytes are anchored in integration.SmooTargetS19.
TEST(unit, SmooTargetS19MdesWithoutQdesKeepsQgamm) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::initializeMdesNumerics(s).isOk());
  CHECK_EQ(s.inverse.shared.kqtarg, -1);
  const auto qspecBefore = s.inverse.shared.qspecs[0].q;
  const auto qgammBefore = s.inverse.shared.qgamm;
  session::MdesSpectrumReport report;
  CHECK(session::mdesSmoothTargetSegment(s, report) ==
        session::MdesCommandStatus::ok);
  CHECK_EQ(report.columns.size(), 1u);
  CHECK(s.inverse.shared.qgamm == qgammBefore);
  CHECK(s.inverse.shared.qspecs[0].q != qspecBefore);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::currentQ);
}

// S19: after QDES, KQTARG is 1 (0-based 0) and a subsequent MDES SMOO
// still selects the fixed KQSP=1 column. QGAMM remains unchanged.
// Exact 6.996 CLI DUMP bytes are anchored in integration.SmooTargetS19.
TEST(unit, SmooTargetS19AfterQdesKeepsQgamm) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  CHECK(session::initializeQdesNumerics(s).isOk());
  CHECK_EQ(s.inverse.shared.kqtarg, 0);
  session::leaveQdesMenuState(s);
  enterMdes(s);
  CHECK_EQ(s.inverse.shared.kqtarg, 0);
  const auto qspecBefore = s.inverse.shared.qspecs[0].q;
  const auto qgammBefore = s.inverse.shared.qgamm;
  session::MdesSpectrumReport report;
  CHECK(session::mdesSmoothTargetSegment(s, report) ==
        session::MdesCommandStatus::ok);
  CHECK_EQ(report.columns.size(), 1u);
  CHECK(s.inverse.shared.qgamm == qgammBefore);
  CHECK(s.inverse.shared.qspecs[0].q != qspecBefore);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::currentQ);
}

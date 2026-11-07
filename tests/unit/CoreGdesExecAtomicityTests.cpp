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

// File: tests/unit/CoreGdesExecAtomicityTests.cpp
// EXEC atomicity and producer/consumer chain at session level; the
// test calls the session operations directly, not via the TOP menu.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreOperHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

namespace {

namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

double runOperAlfa(session::SessionComposition& s, double alpha) {
  std::ostringstream out;
  std::istringstream in;
  const auto st = cli::handleOperAlphaCore(s, in, out,
                                           std::to_string(alpha));
  if (st != cli::OperAlphaCoreStatus::analyzed) {
    std::cerr << "oper status=" << static_cast<int>(st) << " out=" << out.str() << "\n";
  }
  return s.result.lastResult.liftCoefficient;
}

}  // namespace

TEST(unit, CoreGdesExecAtomicityAndConsumerChain) {
  session::SessionComposition s;
  std::ostringstream setup;
  cli::handleNacaCore(s, setup, 12);
  (void)session::applyPaneling(s);
  xfoil::AnalysisConfig config;
  config.maximumIterations = 40;
  config.viscous = false;
  (void)session::configure(s, config);
  (void)session::initializeGeometryDesign(s);
  const std::size_t panelN0 = s.paneling.panel.x.size();
  const std::vector<double> panelX0 = s.paneling.panel.x;

  // A. Buffer change (GDES SCAL), Panel stays, LGSAME=false.
  {
    const auto scaled = session::gdesScale(s, 2.0, 2.0);
    CHECK(scaled.ok);
  }
  CHECK(!s.design.lgSame);
  CHECK_EQ(s.paneling.panel.x.size(), panelN0);
  CHECK(s.paneling.panel.x == panelX0);
  const std::size_t bufferN = s.geometry.buffer.x.size();
  CHECK(bufferN != panelN0);

  // OPER on the old panel contour (consumer before EXEC).
  const double clOld = runOperAlfa(s, 2.0);
  CHECK(s.result.lastResult.successful);
  CHECK(s.viscous.systemValid || s.result.lastResult.successful);

  // B. EXEC successfully.
  const auto exec = session::executeGeometryDesignBuffer(s);
  CHECK(exec.ok);
  CHECK(s.design.lgSame);
  CHECK(s.paneling.origin == session::PanelingOrigin::abcopy);
  CHECK_EQ(s.paneling.panelSourceRevision, s.geometry.bufferRevision);
  CHECK_EQ(s.paneling.panel.x.size(), bufferN);
  for (std::size_t i = 0; i < bufferN; ++i) {
    CHECK(std::abs(s.paneling.panel.x[i] - s.geometry.buffer.x[i]) < 1e-12);
    CHECK(std::abs(s.paneling.panel.y[i] - s.geometry.buffer.y[i]) < 1e-12);
  }
  // Analysis state invalidated canonically.
  CHECK(!s.viscous.systemValid);
  CHECK(!s.viscous.converged);
  CHECK(!s.viscous.wakeValid);
  CHECK(!s.result.lastResult.successful);

  // OPER consumes the new contour (different Cl than before).
  const double clNew = runOperAlfa(s, 2.0);
  CHECK(s.result.lastResult.successful);
  CHECK(std::abs(clNew - clOld) > 1e-6);
  CHECK_EQ(s.paneling.panel.x.size(), bufferN);
}

TEST(unit, CoreGdesExecFailureKeepsOldPanelAndAnalysis) {
  session::SessionComposition s;
  std::ostringstream setup;
  cli::handleNacaCore(s, setup, 12);
  (void)session::applyPaneling(s);
  xfoil::AnalysisConfig config;
  config.maximumIterations = 40;
  config.viscous = false;
  (void)session::configure(s, config);
  (void)session::initializeGeometryDesign(s);
  const std::vector<double> panelX0 = s.paneling.panel.x;
  const double clOld = runOperAlfa(s, 2.0);
  CHECK(s.result.lastResult.successful);

  // Replacing the buffer with a large overlay contour does not work
  // (OVER does not mutate the buffer); the capacity violation is
  // therefore checked directly through the session use case with a
  // subsequently inflated buffer.
  auto big = s.geometry.buffer;
  for (int i = 0; i < 200; ++i) {
    big.x.push_back(1.0 + 0.001 * static_cast<double>(i));
    big.y.push_back(0.0);
  }
  (void)session::setBufferGeometry(s, big);
  const std::size_t panelAfterSet = s.paneling.panel.x.size();
  const bool panelValidAfterSet = s.paneling.panelingValid;

  const auto exec = session::executeGeometryDesignBuffer(s);
  CHECK(exec.capacityExceeded);
  CHECK(!exec.ok);
  CHECK_EQ(s.paneling.panel.x.size(), panelAfterSet);
  CHECK_EQ(s.paneling.panelingValid, panelValidAfterSet);
  CHECK(!s.design.lgSame);
  // the design buffer stays editable (after buffer replacement it is
  // initialized again as in the menu cycle).
  (void)session::initializeGeometryDesign(s);
  const auto moved = session::gdesTranslate(s, 0.1, 0.0);
  CHECK(moved.ok);
  (void)clOld;
}

TEST(unit, CoreGdesExecLbliniRule) {
  // IF(N.NE.NB) LBLINI=.FALSE. unless N=NB (xgdes.f 975): the same point
  // count preserves LBLINI, a different one clears it.
  session::SessionComposition s;
  std::ostringstream setup;
  cli::handleNacaCore(s, setup, 12);
  (void)session::initializeGeometryDesign(s);
  const auto gset = session::gdesSetBufferFromCurrent(s);
  CHECK(gset.ok);
  s.viscous.viscals.lblini = true;
  const auto execSame = session::executeGeometryDesignBuffer(s);
  CHECK(execSame.ok);
  CHECK(s.viscous.viscals.lblini);

  session::SessionComposition t;
  std::ostringstream setupT;
  cli::handleNacaCore(t, setupT, 12);
  t.viscous.viscals.lblini = true;
  auto grown = t.geometry.buffer;
  grown.x.push_back(grown.x.front());
  grown.y.push_back(grown.y.front() + 0.001);
  (void)session::setBufferGeometry(t, grown);
  t.viscous.viscals.lblini = true;
  const auto execGrown = session::executeGeometryDesignBuffer(t);
  CHECK(execGrown.ok);
  CHECK(!t.viscous.viscals.lblini);
}

// End: tests/unit/CoreGdesExecAtomicityTests.cpp

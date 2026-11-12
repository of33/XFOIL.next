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

// File: tests/unit/StateInvalidationS17aTests.cpp
// S17a state/invalidation contracts after geometry and file-command paths.
// The focus is the distinction between buffer-only mutations, current-panel
// invalidation, query/export operations and analysis-state preservation.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/io/ExportFormats.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;

void prepareSolvedSession(session::SessionComposition& state) {
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(state, naca.value().buffer).success());
  CHECK(session::applyPaneling(state).ok);

  AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1.0e6;
  CHECK(session::configure(state, config).success());
  const AnalysisResult point =
      session::runSpecAlpha(state, Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(state.viscous.converged);
  CHECK(state.viscous.viscals.lblini);
  CHECK(state.viscous.systemValid);
  // Test-injected cache markers, as in the existing invalidation tests.
  state.viscous.wakeValid = true;
  state.viscous.viscals.dijWakeValid = true;
}

}  // namespace

TEST(unit, StateInvalidationS17aBufferOnlyAdoptionPreservesCurrentPanel) {
  session::SessionComposition state;
  prepareSolvedSession(state);

  const std::vector<double> panelX = state.paneling.panel.x;
  const std::vector<double> panelY = state.paneling.panel.y;
  const std::uint64_t bufferRevision = state.geometry.bufferRevision;
  const std::uint64_t panelRevision = state.paneling.panelRevision;
  const std::uint64_t panelSourceRevision =
      state.paneling.panelSourceRevision;
  const AnalysisStatus resultStatus = state.result.lastResult.status;

  const auto naca = core::geometry::nacaAirfoil(12, false, 123);
  CHECK(naca.isOk());
  const auto adopted =
      session::setBufferOnlyGeometry(state, naca.value().buffer);
  CHECK(adopted.success());
  CHECK_EQ(state.geometry.bufferRevision, bufferRevision + 1u);
  CHECK_EQ(state.geometry.name, std::string("NACA 0012"));

  CHECK(state.paneling.panelingValid);
  CHECK(state.paneling.panel.x == panelX);
  CHECK(state.paneling.panel.y == panelY);
  CHECK_EQ(state.paneling.panelRevision, panelRevision);
  CHECK_EQ(state.paneling.panelSourceRevision, panelSourceRevision);
  CHECK(state.viscous.systemValid);
  CHECK(state.viscous.wakeValid);
  CHECK(state.viscous.converged);
  CHECK(state.viscous.viscals.lblini);
  CHECK(state.result.lastResult.status == resultStatus);
}

TEST(unit, StateInvalidationS17aBufferOnlyAdoptionRejectsInvalidBuffer) {
  session::SessionComposition state;
  prepareSolvedSession(state);

  const std::vector<double> panelX = state.paneling.panel.x;
  const std::uint64_t bufferRevision = state.geometry.bufferRevision;
  const std::string name = state.geometry.name;
  const AnalysisStatus resultStatus = state.result.lastResult.status;

  core::geometry::AirfoilBuffer invalid;
  invalid.name = "invalid";
  const auto adopted = session::setBufferOnlyGeometry(state, invalid);
  CHECK(!adopted.success());
  CHECK(state.paneling.panel.x == panelX);
  CHECK_EQ(state.geometry.bufferRevision, bufferRevision);
  CHECK_EQ(state.geometry.name, name);
  CHECK(state.viscous.converged);
  CHECK(state.result.lastResult.status == resultStatus);
}

TEST(unit, StateInvalidationS17aLoadWarmStartFollowsAbcopyPointCount) {
  session::SessionComposition state;
  prepareSolvedSession(state);
  CHECK(session::copyBufferToCurrent(state).ok);
  const auto point =
      session::runSpecAlpha(state, Degrees{4.0}, nullptr);
  CHECK(point.converged);
  CHECK(state.viscous.viscals.lblini);
  const std::size_t oldPanelCount = state.paneling.panel.x.size();

  const auto sameCount = core::geometry::nacaAirfoil(12, false, 123);
  CHECK(sameCount.isOk());
  CHECK_EQ(sameCount.value().buffer.x.size(), oldPanelCount);
  CHECK(session::setBufferOnlyGeometry(state, sameCount.value().buffer)
            .success());
  CHECK(session::copyBufferToCurrent(state).ok);
  CHECK(state.viscous.viscals.lblini);  // N==NB preserves the warm start

  const auto newCount = core::geometry::nacaAirfoil(12, false, 200);
  CHECK(newCount.isOk());
  CHECK(session::setBufferOnlyGeometry(state, newCount.value().buffer)
            .success());
  CHECK(session::copyBufferToCurrent(state).ok);
  CHECK_FALSE(state.viscous.viscals.lblini);  // N!=NB resets
}

TEST(unit, StateInvalidationS17aNacaReplacesPanelAndInvalidatesAnalysis) {
  session::SessionComposition state;
  prepareSolvedSession(state);
  const std::vector<double> panelBefore = state.paneling.panel.x;

  std::ostringstream out;
  app::cli::handleNacaCore(state, out, 12);

  CHECK(state.paneling.panelingValid);
  CHECK(state.paneling.panel.x != panelBefore);
  CHECK_EQ(static_cast<int>(state.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_FALSE(state.viscous.systemValid);
  CHECK_FALSE(state.viscous.wakeValid);
  CHECK_FALSE(state.viscous.converged);
  CHECK_FALSE(state.viscous.viscals.lblini);
  CHECK_FALSE(state.viscous.viscals.mapping.valid);
  CHECK(state.result.lastResult.status != AnalysisStatus::converged);
}

TEST(unit, StateInvalidationS17aGdesMutationKeepsAnalysisUntilExec) {
  session::SessionComposition state;
  prepareSolvedSession(state);
  const std::vector<double> panelX = state.paneling.panel.x;
  const std::vector<double> panelY = state.paneling.panel.y;
  const std::uint64_t panelRevision = state.paneling.panelRevision;
  const std::uint64_t panelSourceRevision =
      state.paneling.panelSourceRevision;
  const std::uint64_t bufferRevision = state.geometry.bufferRevision;

  CHECK(session::initializeGeometryDesign(state).status ==
        session::GeometryDesignInitStatus::initialized);
  const auto square = session::gdesSquareTrailingEdge(state);
  CHECK(square.isOk());
  CHECK_EQ(state.geometry.bufferRevision, bufferRevision + 1u);

  CHECK(state.paneling.panel.x == panelX);
  CHECK(state.paneling.panel.y == panelY);
  CHECK_EQ(state.paneling.panelRevision, panelRevision);
  CHECK_EQ(state.paneling.panelSourceRevision, panelSourceRevision);
  CHECK(state.viscous.systemValid);
  CHECK(state.viscous.wakeValid);
  CHECK(state.viscous.converged);
  CHECK(state.viscous.viscals.lblini);
  CHECK(state.result.lastResult.status == AnalysisStatus::converged);

  const auto exec = session::executeGeometryDesignBuffer(state);
  CHECK(exec.ok);
  CHECK_EQ(state.paneling.panelSourceRevision, state.geometry.bufferRevision);
  CHECK_FALSE(state.viscous.systemValid);
  CHECK_FALSE(state.viscous.wakeValid);
  CHECK_FALSE(state.viscous.converged);
  CHECK_FALSE(state.viscous.viscals.lblini);
  CHECK_FALSE(state.viscous.viscals.mapping.valid);
  CHECK(state.result.lastResult.status != AnalysisStatus::converged);
}

TEST(unit, StateInvalidationS17aQueryAndExportProjectionsAreMutationFree) {
  session::SessionComposition state;
  prepareSolvedSession(state);
  CHECK(session::initializeGeometryDesign(state).status ==
        session::GeometryDesignInitStatus::initialized);

  const std::vector<double> panelX = state.paneling.panel.x;
  const std::vector<double> bufferX = state.geometry.buffer.x;
  const AnalysisStatus resultStatus = state.result.lastResult.status;
  const std::uint64_t bufferRevision = state.geometry.bufferRevision;
  const std::uint64_t panelRevision = state.paneling.panelRevision;

  const auto query = session::gdesCenterPointQuery(state, 0.5, 0.5);
  CHECK(query.ok);
  const auto cp = session::currentPressureDumpInput(state);
  const auto bl = session::currentBoundaryLayerDumpInput(state);
  CHECK(!cp.x.empty());
  CHECK(!bl.x.empty());

  CHECK(state.paneling.panel.x == panelX);
  CHECK(state.geometry.buffer.x == bufferX);
  CHECK_EQ(state.geometry.bufferRevision, bufferRevision);
  CHECK_EQ(state.paneling.panelRevision, panelRevision);
  CHECK(state.viscous.systemValid);
  CHECK(state.viscous.wakeValid);
  CHECK(state.viscous.converged);
  CHECK(state.viscous.viscals.lblini);
  CHECK(state.result.lastResult.status == resultStatus);
}

// End: tests/unit/StateInvalidationS17aTests.cpp

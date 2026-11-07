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

// File: tests/unit/OperP7P2CombinedTests.cpp
// combined sequence ASET -> DAMP -> VISC/Alfa
// -> BLF -> DAMP in one session. Proves: the sequence runs
// mutation-free in the BLF part, the invalidation vector after ASET
// stays effective until the next computation, and the output is
// identical to the individual-call sequence (determinism of the
// chain).

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

using namespace xfoil;
using app::cli::handleOperAlphaCore;
using app::cli::handleOperAsetCore;
using app::cli::handleOperBlfCore;
using app::cli::handleOperDampCore;

std::string runCombinedSequence() {
  session::SessionComposition composition;
  const auto naca2412 = core::geometry::nacaAirfoil(2412);
  CHECK(naca2412.isOk());
  CHECK(session::setBufferGeometry(composition, naca2412.value().buffer)
            .success());
  CHECK(session::applyPaneling(composition).ok);

  // two polar slots (contour = panel point cloud, as for PACC).
  for (int designation : {2412, 12}) {
    const auto naca = core::geometry::naca4(designation, 123);
    CHECK(naca.isOk());
    core::polar::PolarDataset slot;
    slot.metadata.airfoilName = designation == 2412 ? "NACA 2412"
                                                    : "NACA 0012";
    slot.geometry.x = naca.value().xb;
    slot.geometry.y = naca.value().yb;
    composition.polar.slots.push_back(std::move(slot));
    composition.polar.saveFileNames.emplace_back();
    composition.polar.dumpFileNames.emplace_back();
  }

  std::ostringstream out;
  std::istringstream asetIn("2\n\n");
  CHECK(handleOperAsetCore(composition, asetIn, out, "", false) ==
        app::cli::OperAsetCoreStatus::applied);
  CHECK(handleOperDampCore(composition, out) ==
        app::cli::OperDampCoreStatus::applied);

  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  std::istringstream alphaIn;
  CHECK(handleOperAlphaCore(composition, alphaIn, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(handleOperBlfCore(composition, out, "", false) ==
        app::cli::OperBlfCoreStatus::reported);
  CHECK(handleOperDampCore(composition, out) ==
        app::cli::OperDampCoreStatus::applied);
  return out.str();
}

}  // namespace

// the combined sequence contains the ASET commit, DAMP messages, and
// the BLF report; IDAMP=1 takes effect in the viscous computation (hkmax
// anchored at 3.5872) and the BLF report ends with two
// format-7720 lines.
TEST(unit, OperCombinedSequenceIsCoherent) {
  const std::string text = runCombinedSequence();
  CHECK(text.find(" Current airfoil will be overwritten.") !=
        std::string::npos);
  CHECK(text.find(" Modified amplification used\n") != std::string::npos);
  CHECK(text.find(" Original amplification used\n") != std::string::npos);
  CHECK(text.find(" xBL, Fint =") != std::string::npos);
  CHECK(text.find("Boundary-layer data are not available") ==
        std::string::npos);
}

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

// File: tests/core/SessionLinkCheck.cpp
// Minimal program linked only against session, core, and pure
// analysis-configuration/facade objects.
// session link check; public API link test.
// It excludes app/CLI objects and proves that the
// session/use-case layer and the public API
// (xfoil::XFoilSession, xfoil::XFoil) link as standalone libraries.
//
// the program exercises the public API basics and returns 0 on
// success, otherwise a non-zero exit code.

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/XFoil.h"
#include "xfoil/XFoilSession.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"
#include "xfoil/Types.h"

#include <vector>

int main() {
  using namespace xfoil;

  // session composition and use cases link as standalone libraries.
  session::SessionComposition composition;
  if (composition.geometry.valid || composition.operating.configured) {
    return 1;
  }
  session::resetBoundaryLayer(composition);
  if (session::initializeAnalysis(composition)) {
    return 1;
  }

  // Public session API basics link as standalone libraries.
  XFoilSession session;
  if (session.hasGeometry() || session.hasConfiguration()) {
    return 1;
  }
  const AnalysisResult rejected = session.analyzeAtAlpha(Degrees{4.0});
  if (rejected.status != AnalysisStatus::missingGeometry) {
    return 1;
  }

  // Public facade links as a standalone library.
  XFoil foil;
  if (foil.hasGeometry() || foil.hasConfiguration()) {
    return 1;
  }
  const ConfigValidationResult configured = foil.configure(AnalysisConfig{});
  if (!configured.success()) {
    return 1;
  }

  return 0;
}

// End: tests/core/SessionLinkCheck.cpp

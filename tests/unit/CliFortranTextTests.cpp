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

// File: tests/unit/CliFortranTextTests.cpp
// Emitter preliminary stage: checks the byte-exact reproduction of the
// Fortran text lines emitted by the CLI path
// (ABCOPY confirmation Format 1200, PANGEN convergence
// and capacity messages). The reference strings are the canonical
// Fortran-interface texts.

#include "TestSupport.h"

#include "app/cli/CliFortranText.h"

#include "xfoil/core/paneling/PanelingTypes.h"

#include <sstream>
#include <string>

namespace {

using namespace xfoil;

}  // namespace

TEST(unit, CliEmitterAbcopyConfirmationMatchesFortranFormat) {
  std::ostringstream out;
  app::cli::emitAbcopyConfirmation(out, 151);
  CHECK_EQ(out.str(),
           std::string("\n Current airfoil nodes set from buffer airfoil "
                       "nodes ( 151 )\n"));

  std::ostringstream wide;
  app::cli::emitAbcopyConfirmation(wide, 369);
  CHECK_EQ(wide.str(),
           std::string("\n Current airfoil nodes set from buffer airfoil "
                       "nodes ( 369 )\n"));
}

TEST(unit, CliEmitterPangenConvergenceOnlyWhenLimitReached) {
  core::paneling::PangenResult result;

  std::ostringstream clean;
  result.diagnostics = {core::math::Diagnostic::completed};
  app::cli::emitPangenDiagnostics(clean, result);
  CHECK_EQ(clean.str(), std::string());

  std::ostringstream failed;
  result.diagnostics = {core::math::Diagnostic::limitReached};
  app::cli::emitPangenDiagnostics(failed, result);
  CHECK_EQ(failed.str(),
           std::string("Paneling convergence failed.  Continuing anyway...\n"));
}

TEST(unit, CliEmitterPangenTooManyPanels) {
  std::ostringstream out;
  app::cli::emitPangenTooManyPanels(out);
  CHECK_EQ(out.str(), std::string("Panel: Too many panels. Increase IQX\n"));
}

// NACA buffer message (xfoil.f Format 1200). Reference string from the
// canonical Fortran process (fortran-bin/xfoil, NACA 2412).
TEST(unit, CliEmitterBufferAirfoilSetMatchesFortran) {
  std::ostringstream out;
  app::cli::emitBufferAirfoilSet(out, 245);
  CHECK_EQ(out.str(),
           std::string("\n Buffer airfoil set using 245 points\n"));
}

// PANGEN trailing-edge message (xfoil.f Format 1090 = /1X,A,F9.5).
// reference from the canonical Fortran process (NACA 2412 -> blunt TE,
// GAP 0.00252). Emitting this line
// follows the canonical behavior.
TEST(unit, CliEmitterTrailingEdgeBluntMatchesFortran) {
  std::ostringstream out;
  app::cli::emitTrailingEdge(out, false, 0.00252);
  CHECK_EQ(out.str(),
           std::string("\n Blunt trailing edge.  Gap =  0.00252\n"));
}

TEST(unit, CliEmitterTrailingEdgeSharpMatchesFortran) {
  std::ostringstream out;
  app::cli::emitTrailingEdge(out, true, 0.0);
  CHECK_EQ(out.str(), std::string("\n Sharp trailing edge\n"));
}

// PANGEN parameter table (xfoil.f Format 1100). Reference from the
// canonical Fortran process (NACA 2412, INIT panel parameter).
TEST(unit, CliEmitterPanelingParametersMatchesFortran) {
  std::ostringstream out;
  app::cli::emitPanelingParameters(out, 160, 1.0, 0.15, 0.2, 1.0, 1.0, 1.0,
                                   1.0);
  CHECK_EQ(out.str(),
           std::string("\n Paneling parameters used..."
                       "\n   Number of panel nodes       160"
                       "\n   Panel bunching parameter    1.000"
                       "\n   TE/LE panel density ratio   0.150"
                       "\n   Refined-area/LE panel density ratio    0.200"
                       "\n   Top    side refined area x/c limits  1.000 1.000"
                       "\n   Bottom side refined area x/c limits  1.000 1.000\n"));
}

// GEOPAR thickness/camber line (xgeom.f Format 1000). Reference from the
// canonical Fortran process (NACA 2412).
TEST(unit, CliEmitterThicknessCamberMatchesFortran) {
  std::ostringstream out;
  app::cli::emitThicknessCamber(out, 0.120032, 0.297, 0.019999, 0.403);
  CHECK_EQ(out.str(),
           std::string(" Max thickness =     0.120032  at x =   0.297\n"
                       " Max camber    =     0.019999  at x =   0.403\n"));
}

// End: tests/unit/CliFortranTextTests.cpp

// MDES/QDES diagnostics formatter (oracle bytes).

TEST(unit, CliEmitterMdesTeGapMatchesFortran) {
  std::ostringstream o;
  app::cli::emitMdesTeGapLine(o, 0.0000246, 0.00251, 15.9051);
  CHECK(o.str() ==
        "\n Current TE gap  dx dy = 0.0000 0.0025    TE angle = 15.905 deg."
        "\n\n");
}

TEST(unit, CliEmitterScinitIterationMatchesFortran) {
  std::ostringstream o;
  app::cli::emitScinitIteration(o, 1, 1.8671573587363710e-2);
  CHECK(o.str() ==
        "           1      max(dw) =   1.8671573587363710E-002\n");
  std::ostringstream p;
  app::cli::emitScinitIteration(p, 10, 4.7835528504958091e-7);
  CHECK(p.str() ==
        "          10      max(dw) =   4.7835528504958091E-007\n");
}

TEST(unit, CliEmitterMapgenIterationMatchesFortran) {
  std::ostringstream o;
  app::cli::emitMapgenIteration(o, 1, 7.9827397709057502e-7);
  CHECK(o.str() == "           1   7.9827397709057502E-007\n");
}

TEST(unit, CliEmitterGeoparMetricsMatchesFortran) {
  std::ostringstream o;
  app::cli::emitGeoparMetrics(o, 0.1200201, 0.3044, 0.0199973, 0.4024);
  CHECK(o.str() ==
        " Max thickness =     0.120020  at x =   0.304\n"
        " Max camber    =     0.019997  at x =   0.402\n");
}

TEST(unit, CliEmitterGgCalcAndLefindMatchFortran) {
  std::ostringstream o;
  app::cli::emitGgCalcHeader(o);
  app::cli::emitLefindNotFound(o);
  CHECK(o.str() ==
        " Calculating unit vorticity distributions ...\n"
        " LEFIND:  LE point not found.  Continuing...\n");
}

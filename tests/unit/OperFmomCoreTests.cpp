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

// File: tests/unit/OperFmomCoreTests.cpp
// Handler tests for OPER FMOM. Byte-accurate
// comparison against extracted Fortran CLI oracle segments
// (tests/data/cli-reference/fmom_cli_*.txt, LC_ALL=C fortran-bin/xfoil).

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

using namespace xfoil;

std::string readReference(const char* path) {
  std::ifstream in(path, std::ios::binary);
  CHECK(static_cast<bool>(in));
  // No istreambuf_iterator: GCC -O3 (-Wnull-dereference) cannot prove
  // the buffer pointer of a failed stream to be non-zero.
  // the rdbuf detour avoids the apparent dereference.
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

session::SessionComposition nacaSession(int designation, bool viscous) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(designation, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = viscous;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  return composition;
}

}  // namespace

// FMOM without FNEW: GETXYF path with XOF=YOF=0 (INIT values), no prompts.
TEST(unit, OperFmomGetxyfDefaultByteExactAgainstFortranCli) {
  auto composition = nacaSession(12, false);
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperFmomCore(composition, in, out) ==
        app::cli::OperFmomCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/fmom_cli_prompt.txt"));
  CHECK(composition.design.flap.lFlap);
}

// FNEW 0.7 -0.05, then FMOM: GETXYF without prompts, inline values.
TEST(unit, OperFmomAfterFnewByteExactAgainstFortranCli) {
  auto composition = nacaSession(12, false);
  app::cli::handleOperFnewCore(composition, "0.7 -0.05", true);
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperFmomCore(composition, in, out) ==
        app::cli::OperFmomCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/fmom_cli_getxyf.txt"));
}

// Viscous FMOM (CPV branch) after VISC/alfa.
TEST(unit, OperFmomViscousByteExactAgainstFortranCli) {
  auto composition = nacaSession(12, true);
  app::cli::handleOperFnewCore(composition, "0.7 -0.05", true);
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperFmomCore(composition, in, out) ==
        app::cli::OperFmomCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/fmom_cli_visc.txt"));
}

// FNEW without values (XOF=YOF=-999), FMOM with ASKR prompts and y/t path.
TEST(unit, OperFmomPromptYtByteExactAgainstFortranCli) {
  auto composition = nacaSession(12, false);
  app::cli::handleOperFnewCore(composition, "", false);
  std::istringstream in("0.7\n999\n0.5\n");
  std::ostringstream out;
  CHECK(app::cli::handleOperFmomCore(composition, in, out) ==
        app::cli::OperFmomCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/fmom_cli_yt.txt"));
  CHECK_EQ(composition.design.flap.xof, 0.7);
}

// FNEW with one value (YOF=-999), FMOM with empty y answer:
// ASKR keeps -999, Format F8.4 output runs as '********'.
TEST(unit, OperFmomEmptyYAnswerByteExactAgainstFortranCli) {
  auto composition = nacaSession(12, false);
  app::cli::handleOperFnewCore(composition, "0.7", true);
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(app::cli::handleOperFmomCore(composition, in, out) ==
        app::cli::OperFmomCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/fmom_cli_emptyy.txt"));
}

// Second FMOM after a successful first: LFLAP=true path without
// GETXYF output, identical report.
TEST(unit, OperFmomSecondRunUsesLflapPath) {
  auto composition = nacaSession(12, false);
  app::cli::handleOperFnewCore(composition, "0.7 -0.05", true);
  std::istringstream in;
  std::ostringstream first;
  CHECK(app::cli::handleOperFmomCore(composition, in, first) ==
        app::cli::OperFmomCoreStatus::reported);
  std::ostringstream second;
  CHECK(app::cli::handleOperFmomCore(composition, in, second) ==
        app::cli::OperFmomCoreStatus::reported);
  // Second pass: LFLAP=true -> only Format 1500, no GETXYF text.
  const std::string reportOnly =
      first.str().substr(first.str().find("\n Flap hinge"));
  CHECK_EQ(second.str(), reportOnly);
}

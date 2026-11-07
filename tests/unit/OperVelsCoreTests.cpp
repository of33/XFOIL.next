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

// File: tests/unit/OperVelsCoreTests.cpp
// Handler tests for OPER VELS. Byte-accurate
// comparison against extracted Fortran CLI oracle segments
// (tests/data/cli-reference/vels_cli_*.txt, LC_ALL=C fortran-bin/xfoil).

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

session::SessionComposition naca0012InviscidAlfa2() {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(12);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  CHECK(session::applyPaneling(composition).ok);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  return composition;
}

}  // namespace

// VELS with two inline values: format 1800 byte-exact.
TEST(unit, OperVelsInlineByteExactAgainstFortranCli) {
  auto composition = naca0012InviscidAlfa2();
  std::istringstream in;
  std::ostringstream out;
  const auto res =
      app::cli::handleOperVelsCore(composition, in, out, "0.5 0.1", true);
  CHECK(res.status == app::cli::OperVelsCoreResult::Status::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/vels_cli_inline.txt"));
}

// VELS without values: both ASKR prompts.
TEST(unit, OperVelsPromptsByteExactAgainstFortranCli) {
  auto composition = naca0012InviscidAlfa2();
  std::istringstream in("0.5\n0.1\n");
  std::ostringstream out;
  const auto res =
      app::cli::handleOperVelsCore(composition, in, out, "", false);
  CHECK(res.status == app::cli::OperVelsCoreResult::Status::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/vels_cli_prompt.txt"));
}

// VELS with one inline value: only the y prompt.
TEST(unit, OperVelsOneInlineValueByteExactAgainstFortranCli) {
  auto composition = naca0012InviscidAlfa2();
  std::istringstream in("0.1\n");
  std::ostringstream out;
  const auto res =
      app::cli::handleOperVelsCore(composition, in, out, "0.5", true);
  CHECK(res.status == app::cli::OperVelsCoreResult::Status::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/vels_cli_one.txt"));
}

// Invalid x answer: ASKR repeats the x prompt (oracle
// vels_retry).
TEST(unit, OperVelsInvalidRetryByteExactAgainstFortranCli) {
  auto composition = naca0012InviscidAlfa2();
  std::istringstream in("abc\n0.5\n0.1\n");
  std::ostringstream out;
  const auto res =
      app::cli::handleOperVelsCore(composition, in, out, "", false);
  CHECK(res.status == app::cli::OperVelsCoreResult::Status::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/vels_cli_retry.txt"));
}

// empty answer keeps the required value (ASKR semantics): two empty
// lines after 'VELS' retain XXX=YYY=0 and deliver the
// free-stream point.
TEST(unit, OperVelsEmptyAnswersKeepDefaults) {
  auto composition = naca0012InviscidAlfa2();
  // the required XXX/YYY is COMMON/RINPUT from INIT (0). Two empty
  // answers retain 0/0; the point (0,0) lies on the contour, and
  // PSILIN returns the value at that location.
  std::istringstream in("\n\n");
  std::ostringstream out;
  const auto res =
      app::cli::handleOperVelsCore(composition, in, out, "", false);
  CHECK(res.status == app::cli::OperVelsCoreResult::Status::reported);
  CHECK(out.str().find(" u/Uinf =") != std::string::npos);
}

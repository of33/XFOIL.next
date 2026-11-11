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

// File: tests/unit/OperBlfCoreTests.cpp
// Tests for the OPER BLF handler (xoper.f 1547-1600) with
// the core kernel blForceReport (dplot.f FBLGET). The successful path
// is checked byte-exact against the LC_ALL=C fortran-bin oracle segments
// under tests/data/cli-reference/ (FORMAT 9100, profile dump
// '(1x,5f10.6)', 'yint > ye', FORMAT 7720). The safety guard
// (documented controlled deviation) rejects without usable BL station data,
// mutation-free.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using app::cli::handleOperAlphaCore;
using app::cli::handleOperBlfCore;
using app::cli::OperBlfCoreStatus;

std::string readReference(const char* name) {
  std::ifstream in(name);
  CHECK(static_cast<bool>(in));
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

session::SessionComposition viscousNaca0012Session() {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(12, false, 123);
  CHECK(naca.isOk());
  CHECK(
      session::setBufferGeometry(composition, naca.value().buffer).success());
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  return composition;
}

bool bitsEqual(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::bit_cast<std::uint64_t>(a[i]) !=
        std::bit_cast<std::uint64_t>(b[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

// default DPR 0.01: byte-exact oracle segment.
TEST(unit, OperBlfDefaultDprByteExactAgainstFortranCli) {
  auto composition = viscousNaca0012Session();
  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "", false) ==
        OperBlfCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/blf_cli_dpr001.txt"));
}

// Explicit DPR 0.02 and 0.015: byte-exact oracle segments.
TEST(unit, OperBlfExplicitDprByteExactAgainstFortranCli) {
  {
    auto composition = viscousNaca0012Session();
    std::ostringstream out;
    CHECK(handleOperBlfCore(composition, out, "0.02", true) ==
          OperBlfCoreStatus::reported);
    CHECK_EQ(out.str(),
             readReference("tests/data/cli-reference/blf_cli_dpr002.txt"));
  }
  {
    auto composition = viscousNaca0012Session();
    std::ostringstream out;
    CHECK(handleOperBlfCore(composition, out, "0.015", true) ==
          OperBlfCoreStatus::reported);
    CHECK_EQ(out.str(),
             readReference("tests/data/cli-reference/blf_cli_dpr015.txt"));
  }
}

// Extra argument: canonically only RINPUT(1) is read.
TEST(unit, OperBlfExtraArgumentUsesFirstValueOnly) {
  auto composition = viscousNaca0012Session();
  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "0.01 0.02", true) ==
        OperBlfCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/blf_cli_dpr001.txt"));
}

// A non-parseable argument corresponds to NINPUT=0 -> default 0.01.
TEST(unit, OperBlfNonParseableArgumentFallsBackToDefault) {
  auto composition = viscousNaca0012Session();
  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "abc", true) ==
        OperBlfCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/blf_cli_dpr001.txt"));
}

// Negative DPR: defined report without 'yint > ye' branches.
TEST(unit, OperBlfNegativeDprDefinedReport) {
  auto composition = viscousNaca0012Session();
  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "-0.01", true) ==
        OperBlfCoreStatus::reported);
  CHECK_EQ(out.str(),
           readReference("tests/data/cli-reference/blf_cli_dprneg001.txt"));
}

// BLF is mutation-free: panel, BL station fields, CPI/CPV,
// convergence flags, polar and buffer remain bit-identical.
TEST(unit, OperBlfReportIsMutationFree) {
  auto composition = viscousNaca0012Session();
  const std::vector<double> cpi = composition.viscous.cpi;
  const std::vector<double> cpv = composition.viscous.cpv;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> thetTop = composition.viscous.viscals.thet.top;
  const std::vector<double> uedgBottom =
      composition.viscous.viscals.uedg.bottom;
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const bool converged = composition.viscous.converged;
  const bool systemValid = composition.viscous.systemValid;
  const int idamp = composition.viscous.idamp;

  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "", false) ==
        OperBlfCoreStatus::reported);

  CHECK(bitsEqual(cpi, composition.viscous.cpi));
  CHECK(bitsEqual(cpv, composition.viscous.cpv));
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK(bitsEqual(thetTop, composition.viscous.viscals.thet.top));
  CHECK(bitsEqual(uedgBottom, composition.viscous.viscals.uedg.bottom));
  CHECK(bitsEqual(bufferX, composition.geometry.buffer.x));
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_EQ(composition.viscous.idamp, idamp);
}

// guard without BL station data (only an inviscid point): tight
// safety diagnostic, no partial values, no mutation.
TEST(unit, OperBlfGuardWithoutBlDataIsMutationFree) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(12, false, 123);
  CHECK(naca.isOk());
  CHECK(
      session::setBufferGeometry(composition, naca.value().buffer).success());
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream runOut;
  CHECK(handleOperAlphaCore(composition, in, runOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> cpi = composition.viscous.cpi;

  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "", false) ==
        OperBlfCoreStatus::blDataUnavailable);
  CHECK_EQ(out.str(), " Boundary-layer data are not available.\n");
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK(bitsEqual(cpi, composition.viscous.cpi));
}

// guard also without any analysis (fresh session): no solver
// initialization, no mutation.
TEST(unit, OperBlfGuardOnFreshSessionNoSideEffects) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK(handleOperBlfCore(composition, out, "0.02", true) ==
        OperBlfCoreStatus::blDataUnavailable);
  CHECK_EQ(out.str(), " Boundary-layer data are not available.\n");
  CHECK(!composition.paneling.panelingValid);
  CHECK_EQ(composition.viscous.idamp, 0);
}

// session independence: a BLF report in one session leaves the other
// untouched.
TEST(unit, OperBlfIndependentAcrossSessions) {
  auto a = viscousNaca0012Session();
  auto b = viscousNaca0012Session();
  const std::vector<double> bThet = b.viscous.viscals.thet.top;
  std::ostringstream out;
  CHECK(handleOperBlfCore(a, out, "0.02", true) ==
        OperBlfCoreStatus::reported);
  CHECK(bitsEqual(bThet, b.viscous.viscals.thet.top));
}

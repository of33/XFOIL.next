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

// File: tests/unit/OperDampCoreTests.cpp
// Tests for the OPER DAMP handler (xoper.f 1744-1750).
// Proves: the toggle sequence 0->1->0 with byte-exact canonical
// messages, no invalidation (existing solution bit-identical),
// the effect on the next viscous computation via dampl2/dampl
// (Fortran anchors hkmax 3.5872/3.5895), continuation '!',
// persistence across PANE/airfoil change, session independence,
// and that arguments are canonically ignored.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <bit>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using app::cli::handleOperAlphaCore;
using app::cli::handleOperBangCore;
using app::cli::handleOperDampCore;
using app::cli::handleOperViscCore;

void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(
      session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);
}

void configureViscous(session::SessionComposition& composition) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
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

// toggle sequence and canonical messages byte-exact; default is 0.
TEST(unit, OperDampToggleSequenceAndTexts) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  CHECK_EQ(composition.viscous.idamp, 0);

  std::ostringstream out;
  CHECK(app::cli::handleOperDampCore(composition, out) ==
        app::cli::OperDampCoreStatus::applied);
  CHECK_EQ(composition.viscous.idamp, 1);
  CHECK(app::cli::handleOperDampCore(composition, out) ==
        app::cli::OperDampCoreStatus::applied);
  CHECK_EQ(composition.viscous.idamp, 0);
  CHECK(app::cli::handleOperDampCore(composition, out) ==
        app::cli::OperDampCoreStatus::applied);
  CHECK_EQ(composition.viscous.idamp, 1);
  CHECK_EQ(out.str(),
           " Modified amplification used\n"
           " Original amplification used\n"
           " Modified amplification used\n");
}

// An unexpected internal value falls through the canonical ELSE branch to 0.
TEST(unit, OperDampUnexpectedValueTogglesToOriginal) {
  session::SessionComposition composition;
  composition.viscous.idamp = 7;
  std::ostringstream out;
  CHECK(app::cli::handleOperDampCore(composition, out) ==
        app::cli::OperDampCoreStatus::applied);
  CHECK_EQ(composition.viscous.idamp, 0);
  CHECK_EQ(out.str(), " Original amplification used\n");
}

// DAMP invalidates nothing: influence system, CPI, wake, BL station
// arrays and result state remain bit-identical.
TEST(unit, OperDampInvalidatesNothing) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const std::vector<double> cpi = composition.viscous.cpi;
  const std::vector<double> cpv = composition.viscous.cpv;
  const std::vector<double> gam = composition.viscous.viscals.gam;
  const std::vector<double> ueTop = composition.viscous.viscals.uedg.top;
  const std::vector<double> ueBot = composition.viscous.viscals.uedg.bottom;
  const bool systemValid = composition.viscous.systemValid;
  const bool wakeValid = composition.viscous.wakeValid;
  const bool converged = composition.viscous.converged;
  const bool lblini = composition.viscous.viscals.lblini;
  const double blparSccon = composition.viscous.blpar.sccon;
  const int idampBefore = composition.viscous.idamp;

  std::ostringstream dampOut;
  CHECK(app::cli::handleOperDampCore(composition, dampOut) ==
        app::cli::OperDampCoreStatus::applied);

  CHECK_EQ(composition.viscous.idamp, idampBefore == 0 ? 1 : 0);
  CHECK(bitsEqual(cpi, composition.viscous.cpi));
  CHECK(bitsEqual(cpv, composition.viscous.cpv));
  CHECK(bitsEqual(gam, composition.viscous.viscals.gam));
  CHECK(bitsEqual(ueTop, composition.viscous.viscals.uedg.top));
  CHECK(bitsEqual(ueBot, composition.viscous.viscals.uedg.bottom));
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(composition.viscous.viscals.lblini, lblini);
  CHECK_EQ(composition.viscous.blpar.sccon, blparSccon);
}

// effect proof: DAMP->Alfa 2 reaches the dampl2 branch
// (hkmax 3.5872); a renewed DAMP->Alfa 2 goes back to dampl
// (hkmax 3.5895); continuation '!' carries IDAMP.
TEST(unit, OperDampReachesDamplAndDampl2InNextRuns) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition);
  std::istringstream in0;
  std::ostringstream out0;
  CHECK(handleOperAlphaCore(composition, in0, out0, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(composition.result.lastDiagnostics.completion.hkmax, 3.5895,
             5.0e-5, 0.0);

  std::ostringstream dampOut;
  CHECK(app::cli::handleOperDampCore(composition, dampOut) ==
        app::cli::OperDampCoreStatus::applied);
  std::istringstream in1;
  CHECK(handleOperAlphaCore(composition, in1, out0, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(composition.result.lastDiagnostics.completion.hkmax, 3.5872,
             5.0e-5, 0.0);

  // continuation '!' with unchanged IDAMP=1 stays in the dampl2 branch.
  CHECK(handleOperBangCore(composition, out0) !=
        app::cli::OperBangCoreStatus::noPreviousCommand);
  CHECK_EQ(composition.viscous.idamp, 1);
  CHECK_NEAR(composition.result.lastDiagnostics.completion.hkmax, 3.5872,
             5.0e-5, 0.0);

  std::ostringstream dampOut2;
  CHECK(app::cli::handleOperDampCore(composition, dampOut2) ==
        app::cli::OperDampCoreStatus::applied);
  std::istringstream in2;
  CHECK(handleOperAlphaCore(composition, in2, out0, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(composition.result.lastDiagnostics.completion.hkmax, 3.5895,
             5.0e-5, 0.0);
}

// DAMP before VISC: the toggle is preserved and the next computation
// uses it; VISC itself does not reset IDAMP.
TEST(unit, OperDampPersistsAcrossViscToggleAndPane) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition);
  std::ostringstream out;
  std::ostringstream dampOut;
  CHECK(app::cli::handleOperDampCore(composition, dampOut) ==
        app::cli::OperDampCoreStatus::applied);
  CHECK_EQ(composition.viscous.idamp, 1);
  CHECK(session::applyPaneling(composition).ok);
  CHECK_EQ(composition.viscous.idamp, 1);
  std::istringstream in;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(composition.result.lastDiagnostics.completion.hkmax, 3.5872,
             5.0e-5, 0.0);
}

// session independence: DAMP in one session leaves the other
// untouched.
TEST(unit, OperDampIndependentAcrossSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  buildNaca2412Paned(a);
  buildNaca2412Paned(b);
  std::ostringstream out;
  CHECK(app::cli::handleOperDampCore(a, out) ==
        app::cli::OperDampCoreStatus::applied);
  CHECK_EQ(a.viscous.idamp, 1);
  CHECK_EQ(b.viscous.idamp, 0);
}

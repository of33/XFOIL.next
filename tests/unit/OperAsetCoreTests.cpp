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

// File: tests/unit/OperAsetCoreTests.cpp
// Tests for the OPER ASET handler (xoper.f 1252-1289) with
// APCOPY commit (xpol.f 139-176) via the existing session
// use case applyArchivedPolarGeometry. Proves: prompt/selection/
// confirmation contract byte-exact against the LC_ALL=C oracle,
// canonical invalidation vector (LGAMU/LQINU/LWAKE/LQAIJ/LADIJ/
// LWDIJ/LIPAN/LVCONV/LSCINI false; LBLINI only on an N switch),
// buffer/polar/PACC/IDAMP/BLPAR immutability, CPI not left
// stale-valid, EOF paths, session independence.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/polar/PolarTypes.h"
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
using app::cli::handleOperAsetCore;
using app::cli::OperAsetCoreStatus;

void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(
      session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);
}

void addSlot(session::SessionComposition& composition, int designation,
             const std::string& name) {
  const auto naca = core::geometry::naca4(designation, 123);
  CHECK(naca.isOk());
  core::polar::PolarDataset slot;
  slot.metadata.airfoilName = name;
  slot.geometry.x = naca.value().xb;
  slot.geometry.y = naca.value().yb;
  composition.polar.slots.push_back(std::move(slot));
  composition.polar.saveFileNames.emplace_back();
  composition.polar.dumpFileNames.emplace_back();
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

// NPOL==0: canonical message, no mutation.
TEST(unit, OperAsetNoPolarAirfoilsMessage) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  const std::vector<double> panelX = composition.paneling.panel.x;
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "", false) ==
        OperAsetCoreStatus::noPolars);
  CHECK_EQ(out.str(), "\n No polar airfoils are stored\n");
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
}

// NPOL==1: IP=1 without PLRSUM and without the index prompt; directly
// the confirmation prompt; empty answer = default Y.
TEST(unit, OperAsetSingleSlotSkipsSelectionPrompt) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 12, "NACA 0012");
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "", false) ==
        OperAsetCoreStatus::applied);
  CHECK_EQ(out.str(),
           "\n Current airfoil will be overwritten.  Proceed?  Y\n");
  CHECK_EQ(composition.paneling.panel.x.size(), std::size_t{245});
  CHECK_EQ(composition.geometry.name, std::string("NACA 0012"));
}

// NPOL>1: PLRSUM table + index prompt; selection '2' adopts the
// slot contour.
TEST(unit, OperAsetMultipleSlotsSelection) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 2412, "NACA 2412");
  addSlot(composition, 12, "NACA 0012");
  composition.polar.activeSlot = 1;
  std::istringstream in("2\n\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "", false) ==
        OperAsetCoreStatus::applied);
  CHECK(out.str().find("       airfoil                    Re           Mach") !=
        std::string::npos);
  CHECK(out.str().find("Enter index of polar airfoil to set   i>  ") !=
        std::string::npos);
  CHECK_EQ(composition.paneling.panel.x.size(), std::size_t{245});
  CHECK_EQ(composition.geometry.name, std::string("NACA 0012"));
}

// inline index: no PLRSUM, no index prompt.
TEST(unit, OperAsetInlineIndexNoListing) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 2412, "NACA 2412");
  addSlot(composition, 12, "NACA 0012");
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "2", true) ==
        OperAsetCoreStatus::applied);
  CHECK(out.str().find("airfoil                    Re") == std::string::npos);
  CHECK(out.str().find("Enter index") == std::string::npos);
  CHECK_EQ(composition.geometry.name, std::string("NACA 0012"));
}

// Invalid inline index: message, then prompt path with retry;
// '0' leads to a silent return without any mutation.
TEST(unit, OperAsetInvalidInlineIndexRetriesThenZero) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 2412, "NACA 2412");
  addSlot(composition, 12, "NACA 0012");
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t panelRevision = composition.paneling.panelRevision;
  std::istringstream in("0\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "5", true) ==
        OperAsetCoreStatus::abortedByZero);
  CHECK(out.str().find(" Specified polar airfoil does not exist\n") !=
        std::string::npos);
  CHECK(out.str().find("Enter index of polar airfoil to set   i>  ") !=
        std::string::npos);
  CHECK(out.str().find("Proceed?") == std::string::npos);
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK_EQ(composition.paneling.panelRevision, panelRevision);
}

// A non-parseable inline argument corresponds to NINPUT=0 -> prompt path;
// an empty answer at the index prompt keeps the previous IP value 0 ->
// silent return (ASKI quirk).
TEST(unit, OperAsetNonParseableInlineFallsBackToPrompt) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 2412, "NACA 2412");
  addSlot(composition, 12, "NACA 0012");
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "xyz", true) ==
        OperAsetCoreStatus::abortedByZero);
  CHECK(out.str().find("Enter index of polar airfoil to set   i>  ") !=
        std::string::npos);
  CHECK(out.str().find("Proceed?") == std::string::npos);
}

// index 0 at the prompt: silent return, no confirmation prompt.
TEST(unit, OperAsetIndexZeroSilentReturn) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 12, "NACA 0012");
  addSlot(composition, 2412, "NACA 2412");
  std::istringstream in("0\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "", false) ==
        OperAsetCoreStatus::abortedByZero);
  CHECK(out.str().find("Proceed?") == std::string::npos);
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
}

// slot without a stored contour: controlled rejection without
// output and without mutation (Fortran would read out of bounds).
TEST(unit, OperAsetSlotWithoutContourNoMutation) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  core::polar::PolarDataset empty;
  empty.metadata.airfoilName = "EMPTY";
  composition.polar.slots.push_back(std::move(empty));
  composition.polar.saveFileNames.emplace_back();
  composition.polar.dumpFileNames.emplace_back();
  const std::vector<double> panelX = composition.paneling.panel.x;
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "1", true) ==
        OperAsetCoreStatus::slotGeometryUnavailable);
  CHECK_EQ(out.str(),
           "\n Current airfoil will be overwritten.  Proceed?  Y\n");
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
}

// confirmation 'N' and 'n': canonical abort text, no mutation.
TEST(unit, OperAsetCancelWithNAndLowerN) {
  for (const char* answer : {"N", "n"}) {
    session::SessionComposition composition;
    buildNaca2412Paned(composition);
    addSlot(composition, 12, "NACA 0012");
    const std::vector<double> panelX = composition.paneling.panel.x;
    std::istringstream in(std::string(answer) + "\n");
    std::ostringstream out;
    CHECK(handleOperAsetCore(composition, in, out, "1", true) ==
          OperAsetCoreStatus::cancelled);
    CHECK_EQ(out.str(),
             "\n Current airfoil will be overwritten.  Proceed?  Y\n"
             " No action taken\n");
    CHECK(bitsEqual(panelX, composition.paneling.panel.x));
    CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  }
}

// exact APCOPY invalidation vector: all analysis flags false;
// LBLINI is preserved for the same point count.
TEST(unit, OperAsetInvalidationVectorSameNKeepsLblini) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  composition.viscous.viscals.lblini = true;
  const double blparSccon = composition.viscous.blpar.sccon;
  const int idamp = composition.viscous.idamp;
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::size_t slotCount = composition.polar.slots.size();

  // Create a slot with identical panel point count (NOLD == N).
  const auto& panel = composition.paneling.panel;
  core::polar::PolarDataset sameN;
  sameN.metadata.airfoilName = "SAME-N";
  sameN.geometry.x = panel.x;
  sameN.geometry.y = panel.y;
  composition.polar.slots.push_back(std::move(sameN));
  composition.polar.saveFileNames.emplace_back();
  composition.polar.dumpFileNames.emplace_back();

  std::istringstream ain("1\n\n");
  std::ostringstream aout;
  CHECK(handleOperAsetCore(composition, ain, aout, "", false) ==
        OperAsetCoreStatus::applied);

  CHECK(!composition.viscous.systemValid);
  CHECK(!composition.viscous.wakeValid);
  CHECK(!composition.viscous.converged);
  CHECK(!composition.viscous.viscals.mapping.valid);
  CHECK(!composition.viscous.viscals.dijWakeValid);
  CHECK(composition.viscous.viscals.lblini);  // NOLD == N -> preserve
  CHECK_EQ(composition.viscous.blpar.sccon, blparSccon);
  CHECK_EQ(composition.viscous.idamp, idamp);
  CHECK(bitsEqual(bufferX, composition.geometry.buffer.x));
  CHECK_EQ(composition.polar.slots.size(), slotCount + 1);
  CHECK(composition.paneling.panelingValid);
}

// N switch: LBLINI is invalidated.
TEST(unit, OperAsetDifferentNInvalidatesLblini) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  composition.viscous.viscals.lblini = true;
  addSlot(composition, 12, "NACA 0012");  // 245 != 160

  std::istringstream ain("1\n\n");
  std::ostringstream aout;
  CHECK(handleOperAsetCore(composition, ain, aout, "", false) ==
        OperAsetCoreStatus::applied);
  CHECK(!composition.viscous.viscals.lblini);
  CHECK_EQ(composition.paneling.panel.x.size(), std::size_t{245});
}

// After ASET the old CPI is not valid; a new inviscid
// point regenerates CPI on the new contour.
TEST(unit, OperAsetCpiNotStaleValidAfterCommit) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<double> oldCpi = composition.viscous.cpi;
  addSlot(composition, 12, "NACA 0012");
  std::istringstream ain("1\n\n");
  std::ostringstream aout;
  CHECK(handleOperAsetCore(composition, ain, aout, "", false) ==
        OperAsetCoreStatus::applied);
  CHECK(!composition.viscous.systemValid);
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_EQ(composition.viscous.cpi.size(), std::size_t{245});
  CHECK(!bitsEqual(oldCpi, composition.viscous.cpi));
}

// After ASET a new viscous computation works on the new
// contour.
TEST(unit, OperAsetViscousPointAfterCommit) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
  addSlot(composition, 12, "NACA 0012");
  std::istringstream ain("1\n\n");
  std::ostringstream aout;
  CHECK(handleOperAsetCore(composition, ain, aout, "", false) ==
        OperAsetCoreStatus::applied);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(composition.viscous.converged);
}

// Active polar accumulation stays preserved (PACC state unchanged).
TEST(unit, OperAsetKeepsPolarAccumulationState) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 12, "NACA 0012");
  composition.polar.activeSlot = 0;
  std::istringstream in("1\n\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(composition, in, out, "", false) ==
        OperAsetCoreStatus::applied);
  CHECK_EQ(composition.polar.activeSlot, 0);
  CHECK_EQ(composition.polar.slots.size(), std::size_t{1});
}

// EOF at the index prompt and at the confirmation prompt: controlled
// abort without mutation.
TEST(unit, OperAsetEofAtPromptsNoMutation) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addSlot(composition, 12, "NACA 0012");
  addSlot(composition, 2412, "NACA 2412");
  const std::vector<double> panelX = composition.paneling.panel.x;

  std::istringstream eofIndex("");
  std::ostringstream out1;
  CHECK(handleOperAsetCore(composition, eofIndex, out1, "", false) ==
        OperAsetCoreStatus::inputAborted);

  std::istringstream eofConfirm("1\n");
  std::ostringstream out2;
  CHECK(handleOperAsetCore(composition, eofConfirm, out2, "", false) ==
        OperAsetCoreStatus::inputAborted);
  CHECK(bitsEqual(panelX, composition.paneling.panel.x));
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
}

// session independence: ASET in one session leaves the other
// untouched.
TEST(unit, OperAsetIndependentAcrossSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  buildNaca2412Paned(a);
  buildNaca2412Paned(b);
  addSlot(a, 12, "NACA 0012");
  const std::vector<double> bPanel = b.paneling.panel.x;
  std::istringstream in("1\n\n");
  std::ostringstream out;
  CHECK(handleOperAsetCore(a, in, out, "", false) ==
        OperAsetCoreStatus::applied);
  CHECK(bitsEqual(bPanel, b.paneling.panel.x));
  CHECK_EQ(b.geometry.name, std::string("NACA 2412"));
}

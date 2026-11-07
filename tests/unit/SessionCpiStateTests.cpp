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

// File: tests/unit/SessionCpiStateTests.cpp
// session-bound CPI representation (Fortran
// COMMON/CR04 CPI). Proves: adoption from the inviscid/viscous
// core path (C3->C4 bit-identical), active length and panel provenance,
// update for a new alpha point, separation of CPV, behavior for
// PANE/airfoil change/buffer operations/VPAR/Mach, session independence,
// no stale consumption after an error path, and direct mhinge-core
// access on session CPI/session CPV without a CLI state copy.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/Types.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <bit>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using xfoil::app::cli::handleOperAlphaCore;
using xfoil::app::cli::handleOperMachCore;

void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  CHECK(session::applyPaneling(composition).ok);
}

void configure(session::SessionComposition& composition, bool viscous,
               int maximumIterations) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = maximumIterations;
  config.viscous = viscous;
  CHECK(session::configure(composition, config).success());
}

std::uint64_t foldBits(const std::vector<double>& values) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const double v : values) {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(v);
    hash ^= bits;
    hash *= 1099511628211ULL;
  }
  return hash;
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

// C3->C4: the session-side CPI is bit-identical to the
// SpecPointResult.cp produced directly in the core (no dual write, no new computation).
TEST(unit, SessionCpiTransferBitIdenticalToCoreResult) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const auto& panel = composition.paneling.panel;
  const auto& system = composition.viscous.system;
  CHECK(composition.viscous.systemValid);
  const core::inviscid::MachReynoldsSettings settings{1, 1, 0.0, 1.0e6};
  const double alfaRad = 2.0 * 3.14159265358979323846 / 180.0;
  const auto direct = core::inviscid::specAlpha(system, panel, settings,
                                                alfaRad, 1.0, 0.25, 0.0);
  CHECK(direct.isOk());

  CHECK_EQ(composition.viscous.cpi.size(), panel.x.size());
  CHECK(bitsEqual(composition.viscous.cpi, direct.value().cp));
}

// new alpha point updates CPI (Fortran: CPCALC on SPECAL completion).
TEST(unit, SessionCpiUpdatedOnNewAlphaPoint) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream in0;
  std::ostringstream out0;
  CHECK(handleOperAlphaCore(composition, in0, out0, "0") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::uint64_t after0 = foldBits(composition.viscous.cpi);

  std::istringstream in2;
  std::ostringstream out2;
  CHECK(handleOperAlphaCore(composition, in2, out2, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::uint64_t after2 = foldBits(composition.viscous.cpi);
  CHECK_NE(after0, after2);
  CHECK_EQ(composition.viscous.cpi.size(),
           composition.paneling.panel.x.size());
}

// viscous point: CPI and CPV have length N+NW, separate domain
// data; CPI is not overwritten by CPV.
TEST(unit, SessionCpiAndCpvSeparateWithWakeLength) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const std::size_t n = composition.paneling.panel.x.size();
  const std::size_t nw = composition.viscous.wake.x.size();
  CHECK(nw > 0);
  CHECK_EQ(composition.viscous.cpi.size(), n + nw);
  CHECK_EQ(composition.viscous.cpv.size(), n + nw);
  CHECK_FALSE(bitsEqual(composition.viscous.cpi, composition.viscous.cpv));
}

// Pure buffer operations (GDES) do not change current CPI.
TEST(unit, SessionCpiUnchangedByBufferOnlyOperations) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::uint64_t before = foldBits(composition.viscous.cpi);

  CHECK(session::initializeGeometryDesign(composition).status ==
        session::GeometryDesignInitStatus::initialized);
  CHECK(session::gdesSetBufferFromCurrent(composition).ok);
  CHECK(session::gdesRotate(composition, 0.05).ok);
  CHECK_EQ(foldBits(composition.viscous.cpi), before);
}

// VPAR commands do not change CPI (canonically no CPCALC call).
TEST(unit, SessionCpiUnchangedByVparCommands) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::uint64_t before = foldBits(composition.viscous.cpi);

  session::setTransitionParameters(composition, 8.0, 1.0, 1.0);
  session::toggleBoundaryLayerInitialization(composition);
  session::toggleBoundaryLayerInitialization(composition);
  session::setViscousAcceleration(composition, 0.01);
  CHECK_EQ(foldBits(composition.viscous.cpi), before);
}

// PANE (new current geometry from the same buffer) leaves CPI as
// COMMON state unchanged; point validity is expressed through the
// invalidation flags as in Fortran.
TEST(unit, SessionCpiPersistsAcrossPaneButPointInvalidated) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::uint64_t before = foldBits(composition.viscous.cpi);

  CHECK(session::applyPaneling(composition).ok);
  CHECK_EQ(foldBits(composition.viscous.cpi), before);
  // Fortran PANE invalidates the unit solution (LGAMU/LQAIJ false);
  // the consumer must re-analyze before CPI is domain-valid again.
  CHECK_FALSE(composition.viscous.systemValid);
}

// Profile change: CPI is preserved bytewise as COMMON state,
// but the invalidation makes the old analysis unusable.
TEST(unit, SessionCpiStaleAfterProfileChangeButInvalidated) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<double> cpiBefore = composition.viscous.cpi;

  const auto naca = core::geometry::nacaAirfoil(4412);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  CHECK(session::applyPaneling(composition).ok);
  CHECK(bitsEqual(composition.viscous.cpi, cpiBefore));
  CHECK_FALSE(composition.viscous.systemValid);

  // the next analysis replaces CPI by the new contour provenance.
  std::istringstream in2;
  std::ostringstream out2;
  CHECK(handleOperAlphaCore(composition, in2, out2, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_EQ(composition.viscous.cpi.size(),
           composition.paneling.panel.x.size());
  CHECK_FALSE(bitsEqual(composition.viscous.cpi, cpiBefore));
}

// Mach treats CPI according to Fortran (xoper.f:328): the first N
// entries are overwritten with the new MINF.
TEST(unit, SessionCpiUpdatedByMachPerFortranContract) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream inAlfa;
  std::ostringstream outAlfa;
  CHECK(handleOperAlphaCore(composition, inAlfa, outAlfa, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<double> cpiBefore = composition.viscous.cpi;
  CHECK_EQ(composition.viscous.viscals.minf, 0.0);

  std::istringstream inMach("0.3\n");
  std::ostringstream outMach;
  CHECK(handleOperMachCore(composition, inMach, outMach, "") ==
        app::cli::OperMachCoreStatus::applied);
  CHECK(composition.viscous.viscals.minf > 0.0);

  const std::size_t n = composition.paneling.panel.x.size();
  CHECK_EQ(composition.viscous.cpi.size(), n);
  CHECK_FALSE(bitsEqual(composition.viscous.cpi, cpiBefore));

  // bit proof of the Mach CPI: cpCalc(QINV, new MINF) at the
  // canonical digit. QINV of the inviscid point is -- as held in
  // Fortran COMMON -- reconstructed via QISET from the retained
  // alpha=0/90 basis (identical to the SPECAL QINV).
  std::vector<double> qinvRef;
  std::vector<double> qinvARef;
  core::inviscid::qiset(composition.viscous.system.qinvu1,
                        composition.viscous.system.qinvu2,
                        composition.viscous.viscals.alfa, qinvRef,
                        qinvARef);
  const auto expected =
      core::inviscid::cpCalc(qinvRef, composition.operating.qinf,
                             composition.viscous.viscals.minf);
  CHECK(expected.isOk());
  CHECK(bitsEqual(composition.viscous.cpi, expected.value()));
}

// session independence: CPI is instance-specific.
TEST(unit, SessionCpiIndependentAcrossSessions) {
  session::SessionComposition a;
  buildNaca2412Paned(a);
  configure(a, false, 100);
  std::istringstream inA;
  std::ostringstream outA;
  CHECK(handleOperAlphaCore(a, inA, outA, "0") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  session::SessionComposition b;
  buildNaca2412Paned(b);
  configure(b, false, 100);
  std::istringstream inB;
  std::ostringstream outB;
  CHECK(handleOperAlphaCore(b, inB, outB, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  CHECK_NE(foldBits(a.viscous.cpi), foldBits(b.viscous.cpi));
  const std::vector<double> aCpi = a.viscous.cpi;
  std::istringstream inB2;
  std::ostringstream outB2;
  CHECK(handleOperAlphaCore(b, inB2, outB2, "4") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(bitsEqual(a.viscous.cpi, aCpi));
}

// error path: a failed analysis does not partially mutate CPI;
// the retained state stays, and point validity decides.
TEST(unit, SessionCpiNotPartiallyMutatedOnFailedAnalysis) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  std::istringstream inOk;
  std::ostringstream outOk;
  CHECK(handleOperAlphaCore(composition, inOk, outOk, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<double> cpiBefore = composition.viscous.cpi;

  // Controlled error: missing configuration after reset.
  composition.operating.configured = false;
  const auto result =
      session::runOperAlphaPoint(composition, Degrees{0.0}, nullptr);
  CHECK_FALSE(result.successful);
  CHECK(bitsEqual(composition.viscous.cpi, cpiBefore));
}

// the existing mhinge core can take session CPI (LVISC=false) and
// session CPV (LVISC=true) directly as input; the selection
// follows the LVISC contract and the results stay separate.
TEST(unit, SessionCpiCpvFeedMhingeCorePerLviscContract) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const auto& panel = composition.paneling.panel;
  const auto& cpi = composition.viscous.cpi;
  const auto& cpv = composition.viscous.cpv;
  CHECK(cpi.size() >= panel.x.size());
  CHECK(cpv.size() >= panel.x.size());

  const auto inv = core::inviscid::mhinge(panel.x, panel.xp, panel.y,
                                          panel.yp, panel.s, cpi, cpv,
                                          false, 0.7, -0.05);
  const auto vis = core::inviscid::mhinge(panel.x, panel.xp, panel.y,
                                          panel.yp, panel.s, cpi, cpv,
                                          true, 0.7, -0.05);
  CHECK_NE(inv.hmom, vis.hmom);
}

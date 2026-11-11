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

// File: tests/unit/CoreOperSinglePointTests.cpp
// permanent tests of the
// OPER single-point contract kernel (runOperAlphaPoint/
// runOperCliPoint/runOperClPoint). The numerical final states,
// iteration counts, convergence, warm-start flags and the Cp minimum are
// checked pointwise against the canonical Fortran harness reference
// tests/data/core-c11a-state-reference/c11a_oper_singlepoint_harness_reference.txt
// (NACA 2412, PANGEN, Re=1e6, It24.16). The harness transcribes
// the real OPER single-command branches (xoper.f 416-542) with the real
// SPECAL-/SPECCL-/VISCAL routines; the C++ session is the implementation.
// Thresholds abs/rel 1e-9 (coefficients/alpha/Cp), analogous to the existing
// core==Fortran bit-close tests. No tolerance increase, no production
// CLI route, no change to the API-/batch cold path or ASEQ/CSEQ.

#include "TestSupport.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using namespace xfoil;

// NACA 2412 + PANGEN (identical to the harness: NACA4 + PANGEN(.TRUE.)).
void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
}

void configureViscous(session::SessionComposition& composition,
                      int maximumIterations) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = maximumIterations;
  config.viscous = true;
  CHECK(session::configure(composition, config).success());
}

void configureInviscid(session::SessionComposition& composition,
                       int maximumIterations) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = maximumIterations;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());
}

}  // namespace

// A: repeated alpha single point. First point cold (~9 iterations),
// second identical point warm (1 iteration) per harness
// A-Alfa2-KALT/A-Alfa2-WARM. Warm start preserves LBLINI/LIPAN.
TEST(unit, OperAlphaPointRepeatWarmStartMatchesFortran) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  const AnalysisResult first = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.successful);
  CHECK(first.converged);
  // iteration count of the first point pinned as the target contract: with the
  // MRCHDU-recovery TRCHEK (Fortran label 109, xbl.f:1139-1145) the
  // core returns 9 iterations like canonical Fortran; the second identical
  // point 1 iteration. First corrected local variable: AMI at the
  // recovery TRCHEK of station 76, side 2 (T14), AMI = AMPL2.
  CHECK_EQ(first.iterations, 9);
  CHECK_NEAR(first.alpha.value, 2.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(first.liftCoefficient, 4.4955908611657941e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(first.dragCoefficient, 5.7778272636686305e-03, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.cdp, 7.9162164803757602e-04,
             1.0e-9, 1.0e-9);
  // CDF (wall friction): exactly Fortran 4.1154769718219648E-03 after the
  // MRCHDU-recovery-TRCHEK correction (earlier 3.7478771311E-09 delta,
 // cause: missing recovery TRCHEK, xbl.f:1139-1145).
  CHECK_NEAR(composition.viscous.viscals.cdf, 4.1154769718219648e-03,
             1.0e-9, 1.0e-9);
  CHECK_EQ(composition.viscous.viscals.itran[0] + 1, 55);
  CHECK_EQ(composition.viscous.viscals.itran[1] + 1, 75);

 // Critical MRCHDU recovery stations 72-77, side 2: CTAU at the
 // station ends (laminar AMI, turbulent CTI) pinned bit-for-bit against Fortran
 // (machine-precision station comparison, boundaries T0-T14).
  CHECK_NEAR(composition.viscous.viscals.ctau.bottom[71],
             8.1797952462447707e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.ctau.bottom[72],
             8.5897265365526092e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.ctau.bottom[73],
             8.9843290679622640e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.ctau.bottom[74],
             5.7505509485874483e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.ctau.bottom[75],
             6.3510811065289829e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.ctau.bottom[76],
             6.5362315997710391e-02, 1.0e-9, 1.0e-9);

 // warm-start-flags after the first point preserve (LBLINI/LIPAN set).
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.viscals.mapping.valid);
  CHECK(composition.viscous.viscals.lvconv);
  CHECK(composition.viscous.wakeValid);
  CHECK_NEAR(composition.viscous.awake, 3.4906585039886591e-02, 1.0e-9, 1.0e-9);

  const AnalysisResult second = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(second.successful);
  CHECK(second.converged);
  CHECK_EQ(second.iterations, 1);  // warm start: identical point takes 1 iteration
  CHECK_NEAR(second.liftCoefficient, 4.4955908724583621e-01, 1.0e-9, 1.0e-9);
 // LBLINI/LIPAN are preserved across single points (no cold start).
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.viscals.mapping.valid);

 // Cp minimum (FCPMIN) against the harness.
  const session::CpObservation cp = session::currentCpMinimum(composition);
  CHECK(cp.valid);
  CHECK_NEAR(cp.minimum.cpmin, -7.8848483191049712e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(cp.minimum.cpmnv, -7.8848483191049712e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(cp.minimum.cpmni, -8.2368052140388981e-01, 1.0e-9, 1.0e-9);
}

// B1: CLI inviscid (LALFA=true, SPECCL, no SPECAL, no VISCAL).
TEST(unit, OperCliPointInviscidMatchesFortran) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition, 100);

  const AnalysisResult point =
      session::runOperCliPoint(composition, 0.8, nullptr);
  CHECK(point.successful);
  CHECK_EQ(point.iterations, 0);
  CHECK_NEAR(point.alpha.value, 4.5196229276367355e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(point.liftCoefficient, 8.0000000000000082e-01, 1.0e-9, 1.0e-9);
 // inviscid point: no VISCAL, public Cd stays 0 (existing
 // API contract). The CDP produced by SPECCL/CLCALC is an internal
 // inviscid diagnostic value and is not persisted on the inviscid path.
  CHECK_NEAR(point.dragCoefficient, 0.0, 0.0, 0.0);
 // CLI sets LALFA=true (SPECCL with prescribed alpha in the BL Newton).
  CHECK(composition.viscous.lastPointLalfa);
}

// B2: CLI viscous (LALFA=true). Harness B-CLI-VISC.
TEST(unit, OperCliPointViscousMatchesFortran) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  const AnalysisResult point =
      session::runOperCliPoint(composition, 0.8, nullptr);
  CHECK(point.successful);
  CHECK(point.converged);
  CHECK_EQ(point.iterations, 5);
  CHECK_NEAR(point.alpha.value, 4.5196229276367355e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(point.liftCoefficient, 7.6420196157424947e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(point.dragCoefficient, 7.2986530277244846e-03, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.cdp, 1.1373022515193966e-03,
             1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.cdf, 4.7831488500049832e-03,
             1.0e-9, 1.0e-9);
  CHECK(composition.viscous.lastPointLalfa);

  const session::CpObservation cp = session::currentCpMinimum(composition);
  CHECK(cp.valid);
  CHECK_NEAR(cp.minimum.cpmin, -1.5264774550540223e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(cp.minimum.cpmni, -1.6066490609423991e+00, 1.0e-9, 1.0e-9);
}

// C: Cl viscous (LALFA=false). Harness C-CL-VISC. No SPECAL after SPECCL:
// the end state (alpha 4.8999, Cl exact 0.8) distinguishes itself unique
// from the SPECCL+SPECAL-path and from the CLI-path.
TEST(unit, OperClPointViscousMatchesFortran) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  const AnalysisResult point =
      session::runOperClPoint(composition, 0.8, nullptr);
  CHECK(point.successful);
  CHECK(point.converged);
  CHECK_EQ(point.iterations, 5);
  CHECK_NEAR(point.alpha.value, 4.8998618672470515e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(point.liftCoefficient, 8.0000000000000115e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(point.dragCoefficient, 7.6490105239520800e-03, 1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.cdp, 1.2258123056675030e-03,
             1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.viscous.viscals.cdf, 4.9277020026595193e-03,
             1.0e-9, 1.0e-9);
  CHECK(!composition.viscous.lastPointLalfa);

  const session::CpObservation cp = session::currentCpMinimum(composition);
  CHECK(cp.valid);
  CHECK_NEAR(cp.minimum.cpmin, -1.6792712209942491e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(cp.minimum.cpmni, -1.7881131380859041e+00, 1.0e-9, 1.0e-9);
}

// D: CLI vs. Cl with the same target Cl and the same fresh state.
// the different LALFA semantics are observable in the final state; no
// artificial equating.
TEST(unit, OperCliAndClDifferForSameTargetCl) {
  session::SessionComposition cliSession;
  buildNaca2412Paned(cliSession);
  configureViscous(cliSession, 100);
  const AnalysisResult cli = session::runOperCliPoint(cliSession, 0.8, nullptr);

  session::SessionComposition clSession;
  buildNaca2412Paned(clSession);
  configureViscous(clSession, 100);
  const AnalysisResult cl = session::runOperClPoint(clSession, 0.8, nullptr);

  CHECK(cli.successful);
  CHECK(cl.successful);
 // CLI (LALFA=true): alpha 4.5196, viscous CL decreasing (0.7642).
 // Cl (LALFA=false): alpha 4.8999, Cl held at CLSPEC (0.8000).
  CHECK(cli.alpha.value < cl.alpha.value);
  CHECK(cli.liftCoefficient < cl.liftCoefficient);
  CHECK_NEAR(cli.alpha.value, 4.5196229276367355e+00, 1.0e-9, 1.0e-9);
  CHECK_NEAR(cl.alpha.value, 4.8998618672470515e+00, 1.0e-9, 1.0e-9);
}

// E: non-convergence with ITMAX=1. The point is executed; successful and
// converged are correctly separated; no NSEQEX single-point halt (single command).
TEST(unit, OperAlphaPointNonConvergenceSeparatesSuccessfulAndConverged) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 1);

  const AnalysisResult point = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(point.successful);
  CHECK(!point.converged);
  CHECK_EQ(point.iterations, 1);  // VISCAL(ITMAX=1), not ITMAX+5
  CHECK_NEAR(point.liftCoefficient, 4.9165931220531012e-01, 1.0e-9, 1.0e-9);
}

// Conditional invalidation: an alpha change invalidates wake/LVCONV but
// preserves LBLINI/LIPAN (warm start). MINF/MVISC involvement via lvconv.
TEST(unit, OperAlphaPointConditionalInvalidationPreservesLipaniLblini) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  const AnalysisResult first = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.converged);
  CHECK(composition.viscous.wakeValid);
  CHECK(composition.viscous.viscals.lvconv);
  const double awakeAfterFirst = composition.viscous.awake;

 // A larger alpha change invalidates wake and LVCONV
 // but preserves LBLINI/LIPAN (warm start, no cold start).
  const AnalysisResult second = session::runOperAlphaPoint(
      composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(second.successful);
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.viscals.mapping.valid);
  CHECK(composition.viscous.wakeValid);
  CHECK(composition.viscous.awake != awakeAfterFirst);
}

// Abort before the analysis is requested returns cancelled without a
// point solution.
TEST(unit, OperSinglePointCancellationIsControlled) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  CancellationToken token;
  token.requestCancellation();
  const AnalysisResult point = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, &token);
  CHECK(!point.successful);
  CHECK(point.status == AnalysisStatus::cancelled);
}

// without valid panel contour: controlled missingGeometry without solution.
TEST(unit, OperSinglePointWithoutPanelingIsControlled) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
 // No applyPaneling -> no valid panel contour.

  const AnalysisResult alpha = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, nullptr);
  const AnalysisResult cli =
      session::runOperCliPoint(composition, 0.8, nullptr);
  const AnalysisResult cl =
      session::runOperClPoint(composition, 0.8, nullptr);
  CHECK(alpha.status == AnalysisStatus::missingGeometry);
  CHECK(cli.status == AnalysisStatus::missingGeometry);
  CHECK(cl.status == AnalysisStatus::missingGeometry);
}

// ABCOPY and PANGEN panels are consumed exactly; no geometry-
// or panel mutation by the single points.
TEST(unit, OperSinglePointDoesNotMutateGeometryOrPaneling) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);
  const std::size_t panelCount = composition.paneling.panel.x.size();
  const auto origin = session::panelingOrigin(composition);

  CHECK(session::runOperAlphaPoint(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  CHECK(session::runOperCliPoint(composition, 0.8, nullptr).successful);
  CHECK(session::runOperClPoint(composition, 0.8, nullptr).successful);

  CHECK_EQ(composition.paneling.panel.x.size(), panelCount);
  CHECK(session::panelingOrigin(composition) == origin);
}

// Independent sessions: one run does not change the state of another.
TEST(unit, OperSinglePointIndependentSessions) {
  session::SessionComposition a;
  buildNaca2412Paned(a);
  configureViscous(a, 100);
  session::SessionComposition b;
  buildNaca2412Paned(b);
  configureViscous(b, 100);

  const AnalysisResult ra =
      session::runOperAlphaPoint(a, xfoil::Degrees{2.0}, nullptr);
  const AnalysisResult rb =
      session::runOperAlphaPoint(b, xfoil::Degrees{2.0}, nullptr);
  CHECK_NEAR(ra.liftCoefficient, rb.liftCoefficient, 1.0e-12, 1.0e-12);
  CHECK_EQ(ra.iterations, rb.iterations);
}

// API/batch cold path stays unchanged: runSpecAlpha does not persist wake/
// LVCONV (cold start) and repeated points remain cold.
TEST(unit, ApiRunSpecAlphaColdStartUnchanged) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  const AnalysisResult first =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.successful);
 // the cold path does not persist the OPER warm-start fields.
  CHECK(!composition.viscous.wakeValid);
  const AnalysisResult second =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(second.successful);
 // cold start: the identical point re-iterates again (no warm start).
  CHECK_EQ(second.iterations, first.iterations);
}

// XYWAKE correction: after the SETEXP/intPower correction, the wake
// trajectory of the scene (NACA 2412, PANGEN, Re=1e6, alpha 2) is
// bit-identical to the canonical Fortran XYWAKE routine (xpanel.f
// 1270-1424). Reference: throwaway Fortran probe (fr_xwake probe,
// canonical DBL flags); XYWAKE nodes 1-14 are verified bit-exactly.
// XSSI(91,2) then follows deterministically via XICALC (wake
// coordinates bit-identical, XICALC structurally faithful).
TEST(unit, XywakeNodes14BitIdenticalToFortran) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureViscous(composition, 100);

  const AnalysisResult first = session::runOperAlphaPoint(
      composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.converged);
  CHECK(composition.viscous.wakeValid);
  CHECK_EQ(composition.viscous.wake.nw, static_cast<std::size_t>(23));

 // Fortran-verified bits (XYWAKE nodes 1-14, X/Y):
  const std::array<std::uint64_t, 14> expectedX{
      0x3FF00068A258D200, 0x3FF022AEF8BA8717, 0x3FF049C1506D7E31,
      0x3FF076476A0C04EA, 0x3FF0A9034D1FC0B8, 0x3FF0E2D23CFD2D38,
      0x3FF124B06381A2A4, 0x3FF16FBD1333F807, 0x3FF1C53FA6E4C825,
      0x3FF226AD1119CD15, 0x3FF295AE327E1F2C, 0x3FF31427128A518D,
      0x3FF3A43F19AAD0BB, 0x3FF4486A70A8FF54};
  const std::array<std::uint64_t, 14> expectedY{
      0xBEDB5DC6FCFBE830, 0xBF41976BC61DAB64, 0xBF511248D642F37A,
      0xBF59A4307A17F6A7, 0xBF614A9D41F4C14E, 0xBF65F650DF3F4F33,
      0xBF6AD52B542D9A6A, 0xBF6FE40CD11ECD92, 0xBF728E55691D3DA2,
      0xBF753A9A0C96FF1A, 0xBF77EFE39CEE2929, 0xBF7AA4E323835C88,
      0xBF7D4D85F641E2D2, 0xBF7FDA81E3BF9C1E};
  for (std::size_t iw = 0; iw < 14; ++iw) {
    CHECK_EQ(std::bit_cast<std::uint64_t>(composition.viscous.wake.x[iw]),
             expectedX[iw]);
    // PUB-W2: on Linux (exact platform) raw bits as before. On
    // Windows/MSYS2 the acceptance run measured the y-node deviations
    // with full bit patterns: 15..302 ULP (monotone IEEE-754 order,
    // all same-sign); class kUlpBudget384 (margin 1.27x) is the
    // documented portable bound. x nodes stay bitwise.
    testing::checkPortableNumeric(
        composition.viscous.wake.y[iw],
        std::bit_cast<double>(expectedY[iw]),
        testing::PortableNumericClass::kUlpBudget384,
        "composition.viscous.wake.y[iw]", "expectedY[iw]", __FILE__,
        __LINE__);
  }
}

// end: tests/unit/CoreOperSinglePointTests.cpp

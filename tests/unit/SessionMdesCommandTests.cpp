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

// File: tests/unit/SessionMdesCommandTests.cpp
// unit tests for the MDES command
// use cases (QSET/AQ/CQ/SYMM/TGAP/TANG/FILT/SMOO/SLOP/VISC/REFL).
// direct Fortran anchors: canonical CLI transcripts
// (LC_ALL=C fortran-bin/xfoil, three identical runs) for FORMAT 1200/
// 1210 lines; numerical kernels exclusively in the inverse core (CNFILT/PIQSUM/
// CNCALC/QSPCIR/SMOOQ/QSPINT). No inverse numerics are recomputed in
// the tests.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

using session::SessionComposition;

constexpr double kDtor = std::numbers::pi / 180.0;

// FORMAT field rounding: F11.6 -> half width 5e-7, F9.4 -> 5e-5,
// F7.3 -> 5e-4. These are anchor widths, not tolerance relaxations:
// the comparison is against the rounded Fortran output.
constexpr double kF6Abs = 5.0e-7 + 1.0e-12;
constexpr double kF4DegAbs = 5.0e-5 + 1.0e-9;

void setupPangenAirfoil(SessionComposition& s, int designation) {
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(s);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(s, params).ok);
  std::ostringstream out;
  testing::handleLegacyNacaCore(s, out, designation);
  CHECK(s.paneling.panelingValid);
}

void configureOper(SessionComposition& s) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(s, config).success());
}

// canonical MDES entry with label-1 initialization.
void enterMdes(SessionComposition& s) {
  const auto entry = session::enterMdesMenuState(s);
  CHECK(entry.status == session::InverseMenuEntryStatus::entered);
  const auto init = session::initializeMdesNumerics(s);
  CHECK(init.isOk());
  CHECK(init.qspecRebuilt);
}

}  // namespace

// Precondition: without the numerical INIT state the command
// use-cases do not mutate (xmdes.f label 1 before the menu loop).
TEST(unit, MdesCommandsRequireInitializedNumerics) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);

  session::MdesSpectrumReport report;
  CHECK(session::mdesQspecReset(s) == session::MdesCommandStatus::notInitialized);
  CHECK(session::mdesSetQspecAlphas(s, {0.1}) ==
        session::MdesCommandStatus::notInitialized);
  CHECK(session::mdesSetQspecCls(s, {0.5}) ==
        session::MdesCommandStatus::notInitialized);
  bool sym = false;
  CHECK(session::mdesToggleSymmetry(s, sym) ==
        session::MdesCommandStatus::notInitialized);
  CHECK(!sym);
  CHECK(!s.inverse.flags.lqSym);
  CHECK(session::mdesSetTeGap(s, 0.001, 0.002) ==
        session::MdesCommandStatus::notInitialized);
  CHECK(session::mdesSetTeAngle(s, 16.0) ==
        session::MdesCommandStatus::notInitialized);
  CHECK(session::mdesApplyFilter(s, report) ==
        session::MdesCommandStatus::notInitialized);
  CHECK(session::mdesSmoothTargetSegment(s, report) ==
        session::MdesCommandStatus::notInitialized);
  CHECK_EQ(s.inverse.shared.nqsp, 0);
  CHECK(s.inverse.shared.qspecs.empty());
}

// canonical sequence (Fortran CLI anchor, three times identical):
// NACA 2412 / MDES / INIT / QSET / AQ 5 10 / CQ 0.5 / SYMM / S /
// TGAP 0.001 0.002 / TANG 16.0 0 / FILT
// output (excerpt):
// current : alpha = 0.0000 CL = 0.255075 CM = -0.055687
// qspec 1 : alpha = 1.9278 CL = 0.500000 CM = -0.009962
TEST(unit, MdesCommandSequenceMatchesFortranTextAnchor) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  // INIT command (xmdes.f 199-202): enforces the mapping path.
  const auto reinit = session::initializeMdesNumerics(s, true);
  CHECK(reinit.isOk());
  CHECK(reinit.qspecRebuilt);

  CHECK(session::mdesQspecReset(s) == session::MdesCommandStatus::ok);
  CHECK(session::mdesSetQspecAlphas(s, {5.0 * kDtor, 10.0 * kDtor}) ==
        session::MdesCommandStatus::ok);
  CHECK_EQ(s.inverse.shared.nqsp, 2);
  CHECK_EQ(s.inverse.shared.iacqsp, 1);
  CHECK_NEAR(s.inverse.shared.qspecs[0].al, 5.0 * kDtor, 0.0, 0.0);
  CHECK_NEAR(s.inverse.shared.qspecs[1].al, 10.0 * kDtor, 0.0, 0.0);

  CHECK(session::mdesSetQspecCls(s, {0.5}) == session::MdesCommandStatus::ok);
  CHECK_EQ(s.inverse.shared.nqsp, 1);
  CHECK_EQ(s.inverse.shared.iacqsp, 2);

  bool sym = false;
  CHECK(session::mdesToggleSymmetry(s, sym) == session::MdesCommandStatus::ok);
  CHECK(sym);
  CHECK(s.inverse.flags.lqSym);
  CHECK(session::mdesToggleSymmetry(s, sym) == session::MdesCommandStatus::ok);
  CHECK(!sym);
  CHECK(!s.inverse.flags.lqSym);

  CHECK(session::mdesSetTeGap(s, 0.001, 0.002) ==
        session::MdesCommandStatus::ok);
  CHECK_NEAR(s.inverse.shared.circle.dzte.real(), 0.001, 0.0, 0.0);
  CHECK_NEAR(s.inverse.shared.circle.dzte.imag(), 0.002, 0.0, 0.0);
  CHECK(session::mdesSetTeAngle(s, 16.0) == session::MdesCommandStatus::ok);
  CHECK_NEAR(s.inverse.shared.circle.agte, 16.0 / 180.0, 0.0, 0.0);

  session::MdesSpectrumReport report;
  CHECK(session::mdesApplyFilter(s, report) == session::MdesCommandStatus::ok);
  CHECK_NEAR(report.algam, 0.0, 0.0, kF4DegAbs);
  CHECK_NEAR(report.clgam, 0.255075, kF6Abs, kF6Abs);
  CHECK_NEAR(report.cmgam, -0.055687, kF6Abs, kF6Abs);
  CHECK_EQ(report.columns.size(), 1u);
  CHECK_EQ(report.columns[0].kqsp1, 1);
  CHECK_NEAR(report.columns[0].alfa / kDtor, 1.9278, kF4DegAbs, kF4DegAbs);
  CHECK_NEAR(report.columns[0].cl, 0.500000, kF6Abs, kF6Abs);
  CHECK_NEAR(report.columns[0].cm, -0.009962, kF6Abs, kF6Abs);
}

// AQ/CQ boundaries: IPX=5 (XFOIL.INC 23); an empty requirement list
// takes the canonical <return> path without mutation (xmdes.f 236/269).
TEST(unit, MdesQspecColumnLimitsAndEmptyReturn) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  const std::vector<double> before = [s] {
    std::vector<double> out;
    for (const auto& d : s.inverse.shared.qspecs) {
      out.push_back(d.al);
    }
    return out;
  }();
  const int nqspBefore = s.inverse.shared.nqsp;

  CHECK(session::mdesSetQspecAlphas(s, {}) == session::MdesCommandStatus::ok);
  CHECK_EQ(s.inverse.shared.nqsp, nqspBefore);
  CHECK_EQ(s.inverse.shared.qspecs.size(), before.size());

  std::vector<double> six;
  for (int k = 0; k < 6; ++k) {
    six.push_back((k + 1) * kDtor);
  }
  CHECK(session::mdesSetQspecAlphas(s, six) == session::MdesCommandStatus::ok);
  CHECK_EQ(s.inverse.shared.nqsp, 5);
  CHECK_EQ(s.inverse.shared.qspecs.size(), 5u);
}

// S19: 6.996 SMOO always starts from QSPEC(:,1) (xmdes.f 445: KQSP=1).
// MDES without a prior QDES run leaves KQTARG at its canonical 0 alias,
// but QGAMM stays unchanged and the first Qspec column is smoothed.
TEST(unit, SmooTargetS19WithoutQdesSmoothsFirstQspec) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  CHECK_EQ(s.inverse.shared.kqtarg, -1);
  const auto qspecBefore = s.inverse.shared.qspecs[0].q;
  const auto qgammBefore = s.inverse.shared.qgamm;
  session::MdesSpectrumReport report;
  CHECK(session::mdesSmoothTargetSegment(s, report) ==
        session::MdesCommandStatus::ok);
  CHECK_EQ(report.columns.size(), 1u);
  CHECK(s.inverse.shared.qgamm == qgammBefore);
  CHECK(s.inverse.shared.qspecs[0].q != qspecBefore);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::currentQ);
}

// S19: KQTARG no longer selects the SMOO column. Two identical MDES
// states with different KQTARG values must produce identical QSPEC
// state because both smooth QSPEC(:,1).
TEST(unit, SmooTargetS19IgnoresKqtarg) {
  SessionComposition zero;
  setupPangenAirfoil(zero, 2412);
  configureOper(zero);
  enterMdes(zero);
  SessionComposition two;
  setupPangenAirfoil(two, 2412);
  configureOper(two);
  enterMdes(two);
  const std::vector<double> alphas{5.0 * kDtor, 10.0 * kDtor};
  CHECK(session::mdesSetQspecAlphas(zero, alphas) ==
        session::MdesCommandStatus::ok);
  CHECK(session::mdesSetQspecAlphas(two, alphas) ==
        session::MdesCommandStatus::ok);
  zero.inverse.shared.kqtarg = 0;
  two.inverse.shared.kqtarg = 1;
  session::MdesSpectrumReport zeroReport;
  session::MdesSpectrumReport twoReport;
  CHECK(session::mdesSmoothTargetSegment(zero, zeroReport) ==
        session::MdesCommandStatus::ok);
  CHECK(session::mdesSmoothTargetSegment(two, twoReport) ==
        session::MdesCommandStatus::ok);
  CHECK_EQ(zeroReport.columns.size(), twoReport.columns.size());
  CHECK_EQ(zeroReport.columns.size(), 2u);
  CHECK(zero.inverse.shared.qgamm == two.inverse.shared.qgamm);
  CHECK_EQ(zero.inverse.shared.qspecs.size(),
           two.inverse.shared.qspecs.size());
  for (std::size_t k = 0; k < zero.inverse.shared.qspecs.size(); ++k) {
    const auto& a = zero.inverse.shared.qspecs[k];
    const auto& b = two.inverse.shared.qspecs[k];
    CHECK(a.q == b.q);
    CHECK_NEAR(a.al, b.al, 0.0, 0.0);
    CHECK_NEAR(a.cl, b.cl, 0.0, 0.0);
    CHECK_NEAR(a.cm, b.cm, 0.0, 0.0);
  }
}

// state commands: SLOP/VISC/REFL clamp only flags
// (xmdes.f 316-341, 508-518); INIT defaults LQSLOP=.TRUE.
// (xfoil.f 502), LQVDES/LQREFL=.FALSE. (xfoil.f 467/469).
TEST(unit, MdesStateTogglesOnlyFlags) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  CHECK(s.inverse.flags.lqSlop);
  CHECK(!s.inverse.flags.lqVdes);
  CHECK(!s.inverse.flags.lqRefl);

  CHECK(!session::mdesToggleSlopeMatch(s));
  CHECK(session::mdesToggleQvisOverlay(s));
  CHECK(session::mdesToggleReflOverlay(s));
  CHECK(!s.inverse.flags.lqSlop);
  CHECK(s.inverse.flags.lqVdes);
  CHECK(s.inverse.flags.lqRefl);

  // No numerics/panel mutation by the flags.
  CHECK(s.paneling.panelingValid);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::present);
}

// No geometry/panel/analysis mutation by MDES commands:
// panel revision, panel quantities and analysis validity stay preserved.
TEST(unit, MdesCommandsNeverMutateGeometryOrAnalysis) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  const std::size_t n = s.paneling.panel.x.size();
  const std::uint64_t revision = s.inverse.provenance.sourcePanelRevision;
  const double alfa = s.viscous.viscals.alfa;
  const double minf = s.viscous.viscals.minf;
  const double reinf = s.viscous.viscals.reinf;

  (void)session::mdesQspecReset(s);
  (void)session::mdesSetQspecAlphas(s, {5.0 * kDtor});
  (void)session::mdesSetQspecCls(s, {0.5});
  session::MdesSpectrumReport report;
  (void)session::mdesApplyFilter(s, report);
  (void)session::mdesSetTeGap(s, 0.001, 0.002);
  (void)session::mdesSetTeAngle(s, 16.0);
  (void)session::mdesToggleSlopeMatch(s);
  (void)session::mdesToggleQvisOverlay(s);
  (void)session::mdesToggleReflOverlay(s);

  CHECK_EQ(s.paneling.panel.x.size(), n);
  CHECK_EQ(s.inverse.provenance.sourcePanelRevision, revision);
  CHECK_NEAR(s.viscous.viscals.alfa, alfa, 0.0, 0.0);
  CHECK_NEAR(s.viscous.viscals.minf, minf, 0.0, 0.0);
  CHECK_NEAR(s.viscous.viscals.reinf, reinf, 0.0, 0.0);
}

// Two independent sessions deliver the same final state
// (determinism, no hidden COMMON coupling).
TEST(unit, MdesCommandSessionsAreIndependent) {
  auto run = [] {
    SessionComposition s;
    setupPangenAirfoil(s, 2412);
    configureOper(s);
    enterMdes(s);
    (void)session::initializeMdesNumerics(s, true);
    (void)session::mdesQspecReset(s);
    (void)session::mdesSetQspecAlphas(s, {5.0 * kDtor, 10.0 * kDtor});
    (void)session::mdesSetQspecCls(s, {0.5});
    session::MdesSpectrumReport report;
    (void)session::mdesApplyFilter(s, report);
    return report;
  };
  const auto a = run();
  const auto b = run();
  CHECK_EQ(a.columns.size(), b.columns.size());
  CHECK_NEAR(a.clgam, b.clgam, 0.0, 0.0);
  CHECK_NEAR(a.cmgam, b.cmgam, 0.0, 0.0);
  CHECK_NEAR(a.columns[0].alfa, b.columns[0].alfa, 0.0, 0.0);
  CHECK_NEAR(a.columns[0].cl, b.columns[0].cl, 0.0, 0.0);
  CHECK_NEAR(a.columns[0].cm, b.columns[0].cm, 0.0, 0.0);
}

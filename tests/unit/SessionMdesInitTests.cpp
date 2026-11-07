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

// file: tests/unit/SessionMdesInitTests.cpp
// unit tests for the numerical MDES
// initialization-path use case initializeMdesNumerics.
// reference: tests/data/core-inverse-design-reference/
// mdes_init_matrix_reference.txt (new Fortran harness
// mdes_init_matrix_harness.f with canonical guard orchestration
// xmdes.f 63-107 and panel sources ABCOPY (xgdes.f 961) / PANGEN
// (xfoil.f 1623); cases F/A/D/E/B/C/G).
// the session state after the commit is compared; no
// inverse numerics are recomputed in the test.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreOperHandlers.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace session = xfoil::session;

using session::SessionComposition;

// Tolerance contract: the MDES INIT
// matrix harness is canonically built with -fdefault-real-8 -fdefault-
// double-8 (as fortran-bin/xfoil); the active
// PANGEN/MDES-INIT path contains no explicit REAL*4/
// COMPLEX*8 (xfoil.f, xpanel.f, xmdes.f, xgeom.f, xgdes.f, xqdes.f,
// spline.f, naca.f, xsolve.f, XFOIL.INC, XDES.INC, CIRCLE.INC: only
// unqualified REAL/COMPLEX, effectively REAL(8)/COMPLEX(16)).
// the double core is compared against the REAL(8) reference; the
// tolerances (1e-5 rel/abs, AGTE 1e-4 abs) apply to all fields,
// including the PANGEN cases.
// No additional precision correction is applied.
constexpr double kRel = 1.0e-5;
constexpr double kAbs = 1.0e-5;
constexpr double kPrecAbs = 1.0e-4;

// buffer contour as Fortran NACA4(NSIDE=123) after ABCOPY removal of the
// duplicate point (the core NACA returns NB=245 directly).
void setupAbcopyAirfoil(SessionComposition& s, int designation) {
  const auto naca = core::geometry::nacaAirfoil(designation);
  CHECK(naca.isOk());
  const auto set = session::setBufferGeometry(s, naca.value().buffer);
  CHECK(set.success());
  const auto copy = session::copyBufferToCurrent(s);
  CHECK(copy.ok);
  CHECK(s.paneling.panelingValid);
  CHECK(s.paneling.origin == session::PanelingOrigin::abcopy);
}

// Clark Y buffer from the existing test input file (the same file
// that the new Fortran matrix harness reads via CLARKY_DAT).
void setupClarkYAbcopy(SessionComposition& s) {
  std::ifstream in(
      "tests/fortran-reference/cases/file-export/"
      "dmp-visc-clark-y/input/CLARK_Y.dat");
  CHECK(in.is_open());
  std::string name;
  std::getline(in, name);
  std::vector<double> x;
  std::vector<double> y;
  double xv = 0.0;
  double yv = 0.0;
  while (in >> xv >> yv) {
    x.push_back(xv);
    y.push_back(yv);
  }
  CHECK_EQ(x.size(), 151u);
  core::geometry::AirfoilBuffer buffer(std::move(x), std::move(y), "CLARK Y");
  const auto set = session::setBufferGeometry(s, std::move(buffer));
  CHECK(set.success());
  const auto copy = session::copyBufferToCurrent(s);
  CHECK(copy.ok);
  CHECK_EQ(s.paneling.panel.x.size(), 151u);
}

// PANGEN contour via the existing NACA production commit (buffer+PANGEN).
void setupPangenAirfoil(SessionComposition& s, int designation) {
  std::ostringstream out;
  cli::handleNacaCore(s, out, designation);
  CHECK(s.paneling.panelingValid);
  CHECK(s.paneling.origin == session::PanelingOrigin::pangen);
}

void setAlfa(SessionComposition& s, double alfa) {
  s.viscous.viscals.alfa = alfa;
}

// production OPER configuration path (INIT defaults: viscous, ITMAX=100,
// Re=1e6 as in the OPER handler tests).
void configureOper(SessionComposition& s) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(s, config).success());
}

}  // namespace

// Missing contour: noAirfoil, no state transition (xmdes.f 53-57).
TEST(unit, MdesInitNoAirfoilLeavesStateUnchanged) {
  SessionComposition s;
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::noAirfoil);
  const auto& inv = s.inverse;
  CHECK_EQ(inv.shared.nsp, 0);
  CHECK_EQ(inv.shared.nqsp, 0);
  CHECK(inv.shared.qspecs.empty());
  CHECK(inv.validity.grid == session::CircleGridValidity::notInitialized);
  CHECK(inv.validity.mapping ==
        session::MappingCoordinateValidity::neverInitialized);
  CHECK(inv.validity.qspec == session::QSpecValidity::absent);
  CHECK_EQ(inv.provenance.sourcePanelRevision, 0u);
}

// NACA 0012, ABCOPY: complete INIT path with Fortran values
// (reference MI/MO/MQ/MC/MZ/MN case A).
TEST(unit, MdesInitAbcopyNaca0012MatchesFortran) {
  SessionComposition s;
  setupAbcopyAirfoil(s, 12);
  setAlfa(s, 0.1);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  const auto& inv = s.inverse;
  // active lengths and flags (MI case A: NSP 257, NQSP 1, IAC 1,
  // IQ1 1, IQ2 257, LEIW/LSCINI/LQSPEC 1).
  CHECK_EQ(inv.shared.nsp, 257);
  CHECK_EQ(inv.shared.nqsp, 1);
  CHECK_EQ(inv.shared.iacqsp, 1);
  CHECK_EQ(inv.shared.target.iq1, 0);   // Fortran IQ1=1
  CHECK_EQ(inv.shared.target.iq2, 256); // Fortran IQ2=257
  CHECK(inv.validity.grid == session::CircleGridValidity::initialized);
  CHECK(inv.validity.mapping ==
        session::MappingCoordinateValidity::initializedFromPanel);
  CHECK(inv.validity.qspec == session::QSpecValidity::present);
  CHECK(inv.validity.qspecOrigin == session::QSpecOrigin::currentQ);
  // operating point (MO case A).
  CHECK_NEAR(inv.shared.algam, 1.000000000000e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.clgam, 6.899172243327e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.cmgam, -7.774319900915e-3, kAbs, kRel);
  CHECK_NEAR(inv.shared.ssple, 5.000000000000e-1, kAbs, kRel);
  // target-Q column 1 (MQ case A; ALQSP unchanged, Cl/CM new).
  CHECK_EQ(inv.shared.qspecs.size(), 1u);
  CHECK_NEAR(inv.shared.qspecs[0].al, 1.000000000000e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.qspecs[0].cl, 6.899175856786e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.qspecs[0].cm, -7.774323979645e-3, kAbs, kRel);
  CHECK_EQ(inv.shared.qspecs[0].q.size(), 257u);
  // Circle grid and mapping control values (MC/MZ/MN case A).
  CHECK_EQ(inv.shared.circle.grid.nc, 257);
  CHECK_EQ(inv.shared.circle.grid.mc, 64);
  CHECK_EQ(inv.shared.circle.grid.mct, 16);
  CHECK_NEAR(inv.shared.circle.grid.dwc, 2.454369260617e-2, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.agte, 8.874458431536e-2, kPrecAbs, kRel);
  CHECK_NEAR(inv.shared.circle.ag0, -1.710195993860e0, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.qimold, 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(inv.shared.circle.dzte.real(), 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(inv.shared.circle.dzte.imag(), 2.520000000000e-3, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.chordz.real(), 1.0, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.coeff.cn[1].real(), 9.098052586528e-1,
             kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.coeff.cn[2].real(), -3.527362019525e-1,
             kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.coeff.cn[3].real(), 1.929581326105e-1,
             kAbs, kRel);
  // Provenance: bound to the current contour revision (ABCOPY).
  CHECK_EQ(inv.provenance.sourcePanelRevision, s.paneling.panelRevision);
  CHECK_EQ(inv.provenance.sourceBufferRevision,
           s.paneling.panelSourceRevision);
  CHECK(inv.provenance.sourcePanelOrigin == session::PanelingOrigin::abcopy);
}

// NACA 2412, PANGEN: complete INIT path (reference case B).
TEST(unit, MdesInitPangenNaca2412MatchesFortran) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  setAlfa(s, 0.1);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  const auto& inv = s.inverse;
  CHECK_EQ(inv.shared.nsp, 257);
  CHECK_NEAR(inv.shared.clgam, 9.436642107898e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.cmgam, -6.398714307204e-2, kAbs, kRel);
  CHECK_NEAR(inv.shared.ssple, 5.030178048383e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.qspecs[0].cl, 9.436645359816e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.qspecs[0].cm, -6.398716786548e-2, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.agte, 8.836103890524e-2, kPrecAbs, kRel);
  CHECK_NEAR(inv.shared.circle.ag0, -1.774886202582e0, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.coeff.cn[1].real(), 9.101904684057e-1,
             kAbs, kRel);
  CHECK(inv.provenance.sourcePanelOrigin == session::PanelingOrigin::pangen);
  CHECK_EQ(inv.provenance.sourcePanelRevision, s.paneling.panelRevision);
}

// Clark Y, ABCOPY: complete INIT path (reference case C).
TEST(unit, MdesInitClarkYAbcopyMatchesFortran) {
  SessionComposition s;
  setupClarkYAbcopy(s);
  setAlfa(s, 0.1);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  const auto& inv = s.inverse;
  CHECK_EQ(inv.shared.nsp, 257);
  CHECK_NEAR(inv.shared.clgam, 1.103794973902e0, kAbs, kRel);
  CHECK_NEAR(inv.shared.cmgam, -9.715617345457e-2, kAbs, kRel);
  CHECK_NEAR(inv.shared.ssple, 5.055925349414e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.dzte.imag(), 1.200000000000e-3, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.chordz.real(), 1.000059902663e0, kAbs, kRel);
  CHECK_NEAR(inv.shared.circle.coeff.cn[1].real(), 9.127486653189e-1,
             kAbs, kRel);
}

// Repeated INIT: SCINIT/CNCALC/QSPCIR do not apply, MAPGAM runs again
// (reference case D: CLGAM/CMGAM from the second MAPGAM round).
TEST(unit, MdesInitRepeatedRefreshedMatchesFortran) {
  SessionComposition s;
  setupAbcopyAirfoil(s, 12);
  setAlfa(s, 0.1);
  const auto first = session::initializeMdesNumerics(s);
  CHECK(first.status == session::MdesNumericsInitStatus::initialized);
  const auto second = session::initializeMdesNumerics(s);
  CHECK(second.status == session::MdesNumericsInitStatus::refreshed);
  const auto& inv = s.inverse;
  CHECK_NEAR(inv.shared.clgam, 6.899175856786e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.cmgam, -7.774323979645e-3, kAbs, kRel);
  CHECK(inv.validity.mapping ==
        session::MappingCoordinateValidity::initializedFromPanel);
  CHECK(inv.validity.qspec == session::QSpecValidity::present);
  CHECK(inv.validity.qspecOrigin == session::QSpecOrigin::currentQ);
  CHECK_EQ(inv.shared.nqsp, 1);
}

// INIT after panel change PANGEN: SCINF path again (reference
// case E: PANGEN on the 0012 buffer, CLGAM of the new contour).
TEST(unit, MdesInitAfterPanelChangeMatchesFortran) {
  SessionComposition s;
  setupAbcopyAirfoil(s, 12);
  setAlfa(s, 0.1);
  const auto first = session::initializeMdesNumerics(s);
  CHECK(first.status == session::MdesNumericsInitStatus::initialized);
  const auto paneled = session::applyPaneling(s);
  CHECK(paneled.ok);
  CHECK(s.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(s.inverse.validity.qspec == session::QSpecValidity::absent);
  // No manual alfa intervention: the productive session contract preserves
  // the CR09 operating-point scalars (alfa/MINF/...) across the
  // panel change (xfoil.f 2078-2089 sets only flags); the setAlfa
  // above, before the first INIT, stays effective.
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  const auto& inv = s.inverse;
  CHECK_NEAR(inv.shared.algam, 1.000000000000e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.clgam, 6.899169324957e-1, kAbs, kRel);
  CHECK_NEAR(inv.shared.cmgam, -7.774118287381e-3, kAbs, kRel);
  CHECK_NEAR(inv.shared.ssple, 5.000000000000e-1, kAbs, kRel);
  CHECK(inv.provenance.sourcePanelOrigin == session::PanelingOrigin::pangen);
  CHECK_EQ(inv.provenance.sourcePanelRevision, s.paneling.panelRevision);
}

// Pure buffer change without EXEC: mapping stays valid, the second
// INIT runs as refreshed (SCINIT does not apply).
TEST(unit, MdesInitAfterBufferOnlyChangeIsRefreshed) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  setAlfa(s, 0.1);
  CHECK(session::initializeMdesNumerics(s).status ==
        session::MdesNumericsInitStatus::initialized);
  (void)session::initializeGeometryDesign(s);
  const auto scaled = session::gdesScale(s, 2.0, 2.0);
  CHECK(scaled.ok);
  CHECK(s.inverse.validity.mapping ==
        session::MappingCoordinateValidity::initializedFromPanel);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::refreshed);
}

// Capacity limit: NC1 > ICX controlled error without
// partial mutation (Fortran EIWSET STOP xmdes.f 1621).
TEST(unit, MdesInitCapacityErrorLeavesStateUnchanged) {
  SessionComposition s;
  setupAbcopyAirfoil(s, 12);
  setAlfa(s, 0.1);
  s.inverse.shared.nc1 = 258;  // ICX+1: EIWSET overflow case
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::capacityError);
  const auto& inv = s.inverse;
  CHECK(inv.validity.grid == session::CircleGridValidity::notInitialized);
  CHECK(inv.validity.mapping ==
        session::MappingCoordinateValidity::neverInitialized);
  CHECK(inv.shared.qgamm.empty());
  CHECK_EQ(inv.shared.nsp, 0);
  CHECK_EQ(inv.shared.nqsp, 0);
  CHECK(inv.shared.qspecs.empty());
  CHECK_EQ(inv.provenance.sourcePanelRevision, 0u);
}

// Two independent sessions: an identical setup returns bit-identical
// values, no shared state.
TEST(unit, MdesInitTwoSessionsDeterministicAndIndependent) {
  SessionComposition a;
  SessionComposition b;
  setupPangenAirfoil(a, 2412);
  setupPangenAirfoil(b, 2412);
  setAlfa(a, 0.1);
  setAlfa(b, 0.1);
  CHECK(session::initializeMdesNumerics(a).isOk());
  CHECK(session::initializeMdesNumerics(b).isOk());
  b.viscous.viscals.alfa = 0.2;
  CHECK(session::initializeMdesNumerics(b).isOk());
  CHECK(a.inverse.shared.clgam != b.inverse.shared.clgam);
  CHECK_NEAR(a.inverse.shared.clgam, 9.436642107898e-1, kAbs, kRel);
  // Determinism: third session identical to a.
  SessionComposition c;
  setupPangenAirfoil(c, 2412);
  setAlfa(c, 0.1);
  CHECK(session::initializeMdesNumerics(c).isOk());
  CHECK_EQ(c.inverse.shared.clgam, a.inverse.shared.clgam);
  CHECK_EQ(c.inverse.shared.circle.coeff.cn[1].real(),
           a.inverse.shared.circle.coeff.cn[1].real());
  CHECK(c.inverse.shared.qspecs[0].q == a.inverse.shared.qspecs[0].q);
}

// panel/buffer/analysis contracts remain unchanged.
TEST(unit, MdesInitDoesNotMutatePanelBufferOrAnalysis) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  setAlfa(s, 0.1);
  const std::vector<double> panelX = s.paneling.panel.x;
  const std::vector<double> bufferX = s.geometry.buffer.x;
  const std::uint64_t panelRev = s.paneling.panelRevision;
  const std::uint64_t bufferRev = s.geometry.bufferRevision;
  const auto statusBefore = s.result.lastResult.status;
  const bool systemValidBefore = s.viscous.systemValid;
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  CHECK(s.paneling.panel.x == panelX);
  CHECK(s.geometry.buffer.x == bufferX);
  CHECK_EQ(s.paneling.panelRevision, panelRev);
  CHECK_EQ(s.geometry.bufferRevision, bufferRev);
  CHECK(s.result.lastResult.status == statusBefore);
  CHECK_EQ(s.viscous.systemValid, systemValidBefore);
}

// Menu/redo states remain unchanged (INIT Label 1 does not write
// them; the QDES lower state stays separate).
TEST(unit, MdesInitLeavesMenuAndRedoStatesUntouched) {
  SessionComposition s;
  setupAbcopyAirfoil(s, 12);
  setAlfa(s, 0.1);
  (void)session::enterMdesMenuState(s);
  s.inverse.mdes.redo = std::make_pair("QSET", "");
  (void)session::enterQdesMenuState(s);
  s.inverse.qdes.redo = std::make_pair("EXEC", "");
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  CHECK(s.inverse.mdes.active);
  CHECK(s.inverse.qdes.active);
  CHECK(s.inverse.mdes.redo.has_value());
  CHECK_EQ(s.inverse.mdes.redo.value().first, std::string("QSET"));
  CHECK(s.inverse.qdes.redo.has_value());
  CHECK_EQ(s.inverse.qdes.redo.value().first, std::string("EXEC"));
}

// NQSP>1: QSPCIR computes all columns (KQSP=1..NQSP); IACQSP=2 uses
// the Cl requirement (Newton, AEPS=5e-7).
TEST(unit, MdesInitRebuildsAllQspecColumns) {
  SessionComposition s;
  setupAbcopyAirfoil(s, 12);
  setAlfa(s, 0.1);
  s.inverse.shared.nqsp = 2;
  s.inverse.shared.iacqsp = 2;
  s.inverse.shared.qspecs.assign(2, core::inverse::QSpecDistribution{});
  s.inverse.shared.qspecs[0].cl = 0.5;
  s.inverse.shared.qspecs[1].cl = 0.8;
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  const auto& inv = s.inverse;
  CHECK_EQ(inv.shared.nqsp, 2);
  CHECK_EQ(inv.shared.iacqsp, 2);
  CHECK_EQ(inv.shared.qspecs.size(), 2u);
  CHECK_EQ(inv.shared.qspecs[0].q.size(), 257u);
  CHECK_EQ(inv.shared.qspecs[1].q.size(), 257u);
  CHECK_NEAR(inv.shared.qspecs[0].cl, 0.5, 1.0e-4, 1.0e-4);
  CHECK_NEAR(inv.shared.qspecs[1].cl, 0.8, 1.0e-4, 1.0e-4);
}

// Provenance follows the contour revision after a PCOP/ABCOPY commit.
TEST(unit, MdesInitProvenanceTracksPanelRevision) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  setAlfa(s, 0.1);
  CHECK(session::initializeMdesNumerics(s).isOk());
  const std::uint64_t rev0 = s.inverse.provenance.sourcePanelRevision;
  CHECK(rev0 > 0);
  const auto copy = session::copyBufferToCurrent(s);
  CHECK(copy.ok);
  CHECK(s.inverse.validity.mapping ==
        session::MappingCoordinateValidity::staleAfterPanelChange);
  CHECK(session::initializeMdesNumerics(s).isOk());
  CHECK(s.inverse.provenance.sourcePanelRevision > rev0);
  CHECK_EQ(s.inverse.provenance.sourcePanelRevision,
           s.paneling.panelRevision);
  CHECK(s.inverse.provenance.sourcePanelOrigin ==
        session::PanelingOrigin::abcopy);
}

// Native sequence (alpha -> PANE -> MDES INIT) without manual
// state repair: alpha is set via the productive OPER alpha path,
// the panel change preserves the CR09 operating-point scalars,
// and MDES INIT reads them. Fortran anchor: fortran-bin/xfoil, sequence
// NACA 2412 / PANE / OPER / ALFA 4 / <exit> / PANE / MDES / INIT / AQ
// returns "current Q operating condition: alpha = 4.000 Cl = 0.7365"
// and "current Qspec alphas = 4.000".
TEST(unit, MdesInitNativeAlfaSurvivesPane) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("");
  std::ostringstream out;
  const auto alpha =
      cli::handleOperAlphaCore(s, in, out, "4");
  CHECK(alpha == cli::OperAlphaCoreStatus::analyzed);
  const double alfaRad = 4.0 * 0.017453292519943295;
  CHECK_NEAR(s.viscous.viscals.alfa, alfaRad, 1.0e-12, 1.0e-12);
  const double minfBefore = s.viscous.viscals.minf;
  const double reinfBefore = s.viscous.viscals.reinf;
  const auto paneled = session::applyPaneling(s);
  CHECK(paneled.ok);
  // operating scalars preserved (xfoil.f 2078-2089 sets only flags),
  // BL/cache fields invalidated.
  CHECK_NEAR(s.viscous.viscals.alfa, alfaRad, 1.0e-12, 1.0e-12);
  CHECK_NEAR(s.viscous.viscals.minf, minfBefore, 0.0, 0.0);
  CHECK_NEAR(s.viscous.viscals.reinf, reinfBefore, 0.0, 0.0);
  CHECK(!s.viscous.systemValid);
  CHECK(s.viscous.viscals.qinv.empty());
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.status == session::MdesNumericsInitStatus::initialized);
  const auto& inv = s.inverse;
  CHECK_NEAR(inv.shared.algam, alfaRad, 1.0e-12, 1.0e-12);
  CHECK_NEAR(inv.shared.qspecs[0].al, alfaRad, 1.0e-12, 1.0e-12);
  CHECK_NEAR(inv.shared.clgam, 7.365e-1, 5.0e-4, 5.0e-4);
}

// Native sequence (Mach -> Alfa -> PANE -> MDES INIT): MINF/REINF
// are set via the productive Mach path and remain after the
// panel change; MDES INIT runs with the retained MINF.
TEST(unit, MdesInitNativeMinfReinfSurvivePane) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  session::setMachNumber(s, 0.3);
  std::istringstream in("");
  std::ostringstream out;
  CHECK(cli::handleOperAlphaCore(s, in, out, "2") ==
        cli::OperAlphaCoreStatus::analyzed);
  CHECK(s.viscous.viscals.minf > 0.0);
  const double minfBefore = s.viscous.viscals.minf;
  const double reinfBefore = s.viscous.viscals.reinf;
  const double alfaBefore = s.viscous.viscals.alfa;
  CHECK(session::applyPaneling(s).ok);
  CHECK_NEAR(s.viscous.viscals.minf, minfBefore, 0.0, 0.0);
  CHECK_NEAR(s.viscous.viscals.reinf, reinfBefore, 0.0, 0.0);
  CHECK_NEAR(s.viscous.viscals.alfa, alfaBefore, 0.0, 0.0);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.isOk());
  CHECK_NEAR(s.inverse.shared.algam, alfaBefore, 1.0e-12, 1.0e-12);
}

// Native sequence (Alfa -> PCOP/ABCOPY -> MDES INIT): the
// ABCOPY commit (xgdes.f 1019-1028 sets only flags) also preserves alfa.
TEST(unit, MdesInitNativeAlfaSurvivesAbcopy) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("");
  std::ostringstream out;
  CHECK(cli::handleOperAlphaCore(s, in, out, "4") ==
        cli::OperAlphaCoreStatus::analyzed);
  const double alfaRad = 4.0 * 0.017453292519943295;
  CHECK(session::copyBufferToCurrent(s).ok);
  CHECK_NEAR(s.viscous.viscals.alfa, alfaRad, 1.0e-12, 1.0e-12);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.isOk());
  CHECK_NEAR(s.inverse.shared.algam, alfaRad, 1.0e-12, 1.0e-12);
}

// Native sequence (Alfa -> GDES EXEC -> MDES INIT): the
// buffer-to-contour commit of EXEC preserves the operating scalars.
TEST(unit, MdesInitNativeAlfaSurvivesGdesExec) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  std::istringstream in("");
  std::ostringstream out;
  CHECK(cli::handleOperAlphaCore(s, in, out, "4") ==
        cli::OperAlphaCoreStatus::analyzed);
  const double alfaRad = 4.0 * 0.017453292519943295;
  (void)session::initializeGeometryDesign(s);
  CHECK(session::gdesScale(s, 1.0, 1.0).ok);
  CHECK(session::executeGeometryDesignBuffer(s).ok);
  CHECK_NEAR(s.viscous.viscals.alfa, alfaRad, 1.0e-12, 1.0e-12);
  const auto r = session::initializeMdesNumerics(s);
  CHECK(r.isOk());
  CHECK_NEAR(s.inverse.shared.algam, alfaRad, 1.0e-12, 1.0e-12);
}

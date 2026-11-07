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

// File: tests/unit/VelsPsilinBitTests.cpp
// bit-for-bit verification of the VELS/PSILIN paths against the focused
// Fortran harness tests/fortran-reference/harnesses/vels/vels_harness.f
// (reference: tests/data/core-fmom-vels-reference/vels_reference.txt,
// -fdefault-real-8 -fdefault-double-8). The following are compared exactly:
// UUU/VVV/QQQ/CPP from the VELS dispatch formula for harness cases
// A1..A5, H, F, G.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Influence.h"
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

struct VelsCase {
  double x{0.0};
  double y{0.0};
  double u{0.0};
  double v{0.0};
  double q{0.0};
  double cp{0.0};
};

struct HarnessValues {
  std::vector<VelsCase> abc;  // case A, points 1..5
  VelsCase h, f, g;
};

std::vector<std::string> tokenize(const std::string& line) {
  std::istringstream tok(line);
  std::vector<std::string> tokens;
  std::string t;
  while (tok >> t) {
    tokens.push_back(t);
  }
  return tokens;
}

void readFour(const std::vector<std::string>& tokens, std::size_t start,
              double& a, double& b, double& c, double& d) {
  a = std::stod(tokens[start]);
  b = std::stod(tokens[start + 1]);
  c = std::stod(tokens[start + 2]);
  d = std::stod(tokens[start + 3]);
}

HarnessValues readHarnessReference() {
  HarnessValues hv;
  std::ifstream in(
      "tests/data/core-fmom-vels-reference/vels_reference.txt");
  CHECK(static_cast<bool>(in));
  // harness points for the ABC lines (vels_harness.f 38-47).
  const double abcX[5] = {0.5, 0.3, 0.5, 0.5, 1.2};
  const double abcY[5] = {0.1, 0.0, 0.2, -0.2, 0.0};
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("VELS ABC U V Q CP", 0) == 0) {
      const std::vector<std::string> t = tokenize(line);
      CHECK_EQ(t.size(), std::size_t{10});
      CHECK(hv.abc.size() < std::size_t{5});
      VelsCase vc;
      vc.x = abcX[hv.abc.size()];
      vc.y = abcY[hv.abc.size()];
      readFour(t, 6, vc.u, vc.v, vc.q, vc.cp);
      hv.abc.push_back(vc);
    } else if (line.rfind("VELS H U V Q CP", 0) == 0) {
      const std::vector<std::string> t = tokenize(line);
      CHECK_EQ(t.size(), std::size_t{10});
      hv.h.x = 0.5;
      hv.h.y = 0.1;
      readFour(t, 6, hv.h.u, hv.h.v, hv.h.q, hv.h.cp);
    } else if (line.rfind("VELS F U V Q CP", 0) == 0) {
      const std::vector<std::string> t = tokenize(line);
      CHECK_EQ(t.size(), std::size_t{10});
      hv.f.x = 0.5;
      hv.f.y = 0.1;
      readFour(t, 6, hv.f.u, hv.f.v, hv.f.q, hv.f.cp);
    } else if (line.rfind("VELS G U V Q CP", 0) == 0) {
      const std::vector<std::string> t = tokenize(line);
      CHECK_EQ(t.size(), std::size_t{10});
      hv.g.x = 0.5;
      hv.g.y = 0.1;
      readFour(t, 6, hv.g.u, hv.g.v, hv.g.q, hv.g.cp);
    }
  }
  CHECK_EQ(hv.abc.size(), std::size_t{5});
  return hv;
}

// PUB-W3: portable contract for harness comparisons. On Linux (exact
// platform) this stays bitwise, as before. On Windows/MSYS2 the third
// acceptance run measured abs maxima 1e-17..3.64e-15 and near-zero
// values with ULP up to 311296 (abs <= 8.9e-16, rel <= 5.9e-11), so
// ULP alone is the wrong instrument near zero. Class
// kVelsReferenceValue (abs <= 1e-14 or rel <= 2e-10; margins 2.7x /
// 3.4x) is the documented contract; violations keep reporting the
// full PORTABLE-NUMERIC measurement.
#define CHECK_BITS(expectedValue, actualValue)                        \
  testing::checkPortableNumeric(                                      \
      (actualValue), (expectedValue),                                 \
      testing::PortableNumericClass::kVelsReferenceValue,             \
      #actualValue, #expectedValue, __FILE__, __LINE__)

session::SessionComposition nacaSession(int designation, bool viscous,
                                        double alphaDeg) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(designation);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  CHECK(session::applyPaneling(composition).ok);
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = viscous;
  CHECK(session::configure(composition, config).success());
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(composition, in, out,
                                      std::to_string(alphaDeg)) ==
        app::cli::OperAlphaCoreStatus::analyzed);
  return composition;
}

void checkCase(const VelsCase& hc,
               const session::VelocityAtPointResult& got) {
  CHECK_BITS(hc.u, got.u);
  CHECK_BITS(hc.v, got.v);
  CHECK_BITS(hc.q, got.q);
  CHECK_BITS(hc.cp, got.cp);
}

}  // namespace

// V1/V2: PSILIN branches and the VELS final formula bit-identical
// (inviscid, harness points 1..5).
TEST(unit, VelsVelocityInviscidBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSession(12, false, 2.0);
  {
    const auto& panel = composition.paneling.panel;
    const std::size_t n = panel.x.size();
    core::inviscid::PsiLinContext ctx;
    ctx.x = panel.x; ctx.y = panel.y; ctx.s = panel.s;
    ctx.nx = panel.xn; ctx.ny = panel.yn; ctx.apanel = panel.apanel;
    ctx.gam = composition.viscous.viscals.gam;
    ctx.gamA = composition.viscous.viscals.gamA;
    std::vector<double> zero(n, 0.0), sig(n, 0.0);
    ctx.gamu1 = zero; ctx.gamu2 = zero; ctx.sig = sig;
    const auto& te = composition.viscous.lastTe;
    ctx.sharp = te.sharp; ctx.dste = te.dste; ctx.ante = te.ante; ctx.aste = te.aste;
    ctx.qinf = composition.operating.qinf; ctx.image = false; ctx.yimage = 0.0;
    core::inviscid::PsiLinInputs in;
    in.node = 0; in.xi = 0.5; in.yi = 0.1; in.nodeOnAirfoil = false;
    in.siglin = true; in.geolin = false; in.alfa = composition.viscous.viscals.alfa;
    in.nxi = -1.0; in.nyi = 0.0;
    const auto rv = core::inviscid::psiLin(in, ctx);
    in.nxi = 0.0; in.nyi = 1.0;
    const auto ru = core::inviscid::psiLin(in, ctx);
    printf("core psi=%.17e psiNi_v=%.17e psiNi_u=%.17e gamsize=%zu\n", rv.psi, rv.psiNi, ru.psiNi, composition.viscous.viscals.gam.size());
    printf("gam[0..3]=%.6e %.6e %.6e %.6e\n", composition.viscous.viscals.gam[0], composition.viscous.viscals.gam[1], composition.viscous.viscals.gam[2], composition.viscous.viscals.gam[3]);
  }
  for (const VelsCase& hc : hv.abc) {
    const auto got =
        session::evaluateVelocityAtPoint(composition, hc.x, hc.y);
    CHECK(got.isOk());
    
    checkCase(hc, got.value());
  }
}

// V3: second SPECAL pass (alfa -2) on the same session.
TEST(unit, VelsVelocitySecondSpecalBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSession(12, false, 2.0);
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(composition, in, out, "-2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const auto got = session::evaluateVelocityAtPoint(composition, 0.5, 0.1);
  CHECK(got.isOk());
  checkCase(hv.h, got.value());
}

// V4: asymmetric airfoil NACA 2412, alfa 4.
TEST(unit, VelsVelocityAsymmetricBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSession(2412, false, 4.0);
  const auto got = session::evaluateVelocityAtPoint(composition, 0.5, 0.1);
  CHECK(got.isOk());
  checkCase(hv.f, got.value());
}

// V5: viscous PSILIN branch after VISCAL.
TEST(unit, VelsVelocityViscousBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  // harness case G uses the NACA 2412 contour of case F
  // (LVISC=.TRUE., SPECAL+VISCAL for alfa 2).
  auto composition = nacaSession(2412, true, 2.0);
  const auto got = session::evaluateVelocityAtPoint(composition, 0.5, 0.1);
  CHECK(got.isOk());
  checkCase(hv.g, got.value());
}

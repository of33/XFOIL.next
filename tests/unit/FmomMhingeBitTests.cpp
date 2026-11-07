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

// File: tests/unit/FmomMhingeBitTests.cpp
// bit-for-bit verification of the FMOM/MHINGE paths against the focused
// Fortran harness tests/fortran-reference/harnesses/fmom/fmom_harness.f
// (reference: tests/data/core-fmom-vels-reference/fmom_reference.txt,
// -fdefault-real-8 -fdefault-double-8). The following are compared exactly:
// M1 TOPS/BOTS of both estimate paths, M7 final HMOM/HFX/HFY.
// the session uses the PANGEN contour (NACA sets the current contour via
// PANGEN) as the harness does.

#include "TestSupport.h"

#include "app/cli/CoreOperHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;

struct HarnessCase {
  double tops{0.0};
  double bots{0.0};
  double hmom{0.0};
  double hfx{0.0};
  double hfy{0.0};
};

struct HarnessValues {
  HarnessCase a, b, c, e1, e2, g, f;
  double dXof{0.0};
  double dYof{0.0};
  double dHmom{0.0};
  double dHfx{0.0};
  double dHfy{0.0};
};

void readTwo(const std::string& line, double& a, double& b) {
  std::istringstream tok(line);
  std::vector<std::string> tokens;
  std::string t;
  while (tok >> t) {
    tokens.push_back(t);
  }
  CHECK_EQ(tokens.size(), std::size_t{6});
  a = std::stod(tokens[4]);
  b = std::stod(tokens[5]);
}

void readThree(const std::string& line, HarnessCase& hc) {
  std::istringstream tok(line);
  std::vector<std::string> tokens;
  std::string t;
  while (tok >> t) {
    tokens.push_back(t);
  }
  CHECK_EQ(tokens.size(), std::size_t{8});
  hc.hmom = std::stod(tokens[5]);
  hc.hfx = std::stod(tokens[6]);
  hc.hfy = std::stod(tokens[7]);
}

HarnessValues readHarnessReference() {
  HarnessValues hv;
  std::ifstream in(
      "tests/data/core-fmom-vels-reference/fmom_reference.txt");
  CHECK(static_cast<bool>(in));
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("CASE A TOPS BOTS", 0) == 0) {
      readTwo(line, hv.a.tops, hv.a.bots);
    } else if (line.rfind("CASE A HMOM", 0) == 0) {
      readThree(line, hv.a);
    } else if (line.rfind("CASE B TOPS BOTS", 0) == 0) {
      readTwo(line, hv.b.tops, hv.b.bots);
    } else if (line.rfind("CASE B HMOM", 0) == 0) {
      readThree(line, hv.b);
    } else if (line.rfind("CASE C TOPS BOTS", 0) == 0) {
      readTwo(line, hv.c.tops, hv.c.bots);
    } else if (line.rfind("CASE C HMOM", 0) == 0) {
      readThree(line, hv.c);
    } else if (line.rfind("CASE E1 TOPS BOTS", 0) == 0) {
      readTwo(line, hv.e1.tops, hv.e1.bots);
    } else if (line.rfind("CASE E1 HMOM", 0) == 0) {
      readThree(line, hv.e1);
    } else if (line.rfind("CASE E2 TOPS BOTS", 0) == 0) {
      readTwo(line, hv.e2.tops, hv.e2.bots);
    } else if (line.rfind("CASE E2 HMOM", 0) == 0) {
      readThree(line, hv.e2);
    } else if (line.rfind("CASE G TOPS BOTS", 0) == 0) {
      readTwo(line, hv.g.tops, hv.g.bots);
    } else if (line.rfind("CASE G HMOM", 0) == 0) {
      readThree(line, hv.g);
    } else if (line.rfind("CASE F TOPS BOTS", 0) == 0) {
      readTwo(line, hv.f.tops, hv.f.bots);
    } else if (line.rfind("CASE F HMOM", 0) == 0) {
      readThree(line, hv.f);
    } else if (line.rfind("CASE D XOF YOF", 0) == 0) {
      readTwo(line, hv.dXof, hv.dYof);
    } else if (line.rfind("CASE D HMOM", 0) == 0) {
      std::istringstream tok(line);
      std::vector<std::string> tokens;
      std::string t;
      while (tok >> t) {
        tokens.push_back(t);
      }
      CHECK_EQ(tokens.size(), std::size_t{8});
      hv.dHmom = std::stod(tokens[5]);
      hv.dHfx = std::stod(tokens[6]);
      hv.dHfy = std::stod(tokens[7]);
    }
  }
  return hv;
}

// PUB-W3: portable contract for harness comparisons. On Linux (exact
// platform) this stays bitwise, as before. On Windows/MSYS2 the third
// acceptance run measured abs <= 2.27e-15, rel <= 3.11e-13 (ULP up to
// 1823). Class kFmomReferenceValue (abs <= 5e-15 or rel <= 1e-12;
// margins 2.2x / 3.2x) is the documented contract; violations keep
// reporting the full PORTABLE-NUMERIC measurement.
#define CHECK_BITS(expectedValue, actualValue)                        \
  testing::checkPortableNumeric(                                      \
      (actualValue), (expectedValue),                                 \
      testing::PortableNumericClass::kFmomReferenceValue,             \
      #actualValue, #expectedValue, __FILE__, __LINE__)

// harness contour: NACA sets the buffer and calls PANGEN(.TRUE.);
// the current contour is the PANGEN contour.
session::SessionComposition nacaSessionNoPane(int designation, bool viscous) {
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
  CHECK(app::cli::handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  return composition;
}

void setFlap(session::SessionComposition& s, double xof, double yof) {
  s.design.flap.xof = xof;
  s.design.flap.yof = yof;
  s.design.flap.lFlap = true;
}

void checkCase(const HarnessCase& hc,
               const core::inviscid::HingeMomentResult& got) {
  CHECK_BITS(hc.hmom, got.hmom);
  CHECK_BITS(hc.hfx, got.hfx);
  CHECK_BITS(hc.hfy, got.hfy);
}

}  // namespace

// M1: TOPS/BOTS of the LFLAP=true estimate path bit-identical.
TEST(unit, FmomHingeArcGuessBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSessionNoPane(12, false);
  const auto& panel = composition.paneling.panel;
  const std::size_t n = panel.x.size();

  double tops = 0.0;
  double bots = 0.0;
  // case A (xof=0)
  tops = 0.0;
  bots = panel.s[n - 1] - 0.0;
  core::math::sinvrt(tops, 0.0, panel.x, panel.xp, panel.s);
  core::math::sinvrt(bots, 0.0, panel.x, panel.xp, panel.s);
  CHECK_BITS(hv.a.tops, tops);
  CHECK_BITS(hv.a.bots, bots);
  // case B (xof=0.7)
  tops = 0.7;
  bots = panel.s[n - 1] - 0.7;
  core::math::sinvrt(tops, 0.7, panel.x, panel.xp, panel.s);
  core::math::sinvrt(bots, 0.7, panel.x, panel.xp, panel.s);
  CHECK_BITS(hv.b.tops, tops);
  CHECK_BITS(hv.b.bots, bots);
}

// M7: HMOM/HFX/HFY bit-identical for cases A/B (inviscid,
// LFLAP=true path via the session use case).
TEST(unit, FmomHingeLoadsInviscidBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  {
    auto composition = nacaSessionNoPane(12, false);
    setFlap(composition, 0.0, 0.0);
    const auto got = session::computeCurrentHingeLoads(composition);
    CHECK(got.isOk());
    checkCase(hv.a, got.value());
  }
  {
    auto composition = nacaSessionNoPane(12, false);
    setFlap(composition, 0.7, -0.05);
    const auto got = session::computeCurrentHingeLoads(composition);
    CHECK(got.isOk());
    checkCase(hv.b, got.value());
  }
}

// M7: hinge near the upper/lower side (E1/E2) and TE base case G.
TEST(unit, FmomHingeLoadsNearSurfaceAndTeBaseBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  {
    auto composition = nacaSessionNoPane(12, false);
    setFlap(composition, 0.5, 0.06);
    const auto got = session::computeCurrentHingeLoads(composition);
    CHECK(got.isOk());
    checkCase(hv.e1, got.value());
  }
  {
    auto composition = nacaSessionNoPane(12, false);
    setFlap(composition, 0.5, -0.06);
    const auto got = session::computeCurrentHingeLoads(composition);
    CHECK(got.isOk());
    checkCase(hv.e2, got.value());
  }
  {
    auto composition = nacaSessionNoPane(12, false);
    setFlap(composition, 0.7, -0.05);
    const auto got = session::computeCurrentHingeLoads(composition);
    CHECK(got.isOk());
    checkCase(hv.g, got.value());
  }
}

// M7: viscous CPV branch (case C).
TEST(unit, FmomHingeLoadsViscousCpvBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSessionNoPane(12, true);
  setFlap(composition, 0.7, -0.05);
  const auto got = session::computeCurrentHingeLoads(composition);
  CHECK(got.isOk());
  checkCase(hv.c, got.value());
}

// M7: asymmetric airfoil NACA 2412 (case F).
TEST(unit, FmomHingeLoadsAsymmetricBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSessionNoPane(2412, false);
  setFlap(composition, 0.7, -0.05);
  const auto got = session::computeCurrentHingeLoads(composition);
  CHECK(got.isOk());
  checkCase(hv.f, got.value());
}

// M7: GETXYF guess path (case D) directly against the core with
// the GETXYF initial guess.
TEST(unit, FmomGetxyfGuessPathBitAgainstHarness) {
  const HarnessValues hv = readHarnessReference();
  auto composition = nacaSessionNoPane(12, false);
  const auto& panel = composition.paneling.panel;
  const auto got = core::inviscid::mhinge(
      panel.x, panel.xp, panel.y, panel.yp, panel.s, composition.viscous.cpi,
      composition.viscous.cpv, false, hv.dXof, hv.dYof);
  CHECK_BITS(hv.dHmom, got.hmom);
  CHECK_BITS(hv.dHfx, got.hfx);
  CHECK_BITS(hv.dHfy, got.hfy);
}

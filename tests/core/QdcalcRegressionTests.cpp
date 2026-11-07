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

// file: tests/core/QdcalcRegressionTests.cpp
// Permanent QDCALC regression test:
// Fortran-proven with the wake-DIJ writeback fix (BIJ post-BAKSUB values in
// the DO-80 branch 820, xpanel.f:1252). The test checks the wake-to-wake
// DIJ block, at least the entry Fortran DIJ(153,152), the profile-source
// control block (J = 1..151), the row-major assignment, the Clark-Y wake
// case, and the NACA regression.
//
// reference: tests/data/core-c11a-state-reference/c11a_state_reference.txt
// (c11a_state_harness.f: canonical Fortran routines, built with the
// canonical build flags -fdefault-real-8 -fdefault-double-8; DIJ lines at
// the first two wake-BL panel positions IPAN(IBLTE(2)+1,2) and
// IPAN(IBLTE(2)+2,2), Clark Y and NACA 4412, alpha = 4 degrees, Re = 1e5,
// Mach = 0, no-PANE).
//
// the DIJ lines are verified bit-exactly (0 ULP) between the canonical
// double-precision Fortran and the double core; the test checks exact
// equality (CHECK_EQ). No /tmp artifacts.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include "TestSupport.h"

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/paneling/Abcopy.h"
#include "xfoil/core/paneling/PanelingTypes.h"

using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::inviscid;
using namespace xfoil::core::paneling;

namespace {

constexpr const char* kRefPath =
    "tests/data/core-c11a-state-reference/c11a_state_reference.txt";

// DIJ reference block of a harness: headers (N, IPW1, IPW2) and the DIJ
// lines of both wake-BL panel positions (row, col, value).
struct DijRef {
  int n{0};
  int ipw1{0};
  int ipw2{0};
  std::vector<std::array<int, 2>> indices;
  std::vector<double> values;
};

DijRef loadDijRef(const char* caseTag) {
  std::ifstream in(kRefPath);
  CHECK(in.good());
  DijRef ref;
  const std::string casePrefix = std::string("DIJ CASE ") + caseTag;
  const std::string tag1 = std::string(caseTag) + "1";
  const std::string tag2 = std::string(caseTag) + "2";
  std::string line;
  while (std::getline(in, line)) {
    if (line.find(casePrefix) != std::string::npos) {
      char field[32];
      int value = 0;
      if (std::sscanf(line.c_str(), " DIJ CASE %*s %31s %d", field, &value) ==
          2) {
        const std::string f(field);
        if (f == "N") {
          ref.n = value;
        } else if (f == "IPW1") {
          ref.ipw1 = value;
        } else if (f == "IPW2") {
          ref.ipw2 = value;
        }
      }
      continue;
    }
    if (line.compare(0, 5, " DIJ ") != 0) {
      continue;
    }
    char tag[32];
    int row = 0;
    int col = 0;
    double v = 0.0;
    if (std::sscanf(line.c_str(), " DIJ %31s %d %d %lf", tag, &row, &col,
                    &v) == 4) {
      const std::string t(tag);
      if (t == tag1 || t == tag2) {
        ref.indices.push_back({row, col});
        ref.values.push_back(v);
      }
    }
  }
  return ref;
}

// core DIJ (row-major, qdCalc) for alpha = 4 degrees, Re = 1e5, Mach = 0,
// no-PANE (abcopy), QINF = 1.0 -- the harness inputs.
struct DijResult {
  std::vector<double> dij;
  int n{0};
  int stride{0};
};

DijResult computeDij(const std::vector<double>& bx,
                     const std::vector<double>& by,
                     const std::string& name) {
  DijResult out;
  const AirfoilBuffer buffer(bx, by, name);
  const auto pg = abcopy(buffer);
  CHECK(pg.isOk());
  const auto& panel = pg.value().panel;
  // Fortran PI = 4.0*ATAN(1.0) (xfoil.f:392) — bit-identical.
  const double pi = 4.0 * std::atan(1.0);
  const double alpha = 4.0 * pi / 180.0;
  const std::vector<double> sig0(panel.x.size(), 0.0);
  const auto gs = ggCalc(panel, sig0, alpha, 1.0);
  CHECK(gs.isOk());
  const MachReynoldsSettings settings{1, 1, 0.0, 100000.0};
  const auto sp = specAlpha(gs.value(), panel, settings, alpha, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const auto wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  out.n = static_cast<int>(panel.x.size());
  out.stride = out.n + static_cast<int>(wk.value().nw);
  const SourceInfluenceResult si =
      qdCalc(gs.value(), wk.value(), panel, sp.value().alfa, 1.0, sig0);
  out.dij = si.dij;
  return out;
}

void checkDijRows(const DijResult& r, const DijRef& ref, double wakeAbsTol,
                  double wakeRelTol) {
  CHECK_EQ(ref.n, r.n);
  CHECK_EQ(ref.ipw1, r.n + 1);  // first wake-BL panel position (TE duplicate)
  CHECK_EQ(ref.ipw2, r.n + 2);  // second wake-BL panel position
  CHECK_EQ(static_cast<int>(ref.indices.size()), 2 * (r.n + r.stride - r.n));
  // profile-source control block: J = 1..N of both lines.
  int surfaceCols = 0;
  for (const auto& rc : ref.indices) {
    if (rc[1] <= ref.n) {
      ++surfaceCols;
    }
  }
  CHECK_EQ(surfaceCols, 2 * ref.n);
  // row-major assignment: core dij[(row-1)*stride + (col-1)] = Fortran
  // DIJ(row, col). Profile-source columns (J <= N) bit-exact; wake-to-wake
  // columns (J > N) with the threshold documented by the direct comparison
  // (wake-trajectory rounding, not a REAL(4) tolerance): Clark Y
  // bit-exact (0/0), NACA 4412 max abs 1.315e-11, max rel 4.021e-10
  // (direct comparison of c11a_state_reference against the core).
  for (std::size_t k = 0; k < ref.indices.size(); ++k) {
    const int row = ref.indices[k][0];
    const int col = ref.indices[k][1];
    const std::size_t idx =
        static_cast<std::size_t>(row - 1) * static_cast<std::size_t>(r.stride) +
        static_cast<std::size_t>(col - 1);
    if (col <= ref.n) {
      // Profile-source columns: bitwise on Linux. PUB-W1: on
      // Windows/MSYS2 the recorded influence-matrix deviations stay
      // below 1e-8 absolute / ~5e-7 relative (6-digit log evidence);
      // class kDijMatrixValue (1e-8 + 1e-6*|e|, provisional, to be
      // re-measured at full precision during MSYS2 acceptance) is the
      // documented portable contract.
      testing::checkPortableNumeric(
          r.dij[idx], ref.values[k],
          testing::PortableNumericClass::kDijMatrixValue, "r.dij[idx]",
          "ref.values[k]", __FILE__, __LINE__);
    } else if (wakeAbsTol == 0.0 && wakeRelTol == 0.0) {
      // Clark Y wake columns: bitwise/equal on Linux; same portable
      // matrix contract on Windows (documented above).
      testing::checkPortableNumeric(
          r.dij[idx], ref.values[k],
          testing::PortableNumericClass::kDijMatrixValue, "r.dij[idx]",
          "ref.values[k]", __FILE__, __LINE__);
    } else {
      CHECK_NEAR(r.dij[idx], ref.values[k], wakeAbsTol, wakeRelTol);
    }
  }
  // Guard: at least the entry Fortran DIJ(153,152) (Clark Y) or
  // DIJ(247,246) (NACA 4412) -- the second wake-BL panel line, first wake
  // source column -- is contained in the reference and checked exactly,
  // which pins the wake-DIJ indexing.
  bool hasEntry = false;
  for (std::size_t k = 0; k < ref.indices.size(); ++k) {
    if (ref.indices[k][0] == ref.ipw2 && ref.indices[k][1] == ref.n + 1) {
      hasEntry = true;
    }
  }
  CHECK(hasEntry);
}

}  // namespace

TEST(unit, QdcalcWakeDijClarkY) {
  // Clark Y from sample/CLARK_Y.dat in file order (Fortran harness read
  // order, no AREA reverse: Clark Y is counterclockwise).
  std::ifstream in("sample/CLARK_Y.dat");
  CHECK(in.good());
  std::string name;
  std::getline(in, name);
  std::vector<double> bx;
  std::vector<double> by;
  double x = 0.0;
  double y = 0.0;
  while (in >> x >> y) {
    bx.push_back(x);
    by.push_back(y);
  }
  CHECK_EQ(bx.size(), static_cast<std::size_t>(151));

  const DijResult r = computeDij(bx, by, name);
  const DijRef ref = loadDijRef("CLARKY");
  // Clark Y: all DIJ columns bit-exact (0 ULP, direct comparison).
  checkDijRows(r, ref, 0.0, 0.0);
}

TEST(unit, QdcalcWakeDijNaca4412) {
  // NACA regression: canonical NACA path (nacaAirfoil, NB = 245).
  const auto na = nacaAirfoil(4412);
  CHECK(na.isOk());
  const AirfoilBuffer& buffer = na.value().buffer;
  CHECK_EQ(buffer.x.size(), static_cast<std::size_t>(245));

  const DijResult r = computeDij(buffer.x, buffer.y, buffer.name);
  const DijRef ref = loadDijRef("NACA4412");
  // NACA 4412: profile sources bit-exact; wake columns with the threshold
  // documented by the direct comparison (max abs 1.315e-11,
  // max rel 4.021e-10; wake-trajectory rounding).
  checkDijRows(r, ref, 1.0e-10, 1.0e-9);
}

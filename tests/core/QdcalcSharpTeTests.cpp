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

// file: tests/core/QdcalcSharpTeTests.cpp
// S06 QDCALC sharp-TE row: the zero-velocity condition on the TE-bisector
// control point from XFOIL 6.996 xpanel.f QDCALC. Reference values were
// produced by H-S06-QDCALC: canonical 6.996 routines, double-precision
// Fortran build (-fdefault-real-8 -fdefault-double-8), NACA 0012/2412 and
// SQTE 2412, alpha = 4 degrees, MATYP=1/RETYP=1, MINF1=0, REINF1=3e6.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "TestSupport.h"

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry_design/GeometryDesignOps.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/paneling/Pangen.h"

using namespace xfoil::core;
using namespace xfoil::core::geometry;
using namespace xfoil::core::geometry_design;
using namespace xfoil::core::inviscid;
using namespace xfoil::core::paneling;

namespace {

constexpr double kPi = 3.14159265358979323846;

AirfoilBuffer nacaBuffer(int ides, bool sharp) {
  const Result<NacaResult> n = naca4(ides, 123, sharp);
  CHECK(n.isOk());
  return AirfoilBuffer(n.value().xb, n.value().yb, n.value().name);
}

AirfoilBuffer sqteBuffer() {
  const Result<NacaResult> n = naca4(2412, 123, false);
  CHECK(n.isOk());
  std::vector<double> x = n.value().xb;
  std::vector<double> y = n.value().yb;
  std::vector<double> s(x.size());
  std::vector<double> xp(x.size());
  std::vector<double> yp(x.size());
  math::scalc(x, y, s);
  math::segspl(x, s, xp);
  math::segspl(y, s, yp);
  GeometryDesignBuffer b(std::move(x), std::move(y), std::move(s),
                         std::move(xp), std::move(yp));
  b.metrics = geopar(b.x, b.xp, b.y, b.yp, b.s);
  CHECK(squareTrailingEdge(b).isOk());
  return AirfoilBuffer(b.x, b.y, "SQTE2412");
}

struct RowResult {
  std::vector<double> dij;
  std::size_t n{0};
  std::size_t stride{0};
  double cl{0.0};
  double cm{0.0};
  bool sharp{false};
};

RowResult computeRow(const AirfoilBuffer& buffer) {
  const Result<PangenResult> pg = pangen(buffer, PanelingParameters{});
  CHECK(pg.isOk());
  const PanelGeometry& panel = pg.value().panel;
  const std::size_t n = panel.x.size();
  const std::vector<double> sig0(n, 0.0);
  const Result<InfluenceSystem> gs = ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const double alpha = 4.0 * kPi / 180.0;
  const MachReynoldsSettings settings{1, 1, 0.0, 3.0e6};
  const Result<SpecPointResult> sp =
      specAlpha(gs.value(), panel, settings, alpha, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const Result<WakeState> wk =
      xywake(panel, sp.value().gam, sp.value().gamA, gs.value().gamu1,
             gs.value().gamu2, sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const SourceInfluenceResult qd =
      qdCalc(gs.value(), wk.value(), panel, sp.value().alfa, 1.0, sig0);
  RowResult out;
  out.dij = qd.dij;
  out.n = n;
  out.stride = n + wk.value().nw;
  out.cl = sp.value().cl;
  out.cm = sp.value().cm;
  out.sharp = panel.sharpTe;
  CHECK_EQ(out.dij.size(), out.stride * out.stride);
  return out;
}

double rowValue(const RowResult& r, std::size_t row, std::size_t col) {
  return r.dij[(row - 1) * r.stride + (col - 1)];
}

void checkPortable(double actual, double expected) {
  testing::checkPortableNumeric(
      actual, expected, testing::PortableNumericClass::kDijMatrixValue,
      "actual", "expected", __FILE__, __LINE__);
}

void checkExpectedRow(const RowResult& r, double n1, double nn, double nn1,
                      double nnw, double nm1n1) {
  const std::size_t n = r.n;
  checkPortable(rowValue(r, n, 1), n1);
  checkPortable(rowValue(r, n, n), nn);
  checkPortable(rowValue(r, n, n + 1), nn1);
  checkPortable(rowValue(r, n, n + 23), nnw);
  checkPortable(rowValue(r, n - 1, n + 1), nm1n1);
}

}  // namespace

TEST(unit, QdcalcSharpTeRowNaca0012) {
  const RowResult r = computeRow(nacaBuffer(12, true));
  CHECK(r.sharp);
  checkExpectedRow(r, 4.3844547379763831e1, -4.3844547379772379e1,
                   -4.5323097397452877e1, 1.3385304304669707e-1,
                   -1.1951145857019286e1);
  checkPortable(r.cl, 4.8231955315951469e-1);
  checkPortable(r.cm, -5.2707106554811273e-3);
}

TEST(unit, QdcalcSharpTeRowNaca2412) {
  const RowResult r = computeRow(nacaBuffer(2412, true));
  CHECK(r.sharp);
  checkExpectedRow(r, 4.3885551712752530e1, -4.4038131848513153e1,
                   -4.5424339104302106e1, 1.3380918528357205e-1,
                   -1.1893395117425696e1);
  checkPortable(r.cl, 7.3638631518672759e-1);
  checkPortable(r.cm, -6.1103475019161156e-2);
}

TEST(unit, QdcalcOpenTeRowNaca2412Unchanged) {
  const RowResult r = computeRow(nacaBuffer(2412, false));
  CHECK_FALSE(r.sharp);
  checkExpectedRow(r, 5.0433303005528842e1, -4.8828268896632316e1,
                   -5.6863558887090569e1, 1.3517922326693399e-1,
                   -5.8828113825062811e0);
}

TEST(unit, QdcalcSqte2412RowUnchanged) {
  const RowResult r = computeRow(sqteBuffer());
  CHECK_FALSE(r.sharp);
  checkExpectedRow(r, 4.9558473600283278e1, -4.9708015380633093e1,
                   -5.6868002138398936e1, 1.3513136354654826e-1,
                   -6.1288783928333039e0);
}

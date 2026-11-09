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

// File: tests/core/PolarTests.cpp
// core tests for the polar core and the export-data projections.
// harness-based format comparisons follow in the integration tests against
// tests/data/core-polar-io-reference/.

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "TestSupport.h"
#include "xfoil/core/Result.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/core/polar/PolarOps.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/core/math/Spline.h"

using namespace xfoil::core;
using namespace xfoil::core::polar;
namespace ex = xfoil::core::export_data;

namespace {

PolarPointInput makeInput(double alpha) {
  PolarPointInput in;
  in.alphaDeg = alpha;
  in.cl = 0.5 + 0.1 * alpha;
  in.cd = 0.01;
  in.cm = -0.1;
  in.cdPres = 0.005;
  in.cpMin = -4.0;
  in.mach = 0.3;
  in.viscous = true;
  in.reInf = 3.0e6;
  in.acrit = {9.0, 9.0};
  in.xtrip = {1.0, 1.0};
  in.xoctr = {0.5, 0.4};
  in.tindex = {10.0, 12.0};
  in.hingeMoment = 0.0;
  return in;
}

}  // namespace

TEST(unit, PolarTypesDefaults) {
  PolarPoint p;
  CHECK_EQ(p.alpha, 0.0);
  CHECK_EQ(p.cmDot, 0.0);
  PolarSideData sd;
  CHECK_EQ(sd.itr, 0.0);
  PolarMetadata md;
  CHECK_EQ(md.code, std::string("XFOIL"));
  CHECK_EQ(md.version, 6.99);
  CHECK_EQ(md.elementCount, 1);
  PolarDataset ds;
  CHECK_EQ(ds.points.size(), 0u);
  ReferencePolarDataset ref;
  CHECK_EQ(ref.blocks.size(), 4u);
  CHECK_EQ(maxPolarPoints, 800u);
  CHECK_EQ(maxPolarSlots, 20u);
  CHECK_EQ(maxReferencePoints, 128u);
  CHECK_EQ(polarColumnCount, 13u);
  CHECK_EQ(sideColumnCount, 4u);
}

TEST(unit, PolarAddPointViscous) {
  PolarDataset ds;
  auto st = addPoint(ds, makeInput(2.0));
  CHECK(st.isOk());
  CHECK_EQ(ds.points.size(), 1u);
  CHECK_EQ(ds.sideData.size(), 1u);
  const auto& p = ds.points[0];
  CHECK_EQ(p.alpha, 2.0);
  CHECK_EQ(p.cl, 0.7);
  CHECK_EQ(p.cd, 0.01);
  CHECK_EQ(p.cdVisc, 0.01);
  CHECK_EQ(p.cdPres, 0.005);
  CHECK_EQ(p.re, 3.0e6);
  CHECK_EQ(p.mach, 0.3);
  CHECK_EQ(p.hinge, 0.0);
  CHECK_EQ(p.cpMin, -4.0);
  const auto& sd = ds.sideData[0];
  CHECK_EQ(sd[0].ncrit, 9.0);
  CHECK_EQ(sd[0].xtrip, 1.0);
  CHECK_EQ(sd[0].xtr, 0.5);
  CHECK_EQ(sd[1].xtr, 0.4);
  CHECK_EQ(sd[0].itr, 10.0);
  CHECK_EQ(sd[1].itr, 12.0);
}

TEST(unit, PolarAddPointInviscid) {
  PolarDataset ds;
  auto in = makeInput(1.0);
  in.viscous = false;
  auto st = addPoint(ds, in);
  CHECK(st.isOk());
  const auto& p = ds.points[0];
  CHECK_EQ(p.cd, 0.0);
  CHECK_EQ(p.cdVisc, 0.0);
  CHECK_EQ(p.re, 0.0);
  CHECK_EQ(ds.sideData[0][0].xtr, 0.0);
}

TEST(unit, PolarAddPointCapacityNoMutation) {
  PolarDataset ds;
  for (std::size_t i = 0; i < maxPolarPoints; ++i) {
    auto st = addPoint(ds, makeInput(static_cast<double>(i)));
    CHECK(st.isOk());
  }
  CHECK_EQ(ds.points.size(), maxPolarPoints);
  const auto st = addPoint(ds, makeInput(99.0));
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::capacityExceeded);
  CHECK_EQ(ds.points.size(), maxPolarPoints);
}

TEST(unit, PolarSortAscendingAndDescending) {
  PolarDataset ds;
  for (double a : {3.0, 1.0, 2.0}) {
    CHECK(addPoint(ds, makeInput(a)).isOk());
  }
  CHECK(sortPoints(ds, SortKey::alpha).isOk());
  CHECK_EQ(ds.points[0].alpha, 1.0);
  CHECK_EQ(ds.points[1].alpha, 2.0);
  CHECK_EQ(ds.points[2].alpha, 3.0);
  // side data follow the canonical permutation.
  CHECK_EQ(ds.sideData[0][0].xtr, 0.5);

  CHECK(sortPoints(ds, SortKey::cl).isOk());
  CHECK(ds.points[0].cl <= ds.points[1].cl);
  CHECK(ds.points[1].cl <= ds.points[2].cl);
}

TEST(unit, PolarSortEmptyAndSingleAndEqualKeys) {
  PolarDataset ds;
  CHECK(sortPoints(ds, SortKey::alpha).isOk());
  CHECK(addPoint(ds, makeInput(1.0)).isOk());
  CHECK(sortPoints(ds, SortKey::alpha).isOk());
  CHECK_EQ(ds.points.size(), 1u);

  CHECK(addPoint(ds, makeInput(1.0)).isOk());
  CHECK(addPoint(ds, makeInput(1.0)).isOk());
  CHECK(sortPoints(ds, SortKey::alpha).isOk());
  CHECK_EQ(ds.points.size(), 3u);
}

TEST(unit, PolarCopySlotAndSelfCopy) {
  PolarDataset src;
  CHECK(addPoint(src, makeInput(1.0)).isOk());
  src.metadata.airfoilName = "NACA 0012";
  src.geometry.x = {0.0, 0.5, 1.0};
  src.geometry.y = {0.0, 0.1, 0.0};

  PolarDataset dst;
  CHECK(copySlot(src, dst).isOk());
  CHECK_EQ(dst.metadata.airfoilName, std::string("NACA 0012"));
  CHECK_EQ(dst.points.size(), 1u);
  CHECK_EQ(dst.geometry.size(), 3u);

  // Self-copy leaves the state unchanged.
  CHECK(copySlot(src, src).isOk());
  CHECK_EQ(src.points.size(), 1u);
  CHECK_EQ(src.geometry.size(), 3u);
}

TEST(unit, PolarCopyReferenceSlot) {
  ReferencePolarDataset src;
  src.label = "test ref";
  src.blocks[0] = {{1.0, 2.0}, {3.0, 4.0}};
  src.blocks[2] = {{5.0, 6.0}};
  ReferencePolarDataset dst;
  CHECK(copyReferenceSlot(src, dst).isOk());
  CHECK_EQ(dst.label, std::string("test ref"));
  CHECK_EQ(dst.blocks[0].size(), 2u);
  CHECK_EQ(dst.blocks[2][0].x, 5.0);
  CHECK(copyReferenceSlot(src, src).isOk());
}

TEST(unit, PolarVersionTag) {
  auto r1 = applyVersionTag("NACA 0012 [5]", 1, 1);
  CHECK(r1.isOk());
  CHECK_EQ(r1.value(), std::string("NACA 0012 [6]"));

  auto r2 = applyVersionTag("NACA 0012", 1, 1);
  CHECK_EQ(r2.value(), std::string("NACA 0012 [1]"));

  auto r3 = applyVersionTag("NACA 0012", 1, -1);
  CHECK_EQ(r3.value(), std::string("NACA 0012"));

  auto r4 = applyVersionTag("A [99]", 1, 1);
  // MOD(99+1,100) = 0 -> NAMDEF stays the original name
  // (nammod_reference.txt case 3).
  CHECK_EQ(r4.value(), std::string("A [99]"));

  auto r5 = applyVersionTag("A [ 0]", -1, 1);
  CHECK(r5.isOk());
  // MOD(0-1,100) = -1 -> no tag, NAMDEF = original (case 7 analogous).
  CHECK_EQ(r5.value(), std::string("A [ 0]"));

  auto r6 = applyVersionTag("A [x]", 1, 1);
  CHECK(r6.isErr());
}

TEST(unit, PolarReEncoding) {
  auto e1 = encodeReMantissaExponent(3.0e6);
  CHECK_EQ(e1.exponent, 6);
  CHECK_NEAR(e1.mantissa, 3.0, 1e-12, 1e-12);

  auto e2 = encodeReMantissaExponent(0.0);
  CHECK_EQ(e2.mantissa, 0.0);
  CHECK_EQ(e2.exponent, 0);

  auto e3 = encodeReMantissaExponent(9.99e9);
  CHECK_EQ(e3.exponent, 9);
}

TEST(unit, PolarSaveHeaderComparison) {
  PolarMetadata file;
  file.airfoilName = "NACA 0012";
  file.reCl1 = 3.0e6;
  file.machCl1 = 0.3;
  file.ncrit = {9.0, 9.0};
  file.xtrip = {1.0, 1.0};
  SessionParameters cur;
  cur.name = "NACA 0012";
  cur.reinf1 = 3.0e6;
  cur.minf1 = 0.3;
  cur.acrit = {9.0, 9.0};
  cur.xtrip = {1.0, 1.0};
  CHECK(compareSaveHeader(file, cur).matches());

  cur.reinf1 = 6.0e6;
  auto cmp = compareSaveHeader(file, cur);
  CHECK_FALSE(cmp.matches());
  CHECK(cmp.reDiffers);
}

TEST(unit, PolarDumpHeaderComparison) {
  DumpHeader file;
  file.name = "CLARK Y";
  file.mach = 0.3;
  file.re = 3.0e6;
  file.ncrit = {9.0, 9.0};
  file.matyp = 1;
  file.retyp = 1;
  PolarMetadata cur;
  cur.airfoilName = "CLARK Y";
  cur.reCl1 = 3.0e6;
  cur.machCl1 = 0.3;
  cur.ncrit = {9.0, 9.0};
  cur.reType = ReType::fixed;
  cur.machType = MachType::fixed;
  CHECK(compareDumpHeader(file, cur).matches());

  cur.machCl1 = 0.5;
  CHECK_FALSE(compareDumpHeader(file, cur).matches());
}

TEST(unit, PolarPrepareAppendSlotNew) {
  SessionParameters cur;
  cur.name = "NACA 2412";
  cur.viscous = true;
  cur.reinf1 = 6.0e6;
  cur.minf1 = 0.2;
  PolarGeometrySnapshot geom;
  geom.x = {0.0, 0.5, 1.0};
  geom.y = {0.0, 0.2, 0.0};
  auto res = prepareAppendSlot(nullptr, cur, geom);
  CHECK(res.kind == SlotSetupResult::Kind::newSlot);
  CHECK_EQ(res.slot.points.size(), 0u);
  CHECK_EQ(res.slot.metadata.reCl1, 6.0e6);
  CHECK_EQ(res.slot.metadata.airfoilName, std::string("NACA 2412"));
  CHECK_EQ(res.slot.geometry.size(), 3u);
}

TEST(unit, PolarPrepareAppendSlotAdoptAndConflict) {
  PolarDataset stored;
  stored.metadata.airfoilName = "NACA 0012";
  stored.metadata.reCl1 = 1.0e6;
  stored.metadata.machCl1 = 0.1;
  stored.geometry.x = {0.0, 0.5, 1.0};
  stored.geometry.y = {0.0, 0.1, 0.0};

  SessionParameters cur;
  cur.name = "other Name";
  PolarGeometrySnapshot geom;
  geom.x = {0.0, 0.5, 1.0};
  geom.y = {0.0, 0.1, 0.0};
  auto res = prepareAppendSlot(&stored, cur, geom);
  CHECK(res.kind == SlotSetupResult::Kind::adoptStoredParameters);
  CHECK_EQ(res.adopted.name, std::string("NACA 0012"));
  CHECK_EQ(res.adopted.reinf1, 1.0e6);

  geom.y[1] = 0.2;  // Geometry differs clearly
  auto res2 = prepareAppendSlot(&stored, cur, geom);
  CHECK(res2.kind == SlotSetupResult::Kind::geometryConflict);

  geom.y[1] = 0.1 + 1.0e-7;  // within the canonical threshold 1e-5
  auto res3 = prepareAppendSlot(&stored, cur, geom);
  CHECK(res3.kind == SlotSetupResult::Kind::adoptStoredParameters);
}

TEST(unit, PolarRestoreArchivedGeometry) {
  PolarGeometrySnapshot snap;
  // Closed lens contour TE->LE->TE (round LE, sharp TE).
  for (int i = 0; i <= 40; ++i) {
    const double t = i / 40.0;
    snap.x.push_back(0.5 * (1.0 + std::cos(2.0 * 3.14159265358979323846 * t)));
    snap.y.push_back(0.1 * std::sin(2.0 * 3.14159265358979323846 * t));
  }
  geometry::TrailingEdgeInputs teIn;
  auto res = restoreArchivedGeometry(snap, 41, teIn);
  CHECK(res.isOk());
  const auto& rec = res.value();
  CHECK_EQ(rec.n, 41u);
  CHECK_EQ(rec.s.size(), 41u);
  CHECK(rec.chord > 0.9);
  CHECK_EQ(rec.xte, 0.5 * (snap.x[0] + snap.x[40]));
  CHECK_EQ(rec.apanel.size(), 41u);
  CHECK_FALSE(rec.resetBoundaryLayerInit);

  auto res2 = restoreArchivedGeometry(snap, 15, teIn);
  CHECK(res2.value().resetBoundaryLayerInit);

  PolarGeometrySnapshot tiny;
  tiny.x = {0.0, 1.0};
  tiny.y = {0.0, 0.0};
  auto res3 = restoreArchivedGeometry(tiny, 2, teIn);
  CHECK(res3.isErr());
}

TEST(unit, ExportBoundaryLayerDumpRow) {
  ex::DumpFlowState flow;
  flow.qinf = 1.0;
  flow.minf = 0.3;
  flow.tklam = 0.3 * 0.3 / std::pow(1.0 + std::sqrt(1.0 - 0.09), 2);
  ex::BoundaryLayerDumpInput in;
  in.s = 0.5;
  in.x = 0.5;
  in.y = 0.01;
  in.gam = 1.2;
  in.viscousStation = true;
  in.dstar = 0.002;
  in.theta = 0.0005;
  in.tstar = 0.0002;
  in.tau = 0.001;
  const auto rec = ex::boundaryLayerDumpRow(in, flow);
  CHECK(rec.ue > 0.0);
  CHECK_NEAR(rec.dstarUe, rec.dstar * rec.ue, 1e-12, 1e-12);
  CHECK_NEAR(rec.thetaUe2, rec.theta * rec.ue * rec.ue, 1e-12, 1e-12);
  CHECK(rec.hk > 0.0);
  CHECK_NEAR(rec.cf, in.tau / (0.5 * flow.qinf * flow.qinf), 1e-12, 1e-12);

  ex::BoundaryLayerDumpInput inv = in;
  inv.viscousStation = false;
  const auto recInv = ex::boundaryLayerDumpRow(inv, flow);
  CHECK_EQ(recInv.dstar, 0.0);
  CHECK_EQ(recInv.cf, 0.0);
  CHECK_EQ(recInv.hs, 2.0);
}

TEST(unit, ExportPressureDumpRow) {
  ex::DumpFlowState flow;
  flow.qinf = 1.0;
  flow.minf = 0.3;
  const auto rec = ex::pressureDumpRow(0.5, 1.1, flow);
  const double beta = std::sqrt(1.0 - 0.09);
  const double bfac = 0.5 * 0.09 / (1.0 + beta);
  const double cpinc = 1.0 - 1.1 * 1.1;
  CHECK_NEAR(rec.cpCom, cpinc / (beta + bfac * cpinc), 1e-12, 1e-12);
}

TEST(unit, ExportCircleCoefficientRows) {
  std::vector<double> cnRe = {1.0, 2.0}, cnIm = {0.0, -0.5};
  std::vector<double> piqRe(4, 0.25), piqIm(4, -0.1);
  auto res = ex::circleCoefficientDumpRows(cnRe, cnIm, piqRe, piqIm, 1, 3);
  CHECK(res.isOk());
  const auto& rows = res.value();
  CHECK_EQ(rows.size(), 3u);
  CHECK_EQ(rows[0].m, 0);
  CHECK_EQ(rows[0].cnReal, 1.0);
  CHECK_EQ(rows[1].m, 1);
  CHECK_EQ(rows[2].m, 2);
  CHECK_EQ(rows[2].cnReal, 0.0);  // zero padding m > MC
  CHECK_EQ(rows[2].piqReal, 0.25);

  auto bad = ex::circleCoefficientDumpRows(cnRe, cnIm, piqRe, piqIm, 5, 3);
  CHECK(bad.isErr());
}

TEST(unit, ExportBlExtendedDumpRows) {
  ex::BlSideStations side;
  const std::size_t n = 5;
  side.xssi.resize(n);
  side.uedg.assign(n, 1.1);
  side.dstr.assign(n, 0.002);
  side.thet.assign(n, 0.0005);
  side.tstr.assign(n, 0.0002);
  side.tau.assign(n, 0.001);
  side.dis.assign(n, 0.0001);
  side.ctau.assign(n, 0.0);
  side.gam.assign(n, 1.1);
  for (std::size_t i = 0; i < n; ++i) {
    side.xssi[i] = static_cast<double>(i) * 0.25;
    side.x.push_back(static_cast<double>(i) * 0.25);
    side.y.push_back(0.0);
  }
  side.iblte = 3;
  side.viscous = true;

  ex::BlExtendedDumpInput in;
  in.top = side;
  in.bottom = side;
  in.flow.qinf = 1.0;
  in.flow.minf = 0.3;
  in.flow.tklam = 0.0;
  in.reinf = 1.0e6;
  auto res = ex::boundaryLayerExtendedDumpRows(in);
  CHECK(res.isOk());
  // rows IBL=2..NBL per side.
  CHECK_EQ(res.value().topRows.size(), n - 1);
  CHECK_EQ(res.value().bottomRows.size(), n - 1);
  // PTAU trapezoidal integration for IBL=IBLTE(2)+1: sum of both sides.
  const auto& wakeRow = res.value().bottomRows[side.iblte];
  const double ptaiTop = 0.5 * (0.001 + 0.001)  * 0.25 * static_cast<double>(side.iblte);
  CHECK_NEAR(wakeRow.ptai, 2.0 * ptaiTop, 1e-12, 1e-12);

  auto bad = in;
  bad.top.xssi.pop_back();
  CHECK(ex::boundaryLayerExtendedDumpRows(bad).isErr());
}

TEST(unit, ExportVelocityRatioResample) {
  ex::VelocityRatioTable table;
  for (int i = 0; i <= 10; ++i) {
    table.s.push_back(static_cast<double>(i));
    table.vov.push_back(1.0 + 0.05 * std::sin(0.3 * i));
  }
  inverse::QSpecDistribution dist;
 // Convention: SSPEC is stored strictly monotonically increasing.
  for (int i = 0; i <= 20; ++i) {
    dist.s.push_back(i / 20.0);
  }
  dist.q.assign(dist.s.size(), 0.0);
  dist.qp.assign(dist.s.size(), 0.0);
  auto st = ex::resampleVelocityRatioTable(table, 1.0, 0.0, dist);
  CHECK(st.isOk());
  bool anyNonZero = false;
  for (double q : dist.q) {
    if (q != 0.0) {
      anyNonZero = true;
    }
  }
  CHECK(anyNonZero);

  ex::VelocityRatioTable empty;
  CHECK(ex::resampleVelocityRatioTable(empty, 1.0, 0.0, dist).isErr());
}

// --- GETVOV resample kernel: input and bounds contract -------------------
// The kernel normalizes, sorts and spline-resamples an (s, V/Vinf)
// table. Every element access must be dominated by an explicit count
// and parallel-field size proof; degenerate tables (empty, single
// point, inconsistent fields, zero arc-length span) must fail with a
// status of the existing class and must leave the QSpec distribution
// completely untouched.

namespace {

inverse::QSpecDistribution makeResampleDistribution() {
  inverse::QSpecDistribution dist;
  for (int i = 0; i <= 20; ++i) {
    dist.s.push_back(i / 20.0);
  }
  dist.q.assign(dist.s.size(), 0.0);
  dist.qp.assign(dist.s.size(), 0.0);
  return dist;
}

bool allFinite(const std::vector<double>& values) {
  for (double v : values) {
    if (!std::isfinite(v)) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(unit, ExportVelocityRatioResampleRejectsEmptyTable) {
  const ex::VelocityRatioTable empty;
  auto dist = makeResampleDistribution();
  const auto qBefore = dist.q;
  const auto qpBefore = dist.qp;
  const auto st = ex::resampleVelocityRatioTable(empty, 1.0, 0.0, dist);
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::invalidArgument);
  CHECK(dist.q == qBefore);
  CHECK(dist.qp == qpBefore);
}

TEST(unit, ExportVelocityRatioResampleRejectsSinglePointTable) {
  ex::VelocityRatioTable table;
  table.s.push_back(0.5);
  table.vov.push_back(1.0);
  auto dist = makeResampleDistribution();
  const auto qBefore = dist.q;
  const auto qpBefore = dist.qp;
  const auto st = ex::resampleVelocityRatioTable(table, 1.0, 0.0, dist);
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::invalidArgument);
  CHECK(dist.q == qBefore);
  CHECK(dist.qp == qpBefore);
}

TEST(unit, ExportVelocityRatioResampleAcceptsTwoPointTable) {
  // Smallest valid table: two points with a non-zero arc-length span.
  ex::VelocityRatioTable table;
  table.s.push_back(0.0);
  table.s.push_back(1.0);
  table.vov.push_back(1.0);
  table.vov.push_back(1.1);
  auto dist = makeResampleDistribution();
  const auto st = ex::resampleVelocityRatioTable(table, 1.0, 0.0, dist);
  CHECK(st.isOk());
  CHECK(allFinite(dist.q));
  CHECK(allFinite(dist.qp));
}

TEST(unit, ExportVelocityRatioResampleRejectsInconsistentParallelFields) {
  ex::VelocityRatioTable table;
  table.s.push_back(0.0);
  table.s.push_back(0.5);
  table.s.push_back(1.0);
  table.vov.push_back(1.0);
  table.vov.push_back(1.1);
  auto dist = makeResampleDistribution();
  const auto qBefore = dist.q;
  const auto qpBefore = dist.qp;
  const auto st = ex::resampleVelocityRatioTable(table, 1.0, 0.0, dist);
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::sizeMismatch);
  CHECK(dist.q == qBefore);
  CHECK(dist.qp == qpBefore);
}

TEST(unit, ExportVelocityRatioResampleRejectsZeroArcLengthSpan) {
  // w1[last] == w1[first]: the normalization would divide by zero; the
  // kernel must reject the table instead of producing NaN output.
  ex::VelocityRatioTable table;
  table.s.push_back(0.5);
  table.s.push_back(0.5);
  table.s.push_back(0.5);
  table.vov.push_back(1.0);
  table.vov.push_back(1.1);
  table.vov.push_back(1.2);
  auto dist = makeResampleDistribution();
  const auto qBefore = dist.q;
  const auto qpBefore = dist.qp;
  const auto st = ex::resampleVelocityRatioTable(table, 1.0, 0.0, dist);
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::invalidArgument);
  CHECK(dist.q == qBefore);
  CHECK(dist.qp == qpBefore);
}

TEST(unit, ExportVelocityRatioResampleValidResultIsDeterministic) {
  ex::VelocityRatioTable table;
  for (int i = 0; i <= 10; ++i) {
    table.s.push_back(static_cast<double>(i));
    table.vov.push_back(1.0 + 0.05 * std::sin(0.3 * i));
  }
  auto distA = makeResampleDistribution();
  auto distB = makeResampleDistribution();
  CHECK(ex::resampleVelocityRatioTable(table, 0.3, 2.0, distA).isOk());
  CHECK(ex::resampleVelocityRatioTable(table, 0.3, 2.0, distB).isOk());
  CHECK(distA.q == distB.q);
  CHECK(distA.qp == distB.qp);
  CHECK(allFinite(distA.q));
}


namespace {
ex::PlxaddInputs plxaddHarnessInputs() {
  ex::PlxaddInputs in;
  in.viscous = true;
  in.minf = 0.3;
  in.qinf = 0.0;  // harness: QINF unset -> QUE=0 (canonical inf/NaN)
  in.alfaRad = std::numeric_limits<double>::infinity();  // DTOR unset
  in.cl = 0.3;
  in.cd = 0.012;
  in.cm = -0.1;
  in.xoctrTop = 0.5;
  in.xoctrBottom = 0.4;
  in.x = {1.0, 0.5, 0.0, 0.5, 1.0, 0.5, 1.0};
  const std::vector<double> y = {0.0, 0.05, 0.0, -0.05, 0.0, 0.02, 0.0};
  in.s.assign(7, 0.0);
  in.xp.assign(7, 0.0);
  std::vector<double> yp(7, 0.0);
  math::scalc(std::span<const double>(in.x), std::span<const double>(y),
              std::span<double>(in.s));
  math::segspl(std::span<const double>(in.x), std::span<const double>(in.s),
               std::span<double>(in.xp));
  math::segspl(std::span<const double>(y), std::span<const double>(in.s),
               std::span<double>(yp));
  in.sst = 0.5 * (in.s[0] + in.s[6]);
  in.cpv.resize(7);
  for (int i = 1; i <= 7; ++i) {
    const double gam = 0.8 + 0.05 * i;
    in.cpv[static_cast<std::size_t>(i - 1)] = 1.0 - gam * gam;
  }
  for (int is = 0; is < 2; ++is) {
    ex::PlxaddSideInputs& side = in.sides[is];
    side.nbl = (is == 0) ? 5u : 6u;
    side.iblte = 4;
    const std::size_t cap = 7;  // NSIDE(IS)+1, COMMON zero beyond NBL
    side.ipan.assign(cap, 0);
    side.thet.assign(cap, 0.0);
    side.dstr.assign(cap, 0.0);
    side.ctau.assign(cap, 0.0);
    side.tau.assign(cap, 0.0);
    for (std::size_t ibl = 1; ibl <= side.nbl; ++ibl) {
      side.ipan[ibl] = static_cast<int>((ibl - 1) % 5) + 1;
      side.dstr[ibl] = 0.001 * static_cast<double>(ibl);
      side.thet[ibl] = 0.0005 * static_cast<double>(ibl);
      side.ctau[ibl] = 0.00002 * static_cast<double>(ibl);
      side.tau[ibl] = 0.0001 * static_cast<double>(ibl);
    }
  }
  return in;
}
}  // namespace

TEST(unit, ExportPlxaddProjection) {
  const ex::PlxaddInputs in = plxaddHarnessInputs();
  const auto res = ex::projectPlxaddPoint(in);
  CHECK(res.isOk());
  const ex::PlxaddPoint& p = res.value();
  // Seven force coefficients (alfa/DTOR inf: DTOR unset as in the harness).
  CHECK(std::isinf(p.alfaRad));
  CHECK_NEAR(p.cl, 0.3, 1e-12, 1e-12);
  CHECK_NEAR(p.cdTot, 0.012, 1e-12, 1e-12);
  CHECK_NEAR(p.zero, 0.0, 1e-12, 1e-12);
  CHECK_NEAR(p.cm, -0.1, 1e-12, 1e-12);
  CHECK_NEAR(p.xtTop, 0.5, 1e-12, 1e-12);
  CHECK_NEAR(p.xtBottom, 0.4, 1e-12, 1e-12);
  // NSIDE/IBLTE including MAX(...,2).
  CHECK_EQ(p.nside1, 6);
  CHECK_EQ(p.nside2, 6);
  CHECK_EQ(p.iblte1, 4);
  CHECK_EQ(p.iblte2, 4);
  CHECK_EQ(p.top.size(), 6u);
  CHECK_EQ(p.bottom.size(), 6u);
  // stagnation record of both sides identical.
  CHECK_NEAR(p.top[0].xx, 0.4967080861978665, 1e-12, 1e-9);
  CHECK_NEAR(p.top[0].cp, 1.0235733018456523, 1e-12, 1e-9);
  CHECK_NEAR(p.top[0].cf, 0.0, 1e-12, 1e-12);
  CHECK_NEAR(p.top[0].thet, 0.001, 1e-12, 1e-12);
  CHECK_NEAR(p.top[0].dstr, 0.002, 1e-12, 1e-12);
  CHECK_NEAR(p.top[0].ctau, 0.0, 1e-12, 1e-12);
  CHECK(p.bottom[0].xx == p.top[0].xx && p.bottom[0].cp == p.top[0].cp);
  // Inner station IBL=2.
  CHECK_NEAR(p.top[1].xx, 0.5, 1e-12, 1e-12);
  CHECK_NEAR(p.top[1].cp, 0.19, 1e-12, 1e-9);
  CHECK_NEAR(p.top[1].thet, 0.001, 1e-12, 1e-12);
  CHECK_NEAR(p.top[1].dstr, 0.002, 1e-12, 1e-12);
  CHECK(std::isinf(p.top[1].cf));  // QUE=0
  CHECK_NEAR(p.top[1].ctau, 4e-5, 1e-12, 1e-12);
  // TE station IBL=IBLTE=4.
  CHECK_NEAR(p.top[3].xx, 0.5, 1e-12, 1e-12);
  CHECK_NEAR(p.top[3].cp, 0.0, 1e-12, 1e-9);
  CHECK_NEAR(p.top[3].thet, 0.002, 1e-12, 1e-12);
  // wake station IBL=5.
  CHECK_NEAR(p.top[4].xx, 1.0, 1e-12, 1e-12);
  CHECK_NEAR(p.top[4].thet, 0.0025, 1e-12, 1e-12);
  // COMMON-zero station IBL=6 on side 1 (beyond NBL(1)=5).
  CHECK_NEAR(p.top[5].xx, 0.0, 1e-12, 1e-12);
  CHECK_NEAR(p.top[5].cp, 0.0, 1e-12, 1e-12);
  CHECK(std::isnan(p.top[5].cf));  // TAU=0, QUE=0
  // wake station IBL=6 on side 2 (valid, IPAN=1).
  CHECK_NEAR(p.bottom[5].xx, 1.0, 1e-12, 1e-12);
  CHECK_NEAR(p.bottom[5].cp, 0.2775, 1e-12, 1e-9);
  CHECK_NEAR(p.bottom[5].thet, 0.003, 1e-12, 1e-12);
}

TEST(unit, ExportPlxaddInviscidAndLimits) {
  ex::PlxaddInputs in = plxaddHarnessInputs();
  in.viscous = false;
  const auto res = ex::projectPlxaddPoint(in);
  CHECK(res.isOk());
  CHECK_NEAR(res.value().cdTot, 0.0, 1e-12, 1e-12);
  CHECK_NEAR(res.value().xtTop, 0.0, 1e-12, 1e-12);
  CHECK_NEAR(res.value().xtBottom, 0.0, 1e-12, 1e-12);
  // MAX(...,2): small sides.
  ex::PlxaddInputs small = plxaddHarnessInputs();
  for (int is = 0; is < 2; ++is) {
    small.sides[is].nbl = 2;
    small.sides[is].iblte = 2;
  }
  const auto smallRes = ex::projectPlxaddPoint(small);
  CHECK(smallRes.isOk());
  CHECK_EQ(smallRes.value().nside1, 2);
  CHECK_EQ(smallRes.value().nside2, 2);
}

// Fresh inviscid state. IBLPAN has never run (single producer, only
// viscous). NBL=IBLTE=0 (COMMON initial zero), side vectors are only
// placeholders. PLXADD writes NSIDE=(2,2), IBLTE=(0,0); station 1 is the
// stagnation synthesis (SEVAL(0)=X(1), Cp=1/(BETA+BFAC)), station 2 lies
// on the COMMON zero.
TEST(unit, ExportPlxaddInviscidFreshCommonState) {
  ex::PlxaddInputs in;
  in.viscous = false;
  in.minf = 0.0;
  in.qinf = 1.0;
  in.alfaRad = 0.0;
  in.cl = 0.24;
  in.cd = 0.0;
  in.cm = -0.003;
  in.x = {1.0, 0.5, 0.0, 0.5, 1.0};
  in.xp.assign(5, 0.0);
  in.s = {0.0, 0.4, 0.8, 1.2, 1.6};
  in.cpv.assign(5, 0.0);
  in.contourCount = 5;
  in.sst = 0.0;  // COMMON initial zero: STFIND/STMOVE run only for viscous cases
  for (auto& side : in.sides) {
    side.nbl = 0;
    side.iblte = 0;
    side.ipan.assign(1, 0);  // only placeholder
    side.thet.assign(1, 0.0);
    side.dstr.assign(1, 0.0);
    side.ctau.assign(1, 0.0);
    side.tau.assign(1, 0.0);
  }
  const ex::PlxaddInputs before = in;
  const auto res = ex::projectPlxaddPoint(in);
  CHECK(res.isOk());
  const auto& p = res.value();
  CHECK_EQ(p.nside1, 2);
  CHECK_EQ(p.nside2, 2);
  CHECK_EQ(p.iblte1, 0);
  CHECK_EQ(p.iblte2, 0);
  CHECK_EQ(p.top.size(), 2u);
  CHECK_EQ(p.bottom.size(), 2u);
  CHECK_NEAR(p.cdTot, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.xtTop, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.xtBottom, 0.0, 1e-15, 1e-15);
  // stagnation station: SEVAL(0) = X(1), CP = 1/(BETA+BFAC) = 1.
  CHECK(p.top[0].xx == in.x[0]);
  CHECK(p.bottom[0].xx == in.x[0]);
  CHECK_NEAR(p.top[0].cp, 1.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[0].thet, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[0].dstr, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[0].cf, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[0].ctau, 0.0, 1e-15, 1e-15);
  // station 2: never-written COMMON fields read as zero (IPAN=0 -> the
  // X(0)/CPV(0) read artefact of the reference binaries is the
  // zero-initialized COMMON neighbourhood, deterministically 0.0).
  CHECK_NEAR(p.top[1].xx, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].cp, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].thet, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].dstr, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].cf, 0.0, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].ctau, 0.0, 1e-15, 1e-15);
  // projection is read-only: inputs unchanged.
  CHECK(in.sides[0].nbl == before.sides[0].nbl);
  CHECK(in.sides[0].ipan == before.sides[0].ipan);
  CHECK(in.sides[0].thet == before.sides[0].thet);
  CHECK(in.x == before.x);
  CHECK(in.cpv == before.cpv);
}

// An inviscid point with COMMON left-over values from an earlier (viscous)
// computation must not blanket-zero the persistence: NBL/IBLTE remain at
// the initial zero, and the provided station fields are read unchanged by
// the projection.
TEST(unit, ExportPlxaddInviscidReadsPersistentCommonTail) {
  ex::PlxaddInputs in;
  in.viscous = false;
  in.minf = 0.0;
  in.qinf = 2.0;
  in.alfaRad = 0.0;
  in.cl = 0.1;
  in.cm = 0.0;
  in.x = {1.0, 0.5, 0.0, 0.5, 1.0};
  in.xp.assign(5, 0.0);
  in.s = {0.0, 0.4, 0.8, 1.2, 1.6};
  in.cpv.assign(5, 0.0);
  in.contourCount = 5;
  in.sst = 0.0;
  for (auto& side : in.sides) {
    side.nbl = 0;
    side.iblte = 0;
    side.ipan.assign(3, 0);
    side.thet.assign(3, 0.0);
    side.dstr.assign(3, 0.0);
    side.ctau.assign(3, 0.0);
    side.tau.assign(3, 0.0);
  }
  in.sides[0].thet[2] = 0.004;
  in.sides[0].dstr[2] = 0.005;
  in.sides[0].ctau[2] = 0.006;
  in.sides[0].tau[2] = 0.007;
  in.sides[1].thet[2] = 0.008;
  in.sides[1].dstr[2] = 0.010;
  const auto res = ex::projectPlxaddPoint(in);
  CHECK(res.isOk());
  const auto& p = res.value();
  CHECK_EQ(p.nside1, 2);
  CHECK_EQ(p.nside2, 2);
  CHECK_NEAR(p.top[1].thet, 0.004, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].dstr, 0.005, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].ctau, 0.006, 1e-15, 1e-15);
  CHECK_NEAR(p.top[1].cf, 0.007 / 2.0, 1e-15, 1e-15);  // TAU/QUE
  CHECK_NEAR(p.bottom[1].thet, 0.008, 1e-15, 1e-15);
  // stagnation averaging from persisted station 2 of both sides.
  CHECK_NEAR(p.top[0].thet, 0.006, 1e-15, 1e-15);
  CHECK_NEAR(p.top[0].dstr, 0.0075, 1e-15, 1e-15);
}

TEST(unit, ExportPlxaddValidationNoPartialResult) {
  ex::PlxaddInputs bad = plxaddHarnessInputs();
  bad.sides[0].ipan[3] = 0;  // valid range, but IPAN=0
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  bad = plxaddHarnessInputs();
  bad.sides[0].ipan[3] = 8;  // > N
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  bad = plxaddHarnessInputs();
  bad.sides[0].nbl = 1;  // no canonical state (start 0, IBLPAN >= 2)
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  bad = plxaddHarnessInputs();
  bad.sides[0].iblte = 9;  // > NBL
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  bad = plxaddHarnessInputs();
  bad.sides[0].thet[6] = 0.5;  // beyond NBL: finite persistent value
  CHECK(ex::projectPlxaddPoint(bad).isOk());  // COMMON tail is permitted
  bad = plxaddHarnessInputs();
  bad.sides[0].thet[6] = std::numeric_limits<double>::quiet_NaN();
  CHECK(ex::projectPlxaddPoint(bad).isErr());  // non-finite tail value
  bad = plxaddHarnessInputs();
  bad.cpv.pop_back();  // size inconsistency
  CHECK(ex::projectPlxaddPoint(bad).isErr());
}

// PLXADD reads IPAN up to NSIDE(IS); NSIDE(1) = IBLTE(1)+(NBL(2)-IBLTE(2))
// can exceed NBL(1). IBLPAN (xpanel.f, loop 35) writes the wake pointer of
// the lower side there (panel indices N+1..N+NW in the shared COMMON field
// X/CPV); never-written station fields remain COMMON zero, while written
// fields are preserved as multi-point state across the wake pointer
// path.
TEST(unit, ExportPlxaddPersistentWakePointerTail) {
  ex::PlxaddInputs in = plxaddHarnessInputs();
  in.contourCount = 5;  // N=5 contour, indices 6..7 are wake nodes
  auto& top = in.sides[0];
  top.ipan[6] = 7;  // canonical wake pointer I = N+IW
  top.thet[6] = 0.004;   // retained station value of an earlier point
  top.dstr[6] = 0.005;
  top.ctau[6] = 0.006;
  top.tau[6] = 0.007;
  const auto res = ex::projectPlxaddPoint(in);
  CHECK(res.isOk());
  const auto& st = res.value().top[5];
  CHECK_NEAR(st.xx, in.x[6], 1e-12, 1e-12);
  CHECK_NEAR(st.cp, in.cpv[6], 1e-12, 1e-12);
  CHECK_NEAR(st.thet, 0.004, 1e-12, 1e-12);
  CHECK_NEAR(st.dstr, 0.005, 1e-12, 1e-12);
  CHECK_NEAR(st.ctau, 0.006, 1e-12, 1e-12);
  // QUE=0 in the harness: CF = TAU/QUE -> inf.
  CHECK(std::isinf(st.cf));
}

TEST(unit, ExportPlxaddTailRejectionsAndNull) {
  ex::PlxaddInputs bad = plxaddHarnessInputs();
  bad.contourCount = 5;
  bad.sides[0].ipan[6] = 8;  // beyond N+NW
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  bad = plxaddHarnessInputs();
  bad.contourCount = 5;
  bad.sides[0].ipan[6] = -1;
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  // Null tail (never-written COMMON entry) stays permitted.
  const auto ok = ex::projectPlxaddPoint(plxaddHarnessInputs());
  CHECK(ok.isOk());
  CHECK_NEAR(ok.value().top[5].xx, 0.0, 1e-12, 1e-12);
  // contourCount larger than the combined field: rejected.
  bad = plxaddHarnessInputs();
  bad.contourCount = 9;
  CHECK(ex::projectPlxaddPoint(bad).isErr());
  // IPAN side vector too short: rejected.
  bad = plxaddHarnessInputs();
  bad.sides[0].ipan.pop_back();
  CHECK(ex::projectPlxaddPoint(bad).isErr());
}

TEST(unit, ExportPlxaddContourCountLimitsSeval) {
  // the wake node must not shift the stagnation-point SEVAL (Fortran
  // SEVAL(SST,X,XP,S,N)) when contourCount is set.
  ex::PlxaddInputs a = plxaddHarnessInputs();
  a.contourCount = 5;
  ex::PlxaddInputs b = a;
  b.x[5] = 9.0;
  b.x[6] = -9.0;
  const auto ra = ex::projectPlxaddPoint(a);
  const auto rb = ex::projectPlxaddPoint(b);
  CHECK(ra.isOk() && rb.isOk());
  CHECK(ra.value().top[0].xx == rb.value().top[0].xx);
  // without contourCount (backward compatibility) the full field acts:
  // the wake node shifts the SEVAL basis if S/XP are re-derived from the
  // full X field as in the harness.
  ex::PlxaddInputs c = plxaddHarnessInputs();
  c.x[6] = -9.0;
  const std::vector<double> y = {0.0, 0.05, 0.0, -0.05, 0.0, 0.02, 0.0};
  std::vector<double> yp(7, 0.0);
  math::scalc(std::span<const double>(c.x), std::span<const double>(y),
              std::span<double>(c.s));
  math::segspl(std::span<const double>(c.x), std::span<const double>(c.s),
               std::span<double>(c.xp));
  math::segspl(std::span<const double>(y), std::span<const double>(c.s),
               std::span<double>(yp));
  c.sst = 0.5 * (c.s[0] + c.s[6]);
  const auto rc = ex::projectPlxaddPoint(c);
  CHECK(rc.isOk());
  CHECK(rc.value().top[0].xx != ra.value().top[0].xx);
}

TEST(unit, ExportPlxaddProjectionDeterministicNoMutation) {
  const ex::PlxaddInputs original = plxaddHarnessInputs();
  ex::PlxaddInputs in = original;
  const auto first = ex::projectPlxaddPoint(in);
  const auto second = ex::projectPlxaddPoint(in);
  CHECK(first.isOk() && second.isOk());
  CHECK(first.value().top.size() == second.value().top.size());
  for (std::size_t i = 0; i < first.value().top.size(); ++i) {
    CHECK(first.value().top[i].xx == second.value().top[i].xx);
    CHECK(first.value().top[i].cp == second.value().top[i].cp);
    CHECK(first.value().top[i].thet == second.value().top[i].thet);
  }
  CHECK(in.x == original.x);
  CHECK(in.cpv == original.cpv);
  CHECK(in.sides[0].ipan == original.sides[0].ipan);
  CHECK(in.sides[1].thet == original.sides[1].thet);
}

// End: tests/core/PolarTests.cpp

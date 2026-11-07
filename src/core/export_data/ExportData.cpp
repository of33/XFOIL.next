// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

// File: src/core/export_data/ExportData.cpp
// Implementation of the export data projections.
// Ported from fortran-src/xoper.f (BLDUMP, BLDUMP2, CPDUMP) and
// fortran-src/xmdes.f (CNDUMP, GETVOV resampling).

#include "xfoil/core/export_data/ExportData.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/math/Sort.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::export_data {
namespace {

double at(const std::vector<double>& v, std::size_t i) {
  return i < v.size() ? v[i] : 0.0;
}

double ueTransform(double gamOrUedge, const DumpFlowState& flow) {
  // UE = (G/QINF)*(1-TKLAM)/(1-TKLAM*(G/QINF)^2) (xoper.f:1987/2205)
  const double r = gamOrUedge / flow.qinf;
  return r * (1.0 - flow.tklam) / (1.0 - flow.tklam * r * r);
}

double amsqFromUe(double ue, const DumpFlowState& flow) {
  // AMSQ = UE^2*HSTINV/(GAMM1*(1-0.5*UE^2*HSTINV)) (xoper.f:1988/2194)
  const double g1 = inviscid::kGamm1;
  const double hi = g1 * std::pow(flow.minf / flow.qinf, 2) /
                    (1.0 + 0.5 * g1 * flow.minf * flow.minf);
  return ue * ue * hi / (g1 * (1.0 - 0.5 * ue * ue * hi));
}

}  // namespace

BoundaryLayerDumpRecord boundaryLayerDumpRow(
    const BoundaryLayerDumpInput& in, const DumpFlowState& flow) {
  // BLDUMP (xoper.f:1957-2045).
  BoundaryLayerDumpRecord rec;
  rec.s = in.s;
  rec.x = in.x;
  rec.y = in.y;

  double ds = 0.0;
  double th = 0.0;
  double ts = 0.0;
  double cf = 0.0;
  double h = 1.0;
  double hs = 2.0;
  if (in.viscousStation) {
    ds = in.dstar;
    th = in.theta;
    ts = in.tstar;
    cf = in.tau / (0.5 * flow.qinf * flow.qinf);
    if (th == 0.0) {
      h = 1.0;
      hs = 1.0;
    } else {
      h = ds / th;
      hs = ts / th;
    }
  }
  const double ue = ueTransform(in.gam, flow);
  const double amsq = amsqFromUe(ue, flow);
  const double hk = boundary_layer::hkin(h, amsq).value;

  rec.ue = ue;
  rec.dstar = ds;
  rec.theta = th;
  rec.cf = cf;
  rec.hk = hk;
  rec.hs = hs;
  rec.thetaUe2 = th * ue * ue;
  rec.dstarUe = ds * ue;
  rec.tstarUe3 = ts * ue * ue * ue;
  return rec;
}

Result<BlExtendedDumpRows> boundaryLayerExtendedDumpRows(
    const BlExtendedDumpInput& in) {
  // BLDUMP2 (xoper.f:2053-2261).
  const BlSideStations& top = in.top;
  const BlSideStations& bottom = in.bottom;
  const std::size_t n1 = top.xssi.size();
  const std::size_t n2 = bottom.xssi.size();
  for (const BlSideStations& side : {top, bottom}) {
    const std::size_t n = side.xssi.size();
    if (side.uedg.size() != n || side.dstr.size() != n ||
        side.thet.size() != n || side.tstr.size() != n ||
        side.tau.size() != n || side.dis.size() != n ||
        side.ctau.size() != n || side.x.size() != n || side.y.size() != n ||
        side.gam.size() != n) {
      return Result<BlExtendedDumpRows>::fail(
          Error::make(ErrorCode::sizeMismatch,
                      "BLDUMP2: side arrays of differing length"));
    }
    if (n < 2 || side.iblte >= n) {
      return Result<BlExtendedDumpRows>::fail(
          Error::make(ErrorCode::invalidArgument,
                      "BLDUMP2: invalid station count or IBLTE"));
    }
  }

  // PTAU/EDIS trapezoidal integration (xoper.f:2118-2140), 0-based.
  std::vector<double> ptai1(n1, 0.0), edis1(n1, 0.0);
  std::vector<double> ptai2(n2, 0.0), edis2(n2, 0.0);
  for (std::size_t ibl = 1; ibl <= top.iblte; ++ibl) {
    const double dx = top.xssi[ibl] - top.xssi[ibl - 1];
    const double ta = 0.5 * (top.tau[ibl] + top.tau[ibl - 1]);
    const double da = 0.5 * (top.dis[ibl] + top.dis[ibl - 1]);
    ptai1[ibl] = ptai1[ibl - 1] + ta * dx;
    edis1[ibl] = edis1[ibl - 1] + da * dx;
  }
  for (std::size_t ibl = 1; ibl <= bottom.iblte; ++ibl) {
    const double dx = bottom.xssi[ibl] - bottom.xssi[ibl - 1];
    const double ta = 0.5 * (bottom.tau[ibl] + bottom.tau[ibl - 1]);
    const double da = 0.5 * (bottom.dis[ibl] + bottom.dis[ibl - 1]);
    ptai2[ibl] = ptai2[ibl - 1] + ta * dx;
    edis2[ibl] = edis2[ibl - 1] + da * dx;
  }
  if (bottom.iblte + 1 < n2) {
    ptai2[bottom.iblte + 1] = ptai1[top.iblte] + ptai2[bottom.iblte];
    edis2[bottom.iblte + 1] = edis1[top.iblte] + edis2[bottom.iblte];
    for (std::size_t ibl = bottom.iblte + 2; ibl < n2; ++ibl) {
      const double dx = bottom.xssi[ibl] - bottom.xssi[ibl - 1];
      const double ta = 0.5 * (bottom.tau[ibl] + bottom.tau[ibl - 1]);
      const double da = 0.5 * (bottom.dis[ibl] + bottom.dis[ibl - 1]);
      ptai2[ibl] = ptai2[ibl - 1] + ta * dx;
      edis2[ibl] = edis2[ibl - 1] + da * dx;
    }
  }

  BlExtendedDumpRows rows;

  auto projectSide = [&](const BlSideStations& side,
                         const std::vector<double>& ptai,
                         const std::vector<double>& edis, bool isBottom) {
    const std::size_t n = side.xssi.size();
    std::vector<BoundaryLayerExtendedDumpRecord>& out =
        isBottom ? rows.bottomRows : rows.topRows;
    for (std::size_t ibl = 1; ibl < n; ++ibl) {
      BoundaryLayerExtendedDumpRecord rec;
      std::size_t iblm = (ibl == 1) ? ibl : ibl - 1;
      std::size_t iblp = (ibl == side.iblte) ? ibl : ibl + 1;
      if (isBottom) {
        if (ibl == side.iblte + 1) {
          iblm = ibl;
        }
        if (ibl == n - 1) {
          iblp = ibl;
        }
      }
      double xs = side.xssi[ibl];
      if (isBottom && ibl > side.iblte) {
        xs = xs - bottom.xssi[bottom.iblte] + top.xssi[top.iblte];
      }
      rec.xs = xs;
      rec.x = side.x[ibl];
      rec.y = side.y[ibl];

      double ue = 0.0;
      double ds = 0.0, th = 0.0, ts = 0.0, cf = 0.0, cdis = 0.0;
      double ptaiVal = 0.0, ppei = 0.0, dprei = 0.0;
      double h = 1.0, hs = 1.0, hk = 1.0;
      double duds = 0.0, dmds = 0.0, dk = 0.0, ct = 0.0, rt = 0.0;
      if (side.viscous) {
        ue = side.uedg[ibl];
        ds = side.dstr[ibl];
        th = side.thet[ibl];
        ts = side.tstr[ibl];
        cf = side.tau[ibl] / (0.5 * in.flow.qinf * in.flow.qinf);
        cdis = side.dis[ibl] / std::pow(in.flow.qinf, 3);
        ptaiVal = ptai[ibl];
        ppei = th * ue * ue - ptaiVal;
        dprei = (1.0 - ue * ue) * 0.5;  // canonical override of the CPINC branch
        if (th == 0.0) {
          h = 1.0;
          hs = 1.0;
        } else {
          h = ds / th;
          hs = ts / th;
        }
        const double amsq = amsqFromUe(ue, in.flow);
        hk = boundary_layer::hkin(h, amsq).value;
        // Fortran reads IBLP=IBLTE+1 from the static COMMON (Zero).
        const double uep = at(side.uedg, iblp);
        const double uem = at(side.uedg, iblm);
        const double dsp = at(side.dstr, iblp);
        const double dsm = at(side.dstr, iblm);
        const double xsp = at(side.xssi, iblp);
        const double xsm = at(side.xssi, iblm);
        duds = (uep - uem) / (xsp - xsm);
        dmds = (uep * dsp - uem * dsm) / (xsp - xsm);
        dk = hk * side.thet[ibl];
        ct = side.ctau[ibl];
        rt = ue * th * in.reinf;
      } else {
        ue = ueTransform(side.gam[ibl], in.flow);
      }

      rec.ue = ue;
      rec.dstar = ds;
      rec.theta = th;
      rec.tstar = ts;
      rec.cf = cf;
      rec.cdis = cdis;
      rec.hk = hk;
      rec.hs = hs;
      rec.dstarUe = ds * ue;
      rec.thetaUe2 = th * ue * ue;
      rec.ptai = ptaiVal;
      rec.ppei = ppei;
      rec.halfTstarUe3 = 0.5 * ts * ue * ue * ue;
      rec.tau = side.tau[ibl];
      rec.dis = side.dis[ibl];
      rec.ueDstarDuds = -ue * ds * duds;
      rec.tauMinus = side.tau[ibl] - ue * ds * duds;
      rec.dv = th * ue * ue - dprei * dk;
      rec.dmds = dmds;
      rec.ctau = ct;
      rec.reTheta = rt;
      (void)edis;
      out.push_back(rec);
    }
  };

  projectSide(top, ptai1, edis1, false);
  projectSide(bottom, ptai2, edis2, true);
  return Result<BlExtendedDumpRows>::ok(std::move(rows));
}

PressureDumpRecord pressureDumpRow(double x, double gam,
                                   const DumpFlowState& flow) {
  // CPDUMP (xoper.f:2321-2342).
  const double beta = std::sqrt(1.0 - flow.minf * flow.minf);
  const double bfac = 0.5 * flow.minf * flow.minf / (1.0 + beta);
  const double cpinc = 1.0 - std::pow(gam / flow.qinf, 2);
  const double den = beta + bfac * cpinc;
  return PressureDumpRecord{x, cpinc / den};
}

Result<std::vector<CircleCoefficientDumpRecord>> circleCoefficientDumpRows(
    const std::vector<double>& cnRe, const std::vector<double>& cnIm,
    const std::vector<double>& piqRe, const std::vector<double>& piqIm,
    int mc, int nc) {
  // CNDUMP (xmdes.f:1753-1761).
  if (mc < 0 || nc < 1 || mc + 1 > static_cast<int>(cnRe.size()) ||
      mc + 1 > static_cast<int>(cnIm.size()) ||
      nc > static_cast<int>(piqRe.size()) ||
      nc > static_cast<int>(piqIm.size())) {
    return Result<std::vector<CircleCoefficientDumpRecord>>::fail(
        Error::make(ErrorCode::sizeMismatch, "CNDUMP: array sizes inconsistent"));
  }
  std::vector<CircleCoefficientDumpRecord> rows;
  rows.reserve(static_cast<std::size_t>(nc));
  for (int m = 0; m <= mc; ++m) {
    rows.push_back(CircleCoefficientDumpRecord{
        m, cnRe[static_cast<std::size_t>(m)],
        cnIm[static_cast<std::size_t>(m)],
        piqRe[static_cast<std::size_t>(m + 1)],
        piqIm[static_cast<std::size_t>(m + 1)]});
  }
  for (int m = mc + 1; m <= nc - 1; ++m) {
    rows.push_back(CircleCoefficientDumpRecord{
        m, 0.0, 0.0, piqRe[static_cast<std::size_t>(m + 1)],
        piqIm[static_cast<std::size_t>(m + 1)]});
  }
  return Result<std::vector<CircleCoefficientDumpRecord>>::ok(std::move(rows));
}

Status resampleVelocityRatioTable(const VelocityRatioTable& table,
                                  double qinf, double tklam,
                                  inverse::QSpecDistribution& distribution) {
  // GETVOV (xmdes.f:1773-1819), file-free kernel.
  if (table.s.size() != table.vov.size()) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "GETVOV: s/vov lengths diverge"));
  }
  if (distribution.s.empty() || distribution.q.size() != distribution.s.size()) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "GETVOV: Qspec distribution inconsistent"));
  }

  std::vector<double> w1 = table.s;
  std::vector<double> w2 = table.vov;

  // Normalization (xmdes.f:1793-1797) needs at least two arc-length
  // points and indexes both parallel fields over the count: prove the
  // count and the sizes of the accessed copies in the same dominating
  // flow before the first element access.
  std::size_t kk = w1.size();
  if (kk < 2 || w2.size() < kk) {
    return Status::fail(
        Error::make(ErrorCode::invalidArgument,
                    "GETVOV: fewer than two table points"));
  }

  // Normalization of the arc lengths (xmdes.f:1793-1797). A zero span
  // (all s equal) is degenerate: the normalization would divide by zero.
  const double sstart = w1.front();
  const double sspan = w1[kk - 1] - sstart;
  if (sspan == 0.0) {
    return Status::fail(
        Error::make(ErrorCode::invalidArgument,
                    "GETVOV: zero arc-length span"));
  }
  for (std::size_t k = 0; k < kk; ++k) {
    w1[k] = 1.0 - (w1[k] - sstart) / sspan;
  }

  // SORT: sort and remove duplicate pairs (xmdes.f:1800).
  math::sortAndRemovePairs(kk, std::span<double>(w1), std::span<double>(w2));
  if (kk < 2) {
    return Status::fail(
        Error::make(ErrorCode::invalidArgument,
                    "GETVOV: too few points after duplicate removal"));
  }

  // SPLIND with free boundary conditions (xmdes.f:1803).
  std::vector<double> w3(kk, 0.0);
  math::splind(std::span<const double>(w2.data(), kk),
               std::span<const double>(w1.data(), kk),
               std::span<double>(w3.data(), kk), -999.0, -999.0);

  // SEVAL on SSPEC and QINCOM (xmdes.f:1806-1815).
  for (std::size_t i = 0; i < distribution.s.size(); ++i) {
    const double qsnew =
        math::seval(distribution.s[i],
                    std::span<const double>(w2.data(), kk),
                    std::span<const double>(w3.data(), kk),
                    std::span<const double>(w1.data(), kk));
    distribution.q[i] = inverse::qIncompressible(qsnew, qinf, tklam);
  }

  // SPLQSP (xmdes.f:1818).
  return inverse::splQSpec(distribution);
}

// ---
// PLXADD (xpol.f:652).
// ---
Result<PlxaddPoint> projectPlxaddPoint(const PlxaddInputs& in) {
  const std::size_t n = in.x.size();
  const std::size_t ncontour = in.contourCount != 0 ? in.contourCount : n;
  if (ncontour < 2 || ncontour > n) {
    return Result<PlxaddPoint>::fail(
        Error::make(ErrorCode::sizeMismatch,
                    "PLXADD: contour node count N invalid or larger than "
                    "X(1..N+NW)"));
  }
  if (in.xp.size() != n || in.s.size() != n || in.cpv.size() != n) {
    return Result<PlxaddPoint>::fail(
        Error::make(ErrorCode::sizeMismatch,
                    "PLXADD: Contour-/CPV-vector sizes inconsistent"));
  }
  const std::size_t nbl1 = in.sides[0].nbl;
  const std::size_t nbl2 = in.sides[1].nbl;
  const std::size_t iblte1 = in.sides[0].iblte;
  const std::size_t iblte2 = in.sides[1].iblte;
  // NBL(IS) == 0 is the canonical COMMON initial state: IBLPAN (xpanel.f)
  // is the only producer of NBL/IBLTE/IPAN and runs exclusively in the
  // viscous path (xoper.f 2922, xpanel.f 1648). An inviscid point leaves
  // NBL/IBLTE at the COMMON zero. PLXADD (xpol.f 682-689) does not reject
  // this state: NSIDE(IS) = MAX(...,2) produces the canonical group of
  // two, and the never-written COMMON fields (IPAN/THET/DSTR/CTAU/TAU,
  // index 2 initialized to zero at the start) read as COMMON zero.
  // NBL(IS) == 1 is neither the initial state nor an IBLPAN outcome
  // (IBLPAN produces NBL >= 2) and remains rejected.
  if (nbl1 == 1 || nbl2 == 1) {
    return Result<PlxaddPoint>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "PLXADD: NBL(IS) == 1 is not a canonical state "
                    "(COMMON start is 0, IBLPAN produces NBL >= 2)"));
  }
  if (iblte1 > nbl1 || iblte2 > nbl2) {
    return Result<PlxaddPoint>::fail(
        Error::make(ErrorCode::outOfRange, "PLXADD: IBLTE(IS) > NBL(IS)"));
  }
  if (nbl1 > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
      nbl2 > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return Result<PlxaddPoint>::fail(
        Error::make(ErrorCode::capacityExceeded, "PLXADD: NBL overflow"));
  }
  if (iblte1 > nbl2 || iblte1 - iblte2 > nbl2) {
    // NSIDE(1) = IBLTE(1) + (NBL(2) - IBLTE(2)) must not become negative.
    return Result<PlxaddPoint>::fail(
        Error::make(ErrorCode::outOfRange,
                    "PLXADD: NSIDE(1) arithmetic underflow"));
  }
  std::size_t nside1 = iblte1 + (nbl2 - iblte2);
  std::size_t nside2 = nbl2;
  nside1 = std::max<std::size_t>(nside1, 2);  // canonical MAX(...,2)
  nside2 = std::max<std::size_t>(nside2, 2);

  // Range helpers: an entry beyond the provided vector length is a COMMON
  // field that has never been written (INITCOM state) and counts as COMMON zero.
  // The active range 2..NBL(IS) must be fully present (viscous contract
  // unchanged).
  const auto presentOrZero = [](const std::vector<double>& v,
                                std::size_t i) {
    return i < v.size() ? v[i] : 0.0;
  };
  const auto presentOrZeroInt = [](const std::vector<int>& v, std::size_t i) {
    return i < v.size() ? v[i] : 0;
  };
  for (int is = 0; is < 2; ++is) {
    const PlxaddSideInputs& side = in.sides[is];
    const std::size_t nside = (is == 0) ? nside1 : nside2;
    const std::size_t nbl = side.nbl;
    const std::size_t activeExtent = (nbl >= 2) ? nbl + 1 : 1;
    if (side.ipan.size() < activeExtent || side.thet.size() < activeExtent ||
        side.dstr.size() < activeExtent || side.ctau.size() < activeExtent ||
        side.tau.size() < activeExtent) {
      return Result<PlxaddPoint>::fail(
          Error::make(ErrorCode::sizeMismatch,
                      "PLXADD: side vectors are smaller than the active range "
                      "1..NBL(IS)"));
    }
    if (side.ipan.size() != side.thet.size() ||
        side.ipan.size() != side.dstr.size() ||
        side.ipan.size() != side.ctau.size() ||
        side.ipan.size() != side.tau.size()) {
      return Result<PlxaddPoint>::fail(
          Error::make(ErrorCode::sizeMismatch,
                      "PLXADD: side vectors inconsistent"));
    }
    for (std::size_t ibl = 2; ibl <= nbl; ++ibl) {
      const int ip = side.ipan[ibl];
      if (ip < 1 || ip > static_cast<int>(n)) {
        return Result<PlxaddPoint>::fail(
            Error::make(ErrorCode::outOfRange,
                        "PLXADD: IPAN(IBL,IS) outside 1..N"));
      }
    }
    // Persistent COMMON read range NBL(IS)+1..NSIDE(IS): IBLPAN
    // (xpanel.f, loop 35) writes the wake pointers below the page into
    // IPAN(IBLTE(1)+1..IBLTE(1)+NW,1); station field entries never
    // written remain as COMMON zeros. Permitted are therefore the
    // canonical written panel index 1..N+NW or the COMMON zero;
    // otherwise the entry is rejected.
    for (std::size_t ibl = nbl + 1; ibl <= nside; ++ibl) {
      const int ip = presentOrZeroInt(side.ipan, ibl);
      if (ip != 0 && (ip < 1 || ip > static_cast<int>(n))) {
        return Result<PlxaddPoint>::fail(
            Error::make(ErrorCode::outOfRange,
                        "PLXADD: persistent IPAN entry beyond NBL(IS) "
                        "outside 0 or 1..N+NW"));
      }
      if (ibl < side.thet.size()) {
        const double values[4] = {side.thet[ibl], side.dstr[ibl],
                                  side.ctau[ibl], side.tau[ibl]};
        for (const double value : values) {
          if (!std::isfinite(value)) {
            return Result<PlxaddPoint>::fail(
                Error::make(ErrorCode::invalidArgument,
                            "PLXADD: non-finite persistent "
                            "station-field value beyond NBL(IS)"));
          }
        }
      }
    }
  }

  const double beta = std::sqrt(1.0 - in.minf * in.minf);
  const double bfac = 0.5 * in.minf * in.minf / (1.0 + beta);
  const double que = 0.5 * in.qinf * in.qinf;

  PlxaddPoint out;
  out.alfaRad = in.alfaRad;
  out.cl = in.cl;
  out.cdTot = in.viscous ? in.cd : 0.0;
  out.zero = 0.0;
  out.cm = in.cm;
  out.xtTop = in.viscous ? in.xoctrTop : 0.0;
  out.xtBottom = in.viscous ? in.xoctrBottom : 0.0;
  out.nside1 = static_cast<int>(nside1);
  out.nside2 = static_cast<int>(nside2);
  out.iblte1 = static_cast<int>(iblte1);
  out.iblte2 = static_cast<int>(iblte2);

    // Stagnation point IBL=1 (both sides identical). SEVAL runs, as in
  // Fortran, only through the contour X(1..N), not through the wake node.
  PlxaddStation stag;
  stag.xx = math::seval(in.sst, std::span<const double>(in.x.data(), ncontour),
                        std::span<const double>(in.xp.data(), ncontour),
                        std::span<const double>(in.s.data(), ncontour));
  stag.cp = 1.0 / (beta + bfac);
  stag.cf = 0.0;
  stag.thet =
      0.5 * (presentOrZero(in.sides[0].thet, 2) + presentOrZero(in.sides[1].thet, 2));
  stag.dstr =
      0.5 * (presentOrZero(in.sides[0].dstr, 2) + presentOrZero(in.sides[1].dstr, 2));
  stag.ctau = 0.0;

  for (int is = 0; is < 2; ++is) {
    const PlxaddSideInputs& side = in.sides[is];
    const std::size_t nside = (is == 0) ? nside1 : nside2;
    std::vector<PlxaddStation>& dst = (is == 0) ? out.top : out.bottom;
    dst.reserve(nside);
    dst.push_back(stag);
    for (std::size_t ibl = 2; ibl <= nside; ++ibl) {
      PlxaddStation st;
      const int ip = presentOrZeroInt(side.ipan, ibl);
      if (ip >= 1) {
        st.xx = in.x[static_cast<std::size_t>(ip - 1)];
        st.cp = in.cpv[static_cast<std::size_t>(ip - 1)];
      }
      st.thet = presentOrZero(side.thet, ibl);
      st.dstr = presentOrZero(side.dstr, ibl);
      st.cf = presentOrZero(side.tau, ibl) / que;
      st.ctau = presentOrZero(side.ctau, ibl);
      dst.push_back(st);
    }
  }
  return Result<PlxaddPoint>::ok(std::move(out));
}

}  // namespace xfoil::core::export_data

// End: src/core/export_data/ExportData.cpp

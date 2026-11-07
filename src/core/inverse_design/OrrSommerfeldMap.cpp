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

// File: src/core/inverse_design/OrrSommerfeldMap.cpp
// Implementation of OSMAP (tricubic interpolation from the raw data
// passed in via fromRaw, already decoded) and NTCALC, ported from
// fortran-src/osmap.f and fortran-src/ntcalc.f. The numerical core
// performs no file, path or environment I/O; decoding osmap.dat
// is the job of a non-core adapter (test adapter outside).

#include "xfoil/core/inverse/OrrSommerfeldMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::inverse {
namespace {

constexpr std::size_t sz(int v) { return static_cast<std::size_t>(v); }

}  // namespace

Result<OrrSommerfeldMap> OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw&& raw) {
  OrrSommerfeldMap m;
  // Dimensions: at least 2 points per axis (spline/increment), at most
  // the Fortran PARAMETER bounds (NRX/NWX/NHX).
  if (raw.nr < 2 || raw.nw < 2 || raw.nh < 2) {
    return Result<OrrSommerfeldMap>::fail(
        Error::make(ErrorCode::invalidArgument, "OSMAP: NR/NW/NH < 2"));
  }
  if (raw.nr > kOsMaxR || raw.nw > kOsMaxW || raw.nh > kOsMaxH) {
    return Result<OrrSommerfeldMap>::fail(
        Error::make(ErrorCode::capacityExceeded, "OSMAP: array limit exceeded"));
  }
  if (static_cast<int>(raw.rl.size()) != raw.nr ||
      static_cast<int>(raw.wl.size()) != raw.nw ||
      static_cast<int>(raw.hl.size()) != raw.nh ||
      static_cast<int>(raw.ir1.size()) != raw.nh ||
      static_cast<int>(raw.ir2.size()) != raw.nh ||
      static_cast<int>(raw.iw1.size()) != raw.nh ||
      static_cast<int>(raw.iw2.size()) != raw.nh) {
    return Result<OrrSommerfeldMap>::fail(
        Error::make(ErrorCode::sizeMismatch, "OSMAP: coordinate/index sizes"));
  }
  const std::size_t ntot = sz(2 * kOsMaxH * kOsMaxW * kOsMaxR);
  for (const std::vector<double>* t :
       {&raw.a, &raw.ar, &raw.aw, &raw.ah, &raw.arw, &raw.arh, &raw.awh,
        &raw.arwh}) {
    if (t->size() != ntot) {
      return Result<OrrSommerfeldMap>::fail(
          Error::make(ErrorCode::sizeMismatch, "OSMAP: table size mismatch"));
    }
  }
  // Coordinates strictly monotonically increasing and finite (RL/WL/HL).
  auto checkCoord = [](const std::vector<double>& c, const char* what) -> bool {
    for (std::size_t i = 0; i < c.size(); ++i) {
      if (!std::isfinite(c[i])) return false;
      if (i > 0 && !(c[i] > c[i - 1])) {
        (void)what;
        return false;
      }
    }
    return true;
  };
  if (!checkCoord(raw.rl, "RL") || !checkCoord(raw.wl, "WL") ||
      !checkCoord(raw.hl, "HL")) {
    return Result<OrrSommerfeldMap>::fail(
        Error::make(ErrorCode::outOfRange, "OSMAP: coordinates not monotonic"));
  }
  // Per H-level: valid, non-empty cell ranges within the axes.
  for (int ih = 0; ih < raw.nh; ++ih) {
    const int a1 = raw.ir1[static_cast<std::size_t>(ih)];
    const int a2 = raw.ir2[static_cast<std::size_t>(ih)];
    const int b1 = raw.iw1[static_cast<std::size_t>(ih)];
    const int b2 = raw.iw2[static_cast<std::size_t>(ih)];
    if (a1 < 1 || a2 > raw.nr || a1 > a2 || b1 < 1 || b2 > raw.nw || b1 > b2) {
      return Result<OrrSommerfeldMap>::fail(
          Error::make(ErrorCode::outOfRange, "OSMAP: invalid cell range"));
    }
  }
  // Neighbouring H-levels must share at least one complete interpolation
  // cell (IR- and IW-range), otherwise the tricubic interpolation between
  // the levels is not defined. Cell [ir-1,ir] is valid if ir1 <= ir-1 and
  // ir <= ir2; the intersection contains >=1 cell exactly when
  // max(ir1) < min(ir2) (analogous for IW). 0-based vector indices,
  // neighbouring levels k and k-1.
  for (int k = 1; k < raw.nh; ++k) {
    const std::size_t i = static_cast<std::size_t>(k);
    const std::size_t p = static_cast<std::size_t>(k - 1);
    const int irlo = std::max(raw.ir1[i], raw.ir1[p]);
    const int irhi = std::min(raw.ir2[i], raw.ir2[p]);
    const int iwlo = std::max(raw.iw1[i], raw.iw1[p]);
    const int iwhi = std::min(raw.iw2[i], raw.iw2[p]);
    if (!(irlo < irhi) || !(iwlo < iwhi)) {
      return Result<OrrSommerfeldMap>::fail(
          Error::make(ErrorCode::outOfRange,
                      "OSMAP: neighbouring H-levels without shared cell"));
    }
  }
  // All values in the tables are finite.
  for (const std::vector<double>* t :
       {&raw.a, &raw.ar, &raw.aw, &raw.ah, &raw.arw, &raw.arh, &raw.awh,
        &raw.arwh}) {
    for (double v : *t) {
      if (!std::isfinite(v)) {
        return Result<OrrSommerfeldMap>::fail(
            Error::make(ErrorCode::outOfRange, "OSMAP: non-finite table value"));
      }
    }
  }
  m.nr_ = raw.nr;
  m.nw_ = raw.nw;
  m.nh_ = raw.nh;
  m.rl_ = std::move(raw.rl);
  m.wl_ = std::move(raw.wl);
  m.hl_ = std::move(raw.hl);
  m.ir1_ = std::move(raw.ir1);
  m.ir2_ = std::move(raw.ir2);
  m.iw1_ = std::move(raw.iw1);
  m.iw2_ = std::move(raw.iw2);
  m.a_ = std::move(raw.a);
  m.ar_ = std::move(raw.ar);
  m.aw_ = std::move(raw.aw);
  m.ah_ = std::move(raw.ah);
  m.arw_ = std::move(raw.arw);
  m.arh_ = std::move(raw.arh);
  m.awh_ = std::move(raw.awh);
  m.arwh_ = std::move(raw.arwh);
  m.ic1_ = raw.complete ? 1 : 2;
  m.ic2_ = 2;
  m.rincr_ = (m.rl_[sz(m.nr_ - 1)] - m.rl_[0]) / static_cast<double>(m.nr_ - 1);
  m.wincr_ = (m.wl_[sz(m.nw_ - 1)] - m.wl_[0]) / static_cast<double>(m.nw_ - 1);
  // The increments must be finite and strictly positive. Strictly monotone,
  // finite coordinates already guarantee > 0; the finiteness check catches
  // the coordinate span running through infinity (difference -> Inf).
  if (!std::isfinite(m.rincr_) || m.rincr_ <= 0.0 ||
      !std::isfinite(m.wincr_) || m.wincr_ <= 0.0) {
    return Result<OrrSommerfeldMap>::fail(
        Error::make(ErrorCode::outOfRange, "OSMAP: RINCR/WINCR not finite/positive"));
  }
  m.loaded_ = true;
  return Result<OrrSommerfeldMap>::ok(std::move(m));
}

OrrSommerfeldResult OrrSommerfeldMap::lookup(double rsp, double wsp,
                                             double hsp) const {
  OrrSommerfeldResult r;
  if (!loaded_ || nr_ == 0 || nw_ == 0 || nh_ == 0) {
    return r;
  }
  // Safe inputs: positive, finite arguments. For NaN/Inf/not-positive
  // the result is OK=false with exclusively finite zero values provided
  // (no log10(<=0), no NaN/Inf->int conversion, no OOB).
  if (!(rsp > 0.0) || !(wsp > 0.0) || !std::isfinite(rsp) ||
      !std::isfinite(wsp) || !std::isfinite(hsp)) {
    return r;
  }

  const double al10 = 2.302585093;
  double rlsp = std::log10(rsp);
  double wlsp = std::log10(wsp) + 0.5 * rlsp;
  double hlsp = hsp;
  bool ok = true;

  int ih = nh_;
  for (int k = 2; k <= nh_; ++k) {
    if (m_hl(k) >= hlsp) {
      ih = k;
      break;
    }
  }
  if (hlsp < m_hl(1) || hlsp > m_hl(nh_)) {
    hlsp = std::max(m_hl(1), std::min(m_hl(nh_), hlsp));
  }

  // Safe index determination: the raw value is clamped into the
  // representable range [1, n+2] before the int conversion. Valid cell
  // selection stays unchanged (valid values already lie in [2, n]);
  // extreme but finite rsp/wsp (e.g. for a very small increment) lead
  // not to UB or OOB, but to the existing OK=false clamping path.
  auto safeIndex = [](double raw, int n) -> int {
    if (!(raw > 1.0)) return 1;
    const double hi = static_cast<double>(n + 2);
    if (raw > hi) return n + 2;
    return static_cast<int>(raw);
  };
  int ir = safeIndex((rlsp - m_rl(1)) / rincr_ + 2.001, nr_);
  int ir1x = std::max(m_ir1(ih), m_ir1(ih - 1));
  int ir2x = std::min(m_ir2(ih), m_ir2(ih - 1));
  if (ir - 1 < ir1x || ir > ir2x) {
    ok = false;
    ir = std::max(ir1x + 1, std::min(ir2x, ir));
    rlsp = std::max(m_rl(1), std::min(m_rl(nr_), rlsp));
  }

  int iw = safeIndex((wlsp - m_wl(1)) / wincr_ + 2.001, nw_);
  int iw1x = std::max(m_iw1(ih), m_iw1(ih - 1));
  int iw2x = std::min(m_iw2(ih), m_iw2(ih - 1));
  if (iw - 1 < iw1x || iw > iw2x) {
    ok = false;
    iw = std::max(iw1x + 1, std::min(iw2x, iw));
    wlsp = std::max(m_wl(1), std::min(m_wl(nw_), wlsp));
  }

  const double drl = m_rl(ir) - m_rl(ir - 1);
  const double dwl = m_wl(iw) - m_wl(iw - 1);
  const double dhl = m_hl(ih) - m_hl(ih - 1);
  double tr = (rlsp - m_rl(ir - 1)) / drl;
  double tw = (wlsp - m_wl(iw - 1)) / dwl;
  double th = (hlsp - m_hl(ih - 1)) / dhl;
  tr = std::max(0.0, std::min(1.0, tr));
  tw = std::max(0.0, std::min(1.0, tw));
  th = std::max(0.0, std::min(1.0, th));

  double B[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BR[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BW[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BH[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BRW[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BRH[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BWH[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double BRWH[2][2] = {{0.0, 0.0}, {0.0, 0.0}};
  double C[2] = {0.0, 0.0};
  double CR[2] = {0.0, 0.0};
  double CW[2] = {0.0, 0.0};
  double CH[2] = {0.0, 0.0};
  double CRW[2] = {0.0, 0.0};
  double CRH[2] = {0.0, 0.0};
  double CWH[2] = {0.0, 0.0};
  double CRWH[2] = {0.0, 0.0};
  double aint[2] = {0.0, 0.0};
  double aintR[2] = {0.0, 0.0};
  double aintW[2] = {0.0, 0.0};
  double aintH[2] = {0.0, 0.0};
  double aintwR[2] = {0.0, 0.0};
  double aintwW[2] = {0.0, 0.0};
  double aintwH[2] = {0.0, 0.0};

  for (int ic = ic1_; ic <= ic2_; ++ic) {
    for (int kh = 1; kh <= 2; ++kh) {
      const int jh = ih + kh - 2;
      for (int kw = 1; kw <= 2; ++kw) {
        const int jw = iw + kw - 2;
        const double a1 = a_[idx(ir - 1, jw, jh, ic)];
        const double ar1 = ar_[idx(ir - 1, jw, jh, ic)];
        const double aw1 = aw_[idx(ir - 1, jw, jh, ic)];
        const double ah1 = ah_[idx(ir - 1, jw, jh, ic)];
        const double arw1 = arw_[idx(ir - 1, jw, jh, ic)];
        const double arh1 = arh_[idx(ir - 1, jw, jh, ic)];
        const double awh1 = awh_[idx(ir - 1, jw, jh, ic)];
        const double arwh1 = arwh_[idx(ir - 1, jw, jh, ic)];
        const double a2 = a_[idx(ir, jw, jh, ic)];
        const double ar2 = ar_[idx(ir, jw, jh, ic)];
        const double aw2 = aw_[idx(ir, jw, jh, ic)];
        const double ah2 = ah_[idx(ir, jw, jh, ic)];
        const double arw2 = arw_[idx(ir, jw, jh, ic)];
        const double arh2 = arh_[idx(ir, jw, jh, ic)];
        const double awh2 = awh_[idx(ir, jw, jh, ic)];
        const double arwh2 = arwh_[idx(ir, jw, jh, ic)];

        const double da1 = drl * ar1 - a2 + a1;
        const double da2 = drl * ar2 - a2 + a1;
        const double daw1 = drl * arw1 - aw2 + aw1;
        const double daw2 = drl * arw2 - aw2 + aw1;
        const double dah1 = drl * arh1 - ah2 + ah1;
        const double dah2 = drl * arh2 - ah2 + ah1;
        const double dawh1 = drl * arwh1 - awh2 + awh1;
        const double dawh2 = drl * arwh2 - awh2 + awh1;

        B[kw - 1][kh - 1] = (1.0 - tr) * a1 + tr * a2 +
                            ((1.0 - tr) * da1 - tr * da2) * (tr - tr * tr);
        BW[kw - 1][kh - 1] = (1.0 - tr) * aw1 + tr * aw2 +
                             ((1.0 - tr) * daw1 - tr * daw2) * (tr - tr * tr);
        BH[kw - 1][kh - 1] = (1.0 - tr) * ah1 + tr * ah2 +
                             ((1.0 - tr) * dah1 - tr * dah2) * (tr - tr * tr);
        BWH[kw - 1][kh - 1] = (1.0 - tr) * awh1 + tr * awh2 +
                              ((1.0 - tr) * dawh1 - tr * dawh2) * (tr - tr * tr);
        BR[kw - 1][kh - 1] =
            (a2 - a1 + (1.0 - 4.0 * tr + 3.0 * tr * tr) * da1 +
             (3.0 * tr - 2.0) * tr * da2) /
            drl;
        BRW[kw - 1][kh - 1] =
            (aw2 - aw1 + (1.0 - 4.0 * tr + 3.0 * tr * tr) * daw1 +
             (3.0 * tr - 2.0) * tr * daw2) /
            drl;
        BRH[kw - 1][kh - 1] =
            (ah2 - ah1 + (1.0 - 4.0 * tr + 3.0 * tr * tr) * dah1 +
             (3.0 * tr - 2.0) * tr * dah2) /
            drl;
        BRWH[kw - 1][kh - 1] =
            (awh2 - awh1 + (1.0 - 4.0 * tr + 3.0 * tr * tr) * dawh1 +
             (3.0 * tr - 2.0) * tr * dawh2) /
            drl;
      }
    }

    for (int kw = 1; kw <= 2; ++kw) {
      const double b1 = B[kw - 1][0], br1 = BR[kw - 1][0], bw1 = BW[kw - 1][0],
                   bh1 = BH[kw - 1][0], brw1 = BRW[kw - 1][0],
                   brh1 = BRH[kw - 1][0], bwh1 = BWH[kw - 1][0],
                   brwh1 = BRWH[kw - 1][0];
      const double b2 = B[kw - 1][1], br2 = BR[kw - 1][1], bw2 = BW[kw - 1][1],
                   bh2 = BH[kw - 1][1], brw2 = BRW[kw - 1][1],
                   brh2 = BRH[kw - 1][1], bwh2 = BWH[kw - 1][1],
                   brwh2 = BRWH[kw - 1][1];

      const double db1 = dhl * bh1 - b2 + b1;
      const double db2 = dhl * bh2 - b2 + b1;
      const double dbr1 = dhl * brh1 - br2 + br1;
      const double dbr2 = dhl * brh2 - br2 + br1;
      const double dbw1 = dhl * bwh1 - bw2 + bw1;
      const double dbw2 = dhl * bwh2 - bw2 + bw1;
      const double dbrw1 = dhl * brwh1 - brw2 + brw1;
      const double dbrw2 = dhl * brwh2 - brw2 + brw1;

      C[kw - 1] = (1.0 - th) * b1 + th * b2 +
                  ((1.0 - th) * db1 - th * db2) * (th - th * th);
      CR[kw - 1] = (1.0 - th) * br1 + th * br2 +
                   ((1.0 - th) * dbr1 - th * dbr2) * (th - th * th);
      CW[kw - 1] = (1.0 - th) * bw1 + th * bw2 +
                   ((1.0 - th) * dbw1 - th * dbw2) * (th - th * th);
      CRW[kw - 1] = (1.0 - th) * brw1 + th * brw2 +
                    ((1.0 - th) * dbrw1 - th * dbrw2) * (th - th * th);
      CH[kw - 1] = (b2 - b1 + (1.0 - 4.0 * th + 3.0 * th * th) * db1 +
                    (3.0 * th - 2.0) * th * db2) /
                   dhl;
      CRH[kw - 1] = (br2 - br1 + (1.0 - 4.0 * th + 3.0 * th * th) * dbr1 +
                     (3.0 * th - 2.0) * th * dbr2) /
                    dhl;
      CWH[kw - 1] = (bw2 - bw1 + (1.0 - 4.0 * th + 3.0 * th * th) * dbw1 +
                     (3.0 * th - 2.0) * th * dbw2) /
                    dhl;
      CRWH[kw - 1] = (brw2 - brw1 + (1.0 - 4.0 * th + 3.0 * th * th) * dbrw1 +
                      (3.0 * th - 2.0) * th * dbrw2) /
                     dhl;
    }

    const double c1 = C[0], cr1 = CR[0], cw1 = CW[0], ch1 = CH[0],
                 crw1 = CRW[0], cwh1 = CWH[0];
    const double c2 = C[1], cr2 = CR[1], cw2 = CW[1], ch2 = CH[1],
                 crw2 = CRW[1], cwh2 = CWH[1];

    const double dc1 = dwl * cw1 - c2 + c1;
    const double dc2 = dwl * cw2 - c2 + c1;
    const double dch1 = dwl * cwh1 - ch2 + ch1;
    const double dch2 = dwl * cwh2 - ch2 + ch1;
    const double dcr1 = dwl * crw1 - cr2 + cr1;
    const double dcr2 = dwl * crw2 - cr2 + cr1;

    const double aintRl = (1.0 - tw) * cr1 + tw * cr2 +
                          ((1.0 - tw) * dcr1 - tw * dcr2) * (tw - tw * tw);
    const double aintHl = (1.0 - tw) * ch1 + tw * ch2 +
                          ((1.0 - tw) * dch1 - tw * dch2) * (tw - tw * tw);
    aint[ic - 1] = (1.0 - tw) * c1 + tw * c2 +
                   ((1.0 - tw) * dc1 - tw * dc2) * (tw - tw * tw);
    const double aintWl =
        (c2 - c1 + (1.0 - 4.0 * tw + 3.0 * tw * tw) * dc1 +
         (3.0 * tw - 2.0) * tw * dc2) /
        dwl;
    const double aintwRl =
        (cr2 - cr1 + (1.0 - 4.0 * tw + 3.0 * tw * tw) * dcr1 +
         (3.0 * tw - 2.0) * tw * dcr2) /
        dwl;
    const double aintwHl =
        (ch2 - ch1 + (1.0 - 4.0 * tw + 3.0 * tw * tw) * dch1 +
         (3.0 * tw - 2.0) * tw * dch2) /
        dwl;
    const double aintwWl =
        ((6.0 * tw - 4.0) * dc1 + (6.0 * tw - 2.0) * dc2) / (dwl * dwl);

    aintR[ic - 1] = (aintRl + 0.5 * aintWl) / (al10 * rsp);
    aintW[ic - 1] = (aintWl) / (al10 * wsp);
    aintH[ic - 1] = aintHl;
    aintwR[ic - 1] = (aintwRl + 0.5 * aintwWl) / (al10 * al10 * wsp * rsp);
    aintwW[ic - 1] = (aintwWl - al10 * aintWl) / (al10 * al10 * wsp * wsp);
    aintwH[ic - 1] = (aintwHl) / (al10 * wsp);
  }

  (void)CRH;
  (void)CRWH;
  r.alfr = aint[0];
  r.alfrR = aintR[0];
  r.alfrW = aintW[0];
  r.alfrH = aintH[0];
  r.alfrwR = aintwR[0];
  r.alfrwW = aintwW[0];
  r.alfrwH = aintwH[0];
  r.alfi = aint[1];
  r.alfiR = aintR[1];
  r.alfiW = aintW[1];
  r.alfiH = aintH[1];
  r.alfiwR = aintwR[1];
  r.alfiwW = aintwW[1];
  r.alfiwH = aintwH[1];
  r.ok = ok;
  if (!ok) {
    r.alfrR = r.alfrW = r.alfrH = r.alfrwR = r.alfrwW = r.alfrwH = 0.0;
    r.alfiR = r.alfiW = r.alfiH = r.alfiwR = r.alfiwW = r.alfiwH = 0.0;
  }
  return r;
}

Result<NtCalcResult> ntCalc(const OrrSommerfeldMap& map, int nw,
                            std::span<const double> x,
                            std::span<const double> hk,
                            std::span<const double> th,
                            std::span<const double> ue,
                             std::span<const double> ve, int n) {
  if (!map.loaded()) {
    return Result<NtCalcResult>::fail(
        Error::make(ErrorCode::invalidState, "NTCALC: OSMAP table not loaded"));
  }
  if (n < 2 || nw < 2) {
    return Result<NtCalcResult>::fail(
        Error::make(ErrorCode::invalidArgument, "NTCALC: invalid quantity"));
  }
  if (static_cast<int>(x.size()) < n || static_cast<int>(hk.size()) < n ||
      static_cast<int>(th.size()) < n || static_cast<int>(ue.size()) < n ||
      static_cast<int>(ve.size()) < n) {
    return Result<NtCalcResult>::fail(
        Error::make(ErrorCode::sizeMismatch, "NTCALC: size conflict"));
  }

  NtCalcResult res;
  res.n = n;
  res.nw = nw;
  res.w.assign(sz(nw), 0.0);
  res.a.assign(sz(n) * sz(nw), 0.0);

  const double dw = -1.50 / static_cast<double>(nw - 1);

  int istart = -1;
  for (int i = 1; i <= n - 1; ++i) {
    const double rdl = std::log10(hk[sz(i - 1)] * th[sz(i - 1)] * ue[sz(i - 1)] /
                                  ve[sz(i - 1)]);
    const double hkb = 1.0 / (hk[sz(i - 1)] - 1.0);
    const double rdlc =
        2.23 + 1.35 * hkb + 0.85 * std::tanh(10.4 * hkb - 7.07) - 0.1;
    if (rdl >= rdlc) {
      istart = i;
      break;
    }
  }
  if (istart < 0) {
    return Result<NtCalcResult>::ok(std::move(res));
  }

  const double uot = ue[sz(istart - 1)] / th[sz(istart - 1)];
  for (int iw = 1; iw <= nw; ++iw) {
    const double wlog = -1.0 + dw * static_cast<double>(iw - 1);
    res.w[sz(iw - 1)] = std::pow(10.0, wlog) * uot;
  }

  for (int i = istart + 1; i <= n; ++i) {
    const int im = i - 1;
    double ua = (ue[sz(im - 1)] + ue[sz(i - 1)]) * 0.5;
    double va = (ve[sz(im - 1)] + ve[sz(i - 1)]) * 0.5;
    double ta = (th[sz(im - 1)] + th[sz(i - 1)]) * 0.5;
    double ha = (hk[sz(im - 1)] + hk[sz(i - 1)]) * 0.5;
    if (i == n) {
      ua = 1.5 * ue[sz(im - 1)] - 0.5 * ue[sz(im - 2)];
      va = 1.5 * ve[sz(im - 1)] - 0.5 * ve[sz(im - 2)];
      ta = 1.5 * th[sz(im - 1)] - 0.5 * th[sz(im - 2)];
      ha = 1.5 * hk[sz(im - 1)] - 0.5 * hk[sz(im - 2)];
    }
    const double rsp = ua * ta / va;
    double hsp = ha;
    hsp = std::min(hsp, 19.999);
    for (int iw = 1; iw <= nw; ++iw) {
      const double wsp = res.w[sz(iw - 1)] * ta / ua;
      const OrrSommerfeldResult os = map.lookup(rsp, wsp, hsp);
      const double dx = x[sz(i - 1)] - x[sz(im - 1)];
      double amp = res.a[sz(im - 1) * sz(nw) + sz(iw - 1)] - os.alfi * dx / ta;
      amp = std::max(amp, 0.0);
      res.a[sz(i - 1) * sz(nw) + sz(iw - 1)] = amp;
    }
  }
  return Result<NtCalcResult>::ok(std::move(res));
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/OrrSommerfeldMap.cpp

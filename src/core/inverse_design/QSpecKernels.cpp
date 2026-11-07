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

// File: src/core/inverse_design/QSpecKernels.cpp
// Port of SPLQSP, SMOOQ, QINCOM, GAMQSP, SYMQSP from fortran-src/xqdes.f.

#include "xfoil/core/inverse/QSpecKernels.h"

#include <cmath>
#include <span>

#include "xfoil/core/math/Spline.h"

namespace xfoil::core::inverse {

double qIncompressible(double qc, double qinf, double tklam) {
  if (tklam < 1.0e-4 || std::fabs(qc) < 1.0e-4) {
    return qc / (1.0 - tklam);
  }
  const double tmp = 0.5 * (1.0 - tklam) * qinf / (qc * tklam);
  return qinf * tmp * (std::sqrt(1.0 + 1.0 / (tklam * tmp * tmp)) - 1.0);
}

Status splQSpec(QSpecDistribution& d) {
  const std::size_t nsp = d.s.size();
  if (nsp < 4 || d.q.size() != nsp || d.qp.size() != nsp) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "SPLQSP: NSP >= 4 and consistent Sizes"));
  }
  // The spline coordinate must be strictly monotonically increasing
  // (canonical SSPEC).
  for (std::size_t i = 1; i < nsp; ++i) {
    if (!(d.s[i] > d.s[i - 1])) {
      return Status::fail(Error::make(ErrorCode::outOfRange,
                                      "SPLQSP: SSPEC not strictly monotonic"));
    }
  }
  // Usual spline with natural-end boundary conditions via QSPEC(2..NSP-1).
  math::splind(std::span<const double>(d.q).subspan(1, nsp - 2),
               std::span<const double>(d.s).subspan(1, nsp - 2),
               std::span<double>(d.qp).subspan(1, nsp - 2), -999.0, -999.0);
  // boundary interval I=1.
  math::splind(std::span<const double>(d.q).subspan(0, 2),
               std::span<const double>(d.s).subspan(0, 2),
               std::span<double>(d.qp).subspan(0, 2), -999.0, d.qp[1]);
  // boundary interval I=NSP-1.
  math::splind(std::span<const double>(d.q).subspan(nsp - 2, 2),
               std::span<const double>(d.s).subspan(nsp - 2, 2),
               std::span<double>(d.qp).subspan(nsp - 2, 2), d.qp[nsp - 2],
               -999.0);
  return Status::ok();
}

Status smooQ(QSpecDistribution& d, int kq1, int kq2, bool lqslop) {
  const std::size_t nsp = d.s.size();
  if (d.q.size() != nsp || d.qp.size() != nsp) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "SMOOQ: inconsistent sizes"));
  }
  if (kq1 < 0 || kq2 >= static_cast<int>(nsp)) {
    return Status::fail(Error::make(ErrorCode::outOfRange,
                                    "SMOOQ: segment bounds outside"));
  }
  if (kq2 - kq1 < 2) {
    return Status::ok();  // canonical: too short a segment, no smoothing
  }
  const std::vector<double>& w8 = d.s;
  const double smool = 0.002 * (w8[nsp - 1] - w8[0]);
  const double smoosq = smool * smool;

  std::vector<double> w1(nsp, 0.0), w2(nsp, 0.0), w3(nsp, 0.0);
  for (int I = kq1 + 1; I <= kq2 - 1; ++I) {
    const std::size_t i = static_cast<std::size_t>(I);
    const double dsm = w8[i] - w8[i - 1];
    const double dsp = w8[i + 1] - w8[i];
    const double dso = 0.5 * (w8[i + 1] - w8[i - 1]);
    w1[i] = smoosq * (-1.0 / dsm) / dso;
    w2[i] = smoosq * (1.0 / dsp + 1.0 / dsm) / dso + 1.0;
    w3[i] = smoosq * (-1.0 / dsp) / dso;
  }
  w2[static_cast<std::size_t>(kq1)] = 1.0;
  w3[static_cast<std::size_t>(kq1)] = 0.0;
  w1[static_cast<std::size_t>(kq2)] = 0.0;
  w2[static_cast<std::size_t>(kq2)] = 1.0;

  if (lqslop) {
    {
      const std::size_t i = static_cast<std::size_t>(kq1 + 1);
      const double dsm = w8[i] - w8[i - 1];
      const double dsp = w8[i + 1] - w8[i];
      const double ds = w8[i + 1] - w8[i - 1];
      w1[i] = -1.0 / dsm - (dsm / ds) / dsm;
      w2[i] = 1.0 / dsm + (dsm / ds) / dsm + (dsm / ds) / dsp;
      w3[i] = -(dsm / ds) / dsp;
      const double qspp1 = w1[i] * d.q[i - 1] + w2[i] * d.q[i] +
                           w3[i] * d.q[i + 1];
      d.q[i] = qspp1;
    }
    {
      const std::size_t i = static_cast<std::size_t>(kq2 - 1);
      const double dsm = w8[i] - w8[i - 1];
      const double dsp = w8[i + 1] - w8[i];
      const double ds = w8[i + 1] - w8[i - 1];
      w1[i] = (dsp / ds) / dsm;
      w2[i] = -1.0 / dsp - (dsp / ds) / dsp - (dsp / ds) / dsm;
      w3[i] = 1.0 / dsp + (dsp / ds) / dsp;
      const double qspp2 = w1[i] * d.q[i - 1] + w2[i] * d.q[i] +
                           w3[i] * d.q[i + 1];
      d.q[i] = qspp2;
    }
  }

  const std::size_t lo = static_cast<std::size_t>(kq1);
  const std::size_t kk = static_cast<std::size_t>(kq2 - kq1 + 1);
  math::triSol(std::span<double>(w2).subspan(lo, kk),
               std::span<const double>(w1).subspan(lo, kk),
               std::span<double>(w3).subspan(lo, kk),
               std::span<double>(d.q).subspan(lo, kk));
  return Status::ok();
}

Status gamQSpec(QSpecDistribution& d, const std::vector<double>& qgamm,
                double algam, double clgam, double cmgam, bool liqset, int& iq1,
                int& iq2) {
  const std::size_t nsp = d.s.size();
  if (d.q.size() != nsp || d.qp.size() != nsp || qgamm.size() != nsp) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "GAMQSP: inconsistent sizes"));
  }
  d.al = algam;
  d.cl = clgam;
  d.cm = cmgam;
  for (std::size_t i = 0; i < nsp; ++i) {
    d.q[i] = qgamm[i];
  }
  d.qdof0 = 0.0;
  d.qdof1 = 0.0;
  d.qdof2 = 0.0;
  d.qdof3 = 0.0;
  Status st = splQSpec(d);
  if (st.isErr()) return st;
  if (!liqset) {
    iq1 = 0;
    iq2 = static_cast<int>(nsp) - 1;
  }
  return Status::ok();
}

Status symQSpec(QSpecDistribution& d) {
  const std::size_t nsp = d.s.size();
  if (d.q.size() != nsp || d.qp.size() != nsp) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "SYMQSP: inconsistent sizes"));
  }
  d.al = 0.0;
  d.cl = 0.0;
  d.cm = 0.0;
  const double sspmid = 0.5 * (d.s[nsp - 1] - d.s[0]);
  const std::size_t half = (nsp + 1) / 2;
  for (std::size_t I = 1; I <= half; ++I) {
    const std::size_t i = I - 1;
    const std::size_t j = nsp - I;
    d.s[i] = sspmid + 0.5 * (d.s[i] - d.s[j]);
    d.q[i] = 0.5 * (d.q[i] - d.q[j]);
  }
  for (std::size_t I = half + 1; I <= nsp; ++I) {
    const std::size_t i = I - 1;
    const std::size_t j = nsp - I;
    d.s[i] = -d.s[j] + 2.0 * sspmid;
    d.q[i] = -d.q[j];
  }
  d.qdof0 = 0.0;
  d.qdof1 = 0.0;
  d.qdof2 = 0.0;
  d.qdof3 = 0.0;
  return splQSpec(d);
}

Status setTargetSegment(QSpecTargetSegment& seg, int iqnew1, int iqnew2,
                        int nsp) {
  if (nsp < 1) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "IQSGET: NSP < 1"));
  }
  if (iqnew1 < 0 || iqnew1 >= nsp || iqnew2 < 0 || iqnew2 >= nsp) {
    return Status::fail(Error::make(ErrorCode::outOfRange,
                                    "IQSGET: endpoint outside 0..NSP-1"));
  }
  if (iqnew1 == iqnew2) {
    // canonical: 'Endpoints must be distinct' / 'NEW SEGMENT NOT MARKED
    // OFF' / RETURN; no Partial-mutation result.
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "IQSGET: endpoints identical"));
  }
  // canonical: IQ1 = MIN0(IQNEW1,IQNEW2), IQ2 = MAX0(...); no Circulation.
  seg.iq1 = (iqnew1 < iqnew2) ? iqnew1 : iqnew2;
  seg.iq2 = (iqnew1 < iqnew2) ? iqnew2 : iqnew1;
  seg.liqset = true;
  return Status::ok();
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/QSpecKernels.cpp

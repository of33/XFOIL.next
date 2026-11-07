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

// File: src/core/boundary_layer_solver/BlForceReport.cpp
// FBLGET (dplot.f 320-503) in 0-based core notation. All
// numeric leaves (HKIN, PRWALL, UWALL, FS) are the bit-confirmed
// kernels; this kernel only adds the canonical
// station selection, interpolation and integration.

#include "xfoil/core/boundary_layer/BlForceReport.h"

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/inverse/WallProfileKernels.h"

#include <array>
#include <cmath>
#include <span>
#include <vector>

namespace xfoil::core::boundary_layer {

namespace {

constexpr int kNn = kBlForceProfilePoints;  // Fortran NN = KPRX = 129

}  // namespace

Result<BlForceReportResult> blForceReport(const BlForceReportInputs& in,
                                         double xpr, double ypr,
                                         double yint) {
  const std::size_t n = in.x.size();
  if (n != in.y.size() || n != in.s.size() || n != in.xp.size() ||
      n != in.yp.size() || n != in.nx.size() || n != in.ny.size()) {
    return Result<BlForceReportResult>::fail(Error::make(
        ErrorCode::sizeMismatch, "FBLGET: contour fields diverge"));
  }
  if (n < 2) {
    return Result<BlForceReportResult>::fail(
        Error::make(ErrorCode::invalidArgument, "FBLGET: contour too short"));
  }
  for (int is = 0; is < 2; ++is) {
    if (in.iblte[static_cast<std::size_t>(is)] < 1 || in.ipan[static_cast<std::size_t>(is)].size() <=
                                static_cast<std::size_t>(in.iblte[static_cast<std::size_t>(is)]) ||
        in.dstr[static_cast<std::size_t>(is)].size() <= static_cast<std::size_t>(in.iblte[static_cast<std::size_t>(is)]) ||
        in.thet[static_cast<std::size_t>(is)].size() <= static_cast<std::size_t>(in.iblte[static_cast<std::size_t>(is)]) ||
        in.uedg[static_cast<std::size_t>(is)].size() <= static_cast<std::size_t>(in.iblte[static_cast<std::size_t>(is)]) ||
        in.xssi[static_cast<std::size_t>(is)].size() <= static_cast<std::size_t>(in.iblte[static_cast<std::size_t>(is)])) {
      return Result<BlForceReportResult>::fail(
          Error::make(ErrorCode::invalidArgument,
                      "FBLGET: BL-Stationsfelder incomplete"));
    }
    for (int s0 = 1; s0 <= in.iblte[static_cast<std::size_t>(is)]; ++s0) {
      const int panel = in.ipan[static_cast<std::size_t>(is)][static_cast<std::size_t>(s0)];
      if (panel < 0 || static_cast<std::size_t>(panel) >= n) {
        return Result<BlForceReportResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "FBLGET: IPAN index outside the contour"));
      }
    }
  }
  if (in.qinf == 0.0) {
    return Result<BlForceReportResult>::fail(
        Error::make(ErrorCode::invalidArgument, "FBLGET: QINF = 0"));
  }

  BlForceReportResult out;
  BlForceReportSide& side = out.side;

  const double xc = xpr;
  const double yc = ypr;

  // Next Surfaces point (dplot.f 332-347).
  double rsqmin = 1.0e23;
  int ismin = 0;
  int iblmin = 0;  // 0-based station index
  const double doff = 0.00001 * (in.s[n - 1] - in.s[0]);
  for (int is = 0; is < 2; ++is) {
    for (int s0 = 1; s0 <= in.iblte[static_cast<std::size_t>(is)]; ++s0) {
      const int i = in.ipan[static_cast<std::size_t>(is)][static_cast<std::size_t>(s0)];
      const double xsurf = in.x[static_cast<std::size_t>(i)] +
                           doff * in.yp[static_cast<std::size_t>(i)];
      const double ysurf = in.y[static_cast<std::size_t>(i)] -
                           doff * in.xp[static_cast<std::size_t>(i)];
      const double rsq = (xc - xsurf) * (xc - xsurf) +
                         (yc - ysurf) * (yc - ysurf);
      if (rsq <= rsqmin) {
        rsqmin = rsq;
        ismin = is + 1;  // canonical 1-based
        iblmin = s0;     // canonical 1-based
      }
    }
  }

  const int is = ismin;
  const int is0 = is - 1;
  const int ibl0 = iblmin - 1;  // 0-based
  const int iblte0 = in.iblte[static_cast<std::size_t>(is0)];

  const int i = in.ipan[static_cast<std::size_t>(is0)][static_cast<std::size_t>(ibl0)];
  double crsp = (xc - in.x[static_cast<std::size_t>(i)]) *
                    in.ny[static_cast<std::size_t>(i)] -
                (yc - in.y[static_cast<std::size_t>(i)]) *
                    in.nx[static_cast<std::size_t>(i)];
  if (is == 2) {
    crsp = -crsp;
  }

  int iblp0;
  int iblo0;
  if (crsp > 0.0) {
    iblp0 = ibl0 + 1;
    iblo0 = ibl0;
  } else {
    iblp0 = ibl0;
    iblo0 = ibl0 - 1;
  }
  int isp0 = is0;
  int iso0 = is0;

  if (iblp0 > iblte0) {
    iblp0 = iblte0;
    iblo0 = iblp0 - 1;
  } else if (iblo0 < 1) {
    iblo0 = 1;
    iso0 = (iso0 == 0) ? 1 : 0;
  }

  const int ip = in.ipan[static_cast<std::size_t>(isp0)][static_cast<std::size_t>(iblp0)];
  const int io = in.ipan[static_cast<std::size_t>(iso0)][static_cast<std::size_t>(iblo0)];

  const double dx = in.x[static_cast<std::size_t>(ip)] -
                    in.x[static_cast<std::size_t>(io)];
  const double dy = in.y[static_cast<std::size_t>(ip)] -
                    in.y[static_cast<std::size_t>(io)];
  const double vx = xc - in.x[static_cast<std::size_t>(io)];
  const double vy = yc - in.y[static_cast<std::size_t>(io)];
  double frac = (dx * vx + dy * vy) / (dx * dx + dy * dy);
  frac = std::min(std::max(frac, 0.0), 1.0);

  double ca = frac * in.ny[static_cast<std::size_t>(ip)] +
              (1.0 - frac) * in.ny[static_cast<std::size_t>(io)];
  double sa = frac * in.nx[static_cast<std::size_t>(ip)] +
              (1.0 - frac) * in.nx[static_cast<std::size_t>(io)];
  const double csmod = std::sqrt(ca * ca + sa * sa);
  ca = ca / csmod;
  sa = sa / csmod;

  const auto pick = [](const std::span<const double>& field, int station0) {
    return field[static_cast<std::size_t>(station0)];
  };

  side.is = is;
  side.ibl = iblmin;
  side.iblo = iblo0 + 1;
  side.iblp = iblp0 + 1;
  side.iso = iso0 + 1;
  side.isp = isp0 + 1;

  side.x0 = frac * in.x[static_cast<std::size_t>(ip)] +
            (1.0 - frac) * in.x[static_cast<std::size_t>(io)];
  side.y0 = frac * in.y[static_cast<std::size_t>(ip)] +
            (1.0 - frac) * in.y[static_cast<std::size_t>(io)];

  side.ds = frac * pick(in.dstr[static_cast<std::size_t>(isp0)], iblp0) +
            (1.0 - frac) * pick(in.dstr[static_cast<std::size_t>(iso0)], iblo0);
  side.th = frac * pick(in.thet[static_cast<std::size_t>(isp0)], iblp0) +
            (1.0 - frac) * pick(in.thet[static_cast<std::size_t>(iso0)], iblo0);
  side.ue = frac * pick(in.uedg[static_cast<std::size_t>(isp0)], iblp0) +
            (1.0 - frac) * pick(in.uedg[static_cast<std::size_t>(iso0)], iblo0);
  side.xi = frac * pick(in.xssi[static_cast<std::size_t>(isp0)], iblp0) +
            (1.0 - frac) * pick(in.xssi[static_cast<std::size_t>(iso0)], iblo0);
  side.turb = side.xi > in.xssitr[static_cast<std::size_t>(is0)];

  // 1/(total enthalpy) (dplot.f 411).
  side.hstinv = in.gamm1 * (in.minf / in.qinf) * (in.minf / in.qinf) /
                (1.0 + 0.5 * in.gamm1 * in.minf * in.minf);

  // Sutherland-related quantities (dplot.f 417-424).
  const double uec = side.ue * (1.0 - in.tklam) /
                     (1.0 - in.tklam * (side.ue / in.qinf) *
                                  (side.ue / in.qinf));
  side.uec = uec;
  side.herat = (1.0 - 0.5 * side.hstinv * uec * uec) /
               (1.0 - 0.5 * side.hstinv * in.qinf * in.qinf);
  side.rhoe = std::pow(side.herat, 1.0 / in.gamm1);
  side.amue = std::sqrt(side.herat * side.herat * side.herat) *
              (1.0 + 0.35) / (side.herat + 0.35);
  side.rtheta = in.reinf * side.rhoe * side.ue * side.th / side.amue;

  side.amsq = uec * uec * side.hstinv /
              (in.gamm1 * (1.0 - 0.5 * uec * uec * side.hstinv));
  side.hk = hkin(side.ds / side.th, side.amsq).value;

  // Profile arrays (dplot.f 437-467), 0-based (NN = 129).
  const double uei = side.ue / in.qinf;
  std::vector<double> yy(static_cast<std::size_t>(kNn), 0.0);
  std::vector<double> uu(static_cast<std::size_t>(kNn), 0.0);
  const double uo = 1.0;
  const double dk = side.hk * side.th;

  if (side.turb) {
    inverse::WallProfileState st;
    const Status wall =
        inverse::prWall(dk, side.th, side.rtheta, side.amsq, 0.0, in.cffac, st);
    if (!wall.isOk()) {
      return Result<BlForceReportResult>::fail(wall.error());
    }
    const Status prof =
        inverse::uwWall(side.th, uo, st.do_, st.ui, side.rtheta, st.cf, st.bb,
                        kNn, yy, uu);
    if (!prof.isOk()) {
      return Result<BlForceReportResult>::fail(prof.error());
    }
    for (int k = 0; k < kNn; ++k) {
      uu[static_cast<std::size_t>(k)] *= uei;
    }
    side.de = st.do_;
    side.etae = 0.0;
  } else {
    const double etae = 1.5 * (3.15 + 1.72 / (side.hk - 1.0) + side.hk);
    const auto fs = inverse::falknerSkan(3, 2, 0.0, side.hk, kNn, etae, 1.0);
    if (!fs.isOk()) {
      return Result<BlForceReportResult>::fail(fs.error());
    }
    // Fortran dplot.f 462-466: FS returns ETA(1..N); YY(K) = ETA(K)*TH,
    // UU(K) = U(K)*UEI.
    for (int k = 0; k < kNn; ++k) {
      yy[static_cast<std::size_t>(k)] =
          fs.value().eta[static_cast<std::size_t>(k)] * side.th;
      uu[static_cast<std::size_t>(k)] =
          fs.value().u[static_cast<std::size_t>(k)] * uei;
    }
    side.de = etae * side.th;
    side.etae = etae;
  }

  // Integration up to YINT (dplot.f 469-502). The canonical Label 80 is
  // entered for YY(K+1) > YINT with loop index K; otherwise it is reached
  // after the loop ends with K = NN-1 and the canonical text
  // 'yint > ye' (here as the structured flag yintExceeded).
  double fint = 0.0;
  int k0 = 0;
  bool beyondProfile = true;
  for (int kk = 0; kk <= kNn - 2; ++kk) {
    if (yy[static_cast<std::size_t>(kk + 1)] > yint) {
      k0 = kk;
      beyondProfile = false;
      break;
    }
    const double dyy = yy[static_cast<std::size_t>(kk + 1)] -
                       yy[static_cast<std::size_t>(kk)];
    const double yya = (yy[static_cast<std::size_t>(kk + 1)] +
                        yy[static_cast<std::size_t>(kk)]) *
                       0.5;
    const double uua = (uu[static_cast<std::size_t>(kk + 1)] +
                        uu[static_cast<std::size_t>(kk)]) *
                       0.5;
    fint += uua * std::fabs(uua) * yya * dyy;
  }
  if (beyondProfile) {
    k0 = kNn - 2;
  }
  // The canonical 'yint > ye' is written exactly when the loop ran to
  // completion (YINT beyond the profile arrays); the jump to Label 80
  // suppresses the text.
  side.yintExceeded = beyondProfile;
  const double dyy = yint - yy[static_cast<std::size_t>(k0)];
  const double yya = (yint + yy[static_cast<std::size_t>(k0)]) * 0.5;
  const double uua = (uu[static_cast<std::size_t>(k0 + 1)] +
                      uu[static_cast<std::size_t>(k0)]) *
                     0.5;
  fint += uua * std::fabs(uua) * yya * dyy;
  side.fint = fint;

  side.yy = std::move(yy);
  side.uu = std::move(uu);
  return Result<BlForceReportResult>::ok(std::move(out));
}

}  // namespace xfoil::core::boundary_layer

// End: src/core/boundary_layer_solver/BlForceReport.cpp

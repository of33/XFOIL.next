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

// File: src/core/inverse_design/CircleGeometry.cpp
// Implementation of the mapping/geometry kernels for the circle
// mapping, ported from fortran-src/xmdes.f:
// ZCCALC -> zcCalc ZLEFIND -> zleFind ZCNORM -> zcNorm
// CNCALC -> calcCoefficients SCINIT -> initMappingCoordinate
//
// Formulas, operation/summation order, iteration limits and special cases
// are taken exactly from Fortran. Internal computations use
// double; the canonical Fortran values are REAL(4).

#include "xfoil/core/inverse/CircleGeometry.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inverse/CircleKernels.h"
#include "xfoil/core/math/LinearAlgebra.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::inverse {
namespace {

// Index in the row-major EIW table (nc lines, mc+1 columns).
std::size_t eiwIndex(int ic0, int m, int mc) {
  return static_cast<std::size_t>(ic0) * static_cast<std::size_t>(mc + 1) +
         static_cast<std::size_t>(m);
}

// Index in the row-major ZC_CN table (nc lines, kZcCnCols columns).
std::size_t zcCnIndex(int ic0, int m) {
  return static_cast<std::size_t>(ic0) * static_cast<std::size_t>(kZcCnCols) +
         static_cast<std::size_t>(m - 1);
}

// dz/dw factor on a circle grid point (Fortran SINWE*EXP(PIQ+i HWC)).
std::complex<double> dzdw(const CircleMappingState& s, int ic0) {
  const double sinw = 2.0 * std::sin(0.5 * s.grid.wc[static_cast<std::size_t>(ic0)]);
  double sinwe = 0.0;
  if (sinw > 0.0) {
    sinwe = std::pow(sinw, 1.0 - s.agte);
  }
  const double hwc =
      0.5 * (s.grid.wc[static_cast<std::size_t>(ic0)] - s.grid.pi) *
          (1.0 + s.agte) -
      0.5 * s.grid.pi;
  const std::complex<double> piq =
      s.coeff.piq[static_cast<std::size_t>(ic0)];
  return sinwe * std::exp(piq + std::complex<double>(0.0, hwc));
}

}  // namespace

void initState(CircleMappingState& state) {
  const int nc = state.grid.nc;
  const int mc = state.grid.mc;
  state.agte = 0.0;
  state.ag0 = 0.0;
  state.qim0 = 0.0;
  state.qimold = 0.0;
  state.sc.assign(static_cast<std::size_t>(nc), 0.0);
  state.scold.assign(static_cast<std::size_t>(nc), 0.0);
  state.xcold.assign(static_cast<std::size_t>(nc), 0.0);
  state.ycold.assign(static_cast<std::size_t>(nc), 0.0);
  state.dzte = std::complex<double>(0.0, 0.0);
  state.chordz = std::complex<double>(0.0, 0.0);
  state.zleold = std::complex<double>(0.0, 0.0);
  state.zcoldw.assign(static_cast<std::size_t>(nc), std::complex<double>(0.0, 0.0));
  state.zc.assign(static_cast<std::size_t>(nc), std::complex<double>(0.0, 0.0));
  state.zc_cn.assign(static_cast<std::size_t>(nc) *
                         static_cast<std::size_t>(kZcCnCols),
                     std::complex<double>(0.0, 0.0));
  state.coeff.cn.assign(static_cast<std::size_t>(mc + 1),
                        std::complex<double>(0.0, 0.0));
  state.coeff.piq.assign(static_cast<std::size_t>(nc),
                         std::complex<double>(0.0, 0.0));
}

void setTeGap(CircleMappingState& state, double dxnew, double dynew) {
  state.dzte = std::complex<double>(dxnew, dynew);
}

void setTeAngleDeg(CircleMappingState& state, double agted) {
  state.agte = agted / 180.0;
}

Status zcCalc(CircleMappingState& state, int mtest) {
  const int nc = state.grid.nc;
  const int mc = state.grid.mc;
  if (nc <= 0 || mc < 0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  if (mtest < 0 || mtest > kZcCnCols) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  if (static_cast<int>(state.coeff.piq.size()) != nc ||
      static_cast<int>(state.zc.size()) != nc ||
      static_cast<int>(state.zc_cn.size()) != nc * kZcCnCols) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch, "CircleGeometry: size conflict"));
  }

  // IC = 1 (Index 0): z = (4,0), Sensitivities 0.
  state.zc[0] = std::complex<double>(4.0, 0.0);
  for (int m = 1; m <= mtest; ++m) {
    state.zc_cn[zcCnIndex(0, m)] = std::complex<double>(0.0, 0.0);
  }
  std::complex<double> dzdw1 = dzdw(state, 0);

  for (int ic = 2; ic <= nc; ++ic) {
    const int i0 = ic - 1;
    const std::complex<double> dzdw2 = dzdw(state, i0);
    state.zc[static_cast<std::size_t>(i0)] =
        0.5 * (dzdw1 + dzdw2) * state.grid.dwc +
        state.zc[static_cast<std::size_t>(i0 - 1)];
    const std::complex<double> dzpiq1 = 0.5 * dzdw1 * state.grid.dwc;
    const std::complex<double> dzpiq2 = 0.5 * dzdw2 * state.grid.dwc;
    for (int m = 1; m <= mtest; ++m) {
      state.zc_cn[zcCnIndex(i0, m)] =
          dzpiq1 * std::conj(state.grid.eiw[eiwIndex(i0 - 1, m, mc)]) +
          dzpiq2 * std::conj(state.grid.eiw[eiwIndex(i0, m, mc)]) +
          state.zc_cn[zcCnIndex(i0 - 1, m)];
    }
    dzdw1 = dzdw2;
  }

  // Arc length s(w) and Normalization.
  state.sc[0] = 0.0;
  for (int ic = 2; ic <= nc; ++ic) {
    const int i0 = ic - 1;
    state.sc[static_cast<std::size_t>(i0)] =
        state.sc[static_cast<std::size_t>(i0 - 1)] +
        std::abs(state.zc[static_cast<std::size_t>(i0)] -
                 state.zc[static_cast<std::size_t>(i0 - 1)]);
  }
  const double scEnd = state.sc[static_cast<std::size_t>(nc - 1)];
  for (int ic = 1; ic <= nc; ++ic) {
    state.sc[static_cast<std::size_t>(ic - 1)] /= scEnd;
  }
  return Status::ok();
}

std::complex<double> zleFind(const CircleMappingState& state,
                             bool* lefindFailed) {
  const int nc = state.grid.nc;
  const std::complex<double> zte =
      0.5 * (state.zc[0] + state.zc[static_cast<std::size_t>(nc - 1)]);

  int icle = 1;
  double dmax = 0.0;
  for (int ic = 1; ic <= nc; ++ic) {
    const double dist =
        std::abs(state.zc[static_cast<std::size_t>(ic - 1)] - zte);
    if (dist > dmax) {
      dmax = dist;
      icle = ic;
    }
  }

  const int ic1 = std::max(icle - (kZleNtx - 1) / 2, 1);
  const int ic2 = std::min(icle + (kZleNtx - 1) / 2, nc);

  const std::complex<double> dzdw1 = dzdw(state, ic1 - 1);
  const std::complex<double> dzdw2 = dzdw(state, ic2 - 1);

  const int nic = ic2 - ic1 + 1;
  std::vector<double> xc(static_cast<std::size_t>(nic));
  std::vector<double> yc(static_cast<std::size_t>(nic));
  for (int ic = ic1; ic <= ic2; ++ic) {
    const int i = ic - ic1;
    xc[static_cast<std::size_t>(i)] =
        std::real(state.zc[static_cast<std::size_t>(ic - 1)]);
    yc[static_cast<std::size_t>(i)] =
        std::imag(state.zc[static_cast<std::size_t>(ic - 1)]);
  }
  std::vector<double> xcw(static_cast<std::size_t>(nic));
  std::vector<double> ycw(static_cast<std::size_t>(nic));
  const std::span<const double> sspan(
      state.grid.wc.data() + (ic1 - 1), static_cast<std::size_t>(nic));

  math::splind(xc, sspan, xcw, std::real(dzdw1), std::real(dzdw2));
  math::splind(yc, sspan, ycw, std::imag(dzdw1), std::imag(dzdw2));

  const double xcte =
      0.5 * std::real(state.zc[0] + state.zc[static_cast<std::size_t>(nc - 1)]);
  const double ycte =
      0.5 * std::imag(state.zc[0] + state.zc[static_cast<std::size_t>(nc - 1)]);

  double wcle = state.grid.wc[static_cast<std::size_t>(icle - 1)];
  bool converged = false;
  for (int itcle = 1; itcle <= 10; ++itcle) {
    const double xcle = math::seval(wcle, xc, xcw, sspan);
    const double ycle = math::seval(wcle, yc, ycw, sspan);
    const double dxdw = math::deval(wcle, xc, xcw, sspan);
    const double dydw = math::deval(wcle, yc, ycw, sspan);
    const double dxdd = math::d2val(wcle, xc, xcw, sspan);
    const double dydd = math::d2val(wcle, yc, ycw, sspan);

    const double xchord = xcle - xcte;
    const double ychord = ycle - ycte;
    const double res = xchord * dxdw + ychord * dydw;
    const double resw =
        dxdw * dxdw + dydw * dydw + xchord * dxdd + ychord * dydd;
    const double dwcle = -res / resw;
    wcle += dwcle;
    if (std::abs(dwcle) < 1.0e-5) {
      converged = true;
      break;
    }
  }
  if (!converged) {
    if (lefindFailed != nullptr) {
      *lefindFailed = true;
    }
    wcle = state.grid.wc[static_cast<std::size_t>(icle - 1)];
  }

  const double xcle = math::seval(wcle, xc, xcw, sspan);
  const double ycle = math::seval(wcle, yc, ycw, sspan);
  return std::complex<double>(xcle, ycle);
}

Status zcNorm(CircleMappingState& state, int mtest, bool* lefindFailed) {
  const int nc = state.grid.nc;
  if (nc <= 0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  if (mtest < 0 || mtest > kZcCnCols) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  if (static_cast<int>(state.coeff.cn.size()) < 1) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch, "CircleGeometry: size conflict"));
  }

  const std::complex<double> zle = zleFind(state, lefindFailed);
  for (int ic = 1; ic <= nc; ++ic) {
    state.zc[static_cast<std::size_t>(ic - 1)] -= zle;
  }

  const std::complex<double> zte =
      0.5 * (state.zc[0] + state.zc[static_cast<std::size_t>(nc - 1)]);
  std::vector<std::complex<double>> zteCn(static_cast<std::size_t>(mtest));
  for (int m = 1; m <= mtest; ++m) {
    zteCn[static_cast<std::size_t>(m - 1)] =
        0.5 * (state.zc_cn[zcCnIndex(0, m)] +
               state.zc_cn[zcCnIndex(nc - 1, m)]);
  }

  for (int ic = 1; ic <= nc; ++ic) {
    const int i0 = ic - 1;
    const std::complex<double> zcnew =
        state.chordz * state.zc[static_cast<std::size_t>(i0)] / zte;
    const std::complex<double> zczte = -zcnew / zte;
    state.zc[static_cast<std::size_t>(i0)] = zcnew;
    for (int m = 1; m <= mtest; ++m) {
      state.zc_cn[zcCnIndex(i0, m)] =
          state.chordz * state.zc_cn[zcCnIndex(i0, m)] / zte +
          zczte * zteCn[static_cast<std::size_t>(m - 1)];
    }
  }

  const double qimoff =
      -std::imag(std::log(state.chordz / zte));
  state.coeff.cn[0] -= std::complex<double>(0.0, qimoff);

  for (int ic = 1; ic <= nc; ++ic) {
    state.zc[static_cast<std::size_t>(ic - 1)] += state.zleold;
  }
  return Status::ok();
}

Status calcCoefficients(CircleMappingState& state,
                        std::span<const double> qc, bool lsymm) {
  const int nc = state.grid.nc;
  const int mc = state.grid.mc;
  if (nc <= 0 || mc < 0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  if (static_cast<int>(qc.size()) != nc ||
      static_cast<int>(state.coeff.cn.size()) != mc + 1 ||
      static_cast<int>(state.coeff.piq.size()) != nc) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch, "CircleGeometry: size conflict"));
  }

  std::vector<double> qcw(static_cast<std::size_t>(nc));
  math::splind(qc, state.grid.wc, qcw, -999.0, -999.0);

  int icStag = nc + 1;
  for (int ic = 2; ic <= nc; ++ic) {
    if (qc[static_cast<std::size_t>(ic - 1)] < 0.0) {
      icStag = ic;
      break;
    }
  }
  if (icStag > nc) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  double wcle = state.grid.wc[static_cast<std::size_t>(icStag - 1)];
  const math::Diagnostic diag =
      math::sinvrt(wcle, 0.0, qc, qcw, state.grid.wc);
  (void)diag;

  const double alfCir = 0.5 * (wcle - state.grid.pi);
  for (int ic = 2; ic <= nc - 1; ++ic) {
    const int i0 = ic - 1;
    const double cosw =
        2.0 * std::cos(0.5 * state.grid.wc[static_cast<std::size_t>(i0)] - alfCir);
    const double sinw =
        2.0 * std::sin(0.5 * state.grid.wc[static_cast<std::size_t>(i0)]);
    const double sinwe = std::pow(sinw, state.agte);
    double pfun = 0.0;
    if (std::abs(cosw) < 1.0e-4) {
      pfun = std::abs(sinwe / qcw[static_cast<std::size_t>(i0)]);
    } else {
      pfun = std::abs(cosw * sinwe / qc[static_cast<std::size_t>(i0)]);
    }
    state.coeff.piq[static_cast<std::size_t>(i0)] =
        std::complex<double>(std::log(pfun), 0.0);
  }

  state.coeff.piq[0] = 3.0 * state.coeff.piq[1] -
                       3.0 * state.coeff.piq[2] + state.coeff.piq[3];
  state.coeff.piq[static_cast<std::size_t>(nc - 1)] =
      3.0 * state.coeff.piq[static_cast<std::size_t>(nc - 2)] -
      3.0 * state.coeff.piq[static_cast<std::size_t>(nc - 3)] +
      state.coeff.piq[static_cast<std::size_t>(nc - 4)];

  std::vector<std::complex<double>> cnsav = state.coeff.cn;
  const Status ftp = fourierTransform(state.grid, state.coeff);
  if (ftp.isErr()) {
    return ftp;
  }
  state.coeff.cn[0] = std::complex<double>(0.0, state.qimold);
  if (lsymm) {
    for (int m = 1; m <= mc; ++m) {
      const double cnr = 2.0 * std::real(state.coeff.cn[static_cast<std::size_t>(m)]) -
                         std::real(cnsav[static_cast<std::size_t>(m)]);
      state.coeff.cn[static_cast<std::size_t>(m)] =
          std::complex<double>(cnr, 0.0);
    }
  }
  return piqSum(state.grid, state.coeff);
}

Status initMappingCoordinate(CircleMappingState& state,
                             std::span<const double> x,
                             std::span<const double> xp,
                             std::span<const double> y,
                             std::span<const double> yp,
                             std::span<const double> s, double sle,
                             ScinitDiagnostics* diag) {
  const int nc = state.grid.nc;
  const int n = static_cast<int>(x.size());
  if (nc <= 0 || n < 2) {
    return Status::fail(Error::make(ErrorCode::invalidArgument, "CircleGeometry: invalid argument"));
  }
  if (static_cast<int>(xp.size()) != n || static_cast<int>(y.size()) != n ||
      static_cast<int>(yp.size()) != n || static_cast<int>(s.size()) != n) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch, "CircleGeometry: size conflict"));
  }
  if (static_cast<int>(state.sc.size()) != nc ||
      static_cast<int>(state.scold.size()) != nc ||
      static_cast<int>(state.xcold.size()) != nc ||
      static_cast<int>(state.ycold.size()) != nc ||
      static_cast<int>(state.zcoldw.size()) != nc) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch, "CircleGeometry: size conflict"));
  }

  const double pi = state.grid.pi;
  state.agte = (std::atan2(xp[static_cast<std::size_t>(n - 1)],
                           -yp[static_cast<std::size_t>(n - 1)]) -
                std::atan2(xp[0], -yp[0])) /
                   pi -
               1.0;
  state.ag0 = std::atan2(xp[0], -yp[0]);
  state.qim0 = state.ag0 + 0.5 * pi * (1.0 + state.agte);

  const double xle = math::seval(sle, x, xp, s);
  const double yle = math::seval(sle, y, yp, s);

  const double dxte = x[0] - x[static_cast<std::size_t>(n - 1)];
  const double dyte = y[0] - y[static_cast<std::size_t>(n - 1)];
  state.dzte = std::complex<double>(dxte, dyte);
  if (diag != nullptr) {
    // Format-1100 values at the canonical SCINIT time point.
    diag->dxte = dxte;
    diag->dyte = dyte;
    diag->teAngleDeg = state.agte * 180.0;
  }

  const double chordx =
      0.5 * (x[0] + x[static_cast<std::size_t>(n - 1)]) - xle;
  const double chordy =
      0.5 * (y[0] + y[static_cast<std::size_t>(n - 1)]) - yle;
  state.chordz = std::complex<double>(chordx, chordy);
  state.zleold = std::complex<double>(xle, yle);

  const double cvle = math::curv(sle, x, xp, y, yp, s) * s[static_cast<std::size_t>(n - 1)];
  const double cvabs = std::abs(cvle);
  const double dsdwle = std::max(1.0e-3, 0.5 / cvabs);

  const double tops = sle / s[static_cast<std::size_t>(n - 1)];
  const double bots = (s[static_cast<std::size_t>(n - 1)] - sle) /
                      s[static_cast<std::size_t>(n - 1)];

  double wwt = 1.0 - 2.0 * dsdwle / tops;
  for (int ic = 1; ic <= (nc - 1) / 2 + 1; ++ic) {
    state.sc[static_cast<std::size_t>(ic - 1)] =
        tops * (1.0 - std::cos(wwt * state.grid.wc[static_cast<std::size_t>(ic - 1)])) /
        (1.0 - std::cos(wwt * pi));
  }
  wwt = 1.0 - 2.0 * dsdwle / bots;
  for (int ic = (nc - 1) / 2 + 2; ic <= nc; ++ic) {
    state.sc[static_cast<std::size_t>(ic - 1)] =
        1.0 -
        bots *
            (1.0 - std::cos(wwt * (state.grid.wc[static_cast<std::size_t>(nc - 1)] -
                                   state.grid.wc[static_cast<std::size_t>(ic - 1)]))) /
            (1.0 - std::cos(wwt * pi));
  }

  const double ceps = 1.0e-7;
  const double seps = 5.0e-7;
  for (int ipass = 1; ipass <= 30; ++ipass) {
    for (int ic = 1; ic <= nc; ++ic) {
      const int i0 = ic - 1;
      const double sic = s[0] +
                         (s[static_cast<std::size_t>(n - 1)] - s[0]) *
                             state.sc[static_cast<std::size_t>(i0)];
      const double dxds = math::deval(sic, x, xp, s);
      const double dyds = math::deval(sic, y, yp, s);
      const double qim =
          std::atan2(dxds, -dyds) -
          0.5 * (state.grid.wc[static_cast<std::size_t>(i0)] - pi) *
              (1.0 + state.agte) -
          state.qim0;
      state.coeff.piq[static_cast<std::size_t>(i0)] =
          std::complex<double>(0.0, qim);
    }

    const Status ftp = fourierTransform(state.grid, state.coeff);
    if (ftp.isErr()) {
      return ftp;
    }
    state.coeff.cn[0] =
        std::complex<double>(0.0, std::imag(state.coeff.cn[0]) + state.qim0);
    const Status ps = piqSum(state.grid, state.coeff);
    if (ps.isErr()) {
      return ps;
    }

    state.scold = state.sc;

    for (int itgap = 1; itgap <= 5; ++itgap) {
      const Status zc = zcCalc(state, 1);
      if (zc.isErr()) {
        return zc;
      }
      const std::complex<double> zle = zleFind(state);
      const std::complex<double> zte =
          0.5 * (state.zc[0] + state.zc[static_cast<std::size_t>(nc - 1)]);
      const double dzwt = std::abs(zte - zle) / std::abs(state.chordz);
      const std::complex<double> dcn =
          -(state.zc[0] - state.zc[static_cast<std::size_t>(nc - 1)] -
            dzwt * state.dzte) /
          (state.zc_cn[zcCnIndex(0, 1)] -
           state.zc_cn[zcCnIndex(nc - 1, 1)]);
      state.coeff.cn[1] += dcn;
      const Status ps2 = piqSum(state.grid, state.coeff);
      if (ps2.isErr()) {
        return ps2;
      }
      if (std::abs(dcn) < ceps) {
        break;
      }
    }

    double dscmax = 0.0;
    for (int ic = 1; ic <= nc; ++ic) {
      dscmax = std::max(
          dscmax,
          std::abs(state.sc[static_cast<std::size_t>(ic - 1)] -
                   state.scold[static_cast<std::size_t>(ic - 1)]));
    }
    if (diag != nullptr) {
      diag->iterations.push_back(ScinitIterationRecord{ipass, dscmax});
    }
    if (dscmax < seps) {
      if (diag != nullptr) {
        diag->converged = true;
      }
      break;
    }
  }

  const Status zn = zcNorm(state, 1);
  if (zn.isErr()) {
    return zn;
  }

  for (int ic = 1; ic <= nc; ++ic) {
    const int i0 = ic - 1;
    state.scold[static_cast<std::size_t>(i0)] =
        state.sc[static_cast<std::size_t>(i0)];
    state.xcold[static_cast<std::size_t>(i0)] =
        std::real(state.zc[static_cast<std::size_t>(i0)]);
    state.ycold[static_cast<std::size_t>(i0)] =
        std::imag(state.zc[static_cast<std::size_t>(i0)]);
  }

  for (int ic = 1; ic <= nc; ++ic) {
    const int i0 = ic - 1;
    const double sinw =
        2.0 * std::sin(0.5 * state.grid.wc[static_cast<std::size_t>(i0)]);
    double sinwe = 0.0;
    if (sinw > 0.0) {
      sinwe = std::pow(sinw, 1.0 - state.agte);
    }
    const double hwc =
        0.5 * (state.grid.wc[static_cast<std::size_t>(i0)] - pi) *
            (1.0 + state.agte) -
        0.5 * pi;
    state.zcoldw[static_cast<std::size_t>(i0)] =
        sinwe * std::exp(state.coeff.piq[static_cast<std::size_t>(i0)] +
                        std::complex<double>(0.0, hwc));
  }

  state.qimold = std::imag(state.coeff.cn[0]);
  return Status::ok();
}

Result<CircleSpeedResult> qCCalc(const CircleMappingState& state, int ispec,
                                 double alfa, double clTarget, double minf,
                                 double qinf) {
  const int nc = state.grid.nc;
  if (nc <= 0 || qinf == 0.0) {
    return Result<CircleSpeedResult>::fail(
        Error::make(ErrorCode::invalidArgument, "QCCALC: invalid argument"));
  }
  if (state.coeff.cn.empty() ||
      static_cast<int>(state.coeff.piq.size()) != nc ||
      static_cast<int>(state.zc.size()) != nc ||
      static_cast<int>(state.sc.size()) != nc) {
    return Result<CircleSpeedResult>::fail(
        Error::make(ErrorCode::sizeMismatch, "QCCALC: size conflict"));
  }

  const double beta = std::sqrt(1.0 - minf * minf);
  const double bfac = 0.5 * minf * minf / (1.0 + beta);

  CircleSpeedResult res;
  res.qcir.assign(static_cast<std::size_t>(nc), 0.0);
  res.xcir.assign(static_cast<std::size_t>(nc), 0.0);
  res.ycir.assign(static_cast<std::size_t>(nc), 0.0);
  res.scir.assign(static_cast<std::size_t>(nc), 0.0);
  std::vector<double> qcA(static_cast<std::size_t>(nc), 0.0);

  double alfaCur = alfa;
  double cl = clTarget;
  double cm = 0.0;
  double dalfa = 0.0;

  for (int ipass = 1; ipass <= 10; ++ipass) {
    const double alfCir = alfaCur - std::imag(state.coeff.cn[0]);
    std::complex<double> cmt(0.0, 0.0);
    std::complex<double> cft(0.0, 0.0);
    std::complex<double> cftA(0.0, 0.0);

    for (int ic = 1; ic <= nc; ++ic) {
      const int i0 = ic - 1;
      const double ppp = std::real(state.coeff.piq[static_cast<std::size_t>(i0)]);
      const double eppp = std::exp(-ppp);
      const double sinw =
          2.0 * std::sin(0.5 * state.grid.wc[static_cast<std::size_t>(i0)]);
      double sinwe = 0.0;
      if (state.agte == 0.0) {
        sinwe = 1.0;
      } else if (sinw > 0.0) {
        sinwe = std::pow(sinw, state.agte);
      }
      res.qcir[static_cast<std::size_t>(i0)] =
          2.0 * std::cos(0.5 * state.grid.wc[static_cast<std::size_t>(i0)] -
                         alfCir) *
          sinwe * eppp;
      qcA[static_cast<std::size_t>(i0)] =
          2.0 * std::sin(0.5 * state.grid.wc[static_cast<std::size_t>(i0)] -
                         alfCir) *
          sinwe * eppp;
      res.xcir[static_cast<std::size_t>(i0)] =
          std::real(state.zc[static_cast<std::size_t>(i0)]);
      res.ycir[static_cast<std::size_t>(i0)] =
          std::imag(state.zc[static_cast<std::size_t>(i0)]);
      res.scir[static_cast<std::size_t>(i0)] = state.sc[static_cast<std::size_t>(i0)];
    }

    double cpinc1 = 1.0 - (res.qcir[0] / qinf) * (res.qcir[0] / qinf);
    double cpiQ1 = -2.0 * res.qcir[0] / (qinf * qinf);
    double cpcom1 = cpinc1 / (beta + bfac * cpinc1);
    double cpcQ1 = (1.0 - bfac * cpcom1) / (beta + bfac * cpinc1) * cpiQ1;
    double cpcA1 = cpcQ1 * qcA[0];

    for (int ic = 1; ic <= nc; ++ic) {
      int icp = ic + 1;
      if (ic == nc) {
        icp = 1;
      }
      const int i0 = ic - 1;
      const int p0 = icp - 1;
      const double cpinc2 =
          1.0 - (res.qcir[static_cast<std::size_t>(p0)] / qinf) *
                    (res.qcir[static_cast<std::size_t>(p0)] / qinf);
      const double cpiQ2 =
          -2.0 * res.qcir[static_cast<std::size_t>(p0)] / (qinf * qinf);
      const double cpcom2 = cpinc2 / (beta + bfac * cpinc2);
      const double cpcQ2 =
          (1.0 - bfac * cpcom2) / (beta + bfac * cpinc2) * cpiQ2;
      const double cpcA2 = cpcQ2 * qcA[static_cast<std::size_t>(p0)];

      const std::complex<double> za =
          (state.zc[static_cast<std::size_t>(p0)] +
           state.zc[static_cast<std::size_t>(i0)]) *
              0.5 -
          std::complex<double>(0.25, 0.0);
      const std::complex<double> dz =
          state.zc[static_cast<std::size_t>(p0)] -
          state.zc[static_cast<std::size_t>(i0)];

      cmt = cmt - 0.5 * (cpcom1 + cpcom2) * dz * std::conj(za) +
            (cpcom1 - cpcom2) * dz * std::conj(dz) / 12.0;
      cft = cft + 0.5 * (cpcom1 + cpcom2) * dz;
      cftA = cftA + 0.5 * (cpcA1 + cpcA2) * dz;

      cpcom1 = cpcom2;
      cpcA1 = cpcA2;
    }

    const std::complex<double> eia =
        std::exp(std::complex<double>(0.0, -alfaCur));
    cft = cft * eia;
    cftA = cftA * eia + cft * std::complex<double>(0.0, -1.0);
    const double clt = std::real(cft);
    const double cltA = std::real(cftA);
    cm = std::real(cmt);

    if (ispec == 1) {
      cl = clt;
      res.alfa = alfaCur;
      res.cl = cl;
      res.cm = cm;
      return Result<CircleSpeedResult>::ok(std::move(res));
    }
    dalfa = (cl - clt) / cltA;
    alfaCur += dalfa;
    if (std::abs(dalfa) < 5.0e-7) {
      res.alfa = alfaCur;
      res.cl = cl;
      res.cm = cm;
      return Result<CircleSpeedResult>::ok(std::move(res));
    }
  }

  res.alfa = alfaCur;
  res.cl = cl;
  res.cm = cm;
  return Result<CircleSpeedResult>::ok(std::move(res));
}

Result<SpeedIntegrateResult> qspIntegrate(const CircleMappingState& state,
                                          double alqsp,
                                          std::span<const double> qspec,
                                          double qinf, double minf) {
  const int nc = state.grid.nc;
  if (nc <= 0 || qinf == 0.0) {
    return Result<SpeedIntegrateResult>::fail(
        Error::make(ErrorCode::invalidArgument, "QSPINT: invalid argument"));
  }
  if (static_cast<int>(qspec.size()) != nc ||
      static_cast<int>(state.xcold.size()) != nc ||
      static_cast<int>(state.ycold.size()) != nc) {
    return Result<SpeedIntegrateResult>::fail(
        Error::make(ErrorCode::sizeMismatch, "QSPINT: size conflict"));
  }

  const double sa = std::sin(alqsp);
  const double ca = std::cos(alqsp);
  const double beta = std::sqrt(1.0 - minf * minf);
  const double bfac = 0.5 * minf * minf / (1.0 + beta);

  SpeedIntegrateResult res;
  double cpq1 = 1.0 - (qspec[0] / qinf) * (qspec[0] / qinf);
  cpq1 = cpq1 / (beta + bfac * cpq1);

  for (int i = 1; i <= nc; ++i) {
    int ip = i + 1;
    if (i == nc) {
      ip = 1;
    }
    const int i0 = i - 1;
    const int p0 = ip - 1;
    double cqinc = 1.0 -
                   (qspec[static_cast<std::size_t>(p0)] / qinf) *
                       (qspec[static_cast<std::size_t>(p0)] / qinf);
    const double cpq2 = cqinc / (beta + bfac * cqinc);

    const double dx =
        (state.xcold[static_cast<std::size_t>(p0)] -
         state.xcold[static_cast<std::size_t>(i0)]) *
            ca +
        (state.ycold[static_cast<std::size_t>(p0)] -
         state.ycold[static_cast<std::size_t>(i0)]) *
            sa;
    const double dy =
        (state.ycold[static_cast<std::size_t>(p0)] -
         state.ycold[static_cast<std::size_t>(i0)]) *
            ca -
        (state.xcold[static_cast<std::size_t>(p0)] -
         state.xcold[static_cast<std::size_t>(i0)]) *
            sa;
    const double du = cpq2 - cpq1;
    const double ax =
        0.5 * (state.xcold[static_cast<std::size_t>(p0)] +
               state.xcold[static_cast<std::size_t>(i0)]) *
            ca +
        0.5 * (state.ycold[static_cast<std::size_t>(p0)] +
               state.ycold[static_cast<std::size_t>(i0)]) *
            sa;
    const double ay =
        0.5 * (state.ycold[static_cast<std::size_t>(p0)] +
               state.ycold[static_cast<std::size_t>(i0)]) *
            ca -
        0.5 * (state.xcold[static_cast<std::size_t>(p0)] +
               state.xcold[static_cast<std::size_t>(i0)]) *
            sa;
    const double aq = 0.5 * (cpq2 + cpq1);

    res.cl += dx * aq;
    res.cm += -dx * (aq * (ax - 0.25) + du * dx / 12.0) -
              dy * (aq * ay + du * dy / 12.0);
    cpq1 = cpq2;
  }
  return Result<SpeedIntegrateResult>::ok(res);
}

Status mapGenerate(CircleMappingState& state, double ffilt,
                   std::vector<double>& x, std::vector<double>& y,
                   MapgenDiagnostics* diag) {
  (void)ffilt;  // CNFILT is commented out in the canonical MAPGEN.
  const int nc = state.grid.nc;
  const int mct = state.grid.mct;
  if (nc <= 0 || mct <= 0 || mct > kZcCnCols) {
    return Status::fail(
        Error::make(ErrorCode::invalidArgument, "MAPGEN: invalid argument"));
  }
  if (state.coeff.cn.size() < 2 ||
      static_cast<int>(state.xcold.size()) != nc ||
      static_cast<int>(state.ycold.size()) != nc) {
    return Status::fail(
        Error::make(ErrorCode::sizeMismatch, "MAPGEN: size conflict"));
  }

  const double pi = state.grid.pi;
  const double dx = state.xcold[1] - state.xcold[0];
  const double dy = state.ycold[1] - state.ycold[0];
  const double qim0 = std::atan2(dx, -dy) + 0.5 * pi * (1.0 + state.agte);
  const double qimoff = qim0 - std::imag(state.coeff.cn[0]);
  state.coeff.cn[0] += std::complex<double>(0.0, qimoff);

  Status st = piqSum(state.grid, state.coeff);
  if (st.isErr()) {
    return st;
  }
  st = zcCalc(state, mct);
  if (st.isErr()) {
    return st;
  }
  bool preLefindFailed = false;
  st = zcNorm(state, mct, &preLefindFailed);
  if (st.isErr()) {
    return st;
  }
  if (diag != nullptr && preLefindFailed) {
    diag->preLoopLefindFailed = true;
  }
  state.coeff.cn[0] = std::complex<double>(0.0, std::imag(state.coeff.cn[0]));

  const int ncn = 1;
  std::vector<std::complex<double>> qq(static_cast<std::size_t>(ncn * ncn));
  std::vector<std::complex<double>> dcn(static_cast<std::size_t>(ncn));
  for (int iter = 1; iter <= 10; ++iter) {
    for (int m = 1; m <= ncn; ++m) {
      for (int l = 1; l <= ncn; ++l) {
        qq[static_cast<std::size_t>((m - 1) * ncn + (l - 1))] =
            std::complex<double>(0.0, 0.0);
      }
      dcn[static_cast<std::size_t>(m - 1)] = std::complex<double>(0.0, 0.0);
      qq[static_cast<std::size_t>((m - 1) * ncn + (m - 1))] =
          std::complex<double>(1.0, 0.0);
    }
    const int mm = 1;
    dcn[static_cast<std::size_t>(mm - 1)] =
        state.zc[0] - state.zc[static_cast<std::size_t>(nc - 1)] - state.dzte;
    qq[static_cast<std::size_t>((mm - 1) * ncn + (mm - 1))] =
        state.zc_cn[zcCnIndex(0, mm)] -
        state.zc_cn[zcCnIndex(nc - 1, mm)];

    math::cgauss(qq, dcn, static_cast<std::size_t>(ncn), 1);

    double dcnmax = 0.0;
    for (int m = 1; m <= ncn; ++m) {
      state.coeff.cn[static_cast<std::size_t>(m)] -=
          dcn[static_cast<std::size_t>(m - 1)];
      dcnmax = std::max(dcnmax, std::abs(dcn[static_cast<std::size_t>(m - 1)]));
    }
    st = piqSum(state.grid, state.coeff);
    if (st.isErr()) {
      return st;
    }
    st = zcCalc(state, mct);
    if (st.isErr()) {
      return st;
    }
    bool iterLefindFailed = false;
    st = zcNorm(state, mct, &iterLefindFailed);
    if (st.isErr()) {
      return st;
    }
    if (diag != nullptr) {
      MapgenIterationRecord rec{iter, dcnmax, false};
      rec.lefindFailed = iterLefindFailed;
      diag->iterations.push_back(rec);
    }
    if (dcnmax <= 5.0e-5) {
      if (diag != nullptr) {
        diag->converged = true;
      }
      break;
    }
  }

  x.assign(static_cast<std::size_t>(nc), 0.0);
  y.assign(static_cast<std::size_t>(nc), 0.0);
  for (int i = 1; i <= nc; ++i) {
    x[static_cast<std::size_t>(i - 1)] =
        std::real(state.zc[static_cast<std::size_t>(i - 1)]);
    y[static_cast<std::size_t>(i - 1)] =
        std::imag(state.zc[static_cast<std::size_t>(i - 1)]);
  }
  return Status::ok();
}

Status perturbCoefficient(CircleMappingState& state, int m, double dcnr,
                          double dcni) {
  const int mc = state.grid.mc;
  if (m < 1 || m > mc) {
    return Status::fail(Error::make(ErrorCode::outOfRange,
                                    "PERT: Mode M outside 1..MC"));
  }
  state.coeff.cn[static_cast<std::size_t>(m)] +=
      std::complex<double>(dcnr, dcni);
  return Status::ok();
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/CircleGeometry.cpp

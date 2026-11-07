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

// File: src/core/inverse_design/MixedInverseKernels.cpp
// MIXED/GAMLIN of the new 0-based core, ported from
// fortran-src/xqdes.f. Calls the verified kernels PSILIN (GEOLIN), NCALC,
// SCALC, TECALC and GAUSS.

#include "xfoil/core/inverse/MixedInverseKernels.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/math/LinearAlgebra.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"

namespace xfoil::core::inverse {

namespace {

// Fortran: FUNCTION ATANC(Y,X,THOLD) (xutils.f).
double atanc(double y, double x, double thold) {
  constexpr double pi = 3.1415926535897932384;
  constexpr double tpi = 6.2831853071795864769;
  const double thnew = std::atan2(y, x);
  const double dthet = thnew - thold;
  const double dtcorr = dthet - tpi * std::trunc((dthet + std::copysign(pi, dthet)) / tpi);
  return thold + dtcorr;
}

}  // namespace

void gamLin(std::span<double> q, std::size_t row, std::size_t col, double coef,
            int n, int iq1, int iq2, std::span<const double> qf0,
            std::span<const double> qf1, std::span<const double> qf2,
            std::span<const double> qf3) {
  const std::size_t sys = static_cast<std::size_t>(n) + 5;
  // iq1/iq2 are 0-based including segment boundaries; direct comparison
  // against the 0-based col (no reconversion to the Fortran J).
  if (col >= static_cast<std::size_t>(iq1) && col <= static_cast<std::size_t>(iq2)) {
    q[row * sys + static_cast<std::size_t>(n) + 1] += coef * qf0[col];
    q[row * sys + static_cast<std::size_t>(n) + 2] += coef * qf1[col];
    q[row * sys + static_cast<std::size_t>(n) + 3] += coef * qf2[col];
    q[row * sys + static_cast<std::size_t>(n) + 4] += coef * qf3[col];
  } else {
    q[row * sys + col] += coef;
  }
}

Status mixedInverse(MixedInverseState& st, int niterq) {
  const int n = st.n;
  // Complete precondition check before each mutation.
  if (n < 1 || static_cast<int>(st.x.size()) != n ||
      static_cast<int>(st.y.size()) != n ||
      static_cast<int>(st.s.size()) != n ||
      static_cast<int>(st.xp.size()) != n ||
      static_cast<int>(st.yp.size()) != n ||
      static_cast<int>(st.nx.size()) != n ||
      static_cast<int>(st.ny.size()) != n ||
      static_cast<int>(st.apanel.size()) != n ||
      static_cast<int>(st.gam.size()) != n ||
      static_cast<int>(st.gamA.size()) != n ||
      static_cast<int>(st.gamu1.size()) != n ||
      static_cast<int>(st.gamu2.size()) != n ||
      static_cast<int>(st.sig.size()) != n ||
      static_cast<int>(st.qspec.size()) != n ||
      st.iq1 < 0 || st.iq2 >= n || st.iq1 > st.iq2) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "MixedInverse: invalid state"));
  }
  if (n + 5 > kMixedIqx) {
    return Status::fail(Error::make(ErrorCode::capacityExceeded,
                                    "MixedInverse: N+5 > IQX"));
  }
  if (niterq < 0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "MixedInverse: niterq < 0"));
  }
  // Finite scalar state values.
  const double scalars[] = {st.alfa, st.minf, st.qinf, st.psio, st.qdof0,
                            st.qdof1, st.qdof2, st.qdof3, st.chord, st.xte,
                            st.yte, st.dste, st.ante, st.aste};
  for (double v : scalars) {
    if (!std::isfinite(v)) {
      return Status::fail(Error::make(ErrorCode::invalidArgument,
                                      "MixedInverse: non-finite scalar"));
    }
  }
  const std::size_t a = static_cast<std::size_t>(st.iq1);
  const std::size_t b = static_cast<std::size_t>(st.iq2);
  // Segment arc length ds must finite and!= 0 be (Division in QF0..3).
  const double ds = st.s[b] - st.s[a];
  if (!std::isfinite(ds) || ds == 0.0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "MixedInverse: ds == 0 or not finite"));
  }
  st.converged = false;
  const std::size_t sys = static_cast<std::size_t>(n) + 5;

  st.qf0.assign(static_cast<std::size_t>(n), 0.0);
  st.qf1.assign(static_cast<std::size_t>(n), 0.0);
  st.qf2.assign(static_cast<std::size_t>(n), 0.0);
  st.qf3.assign(static_cast<std::size_t>(n), 0.0);
  for (std::size_t i = a; i <= b; ++i) {
    const double fs = (st.s[i] - st.s[a]) / ds;
    st.qf0[i] = 1.0 - fs;
    st.qf1[i] = fs;
    if (st.lcpXX) {
      st.qf2[i] = std::exp(-5.0 * fs);
      st.qf3[i] = std::exp(-5.0 * (1.0 - fs));
    }
    st.gam[i] = st.qspec[i] + st.qdof0 * st.qf0[i] + st.qdof1 * st.qf1[i] +
                st.qdof2 * st.qf2[i] + st.qdof3 * st.qf3[i];
  }

  std::vector<double> q(sys * sys, 0.0);
  std::vector<double> dq(sys, 0.0);

  for (int iter = 0; iter < niterq; ++iter) {
    std::fill(q.begin(), q.end(), 0.0);
    std::fill(dq.begin(), dq.end(), 0.0);

    paneling::ncalc(st.x, st.y, st.s, st.nx, st.ny);

    inviscid::PsiLinContext ctx;
    ctx.x = st.x; ctx.y = st.y; ctx.s = st.s; ctx.nx = st.nx; ctx.ny = st.ny;
    ctx.apanel = st.apanel; ctx.gam = st.gam; ctx.gamA = st.gamA;
    ctx.gamu1 = st.gamu1; ctx.gamu2 = st.gamu2; ctx.sig = st.sig;
    ctx.sharp = st.sharp; ctx.dste = st.dste; ctx.ante = st.ante;
    ctx.aste = st.aste; ctx.qinf = st.qinf; ctx.image = false; ctx.yimage = 0.0;

    for (int i = 1; i <= n; ++i) {
      const std::size_t i0 = static_cast<std::size_t>(i) - 1;
      inviscid::PsiLinInputs in;
      in.node = i0; in.xi = st.x[i0]; in.yi = st.y[i0]; in.nxi = st.nx[i0];
      in.nyi = st.ny[i0]; in.alfa = st.alfa; in.nodeOnAirfoil = true;
      in.siglin = false; in.geolin = true;
      in.qf0 = st.qf0; in.qf1 = st.qf1; in.qf2 = st.qf2; in.qf3 = st.qf3;
      const inviscid::PsiLinResult pl = inviscid::psiLin(in, ctx);
      std::vector<double> dzdn = pl.dzdn;
      std::vector<double> dzdg = pl.dzdg;
      dzdn[i0] += pl.psiNi;
      for (int j = 1; j <= n; ++j) {
        const std::size_t j0 = static_cast<std::size_t>(j) - 1;
        q[i0 * sys + j0] += (j0 >= a && j0 <= b) ? dzdn[j0] : dzdg[j0];
      }
      dq[i0] = st.psio - pl.psi;
      q[i0 * sys + static_cast<std::size_t>(n)] -= 1.0;
      q[i0 * sys + static_cast<std::size_t>(n) + 1] += pl.zQdof0;
      q[i0 * sys + static_cast<std::size_t>(n) + 2] += pl.zQdof1;
      q[i0 * sys + static_cast<std::size_t>(n) + 3] += pl.zQdof2;
      q[i0 * sys + static_cast<std::size_t>(n) + 4] += pl.zQdof3;
    }

    // Kutta condition (line N+1 -> 0-based n).
    dq[static_cast<std::size_t>(n)] = -(st.gam[0] + st.gam[static_cast<std::size_t>(n) - 1]);
    gamLin(q, static_cast<std::size_t>(n), 0, 1.0, n, st.iq1, st.iq2, st.qf0,
           st.qf1, st.qf2, st.qf3);
    gamLin(q, static_cast<std::size_t>(n), static_cast<std::size_t>(n) - 1, 1.0,
           n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2, st.qf3);

    if (st.sharp) {
      const double ag1 = std::atan2(-st.yp[0], -st.xp[0]);
      const double ag2 = atanc(st.yp[static_cast<std::size_t>(n) - 1],
                               st.xp[static_cast<std::size_t>(n) - 1], ag1);
      const double abis = 0.5 * (ag1 + ag2);
      const double cbis = std::cos(abis);
      const double sbis = std::sin(abis);
      const double ds1 = std::sqrt((st.x[0] - st.x[1]) * (st.x[0] - st.x[1]) +
                                   (st.y[0] - st.y[1]) * (st.y[0] - st.y[1]));
      const double ds2 = std::sqrt(
          (st.x[static_cast<std::size_t>(n) - 1] -
           st.x[static_cast<std::size_t>(n) - 2]) *
              (st.x[static_cast<std::size_t>(n) - 1] -
               st.x[static_cast<std::size_t>(n) - 2]) +
          (st.y[static_cast<std::size_t>(n) - 1] -
           st.y[static_cast<std::size_t>(n) - 2]) *
              (st.y[static_cast<std::size_t>(n) - 1] -
               st.y[static_cast<std::size_t>(n) - 2]));
      const double dsmin = std::min(ds1, ds2);
      const double xbis = st.xte - 0.1 * dsmin * cbis;
      const double ybis = st.yte - 0.1 * dsmin * sbis;
      inviscid::PsiLinInputs in;
      in.node = 0; in.xi = xbis; in.yi = ybis; in.nxi = -sbis; in.nyi = cbis;
      in.alfa = st.alfa; in.nodeOnAirfoil = false; in.siglin = true;
      in.geolin = false;
      const inviscid::PsiLinResult pl = inviscid::psiLin(in, ctx);
      const double res = pl.psiNi;
      const std::size_t rowN = static_cast<std::size_t>(n) - 1;
      for (std::size_t j = 0; j < sys; ++j) {
        q[rowN * sys + j] = 0.0;
      }
      for (int j = 1; j <= n; ++j) {
        const std::size_t j0 = static_cast<std::size_t>(j) - 1;
        gamLin(q, rowN, j0, pl.dqdg[j0], n, st.iq1, st.iq2, st.qf0, st.qf1,
               st.qf2, st.qf3);
        q[rowN * sys + j0] = pl.dqdg[j0];
      }
      q[rowN * sys + static_cast<std::size_t>(n)] = 0.0;
      dq[rowN] = -res;
    }

    // pinned IQ1/IQ2 points.
    q[(static_cast<std::size_t>(n) + 1) * sys + a] = 1.0;
    dq[static_cast<std::size_t>(n) + 1] = 0.0;
    q[(static_cast<std::size_t>(n) + 2) * sys + b] = 1.0;
    dq[static_cast<std::size_t>(n) + 2] = 0.0;

    if (st.iq1 > 0 && st.lcpXX) {
      const std::size_t row = static_cast<std::size_t>(n) + 3;
      const double res = (st.gam[a - 1] - 2.0 * st.gam[a] + st.gam[a + 1]) -
                         (st.qspec[a - 1] - 2.0 * st.qspec[a] + st.qspec[a + 1]);
      gamLin(q, row, a - 1, 1.0, n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2,
             st.qf3);
      gamLin(q, row, a, -2.0, n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2,
             st.qf3);
      gamLin(q, row, a + 1, 1.0, n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2,
             st.qf3);
      dq[row] = -res;
    } else {
      q[(static_cast<std::size_t>(n) + 3) * sys + (static_cast<std::size_t>(n) + 3)] = 1.0;
      dq[static_cast<std::size_t>(n) + 3] = -st.qdof2;
    }

    if (st.iq2 < n - 1 && st.lcpXX) {
      const std::size_t row = static_cast<std::size_t>(n) + 4;
      const double res =
          (st.gam[b - 1] - 2.0 * st.gam[b] + st.gam[b + 1]) -
          (st.qspec[b - 1] - 2.0 * st.qspec[b] + st.qspec[b + 1]);
      gamLin(q, row, b - 1, 1.0, n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2,
             st.qf3);
      gamLin(q, row, b, -2.0, n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2,
             st.qf3);
      gamLin(q, row, b + 1, 1.0, n, st.iq1, st.iq2, st.qf0, st.qf1, st.qf2,
             st.qf3);
      dq[row] = -res;
    } else {
      q[(static_cast<std::size_t>(n) + 4) * sys + (static_cast<std::size_t>(n) + 4)] = 1.0;
      dq[static_cast<std::size_t>(n) + 4] = -st.qdof3;
    }

    math::gauss(q, dq, sys, 1);

    // Fortran GAUSS checks no null pivot (produces undefined values
    // for a singular matrix). The core catches this in a controlled way
    // (cpp-safety deviation) instead of replicating undefined behavior.
    for (std::size_t i = 0; i < sys; ++i) {
      if (!std::isfinite(dq[i])) {
        return Status::fail(Error::make(ErrorCode::numericalFailure,
                                        "MixedInverse: singular matrix"));
      }
    }

    double dnmax = 0.0;
    double dgmax = 0.0;
    for (std::size_t i = 0; i < a; ++i) {
      st.gam[i] += dq[i];
      if (std::abs(dq[i]) > std::abs(dgmax)) dgmax = dq[i];
    }
    for (std::size_t i = a; i <= b; ++i) {
      st.x[i] += st.nx[i] * dq[i];
      st.y[i] += st.ny[i] * dq[i];
      if (std::abs(dq[i]) > std::abs(dnmax)) dnmax = dq[i];
    }
    for (std::size_t i = b + 1; i < static_cast<std::size_t>(n); ++i) {
      st.gam[i] += dq[i];
      if (std::abs(dq[i]) > std::abs(dgmax)) dgmax = dq[i];
    }
    st.psio += dq[static_cast<std::size_t>(n)];
    st.qdof0 += dq[static_cast<std::size_t>(n) + 1];
    st.qdof1 += dq[static_cast<std::size_t>(n) + 2];
    st.qdof2 += dq[static_cast<std::size_t>(n) + 3];
    st.qdof3 += dq[static_cast<std::size_t>(n) + 4];

    math::scalc(st.x, st.y, st.s);
    for (std::size_t i = a; i <= b; ++i) {
      st.gam[i] = st.qspec[i] + st.qdof0 * st.qf0[i] + st.qdof1 * st.qf1[i] +
                  st.qdof2 * st.qf2[i] + st.qdof3 * st.qf3[i];
    }
    const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
        st.x[0], st.y[0], st.xp[0], st.yp[0],
        st.x[static_cast<std::size_t>(n) - 1],
        st.y[static_cast<std::size_t>(n) - 1],
        st.xp[static_cast<std::size_t>(n) - 1],
        st.yp[static_cast<std::size_t>(n) - 1], st.chord);
    st.sharp = teg.sharp;
    st.dste = teg.dste;
    st.ante = teg.ante;
    st.aste = teg.aste;

    if (std::abs(dnmax) < 5.0e-5 && std::abs(dgmax) < 5.0e-4) {
      st.converged = true;
      return Status::ok();
    }
  }
  return Status::ok();
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/MixedInverseKernels.cpp

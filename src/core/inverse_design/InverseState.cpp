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

// File: src/core/inverse_design/InverseState.cpp
// MAPGAM/QSPCIR of the new 0-based cores, ported from
// fortran-src/xmdes.f. Calls the confirmed kernels QCCALC, SEVAL, SPLQSP.

#include "xfoil/core/inverse/InverseState.h"

#include <cmath>
#include <cstddef>

#include "xfoil/core/Result.h"
#include "xfoil/core/inverse/CircleGeometry.h"
#include "xfoil/core/inverse/QSpecKernels.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::inverse {

Result<MappedGammaResult> mapGam(FullInverseState& state, int iac, double alg,
                                 double clg, double cmg) {
  (void)cmg;  // Fortran CMG is pure output of QCCALC; input unused.
  const Result<CircleSpeedResult> res =
      qCCalc(state.circle, iac, alg, clg, state.minf, state.qinf);
  if (res.isErr()) {
    return Result<MappedGammaResult>::fail(res.error());
  }
  const CircleSpeedResult& sp = res.value();

  const PanelGeometry& pg = state.panel;
  if (pg.n <= 0 || static_cast<int>(pg.s.size()) < pg.n) {
    return Result<MappedGammaResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "InverseState: invalid panel geometry"));
  }

  state.sspec = sp.scir;
  state.qgamm = sp.qcir;
  state.nsp = static_cast<int>(sp.scir.size());

  const double chx = pg.xte - pg.xle;
  const double chy = pg.yte - pg.yle;
  const double chsq = chx * chx + chy * chy;
  const double sn = pg.s[static_cast<std::size_t>(pg.n - 1)];

  state.xspoc.assign(static_cast<std::size_t>(state.nsp), 0.0);
  state.yspoc.assign(static_cast<std::size_t>(state.nsp), 0.0);
  for (int i = 0; i < state.nsp; ++i) {
    const double arg = sn * state.sspec[static_cast<std::size_t>(i)];
    const double xic = math::seval(arg, pg.x, pg.xp, pg.s);
    const double yic = math::seval(arg, pg.y, pg.yp, pg.s);
    state.xspoc[static_cast<std::size_t>(i)] =
        ((xic - pg.xle) * chx + (yic - pg.yle) * chy) / chsq;
    state.yspoc[static_cast<std::size_t>(i)] =
        ((yic - pg.yle) * chx - (xic - pg.xle) * chy) / chsq;
  }
  state.ssple = pg.sle / sn;

  MappedGammaResult out;
  out.alfa = sp.alfa;
  out.cl = sp.cl;
  out.cm = sp.cm;
  return Result<MappedGammaResult>::ok(std::move(out));
}

Status qSpecCirc(const FullInverseState& state, QSpecDistribution& d,
                 int iacqsp) {
  const Result<CircleSpeedResult> res =
      qCCalc(state.circle, iacqsp, d.al, d.cl, state.minf, state.qinf);
  if (res.isErr()) {
    return Status::fail(res.error());
  }
  const CircleSpeedResult& sp = res.value();
  d.q = sp.qcir;
  d.s = state.sspec;
  d.qp.assign(d.q.size(), 0.0);
  // Fortran QSPCIR returns ALQSP/CLQSP/CMQSP on QCCALC (In/Out); the
  // updated values are written back.
  d.al = sp.alfa;
  d.cl = sp.cl;
  d.cm = sp.cm;
  return splQSpec(d);
}

Status captureCurrentSpeed(FullInverseState& state,
                           std::span<const double> gam) {
  const int n = state.panel.n;
  if (n < 2 || static_cast<int>(gam.size()) != n) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "QDES capture: Sizes fit not"));
  }
  const double chx = state.panel.xte - state.panel.xle;
  const double chy = state.panel.yte - state.panel.yle;
  const double chsq = chx * chx + chy * chy;
  if (chsq <= 0.0) {
    return Status::fail(Error::make(ErrorCode::invalidState,
                                    "QDES capture: degenerate chord"));
  }
  const double sn = state.panel.s[static_cast<std::size_t>(n - 1)];
  if (sn <= 0.0) {
    return Status::fail(Error::make(ErrorCode::invalidState,
                                    "QDES capture: Arc length <= 0"));
  }
  state.nsp = n;
  state.qgamm.assign(static_cast<std::size_t>(n), 0.0);
  state.sspec.assign(static_cast<std::size_t>(n), 0.0);
  state.xspoc.assign(static_cast<std::size_t>(n), 0.0);
  state.yspoc.assign(static_cast<std::size_t>(n), 0.0);
  for (int i = 1; i <= n; ++i) {
    const std::size_t k = static_cast<std::size_t>(i - 1);
    state.qgamm[k] = gam[k];
    state.sspec[k] = state.panel.s[k] / sn;
    state.xspoc[k] = ((state.panel.x[k] - state.panel.xle) * chx +
                      (state.panel.y[k] - state.panel.yle) * chy) / chsq;
    state.yspoc[k] = ((state.panel.y[k] - state.panel.yle) * chx -
                      (state.panel.x[k] - state.panel.xle) * chy) / chsq;
  }
  state.ssple = state.panel.sle / sn;
  return Status::ok();
}

Status mdesInitializeFullInverse(FullInverseState& state,
                                 QSpecDistribution& qspec, double alfa,
                                 double& clgam, double& cmgam) {
  Status s = initMappingCoordinate(state.circle, state.panel.x, state.panel.xp,
                                   state.panel.y, state.panel.yp, state.panel.s,
                                   state.panel.sle);
  if (s.isErr()) return s;
  const Result<MappedGammaResult> mg = mapGam(state, 1, alfa, clgam, cmgam);
  if (mg.isErr()) return Status::fail(mg.error());
  // Fortran MDES: MAPGAM(1,ALGAM,CLGAM,CMGAM) returns CLGAM/CMGAM back.
  clgam = mg.value().cl;
  cmgam = mg.value().cm;
  s = calcCoefficients(state.circle, state.qgamm, false);
  if (s.isErr()) return s;
  return qSpecCirc(state, qspec, 1);
}

Status qdesInitializeMixedInverse(FullInverseState& state,
                                  QSpecDistribution& qspec,
                                  std::span<const double> gam, double alfa,
                                  double cl, double cm, bool liqset, int& iq1,
                                  int& iq2) {
  Status s = captureCurrentSpeed(state, gam);
  if (s.isErr()) return s;
  qspec.s = state.sspec;
  qspec.q.assign(state.sspec.size(), 0.0);
  qspec.qp.assign(state.sspec.size(), 0.0);
  return gamQSpec(qspec, state.qgamm, alfa, cl, cm, liqset, iq1, iq2);
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/InverseState.cpp

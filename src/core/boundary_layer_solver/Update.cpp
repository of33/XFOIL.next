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

// File: src/core/boundary_layer_solver/Update.cpp
// Implementation of UPDATE. Formula-faithful
// adoption from fortran-src/xbl.f:1253-1560; constants, branch
// conditions and the order of the computations are unchanged.

#include "xfoil/core/boundary_layer/Update.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "xfoil/core/boundary_layer/Coupling.h"

namespace xfoil::core::boundary_layer {

namespace {

std::span<double> sideSpan(BoundaryLayerStationField& f, SurfaceSide is) {
  return is == SurfaceSide::kTop ? std::span<double>(f.top)
                                 : std::span<double>(f.bottom);
}

std::span<const double> sideSpanC(const BoundaryLayerStationField& f,
                                  SurfaceSide is) {
  return is == SurfaceSide::kTop ? std::span<const double>(f.top)
                                 : std::span<const double>(f.bottom);
}

}  // namespace

UpdateResult update(UpdateInput& in) {
  UpdateResult out{};

  // Fortran: max permitted alpha changes per iteration
  const double dalmax = 0.5 * in.dtor;
  const double dalmin = -0.5 * in.dtor;
  // Fortran: max permitted Cl changes per iteration
  double dclmax = 0.5;
  double dclmin = -0.5;
  if (!in.matyp1) {
    dclmin = std::max(-0.5, -0.9 * in.cl);
  }

  // Fortran: HSTINV = GAMM1*(MINF/QINF)**2 / (1 + 0.5*GAMM1*MINF**2)
  const double hstinv =
      in.gamm1 * (in.minf / in.qinf) * (in.minf / in.qinf) /
      (1.0 + 0.5 * in.gamm1 * in.minf * in.minf);

  const std::array<const BoundaryLayerSideMapping*, 2> sides{&in.mapping.top,
                                                             &in.mapping.bottom};
  const std::array<int, 2> itran{in.itranTop, in.itranBottom};

  // Fortran: new Ue distribution without Under-relaxation and the
  // Sensitivity wrt alpha/Re (xbl.f:1284-1311)
  std::array<std::vector<double>, 2> unew;
  std::array<std::vector<double>, 2> uac;
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    static_cast<void>(side);
    const auto uinvS = sideSpanC(in.uinv, side);
    const auto uinvAS = sideSpanC(in.uinvA, side);
    unew[static_cast<std::size_t>(is)].assign(static_cast<std::size_t>(m.nbl) + 1,
                                              0.0);
    uac[static_cast<std::size_t>(is)].assign(static_cast<std::size_t>(m.nbl) + 1,
                                             0.0);
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      double dui = 0.0;
      double duiAc = 0.0;
      for (int js = 0; js < 2; ++js) {
        const BoundaryLayerSideMapping& ms =
            *sides[static_cast<std::size_t>(js)];
        const auto massJs = sideSpanC(in.mass, js == 0 ? SurfaceSide::kTop
                                                       : SurfaceSide::kBottom);
        for (int jbl = 1; jbl <= ms.nbl; ++jbl) {
          const std::size_t kk = static_cast<std::size_t>(jbl);
          const std::size_t j = static_cast<std::size_t>(ms.ipan[kk]);
          const std::size_t idx =
              i * static_cast<std::size_t>(in.dijStride) + j;
          if (idx >= in.dij.size()) {
            throw std::invalid_argument("update: DIJ index outside");
          }
          const double ueM = -m.vti[k] * ms.vti[kk] * in.dij[idx];
          const std::size_t line =
              static_cast<std::size_t>(in.isys[static_cast<std::size_t>(js)][kk]);
          if (line * 3 + 2 >= in.vdelRes.size() ||
              line * 3 + 2 >= in.vdelRe.size()) {
            throw std::invalid_argument("update: VDEL index outside");
          }
          dui += ueM * (massJs[kk] + in.vdelRes[line * 3 + 2]);
          duiAc += ueM * (-in.vdelRe[line * 3 + 2]);
        }
      }
      // Fortran: UINV depends on "AC" only if "AC" alpha is
      const double uinvAc = in.lalfa ? 0.0 : uinvAS[k];
      unew[static_cast<std::size_t>(is)][k] = uinvS[k] + dui;
      uac[static_cast<std::size_t>(is)][k] = uinvAc + duiAc;
    }
  }

  // Fortran: QNEW/Q_AC from UNEW/U_AC with Sign (xbl.f:1314-1320)
  const std::size_t nPan = in.x.size();
  std::vector<double> qnew(nPan, 0.0);
  std::vector<double> qac(nPan, 0.0);
  for (int is = 0; is < 2; ++is) {
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    // Fortran DO 20 IBL=2, IBLTE(IS): the view must extend up to index
    // IBLTE (IBLTE may equal NBL) — quantity NBL+1.
    const auto unewS = std::span<const double>(
        unew[static_cast<std::size_t>(is)].data(),
        static_cast<std::size_t>(m.nbl) + 1);
    const auto uacS = std::span<const double>(
        uac[static_cast<std::size_t>(is)].data(),
        static_cast<std::size_t>(m.nbl) + 1);
    for (int ibl = 1; ibl <= m.iblte; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t i = static_cast<std::size_t>(m.ipan[k]);
      if (i >= nPan) {
        throw std::invalid_argument("update: panel index outside QNEW");
      }
      qnew[i] = m.vti[k] * unewS[k];
      qac[i] = m.vti[k] * uacS[k];
    }
  }

  // Fortran: new Cl from the new Qtan (xbl.f:1322-1374)
  const double sa = std::sin(in.alfa);
  const double ca = std::cos(in.alfa);
  const double beta = std::sqrt(1.0 - in.minf * in.minf);
  const double betaMsq = -0.5 / beta;
  const double bfac = 0.5 * in.minf * in.minf / (1.0 + beta);
  const double bfacMsq =
      0.5 / (1.0 + beta) - bfac / (1.0 + beta) * betaMsq;
  if (nPan < 1) {
    throw std::invalid_argument("update: panel geometry empty");
  }
  double clnew = 0.0;
  double clA = 0.0;
  double clMs = 0.0;
  double clAc = 0.0;
  {
    const std::size_t i0 = 0;
    const double cpg1Q = 1.0 - (qnew[i0] / in.qinf) * (qnew[i0] / in.qinf);
    double cpg1 = cpg1Q / (beta + bfac * cpg1Q);
    double cpg1Ms = -cpg1 / (beta + bfac * cpg1Q) * (betaMsq + bfacMsq * cpg1Q);
    const double cpiQ = -2.0 * qnew[i0] / (in.qinf * in.qinf);
    const double cpcCpi = (1.0 - bfac * cpg1) / (beta + bfac * cpg1Q);
    double cpg1Ac = cpcCpi * cpiQ * qac[i0];
    for (std::size_t i = 0; i < nPan; ++i) {
      const std::size_t ip = (i + 1 < nPan) ? i + 1 : 0;
      const double cpg2Q = 1.0 - (qnew[ip] / in.qinf) * (qnew[ip] / in.qinf);
      const double cpg2 = cpg2Q / (beta + bfac * cpg2Q);
      const double cpg2Ms =
          -cpg2 / (beta + bfac * cpg2Q) * (betaMsq + bfacMsq * cpg2Q);
      const double cpiQ2 = -2.0 * qnew[ip] / (in.qinf * in.qinf);
      const double cpcCpi2 = (1.0 - bfac * cpg2) / (beta + bfac * cpg2Q);
      const double cpg2Ac = cpcCpi2 * cpiQ2 * qac[ip];
      const double dx = (in.x[ip] - in.x[i]) * ca + (in.y[ip] - in.y[i]) * sa;
      const double dxA =
          -(in.x[ip] - in.x[i]) * sa + (in.y[ip] - in.y[i]) * ca;
      const double ag = 0.5 * (cpg2 + cpg1);
      const double agMs = 0.5 * (cpg2Ms + cpg1Ms);
      const double agAc = 0.5 * (cpg2Ac + cpg1Ac);
      clnew += dx * ag;
      clA += dxA * ag;
      clMs += dx * agMs;
      clAc += dx * agAc;
      cpg1 = cpg2;
      cpg1Ms = cpg2Ms;
      cpg1Ac = cpg2Ac;
    }
  }

  // Fortran: Under-relaxation factor (xbl.f:1377-1399)
  double rlx = 1.0;
  double dac = 0.0;
  if (in.lalfa) {
    // alpha given: AC is Cl
    dac = (clnew - in.cl) /
          (1.0 - clAc - clMs * 2.0 * in.minf * in.minfCl);
    if (rlx * dac > dclmax) rlx = dclmax / dac;
    if (rlx * dac < dclmin) rlx = dclmin / dac;
  } else {
    // Cl given: AC is alpha
    dac = (clnew - in.clspec) / (0.0 - clAc - clA);
    if (rlx * dac > dalmax) rlx = dalmax / dac;
    if (rlx * dac < dalmin) rlx = dalmin / dac;
  }

  // Fortran: RMSBL/RMXBL loop (xbl.f:1401-1477)
  double rmsbl = 0.0;
  double rmxbl = 0.0;
  const double dhi = 1.5;
  const double dlo = -0.5;
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    const auto thetS = sideSpanC(in.thet, side);
    const auto dstrS = sideSpanC(in.dstr, side);
    const auto uedgS = sideSpanC(in.uedg, side);
    const auto ctauS = sideSpanC(in.ctau, side);
    const auto unewS = std::span<const double>(
        unew[static_cast<std::size_t>(is)].data(),
        static_cast<std::size_t>(m.nbl) + 1);
    const auto uacS = std::span<const double>(
        uac[static_cast<std::size_t>(is)].data(),
        static_cast<std::size_t>(m.nbl) + 1);
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t line =
          static_cast<std::size_t>(in.isys[static_cast<std::size_t>(is)][k]);
      if (line * 3 + 2 >= in.vdelRes.size() ||
          line * 3 + 2 >= in.vdelRe.size()) {
        throw std::invalid_argument("update: VDEL index outside");
      }
      // Fortran: Changes without Under-relaxation
      const double dctau = in.vdelRes[line * 3 + 0] - dac * in.vdelRe[line * 3 + 0];
      const double dthet = in.vdelRes[line * 3 + 1] - dac * in.vdelRe[line * 3 + 1];
      const double dmass = in.vdelRes[line * 3 + 2] - dac * in.vdelRe[line * 3 + 2];
      const double duedg = unewS[k] + dac * uacS[k] - uedgS[k];
      const double ddstr = (dmass - dstrS[k] * duedg) / uedgS[k];
      // Fortran: normalized changes
      const double dn1 =
          ibl < itran[static_cast<std::size_t>(is)] ? dctau / 10.0
                                                    : dctau / ctauS[k];
      const double dn2 = dthet / thetS[k];
      const double dn3 = ddstr / dstrS[k];
      const double dn4 = std::abs(duedg) / 0.25;
      rmsbl = rmsbl + dn1 * dn1 + dn2 * dn2 + dn3 * dn3 + dn4 * dn4;
      const double rdn1 = rlx * dn1;
      if (std::abs(dn1) > std::abs(rmxbl)) {
        rmxbl = dn1;
        out.vmxbl = ibl < itran[static_cast<std::size_t>(is)] ? 'n' : 'C';
        out.imxbl = ibl;
        out.ismxbl = is;
      }
      if (rdn1 > dhi) rlx = dhi / dn1;
      if (rdn1 < dlo) rlx = dlo / dn1;
      const double rdn2 = rlx * dn2;
      if (std::abs(dn2) > std::abs(rmxbl)) {
        rmxbl = dn2;
        out.vmxbl = 'T';
        out.imxbl = ibl;
        out.ismxbl = is;
      }
      if (rdn2 > dhi) rlx = dhi / dn2;
      if (rdn2 < dlo) rlx = dlo / dn2;
      const double rdn3 = rlx * dn3;
      if (std::abs(dn3) > std::abs(rmxbl)) {
        rmxbl = dn3;
        out.vmxbl = 'D';
        out.imxbl = ibl;
        out.ismxbl = is;
      }
      if (rdn3 > dhi) rlx = dhi / dn3;
      if (rdn3 < dlo) rlx = dlo / dn3;
      const double rdn4 = rlx * dn4;
      if (std::abs(dn4) > std::abs(rmxbl)) {
        rmxbl = duedg;  // Fortran: RMXBL = DUEDG
        out.vmxbl = 'U';
        out.imxbl = ibl;
        out.ismxbl = is;
      }
      if (rdn4 > dhi) rlx = dhi / dn4;
      if (rdn4 < dlo) rlx = dlo / dn4;
    }
  }
  // Fortran: true RMS change. Fortran NBL(1)+NBL(2) (1-based) ->
  // (nblTop+1)+(nblBot+1) 0-based.
  rmsbl = std::sqrt(rmsbl /
                    (4.0 * static_cast<double>(in.mapping.top.nbl + 1 +
                                               in.mapping.bottom.nbl + 1)));
  out.rlx = rlx;
  out.dac = dac;
  out.rmsbl = rmsbl;
  out.rmxbl = rmxbl;

  // Fortran: under-relaxed change in Re (LALFA) or alpha
  if (in.lalfa) {
    in.cl = in.cl + rlx * dac;
  } else {
    in.alfa = in.alfa + rlx * dac;
  }

  // Fortran: BL variables with under-relaxed changes
  // (xbl.f:1493-1543)
  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    const BoundaryLayerSideMapping& m = *sides[static_cast<std::size_t>(is)];
    const auto uedgS = sideSpan(in.uedg, side);
    const auto ctauS = sideSpan(in.ctau, side);
    const auto thetS = sideSpan(in.thet, side);
    const auto dstrS = sideSpan(in.dstr, side);
    const auto massS = sideSpan(in.mass, side);
    const auto unewS = std::span<const double>(
        unew[static_cast<std::size_t>(is)].data(),
        static_cast<std::size_t>(m.nbl) + 1);
    const auto uacS = std::span<const double>(
        uac[static_cast<std::size_t>(is)].data(),
        static_cast<std::size_t>(m.nbl) + 1);
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t line =
          static_cast<std::size_t>(in.isys[static_cast<std::size_t>(is)][k]);
      const double dctau = in.vdelRes[line * 3 + 0] - dac * in.vdelRe[line * 3 + 0];
      const double dthet = in.vdelRes[line * 3 + 1] - dac * in.vdelRe[line * 3 + 1];
      const double dmass = in.vdelRes[line * 3 + 2] - dac * in.vdelRe[line * 3 + 2];
      const double duedg = unewS[k] + dac * uacS[k] - uedgS[k];
      const double ddstr = (dmass - dstrS[k] * duedg) / uedgS[k];
      ctauS[k] = ctauS[k] + rlx * dctau;
      thetS[k] = thetS[k] + rlx * dthet;
      dstrS[k] = dstrS[k] + rlx * ddstr;
      uedgS[k] = uedgS[k] + rlx * duedg;
      // Fortran: DSWAKI
      double dswaki = 0.0;
      if (ibl > m.iblte) {
        // Fortran IW = IBL-IBLTE(IS) (1-based) -> 0-based iw = IW-1.
        const std::size_t iw = static_cast<std::size_t>(ibl - m.iblte - 1);
        if (iw >= in.wgap.size()) {
          throw std::invalid_argument("update: WGAP index outside");
        }
        dswaki = in.wgap[iw];
      }
      // Fortran: eliminate absurd transients
      if (ibl >= itran[static_cast<std::size_t>(is)]) {
        ctauS[k] = std::min(ctauS[k], 0.25);
      }
      const double hklim = ibl <= m.iblte ? 1.02 : 1.00005;
      const double msq =
          uedgS[k] * uedgS[k] * hstinv /
          (in.gamm1 * (1.0 - 0.5 * uedgS[k] * uedgS[k] * hstinv));
      dstrS[k] = dslim(dstrS[k] - dswaki, thetS[k], msq, hklim) + dswaki;
      // Fortran: new mass defect (nonlinear)
      massS[k] = dstrS[k] * uedgS[k];
    }
    // Fortran: no "islands" of negative Ue
    for (int ibl = 2; ibl <= m.iblte; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const std::size_t km = static_cast<std::size_t>(ibl - 1);
      if (uedgS[km] > 0.0 && uedgS[k] <= 0.0) {
        uedgS[k] = uedgS[km];
        massS[k] = dstrS[k] * uedgS[k];
      }
    }
  }

  // Fortran: align upper wake arrays with lower wake arrays
  // (xbl.f:1547-1557)
  const int nw = in.mapping.bottom.nbl - in.mapping.bottom.iblte;
  for (int kbl = 1; kbl <= nw; ++kbl) {
    const std::size_t kt = static_cast<std::size_t>(in.mapping.top.iblte + kbl);
    const std::size_t kb =
        static_cast<std::size_t>(in.mapping.bottom.iblte + kbl);
    if (kt >= in.ctau.top.size() || kb >= in.ctau.bottom.size() ||
        kt >= in.dssr.top.size() || kb >= in.dssr.bottom.size() ||
        kt >= in.mdef.top.size() || kb >= in.mdef.bottom.size() ||
        kt >= in.pdef.top.size() || kb >= in.pdef.bottom.size() ||
        kt >= in.kdef.top.size() || kb >= in.kdef.bottom.size()) {
      throw std::invalid_argument("update: wake index outside");
    }
    in.ctau.top[kt] = in.ctau.bottom[kb];
    in.thet.top[kt] = in.thet.bottom[kb];
    in.dstr.top[kt] = in.dstr.bottom[kb];
    in.uedg.top[kt] = in.uedg.bottom[kb];
    in.tau.top[kt] = in.tau.bottom[kb];
    in.dis.top[kt] = in.dis.bottom[kb];
    in.ctq.top[kt] = in.ctq.bottom[kb];
    in.delt.top[kt] = in.delt.bottom[kb];
    in.tstr.top[kt] = in.tstr.bottom[kb];
    in.dssr.top[kt] = in.dssr.bottom[kb];
    in.mdef.top[kt] = in.mdef.bottom[kb];
    in.pdef.top[kt] = in.pdef.bottom[kb];
    in.kdef.top[kt] = in.kdef.bottom[kb];
  }

  return out;
}

}  // namespace xfoil::core::boundary_layer

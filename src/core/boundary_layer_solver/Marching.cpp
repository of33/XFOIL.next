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

// File: src/core/boundary_layer_solver/Marching.cpp
// Implementation of the BL marching and Newton assemblers.
// Formula-faithful adoption from fortran-src/xbl.f; constants,
// branch conditions and the order of the computations are unchanged.

#include "xfoil/core/boundary_layer/Marching.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include "xfoil/core/boundary_layer/Coupling.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/Update.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/math/LinearAlgebra.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::boundary_layer {

namespace {

// Fortran 1-based -> C++ 0-based: explicit size conversion.
std::size_t sz(int i) {
  return static_cast<std::size_t>(i);
}

// Fortran: Label 109 BLVAR sequence (xbl.f:797-803, xbl.f:1117-1123).
// The COMMON HK2 side effect is replicated by overwriting sec.hk
// between the BLVAR calls.
BoundaryLayerTurbulentSecondary fallbackBlvar(
    bool wake, int ibl, int itran, const BoundaryLayerPrimary& prim,
    const BoundaryLayerSecondary& sec, const BoundaryLayerParameters& blpar) {
  BoundaryLayerSecondary secFb = sec;
  BoundaryLayerTurbulentSecondary ts{};
  if (ibl < itran) {
    ts = blvar(StationType::kLaminar,
               BlvarInput{prim, secFb, blpar});
    secFb.hk = ts.hk;
  } else {
    ts = blvar(StationType::kTurbulent,
               BlvarInput{prim, secFb, blpar});
    secFb.hk = ts.hk;
  }
  if (wake) {
    ts = blvar(StationType::kWake,
               BlvarInput{prim, secFb, blpar});
  }
  return ts;
}

// Fortran COMMON station arrays are dimensioned IVX and always
// writable. Grow only the newly carried output fields to the mapping
// extent; existing station values and the grow-only semantics stay
// unchanged.
void ensureStateOutputFields(MarchInput& in) {
  const int nw = in.mapping.bottom.nbl - in.mapping.bottom.iblte;
  const int topExtent =
      std::max(in.mapping.top.nbl, in.mapping.top.iblte + nw);
  const std::size_t needTop =
      static_cast<std::size_t>(std::max(topExtent, 0)) + 1;
  const std::size_t needBottom =
      static_cast<std::size_t>(std::max(in.mapping.bottom.nbl, 0)) + 1;
  const auto grow = [needTop, needBottom](BoundaryLayerStationField& f) {
    if (f.top.size() < needTop) {
      f.top.resize(needTop, 0.0);
    }
    if (f.bottom.size() < needBottom) {
      f.bottom.resize(needBottom, 0.0);
    }
  };
  grow(in.dssr);
  grow(in.mdef);
  grow(in.pdef);
  grow(in.kdef);
}

// Fortran: BLPRV/BLKIN/BLVAR exit -> complete BLDIF
// station state.
BldifStation makeCom(const BoundaryLayerPrimary& prim,
                     const BoundaryLayerSecondary& sec,
                     const BoundaryLayerTurbulentSecondary& ts) {
  BldifStation st{};
  st.x = prim.x;
  st.ue = prim.ue;
  st.t = prim.t;
  st.d = prim.d;
  st.s = prim.s;
  st.ampl = prim.ampl;
  st.dw = prim.dw;
  st.m = sec.m;
  st.mU = sec.mUe;
  st.mMs = sec.mMs;
  st.h = sec.h;
  st.hT = sec.hT;
  st.hD = sec.hD;
  st.hk = sec.hk;
  st.hkU = sec.hkUe;
  st.hkT = sec.hkT;
  st.hkD = sec.hkD;
  st.hkMs = sec.hkMs;
  st.rt = sec.rt;
  st.rtU = sec.rtUe;
  st.rtT = sec.rtT;
  st.rtMs = sec.rtMs;
  st.rtRe = sec.rtRe;
  st.hs = ts.hs;
  st.hsU = ts.hsU;
  st.hsT = ts.hsT;
  st.hsD = ts.hsD;
  st.hsMs = ts.hsMs;
  st.hsRe = ts.hsRe;
  st.hc = ts.hc;
  st.hcU = ts.hcU;
  st.hcT = ts.hcT;
  st.hcD = ts.hcD;
  st.hcMs = ts.hcMs;
  st.cf = ts.cf;
  st.cfU = ts.cfU;
  st.cfT = ts.cfT;
  st.cfD = ts.cfD;
  st.cfMs = ts.cfMs;
  st.cfRe = ts.cfRe;
  st.di = ts.di;
  st.diU = ts.diU;
  st.diT = ts.diT;
  st.diD = ts.diD;
  st.diS = ts.diS;
  st.diMs = ts.diMs;
  st.diRe = ts.diRe;
  st.us = ts.us;
  st.usU = ts.usU;
  st.usT = ts.usT;
  st.usD = ts.usD;
  st.usMs = ts.usMs;
  st.cq = ts.cq;
  st.cqU = ts.cqU;
  st.cqT = ts.cqT;
  st.cqD = ts.cqD;
  st.cqMs = ts.cqMs;
  st.cqRe = ts.cqRe;
  st.de = ts.de;
  st.deU = ts.deU;
  st.deT = ts.deT;
  st.deD = ts.deD;
  st.deMs = ts.deMs;
  return st;
}

// Fortran: TrchekStation from a BLDIF station state.
TrchekStation makeTrchekStation(const BldifStation& st) {
  TrchekStation s{};
  s.x = st.x;
  s.ue = st.ue;
  s.t = st.t;
  s.d = st.d;
  s.s = st.s;
  s.ampl = st.ampl;
  s.hk = st.hk;
  s.hkT = st.hkT;
  s.hkD = st.hkD;
  s.hkU = st.hkU;
  s.hkMs = st.hkMs;
  s.rt = st.rt;
  s.rtT = st.rtT;
  s.rtU = st.rtU;
  s.rtMs = st.rtMs;
  s.rtRe = st.rtRe;
  return s;
}

// Fortran: GAUSS(4,4,VS2,VSREZ,1) — VS2 as a 4x4 matrix (columns 1..4).
// Fortran Label 100 in TRCHEK2 (xblsys.f:423-425): WRITE
// 'TRCHEK2: N2 convergence failed.' + format 6700, if the
// 30-iteration Newton run ends without convergence and without a punch-out.
// Fortran MAX/MIN internals: gfortran evaluates MAX(a,b) as (a>b)?a:b and
// MIN(a,b) as (a<b)?a:b; a NaN operand therefore loses to the
// other operand (canonically observed in the degenerate
// oper-state/oper-init-inviscid-then-bang regime). The helper
// reproduces these semantics for finite values identically to std::max/
// std::min and replaces it at the MAX/MIN sites of the
// marching code.
double fortranMax(double a, double b) { return a > b ? a : b; }
double fortranMin(double a, double b) { return a < b ? a : b; }
double fortranMax3(double a, double b, double c) {
  return fortranMax(fortranMax(a, b), c);
}

void pushTrchek2Failure(std::vector<ViscousEventRecord>& events,
                        ViscousEventPhase phase, int side, int station,
                        const TrchekResult& tr) {
  if (!tr.n2ConvergenceFailed) {
    return;
  }
  ViscousEventRecord ev{};
  ev.kind = ViscousEventKind::trchek2ConvergenceFailed;
  ev.phase = phase;
  ev.iteration = 0;
  ev.side = side;
  ev.station = station;
  ev.trchekX1 = tr.failX1;
  ev.trchekXt = tr.failXt;
  ev.trchekX2 = tr.failX2;
  ev.trchekAmpl1 = tr.failAmpl1;
  ev.trchekAmplt = tr.failAmplt;
  ev.trchekAmpl2 = tr.failAmpl2;
  ev.trchekAx = tr.failAx;
  ev.trchekDa2 = tr.failDa2;
  events.push_back(ev);
}

void gauss4(std::array<std::array<double, 5>, 4>& vs2,
            std::array<double, 4>& vsrez) {
  std::array<double, 16> z{};
  std::array<double, 4> r{};
  for (int k = 0; k < 4; ++k) {
    for (int l = 0; l < 4; ++l) {
      z[sz(k) * 4 + sz(l)] =
          vs2[sz(k)][sz(l)];
    }
    r[sz(k)] = vsrez[sz(k)];
  }
  xfoil::core::math::gauss(z, r, 4, 1);
  for (int k = 0; k < 4; ++k) {
    vsrez[sz(k)] = r[sz(k)];
  }
}

}  // namespace

MarchDiagnostics mrchue(MarchInput& in, std::array<MarchSideState, 2>& state) {
  ensureStateOutputFields(in);
  MarchDiagnostics diag{};
  // Fortran: shape-factor criteria for separation
  const double hlmax = 3.8;
  const double htmax = 2.5;

  for (int is = 0; is < 2; ++is) {
    // Fortran: WRITE(*,*) ' side ', IS, ' ...' (xbl.f:561).
    diag.events.push_back(ViscousEventRecord{
        ViscousEventKind::marchingSideStart, ViscousEventPhase::initMarch, 0,
        is, -1, 0.0, 0.0, 0.0, -1});
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    static_cast<void>(side);
    const BoundaryLayerSideMapping& m =
        is == 0 ? in.mapping.top : in.mapping.bottom;
    const auto xssiS = in.xssi[sz(is)];
    const auto thetS = is == 0 ? std::span<double>(in.thet.top)
                               : std::span<double>(in.thet.bottom);
    const auto dstrS = is == 0 ? std::span<double>(in.dstr.top)
                               : std::span<double>(in.dstr.bottom);
    const auto ctauS = is == 0 ? std::span<double>(in.ctau.top)
                               : std::span<double>(in.ctau.bottom);
    const auto uedgS = is == 0 ? std::span<double>(in.uedg.top)
                               : std::span<double>(in.uedg.bottom);
    const auto massS = is == 0 ? std::span<double>(in.mass.top)
                               : std::span<double>(in.mass.bottom);
    MarchSideState& st = state[sz(is)];

    const double amcrit = in.acrit[sz(is)];
    const double reybl = in.props.reybl;
    const double gm1bl = in.props.gm1bl;
    const double hstinv = in.props.hstinv;

    // Fortran: similarity station initialized with Thwaites
    // (IBL=2 1-based -> 0-based 1)
    double ami = 0.0;
    double cti = 0.03;
    double thi = 0.0;
    double dsi = 0.0;
    double xsi0 = xssiS[1];
    double uei0 = uedgS[1];
    const double bule0 = 1.0;
    const double ucon = uei0 / std::pow(xsi0, bule0);
    const double tsq =
        0.45 / (ucon * (5.0 * bule0 + 1.0) * reybl) * std::pow(xsi0, 1.0 - bule0);
    thi = std::sqrt(tsq);
    dsi = 2.2 * thi;

    st.tran = false;
    st.turb = false;
    in.itran[sz(is)] = m.iblte;

    // Fortran: downstream march (IBL=2..NBL 1-based ->
    // 0-based 1..nbl inclusive)
    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const bool simi = ibl == 1;
      const bool wake = ibl > m.iblte;
      const double xsi = xssiS[sz(ibl)];
      double uei = uedgS[sz(ibl)];
      double dswaki = 0.0;
      if (wake) {
        // Fortran IW = IBL-IBLTE(IS) (1-based) -> 0-based iw = IW-1.
        const std::size_t iw =
            static_cast<std::size_t>(ibl - m.iblte - 1);
        if (iw >= in.wgap.size()) {
          throw std::invalid_argument("mrchue: WGAP index outside");
        }
        dswaki = in.wgap[iw];
      }
      double dmax = 0.0;
      bool direct = true;
      double hmax = hlmax;
      bool tranNow = false;
      double tteSt = 0.0;
      double dteSt = 0.0;
      double cteSt = 0.0;
      // Fortran: prescribed HK for the inverse computation (xbl.f:691-718,
      // HTARG is station state, no diagnostics).
      double htarg = 0.0;
      // Fortran: post-BLVAR state of the last BLSYS assembly
      // (xbl.f:839-843, COM1=COM2 mixed, xbl.f:848-850).
      BoundaryLayerTurbulentSecondary tsSaved{};
      // Fortran: pre-update primary values of the last BLSYS assembly
      // (xbl.f:839-843: TAU/DIS/TSTR use the primary values R2/U2/T2
      // from before the update).
      BoundaryLayerPrimary primSaved{};
      BoundaryLayerSecondary secSaved{};

      // Fortran: Newton iteration loop (ITBL=1..25) with GO TO 100 retry
      bool converged = false;
      for (int itbl = 1; itbl <= 25; ++itbl) {
        // Fortran: BLPRV+BLKIN
        BoundaryLayerPrimary prim =
            blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
        BoundaryLayerSecondary sec = blkin(prim, in.props);
        st.tran = false;
        // Fortran: Transition check
        if (!simi && !st.turb) {
          TrchekInput ti{};
          ti.first = makeTrchekStation(st.com1);
          ti.second = makeTrchekStation(makeCom(prim, sec, {}));
          ti.xiforc = in.xiforc[sz(is)];
          ti.acrit = amcrit;
          ti.idampv = in.props.idampv;
          ti.hstinv = in.props.hstinv;
          ti.gm1bl = in.props.gm1bl;
          ti.rstbl = in.props.rstbl;
          ti.rstblMs = in.props.rstblMs;
          ti.hstinvMs = in.props.hstinvMs;
          ti.reybl = in.props.reybl;
          ti.reyblMs = in.props.reyblMs;
          ti.reyblRe = in.props.reyblRe;
          ti.hvrat = in.props.hvrat;
          const auto tr = trchek2(ti);
          pushTrchek2Failure(diag.events, ViscousEventPhase::initMarch, is, ibl, tr);
          ami = tr.ampl;
          st.xt = tr.xt;
          st.xtA1 = tr.xtA1;
          st.xtT1 = tr.xtT1;
          st.xtD1 = tr.xtD1;
          st.xtU1 = tr.xtU1;
          st.xtX1 = tr.xtX1;
          st.xtT2 = tr.xtT2;
          st.xtD2 = tr.xtD2;
          st.xtU2 = tr.xtU2;
          st.xtX2 = tr.xtX2;
          st.xtMs = tr.xtMs;
          st.xtRe = tr.xtRe;
          st.xtXf = tr.xtXf;
          st.tran = tr.tran;
          st.trforc = tr.trforc;
          if (tr.tran) {
            in.itran[sz(is)] = ibl;
            if (cti <= 0.0) {
              cti = 0.03;
            }
          } else {
            // Fortran MRCHUE: ITRAN(IS) = IBL+2 (xbl.f:633/817).
            in.itran[sz(is)] = ibl + 2;
          }
          // Fortran: AMPL2 = AMI and, for transition, S2 = CTI in the
          // COM2 state (xbl.f:628-630): BLPRV/BLKIN with the
          // updated AMI/CTI.
          prim = blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
          sec = blkin(prim, in.props);
        }
        tranNow = st.tran;
        primSaved = prim;
        secSaved = sec;
        // Fortran: TESYS or BLSYS
        BldifSystem sys{};
        if (ibl == m.iblte + 1) {
          // Fortran: TTE/DTE/CTE at the wake start with separate IBLTE
          // indices per page (xbl.f:640-643: IBLTE(1),1 / IBLTE(2),2).
          const int iblteOther = is == 0 ? in.mapping.bottom.iblte
                                         : in.mapping.top.iblte;
          const double tte =
              thetS[sz(m.iblte)] +
              (is == 0 ? in.thet.bottom[sz(iblteOther)]
                       : in.thet.top[sz(iblteOther)]);
          const double dte =
              dstrS[sz(m.iblte)] +
              (is == 0 ? in.dstr.bottom[sz(iblteOther)]
                       : in.dstr.top[sz(iblteOther)]) +
              in.ante;
          const double thetOther = is == 0 ? in.thet.bottom[sz(iblteOther)]
                                           : in.thet.top[sz(iblteOther)];
          const double cteOther =
              is == 0 ? in.ctau.bottom[sz(iblteOther)]
                      : in.ctau.top[sz(iblteOther)];
          const double cte =
              (ctauS[sz(m.iblte)] * thetS[sz(m.iblte)] +
               cteOther * thetOther) /
              (thetS[sz(m.iblte)] + thetOther);
          tteSt = tte;
          dteSt = dte;
          cteSt = cte;
          TesysInput teIn{};
          teIn.wake.primary = prim;
          teIn.wake.secondary = sec;
          teIn.wake.blpar = in.blpar;
          teIn.cte = cte;
          teIn.tte = tte;
          teIn.dte = dte;
          const auto te = tesys(teIn);
          sys = te.system;
          tsSaved = te.wakeSecondary;  // Fortran BLVAR(3) state
        } else {
          BlsysInput bi{};
          bi.first = st.com1;
          bi.primary2 = prim;
          bi.secondary2 = sec;
          bi.wake = wake;
          bi.turb = st.turb;
          bi.tran = tranNow;
          bi.simi = simi;
          bi.ueEi1 = st.ueEi1;  // Fortran U1_UEI (BLPRV of the "1" station)
          bi.ueMs1 = st.ueMs1;
          bi.ueEi2 = prim.ueEi;
          bi.ueMs2 = prim.ueMs;
          bi.amcrit = amcrit;
          bi.idampv = in.props.idampv;
          bi.bule = 1.0;
          bi.xt = st.xt;
          bi.xtA1 = st.xtA1;
          bi.xtT1 = st.xtT1;
          bi.xtD1 = st.xtD1;
          bi.xtU1 = st.xtU1;
          bi.xtX1 = st.xtX1;
          bi.xtT2 = st.xtT2;
          bi.xtD2 = st.xtD2;
          bi.xtU2 = st.xtU2;
          bi.xtX2 = st.xtX2;
          bi.xtMs = st.xtMs;
          bi.xtRe = st.xtRe;
          bi.xtXf = st.xtXf;
          bi.props = in.props;
          bi.blpar = in.blpar;
          sys = blsys(bi, tsSaved);
        }
        if (simi) {
          // Fortran BLSYS: on the similarity station the "1" and
          // "2" variables are identical; COMMON COM1 is overwritten
          // (xblsys.f:617-621) and affects HTARG/HK1 consumers in
          // the same and following Newton iterations.
          st.com1 = makeCom(prim, sec, tsSaved);
          st.com1.hk = tsSaved.hk;
          st.ueEi1 = prim.ueEi;
          st.ueMs1 = prim.ueMs;
        }
        // Fortran: VS2(4,*) and VSREZ(4) for the 4x4 GAUSS call
        auto vs2mat = std::array<std::array<double, 5>, 4>{};
        auto vsrez4 = std::array<double, 4>{};
        for (int k = 0; k < 4; ++k) {
          for (int l = 0; l < 5; ++l) {
            vs2mat[sz(k)][sz(l)] =
                sys.vs2[sz(k)][sz(l)];
          }
          vsrez4[sz(k)] =
              sys.vsrez[sz(k)];
        }

        if (direct) {
          // Fortran direct mode (dUe = 0 in the empty fourth row)
          vs2mat[3][0] = 0.0;
          vs2mat[3][1] = 0.0;
          vs2mat[3][2] = 0.0;
          vs2mat[3][3] = 1.0;
          vsrez4[3] = 0.0;
        gauss4(vs2mat, vsrez4);
        // Fortran: maximum changes and under-relaxation
          dmax = fortranMax(std::abs(vsrez4[1] / thi),
                          std::abs(vsrez4[2] / dsi));
          if (ibl < in.itran[sz(is)]) {
            dmax = fortranMax(dmax, std::abs(vsrez4[0] / 10.0));
          }
          if (ibl >= in.itran[sz(is)]) {
            dmax = fortranMax(dmax, std::abs(vsrez4[0] / cti));
          }
          double rlx = 1.0;
          if (dmax > 0.3) rlx = 0.3 / dmax;
          // Fortran: is the direct mode applicable?
          if (ibl != m.iblte + 1) {
            const double msq =
                uei * uei * hstinv / (gm1bl * (1.0 - 0.5 * uei * uei * hstinv));
            const double htest =
                (dsi + rlx * vsrez4[2]) / (thi + rlx * vsrez4[1]);
            const KinematicShapeValue hkTest = hkin(htest, msq);
            hmax = ibl < in.itran[sz(is)] ? hlmax : htmax;
            direct = hkTest.value < hmax;
          }
          if (direct) {
            // Fortran: usual update
            if (ibl >= in.itran[sz(is)]) {
              cti = cti + rlx * vsrez4[0];
            }
            thi = thi + rlx * vsrez4[1];
            dsi = dsi + rlx * vsrez4[2];
          } else {
            // Fortran: prescribed HK for the inverse computation
            htarg = 0.0;
            if (ibl < in.itran[sz(is)]) {
              htarg = st.com1.hk + 0.03 * (xsi - st.com1.x) / st.com1.t;
            } else if (ibl == in.itran[sz(is)]) {
              htarg = st.com1.hk +
                      (0.03 * (st.xt - st.com1.x) - 0.15 * (xsi - st.xt)) /
                          st.com1.t;
            } else if (wake) {
              const double konst = 0.03 * (xsi - st.com1.x) / st.com1.t;
              double hk2 = st.com1.hk;
              for (int n = 0; n < 3; ++n) {
                hk2 = hk2 - (hk2 + konst * (hk2 - 1.0) * (hk2 - 1.0) * (hk2 - 1.0) -
                             st.com1.hk) /
                                (1.0 + 3.0 * konst * (hk2 - 1.0) * (hk2 - 1.0));
              }
              htarg = hk2;
            } else {
              htarg = st.com1.hk - 0.15 * (xsi - st.com1.x) / st.com1.t;
            }
            if (wake) {
              htarg = fortranMax(htarg, 1.01);
            } else {
              htarg = fortranMax(htarg, hmax);
            }
            diag.lastInverseStation = ibl;
            diag.lastInverseHk = htarg;  // copy for diagnostics
            // Fortran: WRITE(*,1300) IBL, HTARG (xbl.f:723).
            diag.events.push_back(ViscousEventRecord{
                ViscousEventKind::mrchueInverseMode,
                ViscousEventPhase::initMarch, 0, is, ibl, htarg, 0.0, 0.0,
                -1});
            continue;  // Fortran: GO TO 100 (retry with prescribed HK)
          }
        } else {
          // Fortran: inverse mode (enforce HK at HTARG)
          vs2mat[3][0] = 0.0;
          vs2mat[3][1] = sec.hkT;
          vs2mat[3][2] = sec.hkD;
          vs2mat[3][3] = sec.hkUe;
          vsrez4[3] = htarg - tsSaved.hk;
          gauss4(vs2mat, vsrez4);
          dmax = fortranMax3(std::abs(vsrez4[1] / thi),
                             std::abs(vsrez4[2] / dsi),
                             std::abs(vsrez4[3] / uei));
          if (ibl >= in.itran[sz(is)]) {
            dmax = fortranMax(dmax, std::abs(vsrez4[0] / cti));
          }
          double rlx = 1.0;
          if (dmax > 0.3) rlx = 0.3 / dmax;
          if (ibl >= in.itran[sz(is)]) {
            cti = cti + rlx * vsrez4[0];
          }
          thi = thi + rlx * vsrez4[1];
          dsi = dsi + rlx * vsrez4[2];
          uei = uei + rlx * vsrez4[3];
        }
        // Fortran: eliminate absurd transients
        if (ibl >= in.itran[sz(is)]) {
          cti = fortranMin(cti, 0.30);
          cti = fortranMax(cti, 0.0000001);
        }
        const double hklim = ibl <= m.iblte ? 1.02 : 1.00005;
        const double msq2 =
            uei * uei * hstinv / (gm1bl * (1.0 - 0.5 * uei * uei * hstinv));
        dsi = dslim(dsi - dswaki, thi, msq2, hklim) + dswaki;
        if (dmax <= 1.0e-5) {
          converged = true;
          break;
        }
      }
      if (!converged) {
        diag.converged = false;
        diag.failedStation = ibl;
        diag.failedSide = is;
        diag.failedResidual = dmax;
        // Fortran: WRITE(*,1350) IBL, IS, DMAX (xbl.f:779, MRCHUE).
        diag.events.push_back(ViscousEventRecord{
            ViscousEventKind::mrchueConvergenceFailure,
            ViscousEventPhase::initMarch, 0, is, ibl, 0.0, dmax, 0.0, -1});
        if (dmax > 0.1) {
          // Fortran: current solution is unusable -> extrapolate
          // (xbl.f:788 IBL.GT.3 1-based -> 0-based ibl > 2).
          if (ibl > 2) {
            if (ibl <= m.iblte) {
              thi = thetS[sz(ibl - 1)] *
                    std::pow(xssiS[sz(ibl)] /
                                 xssiS[sz(ibl - 1)],
                             0.5);
              dsi = dstrS[sz(ibl - 1)] *
                    std::pow(xssiS[sz(ibl)] /
                                 xssiS[sz(ibl - 1)],
                             0.5);
            } else if (ibl == m.iblte + 1) {
              cti = cteSt;  // Fortran CTI=CTE (xbl.f:793)
              thi = tteSt;  // Fortran THI=TTE
              dsi = dteSt;  // Fortran DSI=DTE
            } else {
              thi = thetS[sz(ibl - 1)];
              const double ratlen =
                  (xssiS[sz(ibl)] -
                   xssiS[sz(ibl - 1)]) /
                  (10.0 * dstrS[sz(ibl - 1)]);
              dsi = (dstrS[sz(ibl - 1)] +
                     thi * ratlen) /
                    (1.0 + ratlen);
            }
            if (ibl == in.itran[sz(is)]) cti = 0.05;
            if (ibl > in.itran[sz(is)]) {
              cti = ctauS[sz(ibl - 1)];
            }
            uei = uedgS[sz(ibl)];
            // Fortran: IF(IBL.GT.2 .AND. IBL.LT.NBL(IS)) (xbl.f:805-806)
            // 1-based -> 0-based ibl > 1 && ibl < m.nbl.
            if (ibl > 1 && ibl < m.nbl) {
              uei = 0.5 * (uedgS[sz(ibl - 1)] +
                           uedgS[sz(ibl + 1)]);
            }
          }
        }
      }
      // Fortran: converged path (GO TO 110, xbl.f:830): only
      // BLPRV/BLKIN again; the BLVAR fields (HS/HC/US/CQ/CF/DI/DE)
      // from the last BLSYS assembly are kept (xbl.f:839-843,
      // COM1=COM2 mixed, xbl.f:848-850). Fallback path (Label 109,
      // xbl.f:809-827): BLVAR/BLMID again.
      BoundaryLayerPrimary primS =
          blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
      BoundaryLayerSecondary secS = blkin(primS, in.props);
      // Fortran: Label 109 (xbl.f:813-818) — only on the non-converged
      // path: TRCHEK with AMPL2 update and ITRAN rewriting.
      if (!converged && !simi && !st.turb) {
        TrchekInput ti{};
        ti.first = makeTrchekStation(st.com1);
        ti.second = makeTrchekStation(makeCom(primS, secS, {}));
        ti.xiforc = in.xiforc[sz(is)];
        ti.acrit = amcrit;
        ti.idampv = in.props.idampv;
        ti.hstinv = in.props.hstinv;
        ti.gm1bl = in.props.gm1bl;
        ti.rstbl = in.props.rstbl;
        ti.rstblMs = in.props.rstblMs;
        ti.hstinvMs = in.props.hstinvMs;
        ti.reybl = in.props.reybl;
        ti.reyblMs = in.props.reyblMs;
        ti.reyblRe = in.props.reyblRe;
        ti.hvrat = in.props.hvrat;
        const auto tr = trchek2(ti);
        pushTrchek2Failure(diag.events, ViscousEventPhase::initMarch, is, ibl, tr);
        ami = tr.ampl;
        st.xt = tr.xt;
        st.xtA1 = tr.xtA1;
        st.xtT1 = tr.xtT1;
        st.xtD1 = tr.xtD1;
        st.xtU1 = tr.xtU1;
        st.xtX1 = tr.xtX1;
        st.xtT2 = tr.xtT2;
        st.xtD2 = tr.xtD2;
        st.xtU2 = tr.xtU2;
        st.xtX2 = tr.xtX2;
        st.xtMs = tr.xtMs;
        st.xtRe = tr.xtRe;
        st.xtXf = tr.xtXf;
        st.tran = tr.tran;
        st.trforc = tr.trforc;
        if (tr.tran) {
          in.itran[sz(is)] = ibl;
        } else {
          in.itran[sz(is)] = ibl + 2;
        }
        // Fortran: TRCHEK sets AMPL2 in the COMMON (xbl.f:814) — the
        // "2"-station amplification follows the updated AMI (as in the
        // Newton loop via BLPRV/BLKIN with the updated AMI).
        const BoundaryLayerPrimary primTr =
            blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
        secS = blkin(primTr, in.props);
        primS = primTr;
      }
      BoundaryLayerTurbulentSecondary tsUse{};
      if (converged) {
        tsUse = tsSaved;
      } else {
        tsUse = fallbackBlvar(wake, ibl, in.itran[sz(is)], primS, secS,
                              in.blpar);
      }
      BldifStation com2 = makeCom(primS, secS, tsUse);
      // Fortran: MRCHDU/MRCHUE Label 110 calls BLPRV/BLKIN again before
      // COM1=COM2; COM1-HK2 is the unclamped HKIN value there,
      // also in the fallback path Label 109 (xbl.f:839-843, xbl.f:1131-1174).
      // Fortran: TAU/DIS/CTQ/DELT/TSTR (xbl.f:839-843) use the primary values
      // from before the update in the converged path
      // (last BLSYS assembly), and the fresh Label-109 values in the fallback
      // path.
      const BoundaryLayerPrimary& primUse = converged ? primSaved : primS;
      const BoundaryLayerSecondary& secUse = converged ? secSaved : secS;
      // Fortran: store primary variables
      ctauS[sz(ibl)] =
          ibl < in.itran[sz(is)] ? ami : cti;
      thetS[sz(ibl)] = thi;
      dstrS[sz(ibl)] = dsi;
      uedgS[sz(ibl)] = uei;
      massS[sz(ibl)] = dsi * uei;
      if (is == 0) {
        in.tau.top[sz(ibl)] =
            0.5 * secUse.r * primUse.ue * primUse.ue * tsUse.cf;
        in.dis.top[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.ue * tsUse.di *
            tsUse.hs * 0.5;
        in.ctq.top[sz(ibl)] = tsUse.cq;
        in.delt.top[sz(ibl)] = tsUse.de;
        in.tstr.top[sz(ibl)] = tsUse.hs * primUse.t;
        in.dssr.top[sz(ibl)] = tsUse.hc * primUse.t;
        in.mdef.top[sz(ibl)] = secUse.r * primUse.ue * primUse.d;
        in.pdef.top[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.t;
        in.kdef.top[sz(ibl)] = secUse.r * primUse.ue * primUse.ue *
                               primUse.ue * primUse.t * tsUse.hs * 0.5;
      } else {
        in.tau.bottom[sz(ibl)] =
            0.5 * secUse.r * primUse.ue * primUse.ue * tsUse.cf;
        in.dis.bottom[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.ue * tsUse.di *
            tsUse.hs * 0.5;
        in.ctq.bottom[sz(ibl)] = tsUse.cq;
        in.delt.bottom[sz(ibl)] = tsUse.de;
        in.tstr.bottom[sz(ibl)] = tsUse.hs * primUse.t;
        in.dssr.bottom[sz(ibl)] = tsUse.hc * primUse.t;
        in.mdef.bottom[sz(ibl)] = secUse.r * primUse.ue * primUse.d;
        in.pdef.bottom[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.t;
        in.kdef.bottom[sz(ibl)] = secUse.r * primUse.ue * primUse.ue *
                                  primUse.ue * primUse.t * tsUse.hs * 0.5;
      }
      // Fortran: set "1" variables from "2" variables
      st.com1 = com2;
      st.ueEi1 = primS.ueEi;
      st.ueMs1 = primS.ueMs;
      st.ampl1 = ami;
      // Fortran: turbulent intervals follow the transition point or TE
      if (tranNow || ibl == m.iblte) {
        st.turb = true;
        in.tforce[sz(is)] = st.trforc;
        in.xssitr[sz(is)] = st.xt;
      }
      st.tran = false;
      // Fortran: work variables for the next station at the wake start
      // (xbl.f:866-869).
      if (ibl == m.iblte) {
        const auto thetTop =
            std::span<const double>(in.thet.top);
        const auto thetBot =
            std::span<const double>(in.thet.bottom);
        const auto dstrTop =
            std::span<const double>(in.dstr.top);
        const auto dstrBot =
            std::span<const double>(in.dstr.bottom);
        thi = thetTop[sz(in.mapping.top.iblte)] +
              thetBot[sz(in.mapping.bottom.iblte)];
        dsi = dstrTop[sz(in.mapping.top.iblte)] +
              dstrBot[sz(in.mapping.bottom.iblte)] + in.ante;
      }
      (void)tranNow;
    }
  }
  return diag;
}


MarchDiagnostics mrchdu(MarchInput& in, std::array<MarchSideState, 2>& state) {
  ensureStateOutputFields(in);
  MarchDiagnostics diag{};
  // Fortran: SENSWT/DEPS
  const double senswt = 1000.0;
  const double deps = 5.0e-6;
  // Fortran: AMI is a persistent local REAL(8) of MRCHDU;
  // it is assigned only in the IBL.LT.ITROLD branch (xbl.f:936) and by TRCHEK
  // (xbl.f:968, AMI = AMPL2), and keeps the value from the previous station
  // for stations >= ITROLD.
  double ami = 0.0;

  for (int is = 0; is < 2; ++is) {
    const SurfaceSide side = is == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
    static_cast<void>(side);
    const BoundaryLayerSideMapping& m =
        is == 0 ? in.mapping.top : in.mapping.bottom;
    const auto xssiS = in.xssi[sz(is)];
    const auto thetS = is == 0 ? std::span<double>(in.thet.top)
                               : std::span<double>(in.thet.bottom);
    const auto dstrS = is == 0 ? std::span<double>(in.dstr.top)
                               : std::span<double>(in.dstr.bottom);
    const auto ctauS = is == 0 ? std::span<double>(in.ctau.top)
                               : std::span<double>(in.ctau.bottom);
    const auto uedgS = is == 0 ? std::span<double>(in.uedg.top)
                               : std::span<double>(in.uedg.bottom);
    const auto massS = is == 0 ? std::span<double>(in.mass.top)
                               : std::span<double>(in.mass.bottom);
    MarchSideState& st = state[sz(is)];

    const double amcrit = in.acrit[sz(is)];
    const double gm1bl = in.props.gm1bl;
    const double hstinv = in.props.hstinv;

    // Fortran: old transition station
    const int itrold = in.itran[sz(is)];
    st.tran = false;
    st.turb = false;
    in.itran[sz(is)] = m.iblte;
    double sens = 0.0;
    double sennew = 0.0;

    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const bool simi = ibl == 1;
      const bool wake = ibl > m.iblte;
      const double xsi = xssiS[sz(ibl)];
      double uei = uedgS[sz(ibl)];
      double thi = thetS[sz(ibl)];
      double dsi = dstrS[sz(ibl)];
      double cti = 0.0;
      if (ibl < itrold) {
        ami = ctauS[sz(ibl)];
        cti = 0.03;
      } else {
        cti = ctauS[sz(ibl)];
        if (cti <= 0.0) cti = 0.03;
      }
      double dswaki = 0.0;
      if (wake) {
        // Fortran IW = IBL-IBLTE(IS) (1-based) -> 0-based iw = IW-1.
        const std::size_t iw = sz(ibl - m.iblte - 1);
        if (iw >= in.wgap.size()) {
          throw std::invalid_argument("mrchdu: WGAP index outside");
        }
        dswaki = in.wgap[iw];
      }
      // Fortran: DSI clamping
      if (ibl <= m.iblte) {
        dsi = fortranMax(dsi - dswaki, 1.02000 * thi) + dswaki;
      } else {
        dsi = fortranMax(dsi - dswaki, 1.00005 * thi) + dswaki;
      }

      double dmax = 0.0;
      bool converged = false;
      double ueref = 0.0;
      double hkref = 0.0;
      double tteSt = 0.0;
      double dteSt = 0.0;
      double cteSt = 0.0;
      // Fortran: post-BLVAR state of the last BLSYS assembly
      // (xbl.f:839-843, COM1=COM2 mixed, xbl.f:848-850).
      BoundaryLayerTurbulentSecondary tsSaved{};
      // Fortran: pre-update primary values of the last BLSYS assembly
      // (xbl.f:1160-1166: TAU/DIS/TSTR use the primary values R2/U2/T2
      // from before the update).
      BoundaryLayerPrimary primSaved{};
      BoundaryLayerSecondary secSaved{};
      for (int itbl = 1; itbl <= 25; ++itbl) {
        BoundaryLayerPrimary prim =
            blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
        BoundaryLayerSecondary sec = blkin(prim, in.props);
        st.tran = false;
        if (!simi && !st.turb) {
          TrchekInput ti{};
          ti.first = makeTrchekStation(st.com1);
          ti.second = makeTrchekStation(makeCom(prim, sec, {}));
          ti.xiforc = in.xiforc[sz(is)];
          ti.acrit = amcrit;
          ti.idampv = in.props.idampv;
          ti.hstinv = in.props.hstinv;
          ti.gm1bl = in.props.gm1bl;
          ti.rstbl = in.props.rstbl;
          ti.rstblMs = in.props.rstblMs;
          ti.hstinvMs = in.props.hstinvMs;
          ti.reybl = in.props.reybl;
          ti.reyblMs = in.props.reyblMs;
          ti.reyblRe = in.props.reyblRe;
          ti.hvrat = in.props.hvrat;
          const auto tr = trchek2(ti);
          pushTrchek2Failure(diag.events, ViscousEventPhase::postMarch, is, ibl, tr);
          ami = tr.ampl;
          st.xt = tr.xt;
          st.xtA1 = tr.xtA1;
          st.xtT1 = tr.xtT1;
          st.xtD1 = tr.xtD1;
          st.xtU1 = tr.xtU1;
          st.xtX1 = tr.xtX1;
          st.xtT2 = tr.xtT2;
          st.xtD2 = tr.xtD2;
          st.xtU2 = tr.xtU2;
          st.xtX2 = tr.xtX2;
          st.xtMs = tr.xtMs;
          st.xtRe = tr.xtRe;
          st.xtXf = tr.xtXf;
          st.tran = tr.tran;
          st.trforc = tr.trforc;
          if (tr.tran) {
            in.itran[sz(is)] = ibl;
          } else {
            // Fortran MRCHDU: ITRAN(IS) = IBL+2 (xbl.f:970/1139).
            in.itran[sz(is)] = ibl + 2;
          }
          // Fortran: AMPL2 = AMI (TRCHEK exit) in the COM2 state
          // (xbl.f:968): BLPRV/BLKIN with the updated AMI.
          prim = blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
          sec = blkin(prim, in.props);
        }
        primSaved = prim;
        secSaved = sec;
        // Fortran: TESYS or BLSYS
        BldifSystem sys{};
        if (ibl == m.iblte + 1) {
          // Fortran: separate IBLTE indices per page (xbl.f:640-643).
          const int iblteOther = is == 0 ? in.mapping.bottom.iblte
                                         : in.mapping.top.iblte;
          const double tte =
              thetS[sz(m.iblte)] +
              (is == 0 ? in.thet.bottom[sz(iblteOther)]
                       : in.thet.top[sz(iblteOther)]);
          const double dte =
              dstrS[sz(m.iblte)] +
              (is == 0 ? in.dstr.bottom[sz(iblteOther)]
                       : in.dstr.top[sz(iblteOther)]) +
              in.ante;
          const double thetOther = is == 0 ? in.thet.bottom[sz(iblteOther)]
                                           : in.thet.top[sz(iblteOther)];
          const double cteOther =
              is == 0 ? in.ctau.bottom[sz(iblteOther)]
                      : in.ctau.top[sz(iblteOther)];
          const double cte =
              (ctauS[sz(m.iblte)] * thetS[sz(m.iblte)] +
               cteOther * thetOther) /
              (thetS[sz(m.iblte)] + thetOther);
          tteSt = tte;
          dteSt = dte;
          cteSt = cte;
          TesysInput teIn{};
          teIn.wake.primary = prim;
          teIn.wake.secondary = sec;
          teIn.wake.blpar = in.blpar;
          teIn.cte = cte;
          teIn.tte = tte;
          teIn.dte = dte;
          const auto te = tesys(teIn);
          sys = te.system;
          tsSaved = te.wakeSecondary;  // Fortran BLVAR(3) state
        } else {
          BlsysInput bi{};
          bi.first = st.com1;
          bi.primary2 = prim;
          bi.secondary2 = sec;
          bi.wake = wake;
          bi.turb = st.turb;
          bi.tran = st.tran;
          bi.simi = simi;
          bi.ueEi1 = st.ueEi1;
          bi.ueMs1 = st.ueMs1;
          bi.ueEi2 = prim.ueEi;
          bi.ueMs2 = prim.ueMs;
          bi.amcrit = amcrit;
          bi.idampv = in.props.idampv;
          bi.bule = 1.0;
          bi.xt = st.xt;
          bi.xtA1 = st.xtA1;
          bi.xtT1 = st.xtT1;
          bi.xtD1 = st.xtD1;
          bi.xtU1 = st.xtU1;
          bi.xtX1 = st.xtX1;
          bi.xtT2 = st.xtT2;
          bi.xtD2 = st.xtD2;
          bi.xtU2 = st.xtU2;
          bi.xtX2 = st.xtX2;
          bi.xtMs = st.xtMs;
          bi.xtRe = st.xtRe;
          bi.xtXf = st.xtXf;
          bi.props = in.props;
          bi.blpar = in.blpar;
          sys = blsys(bi, tsSaved);
        }
        if (simi) {
          // Fortran BLSYS: on the similarity station the "1" and
          // "2" variables are identical; COMMON COM1 is overwritten
          // (xblsys.f:617-621) and affects HTARG/HK1 consumers in
          // the same and following Newton iterations.
          st.com1 = makeCom(prim, sec, tsSaved);
          st.com1.hk = tsSaved.hk;
          st.ueEi1 = prim.ueEi;
          st.ueMs1 = prim.ueMs;
        }
        // Fortran: baseline Ue/Hk in the first iteration after BLSYS
        // (xbl.f:991-1009); HKREF = HK2 after the BLVAR clamp
        // (xblsys.f:799). UEREF/HKREF persist through all Newton
        // iterations for the station (no Re initialization).
        if (itbl == 1) {
          ueref = prim.ue;     // Fortran U2
          hkref = tsSaved.hk;  // Fortran HK2 (clamped)
          if (ibl < in.itran[sz(is)] && ibl >= itrold) {
            // Fortran: baseline HK extrapolated upstream
            const double uem = uedgS[sz(ibl - 1)];
            const double dsm = dstrS[sz(ibl - 1)];
            const double thm = thetS[sz(ibl - 1)];
            const double msq =
                uem * uem * hstinv / (gm1bl * (1.0 - 0.5 * uem * uem * hstinv));
            hkref = hkin(dsm / thm, msq).value;
          }
          if (ibl < itrold) {
            if (st.tran) ctauS[sz(ibl)] = 0.03;
            if (st.turb) ctauS[sz(ibl)] = ctauS[sz(ibl - 1)];
            if (st.tran || st.turb) {
              cti = ctauS[sz(ibl)];
            }
          }
        }
        auto vs2mat = std::array<std::array<double, 5>, 4>{};
        auto vsrez4 = std::array<double, 4>{};
        for (int k = 0; k < 4; ++k) {
          for (int l = 0; l < 5; ++l) {
            vs2mat[sz(k)][sz(l)] = sys.vs2[sz(k)][sz(l)];
          }
          vsrez4[sz(k)] = sys.vsrez[sz(k)];
        }
        // Fortran: similarity station or first wake point -> prescribe Ue,
        // otherwise use the Ue-Hk characteristic
        if (simi || ibl == m.iblte + 1) {
          vs2mat[3][0] = 0.0;
          vs2mat[3][1] = 0.0;
          vs2mat[3][2] = 0.0;
          vs2mat[3][3] = prim.ueEi;
          vsrez4[3] = ueref - prim.ue;
        } else {
          auto vtmp = std::array<std::array<double, 5>, 4>{};
          auto vztmp = std::array<double, 4>{};
          for (int k = 0; k < 4; ++k) {
            vztmp[sz(k)] = vsrez4[sz(k)];
            for (int l = 0; l < 5; ++l) {
              vtmp[sz(k)][sz(l)] = vs2mat[sz(k)][sz(l)];
            }
          }
          // Fortran: unit dHk
          vtmp[3][0] = 0.0;
          vtmp[3][1] = sec.hkT;
          vtmp[3][2] = sec.hkD;
          vtmp[3][3] = sec.hkUe * prim.ueEi;
          vztmp[3] = 1.0;
          gauss4(vtmp, vztmp);
          // Fortran: SENSWT * (normalized dUe/dHk)
          sennew = senswt * vztmp[3] * hkref / ueref;
          if (itbl <= 5) {
            sens = sennew;
          } else if (itbl <= 15) {
            sens = 0.5 * (sens + sennew);
          }
          // Fortran: prescribed Ue-Hk combination
          vs2mat[3][0] = 0.0;
          vs2mat[3][1] = sec.hkT * hkref;
          vs2mat[3][2] = sec.hkD * hkref;
          vs2mat[3][3] = (sec.hkUe * hkref + sens / ueref) * prim.ueEi;
          vsrez4[3] = -(hkref * hkref) * (tsSaved.hk / hkref - 1.0) -
                      sens * (prim.ue / ueref - 1.0);
        }
        gauss4(vs2mat, vsrez4);
        // Fortran: maximum changes and under-relaxation
        dmax = fortranMax3(std::abs(vsrez4[1] / thi), std::abs(vsrez4[2] / dsi),
                           std::abs(vsrez4[3] / uei));
        if (ibl >= in.itran[sz(is)]) {
          dmax = fortranMax(dmax, std::abs(vsrez4[0] / (10.0 * cti)));
        }
        double rlx = 1.0;
        if (dmax > 0.3) rlx = 0.3 / dmax;
        // Fortran: update
        if (ibl < in.itran[sz(is)]) ami = ami + rlx * vsrez4[0];
        if (ibl >= in.itran[sz(is)]) cti = cti + rlx * vsrez4[0];
        thi = thi + rlx * vsrez4[1];
        dsi = dsi + rlx * vsrez4[2];
        uei = uei + rlx * vsrez4[3];
        // Fortran: absurd transients
        if (ibl >= in.itran[sz(is)]) {
          cti = fortranMin(cti, 0.30);
          cti = fortranMax(cti, 0.0000001);
        }
        const double hklim = ibl <= m.iblte ? 1.02 : 1.00005;
        const double msq2 =
            uei * uei * hstinv / (gm1bl * (1.0 - 0.5 * uei * uei * hstinv));
        dsi = dslim(dsi - dswaki, thi, msq2, hklim) + dswaki;
        if (dmax <= deps) {
          converged = true;
          break;
        }
      }
      if (!converged) {
        diag.converged = false;
        diag.failedStation = ibl;
        diag.failedSide = is;
        diag.failedResidual = dmax;
        // Fortran: WRITE(*,1350) IBL, IS, DMAX (xbl.f:1102, MRCHDU).
        diag.events.push_back(ViscousEventRecord{
            ViscousEventKind::mrchduConvergenceFailure,
            ViscousEventPhase::postMarch, 0, is, ibl, 0.0, dmax, 0.0, -1});
        // Fortran: xbl.f:1107 IBL.GT.3 1-based -> 0-based ibl > 2.
        if (dmax > 0.1 && ibl > 2) {
          if (ibl <= m.iblte) {
            thi = thetS[sz(ibl - 1)] *
                  std::pow(xssiS[sz(ibl)] / xssiS[sz(ibl - 1)], 0.5);
            dsi = dstrS[sz(ibl - 1)] *
                  std::pow(xssiS[sz(ibl)] / xssiS[sz(ibl - 1)], 0.5);
            uei = uedgS[sz(ibl - 1)];
          } else if (ibl == m.iblte + 1) {
            cti = cteSt;  // Fortran CTI=CTE (xbl.f:1117)
            thi = tteSt;  // Fortran THI=TTE
            dsi = dteSt;  // Fortran DSI=DTE
            uei = uedgS[sz(ibl - 1)];
          } else {
            thi = thetS[sz(ibl - 1)];
            const double ratlen =
                (xssiS[sz(ibl)] - xssiS[sz(ibl - 1)]) /
                (10.0 * dstrS[sz(ibl - 1)]);
            dsi = (dstrS[sz(ibl - 1)] + thi * ratlen) / (1.0 + ratlen);
            uei = uedgS[sz(ibl - 1)];
          }
          if (ibl == in.itran[sz(is)]) cti = 0.05;
          if (ibl > in.itran[sz(is)]) cti = ctauS[sz(ibl - 1)];
        }
      }
      sens = sennew;
      // Fortran: converged path (GO TO 110, xbl.f:830): only
      // BLPRV/BLKIN again; the BLVAR fields (HS/HC/US/CQ/CF/DI/DE)
      // from the last BLSYS assembly are kept (xbl.f:839-843,
      // COM1=COM2 mixed, xbl.f:848-850). Fallback path (Label 109):
      // BLVAR/BLMID again.
      BoundaryLayerPrimary primS =
          blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
      BoundaryLayerSecondary secS = blkin(primS, in.props);
      // Fortran: Label 109 (xbl.f:1139-1145) — only on the non-converged
      // path: TRCHEK with AMPL2 update and ITRAN rewriting.
      if (!converged && !simi && !st.turb) {
        TrchekInput ti{};
        ti.first = makeTrchekStation(st.com1);
        ti.second = makeTrchekStation(makeCom(primS, secS, {}));
        ti.xiforc = in.xiforc[sz(is)];
        ti.acrit = amcrit;
        ti.idampv = in.props.idampv;
        ti.hstinv = in.props.hstinv;
        ti.gm1bl = in.props.gm1bl;
        ti.rstbl = in.props.rstbl;
        ti.rstblMs = in.props.rstblMs;
        ti.hstinvMs = in.props.hstinvMs;
        ti.reybl = in.props.reybl;
        ti.reyblMs = in.props.reyblMs;
        ti.reyblRe = in.props.reyblRe;
        ti.hvrat = in.props.hvrat;
        const auto tr = trchek2(ti);
        pushTrchek2Failure(diag.events, ViscousEventPhase::postMarch, is, ibl, tr);
        ami = tr.ampl;
        st.xt = tr.xt;
        st.xtA1 = tr.xtA1;
        st.xtT1 = tr.xtT1;
        st.xtD1 = tr.xtD1;
        st.xtU1 = tr.xtU1;
        st.xtX1 = tr.xtX1;
        st.xtT2 = tr.xtT2;
        st.xtD2 = tr.xtD2;
        st.xtU2 = tr.xtU2;
        st.xtX2 = tr.xtX2;
        st.xtMs = tr.xtMs;
        st.xtRe = tr.xtRe;
        st.xtXf = tr.xtXf;
        st.tran = tr.tran;
        st.trforc = tr.trforc;
        if (tr.tran) {
          in.itran[sz(is)] = ibl;
        } else {
          in.itran[sz(is)] = ibl + 2;
        }
        // Fortran: TRCHEK sets AMPL2 in the COMMON (xbl.f:1141) — the
        // "2"-station amplification follows the updated AMI (as in the
        // Newton loop via BLPRV/BLKIN with the updated AMI).
        const BoundaryLayerPrimary primTr =
            blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.props);
        secS = blkin(primTr, in.props);
        primS = primTr;
      }
      BoundaryLayerTurbulentSecondary tsUse{};
      if (converged) {
        tsUse = tsSaved;
      } else {
        tsUse = fallbackBlvar(wake, ibl, in.itran[sz(is)], primS, secS,
                              in.blpar);
      }
      BldifStation com2 = makeCom(primS, secS, tsUse);
      // Fortran: MRCHDU/MRCHUE Label 110 calls BLPRV/BLKIN again before
      // COM1=COM2; COM1-HK2 is the unclamped HKIN value there,
      // also in the fallback path Label 109 (xbl.f:1131-1174).
      // Fortran: TAU/DIS/CTQ/DELT/TSTR (xbl.f:1160-1166) use the primary
      // values from before the update in the converged path, and the fresh
      // Label-109 values in the fallback path.
      const BoundaryLayerPrimary& primUse = converged ? primSaved : primS;
      const BoundaryLayerSecondary& secUse = converged ? secSaved : secS;
      ctauS[sz(ibl)] = ibl < in.itran[sz(is)] ? ami : cti;
      thetS[sz(ibl)] = thi;
      dstrS[sz(ibl)] = dsi;
      uedgS[sz(ibl)] = uei;
      massS[sz(ibl)] = dsi * uei;
      if (is == 0) {
        in.tau.top[sz(ibl)] =
            0.5 * secUse.r * primUse.ue * primUse.ue * tsUse.cf;
        in.dis.top[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.ue * tsUse.di *
            tsUse.hs * 0.5;
        in.ctq.top[sz(ibl)] = tsUse.cq;
        in.delt.top[sz(ibl)] = tsUse.de;
        in.tstr.top[sz(ibl)] = tsUse.hs * primUse.t;
        in.dssr.top[sz(ibl)] = tsUse.hc * primUse.t;
        in.mdef.top[sz(ibl)] = secUse.r * primUse.ue * primUse.d;
        in.pdef.top[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.t;
        in.kdef.top[sz(ibl)] = secUse.r * primUse.ue * primUse.ue *
                               primUse.ue * primUse.t * tsUse.hs * 0.5;
      } else {
        in.tau.bottom[sz(ibl)] =
            0.5 * secUse.r * primUse.ue * primUse.ue * tsUse.cf;
        in.dis.bottom[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.ue * tsUse.di *
            tsUse.hs * 0.5;
        in.ctq.bottom[sz(ibl)] = tsUse.cq;
        in.delt.bottom[sz(ibl)] = tsUse.de;
        in.tstr.bottom[sz(ibl)] = tsUse.hs * primUse.t;
        in.dssr.bottom[sz(ibl)] = tsUse.hc * primUse.t;
        in.mdef.bottom[sz(ibl)] = secUse.r * primUse.ue * primUse.d;
        in.pdef.bottom[sz(ibl)] =
            secUse.r * primUse.ue * primUse.ue * primUse.t;
        in.kdef.bottom[sz(ibl)] = secUse.r * primUse.ue * primUse.ue *
                                  primUse.ue * primUse.t * tsUse.hs * 0.5;
      }



      st.com1 = com2;
      st.ueEi1 = primS.ueEi;
      st.ueMs1 = primS.ueMs;
      st.ampl1 = ami;
      if (st.tran || ibl == m.iblte) {
        st.turb = true;
        in.tforce[sz(is)] = st.trforc;
        in.xssitr[sz(is)] = st.xt;
      }
      st.tran = false;
    }
  }
  return diag;
}


SetblDiagnostics setbl(SetblInput& in, std::array<MarchSideState, 2>& state) {
  SetblDiagnostics diag{};
  // Fortran XOCTR/ITRAN/TFORCE/XSSITR are COMMON and are retained
  // if no transition occurs in this SETBL (xbl.f:502-509,
  // xfoil.f:677-680 initial values).
  diag.tforce = in.march.tforce;
  diag.xoctr = in.march.xoctr;
  diag.itran = in.march.itran;
  diag.xssitr = in.march.xssitr;

  const BoundaryLayerSideMapping& mTop = in.mapping.top;
  const BoundaryLayerSideMapping& mBot = in.mapping.bottom;
  const int nbl1 = mTop.nbl;
  const int nbl2 = mBot.nbl;
  const int iblte1 = mTop.iblte;
  const int iblte2 = mBot.iblte;
  const int nsys = in.nsys;

  // Fortran: ISYS/NBL/IS validation before each mutation.
  if (nsys <= 0) {
    throw std::invalid_argument("setbl: NSYS <= 0");
  }
  if (nbl1 < 1 || nbl2 < 1) {
    throw std::invalid_argument("setbl: NBL < 1");
  }
  for (int is = 0; is < 2; ++is) {
    const BoundaryLayerSideMapping& m =
        is == 0 ? mTop : mBot;
    if (sz(m.nbl) > in.isys[sz(is)].size()) {
      throw std::invalid_argument("setbl: ISYS too small");
    }
    if (sz(m.nbl) > in.march.xssi[sz(is)].size()) {
      throw std::invalid_argument("setbl: XSSI too small");
    }
  }
  for (const std::span<const double> span :
       {std::span<const double>(in.xPan), std::span<const double>(in.xpPan),
        std::span<const double>(in.yPan), std::span<const double>(in.ypPan),
        std::span<const double>(in.sPan)}) {
    if (span.size() < sz(in.nPan)) {
      throw std::invalid_argument("setbl: panel geometry too small");
    }
  }

  // Fortran: dij access with check (row-major, i*dijStride + j).
  auto dijAt = [&](int i, int j) -> double {
    const std::size_t idx = sz(i) * sz(in.dijStride) + sz(j);
    if (idx >= in.dij.size()) {
      throw std::invalid_argument("setbl: DIJ index outside");
    }
    return in.dij[idx];
  };

  // Fortran: CLMR = Cl (LALFA) or CLSPEC (xbl.f:39-43).
  const double clmr = in.lalfa ? in.cl : in.clspec;
  // Fortran: CALL MRCL(CLMR,MA_CLMR,Re_CLMR) (xbl.f:46).
  const inviscid::MachReynoldsSettings mrSettings{in.machType, in.reType,
                                                  in.minf1, in.reinf1};
  const auto mrRes = inviscid::mrcl(mrSettings, clmr);
  if (mrRes.isErr()) {
    throw std::invalid_argument("setbl: MRCL inputs invalid");
  }
  in.minf = mrRes.value().minf;    // Fortran MINF (MRCL session state)
  in.reinf = mrRes.value().reinf;  // Fortran REINF
  const double maClmr = mrRes.value().mCls;
  const double reClmr = mrRes.value().rCls;
  const double msqClmr = 2.0 * in.minf * maClmr;

  // Fortran: CALL COMSET (xbl.f:50) — TKLAM/TK_MSQ come from the caller
 // (comset); GAMBL/GM1BL/QINFBL/TKBL/RSTBL/HSTINV/REYBL data share
  // (xbl.f:53-77) via boundaryLayerProperties.
  // IDAMPV = IDAMP (xbl.f:77) at the canonical producer location;
  // the fresh session/input value (SetblInput.idamp) replaces the
  // previous props-threshold value — bit-identical with default 0.
  if (in.march.wgap.empty()) {
    throw std::invalid_argument("setbl: WGAP empty");
  }
  const BoundaryLayerProperties props = boundaryLayerProperties(
      in.minf, in.reinf, in.qinf, in.gamma, in.gamm1, in.tklam, in.tkMsq,
      in.hvrat, in.march.wgap[0], in.idamp);
  in.march.props = props;

  // Fortran: DWTE = WGAP(1) (xbl.f:80) — props.dwte; the BL marching
  // routines do not need DWTE (dswaki is passed per station).

  // Fortran: MRCHUE for LBLINI=FALSE, afterwards MRCHDU (xbl.f:82-93).
  if (!in.lblini) {
    // Fortran: WRITE(*,*) 'Initializing BL ...' (xbl.f:85).
    diag.events.push_back(ViscousEventRecord{
        ViscousEventKind::boundaryLayerInitialization,
        ViscousEventPhase::initMarch, 0, -1, -1, 0.0, 0.0, 0.0, -1});
    MarchDiagnostics diagMrchue = mrchue(in.march, state);
    diag.events.insert(diag.events.end(), diagMrchue.events.begin(),
                       diagMrchue.events.end());
    in.lblini = true;
    MarchDiagnostics diagMrchdu = mrchdu(in.march, state);
    diag.marchDiagnostics = diagMrchdu;
    diag.events.insert(diag.events.end(), diagMrchdu.events.begin(),
                       diagMrchdu.events.end());
    // MRCHDU does not set lastInverse (only MRCHUE does). The Fortran
    // prints the 'Inverse mode' message in both marches; for
    // diagnostics the last MRCHUE value is kept (xbl.f:724).
    if (diag.marchDiagnostics.lastInverseStation < 0) {
      diag.marchDiagnostics.lastInverseStation = diagMrchue.lastInverseStation;
      diag.marchDiagnostics.lastInverseHk = diagMrchue.lastInverseHk;
    }
  } else {
    MarchDiagnostics diagMrchdu = mrchdu(in.march, state);
    diag.marchDiagnostics = diagMrchdu;
    diag.events.insert(diag.events.end(), diagMrchdu.events.begin(),
                       diagMrchdu.events.end());
  }

  auto sideSpan = [](BoundaryLayerStationField& f, int is) {
    return std::span<double>(is == 0 ? f.top : f.bottom);
  };
  auto sideSpanConst = [](const BoundaryLayerStationField& f, int is) {
    return std::span<const double>(is == 0 ? f.top : f.bottom);
  };
  const std::array<const BoundaryLayerSideMapping*, 2> sides{&mTop, &mBot};

  // Fortran: USAV store (xbl.f:95-99) — IBL=2..NBL 1-based ->
  // 0-based 1..nbl inclusive.
  std::array<std::vector<double>, 2> usav{};
  for (int is = 0; is < 2; ++is) {
    const auto uedgS = sideSpanConst(in.march.uedg, is);
    usav[sz(is)].assign(sz(sides[sz(is)]->nbl) + 1, 0.0);
    for (int ibl = 1; ibl <= sides[sz(is)]->nbl; ++ibl) {
      usav[sz(is)][sz(ibl)] = uedgS[sz(ibl)];
    }
  }

  // Fortran: CALL UESET (xbl.f:101) — UEDG = UINV + dij*Mass source term.
  ueset(in.mapping, in.dij, in.dijStride, in.uinv, in.march.mass,
        in.march.uedg);

  // Fortran: USAV <-> UEDG swap (xbl.f:103-109) — afterwards
  // USAV = UINV+dij*MASS and UEDG is the march value.
  for (int is = 0; is < 2; ++is) {
    auto uedgS = sideSpan(in.march.uedg, is);
    for (int ibl = 1; ibl <= sides[sz(is)]->nbl; ++ibl) {
      const double temp = usav[sz(is)][sz(ibl)];
      usav[sz(is)][sz(ibl)] = uedgS[sz(ibl)];
      uedgS[sz(ibl)] = temp;
    }
  }

  // Fortran: ILE1/ILE2/ITE1/ITE2/JVTE1/JVTE2 (xbl.f:111-117).
  const int ile1 = mTop.ipan[1];
  const int ile2 = mBot.ipan[1];
  const int ite1 = mTop.ipan[sz(iblte1)];
  const int ite2 = mBot.ipan[sz(iblte2)];
  const int jvte1 = in.isys[0][sz(iblte1)];
  const int jvte2 = in.isys[1][sz(iblte2)];

  // Fortran: DULE1/DULE2 (xbl.f:119-120).
  const double dule1 = sideSpanConst(in.march.uedg, 0)[1] - usav[0][1];
  const double dule2 = sideSpanConst(in.march.uedg, 1)[1] - usav[1][1];

  // Fortran: LE/TE Ue sensitivities wrt all m (xbl.f:123-132).
  std::vector<double> ule1M(sz(nsys), 0.0);
  std::vector<double> ule2M(sz(nsys), 0.0);
  std::vector<double> ute1M(sz(nsys), 0.0);
  std::vector<double> ute2M(sz(nsys), 0.0);
  const double vtiTop2 = mTop.vti[1];
  const double vtiBot2 = mBot.vti[1];
  const double vtiTopTe = mTop.vti[sz(iblte1)];
  const double vtiBotTe = mBot.vti[sz(iblte2)];
  for (int js = 0; js < 2; ++js) {
    const BoundaryLayerSideMapping& ms = *sides[sz(js)];
    for (int jbl = 1; jbl <= ms.nbl; ++jbl) {
      const int j = ms.ipan[sz(jbl)];
      const int jv = in.isys[sz(js)][sz(jbl)];
      ule1M[sz(jv)] = -vtiTop2 * ms.vti[sz(jbl)] * dijAt(ile1, j);
      ule2M[sz(jv)] = -vtiBot2 * ms.vti[sz(jbl)] * dijAt(ile2, j);
      ute1M[sz(jv)] = -vtiTopTe * ms.vti[sz(jbl)] * dijAt(ite1, j);
      ute2M[sz(jv)] = -vtiBotTe * ms.vti[sz(jbl)] * dijAt(ite2, j);
    }
  }

  // Fortran: ULE1_A/ULE2_A (xbl.f:134-135); TINDEX zeros (xbl.f:137-138).
  const double ule1a = sideSpanConst(in.uinvA, 0)[1];
  const double ule2a = sideSpanConst(in.uinvA, 1)[1];
  diag.tindex[0] = 0.0;
  diag.tindex[1] = 0.0;

  // Fortran: USLP exit size (plot exit, per page nbl+1,
  // including the stagnation-point station).
  diag.uslp.top.assign(sz(nbl1) + 1, 0.0);
  diag.uslp.bottom.assign(sz(nbl2) + 1, 0.0);

  // Fortran: Newton workspace size (VM/VB/VA/VDEL/VZ).
  BlsysWorkspace& ws = in.ws;
  ws.nsys = nsys;
  ws.va.assign(sz(nsys), std::array<std::array<double, 2>, 3>{});
  ws.vb.assign(sz(nsys), std::array<std::array<double, 2>, 3>{});
  ws.vdel.assign(sz(nsys), std::array<std::array<double, 2>, 3>{});
  ws.vm.assign(sz(nsys),
               std::vector<std::array<double, 3>>(sz(nsys),
                                                  std::array<double, 3>{}));

  // Fortran: U1_M/U2_M/D1_M/D2_M chains (xbl.f:32-33).
  std::vector<double> u1M(sz(nsys), 0.0);
  std::vector<double> d1M(sz(nsys), 0.0);
  std::vector<double> u2M(sz(nsys), 0.0);
  std::vector<double> d2M(sz(nsys), 0.0);

  // Fortran: sweep over each BL/wake (xbl.f:141-513).
  for (int is = 0; is < 2; ++is) {
    const BoundaryLayerSideMapping& m = *sides[sz(is)];
    const auto xssiS = std::span<const double>(in.march.xssi[sz(is)]);
    const auto thetS = sideSpan(in.march.thet, is);
    const auto ctauS = sideSpan(in.march.ctau, is);
    const auto uedgS = sideSpan(in.march.uedg, is);
    const auto massS = sideSpanConst(in.march.mass, is);
    const auto uinvAS = sideSpanConst(in.uinvA, is);
    MarchSideState& st = state[sz(is)];

    // Fortran: zero the "1" station similarity values (xbl.f:144-155).
    std::fill(u1M.begin(), u1M.end(), 0.0);
    std::fill(d1M.begin(), d1M.end(), 0.0);
    double u1a = 0.0;
    double d1a = 0.0;
    double due1 = 0.0;
    double dds1 = 0.0;

    // Fortran: BULE=1.0, AMCRIT=ACRIT(IS), XIFSET(IS) (xbl.f:158-166).
    const double bule = 1.0;
    const double amcrit = in.march.acrit[sz(is)];
    const double xiforc = in.march.xiforc[sz(is)];
    bool tran = false;
    bool turb = false;
    bool wake = false;
    bool trforc = false;
    double ami = 0.0;  // Fortran AMI (local variable, as in Fortran)

    for (int ibl = 1; ibl <= m.nbl; ++ibl) {
      const int iv = in.isys[sz(is)][sz(ibl)];
      const bool simi = ibl == 1;
      wake = ibl > m.iblte;
      tran = ibl == in.march.itran[sz(is)];
      turb = ibl > in.march.itran[sz(is)];
      const int i = m.ipan[sz(ibl)];

      // Fortran: primary variables of the current station (xbl.f:182-196).
      const double xsi = xssiS[sz(ibl)];
      double cti = 0.0;
      if (ibl < in.march.itran[sz(is)]) {
        ami = ctauS[sz(ibl)];
      }
      if (ibl >= in.march.itran[sz(is)]) {
        cti = ctauS[sz(ibl)];
      }
      const double uei = uedgS[sz(ibl)];
      const double thi = thetS[sz(ibl)];
      const double mdi = massS[sz(ibl)];
      const double dsi = mdi / uei;
      double dswaki = 0.0;
      if (wake) {
        // Fortran IW = IBL-IBLTE(IS) (1-based) -> 0-based iw = IW-1.
        const std::size_t iw = sz(ibl - m.iblte - 1);
        if (iw >= in.march.wgap.size()) {
          throw std::invalid_argument("setbl: WGAP index outside");
        }
        dswaki = in.march.wgap[iw];
      }

      // Fortran: DSI derivatives D2_M2/D2_U2 (xbl.f:199-200).
      const double d2m2 = 1.0 / uei;
      const double d2u2 = -dsi / uei;

      // Fortran: U2_M/D2_M chains over all stations (xbl.f:202-210).
      std::fill(u2M.begin(), u2M.end(), 0.0);
      std::fill(d2M.begin(), d2M.end(), 0.0);
      for (int js = 0; js < 2; ++js) {
        const BoundaryLayerSideMapping& ms = *sides[sz(js)];
        for (int jbl = 1; jbl <= ms.nbl; ++jbl) {
          const int j = ms.ipan[sz(jbl)];
          const int jv = in.isys[sz(js)][sz(jbl)];
          u2M[sz(jv)] = -m.vti[sz(ibl)] * ms.vti[sz(jbl)] * dijAt(i, j);
          d2M[sz(jv)] = d2u2 * u2M[sz(jv)];
        }
      }
      d2M[sz(iv)] += d2m2;

      const double u2a = uinvAS[sz(ibl)];
      const double d2a = d2u2 * u2a;

      // Fortran: forced changes UEDG-USAV (xbl.f:216-217).
      const double due2 = uedgS[sz(ibl)] - usav[sz(is)][sz(ibl)];
      const double dds2 = d2u2 * due2;

      // Fortran: CALL BLPRV/BLKIN (xbl.f:219-220).
      const BoundaryLayerPrimary prim2 =
          blprv(xsi, ami, cti, thi, dsi, dswaki, uei, in.march.props);
      const BoundaryLayerSecondary sec2 = blkin(prim2, in.march.props);
      const BlvarInput vin{prim2, sec2, in.march.blpar};

      // Fortran: TRCHEK when TRAN is set (xbl.f:223-226).
      double trAmpl2 = prim2.ampl;  // Fortran AMPL2 (BLPRV input)
      if (tran) {
        TrchekInput ti{};
        ti.first = makeTrchekStation(st.com1);
        ti.second = makeTrchekStation(makeCom(prim2, sec2, {}));
        ti.xiforc = xiforc;
        ti.acrit = amcrit;
        ti.idampv = in.march.props.idampv;
        ti.hstinv = in.march.props.hstinv;
        ti.gm1bl = in.march.props.gm1bl;
        ti.rstbl = in.march.props.rstbl;
        ti.rstblMs = in.march.props.rstblMs;
        ti.hstinvMs = in.march.props.hstinvMs;
        ti.reybl = in.march.props.reybl;
        ti.reyblMs = in.march.props.reyblMs;
        ti.reyblRe = in.march.props.reyblRe;
        ti.hvrat = in.march.props.hvrat;
        const auto tr = trchek2(ti);
        pushTrchek2Failure(diag.events, ViscousEventPhase::postMarch, is, ibl, tr);
        tran = tr.tran;
        trforc = tr.trforc;
        trAmpl2 = tr.ampl;
        ami = tr.ampl;  // Fortran AMI = AMPL2
        st.xt = tr.xt;
        st.xtA1 = tr.xtA1;
        st.xtT1 = tr.xtT1;
        st.xtD1 = tr.xtD1;
        st.xtU1 = tr.xtU1;
        st.xtX1 = tr.xtX1;
        st.xtT2 = tr.xtT2;
        st.xtD2 = tr.xtD2;
        st.xtU2 = tr.xtU2;
        st.xtX2 = tr.xtX2;
        st.xtMs = tr.xtMs;
        st.xtRe = tr.xtRe;
        st.xtXf = tr.xtXf;
      }
      if (ibl == in.march.itran[sz(is)] && !tran) {
        // Fortran: 'SETBL: Xtr??? n1 n2' (xbl.f:228) as Diagnostics.
        diag.xtrWarning = true;
        diag.xtrAmpl1 = st.com1.ampl;
        diag.xtrAmpl2 = trAmpl2;
        ViscousEventRecord ev{};
        ev.kind = ViscousEventKind::setblXtrWarning;
        ev.phase = ViscousEventPhase::postMarch;
        ev.iteration = 0;
        ev.side = is;
        ev.station = ibl;
        ev.trchekAmpl1 = st.com1.ampl;
        ev.trchekAmpl2 = trAmpl2;
        diag.events.push_back(ev);
      }

      // Fortran: TESYS or BLSYS (xbl.f:234-274).
      BldifSystem sys{};
      BoundaryLayerTurbulentSecondary ts{};
      double tteTte1 = 0.0;
      double tteTte2 = 0.0;
      double dteMte1 = 0.0;
      double dteUte1 = 0.0;
      double dteMte2 = 0.0;
      double dteUte2 = 0.0;
      double cteCte1 = 0.0;
      double cteCte2 = 0.0;
      double cteTte1 = 0.0;
      double cteTte2 = 0.0;
      if (ibl == m.iblte + 1) {
        // Fortran: TTE/DTE/CTE at the wake start (xbl.f:237-252).
        const auto thetTop = sideSpanConst(in.march.thet, 0);
        const auto thetBot = sideSpanConst(in.march.thet, 1);
        const auto dstrTop = sideSpanConst(in.march.dstr, 0);
        const auto dstrBot = sideSpanConst(in.march.dstr, 1);
        const auto ctauTop = sideSpanConst(in.march.ctau, 0);
        const auto ctauBot = sideSpanConst(in.march.ctau, 1);
        const auto uedgTop = sideSpanConst(in.march.uedg, 0);
        const auto uedgBot = sideSpanConst(in.march.uedg, 1);
        const double tte = thetTop[sz(iblte1)] + thetBot[sz(iblte2)];
        const double dte =
            dstrTop[sz(iblte1)] + dstrBot[sz(iblte2)] + in.march.ante;
        const double cte =
            (ctauTop[sz(iblte1)] * thetTop[sz(iblte1)] +
             ctauBot[sz(iblte2)] * thetBot[sz(iblte2)]) /
            tte;
        TesysInput teIn{};
        teIn.wake.primary = prim2;
        teIn.wake.secondary = sec2;
        teIn.wake.blpar = in.march.blpar;
        teIn.cte = cte;
        teIn.tte = tte;
        teIn.dte = dte;
          const auto te = tesys(teIn);
          sys = te.system;
        ts = te.wakeSecondary;

        tteTte1 = 1.0;
        tteTte2 = 1.0;
        dteMte1 = 1.0 / uedgTop[sz(iblte1)];
        dteUte1 = -dstrTop[sz(iblte1)] / uedgTop[sz(iblte1)];
        dteMte2 = 1.0 / uedgBot[sz(iblte2)];
        dteUte2 = -dstrBot[sz(iblte2)] / uedgBot[sz(iblte2)];
        cteCte1 = thetTop[sz(iblte1)] / tte;
        cteCte2 = thetBot[sz(iblte2)] / tte;
        cteTte1 = (ctauTop[sz(iblte1)] - cte) / tte;
        cteTte2 = (ctauBot[sz(iblte2)] - cte) / tte;

        // Fortran: D1 sensitivities with respect to m, newly defined (xbl.f:255-263).
        for (int js = 0; js < 2; ++js) {
          const BoundaryLayerSideMapping& ms = *sides[sz(js)];
          for (int jbl = 1; jbl <= ms.nbl; ++jbl) {
            const int jv = in.isys[sz(js)][sz(jbl)];
            d1M[sz(jv)] = dteUte1 * ute1M[sz(jv)] + dteUte2 * ute2M[sz(jv)];
          }
        }
        d1M[sz(jvte1)] += dteMte1;
        d1M[sz(jvte2)] += dteMte2;

        // Fortran: forced changes at the wake start (xbl.f:266-268).
        due1 = 0.0;
        dds1 = dteUte1 * (uedgTop[sz(iblte1)] - usav[0][sz(iblte1)]) +
               dteUte2 * (uedgBot[sz(iblte2)] - usav[1][sz(iblte2)]);
      } else {
        BlsysInput bi{};
        bi.first = st.com1;
        bi.primary2 = prim2;
        bi.secondary2 = sec2;
        bi.wake = wake;
        bi.turb = turb;
        bi.tran = tran;
        bi.simi = simi;
        bi.ueEi1 = st.ueEi1;
        bi.ueMs1 = st.ueMs1;
        bi.ueEi2 = prim2.ueEi;
        bi.ueMs2 = prim2.ueMs;
        bi.amcrit = amcrit;
        bi.idampv = in.march.props.idampv;
        bi.bule = bule;
        bi.xt = st.xt;
        bi.xtA1 = st.xtA1;
        bi.xtT1 = st.xtT1;
        bi.xtD1 = st.xtD1;
        bi.xtU1 = st.xtU1;
        bi.xtX1 = st.xtX1;
        bi.xtT2 = st.xtT2;
        bi.xtD2 = st.xtD2;
        bi.xtU2 = st.xtU2;
        bi.xtX2 = st.xtX2;
        bi.xtMs = st.xtMs;
        bi.xtRe = st.xtRe;
        bi.xtXf = st.xtXf;
        bi.props = in.march.props;
        bi.blpar = in.march.blpar;
        sys = blsys(bi);
        // (BLSYS calls BLVAR(ITYP) internally); setbl calls blvar with the same
        // ITYP selection (deterministic identical values).
        const StationType itypBlvar =
            wake ? StationType::kWake
                 : (turb || tran) ? StationType::kTurbulent
                                   : StationType::kLaminar;
        ts = blvar(itypBlvar, vin);
      }

      // Fortran: TAU/DIS/CTQ/DELT/USLP store (xbl.f:278-282).
      if (is == 0) {
        in.march.tau.top[sz(ibl)] =
            0.5 * sec2.r * prim2.ue * prim2.ue * ts.cf;
        in.march.dis.top[sz(ibl)] = sec2.r * prim2.ue * prim2.ue * prim2.ue *
                                    ts.di * ts.hs * 0.5;
        in.march.ctq.top[sz(ibl)] = ts.cq;
        in.march.delt.top[sz(ibl)] = ts.de;
        diag.uslp.top[sz(ibl)] = 1.60 / (1.0 + ts.us);
      } else {
        in.march.tau.bottom[sz(ibl)] =
            0.5 * sec2.r * prim2.ue * prim2.ue * ts.cf;
        in.march.dis.bottom[sz(ibl)] = sec2.r * prim2.ue * prim2.ue *
                                       prim2.ue * ts.di * ts.hs * 0.5;
        in.march.ctq.bottom[sz(ibl)] = ts.cq;
        in.march.delt.bottom[sz(ibl)] = ts.de;
        diag.uslp.bottom[sz(ibl)] = 1.60 / (1.0 + ts.us);
      }

      // Fortran: XI sensitivities with respect to LE Ue (xbl.f:318-324).
      double xiUle1 = 0.0;
      double xiUle2 = 0.0;
      if (is == 0) {
        xiUle1 = in.sstGo;
        xiUle2 = -in.sstGp;
      } else {
        xiUle1 = -in.sstGo;
        xiUle2 = in.sstGp;
      }

      // Fortran: VM/VB/VA/VDEL assembly, lines L=1..3
      // (xbl.f:328-415). Fortran VS1(L,3) -> vs1[l][2] etc.
      const std::size_t ivSz = sz(iv);
      for (int l = 0; l < 3; ++l) {
        const std::size_t ls = sz(l);
        const double vs15vs25vsx =
            sys.vs1[ls][4] + sys.vs2[ls][4] + sys.vsx[ls];
        for (int jv = 0; jv < nsys; ++jv) {
          const std::size_t jvs = sz(jv);
          // Fortran VM(K,JV,IV) (xbl.f:328-415): vm[line][column][line]
          // (layout as in the BLSOLV read case vmRow(line,l)[k]).
          ws.vm[ivSz][jvs][ls] =
              sys.vs1[ls][2] * d1M[jvs] + sys.vs1[ls][3] * u1M[jvs] +
              sys.vs2[ls][2] * d2M[jvs] + sys.vs2[ls][3] * u2M[jvs] +
              vs15vs25vsx * (xiUle1 * ule1M[jvs] + xiUle2 * ule2M[jvs]);
        }
        ws.vb[ivSz][ls][0] = sys.vs1[ls][0];
        ws.vb[ivSz][ls][1] = sys.vs1[ls][1];
        ws.va[ivSz][ls][0] = sys.vs2[ls][0];
        ws.va[ivSz][ls][1] = sys.vs2[ls][1];
        if (in.lalfa) {
          ws.vdel[ivSz][ls][1] = sys.vsr[ls] * reClmr + sys.vsm[ls] * msqClmr;
        } else {
          ws.vdel[ivSz][ls][1] =
              (sys.vs1[ls][3] * u1a + sys.vs1[ls][2] * d1a) +
              (sys.vs2[ls][3] * u2a + sys.vs2[ls][2] * d2a) +
              vs15vs25vsx * (xiUle1 * ule1a + xiUle2 * ule2a);
        }
        ws.vdel[ivSz][ls][0] =
            sys.vsrez[ls] +
            (sys.vs1[ls][3] * due1 + sys.vs1[ls][2] * dds1) +
            (sys.vs2[ls][3] * due2 + sys.vs2[ls][2] * dds2) +
            vs15vs25vsx * (xiUle1 * dule1 + xiUle2 * dule2);
      }

      // Fortran: TE coefficients for TTE/DTE (xbl.f:418-436).
      if (ibl == m.iblte + 1) {
        for (int l = 0; l < 3; ++l) {
          const std::size_t ls = sz(l);
          ws.vz[ls][0] = sys.vs1[ls][0] * cteCte1;
          ws.vz[ls][1] =
              sys.vs1[ls][0] * cteTte1 + sys.vs1[ls][1] * tteTte1;
          ws.vb[ivSz][ls][0] = sys.vs1[ls][0] * cteCte2;
          ws.vb[ivSz][ls][1] =
              sys.vs1[ls][0] * cteTte2 + sys.vs1[ls][1] * tteTte2;
        }
      }

      // Fortran: store transition position (xbl.f:439-461).
      if (tran) {
        turb = true;
        in.march.itran[sz(is)] = ibl;
        in.march.tforce[sz(is)] = trforc;
        in.march.xssitr[sz(is)] = st.xt;
        diag.tforce[sz(is)] = trforc;
        diag.itran[sz(is)] = ibl;
        diag.xssitr[sz(is)] = st.xt;
        // Fortran: SEVAL interpolation for XOCTR/YOCTR (xbl.f:449-460).
        const double str = is == 0 ? in.sst - st.xt : in.sst + st.xt;
        const double chx = in.xte - in.xle;
        const double chy = in.yte - in.yle;
        const double chsq = chx * chx + chy * chy;
        const double xtr = math::seval(str, in.xPan, in.xpPan, in.sPan);
        const double ytr = math::seval(str, in.yPan, in.ypPan, in.sPan);
        diag.xoctr[sz(is)] =
            ((xtr - in.xle) * chx + (ytr - in.yle) * chy) / chsq;
        diag.yoctr[sz(is)] =
            ((ytr - in.yle) * chx - (xtr - in.xle) * chy) / chsq;
        // Fortran XOCTR is COMMON and persists between points
        // (display-only); the retained value is written back so that
        // later SETBL calls see the same state.
        in.march.xoctr[sz(is)] = diag.xoctr[sz(is)];
      }
      tran = false;

      // Fortran: TE -> wake correlations for the next station
      // (xbl.f:465-472). BLMID(3) sets only the CFM common; no
      // state required.
      if (ibl == m.iblte) {
        turb = true;
        wake = true;
        ts = blvar(StationType::kWake, vin);
      }

      // Fortran: set "1" variables to "2" variables (xbl.f:474-486).
      std::copy(u2M.begin(), u2M.end(), u1M.begin());
      std::copy(d2M.begin(), d2M.end(), d1M.begin());
      u1a = u2a;
      d1a = d2a;
      due1 = due2;
      dds1 = dds2;

      // Fortran: TINDEX (xbl.f:488-494). Fortran Is (1-based) ->
      // in.is (0-based); Fortran ITRAN(IS) -> itran[is] (0-based).
      if (ibl == in.march.itran[sz(is)] && xsi > st.com1.x) {
        const double x1 = st.com1.x;
        const double x2 = xsi;
        if (is == 0) {
          diag.tindex[sz(is)] =
              static_cast<double>(in.ist - in.march.itran[sz(is)] + 3) -
              (st.xt - x1) / (x2 - x1);
        } else {
          diag.tindex[sz(is)] =
              static_cast<double>(in.ist + in.march.itran[sz(is)]) +
              (st.xt - x1) / (x2 - x1);
        }
      }

      // Fortran: COM1 = COM2 (xbl.f:497-499). AMPL2 is updated after
      // TRCHEK (TRAN) or is AMI (BLPRV input).
      BldifStation com2 = makeCom(prim2, sec2, ts);
      com2.ampl = ami;
      // Fortran: COM1/COM2 HK2 is COMMON after the BLVAR clamp (xblsys.f:799).
      com2.hk = ts.hk;
      st.com1 = com2;
      st.ueEi1 = prim2.ueEi;
      st.ueMs1 = prim2.ueMs;
      st.ampl1 = ami;
    }

    // Fortran: Label 1000, WRITE(*,9100/9200) per page at the end of the
    // side loop (xbl.f:502-509). Both lines are written per page and
    // per SETBL call independently of whether a transition occurred
    // in this iteration; XOCTR/ITRAN are COMMON values.
    diag.events.push_back(ViscousEventRecord{
        in.march.tforce[sz(is)] ? ViscousEventKind::transitionForced
                                : ViscousEventKind::transitionFree,
        ViscousEventPhase::postMarch, 0, is, -1, 0.0, 0.0,
        in.march.xoctr[sz(is)], in.march.itran[sz(is)]});
  }
  diag.lblini = in.lblini;
  diag.itran = in.march.itran;
  return diag;
}

}  // namespace xfoil::core::boundary_layer





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

// File: src/core/boundary_layer_solver/NewtonSystem.cpp
// Implementation of the IBLSYS/BLSYS/TESYS/BLSOLV kernels. Formula-faithful
// adoption from fortran-src/xbl.f, xblsys.f and
// xsolve.f; constants, branch conditions and the order of
// computations are unchanged.

#include "xfoil/core/boundary_layer/NewtonSystem.h"

#include <stdexcept>
#include <utility>

namespace xfoil::core::boundary_layer {

namespace {

// Fortran: BLKIN exit or BLDIF station -> BLMID inputs.
BlmidStation blmidStationOfSec(const BoundaryLayerSecondary& sec) {
  BlmidStation m{};
  m.hk = sec.hk;
  m.hkU = sec.hkUe;
  m.hkT = sec.hkT;
  m.hkD = sec.hkD;
  m.hkMs = sec.hkMs;
  m.rt = sec.rt;
  m.rtU = sec.rtUe;
  m.rtT = sec.rtT;
  m.rtMs = sec.rtMs;
  m.rtRe = sec.rtRe;
  m.m = sec.m;
  m.mU = sec.mUe;
  m.mMs = sec.mMs;
  return m;
}

BlmidStation blmidStationOfSt(const BldifStation& st) {
  BlmidStation m{};
  m.hk = st.hk;
  m.hkU = st.hkU;
  m.hkT = st.hkT;
  m.hkD = st.hkD;
  m.hkMs = st.hkMs;
  m.rt = st.rt;
  m.rtU = st.rtU;
  m.rtT = st.rtT;
  m.rtMs = st.rtMs;
  m.rtRe = st.rtRe;
  m.m = st.m;
  m.mU = st.mU;
  m.mMs = st.mMs;
  return m;
}

// Fortran: BLPRV/BLKIN/BLVAR exit -> complete BLDIF
// station state.
BldifStation bldifStationOfSec(const BoundaryLayerPrimary& prim,
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

}  // namespace

BlsysMapping iblsys(const std::array<int, 2>& nbl, int ivxLimit) {
  for (const int n : nbl) {
    if (n < 1) {
      throw std::invalid_argument("iblsys: nbl must be >= 1 ");
    }
  }
  BlsysMapping out{};
  int iv = 0;  // Fortran IV=1..NSYS -> C++ 0-based line IV-1
  for (int is = 0; is < 2; ++is) {
    const std::size_t side = static_cast<std::size_t>(is);
    const int nIs = nbl[side];
    // Fortran ISYS is (IVX,2) and covers the stations 1..NBL(IS)
    // (1-based) resp. 0..nbl (0-based, stagnation point unset).
    out.isys[side].assign(static_cast<std::size_t>(nIs) + 1, 0);
    // Fortran IBL=2..NBL(IS) -> C++ stations 1..nbl (inclusive)
    for (int ibl = 1; ibl <= nIs; ++ibl) {
      out.isys[side][static_cast<std::size_t>(ibl)] = iv;
      ++iv;
    }
  }
  out.nsys = iv;
  // Fortran: IF(NSYS.GT.2*IVX) STOP '*** IBLSYS: BL system array
  // overflow. ***' -> controlled exception (cpp-safety deviation).
  if (out.nsys > 2 * ivxLimit) {
    throw std::invalid_argument("iblsys: BL system array overflow");
  }
  return out;
}

TesysResult tesys(const TesysInput& in) {
  TesysResult out{};
  // Fortran: VSREZ/VSM/VSR/VSX/VS1/VS2 zeros (xblsys.f:672-681).
  BldifSystem& sys = out.system;
  // Fortran: CALL BLVAR(3) (xblsys.f:683) — updated the
  // secondary wake quantities of COM2.
  out.wakeSecondary = blvar(StationType::kWake, in.wake);
  // Fortran: VS1(1,1)=-1, VS2(1,1)=1, VSREZ(1)=CTE-S2 etc.
  sys.vs1[0][0] = -1.0;
  sys.vs2[0][0] = 1.0;
  sys.vsrez[0] = in.cte - in.wake.primary.s;
  sys.vs1[1][1] = -1.0;
  sys.vs2[1][1] = 1.0;
  sys.vsrez[1] = in.tte - in.wake.primary.t;
  sys.vs1[2][2] = -1.0;
  sys.vs2[2][2] = 1.0;
  sys.vsrez[2] = in.dte - in.wake.primary.d - in.wake.primary.dw;
  return out;
}

BldifSystem blsys(const BlsysInput& in) {
  BoundaryLayerTurbulentSecondary tsUnused{};
  return blsys(in, tsUnused);
}

BldifSystem blsys(const BlsysInput& in,
                  BoundaryLayerTurbulentSecondary& tsOut) {
  // Fortran: BLVAR/BLMID depending on WAKE/TURB/TRAN (xblsys.f:605-614).
  const StationType ityp = in.wake ? StationType::kWake
                           : (in.turb || in.tran) ? StationType::kTurbulent
                                                  : StationType::kLaminar;
  const BlvarInput v1{in.primary2, in.secondary2, in.blpar};
  const BoundaryLayerTurbulentSecondary ts2 = blvar(ityp, v1);
  tsOut = ts2;
  BlmidInput mi{};
  mi.first = blmidStationOfSt(in.first);
  mi.second = blmidStationOfSec(in.secondary2);
  // Fortran: BLMID reads COMMON HK2 after the BLVAR clamp (xblsys.f:799).
  mi.second.hk = ts2.hk;
  mi.simi = in.simi;
  mi.blpar = in.blpar;
  const MidpointSkinFriction mid = blmid(ityp, mi);

  // Fortran: SIMI -> COM1=COM2 (xblsys.f:617-621). In C++ local copies.
  BoundaryLayerPrimary prim1 = in.primary2;
  BoundaryLayerSecondary sec1 = in.secondary2;
  BoundaryLayerTurbulentSecondary ts1 = ts2;
  BldifStation st1 = in.first;
  if (in.simi) {
    prim1 = in.primary2;
    sec1 = in.secondary2;
    ts1 = ts2;
    st1 = bldifStationOfSec(prim1, sec1, ts1);
    st1.hk = ts1.hk;
  }

  // Fortran: TRDIF/BLDIF depending on TRAN/SIMI/TURB/WAKE (xblsys.f:624-634).
  BldifStation st2 = bldifStationOfSec(in.primary2, in.secondary2, ts2);
  // Fortran: BLDIF reads COMMON HK2 after the BLVAR clamp (xblsys.f:799).
  st2.hk = ts2.hk;
  BldifSystem sys{};
  if (in.tran) {
    TrdifInput ti{};
    ti.first = st1;
    ti.second = st2;
    ti.xt = in.xt;
    ti.xtA1 = in.xtA1;
    ti.xtT1 = in.xtT1;
    ti.xtD1 = in.xtD1;
    ti.xtU1 = in.xtU1;
    ti.xtX1 = in.xtX1;
    ti.xtT2 = in.xtT2;
    ti.xtD2 = in.xtD2;
    ti.xtU2 = in.xtU2;
    ti.xtX2 = in.xtX2;
    ti.xtMs = in.xtMs;
    ti.xtRe = in.xtRe;
    ti.xtXf = in.xtXf;
    ti.amcrit = in.amcrit;
    ti.idampv = in.idampv;
    ti.props = in.props;
    ti.blpar = in.blpar;
    sys = trdif(ti);
  } else {
    BldifInput bi{};
    bi.first = st1;
    bi.second = st2;
    bi.mid = mid;
    if (in.simi) {
      bi.ityp = StationType::kLaminar;
      bi.similarity = true;
    } else if (!in.turb) {
      bi.ityp = StationType::kLaminar;
      bi.similarity = false;
    } else if (in.wake) {
      bi.ityp = StationType::kWake;
      bi.similarity = false;
    } else {
      bi.ityp = StationType::kTurbulent;
      bi.similarity = false;
    }
    bi.bule = in.bule;
    // Fortran AMPL2 = COM2(6) (BLPRV input AMI); AMCRIT enters only in
    // AXSET (ITYP=1) or TRDIF.
    bi.ampl2 = in.primary2.ampl;
    bi.acrit = in.amcrit;
    bi.idampv = in.idampv;
    bi.blpar = in.blpar;
    sys = bldif(bi);
  }

  // Fortran: SIMI -> VS2=VS1+VS2, VS1=0 (xblsys.f:636-644).
  if (in.simi) {
    for (int k = 0; k < 4; ++k) {
      const std::size_t kr = static_cast<std::size_t>(k);
      for (int l = 0; l < 5; ++l) {
        const std::size_t lc = static_cast<std::size_t>(l);
        sys.vs2[kr][lc] = sys.vs1[kr][lc] + sys.vs2[kr][lc];
        sys.vs1[kr][lc] = 0.0;
      }
    }
  }

  // Fortran: conversion to incompressible Uei and Mach (xblsys.f:647-658).
  for (int k = 0; k < 4; ++k) {
    const std::size_t kr = static_cast<std::size_t>(k);
    const double resU1 = sys.vs1[kr][3];
    const double resU2 = sys.vs2[kr][3];
    const double resMs = sys.vsm[kr];
    sys.vs1[kr][3] = resU1 * in.ueEi1;
    sys.vs2[kr][3] = resU2 * in.ueEi2;
    sys.vsm[kr] = resU1 * in.ueMs1 + resU2 * in.ueMs2 + resMs;
  }
  return sys;
}

void blsolv(BlsysWorkspace& ws, double vaccel, double arcSpan, int ivte1,
            int ivz) {
  const int nsys = ws.nsys;
  // Fortran: VACC1 = VACCEL ; VACC2 = VACC3 = VACCEL*2.0/(S(N)-S(1))
  const double vacc1 = vaccel;
  const double vacc2 = vaccel * 2.0 / arcSpan;
  const double vacc3 = vacc2;

  for (int iv = 0; iv < nsys; ++iv) {
    const int ivp = iv + 1;
    auto& vaIv = ws.va[static_cast<std::size_t>(iv)];
    auto& vdelIv = ws.vdel[static_cast<std::size_t>(iv)];
    auto vmRow = [&ws](int line, int l) -> std::array<double, 3>& {
      return ws.vm[static_cast<std::size_t>(line)]
                  [static_cast<std::size_t>(l)];
    };

    // ===== Invert VA(IV) block =====
    // Fortran: normalize first row
    double pivot = 1.0 / vaIv[0][0];
    vaIv[0][1] *= pivot;
    for (int l = iv; l < nsys; ++l) {
      vmRow(iv, l)[0] *= pivot;
    }
    vdelIv[0][0] *= pivot;
    vdelIv[0][1] *= pivot;
    // Fortran: eliminate lower first column in VA block
    for (int k = 1; k < 3; ++k) {
      const std::size_t kr = static_cast<std::size_t>(k);
      const double vtmp = vaIv[kr][0];
      vaIv[kr][1] -= vtmp * vaIv[0][1];
      for (int l = iv; l < nsys; ++l) {
        vmRow(iv, l)[kr] -= vtmp * vmRow(iv, l)[0];
      }
      vdelIv[kr][0] -= vtmp * vdelIv[0][0];
      vdelIv[kr][1] -= vtmp * vdelIv[0][1];
    }
    // Fortran: normalize second row
    pivot = 1.0 / vaIv[1][1];
    for (int l = iv; l < nsys; ++l) {
      vmRow(iv, l)[1] *= pivot;
    }
    vdelIv[1][0] *= pivot;
    vdelIv[1][1] *= pivot;
    // Fortran: eliminate lower second column in VA block
    {
      const int k = 2;
      const double vtmp = vaIv[k][1];
      for (int l = iv; l < nsys; ++l) {
        vmRow(iv, l)[k] -= vtmp * vmRow(iv, l)[1];
      }
      vdelIv[k][0] -= vtmp * vdelIv[1][0];
      vdelIv[k][1] -= vtmp * vdelIv[1][1];
    }
    // Fortran: normalize third row
    pivot = 1.0 / vmRow(iv, iv)[2];
    for (int l = ivp; l < nsys; ++l) {
      vmRow(iv, l)[2] *= pivot;
    }
    vdelIv[2][0] *= pivot;
    vdelIv[2][1] *= pivot;
    // Fortran: eliminate upper third column in VA block
    {
      const double vtmp1 = vmRow(iv, iv)[0];
      const double vtmp2 = vmRow(iv, iv)[1];
      for (int l = ivp; l < nsys; ++l) {
        vmRow(iv, l)[0] -= vtmp1 * vmRow(iv, l)[2];
        vmRow(iv, l)[1] -= vtmp2 * vmRow(iv, l)[2];
      }
      vdelIv[0][0] -= vtmp1 * vdelIv[2][0];
      vdelIv[1][0] -= vtmp2 * vdelIv[2][0];
      vdelIv[0][1] -= vtmp1 * vdelIv[2][1];
      vdelIv[1][1] -= vtmp2 * vdelIv[2][1];
    }
    // Fortran: eliminate upper second column in VA block
    {
      const double vtmp = vaIv[0][1];
      for (int l = ivp; l < nsys; ++l) {
        vmRow(iv, l)[0] -= vtmp * vmRow(iv, l)[1];
      }
      vdelIv[0][0] -= vtmp * vdelIv[1][0];
      vdelIv[0][1] -= vtmp * vdelIv[1][1];
    }

    if (iv == nsys - 1) {
      continue;  // Fortran: IF(IV.EQ.NSYS) GO TO 1000
    }

    // ===== Eliminate VB(IV+1) block, rows 1 -> 3 =====
    auto& vbIvp = ws.vb[static_cast<std::size_t>(ivp)];
    auto& vdelIvp = ws.vdel[static_cast<std::size_t>(ivp)];
    for (int k = 0; k < 3; ++k) {
      const std::size_t kr = static_cast<std::size_t>(k);
      const double vtmp1 = vbIvp[kr][0];
      const double vtmp2 = vbIvp[kr][1];
      const double vtmp3 = vmRow(ivp, iv)[kr];
      for (int l = ivp; l < nsys; ++l) {
        vmRow(ivp, l)[kr] -=
            (vtmp1 * vmRow(iv, l)[0] + vtmp2 * vmRow(iv, l)[1] +
             vtmp3 * vmRow(iv, l)[2]);
      }
      vdelIvp[kr][0] -= (vtmp1 * vdelIv[0][0] + vtmp2 * vdelIv[1][0] +
                         vtmp3 * vdelIv[2][0]);
      vdelIvp[kr][1] -= (vtmp1 * vdelIv[0][1] + vtmp2 * vdelIv[1][1] +
                         vtmp3 * vdelIv[2][1]);
    }

    if (iv == ivte1) {
      // Fortran: eliminate VZ block
      auto& vdelIvz = ws.vdel[static_cast<std::size_t>(ivz)];
      auto& vmIvz = ws.vm[static_cast<std::size_t>(ivz)];
      for (int k = 0; k < 3; ++k) {
        const std::size_t kr = static_cast<std::size_t>(k);
        const double vtmp1 = ws.vz[kr][0];
        const double vtmp2 = ws.vz[kr][1];
        for (int l = ivp; l < nsys; ++l) {
          vmIvz[static_cast<std::size_t>(l)][kr] -=
              (vtmp1 * vmRow(iv, l)[0] + vtmp2 * vmRow(iv, l)[1]);
        }
        vdelIvz[kr][0] -= (vtmp1 * vdelIv[0][0] + vtmp2 * vdelIv[1][0]);
        vdelIvz[kr][1] -= (vtmp1 * vdelIv[0][1] + vtmp2 * vdelIv[1][1]);
      }
    }

    if (ivp == nsys - 1) {
      continue;  // Fortran: IF(IVP.EQ.NSYS) GO TO 1000
    }

    // ===== Eliminate lower VM column =====
    for (int kv = iv + 2; kv < nsys; ++kv) {
      const double vtmp1 = vmRow(kv, iv)[0];
      const double vtmp2 = vmRow(kv, iv)[1];
      const double vtmp3 = vmRow(kv, iv)[2];
      auto& vdelKv = ws.vdel[static_cast<std::size_t>(kv)];
      if (std::abs(vtmp1) > vacc1) {
        for (int l = ivp; l < nsys; ++l) {
          vmRow(kv, l)[0] -= vtmp1 * vmRow(iv, l)[2];
        }
        vdelKv[0][0] -= vtmp1 * vdelIv[2][0];
        vdelKv[0][1] -= vtmp1 * vdelIv[2][1];
      }
      if (std::abs(vtmp2) > vacc2) {
        for (int l = ivp; l < nsys; ++l) {
          vmRow(kv, l)[1] -= vtmp2 * vmRow(iv, l)[2];
        }
        vdelKv[1][0] -= vtmp2 * vdelIv[2][0];
        vdelKv[1][1] -= vtmp2 * vdelIv[2][1];
      }
      if (std::abs(vtmp3) > vacc3) {
        for (int l = ivp; l < nsys; ++l) {
          vmRow(kv, l)[2] -= vtmp3 * vmRow(iv, l)[2];
        }
        vdelKv[2][0] -= vtmp3 * vdelIv[2][0];
        vdelKv[2][1] -= vtmp3 * vdelIv[2][1];
      }
    }
  }

  // Fortran: backward substitution (xbl.f ... xsolve.f:468-485)
  for (int iv = nsys - 1; iv >= 1; --iv) {
    auto& vdelIv = ws.vdel[static_cast<std::size_t>(iv)];
    const double vtmp1 = vdelIv[2][0];
    for (int kv = iv - 1; kv >= 0; --kv) {
      auto& vdelKv = ws.vdel[static_cast<std::size_t>(kv)];
      const auto& vmKv = ws.vm[static_cast<std::size_t>(kv)]
                              [static_cast<std::size_t>(iv)];
      vdelKv[0][0] -= vmKv[0] * vtmp1;
      vdelKv[1][0] -= vmKv[1] * vtmp1;
      vdelKv[2][0] -= vmKv[2] * vtmp1;
    }
    const double vtmp2 = vdelIv[2][1];
    for (int kv = iv - 1; kv >= 0; --kv) {
      auto& vdelKv = ws.vdel[static_cast<std::size_t>(kv)];
      const auto& vmKv = ws.vm[static_cast<std::size_t>(kv)]
                              [static_cast<std::size_t>(iv)];
      vdelKv[0][1] -= vmKv[0] * vtmp2;
      vdelKv[1][1] -= vmKv[1] * vtmp2;
      vdelKv[2][1] -= vmKv[2] * vtmp2;
    }
  }
}

}  // namespace xfoil::core::boundary_layer

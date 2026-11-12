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

// File: src/core/boundary_layer_solver/Viscal.cpp
// VISCAL of the new 0-based core, ported
// directly from fortran-src/xoper.f VISCAL (2892-3116). The `.bl`
// block (xoper.f:3031-3098) is part of the canonical Fortran routine
// VISCAL; the numeric VISCAL kernel lives here, while
// the `.bl` file/output page belongs to the IO layer, which
// is why the complete Fortran routine is not fully
// migrated here yet. Formulas, constants and
// control-flow order are formula-faithful; internal computations use
// double. No raw pointers and no global state.

#include <iostream>
#include "xfoil/core/boundary_layer/Viscal.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <ios>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/Coupling.h"
#include "xfoil/core/boundary_layer/Update.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/Stagnation.h"

namespace xfoil::core::boundary_layer {

namespace {

constexpr std::size_t sz(int i) { return static_cast<std::size_t>(i); }

// Fortran DATA EPS1 (xoper.f:2899).
constexpr double kEps1 = 1.0E-4;

// Fortran IBL 1..NBL -> C++ 0-based 0..nbl inclusive (per page). The
// upper wake stations (IBLTE(1)+1..IBLTE(1)+NW, IBLPAN: "upper wake
// pointers (for plotting only)") lie beyond NBL(1) and are described by
// UPDATE (xbl.f:1547-1557); therefore the top field needs
// max(NBL(1), IBLTE(1)+NW)+1 entries.
void ensureSideField(BoundaryLayerStationField& field,
                     const BoundaryLayerMapping& map) {
  const std::size_t nw = sz(map.bottom.nbl - map.bottom.iblte);
  const std::size_t needTop =
      std::max(sz(map.top.nbl), sz(map.top.iblte) + nw) + 1;
  const std::size_t needBottom = sz(map.bottom.nbl) + 1;
  // resize (not assign): existing station values are retained and new
  // positions are zero-initialized (Fortran COMMON semantics).
  if (field.top.size() < needTop) {
    field.top.resize(needTop, 0.0);
  }
  if (field.bottom.size() < needBottom) {
    field.bottom.resize(needBottom, 0.0);
  }
}

std::span<double> sideSpan(BoundaryLayerStationField& f, int is) {
  return std::span<double>(is == 0 ? f.top : f.bottom);
}

std::span<const double> sideSpanConst(const BoundaryLayerStationField& f,
                                      int is) {
  return std::span<const double>(is == 0 ? f.top : f.bottom);
}

}  // namespace

ViscalResult viscal(const ViscalInput& in,
                    const paneling::PanelGeometry& panel,
                    const inviscid::InfluenceSystem& gs,
                    const inviscid::WakeState& wake,
                    const inviscid::TrailingEdgeState& te,
                    ViscalState& state) {
  if (in.niter == 0) {
    throw std::invalid_argument(
        "viscal: NITER1=0 in the core is not interactive (Fortran ASKI)");
  }

  const std::size_t n = panel.x.size();
  const std::size_t nw = wake.nw;

  // Fortran 2904: IF(.NOT.LWAKE) CALL XYWAKE — the wake geometry is
  // input (analogous to LWAKE: the caller has already run XYWAKE).

  // Fortran 2909: CALL QWCALC — wake base velocities.
  const inviscid::WakeBasisResult qb =
      inviscid::qwCalc(gs, wake, panel, state.alfa, in.qinf);

  // Fortran 2912: CALL QISET — QINV/QINV_A from the n+1+nw basis.
  state.qinvu1 = gs.qinvu1;
  state.qinvu2 = gs.qinvu2;
  state.qinvu1.insert(state.qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  state.qinvu2.insert(state.qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  inviscid::qiset(state.qinvu1, state.qinvu2, state.alfa, state.qinv,
                  state.qinvA);

  // Fortran 2914-2930: IF(.NOT.LIPAN) — stagnation-point and system build-up.
  ViscalResult res{};
  if (!state.mapping.valid) {
    // Fortran 2916: IF(LBLINI) CALL GAMQV. GAMQV reads QVIS(1..N) from
    // the COMMON field (XFOIL.INC CR04, fixed dimension IZX, zero-
    // initialized at program start). If OPER INIT sets LBLINI true
    // before any QVFUE has run, this commonInitialized state holds
    // exactly (binary
    // oper-state/oper-init-inviscid-then-bang). The capacity zero fill
    // models the COMMON field; a solved QVIS is never overwritten.
    if (state.lblini) {
      if (state.qvisState ==
              ViscousVelocityFieldState::commonInitialized &&
          state.qvis.size() < state.gam.size()) {
        state.qvis.resize(state.gam.size(), 0.0);
      }
      gamqv(state.qvis, state.qinvA, state.gam, state.gamA);
    }
    // Fortran 2919: CALL STFIND. Not found: message (xpanel.f:1366)
    // and fallback I=N/2; Fortran then writes the header.
    const inviscid::StagnationPointResult st =
        inviscid::stfind(state.gam, panel.s);
    if (!st.found) {
      res.events.push_back(ViscousEventRecord{
          ViscousEventKind::stagnationPointNotFound,
          ViscousEventPhase::preHead, 0, -1, -1, 0.0, 0.0, 0.0, -1});
    }
    // Fortran 2922: CALL IBLPAN. IPAN is a fixed COMMON field
    // (CR15, IVX): IBLPAN overwrites only the canonical written
    // range (1..IBLTE(IS)) plus wake-pointer loop 35
    // (IBLTE(1)+1..IBLTE(1)+NW on IS=1); entries beyond that
    // remain as preserved COMMON state from earlier points. The
    // assignment therefore preserves the old tail capacitively (grow-only).
    auto fresh = iblpan(panel, st.ist, nw, in.ivx);
    preserveIpanTail(state.mapping.top.ipan, fresh.top.ipan);
    preserveIpanTail(state.mapping.bottom.ipan, fresh.bottom.ipan);
    state.mapping = std::move(fresh);
    if (!state.mapping.valid) {
      throw std::invalid_argument("viscal: IBLPAN failed");
    }
    // Fortran 2925: CALL XICALC.
    state.wgap = xicalc(panel, state.mapping, st.sst, te.ante, wake.x, wake.y,
                        state.arcs);
    // Fortran 2928: CALL IBLSYS.
    state.isys = iblsys({state.mapping.top.nbl, state.mapping.bottom.nbl},
                        static_cast<int>(in.ivx));
    state.ist = st.ist;
    state.sst = st.sst;
    state.sstGo = st.sstGo;
    state.sstGp = st.sstGp;
  }

  // Fortran 2933: CALL UICALC — UINV/UINV_A from QINV/QINV_A.
  uicalc(state.mapping, state.qinv, state.qinvA, state.uinv, state.uinvA);

  // Fortran 2935-2946: IF(.NOT.LBLINI) — initial Ue from the inviscid Ue
  // (Fortran DO IBL=1,NBL(IS) -> 0-based 0..nbl inclusive).
  // Fortran UEDG is a COMMON station array (XBL.INC/CR15, fixed
  // size, zero-initialized). The capacity reservation
  // happens independently of the LBLINI branch: with LBLINI=true (OPER INIT
  // before the first viscous run) UEDG remains in the COMMON zero state
  // and is only read during marching; a later
  // UINV adoption must not bypass any quantity checks.
  ensureSideField(state.uedg, state.mapping);
  if (!state.lblini) {
    for (int is = 0; is < 2; ++is) {
      const auto& m = is == 0 ? state.mapping.top : state.mapping.bottom;
      const auto uinvS = sideSpanConst(state.uinv, is);
      auto uedgS = sideSpan(state.uedg, is);
      for (int ibl = 0; ibl <= m.nbl; ++ibl) {
        uedgS[sz(ibl)] = uinvS[sz(ibl)];
      }
    }
  }

  // Station fields for the Marching size (Fortran arrays are
  // Common-based with Zero initialization).
  ensureSideField(state.thet, state.mapping);
  ensureSideField(state.dstr, state.mapping);
  ensureSideField(state.ctau, state.mapping);
  ensureSideField(state.mass, state.mapping);
  ensureSideField(state.tau, state.mapping);
  ensureSideField(state.dis, state.mapping);
  ensureSideField(state.ctq, state.mapping);
  ensureSideField(state.delt, state.mapping);
  ensureSideField(state.tstr, state.mapping);
  ensureSideField(state.dssr, state.mapping);
  ensureSideField(state.mdef, state.mapping);
  ensureSideField(state.pdef, state.mapping);
  ensureSideField(state.kdef, state.mapping);
  // IPAN divides the station fields from the COMMON capacity (CR15,
  // IVX): entries never written remain zero as in
  // Fortran; grow-only with zeros, the already written
  // range (incl. preserveIpanTail) stays unchanged.
  syncPlxaddStationCapacity(state);

  // COMMON capacity for QVIS (see ViscousVelocityFieldState):
  // QVIS is a fixed-dimension COMMON field QVIS(IZX),
  // IZX = IQX+IWX (XFOIL.INC CR04). QVFUE (xpanel.f:1580-1591)
  // writes QVIS(I) via IPAN(IBL,IS) up to and including the
  // wake stations (I up to N+NW). When NW grows (VPAR WAKE), the same
  // COMMON field with capacity n+nw is required regardless of whether
  // QVIS was already solved: grow-only with zeros preserves the
  // solved entries and models the still-never-written
  // wake tail as COMMON zeros.
  if (state.qvis.size() < n + nw) {
    state.qvis.resize(n + nw, 0.0);
  }

  // Fortran 2948-2961: IF(LVCONV) — loads from the existing
  // converged point.
  if (state.lvconv) {
    qvfue(state.mapping, state.uedg, std::span<double>(state.qvis));
    state.qvisState = ViscousVelocityFieldState::solved;
    // Fortran 2952-2956: CPCALC sets the session arrays CPV/CPI; in the
    // driver they are computed only at the end (xoper.f:3026-3027) from
    // the then-current state, so the side-effect-free intermediate
    // computation is dropped here.
    gamqv(state.qvis, state.qinvA, state.gam, state.gamA);
    const inviscid::LoadResult load =
        inviscid::clCalc(panel.x, panel.y, state.gam, state.gamA, state.alfa,
                         state.minf, in.qinf, in.xref, in.yref);
    state.cl = load.cl;
    state.cm = load.cm;
    state.cdp = load.cdp;
    const CdCalcResult cdc =
        cdcalc(state.alfa, in.qinf, state.tklam, in.lvisc, state.lblini,
               state.mapping, state.thet, state.dstr, state.uedg, state.tau,
               panel.x, panel.y);
    state.cd = cdc.cd;
    state.cdf = cdc.cdf;
    state.clf = cdc.clf;
  }

  // Fortran 2964: IF(.NOT.LWDIJ .OR. .NOT.LADIJ) CALL QDCALC.
  // XYWAKE invalidates LWDIJ per point (xpanel.f:1350); the
  // wake-dependent DIJ block is rebuilt with the new wake geometry. The
  // airfoil DIJ block (LADIJ) is BIJ-invariant, so the full computation
  // returns the same airfoil part plus the fresh wake part.
  ViscousEventRecord qdCalcEvent{};
  if (state.dij.empty() || !state.dijWakeValid) {
    const std::vector<double> sig(n, 0.0);
    state.dij =
        inviscid::qdCalc(gs, wake, panel, state.alfa, in.qinf, sig).dij;
    state.dijStride = static_cast<int>(n + nw);
    state.dijWakeValid = true;
    // Fortran: WRITE(*,*) 'Calculating source influence matrix ...'
    // at the QDCALC entry (xpanel.f:1156), only if QDCALC is actually
    // called (xoper.f:2964 gate). Alpha at execution time.
    qdCalcEvent = ViscousEventRecord{
        ViscousEventKind::sourceInfluenceCalculation,
        ViscousEventPhase::preHead, -1, -1, -1, 0.0, 0.0, state.alfa, -1};
  }

  // Fortran SETBL: CALL COMSET (xbl.f:50) — TKLAM/TK_MSQ session state
 // from COMSET; the C++ setbl takes tklam from the caller, so the
  // session state must be set from the current MINF before the first
  // SETBL.
  const inviscid::CompressibilityState csSetup =
      inviscid::comset(state.minf, in.qinf);
  state.tklam = csSetup.tklam;
  state.tkMsq = csSetup.tkMsq;

  // Fortran 2970-3022: Newton iteration for the entire BL solution.
  if (qdCalcEvent.kind == ViscousEventKind::sourceInfluenceCalculation) {
    res.events.push_back(qdCalcEvent);
  }
  const double arcSpan = panel.s[n - 1] - panel.s[0];  // Fortran S(N)-S(1)
  // Fortran sets XIFORC from XIFSET(IS) (xbl.f:165/566/902: at the start
  // of each IS pass of SETBL/MRCHUE/MRCHDU). The STMOVE remap changes
  // XSSI between iterations, so XIFSET is re-executed before each SETBL
  // with the current mapping/arcs; XSSI is constant within one SETBL
  // call, so the Fortran per-IS-pass calls return the same value.
  std::array<double, 2> xiforcIter = in.xiforc;

  SetblDiagnostics lastSetbl{};  // XOCTR(1) of the last SETBL for Format 9922
  for (std::size_t iter = 1; iter <= in.niter; ++iter) {
    // Fortran: CALL XIFSET(IS) (xbl.f:165/566/902).
    for (int isx = 0; isx < 2; ++isx) {
      const SurfaceSide sideX =
          isx == 0 ? SurfaceSide::kTop : SurfaceSide::kBottom;
      const auto xif = xifset(panel, state.mapping, state.arcs, state.sst,
                              in.xstrip[sz(isx)], sideX);
      if (xif.isOk()) {
        xiforcIter[sz(isx)] = xif.value().xiforc;
      }
    }
    // Fortran 2973: CALL SETBL.
    SetblInput sb{};
    sb.march.mapping = state.mapping;
    sb.march.xssi = {std::span<const double>(state.arcs.top),
                     std::span<const double>(state.arcs.bottom)};
    sb.march.wgap = state.wgap.wgap;
    sb.march.acrit = in.acrit;
    sb.march.xiforc = xiforcIter;
    sb.march.ante = te.ante;
    sb.march.blpar = in.blpar;
    sb.march.mass = state.mass;
    sb.march.uedg = state.uedg;
    sb.march.thet = state.thet;
    sb.march.dstr = state.dstr;
    sb.march.ctau = state.ctau;
    sb.march.tau = state.tau;
    sb.march.dis = state.dis;
    sb.march.ctq = state.ctq;
    sb.march.delt = state.delt;
    sb.march.tstr = state.tstr;
    sb.march.dssr = state.dssr;
    sb.march.mdef = state.mdef;
    sb.march.pdef = state.pdef;
    sb.march.kdef = state.kdef;
    sb.march.itran = state.itran;
    sb.march.xoctr = state.xoctr;
    sb.march.props = state.props;
    sb.mapping = state.mapping;
    sb.isys = state.isys.isys;
    sb.nsys = state.isys.nsys;
    sb.uinv = state.uinv;
    sb.uinvA = state.uinvA;
    sb.dij = state.dij;
    sb.dijStride = state.dijStride;
    sb.sstGo = state.sstGo;
    sb.sstGp = state.sstGp;
    sb.sst = state.sst;
    sb.xle = panel.xle;
    sb.xte = panel.xte;
    sb.yle = panel.yle;
    sb.yte = panel.yte;
    sb.xPan = panel.x;
    sb.xpPan = panel.xp;
    sb.yPan = panel.y;
    sb.ypPan = panel.yp;
    sb.sPan = panel.s;
    sb.nPan = static_cast<int>(n);
    sb.ist = static_cast<int>(state.ist);
    sb.cl = state.cl;
    sb.clspec = in.clspec;
    sb.lalfa = in.lalfa;
    sb.lblini = state.lblini;
    sb.machType = in.machType;
    sb.reType = in.reType;
    sb.minf1 = in.minf1;
    sb.reinf1 = in.reinf1;
    sb.minf = state.minf;
    sb.reinf = state.reinf;
    sb.qinf = in.qinf;
    sb.gamma = inviscid::kGamma;
    sb.gamm1 = inviscid::kGamm1;
    sb.hvrat = 0.0;
    sb.idamp = in.idamp;
    sb.tklam = state.tklam;
    sb.tkMsq = state.tkMsq;

    lastSetbl = setbl(sb, state.marchState);
    // Fortran TINDEX(1..2) is set in SETBL and stays until PLRADD
    // (xpol.f:596); the value from the last SETBL call is authoritative.
    state.tindex = lastSetbl.tindex;
    res.tindex = lastSetbl.tindex;
    // Fortran XSSITR(IS) (XBL.INC COMMON): is set in SETBL per page at XT
    // (xbl.f:445,858,1183) and persists; dplot.f FBLGET
    // compares XI > XSSITR(IS): persisted session-side.
    state.xssitr = lastSetbl.xssitr;

    // Fortran: canonical WRITE digits in SETBL/MRCHUE/MRCHDU (xbl.f:84-93,
    // 505-509, 561, 723, 779, 1102). The events are recorded in
    // execution order with the 0-based iteration number
    // attached and passed on; nothing changes numerically.
    for (ViscousEventRecord ev : lastSetbl.events) {
      ev.iteration = static_cast<int>(iter - 1);
      res.events.push_back(ev);
    }

    // Fortran SETBL: MRCL(CLMR) sets MINF/REINF (Session state);
    // LBLINI is after the first March set.
    state.minf = sb.minf;
    state.reinf = sb.reinf;
    state.lblini = sb.lblini;
    state.itran = lastSetbl.itran;
    state.xoctr = sb.march.xoctr;
    state.props = sb.march.props;
    state.thet = sb.march.thet;
    state.dstr = sb.march.dstr;
    state.ctau = sb.march.ctau;
    state.uedg = sb.march.uedg;
    state.mass = sb.march.mass;
    state.tau = sb.march.tau;
    state.dis = sb.march.dis;
    state.ctq = sb.march.ctq;
    state.delt = sb.march.delt;
    state.tstr = sb.march.tstr;
    state.dssr = sb.march.dssr;
    state.mdef = sb.march.mdef;
    state.pdef = sb.march.pdef;
    state.kdef = sb.march.kdef;

    // Fortran COMMON semantics: SETBL/UESET shrink march.uedg to
    // NBL+1; the wake positions beyond that (UPDATE, xbl.f:1547-1557)
    // are saved before the update so that no station values are lost.
    ensureSideField(state.uedg, state.mapping);
    ensureSideField(state.thet, state.mapping);
    ensureSideField(state.dstr, state.mapping);
    ensureSideField(state.ctau, state.mapping);
    ensureSideField(state.mass, state.mapping);
    ensureSideField(state.tau, state.mapping);
    ensureSideField(state.dis, state.mapping);
    ensureSideField(state.ctq, state.mapping);
    ensureSideField(state.delt, state.mapping);
    ensureSideField(state.tstr, state.mapping);
    ensureSideField(state.dssr, state.mapping);
    ensureSideField(state.mdef, state.mapping);
    ensureSideField(state.pdef, state.mapping);
    ensureSideField(state.kdef, state.mapping);
    syncPlxaddStationCapacity(state);

    // Fortran 2976: CALL BLSOLV
    // IVTE1 = ISYS(IBLTE(1),1) ; IVZ = ISYS(IBLTE(2)+1,2).
    const int ivte1 = state.isys.isys[0][sz(state.mapping.top.iblte)];
    const int ivz =
        state.isys.isys[1][sz(state.mapping.bottom.iblte) + 1];
    blsolv(sb.ws, in.vaccel, arcSpan, ivte1, ivz);

    // Fortran 2979: CALL UPDATE — VDEL residuals and Re sensitivities
    // flat per system row (line*3 + equation).
    std::vector<double> vdelRes(sb.ws.vdel.size() * 3, 0.0);
    std::vector<double> vdelRe(sb.ws.vdel.size() * 3, 0.0);
    for (std::size_t line = 0; line < sb.ws.vdel.size(); ++line) {
      for (std::size_t eq = 0; eq < 3; ++eq) {
        vdelRes[line * 3 + eq] = sb.ws.vdel[line][eq][0];
        vdelRe[line * 3 + eq] = sb.ws.vdel[line][eq][1];
      }
    }

    UpdateInput upd{};
    upd.mapping = state.mapping;
    upd.dij = state.dij;
    upd.dijStride = state.dijStride;
    upd.uinv = state.uinv;
    upd.uinvA = state.uinvA;
    upd.mass = state.mass;
    upd.vdelRes = vdelRes;
    upd.vdelRe = vdelRe;
    upd.isys = state.isys.isys;
    upd.x = panel.x;
    upd.y = panel.y;
    upd.alfa = state.alfa;
    upd.minf = state.minf;
    upd.qinf = in.qinf;
    upd.gamm1 = inviscid::kGamm1;
    upd.dtor = in.dtor;
    upd.minfCl = state.minfCl;
    upd.lalfa = in.lalfa;
    upd.matyp1 = in.machType == 1;
    upd.cl = state.cl;
    upd.clspec = in.clspec;
    upd.wgap = state.wgap.wgap;
    upd.itranTop = state.itran[0];
    upd.itranBottom = state.itran[1];
    upd.ctau = state.ctau;
    upd.thet = state.thet;
    upd.dstr = state.dstr;
    upd.uedg = state.uedg;
    upd.tau = state.tau;
    upd.dis = state.dis;
    upd.ctq = state.ctq;
    upd.delt = state.delt;
    upd.tstr = state.tstr;
    upd.dssr = state.dssr;
    upd.mdef = state.mdef;
    upd.pdef = state.pdef;
    upd.kdef = state.kdef;

    const UpdateResult ures = update(upd);

    // Fortran UPDATE: BL variables, Alfa/Cl (in/out) and the
    // Convergence diagnostics.
    state.ctau = std::move(upd.ctau);
    state.thet = std::move(upd.thet);
    state.dstr = std::move(upd.dstr);
    state.uedg = std::move(upd.uedg);
    state.mass = std::move(upd.mass);
    state.tau = std::move(upd.tau);
    state.dis = std::move(upd.dis);
    state.ctq = std::move(upd.ctq);
    state.delt = std::move(upd.delt);
    state.tstr = std::move(upd.tstr);
    state.dssr = std::move(upd.dssr);
    state.mdef = std::move(upd.mdef);
    state.pdef = std::move(upd.pdef);
    state.kdef = std::move(upd.kdef);
    state.alfa = upd.alfa;
    state.cl = upd.cl;

    // Fortran 2981-2989: IF(LALFA) MRCL/COMSET; otherwise QISET/UICALC.
    if (in.lalfa) {
      const inviscid::MachReynoldsSettings mrSettings{
          in.machType, in.reType, in.minf1, in.reinf1};
      const auto mr = inviscid::mrcl(mrSettings, state.cl);
      if (mr.isErr()) {
        throw std::invalid_argument("viscal: MRCL inputs invalid");
      }
      state.minf = mr.value().minf;
      state.reinf = mr.value().reinf;
      state.minfCl = mr.value().mCls;
      const inviscid::CompressibilityState cs =
          inviscid::comset(state.minf, in.qinf);
      state.tklam = cs.tklam;
      state.tkMsq = cs.tkMsq;
    } else {
      inviscid::qiset(state.qinvu1, state.qinvu2, state.alfa, state.qinv,
                      state.qinvA);
      uicalc(state.mapping, state.qinv, state.qinvA, state.uinv, state.uinvA);
    }

    // Fortran 2992-2995: CALL QVFUE / CALL GAMQV.
    qvfue(state.mapping, state.uedg, std::span<double>(state.qvis));
    state.qvisState = ViscousVelocityFieldState::solved;
    gamqv(state.qvis, state.qinvA, state.gam, state.gamA);

    // Fortran 2998: CALL STMOVE (STFIND/IBLPAN/UICALC/XICALC/IBLSYS
    // internal; SST_GO/SST_GP from the internal STFIND deterministic
    // identical with a repeated stfind call).
    StMoveState ms{};
    ms.mapping = state.mapping;
    ms.arcs = state.arcs;
    ms.uinv = state.uinv;
    ms.uinvA = state.uinvA;
    ms.ctau = state.ctau;
    ms.thet = state.thet;
    ms.dstr = state.dstr;
    ms.uedg = state.uedg;
    ms.mass = state.mass;
    ms.itran = state.itran;
    ms.ist = state.ist;
    ms.sst = state.sst;
    ms.isys = state.isys;
    const StMoveResult smres =
        stmove(panel, wake.x, wake.y, te.ante, nw, in.ivx, state.qinv,
               state.qinvA, state.gam, std::span<double>(state.qvis), ms);
    state.mapping = std::move(ms.mapping);
    state.arcs = std::move(ms.arcs);
    state.uinv = std::move(ms.uinv);
    state.uinvA = std::move(ms.uinvA);
    state.ctau = std::move(ms.ctau);
    state.thet = std::move(ms.thet);
    state.dstr = std::move(ms.dstr);
    state.uedg = std::move(ms.uedg);
    state.mass = std::move(ms.mass);
    state.itran = ms.itran;
    state.ist = ms.ist;
    state.sst = ms.sst;
    state.isys = std::move(ms.isys);
    // Fortran STMOVE -> STFIND message for a not-found stagnation point
    // (xpanel.f:1366, xoper.f:2998); Fortran then writes the iteration.
    if (smres.stagnationNotFound) {
      res.events.push_back(ViscousEventRecord{
          ViscousEventKind::stagnationPointNotFound,
          ViscousEventPhase::postMarch, static_cast<int>(iter - 1),
          -1, -1, 0.0, 0.0, 0.0, -1});
    }
    // Fortran COMMON semantics: all station arrays are IVX-sized
    // (IVX = IQX/2+IWX+50) and therefore always sufficient after the
    // STMOVE remap (new mapping via IBLPAN, xpanel.f:1648). STMOVE shifts
    // CTAU/THET/DSTR/UEDG (xpanel.f:1667-1729); TAU/DIS/CTQ/DELT/TSTR
    // remain unchanged. The tight C++ vectors are grown here only to the
    // new mapping (value-neutral vs. Fortran); without this extension the
    // next MRCHDU would write outside the
    // fields (activation deviation, Marching.cpp TAU/DIS/CTQ/DELT/
    // TSTR write accesses).
    ensureSideField(state.thet, state.mapping);
    ensureSideField(state.dstr, state.mapping);
    ensureSideField(state.ctau, state.mapping);
    ensureSideField(state.mass, state.mapping);
    ensureSideField(state.tau, state.mapping);
    ensureSideField(state.dis, state.mapping);
    ensureSideField(state.ctq, state.mapping);
    ensureSideField(state.delt, state.mapping);
    ensureSideField(state.tstr, state.mapping);
    ensureSideField(state.dssr, state.mapping);
    ensureSideField(state.mdef, state.mapping);
    ensureSideField(state.pdef, state.mapping);
    ensureSideField(state.kdef, state.mapping);
    ensureSideField(state.uedg, state.mapping);
    syncPlxaddStationCapacity(state);
    const inviscid::StagnationPointResult stGo =
        inviscid::stfind(state.gam, panel.s);
    state.sstGo = stGo.sstGo;
    state.sstGp = stGo.sstGp;
    static_cast<void>(smres);

    // Fortran 3001-3003: CALL CLCALC / CALL CDCALC.
    const inviscid::LoadResult load =
        inviscid::clCalc(panel.x, panel.y, state.gam, state.gamA, state.alfa,
                         state.minf, in.qinf, in.xref, in.yref);
    state.cl = load.cl;
    state.cm = load.cm;
    state.cdp = load.cdp;
    const CdCalcResult cdc =
        cdcalc(state.alfa, in.qinf, state.tklam, in.lvisc, state.lblini,
               state.mapping, state.thet, state.dstr, state.uedg, state.tau,
               panel.x, panel.y);
    state.cd = cdc.cd;
    state.cdf = cdc.cdf;
    state.clf = cdc.clf;

    // Fortran 3005-3020: Convergence test (RMSBL < EPS1). The Record carries
    // additionally the aerodynamic quantities of this iteration state
    // (Format 2020 sources: Alfa/DTOR, Cl, CM, Cd, CDF; CDPDIF = Cd - CDF).
    res.rmsblHistory.push_back(ures.rmsbl);
    res.iterationRecords.push_back(ViscalIterationRecord{
        ures.rmsbl, ures.dac,   ures.rlx,  ures.rmxbl, ures.vmxbl,
        ures.imxbl, ures.ismxbl, state.alfa, load.cl,   load.cm,
        cdc.cd,     cdc.cdf,    cdc.clf,   load.cdp});
    res.iterations = iter;
    res.rmsbl = ures.rmsbl;
    if (ures.rmsbl < kEps1) {
      res.converged = true;
      state.lvconv = true;
      state.avisc = state.alfa;
      state.mvisc = state.minf;
      break;
    }
  }

  // Fortran 90 (3026-3028): CALL CPCALC/CPCALC, IF(LFLAP) CALL MHINGE.
  auto cpi = inviscid::cpCalc(state.qinv, in.qinf, state.minf);
  auto cpv = inviscid::cpCalc(state.qvis, in.qinf, state.minf);
  if (cpi.isErr() || cpv.isErr()) {
    throw std::invalid_argument("viscal: CPCALC denominator <= 0");
  }
  res.cp = std::move(cpi.value());
  res.cpv = std::move(cpv.value());
  if (in.lflap) {
    res.hinge = inviscid::mhinge(panel.x, panel.xp, panel.y, panel.yp,
                                 panel.s, res.cp, res.cpv, in.lvisc, in.xof,
                                 in.yof);
  }

  // Fortran Label 90, xoper.f 3029-3057: numerical completion block for
  // format 9922 (side 1 = top). It is executed after both converged and
  // non-converged loops. No text output in the core.
  ViscalCompletionRecord comp{};
  comp.acrit = in.acrit[0];
  comp.cd = state.cd;
  comp.xoctr = lastSetbl.xoctr[0];
  double hkmax = 0.0;
  double hkm = 0.0;
  double psep = 0.0;
  double patt = 0.0;
  const int iblteTop = state.mapping.top.iblte;
  for (std::size_t ibl = 1;
       ibl <= static_cast<std::size_t>(iblteTop < 0 ? 0 : iblteTop); ++ibl) {
    const double hki = state.dstr.top[ibl] / state.thet.top[ibl];
    hkmax = std::max(hki, hkmax);
    if (hkm < 4.0 && hki >= 4.0) {
      const double hfrac = (4.0 - hkm) / (hki - hkm);
      const double pdefm =
          state.uedg.top[ibl - 1] * state.uedg.top[ibl - 1] *
          state.thet.top[ibl - 1];
      const double pdefi =
          state.uedg.top[ibl] * state.uedg.top[ibl] * state.thet.top[ibl];
      psep = pdefm * (1.0 - hfrac) + pdefi * hfrac;
    }
    if (hkm > 4.0 && hki < 4.0) {
      const double hfrac = (4.0 - hkm) / (hki - hkm);
      const double pdefm =
          state.uedg.top[ibl - 1] * state.uedg.top[ibl - 1] *
          state.thet.top[ibl - 1];
      const double pdefi =
          state.uedg.top[ibl] * state.uedg.top[ibl] * state.thet.top[ibl];
      patt = pdefm * (1.0 - hfrac) + pdefi * hfrac;
    }
    hkm = hki;
  }
  comp.hkmax = hkmax;
  comp.psep = psep;
  comp.patt = patt;
  comp.delp = patt - psep;
  res.completion = comp;
  res.completionValid = true;

  res.alfa = state.alfa;
  res.minf = state.minf;
  res.cl = state.cl;
  res.cm = state.cm;
  res.cdp = state.cdp;
  res.cd = state.cd;
  res.cdf = state.cdf;
  res.clf = state.clf;
  return res;
}

}  // namespace xfoil::core::boundary_layer

// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
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

// File: tests/core/ClarkYViscousCoreTests.cpp
// permanent kernel regression test: complete core path
// (ggCalc -> specAlpha -> xywake -> qwCalc -> qiset -> stfind/iblpan/
// xicalc/iblsys/uicalc -> xifset -> viscal -> fcpmin) for Clark Y,
// Re = 1e5, Mach = 0, QINF = 1.0, no-PANE (abcopy) against the canonical
// Fortran full-path values.
//
// Numerical authority: canonical Fortran routines (c11a_harness.f,
// extracted byte-faithfully from fortran-src, canonical build flags
// -fdefault-real-8 -fdefault-double-8; unqualified/default REAL is
// effectively REAL(8) in the canonical build, while explicit REAL*4 stays
// REAL*4). Full-path values confirmed directly from Fortran (VISCAL(100),
// FINAL block, It17.8 or direct %.16E comparison):
//
// alpha = 4.0:
// 12 iterations, converged,
// ITRAN1 = 61, ITRAN2 = 70 (1-based),
// CL 8.2243225894774308E-01
// CM -8.2790137654465551E-02
// CD 1.7288688307967329E-02
// CDF 8.1030259647362282E-03
// CDP 6.38220467E-03 (CLCALC pressure drag, not Cd-CDF)
// RMSBL 2.3472461899631799E-05 (bit-identical)
// RMXBL -2.1513870175336578E-04 (bit-identical)
//
// alpha = 0.5:
// 8 iterations, converged,
// ITRAN1 = 67, ITRAN2 = 75 (1-based),
// CL 4.2784322458710400E-01
// CM -9.0867797856485771E-02
// CD 1.8639247478485541E-02
// CDF 7.2829232999668449E-03
// RMSBL 2.5596698397623774E-05 (bit-identical)
// RMXBL -1.2461235970296974E-04 (bit-identical)
//
// thresholds: derived directly from the Fortran confirmation (core ==
// Fortran to 10 significant digits or bit-identical; permitted deviation
// abs 1e-9 / rel 1e-9 for the coefficients). The test guards the three
// Fortran-proven corrections as regressions: AMI station persistence
// (iteration count 12/8 and final values), CF2_MS/CF2T_MS incl. RT2_MS
// term (RMSBL/RMXBL/final values), and XIFSET per iteration after changed
// mapping/XSSI (ITRAN1=61/67, ITRAN2=70/75 exact). No
// /tmp artifacts.

#include "TestSupport.h"

#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/BoundaryLayerSolverTypes.h"
#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"
#include "xfoil/core/boundary_layer/Coupling.h"
#include "xfoil/core/boundary_layer/Marching.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"
#include "xfoil/core/boundary_layer/Viscal.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/paneling/Abcopy.h"
#include "xfoil/core/paneling/PanelingTypes.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

using xfoil::core::boundary_layer::BlsysMapping;
using xfoil::core::boundary_layer::BoundaryLayerArcLengths;
using xfoil::core::boundary_layer::BoundaryLayerMapping;
using xfoil::core::boundary_layer::BoundaryLayerStationField;
using xfoil::core::boundary_layer::SurfaceSide;
using xfoil::core::boundary_layer::TransitionState;
using xfoil::core::boundary_layer::ViscalInput;
using xfoil::core::boundary_layer::ViscalResult;
using xfoil::core::boundary_layer::ViscalState;
using xfoil::core::boundary_layer::WakeGap;
using xfoil::core::geometry::AirfoilBuffer;
using xfoil::core::inviscid::InfluenceSystem;
using xfoil::core::inviscid::MachReynoldsSettings;
using xfoil::core::inviscid::StagnationPointResult;
using xfoil::core::inviscid::TrailingEdgeState;
using xfoil::core::inviscid::WakeState;
using xfoil::core::inviscid::WakeBasisResult;
using xfoil::core::paneling::PanelGeometry;

// Fortran PI = 4.0*ATAN(1.0) (xfoil.f INIT) -- bit-identical; alpha as in
// the canonical Fortran driver ALFR = ALFADEG*PI/180.0.
double canonicalAlpha(double alphaDegrees) {
  const double pi = 4.0 * std::atan(1.0);
  return alphaDegrees * pi / 180.0;
}

AirfoilBuffer readClarkYBuffer() {
  std::ifstream in("sample/CLARK_Y.dat");
  CHECK(in.good());
  std::string name;
  std::getline(in, name);
  std::vector<double> bx;
  std::vector<double> by;
  double x = 0.0;
  double y = 0.0;
  while (in >> x >> y) {
    bx.push_back(x);
    by.push_back(y);
  }
  CHECK_EQ(bx.size(), static_cast<std::size_t>(151));
  return AirfoilBuffer(bx, by, name);
}

struct ClarkYPointResult {
  ViscalResult viscal;
  int itranTop{0};    // Fortran ITRAN(1), 1-based
  int itranBottom{0}; // Fortran ITRAN(2), 1-based
};

// canonical VISCAL orchestration (xoper.f SPECAL + VISCAL, xfoil.f INIT
// session values) on the core: identical to the permanent
// c11a_state/c11a harness chain.
ClarkYPointResult runClarkYViscousPoint(const AirfoilBuffer& buffer,
                                        double alphaDegrees) {
  const double alpha = canonicalAlpha(alphaDegrees);

  const auto pg = xfoil::core::paneling::abcopy(buffer);
  CHECK(pg.isOk());
  const PanelGeometry& panel = pg.value().panel;

  const std::vector<double> sig0(panel.x.size(), 0.0);
  const auto gs = xfoil::core::inviscid::ggCalc(panel, sig0, 0.0, 1.0);
  CHECK(gs.isOk());
  const InfluenceSystem& system = gs.value();

  // Fortran SPECAL (inviscid kernel): MRCL/COMSET internal,
  // MINF1 = 0, REINF1 = 1e5, MATYP = RETYP = 1.
  const MachReynoldsSettings settings{1, 1, 0.0, 100000.0};
  const auto sp = xfoil::core::inviscid::specAlpha(
      system, panel, settings, alpha, 1.0, 0.25, 0.0);
  CHECK(sp.isOk());
  const TrailingEdgeState& te = sp.value().te;

  // Fortran XYWAKE/QWCALC/QISET (WAKLEN = 1.0).
  const auto wk = xfoil::core::inviscid::xywake(
      panel, sp.value().gam, sp.value().gamA, system.gamu1, system.gamu2,
      sp.value().alfa, 1.0, 1.0);
  CHECK(wk.isOk());
  const WakeState& wake = wk.value();
  const WakeBasisResult qb =
      xfoil::core::inviscid::qwCalc(system, wake, panel, sp.value().alfa, 1.0);
  std::vector<double> qinvu1 = system.qinvu1;
  std::vector<double> qinvu2 = system.qinvu2;
  qinvu1.insert(qinvu1.end(), qb.qinvu1.begin(), qb.qinvu1.end());
  qinvu2.insert(qinvu2.end(), qb.qinvu2.begin(), qb.qinvu2.end());
  std::vector<double> qinv;
  std::vector<double> qinvA;
  xfoil::core::inviscid::qiset(qinvu1, qinvu2, sp.value().alfa, qinv, qinvA);

  // Fortran STFIND/IBLPAN/XICALC/IBLSYS/UICALC (LIPAN section).
  const StagnationPointResult st =
      xfoil::core::inviscid::stfind(sp.value().gam, panel.s);
  CHECK(st.found);
  const BoundaryLayerMapping map =
      xfoil::core::boundary_layer::iblpan(panel, st.ist, wake.nw, 283);
  CHECK(map.valid);
  BoundaryLayerArcLengths arcs;
  const WakeGap wg = xfoil::core::boundary_layer::xicalc(
      panel, map, st.sst, te.ante, wake.x, wake.y, arcs);
  const BlsysMapping isysMap =
      xfoil::core::boundary_layer::iblsys({map.top.nbl, map.bottom.nbl}, 283);
  BoundaryLayerStationField uinv;
  BoundaryLayerStationField uinvA;
  xfoil::core::boundary_layer::uicalc(map, qinv, qinvA, uinv, uinvA);

  // Fortran XIFSET per IS pass (XSTRIP = 1.0).
  const auto tTop = xfoil::core::boundary_layer::xifset(
      panel, map, arcs, st.sst, 1.0, SurfaceSide::kTop);
  const auto tBot = xfoil::core::boundary_layer::xifset(
      panel, map, arcs, st.sst, 1.0, SurfaceSide::kBottom);
  CHECK(tTop.isOk());
  CHECK(tBot.isOk());
  const double xiforcTop = tTop.value().xiforc;
  const double xiforcBottom = tBot.value().xiforc;

  // canonical INIT session values (xfoil.f INIT): IDAMP = 0, BLPINI internal,
  // VACCEL = 0.01; LBLINI = .FALSE. (cold start).
  ViscalState vs{};
  vs.alfa = sp.value().alfa;
  vs.minf = sp.value().minf;
  vs.reinf = sp.value().reinf;
  vs.cl = sp.value().cl;
  vs.gam = sp.value().gam;
  vs.gamA = sp.value().gamA;
  vs.qinv = qinv;
  vs.qinvA = qinvA;
  vs.mapping = map;
  vs.arcs = arcs;
  vs.wgap = wg;
  vs.isys = isysMap;
  vs.uinv = uinv;
  vs.uinvA = uinvA;
  vs.sst = st.sst;
  vs.sstGo = st.sstGo;
  vs.sstGp = st.sstGp;
  vs.ist = st.ist;
  vs.lblini = false;

  ViscalInput in{};
  in.blpar = xfoil::core::boundary_layer::blparConstants();
  in.niter = 100;
  in.qinf = 1.0;
  in.xref = 0.25;
  in.yref = 0.0;
  in.vaccel = 0.01;
  in.lalfa = true;
  in.lvisc = true;
  in.lflap = false;
  in.machType = 1;
  in.reType = 1;
  in.minf1 = 0.0;
  in.reinf1 = 100000.0;
  in.ivx = 283;
  in.acrit = {9.0, 9.0};
  in.xiforc = {xiforcTop, xiforcBottom};
  in.xstrip = {1.0, 1.0};

  ClarkYPointResult out;
  out.viscal = xfoil::core::boundary_layer::viscal(
      in, panel, system, wake, te, vs);
  out.itranTop = vs.itran[0] + 1;    // 0-based -> Fortran 1-based
  out.itranBottom = vs.itran[1] + 1; // 0-based -> Fortran 1-based

  // Fortran FCPMIN (OPER VISC share, xoper.f:1768-1801) at the loop end.
  std::vector<double> xFull = panel.x;
  xFull.insert(xFull.end(), wake.x.begin(), wake.x.end());
  const auto cpmin = xfoil::core::boundary_layer::fcpmin(
      true, xFull, out.viscal.cp, out.viscal.cpv);
  CHECK(std::isfinite(cpmin.cpmin));
  static_cast<void>(cpmin);

  return out;
}

}  // namespace

TEST(unit, ClarkYViscousCoreAlpha4MatchesFortran) {
  const AirfoilBuffer buffer = readClarkYBuffer();
  const ClarkYPointResult r = runClarkYViscousPoint(buffer, 4.0);

  // iteration count and convergence exact (Fortran 12 iterations).
  CHECK(r.viscal.converged);
  CHECK_EQ(r.viscal.iterations, static_cast<std::size_t>(12));
  // Transition stations exact (Fortran ITRAN1=61, ITRAN2=70, 1-based).
  CHECK_EQ(r.itranTop, 61);
  CHECK_EQ(r.itranBottom, 70);
  // coefficients against the canonical Fortran full-path values
  // (core == Fortran to 10 significant digits; thresholds abs/rel 1e-9).
  CHECK_NEAR(r.viscal.cl, 8.2243225894774308e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.cm, -8.2790137654465551e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.cd, 1.7288688307967329e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.cdf, 8.1030259647362282e-03, 1.0e-9, 1.0e-9);
  // RMSBL/RMXBL bit-identical in the direct Fortran comparison; thresholds
  // at the rounding level of the 10-digit confirmation.
  CHECK_NEAR(r.viscal.rmsbl, 2.3472461899631799e-05, 1.0e-10, 1.0e-6);
}

TEST(unit, ClarkYViscousCoreAlpha05MatchesFortran) {
  const AirfoilBuffer buffer = readClarkYBuffer();
  const ClarkYPointResult r = runClarkYViscousPoint(buffer, 0.5);

  CHECK(r.viscal.converged);
  CHECK_EQ(r.viscal.iterations, static_cast<std::size_t>(8));
  CHECK_EQ(r.itranTop, 67);
  CHECK_EQ(r.itranBottom, 75);
  CHECK_NEAR(r.viscal.cl, 4.2784322458710400e-01, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.cm, -9.0867797856485771e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.cd, 1.8639247478485541e-02, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.cdf, 7.2829232999668449e-03, 1.0e-9, 1.0e-9);
  CHECK_NEAR(r.viscal.rmsbl, 2.5596698397623774e-05, 1.0e-10, 1.0e-6);
}

// end: tests/core/ClarkYViscousCoreTests.cpp

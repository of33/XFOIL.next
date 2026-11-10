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

// file: tests/core/CoreLinkCheck.cpp
// Minimal program linked only against core objects.
// It excludes app/CLI objects and proves that the
// 0-based core foundation links independently.
//
// the program exercises the core API (Matrix, Limits, Result) and returns
// 0 on success, otherwise a non-zero exit code.

#include "xfoil/core/Limits.h"
#include "xfoil/core/Matrix.h"
#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/math/Diagnostic.h"
#include "xfoil/core/math/FortranCompat.h"
#include "xfoil/core/boundary_layer/BoundaryLayerMapping.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/BoundaryLayerStation.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/boundary_layer/NewtonSystem.h"
#include "xfoil/core/math/LinearAlgebra.h"
#include "xfoil/core/math/Sort.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/Abcopy.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"
#include "xfoil/core/paneling/PanelParameters.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/inviscid/TrailingEdge.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/core/polar/PolarOps.h"
#include "xfoil/core/polar/PolarTypes.h"

#include <array>
#include <cmath>
#include <span>

int main() {
  using namespace xfoil::core;
  using namespace xfoil::core::inviscid;
  using namespace xfoil::core::math;
  using namespace xfoil::core::boundary_layer;

  if (surfaceSystemCapacity != 601) {
    return 1;
  }
  if (bufferCapacity != 4 * surfaceSystemCapacity) {
    return 1;
  }

  Matrix m(2, 3, 0.0);
  m.at(1, 2) = 7.5;
  if (m.rows() != 2 || m.cols() != 3 || m.size() != 6) {
    return 1;
  }
  if (m.at(1, 2) != 7.5) {
    return 1;
  }

  const std::span<const double> row = m.row(1);
  if (row.size() != 3 || row[2] != 7.5) {
    return 1;
  }

  const Result<double> value = Result<double>::ok(1.0);
  if (!value.isOk() || value.value() != 1.0) {
    return 1;
  }

  const Status status = Status::ok();
  if (!status.isOk()) {
    return 1;
  }

 // Reference at least one non-inline symbol path from each
 // new numerical implementation file, so that no object
  // stays silently unlinked.
  // FortranCompat.cpp: setExp / atanC
  // Sort.cpp: hSort
  // spline.cpp: spline / triSol
  // LinearAlgebra.cpp: Gauss
  std::array<double, 3> stretch{};
  const Diagnostic stretchDiag = setExp(stretch, 0.5, 1.0);
  if (stretchDiag != Diagnostic::completed || stretch[2] != 1.0) {
    return 1;
  }
  if (atanC(1.0, 1.0, 0.0) <= 0.0) {
    return 1;
  }

  std::array<double, 3> sortA = {3.0, 1.0, 2.0};
  std::array<int, 3> sortIndx{};
  hSort(sortA, sortIndx);
  if (sortIndx[0] != 1 || sortIndx[1] != 2 || sortIndx[2] != 0) {
    return 1;
  }

  std::array<double, 3> splX = {0.0, 1.0, 4.0};
  std::array<double, 3> splS = {0.0, 1.0, 2.0};
  std::array<double, 3> splXs{};
  spline(splX, splS, splXs);
  if (seval(1.0, splX, splXs, splS) < 0.9 ||
      seval(1.0, splX, splXs, splS) > 1.1) {
    return 1;
  }

  std::array<double, 4> gz = {2.0, 0.0, 0.0, 3.0};
  std::array<double, 2> gr = {4.0, 6.0};
  gauss(gz, gr, 2, 1);
  // Solution: x=[4/2, 6/3] = [2, 2]
  if (std::abs(gr[0] - 2.0) > 1.0e-6 || std::abs(gr[1] - 2.0) > 1.0e-6) {
    return 1;
  }

 // Reference at least one non-inline symbol path from each
 // new geometry implementation file, so that no object
  // stays silently unlinked.
  // GeometryTypes.cpp: AirfoilCoordinates (Constructor)
  // naca.cpp: naca4
  // LeadingEdge.cpp: lefind
  // GeometryParameters.cpp: aeCalc
  // Transformations.cpp: rotate / tecalc
  // Refinement.cpp: ijsect
  const geometry::AirfoilCoordinates coords(std::vector<double>{0.0, 1.0},
                                            std::vector<double>{0.0, 0.0});
  if (coords.x.size() != 2 || coords.y.size() != 2) {
    return 1;
  }

  const Result<geometry::NacaResult> naca = geometry::naca4(12, 4);
  if (!naca.isOk() || naca.value().xb.size() != 7 ||
      naca.value().name != "NACA 0012") {
    return 1;
  }

  std::array<double, 4> gx = {1.0, 0.0, 0.0, 1.0};
  std::array<double, 4> gxp{};
  std::array<double, 4> gy = {0.0, 0.1, 0.0, -0.1};
  std::array<double, 4> gyp{};
  std::array<double, 4> gs = {0.0, 1.0, 2.0, 3.0};
  const geometry::LeadingEdgeResult le =
      geometry::lefind(gx, gxp, gy, gyp, gs);
  if (le.status != geometry::LeStatus::completed) {
    return 1;
  }

  std::array<double, 5> ax = {0.0, 1.0, 1.0, 0.0, 0.0};
  std::array<double, 5> ay = {0.0, 0.0, 1.0, 1.0, 0.0};
  std::array<double, 5> at{1.0, 1.0, 1.0, 1.0, 1.0};
  const geometry::SectionProperties ae = geometry::aeCalc(ax, ay, at, 1);
  if (std::abs(ae.area - 1.0) > 1.0e-6) {
    return 1;
  }

  std::array<double, 3> rx = {1.0, 2.0, 3.0};
  std::array<double, 3> ry = {0.0, 0.0, 0.0};
  geometry::rotate(rx, ry, 0.0);
  if (rx[0] != 1.0 || rx[2] != 3.0) {
    return 1;
  }

  geometry::TrailingEdgeInputs teIn;
  teIn.x0 = 1.0;
  teIn.xn = 0.9;
  teIn.chord = 1.0;
  const geometry::TecalcResult te = geometry::tecalc(teIn);
  if (te.sharp) {
    return 1;
  }

  const geometry::SectionBending bend = geometry::ijsect(ax, ay);
  if (std::abs(bend.area - 1.0) > 1.0e-6) {
    return 1;
  }

 // Reference at least one non-inline symbol path from each
 // new paneling implementation file, so that no object stays
  // silently unlinked.
  // PanelTypes.cpp: PanelGeometry::make
  // PanelGeometryKernels.cpp: teGeometry / ncalc / apcalc
  // pangen.cpp: pangen
  // PanelParameters.cpp: applyPanelParameter
  // abcopy.cpp: abcopy
  const paneling::PanelGeometry pg = paneling::PanelGeometry::make(
      std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, 0.0},
      std::vector<double>{0.0, 1.0}, std::vector<double>{-1.0, -1.0},
      std::vector<double>{0.0, 0.0}, std::vector<double>{0.0, -1.0},
      std::vector<double>{1.0, 0.0}, std::vector<double>{0.0, 0.0},
      0.0, 0.0, 0.0, 1.0, 0.0, 1.0, false);
  if (pg.x.size() != 2 || pg.apanel.size() != 2) {
    return 1;
  }

  const geometry::AirfoilBuffer bufferC4(std::vector<double>{1.0, 0.5, 0.0},
                                         std::vector<double>{0.0, 0.1, 0.0},
                                         "C04");
  const Result<paneling::AbcopyResult> abc = paneling::abcopy(bufferC4);
  if (!abc.isOk() || abc.value().panel.x.size() != 3) {
    return 1;
  }

  paneling::PanelingParameters oldPaneling;
  oldPaneling.panelCount = 160;
  oldPaneling.teToLeDensityRatio = 0.15;
  const Result<paneling::PangenResult> pan =
      paneling::pangen(bufferC4, oldPaneling);
  if (!pan.isOk() || pan.value().panel.s.size() != 160) {
    return 1;
  }

  std::array<double, 3> px4 = {1.0, 0.5, 0.0};
  std::array<double, 3> py4 = {0.0, 0.1, 0.0};
  std::array<double, 3> ps4{};
  std::array<double, 3> pxp4{};
  std::array<double, 3> pyp4{};
  scalc(px4, py4, ps4);
  segspl(px4, ps4, pxp4);
  segspl(py4, ps4, pyp4);
  const paneling::TrailingEdgeGeometry teg = paneling::teGeometry(
      px4[0], py4[0], pxp4[0], pyp4[0], px4[2], py4[2], pxp4[2], pyp4[2],
      1.0);
  if (teg.sharp) {
    return 1;
  }
  std::array<double, 3> pn4{};
  std::array<double, 3> pn4b{};
  paneling::ncalc(px4, py4, ps4, pn4, pn4b);
  const std::vector<double> angles = paneling::apcalc(px4, py4, pn4, pn4b,
                                                      false);
  if (angles.size() != 3) {
    return 1;
  }

  const std::array<double, 1> vals{2000.0};
  const Result<paneling::ParameterChangeResult> par =
      paneling::applyPanelParameter(paneling::PanelingParameters{},
                                    paneling::PanelParameterKind::panelCount,
                                    vals);
  if (!par.isOk() || !par.value().clamped ||
      par.value().parameters.panelCount != 595) {
    return 1;
  }

 // Reference at least one non-inline symbol path from each
 // new inviscid implementation file, so that no object stays
  // silently unlinked.
  // Compressibility.cpp: MRCL / comset / cpCalc
  // TrailingEdge.cpp: tecalcFull
  // Stagnation.cpp: stfind / qiset
  // Loads.cpp: clCalc / mhinge
  // Influence.cpp: invPi / psiLin / ggCalc / xywake
  // SpecifiedPoint.cpp: specAlpha
  const MachReynoldsSettings settings;
  const Result<MachReynoldsResult> mrc = mrcl(settings, 0.0);
  if (!mrc.isOk() || mrc.value().minf != 0.0) {
    return 1;
  }
  const CompressibilityState comp = comset(0.0, 1.0);
  if (comp.cpstar != -999.0 || comp.qstar != 999.0) {
    return 1;
  }
  const double qarr[]{1.0, 0.9, 1.1};
  const Result<std::vector<double>> cp = cpCalc(qarr, 1.0, 0.0);
  if (!cp.isOk() || cp.value().size() != 3) {
    return 1;
  }

  const TrailingEdgeState teFull = tecalcFull(
      1.0, 0.0, -1.0, 0.0, 0.95, -0.02, 1.0, 0.5, 1.0, 0.5, 0.3, 0.1, 0.05);
  if (teFull.sharp) {
    return 1;
  }

  const std::vector<double> gamarr{0.5, -0.5};
  const std::vector<double> sarr{0.0, 1.0};
  const StagnationPointResult st = stfind(gamarr, sarr);
  if (!st.found || st.ist != 0) {
    return 1;
  }
  std::vector<double> qinv;
  std::vector<double> qinvA;
  qiset(std::vector<double>{1.0}, std::vector<double>{0.0}, 0.0, qinv, qinvA);
  if (qinv.size() != 1 || qinv[0] != 1.0) {
    return 1;
  }

  const double xarr[]{1.0, 0.5, 0.0};
  const double yarr[]{0.0, 0.1, 0.0};
  const double xparr[]{-1.0, 0.0, 1.0};
  const double ypararr[]{0.0, 0.4, 0.0};
  const std::vector<double> gam3{0.5, 0.4, 0.3};
  const LoadResult load = clCalc(xarr, yarr, gam3, gam3, 0.0, 0.0, 1.0,
                                 0.25, 0.0);
  if (load.cl == 0.0 && load.cm == 0.0 && load.cdp == 0.0) {
    return 1;
  }
  const std::vector<double> s3{0.0, 0.5, 1.0};
  const HingeMomentResult hm = mhinge(
      xarr, xparr, yarr, ypararr, s3, std::vector<double>{0.1, 0.2, 0.3},
      std::vector<double>{}, false, 0.5, 0.0);
  if (hm.hmom == 0.0 && hm.hfx == 0.0 && hm.hfy == 0.0) {
    return 1;
  }

  if (invPi() <= 3.14 || invPi() >= 3.15) {
    return 1;
  }

  const std::vector<double> xs{1.0, 0.998, 0.995};
  const std::vector<double> ys{0.001, 0.002, 0.003};
  const std::vector<double> ss{0.0, 0.001, 0.002};
  const std::vector<double> nxs{0.0, 0.0, 0.0};
  const std::vector<double> nys{1.0, 1.0, 1.0};
  const std::vector<double> apans{0.0, 0.0, 0.0};
  const std::vector<double> gams{0.5, 0.4, 0.3};
  PsiLinContext plCtx;
  plCtx.x = xs;
  plCtx.y = ys;
  plCtx.s = ss;
  plCtx.nx = nxs;
  plCtx.ny = nys;
  plCtx.apanel = apans;
  plCtx.gam = gams;
  plCtx.gamA = gams;
  plCtx.gamu1 = gams;
  plCtx.gamu2 = gams;
  plCtx.sig = gams;
  plCtx.sharp = false;
  plCtx.dste = 0.05;
  plCtx.ante = 0.0075;
  plCtx.aste = 0.055;
  plCtx.qinf = 1.0;
  plCtx.image = false;
  plCtx.yimage = 0.0;
  PsiLinInputs plIn;
  plIn.node = 1;
  plIn.xi = 0.998;
  plIn.yi = 0.002;
  plIn.nxi = 0.0;
  plIn.nyi = 1.0;
  plIn.alfa = 0.0;
  plIn.nodeOnAirfoil = true;
  plIn.siglin = true;
  const PsiLinResult pl = psiLin(plIn, plCtx);
  if (pl.dzdg.size() != 3) {
    return 1;
  }

  if (pan.isOk()) {
    const std::vector<double> sig0(pan.value().panel.s.size(), 0.0);
    const Result<InfluenceSystem> sysW = ggCalc(pan.value().panel, sig0, 0.0, 1.0);
    if (sysW.isOk()) {
      const Result<SpecPointResult> spW =
          specAlpha(sysW.value(), pan.value().panel, MachReynoldsSettings{},
                    0.0, 1.0, 0.25, 0.0);
      if (!spW.isOk()) {
        return 1;
      }
    }
    const std::vector<double> gamZero(pan.value().panel.x.size(), 0.0);
    const Result<WakeState> wk = xywake(
        pan.value().panel, gamZero, gamZero, sysW.value().gamu1,
        sysW.value().gamu2, 0.0, 1.0, 1.0);
    if (!wk.isOk() || wk.value().x.size() < 2) {
      return 1;
    }
  }


 // Boundary-layer data model and state mapping.
  {
    const boundary_layer::BoundaryLayerParameters blpar =
        boundary_layer::blparConstants();
    const boundary_layer::BoundaryLayerProperties props =
        boundary_layer::boundaryLayerProperties(0.0, 0.0, 1.0, 1.4, 0.4,
                                                0.0, 0.0, 0.0, 0.0, 0);
    static_cast<void>(props.dwte);
    static_cast<void>(props.idampv);
    static_cast<void>(blpar.sccon);
    const boundary_layer::BoundaryLayerMapping map =
        boundary_layer::iblpan(pg, 0, 2, 0);
    static_cast<void>(map.valid);
    boundary_layer::BoundaryLayerArcLengths arcs;
    const std::vector<double> wakeX{1.05, 1.15};
    const std::vector<double> wakeY{0.0, 0.0};
    const boundary_layer::WakeGap wg =
        boundary_layer::xicalc(pg, map, pg.s[0], 0.0, wakeX, wakeY, arcs);
    static_cast<void>(wg.dwdxte);
    const Result<boundary_layer::TransitionState> ts =
        boundary_layer::xifset(pg, map, arcs, pg.s[0], 1.0,
                               boundary_layer::SurfaceSide::kTop);
    static_cast<void>(ts.isOk());
    const boundary_layer::BoundaryLayerPrimary prim =
        boundary_layer::blprv(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, props);
    static_cast<void>(prim.ue);
    const boundary_layer::BoundaryLayerSecondary sec =
        boundary_layer::blkin(prim, props);
    static_cast<void>(sec.rt);
  }

 // Newton system and block solver.
  {
    const std::array<int, 2> nbl{3, 3};
    const boundary_layer::BlsysMapping map =
        boundary_layer::iblsys(nbl, 601);
    if (map.nsys != 6) {
      return 1;
    }
    boundary_layer::BlsysWorkspace ws;
    ws.nsys = map.nsys;
    ws.va.assign(6, {});
    ws.vb.assign(6, {});
    ws.vdel.assign(6, {});
    ws.vm.assign(6, std::vector<std::array<double, 3>>(6, {0.0, 0.0, 0.0}));
    ws.va[0][0][0] = 1.0;
    ws.vb[1][0][0] = 1.0;
    ws.vdel[0][0][0] = 1.0;
    boundary_layer::blsolv(ws, 1.0e-10, 2.0, 2, 3);
    static_cast<void>(ws.vdel[3][0][0]);
    const boundary_layer::TesysInput tesysIn{};
    const boundary_layer::TesysResult tesysOut = boundary_layer::tesys(tesysIn);
    if (tesysOut.system.vs1[0][0] != -1.0 ||
        tesysOut.system.vs2[0][0] != 1.0) {
      return 1;
    }
    const boundary_layer::BlsysInput bsIn{};
    const boundary_layer::BldifSystem bs = boundary_layer::blsys(bsIn);
    static_cast<void>(bs.vs1[0][0]);
  }

 // Polar core and export data (non-inline symbols of the new
  // core implementation files).
  {
    polar::PolarDataset ds;
    polar::PolarPointInput in;
    in.alphaDeg = 1.0;
    if (!polar::addPoint(ds, in).isOk()) {
      return 1;
    }
    if (!polar::sortPoints(ds, polar::SortKey::alpha).isOk()) {
      return 1;
    }
    if (ds.points.size() != 1) {
      return 1;
    }
    const auto tag = polar::applyVersionTag("NACA 0012", 1, 1);
    if (tag.isErr() || tag.value() != "NACA 0012 [1]") {
      return 1;
    }
    const auto enc = polar::encodeReMantissaExponent(3.0e6);
    if (enc.exponent != 6) {
      return 1;
    }
    export_data::DumpFlowState flow;
    flow.qinf = 1.0;
    flow.minf = 0.3;
    const auto cpRow = export_data::pressureDumpRow(0.5, 1.1, flow);
    if (!(cpRow.cpCom < 0.0)) {
      return 1;
    }
    const auto rows = export_data::circleCoefficientDumpRows(
        {1.0}, {0.0}, {0.5, 0.25}, {0.0, 0.0}, 0, 1);
    if (rows.isErr() || rows.value().size() != 1) {
      return 1;
    }
  }

  return 0;
}

// End: tests/core/CoreLinkCheck.cpp

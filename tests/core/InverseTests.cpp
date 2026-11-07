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

// file: tests/core/InverseTests.cpp
// Unit tests for the new 0-based core: circle-grid and coefficient kernels
// (EIWSET, PIQSUM, FTP, CNFILT, CNSYMM).
//
// the tests use only xfoil::core and no app, CLI, or 1-based code.
//
// reference comparison: the expected values come from the focused Fortran
// harness tests/fortran-reference/harnesses/inverse-design/
// circle_kernels_harness.f (reference data: tests/data/
// core-inverse-design-reference/circle_kernels_reference.txt), which
// calls the canonical routines unchanged. Fortran REAL is single
// precision; the new core computes in double. Formula and operation order
// are identical; the deviations lie in the single-to-double range.
//
// tolerance notes:
// - kFortranAbsTol/kFortranRelTol: smooth O(1) values.
// - kSumAbsTol (1e-5): PIQSUM via MC+1 terms (individual accuracy of the
// accumulation).
// - kNoiseAbsTol (1e-6): Fourier coefficients that should be exactly ~0;
// the Fortran individual accuracy returns noise up to ~5e-8.
// the covered kernels are exercised over the canonical grid dimensions,
// target-segment bounds, and structured error paths.

#include "TestSupport.h"
#include "OsmapLoader.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/inverse/InverseState.h"
#include "xfoil/core/inverse/CircleGeometry.h"
#include "xfoil/core/inverse/CircleKernels.h"
#include "xfoil/core/inverse/MixedInverseKernels.h"
#include "xfoil/core/inverse/OrrSommerfeldMap.h"
#include "xfoil/core/inverse/QSpecKernels.h"
#include "xfoil/core/inverse/WallProfileKernels.h"
#include "xfoil/core/math/Spline.h"

namespace {

using namespace xfoil::core;
using namespace xfoil::core::inverse;

constexpr double kFortranAbsTol = 1.0e-6;
constexpr double kFortranRelTol = 1.0e-5;
constexpr double kSumAbsTol = 1.0e-5;
constexpr double kNoiseAbsTol = 1.0e-6;
// Accepted accuracy deviation: quantities resulting from poorly
// conditioned operations at individual accuracy (AGTE as difference of two
// ATAN2 on SPLIND derivatives; PIQ endpoints via cubic extrapolation
// 3-3+1). The canonical Fortran reference is REAL(4); the core computes in
// double. Direct intermediate-value comparison (harness) shows deviations
// up to ~4e-5, formula and order identical.
constexpr double kPrecAbsTol = 1.0e-4;

CircleGrid makeGrid() {
  const Result<CircleGrid> r = setCircleGrid(257);
  if (r.isErr()) {
    throw std::runtime_error("setCircleGrid failed");
  }
  return r.value();
}

// Builds the NACA-2412 buffer contour as in the harness: NACA4 -> SCALC ->
// SPLIND(x), SPLIND(y) -> LEFIND. Reference: scinit_mapping_reference.txt.
struct Airfoil {
  std::vector<double> x, y, s, xp, yp;
  double sle{0.0};
};

Airfoil makeNaca2412() {
  const Result<xfoil::core::geometry::NacaResult> naca =
      xfoil::core::geometry::naca4(2412, 123);
  if (naca.isErr()) {
    throw std::runtime_error("naca4 failed");
  }
  Airfoil a;
  a.x = naca.value().xb;
  a.y = naca.value().yb;
  const std::size_t n = a.x.size();
  a.s.assign(n, 0.0);
  a.xp.assign(n, 0.0);
  a.yp.assign(n, 0.0);
  xfoil::core::math::scalc(a.x, a.y, a.s);
  xfoil::core::math::splind(a.x, a.s, a.xp, -999.0, -999.0);
  xfoil::core::math::splind(a.y, a.s, a.yp, -999.0, -999.0);
  const xfoil::core::geometry::LeadingEdgeResult le =
      xfoil::core::geometry::lefind(a.x, a.xp, a.y, a.yp, a.s);
  a.sle = le.sle;
  return a;
}

}  // namespace

TEST(unit, InverseSetCircleGridDimensions) {
  const CircleGrid g = makeGrid();
  // reference: circle_kernels_reference.txt GRID / DWC.
  CHECK_EQ(g.nc, 257);
  CHECK_EQ(g.mc, 64);
  CHECK_EQ(g.mct, 16);
  CHECK_NEAR(g.dwc, 2.454369328916e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.pi, 3.141592653589793, 1.0e-12, 1.0e-12);
}

TEST(unit, InverseSetCircleGridRejection) {
  // EIWSET: STOP for NC > ICX(257) -> structured capacity error.
  const Result<CircleGrid> over = setCircleGrid(258);
  CHECK(over.isErr());
  CHECK(over.error().code == ErrorCode::capacityExceeded);
  const Result<CircleGrid> tiny = setCircleGrid(1);
  CHECK(tiny.isErr());
  CHECK(tiny.error().code == ErrorCode::invalidArgument);
}

TEST(unit, InverseCircleGridWcAndEiw) {
  const CircleGrid g = makeGrid();
  // reference: circle_kernels_reference.txt EIW (IC=1..6).
  CHECK_NEAR(g.wc[0], 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(g.wc[1], 2.454369328916e-2, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(g.wc[5], 1.227184683084e-1, kFortranAbsTol, kFortranRelTol);

  const auto eiw = [&](int ic, int m) {
    return g.eiw[static_cast<std::size_t>(ic) *
                    static_cast<std::size_t>(g.mc + 1) +
                static_cast<std::size_t>(m)];
  };
  // EIW(IC,0) = (1,0) for all IC.
  CHECK_NEAR(eiw(0, 0).real(), 1.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(eiw(0, 0).imag(), 0.0, 1.0e-12, 1.0e-12);
  // EIW(2,1) = exp(i w2).
  CHECK_NEAR(eiw(1, 1).real(), 9.996988177299e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(eiw(1, 1).imag(), 2.454122900963e-2, kFortranAbsTol, kFortranRelTol);
  // EIW(2,MC) via cyclic indexing.
  CHECK_NEAR(eiw(1, g.mc).real(), -4.371138828674e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(eiw(1, g.mc).imag(), 1.0, kFortranAbsTol, kFortranRelTol);
  // EIW(3,MC) ~ (-1, 0).
  CHECK_NEAR(eiw(2, g.mc).real(), -1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(eiw(2, g.mc).imag(), -8.742277657348e-8, kNoiseAbsTol, kFortranRelTol);
}

TEST(unit, InversePiqSum) {
  const CircleGrid g = makeGrid();
  CircleCoefficients c;
  c.cn.assign(static_cast<std::size_t>(g.mc + 1), std::complex<double>(0.0, 0.0));
  c.piq.assign(static_cast<std::size_t>(g.nc), std::complex<double>(0.0, 0.0));
  for (int m = 0; m <= g.mc; ++m) {
    c.cn[static_cast<std::size_t>(m)] = std::complex<double>(
        1.0 / static_cast<double>(m + 1),
        0.5 / static_cast<double>(m + 2));
  }
  const Status st = piqSum(g, c);
  CHECK(st.isOk());
  // reference: circle_kernels_reference.txt PIQ (IC=1..6).
  CHECK_NEAR(c.piq[0].real(), 4.759276390076e0, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[0].imag(), 1.887213468552e0, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[1].real(), 4.818885803223e0, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[1].imag(), 3.422989547253e-1, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[2].real(), 3.933920860291e0, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[2].imag(), -5.664465427399e-1, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[5].real(), 2.924356937408e0, kSumAbsTol, kFortranRelTol);
  CHECK_NEAR(c.piq[5].imag(), -4.693753719330e-1, kSumAbsTol, kFortranRelTol);
}

TEST(unit, InversePiqSumRejectsSizeMismatch) {
  const CircleGrid g = makeGrid();
  CircleCoefficients c;
  c.cn.assign(3, std::complex<double>(0.0, 0.0));
  c.piq.assign(2, std::complex<double>(0.0, 0.0));
  const Status st = piqSum(g, c);
  CHECK(st.isErr());
  CHECK(st.error().code == ErrorCode::sizeMismatch);
}

TEST(unit, InverseFourierTransform) {
  const CircleGrid g = makeGrid();
  CircleCoefficients c;
  c.cn.assign(static_cast<std::size_t>(g.mc + 1), std::complex<double>(0.0, 0.0));
  c.piq.assign(static_cast<std::size_t>(g.nc), std::complex<double>(0.0, 0.0));
  for (int ic = 0; ic < g.nc; ++ic) {
    const double w = g.wc[static_cast<std::size_t>(ic)];
    c.piq[static_cast<std::size_t>(ic)] =
        std::complex<double>(std::cos(w), std::sin(2.0 * w));
  }
  const Status st = fourierTransform(g, c);
  CHECK(st.isOk());
  // reference: circle_kernels_reference.txt CNFTP (M=0..6).
  CHECK_NEAR(c.cn[0].real(), 4.074536263943e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[0].imag(), -1.629909540668e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[1].real(), 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[1].imag(), 3.953470795182e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[2].real(), -1.000000357628e0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[2].imag(), 6.539440988718e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[3].real(), 1.201406121254e-7, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[6].real(), -2.887099981308e-8, kNoiseAbsTol, kFortranRelTol);
}

TEST(unit, InverseCoeffFilter) {
  const CircleGrid g = makeGrid();
  CircleCoefficients c;
  c.cn.assign(static_cast<std::size_t>(g.mc + 1), std::complex<double>(0.0, 0.0));
  c.piq.assign(static_cast<std::size_t>(g.nc), std::complex<double>(0.0, 0.0));
  for (int ic = 0; ic < g.nc; ++ic) {
    const double w = g.wc[static_cast<std::size_t>(ic)];
    c.piq[static_cast<std::size_t>(ic)] =
        std::complex<double>(std::cos(w), std::sin(2.0 * w));
  }
  CHECK(fourierTransform(g, c).isOk());
  coeffFilter(c, 2.0);
  // reference: circle_kernels_reference.txt CNFILT (M=0..6).
  CHECK_NEAR(c.cn[0].real(), 4.074536263943e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[1].real(), 9.987958669662e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[2].real(), -9.951907992363e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[3].real(), 1.188437863675e-7, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[6].real(), -2.764121020959e-8, kNoiseAbsTol, kFortranRelTol);
}

TEST(unit, InverseCoeffFilterZeroIsNoop) {
  CircleCoefficients c;
  c.cn = {std::complex<double>(2.0, 1.0), std::complex<double>(3.0, 4.0)};
  coeffFilter(c, 0.0);
  CHECK_NEAR(c.cn[0].real(), 2.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(c.cn[1].imag(), 4.0, 1.0e-12, 1.0e-12);
}

TEST(unit, InverseCoeffSymmetrize) {
  const CircleGrid g = makeGrid();
  CircleCoefficients c;
  c.cn.assign(static_cast<std::size_t>(g.mc + 1), std::complex<double>(0.0, 0.0));
  c.piq.assign(static_cast<std::size_t>(g.nc), std::complex<double>(0.0, 0.0));
  for (int ic = 0; ic < g.nc; ++ic) {
    const double w = g.wc[static_cast<std::size_t>(ic)];
    c.piq[static_cast<std::size_t>(ic)] =
        std::complex<double>(std::cos(w), std::sin(2.0 * w));
  }
  CHECK(fourierTransform(g, c).isOk());
  coeffFilter(c, 2.0);
  for (int m = 0; m <= g.mc; ++m) {
    const double re = c.cn[static_cast<std::size_t>(m)].real();
    c.cn[static_cast<std::size_t>(m)] =
        std::complex<double>(re, 3.0 + static_cast<double>(m));
  }
  coeffSymmetrize(c);
  // reference: circle_kernels_reference.txt CNSYMM (M=0..6).
  // M=0 keeps imaginary part (CNSYMM runs M=1..MC).
  CHECK_NEAR(c.cn[0].real(), 4.074536263943e-8, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[0].imag(), 3.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(c.cn[1].real(), 9.987958669662e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[1].imag(), 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(c.cn[2].real(), -9.951907992363e-1, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(c.cn[2].imag(), 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(c.cn[6].imag(), 0.0, 1.0e-12, 1.0e-12);
}

TEST(unit, InverseScinitMappingChain) {
  // end-to-end chain: NACA2412 -> SCINIT -> CNCALC -> ZCCALC -> ZCNORM.
  // reference: scinit_mapping_reference.txt (canonical Fortran routines).
  const Airfoil a = makeNaca2412();
  const CircleGrid g = makeGrid();

  CircleMappingState st;
  st.grid = g;
  initState(st);

  const Status sc =
      initMappingCoordinate(st, a.x, a.xp, a.y, a.yp, a.s, a.sle);
  CHECK(sc.isOk());

  // ANG: SLE, AGTE, AG0, QIM0.
  CHECK_NEAR(a.sle, 1.026789188385e0, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.agte, 8.835589885712e-2, kPrecAbsTol, kFortranRelTol);
  CHECK_NEAR(st.ag0, -1.774879932404e0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.qim0, -6.529438495636e-2, kPrecAbsTol, kFortranRelTol);

  // CPLX: DZTE, CHORDZ, ZLEOLD.
  CHECK_NEAR(st.dzte.real(), 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(st.dzte.imag(), 2.520015696064e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.chordz.real(), 1.0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.chordz.imag(), 0.0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.zleold.real(), 0.0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.zleold.imag(), 0.0, 1.0e-6, kFortranRelTol);

  // SCZ / SCZL: SC, ZC after SCINIT (normalized).
  CHECK_NEAR(st.sc[0], 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(st.sc[1], 1.113008838729e-4, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.sc[5], 2.506613265723e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.sc[256], 1.0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.zc[0].real(), 9.999999403954e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.zc[0].imag(), 1.260014600120e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.zc[1].real(), 9.997774362564e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.zc[256].real(), 1.000000119209e0, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.zc[256].imag(), -1.260014600120e-3, 1.0e-6, kFortranRelTol);
  // XCOLD/YCOLD are the real/imaginary parts of ZC.
  CHECK_NEAR(st.xcold[0], st.zc[0].real(), 1.0e-12, 1.0e-12);
  CHECK_NEAR(st.ycold[256], st.zc[256].imag(), 1.0e-12, 1.0e-12);

  // CNSC: CN after SCINIT (M=0..6).
  CHECK_NEAR(st.coeff.cn[0].real(), 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(st.coeff.cn[0].imag(), -3.688532486558e-2, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[1].real(), 9.101952314377e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[2].real(), -3.495810329914e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[2].imag(), -5.537058785558e-2, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[6].real(), -4.612033441663e-2, 1.0e-5, kFortranRelTol);

  // CNCALC with QC(w) = cos(w/2), LSYMM=false.
  std::vector<double> qc(static_cast<std::size_t>(g.nc));
  for (int ic = 0; ic < g.nc; ++ic) {
    qc[static_cast<std::size_t>(ic)] =
        std::cos(0.5 * g.wc[static_cast<std::size_t>(ic)]);
  }
  const Status cc = calcCoefficients(st, qc, false);
  CHECK(cc.isOk());

  // CNQC (M=0..6).
  CHECK_NEAR(st.coeff.cn[0].imag(), -3.688532486558e-2, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[1].real(), -8.776415884495e-2, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[1].imag(), -2.734433568463e-9, kNoiseAbsTol, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[2].real(), -4.358636587858e-2, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[6].real(), -1.413480844349e-2, 1.0e-5, kFortranRelTol);

  // PIQQC (IC=1,2). IC=1 is cubic extrapolation -> kPrecAbsTol.
  CHECK_NEAR(st.coeff.piq[0].real(), -3.824572265148e-1, kPrecAbsTol, kFortranRelTol);
  CHECK_NEAR(st.coeff.piq[0].imag(), -3.688523545861e-2, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.piq[1].real(), -3.458661735058e-1, kPrecAbsTol, kFortranRelTol);

  // ZCCALC(MCT): ZC, SC, ZC_CN(NC,M).
  const Status zc = zcCalc(st, g.mct);
  CHECK(zc.isOk());
  CHECK_NEAR(st.zc[0].real(), 4.0, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.zc[0].imag(), 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(st.sc[1], 3.698539148900e-5, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.zc[1].real(), 3.999704837799e0, 1.0e-5, kFortranRelTol);
  const std::size_t zcCnLast =
      static_cast<std::size_t>(g.nc - 1) * static_cast<std::size_t>(kZcCnCols);
  CHECK_NEAR(st.zc_cn[zcCnLast + 0].real(), 2.317056655884e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.zc_cn[zcCnLast + 0].imag(), 6.278897285461e0, 1.0e-5, kFortranRelTol);

  // ZCNORM(MCT): ZC, CN.
  const Status zn = zcNorm(st, g.mct);
  CHECK(zn.isOk());
  CHECK_NEAR(st.zc[0].real(), 3.039527655346e-6, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.zc[0].imag(), 1.604437275091e-4, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[0].imag(), 1.570636034012e0, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[1].real(), -8.776415884495e-2, 1.0e-5, kFortranRelTol);
}

TEST(unit, InverseQcalcMapgenChain) {
  // end-to-end: SCINIT -> QCCALC(ISPEC=1) -> QSPINT -> MAPGEN.
  // reference: qcalc_mapgen_reference.txt (canonical Fortran routines).
  const Airfoil a = makeNaca2412();
  const CircleGrid g = makeGrid();
  CircleMappingState st;
  st.grid = g;
  initState(st);
  CHECK(initMappingCoordinate(st, a.x, a.xp, a.y, a.yp, a.s, a.sle).isOk());

  const Result<CircleSpeedResult> qs = qCCalc(st, 1, 0.1, 0.0, 0.0, 1.0);
  CHECK(qs.isOk());
  const CircleSpeedResult& sp = qs.value();
  CHECK_NEAR(sp.alfa, 0.1, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(sp.cl, 9.436832070351e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(sp.cm, -6.399199366570e-2, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(sp.qcir[0], 0.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(sp.qcir[1], 7.242652177811e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(sp.qcir[5], 8.452936410904e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(sp.qcir[254], -7.637673020363e-1, 1.0e-5, kFortranRelTol);
  // TE point IC=NC: SINWE = (2 sin(w/2))**AGTE. In REAL(4) sin(pi) is
  // slightly negative -> SINWE=0 (QCIR=0); in double slightly positive ->
  // SINWE~0.04. Accepted accuracy deviation at the singular TE point
  // (Cl/CM is not affected; see the QSP/QC comparison).
  CHECK(std::abs(sp.qcir[256]) < 5.0e-2);
  CHECK_NEAR(sp.scir[1], 1.113008838729e-4, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(sp.xcir[0], 9.999999403954e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(sp.ycir[0], 1.260014600120e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(sp.scir[256], 1.0, 1.0e-6, kFortranRelTol);

  const Result<SpeedIntegrateResult> si = qspIntegrate(st, 0.1, sp.qcir, 1.0, 0.0);
  CHECK(si.isOk());
  CHECK_NEAR(si.value().cl, 9.436832070351e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(si.value().cm, -6.275077909231e-2, 1.0e-5, kFortranRelTol);

  std::vector<double> xn, yn;
  CHECK(mapGenerate(st, 0.0, xn, yn).isOk());
  CHECK_NEAR(xn[0], 9.999998807907e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(yn[0], 1.259966753423e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(xn[1], 9.997773766518e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(xn[256], 1.000000119209e0, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(yn[256], -1.259966753423e-3, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[0].imag(), -3.688526526093e-2, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[1].real(), 9.101952910423e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(st.coeff.cn[2].real(), -3.495810329914e-1, 1.0e-5, kFortranRelTol);
}

TEST(unit, InverseOrrSommerfeldNtcalc) {
  // OSMAP tricubic interpolation + NTCALC amplitude integration.
  // reference: osmap_nt_reference.txt (canonical Fortran routines, REAL(4)).
  const Result<OrrSommerfeldMap> mr =
      loadOsmapForTest("fortran-src/osmap.dat");
  CHECK(mr.isOk());
  const OrrSommerfeldMap& map = mr.value();
  CHECK(map.loaded());

  const OrrSommerfeldResult o1 = map.lookup(1000.0, 0.01, 2.5);
  CHECK(o1.ok);
  CHECK_NEAR(o1.alfr, 3.952297940850e-2, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o1.alfrW, 3.135886430740e0, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o1.alfrwW, -5.488605880737e1, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o1.alfi, 2.481101779267e-3, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o1.alfiW, -5.317299962044e-1, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o1.alfiwH, -1.285753011703e0, 1.0e-9, 1.0e-3);

  const OrrSommerfeldResult o2 = map.lookup(300.0, 0.01, 2.5);
  CHECK(o2.ok);
  CHECK_NEAR(o2.alfr, 3.438586741686e-2, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o2.alfi, 7.729288656265e-3, 1.0e-9, 1.0e-3);

  const OrrSommerfeldResult o3 = map.lookup(2000.0, 0.01, 2.5);
  CHECK(o3.ok);
  CHECK_NEAR(o3.alfr, 4.278814047575e-2, 1.0e-9, 1.0e-3);
  CHECK_NEAR(o3.alfi, -3.932026447728e-4, 1.0e-9, 1.0e-3);

  // NTCALC on synthetic airfoils.
  const int n = 10;
  const int nw = 5;
  std::vector<double> x(n), hk(n), th(n), ue(n), ve(n);
  for (int i = 1; i <= n; ++i) {
    const std::size_t k = static_cast<std::size_t>(i - 1);
    x[k] = 0.1 * i;
    hk[k] = 2.5;
    th[k] = 0.01 + 0.001 * i;
    ue[k] = 1.0;
    ve[k] = 1.0e-5;
  }
  const Result<NtCalcResult> nr = ntCalc(map, nw, x, hk, th, ue, ve, n);
  CHECK(nr.isOk());
  const NtCalcResult& nt = nr.value();
  CHECK_NEAR(nt.w[0], 9.090909004211e0, 1.0e-9, 1.0e-5);
  CHECK_NEAR(nt.w[1], 3.833604335785e0, 1.0e-9, 1.0e-5);
  CHECK_NEAR(nt.w[4], 2.874797880650e-1, 1.0e-9, 1.0e-5);
  const auto amp = [&](int i, int iw) {
    return nt.a[static_cast<std::size_t>(i - 1) * static_cast<std::size_t>(nw) +
                static_cast<std::size_t>(iw - 1)];
  };
  CHECK_NEAR(amp(1, 1), 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(amp(2, 3), 1.563709229231e-2, 1.0e-6, 1.0e-5);
  CHECK_NEAR(amp(10, 3), 5.101699382067e-2, 1.0e-6, 1.0e-5);
  CHECK_NEAR(amp(10, 4), 2.579382434487e-2, 1.0e-6, 1.0e-5);
  CHECK_NEAR(amp(10, 5), 0.0, 1.0e-12, 1.0e-12);
}

TEST(unit, InverseB3SolvBlockTridiagonal) {
  // 3x3 block-tridiagonal system (N=2, NRHS=1).
  // reference: wall_airfoils_reference.txt BSL lines (canonical B3SOLV).
  std::vector<double> a(18, 0.0), b(18, 0.0), c(18, 0.0), r(6, 0.0);
  const auto bi = [](int k, int l, int i) {
    return static_cast<std::size_t>((i - 1) * 9 + (l - 1) * 3 + (k - 1));
  };
  const auto ri = [](int k, int l, int i) {
    return static_cast<std::size_t>((i - 1) * 3 + (l - 1) * 3 + (k - 1));
  };
  a[bi(1, 1, 1)] = 2.0;
  a[bi(1, 2, 1)] = 1.0;
  c[bi(1, 1, 1)] = 1.0;
  b[bi(1, 1, 2)] = 1.0;
  a[bi(1, 1, 2)] = 2.0;
  a[bi(2, 2, 1)] = 3.0;
  c[bi(2, 2, 1)] = 1.0;
  b[bi(2, 2, 2)] = 1.0;
  a[bi(2, 2, 2)] = 3.0;
  a[bi(3, 3, 1)] = 4.0;
  c[bi(3, 3, 1)] = 1.0;
  b[bi(3, 3, 2)] = 1.0;
  a[bi(3, 3, 2)] = 4.0;
  r[ri(1, 1, 1)] = 3.0;
  r[ri(2, 1, 1)] = 4.0;
  r[ri(3, 1, 1)] = 5.0;
  r[ri(1, 1, 2)] = 3.0;
  r[ri(2, 1, 2)] = 4.0;
  r[ri(3, 1, 2)] = 5.0;
  CHECK(b3Solv(a, b, c, r, 2, 1).isOk());
  CHECK_NEAR(r[ri(1, 1, 1)], 3.333333134651e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(r[ri(2, 1, 1)], 1.0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(r[ri(3, 1, 1)], 1.0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(r[ri(1, 1, 2)], 1.333333373070e0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(r[ri(2, 1, 2)], 9.999999403954e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(r[ri(3, 1, 2)], 1.0, 1.0e-6, 1.0e-5);
}

TEST(unit, InverseFalknerSkanProfiles) {
  // Falkner-Skan neighbors (FS) with B3SOLV.
  // reference: wall_airfoils_reference.txt FSR/FSP lines (REAL(4)).
  const Result<FalknerSkanResult> r1 = falknerSkan(1, 1, 0.0, 0.0, 61, 10.0, 1.0);
  CHECK(r1.isOk());
  const FalknerSkanResult& a = r1.value();
  CHECK_NEAR(a.bspec, 0.0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(a.hspec, 2.587738990784e0, 1.0e-5, 1.0e-4);
  CHECK_NEAR(a.delta, 1.0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(a.s[0], 3.320080637932e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(a.u[10], 5.360198020935e-1, 1.0e-5, 1.0e-4);
  CHECK_NEAR(a.f[20], 1.685556650162e0, 1.0e-5, 1.0e-4);
  CHECK_NEAR(a.u[30], 9.914990067482e-1, 1.0e-5, 1.0e-4);
  CHECK_NEAR(a.f[60], 8.277013778687e0, 1.0e-5, 1.0e-4);

  const Result<FalknerSkanResult> r2 = falknerSkan(1, 2, 0.0, 2.5, 61, 10.0, 1.0);
  CHECK(r2.isOk());
  const FalknerSkanResult& b = r2.value();
  CHECK_NEAR(b.bspec, 3.911806643009e-2, 1.0e-6, 1.0e-4);
  CHECK_NEAR(b.hspec, 2.500000476837e0, 1.0e-5, 1.0e-4);
  CHECK_NEAR(b.u[10], 5.988073945045e-1, 1.0e-5, 1.0e-4);
  CHECK_NEAR(b.f[30], 3.460821628571e0, 1.0e-5, 1.0e-4);
  CHECK_NEAR(b.s[40], 2.049808390439e-4, 1.0e-9, 1.0e-3);

  const Result<FalknerSkanResult> r3 = falknerSkan(3, 2, 0.0, 3.0, 61, 10.0, 1.0);
  CHECK(r3.isOk());
  const FalknerSkanResult& d = r3.value();
  CHECK_NEAR(d.bspec, -6.844367086887e-2, 1.0e-6, 1.0e-4);
  CHECK_NEAR(d.hspec, 2.999999523163e0, 1.0e-5, 1.0e-4);
  CHECK_NEAR(d.delta, 7.964130043983e-1, 1.0e-6, 1.0e-4);
  CHECK_NEAR(d.u[10], 2.603587806225e-1, 1.0e-5, 1.0e-4);
  CHECK_NEAR(d.f[20], 9.096518754959e-1, 1.0e-5, 1.0e-4);
  CHECK_NEAR(d.f[60], 7.0, 1.0e-5, 1.0e-4);
}

TEST(unit, InversePrWallUWall) {
  // PRWALL (Spalding wall-profile Newton) + UWALL (wall profile U(Y)).
  // reference: wall_airfoils_reference.txt PRA/PRB/PRC/PRD/UWP lines (REAL(4)).
  WallProfileState st;
  CHECK(prWall(0.025, 0.01, 2000.0, 0.0, 0.0, 1.0, st).isOk());
  CHECK_NEAR(st.bb, 1.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(st.do_, 6.929558515549e-2, 1.0e-8, 1.0e-4);
  CHECK_NEAR(st.do_th, 6.929580688477e0, 1.0e-6, 1.0e-4);
  CHECK_NEAR(st.do_rt, -2.666653017513e-6, 1.0e-12, 1.0e-3);
  CHECK_NEAR(st.do_ms, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(st.ui, 2.532097697258e-1, 1.0e-7, 1.0e-4);
  CHECK_NEAR(st.ui_th, 2.149581909180e-3, 1.0e-9, 1.0e-2);
  CHECK_NEAR(st.ui_rt, 3.660545189632e-6, 1.0e-12, 1.0e-3);
  CHECK_NEAR(st.cf, 4.874271398876e-4, 1.0e-10, 1.0e-4);
  CHECK_NEAR(st.cf_th, 7.056970389385e-6, 1.0e-12, 1.0e-2);
  CHECK_NEAR(st.cf_rt, -5.179219542129e-8, 1.0e-14, 1.0e-3);
  // Pass-through fields remain at the input value (canonically unwritten).
  CHECK_NEAR(st.do_ds, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(st.do_uo, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(st.hs, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(st.cd, 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(st.cd_ct, 0.0, 1.0e-12, 1.0e-12);

  std::vector<double> y, u;
  CHECK(uwWall(0.01, 1.0, 0.045, 0.30, 2000.0, 0.004, 1.0, 65, y, u).isOk());
  CHECK_EQ(y.size(), 65u);
  CHECK_NEAR(y[0], 0.0, 1.0e-12, 1.0e-12);
  CHECK_NEAR(y[8], 2.735438756645e-3, 1.0e-9, 1.0e-4);
  CHECK_NEAR(u[8], 5.448272228241e-1, 1.0e-6, 1.0e-4);
  CHECK_NEAR(y[24], 9.620633907616e-3, 1.0e-9, 1.0e-4);
  CHECK_NEAR(u[24], 7.382850050926e-1, 1.0e-6, 1.0e-4);
  CHECK_NEAR(y[64], 4.500001668930e-2, 1.0e-8, 1.0e-4);
  CHECK_NEAR(u[64], 9.999999403954e-1, 1.0e-6, 1.0e-4);
}

TEST(unit, InverseQSpecKernels) {
  // SPLQSP/SMOOQ/QINCOM/GAMQSP/SYMQSP.
  // reference: qdes_kernels_reference.txt (canonical Fortran routines, REAL(4)).
  const int nsp = 21;
  auto make = [&]() {
    QSpecDistribution d;
    d.s.resize(nsp);
    d.q.resize(nsp);
    d.qp.assign(nsp, 0.0);
    for (std::size_t i = 0; i < static_cast<std::size_t>(nsp); ++i) {
      d.s[i] = static_cast<double>(i) / static_cast<double>(nsp - 1);
      d.q[i] = 1.0 - 0.5 * d.s[i];
    }
    return d;
  };
  const auto at = [](int one) { return static_cast<std::size_t>(one - 1); };

  QSpecDistribution d = make();
  CHECK(splQSpec(d).isOk());
  CHECK_NEAR(d.qp[at(1)], -4.999972581863e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.qp[at(5)], -5.000004172325e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.qp[at(13)], -5.000003576279e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.qp[at(21)], -4.999977350235e-1, 1.0e-6, 1.0e-5);

  d = make();
  CHECK(splQSpec(d).isOk());
  CHECK(smooQ(d, 1, nsp - 2, false).isOk());
  CHECK_NEAR(d.q[at(5)], 8.999998569489e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(9)], 7.999999523163e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(17)], 6.000000238419e-1, 1.0e-6, 1.0e-5);

  d = make();
  CHECK(splQSpec(d).isOk());
  CHECK(smooQ(d, 2, nsp - 3, true).isOk());
  CHECK_NEAR(d.q[at(5)], 8.999998569489e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(13)], 6.999999880791e-1, 1.0e-6, 1.0e-5);

  CHECK_NEAR(qIncompressible(1.0, 1.0, 0.0), 1.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(qIncompressible(1.2, 1.0, 0.3), 1.096243381500e0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(qIncompressible(0.0, 1.0, 0.5), 0.0, 1.0e-12, 1.0e-12);

  d = make();
  std::vector<double> qgamm(nsp);
  for (std::size_t i = 0; i < static_cast<std::size_t>(nsp); ++i) {
    qgamm[i] = std::cos(3.1415926535898 * d.s[i]);
  }
  int iq1 = 0, iq2 = 0;
  CHECK(gamQSpec(d, qgamm, 2.0, 0.5, -0.05, false, iq1, iq2).isOk());
  CHECK_EQ(iq1, 0);
  CHECK_EQ(iq2, nsp - 1);
  CHECK_NEAR(d.al, 2.0, 1.0e-9, 1.0e-9);
  CHECK_NEAR(d.cl, 0.5, 1.0e-9, 1.0e-9);
  CHECK_NEAR(d.cm, -5.000000074506e-2, 1.0e-9, 1.0e-5);
  CHECK_NEAR(d.q[at(5)], 8.090170025826e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(13)], -3.090171515942e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(21)], -1.0, 1.0e-6, 1.0e-5);

  d = make();
  for (std::size_t i = 0; i < static_cast<std::size_t>(nsp); ++i) {
    d.q[i] = 1.0 + 0.3 * d.s[i] - 0.1 * d.s[i] * d.s[i];
  }
  CHECK(symQSpec(d).isOk());
  CHECK_NEAR(d.s[at(1)], 0.0, 1.0e-9, 1.0e-5);
  CHECK_NEAR(d.s[at(5)], 1.999999880791e-1, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.s[at(21)], 1.0, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(1)], -9.999996423721e-2, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.q[at(13)], 2.000004053116e-2, 1.0e-6, 1.0e-5);
  CHECK_NEAR(d.qp[at(9)], 1.999999582767e-1, 1.0e-6, 1.0e-5);
}

TEST(unit, InverseMdesTeSetters) {
  // DZTSET/AGTSET (non-interactive branch) as setters on the state.
  // reference: mdes_setters_reference.txt (canonical Fortran routines).
  CircleMappingState st;
  st.grid = makeGrid();
  initState(st);
  setTeGap(st, 0.01, 0.002);
  CHECK_NEAR(st.dzte.real(), 9.999999776483e-3, 1.0e-8, 1.0e-5);
  CHECK_NEAR(st.dzte.imag(), 2.000000094995e-3, 1.0e-8, 1.0e-5);
  setTeAngleDeg(st, 15.0);
  CHECK_NEAR(st.agte, 8.333333581686e-2, 1.0e-8, 1.0e-5);
  setTeGap(st, -0.005, 0.003);
  CHECK_NEAR(st.dzte.real(), -4.999999888241e-3, 1.0e-8, 1.0e-5);
  CHECK_NEAR(st.dzte.imag(), 3.000000026077e-3, 1.0e-8, 1.0e-5);
  setTeAngleDeg(st, 0.0);
  CHECK_NEAR(st.agte, 0.0, 1.0e-12, 1.0e-12);
}

TEST(unit, InverseMapgamQspcir) {
  // MAPGAM/QSPCIR on the full MDES forward state.
  // reference: mapgam_qspcir_reference.txt (canonical Fortran routines).
  const Airfoil a = makeNaca2412();
  const CircleGrid g = makeGrid();

  FullInverseState fs;
  fs.circle.grid = g;
  initState(fs.circle);
  CHECK(initMappingCoordinate(fs.circle, a.x, a.xp, a.y, a.yp, a.s, a.sle).isOk());
  std::vector<double> qc(static_cast<std::size_t>(g.nc));
  for (int ic = 0; ic < g.nc; ++ic) {
    qc[static_cast<std::size_t>(ic)] =
        std::cos(0.5 * g.wc[static_cast<std::size_t>(ic)]);
  }
  CHECK(calcCoefficients(fs.circle, qc, false).isOk());
  CHECK(zcCalc(fs.circle, g.mct).isOk());
  CHECK(zcNorm(fs.circle, g.mct).isOk());

  const int n = static_cast<int>(a.x.size());
  fs.panel.x = a.x;
  fs.panel.xp = a.xp;
  fs.panel.y = a.y;
  fs.panel.yp = a.yp;
  fs.panel.s = a.s;
  fs.panel.n = n;
  fs.panel.sle = a.sle;
  fs.panel.xte = 0.5 * (a.x[0] + a.x[static_cast<std::size_t>(n - 1)]);
  fs.panel.yte = 0.5 * (a.y[0] + a.y[static_cast<std::size_t>(n - 1)]);
  fs.panel.xle = xfoil::core::math::seval(a.sle, a.x, a.xp, a.s);
  fs.panel.yle = xfoil::core::math::seval(a.sle, a.y, a.yp, a.s);
  fs.minf = 0.0;
  fs.qinf = 1.0;

  const Result<MappedGammaResult> mg1 = mapGam(fs, 1, 0.1, 0.0, 0.0);
  CHECK(mg1.isOk());
  // IAC=1: ALG given (unchanged), CLG/CMG computed.
  CHECK_NEAR(mg1.value().alfa, 1.000000014901e-1, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(mg1.value().cl, -5.974309444427e0, 1.0e-3, 1.0e-3);
  CHECK_NEAR(mg1.value().cm, 4.663342952728e0, 1.0e-3, 1.0e-3);
  CHECK_EQ(fs.nsp, 257);
  CHECK_NEAR(fs.ssple, 5.030183196068e-1, 1.0e-6, kFortranRelTol);
  const int idx[5] = {0, 64, 128, 192, 256};
  const double sspec[5] = {0.0, 1.464470326900e-1, 5.000001788139e-1,
                           8.535532355309e-1, 1.0};
  const double qgamm[5] = {0.0, -1.266610503197e0, -1.991395592690e0,
                           -1.549633622169e0, 0.0};
  const double xspoc[5] = {1.0, 7.052878141403e-1, 1.133780227974e-3,
                           7.017536163330e-1, 1.0};
  const double yspoc[5] = {1.260007848032e-3, 5.094487592578e-2,
                           6.025502458215e-3, -2.152687311172e-2,
                           -1.260007848032e-3};
  for (int k = 0; k < 5; ++k) {
    const std::size_t i = static_cast<std::size_t>(idx[k]);
    // TE point (k=4, IC=NC): documented QCCALC TE artefact
    // (sin(w/2) sign for w=2pi in REAL(4) vs. double) -> loose tolerance.
    const bool te = (k == 4);
    const double atol = te ? 5.0e-2 : 1.0e-6;
    const double rtol = te ? 1.0e-1 : kFortranRelTol;
    CHECK_NEAR(fs.sspec[i], sspec[k], 1.0e-6, kFortranRelTol);
    CHECK_NEAR(fs.qgamm[i], qgamm[k], atol, rtol);
    CHECK_NEAR(fs.xspoc[i], xspoc[k], 1.0e-6, kFortranRelTol);
    CHECK_NEAR(fs.yspoc[i], yspoc[k], 1.0e-6, kFortranRelTol);
  }

  // IAC=2: CLG given, ALG (Newton) and CMG computed.
  const Result<MappedGammaResult> mg2 = mapGam(fs, 2, 0.1, -6.0, 0.0);
  CHECK(mg2.isOk());
  CHECK_NEAR(mg2.value().alfa, 3.814530372620e-2, 1.0e-3, 1.0e-2);
  CHECK_NEAR(mg2.value().cl, -6.000000000000e0, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(mg2.value().cm, 4.571991920471e0, 1.0e-3, 1.0e-3);

  QSpecDistribution d;
  d.al = 0.1;
  d.cl = 0.0;
  d.cm = 0.0;
  CHECK(qSpecCirc(fs, d, 1).isOk());
  // IACQSP=1: AL unchanged, Cl/CM written back from QCCALC.
  CHECK_NEAR(d.al, 1.000000014901e-1, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(d.cl, -5.974309444427e0, 1.0e-3, 1.0e-3);
  CHECK_NEAR(d.cm, 4.663342952728e0, 1.0e-3, 1.0e-3);
  const double qsc[5] = {0.0, -1.266610503197e0, -1.991395592690e0,
                         -1.549633622169e0, 0.0};
  const double qscp[5] = {9.952515625000e3, -4.569067478180e0,
                          -4.003789424896e-1, 3.809844493866e0,
                          1.213185937500e4};
  for (int k = 0; k < 5; ++k) {
    const std::size_t i = static_cast<std::size_t>(idx[k]);
    const bool te = (k == 4);
    CHECK_NEAR(d.q[i], qsc[k], te ? 5.0e-2 : 1.0e-6,
               te ? 1.0e-1 : kFortranRelTol);
    CHECK_NEAR(d.qp[i], qscp[k], 1.0e-4, te ? 1.0e-1 : 1.0e-3);
  }

  // IACQSP=2: Cl given, AL (Newton) and CM written back.
  QSpecDistribution d2;
  d2.al = 0.1;
  d2.cl = -6.0;
  d2.cm = 0.0;
  CHECK(qSpecCirc(fs, d2, 2).isOk());
  CHECK_NEAR(d2.al, 3.814530372620e-2, 1.0e-3, 1.0e-2);
  CHECK_NEAR(d2.cl, -6.000000000000e0, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(d2.cm, 4.571991920471e0, 1.0e-3, 1.0e-3);
  const double q2sc[5] = {0.0, -1.359991788864e0, -1.999959111214e0,
                          -1.468362569809e0, 0.0};
  for (int k = 0; k < 5; ++k) {
    const std::size_t i = static_cast<std::size_t>(idx[k]);
    const bool te = (k == 4);
    CHECK_NEAR(d2.q[i], q2sc[k], te ? 5.0e-2 : 1.0e-3,
               te ? 1.0e-1 : 1.0e-3);
  }
}

// IQSGET numerical subkernel: setTargetSegment.
// Fortran IQSGET (xqdes.f:969): numerical part = check the difference of
// the endpoints, canonical sort (MIN0/MAX0), LIQSET=.TRUE.; identical
// endpoints -> controlled error without mutation. Cursor/plot/prompt part
// (GETCURSORXY, QSPLOT, next plot point, KQTARG) remains outside this
// numerical test.
TEST(unit, InverseIqsgetSetTargetSegment) {
  using xfoil::core::inverse::QSpecTargetSegment;
  using xfoil::core::inverse::setTargetSegment;
  const int nsp = 40;

  // two different endpoints, swapped input -> canonical order
  QSpecTargetSegment seg;
  CHECK(setTargetSegment(seg, 29, 11, nsp).isOk());
  CHECK_EQ(seg.iq1, 11);
  CHECK_EQ(seg.iq2, 29);
  CHECK(seg.liqset);

  // upper side (low indices) and lower side (high indices)
  QSpecTargetSegment up;
  CHECK(setTargetSegment(up, 1, 4, nsp).isOk());
  CHECK_EQ(up.iq1, 1);
  CHECK_EQ(up.iq2, 4);
  QSpecTargetSegment lo;
  CHECK(setTargetSegment(lo, nsp - 4, nsp - 1, nsp).isOk());
  CHECK_EQ(lo.iq1, nsp - 4);
  CHECK_EQ(lo.iq2, nsp - 1);

  // no wrap semantics: MIN0/MAX0 forces iq1 < iq2
  QSpecTargetSegment nw;
  CHECK(setTargetSegment(nw, nsp - 1, 0, nsp).isOk());
  CHECK_EQ(nw.iq1, 0);
  CHECK_EQ(nw.iq2, nsp - 1);
  CHECK(nw.iq1 < nw.iq2);

  // first and last valid position
  QSpecTargetSegment fl;
  CHECK(setTargetSegment(fl, 0, nsp - 1, nsp).isOk());
  CHECK_EQ(fl.iq1, 0);
  CHECK_EQ(fl.iq2, nsp - 1);

  // identical endpoints: controlled error, no partial mutation
  QSpecTargetSegment id;
  CHECK(setTargetSegment(id, 6, 8, nsp).isOk());
  const int iq1_before = id.iq1, iq2_before = id.iq2;
  const bool liq_before = id.liqset;
  CHECK(setTargetSegment(id, 8, 8, nsp).isErr());
  CHECK_EQ(id.iq1, iq1_before);
  CHECK_EQ(id.iq2, iq2_before);
  CHECK_EQ(id.liqset, liq_before);

  // outside 0..nsp-1: controlled error, no partial mutation
  CHECK(setTargetSegment(id, -1, 4, nsp).isErr());
  CHECK_EQ(id.iq1, iq1_before);
  CHECK_EQ(id.iq2, iq2_before);
  CHECK(setTargetSegment(id, 4, nsp, nsp).isErr());
  CHECK_EQ(id.iq1, iq1_before);
  CHECK_EQ(id.iq2, iq2_before);
}

// PERT numerical subkernel: perturbCoefficient.
// Fortran PERT (xmdes.f:1655): CN(M) += (dCnr,dCni) for a mode M chosen
// per READ; the downstream geometry/TE-gap Newton is numerically identical
// to MAPGEN (NCN=1). Here only the eigenmode part.
TEST(unit, InversePertPerturbCoefficient) {
  using xfoil::core::inverse::CircleMappingState;
  using xfoil::core::inverse::perturbCoefficient;
  CircleMappingState st;
  st.grid.mc = 8;
  st.grid.nc = 32;
  st.coeff.cn.assign(9, std::complex<double>(0.0, 0.0));
  st.coeff.cn[3] = std::complex<double>(1.0, -2.0);

  CHECK(perturbCoefficient(st, 3, 0.5, 0.25).isOk());
  CHECK_NEAR(st.coeff.cn[3].real(), 1.5, 1e-12, 0.0);
  CHECK_NEAR(st.coeff.cn[3].imag(), -1.75, 1e-12, 0.0);

  // mode outside 1..MC: controlled error, no mutation
  const std::complex<double> before = st.coeff.cn[3];
  CHECK(perturbCoefficient(st, 0, 1.0, 1.0).isErr());
  CHECK(perturbCoefficient(st, 9, 1.0, 1.0).isErr());
  CHECK_NEAR(st.coeff.cn[3].real(), before.real(), 0.0, 0.0);
  CHECK_NEAR(st.coeff.cn[3].imag(), before.imag(), 0.0, 0.0);
}

// MDES numerical initialization path (orchestrator):
// SCINIT -> MAPGAM(1,ALFA) -> CNCALC(QGAMM) -> QSPCIR.
// reference: mdes_init_reference.txt (canonical Fortran routines, NACA2412,
// ALG=0.1). Compares SSPEC/QGAMM (MAPGAM) and QSPEC (QSPCIR).
TEST(unit, InverseMdesInitOrchestrator) {
  const Airfoil a = makeNaca2412();
  const CircleGrid g = makeGrid();
  FullInverseState fs;
  fs.circle.grid = g;
  initState(fs.circle);
  const int n = static_cast<int>(a.x.size());
  fs.panel.x = a.x; fs.panel.xp = a.xp; fs.panel.y = a.y; fs.panel.yp = a.yp;
  fs.panel.s = a.s; fs.panel.n = n; fs.panel.sle = a.sle;
  fs.panel.xte = 0.5 * (a.x[0] + a.x[static_cast<std::size_t>(n - 1)]);
  fs.panel.yte = 0.5 * (a.y[0] + a.y[static_cast<std::size_t>(n - 1)]);
  fs.panel.xle = xfoil::core::math::seval(a.sle, a.x, a.xp, a.s);
  fs.panel.yle = xfoil::core::math::seval(a.sle, a.y, a.yp, a.s);
  fs.minf = 0.0; fs.qinf = 1.0;

  QSpecDistribution qspec;
  qspec.al = 0.1; qspec.cl = 0.0; qspec.cm = 0.0;
  double clgam = 0.0, cmgam = 0.0;
  CHECK(mdesInitializeFullInverse(fs, qspec, 0.1, clgam, cmgam).isOk());
  // MAPGAM(1,ALG=0.1) returns CLGAM/CMGAM as Fortran output.
  CHECK_NEAR(clgam, 9.436832070351e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(cmgam, -6.399199366570e-2, 1.0e-5, kFortranRelTol);
  // QSPCIR(1) writes ALQSP/CLQSP/CMQSP back into qspec.
  CHECK_NEAR(qspec.al, 1.000000014901e-1, 1.0e-6, kFortranRelTol);
  CHECK_NEAR(qspec.cl, 9.436841011047e-1, 1.0e-5, kFortranRelTol);
  CHECK_NEAR(qspec.cm, -6.399212777615e-2, 1.0e-5, kFortranRelTol);
  CHECK_EQ(fs.nsp, 257);
  CHECK_NEAR(fs.ssple, 5.030183196068e-1, 1.0e-6, kFortranRelTol);

  const int idx[6] = {0, 8, 64, 128, 192, 256};  // IC 1,9,65,129,193,257
  const double sspec[6] = {0.0, 6.136415060610e-3, 2.548791766167e-1,
                           4.998890161514e-1, 7.308301329613e-1, 1.0};
  const double qgamm[6] = {0.0, 8.897043466568e-1, 1.289042949677e0,
                           1.677450418472e0, -9.234686493874e-1, 0.0};
  const double qspec_ref[6] = {0.0, 8.897041082382e-1, 1.289043188095e0,
                               1.677450895309e0, -9.234684109688e-1, 0.0};
  for (int k = 0; k < 6; ++k) {
    const std::size_t i = static_cast<std::size_t>(idx[k]);
    // TE endpoint (IC=257): documented QCCALC TE artefact
    // (sin(w/2) sign for w=2pi, REAL(4) vs. double) -> loose tolerance.
    const bool te = (k == 5);
    const double atol = te ? 5.0e-2 : 1.0e-4;
    const double rtol = te ? 1.0e-1 : 1.0e-4;
    CHECK_NEAR(fs.sspec[i], sspec[k], 1.0e-6, kFortranRelTol);
    CHECK_NEAR(fs.qgamm[i], qgamm[k], atol, rtol);
    CHECK_NEAR(qspec.q[i], qspec_ref[k], atol, rtol);
  }
  // QSPECP only on inner samples (endpoints = stagnation spline artefact).
  const double qp_ref[4] = {8.750699043274e0, 1.192178726196e0,
                            -6.517644500732e1, -1.156390085816e-1};
  for (int k = 0; k < 4; ++k) {
    const std::size_t i = static_cast<std::size_t>(idx[k + 1]);
    CHECK_NEAR(qspec.qp[i], qp_ref[k], 1.0e-4, 1.0e-3);
  }
}

// QDES current Q capture (captureCurrentSpeed): SSPEC = S/S(N),
// XSPOC/YSPOC chord-projected, SSPLE = SLE/S(N), QGAMM = GAM.
TEST(unit, InverseQdesCaptureCurrentSpeed) {
  const Airfoil a = makeNaca2412();
  FullInverseState fs;
  const std::size_t n = a.x.size();
  fs.panel.x = a.x; fs.panel.xp = a.xp; fs.panel.y = a.y; fs.panel.yp = a.yp;
  fs.panel.s = a.s; fs.panel.n = static_cast<int>(n); fs.panel.sle = a.sle;
  fs.panel.xte = 0.5 * (a.x[0] + a.x[n - 1]);
  fs.panel.yte = 0.5 * (a.y[0] + a.y[n - 1]);
  fs.panel.xle = xfoil::core::math::seval(a.sle, a.x, a.xp, a.s);
  fs.panel.yle = xfoil::core::math::seval(a.sle, a.y, a.yp, a.s);
  std::vector<double> gam(n, 1.0);
  CHECK(captureCurrentSpeed(fs, gam).isOk());
  CHECK_EQ(fs.nsp, static_cast<int>(n));
  const double sn = a.s[n - 1];
  CHECK_NEAR(fs.sspec[0], 0.0, 1e-12, 0.0);
  CHECK_NEAR(fs.sspec[n - 1], 1.0, 1e-12, 0.0);
  CHECK_NEAR(fs.ssple, a.sle / sn, 1e-12, 1e-9);
  CHECK_NEAR(fs.qgamm[0], 1.0, 0.0, 0.0);
  // XSPOC/YSPOC are chord-projected normalized coordinates.
  const double chx = fs.panel.xte - fs.panel.xle;
  const double chy = fs.panel.yte - fs.panel.yle;
  const double chsq = chx * chx + chy * chy;
  const double xref = ((a.x[0] - fs.panel.xle) * chx +
                       (a.y[0] - fs.panel.yle) * chy) / chsq;
  CHECK_NEAR(fs.xspoc[0], xref, 1e-12, 1e-9);
}

// QDES numerical initialization path (qdesInitializeMixedInverse):
// captureCurrentSpeed -> GAMQSP(1). Checks the complete numerical initial
// state: current Q, normalized SSPEC, qspec AL/Cl/CM, 0-based target
// segment, LIQSET, QDOF initial values.
TEST(unit, InverseQdesMixedInit) {
  const Airfoil a = makeNaca2412();
  FullInverseState fs;
  const std::size_t n = a.x.size();
  fs.panel.x = a.x; fs.panel.xp = a.xp; fs.panel.y = a.y; fs.panel.yp = a.yp;
  fs.panel.s = a.s; fs.panel.n = static_cast<int>(n); fs.panel.sle = a.sle;
  fs.panel.xte = 0.5 * (a.x[0] + a.x[n - 1]);
  fs.panel.yte = 0.5 * (a.y[0] + a.y[n - 1]);
  fs.panel.xle = xfoil::core::math::seval(a.sle, a.x, a.xp, a.s);
  fs.panel.yle = xfoil::core::math::seval(a.sle, a.y, a.yp, a.s);
  std::vector<double> gam(n, 1.0);
  for (std::size_t i = 0; i < n; ++i) {
    gam[i] = 1.0 + 0.5 * std::cos(6.2831853 * static_cast<double>(i) /
                                      static_cast<double>(n));
  }
  QSpecDistribution qspec;
  int iq1 = -1, iq2 = -1;
  CHECK(qdesInitializeMixedInverse(fs, qspec, gam, 0.1, 0.5, -0.05, false,
                                   iq1, iq2).isOk());
  // current velocity and normalized SSPEC.
  CHECK_EQ(fs.nsp, static_cast<int>(n));
  CHECK_NEAR(fs.sspec[0], 0.0, 1e-12, 0.0);
  CHECK_NEAR(fs.sspec[n - 1], 1.0, 1e-12, 0.0);
  CHECK_NEAR(fs.qgamm[0], gam[0], 0.0, 0.0);
  // qspec AL/Cl/CM from GAMQSP.
  CHECK_NEAR(qspec.al, 0.1, 1e-12, 0.0);
  CHECK_NEAR(qspec.cl, 0.5, 1e-12, 0.0);
  CHECK_NEAR(qspec.cm, -0.05, 1e-12, 0.0);
  // qspec values = QGAMM, spline-derived.
  CHECK_NEAR(qspec.q[0], gam[0], 0.0, 0.0);
  CHECK_NEAR(qspec.s[0], 0.0, 1e-12, 0.0);
  // 0-based target segment (liqset=false -> [0, nsp-1]), LIQSET state.
  CHECK_EQ(iq1, 0);
  CHECK_EQ(iq2, static_cast<int>(n) - 1);
  // QDOF initial values zero.
  CHECK_NEAR(qspec.qdof0, 0.0, 0.0, 0.0);
  CHECK_NEAR(qspec.qdof1, 0.0, 0.0, 0.0);
  CHECK_NEAR(qspec.qdof2, 0.0, 0.0, 0.0);
  CHECK_NEAR(qspec.qdof3, 0.0, 0.0, 0.0);
}

// OSMAP numerical kernel without file/environment I/O: fromRaw validation
// and missing/invalid table as structured error.
TEST(unit, InverseOrrSommerfeldRawValidation) {
  using namespace xfoil::core::inverse;

  const std::size_t ntot = 2 * static_cast<std::size_t>(kOsMaxH) *
                           static_cast<std::size_t>(kOsMaxW) *
                           static_cast<std::size_t>(kOsMaxR);
  auto validRaw = [&]() {
    OrrSommerfeldRaw r;
    r.nr = 3; r.nw = 3; r.nh = 3; r.complete = true;
    r.rl = {2.0, 2.5, 3.0};
    r.wl = {0.0, 0.5, 1.0};
    r.hl = {1.0, 2.0, 3.0};
    r.ir1 = {1, 1, 1}; r.ir2 = {3, 3, 3};
    r.iw1 = {1, 1, 1}; r.iw2 = {3, 3, 3};
    r.a.assign(ntot, 0.0); r.ar = r.a; r.aw = r.a; r.ah = r.a;
    r.arw = r.a; r.arh = r.a; r.awh = r.a; r.arwh = r.a;
    return r;
  };

  // valid base table is assumed.
  CHECK(OrrSommerfeldMap::fromRaw(validRaw()).isOk());

  // empty table -> structured error
  CHECK(OrrSommerfeldMap::fromRaw(OrrSommerfeldRaw{}).isErr());
  // NR/NW/NH each 1 -> structured error
  { auto r = validRaw(); r.nr = 1; r.rl = {2.0}; r.ir1 = {1}; r.ir2 = {1};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.nw = 1; r.wl = {0.0}; r.iw1 = {1}; r.iw2 = {1};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.nh = 1; r.hl = {1.0}; r.ir1 = {1}; r.ir2 = {3};
    r.iw1 = {1}; r.iw2 = {3};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // Dimension exceeds capacity -> structured error
  { auto r = validRaw(); r.nr = kOsMaxR + 1; r.rl.assign(static_cast<std::size_t>(kOsMaxR + 1), 0.0);
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // size conflict (rl length != nr) -> structured error
  { auto r = validRaw(); r.rl = {2.0};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // non-monotone rl/wl/hl -> structured error
  { auto r = validRaw(); r.rl = {2.0, 2.0, 3.0};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.wl = {0.0, -0.5, 1.0};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.hl = {1.0, 3.0, 2.0};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // invalid ir1/ir2/iw1/iw2 (outside / empty range)
  { auto r = validRaw(); r.ir2 = {4, 3, 3};  // > nr
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.ir1 = {2, 1, 1}; r.ir2 = {1, 3, 3};  // ir1>ir2
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.iw1 = {0, 1, 1};  // < 1
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // NaN/Inf in coordinates or table
  { auto r = validRaw(); r.rl[1] = std::numeric_limits<double>::quiet_NaN();
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.a[0] = std::numeric_limits<double>::infinity();
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }

  // Neighboring H levels without a shared IR cell (max(ir1) >= min(ir2)).
  { auto r = validRaw(); r.ir1 = {1, 2, 1}; r.ir2 = {1, 3, 3};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // Neighboring H levels without a shared IW cell (max(iw1) >= min(iw2)).
  { auto r = validRaw(); r.iw1 = {1, 2, 1}; r.iw2 = {1, 3, 3};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // coordinate span too large -> RINCR/WINCR not finite (Inf).
  { auto r = validRaw(); r.rl = {-1e308, 0.0, 1e308};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  { auto r = validRaw(); r.wl = {-1e308, 0.0, 1e308};
    CHECK(OrrSommerfeldMap::fromRaw(std::move(r)).isErr()); }
  // Extremely large but finite rsp/wsp for a very small increment:
  // no int conversion outside the range (no UB/OOB), instead
  // OK=false with exclusively finite values.
  {
    auto r = validRaw();
    r.rl = {0.0, 1e-9, 2e-9};  // rincr_ = 1e-9
    const Result<OrrSommerfeldMap> tr = OrrSommerfeldMap::fromRaw(std::move(r));
    CHECK(tr.isOk());
    const OrrSommerfeldResult e = tr.value().lookup(1e308, 1e308, 2.0);
    CHECK_FALSE(e.ok);
    CHECK(std::isfinite(e.alfr)); CHECK(std::isfinite(e.alfi));
    CHECK(std::isfinite(e.alfrR)); CHECK(std::isfinite(e.alfrW));
    CHECK(std::isfinite(e.alfrwW)); CHECK(std::isfinite(e.alfiwR));
  }

  // not loaded map: lookup OK=false, ntCalc structured error
  OrrSommerfeldMap empty;
  CHECK_FALSE(empty.loaded());
  CHECK_FALSE(empty.lookup(1000.0, 0.01, 2.5).ok);
  std::vector<double> v(4, 0.1);
  const Result<NtCalcResult> nr = ntCalc(empty, 3, v, v, v, v, v, 4);
  CHECK(nr.isErr());

  // lookup input guards: rsp<=0, wsp<=0 -> OK=false.
  const Result<OrrSommerfeldMap> mr = loadOsmapForTest("fortran-src/osmap.dat");
  CHECK(mr.isOk());
  const OrrSommerfeldMap& map = mr.value();
  for (double bad : {0.0, -1.0}) {
    CHECK_FALSE(map.lookup(bad, 0.01, 2.5).ok);
    CHECK_FALSE(map.lookup(1000.0, bad, 2.5).ok);
  }
  // NaN/Inf in rsp/wsp/hsp -> OK=false, only finite zero values.
  for (double bad : {std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity()}) {
    const OrrSommerfeldResult a = map.lookup(bad, 0.01, 2.5);
    const OrrSommerfeldResult b = map.lookup(1000.0, bad, 2.5);
    const OrrSommerfeldResult c = map.lookup(1000.0, 0.01, bad);
    CHECK_FALSE(a.ok); CHECK_FALSE(b.ok); CHECK_FALSE(c.ok);
    CHECK(std::isfinite(a.alfr)); CHECK(std::isfinite(b.alfr));
    CHECK(std::isfinite(c.alfr));
    CHECK_NEAR(a.alfr, 0.0, 0.0, 0.0);
    CHECK_NEAR(b.alfi, 0.0, 0.0, 0.0);
    CHECK_NEAR(c.alfrR, 0.0, 0.0, 0.0);
  }
  // Extremely large but finite rsp/wsp on the real map: OK=false,
  // only finite values (index clamping prevents UB/OOB).
  {
    const OrrSommerfeldResult e = map.lookup(1e308, 1e308, 2.5);
    CHECK_FALSE(e.ok);
    CHECK(std::isfinite(e.alfr)); CHECK(std::isfinite(e.alfi));
    CHECK(std::isfinite(e.alfrR)); CHECK(std::isfinite(e.alfiwW));
  }
}

// B3SOLV singularity paths: Fortran STOP on a null pivot -> C++ structured
// error. b3Solv mutates a/b/c/r in place; on a detected null pivot it aborts
// in a controlled way (documented in-place semantics up to the pivot,
// analogous to the partial Fortran state before STOP).
TEST(unit, InverseB3SolvSingularPivot) {
  const auto bi = [](int k, int l, int i) {
    return static_cast<std::size_t>((i - 1) * 9 + (l - 1) * 3 + (k - 1));
  };
  auto base = [&]() {
    std::vector<double> a(18, 0.0), b(18, 0.0), c(18, 0.0), r(6, 0.0);
    a[bi(1, 1, 1)] = 2.0; a[bi(2, 2, 1)] = 3.0; a[bi(3, 3, 1)] = 4.0;
    a[bi(1, 1, 2)] = 2.0; a[bi(2, 2, 2)] = 3.0; a[bi(3, 3, 2)] = 4.0;
    return std::make_tuple(a, b, c, r);
  };

  // singular first block: column KPIV=1 of block I=1 completely zero
  {
    auto [a, b, c, r] = base();
    a[bi(1, 1, 1)] = 0.0; a[bi(2, 1, 1)] = 0.0; a[bi(3, 1, 1)] = 0.0;
    CHECK(b3Solv(a, b, c, r, 2, 1).isErr());
  }
  // singular inner block: column KPIV=1 of block I=2 completely zero
  {
    auto [a, b, c, r] = base();
    a[bi(1, 1, 2)] = 0.0; a[bi(2, 1, 2)] = 0.0; a[bi(3, 1, 2)] = 0.0;
    CHECK(b3Solv(a, b, c, r, 2, 1).isErr());
  }
  // regular system stays solvable (regression)
  {
    auto [a, b, c, r] = base();
    CHECK(b3Solv(a, b, c, r, 2, 1).isOk());
  }
}

// ---: mandatory limits and controlled error cases ---

namespace {
// synthetic mixed state of size n for guard tests (niterq=0 causes
// no Newton iteration to run, but checks preconditions and segment build).
xfoil::core::inverse::MixedInverseState makeSyntheticMixed(int n) {
  xfoil::core::inverse::MixedInverseState st;
  const std::size_t m = static_cast<std::size_t>(n);
  st.x.assign(m, 0.0); st.y.assign(m, 0.0); st.s.assign(m, 0.0);
  st.xp.assign(m, 1.0); st.yp.assign(m, 0.0);
  st.nx.assign(m, 0.0); st.ny.assign(m, 1.0); st.apanel.assign(m, 0.0);
  st.gam.assign(m, 0.0); st.qspec.assign(m, 0.0);
  st.gamA.assign(m, 0.0); st.gamu1 = st.gamA; st.gamu2 = st.gamA; st.sig = st.gamA;
  st.qf0.assign(m, 0.0); st.qf1 = st.qf0; st.qf2 = st.qf0; st.qf3 = st.qf0;
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) * 0.01;
    st.x[static_cast<std::size_t>(i)] = t;
    st.s[static_cast<std::size_t>(i)] = t;
  }
  st.n = n; st.iq1 = 0; st.iq2 = 1; st.lcpXX = false;
  st.chord = 1.0; st.qinf = 1.0;
  return st;
}
}  // namespace

TEST(unit, InverseMixedCapacity) {
  using namespace xfoil::core::inverse;
  // N + 5 > IQX (370) -> controlled capacity error.
  auto over = makeSyntheticMixed(kMixedMaxN + 1);  // 366
  const Status sOver = mixedInverse(over, 0);
  CHECK(sOver.isErr());
  CHECK_EQ(static_cast<int>(sOver.error().code),
           static_cast<int>(xfoil::core::ErrorCode::capacityExceeded));
  // Maximum valid N = 365 (N+5 = 370) -> boundary passed.
  auto maxOk = makeSyntheticMixed(kMixedMaxN);  // 365
  CHECK(mixedInverse(maxOk, 0).isOk());
  // under it also valid.
  auto below = makeSyntheticMixed(kMixedMaxN - 1);  // 364
  CHECK(mixedInverse(below, 0).isOk());
}

TEST(unit, InverseMixedSegmentBounds) {
  using namespace xfoil::core::inverse;
  const int n = 40;
  // minimal valid segment (2 points, [0,1]).
  auto minSeg = makeSyntheticMixed(n);
  minSeg.iq1 = 0; minSeg.iq2 = 1;
  CHECK(mixedInverse(minSeg, 0).isOk());
  // maximal valid segment (entire contour, [0,n-1]).
  auto maxSeg = makeSyntheticMixed(n);
  maxSeg.iq1 = 0; maxSeg.iq2 = n - 1;
  CHECK(mixedInverse(maxSeg, 0).isOk());
  // segment on the upper side (low indices).
  auto upper = makeSyntheticMixed(n);
  upper.iq1 = 0; upper.iq2 = n / 4 - 1;
  CHECK(mixedInverse(upper, 0).isOk());
  // segment on the lower side (high indices).
  auto lower = makeSyntheticMixed(n);
  lower.iq1 = 3 * n / 4; lower.iq2 = n - 1;
  CHECK(mixedInverse(lower, 0).isOk());
  // Slope condition on/off (lcpXX) does not change validity.
  auto slopeOn = makeSyntheticMixed(n);
  slopeOn.iq1 = 4; slopeOn.iq2 = 19; slopeOn.lcpXX = true;
  CHECK(mixedInverse(slopeOn, 0).isOk());
  auto slopeOff = makeSyntheticMixed(n);
  slopeOff.iq1 = 4; slopeOff.iq2 = 19; slopeOff.lcpXX = false;
  CHECK(mixedInverse(slopeOff, 0).isOk());
  // controlled errors: iq1 > iq2, iq2 >= n, iq1 < 0.
  auto bad1 = makeSyntheticMixed(n); bad1.iq1 = 9; bad1.iq2 = 4;
  CHECK(bad1.n >= 1); CHECK(mixedInverse(bad1, 0).isErr());
  auto bad2 = makeSyntheticMixed(n); bad2.iq1 = 0; bad2.iq2 = n;
  CHECK(mixedInverse(bad2, 0).isErr());
  auto bad3 = makeSyntheticMixed(n); bad3.iq1 = -1; bad3.iq2 = 4;
  CHECK(mixedInverse(bad3, 0).isErr());
}

TEST(unit, InverseMixedValidation) {
  using namespace xfoil::core::inverse;
  const int n = 40;
  // contour/derivative vectors too short -> controlled error.
  for (int which = 0; which < 5; ++which) {
    auto st = makeSyntheticMixed(n);
    switch (which) {
      case 0: st.xp.pop_back(); break;
      case 1: st.yp.pop_back(); break;
      case 2: st.nx.pop_back(); break;
      case 3: st.ny.pop_back(); break;
      case 4: st.apanel.pop_back(); break;
    }
    CHECK(mixedInverse(st, 0).isErr());
  }
  // GAM basis/SIG vectors too short.
  for (int which = 0; which < 4; ++which) {
    auto st = makeSyntheticMixed(n);
    switch (which) {
      case 0: st.gamA.pop_back(); break;
      case 1: st.gamu1.pop_back(); break;
      case 2: st.gamu2.pop_back(); break;
      case 3: st.sig.pop_back(); break;
    }
    CHECK(mixedInverse(st, 0).isErr());
  }
  // ds == 0 (identical segment coordinates) -> controlled error.
  {
    auto st = makeSyntheticMixed(n);
    st.iq1 = 5; st.iq2 = 9;
    for (int i = 5; i <= 9; ++i) st.s[static_cast<std::size_t>(i)] = 0.05;
    CHECK(mixedInverse(st, 0).isErr());
  }
  // NaN in a segment coordinate -> ds not finite -> controlled error.
  {
    auto st = makeSyntheticMixed(n);
    st.iq1 = 5; st.iq2 = 9;
    st.s[static_cast<std::size_t>(9)] = std::numeric_limits<double>::quiet_NaN();
    CHECK(mixedInverse(st, 0).isErr());
  }
  // negative niterq -> controlled error.
  {
    auto st = makeSyntheticMixed(n);
    CHECK(mixedInverse(st, -1).isErr());
  }
  // No partial mutation for precondition errors: gam/qspec/qf0 unchanged.
  {
    auto st = makeSyntheticMixed(n);
    for (int i = 0; i < n; ++i) {
      st.gam[static_cast<std::size_t>(i)] = 1.0 + 0.1 * static_cast<double>(i);
      st.qspec[static_cast<std::size_t>(i)] = 0.5 * static_cast<double>(i);
    }
    const std::vector<double> gamBefore = st.gam;
    const std::vector<double> qspecBefore = st.qspec;
    st.sig.pop_back();  // precondition error
    CHECK(mixedInverse(st, 1).isErr());
    for (int i = 0; i < n; ++i) {
      CHECK_NEAR(st.gam[static_cast<std::size_t>(i)],
                 gamBefore[static_cast<std::size_t>(i)], 0.0, 0.0);
      CHECK_NEAR(st.qspec[static_cast<std::size_t>(i)],
                 qspecBefore[static_cast<std::size_t>(i)], 0.0, 0.0);
    }
  }
}

TEST(unit, InverseCalcCoefficientsSize) {
  using namespace xfoil::core::inverse;
  const Result<CircleGrid> r = setCircleGrid(40);
  CHECK(r.isOk());
  CircleMappingState st;
  st.grid = r.value();
  // Wrong coefficient count (qc size != NC) -> controlled error.
  std::vector<double> wrong(static_cast<std::size_t>(st.grid.nc) - 1, 1.0);
  // error without partial mutation: cn stays unchanged (here empty).
  const std::size_t cnBefore = st.coeff.cn.size();
  const Status s = calcCoefficients(st, wrong, false);
  CHECK(s.isErr());
  CHECK_EQ(static_cast<int>(s.error().code),
           static_cast<int>(xfoil::core::ErrorCode::sizeMismatch));
  CHECK_EQ(st.coeff.cn.size(), cnBefore);
}

TEST(unit, InverseSplqspNonMonotone) {
  using namespace xfoil::core::inverse;
  QSpecDistribution d;
  d.s = {0.0, 0.3, 0.6, 0.5, 1.0};  // not strict monotonic (0.6 -> 0.5)
  d.q.assign(d.s.size(), 1.0);
  d.qp.assign(d.s.size(), 0.0);
  const Status s = splQSpec(d);
  CHECK(s.isErr());
  CHECK_EQ(static_cast<int>(s.error().code),
           static_cast<int>(xfoil::core::ErrorCode::outOfRange));
  // error without partial mutation: qp stays unchanged.
  for (std::size_t i = 0; i < d.qp.size(); ++i) CHECK_NEAR(d.qp[i], 0.0, 0.0, 0.0);
  // strict monotonic -> Success.
  QSpecDistribution ok;
  ok.s = {0.0, 0.3, 0.6, 0.8, 1.0};
  ok.q = {1.0, 1.2, 1.1, 0.9, 1.0};
  ok.qp.assign(ok.s.size(), 0.0);
  CHECK(splQSpec(ok).isOk());
}

TEST(unit, InverseOrrSommerfeldDomainClamp) {
  using namespace xfoil::core::inverse;
  const Result<OrrSommerfeldMap> mr = loadOsmapForTest("fortran-src/osmap.dat");
  CHECK(mr.isOk());
  const OrrSommerfeldMap& map = mr.value();
  CHECK(map.loaded());

  // reference value for mutation check.
  const double refAlfr = map.lookup(1000.0, 0.01, 2.5).alfr;

  // Within the domain: OK=true.
  const OrrSommerfeldResult in = map.lookup(1000.0, 0.01, 2.5);
  CHECK(in.ok);
  CHECK(std::isfinite(in.alfr));

  // outside the Re domain: OK=false, but a finite clamped value.
  const OrrSommerfeldResult outR = map.lookup(1.0e12, 0.01, 2.5);
  CHECK(!outR.ok);
  CHECK(std::isfinite(outR.alfr));
  CHECK(std::isfinite(outR.alfi));

  // outside the beta domain: OK=false, finite clamped value.
  const OrrSommerfeldResult outW = map.lookup(1000.0, 1.0e12, 2.5);
  CHECK(!outW.ok);
  CHECK(std::isfinite(outW.alfr));

  // outside the H domain: H is still clamped (Fortran behavior), the value
  // stays finite; OK is not reset for H.
  const OrrSommerfeldResult outH = map.lookup(1000.0, 0.01, 1.0e6);
  CHECK(std::isfinite(outH.alfr));
  CHECK(std::isfinite(outH.alfi));
  // clamping: value at the H-domain edge is identical.
  const double edgeH = map.lookup(1000.0, 0.01, 1.0e9).alfr;
  CHECK_NEAR(outH.alfr, edgeH, 0.0, 0.0);

  // No global mutation: lookup is const, map and reference value unchanged.
  CHECK(map.loaded());
  CHECK_NEAR(map.lookup(1000.0, 0.01, 2.5).alfr, refAlfr, 0.0, 0.0);
}

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

// File: src/core/paneling/Pangen.cpp
// Paneling kernel of the new 0-based core, directly
// ported from fortran-src/xfoil.f PANGEN.
//
// Formulas, operation order and special cases are adopted exactly from
// Fortran:
// - IPFAC is effective 5 (Fortran sets 3 and overwrites with 5).
// - FLOAT(NPAN/2) is an integer division.
// - TRISOL argument order: W2 is the main diagonal, W1 the lower,
// W3 the upper, W5/W4 the right-hand side.
// - For a sharp LE the smoothing system is split into two TRISOL part ranges
// ([0, IBLE) and [IBLE, NB) in Fortran indices).
// - NFRAC1 = (N*IBLE)/NB is an integer division.
// - Newton loop: max 20 iterations; endpoints fixed; RTF fudge I=2/I=NN-1;
// sharp-LE fix I=NN1; RLX adaptation in the loop; ends when |DMAX| < 1e-3.
// - Corner node insertion with shift and adjacent node fixes;
// N > IQX-1 = 600 is a STOP in Fortran and becomes a
// controlled error here.
// - The session flags and flap state of the Fortran are session state
// held outside the kernel. TECALC is used only for the geometry part
// (SIGTE/GAMTE = solver state).
//
// The Fortran STOP and WRITE text remains outer behavior; diagnostics
// are returned in structured form (the kernel does not write to stdout).

#include "xfoil/core/paneling/Pangen.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "xfoil/core/Limits.h"
#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"

namespace xfoil::core::paneling {

using math::curv;
using math::deval;
using math::scalc;
using math::segspl;
using math::seval;
using math::triSol;

namespace {

// Fortran: IQX = 601 (XFOIL.INC); corner node boundary IQX-1 = 600. The
// work vectors preserve the Fortran capacity (IQX, 0-based 601
// slots), so that the shift for the corner node insertion always
// has room for the new node.
constexpr std::size_t kPanelCapacity = xfoil::core::surfaceSystemCapacity;

// Fortran: Newton iteration limit.
constexpr int kMaxNewtonIterations = 20;

}  // namespace

// Fortran: SUBROUTINE PANGEN(SHOPAR).
Result<PangenResult> pangen(const geometry::AirfoilBuffer& buffer,
                            const PanelingParameters& params) {
  const std::size_t nb = buffer.x.size();
  if (nb != buffer.y.size()) {
    throw std::invalid_argument(
        "xfoil::core::paneling::pangen: buffer lengths must be equal ");
  }
  // Fortran: NB < 2 -> message, N = 0, return.
  if (nb < 2) {
    return Result<PangenResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "PANGEN: Buffer airfoil not available."));
  }
  // Controlled rejection below the numerically required
  // minimum size (Fortran would run degenerate loops).
  if (params.panelCount < 2) {
    return Result<PangenResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "pangen: panelCount must be at least 2 "));
  }
  // Capacity protection: the canonical PANGEN protection semantics end at
  // IQX-1 = 600 (Fortran STOP 'PANEL: Too many panels...', xfoil.f:2025 —
  // the only explicit panel-count bound in PANGEN). IQX-6 = 595
  // applies only to the GETPAN/PPAR input boundary
  // (xfoil.f:2218-2219) and does not apply to a direct PANGEN call.
  // NPAN=596..600 are processed directly within the 6*IQX Newton
  // arrays, 5*IBX SNEW storage and IZX=IQX+IWX coordinate arrays.
  // Above 600 there is no canonical protection semantics any more.
  if (params.panelCount > kMaxPanelNodeCount) {
    return Result<PangenResult>::fail(
        Error::make(ErrorCode::capacityExceeded,
                    "pangen: panelCount exceeds the canonical "
                    "PANGEN protection limit (600)"));
  }

  PangenResult out;

  // Fortran: IPFAC = 3; IPFAC = 5 (effective).
  constexpr int kIpfac = 5;
  const std::size_t n = static_cast<std::size_t>(params.panelCount);

  // Fortran: SCALC(XB,YB,SB,NB).
  std::vector<double> sb(nb);
  scalc(buffer.x, buffer.y, sb);

  // Fortran: SEGSPL(XB,XBP,SB,NB), SEGSPL(YB,YBP,SB,NB).
  std::vector<double> xbp(nb);
  std::vector<double> ybp(nb);
  segspl(buffer.x, sb, xbp);
  segspl(buffer.y, sb, ybp);

  // Fortran: SBREF = 0.5*(SB(NB)-SB(1)).
  const double sbref = 0.5 * (sb[nb - 1] - sb[0]);

  // Fortran: W5(I) = ABS(CURV(SB(I),...))*SBREF.
  std::vector<double> w5(nb);
  for (std::size_t i = 0; i < nb; ++i) {
    w5[i] = std::abs(curv(sb[i], buffer.x, xbp, buffer.y, ybp, sb)) * sbref;
  }

  // Fortran: LEFIND -> SBLE, CVLE.
  const geometry::LeadingEdgeResult le =
      geometry::lefind(buffer.x, xbp, buffer.y, ybp, sb);
  const double sble = le.sle;
  const double cvle =
      std::abs(curv(sble, buffer.x, xbp, buffer.y, ybp, sb)) * sbref;

  // Fortran: sharp-LE search; IBLE (1-based) or 0 = not present.
  std::size_t ible = 0;
  for (std::size_t i = 0; i + 1 < nb; ++i) {
    if (sble == sb[i] && sble == sb[i + 1]) {
      ible = i + 1;
      // Fortran: WRITE 'Sharp leading edge' -> structured.
      out.sharpLeadingEdge = true;
      break;
    }
  }
  const std::size_t ible0 = ible == 0 ? 0 : ible - 1;

  // Fortran: XBLE/YBLE/XBTE/YBTE/CHBSQ.
  const double xble = seval(sble, buffer.x, xbp, sb);
  const double yble = seval(sble, buffer.y, ybp, sb);
  const double xbte = 0.5 * (buffer.x[0] + buffer.x[nb - 1]);
  const double ybte = 0.5 * (buffer.y[0] + buffer.y[nb - 1]);
  const double chbsq = (xbte - xble) * (xbte - xble) +
                       (ybte - yble) * (ybte - yble);

  // Fortran: CVAVG (NK=3, SBK = SBLE + FRAC*SBREF/MAX(CVLE,20.0)).
  constexpr int kNk = 3;
  double cvsum = 0.0;
  for (int k = -kNk; k <= kNk; ++k) {
    const double frac =
        static_cast<double>(k) / static_cast<double>(kNk);
    const double sbk = sble + frac * sbref / std::max(cvle, 20.0);
    cvsum += std::abs(curv(sbk, buffer.x, xbp, buffer.y, ybp, sb)) * sbref;
  }
  double cvavg = cvsum / static_cast<double>(2 * kNk + 1);
  // Fortran: dummy curvature for a sharp LE.
  if (ible != 0) {
    cvavg = 10.0;
  }

  // Fortran: CC = 6.0*CVPAR.
  const double cc = 6.0 * params.bunchingParam;

  // Fortran: CVTE = CVAVG*CTERAT, W5(1) = W5(NB) = CVTE.
  const double cvte = cvavg * params.teToLeDensityRatio;
  w5[0] = cvte;
  w5[nb - 1] = cvte;

  // Fortran: SMOOL = MAX(1/MAX(CVAVG,20.0), 0.25/FLOAT(NPAN/2)).
  // FLOAT(NPAN/2) is an integer division.
  const double smool = std::max(
      1.0 / std::max(cvavg, 20.0),
      0.25 / static_cast<double>(params.panelCount / 2));
  const double smoosq = (smool * sbref) * (smool * sbref);

  // Fortran: tridiagonal smoothing system (W2 main diagonal, W1 lower,
  // W3 upper diagonal), corners unchanged.
  std::vector<double> w1(nb, 0.0);
  std::vector<double> w2(nb, 0.0);
  std::vector<double> w3(nb, 0.0);
  w2[0] = 1.0;
  w3[0] = 0.0;
  for (std::size_t i = 1; i + 1 < nb; ++i) {
    const double dsm = sb[i] - sb[i - 1];
    const double dsp = sb[i + 1] - sb[i];
    const double dso = 0.5 * (sb[i + 1] - sb[i - 1]);
    if (dsm == 0.0 || dsp == 0.0) {
      w1[i] = 0.0;
      w2[i] = 1.0;
      w3[i] = 0.0;
    } else {
      w1[i] = smoosq * (-1.0 / dsm) / dso;
      w2[i] = smoosq * (1.0 / dsp + 1.0 / dsm) / dso + 1.0;
      w3[i] = smoosq * (-1.0 / dsp) / dso;
    }
  }
  w1[nb - 1] = 0.0;
  w2[nb - 1] = 1.0;

  // Fortran: LE fix loop (GO TO 51 for modification).
  bool leFixed = false;
  for (std::size_t i = 1; i + 1 < nb && !leFixed; ++i) {
    if (sb[i] == sble || (ible != 0 && (i == ible0 || i == ible0 + 1))) {
      w1[i] = 0.0;
      w2[i] = 1.0;
      w3[i] = 0.0;
      w5[i] = cvle;
    } else if (sb[i - 1] < sble && sb[i] > sble) {
      double dsm = sb[i - 1] - sb[i - 2];
      double dsp = sble - sb[i - 1];
      double dso = 0.5 * (sble - sb[i - 2]);
      w1[i - 1] = smoosq * (-1.0 / dsm) / dso;
      w2[i - 1] = smoosq * (1.0 / dsp + 1.0 / dsm) / dso + 1.0;
      w3[i - 1] = 0.0;
      w5[i - 1] += smoosq * cvle / (dsp * dso);
      dsm = sb[i] - sble;
      dsp = sb[i + 1] - sb[i];
      dso = 0.5 * (sb[i + 1] - sble);
      w1[i] = 0.0;
      w2[i] = smoosq * (1.0 / dsp + 1.0 / dsm) / dso + 1.0;
      w3[i] = smoosq * (-1.0 / dsp) / dso;
      w5[i] += smoosq * cvle / (dsm * dso);
      leFixed = true;
    }
  }

  // Fortran: refinement areas (XOC, top/bottom).
  for (std::size_t i = 1; i + 1 < nb; ++i) {
    const double xoc =
        ((buffer.x[i] - xble) * (xbte - xble) +
         (buffer.y[i] - yble) * (ybte - yble)) /
        chbsq;
    if (sb[i] < sble) {
      if (xoc > params.upperRefinementRange[0] &&
          xoc < params.upperRefinementRange[1]) {
        w1[i] = 0.0;
        w2[i] = 1.0;
        w3[i] = 0.0;
        w5[i] = cvle * params.refinementDensityRatio;
      }
    } else {
      if (xoc > params.lowerRefinementRange[0] &&
          xoc < params.lowerRefinementRange[1]) {
        w1[i] = 0.0;
        w2[i] = 1.0;
        w3[i] = 0.0;
        w5[i] = cvle * params.refinementDensityRatio;
      }
    }
  }

  // Fortran: TRISOL (two part ranges for a sharp LE).
  if (ible == 0) {
    triSol(w2, w1, w3, w5);
  } else {
    triSol(std::span<double>(w2.data(), ible),
           std::span<const double>(w1.data(), ible),
           std::span<double>(w3.data(), ible),
           std::span<double>(w5.data(), ible));
    const std::size_t start = ible;
    triSol(std::span<double>(w2.data() + start, nb - start),
           std::span<const double>(w1.data() + start, nb - start),
           std::span<double>(w3.data() + start, nb - start),
           std::span<double>(w5.data() + start, nb - start));
  }

  // Fortran: CVMAX and normalization (degenerate as in Fortran).
  double cvmax = 0.0;
  for (std::size_t i = 0; i < nb; ++i) {
    cvmax = std::max(cvmax, std::abs(w5[i]));
  }
  for (std::size_t i = 0; i < nb; ++i) {
    w5[i] = w5[i] / cvmax;
  }

  // Fortran: SEGSPL(W5,W6,SB,NB).
  std::vector<double> w6(nb);
  segspl(w5, sb, w6);

  // Fortran: NN = IPFAC*(N-1)+1; RDSTE = 0.667; RTF = (RDSTE-1)*2+1.
  const std::size_t nn = static_cast<std::size_t>(kIpfac) * (n - 1) + 1;
  const double rdste = 0.667;
  const double rtf = (rdste - 1.0) * 2.0 + 1.0;

  std::vector<double> snew(nn, 0.0);
  std::size_t nn1 = 0;
  if (ible == 0) {
    const double dsavg = (sb[nb - 1] - sb[0]) /
                         (static_cast<double>(nn - 3) + 2.0 * rtf);
    snew[0] = sb[0];
    for (std::size_t i = 1; i + 1 < nn; ++i) {
      snew[i] = sb[0] + dsavg * (static_cast<double>(i - 1) + rtf);
    }
    snew[nn - 1] = sb[nb - 1];
  } else {
    // Fortran: NFRAC1 = (N*IBLE)/NB (integer division).
    const std::size_t nfrac1 = (n * ible) / nb;
    if (nfrac1 == 0) {
      // Fortran: NN1 = IPFAC*(NFRAC1-1)+1 < 1 would write outside the
      // range (UB); controlled rejection.
      return Result<PangenResult>::fail(
          Error::make(ErrorCode::invalidState,
                      "pangen: sharp LE position yields an invalid "
                      "node distribution"));
    }
    nn1 = static_cast<std::size_t>(kIpfac) * (nfrac1 - 1) + 1;
    const double dsavg1 = (sble - sb[0]) /
                          (static_cast<double>(nn1 - 2) + rtf);
    snew[0] = sb[0];
    for (std::size_t i = 1; i < nn1; ++i) {
      snew[i] = sb[0] + dsavg1 * (static_cast<double>(i - 1) + rtf);
    }
    const std::size_t nn2 = nn - nn1 + 1;
    const double dsavg2 = (sb[nb - 1] - sble) /
                          (static_cast<double>(nn2 - 2) + rtf);
    for (std::size_t i = 1; i + 1 < nn2; ++i) {
      snew[i - 1 + nn1] =
          sble + dsavg2 * (static_cast<double>(i - 1) + rtf);
    }
    snew[nn - 1] = sb[nb - 1];
  }

  // Fortran: Newton system vectors (quantity nn).
  std::vector<double> w1n(nn, 0.0);
  std::vector<double> w2n(nn, 0.0);
  std::vector<double> w3n(nn, 0.0);
  std::vector<double> w4n(nn, 0.0);

  bool converged = false;
  for (int iteration = 0; iteration < kMaxNewtonIterations && !converged;
       ++iteration) {
    double cv1 = seval(snew[0], w5, w6, sb);
    double cv2 = seval(snew[1], w5, w6, sb);
    double cvs1 = deval(snew[0], w5, w6, sb);
    double cvs2 = deval(snew[1], w5, w6, sb);

    double cavm = std::sqrt(cv1 * cv1 + cv2 * cv2);
    double cavmS1 = 0.0;
    double cavmS2 = 0.0;
    if (cavm != 0.0) {
      cavmS1 = cvs1 * cv1 / cavm;
      cavmS2 = cvs2 * cv2 / cavm;
    }

    for (std::size_t i = 1; i + 1 < nn; ++i) {
      const double dsm = snew[i] - snew[i - 1];
      const double dsp = snew[i] - snew[i + 1];
      const double cv3 = seval(snew[i + 1], w5, w6, sb);
      const double cvs3 = deval(snew[i + 1], w5, w6, sb);

      const double cavp = std::sqrt(cv3 * cv3 + cv2 * cv2);
      double cavpS2 = 0.0;
      double cavpS3 = 0.0;
      if (cavp != 0.0) {
        cavpS2 = cvs2 * cv2 / cavp;
        cavpS3 = cvs3 * cv3 / cavp;
      }

      const double fm = cc * cavm + 1.0;
      const double fp = cc * cavp + 1.0;
      const double rez = dsp * fp + dsm * fm;

      w1n[i] = -fm + cc * dsm * cavmS1;
      w2n[i] = fp + fm + cc * (dsp * cavpS2 + dsm * cavmS2);
      w3n[i] = -fp + cc * dsp * cavpS3;
      w4n[i] = -rez;

      cv1 = cv2;
      cv2 = cv3;
      cvs1 = cvs2;
      cvs2 = cvs3;
      cavm = cavp;
      cavmS1 = cavpS2;
      cavmS2 = cavpS3;
    }

    // Fortran: endpoints fixed.
    w2n[0] = 1.0;
    w3n[0] = 0.0;
    w4n[0] = 0.0;
    w1n[nn - 1] = 0.0;
    w2n[nn - 1] = 1.0;
    w4n[nn - 1] = 0.0;

    if (rtf != 1.0) {
      // Fortran I=2 (0-based 1).
      w4n[1] = -((snew[1] - snew[0]) + rtf * (snew[1] - snew[2]));
      w1n[1] = -1.0;
      w2n[1] = 1.0 + rtf;
      w3n[1] = -rtf;
      // Fortran I=NN-1 (0-based nn-2).
      w4n[nn - 2] = -((snew[nn - 2] - snew[nn - 1]) +
                      rtf * (snew[nn - 2] - snew[nn - 3]));
      w3n[nn - 2] = -1.0;
      w2n[nn - 2] = 1.0 + rtf;
      w1n[nn - 2] = -rtf;
    }

    // Fortran: sharp-LE fix I=NN1 (0-based nn1-1).
    if (ible != 0) {
      const std::size_t iLe = nn1 - 1;
      w1n[iLe] = 0.0;
      w2n[iLe] = 1.0;
      w3n[iLe] = 0.0;
      w4n[iLe] = sble - snew[iLe];
    }

    // Fortran: TRISOL(W2,W1,W3,W4,NN).
    triSol(w2n, w1n, w3n, w4n);

    // Fortran: RLX adaptation (RLX is changed in the loop).
    double rlx = 1.0;
    double dmax = 0.0;
    for (std::size_t i = 0; i + 1 < nn; ++i) {
      const double ds = snew[i + 1] - snew[i];
      const double dds = w4n[i + 1] - w4n[i];
      const double dsrat = 1.0 + rlx * dds / ds;
      if (dsrat > 4.0) {
        rlx = (4.0 - 1.0) * ds / dds;
      }
      if (dsrat < 0.2) {
        rlx = (0.2 - 1.0) * ds / dds;
      }
      dmax = std::max(std::abs(w4n[i]), dmax);
    }

    // Fortran: SNEW update.
    for (std::size_t i = 1; i + 1 < nn; ++i) {
      snew[i] = snew[i] + rlx * w4n[i];
    }

    // Fortran: |DMAX| < 1e-3 ended the loop.
    if (dmax < 1.0e-3) {
      converged = true;
    }
  }
  if (!converged) {
    // Fortran: 'Paneling convergence failed. Continuing anyway...'
    out.diagnostics.push_back(math::Diagnostic::limitReached);
  }

  // Fortran: work vectors with Fortran capacity for the
  // Corner node insertion.
  std::vector<double> px(kPanelCapacity, 0.0);
  std::vector<double> py(kPanelCapacity, 0.0);
  std::vector<double> ps(kPanelCapacity, 0.0);

  // Fortran: new panel node (IND = IPFAC*(I-1)+1, 0-based IPFAC*i).
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t ind = static_cast<std::size_t>(kIpfac) * i;
    ps[i] = snew[ind];
    px[i] = seval(snew[ind], buffer.x, xbp, sb);
    py[i] = seval(snew[ind], buffer.y, ybp, sb);
  }

  // Fortran: corner node insertion (GO TO 25 = next buffer point).
  std::size_t np = n;
  for (std::size_t ib = 0; ib + 1 < nb; ++ib) {
    if (sb[ib] != sb[ib + 1]) {
      continue;
    }
    out.cornersInserted += 1;
    const double xbcorn = buffer.x[ib];
    const double ybcorn = buffer.y[ib];
    const double sbcorn = sb[ib];

    // Fortran: search for the first panel behind the corner point; without
    // a hit nothing happens (Fortran DO 252 simply runs through).
    std::size_t iInsert = np;
    for (std::size_t i = 0; i < np; ++i) {
      if (ps[i] > sbcorn) {
        iInsert = i;
        break;
      }
    }
    if (iInsert == np) {
      continue;
    }

    // Fortran: Shift J=N..I,-1 -> 0-based j = np..iInsert+1.
    for (std::size_t j = np; j > iInsert; --j) {
      px[j] = px[j - 1];
      py[j] = py[j - 1];
      ps[j] = ps[j - 1];
    }
    np += 1;
    // Fortran: N > IQX-1 -> STOP 'PANEL: Too many panels...' ->
    // controlled error (no partial state).
    if (np > kMaxPanelNodeCount) {
      return Result<PangenResult>::fail(
          Error::make(ErrorCode::capacityExceeded,
                      "PANEL: Too many panels. Increase IQX in XFOIL.INC"));
    }
    px[iInsert] = xbcorn;
    py[iInsert] = ybcorn;
    ps[iInsert] = sbcorn;

    // Fortran: adjacent node shift.
    if (iInsert >= 2) {
      ps[iInsert - 1] = 0.5 * (ps[iInsert] + ps[iInsert - 2]);
      px[iInsert - 1] = seval(ps[iInsert - 1], buffer.x, xbp, sb);
      py[iInsert - 1] = seval(ps[iInsert - 1], buffer.y, ybp, sb);
    }
    if (iInsert + 2 <= np) {
      ps[iInsert + 1] = 0.5 * (ps[iInsert] + ps[iInsert + 2]);
      px[iInsert + 1] = seval(ps[iInsert + 1], buffer.x, xbp, sb);
      py[iInsert + 1] = seval(ps[iInsert + 1], buffer.y, ybp, sb);
    }
  }

  px.resize(np);
  py.resize(np);
  ps.resize(np);

  // Fortran: SCALC/SEGSPL x2/LEFIND on the final contour.
  scalc(px, py, ps);
  std::vector<double> pxp(np);
  std::vector<double> pyp(np);
  segspl(px, ps, pxp);
  segspl(py, ps, pyp);
  const geometry::LeadingEdgeResult leFinal =
      geometry::lefind(px, pxp, py, pyp, ps);

  const double panelSle = leFinal.sle;
  const double panelXle = seval(panelSle, px, pxp, ps);
  const double panelYle = seval(panelSle, py, pyp, ps);
  const double panelXte = 0.5 * (px[0] + px[np - 1]);
  const double panelYte = 0.5 * (py[0] + py[np - 1]);
  const double panelChord = std::sqrt(
      (panelXte - panelXle) * (panelXte - panelXle) +
      (panelYte - panelYle) * (panelYte - panelYle));

  // Fortran: panel size ratios (user information).
  double dsmin = 1000.0;
  double dsmax = -1000.0;
  for (std::size_t i = 0; i + 1 < np; ++i) {
    const double ds = ps[i + 1] - ps[i];
    if (ds == 0.0) {
      continue;
    }
    dsmin = std::min(dsmin, ds);
    dsmax = std::max(dsmax, ds);
  }
  dsmin *= static_cast<double>(np - 1) / ps[np - 1];
  dsmax *= static_cast<double>(np - 1) / ps[np - 1];
  out.dsminRatio = dsmin;
  out.dsmaxRatio = dsmax;

  // Fortran: TECALC (geometry share only; SHARP determines the TE
  // panel angle handling).
  const TrailingEdgeGeometry te =
      teGeometry(px[0], py[0], pxp[0], pyp[0], px[np - 1], py[np - 1],
                 pxp[np - 1], pyp[np - 1], panelChord);
  const bool sharpTe = te.sharp;

  // Fortran: NCALC.
  std::vector<double> nx(np);
  std::vector<double> ny(np);
  ncalc(px, py, ps, nx, ny);

  // Fortran: APCALC.
  std::vector<double> apanel = apcalc(px, py, nx, ny, sharpTe);

  // Fortran: GAP (identical Formula as DSTE from TECALC).
  out.teGap = std::sqrt(
      (px[0] - px[np - 1]) * (px[0] - px[np - 1]) +
      (py[0] - py[np - 1]) * (py[0] - py[np - 1]));

  out.panel = PanelGeometry::make(std::move(px), std::move(py),
                                  std::move(ps), std::move(pxp),
                                  std::move(pyp), std::move(nx),
                                  std::move(ny), std::move(apanel),
                                  panelSle, panelXle, panelYle, panelXte,
                                  panelYte, panelChord, sharpTe);
  return Result<PangenResult>::ok(std::move(out));
}

}  // namespace xfoil::core::paneling

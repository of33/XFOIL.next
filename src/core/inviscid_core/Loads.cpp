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

// File: src/core/inviscid_core/Loads.cpp
// Load-integration kernels of the new 0-based core, directly
// ported from fortran-src/xfoil.f (CLCALC) and fortran-src/xoper.f
// (MHINGE, inviscid branch).
//
// Formulas and operation order exactly as in Fortran:
// - CLCALC: SA/CA, BETA/BFAC/BETA_MSQ/BFAC_MSQ, before-run I=1, loop I=1..N
// (IP=I+1, I=N -> IP=1): CPG2/CPG2_MSQ/CPG2_ALF, DX/DY/DG, AX/AY/AG,
// DX_ALF/AG_ALF/AG_MSQ; CL+=DX*AG, CDP-=DY*AG,
// CM-=DX*(AG*AX+DG*DX/12)-DY*(AG*AY+DG*DY/12),
// CL_ALF+=DX*AG_ALF+AG*DX_ALF, CL_MSQ+=DX*AG_MSQ; Shift CPG1=CPG2.
// - MHINGE (inviscid branch): TOPS/BOTS via SINVRT, TOPX..BOTY via SEVAL,
// HMOM/HFX/HFY=0; loop I=2..N (segment top..bot), top chunk, inside flap,
// bottom chunk, inside flap, TE base contribution.

#include "xfoil/core/inviscid/Loads.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include "xfoil/core/math/Spline.h"

namespace xfoil::core::inviscid {

using math::seval;
using math::sinvrt;

// Fortran: SUBROUTINE CLCALC.
LoadResult clCalc(std::span<const double> x, std::span<const double> y,
                  std::span<const double> gam, std::span<const double> gamA,
                  double alfa, double minf, double qinf, double xref,
                  double yref) {
  const std::size_t n = x.size();
  if (n != y.size() || n != gam.size() || n != gamA.size()) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::clCalc: all lengths must be equal ");
  }

  LoadResult result;

  const double sa = std::sin(alfa);
  const double ca = std::cos(alfa);

  // Fortran: BETA/BFAC and derivatives.
  const double beta = std::sqrt(1.0 - minf * minf);
  const double betaMsq = -0.5 / beta;
  const double bfac = 0.5 * minf * minf / (1.0 + beta);
  const double bfacMsq =
      0.5 / (1.0 + beta) - bfac / (1.0 + beta) * betaMsq;

  result.cl = 0.0;
  result.cm = 0.0;
  result.cdp = 0.0;
  result.clAlf = 0.0;
  result.clMsq = 0.0;

  // XCP diagnostic quantity (no canonical Fortran field):
  // Re enters as an algebraic loop term built
  // exclusively from quantities already computed below (dx, AG, X). The
  // remaining outputs stay unchanged; the formula is bit-identical
  // to the previous public API path (proof:
  // tests/core/XcpDerivationTests.cpp).
  double xcp = 0.0;

  // Fortran: I = 1 before the run (0-based i0 = 0).
  std::size_t i = 0;
  const double cgInc0 = 1.0 - (gam[i] / qinf) * (gam[i] / qinf);
  double cpg1 = cgInc0 / (beta + bfac * cgInc0);
  double cpg1Msq = -cpg1 / (beta + bfac * cgInc0) *
                   (betaMsq + bfacMsq * cgInc0);
  const double cpiGam0 = -2.0 * gam[i] / (qinf * qinf);
  const double cpcCpi0 = (1.0 - bfac * cpg1) / (beta + bfac * cgInc0);
  double cpg1Alf = cpcCpi0 * cpiGam0 * gamA[i];

  // Fortran: Loop I=1..N (IP=I+1, I=N -> IP=1).
  for (std::size_t i0 = 0; i0 < n; ++i0) {
    std::size_t ip = i0 + 1;
    if (i0 == n - 1) {
      ip = 0;
    }

    const double cpg2Inc = 1.0 - (gam[ip] / qinf) * (gam[ip] / qinf);
    const double cpg2 = cpg2Inc / (beta + bfac * cpg2Inc);
    const double cpg2Msq =
        -cpg2 / (beta + bfac * cpg2Inc) * (betaMsq + bfacMsq * cpg2Inc);
    const double cpiGam = -2.0 * gam[ip] / (qinf * qinf);
    const double cpcCpi = (1.0 - bfac * cpg2) / (beta + bfac * cpg2Inc);
    const double cpg2Alf = cpcCpi * cpiGam * gamA[ip];

    const double dx = (x[ip] - x[i0]) * ca + (y[ip] - y[i0]) * sa;
    const double dy = (y[ip] - y[i0]) * ca - (x[ip] - x[i0]) * sa;
    const double dg = cpg2 - cpg1;

    const double ax = (0.5 * (x[ip] + x[i0]) - xref) * ca +
                      (0.5 * (y[ip] + y[i0]) - yref) * sa;
    const double ay = (0.5 * (y[ip] + y[i0]) - yref) * ca -
                      (0.5 * (x[ip] + x[i0]) - xref) * sa;
    const double ag = 0.5 * (cpg2 + cpg1);

    const double dxAlf = -(x[ip] - x[i0]) * sa + (y[ip] - y[i0]) * ca;
    const double agAlf = 0.5 * (cpg2Alf + cpg1Alf);
    const double agMsq = 0.5 * (cpg2Msq + cpg1Msq);

    result.cl += dx * ag;
    result.cdp -= dy * ag;
    result.cm -= dx * (ag * ax + dg * dx / 12.0);
    result.cm -= dy * (ag * ay + dg * dy / 12.0);
    // XCP diagnostics (same loop term as in the previous
    // public API path; formula and order unchanged).
    xcp += dx * ag * (x[ip] + x[i0]) / 2.0;
    // Fortran: Cl_ALF = Cl_ALF + DX*AG_ALF + AG*DX_ALF is
    // left-associative: ((Cl_ALF + DX*AG_ALF) + AG*DX_ALF). A
    // single += with two products on the RHS would add the products
    // first and thus yield a different rounding.
    result.clAlf += dx * agAlf;
    result.clAlf += ag * dxAlf;
    result.clMsq += dx * agMsq;

    cpg1 = cpg2;
    cpg1Alf = cpg2Alf;
    cpg1Msq = cpg2Msq;
  }

  if (std::fabs(result.cl) > 0.0) {
    result.centerOfPressureX = xcp / result.cl;
  } else {
    result.centerOfPressureX = 0.0;
  }

  return result;
}

// Fortran: SUBROUTINE MHINGE (Inviscid- and LVISC branch).
HingeMomentResult mhinge(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s,
                         std::span<const double> cpi,
                         std::span<const double> cpv, bool lvisc,
                         double xof, double yof) {
  // Canonical GETXYF initial-estimate path (xgdes.f 1887-1888).
  if (!s.empty() && !x.empty()) {
    const HingeArcGuess guess{s[0] + (x[0] - xof),
                              s[s.size() - 1] - (x[x.size() - 1] - xof)};
    return mhinge(x, xp, y, yp, s, cpi, cpv, lvisc, xof, yof, guess);
  }
  return mhinge(x, xp, y, yp, s, cpi, cpv, lvisc, xof, yof,
                HingeArcGuess{});
}

HingeMomentResult mhinge(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s,
                         std::span<const double> cpi,
                         std::span<const double> cpv, bool lvisc,
                         double xof, double yof,
                         const HingeArcGuess& guess) {
  const std::size_t n = x.size();
  if (n != xp.size() || n != y.size() || n != yp.size() || n != s.size()) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::mhinge: all lengths must be equal ");
  }
  if (cpi.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::mhinge: CPI must be at least n long ");
  }
  if (lvisc && cpv.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::mhinge: CPV must for LVISC at least n long "
        "be");
  }

  // Fortran: IF(LVISC) CPV, otherwise CPI (used equally in all
  // pressure-integration digits).
  const std::span<const double> cp = lvisc ? cpv : cpi;

  HingeMomentResult result;

  // Fortran: TOPS/BOTS via SINVRT (hinge location as input; the
  // canonical initial guess depends on the path: GETXYF
  // (xgdes.f 1887-1888) or LFLAP=true (xoper.f 2367-2368)).
  double tops = guess.tops;
  double bots = guess.bots;
  math::Diagnostic diag = sinvrt(tops, xof, x, xp, s);
  diag = sinvrt(bots, xof, x, xp, s);
  static_cast<void>(diag);

  const double topx = seval(tops, x, xp, s);
  const double topy = seval(tops, y, yp, s);
  const double botx = seval(bots, x, xp, s);
  const double boty = seval(bots, y, yp, s);

  result.hmom = 0.0;
  result.hfx = 0.0;
  result.hfy = 0.0;

  // Fortran: Loop I=2..N (Segment top..bot; 0-based i0 = I-1).
  for (std::size_t i0 = 1; i0 < n; ++i0) {
    if (s[i0 - 1] >= tops && s[i0] <= bots) {
      continue;
    }
    const double dx = x[i0] - x[i0 - 1];
    const double dy = y[i0] - y[i0 - 1];
    const double xmid = 0.5 * (x[i0] + x[i0 - 1]) - xof;
    const double ymid = 0.5 * (y[i0] + y[i0 - 1]) - yof;
    const double pmid = 0.5 * (cp[i0] + cp[i0 - 1]);
    result.hmom += pmid * (xmid * dx + ymid * dy);
    result.hfx -= pmid * dy;
    result.hfy += pmid * dx;
  }

  // Fortran: S interval with s=TOPS search (I=2..N; 0-based i0 = I-1).
  std::size_t iTop = n - 1;
  for (std::size_t i0 = 1; i0 < n; ++i0) {
    if (s[i0] > tops) {
      iTop = i0;
      break;
    }
  }

  // Fortran: top chunk TOPS..S(I-1).
  {
    const double dx = topx - x[iTop - 1];
    const double dy = topy - y[iTop - 1];
    const double xmid = 0.5 * (topx + x[iTop - 1]) - xof;
    const double ymid = 0.5 * (topy + y[iTop - 1]) - yof;
    double frac = 0.0;
    if (s[iTop] != s[iTop - 1]) {
      frac = (tops - s[iTop - 1]) / (s[iTop] - s[iTop - 1]);
    }
    const double topp = cp[iTop] * frac + cp[iTop - 1] * (1.0 - frac);
    const double pmid = 0.5 * (topp + cp[iTop - 1]);
    result.hmom += pmid * (xmid * dx + ymid * dy);
    result.hfx -= pmid * dy;
    result.hfy += pmid * dx;

    // Fortran: inside-flap contribution of the hinge to the top surface.
    const double dxIn = xof - topx;
    const double dyIn = yof - topy;
    const double xmidIn = 0.5 * (topx + xof) - xof;
    const double ymidIn = 0.5 * (topy + yof) - yof;
    result.hmom += pmid * (xmidIn * dxIn + ymidIn * dyIn);
    result.hfx -= pmid * dyIn;
    result.hfy += pmid * dxIn;
  }

  // Fortran: S interval with s=BOTS search (I=N..2, -1; 0-based i0).
  std::size_t iBot = 1;
  for (std::size_t k = n; k >= 2; --k) {
    if (s[k - 2] < bots) {
      iBot = k - 1;
      break;
    }
  }

  // Fortran: bottom chunk BOTS..S(I).
  {
    const double dx = x[iBot] - botx;
    const double dy = y[iBot] - boty;
    const double xmid = 0.5 * (botx + x[iBot]) - xof;
    const double ymid = 0.5 * (boty + y[iBot]) - yof;
    double frac = 0.0;
    if (s[iBot] != s[iBot - 1]) {
      frac = (bots - s[iBot - 1]) / (s[iBot] - s[iBot - 1]);
    }
    const double botp = cp[iBot] * frac + cp[iBot - 1] * (1.0 - frac);
    const double pmid = 0.5 * (botp + cp[iBot]);
    result.hmom += pmid * (xmid * dx + ymid * dy);
    result.hfx -= pmid * dy;
    result.hfy += pmid * dx;

    // Fortran: inside-flap contribution of the hinge to the bottom surface.
    const double dxIn = botx - xof;
    const double dyIn = boty - yof;
    const double xmidIn = 0.5 * (botx + xof) - xof;
    const double ymidIn = 0.5 * (boty + yof) - yof;
    result.hmom += pmid * (xmidIn * dxIn + ymidIn * dyIn);
    result.hfx -= pmid * dyIn;
    result.hfy += pmid * dxIn;
  }

  // Fortran: TE base-edge contribution.
  {
    const double dx = x[0] - x[n - 1];
    const double dy = y[0] - y[n - 1];
    const double xmid = 0.5 * (x[0] + x[n - 1]) - xof;
    const double ymid = 0.5 * (y[0] + y[n - 1]) - yof;
    const double pmid = 0.5 * (cp[0] + cp[n - 1]);
    result.hmom += pmid * (xmid * dx + ymid * dy);
    result.hfx -= pmid * dy;
    result.hfy += pmid * dx;
  }

  return result;
}

}  // namespace xfoil::core::inviscid

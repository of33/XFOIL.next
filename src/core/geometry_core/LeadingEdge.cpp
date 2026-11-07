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

// File: src/core/geometry_core/LeadingEdge.cpp
// Implementation of the LE pipeline, directly ported from
// fortran-src/xgeom.f.
//
// 0-based index mapping:
// Fortran X/XP/Y/YP/S(1..N) <-> C++ x/xp/y/yp/s[0..n-1]
// Fortran XTE = 0.5*(X(1)+X(N)) <-> 0.5*(x[0]+x[n-1])
// Fortran Guess loop DO 10 I=3,N-2 <-> k = 2..n-3 (k+1 = Fortran I)
// The Fortran DO variable I stays at the initial value 3 for the zero pass
// -> 0-based index 2; after a regular end I = N-1 -> index n-2.
// Fortran SLE = S(I) <-> sle = s[i]
// Fortran S(I)==S(I-1) <-> s[i]==s[i-1] (i >= 2, underflow-safe)

#include "xfoil/core/geometry/LeadingEdge.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>

#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry {

namespace {

// Safety check of the span lengths before each use.
void checkSpans(std::span<const double> x, std::span<const double> xp,
                std::span<const double> y, std::span<const double> yp,
                std::span<const double> s) {
  const std::size_t n = s.size();
  if (x.size() < n || xp.size() < n || y.size() < n || yp.size() < n) {
    throw std::invalid_argument(
        "lefind/xlfind/nsfind: span lengths inconsistent");
  }
}

}  // namespace

LeadingEdgeResult lefind(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s) {
  const std::size_t n = s.size();
  checkSpans(x, xp, y, yp, s);
  if (n < 3) {
    throw std::invalid_argument(
        "lefind: N < 3 (Fortran reads S(3)/S(2) also for Zero-Pass)");
  }

  // Fortran: DSEPS = (S(N)-S(1)) * 1.0E-5
  const double dseps = (s[n - 1] - s[0]) * 1.0e-5;
  // Fortran: XTE = 0.5*(X(1)+X(N)); YTE = 0.5*(Y(1)+Y(N))
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);

  // Fortran: DO 10 I=3,N-2 ... GO TO 11; end without hit: I = N-1
  // (regular) or I = 3 (zero pass). Both cases land at
  // SLE = S(I); 0-based indices: hit k -> index k, zero pass
  // -> index 2, regular end -> index n-2.
  std::size_t i = n - 2;
  for (std::size_t k = 2; k + 2 < n; ++k) {  // Fortran I = 3..N-2
    const double dxte = x[k] - xte;
    const double dyte = y[k] - yte;
    const double dx = x[k + 1] - x[k];
    const double dy = y[k + 1] - y[k];
    const double dotp = dxte * dx + dyte * dy;
    if (dotp < 0.0) {
      i = k;
      break;
    }
  }

  LeadingEdgeResult result;
  result.sle = s[i];  // Fortran: SLE = S(I)

  // Fortran: IF(S(I) .EQ. S(I-1)) RETURN (sharp LE)
  if (s[i] == s[i - 1]) {
    result.status = LeStatus::sharpLE;
    return result;
  }

  // Fortran: Newton iteration DO 20 ITER=1,50
  for (int iter = 1; iter <= 50; ++iter) {
    const double xle = math::seval(result.sle, x, xp, s);
    const double yle = math::seval(result.sle, y, yp, s);
    const double dxds = math::deval(result.sle, x, xp, s);
    const double dyds = math::deval(result.sle, y, yp, s);
    const double dxdd = math::d2val(result.sle, x, xp, s);
    const double dydd = math::d2val(result.sle, y, yp, s);

    const double xchord = xle - xte;
    const double ychord = yle - yte;

    // Fortran: RES = XCHORD*DXDS + YCHORD*DYDS
    // RESS = DXDS*DXDS + DYDS*DYDS + XCHORD*DXDD + YCHORD*DYDD
    const double res = xchord * dxds + ychord * dyds;
    const double ress =
        dxds * dxds + dyds * dyds + xchord * dxdd + ychord * dydd;

    double dsle = -res / ress;
    dsle = std::max(dsle, -0.02 * std::abs(xchord + ychord));
    dsle = std::min(dsle, 0.02 * std::abs(xchord + ychord));
    result.sle += dsle;
    if (std::abs(dsle) < dseps) {
      return result;
    }
  }

  // Fortran: WRITE 'LEFIND: LE point not found. Continuing...';
  // SLE = S(I)
  result.sle = s[i];
  result.status = LeStatus::limitReached;
  return result;
}

LeadingEdgeResult xlfind(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s) {
  const std::size_t n = s.size();
  checkSpans(x, xp, y, yp, s);
  if (n < 3) {
    throw std::invalid_argument(
        "xlfind: N < 3 (Fortran reads S(3)/S(2) also for Zero-Pass)");
  }

  // Fortran: DSLEN = S(N) - S(1); DSEPS = (S(N)-S(1)) * 1.0E-5
  const double dslen = s[n - 1] - s[0];
  const double dseps = dslen * 1.0e-5;

  // Fortran: DO 10 I=3,N-2; DX = X(I+1)-X(I); IF(DX > 0) Break
  std::size_t i = n - 2;
  for (std::size_t k = 2; k + 2 < n; ++k) {
    const double dx = x[k + 1] - x[k];
    if (dx > 0.0) {
      i = k;
      break;
    }
  }

  LeadingEdgeResult result;
  result.sle = s[i];  // Fortran: SLE = S(I)

  if (s[i] == s[i - 1]) {  // Fortran: sharp LE
    result.status = LeStatus::sharpLE;
    return result;
  }

  // Fortran: Newton iteration DO 20 ITER=1,50
  for (int iter = 1; iter <= 50; ++iter) {
    const double dxds = math::deval(result.sle, x, xp, s);
    const double dxdd = math::d2val(result.sle, x, xp, s);

    const double res = dxds;
    const double ress = dxdd;

    double dsle = -res / ress;
    dsle = std::max(dsle, -0.01 * std::abs(dslen));
    dsle = std::min(dsle, 0.01 * std::abs(dslen));
    result.sle += dsle;
    if (std::abs(dsle) < dseps) {
      return result;
    }
  }

  // Fortran: WRITE 'XLFIND: Left point not found. Continuing...';
  // SLE = S(I)
  result.sle = s[i];
  result.status = LeStatus::limitReached;
  return result;
}

LeadingEdgeResult nsfind(std::span<const double> x,
                         std::span<const double> xp,
                         std::span<const double> y,
                         std::span<const double> yp,
                         std::span<const double> s) {
  const std::size_t n = s.size();
  checkSpans(x, xp, y, yp, s);
  // Fortran: PARAMETER (NMAX=500); IF(N.GT.NMAX) STOP 'NSFIND: Local
  // array overflow. Increase NMAX.'
  constexpr std::size_t nmax = 500;
  if (n > nmax) {
    throw std::invalid_argument(
        "nsfind: N > NMAX(500) (Fortran-STOP 'NSFIND: Local array "
        "overflow. Increase NMAX.')");
  }
  if (n < 2) {
    throw std::invalid_argument("nsfind: N < 2");
  }

  std::array<double, nmax> a;
  std::array<double, nmax> b;
  std::array<double, nmax> c;
  std::array<double, nmax> cv;

  // Fortran: DO 3 I=1,N ; CV(I) = CURV(S(I),...)
  for (std::size_t i = 0; i < n; ++i) {
    cv[i] = math::curv(s[i], x, xp, y, yp, s);
  }

  // Fortran: SMOOL = 0.006*(S(N)-S(1)); SMOOSQ = SMOOL**2
  const double smool = 0.006 * (s[n - 1] - s[0]);
  const double smoosq = smool * smool;

  // Fortran: tridiagonal system A(1)=1, C(1)=0; I=2..N-1
  a[0] = 1.0;
  c[0] = 0.0;
  for (std::size_t i = 1; i + 1 < n; ++i) {  // Fortran I = 2..N-1
    const double dsm = s[i] - s[i - 1];
    const double dsp = s[i + 1] - s[i];
    const double dso = 0.5 * (s[i + 1] - s[i - 1]);
    if (dsm == 0.0 || dsp == 0.0) {
      // Fortran: keep the corner unchanged
      b[i] = 0.0;
      a[i] = 1.0;
      c[i] = 0.0;
    } else {
      b[i] = smoosq * (-1.0 / dsm) / dso;
      a[i] = smoosq * (1.0 / dsp + 1.0 / dsm) / dso + 1.0;
      c[i] = smoosq * (-1.0 / dsp) / dso;
    }
  }
  b[n - 1] = 0.0;
  a[n - 1] = 1.0;

  math::triSol(std::span<double>(a.data(), n),
               std::span<const double>(b.data(), n),
               std::span<double>(c.data(), n),
               std::span<double>(cv.data(), n));

  // Fortran: CVMAX = 0.; IVMAX = 0; DO 71 I=2,N-1
  double cvmax = 0.0;
  std::size_t ivmax = 0;  // Fortran IVMAX = 0 (1-based)
  for (std::size_t i = 1; i + 1 < n; ++i) {
    if (std::abs(cv[i]) > cvmax) {
      cvmax = std::abs(cv[i]);
      ivmax = i + 1;  // Fortran IVMAX = I (1-based)
    }
  }
  if (ivmax == 0) {
    // Fortran would read S(0) outside the range; controlled
    // error instead of reproducing the out-of-bounds access.
    throw std::invalid_argument(
        "nsfind: no inneres Kruemmungsmaximum (IVMAX == 0)");
  }

  // Fortran: I = IVMAX; IP = I+1; IM = I-1;
  // S(I)==S(IP) -> IP=I+2; S(I)==S(IM) -> IM=I-2
  const std::size_t i0 = ivmax - 1;  // 0-based
  std::size_t ip = i0 + 1;
  std::size_t im = i0 - 1;
  if (s[i0] == s[ip] && ip + 1 < n) {
    ip += 1;  // Fortran IP = I+2 (index guard instead of Fortran out-of-bounds)
  }
  if (s[i0] == s[im] && im >= 1) {
    im -= 1;  // Fortran IM = I-2 (index guard instead of Fortran out-of-bounds)
  }

  const double dsm = s[i0] - s[im];
  const double dsp = s[ip] - s[i0];
  const double cvsm = (cv[i0] - cv[im]) / dsm;
  const double cvsp = (cv[ip] - cv[i0]) / dsp;

  // Fortran: CVS = (CVSM*DSP + CVSP*DSM)/(DSP+DSM)
  // CVSS = 2.0*(CVSP-CVSM)/(DSP+DSM)
  const double cvs = (cvsm * dsp + cvsp * dsm) / (dsp + dsm);
  const double cvss = 2.0 * (cvsp - cvsm) / (dsp + dsm);

  // Fortran: DS = -CVS/CVSS; SLE = S(I) + DS
  const double ds = -cvs / cvss;

  LeadingEdgeResult result;
  result.sle = s[i0] + ds;
  result.status = LeStatus::completed;
  return result;
}

OppositePointResult sopps(double si, std::span<const double> x,
                          std::span<const double> xp,
                          std::span<const double> y,
                          std::span<const double> yp,
                          std::span<const double> s, double sle) {
  const std::size_t n = s.size();
  checkSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("sopps: N < 2");
  }

  OppositePointResult result;

  // Fortran: SLEN = S(N) - S(1)
  const double slen = s[n - 1] - s[0];

  // Fortran: XLE/YLE = SEVAL(SLE,...); XTE = 0.5*(X(1)+X(N)); CHORD; DXC/DYC
  double xle = math::seval(sle, x, xp, s);
  double yle = math::seval(sle, y, yp, s);
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);
  const double chord =
      std::sqrt((xte - xle) * (xte - xle) + (yte - yle) * (yte - yle));
  const double dxc = (xte - xle) / chord;
  const double dyc = (yte - yle) / chord;

  // Fortran: SI<SLE -> In=1, INOPP=N; otherwise In=N, INOPP=1
  std::size_t in;
  std::size_t inopp;
  if (si < sle) {
    in = 0;
    inopp = n - 1;
  } else {
    in = n - 1;
    inopp = 0;
  }

  // Fortran: SFRAC = (SI-SLE)/(S(IN)-SLE); SOPP = SLE + SFRAC*(S(INOPP)-SLE)
  const double sfrac = (si - sle) / (s[in] - sle);
  double sopp = sle + sfrac * (s[inopp] - sle);

  // Fortran: IF(ABS(SFRAC) .LE. 1.0E-5) -> SOPP = SLE, RETURN
  if (std::abs(sfrac) <= 1.0e-5) {
    result.sopp = sle;
    result.status = math::Diagnostic::completed;
    return result;
  }

  // Fortran: XBAR = (XI-XLE)*DXC + (YI-YLE)*DYC
  // (XI/YI and XLE/YLE be as in Fortran new evaluated)
  const double xi = math::seval(si, x, xp, s);
  const double yi = math::seval(si, y, yp, s);
  xle = math::seval(sle, x, xp, s);
  yle = math::seval(sle, y, yp, s);
  const double xbar = (xi - xle) * dxc + (yi - yle) * dyc;

  // Fortran: Newton iteration DO 300 ITER=1,12
  for (int iter = 1; iter <= 12; ++iter) {
    const double xopp = math::seval(sopp, x, xp, s);
    const double yopp = math::seval(sopp, y, yp, s);
    const double xoppd = math::deval(sopp, x, xp, s);
    const double yoppd = math::deval(sopp, y, yp, s);

    const double res = (xopp - xle) * dxc + (yopp - yle) * dyc - xbar;
    const double resd = xoppd * dxc + yoppd * dyc;

    if (std::abs(res) / slen < 1.0e-5) {
      result.sopp = sopp;
      result.status = math::Diagnostic::completed;
      return result;
    }
    if (resd == 0.0) {
      break;  // Fortran: GO TO 303
    }

    const double dsopp = -res / resd;
    sopp += dsopp;

    if (std::abs(dsopp) / slen < 1.0e-5) {
      result.sopp = sopp;
      result.status = math::Diagnostic::completed;
      return result;
    }
  }

  // Fortran Label 303: WRITE 'SOPPS: Opposite-point location failed.
  // Continuing...'; SOPP = SLE + SFRAC*(S(INOPP)-SLE)
  result.sopp = sle + sfrac * (s[inopp] - sle);
  result.status = math::Diagnostic::limitReached;
  return result;
}

}  // namespace xfoil::core::geometry

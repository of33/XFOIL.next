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

// File: src/core/geometry_core/Refinement.cpp
// Implementation of the interpolation/refinement, directly
// ported from fortran-src/xgeom.f.
//
// 0-based index mapping:
// Fortran X0/S0(1..N0) <-> C++ p0.x/p0.s[0..n0-1]
// Fortran X1/S1(1..N1) <-> C++ p1.x/p1.s[0..n1-1]
// Fortran X/Y(1..N) <-> C++ x/y[0..n-1]
// INTER/INTERX: Fortran DO 50 I=1,N <-> i = 0..n-1
// IJSECT:
// Fortran DO 10 I=2,N <-> i = 1..n-1
// Fortran IMID loop DO I=1,N-1 <-> k = 0..n-2; end without hit:
// Fortran I=N, execution continues at Label 30 -> IMID = I = N
// <-> 0-based imid = n-1 (the assignment IMID=N/2 is dead code)
// Fortran AJ loop DO I=2,IMID <-> i = 1..imid
// Fortran J loop DO J=N..IMID,-1 <-> j = n-1..imid (downward)
// AREFINE:
// Fortran K (1-based counter) <-> C++ k (1-based counter)
// Fortran XNEW(K) <-> C++ xnew[k-1]; Fortran K>NDIM <-> k > ndim
// Fortran DO 10 I=2,N-1 <-> i = 1..n-2
// SCHECK:
// Fortran DO 20 I=2,N-2 <-> i = 1..n-3
// Fortran Shift X(L)=X(L+1), L=I+1..N <-> l = i+1..n-2 (reads in
// Fortran X(N+1) out of bounds; C++ shifts only up to the new active length)
// HALF:
// Fortran K (1-based counter) <-> C++ k (0-based write index)
// Fortran INEXT (1-based) <-> C++ inext (0-based trigger index)
// Fortran DO 20 I=2,N-1 <-> i = 1..n-2

#include "xfoil/core/geometry/Refinement.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry {

namespace {

void checkProfile(const SplineProfile& p, const char* who) {
  const std::size_t n = p.x.size();
  if (p.y.size() != n || p.s.size() != n || p.xp.size() != n ||
      p.yp.size() != n) {
    throw std::invalid_argument(std::string(who) +
                                ": Airfoil lengths inconsistent");
  }
  if (n < 2) {
    throw std::invalid_argument(std::string(who) + ": N < 2");
  }
}

}  // namespace

AirfoilCoordinates inter(const SplineProfile& p0, const SplineProfile& p1,
                         double frac) {
  checkProfile(p0, "inter");
  checkProfile(p1, "inter");

  // Fortran: N = N0
  const std::size_t n = p0.x.size();

  // Fortran: F0 = 1.0-FRAC; F1 = FRAC
  const double f0 = 1.0 - frac;
  const double f1 = frac;

  // Fortran: TOPS0 = S0(1)-SLE0; TOPS1 = S1(1)-SLE1;
  // BOTS0 = S0(N0)-SLE0; BOTS1 = S1(N1)-SLE1
  const double tops0 = p0.s[0] - p0.sle;
  const double tops1 = p1.s[0] - p1.sle;
  const double bots0 = p0.s[n - 1] - p0.sle;
  const double bots1 = p1.s[p1.x.size() - 1] - p1.sle;

  AirfoilCoordinates out;
  out.x.resize(n);
  out.y.resize(n);

  // Fortran: DO 50 I=1,N
  for (std::size_t i = 0; i < n; ++i) {
    // Fortran: SN = (S0(I)-SLE0)/TOPS0 or. /BOTS0
    // (the both Fortran-IFs are complementary; if/else is identical)
    double sn;
    if (p0.s[i] < p0.sle) {
      sn = (p0.s[i] - p0.sle) / tops0;
    } else {
      sn = (p0.s[i] - p0.sle) / bots0;
    }

    // Fortran: ST0 = S0(I); ST1 = SLE1 + TOPS1*SN or. SLE1 + BOTS1*SN
    const double st0 = p0.s[i];
    double st1;
    if (st0 < p0.sle) {
      st1 = p1.sle + tops1 * sn;
    } else {
      st1 = p1.sle + bots1 * sn;
    }

    // Fortran: XT0/YT0 = SEVAL(ST0,...); XT1/YT1 = SEVAL(ST1,...)
    const double xt0 = math::seval(st0, p0.x, p0.xp, p0.s);
    const double yt0 = math::seval(st0, p0.y, p0.yp, p0.s);
    const double xt1 = math::seval(st1, p1.x, p1.xp, p1.s);
    const double yt1 = math::seval(st1, p1.y, p1.yp, p1.s);

    // Fortran: X(I) = F0*XT0 + F1*XT1; Y(I) = F0*YT0 + F1*YT1
    out.x[i] = f0 * xt0 + f1 * xt1;
    out.y[i] = f0 * yt0 + f1 * yt1;
  }

  return out;
}

AirfoilCoordinates interX(const SplineProfile& p0, const SplineProfile& p1,
                          double frac) {
  checkProfile(p0, "interX");
  checkProfile(p1, "interX");

  const std::size_t n = p0.x.size();  // Fortran N = N0
  const std::size_t n1 = p1.x.size();

  const double f0 = 1.0 - frac;
  const double f1 = frac;

  // Fortran: XLE0 = SEVAL(SLE0,...); XLE1 = SEVAL(SLE1,...)
  const double xle0 = math::seval(p0.sle, p0.x, p0.xp, p0.s);
  const double xle1 = math::seval(p1.sle, p1.x, p1.xp, p1.s);

  AirfoilCoordinates out;
  out.x.resize(n);
  out.y.resize(n);

  // Fortran: DO 50 I=1,N
  for (std::size_t i = 0; i < n; ++i) {
    // Fortran: XN = (X0(I)-XLE0)/(X0(1)-XLE0) or /(X0(N0)-XLE0)
    // (the two Fortran IFs are complementary; if/else is identical)
    double xn;
    if (p0.s[i] < p0.sle) {
      xn = (p0.x[i] - xle0) / (p0.x[0] - xle0);
    } else {
      xn = (p0.x[i] - xle0) / (p0.x[n - 1] - xle0);
    }

    // Fortran: XT0 = X0(I); ST0 = S0(I); XT1/ST1 via differences X1(1)/X1(N1)
    double xt0 = p0.x[i];
    double st0 = p0.s[i];
    double xt1;
    double st1;
    if (st0 < p0.sle) {
      xt1 = xle1 + (p1.x[0] - xle1) * xn;
      st1 = p1.sle + (p1.s[0] - p1.sle) * xn;
    } else {
      xt1 = xle1 + (p1.x[n1 - 1] - xle1) * xn;
      st1 = p1.sle + (p1.s[n1 - 1] - p1.sle) * xn;
    }

    // Fortran: CALL SINVRT(ST0,XT0,X0,XP0,S0,N0);
    // CALL SINVRT(ST1,XT1,X1,XP1,S1,N1)
    (void)math::sinvrt(st0, xt0, p0.x, p0.xp, p0.s);
    (void)math::sinvrt(st1, xt1, p1.x, p1.xp, p1.s);

    // Fortran: XT0/YT0 = SEVAL(ST0,...); XT1/YT1 = SEVAL(ST1,...)
    const double xt0v = math::seval(st0, p0.x, p0.xp, p0.s);
    const double yt0v = math::seval(st0, p0.y, p0.yp, p0.s);
    const double xt1v = math::seval(st1, p1.x, p1.xp, p1.s);
    const double yt1v = math::seval(st1, p1.y, p1.yp, p1.s);

    out.x[i] = f0 * xt0v + f1 * xt1v;
    out.y[i] = f0 * yt0v + f1 * yt1v;
  }

  return out;
}

SectionBending ijsect(std::span<const double> x, std::span<const double> y) {
  const std::size_t n = x.size();
  if (y.size() != n) {
    throw std::invalid_argument("ijsect: span lengths inconsistent");
  }
  if (n < 1) {
    throw std::invalid_argument("ijsect: N < 1");
  }

  SectionBending r;

  // Fortran: XMIN = XMAX = X(1); YMIN = YMAX = Y(1)
  double xmin = x[0];
  double xmax = x[0];
  double ymin = y[0];
  double ymax = y[0];

  // Fortran: closing segment I=N -> 1
  double dx = x[0] - x[n - 1];
  double dy = y[0] - y[n - 1];
  double ds = std::sqrt(dx * dx + dy * dy);
  double xavg = 0.5 * (x[0] + x[n - 1]);
  double yavg = 0.5 * (y[0] + y[n - 1]);

  double x_dy = dy * xavg;
  double xx_dy = dy * (xavg * xavg);  // Fortran DY*XAVG**2
  double xxx_dy = dy * (xavg * xavg * xavg);  // Fortran DY*XAVG**3
  double x_ds = ds * xavg;
  double xx_ds = ds * (xavg * xavg);
  double y_dx = dx * yavg;
  double yy_dx = dx * (yavg * yavg);
  double yyy_dx = dx * (yavg * yavg * yavg);
  double y_ds = ds * yavg;
  double yy_ds = ds * (yavg * yavg);
  double c_ds = ds;

  // Fortran: DO 10 I=2,N
  for (std::size_t i = 1; i < n; ++i) {
    dx = x[i] - x[i - 1];
    dy = y[i] - y[i - 1];
    ds = std::sqrt(dx * dx + dy * dy);
    xavg = 0.5 * (x[i] + x[i - 1]);
    yavg = 0.5 * (y[i] + y[i - 1]);

    x_dy += dy * xavg;
    xx_dy += dy * (xavg * xavg);
    xxx_dy += dy * (xavg * xavg * xavg);
    x_ds += ds * xavg;
    xx_ds += ds * (xavg * xavg);

    y_dx += dx * yavg;
    yy_dx += dx * (yavg * yavg);
    yyy_dx += dx * (yavg * yavg * yavg);
    y_ds += ds * yavg;
    yy_ds += ds * (yavg * yavg);

    c_ds += ds;

    xmin = std::min(xmin, x[i]);
    xmax = std::max(xmax, x[i]);
    ymin = std::min(ymin, y[i]);
    ymax = std::max(ymax, y[i]);
  }

  r.xmin = xmin;
  r.xmax = xmax;
  r.ymin = ymin;
  r.ymax = ymax;

  // Fortran: AREA = -Y_DX; SLEN = C_DS
  r.area = -y_dx;
  r.slen = c_ds;

  if (r.area == 0.0) {
    // Fortran: RETURN before the XC/YC computation (remainder undefined);
    // here it is defined as 0.0 (controlled behavior, documented).
    return r;
  }

  // Fortran: XC = XX_DY/(2.0*X_DY); XCT = X_DS/C_DS;
  // AIYY = XXX_DY/3.0 - XX_DY*XC + X_DY*XC**2
  // AIYYT = XX_DS - X_DS*XCT*2.0 + C_DS*XCT**2
  const double xc = xx_dy / (2.0 * x_dy);
  const double xct = x_ds / c_ds;
  r.xc = xc;
  r.xct = xct;
  r.aiyy = xxx_dy / 3.0 - xx_dy * xc + x_dy * (xc * xc);
  r.aiyyt = xx_ds - x_ds * xct * 2.0 + c_ds * (xct * xct);

  // Fortran: YC = YY_DX/(2.0*Y_DX); YCT = Y_DS/C_DS;
  // AIXX = -YYY_DX/3.0 + YY_DX*YC - Y_DX*YC**2
  // AIXXT = YY_DS - Y_DS*YCT*2.0 + C_DS*YCT**2
  const double yc = yy_dx / (2.0 * y_dx);
  const double yct = y_ds / c_ds;
  r.yc = yc;
  r.yct = yct;
  r.aixx = -yyy_dx / 3.0 + yy_dx * yc - y_dx * (yc * yc);
  r.aixxt = yy_ds - y_ds * yct * 2.0 + c_ds * (yct * yct);

  // Fortran: SINT loop DO 20 I=2,N (XINT/YINT unused, only SINT)
  double sint = 0.0;
  for (std::size_t i = 1; i < n; ++i) {
    const double dx2 = x[i] - x[i - 1];
    const double dy2 = y[i] - y[i - 1];
    const double ds2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    sint += ds2;
  }
  (void)sint;  // SINT is not used further in the canonical Fortran

  // Fortran: IMID loop DO I=1,N-1; end without hit: I=N, Label 30
  // is executed -> IMID = I = N <-> 0-based imid = n-1
  std::size_t imid = n - 1;
  for (std::size_t k = 0; k + 1 < n; ++k) {  // Fortran I = 1..N-1
    if (x[k + 1] >= x[k]) {
      imid = k;  // Fortran IMID = I
      break;
    }
  }

  // Fortran: AJ loop DO I=2,IMID
  double aj = 0.0;
  for (std::size_t i = 1; i <= imid; ++i) {
    const double xavgi = 0.5 * (x[i] + x[i - 1]);
    const double yavgi = 0.5 * (y[i] + y[i - 1]);
    const double dxi = x[i - 1] - x[i];

    double yopp = y[imid];  // fallback (Fortran YOPP uninitialized)
    if (xavgi > x[n - 1]) {
      yopp = y[n - 1];
    } else if (xavgi <= x[imid]) {
      yopp = y[imid];
    } else {
      // Fortran: DO J=N,IMID,-1 ; XAVG > X(J-1) .AND. XAVG <= X(J)
      for (std::size_t j = n - 1; j >= imid; --j) {
        if (j == 0) {
          break;  // Guard: X(J-1) would OOB read (imid == 0)
        }
        if (xavgi > x[j - 1] && xavgi <= x[j]) {
          const double frac = (xavgi - x[j - 1]) / (x[j] - x[j - 1]);
          yopp = y[j - 1] + (y[j] - y[j - 1]) * frac;
          break;
        }
      }
    }

    // Fortran: AJ = AJ + ABS(YAVG-YOPP)**3 * DX / 3.0
    const double dyo = std::abs(yavgi - yopp);
    aj += dyo * dyo * dyo * dxi / 3.0;
  }
  r.aj = aj;

  // Fortran: AJT = 4.0*AREA**2/SLEN
  const double areaSq = r.area * r.area;
  r.ajt = 4.0 * areaSq / r.slen;

  return r;
}

RefineResult arefine(std::span<const double> x, std::span<const double> y,
                     std::span<const double> s, std::span<const double> xs,
                     std::span<const double> ys, double atol,
                     std::size_t ndim, double x1, double x2) {
  const std::size_t n = s.size();
  if (x.size() < n || y.size() < n || xs.size() < n || ys.size() < n) {
    throw std::invalid_argument("arefine: span lengths inconsistent");
  }
  if (n < 2) {
    throw std::invalid_argument("arefine: N < 2");
  }

  // Fortran: ATOLR = ATOL * 3.14159/180.0 (truncated it pi, exact)
  const double atolr = atol * 3.14159 / 180.0;

  RefineResult result;
  result.status = math::Diagnostic::completed;

  // Fortran: K = 1; XNEW(1) = X(1); YNEW(1) = Y(1)
  std::size_t k = 1;  // 1-based counter as in Fortran
  result.x.reserve(ndim);
  result.y.reserve(ndim);
  result.x.push_back(x[0]);
  result.y.push_back(y[0]);

  const auto addPoint = [&](double xk, double yk) {
    // Fortran: K = K+1; IF(K .GT. NDIM) GO TO 90; XNEW(K) = XK
    k += 1;
    if (k > ndim) {
      // Fortran: WRITE 'SDOUBLE: Arrays will overflow. No action taken.';
      // NNEW = 0; RETURN
      result.nnew = 0;
      result.x.clear();
      result.y.clear();
      result.status = math::Diagnostic::limitReached;
      return false;
    }
    result.x.push_back(xk);
    result.y.push_back(yk);
    return true;
  };

  // Fortran: DO 10 I=2,N-1
  for (std::size_t i = 1; i + 1 < n; ++i) {
    const double dxm = x[i] - x[i - 1];
    const double dym = y[i] - y[i - 1];
    const double dxp = x[i + 1] - x[i];
    const double dyp = y[i + 1] - y[i];

    // Fortran: CRSP = DXM*DYP - DYM*DXP; DOTP = DXM*DXP + DYM*DYP
    const double crsp = dxm * dyp - dym * dxp;
    const double dotp = dxm * dxp + dym * dyp;
    const double aseg =
        (crsp == 0.0 && dotp == 0.0) ? 0.0 : std::atan2(crsp, dotp);
    const bool lref = std::abs(aseg) > atolr;

    if (lref) {
      // Fortran: SMID = S(I) - 0.3333*(S(I)-S(I-1))
      const double smid = s[i] - 0.3333 * (s[i] - s[i - 1]);
      const double xk = math::seval(smid, x, xs, s);
      const double yk = math::seval(smid, y, ys, s);
      if (xk >= x1 && xk <= x2) {
        if (!addPoint(xk, yk)) {
          return result;
        }
      }
    }

    // Fortran: Node itself
    if (!addPoint(x[i], y[i])) {
      return result;
    }

    if (lref) {
      // Fortran: SMID = S(I) + 0.3333*(S(I+1)-S(I))
      const double smid = s[i] + 0.3333 * (s[i + 1] - s[i]);
      const double xk = math::seval(smid, x, xs, s);
      const double yk = math::seval(smid, y, ys, s);
      if (xk >= x1 && xk <= x2) {
        if (!addPoint(xk, yk)) {
          return result;
        }
      }
    }
  }

  // Fortran: K = K+1; IF(K > NDIM) GO TO 90; XNEW(K) = X(N)
  if (!addPoint(x[n - 1], y[n - 1])) {
    return result;
  }

  result.nnew = k;  // Fortran NNEW = K
  return result;
}

ScheckResult scheck(std::span<double> x, std::span<double> y,
                    std::size_t& n, double stol) {
  if (x.size() < n || y.size() < n) {
    throw std::invalid_argument("scheck: span lengths inconsistent");
  }

  ScheckResult result;

  // Fortran: IF(STOL.GT.0.3) WRITE 'SCHECK: Bad value for small panels
  // (STOL > 0.3)'; RETURN (no change)
  if (stol > 0.3) {
    result.badStol = true;
    return result;
  }

  bool restart = true;
  while (restart) {
    restart = false;
    // Fortran: DO 20 I=2,N-2
    for (std::size_t i = 1; i + 2 < n; ++i) {
      const double dxm1 = x[i] - x[i - 1];
      const double dym1 = y[i] - y[i - 1];
      const double dsm1 = std::sqrt(dxm1 * dxm1 + dym1 * dym1);

      const double dxp1 = x[i + 1] - x[i];
      const double dyp1 = y[i + 1] - y[i];
      const double dsp1 = std::sqrt(dxp1 * dxp1 + dyp1 * dyp1);

      const double dxp2 = x[i + 2] - x[i + 1];
      const double dyp2 = y[i + 2] - y[i + 1];
      const double dsp2 = std::sqrt(dxp2 * dxp2 + dyp2 * dyp2);

      // Fortran: IF(DSP1.EQ.0.0) GO TO 20 (Corner unchanged)
      if (dsp1 == 0.0) {
        continue;
      }

      if (dsp1 < stol * dsm1 || dsp1 < stol * dsp2) {
        // Fortran: Node I with I+1 averaged
        x[i] = 0.5 * (x[i] + x[i + 1]);
        y[i] = 0.5 * (y[i] + y[i + 1]);

        // Fortran: Node I+1 remove (Shift X(L)=X(L+1), L=I+1..N;
        // reads in Fortran X(N+1) OOB — here only until the new active
        // length, defined behavior identical)
        for (std::size_t l = i + 1; l + 1 < n; ++l) {
          x[l] = x[l + 1];
          y[l] = y[l + 1];
        }
        n -= 1;
        result.changed = true;
        result.removedIndices.push_back(i);
        restart = true;  // Fortran: GO TO 10
        break;
      }
    }
  }

  return result;
}

void half(std::span<const double> s, std::span<double> x,
          std::span<double> y, std::size_t& n) {
  const std::size_t nIn = n;
  if (x.size() < nIn || y.size() < nIn) {
    throw std::invalid_argument("half: span lengths inconsistent");
  }
  if (s.size() < nIn) {
    throw std::invalid_argument("half: s length inconsistent");
  }

  // Fortran: K = 1 (1-based) <-> 0-based write index k = 0
  std::size_t k = 0;
  // Fortran: INEXT = 3 <-> 0-based trigger index inext = 2
  std::size_t inext = 2;

  // Fortran: DO 20 I=2,N-1
  for (std::size_t i = 1; i + 1 < nIn; ++i) {
    // Fortran: corner S(I)==S(I+1) is preserved
    if (s[i] == s[i + 1]) {
      k += 1;
      if (k >= x.size()) {
        throw std::invalid_argument(
            "half: output capacity exceeded (Fortran would OOB "
            "write)");
      }
      x[k] = x[i];
      y[k] = y[i];
      k += 1;
      if (k >= x.size()) {
        throw std::invalid_argument(
            "half: output capacity exceeded (Fortran would OOB "
            "write)");
      }
      x[k] = x[i + 1];
      y[k] = y[i + 1];
      inext = i + 3;  // Fortran INEXT = I+3 (0-based trigger index)
    }

    if (i == inext) {
      k += 1;
      if (k >= x.size()) {
        throw std::invalid_argument(
            "half: output capacity exceeded (Fortran would OOB "
            "write)");
      }
      x[k] = x[i];
      y[k] = y[i];
      inext = i + 2;  // Fortran INEXT = I+2 (0-based trigger index)
    }
  }

  // Fortran: K = K+1; X(K) = X(N); N = K
  k += 1;
  if (k >= x.size()) {
    throw std::invalid_argument(
        "half: output capacity exceeded (Fortran would OOB "
        "write)");
  }
  x[k] = x[nIn - 1];
  y[k] = y[nIn - 1];
  n = k + 1;
}

}  // namespace xfoil::core::geometry

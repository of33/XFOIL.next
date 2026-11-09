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

// File: src/core/geometry_core/Transformations.cpp
// Implementation of the transformations, directly ported from
// fortran-src/xgeom.f and fortran-src/xfoil.f.
//
// 0-based index mapping:
// Fortran X/XP/Y/YP/S(1..N) <-> C++ x/xp/y/yp/s[0..n-1]
// Fortran XNEW/YNEW(1..NNEW) <-> C++ xnew/ynew[0..nnew-1]
// YSYM:
// Fortran DO 5 I=2,N (ILE detection) <-> k = 1..n-1
// Fortran ILE = I-1 <-> 0-based ile = k-1
// End without break: Fortran I=N+1 -> ILE=N -> S(ILE+1) out of bounds
// -> controlled error (no out-of-bounds reproduction).
// Fortran ILE1/ILE2 (1-based) <-> 0-based ile1/ile2 (each -1)
// Fortran IG1/IG2 (1-based) <-> 0-based ig1/ig2
// Fortran NNEW = 2*(IABS(IG2-IG1)+1)+1 (identical)
// Fortran DO 10 I=IG1..IG2,IGDIR <-> i = ig1 + k*igdir (k = 0..count-1)
// Fortran I1 = 1+(I-IG1)*IGDIR <-> 0-based k
// Fortran I2 = NNEW-(I-IG1)*IGDIR <-> 0-based nnew-1-k
// Fortran XNEW(NNEW/2+1) <-> 0-based nnew/2 (integer division preserved)
// Fortran DO 20 IG=1,N <-> ig = 0..n-1; IGDIR=-1: I=N-IG+1 <-> n-1-ig
// CLIS:
// Fortran DO 30 I=1,N <-> i = 0..n-1; Fortran IMAX (1-based) <-> 0-based
// CANG:
// Fortran DO 30 I=2,N-1 <-> i = 1..n-2; Fortran X(I-2)/X(I+2)-Fallback
// only for an existing neighbor index (Fortran out-of-bounds not replicated)
// ROTATE:
// Fortran DO 8 I=1,N <-> i = 0..n-1

#include "xfoil/core/geometry/Transformations.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/FortranCompat.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry {

namespace {

// Fortran: DATA EPS / 1.0E-5 / (SSS)
constexpr double kSssEps = 1.0e-5;

void checkContourSpans(std::span<const double> x, std::span<const double> xp,
                       std::span<const double> y, std::span<const double> yp,
                       std::span<const double> s) {
  const std::size_t n = s.size();
  if (x.size() < n || xp.size() < n || y.size() < n || yp.size() < n) {
    throw std::invalid_argument(
        "ysym/lerscl/sss/clis: span lengths inconsistent");
  }
}

}  // namespace

Status ysym(std::span<double> x, std::span<double> xp, std::span<double> y,
            std::span<double> yp, std::span<double> s, std::size_t& n,
            int iside, std::span<double> xnew, std::span<double> ynew) {
  const std::size_t nx = x.size();  // Fortran NX
  if (xp.size() != nx || y.size() != nx || yp.size() != nx ||
      s.size() != nx) {
    throw std::invalid_argument("ysym: capacity lengths inconsistent");
  }
  if (n < 2 || n > nx) {
    throw std::invalid_argument("ysym: invalid active point count n");
  }
  if (xnew.size() < nx || ynew.size() < nx) {
    throw std::invalid_argument(
        "ysym: work arrays are smaller than the capacity NX");
  }

  // Fortran: CALL LEFIND(SLE,X,XP,Y,YP,S,N); XLE/YLE = SEVAL(...)
  const LeadingEdgeResult le = lefind(
      x.first(n), xp.first(n), y.first(n), yp.first(n), s.first(n));
  const double sle = le.sle;
  const double xle = math::seval(sle, x.first(n), xp.first(n), s.first(n));
  const double yle = math::seval(sle, y.first(n), yp.first(n), s.first(n));

  // Fortran: XTE = 0.5*(X(1)+X(N)); YTE = ...; CHSQ;
  // DXC = (XTE-XLE)/SQRT(CHSQ); DYC = (YTE-YLE)/SQRT(CHSQ)
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);
  const double chsq =
      (xte - xle) * (xte - xle) + (yte - yle) * (yte - yle);
  const double dxc = (xte - xle) / std::sqrt(chsq);
  const double dyc = (yte - yle) / std::sqrt(chsq);

  // Fortran: DO 5 I=2,N ; DS = S(I)-S(I-1);
  // IF(S(I)-SLE .GE. -0.01*DS) GO TO 6
  // End without Break: Fortran I=N+1 -> ILE=N -> S(ILE+1) would OOB read.
  bool found = false;
  std::size_t kk = 1;
  for (; kk < n; ++kk) {
    const double ds = s[kk] - s[kk - 1];
    if (s[kk] - sle >= -0.01 * ds) {
      found = true;
      break;
    }
  }
  if (!found) {
    return Status::fail(Error::make(
        ErrorCode::invalidState,
        "ysym: LE point not within the contour (the Fortran code would "
        "read S(ILE+1) outside the range)"));
  }
  const std::size_t ile = kk - 1;  // 0-based (Fortran ILE = I-1)

  // Fortran: DS = S(ILE+1)-S(ILE)
  const double dsle = s[ile + 1] - s[ile];

  // Fortran: IF(SLE-S(ILE-1) .LT. 0.1*DS) -> ILE1=ILE-1, ILE2=ILE+1
  // ELIF(S(ILE+1)-SLE .LT. 0.1*DS) -> ILE1=ILE, ILE2=ILE+2
  // ELSE -> ILE1=ILE, ILE2=ILE+1
  // 0-based mapping (Fortran ILE <-> ile): ILE1=ILE-1 <-> ile-1;
  // ILE2=ILE+1 <-> ile+1; ILE1=ILE <-> ile; ILE2=ILE+2 <-> ile+2.
  // S(ILE-1) for ile == 0 would be an out-of-bounds read in Fortran (guarded).
  std::size_t ile1;
  std::size_t ile2;
  if (ile >= 1 && sle - s[ile - 1] < 0.1 * dsle) {
    ile1 = ile - 1;  // Fortran ILE1 = ILE-1
    ile2 = ile + 1;  // Fortran ILE2 = ILE+1
  } else if (s[ile + 1] - sle < 0.1 * dsle) {
    ile1 = ile;      // Fortran ILE1 = ILE
    ile2 = ile + 2;  // Fortran ILE2 = ILE+2
  } else {
    ile1 = ile;      // Fortran ILE1 = ILE
    ile2 = ile + 1;  // Fortran ILE2 = ILE+1
  }
  if (ile2 >= n) {
    // Fortran would X(ILE2) outside of the Range read; controlled
    // Error instead of OOB reproduction (before each mutation).
    return Status::fail(Error::make(
        ErrorCode::invalidState,
        "ysym: ILE2 outside the contour (the Fortran code would read "
        "outside the range)"));
  }

  // Fortran: ISIDE==1 -> IG1=1, IG2=ILE1, IGDIR=+1; otherwise IG1=N, IG2=ILE2,
  // IGDIR=-1
  std::size_t ig1;
  std::size_t ig2;
  int igdir;
  if (iside == 1) {
    ig1 = 0;
    ig2 = ile1;
    igdir = +1;
  } else {
    ig1 = n - 1;
    ig2 = ile2;
    igdir = -1;
  }

  // Fortran: NNEW = 2*(IABS(IG2-IG1)+1)+1; NNEW > NX -> STOP
  const std::size_t nnew = 2 * ((ig2 > ig1 ? ig2 - ig1 : ig1 - ig2) + 1) + 1;
  if (nnew > nx) {
    // Fortran-STOP before each Mutation; no partial mutation.
    return Status::fail(Error::make(
        ErrorCode::capacityExceeded,
        "ysym: array overflow in the passed arrays (NNEW > NX)"));
  }

  // Fortran: DO 10 I=IG1..IG2,IGDIR
  const std::size_t count =
      (ig2 > ig1 ? ig2 - ig1 : ig1 - ig2) + 1;
  for (std::size_t k = 0; k < count; ++k) {
    const std::size_t i = static_cast<std::size_t>(
        static_cast<std::ptrdiff_t>(ig1) +
        static_cast<std::ptrdiff_t>(k) * igdir);
    // Fortran: XBAR/YBAR in chord axes
    const double xbar = (x[i] - xle) * dxc + (y[i] - yle) * dyc;
    const double ybar = (y[i] - yle) * dxc - (x[i] - xle) * dyc;
    // Fortran: I1 = 1+(I-IG1)*IGDIR <-> 0-based k
    // I2 = NNEW-(I-IG1)*IGDIR <-> 0-based nnew-1-k
    const std::size_t i1 = k;
    const std::size_t i2 = nnew - 1 - k;
    xnew[i1] = xle + xbar * dxc - ybar * dyc;
    xnew[i2] = xle + xbar * dxc + ybar * dyc;
    ynew[i1] = yle + ybar * dxc + xbar * dyc;
    ynew[i2] = yle - ybar * dxc + xbar * dyc;
  }

  // Fortran: XNEW(NNEW/2+1) = XLE <-> 0-based nnew/2
  xnew[nnew / 2] = xle;
  ynew[nnew / 2] = yle;

  // Fortran: N = NNEW; DO 20 IG=1,N; IGDIR=-1: I = N-IG+1
  n = nnew;
  for (std::size_t ig = 0; ig < nnew; ++ig) {
    const std::size_t i = (igdir > 0) ? ig : nnew - 1 - ig;
    x[i] = xnew[ig];
    y[i] = ynew[ig];
  }

  // Fortran: CALL SCALC(X,Y,S,N); CALL SEGSPL(X,XP,S,N);
  // CALL SEGSPL(Y,YP,S,N)
  math::scalc(x.first(n), y.first(n), s.first(n));
  math::segspl(x.first(n), s.first(n), xp.first(n));
  math::segspl(y.first(n), s.first(n), yp.first(n));

  return Status::ok();
}

AirfoilCoordinates lerscl(std::span<const double> x,
                          std::span<const double> xp,
                          std::span<const double> y,
                          std::span<const double> yp,
                          std::span<const double> s, double doc,
                          double rfac) {
  const std::size_t n = s.size();
  checkContourSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("lerscl: N < 2");
  }

  // Fortran: CALL LEFIND(SLE,...); XLE/YLE/XTE/YTE; CHORD; DXC/DYC
  const LeadingEdgeResult le = lefind(x, xp, y, yp, s);
  const double xle = math::seval(le.sle, x, xp, s);
  const double yle = math::seval(le.sle, y, yp, s);
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);
  const double chord =
      std::sqrt((xte - xle) * (xte - xle) + (yte - yle) * (yte - yle));
  const double dxc = (xte - xle) / chord;
  const double dyc = (yte - yle) / chord;

  // Fortran: SRFAC = SQRT(ABS(RFAC))
  const double srfac = std::sqrt(std::abs(rfac));

  AirfoilCoordinates out;
  out.x.resize(n);
  out.y.resize(n);

  // Fortran: DO 30 I=1,N
  for (std::size_t i = 0; i < n; ++i) {
    const double xbar = (x[i] - xle) * dxc + (y[i] - yle) * dyc;
    const double ybar = (y[i] - yle) * dxc - (x[i] - xle) * dyc;

    const OppositePointResult op = sopps(s[i], x, xp, y, yp, s, le.sle);
    const double xopp = math::seval(op.sopp, x, xp, s);
    const double yopp = math::seval(op.sopp, y, yp, s);

    const double ybarop = (yopp - yle) * dxc - (xopp - xle) * dyc;

    // Fortran: XOC = XBAR/CHORD; ARG = MIN(XOC/DOC, 15.0);
    // TFAC = 1.0 - (1.0-SRFAC)*EXP(-ARG)
    const double xoc = xbar / chord;
    const double arg = std::min(xoc / doc, 15.0);
    const double tfac = 1.0 - (1.0 - srfac) * std::exp(-arg);

    const double ybarct =
        0.5 * (ybar + ybarop) + tfac * 0.5 * (ybar - ybarop);
    out.x[i] = xle + xbar * dxc - ybarct * dyc;
    out.y[i] = yle + ybarct * dxc + xbar * dyc;
  }

  return out;
}

BreakPointResult sss(double ss, double xbf, double ybf, double del,
                     int iside, std::span<const double> x,
                     std::span<const double> xp, std::span<const double> y,
                     std::span<const double> yp, std::span<const double> s) {
  const std::size_t n = s.size();
  checkContourSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("sss: N < 2");
  }

  BreakPointResult result;

  // Fortran: STOT = ABS(S(N)-S(1)); Are = SIN(0.5*ABS(DEL))
  const double stot = std::abs(s[n - 1] - s[0]);
  const double sind = std::sin(0.5 * std::abs(del));

  // Fortran: SSGN = 1.0; IF(ISIDE.EQ.1) SSGN = -1.0
  double ssgn = 1.0;
  if (iside == 1) {
    ssgn = -1.0;
  }

  // Fortran: RSQ = (SEVAL(SS,X,XP,S,N)-XBF)**2 + (SEVAL(SS,Y,YP,S,N)-YBF)**2
  const double dx0 = math::seval(ss, x, xp, s) - xbf;
  const double dy0 = math::seval(ss, y, yp, s) - ybf;
  const double rsq = dx0 * dx0 + dy0 * dy0;
  const double rsqSqrt = std::sqrt(rsq);

  // Fortran: S1 = SS - (Are*SQRT(RSQ) + EPS*STOT)*SSGN
  // S2 = SS + (Are*SQRT(RSQ) + EPS*STOT)*SSGN
  double s1 = ss - (sind * rsqSqrt + kSssEps * stot) * ssgn;
  double s2 = ss + (sind * rsqSqrt + kSssEps * stot) * ssgn;

  // Fortran: Newton iteration loop DO 10 ITER=1,10
  bool converged = false;
  for (int iter = 1; iter <= 10; ++iter) {
    const double x1 = math::seval(s1, x, xp, s);
    const double x1p = math::deval(s1, x, xp, s);
    const double y1 = math::seval(s1, y, yp, s);
    const double y1p = math::deval(s1, y, yp, s);
    const double x2 = math::seval(s2, x, xp, s);
    const double x2p = math::deval(s2, x, xp, s);
    const double y2 = math::seval(s2, y, yp, s);
    const double y2p = math::deval(s2, y, yp, s);

    const double r1sq = (x1 - xbf) * (x1 - xbf) + (y1 - ybf) * (y1 - ybf);
    const double r2sq = (x2 - xbf) * (x2 - xbf) + (y2 - ybf) * (y2 - ybf);
    const double r1 = std::sqrt(r1sq);
    const double r2 = std::sqrt(r2sq);

    const double rrsq = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
    const double rr = std::sqrt(rrsq);

    if (r1 <= kSssEps * stot || r2 <= kSssEps * stot) {
      // Fortran: S1 = SS; S2 = SS; RETURN
      result.s1 = ss;
      result.s2 = ss;
      result.status = math::Diagnostic::completed;
      return result;
    }

    const double r1_s1 = (x1p * (x1 - xbf) + y1p * (y1 - ybf)) / r1;
    const double r2_s2 = (x2p * (x2 - xbf) + y2p * (y2 - ybf)) / r2;

    double rs1;
    double rs2;
    double a11;
    double a12;
    double a21;
    double a22;
    if (sind > 0.01) {
      if (rr == 0.0) {
        // Fortran: RETURN with current S1/S2
        result.s1 = s1;
        result.s2 = s2;
        result.status = math::Diagnostic::completed;
        return result;
      }
      const double rr_s1 = (x1p * (x1 - x2) + y1p * (y1 - y2)) / rr;
      const double rr_s2 = -(x2p * (x1 - x2) + y2p * (y1 - y2)) / rr;

      // Fortran: Residual 1 (Dot product)
      rs1 = ((xbf - x1) * (x2 - x1) + (ybf - y1) * (y2 - y1)) / rr -
            sind * r1;
      a11 = ((xbf - x1) * (-x1p) + (ybf - y1) * (-y1p)) / rr +
            ((-x1p) * (x2 - x1) + (-y1p) * (y2 - y1)) / rr -
            ((xbf - x1) * (x2 - x1) + (ybf - y1) * (y2 - y1)) * rr_s1 /
                rrsq -
            sind * r1_s1;
      a12 = ((xbf - x1) * x2p + (ybf - y1) * y2p) / rr -
            ((xbf - x1) * (x2 - x1) + (ybf - y1) * (y2 - y1)) * rr_s2 /
                rrsq;

      // Fortran: residual 2 (same segment lengths)
      rs2 = r1 - r2;
      a21 = r1_s1;
      a22 = -r2_s2;
    } else {
      // Fortran: residual 1 (small-angle approximation)
      rs1 = (r1 + r2) * sind + (s1 - s2) * ssgn;
      a11 = r1_s1 * sind + ssgn;
      a12 = r2_s2 * sind - ssgn;

      // Fortran: residual 2 (vector sum perpendicular to the contour)
      const double x1pp = math::d2val(s1, x, xp, s);
      const double y1pp = math::d2val(s1, y, yp, s);
      const double x2pp = math::d2val(s2, x, xp, s);
      const double y2pp = math::d2val(s2, y, yp, s);

      const double xtot = x1 + x2 - 2.0 * xbf;
      const double ytot = y1 + y2 - 2.0 * ybf;

      rs2 = xtot * (x1p + x2p) + ytot * (y1p + y2p);
      a21 = x1p * (x1p + x2p) + y1p * (y1p + y2p) + xtot * x1pp +
            ytot * y1pp;
      a22 = x2p * (x1p + x2p) + y2p * (y1p + y2p) + xtot * x2pp +
            ytot * y2pp;
    }

    const double det = a11 * a22 - a12 * a21;
    double ds1 = -(rs1 * a22 - a12 * rs2) / det;
    double ds2 = -(a11 * rs2 - rs1 * a21) / det;

    // Fortran: MIN/MAX clamps +-0.01*STOT
    ds1 = std::min(ds1, 0.01 * stot);
    ds1 = std::max(ds1, -0.01 * stot);
    ds2 = std::min(ds2, 0.01 * stot);
    ds2 = std::max(ds2, -0.01 * stot);

    s1 += ds1;
    s2 += ds2;
    if (std::abs(ds1) + std::abs(ds2) < kSssEps * stot) {
      converged = true;
      break;
    }
  }

  if (!converged) {
    // Fortran: WRITE 'SSS: failed to converge subtending angle points';
    // S1 = SS; S2 = SS
    result.s1 = ss;
    result.s2 = ss;
    result.status = math::Diagnostic::limitReached;
    return result;
  }

  // Fortran Label 11: DEL == 0 -> S1 = 0.5*(S1+S2); S2 = S1
  if (del == 0.0) {
    s1 = 0.5 * (s1 + s2);
    s2 = s1;
  }

  result.s1 = s1;
  result.s2 = s2;
  return result;
}

CurvatureReport clis(std::span<const double> x,
                     std::span<const double> xp,
                     std::span<const double> y,
                     std::span<const double> yp,
                     std::span<const double> s) {
  const std::size_t n = s.size();
  checkContourSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("clis: N < 2");
  }

  // Fortran: PI = 4.0*ATAN(1.0) (local, not the AECALC literal)
  const double pi = 4.0 * std::atan(1.0);

  CurvatureReport report;

  double arad = 0.0;
  // Fortran: DO 30 I=1,N
  for (std::size_t i = 0; i < n; ++i) {
    if (i == 0) {
      arad = std::atan2(-yp[i], -xp[i]);
    } else {
      arad = math::atanC(-yp[i], -xp[i], arad);
    }
    const double adeg = arad * 180.0 / pi;
    const double cv = math::curv(s[i], x, xp, y, yp, s);
    report.rows.push_back(CurvatureRow{x[i], y[i], s[i], adeg, cv});
    if (std::abs(cv) > std::abs(report.maxCurvature)) {
      report.maxCurvature = cv;
      report.maxIndex = i;  // Fortran IMAX (0-based returned)
    }
  }

  return report;
}

CornerAngleReport cang(std::span<const double> x, std::span<const double> y) {
  const std::size_t n = x.size();
  if (y.size() != n) {
    throw std::invalid_argument("cang: span lengths inconsistent");
  }
  if (n < 3) {
    throw std::invalid_argument("cang: N < 3 (Fortran I=2..N-1)");
  }

  CornerAngleReport report;
  report.angles.reserve(n - 2);

  // Fortran: DO 30 I=2,N-1
  for (std::size_t i = 1; i + 1 < n; ++i) {
    double dx1 = x[i] - x[i - 1];
    double dy1 = y[i] - y[i - 1];
    double dx2 = x[i + 1] - x[i];
    double dy2 = y[i + 1] - y[i];

    // Fortran 6.996: allow doubled points; X(I-2)/X(I+2) fallback only
    // for an existing neighbor index (Fortran out-of-bounds not replicated).
    if (dx1 == 0.0 && dy1 == 0.0) {
      if (i >= 2) {
        dx1 = x[i] - x[i - 2];
        dy1 = y[i] - y[i - 2];
      }
    }
    if (dx2 == 0.0 && dy2 == 0.0) {
      if (i + 2 <= n - 1) {
        dx2 = x[i + 2] - x[i];
        dy2 = y[i + 2] - y[i];
      }
    }

    // Fortran 6.996: CRS = DX1*DY2-DY1*DX2; DOT = DX1*DX2+DY1*DY2;
    // ANGL = ATAN2(CRS,DOT)*(180/pi).
    const double crs = dx1 * dy2 - dy1 * dx2;
    const double dot = dx1 * dx2 + dy1 * dy2;
    const double angl = std::atan2(crs, dot) * (180.0 / 3.1415926);

    report.angles.push_back(angl);
    if (std::abs(angl) > std::abs(report.maxAngle)) {
      report.maxAngle = angl;
      report.maxIndex = i;  // Fortran IMAX (0-based returned)
    }
  }

  return report;
}

void rotate(std::span<double> x, std::span<double> y, double alfa) {
  const std::size_t n = x.size();
  if (y.size() != n) {
    throw std::invalid_argument("rotate: span lengths inconsistent");
  }

  // Fortran: SA = SIN(Alfa); CA = COS(Alfa); XOFF = 0.; YOFF = 0.
  const double sa = std::sin(alfa);
  const double ca = std::cos(alfa);
  const double xoff = 0.0;
  const double yoff = 0.0;

  // Fortran: DO 8 I=1,N ; XT = X(I); YT = Y(I);
  // X(I) = CA*XT + SA*YT + XOFF; Y(I) = CA*YT - SA*XT + YOFF
  for (std::size_t i = 0; i < n; ++i) {
    const double xt = x[i];
    const double yt = y[i];
    x[i] = ca * xt + sa * yt + xoff;
    y[i] = ca * yt - sa * xt + yoff;
  }
}

TecalcResult tecalc(const TrailingEdgeInputs& in) {
  TecalcResult r;

  // Fortran: DXTE = X(1)-X(N); DYTE = Y(1)-Y(N);
  // DXS = 0.5*(-XP(1)+XP(N)); DYS = 0.5*(-YP(1)+YP(N))
  const double dxte = in.x0 - in.xn;
  const double dyte = in.y0 - in.yn;
  const double dxs = 0.5 * (-in.xp0 + in.xpn);
  const double dys = 0.5 * (-in.yp0 + in.ypn);

  // Fortran: ANTE = DXS*DYTE - DYS*DXTE; ASTE = DXS*DXTE + DYS*DYTE;
  // DSTE = SQRT(DXTE**2 + DYTE**2)
  r.ante = dxs * dyte - dys * dxte;
  r.aste = dxs * dxte + dys * dyte;
  r.dste = std::sqrt(dxte * dxte + dyte * dyte);

  // Fortran: SHARP = DSTE .LT. 0.0001*CHORD
  r.sharp = r.dste < 0.0001 * in.chord;

  if (r.sharp) {
    r.scs = 1.0;
    r.sds = 0.0;
  } else {
    r.scs = r.ante / r.dste;
    r.sds = r.aste / r.dste;
  }

  // Fortran: SIGTE = 0.5*(GAM(1)-GAM(N))*SCS; GAMTE = -.5*(...)*SDS
  // (analogous with GAM_A)
  r.sigte = 0.5 * (in.gam0 - in.gamn) * r.scs;
  r.gamte = -0.5 * (in.gam0 - in.gamn) * r.sds;
  r.sigteA = 0.5 * (in.gama0 - in.gaman) * r.scs;
  r.gamteA = -0.5 * (in.gama0 - in.gaman) * r.sds;

  return r;
}

}  // namespace xfoil::core::geometry

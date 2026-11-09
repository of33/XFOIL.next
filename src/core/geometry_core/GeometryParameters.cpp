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

// File: src/core/geometry_core/GeometryParameters.cpp
// Implementation of the geometry parameters, directly ported
// from fortran-src/xgeom.f.
//
// 0-based index mapping:
// Fortran X/XP/Y/YP/S/T(1..N) <-> C++ x/xp/y/yp/s/t[0..n-1]
// Fortran XTE = 0.5*(X(1)+X(N)) <-> 0.5*(x[0]+x[n-1])
// Fortran DO 40 I=1,N (NORM) <-> i = 0..n-1
// Fortran DO 10 IO=1,N (AECALC) <-> i0 = 0..n-1; IP=IO+1, IO==N -> IP=1
// <-> ip = i0+1 or 0 (wrap to the first element)
// Fortran DO 30 I=1,N (TCCALC) <-> i = 0..n-1
// Fortran PI (DATA 3.141592653589793238) <-> pi (identical literal)

#include "xfoil/core/geometry/GeometryParameters.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/FortranCompat.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry {

namespace {

// Fortran: DATA PI / 3.141592653589793238 /
constexpr double kPi = 3.141592653589793238;

void checkContourSpans(std::span<const double> x, std::span<const double> xp,
                       std::span<const double> y, std::span<const double> yp,
                       std::span<const double> s) {
  const std::size_t n = s.size();
  if (x.size() < n || xp.size() < n || y.size() < n || yp.size() < n) {
    throw std::invalid_argument(
        "geopar/norm/tccalc: span lengths inconsistent");
  }
}

}  // namespace

void norm(std::span<double> x, std::span<double> xp, std::span<double> y,
          std::span<double> yp, std::span<double> s) {
  const std::size_t n = s.size();
  checkContourSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("norm: N < 2");
  }

  // Fortran: CALL SCALC(X,Y,S,N); CALL SEGSPL(X,XP,S,N);
  // CALL SEGSPL(Y,YP,S,N); CALL LEFIND(SLE,...)
  math::scalc(x, y, s);
  math::segspl(x, s, xp);
  math::segspl(y, s, yp);
  const LeadingEdgeResult le = lefind(x, xp, y, yp, s);

  // Fortran 6.996: chord vector from LE to TE midpoint.
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);
  const double xle = math::seval(le.sle, x, xp, s);
  const double yle = math::seval(le.sle, y, yp, s);
  const double chord =
      std::sqrt((xte - xle) * (xte - xle) + (yte - yle) * (yte - yle));
  const double xshift = -xle;
  const double yshift = -yle;
  const double scale = 1.0 / chord;

  // Fortran: DO 40 I=1,N ; X=(X+XSHIFT)*SCALE; Y=(Y+YSHIFT)*SCALE;
  // S=S*SCALE. XP/YP remain as set by SEGSPL before scaling.
  for (std::size_t i = 0; i < n; ++i) {
    x[i] = (x[i] + xshift) * scale;
    y[i] = (y[i] + yshift) * scale;
    s[i] = s[i] * scale;
  }
}

GeometryMetrics geopar(std::span<const double> x,
                       std::span<const double> xp,
                       std::span<const double> y,
                       std::span<const double> yp,
                       std::span<const double> s) {
  const std::size_t n = s.size();
  checkContourSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("geopar: N < 2");
  }

  GeometryMetrics m;

  // Fortran: CALL LEFIND(SLE,X,XP,Y,YP,S,N)
  const LeadingEdgeResult le = lefind(x, xp, y, yp, s);
  m.sle = le.sle;
  if (le.status == LeStatus::limitReached) {
    ++m.lefindFailures;  // Fortran WRITE 'LEFIND: LE point not found.'
  }

  // Fortran: XLE/YLE = SEVAL(SLE,...); XTE/YTE = 0.5*(X(1)+X(N));
  // CHSQ; CHORD = SQRT(CHSQ)
  const double xle = math::seval(m.sle, x, xp, s);
  const double yle = math::seval(m.sle, y, yp, s);
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);
  const double chsq =
      (xte - xle) * (xte - xle) + (yte - yle) * (yte - yle);
  m.chord = std::sqrt(chsq);

  // Fortran: CURVLE = CURV(SLE,...); RADLE = 0.0;
  // IF(ABS(CURVLE) > 0.001*(S(N)-S(1))) RADLE = 1.0/CURVLE
  const double curvle = math::curv(m.sle, x, xp, y, yp, s);
  m.radle = 0.0;
  if (std::abs(curvle) > 0.001 * (s[n - 1] - s[0])) {
    m.radle = 1.0 / curvle;
  }

  // Fortran: ANG1 = ATAN2(-YP(1),-XP(1)); ANG2 = ATANC(YP(N),XP(N),ANG1);
  // ANGTE = ANG2 - ANG1
  const double ang1 = std::atan2(-yp[0], -xp[0]);
  const double ang2 = math::atanC(yp[n - 1], xp[n - 1], ang1);
  m.angte = ang2 - ang1;

  // Fortran: DO I=1,N ; T(I) = 1.0 (T is Workspace, no API parameter)
  std::vector<double> t(n, 1.0);

  // Fortran: CALL AECALC(N,X,Y,T,1, AREA,XCENA,YCENA,EI11A,...)
  // (XCENA/YCENA be as in Fortran discarded)
  const SectionProperties area = aeCalc(x, y, t, 1);
  m.area = area.area;
  m.ei11a = area.ei11;
  m.ei22a = area.ei22;
  m.apx1a = area.apx1;
  m.apx2a = area.apx2;

  // Fortran: CALL AECALC(N,X,Y,T,2, SLEN,XCENT,YCENT,EI11T,...)
  // (SLEN/XCENT/YCENT are discarded as in Fortran)
  const SectionProperties skin = aeCalc(x, y, t, 2);
  m.ei11t = skin.ei11;
  m.ei22t = skin.ei22;
  m.apx1t = skin.apx1;
  m.apx2t = skin.apx2;

  // Fortran: CALL TCCALC(X,XP,Y,YP,S,N, THICK,XTHICK,CAMBR,XCAMBR)
  // The Fortran WRITE diagnostics of the 1000 formats (THICK,XTHICK,CAMBR,
  // XCAMBR) are returned as structured fields.
  const ThicknessCamberResult tc = tccalc(x, xp, y, yp, s);
  m.thick = tc.thick;
  m.thickX = tc.thickX;
  m.cambr = tc.cambr;
  m.cambrX = tc.cambrX;
  m.oppositePointFailures = tc.oppositePointFailures;
  m.lefindFailures += tc.lefindFailures;

  return m;
}

SectionProperties aeCalc(std::span<const double> x,
                         std::span<const double> y,
                         std::span<const double> t, int itype) {
  if (itype != 1 && itype != 2) {
    throw std::invalid_argument(
        "aeCalc: ITYPE must be 1 (Area) or 2 (Skin) ");
  }
  const std::size_t n = x.size();
  if (y.size() < n || t.size() < n) {
    throw std::invalid_argument("aeCalc: span lengths inconsistent");
  }
  if (n < 1) {
    throw std::invalid_argument("aeCalc: N < 1");
  }

  // Fortran: SINT/AINT/XINT/YINT/XXINT/XYINT/YYINT = 0.0
  double sint = 0.0;
  double aint = 0.0;
  double xint = 0.0;
  double yint = 0.0;
  double xxint = 0.0;
  double xyint = 0.0;
  double yyint = 0.0;

  // Fortran: DO 10 IO=1,N ; IP = IO+1 or IP=1 (IO==N, closing segment)
  for (std::size_t i0 = 0; i0 < n; ++i0) {
    const std::size_t ip = (i0 + 1 == n) ? 0 : i0 + 1;
    const double dx = x[i0] - x[ip];
    const double dy = y[i0] - y[ip];
    const double xa = (x[i0] + x[ip]) * 0.50;
    const double ya = (y[i0] + y[ip]) * 0.50;
    const double ta = (t[i0] + t[ip]) * 0.50;
    const double ds = std::sqrt(dx * dx + dy * dy);
    sint += ds;

    if (itype == 1) {
      // Fortran: Since = YA*DX; YINT += YA*Since/2.0; XYINT += XA*YA*Since/2.0;
      // YYINT += YA*YA*Since/3.0
      const double da = ya * dx;
      aint += da;
      xint += xa * da;
      yint += ya * da / 2.0;
      xxint += xa * xa * da;
      xyint += xa * ya * da / 2.0;
      yyint += ya * ya * da / 3.0;
    } else {
      // Fortran: Since = TA*DS (all terms without division)
      const double da = ta * ds;
      aint += da;
      xint += xa * da;
      yint += ya * da;
      xxint += xa * xa * da;
      xyint += xa * ya * da;
      yyint += ya * ya * da;
    }
  }

  SectionProperties p;
  p.area = aint;

  // Fortran: IF(AINT.EQ. 0.0) -> zero values, APX2 = ATAN2(1.0,0.0)
  if (aint == 0.0) {
    p.xcen = 0.0;
    p.ycen = 0.0;
    p.ei11 = 0.0;
    p.ei22 = 0.0;
    p.apx1 = 0.0;
    p.apx2 = std::atan2(1.0, 0.0);
    return p;
  }

  p.xcen = xint / aint;
  p.ycen = yint / aint;

  // Fortran: EIXX = YYINT - YCEN*YCEN*AINT; EIXY = XYINT - XCEN*YCEN*AINT;
  // EIYY = XXINT - XCEN*XCEN*AINT
  const double eixx = yyint - p.ycen * p.ycen * aint;
  const double eixy = xyint - p.xcen * p.ycen * aint;
  const double eiyy = xxint - p.xcen * p.xcen * aint;

  // Fortran: EISQ = 0.25*(EIXX-EIYY)**2 + EIXY**2
  // SGN = SIGN(1.0, EIYY-EIXX) (Sign comparison >= 0)
  // EI11 = 0.5*(EIXX+EIYY) - SGN*SQRT(EISQ)
  // EI22 = 0.5*(EIXX+EIYY) + SGN*SQRT(EISQ)
  const double eisq =
      0.25 * (eixx - eiyy) * (eixx - eiyy) + eixy * eixy;
  const double sgn = (eiyy - eixx >= 0.0) ? 1.0 : -1.0;
  p.ei11 = 0.5 * (eixx + eiyy) - sgn * std::sqrt(eisq);
  p.ei22 = 0.5 * (eixx + eiyy) + sgn * std::sqrt(eisq);

  if (p.ei11 == 0.0 || p.ei22 == 0.0) {
    // Fortran: vanishing cross-section stiffness
    p.apx1 = 0.0;
    p.apx2 = std::atan2(1.0, 0.0);
  } else if (eisq / (p.ei11 * p.ei22) <
             (0.001 * sint) * (0.001 * sint) * (0.001 * sint) *
                 (0.001 * sint)) {
    // Fortran: EISQ/(EI11*EI22) .LT. (0.001*SINT)**4
    // (rotation-invariant cross-section: circle, square, ...)
    p.apx1 = 0.0;
    p.apx2 = std::atan2(1.0, 0.0);
  } else {
    // Fortran: regular cross-section; C1/S1/C2/S2, wrap around +-0.5*PI
    const double c1 = eixy;
    const double s1 = eixx - p.ei11;
    const double c2 = eixy;
    const double s2 = eixx - p.ei22;

    if (std::abs(s1) > std::abs(s2)) {
      p.apx1 = std::atan2(s1, c1);
      p.apx2 = p.apx1 + 0.5 * kPi;
    } else {
      p.apx2 = std::atan2(s2, c2);
      p.apx1 = p.apx2 - 0.5 * kPi;
    }

    if (p.apx1 < -0.5 * kPi) {
      p.apx1 += kPi;
    }
    if (p.apx1 > 0.5 * kPi) {
      p.apx1 -= kPi;
    }
    if (p.apx2 < -0.5 * kPi) {
      p.apx2 += kPi;
    }
    if (p.apx2 > 0.5 * kPi) {
      p.apx2 -= kPi;
    }
  }

  return p;
}

ThicknessCamberResult tccalc(std::span<const double> x,
                             std::span<const double> xp,
                             std::span<const double> y,
                             std::span<const double> yp,
                             std::span<const double> s) {
  const std::size_t n = s.size();
  checkContourSpans(x, xp, y, yp, s);
  if (n < 2) {
    throw std::invalid_argument("tccalc: N < 2");
  }

  // Fortran: CALL LEFIND(SLE,...); XLE/YLE/XTE/YTE; CHORD; DXC/DYC
  const LeadingEdgeResult le = lefind(x, xp, y, yp, s);
  ThicknessCamberResult r;  // Fortran: THICK/XTHICK/CAMBR/XCAMBR = 0.
  if (le.status == LeStatus::limitReached) {
    ++r.lefindFailures;  // Fortran WRITE 'LEFIND: LE point not found.'
  }
  const double xle = math::seval(le.sle, x, xp, s);
  const double yle = math::seval(le.sle, y, yp, s);
  const double xte = 0.5 * (x[0] + x[n - 1]);
  const double yte = 0.5 * (y[0] + y[n - 1]);
  const double chord =
      std::sqrt((xte - xle) * (xte - xle) + (yte - yle) * (yte - yle));
  const double dxc = (xte - xle) / chord;
  const double dyc = (yte - yle) / chord;

  // Fortran: DO 30 I=1,N
  // Note: Fortran computed XBAR in the loop, used it but
  // not (dead code); it is not adopted.
  for (std::size_t i = 0; i < n; ++i) {
    const double ybar = (y[i] - yle) * dxc - (x[i] - xle) * dyc;

    // Fortran: CALL SOPPS(SOPP, S(I), X,XP,Y,YP,S,N, SLE)
    const OppositePointResult op = sopps(s[i], x, xp, y, yp, s, le.sle);
    if (op.status != math::Diagnostic::completed) {
      // Fortran: WRITE(*,*) 'SOPPS: Opposite-point location failed.
      // Continuing...' (the kernel only counts; the CLI text follows later).
      ++r.oppositePointFailures;
    }
    const double xopp = math::seval(op.sopp, x, xp, s);
    const double yopp = math::seval(op.sopp, y, yp, s);

    const double ybarop = (yopp - yle) * dxc - (xopp - xle) * dyc;
    const double yc = 0.5 * (ybar + ybarop);
    const double yt = std::abs(ybar - ybarop);

    if (std::abs(yc) > std::abs(r.cambr)) {
      r.cambr = yc;
      r.cambrX = xopp;
    }
    if (std::abs(yt) > std::abs(r.thick)) {
      r.thick = yt;
      r.thickX = xopp;
    }
  }

  return r;
}

}  // namespace xfoil::core::geometry

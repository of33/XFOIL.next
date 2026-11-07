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

// File: src/core/geometry_core/Naca.cpp
// Implementation of the NACA cores, directly ported from
// fortran-src/naca.f.
//
// 0-based index mapping:
// Fortran XX/YT/YC(1..NSIDE) <-> C++ xx/yt/yc[0..nside-1]
// Fortran XB/YB(1..2*NSIDE-1) <-> C++ xb/yb[0..2*nside-2]
// Fortran DO 10 I=1,NSIDE <-> i = 0..nside-1
// Fortran DO 20 I=NSIDE,1,-1 <-> i = nside-1..0 (downward loop)
// Fortran DO 30 I=2,NSIDE <-> i = 1..nside-1
// Fortran IB (1-based counter, starts at 0) <-> ib (0-based write index)

#include "xfoil/core/geometry/Naca.h"

#include <cmath>
#include <utility>

#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::geometry {

namespace {

// Fortran: DIGITS = '0123456789'; DIGITS(N+1:N+1) -> '0' + N.
inline char digitChar(int n) {
  return static_cast<char>('0' + n);
}

// Canonical buffer construction from the side arrays (identical in NACA4
// and NACA5):
// Fortran DO 20 I=NSIDE,1,-1 : upper side from LE to TE, YC+YT
// Fortran DO 30 I=2,NSIDE : lower side from point 2, YC-YT
// Fortran NB = IB
void buildBuffer(const std::vector<double>& xx, const std::vector<double>& yt,
                 const std::vector<double>& yc, std::vector<double>& xb,
                 std::vector<double>& yb) {
  const std::size_t nside = xx.size();
  xb.resize(2 * nside - 1);
  yb.resize(2 * nside - 1);
  std::size_t ib = 0;  // Fortran IB = 0
  for (std::size_t i = nside; i-- > 0;) {  // Fortran I = NSIDE..1, -1
    xb[ib] = xx[i];
    yb[ib] = yc[i] + yt[i];
    ib += 1;
  }
  for (std::size_t i = 1; i < nside; ++i) {  // Fortran I = 2..NSIDE
    xb[ib] = xx[i];
    yb[ib] = yc[i] - yt[i];
    ib += 1;
  }
}

// Canonical Thickness distribution (identical in NACA4 and NACA5):
// Fortran: YT(I) = (0.29690*SQRT(XX) - 0.12600*XX - 0.35160*XX**2
// + 0.28430*XX**3 - 0.10150*XX**4) * T / 0.20
inline double thickness(double xx, double t) {
  const double xx2 = xx * xx;    // Fortran XX(I)**2
  const double xx3 = xx2 * xx;   // Fortran XX(I)**3
  const double xx4 = xx2 * xx2;  // Fortran XX(I)**4
  return (0.29690 * std::sqrt(xx) - 0.12600 * xx - 0.35160 * xx2 +
          0.28430 * xx3 - 0.10150 * xx4) *
         t / 0.20;
}

// Canonical X distribution with TE point bundling (identical in NACA4 and
// NACA5):
// Fortran: FRAC = FLOAT(I-1)/FLOAT(NSIDE-1)
// I == NSIDE: XX = 1.0
// otherwise: XX = 1.0 - ANP*FRAC*(1.0-FRAC)**On - (1.0-FRAC)**ANP
inline double chordX(std::size_t i, std::size_t nside) {
  const double frac = static_cast<double>(i) / static_cast<double>(nside - 1);
  if (i + 1 == nside) {
    return 1.0;
  }
  constexpr double an = 1.5;  // Fortran DATA On / 1.5 /
  const double anp = an + 1.0;
  const double omp = 1.0 - frac;
  return 1.0 - anp * frac * std::pow(omp, an) - std::pow(omp, anp);
}

}  // namespace

Result<NacaResult> naca4(int ides, std::size_t nside) {
  if (ides < 0 || ides > 9999) {
    return Result<NacaResult>::fail(Error::make(
        ErrorCode::invalidArgument,
        "NACA4: Identifier outside 0..9999 (Fortran-Callers NACA "
        "limited IDES; NAME-Digit access DIGITS(N4+1))"));
  }
  if (nside < 2) {
    return Result<NacaResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "NACA4: NSIDE < 2 (Fortran FLOAT(NSIDE-1) Division)"));
  }

  // Fortran: N4 = IDES/1000; N3 = ...; N2 = ...; N1 = ...
  const int n4 = ides / 1000;
  const int n3 = (ides - n4 * 1000) / 100;
  const int n2 = (ides - n4 * 1000 - n3 * 100) / 10;
  const int n1 = (ides - n4 * 1000 - n3 * 100 - n2 * 10);

  // Fortran: M = FLOAT(N4)/100.0; P = FLOAT(N3)/10.0;
  // T = FLOAT(N2*10 + N1)/100.0
  const double m = static_cast<double>(n4) / 100.0;
  const double p = static_cast<double>(n3) / 10.0;
  const double t = static_cast<double>(n2 * 10 + n1) / 100.0;

  NacaResult r;
  r.xx.resize(nside);
  r.yt.resize(nside);
  r.yc.resize(nside);
  for (std::size_t i = 0; i < nside; ++i) {  // Fortran DO 10 I=1,NSIDE
    const double xx = chordX(i, nside);
    r.xx[i] = xx;
    r.yt[i] = thickness(xx, t);
    if (xx < p) {  // Fortran: XX(I) .LT. P
      r.yc[i] = m / (p * p) * (2.0 * p * xx - xx * xx);
    } else {
      r.yc[i] = m / ((1.0 - p) * (1.0 - p)) *
                ((1.0 - 2.0 * p) + 2.0 * p * xx - xx * xx);
    }
  }

  buildBuffer(r.xx, r.yt, r.yc, r.xb, r.yb);

  // Fortran: Name = 'NACA'; Name(6:9) = digits (position 5 stays a space)
  r.name = std::string("NACA ") + digitChar(n4) + digitChar(n3) +
           digitChar(n2) + digitChar(n1);

  return Result<NacaResult>::ok(std::move(r));
}

Result<Naca5Result> naca5(int ides, std::size_t nside) {
  if (nside < 2) {
    return Result<Naca5Result>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "NACA5: NSIDE < 2 (Fortran FLOAT(NSIDE-1) Division)"));
  }

  // Fortran: N5..N1 as NACA4 with 10000it digit; N543 = 100*N5+10*N4+N3
  const int n5 = ides / 10000;
  const int n4 = (ides - n5 * 10000) / 1000;
  const int n3 = (ides - n5 * 10000 - n4 * 1000) / 100;
  const int n2 = (ides - n5 * 10000 - n4 * 1000 - n3 * 100) / 10;
  const int n1 = (ides - n5 * 10000 - n4 * 1000 - n3 * 100 - n2 * 10);

  const int n543 = 100 * n5 + 10 * n4 + n3;

  double m = 0.0;
  double c = 0.0;
  if (n543 == 210) {  // exact adoption of the Fortran branch
    m = 0.0580;
    c = 361.4;
  } else if (n543 == 220) {
    m = 0.1260;
    c = 51.64;
  } else if (n543 == 230) {
    m = 0.2025;
    c = 15.957;
  } else if (n543 == 240) {
    m = 0.2900;
    c = 6.643;
  } else if (n543 == 250) {
    m = 0.3910;
    c = 3.230;
  } else {
    // Fortran: WRITE(*,*) 2 lines; IDES = 0; RETURN without points
    return Result<Naca5Result>::fail(Error::make(
        ErrorCode::invalidArgument,
        "Illegal 5-digit designation; "
        "First three digits must be 210, 220, ... 250"));
  }

  const double t = static_cast<double>(n2 * 10 + n1) / 100.0;

  Naca5Result r;
  r.xx.resize(nside);
  r.yt.resize(nside);
  r.yc.resize(nside);
  for (std::size_t i = 0; i < nside; ++i) {  // Fortran DO 10 I=1,NSIDE
    const double xx = chordX(i, nside);
    r.xx[i] = xx;
    r.yt[i] = thickness(xx, t);
    if (xx < m) {  // Fortran: XX(I) .LT. M
      const double xx2 = xx * xx;
      const double xx3 = xx2 * xx;
      r.yc[i] = (c / 6.0) * (xx3 - 3.0 * m * xx2 + m * m * (3.0 - m) * xx);
    } else {
      const double m3 = m * m * m;  // Fortran M**3
      r.yc[i] = (c / 6.0) * m3 * (1.0 - xx);
    }
  }

  buildBuffer(r.xx, r.yt, r.yc, r.xb, r.yb);

  // Fortran: Name(6:10) = five digits
  r.name = std::string("NACA ") + digitChar(n5) + digitChar(n4) +
           digitChar(n3) + digitChar(n2) + digitChar(n1);

  return Result<Naca5Result>::ok(std::move(r));
}

Result<NacaAirfoilResult> nacaAirfoil(int ides) {
  // Fortran NACA (xfoil.f): NSIDE = IQX/3 = 123 (integer division).
  constexpr std::size_t nside = 123;

  // Fortran: ITYPE = 0; IF(IDES.LE.25099) ITYPE = 5;
  // IF(IDES.LE.9999) ITYPE = 4
  int itype = 0;
  if (ides <= 25099) {
    itype = 5;
  }
  if (ides <= 9999) {
    itype = 4;
  }

  // Fortran: IF(ITYPE.EQ.0) WRITE 'This designation not implemented.';
  // RETURN. IDES <= 0 corresponds to the dialog case (ASKI).
  if (itype == 0 || ides <= 0) {
    return Result<NacaAirfoilResult>::fail(Error::make(
        ErrorCode::invalidArgument,
        "This designation not implemented. (Fortran-Dialog ASKI "
        "not part of the core operation)"));
  }

  // Fortran: IF(ITYPE.EQ.4) CALL NACA4(...); IF(ITYPE.EQ.5) CALL NACA5(...)
  if (itype == 4) {
    const Result<NacaResult> nr = naca4(ides, nside);
    if (nr.isErr()) {
      // Fortran: IF(IDES.EQ.0) RETURN (no buffer set)
      return Result<NacaAirfoilResult>::fail(nr.error());
    }
    const NacaResult& n = nr.value();

    // Fortran: CALL SCALC(XB,YB,SB,NB); CALL SEGSPL(XB,XBP,SB,NB);
    // CALL SEGSPL(YB,YBP,SB,NB); CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,...)
    SplineProfile prof;
    prof.x = n.xb;
    prof.y = n.yb;
    prof.s.resize(n.xb.size());
    prof.xp.resize(n.xb.size());
    prof.yp.resize(n.xb.size());
    math::scalc(prof.x, prof.y, prof.s);
    math::segspl(prof.x, prof.s, prof.xp);
    math::segspl(prof.y, prof.s, prof.yp);
    const GeometryMetrics metrics =
        geopar(prof.x, prof.xp, prof.y, prof.yp, prof.s);
    prof.sle = metrics.sle;

  // Fortran: CALL PANGEN(.TRUE.) follows in the paneling path.
    NacaAirfoilResult out;
    out.buffer = AirfoilBuffer(n.xb, n.yb, n.name);
    out.metrics = metrics;
    return Result<NacaAirfoilResult>::ok(std::move(out));
  }

  const Result<Naca5Result> nr = naca5(ides, nside);
  if (nr.isErr()) {
    // Fortran: IF(IDES.EQ.0) RETURN (no buffer set)
    return Result<NacaAirfoilResult>::fail(nr.error());
  }
  const Naca5Result& n = nr.value();

  // Fortran: CALL SCALC(XB,YB,SB,NB); CALL SEGSPL(XB,XBP,SB,NB);
  // CALL SEGSPL(YB,YBP,SB,NB); CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,...)
  SplineProfile prof;
  prof.x = n.xb;
  prof.y = n.yb;
  prof.s.resize(n.xb.size());
  prof.xp.resize(n.xb.size());
  prof.yp.resize(n.xb.size());
  math::scalc(prof.x, prof.y, prof.s);
  math::segspl(prof.x, prof.s, prof.xp);
  math::segspl(prof.y, prof.s, prof.yp);
  const GeometryMetrics metrics =
      geopar(prof.x, prof.xp, prof.y, prof.yp, prof.s);
  prof.sle = metrics.sle;

  // Fortran: CALL PANGEN(.TRUE.) follows in the paneling path.
  NacaAirfoilResult out;
  out.buffer = AirfoilBuffer(n.xb, n.yb, n.name);
  out.metrics = metrics;
  return Result<NacaAirfoilResult>::ok(std::move(out));
}

}  // namespace xfoil::core::geometry

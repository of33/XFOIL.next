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

// File: src/core/paneling/PanelGeometryKernels.cpp
// Panel geometry kernel of the new 0-based core, directly
// ported from fortran-src/xpanel.f (APCALC, NCALC) and the
// Geometry share of fortran-src/xfoil.f TECALC.
//
// Formulas, operation order and special cases are adopted exactly from
// Fortran:
// - APCALC: degenerate panels (SX==SY==0) use the panel-normal azimuth,
// the TE panel for a sharp trailing edge uses the value PI (Fortran
// COMMON/CR11/: PI = 4.0*ATAN(1.0) as in INIT).
// - NCALC: xn/yn are first used as SEGSPL work areas
// (canonical Fortran behavior), degenerate normals -> (-1,0),
// corner averaging for duplicate points.
// - TECALC: only the geometry share (SIGTE/GAMTE = solver state).
//

#include "xfoil/core/paneling/PanelGeometryKernels.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include "xfoil/core/math/Spline.h"

namespace xfoil::core::paneling {

namespace {

// Fortran COMMON/CR11/: PI = 4.0*ATAN(1.0) (xfoil.f INIT).
double pi() {
  return 4.0 * std::atan(1.0);
}

}  // namespace

// Fortran: SUBROUTINE TECALC (Geometry share).
TrailingEdgeGeometry teGeometry(double x0, double y0, double xp0, double yp0,
                                double xn, double yn, double xpn, double ypn,
                                double chord) {
  if (!(chord > 0.0)) {
    throw std::invalid_argument(
        "xfoil::core::paneling::teGeometry: chord must be positive");
  }

  TrailingEdgeGeometry result;

  // Fortran: DXTE = X(1) - X(N), DYTE = Y(1) - Y(N),
  // DXS = 0.5*(-XP(1) + XP(N)), DYS = 0.5*(-YP(1) + YP(N))
  const double dxte = x0 - xn;
  const double dyte = y0 - yn;
  const double dxs = 0.5 * (-xp0 + xpn);
  const double dys = 0.5 * (-yp0 + ypn);

  // Fortran: ANTE = DXS*DYTE - DYS*DXTE, ASTE = DXS*DXTE + DYS*DYTE
  result.ante = dxs * dyte - dys * dxte;
  result.aste = dxs * dxte + dys * dyte;

  // Fortran: DSTE = SQRT(DXTE**2 + DYTE**2), SHARP = DSTE .LT. 0.0001*CHORD
  result.dste = std::sqrt(dxte * dxte + dyte * dyte);
  result.sharp = result.dste < 0.0001 * chord;

  // Fortran: SCS/SDS
  if (result.sharp) {
    result.scs = 1.0;
    result.sds = 0.0;
  } else {
    result.scs = result.ante / result.dste;
    result.sds = result.aste / result.dste;
  }

  return result;
}

// Fortran: SUBROUTINE NCALC(X,Y,S,N,XN,YN).
void ncalc(std::span<const double> x, std::span<const double> y,
           std::span<const double> s, std::span<double> xn,
           std::span<double> yn) {
  const std::size_t n = x.size();
  if (n != y.size() || n != s.size() || n != xn.size() || n != yn.size()) {
    throw std::invalid_argument(
        "xfoil::core::paneling::ncalc: all lengths must be equal ");
  }
  if (n <= 1) {
    return;
  }

  // Fortran: SEGSPL(X,XN,S,N), SEGSPL(Y,YN,S,N)
  math::segspl(x, s, xn);
  math::segspl(y, s, yn);

  // Fortran: normals I=1..N (SX=YN(I), SY=-XN(I), normalization).
  for (std::size_t i = 0; i < n; ++i) {
    const double sx = yn[i];
    const double sy = -xn[i];
    const double smod = std::sqrt(sx * sx + sy * sy);
    if (smod == 0.0) {
      xn[i] = -1.0;
      yn[i] = 0.0;
    } else {
      xn[i] = sx / smod;
      yn[i] = sy / smod;
    }
  }

  // Fortran: Corner averaging I=1..N-1 (S(I) == S(I+1)).
  for (std::size_t i = 0; i + 1 < n; ++i) {
    if (s[i] != s[i + 1]) {
      continue;
    }
    const double sx = 0.5 * (xn[i] + xn[i + 1]);
    const double sy = 0.5 * (yn[i] + yn[i + 1]);
    const double smod = std::sqrt(sx * sx + sy * sy);
    if (smod == 0.0) {
      xn[i] = -1.0;
      yn[i] = 0.0;
      xn[i + 1] = -1.0;
      yn[i + 1] = 0.0;
    } else {
      xn[i] = sx / smod;
      yn[i] = sy / smod;
      xn[i + 1] = sx / smod;
      yn[i + 1] = sy / smod;
    }
  }
}

  // Fortran: SUBROUTINE APCALC (no arguments, COMMON-based).
std::vector<double> apcalc(std::span<const double> x,
                           std::span<const double> y,
                           std::span<const double> xn,
                           std::span<const double> yn, bool sharpTe) {
  const std::size_t n = x.size();
  if (n != y.size() || n != xn.size() || n != yn.size()) {
    throw std::invalid_argument(
        "xfoil::core::paneling::apcalc: all lengths must be equal ");
  }
  if (n < 1) {
    throw std::invalid_argument(
        "xfoil::core::paneling::apcalc: at least 1 Node required");
  }

  std::vector<double> apanel(n, 0.0);

  // Fortran: Panels I=1..N-1.
  for (std::size_t i = 0; i + 1 < n; ++i) {
    const double sx = x[i + 1] - x[i];
    const double sy = y[i + 1] - y[i];
    if (sx == 0.0 && sy == 0.0) {
      apanel[i] = std::atan2(-yn[i], -xn[i]);
    } else {
      apanel[i] = std::atan2(sx, -sy);
    }
  }

  // Fortran: TE panel I=N, IP=1.
  const std::size_t iTe = n - 1;
  if (sharpTe) {
    apanel[iTe] = pi();
  } else {
    const double sx = x[0] - x[iTe];
    const double sy = y[0] - y[iTe];
    apanel[iTe] = std::atan2(-sx, sy) + pi();
  }

  return apanel;
}

}  // namespace xfoil::core::paneling

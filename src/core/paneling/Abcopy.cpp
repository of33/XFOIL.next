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

// File: src/core/paneling/Abcopy.cpp
// ABCOPY of the new 0-based core, directly ported from
// fortran-src/xgdes.f ABCOPY.
//
// Formulas, operation order and special cases are adopted exactly from
// Fortran:
// - NB <= 1 or NB > IQX-5 = 596: controlled rejection without setting
// the current contour (Fortran messages are returned in structured
// form).
// - The duplicate-point distance loop preserves the exact Fortran
// Behavior: I is incremented before the comparison and is not reset
// after a duplicate removal (a run of k duplicate points removes only a
// part in Fortran; the behavior is preserved).
// - SCALC/SEGSPL/NCALC/LEFIND/TECALC/APCALC in Fortran order.
// - The session flags (LGAMU..LGSAME, LBLINI, LIPAN, LVCONV, LSCINI) and
// the flap state (LBFLAP/XOF/YOF) are session state and live
// outside the kernel.
// - TECALC is used only for the geometry part (SIGTE/GAMTE =
// solver state).
//
// The LCONF parameter controls only the confirmation output in Fortran
// ('Current airfoil nodes set from buffer airfoil nodes ( N )')
// and is not present in the kernel.

#include "xfoil/core/paneling/Abcopy.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/PanelGeometryKernels.h"

namespace xfoil::core::paneling {

using math::scalc;
using math::segspl;
using math::seval;

// Fortran: SUBROUTINE ABCOPY(LCONF).
Result<AbcopyResult> abcopy(const geometry::AirfoilBuffer& buffer) {
  const std::size_t nb = buffer.x.size();
  if (nb != buffer.y.size()) {
    throw std::invalid_argument(
        "xfoil::core::paneling::abcopy: buffer lengths must be equal ");
  }
  // Fortran: NB <= 1 -> message, return without change.
  if (nb <= 1) {
    return Result<AbcopyResult>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "ABCOPY: Buffer airfoil not available."));
  }
  // Fortran: NB > IQX-5 -> current contour is not set.
  if (nb > kMaxAbcopyNodeCount) {
    return Result<AbcopyResult>::fail(
        Error::make(ErrorCode::capacityExceeded,
                    "Current airfoil cannot be set."));
  }

  // Fortran: N = NB, X(I) = XB(I), Y(I) = YB(I).
  std::vector<double> x(buffer.x);
  std::vector<double> y(buffer.y);
  std::size_t n = nb;

  // Fortran: Duplicate points remove (I is first increased, after a
  // Distance not reset).
  {
    std::size_t i0 = 0;  // Fortran I = 1 (0-based)
    do {
      i0 += 1;  // Fortran I = I+1
      if (x[i0 - 1] == x[i0] && y[i0 - 1] == y[i0]) {
        // Fortran: Shift J=I..N-1 -> 0-based j0 = i0..n-2.
        for (std::size_t j0 = i0; j0 + 1 < n; ++j0) {
          x[j0] = x[j0 + 1];
          y[j0] = y[j0 + 1];
        }
        n -= 1;
      }
    } while (i0 < n - 1);  // Fortran IF(I.LT.N) -> 0-based i0 < n-1
  }

  // Fortran: SCALC/SEGSPL x2. The vectors keep the Fortran capacity
  // (nb); the logical size after deduplication is n.
  const std::span<const double> fx(x.data(), n);
  const std::span<const double> fy(y.data(), n);
  std::vector<double> s(n);
  scalc(fx, fy, s);
  std::vector<double> xp(n);
  std::vector<double> yp(n);
  segspl(fx, s, xp);
  segspl(fy, s, yp);

  // Fortran: NCALC.
  std::vector<double> nx(n);
  std::vector<double> ny(n);
  ncalc(fx, fy, s, nx, ny);

  // Fortran: LEFIND -> XLE/YLE.
  const geometry::LeadingEdgeResult le = geometry::lefind(fx, xp, fy, yp, s);
  const double sle = le.sle;
  const double xle = seval(sle, fx, xp, s);
  const double yle = seval(sle, fy, yp, s);

  // Fortran: XTE/YTE/CHORD.
  const double xte = 0.5 * (fx[0] + fx[n - 1]);
  const double yte = 0.5 * (fy[0] + fy[n - 1]);
  const double chord =
      std::sqrt((xte - xle) * (xte - xle) + (yte - yle) * (yte - yle));

  // Fortran: TECALC (geometry share only) then APCALC.
  const TrailingEdgeGeometry te =
      teGeometry(fx[0], fy[0], xp[0], yp[0], fx[n - 1], fy[n - 1], xp[n - 1],
                 yp[n - 1], chord);
  const bool sharpTe = te.sharp;
  std::vector<double> apanel = apcalc(fx, fy, nx, ny, sharpTe);

  // Fortran: X/Y keep the logical size N.
  x.resize(n);
  y.resize(n);

  AbcopyResult out;
  out.leStatus = le.status;
  out.panel = PanelGeometry::make(std::move(x), std::move(y), std::move(s),
                                  std::move(xp), std::move(yp),
                                  std::move(nx), std::move(ny),
                                  std::move(apanel), sle, xle, yle, xte,
                                  yte, chord, sharpTe);
  return Result<AbcopyResult>::ok(std::move(out));
}

}  // namespace xfoil::core::paneling

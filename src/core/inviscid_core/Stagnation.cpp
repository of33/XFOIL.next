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

// File: src/core/inviscid_core/Stagnation.cpp
// Stagnation-point and velocity kernels of the new 0-based core,
// ported directly from fortran-src/xpanel.f (STFIND, QISET).
//
// Formulas and special cases exactly as in Fortran:
// - STFIND: Loop I=1..N-1 (GAM(I)>=0 AND GAM(I+1)<0 -> Label 11); not
// found -> I=N/2 + message; DGAM/DS; GAM(I) < -GAM(I+1) ->
// SST=S(I)-DS*(GAM(I)/DGAM), otherwise SST=S(I+1)-DS*(GAM(I+1)/DGAM);
// tweak at the node (1e-7); SST_GO=(SST-S(I+1))/DGAM,
// SST_GP=(S(I)-SST)/DGAM.
// - QISET: COSA/SINA; loop I=1..N+NW:
// QINV=COSA*QINVU(I,1)+SINA*QINVU(I,2),
// QINV_A=-SINA*QINVU(I,1)+COSA*QINVU(I,2).

#include "xfoil/core/inviscid/Stagnation.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace xfoil::core::inviscid {

// Fortran: SUBROUTINE STFIND.
StagnationPointResult stfind(std::span<const double> gam,
                             std::span<const double> s) {
  const std::size_t n = gam.size();
  if (n != s.size()) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::stfind: GAM and S lengths must be equal");
  }

  StagnationPointResult result;

  // Fortran: Loop I=1..N-1 (0-based i0 = I-1).
  std::size_t i = n - 1;  // Fortran default for not found: I = N/2
  bool found = false;
  for (std::size_t i0 = 0; i0 + 1 < n; ++i0) {
    if (gam[i0] >= 0.0 && gam[i0 + 1] < 0.0) {
      i = i0;  // 0-based
      found = true;
      break;
    }
  }
  if (!found) {
    // Fortran: 'STFIND: Stagnation point not found. Continuing ...'
    // I = N/2 (integer division, 1-based array index field).
    // 0-based corresponds to I-1 = N/2 - 1 (binary
    // oper-state/oper-init-inviscid-then-bang: IBLPAN fallback with
    // N=161 returns IBLTE(1)=81, IBLTE(2)=82, not 82/81).
    result.found = false;
    i = (n / 2 > 0) ? n / 2 - 1 : 0;
    if (i + 1 >= n) {
      i = 0;
    }
  }

  // Fortran: Is = I (1-based) -> 0-based is = I - 1.
  result.ist = i;

  const double dgam = gam[i + 1] - gam[i];
  const double ds = s[i + 1] - s[i];

  // Fortran: evaluation chosen to minimize rounding.
  if (gam[i] < -gam[i + 1]) {
    result.sst = s[i] - ds * (gam[i] / dgam);
  } else {
    result.sst = s[i + 1] - ds * (gam[i + 1] / dgam);
  }

  // Fortran: refine if the stagnation point lies on a node.
  if (result.sst <= s[i]) {
    result.sst = s[i] + 1.0e-7;
  }
  if (result.sst >= s[i + 1]) {
    result.sst = s[i + 1] - 1.0e-7;
  }

  // Fortran: SST_GO/SST_GP.
  result.sstGo = (result.sst - s[i + 1]) / dgam;
  result.sstGp = (s[i] - result.sst) / dgam;

  return result;
}

// Fortran: SUBROUTINE QISET.
void qiset(std::span<const double> qinvu1, std::span<const double> qinvu2,
           double alfa, std::vector<double>& qinv,
           std::vector<double>& qinvA) {
  const std::size_t n = qinvu1.size();
  if (n != qinvu2.size()) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::qiset: QINVU lengths must be equal ");
  }

  const double cosa = std::cos(alfa);
  const double sina = std::sin(alfa);

  qinv.assign(n, 0.0);
  qinvA.assign(n, 0.0);

  // Fortran: Loop I=1..N+NW (0-based i0 = I-1).
  for (std::size_t i = 0; i < n; ++i) {
    qinv[i] = cosa * qinvu1[i] + sina * qinvu2[i];
    qinvA[i] = -sina * qinvu1[i] + cosa * qinvu2[i];
  }
}

}  // namespace xfoil::core::inviscid

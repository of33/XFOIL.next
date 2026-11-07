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

// File: include/xfoil/core/inverse/MixedInverseKernels.h
// Mixed-inverse kernel MIXED and Jacobian helper GAMLIN of the new 0-based
// cores, ported from fortran-src/xqdes.f.
//
// MixedInverseState combines the panel contour (x/y/s/xp/yp/nx/ny/apanel),
// the surface velocity (gam), the QDES target quantities (qspec, qf0..3,
// iq1/iq2, lcpXX), the global unknowns (psio, qdof0..3), and the
// runtime/TE quantities as separate value-semantic parts.
// MIXED solves the (N+5) Newton system with the validated kernels PSILIN
// (GEOLIN=true), GGCALC-free GAMLIN, and GAUSS. No Fortran EQUIVALENCE,
// no global states, no raw pointers, no 1-based containers.
//
// The CLCALC diagnostics output of Fortran (WRITE to stdout only) is
// left out in the core; it does not affect the convergence.

#pragma once

#include <span>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::inverse {

// Fortran IQX = 370; Mixed-inverse boundary N + 5 <= IQX, maximum N = 365.
inline constexpr int kMixedIqx = 370;
inline constexpr int kMixedMaxN = kMixedIqx - 5;  // 365

// Fortran COMMON/XFOIL.INC/: mixed-inverse state (one QSPEC column).
struct MixedInverseState {
  std::vector<double> x, y, s, xp, yp;     // panel contour (n)
  std::vector<double> nx, ny;              // contour normals (n)
  std::vector<double> apanel;              // panel angle (n, as Fortran fix)
  std::vector<double> gam;                 // GAM(1..N)
  std::vector<double> gamA, gamu1, gamu2;  // alpha bases (only QTAN, unused)
  std::vector<double> sig;                 // source distribution (inviscid: 0)
  std::vector<double> qspec;               // QSPEC(1..N) for KQSP
  std::vector<double> qf0, qf1, qf2, qf3;  // DOF shape functions (n)
  int n{0};
  int iq1{0};   // IQ1 (0-based, including segment boundary; Fortran IQ1 = iq1+1)
  int iq2{0};   // IQ2 (0-based, including segment boundary; Fortran IQ2 = iq2+1)
  bool lcpXX{false};
  bool sharp{false};
  double dste{0.0}, ante{0.0}, aste{0.0};
  double xte{0.0}, yte{0.0}, chord{0.0};
  double alfa{0.0}, minf{0.0}, qinf{1.0};
  double psio{0.0};
  double qdof0{0.0}, qdof1{0.0}, qdof2{0.0}, qdof3{0.0};
  // Convergence status of the last mixedInverse call. Fortran reports
  // non-convergence only as text ('Not quite converged. Can EXEC again');
  // the core sets this flag instead of text output (controlled, no error).
  bool converged{false};
};

// Fortran: SUBROUTINE GAMLIN(I,J,COEF)
// q is the (n+5)x(n+5) matrix row-major; row/col are 0-based; iq1/iq2
// are 0-based including segment boundaries (Fortran IQ1 = iq1+1).
void gamLin(std::span<double> q, std::size_t row, std::size_t col, double coef,
            int n, int iq1, int iq2, std::span<const double> qf0,
            std::span<const double> qf1, std::span<const double> qf2,
            std::span<const double> qf3);

// Fortran: SUBROUTINE MIXED(KQSP,NITERQ)
// Runs the Newton iterations on the current contour and updates
// x/y/s/gam/psio/qdof0..3 in the state. niterq is the maximum iteration count.
Status mixedInverse(MixedInverseState& state, int niterq);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/MixedInverseKernels.h

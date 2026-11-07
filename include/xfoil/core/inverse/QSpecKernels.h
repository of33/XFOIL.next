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

// File: include/xfoil/core/inverse/QSpecKernels.h
// QDES kernels of the new 0-based cores, ported from
// fortran-src/xqdes.f: SPLQSP, SMOOQ, QINCOM, GAMQSP, SYMQSP.
//
// The canonical Fortran routines work on COMMON states
// (QSPEC, QSPECP, SSPEC, NSP, W1..W3, LQSLOP, QDOF0..3, ALQSP/CLQSP/CMQSP).
// Here a single active QSPEC record is modeled as a value type
// (QSpecDistribution); the COMMON pass-through is dropped.
//
// Internal computations in double; canonical Fortran values are REAL(4).

#pragma once

#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::inverse {

// One active QSPEC record (one column QSPEC(:,KQSP)).
struct QSpecDistribution {
  std::vector<double> s;   // SSPEC(1..NSP)
  std::vector<double> q;   // QSPEC(1..NSP)
  std::vector<double> qp;  // QSPECP(1..NSP)
  double al{0.0};          // ALQSP(KQSP)
  double cl{0.0};          // CLQSP(KQSP)
  double cm{0.0};          // CMQSP(KQSP)
  double qdof0{0.0};
  double qdof1{0.0};
  double qdof2{0.0};
  double qdof3{0.0};
};

// Fortran: FUNCTION QINCOM(QC,QINF,TKLAM)
[[nodiscard]] double qIncompressible(double qc, double qinf, double tklam);

// Fortran: SUBROUTINE SPLQSP(KQSP)
Status splQSpec(QSpecDistribution& d);

// Fortran: SUBROUTINE SMOOQ(KQ1,KQ2,KQSP)
// kq1,kq2 are 0-based including segment boundaries (Fortran KQ1 = kq1+1).
Status smooQ(QSpecDistribution& d, int kq1, int kq2, bool lqslop);

// Fortran: SUBROUTINE GAMQSP(KQSP)
// qgamm = QGAMM(1..NSP); sets QSPEC, AL/Cl/CM, zeroes QDOFs, splint.
// iq1/iq2 are 0-based including segment boundaries (Fortran IQ1 = iq1+1).
Status gamQSpec(QSpecDistribution& d, const std::vector<double>& qgamm,
                double algam, double clgam, double cmgam, bool liqset,
                int& iq1, int& iq2);

// Fortran: SUBROUTINE SYMQSP(KQSP)
Status symQSpec(QSpecDistribution& d);

// Fortran: SUBROUTINE IQSGET — numerical sub-kernel.
// The cursor/plot/prompt part (GETCURSORXY, QSPLOT, search for the
// next plot point, KQTARG selection) remains outside. The numerical
// effect: the two endpoint indices are checked for difference and
// canonically sorted (MIN0/MAX0). An array wrap-around is not
// possible in Fortran (MIN0/MAX0 forces iq1 <= iq2).
struct QSpecTargetSegment {
  int iq1{0};            // 0-based, inclusive boundary (Fortran IQ1 = iq1+1)
  int iq2{0};            // 0-based, including boundary (Fortran IQ2 = iq2+1)
  bool liqset{false};    // Fortran LIQSET
};

// Fortran: SUBROUTINE IQSGET (numerical part).
// iqnew1, iqnew2 are 0-based end-point indices (0..nsp-1).
// For identical end points or out-of-range values a controlled
// failure is returned, without mutating seg.
[[nodiscard]] Status setTargetSegment(QSpecTargetSegment& seg, int iqnew1,
                                      int iqnew2, int nsp);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/QSpecKernels.h

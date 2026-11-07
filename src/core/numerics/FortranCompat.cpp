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

// File: src/core/numerics/FortranCompat.cpp
// Implementation of the scalar helper functions, ported directly from
// fortran-src/xutils.f. 0-based.

#include "xfoil/core/math/FortranCompat.h"

#include <cmath>
#include <stdexcept>

namespace xfoil::core::math {

namespace {

// Integer power by repeated squaring (square-and-multiply),
// bit-identical to the gfortran semantics of `**` with integer exponent
// (REAL(8) base, INTEGER exponent): libgfortran exponentiates in binary,
// not by n-fold sequential multiplication. Empirically verified
// for exponents 1..48 (144/144 bit-identical, gfortran
// -fdefault-real-8 -fdefault-double-8). n >= 0.
double intPower(double base, std::size_t n) {
  double result = 1.0;
  double b = base;
  std::size_t e = n;
  while (e > 0U) {
    if ((e & 1U) != 0U) {
      result *= b;
    }
    b *= b;
    e >>= 1U;
  }
  return result;
}

}  // namespace

Diagnostic setExp(std::span<double> s, double ds1, double smax) {
  const std::size_t nn = s.size();
  // Fortran: NEX = NN - 1. NEX <= 1 leads to the STOP 'SETEXP: Cannot fill
  // array. N too small.' -> controlled C++-Error.
  if (nn <= 2) {
    throw std::invalid_argument(
        "xfoil::core::math::setExp: NN too small (NEX <= 1)");
  }
  const std::size_t nex = nn - 1;

  // Fortran: RNEX = FLOAT(NEX)
  const double rnex = static_cast<double>(nex);
  const double rni = 1.0 / rnex;

  const double sigma = smax / ds1;

  // Fortran: AAA = RNEX*(RNEX-1.0)*(RNEX-2.0) / 6.0
  const double aaa = rnex * (rnex - 1.0) * (rnex - 2.0) / 6.0;
  // Fortran: BBB = RNEX*(RNEX-1.0) / 2.0
  const double bbb = rnex * (rnex - 1.0) / 2.0;
  // Fortran: CCC = RNEX - SIGMA
  const double ccc = rnex - sigma;

  // Fortran: DISC = BBB**2 - 4.0*AAA*CCC ; DISC = MAX(0.0, DISC)
  double disc = bbb * bbb - 4.0 * aaa * ccc;
  disc = std::max(0.0, disc);

  double ratio;
  if (nex == 2) {
    // Fortran: RATIO = -CCC/BBB + 1.0
    ratio = -ccc / bbb + 1.0;
  } else {
    // Fortran: RATIO = (-BBB + SQRT(DISC))/(2.0*AAA) + 1.0
    ratio = (-bbb + std::sqrt(disc)) / (2.0 * aaa) + 1.0;
  }

  bool converged = true;
  if (ratio != 1.0) {
    // Newton iteration for the actual geometric ratio.
    bool done = false;
    for (int iter = 0; iter < 100; ++iter) {
      const double signan = (intPower(ratio, nex) - 1.0) / (ratio - 1.0);
      const double res = std::pow(signan, rni) - std::pow(sigma, rni);
      const double dresdr =
          rni * std::pow(signan, rni) *
          (static_cast<double>(nex) * intPower(ratio, nex - 1) - signan) /
          (intPower(ratio, nex) - 1.0);

      const double dratio = -res / dresdr;
      ratio = ratio + dratio;
      if (std::abs(dratio) < 1.0E-5) {
        done = true;
        break;
      }
    }
    // Fortran: 'SETEXP: Convergence failed. Continuing anyway ...' is
    // output if convergence is not reached after 100 iterations.
    // The numerical kernel does not write to stdout; the diagnostic state
    // is returned in structured form.
    converged = done;
  }

  // Fortran: S(1)=0.0; DS=DS1; DO N=2,NN: S(N)=S(N-1)+DS; DS=DS*RATIO
  s[0] = 0.0;
  double ds = ds1;
  for (std::size_t n = 1; n < nn; ++n) {
    s[n] = s[n - 1] + ds;
    ds = ds * ratio;
  }

  return converged ? Diagnostic::completed : Diagnostic::limitReached;
}

double atanC(double y, double x, double thold) {
  constexpr double kPi = 3.1415926535897932384;
  constexpr double kTwoPi = 6.2831853071795864769;

  // Fortran: THNEW = ATAN2(Y,X)
  const double thnew = std::atan2(y, x);
  // Fortran: DTHET = THNEW - THOLD
  const double dthet = thnew - thold;

  // Fortran: DTCORR = DTHET - TPI*INT((DTHET + SIGN(PI,DTHET))/TPI)
 // INT truncates toward zero (trunc). SIGN(PI,DTHET) = PI for
  // DTHET >= 0, otherwise -PI.
  const double sign = (dthet >= 0.0) ? kPi : -kPi;
  const double dtcorr = dthet - kTwoPi * std::trunc((dthet + sign) / kTwoPi);

  // Fortran: ATANC = THOLD + DTCORR
  return thold + dtcorr;
}

}  // namespace xfoil::core::math

// End: src/core/numerics/FortranCompat.cpp

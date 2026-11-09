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

// File: src/core/numerics/LinearAlgebra.cpp
// Real and complex linear system solvers, directly
// from fortran-src/xsolve.f ported. 0-based.
//
// Translation column-major (Fortran) -> row-major (C++):
// Fortran Z(I,J) <-> C++ z[(I-1)*nn + (J-1)]
// Fortran R(I,L) <-> C++ r[(I-1)*nrhs + (L-1)]
// Fortran A(I,J) <-> C++ a[(I-1)*n + (J-1)]
// Pivot indices: Fortran 1..N <-> C++ 0..N-1 (conversion at the
// LUDCMP->INDX or BAKSUB read boundary; the stored value is 0-based).

#include "xfoil/core/math/LinearAlgebra.h"

#include "xfoil/core/Limits.h"

#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>

namespace xfoil::core::math {

namespace {

// Via run check: returns rows*cols in out, false for Via run.
bool productFits(std::size_t rows, std::size_t cols, std::size_t& out) {
  if (rows != 0 && cols > std::numeric_limits<std::size_t>::max() / rows) {
    return false;
  }
  out = rows * cols;
  return true;
}

}  // namespace

void gauss(std::span<double> z, std::span<double> r, std::size_t nn,
           std::size_t nrhs) {
  if (nn == 0) {
    throw std::invalid_argument(
        "xfoil::core::math::Gauss: NN == 0 (invalid quantity)");
  }
  std::size_t zn, rn;
  if (!productFits(nn, nn, zn) || !productFits(nn, nrhs, rn) ||
      zn > z.size() || rn > r.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::Gauss: matrix/RHS quantity overflow or "
        "exceeds span length");
  }

  // Fortran: DO NP=1, NN-1 -> np = 0..nn-2
  for (std::size_t np = 0; np + 1 < nn; ++np) {
    const std::size_t np1 = np + 1;

    // Fortran: max pivot index NX find (strict larger)
    std::size_t nx = np;
    for (std::size_t n = np1; n < nn; ++n) {
      if (std::abs(z[n * nn + np]) > std::abs(z[nx * nn + np])) {
        nx = n;
      }
    }

    const double pivot = 1.0 / z[nx * nn + np];
    z[nx * nn + np] = z[np * nn + np];

    // Fortran: swap rows and normalize the pivot row
    for (std::size_t l = np1; l < nn; ++l) {
      const double temp = z[nx * nn + l] * pivot;
      z[nx * nn + l] = z[np * nn + l];
      z[np * nn + l] = temp;
    }
    for (std::size_t l = 0; l < nrhs; ++l) {
      const double temp = r[nx * nrhs + l] * pivot;
      r[nx * nrhs + l] = r[np * nrhs + l];
      r[np * nrhs + l] = temp;
    }

    // Fortran: Forward elimination
    for (std::size_t k = np1; k < nn; ++k) {
      const double ztmp = z[k * nn + np];
      for (std::size_t l = np1; l < nn; ++l) {
        z[k * nn + l] = z[k * nn + l] - ztmp * z[np * nn + l];
      }
      for (std::size_t l = 0; l < nrhs; ++l) {
        r[k * nrhs + l] = r[k * nrhs + l] - ztmp * r[np * nrhs + l];
      }
    }
  }

  // Fortran: resolve the last line
  for (std::size_t l = 0; l < nrhs; ++l) {
    r[(nn - 1) * nrhs + l] = r[(nn - 1) * nrhs + l] /
                             z[(nn - 1) * nn + (nn - 1)];
  }

  // Fortran: DO NP=NN-1, 1, -1 -> np = nn-2..0
  std::size_t np = nn - 1;
  while (np > 0) {
    --np;
    const std::size_t np1 = np + 1;
    for (std::size_t l = 0; l < nrhs; ++l) {
      for (std::size_t k = np1; k < nn; ++k) {
        r[np * nrhs + l] = r[np * nrhs + l] - z[np * nn + k] * r[k * nrhs + l];
      }
    }
  }
}

void cgauss(std::span<std::complex<double>> z,
            std::span<std::complex<double>> r, std::size_t nn,
            std::size_t nrhs) {
  if (nn == 0) {
    throw std::invalid_argument(
        "xfoil::core::math::cgauss: NN == 0 (invalid quantity)");
  }
  std::size_t zn, rn;
  if (!productFits(nn, nn, zn) || !productFits(nn, nrhs, rn) ||
      zn > z.size() || rn > r.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::cgauss: matrix/RHS quantity overflow or "
        "exceeds span length");
  }

  for (std::size_t np = 0; np + 1 < nn; ++np) {
    const std::size_t np1 = np + 1;

    std::size_t nx = np;
    for (std::size_t n = np1; n < nn; ++n) {
      if (std::abs(z[n * nn + np]) > std::abs(z[nx * nn + np])) {
        nx = n;
      }
    }

    const std::complex<double> pivot =
        std::complex<double>(1.0, 0.0) / z[nx * nn + np];
    z[nx * nn + np] = z[np * nn + np];

    for (std::size_t l = np1; l < nn; ++l) {
      const std::complex<double> temp = z[nx * nn + l] * pivot;
      z[nx * nn + l] = z[np * nn + l];
      z[np * nn + l] = temp;
    }
    for (std::size_t l = 0; l < nrhs; ++l) {
      const std::complex<double> temp = r[nx * nrhs + l] * pivot;
      r[nx * nrhs + l] = r[np * nrhs + l];
      r[np * nrhs + l] = temp;
    }

    for (std::size_t k = np1; k < nn; ++k) {
      const std::complex<double> ztmp = z[k * nn + np];
      for (std::size_t l = np1; l < nn; ++l) {
        z[k * nn + l] = z[k * nn + l] - ztmp * z[np * nn + l];
      }
      for (std::size_t l = 0; l < nrhs; ++l) {
        r[k * nrhs + l] = r[k * nrhs + l] - ztmp * r[np * nrhs + l];
      }
    }
  }

  for (std::size_t l = 0; l < nrhs; ++l) {
    r[(nn - 1) * nrhs + l] =
        r[(nn - 1) * nrhs + l] / z[(nn - 1) * nn + (nn - 1)];
  }

  std::size_t np = nn - 1;
  while (np > 0) {
    --np;
    const std::size_t np1 = np + 1;
    for (std::size_t l = 0; l < nrhs; ++l) {
      for (std::size_t k = np1; k < nn; ++k) {
        r[np * nrhs + l] = r[np * nrhs + l] - z[np * nn + k] * r[k * nrhs + l];
      }
    }
  }
}

void ludcmp(std::span<double> a, std::size_t n, std::span<int> indx) {
  constexpr std::size_t kLudcmpMax =
      xfoil::core::linearSolverWorkspaceCapacity;  // Fortran NVX
  if (n == 0) {
    throw std::invalid_argument(
        "xfoil::core::math::ludcmp: N == 0 (invalid quantity)");
  }
  if (n > kLudcmpMax) {
    throw std::invalid_argument(
        "xfoil::core::math::ludcmp: N > NVX (515)");
  }
  std::size_t an;
  if (!productFits(n, n, an) || an > a.size() || n > indx.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::ludcmp: matrix/INDX quantity overflow or "
        "exceeds span length");
  }

  std::array<double, kLudcmpMax> vv{};

  // Fortran: implicit scaling
  for (std::size_t i = 0; i < n; ++i) {
    double aamax = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
      aamax = std::max(std::abs(a[i * n + j]), aamax);
    }
    vv[i] = 1.0 / aamax;
  }

  for (std::size_t j = 0; j < n; ++j) {
    // Fortran: DO I=1, J-1 -> i = 0..j-1
    for (std::size_t i = 0; i < j; ++i) {
      double sum = a[i * n + j];
      for (std::size_t k = 0; k < i; ++k) {  // K=1..I-1
        sum = sum - a[i * n + k] * a[k * n + j];
      }
      a[i * n + j] = sum;
    }

    // Fortran: DO I=J, N -> i = j..n-1, with pivot selection
    double aamax = 0.0;
    std::size_t imax = j;
    for (std::size_t i = j; i < n; ++i) {
      double sum = a[i * n + j];
      for (std::size_t k = 0; k < j; ++k) {  // K=1..J-1
        sum = sum - a[i * n + k] * a[k * n + j];
      }
      a[i * n + j] = sum;
      const double dum = vv[i] * std::abs(sum);
      if (dum >= aamax) {
        imax = i;
        aamax = dum;
      }
    }

    // Fortran: row interchange
    if (j != imax) {
      for (std::size_t k = 0; k < n; ++k) {
        const double dum = a[imax * n + k];
        a[imax * n + k] = a[j * n + k];
        a[j * n + k] = dum;
      }
      vv[imax] = vv[j];
    }

    // Fortran: INDX(J) = IMAX (0-based stored)
    indx[j] = static_cast<int>(imax);

    // Fortran: IF(J.NE.N) DO I=J+1,N: A(I,J)=A(I,J)/A(J,J)
    if (j + 1 != n) {
      const double dum = 1.0 / a[j * n + j];
      for (std::size_t i = j + 1; i < n; ++i) {
        a[i * n + j] = a[i * n + j] * dum;
      }
    }
  }
}

void baksub(std::span<const double> a, std::size_t n,
            std::span<const int> indx, std::span<double> b) {
  std::size_t an;
  if (!productFits(n, n, an) || an > a.size() || n > indx.size() ||
      n > b.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::baksub: matrix/INDX/B quantity overflow or "
        "exceeds span length");
  }

  // Check pivot indices before mutating on [0, n)
  for (std::size_t i = 0; i < n; ++i) {
    const int p = indx[i];
    if (p < 0 || static_cast<std::size_t>(p) >= n) {
      throw std::invalid_argument(
          "xfoil::core::math::baksub: Pivotindex outside [0, N)");
    }
  }

  // Fortran: II = 0 (Flag: still no Not zero seen)
  bool iiSet = false;
  std::size_t ii = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t ll = static_cast<std::size_t>(indx[i]);
    double sum = b[ll];
    b[ll] = b[i];
    if (iiSet) {
      // Fortran: DO J=II, I-1 -> j = ii..i-1
      for (std::size_t j = ii; j < i; ++j) {
        sum = sum - a[i * n + j] * b[j];
      }
    } else if (sum != 0.0) {
      ii = i;
      iiSet = true;
    }
    b[i] = sum;
  }

  // Fortran: DO I=N, 1, -1 -> i = n-1..0
  for (std::size_t i = n; i-- > 0;) {
    double sum = b[i];
    if (i + 1 < n) {
      for (std::size_t j = i + 1; j < n; ++j) {  // J=I+1..N
        sum = sum - a[i * n + j] * b[j];
      }
    }
    b[i] = sum / a[i * n + i];
  }
}

}  // namespace xfoil::core::math

// End: src/core/numerics/LinearAlgebra.cpp

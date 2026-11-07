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

// File: src/core/numerics/Sort.cpp
// Implementation of the sort and duplicate-removal routines, ported
// directly from fortran-src/sort.f. 0-based.
//
// 0-based index mapping:
// Fortran 1..N <-> C++ 0..N-1
// Fortran A(I) <-> C++ a[i] (i = I-1)
// The in-place length KK is passed as std::size_t& and denotes
// the active element count (counted 0-based, as Fortran KK).

#include "xfoil/core/math/Sort.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace xfoil::core::math {

void hSort(std::span<const double> a, std::span<int> indx) {
  const std::size_t n = indx.size();
  if (a.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::hSort: INDX length is larger than A length");
  }
  if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(
        "xfoil::core::math::hSort: N exceeds INT_MAX");
  }
  for (std::size_t i = 0; i < n; ++i) {
    indx[i] = static_cast<int>(i);
  }
  if (n <= 1) {
    return;
  }

  // Fortran: L = N/2 + 1 -> l = n/2 (0-based); IR = N -> ir = n-1.
  std::size_t l = n / 2;
  std::size_t ir = n - 1;

  for (;;) {
    std::size_t indxt;
    double q;
    if (l > 0) {
      // Fortran THEN branch: L = L-1; INDXT = INDX(L); Q = A(INDXT)
      l = l - 1;
      indxt = static_cast<std::size_t>(indx[l]);
      q = a[indxt];
    } else {
      // Fortran ELSE branch
      indxt = static_cast<std::size_t>(indx[ir]);
      q = a[indxt];
      indx[ir] = indx[0];
      ir = ir - 1;
      if (ir == 0) {
        indx[0] = static_cast<int>(indxt);
        return;
      }
    }

    // Fortran: I = L; J = L+L
    // Positions are 1-based: J_f = 2*L_f -> j_c = 2*l_c + 1.
    std::size_t i = l;
    std::size_t j = 2 * l + 1;

    // Fortran label 20: DO WHILE(J <= IR)
    while (j <= ir) {
      if (j < ir) {
        if (a[static_cast<std::size_t>(indx[j])] <
            a[static_cast<std::size_t>(indx[j + 1])]) {
          j = j + 1;
        }
      }
      if (q < a[static_cast<std::size_t>(indx[j])]) {
        indx[i] = indx[j];
        i = j;
        // Fortran: J = J+J -> j_c = 2*j_c + 1
        j = 2 * j + 1;
      } else {
        // Fortran: J = IR+1 -> j_c = ir_c + 1
        j = ir + 1;
      }
    }

    // Fortran: INDX(I) = INDXT ; GO TO 10
    indx[i] = static_cast<int>(indxt);
  }
}

void aSort(std::span<double> a, std::span<const int> indx,
           std::span<double> atmp) {
  const std::size_t n = a.size();
  if (indx.size() < n || atmp.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::aSort: INDX/ATMP length is smaller than A length");
  }
  // Safety: all indices must lie in the range [0, n) before the
  // mutation; otherwise the access atmp[isort] is outside the range.
  for (std::size_t i = 0; i < n; ++i) {
    const int isort = indx[i];
    if (isort < 0 || static_cast<std::size_t>(isort) >= n) {
      throw std::invalid_argument(
          "xfoil::core::math::aSort: INDX outside of the valid Range");
    }
  }
  for (std::size_t i = 0; i < n; ++i) {
    atmp[i] = a[i];
  }
  for (std::size_t i = 0; i < n; ++i) {
    const int isort = indx[i];
    a[i] = atmp[static_cast<std::size_t>(isort)];
  }
}

void remDupIndices(std::span<const double> a, double tol,
                   std::span<int> indx, std::size_t& nnew) {
  const std::size_t n = a.size();
  if (indx.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::remDupIndices: INDX length is smaller than A length");
  }
  if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(
        "xfoil::core::math::remDupIndices: N exceeds INT_MAX");
  }
  if (n == 0) {
    // Empty input: no indices to produce and no mutation of indx.
    nnew = 0;
    return;
  }
  // Fortran: K = 1; INDX(K) = 1 -> k = 0; indx[0] = 0
  std::size_t k = 0;
  indx[k] = 0;
  for (std::size_t i = 1; i < n; ++i) {
    if (std::abs(a[i] - a[i - 1]) > tol) {
      k = k + 1;
      indx[k] = static_cast<int>(i);
    }
  }
  // Fortran: NNEW = K (K is the 1-based counter of the retained
  // indices). In the 0-based representation the count is k+1.
  nnew = k + 1;
}

Diagnostic sortDup(std::span<double> s, std::span<double> w) {
  const std::size_t kk = s.size();
  if (w.size() < kk) {
    throw std::invalid_argument(
        "xfoil::core::math::sortDup: W length is smaller than S length");
  }
  for (int ipass = 0; ipass < 1234; ++ipass) {
    bool done = true;
    for (std::size_t n = 0; n + 1 < kk; ++n) {
      if (s[n + 1] >= s[n]) {
        continue;
      }
      double temp = s[n + 1];
      s[n + 1] = s[n];
      s[n] = temp;
      temp = w[n + 1];
      w[n + 1] = w[n];
      w[n] = temp;
      done = false;
    }
    if (done) {
      return Diagnostic::completed;
    }
  }
  // Fortran: WRITE(*,*) 'Sort failed' (canonical Pass limit without Completion).
  // The numerical kernel does not write to stdout; the diagnostic state is
  // returned in structured form. The array content stays unchanged.
  return Diagnostic::limitReached;
}

void fixDup(std::size_t& kk, std::span<double> s, std::span<double> w) {
  if (w.size() < s.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::fixDup: W length is smaller than S length");
  }
  if (kk > s.size() || kk > w.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::fixDup: KK is larger than S/W length");
  }
  if (kk < 2) {
    // Safety: Fortran implicitly requires KK >= 2 (access to S(2),S(1)
    // and S(KK),S(KK-1)). For KK < 2 there are no duplicates to remove.
    return;
  }
  for (;;) {
    bool done = true;

    // Fortran Label 5: remove leading duplicates
    if (s[1] == s[0]) {
      for (std::size_t n = 0; n + 1 < kk; ++n) {
        s[n] = s[n + 1];
        w[n] = w[n + 1];
      }
      kk = kk - 1;
      done = false;
    }

    // Fortran: remove trailing duplicates
    if (kk >= 2 && s[kk - 1] == s[kk - 2]) {
      s[kk - 2] = s[kk - 1];
      w[kk - 2] = w[kk - 1];
      kk = kk - 1;
      done = false;
    }

    // Fortran label 10: remove more than two in a row that are identical
    // on a following match, restarting the check after each removal.
    for (;;) {
      bool removed = false;
      for (std::size_t n = 0; n + 2 < kk; ++n) {
        if (s[n] == s[n + 1] && s[n] == s[n + 2]) {
          for (std::size_t i = n; i + 1 < kk; ++i) {
            s[i] = s[i + 1];
            w[i] = w[i + 1];
          }
          kk = kk - 1;
          removed = true;
          break;
        }
      }
      if (!removed) {
        break;
      }
    }

    if (done) {
      return;
    }
  }
}

Diagnostic sortAndRemovePairs(std::size_t& kk, std::span<double> s,
                              std::span<double> w) {
  if (w.size() < s.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::sortAndRemovePairs: W length is smaller than "
        "S length");
  }
  const std::size_t cap = s.size();
  if (kk > cap || kk > w.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::sortAndRemovePairs: KK is larger than S/W length");
  }

  // Fortran: bubble sort (strict < swap); detect whether the canonical
  // pass limit (1234) is reached without completion (Fortran 'Sort failed').
  bool sortFailed = false;
  for (int ipass = 0; ipass < 1234; ++ipass) {
    bool done = true;
    for (std::size_t n = 0; n + 1 < kk; ++n) {
      if (s[n + 1] >= s[n]) {
        continue;
      }
      double temp = s[n + 1];
      s[n + 1] = s[n];
      s[n] = temp;
      temp = w[n + 1];
      w[n + 1] = w[n];
      w[n] = temp;
      done = false;
    }
    if (done) {
      break;
    }
    if (ipass == 1233) {
      sortFailed = true;
    }
  }

  // Fortran Label 11: remove duplicate pairs (KK -= 2 per pair).
  const std::size_t kks = kk;
  for (std::size_t k = 0; k < kks; ++k) {
    if (k + 1 >= kk) {
      return sortFailed ? Diagnostic::limitReached
                        : Diagnostic::completed;
    }
    if (s[k] != s[k + 1]) {
      continue;
    }
    // Fortran: KK = KK-2 (before the Move loop)
    kk = kk - 2;
    for (std::size_t kt = k; kt < kk; ++kt) {
      s[kt] = s[kt + 2];
      w[kt] = w[kt + 2];
    }
  }
  return sortFailed ? Diagnostic::limitReached : Diagnostic::completed;
}

void sortWithTolerance(double tol, std::size_t& kk, std::span<double> s,
                       std::span<double> w) {
  if (w.size() < s.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::sortWithTolerance: W length is smaller than "
        "S length");
  }
  const std::size_t cap = s.size();
  if (kk > cap || kk > w.size()) {
    throw std::invalid_argument(
        "xfoil::core::math::sortWithTolerance: KK is larger than S/W length");
  }

  // Fortran: bubble sort (strict < swap)
  for (int ipass = 0; ipass < 1234; ++ipass) {
    bool done = true;
    for (std::size_t n = 0; n + 1 < kk; ++n) {
      if (s[n + 1] < s[n]) {
        double temp = s[n + 1];
        s[n + 1] = s[n];
        s[n] = temp;
        temp = w[n + 1];
        w[n + 1] = w[n];
        w[n] = temp;
        done = false;
      }
    }
    if (done) {
      break;
    }
  }
  // Sort-failure diagnostics (no I/O in the core; see sortDup).

  // Fortran Label 10: remove near-duplicates until none remain
  // (bug fix 4/24/01: outer repetition until all are removed).
  for (;;) {
    const std::size_t kks = kk;
    bool done = true;
    for (std::size_t k = 0; k < kks; ++k) {
      if (k + 1 >= kk) {
        continue;
      }
      const double dx = s[k] - s[k + 1];
      const double dy = w[k] - w[k + 1];
      const double dsq = dx * dx + dy * dy;
      if (dsq >= tol * tol) {
        continue;
      }
      // Fortran: KK = KK-1 (before the Move loop)
      kk = kk - 1;
      for (std::size_t kt = k + 1; kt < kk; ++kt) {
        s[kt] = s[kt + 1];
        w[kt] = w[kt + 1];
      }
      done = false;
    }
    if (done) {
      return;
    }
  }
}

}  // namespace xfoil::core::math

// End: src/core/numerics/Sort.cpp

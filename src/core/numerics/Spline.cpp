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

// File: src/core/numerics/Spline.cpp
// Implementation of the spline and tridiagonal routines, ported
// directly from fortran-src/spline.f. 0-based.
//
// 0-based index mapping:
// Fortran 1..N <-> C++ 0..N-1
// Fortran 2..N-1 <-> C++ 1..N-2
// Fortran A(I) <-> C++ a[i] (i = I-1)
// The local Fortran work arrays (NMAX=2500 for SPLINE/SPLIND,
// KMAX=32 for SPLNXY) use the same active limits. The large spline
// workspace is heap-backed; the small SPLNXY scratch stays fixed.

#include "xfoil/core/math/Spline.h"

#include "xfoil/core/Limits.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace xfoil::core::math {

namespace {

// Fortran: PARAMETER (NMAX=2500) in SPLINE/SPLIND.
constexpr std::size_t kSplineMax = xfoil::core::splineWorkspaceCapacity;
// Fortran: PARAMETER (KMAX=32) in SPLNXY.
constexpr std::size_t kSplineNxyMax = 32;

// Shared binary search (Fortran labels 10/11 in SEVAL/DEVAL/D2VAL/CURV/
// CURVS): returns the interval index i with S(i-1) <= SS < S(i) (or SS at
// the right edge). Return value: i (0-based), so that the interval [i-1, i]
// is active.
std::size_t locateInterval(double ss, std::span<const double> s) {
  const std::size_t n = s.size();
  std::size_t ilow = 0;
  std::size_t i = n - 1;
  while (i - ilow > 1) {
    const std::size_t imid = (i + ilow) / 2;
    if (ss < s[imid]) {
      i = imid;
    } else {
      ilow = imid;
    }
  }
  return i;
}

// Shared checks for the 1-D evaluators (SEVAL/DEVAL/D2VAL):
// N >= 2 and all spans at least as long as s. No mutation.
void validateEval1D(std::span<const double> s, std::span<const double> x,
                    std::span<const double> xs) {
  if (s.size() < 2) {
    throw std::invalid_argument(
        "xfoil::core::math: Evaluation requires N >= 2");
  }
  if (x.size() < s.size() || xs.size() < s.size()) {
    throw std::invalid_argument(
        "xfoil::core::math: X/XS length is smaller than S length");
  }
}

// Shared checks for the 2-D evaluators (CURV/CURVS):
// N >= 2 and all spans (x, xs, y, ys) at least as long as s.
void validateEval2D(std::span<const double> s, std::span<const double> x,
                    std::span<const double> xs, std::span<const double> y,
                    std::span<const double> ys) {
  if (s.size() < 2) {
    throw std::invalid_argument(
        "xfoil::core::math: Evaluation requires N >= 2");
  }
  if (x.size() < s.size() || xs.size() < s.size() ||
      y.size() < s.size() || ys.size() < s.size()) {
    throw std::invalid_argument(
        "xfoil::core::math: X/XS/Y/YS length is smaller than S length");
  }
}

}  // namespace

void spline(std::span<const double> x, std::span<const double> s,
            std::span<double> xs) {
  const std::size_t n = x.size();
  if (s.size() < n || xs.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::spline: S/XS length is smaller than X length");
  }
  if (n < 2) {
    throw std::invalid_argument(
        "xfoil::core::math::spline: N < 2 (minimum point count)");
  }
  if (n > kSplineMax) {
    throw std::invalid_argument(
        "xfoil::core::math::spline: N > NMAX (2500)");
  }

  std::vector<double> a(kSplineMax);
  std::vector<double> b(kSplineMax);
  std::vector<double> c(kSplineMax);

  // Fortran: DO I=2, N-1 -> i = 1..n-2
  for (std::size_t i = 1; i + 1 < n; ++i) {
    const double dsm = s[i] - s[i - 1];
    const double dsp = s[i + 1] - s[i];
    b[i] = dsp;
    a[i] = 2.0 * (dsm + dsp);
    c[i] = dsm;
    xs[i] = 3.0 * (((x[i + 1] - x[i]) * dsm / dsp) +
                   ((x[i] - x[i - 1]) * dsp / dsm));
  }

  // Fortran: zero second-derivative boundary conditions
  a[0] = 2.0;
  c[0] = 1.0;
  xs[0] = 3.0 * (x[1] - x[0]) / (s[1] - s[0]);
  b[n - 1] = 1.0;
  a[n - 1] = 2.0;
  xs[n - 1] = 3.0 * (x[n - 1] - x[n - 2]) / (s[n - 1] - s[n - 2]);

  triSol(std::span<double>(a).first(n), std::span<const double>(b).first(n),
         std::span<double>(c).first(n), xs);
}

void splind(std::span<const double> x, std::span<const double> s,
            std::span<double> xs, double xs1, double xs2) {
  const std::size_t n = x.size();
  if (s.size() < n || xs.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::splind: S/XS length is smaller than X length");
  }
  if (n < 2) {
    throw std::invalid_argument(
        "xfoil::core::math::splind: N < 2 (minimum point count)");
  }
  if (n > kSplineMax) {
    throw std::invalid_argument(
        "xfoil::core::math::splind: N > NMAX (2500)");
  }

  std::vector<double> a(kSplineMax);
  std::vector<double> b(kSplineMax);
  std::vector<double> c(kSplineMax);

  for (std::size_t i = 1; i + 1 < n; ++i) {
    const double dsm = s[i] - s[i - 1];
    const double dsp = s[i + 1] - s[i];
    b[i] = dsp;
    a[i] = 2.0 * (dsm + dsp);
    c[i] = dsm;
    xs[i] = 3.0 * (((x[i + 1] - x[i]) * dsm / dsp) +
                   ((x[i] - x[i - 1]) * dsp / dsm));
  }

  // Fortran: end condition 1
  if (xs1 == 999.0) {
    a[0] = 2.0;
    c[0] = 1.0;
    xs[0] = 3.0 * (x[1] - x[0]) / (s[1] - s[0]);
  } else if (xs1 == -999.0) {
    a[0] = 1.0;
    c[0] = 1.0;
    xs[0] = 2.0 * (x[1] - x[0]) / (s[1] - s[0]);
  } else {
    a[0] = 1.0;
    c[0] = 0.0;
    xs[0] = xs1;
  }

  // Fortran: end condition 2
  if (xs2 == 999.0) {
    b[n - 1] = 1.0;
    a[n - 1] = 2.0;
    xs[n - 1] = 3.0 * (x[n - 1] - x[n - 2]) / (s[n - 1] - s[n - 2]);
  } else if (xs2 == -999.0) {
    b[n - 1] = 1.0;
    a[n - 1] = 1.0;
    xs[n - 1] = 2.0 * (x[n - 1] - x[n - 2]) / (s[n - 1] - s[n - 2]);
  } else {
    a[n - 1] = 1.0;
    b[n - 1] = 0.0;
    xs[n - 1] = xs2;
  }

  // Fortran: Special case N=2 with both zero third-derivative boundary conditions
  if (n == 2 && xs1 == -999.0 && xs2 == -999.0) {
    b[n - 1] = 1.0;
    a[n - 1] = 2.0;
    xs[n - 1] = 3.0 * (x[n - 1] - x[n - 2]) / (s[n - 1] - s[n - 2]);
  }

  triSol(std::span<double>(a).first(n), std::span<const double>(b).first(n),
         std::span<double>(c).first(n), xs);
}

void splina(std::span<const double> x, std::span<const double> s,
            std::span<double> xs) {
  const std::size_t n = x.size();
  if (s.size() < n || xs.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::splina: S/XS length is smaller than X length");
  }
  if (n < 2) {
    throw std::invalid_argument(
        "xfoil::core::math::splina: N < 2 (minimum point count)");
  }

  bool lend = true;
  // Local XS1/XS2 (Fortran implicit); in valid paths XS1 is read only
  // after an assignment; initialization avoids undefined behavior for DS==0
  // in the very first segment.
  double xs1 = 0.0;
  double xs2 = 0.0;

  // Fortran: DO I=1, N-1 -> i = 0..n-2
  for (std::size_t i = 0; i + 1 < n; ++i) {
    const double ds = s[i + 1] - s[i];
    if (ds == 0.0) {
      xs[i] = xs1;
      lend = true;
    } else {
      const double dx = x[i + 1] - x[i];
      xs2 = dx / ds;
      if (lend) {
        xs[i] = xs2;
        lend = false;
      } else {
        xs[i] = 0.5 * (xs1 + xs2);
      }
    }
    xs1 = xs2;
  }

  // Fortran: XS(N) = XS1
  xs[n - 1] = xs1;
}

void triSol(std::span<double> a, std::span<const double> b,
            std::span<double> c, std::span<double> d) {
  const std::size_t kk = a.size();
  if (kk == 0) {
    throw std::invalid_argument(
        "xfoil::core::math::triSol: KK == 0 (invalid quantity)");
  }
  if (b.size() < kk || c.size() < kk || d.size() < kk) {
    throw std::invalid_argument(
        "xfoil::core::math::triSol: B/C/D length is smaller than A length");
  }
  // Fortran: DO K=2, KK -> k = 1..kk-1
  for (std::size_t k = 1; k < kk; ++k) {
    const std::size_t km = k - 1;
    c[km] = c[km] / a[km];
    d[km] = d[km] / a[km];
    a[k] = a[k] - b[k] * c[km];
    d[k] = d[k] - b[k] * d[km];
  }
  d[kk - 1] = d[kk - 1] / a[kk - 1];
  // Fortran: DO K=KK-1, 1, -1 -> k = kk-2..0
  for (std::size_t k = kk - 1; k > 0; --k) {
    d[k - 1] = d[k - 1] - c[k - 1] * d[k];
  }
}

double seval(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> s) {
  validateEval1D(s, x, xs);
  const std::size_t i = locateInterval(ss, s);
  const double ds = s[i] - s[i - 1];
  const double t = (ss - s[i - 1]) / ds;
  const double cx1 = ds * xs[i - 1] - x[i] + x[i - 1];
  const double cx2 = ds * xs[i] - x[i] + x[i - 1];
  return t * x[i] + (1.0 - t) * x[i - 1] +
         (t - t * t) * ((1.0 - t) * cx1 - t * cx2);
}

double deval(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> s) {
  validateEval1D(s, x, xs);
  const std::size_t i = locateInterval(ss, s);
  const double ds = s[i] - s[i - 1];
  const double t = (ss - s[i - 1]) / ds;
  const double cx1 = ds * xs[i - 1] - x[i] + x[i - 1];
  const double cx2 = ds * xs[i] - x[i] + x[i - 1];
  double result = x[i] - x[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cx1 +
                  t * (3.0 * t - 2.0) * cx2;
  return result / ds;
}

double d2val(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> s) {
  validateEval1D(s, x, xs);
  const std::size_t i = locateInterval(ss, s);
  const double ds = s[i] - s[i - 1];
  const double t = (ss - s[i - 1]) / ds;
  const double cx1 = ds * xs[i - 1] - x[i] + x[i - 1];
  const double cx2 = ds * xs[i] - x[i] + x[i - 1];
  const double result = (6.0 * t - 4.0) * cx1 + (6.0 * t - 2.0) * cx2;
  return result / (ds * ds);
}

double curv(double ss, std::span<const double> x,
            std::span<const double> xs, std::span<const double> y,
            std::span<const double> ys, std::span<const double> s) {
  validateEval2D(s, x, xs, y, ys);
  const std::size_t i = locateInterval(ss, s);
  const double ds = s[i] - s[i - 1];
  const double t = (ss - s[i - 1]) / ds;

  const double cx1 = ds * xs[i - 1] - x[i] + x[i - 1];
  const double cx2 = ds * xs[i] - x[i] + x[i - 1];
  const double xd = x[i] - x[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cx1 +
                    t * (3.0 * t - 2.0) * cx2;
  const double xdd = (6.0 * t - 4.0) * cx1 + (6.0 * t - 2.0) * cx2;

  const double cy1 = ds * ys[i - 1] - y[i] + y[i - 1];
  const double cy2 = ds * ys[i] - y[i] + y[i - 1];
  const double yd = y[i] - y[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cy1 +
                    t * (3.0 * t - 2.0) * cy2;
  const double ydd = (6.0 * t - 4.0) * cy1 + (6.0 * t - 2.0) * cy2;

  double sd = std::sqrt(xd * xd + yd * yd);
  sd = std::max(sd, 0.001 * ds);

  return (xd * ydd - yd * xdd) / (sd * sd * sd);
}

double curvs(double ss, std::span<const double> x,
             std::span<const double> xs, std::span<const double> y,
             std::span<const double> ys, std::span<const double> s) {
  validateEval2D(s, x, xs, y, ys);
  const std::size_t i = locateInterval(ss, s);
  const double ds = s[i] - s[i - 1];
  const double t = (ss - s[i - 1]) / ds;

  const double cx1 = ds * xs[i - 1] - x[i] + x[i - 1];
  const double cx2 = ds * xs[i] - x[i] + x[i - 1];
  const double xd = x[i] - x[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cx1 +
                    t * (3.0 * t - 2.0) * cx2;
  const double xdd = (6.0 * t - 4.0) * cx1 + (6.0 * t - 2.0) * cx2;
  const double xddd = 6.0 * cx1 + 6.0 * cx2;

  const double cy1 = ds * ys[i - 1] - y[i] + y[i - 1];
  const double cy2 = ds * ys[i] - y[i] + y[i - 1];
  const double yd = y[i] - y[i - 1] + (1.0 - 4.0 * t + 3.0 * t * t) * cy1 +
                    t * (3.0 * t - 2.0) * cy2;
  const double ydd = (6.0 * t - 4.0) * cy1 + (6.0 * t - 2.0) * cy2;
  const double yddd = 6.0 * cy1 + 6.0 * cy2;

  double sd = std::sqrt(xd * xd + yd * yd);
  sd = std::max(sd, 0.001 * ds);

  const double bot = sd * sd * sd;
  const double dbotdt = 3.0 * sd * (xd * xdd + yd * ydd);

  const double top = xd * ydd - yd * xdd;
  const double dtopdt = xd * yddd - yd * xddd;

  return (dtopdt * bot - dbotdt * top) / (bot * bot);
}

Diagnostic sinvrt(double& si, double xi, std::span<const double> x,
                  std::span<const double> xs, std::span<const double> s) {
  validateEval1D(s, x, xs);
  const double sisav = si;
  for (int iter = 0; iter < 10; ++iter) {
    const double res = seval(si, x, xs, s) - xi;
    const double resp = deval(si, x, xs, s);
    const double ds = -res / resp;
    si = si + ds;
    if (std::abs(ds / (s[s.size() - 1] - s[0])) < 1.0E-5) {
      return Diagnostic::completed;
    }
  }
  // Fortran: WRITE 'SINVRT: spline inversion failed. Input value returned.'
  // (no I/O in the core); the input value is restored and the
  // diagnostic state is returned in structured form.
  si = sisav;
  return Diagnostic::limitReached;
}

void scalc(std::span<const double> x, std::span<const double> y,
           std::span<double> s) {
  const std::size_t n = x.size();
  if (y.size() < n || s.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::scalc: Y/S length is smaller than X length");
  }
  if (n == 0) {
    throw std::invalid_argument(
        "xfoil::core::math::scalc: N == 0 (empty input)");
  }
  s[0] = 0.0;
  for (std::size_t i = 1; i < n; ++i) {
    const double dx = x[i] - x[i - 1];
    const double dy = y[i] - y[i - 1];
    s[i] = s[i - 1] + std::sqrt(dx * dx + dy * dy);
  }
}

Diagnostic splnxy(std::span<const double> x, std::span<double> xs,
                  std::span<const double> y, std::span<double> ys,
                  std::span<double> s) {
  const std::size_t n = x.size();
  if (y.size() < n || xs.size() < n || ys.size() < n || s.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::splnxy: array lengths are smaller than X length");
  }
  if (n < 2) {
    throw std::invalid_argument(
        "xfoil::core::math::splnxy: N < 2 (minimum point count)");
  }
  std::array<double, kSplineNxyMax + 1> xt{};
  std::array<double, kSplineNxyMax + 1> yt{};

  scalc(x, y, s);
  segspl(x, s, xs);
  segspl(y, s, ys);

  // Fortran: NPASS = 10
  for (int ipass = 0; ipass < 10; ++ipass) {
    double serr = 0.0;
    double ds = s[1] - s[0];

    // Fortran: DO I=2, N -> i = 1..n-1
    for (std::size_t i = 1; i < n; ++i) {
      const double dx = x[i] - x[i - 1];
      const double dy = y[i] - y[i - 1];

      const double cx1 = ds * xs[i - 1] - dx;
      const double cx2 = ds * xs[i] - dx;
      const double cy1 = ds * ys[i - 1] - dy;
      const double cy2 = ds * ys[i] - dy;

      xt[0] = 0.0;
      yt[0] = 0.0;
      for (std::size_t k = 1; k < kSplineNxyMax; ++k) {
        const double t = static_cast<double>(k) / static_cast<double>(kSplineNxyMax);
        xt[k] = t * dx + (t - t * t) * ((1.0 - t) * cx1 - t * cx2);
        yt[k] = t * dy + (t - t * t) * ((1.0 - t) * cy1 - t * cy2);
      }
      xt[kSplineNxyMax] = dx;
      yt[kSplineNxyMax] = dy;

      double sint1 = 0.0;
      for (std::size_t k = 1; k <= kSplineNxyMax; ++k) {
        const double ex = xt[k] - xt[k - 1];
        const double ey = yt[k] - yt[k - 1];
        sint1 += std::sqrt(ex * ex + ey * ey);
      }
      double sint2 = 0.0;
      for (std::size_t k = 2; k <= kSplineNxyMax; k += 2) {
        const double ex = xt[k] - xt[k - 2];
        const double ey = yt[k] - yt[k - 2];
        sint2 += std::sqrt(ex * ex + ey * ey);
      }

      const double sint = (4.0 * sint1 - sint2) / 3.0;

      if (std::abs(sint - ds) > std::abs(serr)) {
        serr = sint - ds;
      }
      if (i + 1 < n) {
        ds = s[i + 1] - s[i];
      }
      s[i] = s[i - 1] + std::sqrt(sint);
    }

    serr = serr / (s[n - 1] - s[0]);
    // Fortran: WRITE(*,*) IPASS,SERR (no I/O in the core)

    segspl(x, s, xs);
    segspl(y, s, ys);

    if (std::abs(serr) < 1.0E-7) {
      return Diagnostic::completed;
    }
  }
  // Fortran: NPASS reached without tolerance being met (no explicit
  // diagnostic text in SPLNXY). The diagnostic state is returned
  // in structured form; the numerical result stays unchanged.
  return Diagnostic::limitReached;
}

void segspl(std::span<const double> x, std::span<const double> s,
            std::span<double> xs) {
  const std::size_t n = x.size();
  if (s.size() < n || xs.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::segspl: S/XS length is smaller than X length");
  }
  if (n < 2) {
    throw std::invalid_argument(
        "xfoil::core::math::segspl: N < 2 (minimum point count)");
  }
  if (s[1] == s[0]) {
    throw std::invalid_argument(
        "xfoil::core::math::segspl: First input point duplicated");
  }
  if (s[n - 1] == s[n - 2]) {
    throw std::invalid_argument(
        "xfoil::core::math::segspl: Last input point duplicated");
  }

  std::size_t iseg0 = 1;
  // Fortran: DO ISEG=2, N-2 (ISEG 1-based position)
  for (std::size_t iseg = 2; iseg + 1 < n; ++iseg) {
    if (s[iseg - 1] == s[iseg]) {
      const std::size_t nseg = iseg - iseg0 + 1;
      splind(x.subspan(iseg0 - 1, nseg), s.subspan(iseg0 - 1, nseg),
             xs.subspan(iseg0 - 1, nseg), -999.0, -999.0);
      iseg0 = iseg + 1;
    }
  }
  const std::size_t nseg = n - iseg0 + 1;
  splind(x.subspan(iseg0 - 1, nseg), s.subspan(iseg0 - 1, nseg),
         xs.subspan(iseg0 - 1, nseg), -999.0, -999.0);
}

void segsplD(std::span<const double> x, std::span<const double> s,
             std::span<double> xs, double xs1, double xs2) {
  const std::size_t n = x.size();
  if (s.size() < n || xs.size() < n) {
    throw std::invalid_argument(
        "xfoil::core::math::segsplD: S/XS length is smaller than X length");
  }
  if (n < 2) {
    throw std::invalid_argument(
        "xfoil::core::math::segsplD: N < 2 (minimum point count)");
  }
  if (s[1] == s[0]) {
    throw std::invalid_argument(
        "xfoil::core::math::segsplD: First input point duplicated");
  }
  if (s[n - 1] == s[n - 2]) {
    throw std::invalid_argument(
        "xfoil::core::math::segsplD: Last input point duplicated");
  }

  std::size_t iseg0 = 1;
  for (std::size_t iseg = 2; iseg + 1 < n; ++iseg) {
    if (s[iseg - 1] == s[iseg]) {
      const std::size_t nseg = iseg - iseg0 + 1;
      // XFOIL 6.996: explicit end derivatives apply only at the global
      // first/last point; internal segment joints use the zero-third
      // derivative condition.
      double xs1t = -999.0;
      double xs2t = -999.0;
      if (iseg0 == 1) {
        xs1t = xs1;
      }
      if (iseg0 + nseg - 1 == n) {
        xs2t = xs2;
      }
      splind(x.subspan(iseg0 - 1, nseg), s.subspan(iseg0 - 1, nseg),
             xs.subspan(iseg0 - 1, nseg), xs1t, xs2t);
      iseg0 = iseg + 1;
    }
  }
  const std::size_t nseg = n - iseg0 + 1;
  double xs1t = -999.0;
  double xs2t = -999.0;
  if (iseg0 == 1) {
    xs1t = xs1;
  }
  if (iseg0 + nseg - 1 == n) {
    xs2t = xs2;
  }
  splind(x.subspan(iseg0 - 1, nseg), s.subspan(iseg0 - 1, nseg),
         xs.subspan(iseg0 - 1, nseg), xs1t, xs2t);
}

}  // namespace xfoil::core::math

// End: src/core/numerics/Spline.cpp

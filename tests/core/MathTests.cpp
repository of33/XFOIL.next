// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
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

// File: tests/core/MathTests.cpp
// Unit tests of the new 0-based cores: numerical
// base routines (SETEXP, ATANC, sorting, splines, linear algebra).
//
// the tests use only xfoil::core::math and no app, CLI, or
// 1-based xfoil::math code.
//
// reference comparison: the target values come from the focused
// Fortran harnesses (tests/fortran-reference/harnesses/math), which call
// the canonical routines unchanged. Fortran REAL is single
// precision; the new core computes in double. Numerical
// results are therefore compared with a relative tolerance that covers
// only the single->double precision deviation (formula and
// operation order are identical). Integer results
// (permutations, pivot indices, active lengths) are compared exactly.

#include "TestSupport.h"

#include <complex>
#include <cstddef>
#include <vector>

#include "xfoil/core/math/FortranCompat.h"
#include "xfoil/core/math/LinearAlgebra.h"
#include "xfoil/core/math/Sort.h"
#include "xfoil/core/math/Spline.h"

namespace {

using namespace xfoil::core::math;

// relative tolerance for single(Fortran) -> double(C++)-comparison.
// covers the precision deviation of the Fortran REAL arithmetic.
constexpr double kFortranRelTol = 1.0e-5;
constexpr double kFortranAbsTol = 1.0e-6;

}  // namespace

// ---
// SETEXP
// ---

TEST(unit, MathSetExpGrowing) {
  std::vector<double> s(11, 0.0);
  const Diagnostic diag = setExp(s, 0.1, 1.0);
  CHECK(diag == Diagnostic::completed);
  const std::vector<double> expected = {
      0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
  CHECK_EQ(s.size(), expected.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    CHECK_NEAR(s[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSetExpShrinking) {
  std::vector<double> s(6, 0.0);
  const Diagnostic diag = setExp(s, 0.1, 0.3);
  CHECK(diag == Diagnostic::completed);
  const std::vector<double> expected = {
      0.0, 0.1, 0.174127, 0.229075, 0.269807, 0.3};
  CHECK_EQ(s.size(), expected.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    CHECK_NEAR(s[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSetExpNex2Branch) {
  std::vector<double> s(3, 0.0);
  const Diagnostic diag = setExp(s, 0.2, 1.0);
  CHECK(diag == Diagnostic::completed);
  const std::vector<double> expected = {0.0, 0.2, 1.0};
  for (std::size_t i = 0; i < s.size(); ++i) {
    CHECK_NEAR(s[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSetExpNearlyUniform) {
  std::vector<double> s(5, 0.0);
  const Diagnostic diag = setExp(s, 0.25, 1.0);
  CHECK(diag == Diagnostic::completed);
  const std::vector<double> expected = {0.0, 0.25, 0.5, 0.75, 1.0};
  for (std::size_t i = 0; i < s.size(); ++i) {
    CHECK_NEAR(s[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSetExpTooSmallThrows) {
  std::vector<double> s2(2, 0.0);
  bool threw = false;
  try {
    setExp(s2, 0.1, 1.0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// ---
// ATANC
// ---

TEST(unit, MathAtanCBranchCut) {
  // Fortran harness: ATANC(-1,-1,0.75*pi) = 1.25*pi
  CHECK_NEAR(atanC(-1.0, -1.0, 0.75 * 3.14159265358979),
             1.25 * 3.14159265358979, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, MathAtanCQuadrants) {
  // reference values from utils_reference.txt (Fortran single precision).
  CHECK_NEAR(atanC(1.0, 1.0, 0.0), 0.785398185, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(atanC(1.0, -1.0, 0.0), 2.35619450, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(atanC(-1.0, 1.0, 0.0), -0.785398185, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(atanC(-1.0, -1.0, 0.0), -2.35619450, kFortranAbsTol,
             kFortranRelTol);
}

TEST(unit, MathAtanCBranchCrossing) {
  // harness cases with THOLD > pi (multiple circulations around the branch cut).
  CHECK_NEAR(atanC(-1.0, -1.0, 0.75 * 3.14159265358979), 3.92699099,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(atanC(1.0, -1.0, 2.5 * 3.14159265358979), 8.63937950,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(atanC(-1.0, -1.0, 3.0 * 3.14159265358979), 10.21017647,
             kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(atanC(0.0, 1.0, 3.5 * 3.14159265358979), 12.56637096,
             kFortranAbsTol, kFortranRelTol);
}

// ---
// HSORT / ASORT
// ---

TEST(unit, MathHSortUnsorted) {
  const std::vector<double> a = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0};
  std::vector<int> indx(6, 0);
  hSort(a, indx);
  // Fortran INDX 1-based: {4,2,6,1,3,5} -> 0-based {3,1,5,0,2,4}
  const std::vector<int> expected = {3, 1, 5, 0, 2, 4};
  CHECK_EQ(indx.size(), expected.size());
  for (std::size_t i = 0; i < indx.size(); ++i) {
    CHECK_EQ(indx[i], expected[i]);
  }
  // Verification: A(INDX(i)) ascending
  for (std::size_t i = 1; i < indx.size(); ++i) {
    CHECK(a[static_cast<std::size_t>(indx[i - 1])] <=
          a[static_cast<std::size_t>(indx[i])]);
  }
}

TEST(unit, MathHSortSortedAndReversed) {
  const std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::vector<int> indx(6, 0);
  hSort(sorted, indx);
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_EQ(indx[i], static_cast<int>(i));
  }

  const std::vector<double> reversed = {9.0, 7.0, 5.0, 3.0, 1.0, 0.0};
  std::vector<int> indx2(6, 0);
  hSort(reversed, indx2);
  const std::vector<int> expected = {5, 4, 3, 2, 1, 0};
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_EQ(indx2[i], expected[i]);
  }
}

TEST(unit, MathHSortDuplicates) {
  const std::vector<double> a = {4.0, 2.0, 4.0, 1.0, 2.0, 3.0};
  std::vector<int> indx(6, 0);
  hSort(a, indx);
  // Fortran {4,5,2,6,3,1} -> 0-based {3,4,1,5,2,0}
  const std::vector<int> expected = {3, 4, 1, 5, 2, 0};
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_EQ(indx[i], expected[i]);
  }
}

TEST(unit, MathHSortSingleElement) {
  const std::vector<double> a = {7.0};
  std::vector<int> indx(1, 99);
  hSort(a, indx);
  CHECK_EQ(indx[0], 0);
}

TEST(unit, MathASortReorders) {
  const std::vector<double> unsorted = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0};
  std::vector<int> indx(6, 0);
  hSort(unsorted, indx);
  std::vector<double> a = unsorted;
  std::vector<double> atmp(6, 0.0);
  aSort(a, indx, atmp);
  const std::vector<double> expected = {1.0, 2.0, 3.0, 5.0, 8.0, 9.0};
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_NEAR(a[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// REMD
// ---

TEST(unit, MathRemDupIndicesTolZero) {
  const std::vector<double> a = {1.0, 1.0, 2.0, 2.0, 2.0, 5.0, 6.0, 6.0};
  std::vector<int> indx(8, 0);
  std::size_t nnew = 0;
  remDupIndices(a, 0.0, indx, nnew);
  // Fortran INDX 1-based {1,3,6,7} -> 0-based {0,2,5,6}
  CHECK_EQ(nnew, std::size_t{4});
  CHECK_EQ(indx[0], 0);
  CHECK_EQ(indx[1], 2);
  CHECK_EQ(indx[2], 5);
  CHECK_EQ(indx[3], 6);
}

TEST(unit, MathRemDupIndicesTolNonzero) {
  const std::vector<double> a = {1.0, 1.05, 1.1, 1.16, 2.0};
  std::vector<int> indx(5, 0);
  std::size_t nnew = 0;
  remDupIndices(a, 0.1, indx, nnew);
  // Fortran: NNEW=2, INDX 1-based {1,5} -> 0-based {0,4}
  CHECK_EQ(nnew, std::size_t{2});
  CHECK_EQ(indx[0], 0);
  CHECK_EQ(indx[1], 4);
}

// ---
// SORTDUP
// ---

TEST(unit, MathSortDup) {
  std::vector<double> a = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0};
  std::vector<double> w = {50.0, 20.0, 80.0, 10.0, 90.0, 30.0};
  const Diagnostic sortDupDiag = sortDup(a, w);
  CHECK(sortDupDiag == Diagnostic::completed);
  const std::vector<double> expA = {1.0, 2.0, 3.0, 5.0, 8.0, 9.0};
  const std::vector<double> expW = {10.0, 20.0, 30.0, 50.0, 80.0, 90.0};
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_NEAR(a[i], expA[i], kFortranAbsTol, kFortranRelTol);
    CHECK_NEAR(w[i], expW[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// FIXDUP
// ---

TEST(unit, MathFixDup) {
  std::vector<double> a = {3.0, 3.0, 1.0, 2.0, 2.0, 2.0, 3.0};
  std::vector<double> w = {30.0, 30.0, 10.0, 20.0, 20.0, 20.0, 30.0};
  std::size_t kk = 7;
  fixDup(kk, a, w);
  CHECK_EQ(kk, std::size_t{5});
  const std::vector<double> expA = {3.0, 1.0, 2.0, 2.0, 3.0};
  const std::vector<double> expW = {30.0, 10.0, 20.0, 20.0, 30.0};
  for (std::size_t i = 0; i < kk; ++i) {
    CHECK_NEAR(a[i], expA[i], kFortranAbsTol, kFortranRelTol);
    CHECK_NEAR(w[i], expW[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathFixDupNoChange) {
  std::vector<double> a = {1.0, 2.0, 3.0};
  std::vector<double> w = {10.0, 20.0, 30.0};
  std::size_t kk = 3;
  fixDup(kk, a, w);
  CHECK_EQ(kk, std::size_t{3});
  const std::vector<double> expected = {1.0, 2.0, 3.0};
  for (std::size_t i = 0; i < kk; ++i) {
    CHECK_NEAR(a[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathFixDupTooSmallNoop) {
  std::vector<double> a = {5.0};
  std::vector<double> w = {50.0};
  std::size_t kk = 1;
  fixDup(kk, a, w);
  CHECK_EQ(kk, std::size_t{1});
}

// ---
// SORT (sorting + removal of duplicate pairs)
// ---

TEST(unit, MathSortAndRemovePairs) {
  std::vector<double> a = {4.0, 1.0, 1.0, 3.0, 3.0, 2.0, 5.0, 5.0};
  std::vector<double> w = {40.0, 10.0, 10.0, 30.0, 30.0, 20.0, 50.0, 50.0};
  std::size_t kk = 8;
  const Diagnostic sortDiag = sortAndRemovePairs(kk, a, w);
  CHECK(sortDiag == Diagnostic::completed);
  CHECK_EQ(kk, std::size_t{2});
  CHECK_NEAR(a[0], 2.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[1], 4.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(w[0], 20.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(w[1], 40.0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, MathSortNoPairs) {
  std::vector<double> a = {3.0, 1.0, 2.0};
  std::vector<double> w = {30.0, 10.0, 20.0};
  std::size_t kk = 3;
  const Diagnostic sortNoPairsDiag = sortAndRemovePairs(kk, a, w);
  CHECK(sortNoPairsDiag == Diagnostic::completed);
  CHECK_EQ(kk, std::size_t{3});
  CHECK_NEAR(a[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[1], 2.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[2], 3.0, kFortranAbsTol, kFortranRelTol);
}

// ---
// SORTOL
// ---

TEST(unit, MathSortWithTolerance) {
  std::vector<double> a = {1.0, 1.0, 1.05, 2.0, 2.02};
  std::vector<double> w = {1.0, 0.0, 1.0, 2.0, 2.0};
  std::size_t kk = 5;
  sortWithTolerance(0.2, kk, a, w);
  CHECK_EQ(kk, std::size_t{4});
  const std::vector<double> expA = {1.0, 1.0, 1.05, 2.0};
  const std::vector<double> expW = {1.0, 0.0, 1.0, 2.0};
  for (std::size_t i = 0; i < kk; ++i) {
    CHECK_NEAR(a[i], expA[i], kFortranAbsTol, kFortranRelTol);
    CHECK_NEAR(w[i], expW[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// TRISOL
// ---

TEST(unit, MathTriSol) {
  std::vector<double> a = {2.0, 3.0, 4.0, 5.0};
  const std::vector<double> b = {0.0, 1.0, 1.0, 1.0};
  std::vector<double> c = {1.0, 1.0, 1.0, 0.0};
  std::vector<double> d = {1.0, 2.0, 3.0, 4.0};
  triSol(a, b, c, d);
  const std::vector<double> expected = {0.2941176, 0.4117647, 0.4705882,
                                        0.7058823};
  for (std::size_t i = 0; i < 4; ++i) {
    CHECK_NEAR(d[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// SPLINE + SEVAL/DEVAL/D2VAL (X = S^2)
// ---

TEST(unit, MathSplineAndEval) {
  const std::vector<double> s = {0.0, 0.5, 1.0, 1.5, 2.0};
  std::vector<double> x(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    x[i] = s[i] * s[i];
  }
  std::vector<double> xs(s.size(), 0.0);
  spline(x, s, xs);

  // Fortran XS (single precision): {0.2857, 0.9286, 2.0, 3.0714, 3.7143}
  const std::vector<double> expXs = {0.2857143, 0.9285714, 2.0, 3.0714286,
                                     3.7142857};
  for (std::size_t i = 0; i < xs.size(); ++i) {
    CHECK_NEAR(xs[i], expXs[i], kFortranAbsTol, kFortranRelTol);
  }

  CHECK_NEAR(seval(0.0, x, xs, s), 0.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(1.0, x, xs, s), 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(0.75, x, xs, s), 0.5580357, kFortranAbsTol,
             kFortranRelTol);
  // Extrapolation corresponding Fortran
  CHECK_NEAR(seval(2.5, x, xs, s), 5.75, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(seval(-0.5, x, xs, s), -0.25, kFortranAbsTol, kFortranRelTol);

  CHECK_NEAR(deval(0.75, x, xs, s), 1.517857, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(d2val(0.75, x, xs, s), 2.142857, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(deval(1.0, x, xs, s), 2.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(d2val(1.0, x, xs, s), 1.7142857, kFortranAbsTol,
             kFortranRelTol);
}

// ---
// SPLIND
// ---

TEST(unit, MathSplindSpecifiedEnds) {
  const std::vector<double> s = {0.0, 1.0, 2.0, 3.0};
  const std::vector<double> x = {1.0, 2.0, 4.0, 8.0};
  std::vector<double> xs(4, 0.0);
  splind(x, s, xs, 2.0, 12.0);
  const std::vector<double> expected = {2.0, 1.4666667, 1.1333332, 12.0};
  for (std::size_t i = 0; i < 4; ++i) {
    CHECK_NEAR(xs[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSplindZeroThirdDeriv) {
  const std::vector<double> s = {0.0, 1.0, 2.0, 3.0};
  const std::vector<double> x = {1.0, 2.0, 4.0, 8.0};
  std::vector<double> xs(4, 0.0);
  splind(x, s, xs, -999.0, -999.0);
  const std::vector<double> expected = {0.625, 1.375, 2.875, 5.125};
  for (std::size_t i = 0; i < 4; ++i) {
    CHECK_NEAR(xs[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// SPLINA
// ---

TEST(unit, MathSplina) {
  const std::vector<double> s = {0.0, 0.4, 0.9, 1.3, 2.0};
  const std::vector<double> x = {0.0, 0.5, 1.1, 1.8, 3.0};
  std::vector<double> xs(5, 0.0);
  splina(x, s, xs);
  const std::vector<double> expected = {1.25, 1.225, 1.475, 1.7321427,
                                        1.7142857};
  for (std::size_t i = 0; i < 5; ++i) {
    CHECK_NEAR(xs[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// SCALC
// ---

TEST(unit, MathScalc) {
  const std::vector<double> x = {0.0, 1.0, 1.0, 1.0, 0.0};
  const std::vector<double> y = {0.0, 0.0, 1.0, 2.0, 2.0};
  std::vector<double> s(5, 0.0);
  scalc(x, y, s);
  const std::vector<double> expected = {0.0, 1.0, 2.0, 3.0, 4.0};
  for (std::size_t i = 0; i < 5; ++i) {
    CHECK_NEAR(s[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// CURV / CURVS (circle, radius 1)
// ---

TEST(unit, MathCurvAndCurvs) {
  const std::vector<double> x = {1.0, 0.0, -1.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 1.0, 0.0, -1.0, 0.0};
  std::vector<double> s(5, 0.0);
  std::vector<double> xs(5, 0.0);
  std::vector<double> ys(5, 0.0);
  scalc(x, y, s);
  spline(x, s, xs);
  spline(y, s, ys);
  CHECK_NEAR(curv(1.0, x, xs, y, ys, s), 1.2192187, kFortranAbsTol,
             kFortranRelTol);
  CHECK_NEAR(curvs(0.0, x, xs, y, ys, s), 0.7480246, kFortranAbsTol,
             kFortranRelTol);
}

// ---
// SINVRT
// ---

TEST(unit, MathSinvrt) {
  const std::vector<double> s = {0.0, 0.5, 1.0, 1.5, 2.0};
  std::vector<double> x(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    x[i] = s[i] * s[i];
  }
  std::vector<double> xs(s.size(), 0.0);
  spline(x, s, xs);
  double si = 1.5;
  const Diagnostic sinvrtDiag = sinvrt(si, 1.0, x, xs, s);
  CHECK(sinvrtDiag == Diagnostic::completed);
  CHECK_NEAR(si, 1.0, kFortranAbsTol, kFortranRelTol);
}

// ---
// SPLNXY
// ---

TEST(unit, MathSplnxy) {
  const std::vector<double> x = {1.0, 0.0, -1.0, 0.0, 1.0};
  const std::vector<double> y = {0.0, 1.0, 0.0, -1.0, 0.0};
  std::vector<double> xs(5, 0.0);
  std::vector<double> ys(5, 0.0);
  std::vector<double> s(5, 0.0);
  const Diagnostic splnxyDiag = splnxy(x, xs, y, ys, s);
  // the circle shape does not converge within 10 passes: canonical
  // pass limit (Fortran S after 10 passes).
  CHECK(splnxyDiag == Diagnostic::limitReached);
  // Fortran S (single precision) after 10 passes.
  const std::vector<double> expectedS = {0.0, 1.2481189, 2.4779892,
                                         3.7078595, 4.9559784};
  for (std::size_t i = 0; i < 5; ++i) {
    CHECK_NEAR(s[i], expectedS[i], kFortranAbsTol, kFortranRelTol);
  }
}

// ---
// SEGSPL / SEGSPLD
// ---

TEST(unit, MathSegspl) {
  const std::vector<double> s = {0.0, 0.5, 1.0, 1.0, 1.5, 2.0};
  const std::vector<double> x = {0.0, 0.25, 1.0, 1.0, 2.25, 4.0};
  std::vector<double> xs(6, 0.0);
  segspl(x, s, xs);
  const std::vector<double> expected = {0.0, 1.0, 2.0, 2.0, 3.0, 4.0};
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_NEAR(xs[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSegsplD) {
  const std::vector<double> s = {0.0, 0.5, 1.0, 1.0, 1.5, 2.0};
  const std::vector<double> x = {0.0, 0.25, 1.0, 1.0, 2.25, 4.0};
  std::vector<double> xs(6, 0.0);
  segsplD(x, s, xs, -999.0, -999.0);
  const std::vector<double> expected = {0.0, 1.0, 2.0, 2.0, 3.0, 4.0};
  for (std::size_t i = 0; i < 6; ++i) {
    CHECK_NEAR(xs[i], expected[i], kFortranAbsTol, kFortranRelTol);
  }
}

TEST(unit, MathSegsplDGlobalEndpointsOnly) {
  // XFOIL 6.996 SEGSPLD: explicit end derivatives apply only at the
  // global first/last point. The interior joint uses zero third derivative.
  const std::vector<double> s = {0.0, 1.0, 2.0, 2.0, 3.0};
  const std::vector<double> x = {0.0, 1.0, 1.0, 1.0, 2.0};
  std::vector<double> xs(5, 0.0);

  segsplD(x, s, xs, 2.0, 12.0);
  const std::vector<double> expected = {
      2.0, 1.0 / 3.0, -1.0 / 3.0, -10.0, 12.0};
  for (std::size_t i = 0; i < expected.size(); ++i) {
    CHECK_NEAR(xs[i], expected[i], 1.0e-14, 0.0);
  }

  // The exact joint selects the right segment's one-sided derivative.
  CHECK_NEAR(deval(2.0, x, xs, s), -10.0, 1.0e-14, 0.0);
  // Left/right neighborhoods retain the respective segment derivatives.
  CHECK_NEAR(deval(1.999999, x, xs, s), -0.3333326666666666, 1.0e-6, 0.0);
  CHECK_NEAR(deval(2.000001, x, xs, s), -9.999977999999999, 1.0e-6, 0.0);
  CHECK_NEAR(seval(2.0, x, xs, s), 1.0, 1.0e-14, 0.0);
  CHECK_NEAR(d2val(2.0, x, xs, s), 22.0, 1.0e-12, 0.0);

  // Non-segmented control: both explicit endpoint derivatives are used.
  const std::vector<double> sControl = {0.0, 1.0, 2.0, 3.0};
  const std::vector<double> xControl = {0.0, 1.0, 1.5, 2.5};
  std::vector<double> xsControl(4, 0.0);
  segsplD(xControl, sControl, xsControl, 2.0, 12.0);
  const std::vector<double> expectedControl = {
      2.0, 7.0 / 6.0, -13.0 / 6.0, 12.0};
  for (std::size_t i = 0; i < expectedControl.size(); ++i) {
    CHECK_NEAR(xsControl[i], expectedControl[i], 1.0e-14, 0.0);
  }
}

TEST(unit, MathSegsplDRejectsInvalidInputsWithoutMutation) {
  // Size/minimum and duplicated global-end checks happen before any
  // segment solve; the output span remains untouched.
  const std::vector<double> tinyS = {0.0};
  const std::vector<double> tinyX = {1.0};
  std::vector<double> tinyOut = {42.0};
  bool tinyThrew = false;
  try {
    segsplD(tinyX, tinyS, tinyOut, 2.0, 12.0);
  } catch (const std::invalid_argument&) {
    tinyThrew = true;
  }
  CHECK(tinyThrew);
  CHECK_EQ(tinyOut[0], 42.0);

  const std::vector<double> firstDupS = {0.0, 0.0, 1.0};
  const std::vector<double> firstDupX = {0.0, 1.0, 2.0};
  std::vector<double> firstDupOut = {7.0, 8.0, 9.0};
  bool firstThrew = false;
  try {
    segsplD(firstDupX, firstDupS, firstDupOut, 2.0, 12.0);
  } catch (const std::invalid_argument&) {
    firstThrew = true;
  }
  CHECK(firstThrew);
  CHECK_EQ(firstDupOut[0], 7.0);
  CHECK_EQ(firstDupOut[1], 8.0);
  CHECK_EQ(firstDupOut[2], 9.0);

  const std::vector<double> lastDupS = {0.0, 1.0, 1.0};
  const std::vector<double> lastDupX = {0.0, 1.0, 2.0};
  std::vector<double> lastDupOut = {7.0, 8.0, 9.0};
  bool lastThrew = false;
  try {
    segsplD(lastDupX, lastDupS, lastDupOut, 2.0, 12.0);
  } catch (const std::invalid_argument&) {
    lastThrew = true;
  }
  CHECK(lastThrew);
  CHECK_EQ(lastDupOut[0], 7.0);
  CHECK_EQ(lastDupOut[1], 8.0);
  CHECK_EQ(lastDupOut[2], 9.0);
}

TEST(unit, MathSegsplDuplicatedFirstThrows) {
  const std::vector<double> s = {0.0, 0.0, 1.0};
  const std::vector<double> x = {0.0, 0.0, 1.0};
  std::vector<double> xs(3, 0.0);
  bool threw = false;
  try {
    segspl(x, s, xs);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// ---
// GAUSS
// ---

TEST(unit, MathGauss) {
  std::vector<double> z = {
      0.5, 1.0, 0.0,  // row 0
      1.0, 2.0, 1.0,  // row 1
      0.0, 1.0, 2.0   // row 2
  };
  // row-major (nn rows x nrhs columns): column 0 = RHS0 {1.5,4,3},
  // column 1 = RHS1 {0.5,2,3}.
  std::vector<double> r = {
      1.5, 0.5,  // row 0
      4.0, 2.0,  // row 1
      3.0, 3.0   // row 2
  };
  gauss(z, r, 3, 2);
  // Solution: RHS0 = {1,1,1}, RHS1 = {-1,1,1} (Fortran reference).
  // row-major: r[0]=RHS0[0], r[1]=RHS1[0], r[2]=RHS0[1], ...
  CHECK_NEAR(r[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[1], -1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[2], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[3], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[4], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[5], 1.0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, MathGaussSizeError) {
  std::vector<double> z(9, 0.0);
  std::vector<double> r(6, 0.0);
  bool threw = false;
  try {
    gauss(z, r, 4, 2);  // 4x4 > 9 elements
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// ---
// CGAUSS
// ---

TEST(unit, MathCgauss) {
  std::vector<std::complex<double>> z = {
      {1.0, 1.0}, {2.0, -1.0},  // row 0
      {1.0, -1.0}, {3.0, 2.0}   // row 1
  };
  std::vector<std::complex<double>> r = {
      {3.0, 2.0},
      {4.0, 1.0},
  };
  cgauss(z, r, 2, 1);
  // Fortran reference: CR(1)=1.75+0.5i, CR(2)=0.75+0.25i
  CHECK_NEAR(r[0].real(), 1.75, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[0].imag(), 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[1].real(), 0.75, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(r[1].imag(), 0.25, kFortranAbsTol, kFortranRelTol);
}

// ---
// LUDCMP + BAKSUB
// ---

TEST(unit, MathLudcmpBaksub) {
  std::vector<double> a = {
      2.0, 1.0, 1.0,  //
      1.0, 3.0, 2.0,  //
      1.0, 2.0, 3.0   //
  };
  std::vector<int> indx(3, 0);
  ludcmp(a, 3, indx);
  // Fortran LU factors (1-based) -> 0-based row-major
  CHECK_NEAR(a[0], 2.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[1], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[2], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[3], 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[4], 2.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[5], 1.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[6], 0.5, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[7], 0.6, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(a[8], 1.6, kFortranAbsTol, kFortranRelTol);
  // INDX 0-based {0,1,2}
  CHECK_EQ(indx[0], 0);
  CHECK_EQ(indx[1], 1);
  CHECK_EQ(indx[2], 2);

  std::vector<double> b = {5.0, 9.0, 9.0};
  baksub(a, 3, indx, b);
  // Fortran BAKSUB: B = {0.875, 1.625, 1.625}
  CHECK_NEAR(b[0], 0.875, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b[1], 1.625, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b[2], 1.625, kFortranAbsTol, kFortranRelTol);
}

// ---
// Safety checks: reject invalid quantities before any mutation
// ---

TEST(unit, MathGaussZeroSizeThrows) {
  std::vector<double> z;
  std::vector<double> r;
  bool threw = false;
  try {
    gauss(z, r, 0, 1);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathCgaussZeroSizeThrows) {
  std::vector<std::complex<double>> z;
  std::vector<std::complex<double>> r;
  bool threw = false;
  try {
    cgauss(z, r, 0, 1);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathLudcmpZeroSizeThrows) {
  std::vector<double> a;
  std::vector<int> indx;
  bool threw = false;
  try {
    ludcmp(a, 0, indx);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathLudcmpOverNvxThrows) {
  std::vector<double> a(501 * 501, 1.0);
  std::vector<int> indx(501, 0);
  bool threw = false;
  try {
    ludcmp(a, 501, indx);  // N > NVX (500)
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathBaksubInvalidPivotThrows) {
  std::vector<double> a = {
      2.0, 1.0, 1.0,  //
      1.0, 3.0, 2.0,  //
      1.0, 2.0, 3.0   //
  };
  std::vector<int> indx = {3, 0, 1};  // Index 3 outside [0,3)
  std::vector<double> b = {1.0, 1.0, 1.0};
  bool threw = false;
  try {
    baksub(a, 3, indx, b);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
  // b must remain unmutated.
  CHECK_NEAR(b[0], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b[1], 1.0, kFortranAbsTol, kFortranRelTol);
  CHECK_NEAR(b[2], 1.0, kFortranAbsTol, kFortranRelTol);
}

TEST(unit, MathBaksubNegativePivotThrows) {
  std::vector<double> a = {
      2.0, 1.0, 1.0,  //
      1.0, 3.0, 2.0,  //
      1.0, 2.0, 3.0   //
  };
  std::vector<int> indx = {-1, 0, 1};
  std::vector<double> b = {1.0, 1.0, 1.0};
  bool threw = false;
  try {
    baksub(a, 3, indx, b);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathSortDupInvalidWidthThrows) {
  std::vector<double> s = {3.0, 1.0, 2.0};
  std::vector<double> w = {30.0, 10.0};
  bool threw = false;
  try {
    sortDup(s, w);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathSortAndRemovePairsInvalidWidthThrows) {
  std::vector<double> s = {3.0, 1.0, 2.0};
  std::vector<double> w = {30.0, 10.0};
  std::size_t kk = 3;
  bool threw = false;
  try {
    sortAndRemovePairs(kk, s, w);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathSortAndRemovePairsKkOversizeThrows) {
  std::vector<double> s = {3.0, 1.0, 2.0};
  std::vector<double> w = {30.0, 10.0, 20.0};
  std::size_t kk = 4;  // kk > s.size
  bool threw = false;
  try {
    sortAndRemovePairs(kk, s, w);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathFixDupKkOversizeThrows) {
  std::vector<double> s = {3.0, 1.0, 2.0};
  std::vector<double> w = {30.0, 10.0, 20.0};
  std::size_t kk = 5;  // kk > s.size
  bool threw = false;
  try {
    fixDup(kk, s, w);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathSortWithToleranceKkOversizeThrows) {
  std::vector<double> s = {3.0, 1.0, 2.0};
  std::vector<double> w = {30.0, 10.0, 20.0};
  std::size_t kk = 4;
  bool threw = false;
  try {
    sortWithTolerance(0.1, kk, s, w);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathSplineTooFewPointsThrows) {
  std::vector<double> x = {1.0};
  std::vector<double> s = {0.0};
  std::vector<double> xs(1, 0.0);
  bool threw = false;
  try {
    spline(x, s, xs);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathScalcEmptyThrows) {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> s;
  bool threw = false;
  try {
    scalc(x, y, s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathTriSolZeroSizeThrows) {
  std::vector<double> a;
  std::vector<double> b;
  std::vector<double> c;
  std::vector<double> d;
  bool threw = false;
  try {
    triSol(a, b, c, d);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

TEST(unit, MathSplnxyTooFewPointsThrows) {
  std::vector<double> x = {1.0};
  std::vector<double> y = {0.0};
  std::vector<double> xs(1, 0.0);
  std::vector<double> ys(1, 0.0);
  std::vector<double> s(1, 0.0);
  bool threw = false;
  try {
    splnxy(x, xs, y, ys, s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// End: tests/core/MathTests.cpp

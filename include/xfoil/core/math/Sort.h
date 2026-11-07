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

// File: include/xfoil/core/math/Sort.h
// Sort and duplicate-handling routines of the new 0-based cores,
// directly ported from fortran-src/sort.f.
//
// Contained routines:
// HSORT -> hSort
// ASORT -> aSort
// REMD -> remDupIndices
// SORTDUP -> sortDup
// FIXDUP -> fixDup
// SORT -> sortAndRemovePairs
// SORTOL -> sortWithTolerance
//
// All routines preserve the canonical Fortran structure, comparison and
// tolerance semantics, and the companion-array permutation. The 0-based
// index mapping is documented; no library sort
// algorithms are used as a substitute, which would change the stability,
// duplicate selection, companion permutation, or tolerance semantics.
//
// No raw-pointer API. No 1-based containers.

#pragma once

#include <cstddef>
#include <span>

#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::math {

// Fortran: SUBROUTINE HSORT(N,A,INDX)
// Heapsort. Returns the permutation INDX(0..N-1) with
// A(INDX(i)) < A(INDX(i+1)). A stays unchanged; INDX is
// initialized with 0..N-1. N <= 1: INDX is the identity.
// Safety: indx.size <= a.size and indx.size <= INT_MAX.
// Invalid sizes throw std::invalid_argument before mutation.
void hSort(std::span<const double> a, std::span<int> indx);

// Fortran: SUBROUTINE ASORT(N,A,INDX,ATMP)
// Applies the sorted index list INDX to A and reorders A in place.
// ATMP is a work buffer of length N provided by the caller.
// INDX stays unchanged.
// Safety: a.size == indx.size == atmp.size; each index must lie in
// [0, n). Invalid sizes/indices throw std::invalid_argument
// before mutation.
void aSort(std::span<double> a, std::span<const int> indx,
           std::span<double> atmp);

// Fortran: SUBROUTINE REMD(N,A,INDX,TOL,NNEW)
// Sets the index list INDX(0..nnew-1) so that duplicates of A values
// (with tolerance TOL, strict .GT. TOL) are omitted. Returns the
// resulting active length nnew.
// Safety: indx.size >= a.size and a.size <= INT_MAX. Empty
// input returns nnew = 0 without mutation.
void remDupIndices(std::span<const double> a, double tol,
                   std::span<int> indx, std::size_t& nnew);

// Fortran: SUBROUTINE SORTDUP(KK,S,W)
// Sorts S ascending (bubble sort) and permutes the companion array W
// in step. No duplicate removal. Active length = s.size.
// Return value: Diagnostic::limitReached if the canonical pass limit (1234)
// is reached without completion (Fortran 'Sort failed'); the numerical
// result stays unchanged.
Diagnostic sortDup(std::span<double> s, std::span<double> w);

// Fortran: SUBROUTINE FIXDUP(KK,S,W)
// Removes leading and trailing duplicates as well as more than two
// consecutive identical values. KK (active length) is
// reduced in place. Returns kk (in/out) and S,W in place.
// Safety: kk <= s.size and kk <= w.size. KK < 2 is a no-op
// (Fortran reads out of range for KK<2).
void fixDup(std::size_t& kk, std::span<double> s, std::span<double> w);

// Fortran: SUBROUTINE SORT(KK,S,W)
// Sorts S ascending and then removes adjacent
// duplicate pairs (each two identical values -> both removed, kk -= 2).
// kk (in/out) is reduced in place.
// Return value: Diagnostic::limitReached if the canonical pass limit (1234)
// is reached without completion (Fortran 'Sort failed').
Diagnostic sortAndRemovePairs(std::size_t& kk, std::span<double> s,
                              std::span<double> w);

// Fortran: SUBROUTINE SORTOL(TOL,KK,S,W)
// Sorts S ascending and then removes points whose
// squared distance to their neighbours is < TOL^2 (one point at a time,
// kk -= 1), until none remain. kk (in/out) is reduced
// in place. TOL is the threshold.
// Safety: kk <= s.size and kk <= w.size.
void sortWithTolerance(double tol, std::size_t& kk, std::span<double> s,
                       std::span<double> w);

}  // namespace xfoil::core::math

// End: include/xfoil/core/math/Sort.h

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

// File: include/xfoil/core/inverse/WallProfileKernels.h
// Wall/airfoil kernels of the new 0-based cores, ported from
// fortran-src/profil.f: B3SOLV (3x3 block-tridiagonal solver) and FS
// (Falkner-Skan neighbors).
//
// B3SOLV solves a 3x3 block tridiagonal system with arbitrarily many
// right-hand sides by default block elimination with fixed
// column pivot choice. Singular A blocks lead to a structured
// error (Fortran STOP -> C++ safety deviation).
//
// FS solves the Falkner-Skan equation per Newton iteration (ITMAX=40,
// under-relaxation on max change 0.5, convergence RMS<1e-6 from ITER>3)
// with B3SOLV. Iteration limits and convergence criteria stay exact.
//
// Internal computations in double; canonical Fortran values are REAL(4).

#pragma once

#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::inverse {

// Fortran FS: PARAMETER (NMAX=257, NRMAX=3).
inline constexpr int kFsMaxPoints = 257;
inline constexpr int kFsMaxRhs = 3;

// Fortran: SUBROUTINE B3SOLV(A,B,C,R,N,NRHS,NRMAX)
// A,B,C: 3x3 block diagonal, flat row-major per block:
// Index(K,L,I) = (I-1)*9 + (L-1)*3 + (K-1), K,L=1..3, I=1..N.
// R: solution/right-hand side, Index(K,L,I) = (I-1)*3*nrhs + (L-1)*3 + (K-1).
// Solved in place; the solutions are stored in the R(.,1:nrhs,:) vectors.
Status b3Solv(std::vector<double>& a, std::vector<double>& b,
              std::vector<double>& c, std::vector<double>& r, int n, int nrhs);

// Fortran: SUBROUTINE FS(INORM,ISPEC,BSPEC,HSPEC,N,ETAE,GEO,ETA,F,U,S,DELTA)
struct FalknerSkanResult {
  std::vector<double> eta;
  std::vector<double> f;
  std::vector<double> u;
  std::vector<double> s;
  double delta{0.0};
  double bspec{0.0};
  double hspec{0.0};
};
Result<FalknerSkanResult> falknerSkan(int inorm, int ispec, double bspec,
                                      double hspec, int n, double etae,
                                      double geo);

// Fortran: SUBROUTINE PRWALL(DSTAR,THETA,UO,RT,MS,CT, BB, DO,...,CD_CT)
// Wall Spalding airfoil Newton (40 iterations, convergence
// |REZ1/TH|,|REZ2/TH| < 2e-5, under-relaxation to max change 0.5).
// UO is set internally to 1.0. BB is initialized to 1.0.
// The fields *_DS, *_UO, HS*, Cd*, Cd_CT are not written in the
// canonical Fortran (COMMON pass-through in the caller); they stay here
// at their input value (cpp-safety deviation: defined instead of undefined).
struct WallProfileState {
  double bb{0.0};
  double do_{0.0};
  double do_ds{0.0};
  double do_th{0.0};
  double do_uo{0.0};
  double do_rt{0.0};
  double do_ms{0.0};
  double ui{0.0};
  double ui_ds{0.0};
  double ui_th{0.0};
  double ui_uo{0.0};
  double ui_rt{0.0};
  double ui_ms{0.0};
  double hs{0.0};
  double hs_ds{0.0};
  double hs_th{0.0};
  double hs_uo{0.0};
  double hs_rt{0.0};
  double hs_ms{0.0};
  double cf{0.0};
  double cf_ds{0.0};
  double cf_th{0.0};
  double cf_uo{0.0};
  double cf_rt{0.0};
  double cf_ms{0.0};
  double cd{0.0};
  double cd_ds{0.0};
  double cd_th{0.0};
  double cd_uo{0.0};
  double cd_rt{0.0};
  double cd_ms{0.0};
  double cd_ct{0.0};
};
Status prWall(double dstar, double theta, double rt, double ms, double ct,
              double cffac, WallProfileState& st);

// Fortran: SUBROUTINE UWALL(TH,UO,DO,UI,RT,CF,BB, Y,U,N)
Status uwWall(double th, double uo, double do_, double ui, double rt,
              double cf, double bb, int n, std::vector<double>& y,
              std::vector<double>& u);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/WallProfileKernels.h

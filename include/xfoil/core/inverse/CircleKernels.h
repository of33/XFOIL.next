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

// File: include/xfoil/core/inverse/CircleKernels.h
// Circle grid and coefficient kernels of the new 0-based cores,
// directly ported from fortran-src/xmdes.f (EIWSET, PIQSUM,
// FTP, CNFILT, CNSYMM).
//
// Target namespace: xfoil::core::inverse
//
// Formulas, operation and summation order, and special cases are taken
// exactly from Fortran. Internal computations use double; the canonical
// Fortran values are REAL(4), so reference comparisons must use a
// tolerance matched to the individual accuracy.
//
// The numerical kernel does not write to stdout. No raw-pointer API,
// no owning raw pointers, no 1-based containers, no global
// mutable state, no Fortran EQUIVALENCE, no dependency
// on app, CLI, or tests.

#pragma once

#include <complex>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::inverse {

// Fortran dimensions from CIRCLE.INC: ICX = 257, IMX = (ICX-1)/4 = 64.
inline constexpr int kCircleMaxPoints = 257;
inline constexpr int kCircleMaxCoeffs = 64;

// Fortran: SUBROUTINE EIWSET(NC1)
// Sets the uniformly spaced circle grid WC (omega) and the
// associated complex unit values exp(i m w) (EIW) for the
// slow Fourier transform. NC = NC1, MC = NC1/4, MCT = NC1/16,
// DWC = 2 pi/(NC-1). EIW(IC,0)=1, EIW(IC,1)=exp(i w), and for m>=2
// EIW(IC,m) is obtained from EIW(.,1) by cyclic indexing.
// Error, if NC1 > ICX (257) or NC1 < 2.
struct CircleGrid {
  int nc{0};
  int mc{0};
  int mct{0};
  double pi{0.0};
  double dwc{0.0};
  std::vector<double> wc;                       // nc entries (0-based)
  std::vector<std::complex<double>> eiw;        // nc x (mc+1), row-major
};

Result<CircleGrid> setCircleGrid(int nc1);

// Fortran: SUBROUTINE PIQSUM / FTP / CNFILT / CNSYMM (COMMON/CIRCLE.INC/)
// Coefficient state: cn[0..mc] (Fourier coefficients of P(w)+iQ(w))
// and piq[0..nc-1] (circle-grid values of the harmonic function).
struct CircleCoefficients {
  std::vector<std::complex<double>> cn;   // mc+1 entries
  std::vector<std::complex<double>> piq;  // nc entries
};

// Fortran: SUBROUTINE PIQSUM
// Inverse Fourier synthesis: piq(IC) = sum_{m=0..MC} CN(m) * conj(EIW(IC,m)).
// Summation over m ascending as in Fortran. Error on quantity mismatch.
Status piqSum(const CircleGrid& grid, CircleCoefficients& coeff);

// Fortran: SUBROUTINE FTP
// Slow Fourier transform of PIQ into CN (trapezoidal rule). Inner
// sum IC=2..NC-1 ascending, end points with half weight; CN(0) is
// halved afterwards. Error on quantity mismatch.
Status fourierTransform(const CircleGrid& grid, CircleCoefficients& coeff);

// Fortran: SUBROUTINE CNFILT(FFILT)
// Modified Hanning filter on CN. Returns early for
// FFILT == 0. For FFILT > 0, CWT**FFILT is used; otherwise CWT.
void coeffFilter(CircleCoefficients& coeff, double ffilt);

// Fortran: SUBROUTINE CNSYMM
// Eliminates the imaginary parts (camber shares) of CN(m), m=1..MC.
void coeffSymmetrize(CircleCoefficients& coeff);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/CircleKernels.h

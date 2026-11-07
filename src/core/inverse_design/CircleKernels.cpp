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

// File: src/core/inverse_design/CircleKernels.cpp
// Implementation of the circle grid and coefficient kernels,
// directly ported from fortran-src/xmdes.f (EIWSET, PIQSUM, FTP, CNFILT,
// CNSYMM). Operation and summation order correspond exactly to Fortran.

#include "xfoil/core/inverse/CircleKernels.h"

#include <cmath>
#include <complex>
#include <utility>

namespace xfoil::core::inverse {

namespace {

// Fortran EIWSET: PI = 4.0*ATAN(1.0).
double fortranPi() {
  return 4.0 * std::atan(1.0);
}

// Index in the row-major EIW table (nc lines, mc+1 columns).
std::size_t eiwIndex(int ic, int m, int mc) {
  return static_cast<std::size_t>(ic) * static_cast<std::size_t>(mc + 1) +
         static_cast<std::size_t>(m);
}

}  // namespace

// Fortran: SUBROUTINE EIWSET(NC1)
Result<CircleGrid> setCircleGrid(int nc1) {
  if (nc1 < 2) {
    return Result<CircleGrid>::fail(
        Error::make(ErrorCode::invalidArgument,
                    "xfoil::core::inverse::setCircleGrid: NC1 must be >= 2 "));
  }
  if (nc1 > kCircleMaxPoints) {
    return Result<CircleGrid>::fail(
        Error::make(ErrorCode::capacityExceeded,
                    "xfoil::core::inverse::setCircleGrid: NC1 exceeds "
                    "ICX=257 (EIWSET: array overflow)"));
  }

  CircleGrid grid;
  grid.nc = nc1;
  grid.mc = nc1 / 4;
  grid.mct = nc1 / 16;
  grid.pi = fortranPi();
  grid.dwc = 2.0 * grid.pi / static_cast<double>(grid.nc - 1);

  grid.wc.assign(static_cast<std::size_t>(grid.nc), 0.0);
  grid.eiw.assign(static_cast<std::size_t>(grid.nc) *
                      static_cast<std::size_t>(grid.mc + 1),
                  std::complex<double>(0.0, 0.0));

  // Fortran: DO 10 IC=1,NC ; WC(IC) = DWC*FLOAT(IC-1)
  for (int ic = 1; ic <= grid.nc; ++ic) {
    grid.wc[static_cast<std::size_t>(ic - 1)] =
        grid.dwc * static_cast<double>(ic - 1);
  }

  // Fortran: DO 20 IC=1,NC ; EIW(IC,0) = (1.0, 0.0)
  for (int ic = 1; ic <= grid.nc; ++ic) {
    grid.eiw[eiwIndex(ic - 1, 0, grid.mc)] = std::complex<double>(1.0, 0.0);
  }

 // Fortran: DO 30 IC=1,NC ; EIW(IC,1) = EXP (CMPLX(0.0, WC(IC)))
  for (int ic = 1; ic <= grid.nc; ++ic) {
    grid.eiw[eiwIndex(ic - 1, 1, grid.mc)] =
        std::exp(std::complex<double>(0.0, grid.wc[static_cast<std::size_t>(ic - 1)]));
  }

  // Fortran: DO 40 M=2,MC ; DO 410 IC=1,NC
  // IC1 = M*(IC-1) ; IC1 = MOD(IC1,NC-1)+1 ; EIW(IC,M)=EIW(IC1,1)
  for (int m = 2; m <= grid.mc; ++m) {
    for (int ic = 1; ic <= grid.nc; ++ic) {
      int ic1 = m * (ic - 1);
      ic1 = (ic1 % (grid.nc - 1)) + 1;
      grid.eiw[eiwIndex(ic - 1, m, grid.mc)] =
          grid.eiw[eiwIndex(ic1 - 1, 1, grid.mc)];
    }
  }

  return Result<CircleGrid>::ok(std::move(grid));
}

// Fortran: SUBROUTINE PIQSUM
Status piqSum(const CircleGrid& grid, CircleCoefficients& coeff) {
  if (grid.nc <= 0 || grid.mc < 0) {
    return Status::fail(Error::make(ErrorCode::invalidState,
                                    "xfoil::core::inverse::piqSum: invalid "
                                    "circle grid"));
  }
  if (static_cast<int>(coeff.cn.size()) != grid.mc + 1 ||
      static_cast<int>(coeff.piq.size()) != grid.nc) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "xfoil::core::inverse::piqSum: CN/PIQ-"
                                    "Sizes fit not to the Circle grid"));
  }

  // Fortran: DO 300 IC=1,NC ; ZSUM=(0,0) ; DO 310 M=0,MC ;
  // ZSUM = ZSUM + CN(M)*CONJG(EIW(IC,M)) ; PIQ(IC)=ZSUM
  for (int ic = 1; ic <= grid.nc; ++ic) {
    std::complex<double> zsum(0.0, 0.0);
    for (int m = 0; m <= grid.mc; ++m) {
      zsum += coeff.cn[static_cast<std::size_t>(m)] *
              std::conj(grid.eiw[eiwIndex(ic - 1, m, grid.mc)]);
    }
    coeff.piq[static_cast<std::size_t>(ic - 1)] = zsum;
  }
  return Status::ok();
}

// Fortran: SUBROUTINE FTP
Status fourierTransform(const CircleGrid& grid, CircleCoefficients& coeff) {
  if (grid.nc <= 0 || grid.mc < 0) {
    return Status::fail(Error::make(ErrorCode::invalidState,
                                    "xfoil::core::inverse::fourierTransform: "
                                    "invalid circle grid"));
  }
  if (static_cast<int>(coeff.cn.size()) != grid.mc + 1 ||
      static_cast<int>(coeff.piq.size()) != grid.nc) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "xfoil::core::inverse::fourierTransform: "
                                    "CN/PIQ-Sizes fit not to the "
                                    "Circle grid"));
  }

  const double scale = grid.dwc / grid.pi;

  // Fortran: DO 200 M=0,MC ; ZSUM=(0,0) ; DO 210 IC=2,NC-1 ;
  // ZSUM = ZSUM + PIQ(IC)*EIW(IC,M)
  // CN(M) = (0.5*(PIQ(1)*EIW(1,M)+PIQ(NC)*EIW(NC,M)) + ZSUM)*DWC/PI
  for (int m = 0; m <= grid.mc; ++m) {
    std::complex<double> zsum(0.0, 0.0);
    for (int ic = 2; ic <= grid.nc - 1; ++ic) {
      zsum += coeff.piq[static_cast<std::size_t>(ic - 1)] *
              grid.eiw[eiwIndex(ic - 1, m, grid.mc)];
    }
    std::complex<double> ends =
        0.5 * (coeff.piq[0] * grid.eiw[eiwIndex(0, m, grid.mc)] +
               coeff.piq[static_cast<std::size_t>(grid.nc - 1)] *
                   grid.eiw[eiwIndex(grid.nc - 1, m, grid.mc)]);
    coeff.cn[static_cast<std::size_t>(m)] = (ends + zsum) * scale;
  }

  // Fortran: CN(0) = 0.5*CN(0)
  coeff.cn[0] = 0.5 * coeff.cn[0];
  return Status::ok();
}

// Fortran: SUBROUTINE CNFILT(FFILT)
void coeffFilter(CircleCoefficients& coeff, double ffilt) {
  // Fortran: IF(FFILT.EQ.0.0) RETURN
  if (ffilt == 0.0) {
    return;
  }
  const int mc = static_cast<int>(coeff.cn.size()) - 1;
  if (mc < 0) {
    return;
  }
  const double pi = fortranPi();
  // Fortran: DO 10 M=0,MC ; FREQ=FLOAT(M)/FLOAT(MC) ;
  // CWT = 0.5*(1.0+COS(PI*FREQ)) ; CWTX=CWT ;
  // IF(FFILT.GT.0.0) CWTX=CWT**FFILT ; CN(M)=CN(M)*CWTX
  for (int m = 0; m <= mc; ++m) {
    const double freq = static_cast<double>(m) / static_cast<double>(mc);
    const double cwt = 0.5 * (1.0 + std::cos(pi * freq));
    double cwtx = cwt;
    if (ffilt > 0.0) {
      cwtx = std::pow(cwt, ffilt);
    }
    coeff.cn[static_cast<std::size_t>(m)] *= cwtx;
  }
}

// Fortran: SUBROUTINE CNSYMM
void coeffSymmetrize(CircleCoefficients& coeff) {
  const int mc = static_cast<int>(coeff.cn.size()) - 1;
  // Fortran: DO 10 M=1,MC ; CN(M) = CMPLX(REAL(CN(M)), 0.0)
  for (int m = 1; m <= mc; ++m) {
    coeff.cn[static_cast<std::size_t>(m)] =
        std::complex<double>(coeff.cn[static_cast<std::size_t>(m)].real(), 0.0);
  }
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/CircleKernels.cpp

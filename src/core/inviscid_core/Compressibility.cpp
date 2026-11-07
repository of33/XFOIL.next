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

// File: src/core/inviscid_core/Compressibility.cpp
// Compressibility kernels of the new 0-based core, directly
// ported from fortran-src/xfoil.f (MRCL, COMSET, CPCALC).
//
// Formulas, operation order and clamps are adopted exactly from
// Fortran:
// - MRCL: CLA = MAX(CLS, 1e-6); MATYP=1/3: MINF=MINF1, M_CLS=0;
// MATYP=2: MINF=MINF1/SQRT(CLA), M_CLS=-0.5*MINF/CLA; RETYP=1:
// REINF=REINF1, R_CLS=0; RETYP=2: REINF=REINF1/SQRT(CLA),
// R_CLS=-0.5*REINF/CLA; RETYP=3: REINF=REINF1/CLA, R_CLS=-REINF/CLA;
// MINF >= 0.99 -> 0.99 + M_CLS=0; RRAT=REINF/REINF1 (REINF1>0),
// RRAT > 100 -> REINF=REINF1*100, R_CLS=0.
// - COMSET: BETA=SQRT(1-MINF^2), BETA_MSQ=-0.5/BETA;
// TKLAM=MINF^2/(1+BETA)^2, TK_MSQ=1/(1+BETA)^2 - 2*TKLAM/(1+BETA)*BETA_MSQ;
// MINF==0 -> CPSTAR=-999, QSTAR=999, otherwise canonical sound-speed formulas.
// - CPCALC: BETA=SQRT(1-MINF^2), BFAC=0.5*MINF^2/(1+BETA);
// CPINC=1-(Q/QINF)^2, DEN=BETA+BFAC*CPINC, Cp=CPINC/DEN; DEN<=0 ->
// structured error state (Fortran writes a message and sets a flag).

#include "xfoil/core/inviscid/Compressibility.h"

#include <cmath>
#include <utility>
#include <vector>

namespace xfoil::core::inviscid {

// Fortran: SUBROUTINE MRCL(CLS,M_CLS,R_CLS).
Result<MachReynoldsResult> mrcl(const MachReynoldsSettings& settings,
                                double cls) {
  // Fortran: RETYP/MATYP illegal -> set to 1 (COMMON fix; Fortran
  // sets the session state, here local application of the same values).
  const int reType =
      (settings.reType >= 1 && settings.reType <= 3) ? settings.reType : 1;
  const int machType =
      (settings.machType >= 1 && settings.machType <= 3) ? settings.machType
                                                         : 1;

  // Fortran: CLA = MAX(CLS, 1e-6).
  const double cla = std::max(cls, 0.000001);

  MachReynoldsResult result;

  // Fortran: Mach(Cl)-Dependency.
  if (machType == 1) {
    result.minf = settings.minf1;
    result.mCls = 0.0;
  } else if (machType == 2) {
    result.minf = settings.minf1 / std::sqrt(cla);
    result.mCls = -0.5 * result.minf / cla;
  } else if (machType == 3) {
    result.minf = settings.minf1;
    result.mCls = 0.0;
  }

  // Fortran: Re(Cl)-Dependency.
  if (reType == 1) {
    result.reinf = settings.reinf1;
    result.rCls = 0.0;
  } else if (reType == 2) {
    result.reinf = settings.reinf1 / std::sqrt(cla);
    result.rCls = -0.5 * result.reinf / cla;
  } else if (reType == 3) {
    result.reinf = settings.reinf1 / cla;
    result.rCls = -result.reinf / cla;
  }

  // Fortran: MINF >= 0.99 -> artificial Limit on 0.99.
  if (result.minf >= 0.99) {
    result.minf = 0.99;
    result.mCls = 0.0;
    result.machLimited = true;
  }

  // Fortran: RRAT = REINF/REINF1; RRAT > 100 -> Limit.
  double rrat = 1.0;
  if (settings.reinf1 > 0.0) {
    rrat = result.reinf / settings.reinf1;
  }
  if (rrat > 100.0) {
    result.reinf = settings.reinf1 * 100.0;
    result.rCls = 0.0;
    result.reLimited = true;
  }

  return Result<MachReynoldsResult>::ok(result);
}

// Fortran: SUBROUTINE COMSET.
CompressibilityState comset(double minf, double qinf) {
  CompressibilityState state;

  // Fortran: Karman-Tsien parameter.
  state.beta = std::sqrt(1.0 - minf * minf);
  state.betaMsq = -0.5 / state.beta;
  state.tklam = minf * minf / ((1.0 + state.beta) * (1.0 + state.beta));
  state.tkMsq = 1.0 / ((1.0 + state.beta) * (1.0 + state.beta)) -
                2.0 * state.tklam / (1.0 + state.beta) * state.betaMsq;

  // Fortran: sound-pressure coefficient and speed of sound.
  if (minf == 0.0) {
    state.cpstar = -999.0;
    state.qstar = 999.0;
  } else {
    state.cpstar = 2.0 / (kGamma * minf * minf) *
                   (std::pow((1.0 + 0.5 * kGamm1 * minf * minf) /
                                 (1.0 + 0.5 * kGamm1),
                             kGamma / kGamm1) -
                    1.0);
    state.qstar = qinf / minf * std::sqrt((1.0 + 0.5 * kGamm1 * minf * minf) /
                                          (1.0 + 0.5 * kGamm1));
  }

  return state;
}

// Fortran: SUBROUTINE CPCALC(N,Q,QINF,MINF,CP).
Result<std::vector<double>> cpCalc(std::span<const double> q, double qinf,
                                   double minf) {
  // Fortran: BETA and BFAC.
  const double beta = std::sqrt(1.0 - minf * minf);
  const double bfac = 0.5 * minf * minf / (1.0 + beta);

  std::vector<double> cp(q.size(), 0.0);
  bool denNeg = false;

  // Fortran: Loop I=1..N (0-based i0 = I-1).
  for (std::size_t i = 0; i < q.size(); ++i) {
    const double cpInc = 1.0 - (q[i] / qinf) * (q[i] / qinf);
    const double den = beta + bfac * cpInc;
    cp[i] = cpInc / den;
    if (den <= 0.0) {
      denNeg = true;
    }
  }

  // Fortran: DENNEG -> message (continuation); structured error state.
  if (denNeg) {
    return Result<std::vector<double>>::fail(Error::make(
        ErrorCode::numericalFailure,
        "CPCALC: Local speed too large. Compressibility corrections "
        "invalid."));
  }

  return Result<std::vector<double>>::ok(std::move(cp));
}

}  // namespace xfoil::core::inviscid

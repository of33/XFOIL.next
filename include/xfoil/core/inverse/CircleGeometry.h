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

// File: include/xfoil/core/inverse/CircleGeometry.h
// Mapping/geometry kernels of the circle mapping of the new 0-based cores
// directly ported from fortran-src/xmdes.f (SCINIT, CNCALC,
// ZCCALC, ZCNORM, ZLEFIND).
//
// The state CircleMappingState bundles the circle grid (CircleGrid), the
// coefficients (CircleCoefficients) and the geometric circle state
// (z(w), dz/dCn, s(w), LE/TE quantities) as separated, value-semantic
// parts. No Fortran EQUIVALENCE, no global state,
// no raw pointers, no 1-based containers.
//
// Internal computations use double; the canonical Fortran values are
// REAL(4). Iteration limits and convergence criteria (SCINIT: 30 passes /
// SEPS=5e-7, TE gap 5 / CEPS=1e-7; ZLEFIND: 10 / 1e-5) are
// adopted.

#pragma once

#include <complex>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inverse/CircleKernels.h"

namespace xfoil::core::inverse {

// Fortran ZC_CN(ICX, IMX/4): IMX/4 = 64/4 = 16 Columns.
inline constexpr int kZcCnCols = 16;

// Fortran ZLEFIND: parameter (NTX=33) for the local LE spline window.
inline constexpr int kZleNtx = 33;

// Fortran COMMON/CIRCLE.INC/: complete circle-mapping state.
struct CircleMappingState {
  CircleGrid grid;             // NC, MC, MCT, PI, DWC, WC, EIW
  CircleCoefficients coeff;    // CN(0..MC), PIQ(1..NC)

  // COMMON/CPR01/
  double agte{0.0};
  double ag0{0.0};
  double qim0{0.0};
  double qimold{0.0};
  std::vector<double> sc;      // SC(1..NC)
  std::vector<double> scold;   // SCOLD(1..NC)
  std::vector<double> xcold;   // XCOLD(1..NC)
  std::vector<double> ycold;   // YCOLD(1..NC)

  // COMMON/CPC01/
  std::complex<double> dzte{0.0, 0.0};
  std::complex<double> chordz{0.0, 0.0};
  std::complex<double> zleold{0.0, 0.0};
  std::vector<std::complex<double>> zcoldw;  // ZCOLDW(1..NC)
  std::vector<std::complex<double>> zc;      // ZC(1..NC)
  std::vector<std::complex<double>> zc_cn;   // ZC_CN(1..NC, 1..kZcCnCols)
};

// Initializes the geometry-/coefficient-dependent arrays to the
// Fortran COMMON start values (NC/MC/MCT from the grid).
void initState(CircleMappingState& state);

// Fortran: SUBROUTINE DZTSET(RINPUT,NINPUT) (non-interactive branch).
// Sets the TE gap DZTE = (dx/c, dy/c).
void setTeGap(CircleMappingState& state, double dxnew, double dynew);

// Fortran: SUBROUTINE AGTSET(RINPUT,NINPUT) (non-interactive branch).
// Sets the TE angle AGTE from degrees (AGTE = AGTED/180).
void setTeAngleDeg(CircleMappingState& state, double agted);

// Fortran: SUBROUTINE ZCCALC(MTEST)
// Integrates z(w) from PIQ and sets the sensitivities dz/dCn and s(w).
Status zcCalc(CircleMappingState& state, int mtest);

// Fortran: SUBROUTINE ZLEFIND(ZLE,ZC,WC,NC,PIQ,AGTE)
// Returns the leading edge as a complex point.
// Optional lefindFailed: Fortran writes 'ZLEFIND: LE location failed.'
// if the LE search did not converge and returns to the starting point
// (no error). The flag reports exactly this.
std::complex<double> zleFind(const CircleMappingState& state,
                             bool* lefindFailed = nullptr);

// Fortran: SUBROUTINE ZCNORM(MTEST)
// Normalizes z(w) to the old chord/LE position and recomputes dz/dCn;
// rotates CN(0). Calls ZLEFIND. Optional lefindFailed:
// ZLEFIND warning event of this ZCNORM call (see ZLEFIND).
Status zcNorm(CircleMappingState& state, int mtest,
              bool* lefindFailed = nullptr);

// Fortran: SUBROUTINE CNCALC(QC,LSYMM)
// Determines CN from the circle velocity QC(w); LSYMM doubles the
// real parts and zeroes the imaginary parts.
Status calcCoefficients(CircleMappingState& state,
                        std::span<const double> qc, bool lsymm);

// Fortran: SUBROUTINE SCINIT(N,X,XP,Y,YP,S,SLE)
// Initializes s(w) and the mapping coefficients from the airfoil geometry.
// x/xp/y/yp/s are the splined profile contour (n entries), sle the
// arc length of the leading edge. Requires a circle grid previously set
// via setCircleGrid.
// Optional diag: structured diagnostic values at the canonical
// execution points (format-1100 TE-gap values, IPASS/max(dw) per
// loop pass, convergence status). No stdout output in the kernel.
struct ScinitIterationRecord {
  int pass{0};          // Fortran IPASS (1-based)
  double maxDelta{0.0};  // Fortran DSCMAX
};
struct ScinitDiagnostics {
  double dxte{0.0};        // Fortran DXTE (REAL(DZTE))
  double dyte{0.0};        // Fortran DYTE (IMAG(DZTE))
  double teAngleDeg{0.0};  // Fortran AGTE*180.0
  std::vector<ScinitIterationRecord> iterations;
  bool converged{false};   // DSCMAX < SEPS reached
};
Status initMappingCoordinate(CircleMappingState& state,
                             std::span<const double> x,
                             std::span<const double> xp,
                             std::span<const double> y,
                             std::span<const double> yp,
                             std::span<const double> s, double sle,
                             ScinitDiagnostics* diag = nullptr);

// Fortran: SUBROUTINE QCCALC(ISPEC,ALFA,CL,CM,MINF,QINF,NCIR,XCIR,YCIR,SCIR,QCIR)
// Surface velocity and complex coordinates from PIQ/CN(0). ISPEC=1:
// Alfa given (one pass); ISPEC=2: Cl given (Newton, max 10,
// AEPS=5e-7). Karman-Tsien Cp. Does not change the circle state.
struct CircleSpeedResult {
  double alfa{0.0};
  double cl{0.0};
  double cm{0.0};
  std::vector<double> qcir;   // QCIR(1..NC)
  std::vector<double> xcir;   // XCIR(1..NC)
  std::vector<double> ycir;   // YCIR(1..NC)
  std::vector<double> scir;   // SCIR(1..NC)
};
Result<CircleSpeedResult> qCCalc(const CircleMappingState& state, int ispec,
                                 double alfa, double clTarget, double minf,
                                 double qinf);

// Fortran: SUBROUTINE QSPINT(ALQSP,QSPEC,QINF,MINF,CLQSP,CMQSP)
// Integrates the circle-plane surface pressure (QSPEC) over the old
// contour (XCOLD/YCOLD) into Cl/CM.
struct SpeedIntegrateResult {
  double cl{0.0};
  double cm{0.0};
};
Result<SpeedIntegrateResult> qspIntegrate(const CircleMappingState& state,
                                          double alqsp,
                                          std::span<const double> qspec,
                                          double qinf, double minf);

// Fortran: SUBROUTINE MAPGEN(FFILT,N,X,Y)
// Produces the geometry z(w) from CN while enforcing the TE-gap
// constraint (Newton via CN, CGAUSS, max 10, DCNMAX<=5e-5). Changes
// CN/PIQ/ZC/ZC_CN/SC in the state and returns the new contour X,Y (N=NC).
// Optional diag: structured iteration diagnostics (ITERCN/DCNMAX per
// pass, convergence status DCNMAX<=5e-5) of the same run that produces
// the contour. No stdout output in the kernel.
struct MapgenIterationRecord {
  int iteration{0};    // Fortran ITERCN (1-based)
  double dcnMax{0.0};  // Fortran DCNMAX
  // ZLEFIND warning ('ZLEFIND: LE location failed.') was written in
  // ZCNORM during this iteration, i.e. before this line.
  bool lefindFailed{false};
};
struct MapgenDiagnostics {
  std::vector<MapgenIterationRecord> iterations;
  bool converged{false};
  // ZLEFIND warning from ZCNORM before the Newton loop (before the
  // first iteration row).
  bool preLoopLefindFailed{false};
};
Status mapGenerate(CircleMappingState& state, double ffilt,
                   std::vector<double>& x, std::vector<double>& y,
                   MapgenDiagnostics* diag = nullptr);

// Fortran: SUBROUTINE PERT(QSPEC) — numerical sub-kernel.
// PERT (xmdes.f 1655) = CNCALC(QSPEC) + rotation offset +
// CN(M) += (dCnr,dCni) (interactive read) + TE-gap/chord
// Newton (NCN=1; sources show numerical identity with the
// MAPGEN Newton, since MAPGEN's identity pre-fill of QQ/DCN
// is overwritten by the TE-gap block before CGAUSS). The
// prompt-free numerical part is exactly the
// single-coefficient perturbation; the geometry/gap path is
// mapGenerate (Fortran-confirmed, including
// iteration records and ZLEFIND warning flag). The READ loop and
// the WRITE diagnostics live in the session/CLI.
// PERT checks 1<=M<=NC; the stored coefficients suffice up to MC.
// A mode outside 1..MC is treated as a controlled error
// (the Fortran READ guard M.GT.NC is looser and would be
// limited by the stored coefficients).
[[nodiscard]] Status perturbCoefficient(CircleMappingState& state, int m,
                                        double dcnr, double dcni);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/CircleGeometry.h

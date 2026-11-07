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

// File: include/xfoil/core/inviscid/InviscidTypes.h
// Value types of the inviscid solvers of the new 0-based cores,
// ported from fortran-src/xfoil.f (MRCL, COMSET, CPCALC,
// CLCALC, TECALC), fortran-src/xpanel.f (PSILIN, PSWLIN, GGCALC, QWCALC,
// QDCALC, STFIND, QISET, XYWAKE) and fortran-src/xoper.f (SPECAL, SPECCL,
// MHINGE).
//
// Separated, minimal state and result objects:
// - MachReynoldsSettings: MRCL inputs (session MATYP/RETYP/MINF1/REINF1)
// - MachReynoldsResult: MRCL exit incl. limiting diagnostics
// - CompressibilityState: COMSET exit (Karman-Tsien, sonic limits)
// - LoadResult: CLCALC exit (CL, CM, CDP, CL_ALF, CL_MSQ)
// - StagnationPointResult: STFIND exit
// - TrailingEdgeState: TECALC exit (geometry + SIGTE/GAMTE)
// - HingeMomentResult: MHINGE exit
// - PsiLinResult / PswLinResult: PSILIN/PSWLIN exit
// - InfluenceSystem: GGCALC exit (AIJ/BIJ/Pivot, GAMU/QINVU basis)
// - wake state: wake node state (input for qwCalc/qdCalc)
//
// Consistently 0-based, no raw pointers, no public mutable
// solver arrays, no global state.
// Controlled errors via Result<T>; diagnostics as structured values.

#pragma once

#include <cstddef>
#include <vector>

namespace xfoil::core::inviscid {

// Fortran: MRCL inputs (session MATYP, RETYP, MINF1, REINF1).
// machType/reType in {1, 2, 3}; other values are rejected in a
// controlled way (Fortran sets them to 1 with a message, session state).
struct MachReynoldsSettings {
  int machType{1};      // Fortran MATYP
  int reType{1};        // Fortran RETYP
  double minf1{0.0};    // Fortran MINF1 (unit-Cl Mach)
  double reinf1{0.0};   // Fortran REINF1 (unit-CL Reynolds)
};

// Fortran: MRCL exit (MINF, REINF, M_CLS, R_CLS) plus structured
// limiting diagnostics (Fortran WRITEs are not written to stdout).
struct MachReynoldsResult {
  double minf{0.0};      // Fortran MINF
  double reinf{0.0};     // Fortran REINF
  double mCls{0.0};      // Fortran M_CLS (dM/dCL)
  double rCls{0.0};      // Fortran R_CLS (dRe/dCL)
  bool machLimited{false};  // true: MINF was limited to 0.99
  bool reLimited{false};    // true: REINF was limited to REINF1*100
};

// Fortran: COMSET exit (Karman-Tsien parameter and sonic limits).
struct CompressibilityState {
  double beta{1.0};       // SQRT(1 - MINF^2)
  double betaMsq{0.0};    // -0.5/BETA
  double tklam{0.0};      // Karman-Tsien parameter
  double tkMsq{0.0};      // dTKLAM/d(MINF^2)
  double cpstar{0.0};     // sonic pressure coefficient (-999 for MINF=0)
  double qstar{0.0};      // sonic speed (999 for MINF=0)
};

// Fortran: CLCALC exit (CL, CM, CDP, dCL/dAlpha, dCL/d(MINF^2)).
// centerOfPressureX is the XCP diagnostic quantity (no canonical
// Fortran field): added in an algebraic loop term
// xcp += dx*AG*(X(IP)+X(I))/2, then /Cl (Cl==0 -> 0.0), built from
// terms already computed in the CLCALC loop (dx, AG, X).
// Bit-identical to the previous public API path (proof:
// tests/core/XcpDerivationTests.cpp); the remaining outputs remain
// unchanged.
struct LoadResult {
  double cl{0.0};
  double cm{0.0};
  double cdp{0.0};
  double clAlf{0.0};   // Fortran CL_ALF
  double clMsq{0.0};   // Fortran CL_MSQ
  double centerOfPressureX{0.0};  // XCP diagnostics (no canonical Fortran field)
};

// Fortran: STFIND exit (stagnation-point arc length, panel index, derivatives).
struct StagnationPointResult {
  double sst{0.0};
  std::size_t ist{0};    // Fortran Is (1-based) -> 0-based
  double sstGo{0.0};
  double sstGp{0.0};
  bool found{true};      // false: not found (Fortran I = N/2, message)
};

// Fortran: TECALC exit (complete: geometry part + SIGTE/GAMTE).
struct TrailingEdgeState {
  bool sharp{false};
  double dste{0.0};
  double ante{0.0};
  double aste{0.0};
  double scs{0.0};
  double sds{0.0};
  double sigte{0.0};     // TE-panel source strength
  double gamte{0.0};     // TE panel vortex strength
  double sigteA{0.0};    // dSIGTE/dAlpha
  double gamteA{0.0};    // dGAMTE/dAlpha
};

// Fortran: MHINGE exit (hinge moment and hinge forces).
struct HingeMomentResult {
  double hmom{0.0};
  double hfx{0.0};
  double hfy{0.0};
};

// Fortran: PSILIN exit (stream function incl. sensitivity vectors).
// dzdg/dqdg: n entries (dPsi/dGam, dQtan/dGam at the airfoil).
// dzdm/dqdm: n entries (dPsi/dm, dQtan/dm at the airfoil; wake part in
// pswLin).
struct PsiLinResult {
  double psi{0.0};
  double psiNi{0.0};
  double qtan1{0.0};    // dQtan/dGam, basis alpha=0
  double qtan2{0.0};    // dQtan/dGam, basis alpha=90
  std::vector<double> dzdg;
  std::vector<double> dqdg;
  std::vector<double> dzdm;
  std::vector<double> dqdm;
  std::vector<double> dzdn;     // dPsi/dn (GEOLIN=true), n entries
  double zQinf{0.0};    // dPsi/dQinf
  double zAlfa{0.0};    // dPsi/dAlpha
  double zQdof0{0.0};   // dPsi/dQDOF0 (GEOLIN=true)
  double zQdof1{0.0};   // dPsi/dQDOF1 (GEOLIN=true)
  double zQdof2{0.0};   // dPsi/dQDOF2 (GEOLIN=true)
  double zQdof3{0.0};   // dPsi/dQDOF3 (GEOLIN=true)
};

// Fortran: PSWLIN exit (wake-source stream function incl. sensitivities).
// dzdm/dqdm: nw entries (wake node N+1..N+NW).
struct PswLinResult {
  double psi{0.0};
  double psiNi{0.0};
  std::vector<double> dzdm;
  std::vector<double> dqdm;
};

// Fortran: GGCALC exit (influence system with base solutions).
// aij/bij: (n+1)*(n+1) row-major (Fortran AIJ(IQX,IQX), BIJ(IQX,IZX),
// J=1..N+1); the LU factorization replaced aij in-place, aijpiv holds the
// pivot rows (Fortran AIJPIV, 0-based).
struct InfluenceSystem {
  std::size_t n{0};
  std::vector<double> aij;      // (n+1)*(n+1) row-major, LU-factorized
  std::vector<int> aijpiv;      // n+1 (0-based)
  std::vector<double> bij;      // (n+1)*(n+1) row-major
  std::vector<double> gamu1;    // n+1 (basis alpha=0)
  std::vector<double> gamu2;    // n+1 (basis alpha=90)
  std::vector<double> qinvu1;   // n+1
  std::vector<double> qinvu2;   // n+1
};

// Wake-node state (input for qwCalc/qdCalc; XYWAKE produces it from
// GAM and waklen). x/y/nx/ny/apanel: nw entries (wake node N+1..N+NW,
// 0-based).
struct WakeState {
  std::size_t nw{0};
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> nx;
  std::vector<double> ny;
  std::vector<double> apanel;
  std::vector<double> s;        // Wake arc lengths
};

}  // namespace xfoil::core::inviscid

// End: include/xfoil/core/inviscid/InviscidTypes.h

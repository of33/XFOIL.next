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

// File: include/xfoil/core/export_data/ExportData.h
// Structured, file-free export data projections.
//
// Ports the deterministic data projections from
// fortran-src/xoper.f (BLDUMP, BLDUMP2, CPDUMP)
// fortran-src/xmdes.f (CNDUMP, GETVOV resampling kernel)
//
// The projections duplicate no solver formulas: HKIN, COMSET-like
// compressibility, and the validated kernels are reused unchanged.
// File, path, prompt, and format parts live in
// xfoil::io.

#pragma once

#include <cstddef>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/inverse/QSpecKernels.h"

namespace xfoil::core::export_data {

// ---
// BLDUMP (xoper.f:1892) - boundary-layer export line.
// ---
// Canonical columns (KDELIM=0 format 8500, 12 arguments):
// s, x, y, Ue/Vinf, Dstar, Theta, Cf, Hk, H*, Theta*Ue^2, Dstar*Ue,
// Tstar*Ue^3
struct BoundaryLayerDumpRecord {
  double s{0.0};
  double x{0.0};
  double y{0.0};
  double ue{0.0};
  double dstar{0.0};
  double theta{0.0};
  double cf{0.0};
  double hk{0.0};
  double hs{0.0};
  double thetaUe2{0.0};
  double dstarUe{0.0};
  double tstarUe3{0.0};
};

// Input of a profile node (I=1..N) or of a wake point.
struct BoundaryLayerDumpInput {
  double s{0.0};
  double x{0.0};
  double y{0.0};
  double gam{0.0};        // GAM(I) (airfoil) or UEDG (wake)
  bool viscousStation{false};  // LIPAN .AND. LVISC
  double dstar{0.0};      // DSTR(IBL,IS)
  double theta{0.0};      // THET(IBL,IS)
  double tstar{0.0};      // TSTR(IBL,IS)
  double tau{0.0};        // TAU(IBL,IS)
};

// Freestream quantities for the UE transformation (COMSET derivation).
struct DumpFlowState {
  double qinf{0.0};
  double minf{0.0};
  double tklam{0.0};
};

// UE transformation, Mach number squared, HKIN, H=DSTR/THET, HS, CF,
// point values according to BLDUMP. Wake lines use viscousStation=true with
// gam=UEDG and cf=0 (canonical).
[[nodiscard]] BoundaryLayerDumpRecord boundaryLayerDumpRow(
    const BoundaryLayerDumpInput& in, const DumpFlowState& flow);

// ---
// BLDUMP2 (xoper.f:2053) - extended boundary-layer export line (24 columns).
// ---
struct BoundaryLayerExtendedDumpRecord {
  double xs{0.0};
  double x{0.0};
  double y{0.0};
  double ue{0.0};
  double dstar{0.0};
  double theta{0.0};
  double tstar{0.0};
  double cf{0.0};
  double cdis{0.0};
  double hk{0.0};
  double hs{0.0};
  double dstarUe{0.0};
  double thetaUe2{0.0};
  double ptai{0.0};
  double ppei{0.0};
  double halfTstarUe3{0.0};
  double tau{0.0};
  double dis{0.0};
  double ueDstarDuds{0.0};   // -UE*DS*DUDS
  double tauMinus{0.0};      // TAU - UE*DS*DUDS
  double dv{0.0};            // TH*UE^2 - DPREI*DK
  double dmds{0.0};
  double ctau{0.0};
  double reTheta{0.0};
};

// One airfoil side with stations IBL=1..NBL (0-based).
struct BlSideStations {
  std::vector<double> xssi;  // XSSI(IBL)
  std::vector<double> uedg;  // UEDG(IBL)
  std::vector<double> dstr;  // DSTR(IBL)
  std::vector<double> thet;  // THET(IBL)
  std::vector<double> tstr;  // TSTR(IBL)
  std::vector<double> tau;   // TAU(IBL)
  std::vector<double> dis;   // DIS(IBL)
  std::vector<double> ctau;  // CTAU(IBL)
  std::vector<double> x;     // X(IPAN(IBL))
  std::vector<double> y;     // Y(IPAN(IBL))
  std::vector<double> gam;   // GAM(IPAN(IBL)) for inviscid UE
  std::size_t iblte{0};      // IBLTE(IS) (0-based)
  bool viscous{false};       // LIPAN .AND. LVISC
};

struct BlExtendedDumpInput {
  BlSideStations top;     // IS=1
  BlSideStations bottom;  // IS=2
  DumpFlowState flow{};
  double reinf{0.0};
};

struct BlExtendedDumpRows {
  std::vector<BoundaryLayerExtendedDumpRecord> topRows;     // IS=1
  std::vector<BoundaryLayerExtendedDumpRecord> bottomRows;  // IS=2
};

// Trapezoidal integration PTAU/EDIS, XS normalization, IBLM/IBLP clamping,
// and 24-column projection according to BLDUMP2.
[[nodiscard]] Result<BlExtendedDumpRows> boundaryLayerExtendedDumpRows(
    const BlExtendedDumpInput& in);

// ---
// CPDUMP (xoper.f:2265) - Cp export line.
// ---
struct PressureDumpRecord {
  double x{0.0};
  double cpCom{0.0};
};

// CPINC=1-(GAM/QINF)^2, The=BETA+BFAC*CPINC, CPCOM=CPINC/The
// (operation structure identical to the validated kernel cpCalc).
[[nodiscard]] PressureDumpRecord pressureDumpRow(double x, double gam,
                                                 const DumpFlowState& flow);

// ---
// CNDUMP (xmdes.f:1747) - Fourier-coefficient export line.
// ---
struct CircleCoefficientDumpRecord {
  int m{0};
  double cnReal{0.0};
  double cnImag{0.0};
  double piqReal{0.0};
  double piqImag{0.0};
};

// cnRe/cnIm count mc+1, piqRe/piqIm count nc. For m > mc, CN
// values are canonically filled with 0.0; PIQ is always output.
[[nodiscard]] Result<std::vector<CircleCoefficientDumpRecord>>
circleCoefficientDumpRows(const std::vector<double>& cnRe,
                          const std::vector<double>& cnIm,
                          const std::vector<double>& piqRe,
                          const std::vector<double>& piqIm, int mc, int nc);

// ---
// GETVOV (xmdes.f:1769) - (s, V/Vinf) table and resampling.
// ---
struct VelocityRatioTable {
  std::vector<double> s;   // arc lengths (input, before normalization)
  std::vector<double> vov;  // V/Vinf
};

// Deterministic resampling kernel: normalization
// (s -> 1-(s-SSTART)/SSPAN), SORT (pair removal), SPLIND(-999,-999),
// SEVAL on sspec, QINCOM, SPLQSP. Uses exclusively the
// validated core kernels. distribution corresponds to QSPEC(:,KQSP):
// s = SSPEC, q is updated and spline-interpolated.
[[nodiscard]] Status resampleVelocityRatioTable(
    const VelocityRatioTable& table, double qinf, double tklam,
    inverse::QSpecDistribution& distribution);

// ---
// PLXADD (xpol.f:652) - domain projection of one dump point.
// ---
// Re here is data-flow only: no files, record markers, endianness,
// session flags or prompts. The record layout/byte encoding lives in
// xfoil::io (DumpCodec).
//
// Vectors follow the established export_data convention: the Fortran index
// IBL=1..NBL(IS) corresponds to index 1..NBL(IS); index 0 is unused.
// PLXADD reads IPAN/THET/DSTR/CTAU/TAU up to NSIDE(IS); NSIDE(1) =
// IBLTE(1) + (NBL(2)-IBLTE(2)) can exceed NBL(1). IBLPAN
// (xpanel.f) writes the full read range of IPAN: loop 10
// (1..IBLTE(1)) and loop 35 (IBLTE(1)+1..IBLTE(1)+NW) copy the wake
// pointers of the lower side into the upper side. Entry
// nbl+1..nside(IS) therefore carries the canonically written wake
// pointer; a station field value there that was never written
// (THET/DSTR/CTAU/TAU beyond NBL(IS)) stays as a COMMON zero.
//
// NBL(IS)=0/IBLTE(IS)=0 is the canonical COMMON initial state of an
// inviscid session point (IBLPAN is the only producer and runs
// only viscously). PLXADD then writes NSIDE=(2,2), IBLTE=(0,0) and reads
// the never-written station entries as COMMON zero; entries
// beyond the provided vector length count as never written.
// NBL(IS)=1 is not a canonical state and is rejected.
struct PlxaddSideInputs {
  std::vector<int> ipan;     // IPAN(IBL,IS): 1..N+NW valid, 0 = COMMON zero
  std::vector<double> thet;  // THET(IBL,IS)
  std::vector<double> dstr;  // DSTR(IBL,IS)
  std::vector<double> ctau;  // CTAU(IBL,IS)
  std::vector<double> tau;   // TAU(IBL,IS)
  std::size_t nbl{0};        // NBL(IS): 0 = COMMON start, otherwise >= 2
  std::size_t iblte{0};      // IBLTE(IS): 0 = COMMON start, otherwise >= 2
};

struct PlxaddInputs {
  bool viscous{false};            // LVISC (pure data decision)
  double minf{0.0};               // MINF
  double qinf{0.0};               // QINF (QUE = 0.5*QINF^2)
  double alfaRad{0.0};            // PLXADD writes Alfa/DTOR (degrees; conversion is a session concern)
  double cl{0.0};
  double cd{0.0};
  double cm{0.0};
  double xoctrTop{0.0};           // XOCTR(1)
  double xoctrBottom{0.0};        // XOCTR(2)
  double sst{0.0};                // SST (stagnation arc position)
  // X/XP/S/CPV follow the Fortran COMMON field: indices 1..N are the
  // contour, N+1..N+NW the wake nodes written by XYWAKE/CPCALC.
  std::vector<double> x;          // X(1..N+NW)
  std::vector<double> xp;         // XP(1..N+NW) (SEGSPL derivative, wake unused)
  std::vector<double> s;          // S(1..N+NW)
  std::vector<double> cpv;        // CPV(1..N+NW)
  // Fortran N (contour node count, CI05). SEVAL(SST,...) runs up to N;
 // IPAN values may reach N+NW. 0 means: N = x.size
  // (wake-free inputs, backward compatibility).
  std::size_t contourCount{0};
  PlxaddSideInputs sides[2];      // [0]=IS=1 (top), [1]=IS=2 (bottom)
};

struct PlxaddStation {
  double xx{0.0};
  double cp{0.0};
  double thet{0.0};
  double dstr{0.0};
  double cf{0.0};
  double ctau{0.0};
};

struct PlxaddPoint {
  double alfaRad{0.0};
  double cl{0.0};
  double cdTot{0.0};
  double zero{0.0};
  double cm{0.0};
  double xtTop{0.0};
  double xtBottom{0.0};
  int nside1{0};
  int nside2{0};
  int iblte1{0};
  int iblte2{0};
  std::vector<PlxaddStation> top;     // IS=1, IBL=1..NSIDE(1)
  std::vector<PlxaddStation> bottom;  // IS=2, IBL=1..NSIDE(2)
};

// Projection exact after PLXADD: BETA/BFAC, stagnation point
// (SEVAL(SST), CP=1/(BETA+BFAC), THET/DSTR averaging station 2),
// NSIDE arithmetic incl. MAX(...,2), CPV/TAU/QUE assignment per station,
// CDTOT/XT selection after LVISC. QINF=0 returns canonical inf/NaN.
[[nodiscard]] Result<PlxaddPoint> projectPlxaddPoint(const PlxaddInputs& in);

}  // namespace xfoil::core::export_data

// End: include/xfoil/core/export_data/ExportData.h

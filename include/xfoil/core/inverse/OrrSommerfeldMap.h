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

// File: include/xfoil/core/inverse/OrrSommerfeldMap.h
// Orr-Sommerfeld stability database (OSMAP) and amplitude integration
// (NTCALC) of the new 0-based cores, ported from
// fortran-src/osmap.f and fortran-src/ntcalc.f.
//
// OSMAP interpolates the complex wavenumber eigenvalue Alpha(Rtheta,W,H)
// from a discrete 3-D table via tricubic splines in the
// coordinates RL=log10(Rtheta), WL=log10(W)+0.5 log10(Rtheta), HL=H.
// The table comes from a binary Fortran unformatted sequential file
// (osmap.dat, REAL*4). No global mutable load, no raw
// pointers, no 1-based containers.
//
// Internal computations in double; the loaded table values are REAL(4).
// Reference comparisons against the canonical Fortran harness (REAL(4))
// therefore use an individual-accuracy-matched tolerance.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::inverse {

// Fortran parameter maximum dimensions of the Orr-Sommerfeld table.
inline constexpr int kOsMaxR = 31;
inline constexpr int kOsMaxW = 41;
inline constexpr int kOsMaxH = 21;

// Return value of OSMAP: real/imaginary part of Alpha*Theta and their
// derivatives w.r.t. Rtheta, W, H and mixed derivatives d/dW d/d{R,W,H}.
struct OrrSommerfeldResult {
  double alfr{0.0};
  double alfrR{0.0};
  double alfrW{0.0};
  double alfrH{0.0};
  double alfrwR{0.0};
  double alfrwW{0.0};
  double alfrwH{0.0};
  double alfi{0.0};
  double alfiR{0.0};
  double alfiW{0.0};
  double alfiH{0.0};
  double alfiwR{0.0};
  double alfiwW{0.0};
  double alfiwH{0.0};
  bool ok{false};
};

// Raw, already decoded Orr-Sommerfeld data (layout identical to the
// internal memory order). The numerical core performs no file/
// environment/path I/O; decoding the binary Fortran unformatted
// sequential file osmap.dat is the responsibility of a non-core adapter
// (in tests via a diagnostic tool). The tables are stored in flat
// Fortran memory order (IR,IW,IH,IC) with strides
// (kOsMaxR, kOsMaxW, kOsMaxH, 2).
struct OrrSommerfeldRaw {
  int nr{0};
  int nw{0};
  int nh{0};
  bool complete{true};  // false -> only IC=2 path (Fortran NOFILE analogy)
  std::vector<double> rl, wl, hl;
  std::vector<int32_t> ir1, ir2, iw1, iw2;  // per nh entries
  std::vector<double> a, ar, aw, ah, arw, arh, awh, arwh;  // 2*kOsMaxH*kOsMaxW*kOsMaxR
};

// Loaded Orr-Sommerfeld map. Value-semantic copyable (vectors).
class OrrSommerfeldMap {
 public:
  // Takes already decoded, validated table content. Checks only
  // dimensions/capacity; no file/environment I/O. An invalid
  // table returns a structured error.
  static Result<OrrSommerfeldMap> fromRaw(OrrSommerfeldRaw&& raw);

  OrrSommerfeldResult lookup(double rsp, double wsp, double hsp) const;

  bool loaded() const { return loaded_; }

 private:
  bool loaded_{false};
  int nr_{0};
  int nw_{0};
  int nh_{0};
  int ic1_{1};
  int ic2_{2};
  double rincr_{0.0};
  double wincr_{0.0};
  std::vector<double> rl_;
  std::vector<double> wl_;
  std::vector<double> hl_;
  std::vector<int32_t> ir1_, ir2_, iw1_, iw2_;
  // Eight tables in Fortran memory order (IR,IW,IH,IC), strides
  // (kOsMaxR, kOsMaxW, kOsMaxH, 2). REAL(4) values as double (exact).
  std::vector<double> a_, ar_, aw_, ah_, arw_, arh_, awh_, arwh_;

  std::size_t idx(int ir, int iw, int ih, int ic) const {
    return (static_cast<std::size_t>(ic - 1) * kOsMaxH +
            static_cast<std::size_t>(ih - 1)) *
               static_cast<std::size_t>(kOsMaxW) * kOsMaxR +
           static_cast<std::size_t>(iw - 1) * kOsMaxR +
           static_cast<std::size_t>(ir - 1);
  }

  // 1-based Fortran accesses on the coordinate/index vectors.
  double m_rl(int i) const { return rl_[static_cast<std::size_t>(i - 1)]; }
  double m_wl(int i) const { return wl_[static_cast<std::size_t>(i - 1)]; }
  double m_hl(int i) const { return hl_[static_cast<std::size_t>(i - 1)]; }
  int m_ir1(int i) const { return ir1_[static_cast<std::size_t>(i - 1)]; }
  int m_ir2(int i) const { return ir2_[static_cast<std::size_t>(i - 1)]; }
  int m_iw1(int i) const { return iw1_[static_cast<std::size_t>(i - 1)]; }
  int m_iw2(int i) const { return iw2_[static_cast<std::size_t>(i - 1)]; }
};

// Fortran: SUBROUTINE NTCALC(NX,N,X,HK,TH,UE,VE,NW,W,A)
// frequency range around the critical frequency and amplitude distribution A(i,k)
// by integrating the Orr-Sommerfeld growth rate. i=N counts as turbulent.
struct NtCalcResult {
  int n{0};
  int nw{0};
  std::vector<double> w;   // nw entries
  std::vector<double> a;   // n x nw, row-major (i-1)*nw + (k-1)
};
Result<NtCalcResult> ntCalc(const OrrSommerfeldMap& map, int nw,
                            std::span<const double> x,
                            std::span<const double> hk,
                            std::span<const double> th,
                            std::span<const double> ue,
                            std::span<const double> ve, int n);

}  // namespace xfoil::core::inverse

// End: include/xfoil/core/inverse/OrrSommerfeldMap.h

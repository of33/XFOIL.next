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

// File: include/xfoil/core/boundary_layer/BlForceReport.h
// FBLGET (dplot.f 320-503) as a self-contained core kernel for the
// non-graphical OPER-BLF text report. The kernel knows no
// session types: every geometry, panel and BL station value is
// handed over as flat input fields. The return values are the
// structured raw values of both pages (station, interpolated
// station sizes, profile arrays YY/UU, integral value FINT and the
// canonicalization 'yint > ye'), from which the CLI path forms
// the canonical format-9100/dump/format-7720 lines.
//
// Numerics exclusively via the established kernels HKIN, PRWALL,
// UWALL, and FS. The inputs are not modified.

#pragma once

#include <array>
#include <span>
#include <vector>

#include "xfoil/core/Result.h"

namespace xfoil::core::boundary_layer {

// Fortran FBLGET: PARAMETER (KPRX=129), NN=KPRX.
inline constexpr int kBlForceProfilePoints = 129;

// Fortran indices are 1-based; the kernel fields are 0-based.
// The index fields in the result follow the canonical Fortran
// positions (IS 1/2, IBL/IBLO/IBLP/ISO/ISP 1-based as in dplot.f),
// so that the text/diagnostic assignment remains traceable.
struct BlForceReportInputs {
  // Current contour (Fortran X,Y,S,XP,YP,NX,NY with N = point count).
  std::span<const double> x, y, s, xp, yp, nx, ny;
  // IPAN per page (0-based panel indices, Fortran IPAN(IBL,IS)).
  std::array<std::span<const int>, 2> ipan;
  // 0-based IBLTE(IS) (Fortran TE station).
  std::array<int, 2> iblte;
  // BL station fields per side (Fortran DSTR/THET/UEDG/XSSI).
  std::array<std::span<const double>, 2> dstr;
  std::array<std::span<const double>, 2> thet;
  std::array<std::span<const double>, 2> uedg;
  std::array<std::span<const double>, 2> xssi;
  // XSSITR(IS) (Fortran COMMON/XBL.INC).
  std::array<double, 2> xssitr;
  // Operating-point quantities (Fortran REINF, MINF, QINF, TKLAM, GAMM1).
  double reinf{0.0};
  double minf{0.0};
  double qinf{1.0};
  double tklam{0.0};
  double gamm1{0.0};
  // BLPAR CFFAC (Fortran COMMON/BLPAR; passed through to PRWALL/CFT).
  double cffac{1.0};
};

struct BlForceReportSide {
  // Next BL station (Fortran ISMIN/IBLMIN; IS 1-based).
  int is{0};
  int ibl{0};
  int iblo{0};
  int iblp{0};
  int iso{0};
  int isp{0};
  // Interpolated station sizes.
  double x0{0.0};
  double y0{0.0};
  double ds{0.0};
  double th{0.0};
  double ue{0.0};
  double xi{0.0};
  bool turb{false};
  double hstinv{0.0};
  double uec{0.0};
  double herat{0.0};
  double rhoe{0.0};
  double amue{0.0};
  double rtheta{0.0};
  double amsq{0.0};
  double hk{0.0};
  // Profile array (Fortran YY/UU, NN=129) and edge quantities.
  std::vector<double> yy;
  std::vector<double> uu;
  double de{0.0};
  double etae{0.0};
  // Integration up to YINT; yintExceeded matches the canonical
  // 'yint > ye' condition exactly when the loop ran to completion
  // (YINT beyond the profile arrays, label 80 without an earlier
  // jump).
  double fint{0.0};
  bool yintExceeded{false};
};

struct BlForceReportResult {
  BlForceReportSide side;
};

// FBLGET kernel for a single field point (XPR,YPR) with
// integration limit YINT. FBLGET selects the next BL side
// itself (ISMIN); the result contains exactly that side.
[[nodiscard]] Result<BlForceReportResult> blForceReport(
    const BlForceReportInputs& in, double xpr, double ypr, double yint);

}  // namespace xfoil::core::boundary_layer

// End: include/xfoil/core/boundary_layer/BlForceReport.h

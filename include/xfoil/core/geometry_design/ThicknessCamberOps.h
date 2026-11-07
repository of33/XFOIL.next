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

// File: include/xfoil/core/geometry_design/ThicknessCamberOps.h
// Thickness/camber report and scaling operations of the new
// 0-based cores, directly ported from fortran-src/xtcam.f.
//
// Contained routines:
// TCBUF -> reportThicknessCamber
// TCSCAL -> scaleThicknessCamber
// TCSET -> setThicknessCamberTargets
//
// The Fortran dialog parts (ASKR/ASKL) are modeled as explicit value or
// confirmation inputs; the numerical kernel writes nothing to
// stdout. The LGSYM branches of TCSCAL/TCSET differ in Fortran only in the
// dialog handling (message instead of a follow-up question); the numerical
// factors stay identical (CFAC=1.0 or CNEW skipped), so
// LGSYM has no numerical effect on the core factor computation.
//
// 0-based throughout the core. No raw-pointer API.

#pragma once

#include <optional>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry_design/CamberThickness.h"
#include "xfoil/core/geometry_design/GeometryDesignBuffer.h"

namespace xfoil::core::geometry_design {

// Fortran TCBUF initial group: current thickness/camber distribution of the
// buffer plus maxima (Fortran WRITE 'Max thickness/camber' structured).
// oppositePointFailures = SOPPS failures of the GETCAM phase (Fortran
// WRITE 'SOPPS: Opposite-point location failed. Continuing...' per
// failure); the kernel itself performs no writes.
struct ThicknessCamberReport {
  ThicknessCamberDistribution distribution;
  MaxPoint camberMax;
  MaxPoint thickMax;
  math::Diagnostic camberStatus{math::Diagnostic::completed};
  math::Diagnostic thickStatus{math::Diagnostic::completed};
  std::size_t oppositePointFailures{0};
};

// Fortran TCSCAL/TCSET factors.
struct ThicknessCamberFactors {
  double tfac{1.0};
  double cfac{1.0};
};

// Fortran TCSCAL initial group (before/after plus applied factors).
// thkcamOppositePointFailures = SOPPS failures of the THKCAM own
// opposite-point loop (Fortran WRITE per failure before the GEOPAR output).
struct ScaleResult {
  ThicknessCamberReport before;
  ThicknessCamberReport after;
  ThicknessCamberFactors factors;
  std::size_t thkcamOppositePointFailures{0};
};

// Fortran TCSET initial group. 'abortedQuestionable' corresponds to the
// canonical path 'Questionable input... Apply scaling factors? -> No ->
// No action taken' (no buffer change).
struct SetTargetsResult {
  ThicknessCamberReport before;
  std::optional<ThicknessCamberReport> after;
  ThicknessCamberFactors factors;
  bool abortedQuestionable{false};
  std::size_t thkcamOppositePointFailures{0};
};

// Safety contract: controlled errors leave the buffer unchanged.

// Fortran: SUBROUTINE TCBUF
// GETCAM + GETMAX(Camber) + GETMAX(Thickness) of the Buffers (without stdout).
ThicknessCamberReport reportThicknessCamber(
    const GeometryDesignBuffer& buffer);

// Fortran: SUBROUTINE TCSCAL(RINPUT,NINPUT)
// Before report, THKCAM with the factors, after report. The
// dialog variants (NINPUT 0/1/2) are mapped by optional factors;
// missing factors correspond to the Fortran default 1.0.
Result<ScaleResult> scaleThicknessCamber(GeometryDesignBuffer& buffer,
                                         std::optional<double> tfac,
                                         std::optional<double> cfac);

// Fortran: SUBROUTINE TCSET(RINPUT,NINPUT)
// Target values TNEW/CNEW (std::nullopt corresponds to the Fortran sentinel 999
// '<ret> to skip'); CFAC = CNEW/CYMAX, TFAC = TNEW/(2*TYMAX) (only if
// the respective maximum != 0). For |factor| > 100,
// applyQuestionable decides via the canonical ASKL confirmation; false leads
// to 'No action taken' without buffer change.
Result<SetTargetsResult> setThicknessCamberTargets(
    GeometryDesignBuffer& buffer, std::optional<double> tnew,
    std::optional<double> cnew, bool applyQuestionable = false);

}  // namespace xfoil::core::geometry_design

// End: include/xfoil/core/geometry_design/ThicknessCamberOps.h

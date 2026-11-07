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

// File: src/core/geometry_design/ThicknessCamberOps.cpp
// Port of TCBUF, TCSCAL and TCSET from fortran-src/xtcam.f.
// The dialog parts (ASKR/ASKL) are modeled as explicit value/
// confirmation inputs; stdout output is omitted.

#include "xfoil/core/geometry_design/ThicknessCamberOps.h"

#include <cmath>
#include <optional>
#include <vector>

namespace xfoil::core::geometry_design {

ThicknessCamberReport reportThicknessCamber(
    const GeometryDesignBuffer& buffer) {
  ThicknessCamberReport report;
  report.distribution =
      getCamberThickness(buffer.x, buffer.xp, buffer.y, buffer.yp, buffer.s);
  std::vector<double> ycmp(report.distribution.ncm, 0.0);
  std::vector<double> ytkp(report.distribution.ntk, 0.0);
  const MaxPointResult cam =
      getMaxPoint(report.distribution.xcm, report.distribution.ycm, ycmp);
  const MaxPointResult thk =
      getMaxPoint(report.distribution.xtk, report.distribution.ytk, ytkp);
  report.camberMax = cam.max;
  report.camberStatus = cam.status;
  report.thickMax = thk.max;
  report.thickStatus = thk.status;
  report.oppositePointFailures = report.distribution.oppositePointFailures;
  return report;
}

Result<ScaleResult> scaleThicknessCamber(GeometryDesignBuffer& buffer,
                                         std::optional<double> tfac,
                                         std::optional<double> cfac) {
  ScaleResult result;
  result.before = reportThicknessCamber(buffer);
  // Fortran 916-936: missing factors default to 1.0; the LGSYM branch
  // changes only the dialog, not the factors.
  result.factors.tfac = tfac.value_or(1.0);
  result.factors.cfac = cfac.value_or(1.0);

  const Result<std::size_t> st =
      transformThicknessCamber(buffer, result.factors.tfac,
                               result.factors.cfac);
  if (st.isErr()) {
    return Result<ScaleResult>::fail(st.error());
  }
  result.thkcamOppositePointFailures = st.value();
  result.after = reportThicknessCamber(buffer);
  return Result<ScaleResult>::ok(std::move(result));
}

Result<SetTargetsResult> setThicknessCamberTargets(
    GeometryDesignBuffer& buffer, std::optional<double> tnew,
    std::optional<double> cnew, bool applyQuestionable) {
  SetTargetsResult result;
  result.before = reportThicknessCamber(buffer);

  // Fortran 997-1000: factors from target values (sentinel 999 == nullopt).
  result.factors.cfac = 1.0;
  result.factors.tfac = 1.0;
  if (result.before.camberMax.y != 0.0 && cnew.has_value()) {
    result.factors.cfac = *cnew / result.before.camberMax.y;
  }
  if (result.before.thickMax.y != 0.0 && tnew.has_value()) {
    result.factors.tfac = *tnew / (2.0 * result.before.thickMax.y);
  }

  // Fortran 1003-1013: Plausibility query for |Factor| > 100.
  if (std::abs(result.factors.tfac) > 100.0 ||
      std::abs(result.factors.cfac) > 100.0) {
    if (!applyQuestionable) {
      result.abortedQuestionable = true;
      return Result<SetTargetsResult>::ok(std::move(result));
    }
  }

  const Result<std::size_t> st =
      transformThicknessCamber(buffer, result.factors.tfac,
                               result.factors.cfac);
  if (st.isErr()) {
    return Result<SetTargetsResult>::fail(st.error());
  }
  result.thkcamOppositePointFailures = st.value();
  result.after = reportThicknessCamber(buffer);
  return Result<SetTargetsResult>::ok(std::move(result));
}

}  // namespace xfoil::core::geometry_design

// End: src/core/geometry_design/ThicknessCamberOps.cpp

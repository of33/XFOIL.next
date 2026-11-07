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

// File: src/core/paneling/PanelParameters.cpp
// GETPAN parameter semantics of the new 0-based cores,
// ported from fortran-src/xfoil.f GETPAN.
//
// Only the parameter branching lives here; the menu, prompt and
// plot code stays in the CLI. Clamp diagnostics (Fortran WRITE 1200)
// are reported structured via clamped=true. Values below the
// numerically required minimum size are rejected in a controlled way
// (Fortran would instead re-ask per dialog).

#include "xfoil/core/paneling/PanelParameters.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace xfoil::core::paneling {

// Fortran: GETPAN parameter branching.
Result<ParameterChangeResult> applyPanelParameter(
    const PanelingParameters& current, PanelParameterKind kind,
    std::span<const double> values) {
  ParameterChangeResult result;
  result.parameters = current;
  result.clamped = false;

  switch (kind) {
    case PanelParameterKind::panelCount: {
      // Fortran: 'N' -> NPAN = value (NINPUT >= 1).
      if (values.size() != 1) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: panelCount expected exactly 1 "
                        "Value"));
      }
      const int panelCount = static_cast<int>(values[0]);
      if (panelCount < 2) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: panelCount must at least 2 "
                        "be"));
      }
      // Fortran: NPAN > IQX-6 -> NPAN = IQX-6 + Message (Clamp).
      if (panelCount > kMaxPanelingPanelCount) {
        result.parameters.panelCount = kMaxPanelingPanelCount;
        result.clamped = true;
      } else {
        result.parameters.panelCount = panelCount;
      }
      break;
    }
    case PanelParameterKind::bunchingParam: {
      // Fortran: 'P' -> CVPAR = value.
      if (values.size() != 1) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: bunchingParam expected exactly 1 "
                        "Value"));
      }
      result.parameters.bunchingParam = values[0];
      break;
    }
    case PanelParameterKind::teToLeDensityRatio: {
      // Fortran: 'T' -> CTERAT = value.
      if (values.size() != 1) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: teToLeDensityRatio expected "
                        "exactly 1 Value"));
      }
      result.parameters.teToLeDensityRatio = values[0];
      break;
    }
    case PanelParameterKind::refinementDensityRatio: {
      // Fortran: 'R' -> CTRRAT = value.
      if (values.size() != 1) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: refinementDensityRatio "
                        "expected exactly 1 Value"));
      }
      result.parameters.refinementDensityRatio = values[0];
      break;
    }
    case PanelParameterKind::upperRefinementRange: {
      // Fortran: 'XT' -> XSREF1/2 = 2 values.
      if (values.size() != 2) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: upperRefinementRange expected "
                        "exactly 2 values"));
      }
      result.parameters.upperRefinementRange = {values[0], values[1]};
      break;
    }
    case PanelParameterKind::lowerRefinementRange: {
      // Fortran: 'XB' -> XPREF1/2 = 2 values.
      if (values.size() != 2) {
        return Result<ParameterChangeResult>::fail(
            Error::make(ErrorCode::invalidArgument,
                        "applyPanelParameter: lowerRefinementRange expected "
                        "exactly 2 values"));
      }
      result.parameters.lowerRefinementRange = {values[0], values[1]};
      break;
    }
  }

  return Result<ParameterChangeResult>::ok(std::move(result));
}

}  // namespace xfoil::core::paneling

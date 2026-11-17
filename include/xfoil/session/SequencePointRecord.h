// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
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

// File: include/xfoil/session/SequencePointRecord.h
// Internal per-point sequence record. It carries the existing
// AnalysisResult plus the canonical scalar values that are available only
// while the point's session state is current. Public projections convert
// this record into xfoil::PolarPoint.

#pragma once

#include <cstddef>
#include <optional>

#include "xfoil/AnalysisResult.h"

namespace xfoil::session {

// One executed or attempted sequence point. The optional detail values are
// present only when the result is successful and the corresponding
// canonical point value exists. Cancelled, failed and not-run records leave
// them empty; no value is carried over from a previous point.
struct SequencePointRecord {
  AnalysisResult result;
  std::optional<double> dragCoefficientPressure;
  std::optional<double> dragCoefficientFriction;
  std::optional<double> dragCoefficientViscous;
  std::optional<std::size_t> iterations;
};

}  // namespace xfoil::session

// End: include/xfoil/session/SequencePointRecord.h

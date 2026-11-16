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

// File: tests/unit/LibraryOneShotL4HeaderSmoke.cpp
// Public header compile smoke for the one-shot request/analyze API.

#include <type_traits>
#include <variant>

#include "xfoil/AnalysisRequest.h"
#include "xfoil/Analyze.h"
#include "xfoil/Progress.h"
#include "xfoil/SessionOptions.h"

static_assert(std::is_aggregate_v<xfoil::AnalysisRequest>);
static_assert(std::is_aggregate_v<xfoil::PanelingOptions>);
static_assert(std::variant_size_v<xfoil::AnalysisTarget> == 2);
static_assert(std::is_same_v<
              decltype(static_cast<xfoil::Result<xfoil::OperatingPointData> (*)(
                  const xfoil::AnalysisRequest&,
                  const xfoil::CancellationToken*)>(&xfoil::analyze)),
              xfoil::Result<xfoil::OperatingPointData> (*)(
                  const xfoil::AnalysisRequest&,
                  const xfoil::CancellationToken*)>);
static_assert(std::is_copy_constructible_v<xfoil::ProgressEvent>);
static_assert(std::is_copy_assignable_v<xfoil::ProgressEvent>);
static_assert(std::is_move_constructible_v<xfoil::ProgressEvent>);

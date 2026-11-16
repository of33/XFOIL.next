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

// File: include/xfoil/Analyze.h
// Public one-shot in-memory analysis. Each call uses a fresh call-local
// xfoil::Session and performs no file or console I/O.

#pragma once

#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/Export.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"

namespace xfoil {

// Runs exactly one alpha or CL point on a temporary session. Returns an
// owning operating point. A non-converged but executed point is a
// successful value with ConvergenceStatus::unconverged; errors before a
// valid point are returned as structured Error values.
[[nodiscard]]
XFOIL_API Result<OperatingPointData> analyze(
    const AnalysisRequest& request,
    const CancellationToken* cancellation = nullptr);

// Additive progress overload. The callback receives geometry, paneling,
// initialization and the selected solver phases from the same temporary
// session. configure has no progress event.
[[nodiscard]]
XFOIL_API Result<OperatingPointData> analyze(
    const AnalysisRequest& request,
    const CancellationToken* cancellation,
    const ProgressCallback& progress);

}  // namespace xfoil

// End: include/xfoil/Analyze.h

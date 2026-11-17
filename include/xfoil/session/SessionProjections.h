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

// File: include/xfoil/session/SessionProjections.h
// Pure, non-mutating projections from the existing session state into
// public owning value types. These functions copy and map existing data;
// they never run a solver, never mutate the session and never perform
// file or console I/O. Internal session header; not part of the L2
// public API surface.

#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Result.h"
#include "xfoil/SkinFriction.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/session/SessionState.h"

namespace xfoil::session {

// Current panel contour as an owning public value.
[[nodiscard]]
Result<SurfaceGeometry> projectCurrentPanel(
    const SessionComposition& session);

// Cp distribution of the last analyzed point on the airfoil surface.
[[nodiscard]]
Result<SurfaceDistribution> projectSurfaceDistribution(
    const SessionComposition& session);

// Owning skin-friction distribution of the current valid viscous point.
// Uses the existing final VISCAL tau state and the operating qinf
// (dimensionless freestream speed scale) only; wake stations are excluded.
// Pure and non-mutating.
[[nodiscard]]
Result<SkinFrictionDistribution> projectSkinFriction(
    const SessionComposition& session);

// Detailed result of the last analyzed point.
[[nodiscard]]
Result<OperatingPointData> projectLastOperatingPoint(
    const SessionComposition& session);

// Points of the active stored polar slot as a sequence result. The
// stored polar is a completed accumulation; its points are reported as
// converged because only accepted accumulation points are stored.
[[nodiscard]]
Result<SequenceResult> projectActivePolar(
    const SessionComposition& session);

// Pure mapping of an existing analysis-result sequence. No session state
// is read and no solver is called.
[[nodiscard]]
SequenceResult projectSequence(
    std::span<const AnalysisResult> results,
    SequenceStopReason stopReason = SequenceStopReason::completed,
    std::optional<std::size_t> stoppedAtIndex = std::nullopt);

}  // namespace xfoil::session

// End: include/xfoil/session/SessionProjections.h

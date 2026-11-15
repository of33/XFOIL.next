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

// File: include/xfoil/AnalysisRequest.h
// Public in-memory request for one analysis point. The request owns the
// geometry, paneling, configuration and exactly one analysis target.
// It contains no filenames, streams, callbacks or session state.

#pragma once

#include <variant>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/Types.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil {

// Exactly one of these alternatives is present in AnalysisTarget.
struct AlphaTarget {
  Degrees alpha{};
};

struct LiftTarget {
  double cl{};
};

using AnalysisTarget = std::variant<AlphaTarget, LiftTarget>;

// One-shot analysis inputs. The default target is alpha 0; the default
// geometry is empty, so a default request fails at the geometry step
// instead of silently running an arbitrary analysis.
struct AnalysisRequest {
  geometry::AirfoilGeometry geometry;
  PanelingOptions paneling;
  AnalysisConfig configuration;
  AnalysisTarget target{AlphaTarget{Degrees{0.0}}};
};

}  // namespace xfoil

// End: include/xfoil/AnalysisRequest.h

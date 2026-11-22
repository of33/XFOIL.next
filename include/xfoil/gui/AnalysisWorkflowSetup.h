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

// File: include/xfoil/gui/AnalysisWorkflowSetup.h
// Owning host setup values for the embeddable analysis workflow. These
// types contain no Qt object, widget, session, file path or callback.

#pragma once

#include <string>
#include <variant>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil::gui {

struct MemoryGeometrySetup {
  geometry::AirfoilGeometry geometry;
};

struct NacaGeometrySetup {
  std::string designation;
};

struct NacsGeometrySetup {
  std::string designation;
};

using AnalysisGeometrySetup =
    std::variant<MemoryGeometrySetup, NacaGeometrySetup,
                 NacsGeometrySetup>;

using AnalysisWorkflowRequest =
    std::variant<AlphaTarget, LiftTarget, AlphaSequence, LiftSequence>;

struct AnalysisWorkflowSetup {
  AnalysisGeometrySetup geometry;
  PanelingOptions paneling;
  AnalysisConfig configuration;
  AnalysisWorkflowRequest request;
};

struct AnalysisWorkflowViewOptions {
  bool showPaneling{false};
  bool showPanelNodes{true};
};

}  // namespace xfoil::gui

// End: include/xfoil/gui/AnalysisWorkflowSetup.h

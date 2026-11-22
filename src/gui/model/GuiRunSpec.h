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

// File: src/gui/model/GuiRunSpec.h
// Immutable, owning hand-off snapshot for the G3 worker. A RunSpec can
// only be created by a validated GuiDefinitionModel. It contains no
// widgets, QObject, session, file path, callback or cancellation object.

#pragma once

#include <string>
#include <variant>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil::gui {

struct GuiNacaRequest {
  std::string designation;
  bool sharpTrailingEdge{false};
};

using GuiGeometryRequest =
    std::variant<geometry::AirfoilGeometry, GuiNacaRequest>;
using GuiAnalysisRequest =
    std::variant<AlphaTarget, LiftTarget, AlphaSequence, LiftSequence>;

class GuiRunSpec {
 public:
  GuiRunSpec(const GuiRunSpec&) = default;
  GuiRunSpec(GuiRunSpec&&) noexcept = default;
  GuiRunSpec& operator=(const GuiRunSpec&) = default;
  GuiRunSpec& operator=(GuiRunSpec&&) noexcept = default;
  ~GuiRunSpec() = default;

  [[nodiscard]]
  const GuiGeometryRequest& geometry() const noexcept {
    return geometry_;
  }

  [[nodiscard]]
  const PanelingOptions& paneling() const noexcept {
    return paneling_;
  }

  [[nodiscard]]
  const AnalysisConfig& configuration() const noexcept {
    return configuration_;
  }

  [[nodiscard]]
  const GuiAnalysisRequest& target() const noexcept {
    return target_;
  }

 private:
  friend class GuiDefinitionModel;

  GuiRunSpec(GuiGeometryRequest geometry, PanelingOptions paneling,
             AnalysisConfig configuration, GuiAnalysisRequest target)
      : geometry_(std::move(geometry)),
        paneling_(paneling),
        configuration_(configuration),
        target_(std::move(target)) {}

  GuiGeometryRequest geometry_;
  PanelingOptions paneling_;
  AnalysisConfig configuration_;
  GuiAnalysisRequest target_;
};

}  // namespace xfoil::gui

// End: src/gui/model/GuiRunSpec.h

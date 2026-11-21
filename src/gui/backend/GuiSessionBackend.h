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

// File: src/gui/backend/GuiSessionBackend.h
// G2 backend boundary around exactly one public xfoil::Session. It performs
// only setup and projection work. It has no QObject, widget, file or UI
// dependency and can be moved to a G3 worker without changing its inputs.

#pragma once

#include <string_view>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/Result.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil::gui {

class GuiSessionBackend {
 public:
  GuiSessionBackend() = default;
  ~GuiSessionBackend() = default;

  GuiSessionBackend(const GuiSessionBackend&) = delete;
  GuiSessionBackend& operator=(const GuiSessionBackend&) = delete;
  GuiSessionBackend(GuiSessionBackend&&) noexcept = default;
  GuiSessionBackend& operator=(GuiSessionBackend&&) noexcept = default;

  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry);

  [[nodiscard]]
  Result<geometry::AirfoilGeometry> setNaca(std::string_view designation,
                                            bool sharpTrailingEdge);

  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(const PanelingOptions& options);

  [[nodiscard]]
  Result<void> setConfiguration(const AnalysisConfig& configuration);

 private:
  Session session_;
};

}  // namespace xfoil::gui

// End: src/gui/backend/GuiSessionBackend.h

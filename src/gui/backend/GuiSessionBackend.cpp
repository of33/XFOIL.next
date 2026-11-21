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

// File: src/gui/backend/GuiSessionBackend.cpp
// Synchronous G2 setup adapter. No solve or analysis operation is called.

#include "backend/GuiSessionBackend.h"

#include <utility>

namespace xfoil::gui {

Result<void> GuiSessionBackend::setGeometry(
    const geometry::AirfoilGeometry& geometry) {
  return session_.setGeometry(geometry);
}

Result<geometry::AirfoilGeometry> GuiSessionBackend::setNaca(
    std::string_view designation, bool sharpTrailingEdge) {
  if (sharpTrailingEdge) {
    return session_.setNacs(designation);
  }
  return session_.setNaca(designation);
}

Result<SurfaceGeometry> GuiSessionBackend::generatePanel(
    const PanelingOptions& options) {
  return session_.generatePanel(options);
}

Result<void> GuiSessionBackend::setConfiguration(
    const AnalysisConfig& configuration) {
  return session_.configure(configuration);
}

}  // namespace xfoil::gui

// End: src/gui/backend/GuiSessionBackend.cpp

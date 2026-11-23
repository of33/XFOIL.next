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

// File: src/gui/plots/GuiPointPresentation.cpp
// Fixed alpha formatter; non-converged points get an explicit (nc) suffix.

#include "plots/GuiPointPresentation.h"

#include <iomanip>
#include <sstream>

namespace xfoil::gui {

std::string formatOperatingPointLabel(const GuiPointEntry& point) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(3) << point.alpha;
  if (!point.converged) {
    stream << " (nc)";
  }
  return stream.str();
}

GuiConvergenceText formatConvergence(GuiConvergenceState state) {
  switch (state) {
    case GuiConvergenceState::noPoint:
      return {"No point selected", "○"};
    case GuiConvergenceState::converged:
      return {"Converged", "●"};
    case GuiConvergenceState::notConverged:
      return {"Not converged", "●"};
    case GuiConvergenceState::cancelled:
      return {"Cancelled", "■"};
    case GuiConvergenceState::failed:
      return {"Failed", "✕"};
  }
  return {"No point selected", "○"};
}

}  // namespace xfoil::gui

// End: src/gui/plots/GuiPointPresentation.cpp

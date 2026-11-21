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

// File: src/gui/model/GuiResultSummary.h
// Empty G2a result-summary shell for the 4x3 matrix. No value is computed
// in G2a; absent values are represented as n/a.

#pragma once

#include <optional>
#include <string>

namespace xfoil::gui {

struct GuiResultSummary {
  std::optional<double> cd;
  std::optional<double> cdf;
  std::optional<double> cdp;
  std::optional<double> cl;
  std::optional<double> cm;
  std::optional<double> efficiency;
  std::optional<double> cd0;
  std::optional<double> cl0;
  std::optional<double> cm0;
  std::optional<double> alpha0;
  std::optional<double> clMaximum;
  std::optional<double> efficiencyMaximum;
};

[[nodiscard]]
std::string formatResultValue(const std::optional<double>& value);

}  // namespace xfoil::gui

// End: src/gui/model/GuiResultSummary.h

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
// Solver-independent result matrix and derived sequence summary. No Qt,
// session or solver dependency. Direct values may come from a selected
// non-converged point; derived values use enabled converged points only.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/GuiResultSelectionModel.h"

namespace xfoil::gui {

enum class GuiResultProvenance {
  unavailable,
  exact,
  interpolated,
  sampledProvisional
};

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
  bool clMaximumProvisional{false};
  bool efficiencyMaximumProvisional{false};
  GuiResultProvenance clMaximumProvenance{
      GuiResultProvenance::unavailable};
  GuiResultProvenance efficiencyMaximumProvenance{
      GuiResultProvenance::unavailable};
};

[[nodiscard]]
std::optional<double> guiPointCd(const GuiPointEntry& point);
[[nodiscard]]
std::optional<double> guiPointCdf(const GuiPointEntry& point);
[[nodiscard]]
std::optional<double> guiPointCdp(const GuiPointEntry& point);
[[nodiscard]]
std::optional<double> guiPointCl(const GuiPointEntry& point);
[[nodiscard]]
std::optional<double> guiPointCm(const GuiPointEntry& point);
[[nodiscard]]
std::optional<double> guiPointEfficiency(const GuiPointEntry& point);

[[nodiscard]]
GuiResultSummary computeGuiResultSummary(
    const std::vector<GuiPointEntry>& points,
    const std::optional<GuiPointEntry>& selected);

[[nodiscard]]
std::string formatResultValue(const std::optional<double>& value);

inline constexpr std::string_view kProvisionalClMaxHint =
    "Highest computed CL. Extend the alpha sequence beyond the peak for a "
    "resolved CLmax.";
inline constexpr std::string_view kProvisionalEfficiencyMaxHint =
    "Highest computed CL/CD. Extend the alpha sequence beyond the peak for a "
    "resolved maximum.";

}  // namespace xfoil::gui

// End: src/gui/model/GuiResultSummary.h

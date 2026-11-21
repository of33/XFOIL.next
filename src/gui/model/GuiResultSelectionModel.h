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

// File: src/gui/model/GuiResultSelectionModel.h
// Solver-independent view/filter state for calculated points. Deactivation
// only hides points; source order and source indices are never changed.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace xfoil::gui {

struct GuiPointEntry {
  std::size_t sourceIndex{};
  double alpha{};
  bool converged{false};
  bool active{true};
};

class GuiResultSelectionModel {
 public:
  GuiResultSelectionModel() = default;
  explicit GuiResultSelectionModel(std::vector<GuiPointEntry> points);

  void setPoints(std::vector<GuiPointEntry> points);
  void clear() noexcept;

  [[nodiscard]]
  bool empty() const noexcept {
    return points_.empty();
  }

  [[nodiscard]]
  std::size_t size() const noexcept {
    return points_.size();
  }

  [[nodiscard]]
  const std::vector<GuiPointEntry>& points() const noexcept {
    return points_;
  }

  [[nodiscard]]
  std::vector<GuiPointEntry> activePoints() const;
  [[nodiscard]]
  std::vector<GuiPointEntry> inactivePoints() const;

  [[nodiscard]]
  std::optional<std::size_t> currentSourceIndex() const noexcept {
    return currentSourceIndex_;
  }

  [[nodiscard]]
  std::optional<GuiPointEntry> currentPoint() const;

  [[nodiscard]]
  bool select(std::size_t sourceIndex);
  [[nodiscard]]
  bool deactivate(std::size_t sourceIndex);
  [[nodiscard]]
  bool activate(std::size_t sourceIndex);

 private:
  [[nodiscard]]
  std::optional<std::size_t> indexOf(std::size_t sourceIndex) const;
  void selectNextActiveAfter(std::size_t sourcePosition);
  void selectFirstActiveIfNeeded();

  std::vector<GuiPointEntry> points_;
  std::optional<std::size_t> currentSourceIndex_;
};

}  // namespace xfoil::gui

// End: src/gui/model/GuiResultSelectionModel.h

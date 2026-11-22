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

// File: src/gui/model/GuiPolarManager.h
// Instance-local, Qt-free store for comparison polars. Current analysis
// results are not stored here; they are snapshots only on explicit add.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "xfoil/PolarData.h"
#include "xfoil/Result.h"

namespace xfoil::gui {

enum class GuiPolarOrigin {
  currentSnapshot,
  imported
};

struct GuiStoredPolar {
  std::uint64_t id{0};
  std::string name;
  xfoil::PolarData polar;
  bool visible{true};
  GuiPolarOrigin origin{GuiPolarOrigin::currentSnapshot};
};

class GuiPolarManager {
 public:
  static constexpr std::size_t maxComparisons = 8;

  GuiPolarManager() = default;

  [[nodiscard]]
  const std::vector<GuiStoredPolar>& entries() const noexcept {
    return entries_;
  }

  [[nodiscard]]
  std::optional<std::uint64_t> selectedId() const noexcept {
    return selectedId_;
  }

  [[nodiscard]]
  const GuiStoredPolar* selected() const noexcept;

  [[nodiscard]]
  Result<std::uint64_t> add(xfoil::PolarData polar,
                            GuiPolarOrigin origin);
  [[nodiscard]]
  Result<void> rename(std::uint64_t id, std::string name);
  [[nodiscard]]
  Result<void> setVisible(std::uint64_t id, bool visible);
  [[nodiscard]]
  Result<void> select(std::uint64_t id);
  [[nodiscard]]
  Result<void> moveUp(std::uint64_t id);
  [[nodiscard]]
  Result<void> moveDown(std::uint64_t id);
  [[nodiscard]]
  Result<void> remove(std::uint64_t id);
  void clear();

 private:
  [[nodiscard]]
  GuiStoredPolar* find(std::uint64_t id) noexcept;
  [[nodiscard]]
  const GuiStoredPolar* find(std::uint64_t id) const noexcept;

  std::vector<GuiStoredPolar> entries_;
  std::optional<std::uint64_t> selectedId_;
  std::uint64_t nextId_{1};
};

}  // namespace xfoil::gui

// End: src/gui/model/GuiPolarManager.h

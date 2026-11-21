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

// File: src/gui/model/GuiResultSelectionModel.cpp
// Deterministic source-index selection and enabled/disabled view state.

#include "model/GuiResultSelectionModel.h"

#include <utility>

namespace xfoil::gui {

GuiResultSelectionModel::GuiResultSelectionModel(
    std::vector<GuiPointEntry> points) {
  setPoints(std::move(points));
}

void GuiResultSelectionModel::setPoints(
    std::vector<GuiPointEntry> points) {
  points_ = std::move(points);
  if (currentSourceIndex_.has_value()) {
    const std::optional<std::size_t> index =
        indexOf(*currentSourceIndex_);
    if (!index.has_value() || !points_[*index].enabled) {
      currentSourceIndex_.reset();
    }
  }
  selectFirstEnabledIfNeeded();
}

void GuiResultSelectionModel::clear() noexcept {
  points_.clear();
  currentSourceIndex_.reset();
}

std::optional<std::size_t> GuiResultSelectionModel::indexOf(
    std::size_t sourceIndex) const {
  for (std::size_t index = 0; index < points_.size(); ++index) {
    if (points_[index].sourceIndex == sourceIndex) {
      return index;
    }
  }
  return std::nullopt;
}

std::vector<GuiPointEntry> GuiResultSelectionModel::enabledPoints() const {
  std::vector<GuiPointEntry> result;
  for (const GuiPointEntry& point : points_) {
    if (point.enabled) {
      result.push_back(point);
    }
  }
  return result;
}

std::vector<GuiPointEntry> GuiResultSelectionModel::disabledPoints() const {
  std::vector<GuiPointEntry> result;
  for (const GuiPointEntry& point : points_) {
    if (!point.enabled) {
      result.push_back(point);
    }
  }
  return result;
}

std::optional<GuiPointEntry> GuiResultSelectionModel::currentPoint() const {
  if (!currentSourceIndex_.has_value()) {
    return std::nullopt;
  }
  const std::optional<std::size_t> index =
      indexOf(*currentSourceIndex_);
  if (!index.has_value() || !points_[*index].enabled) {
    return std::nullopt;
  }
  return points_[*index];
}

bool GuiResultSelectionModel::select(std::size_t sourceIndex) {
  const std::optional<std::size_t> index = indexOf(sourceIndex);
  if (!index.has_value() || !points_[*index].enabled) {
    return false;
  }
  currentSourceIndex_ = sourceIndex;
  return true;
}

void GuiResultSelectionModel::selectAfterDeactivate(
    std::size_t sourcePosition) {
  for (std::size_t index = sourcePosition + 1; index < points_.size();
       ++index) {
    if (points_[index].enabled) {
      currentSourceIndex_ = points_[index].sourceIndex;
      return;
    }
  }
  for (std::size_t index = sourcePosition; index > 0; --index) {
    const std::size_t candidate = index - 1;
    if (points_[candidate].enabled) {
      currentSourceIndex_ = points_[candidate].sourceIndex;
      return;
    }
  }
  currentSourceIndex_.reset();
}

void GuiResultSelectionModel::selectFirstEnabledIfNeeded() {
  if (currentSourceIndex_.has_value()) {
    return;
  }
  for (const GuiPointEntry& point : points_) {
    if (point.enabled) {
      currentSourceIndex_ = point.sourceIndex;
      return;
    }
  }
  currentSourceIndex_.reset();
}

bool GuiResultSelectionModel::deactivate(std::size_t sourceIndex) {
  const std::optional<std::size_t> index = indexOf(sourceIndex);
  if (!index.has_value() || !points_[*index].enabled) {
    return false;
  }

  const bool wasCurrent = currentSourceIndex_.has_value() &&
                          *currentSourceIndex_ == sourceIndex;
  points_[*index].enabled = false;
  if (wasCurrent) {
    selectAfterDeactivate(*index);
  }
  return true;
}

bool GuiResultSelectionModel::activate(std::size_t sourceIndex) {
  const std::optional<std::size_t> index = indexOf(sourceIndex);
  if (!index.has_value() || points_[*index].enabled) {
    return false;
  }
  points_[*index].enabled = true;
  selectFirstEnabledIfNeeded();
  return true;
}

}  // namespace xfoil::gui

// End: src/gui/model/GuiResultSelectionModel.cpp

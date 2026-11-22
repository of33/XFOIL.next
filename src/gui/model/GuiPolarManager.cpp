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

#include "model/GuiPolarManager.h"

#include <algorithm>
#include <utility>

namespace xfoil::gui {
namespace {

Error managerError(const char* code, std::string message) {
  return Error::make(ErrorCategory::invalidArgument, code,
                     std::move(message), "polar.manager");
}

Error limitError() {
  return Error::make(ErrorCategory::capacityExceeded,
                     "gui.polarLimitReached",
                     "At most eight comparison polars can be stored",
                     "polar.manager");
}

Error notFoundError() {
  return managerError("gui.polarNotFound",
                      "The selected comparison polar does not exist");
}

}  // namespace

const GuiStoredPolar* GuiPolarManager::find(std::uint64_t id) const noexcept {
  const auto iterator = std::find_if(
      entries_.begin(), entries_.end(),
      [id](const GuiStoredPolar& entry) { return entry.id == id; });
  return iterator == entries_.end() ? nullptr : &*iterator;
}

GuiStoredPolar* GuiPolarManager::find(std::uint64_t id) noexcept {
  const auto iterator = std::find_if(
      entries_.begin(), entries_.end(),
      [id](const GuiStoredPolar& entry) { return entry.id == id; });
  return iterator == entries_.end() ? nullptr : &*iterator;
}

const GuiStoredPolar* GuiPolarManager::selected() const noexcept {
  return selectedId_.has_value() ? find(*selectedId_) : nullptr;
}

Result<std::uint64_t> GuiPolarManager::add(xfoil::PolarData polar,
                                           GuiPolarOrigin origin) {
  if (entries_.size() >= maxComparisons) {
    return Result<std::uint64_t>::fail(limitError());
  }
  if (polar.points.empty()) {
    return Result<std::uint64_t>::fail(managerError(
        "gui.polarEmpty", "An empty polar cannot be stored"));
  }
  const std::uint64_t id = nextId_++;
  GuiStoredPolar entry;
  entry.id = id;
  entry.name = polar.metadata.name;
  if (entry.name.empty()) {
    entry.name = "Comparison " + std::to_string(id);
  }
  entry.polar = std::move(polar);
  entry.origin = origin;
  entries_.push_back(std::move(entry));
  selectedId_ = id;
  return Result<std::uint64_t>::ok(id);
}

Result<void> GuiPolarManager::rename(std::uint64_t id, std::string name) {
  GuiStoredPolar* entry = find(id);
  if (entry == nullptr) {
    return Result<void>::fail(notFoundError());
  }
  if (name.empty()) {
    return Result<void>::fail(managerError(
        "gui.polarName", "A comparison polar name must not be empty"));
  }
  entry->name = name;
  entry->polar.metadata.name = std::move(name);
  return Result<void>::ok();
}

Result<void> GuiPolarManager::setVisible(std::uint64_t id, bool visible) {
  GuiStoredPolar* entry = find(id);
  if (entry == nullptr) {
    return Result<void>::fail(notFoundError());
  }
  entry->visible = visible;
  return Result<void>::ok();
}

Result<void> GuiPolarManager::select(std::uint64_t id) {
  if (find(id) == nullptr) {
    return Result<void>::fail(notFoundError());
  }
  selectedId_ = id;
  return Result<void>::ok();
}

Result<void> GuiPolarManager::moveUp(std::uint64_t id) {
  GuiStoredPolar* entry = find(id);
  if (entry == nullptr) {
    return Result<void>::fail(notFoundError());
  }
  const auto iterator = std::find_if(
      entries_.begin(), entries_.end(),
      [id](const GuiStoredPolar& value) { return value.id == id; });
  if (iterator != entries_.begin()) {
    std::iter_swap(iterator, iterator - 1);
  }
  return Result<void>::ok();
}

Result<void> GuiPolarManager::moveDown(std::uint64_t id) {
  GuiStoredPolar* entry = find(id);
  if (entry == nullptr) {
    return Result<void>::fail(notFoundError());
  }
  const auto iterator = std::find_if(
      entries_.begin(), entries_.end(),
      [id](const GuiStoredPolar& value) { return value.id == id; });
  if (iterator + 1 != entries_.end()) {
    std::iter_swap(iterator, iterator + 1);
  }
  return Result<void>::ok();
}

Result<void> GuiPolarManager::remove(std::uint64_t id) {
  const auto iterator = std::find_if(
      entries_.begin(), entries_.end(),
      [id](const GuiStoredPolar& entry) { return entry.id == id; });
  if (iterator == entries_.end()) {
    return Result<void>::fail(notFoundError());
  }
  entries_.erase(iterator);
  if (selectedId_ == id) {
    selectedId_.reset();
    if (!entries_.empty()) {
      selectedId_ = entries_.front().id;
    }
  }
  return Result<void>::ok();
}

void GuiPolarManager::clear() {
  entries_.clear();
  selectedId_.reset();
}

}  // namespace xfoil::gui

// End: src/gui/model/GuiPolarManager.cpp

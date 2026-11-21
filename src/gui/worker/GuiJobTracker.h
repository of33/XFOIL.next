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

// File: src/gui/worker/GuiJobTracker.h
// Monotonic job and definition-generation identity. Signals carrying an
// old job id or an old generation are rejected before they reach the UI.

#pragma once

#include <optional>

#include <QtGlobal>

namespace xfoil::gui {

struct GuiJobKey {
  qulonglong jobId{};
  quint64 generationId{};
};

class GuiJobTracker {
 public:
  [[nodiscard]]
  bool isRunning() const noexcept {
    return active_.has_value();
  }

  [[nodiscard]]
  const std::optional<GuiJobKey>& active() const noexcept {
    return active_;
  }

  [[nodiscard]]
  std::optional<GuiJobKey> begin() noexcept {
    if (active_.has_value()) {
      return std::nullopt;
    }
    const GuiJobKey key{++lastJobId_, generation_};
    active_ = key;
    return key;
  }

  [[nodiscard]]
  bool accepts(qulonglong jobId, quint64 generationId) const noexcept {
    return active_.has_value() && active_->jobId == jobId &&
           active_->generationId == generationId &&
           generationId == generation_;
  }

  void finish(qulonglong jobId, quint64 generationId) noexcept {
    (void)generationId;
    if (active_.has_value() && active_->jobId == jobId) {
      active_.reset();
    }
  }

  void invalidate() noexcept {
    ++generation_;
  }

  [[nodiscard]]
  quint64 generation() const noexcept {
    return generation_;
  }

 private:
  std::optional<GuiJobKey> active_;
  qulonglong lastJobId_{0};
  quint64 generation_{0};
};

}  // namespace xfoil::gui

// End: src/gui/worker/GuiJobTracker.h

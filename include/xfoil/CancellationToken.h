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

// file: include/xfoil/CancellationToken.h
// Instance-scoped cancellation token. The token is held per
// caller or session and is not copyable.

#pragma once

#include <atomic>

namespace xfoil {

class CancellationToken {
 public:
  CancellationToken() = default;

  CancellationToken(const CancellationToken&) = delete;
  CancellationToken& operator=(const CancellationToken&) = delete;

  void requestCancellation() noexcept {
    requested_.store(true, std::memory_order_relaxed);
  }

  void reset() noexcept {
    requested_.store(false, std::memory_order_relaxed);
  }

  [[nodiscard]]
  bool isCancellationRequested() const noexcept {
    return requested_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic_bool requested_{false};
};

}  // namespace xfoil

// End: include/xfoil/CancellationToken.h

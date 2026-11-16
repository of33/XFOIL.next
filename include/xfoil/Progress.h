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

// File: include/xfoil/Progress.h
// Public optional, synchronous progress observer value types. Progress
// events are owning values and contain no file, stream, environment or
// implementation state.

#pragma once

#include <cstddef>
#include <functional>
#include <optional>

namespace xfoil {

// Phase reported by a progress callback. none is the default and is not
// emitted as a real progress event.
enum class ProgressPhase {
  none,
  geometry,
  paneling,
  initialization,
  inviscidSolve,
  viscousIteration,
  sequence
};

// One owning progress observation. completed counts finished units in the
// current phase; iteration and sequenceIndex are zero-based indices when
// applicable. fraction is present only when total has a positive value.
struct ProgressEvent {
  ProgressPhase phase{ProgressPhase::none};
  std::size_t completed{};
  std::optional<std::size_t> total;
  std::optional<double> fraction;
  std::optional<std::size_t> sequenceIndex;
  std::optional<std::size_t> iteration;
};

// Optional synchronous callback. It runs on the calling solver thread.
using ProgressCallback = std::function<void(const ProgressEvent&)>;

}  // namespace xfoil

// End: include/xfoil/Progress.h

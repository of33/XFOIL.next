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

// File: include/xfoil/session/ProgressObserver.h
// Internal progress observer boundary between the public callback bridge
// and the session point orchestration. No host callback escapes through
// the numerical kernels.

#pragma once

#include <cstddef>
#include <optional>

#include "xfoil/Progress.h"
#include "xfoil/Result.h"

namespace xfoil::session {

enum class ProgressControl {
  continueOperation,
  cancelRequested,
  callbackFailed
};

class ProgressObserver {
 public:
  virtual ~ProgressObserver() = default;

  [[nodiscard]]
  virtual ProgressControl report(const ProgressEvent& event) noexcept = 0;
};

[[nodiscard]]
inline ProgressEvent makeProgressEvent(
    ProgressPhase phase, std::size_t completed, std::size_t total,
    std::optional<std::size_t> sequenceIndex = std::nullopt,
    std::optional<std::size_t> iteration = std::nullopt) {
  ProgressEvent event;
  event.phase = phase;
  event.completed = completed;
  event.total = total;
  if (total > 0) {
    event.fraction = static_cast<double>(completed) /
                     static_cast<double>(total);
  }
  event.sequenceIndex = sequenceIndex;
  event.iteration = iteration;
  return event;
}

[[nodiscard]]
inline Error progressCallbackFailureError() {
  return Error::make(ErrorCategory::invalidArgument,
                     "progress.callbackFailure",
                     "The progress callback failed", "progress");
}

}  // namespace xfoil::session

// End: include/xfoil/session/ProgressObserver.h

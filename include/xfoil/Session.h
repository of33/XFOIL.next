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

// File: include/xfoil/Session.h
// Public in-memory session API. The session owns an isolated logical
// state, accepts only memory-based inputs and returns owning L2 result
// values. It performs no file or console I/O and never terminates the
// host process. A single session is not thread-safe; separate sessions
// have isolated logical state.

#pragma once

#include <memory>
#include <string_view>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/Export.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/Types.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil {

// Alpha sequence with an exclusive end boundary, matching the existing
// session sequence contract. step must be non-zero and its direction must
// match start -> endExclusive. At most 100000 points are run.
struct AlphaSequence {
  Degrees start{};
  Degrees endExclusive{};
  Degrees step{};
};

class XFOIL_API Session {
 public:
  Session();
  ~Session();

  Session(Session&&) noexcept;
  Session& operator=(Session&&) noexcept;

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Adopts an owning airfoil geometry and derives the initial current
  // panel contour through the existing ABCOPY contract. The host geometry
  // is copied and is not mutated.
  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry);

  // Additive progress overloads. The callback runs synchronously on the
  // calling thread. Existing overloads perform the same work without an
  // observer.
  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry,
                           const ProgressCallback& progress);

  // Generates a NACA 4/5-digit geometry in memory using the existing
  // 399-point contract and adopts it like setGeometry. NACS uses the sharp
  // trailing-edge variant.
  [[nodiscard]]
  Result<geometry::AirfoilGeometry> setNaca(
      std::string_view designation);
  [[nodiscard]]
  Result<geometry::AirfoilGeometry> setNaca(
      std::string_view designation,
      const ProgressCallback& progress);
  [[nodiscard]]
  Result<geometry::AirfoilGeometry> setNacs(
      std::string_view designation);
  [[nodiscard]]
  Result<geometry::AirfoilGeometry> setNacs(
      std::string_view designation,
      const ProgressCallback& progress);

  // Stores the paneling options and regenerates the current panel through
  // the existing PANGEN path. On error the previous parameters and panel
  // remain unchanged.
  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(
      const PanelingOptions& options);
  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(
      const PanelingOptions& options,
      const ProgressCallback& progress);

  // Validates and stores the analysis configuration. On error the previous
  // configuration remains unchanged.
  [[nodiscard]]
  Result<void> configure(const AnalysisConfig& config);

  // Runs one point through the existing specified-alpha or specified-CL
  // path. A non-converged but executed point is returned as a successful
  // value with ConvergenceStatus::unconverged. Cancellation with no
  // operating point is reported as a cancelled error.
  [[nodiscard]]
  Result<OperatingPointData> solveAlpha(
      Degrees alpha,
      const CancellationToken* cancellation = nullptr);
  [[nodiscard]]
  Result<OperatingPointData> solveCl(
      double cl,
      const CancellationToken* cancellation = nullptr);
  [[nodiscard]]
  Result<OperatingPointData> solveAlpha(
      Degrees alpha,
      const CancellationToken* cancellation,
      const ProgressCallback& progress);
  [[nodiscard]]
  Result<OperatingPointData> solveCl(
      double cl,
      const CancellationToken* cancellation,
      const ProgressCallback& progress);

  // Runs an alpha sequence through the existing sequence path. The result
  // owns all points and diagnostics; partial results are returned when a
  // later point fails or the sequence is cancelled.
  [[nodiscard]]
  Result<SequenceResult> solveAlphaSequence(
      const AlphaSequence& sequence,
      const CancellationToken* cancellation = nullptr);
  [[nodiscard]]
  Result<SequenceResult> solveAlphaSequence(
      const AlphaSequence& sequence,
      const CancellationToken* cancellation,
      const ProgressCallback& progress);

  [[nodiscard]]
  Result<SurfaceGeometry> currentPanel() const;
  [[nodiscard]]
  Result<SurfaceDistribution> currentSurfaceDistribution() const;
  [[nodiscard]]
  Result<OperatingPointData> lastOperatingPoint() const;

  // Projects the active stored polar. L3 exposes no public polar
  // accumulation operation, so this normally reports polar.noActiveSlot
  // until a later polar API stores a slot.
  [[nodiscard]]
  Result<SequenceResult> activePolar() const;

  // Restores the canonical initial state of a fresh session. Only this
  // session is affected. No file or console output is produced.
  void reset() noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xfoil

// End: include/xfoil/Session.h

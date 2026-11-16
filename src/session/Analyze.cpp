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

// File: src/session/Analyze.cpp
// One-shot convenience layer over the public L3 Session. It owns no
// solver logic and performs no file or console I/O.

#include "xfoil/Analyze.h"

#include <type_traits>
#include <variant>

#include "xfoil/Session.h"

namespace xfoil {
namespace {

Result<OperatingPointData> analyzeImpl(
    const AnalysisRequest& request,
    const CancellationToken* cancellation,
    const ProgressCallback* progress) {
  Session session;

  const auto setGeometry = [&]() -> Result<void> {
    if (progress == nullptr) {
      return session.setGeometry(request.geometry);
    }
    return session.setGeometry(request.geometry, *progress);
  };
  const Result<void> geometry = setGeometry();
  if (geometry.isErr()) {
    return Result<OperatingPointData>::fail(geometry.error());
  }

  const auto generatePanel = [&]() -> Result<SurfaceGeometry> {
    if (progress == nullptr) {
      return session.generatePanel(request.paneling);
    }
    return session.generatePanel(request.paneling, *progress);
  };
  const Result<SurfaceGeometry> panel = generatePanel();
  if (panel.isErr()) {
    return Result<OperatingPointData>::fail(panel.error());
  }

  const Result<void> configuration =
      session.configure(request.configuration);
  if (configuration.isErr()) {
    return Result<OperatingPointData>::fail(configuration.error());
  }

  return std::visit(
      [&](const auto& target) -> Result<OperatingPointData> {
        using Target = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<Target, AlphaTarget>) {
          if (progress == nullptr) {
            return session.solveAlpha(target.alpha, cancellation);
          }
          return session.solveAlpha(target.alpha, cancellation, *progress);
        } else {
          if (progress == nullptr) {
            return session.solveCl(target.cl, cancellation);
          }
          return session.solveCl(target.cl, cancellation, *progress);
        }
      },
      request.target);
}

}  // namespace

Result<OperatingPointData> analyze(
    const AnalysisRequest& request,
    const CancellationToken* cancellation) {
  return analyzeImpl(request, cancellation, nullptr);
}

Result<OperatingPointData> analyze(
    const AnalysisRequest& request,
    const CancellationToken* cancellation,
    const ProgressCallback& progress) {
  return analyzeImpl(request, cancellation, &progress);
}

}  // namespace xfoil

// End: src/session/Analyze.cpp

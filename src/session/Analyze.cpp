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

Result<OperatingPointData> analyze(
    const AnalysisRequest& request,
    const CancellationToken* cancellation) {
  Session session;

  const Result<void> geometry = session.setGeometry(request.geometry);
  if (geometry.isErr()) {
    return Result<OperatingPointData>::fail(geometry.error());
  }

  const Result<SurfaceGeometry> panel =
      session.generatePanel(request.paneling);
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
          return session.solveAlpha(target.alpha, cancellation);
        } else {
          return session.solveCl(target.cl, cancellation);
        }
      },
      request.target);
}

}  // namespace xfoil

// End: src/session/Analyze.cpp

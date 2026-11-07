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

// File: include/xfoil/XFoilSession.h
// Modern session API: the session owns the domain-specific,
// separated session composition (xfoil::session::SessionComposition) and
// delegates internally only to the session and use-case operations
// (xfoil/session/SessionOps.h) and the confirmed core kernels.
// No app dependency, no
// static solver state. GeometrySetError/GeometrySetResult live in the
// neutral value-type header xfoil/SessionTypes.h; this facade header
// includes it (public API -> session base layer is the
// intended direction; the base layer does not depend on this facade
// itself). The session owns its geometry itself,
// exposes no mutable internal solver objects and writes
// nothing directly to stdout or stderr. VACCEL and the abort state are
// instance-specific; thread safety is explicitly not
// guaranteed in this step.

#pragma once

#include <memory>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/SessionTypes.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionState.h"

namespace xfoil {

class XFoilSession {
 public:
  XFoilSession();
  ~XFoilSession();

  XFoilSession(XFoilSession&&);
  XFoilSession& operator=(XFoilSession&&);

  XFoilSession(const XFoilSession&) = delete;
  XFoilSession& operator=(const XFoilSession&) = delete;

  [[nodiscard]]
  GeometrySetResult setGeometry(
      geometry::AirfoilGeometry geometry);

  [[nodiscard]]
  ConfigValidationResult configure(
      const AnalysisConfig& config);

  [[nodiscard]]
  AnalysisResult analyzeAtAlpha(Degrees alpha);

  [[nodiscard]]
  AnalysisResult analyzeAtAlpha(
      Degrees alpha,
      const CancellationToken& cancellation);

  [[nodiscard]]
  std::vector<AnalysisResult> analyzePolar(
      Degrees start,
      Degrees endExclusive,
      Degrees step);

  [[nodiscard]]
  std::vector<AnalysisResult> analyzePolar(
      Degrees start,
      Degrees endExclusive,
      Degrees step,
      const CancellationToken& cancellation);

  [[nodiscard]]
  const geometry::AirfoilGeometry& geometry() const noexcept;

  [[nodiscard]]
  const AnalysisConfig& config() const noexcept;

  [[nodiscard]]
  const AnalysisResult& lastResult() const noexcept;

  [[nodiscard]]
  bool hasGeometry() const noexcept;

  [[nodiscard]]
  bool hasConfiguration() const noexcept;

 private:
  // Domain-separated session composition. The public
  // methods delegate to xfoil::session::SessionOps.
  session::SessionComposition state_;
};

}  // namespace xfoil

// End: include/xfoil/XFoilSession.h

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

// File: include/xfoil/XFoil.h
// Modern facade: small, owning API below namespace xfoil.
// No raw-pointer parameters, no public solver arrays or internal
// flags, no static mutable solver state. The implementation
// is hidden behind an opaque implementation pointer and delegates only to XFoilSession.
// The class is movable but not copyable.

#pragma once

#include <memory>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/XFoilSession.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil {

class XFoil {
 public:
  XFoil();
  ~XFoil();

  XFoil(XFoil&&) noexcept;
  XFoil& operator=(XFoil&&) noexcept;

  XFoil(const XFoil&) = delete;
  XFoil& operator=(const XFoil&) = delete;

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
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xfoil

// End: include/xfoil/XFoil.h

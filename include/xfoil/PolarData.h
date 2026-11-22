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

// File: include/xfoil/PolarData.h
// Public owning in-memory polar data. Values mirror the accepted PolarText
// fields without exposing core containers, session state or file paths.

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/SequenceResult.h"

namespace xfoil {

inline constexpr std::size_t maxPolarTextPoints = 800;
inline constexpr std::size_t maxPolarTextElements = 5;
inline constexpr std::size_t polarTextSideCount = 2;
inline constexpr std::size_t polarTextElementColumnCount = 4;

enum class PolarColumn {
  alpha,
  liftCoefficient,
  dragCoefficient,
  momentCoefficient,
  dragCoefficientWave,
  dragCoefficientViscous,
  dragCoefficientPressure,
  machNumber,
  reynoldsNumber,
  hingeMoment,
  minimumPressureCoefficient,
  hingeDragCoefficient,
  momentCoefficientDerivative
};

enum class PolarSideColumn {
  nCritical,
  tripX,
  transitionX,
  transitionIndex
};

enum class PolarElementQuantity { cx, cy, cm, cdVisc };

struct PolarElementSelection {
  std::array<std::optional<PolarElementQuantity>,
             polarTextElementColumnCount>
      columns{};
  std::size_t count{0};
};

struct PolarSideData {
  double nCritical{0.0};
  double tripX{0.0};
  double transitionX{0.0};
  double transitionIndex{0.0};
};

struct PolarElementData {
  double cx{0.0};
  double cy{0.0};
  double cm{0.0};
  double cdVisc{0.0};
};

struct PolarMetadata {
  std::string name;
  std::string code{"XFOIL"};
  double version{6.99};
  ReynoldsDependency reynoldsDependency{ReynoldsDependency::constant};
  MachDependency machDependency{MachDependency::constant};
  double referenceReynoldsNumber{0.0};
  double referenceMachNumber{0.0};
  std::array<double, polarTextSideCount> nCritical{};
  std::array<double, polarTextSideCount> tripX{};
  double pressureParameter{0.0};
  double etaParameter{0.0};
  std::size_t elementCount{1};
};

struct PolarColumnLayout {
  std::vector<PolarColumn> baseColumns;
  std::vector<PolarSideColumn> sideColumns;
  PolarElementSelection elementSelection;
};

// One in-memory polar. points is the canonical scalar row set; sideData and
// elementData stay aligned with points when their corresponding layouts are
// present. Imported PolarText points are accepted stored points and therefore
// carry ConvergenceStatus::converged; the text format has no separate
// convergence column.
struct PolarData {
  PolarMetadata metadata;
  std::vector<PolarPoint> points;
  std::vector<std::array<PolarSideData, polarTextSideCount>> sideData;
  std::vector<std::array<PolarElementData, maxPolarTextElements>>
      elementData;
  PolarColumnLayout layout;
};

}  // namespace xfoil

// End: include/xfoil/PolarData.h

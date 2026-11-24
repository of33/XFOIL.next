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

// File: src/gui/plots/ScalarPlotView.h
// Five scalar sequence plots. Data are owning GuiPointEntry values; the
// view performs no solve, interpolation or summary calculation.

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QWidget>

#include "model/GuiResultSelectionModel.h"

class QCPCurve;
class QCPGraph;
class QCustomPlot;

namespace xfoil::gui {

class InteractivePlotController;

enum class GuiScalarPlotKind {
  clAlpha,
  clCd,
  cmAlpha,
  cmCl,
  efficiencyCl
};

struct ScalarComparisonSeries {
  std::string name;
  std::vector<std::pair<double, double>> points;
  std::size_t styleIndex{static_cast<std::size_t>(-1)};
};

class ScalarPlotView : public QWidget {
 public:
  explicit ScalarPlotView(QWidget* parent = nullptr);
  ~ScalarPlotView() override;

  ScalarPlotView(const ScalarPlotView&) = delete;
  ScalarPlotView& operator=(const ScalarPlotView&) = delete;

  void setKind(GuiScalarPlotKind kind);
  [[nodiscard]]
  GuiScalarPlotKind kind() const noexcept {
    return kind_;
  }

  void setPoints(const std::vector<GuiPointEntry>& points,
                 std::optional<std::size_t> selectedSourceIndex,
                 bool fitData = false);
  void setComparisons(const std::vector<ScalarComparisonSeries>& series,
                      bool fitData = false);
  void setSelectionCallback(std::function<void(std::size_t)> callback);
  void fitVisibleData();

  [[nodiscard]]
  std::size_t visiblePointCount() const noexcept;
  [[nodiscard]]
  std::size_t comparisonCurveCount() const noexcept;
  [[nodiscard]]
  std::size_t comparisonPointCount() const noexcept;
  [[nodiscard]]
  std::size_t solidPointCount() const noexcept;
  [[nodiscard]]
  std::size_t dashedPointCount() const noexcept;
  [[nodiscard]]
  std::size_t unselectedMarkerCount() const noexcept;
  [[nodiscard]]
  std::size_t selectedMarkerCount() const noexcept;
  [[nodiscard]]
  std::optional<std::size_t> selectedSourceIndex() const noexcept {
    return selectedSourceIndex_;
  }

 private:
  struct DisplayPoint {
    std::size_t sourceIndex{};
    double x{};
    double y{};
    bool converged{false};
  };

  void updateGraphs();
  void handleClick(const QPoint& position);
  [[nodiscard]]
  std::optional<DisplayPoint> pointFor(
      const GuiPointEntry& point) const;
  void applyAxisTitles();

  QCustomPlot* plot_{};
  QCPCurve* solidBefore_{};
  QCPCurve* solidAfter_{};
  QCPCurve* dashed_{};
  QCPGraph* unselectedMarkers_{};
  QCPGraph* selectedMarker_{};
  InteractivePlotController* interaction_{};
  GuiScalarPlotKind kind_{GuiScalarPlotKind::clAlpha};
  std::array<QCPCurve*, 8> comparisonCurves_{};
  std::array<std::size_t, 8> comparisonPointCounts_{};
  std::vector<DisplayPoint> visiblePoints_;
  std::optional<std::size_t> selectedSourceIndex_;
  std::function<void(std::size_t)> selectionCallback_;
};

}  // namespace xfoil::gui

// End: src/gui/plots/ScalarPlotView.h

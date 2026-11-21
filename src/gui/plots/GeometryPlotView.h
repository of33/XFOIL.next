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

// File: src/gui/plots/GeometryPlotView.h
// Geometry and panel preview. The view accepts owning public data values,
// never a Session, and performs no aerodynamic calculation or export.

#pragma once

#include <cstddef>

#include <QWidget>

#include "xfoil/AnalysisData.h"
#include "xfoil/geometry/AirfoilGeometry.h"

class QCPCurve;
class QCPGraph;
class QCustomPlot;
class QResizeEvent;

namespace xfoil::gui {

class InteractivePlotController;

class GeometryPlotView : public QWidget {
 public:
  explicit GeometryPlotView(QWidget* parent = nullptr);
  ~GeometryPlotView() override;

  GeometryPlotView(const GeometryPlotView&) = delete;
  GeometryPlotView& operator=(const GeometryPlotView&) = delete;

  void setGeometry(const geometry::AirfoilGeometry& geometry);
  void setPanelGeometry(const SurfaceGeometry& panel);
  void showPanelNodes(bool visible);
  void clearPanel();
  void clear();
  void fitVisibleData();

  [[nodiscard]]
  bool sampleMarkersEnabled() const noexcept {
    return sampleMarkersEnabled_;
  }
  void setSampleMarkersEnabled(bool enabled);

  [[nodiscard]]
  std::size_t geometryPointCount() const noexcept {
    return geometryPointCount_;
  }

  [[nodiscard]]
  std::size_t panelPointCount() const noexcept {
    return panelPointCount_;
  }

  [[nodiscard]]
  bool panelNodesVisible() const noexcept {
    return panelNodesVisible_;
  }

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void updateAxes();
  void applySampleMarkers();
  void applyEqualAspect();

  QCustomPlot* plot_{};
  QCPCurve* geometryContour_{};
  QCPCurve* panelContour_{};
  QCPGraph* panelNodes_{};
  InteractivePlotController* interaction_{};
  std::size_t geometryPointCount_{0};
  std::size_t panelPointCount_{0};
  bool panelNodesVisible_{true};
  bool sampleMarkersEnabled_{false};
  bool hasGeometry_{false};
  bool hasPanel_{false};
  double xMinimum_{0.0};
  double xMaximum_{1.0};
  double yMinimum_{0.0};
  double yMaximum_{0.0};
};

}  // namespace xfoil::gui

// End: src/gui/plots/GeometryPlotView.h

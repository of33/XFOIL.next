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

// File: src/gui/plots/GeometryPlotView.cpp
// QCustomPlot adapter for public geometry and SurfaceGeometry values.

#include "plots/GeometryPlotView.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QResizeEvent>
#include <QVBoxLayout>
#include <QVector>

#include "qcustomplot.h"

namespace xfoil::gui {
namespace {

struct Bounds {
  double xMinimum{0.0};
  double xMaximum{1.0};
  double yMinimum{0.0};
  double yMaximum{0.0};
  bool valid{false};

  void include(double x, double y) {
    if (!valid) {
      xMinimum = x;
      xMaximum = x;
      yMinimum = y;
      yMaximum = y;
      valid = true;
      return;
    }
    xMinimum = std::min(xMinimum, x);
    xMaximum = std::max(xMaximum, x);
    yMinimum = std::min(yMinimum, y);
    yMaximum = std::max(yMaximum, y);
  }
};

QVector<double> closedX(const std::vector<xfoil::Point2D>& points) {
  QVector<double> values;
  values.reserve(static_cast<int>(points.size()) + 1);
  for (const xfoil::Point2D& point : points) {
    values.push_back(point.x);
  }
  if (!points.empty()) {
    values.push_back(points.front().x);
  }
  return values;
}

QVector<double> closedY(const std::vector<xfoil::Point2D>& points) {
  QVector<double> values;
  values.reserve(static_cast<int>(points.size()) + 1);
  for (const xfoil::Point2D& point : points) {
    values.push_back(point.y);
  }
  if (!points.empty()) {
    values.push_back(points.front().y);
  }
  return values;
}

QVector<double> nodeX(const std::vector<xfoil::Point2D>& points) {
  QVector<double> values;
  values.reserve(static_cast<int>(points.size()));
  for (const xfoil::Point2D& point : points) {
    values.push_back(point.x);
  }
  return values;
}

QVector<double> nodeY(const std::vector<xfoil::Point2D>& points) {
  QVector<double> values;
  values.reserve(static_cast<int>(points.size()));
  for (const xfoil::Point2D& point : points) {
    values.push_back(point.y);
  }
  return values;
}

}  // namespace

GeometryPlotView::GeometryPlotView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  plot_ = new QCustomPlot(this);
  layout->addWidget(plot_);

  plot_->setObjectName(QStringLiteral("geometryPlot"));
  plot_->xAxis->setLabel(QStringLiteral("x/c"));
  plot_->yAxis->setLabel(QStringLiteral("y/c"));

  geometryContour_ = plot_->addGraph();
  geometryContour_->setName(QStringLiteral("Geometry contour"));
  geometryContour_->setPen(QPen(QColor(32, 78, 121), 2.0));

  panelContour_ = plot_->addGraph();
  panelContour_->setName(QStringLiteral("Panel contour"));
  panelContour_->setPen(QPen(QColor(198, 93, 27), 1.5));

  panelNodes_ = plot_->addGraph();
  panelNodes_->setName(QStringLiteral("Panel nodes"));
  panelNodes_->setLineStyle(QCPGraph::lsNone);
  panelNodes_->setScatterStyle(
      QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(198, 93, 27), 3.0));

  clear();
}

GeometryPlotView::~GeometryPlotView() = default;

void GeometryPlotView::setGeometry(
    const geometry::AirfoilGeometry& geometry) {
  const std::vector<Point2D> points(geometry.coordinates().begin(),
                                    geometry.coordinates().end());
  geometryPointCount_ = points.size();
  hasGeometry_ = !points.empty();
  geometryContour_->setData(closedX(points), closedY(points));
  geometryContour_->setVisible(hasGeometry_);
  updateAxes();
}

void GeometryPlotView::setPanelGeometry(const SurfaceGeometry& panel) {
  panelPointCount_ = panel.points.size();
  hasPanel_ = !panel.points.empty();
  panelContour_->setData(closedX(panel.points), closedY(panel.points));
  panelNodes_->setData(nodeX(panel.points), nodeY(panel.points));
  panelContour_->setVisible(hasPanel_);
  panelNodes_->setVisible(hasPanel_ && panelNodesVisible_);
  updateAxes();
}

void GeometryPlotView::showPanelNodes(bool visible) {
  panelNodesVisible_ = visible;
  panelNodes_->setVisible(hasPanel_ && panelNodesVisible_);
  plot_->replot();
}

void GeometryPlotView::clearPanel() {
  panelPointCount_ = 0;
  hasPanel_ = false;
  panelContour_->data()->clear();
  panelNodes_->data()->clear();
  panelContour_->setVisible(false);
  panelNodes_->setVisible(false);
  updateAxes();
}

void GeometryPlotView::clear() {
  geometryPointCount_ = 0;
  panelPointCount_ = 0;
  hasGeometry_ = false;
  hasPanel_ = false;
  geometryContour_->data()->clear();
  panelContour_->data()->clear();
  panelNodes_->data()->clear();
  geometryContour_->setVisible(false);
  panelContour_->setVisible(false);
  panelNodes_->setVisible(false);
  updateAxes();
}

void GeometryPlotView::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateAxes();
}

void GeometryPlotView::updateAxes() {
  Bounds bounds;

  if (hasGeometry_) {
    const auto data = geometryContour_->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
      bounds.include(it->key, it->value);
    }
  }
  if (hasPanel_) {
    const auto data = panelContour_->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
      bounds.include(it->key, it->value);
    }
  }

  if (!bounds.valid) {
    bounds.xMinimum = 0.0;
    bounds.xMaximum = 1.0;
    bounds.yMinimum = 0.0;
    bounds.yMaximum = 0.0;
  }

  const double xSpan = std::max(bounds.xMaximum - bounds.xMinimum, 0.1);
  const double ySpan = std::max(bounds.yMaximum - bounds.yMinimum, 0.1);
  const double span = std::max({xSpan, ySpan, 0.1});
  const double padding = span * 0.06 + 0.01;
  const double xCenter = 0.5 * (bounds.xMinimum + bounds.xMaximum);
  const double yCenter = 0.5 * (bounds.yMinimum + bounds.yMaximum);

  plot_->xAxis->setRange(xCenter - 0.5 * span - padding,
                         xCenter + 0.5 * span + padding);
  plot_->yAxis->setRange(yCenter, 0.1);
  plot_->yAxis->setScaleRatio(plot_->xAxis, 1.0);
  plot_->replot();
}

}  // namespace xfoil::gui

// End: src/gui/plots/GeometryPlotView.cpp

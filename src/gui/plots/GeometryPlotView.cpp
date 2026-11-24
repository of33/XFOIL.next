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
#include <cmath>
#include <utility>
#include <vector>

#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVector>

#include "plots/InteractivePlotController.h"
#include "plots/PlotStyle.h"
#include "qcustomplot.h"

namespace xfoil::gui {
namespace {

}  // namespace
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

void setCurveData(QCPCurve* curve, const QVector<double>& x,
                  const QVector<double>& y) {
  QVector<double> t;
  t.reserve(x.size());
  for (int index = 0; index < x.size(); ++index) {
    t.push_back(index);
  }
  curve->setData(t, x, y, true);
}

}  // namespace

GeometryPlotView::GeometryPlotView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  plot_ = new QCustomPlot(this);
  plot_->setMinimumSize(0, 0);
  plot_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  layout->addWidget(plot_);

  plot_->setObjectName(QStringLiteral("geometryPlot"));
  plot_->xAxis->setLabel(QStringLiteral("x/c"));
  plot_->yAxis->setLabel(QStringLiteral("y/c"));

  geometryContour_ = new QCPCurve(plot_->xAxis, plot_->yAxis);
  geometryContour_->setObjectName(QStringLiteral("geometryContour"));
  geometryContour_->setName(QStringLiteral("Geometry contour"));
  geometryContour_->setPen(
      QPen(QColor(32, 78, 121), kPrimaryLineWidth));
  geometryContour_->setLineStyle(QCPCurve::lsLine);
  geometryContour_->setScatterStyle(QCPScatterStyle::ssNone);

  panelContour_ = new QCPCurve(plot_->xAxis, plot_->yAxis);
  panelContour_->setObjectName(QStringLiteral("panelContour"));
  panelContour_->setName(QStringLiteral("Panel contour"));
  panelContour_->setPen(
      QPen(QColor(200, 30, 30), kPanelContourLineWidth));
  panelContour_->setLineStyle(QCPCurve::lsLine);
  panelContour_->setScatterStyle(QCPScatterStyle::ssNone);

  panelTicks_ = new QCPCurve(plot_->xAxis, plot_->yAxis);
  panelTicks_->setObjectName(QStringLiteral("panelTicks"));
  panelTicks_->setName(QStringLiteral("Panel node ticks"));
  panelTicks_->setPen(
      QPen(QColor(200, 30, 30), kPanelTickLineWidth));
  panelTicks_->setLineStyle(QCPCurve::lsLine);
  panelTicks_->setScatterStyle(QCPScatterStyle::ssNone);

  interaction_ = new InteractivePlotController(plot_, false, true, this);
  interaction_->setFitCallback([this]() { fitVisibleData(); });
  interaction_->setMarkerToggleCallback([this]() {
    setShowPaneling(!showPaneling_);
    if (panelingToggleCallback_) {
      panelingToggleCallback_(showPaneling_);
    }
  });

  clear();
}

GeometryPlotView::~GeometryPlotView() = default;

void GeometryPlotView::setGeometry(
    const geometry::AirfoilGeometry& geometry) {
  const std::vector<Point2D> points(geometry.coordinates().begin(),
                                    geometry.coordinates().end());
  geometryPointCount_ = points.size();
  hasGeometry_ = !points.empty();
  setCurveData(geometryContour_, closedX(points), closedY(points));
  geometryContour_->setVisible(hasGeometry_);
  updateAxes();
}

void GeometryPlotView::setPanelGeometry(const SurfaceGeometry& panel) {
  panelPointCount_ = panel.points.size();
  hasPanel_ = !panel.points.empty();
  setCurveData(panelContour_, closedX(panel.points), closedY(panel.points));

  QVector<double> tickT;
  QVector<double> tickX;
  QVector<double> tickY;
  panelTickCount_ = 0;
  if (panel.points.size() >= 2) {
    double xMinimum = panel.points.front().x;
    double xMaximum = panel.points.front().x;
    for (const Point2D& point : panel.points) {
      xMinimum = std::min(xMinimum, point.x);
      xMaximum = std::max(xMaximum, point.x);
    }
    const double chord = xMaximum - xMinimum;
    const double tickLength =
        0.005 * (std::isfinite(chord) && chord > 1.0e-12 ? chord : 1.0);
    constexpr double kDegenerate = 1.0e-14;
    const auto unit = [](const Point2D& from, const Point2D& to,
                         Point2D& result) {
      const double dx = to.x - from.x;
      const double dy = to.y - from.y;
      const double length = std::hypot(dx, dy);
      if (!std::isfinite(length) || length <= kDegenerate) {
        return false;
      }
      result = {dx / length, dy / length};
      return true;
    };
    for (std::size_t index = 0; index < panel.points.size(); ++index) {
      const Point2D& current = panel.points[index];
      Point2D left{};
      Point2D right{};
      const bool hasLeft =
          index > 0 && unit(panel.points[index - 1], current, left);
      const bool hasRight =
          index + 1 < panel.points.size() &&
          unit(current, panel.points[index + 1], right);
      Point2D tangent{};
      if (hasLeft && hasRight) {
        const double sumX = left.x + right.x;
        const double sumY = left.y + right.y;
        const double sumLength = std::hypot(sumX, sumY);
        if (!std::isfinite(sumLength) || sumLength <= kDegenerate) {
          // Nearly opposite tangents have no unique outward normal.
          continue;
        }
        tangent = {sumX / sumLength, sumY / sumLength};
      } else if (hasRight) {
        tangent = right;
      } else if (hasLeft) {
        tangent = left;
      } else {
        continue;
      }
      // Canonical panel contours are counter-clockwise, so the right-hand
      // normal (dy, -dx) points from the surface to the outside.
      const double normalX = tangent.y;
      const double normalY = -tangent.x;
      const double endX = current.x + tickLength * normalX;
      const double endY = current.y + tickLength * normalY;
      if (!std::isfinite(endX) || !std::isfinite(endY)) {
        continue;
      }
      const double t = static_cast<double>(tickT.size());
      tickT << t << t + 1.0 << t + 2.0;
      tickX << current.x << endX << current.x;
      tickY << current.y << endY << current.y;
      ++panelTickCount_;
    }
  }
  panelTicks_->setData(tickT, tickX, tickY, true);

  updatePanelVisibility();
  updateAxes();
}

void GeometryPlotView::setShowPaneling(bool visible) {
  showPaneling_ = visible;
  updatePanelVisibility();
  plot_->replot();
}

void GeometryPlotView::showPanelNodes(bool visible) {
  panelTicksVisible_ = visible;
  updatePanelVisibility();
  plot_->replot();
}

void GeometryPlotView::setPanelingToggleCallback(
    std::function<void(bool)> callback) {
  panelingToggleCallback_ = std::move(callback);
}

void GeometryPlotView::updatePanelVisibility() {
  panelContour_->setVisible(hasPanel_ && showPaneling_);
  panelTicks_->setVisible(hasPanel_ && showPaneling_ &&
                          panelTicksVisible_ && panelTickCount_ > 0);
}

void GeometryPlotView::clearPanel() {
  panelPointCount_ = 0;
  panelTickCount_ = 0;
  hasPanel_ = false;
  panelContour_->data()->clear();
  panelTicks_->data()->clear();
  panelContour_->setVisible(false);
  panelTicks_->setVisible(false);
  updateAxes();
}

void GeometryPlotView::clear() {
  geometryPointCount_ = 0;
  panelPointCount_ = 0;
  panelTickCount_ = 0;
  hasGeometry_ = false;
  hasPanel_ = false;
  geometryContour_->data()->clear();
  panelContour_->data()->clear();
  panelTicks_->data()->clear();
  geometryContour_->setVisible(false);
  panelContour_->setVisible(false);
  panelTicks_->setVisible(false);
  plot_->xAxis->setRange(0.0, 1.0);
  plot_->yAxis->setRange(-0.01, 0.01);
  plot_->replot();
}

void GeometryPlotView::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
}

void GeometryPlotView::fitVisibleData() {
  if (!hasGeometry_ && !hasPanel_) {
    return;
  }
  updateAxes();
}

void GeometryPlotView::setSampleMarkersEnabled(bool enabled) {
  setShowPaneling(enabled);
}

void GeometryPlotView::applySampleMarkers() {
  updatePanelVisibility();
  plot_->replot();
}

void GeometryPlotView::updateAxes() {
  Bounds bounds;

  if (hasGeometry_) {
    const auto data = geometryContour_->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
      if (std::isfinite(it->key) && std::isfinite(it->value)) {
        bounds.include(it->key, it->value);
      }
    }
  }
  if (hasPanel_) {
    const auto data = panelContour_->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
      if (std::isfinite(it->key) && std::isfinite(it->value)) {
        bounds.include(it->key, it->value);
      }
    }
  }

  if (!bounds.valid) {
    plot_->xAxis->setRange(0.0, 1.0);
    plot_->yAxis->setRange(-0.01, 0.01);
    plot_->replot();
    return;
  }

  constexpr double kXContractMinimum = 0.0;
  constexpr double kXContractMaximum = 1.0;
  constexpr double kYPad = 0.01;
  const double xMinimum = std::min(kXContractMinimum, bounds.xMinimum);
  const double xMaximum = std::max(kXContractMaximum, bounds.xMaximum);
  const double yMinimum = bounds.yMinimum - kYPad;
  const double yMaximum = bounds.yMaximum + kYPad;
  plot_->xAxis->setRange(xMinimum, xMaximum);
  plot_->yAxis->setRange(yMinimum, yMaximum);
  plot_->replot();
}

}  // namespace xfoil::gui

// End: src/gui/plots/GeometryPlotView.cpp

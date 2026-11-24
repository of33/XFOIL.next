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

// File: src/gui/plots/ScalarPlotView.cpp
// Complete enabled scalar curves, blue/open and red/filled markers, and
// dashed selected-NC neighbor segments.

#include "plots/ScalarPlotView.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QPoint>
#include <QVector>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "model/GuiResultSummary.h"
#include "plots/ComparisonStyle.h"
#include "plots/InteractivePlotController.h"
#include "plots/PlotStyle.h"
#include "qcustomplot.h"

namespace xfoil::gui {
namespace {

const QColor kLineColor(34, 93, 148);
const QColor kMarkerColor(34, 93, 148);
const QColor kSelectedColor(205, 45, 45);
const QColor kDashedColor(128, 72, 130);


QCPScatterStyle openCircle() {
  return QCPScatterStyle(QCPScatterStyle::ssCircle,
                         QPen(kMarkerColor, 1.1), Qt::NoBrush, 5.0);
}

QCPScatterStyle filledCircle() {
  return QCPScatterStyle(QCPScatterStyle::ssDisc, kSelectedColor,
                         kSelectedColor, 7.0);
}

struct CurvePoint {
  double t{};
  double x{};
  double y{};
};

void setCurveData(QCPCurve* curve,
                  const std::vector<CurvePoint>& data) {
  QVector<double> t;
  QVector<double> x;
  QVector<double> y;
  t.reserve(static_cast<int>(data.size()));
  x.reserve(static_cast<int>(data.size()));
  y.reserve(static_cast<int>(data.size()));
  for (const CurvePoint& point : data) {
    t.push_back(point.t);
    x.push_back(point.x);
    y.push_back(point.y);
  }
  curve->setData(t, x, y, true);
}

void setMarkerData(QCPGraph* graph,
                   const std::vector<std::pair<double, double>>& data) {
  QVector<double> x;
  QVector<double> y;
  x.reserve(static_cast<int>(data.size()));
  y.reserve(static_cast<int>(data.size()));
  for (const auto& [key, value] : data) {
    x.push_back(key);
    y.push_back(value);
  }
  graph->setData(x, y, false);
}

}  // namespace

ScalarPlotView::ScalarPlotView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  plot_ = new QCustomPlot(this);
  plot_->setObjectName(QStringLiteral("plotCanvas"));
  layout->addWidget(plot_);

  solidBefore_ = new QCPCurve(plot_->xAxis, plot_->yAxis);
  solidBefore_->setObjectName(QStringLiteral("solidBefore"));
  solidBefore_->setName(QStringLiteral("Scalar curve"));
  solidBefore_->setPen(QPen(kLineColor, kPrimaryLineWidth));
  solidBefore_->setLineStyle(QCPCurve::lsLine);
  solidBefore_->setScatterStyle(QCPScatterStyle::ssNone);

  solidAfter_ = new QCPCurve(plot_->xAxis, plot_->yAxis);
  solidAfter_->setObjectName(QStringLiteral("solidAfter"));
  solidAfter_->setName(QStringLiteral("Scalar curve after NC"));
  solidAfter_->setPen(QPen(kLineColor, kPrimaryLineWidth));
  solidAfter_->setLineStyle(QCPCurve::lsLine);
  solidAfter_->setScatterStyle(QCPScatterStyle::ssNone);

  dashed_ = new QCPCurve(plot_->xAxis, plot_->yAxis);
  dashed_->setObjectName(QStringLiteral("ncDashed"));
  dashed_->setName(QStringLiteral("Selected NC neighbors"));
  QPen dashedPen(kDashedColor, kDashedLineWidth);
  dashedPen.setStyle(Qt::DashLine);
  dashed_->setPen(dashedPen);
  dashed_->setLineStyle(QCPCurve::lsLine);
  dashed_->setScatterStyle(QCPScatterStyle::ssNone);

  unselectedMarkers_ = plot_->addGraph();
  unselectedMarkers_->setObjectName(QStringLiteral("unselectedMarkers"));
  unselectedMarkers_->setName(QStringLiteral("Converged points"));
  unselectedMarkers_->setLineStyle(QCPGraph::lsNone);
  unselectedMarkers_->setScatterStyle(openCircle());

  selectedMarker_ = plot_->addGraph();
  selectedMarker_->setObjectName(QStringLiteral("selectedMarker"));
  selectedMarker_->setName(QStringLiteral("Selected point"));
  selectedMarker_->setLineStyle(QCPGraph::lsNone);
  selectedMarker_->setScatterStyle(filledCircle());

  interaction_ = new InteractivePlotController(plot_, false, false, this);
  interaction_->setPointSelectionEnabled(true);
  interaction_->setClickCallback(
      [this](const QPoint& position) { handleClick(position); });
  interaction_->setFitCallback([this]() { fitVisibleData(); });

  applyAxisTitles();
  setPoints({}, std::nullopt, true);
  setComparisons({}, false);
}

ScalarPlotView::~ScalarPlotView() = default;

void ScalarPlotView::setKind(GuiScalarPlotKind kind) {
  kind_ = kind;
  applyAxisTitles();
}

void ScalarPlotView::setPoints(
    const std::vector<GuiPointEntry>& points,
    std::optional<std::size_t> selectedSourceIndex, bool fitData) {
  selectedSourceIndex_ = selectedSourceIndex;
  visiblePoints_.clear();
  for (const GuiPointEntry& point : points) {
    if (!point.enabled) {
      continue;
    }
    const bool selected = selectedSourceIndex.has_value() &&
                          *selectedSourceIndex == point.sourceIndex;
    if (!point.converged && !selected) {
      continue;
    }
    const std::optional<DisplayPoint> display = pointFor(point);
    if (!display.has_value()) {
      continue;
    }
    visiblePoints_.push_back(*display);
  }
  std::sort(visiblePoints_.begin(), visiblePoints_.end(),
            [](const DisplayPoint& left, const DisplayPoint& right) {
              return left.sourceIndex < right.sourceIndex;
            });
  updateGraphs();
  if (fitData) {
    fitVisibleData();
  }
}

void ScalarPlotView::setComparisons(
    const std::vector<ScalarComparisonSeries>& series, bool fitData) {
  bool any = false;
  for (std::size_t index = 0; index < comparisonCurves_.size(); ++index) {
    QVector<double> t;
    QVector<double> x;
    QVector<double> y;
    std::size_t count = 0;
    if (index < series.size() && !series[index].points.empty()) {
      const ScalarComparisonSeries& source = series[index];
      t.reserve(static_cast<int>(source.points.size()));
      x.reserve(static_cast<int>(source.points.size()));
      y.reserve(static_cast<int>(source.points.size()));
      for (const auto& [pointX, pointY] : source.points) {
        if (!std::isfinite(pointX) || !std::isfinite(pointY)) {
          continue;
        }
        t.push_back(static_cast<double>(count));
        x.push_back(pointX);
        y.push_back(pointY);
        ++count;
      }
      if (count == 0) {
        if (comparisonCurves_[index] != nullptr) {
          plot_->removePlottable(comparisonCurves_[index]);
          comparisonCurves_[index] = nullptr;
        }
        comparisonPointCounts_[index] = 0;
        continue;
      }
      QCPCurve*& curve = comparisonCurves_[index];
      if (curve == nullptr) {
        curve = new QCPCurve(plot_->xAxis, plot_->yAxis);
        curve->setObjectName(QStringLiteral("comparisonCurve%1")
                                 .arg(static_cast<qulonglong>(index)));
        curve->setLineStyle(QCPCurve::lsLine);
        curve->setScatterStyle(QCPScatterStyle::ssNone);
      }
      const std::size_t styleIndex =
          source.styleIndex == static_cast<std::size_t>(-1)
              ? index
              : source.styleIndex;
      QPen pen(comparisonColor(styleIndex), kComparisonLineWidth);
      pen.setStyle(comparisonPenStyle(styleIndex));
      curve->setPen(pen);
      curve->setData(t, x, y, true);
      curve->setName(QString::fromStdString(source.name));
      curve->setVisible(true);
      any = true;
    } else if (comparisonCurves_[index] != nullptr) {
      plot_->removePlottable(comparisonCurves_[index]);
      comparisonCurves_[index] = nullptr;
    }
    comparisonPointCounts_[index] = count;
  }
  plot_->legend->setVisible(false);
  (void)any;
  plot_->replot();
  if (fitData) {
    fitVisibleData();
  }
}

void ScalarPlotView::setSelectionCallback(
    std::function<void(std::size_t)> callback) {
  selectionCallback_ = std::move(callback);
}

void ScalarPlotView::fitVisibleData() {
  bool haveData = false;
  double xMin = 0.0;
  double xMax = 0.0;
  double yMin = 0.0;
  double yMax = 0.0;
  const auto include = [&](double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
      return;
    }
    if (!haveData) {
      xMin = xMax = x;
      yMin = yMax = y;
      haveData = true;
      return;
    }
    xMin = std::min(xMin, x);
    xMax = std::max(xMax, x);
    yMin = std::min(yMin, y);
    yMax = std::max(yMax, y);
  };
  for (const DisplayPoint& point : visiblePoints_) {
    include(point.x, point.y);
  }
  for (QCPCurve* curve : comparisonCurves_) {
    if (curve == nullptr || !curve->visible()) {
      continue;
    }
    const auto data = curve->data();
    for (auto iterator = data->constBegin(); iterator != data->constEnd();
         ++iterator) {
      include(iterator->key, iterator->value);
    }
  }
  if (!haveData) {
    return;
  }
  const double xSpan = std::max(xMax - xMin, 1.0e-6);
  const double ySpan = std::max(yMax - yMin, 1.0e-6);
  const double xPadding = xSpan * 0.08 + 1.0e-9;
  const double yPadding = ySpan * 0.08 + 1.0e-9;
  plot_->xAxis->setRange(xMin - xPadding, xMax + xPadding);
  plot_->yAxis->setRange(yMin - yPadding, yMax + yPadding);
  if (plot_->axisRect()->width() > 0 && plot_->axisRect()->height() > 0) {
    plot_->replot();
  }
}

std::size_t ScalarPlotView::visiblePointCount() const noexcept {
  return visiblePoints_.size();
}

std::size_t ScalarPlotView::comparisonCurveCount() const noexcept {
  std::size_t count = 0;
  for (QCPCurve* curve : comparisonCurves_) {
    if (curve != nullptr && curve->visible()) {
      ++count;
    }
  }
  return count;
}

std::size_t ScalarPlotView::comparisonPointCount() const noexcept {
  std::size_t count = 0;
  for (const std::size_t value : comparisonPointCounts_) {
    count += value;
  }
  return count;
}

std::size_t ScalarPlotView::solidPointCount() const noexcept {
  return static_cast<std::size_t>(solidBefore_->dataCount() +
                                  solidAfter_->dataCount());
}

std::size_t ScalarPlotView::dashedPointCount() const noexcept {
  return static_cast<std::size_t>(dashed_->dataCount());
}

std::size_t ScalarPlotView::unselectedMarkerCount() const noexcept {
  return static_cast<std::size_t>(unselectedMarkers_->dataCount());
}

std::size_t ScalarPlotView::selectedMarkerCount() const noexcept {
  return static_cast<std::size_t>(selectedMarker_->dataCount());
}

void ScalarPlotView::applyAxisTitles() {
  switch (kind_) {
    case GuiScalarPlotKind::clAlpha:
      plot_->xAxis->setLabel(QStringLiteral("α [deg]"));
      plot_->yAxis->setLabel(QStringLiteral("CL"));
      break;
    case GuiScalarPlotKind::clCd:
      plot_->xAxis->setLabel(QStringLiteral("CD"));
      plot_->yAxis->setLabel(QStringLiteral("CL"));
      break;
    case GuiScalarPlotKind::cmAlpha:
      plot_->xAxis->setLabel(QStringLiteral("α [deg]"));
      plot_->yAxis->setLabel(QStringLiteral("CM"));
      break;
    case GuiScalarPlotKind::cmCl:
      plot_->xAxis->setLabel(QStringLiteral("CL"));
      plot_->yAxis->setLabel(QStringLiteral("CM"));
      break;
    case GuiScalarPlotKind::efficiencyCl:
      plot_->xAxis->setLabel(QStringLiteral("CL"));
      plot_->yAxis->setLabel(QStringLiteral("CL/CD"));
      break;
  }
}

std::optional<ScalarPlotView::DisplayPoint> ScalarPlotView::pointFor(
    const GuiPointEntry& point) const {
  const std::optional<double> cl = guiPointCl(point);
  const std::optional<double> cd = guiPointCd(point);
  const std::optional<double> cm = guiPointCm(point);
  const std::optional<double> efficiency = guiPointEfficiency(point);
  std::optional<double> x;
  std::optional<double> y;
  switch (kind_) {
    case GuiScalarPlotKind::clAlpha:
      x = point.alpha;
      y = cl;
      break;
    case GuiScalarPlotKind::clCd:
      x = cd;
      y = cl;
      break;
    case GuiScalarPlotKind::cmAlpha:
      x = point.alpha;
      y = cm;
      break;
    case GuiScalarPlotKind::cmCl:
      x = cl;
      y = cm;
      break;
    case GuiScalarPlotKind::efficiencyCl:
      x = cl;
      y = efficiency;
      break;
  }
  if (!x.has_value() || !y.has_value() || !std::isfinite(*x) ||
      !std::isfinite(*y)) {
    return std::nullopt;
  }
  return DisplayPoint{point.sourceIndex, *x, *y, point.converged};
}

void ScalarPlotView::updateGraphs() {
  const DisplayPoint* selected = nullptr;
  if (selectedSourceIndex_.has_value()) {
    for (const DisplayPoint& point : visiblePoints_) {
      if (point.sourceIndex == *selectedSourceIndex_) {
        selected = &point;
        break;
      }
    }
  }
  const bool selectedNotConverged = selected != nullptr && !selected->converged;

  std::vector<CurvePoint> before;
  std::vector<CurvePoint> after;
  std::vector<std::pair<double, double>> markers;
  std::vector<std::pair<double, double>> selectedData;
  std::vector<CurvePoint> dashed;

  for (const DisplayPoint& point : visiblePoints_) {
    if (!point.converged) {
      continue;
    }
    const CurvePoint data{static_cast<double>(point.sourceIndex),
                          point.x, point.y};
    if (selectedNotConverged &&
        point.sourceIndex > selected->sourceIndex) {
      after.push_back(data);
    } else {
      before.push_back(data);
    }
    if (selected == nullptr || point.sourceIndex != selected->sourceIndex) {
      markers.push_back(std::make_pair(point.x, point.y));
    }
  }

  if (selected != nullptr) {
    selectedData.push_back({selected->x, selected->y});
    if (selectedNotConverged) {
      const DisplayPoint* previous = nullptr;
      const DisplayPoint* next = nullptr;
      for (const DisplayPoint& point : visiblePoints_) {
        if (!point.converged) {
          continue;
        }
        if (point.sourceIndex < selected->sourceIndex) {
          previous = &point;
        } else if (point.sourceIndex > selected->sourceIndex) {
          next = &point;
          break;
        }
      }
      if (previous != nullptr) {
        dashed.push_back({static_cast<double>(previous->sourceIndex),
                          previous->x, previous->y});
      }
      dashed.push_back({static_cast<double>(selected->sourceIndex),
                        selected->x, selected->y});
      if (next != nullptr) {
        dashed.push_back({static_cast<double>(next->sourceIndex),
                          next->x, next->y});
      }
      if (dashed.size() < 2) {
        dashed.clear();
      }
    }
  }

  selectedMarker_->setScatterStyle(filledCircle());
  setCurveData(solidBefore_, before);
  setCurveData(solidAfter_, after);
  setCurveData(dashed_, dashed);
  setMarkerData(unselectedMarkers_, markers);
  setMarkerData(selectedMarker_, selectedData);
  solidBefore_->setVisible(!before.empty());
  solidAfter_->setVisible(!after.empty());
  dashed_->setVisible(!dashed.empty());
  unselectedMarkers_->setVisible(!markers.empty());
  selectedMarker_->setVisible(!selectedData.empty());
  if (plot_->axisRect()->width() > 0 && plot_->axisRect()->height() > 0) {
    plot_->replot();
  }
}

void ScalarPlotView::handleClick(const QPoint& position) {
  if (!plot_ || plot_->axisRect() == nullptr) {
    return;
  }
  const int radius = InteractivePlotController::selectionRadiusPixels();
  const double radiusSquared = static_cast<double>(radius) * radius;
  std::optional<DisplayPoint> best;
  double bestDistance = std::numeric_limits<double>::infinity();
  for (const DisplayPoint& point : visiblePoints_) {
    const double x = plot_->xAxis->coordToPixel(point.x);
    const double y = plot_->yAxis->coordToPixel(point.y);
    const double dx = x - position.x();
    const double dy = y - position.y();
    const double distance = dx * dx + dy * dy;
    if (distance > radiusSquared) {
      continue;
    }
    if (!best.has_value() || distance < bestDistance ||
        (std::abs(distance - bestDistance) <= 1.0e-9 &&
         point.sourceIndex < best->sourceIndex)) {
      best = point;
      bestDistance = distance;
    }
  }
  if (best.has_value() && selectionCallback_) {
    selectionCallback_(best->sourceIndex);
  }
}

}  // namespace xfoil::gui

// End: src/gui/plots/ScalarPlotView.cpp

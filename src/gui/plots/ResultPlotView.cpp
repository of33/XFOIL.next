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

// File: src/gui/plots/ResultPlotView.cpp
// Interactive selected-point Cp and Cf presentation.

#include "plots/ResultPlotView.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <QPen>
#include <QVector>
#include <QVBoxLayout>

#include "plots/InteractivePlotController.h"
#include "qcustomplot.h"

namespace xfoil::gui {
namespace {

const QColor kUpperColor(28, 84, 140);
const QColor kLowerColor(198, 93, 27);
const QColor kSampleColor(42, 118, 188);

QVector<double> toVector(const std::vector<double>& values) {
  QVector<double> result;
  result.reserve(static_cast<int>(values.size()));
  for (double value : values) {
    result.push_back(value);
  }
  return result;
}

bool validDistribution(const xfoil::SurfaceDistribution& distribution) {
  return distribution.x.size() == distribution.y.size() &&
         distribution.x.size() == distribution.cp.size() &&
         distribution.x.size() >= 2;
}

struct SplitDistribution {
  std::vector<double> upperX;
  std::vector<double> upperY;
  std::vector<double> lowerX;
  std::vector<double> lowerY;
  bool valid{false};
};

SplitDistribution splitCp(const xfoil::SurfaceDistribution& distribution) {
  SplitDistribution result;
  if (!validDistribution(distribution)) {
    return result;
  }
  std::size_t leadingEdge = 0;
  for (std::size_t index = 1; index < distribution.x.size(); ++index) {
    if (distribution.x[index] < distribution.x[leadingEdge]) {
      leadingEdge = index;
    }
  }
  if (leadingEdge == 0 || leadingEdge + 1 >= distribution.x.size()) {
    return result;
  }

  double firstMean = 0.0;
  double secondMean = 0.0;
  for (std::size_t index = 0; index <= leadingEdge; ++index) {
    firstMean += distribution.y[index];
  }
  firstMean /= static_cast<double>(leadingEdge + 1);
  for (std::size_t index = leadingEdge;
       index < distribution.y.size(); ++index) {
    secondMean += distribution.y[index];
  }
  secondMean /=
      static_cast<double>(distribution.y.size() - leadingEdge);

  const bool firstIsUpper = firstMean >= secondMean;
  const std::size_t firstEnd = leadingEdge;
  const std::size_t secondEnd = distribution.x.size() - 1;
  const auto appendSide = [&](std::size_t start, std::size_t end,
                              std::vector<double>& x,
                              std::vector<double>& y) {
    for (std::size_t index = start; index <= end; ++index) {
      x.push_back(distribution.x[index]);
      y.push_back(-distribution.cp[index]);
    }
  };
  if (firstIsUpper) {
    appendSide(0, firstEnd, result.upperX, result.upperY);
    appendSide(leadingEdge, secondEnd, result.lowerX, result.lowerY);
  } else {
    appendSide(leadingEdge, secondEnd, result.upperX, result.upperY);
    appendSide(0, firstEnd, result.lowerX, result.lowerY);
  }
  result.valid = result.upperX.size() >= 2 && result.lowerX.size() >= 2;
  return result;
}

struct CfData {
  std::vector<double> upperS;
  std::vector<double> upperCf;
  std::vector<double> lowerS;
  std::vector<double> lowerCf;
  bool valid{false};
};

CfData cfData(const xfoil::SkinFrictionDistribution& distribution) {
  CfData result;
  const auto& upper = distribution.upper;
  const auto& lower = distribution.lower;
  if (upper.s.size() != upper.cf.size() ||
      lower.s.size() != lower.cf.size() ||
      upper.s.empty() || lower.s.empty()) {
    return result;
  }
  result.upperS = upper.s;
  result.upperCf = upper.cf;
  result.lowerS = lower.s;
  result.lowerCf = lower.cf;
  result.valid = true;
  return result;
}

}  // namespace

ResultPlotView::ResultPlotView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  plot_ = new QCustomPlot(this);
  plot_->setObjectName(QStringLiteral("plotCanvas"));
  layout->addWidget(plot_);

  upperGraph_ = plot_->addGraph();
  upperGraph_->setObjectName(QStringLiteral("upperDistribution"));
  upperGraph_->setName(QStringLiteral("Upper"));
  upperGraph_->setPen(QPen(kUpperColor, 1.5));
  lowerGraph_ = plot_->addGraph();
  lowerGraph_->setObjectName(QStringLiteral("lowerDistribution"));
  lowerGraph_->setName(QStringLiteral("Lower"));
  lowerGraph_->setPen(QPen(kLowerColor, 1.5));

  zeroLine_ = new QCPItemStraightLine(plot_);
  zeroLine_->setObjectName(QStringLiteral("zeroLine"));
  QPen zeroPen(QColor(110, 110, 110), 1.0);
  zeroPen.setStyle(Qt::DashLine);
  zeroLine_->setPen(zeroPen);
  zeroLine_->setVisible(false);

  messageItem_ = new QCPItemText(plot_);
  messageItem_->position->setType(QCPItemPosition::ptAxisRectRatio);
  messageItem_->position->setCoords(0.5, 0.5);
  messageItem_->setPositionAlignment(Qt::AlignCenter);
  messageItem_->setTextAlignment(Qt::AlignCenter);
  messageItem_->setPen(Qt::NoPen);
  messageItem_->setBrush(Qt::NoBrush);
  messageItem_->setVisible(false);

  interaction_ = new InteractivePlotController(plot_, false, true, this);
  interaction_->setFitCallback([this]() { fitVisibleData(); });
  interaction_->setMarkerToggleCallback([this]() {
    sampleMarkersEnabled_ = interaction_->sampleMarkersEnabled();
    applySampleMarkers();
  });

  plot_->legend->setVisible(true);
  setAxisTitles(QStringLiteral("x/c"), QStringLiteral("-Cp"));
  clear();
}

ResultPlotView::~ResultPlotView() = default;

void ResultPlotView::setAxisTitles(const QString& xTitle,
                                   const QString& yTitle) {
  xTitle_ = xTitle;
  yTitle_ = yTitle;
  applyPresentation();
}

void ResultPlotView::setCpDistribution(
    const xfoil::SurfaceDistribution& distribution) {
  const SplitDistribution split = splitCp(distribution);
  if (!split.valid) {
    clearData();
    setMessage(QString());
    return;
  }
  setDistributionData(split.upperX, split.upperY, split.lowerX,
                      split.lowerY);
  setMessage(QString());
  updateZeroLine(0.0, 0.0, false);
  fitVisibleData();
}

void ResultPlotView::setCfDistribution(
    const xfoil::SkinFrictionDistribution& distribution) {
  const CfData data = cfData(distribution);
  if (!data.valid) {
    clearData();
    setMessage(QString());
    return;
  }
  setDistributionData(data.upperS, data.upperCf, data.lowerS,
                      data.lowerCf);
  setMessage(QString());
  double minimum = data.upperS.front();
  double maximum = minimum;
  for (const auto* values : {&data.upperS, &data.lowerS}) {
    for (double value : *values) {
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
    }
  }
  updateZeroLine(minimum, maximum, true);
  fitVisibleData();
}

void ResultPlotView::clear() {
  clearData();
  setMessage(QString());
}

void ResultPlotView::setMessage(const QString& message) {
  message_ = message;
  messageItem_->setText(message_);
  messageItem_->setVisible(!message_.isEmpty());
  plot_->replot();
}

QString ResultPlotView::messageText() const {
  return message_;
}

void ResultPlotView::setSampleMarkersEnabled(bool enabled) {
  sampleMarkersEnabled_ = enabled;
  if (interaction_ != nullptr) {
    interaction_->setSampleMarkersEnabled(enabled);
  }
  applySampleMarkers();
}

void ResultPlotView::fitVisibleData() {
  if (upperGraph_->dataCount() == 0 &&
      lowerGraph_->dataCount() == 0) {
    return;
  }
  double xMinimum = std::numeric_limits<double>::infinity();
  double xMaximum = -std::numeric_limits<double>::infinity();
  double yMinimum = xMinimum;
  double yMaximum = xMaximum;
  const auto include = [&](QCPGraph* graph) {
    const auto data = graph->data();
    for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
      xMinimum = std::min(xMinimum, it->key);
      xMaximum = std::max(xMaximum, it->key);
      yMinimum = std::min(yMinimum, it->value);
      yMaximum = std::max(yMaximum, it->value);
    }
  };
  include(upperGraph_);
  include(lowerGraph_);
  if (zeroLineVisible_) {
    yMinimum = std::min(yMinimum, 0.0);
    yMaximum = std::max(yMaximum, 0.0);
  }
  const double xSpan = std::max(xMaximum - xMinimum, 1.0e-9);
  const double ySpan = std::max(yMaximum - yMinimum, 1.0e-9);
  const double xPadding = xSpan * 0.06 + 1.0e-9;
  const double yPadding = ySpan * 0.08 + 1.0e-9;
  plot_->xAxis->setRange(xMinimum - xPadding, xMaximum + xPadding);
  plot_->yAxis->setRange(yMinimum - yPadding, yMaximum + yPadding);
  plot_->replot();
}

std::size_t ResultPlotView::upperPointCount() const noexcept {
  return static_cast<std::size_t>(upperGraph_->dataCount());
}

std::size_t ResultPlotView::lowerPointCount() const noexcept {
  return static_cast<std::size_t>(lowerGraph_->dataCount());
}

void ResultPlotView::applyPresentation() {
  plot_->xAxis->setLabel(xTitle_);
  plot_->yAxis->setLabel(yTitle_);
  plot_->replot();
}

void ResultPlotView::applySampleMarkers() {
  const QCPScatterStyle style =
      sampleMarkersEnabled_
          ? QCPScatterStyle(QCPScatterStyle::ssCircle,
                            QPen(kSampleColor, 1.0), Qt::NoBrush, 4.5)
          : QCPScatterStyle(QCPScatterStyle::ssNone);
  if (upperGraph_ != nullptr) {
    upperGraph_->setScatterStyle(style);
  }
  if (lowerGraph_ != nullptr) {
    lowerGraph_->setScatterStyle(style);
  }
  if (plot_ != nullptr) {
    plot_->replot();
  }
}

void ResultPlotView::clearData() {
  upperGraph_->data()->clear();
  lowerGraph_->data()->clear();
  zeroLine_->setVisible(false);
  zeroLineVisible_ = false;
  plot_->replot();
}

void ResultPlotView::setDistributionData(
    const std::vector<double>& upperX,
    const std::vector<double>& upperY,
    const std::vector<double>& lowerX,
    const std::vector<double>& lowerY) {
  upperGraph_->setData(toVector(upperX), toVector(upperY), false);
  lowerGraph_->setData(toVector(lowerX), toVector(lowerY), false);
  upperGraph_->setVisible(!upperX.empty());
  lowerGraph_->setVisible(!lowerX.empty());
  plot_->replot();
}

void ResultPlotView::updateZeroLine(double lower, double upper,
                                    bool visible) {
  zeroLineVisible_ = visible;
  if (visible && std::isfinite(lower) && std::isfinite(upper)) {
    if (upper <= lower) {
      upper = lower + 1.0;
    }
    zeroLine_->point1->setCoords(lower, 0.0);
    zeroLine_->point2->setCoords(upper, 0.0);
  }
  zeroLine_->setVisible(visible);
  plot_->replot();
}

}  // namespace xfoil::gui

// End: src/gui/plots/ResultPlotView.cpp

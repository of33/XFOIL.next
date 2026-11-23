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

// File: src/gui/plots/InteractivePlotController.cpp
// Mouse-only plot navigation and click/drag arbitration.

#include "plots/InteractivePlotController.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMargins>
#include <QMouseEvent>
#include <QStyleHints>
#include <QWheelEvent>

#include "qcustomplot.h"

namespace xfoil::gui {
namespace {

constexpr double kMinimumAxisSpan = 1.0e-12;
constexpr double kMaximumAxisSpan = 1.0e12;
constexpr double kZoomInFactor = 0.82;
constexpr double kZoomOutFactor = 1.0 / kZoomInFactor;
constexpr int kSelectionRadiusPixels = 10;

bool insideAxes(QCustomPlot* plot, const QPoint& position) {
  return plot != nullptr && plot->axisRect() != nullptr &&
         plot->axisRect()->rect().contains(position);
}

QMargins compactMargins(const QCustomPlot* plot) {
  if (plot == nullptr) {
    return QMargins(64, 10, 10, 36);
  }
  const QFontMetrics tickMetrics(plot->xAxis->tickLabelFont());
  const QFontMetrics labelMetrics(plot->xAxis->labelFont());
  const int tickHeight = std::max(tickMetrics.height(), labelMetrics.height());
  const int tickWidth =
      std::max(tickMetrics.horizontalAdvance(QStringLiteral("-0.0000")),
               tickMetrics.horizontalAdvance(QStringLiteral("−0.0000")));
  const int left = tickWidth + tickHeight + 8;
  const int top = tickHeight / 2 + 4;
  const int right = tickHeight / 2 + 4;
  const int bottom = tickHeight * 2 + 8;
  return QMargins(left, top, right, bottom);
}

}  // namespace

InteractivePlotController::InteractivePlotController(
    QCustomPlot* plot, bool equalAspect, bool markerToggle, QObject* parent)
    : QObject(parent), plot_(plot), equalAspect_(equalAspect),
      markerToggle_(markerToggle) {
  if (plot_ != nullptr) {
    plot_->axisRect()->setAutoMargins(QCP::msNone);
    plot_->axisRect()->setMargins(compactMargins(plot_));
    plot_->installEventFilter(this);
    plot_->setCursor(Qt::CrossCursor);
  }
}

InteractivePlotController::~InteractivePlotController() = default;

void InteractivePlotController::setClickCallback(ClickCallback callback) {
  clickCallback_ = std::move(callback);
}

void InteractivePlotController::setFitCallback(ActionCallback callback) {
  fitCallback_ = std::move(callback);
}

void InteractivePlotController::setMarkerToggleCallback(
    ActionCallback callback) {
  markerToggleCallback_ = std::move(callback);
}

void InteractivePlotController::setPointSelectionEnabled(bool enabled) {
  pointSelectionEnabled_ = enabled;
}

void InteractivePlotController::setSampleMarkersEnabled(bool enabled) {
  if (sampleMarkersEnabled_ == enabled) {
    return;
  }
  sampleMarkersEnabled_ = enabled;
  if (markerToggleCallback_) {
    markerToggleCallback_();
  }
}

int InteractivePlotController::selectionRadiusPixels() noexcept {
  return kSelectionRadiusPixels;
}

bool InteractivePlotController::eventFilter(QObject* watched,
                                            QEvent* event) {
  if (watched != plot_ || plot_ == nullptr) {
    return QObject::eventFilter(watched, event);
  }

  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton) {
        if (!insideAxes(plot_, mouse->position().toPoint())) {
          return false;
        }
        leftPressed_ = true;
        dragging_ = false;
        pressPosition_ = mouse->position().toPoint();
        lastPosition_ = pressPosition_;
        plot_->setCursor(Qt::ClosedHandCursor);
        return true;
      }
      if (mouse->button() == Qt::RightButton) {
        handleRightClick(
            mouse->position().toPoint(),
            (mouse->modifiers() & Qt::ControlModifier) != 0);
        return true;
      }
      break;
    }
    case QEvent::MouseMove: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (!leftPressed_ ||
          (mouse->buttons() & Qt::LeftButton) == 0) {
        break;
      }
      const QPoint position = mouse->position().toPoint();
      if (!dragging_ &&
          (position - pressPosition_).manhattanLength() >=
              QGuiApplication::styleHints()->startDragDistance()) {
        dragging_ = true;
      }
      if (dragging_) {
        panBy(lastPosition_, position);
        lastPosition_ = position;
      }
      return true;
    }
    case QEvent::MouseButtonRelease: {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton) {
        if (!leftPressed_) {
          break;
        }
        const QPoint position = mouse->position().toPoint();
        const bool wasDragging = dragging_;
        leftPressed_ = false;
        dragging_ = false;
        plot_->setCursor(Qt::CrossCursor);
        if (!wasDragging && pointSelectionEnabled_ &&
            clickCallback_ && insideAxes(plot_, position)) {
          clickCallback_(position);
        }
        return true;
      }
      if (mouse->button() == Qt::RightButton) {
        return true;
      }
      break;
    }
    case QEvent::Wheel: {
      auto* wheel = static_cast<QWheelEvent*>(event);
      const int delta = wheel->angleDelta().y();
      if (delta == 0) {
        return true;
      }
      zoomAt(wheel->position().toPoint(),
             delta > 0 ? kZoomInFactor : kZoomOutFactor);
      return true;
    }
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}

void InteractivePlotController::panBy(const QPoint& from,
                                      const QPoint& to) {
  if (plot_ == nullptr || plot_->axisRect() == nullptr) {
    return;
  }
  const QRect rect = plot_->axisRect()->rect();
  if (rect.width() <= 0 || rect.height() <= 0) {
    return;
  }
  const double dx = static_cast<double>(to.x() - from.x());
  const double dy = static_cast<double>(to.y() - from.y());
  const QCPRange xRange = plot_->xAxis->range();
  const QCPRange yRange = plot_->yAxis->range();
  if (!std::isfinite(xRange.size()) || !std::isfinite(yRange.size())) {
    return;
  }
  const double xShift = -dx * xRange.size() / rect.width();
  const double yShift = dy * yRange.size() / rect.height();
  plot_->xAxis->setRange(xRange.lower + xShift,
                         xRange.upper + xShift);
  plot_->yAxis->setRange(yRange.lower + yShift,
                         yRange.upper + yShift);
  if (equalAspect_) {
    plot_->yAxis->setScaleRatio(plot_->xAxis, 1.0);
  }
  plot_->replot();
}

void InteractivePlotController::zoomAt(const QPoint& position,
                                       double factor) {
  if (plot_ == nullptr || !(factor > 0.0) || !std::isfinite(factor)) {
    return;
  }
  if (plot_->axisRect() == nullptr) {
    return;
  }
  const QRect rect = plot_->axisRect()->rect();
  if (rect.width() <= 0 || rect.height() <= 0) {
    return;
  }
  const double xCenter = plot_->xAxis->pixelToCoord(position.x());
  const double yCenter = plot_->yAxis->pixelToCoord(position.y());
  const QCPRange xRange = plot_->xAxis->range();
  const QCPRange yRange = plot_->yAxis->range();
  if (!std::isfinite(xCenter) || !std::isfinite(yCenter) ||
      !std::isfinite(xRange.size()) || !std::isfinite(yRange.size())) {
    return;
  }
  const double xFraction = std::clamp(
      static_cast<double>(position.x() - rect.left()) /
          static_cast<double>(rect.width()),
      0.0, 1.0);
  const double yFraction = std::clamp(
      static_cast<double>(position.y() - rect.top()) /
          static_cast<double>(rect.height()),
      0.0, 1.0);

  const double xSpan =
      std::clamp(xRange.size() * factor, kMinimumAxisSpan,
                 kMaximumAxisSpan);
  const double ySpan =
      std::clamp(yRange.size() * factor, kMinimumAxisSpan,
                 kMaximumAxisSpan);
  plot_->xAxis->setRange(xCenter - xFraction * xSpan,
                         xCenter + (1.0 - xFraction) * xSpan);
  plot_->yAxis->setRange(yCenter - (1.0 - yFraction) * ySpan,
                         yCenter + yFraction * ySpan);
  if (equalAspect_) {
    plot_->yAxis->setScaleRatio(plot_->xAxis, 1.0);
  }
  plot_->replot();
}

void InteractivePlotController::handleRightClick(const QPoint& position,
                                                 bool withControl) {
  (void)position;
  if (withControl) {
    if (markerToggle_) {
      setSampleMarkersEnabled(!sampleMarkersEnabled_);
      plot_->replot();
    }
    return;
  }
  if (fitCallback_) {
    fitCallback_();
  }
}

}  // namespace xfoil::gui

// End: src/gui/plots/InteractivePlotController.cpp

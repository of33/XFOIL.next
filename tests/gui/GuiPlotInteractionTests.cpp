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

// File: tests/gui/GuiPlotInteractionTests.cpp
// Shared pan, wheel zoom, right-click fit, Ctrl+right marker toggle and
// per-instance independence.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPoint>
#include <QScrollArea>
#include <QScrollBar>
#include <QTest>
#include <QWheelEvent>

#include "plots/GeometryPlotView.h"
#include "plots/ResultPlotView.h"
#include "plots/ScalarPlotView.h"
#include "qcustomplot.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::SurfaceGeometry;
using xfoil::gui::GeometryPlotView;
using xfoil::gui::GuiPointEntry;
using xfoil::gui::GuiScalarPlotKind;
using xfoil::gui::ResultPlotView;
using xfoil::gui::ScalarPlotView;

GuiPointEntry point(std::size_t sourceIndex, double alpha, double cl) {
  GuiPointEntry entry;
  entry.sourceIndex = sourceIndex;
  entry.alpha = alpha;
  entry.converged = true;
  entry.convergenceStatus = xfoil::ConvergenceStatus::converged;
  entry.polar.alpha = xfoil::Degrees{alpha};
  entry.polar.liftCoefficient = cl;
  entry.polar.dragCoefficient = 0.02;
  entry.polar.momentCoefficient = 0.0;
  entry.polar.convergence = xfoil::ConvergenceStatus::converged;
  return entry;
}

QCustomPlot* canvas(QWidget& view) {
  QCustomPlot* plot = view.findChild<QCustomPlot*>();
  Q_ASSERT(plot != nullptr);
  return plot;
}

void sendMouse(QWidget* widget, QEvent::Type type, const QPoint& position,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QMouseEvent event(type, QPointF(position),
                    QPointF(widget->mapToGlobal(position)), button,
                    buttons, modifiers);
  QApplication::sendEvent(widget, &event);
}

void sendWheel(QWidget* widget, const QPoint& position, int delta) {
  QWheelEvent event(QPointF(position), QPointF(widget->mapToGlobal(position)),
                    QPoint(0, 0), QPoint(0, delta), Qt::NoButton,
                    Qt::NoModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(widget, &event);
}

QPoint centerOf(QCustomPlot* plot) {
  return plot->axisRect()->rect().center();
}

double scaleDifference(QCustomPlot* plot) {
  const QRect rect = plot->axisRect()->rect();
  const double xScale =
      static_cast<double>(rect.width()) / plot->xAxis->range().size();
  const double yScale =
      static_cast<double>(rect.height()) / plot->yAxis->range().size();
  return std::abs(xScale - yScale);
}

xfoil::geometry::AirfoilGeometry diamond() {
  const std::vector<xfoil::Point2D> points = {
      {1.0, 0.0},   {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign("diamond", points);
  Q_ASSERT(assigned.success());
  (void)assigned;
  return geometry;
}

}  // namespace

class GuiPlotInteractionTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void leftDragPansBothAxesWithoutSelecting() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    view.setPoints({point(0, 0.0, 0.0), point(1, 1.0, 0.5),
                    point(2, 2.0, 1.0)},
                   std::size_t{0}, true);
    bool selected = false;
    view.setSelectionCallback([&selected](std::size_t) { selected = true; });
    QCustomPlot* plot = canvas(view);
    const QCPRange beforeX = plot->xAxis->range();
    const QCPRange beforeY = plot->yAxis->range();
    const QPoint start = centerOf(plot);
    sendMouse(plot, QEvent::MouseButtonPress, start, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, start + QPoint(50, 30), Qt::NoButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, start + QPoint(50, 30),
              Qt::LeftButton, Qt::NoButton);
    QVERIFY(!selected);
    QVERIFY(plot->xAxis->range().lower < beforeX.lower);
    QVERIFY(plot->yAxis->range().lower > beforeY.lower);
  }

  void wheelZoomUsesMouseCenterAndBothAxes() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    view.setPoints({point(0, 0.0, 0.0), point(1, 1.0, 0.5),
                    point(2, 2.0, 1.0)},
                   std::size_t{0}, true);
    QCoreApplication::processEvents();
    QCustomPlot* plot = canvas(view);
    const QCPRange beforeX = plot->xAxis->range();
    const QCPRange beforeY = plot->yAxis->range();
    const QRect rect = plot->axisRect()->rect();
    const double xPixel = beforeX.size() / rect.width();
    const double yPixel = beforeY.size() / rect.height();
    const QPoint cursor(rect.left() + rect.width() / 3,
                        rect.top() + rect.height() / 4);
    const double cursorX = plot->xAxis->pixelToCoord(cursor.x());
    const double cursorY = plot->yAxis->pixelToCoord(cursor.y());
    sendWheel(plot, cursor, 120);
    QVERIFY(plot->xAxis->range().size() < beforeX.size());
    QVERIFY(plot->yAxis->range().size() < beforeY.size());
    const double xError =
        std::abs(plot->xAxis->pixelToCoord(cursor.x()) - cursorX);
    const double yError =
        std::abs(plot->yAxis->pixelToCoord(cursor.y()) - cursorY);
    QVERIFY2(xError < 1.1 * xPixel,
             qPrintable(QStringLiteral(
                 "xError=%1 xPixel=%2 cursorX=%3 now=%4 "
                 "before=[%5,%6] after=[%7,%8] rect=%9,%10,%11,%12")
                            .arg(xError)
                            .arg(xPixel)
                            .arg(cursorX)
                            .arg(plot->xAxis->pixelToCoord(cursor.x()))
                            .arg(beforeX.lower)
                            .arg(beforeX.upper)
                            .arg(plot->xAxis->range().lower)
                            .arg(plot->xAxis->range().upper)
                            .arg(rect.x())
                            .arg(rect.y())
                            .arg(rect.width())
                            .arg(rect.height())));
    QVERIFY2(yError < 1.1 * yPixel,
             qPrintable(QStringLiteral("yError=%1 yPixel=%2")
                            .arg(yError)
                            .arg(yPixel)));

    sendWheel(plot, cursor, -120);
    QVERIFY(plot->xAxis->range().size() > beforeX.size() * 0.9);
  }

  void rightClickFitsVisibleDataAndDoesNotSelect() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    view.setPoints({point(0, 0.0, 0.0), point(1, 1.0, 0.5),
                    point(2, 2.0, 1.0)},
                   std::size_t{0}, true);
    bool selected = false;
    view.setSelectionCallback([&selected](std::size_t) { selected = true; });
    QCustomPlot* plot = canvas(view);
    plot->xAxis->setRange(100.0, 200.0);
    plot->yAxis->setRange(100.0, 200.0);
    const QPoint center = centerOf(plot);
    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::RightButton,
              Qt::RightButton);
    sendMouse(plot, QEvent::MouseButtonRelease, center, Qt::RightButton,
              Qt::NoButton);
    QVERIFY(!selected);
    QVERIFY(plot->xAxis->range().contains(0.0));
    QVERIFY(plot->xAxis->range().contains(2.0));
    QVERIFY(plot->yAxis->range().contains(0.0));
    QVERIFY(plot->yAxis->range().contains(1.0));
  }

  void ctrlRightTogglesMarkersOnlyAndDoesNotFit() {
    ResultPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();
    xfoil::SurfaceDistribution distribution;
    distribution.x = {1.0, 0.5, 0.0, 0.5, 1.0};
    distribution.y = {0.0, 0.08, 0.0, -0.08, 0.0};
    distribution.cp = {-0.5, -1.0, -2.0, -0.8, 0.1};
    view.setCpDistribution(distribution);
    QCustomPlot* plot = canvas(view);
    const QCPRange beforeX = plot->xAxis->range();
    const QPoint center = centerOf(plot);
    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::RightButton,
              Qt::RightButton, Qt::ControlModifier);
    sendMouse(plot, QEvent::MouseButtonRelease, center, Qt::RightButton,
              Qt::NoButton, Qt::ControlModifier);
    QVERIFY(view.sampleMarkersEnabled());
    QCOMPARE(plot->xAxis->range().lower, beforeX.lower);
    QCOMPARE(plot->xAxis->range().upper, beforeX.upper);
  }

  void scalarCtrlRightDoesNotToggleOrFit() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();
    view.setPoints({point(0, 0.0, 0.0), point(1, 1.0, 0.5)},
                   std::size_t{0}, true);
    QCustomPlot* plot = canvas(view);
    const QCPRange beforeX = plot->xAxis->range();
    const QPoint center = centerOf(plot);
    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::RightButton,
              Qt::RightButton, Qt::ControlModifier);
    sendMouse(plot, QEvent::MouseButtonRelease, center, Qt::RightButton,
              Qt::NoButton, Qt::ControlModifier);
    QCOMPARE(plot->xAxis->range().lower, beforeX.lower);
    QCOMPARE(plot->xAxis->range().upper, beforeX.upper);
  }

  void emptyPlotFitIsNoOp() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();
    QCustomPlot* plot = canvas(view);
    plot->xAxis->setRange(5.0, 6.0);
    plot->yAxis->setRange(7.0, 8.0);
    const QCPRange beforeX = plot->xAxis->range();
    const QCPRange beforeY = plot->yAxis->range();
    const QPoint center = centerOf(plot);
    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::RightButton,
              Qt::RightButton);
    sendMouse(plot, QEvent::MouseButtonRelease, center, Qt::RightButton,
              Qt::NoButton);
    QCOMPARE(plot->xAxis->range().lower, beforeX.lower);
    QCOMPARE(plot->xAxis->range().upper, beforeX.upper);
    QCOMPARE(plot->yAxis->range().lower, beforeY.lower);
    QCOMPARE(plot->yAxis->range().upper, beforeY.upper);
  }

  void wheelZoomIsBoundedAtExtremes() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    view.setPoints({point(0, 0.0, 0.0), point(1, 1.0, 0.5)},
                   std::size_t{0}, true);
    QCustomPlot* plot = canvas(view);
    plot->xAxis->setRange(0.0, 1.0e-12);
    plot->yAxis->setRange(0.0, 1.0e-12);
    sendWheel(plot, centerOf(plot), 120);
    QVERIFY(plot->xAxis->range().size() >= 1.0e-12);
    QVERIFY(plot->yAxis->range().size() >= 1.0e-12);
    plot->xAxis->setRange(0.0, 1.0e12);
    plot->yAxis->setRange(0.0, 1.0e12);
    sendWheel(plot, centerOf(plot), -120);
    QVERIFY(plot->xAxis->range().size() <= 1.0e12);
    QVERIFY(plot->yAxis->range().size() <= 1.0e12);
  }

  void wheelDoesNotMoveOuterScrollArea() {
    auto* view = new ScalarPlotView;
    view->setKind(GuiScalarPlotKind::clAlpha);
    view->setMinimumSize(500, 1200);
    QScrollArea area;
    area.setWidget(view);
    area.resize(500, 300);
    area.show();
    QCoreApplication::processEvents();
    view->setPoints({point(0, 0.0, 0.0), point(1, 1.0, 0.5),
                     point(2, 2.0, 1.0)},
                    std::size_t{0}, true);
    QCustomPlot* plot = canvas(*view);
    QScrollBar* bar = area.verticalScrollBar();
    bar->setValue(std::min(10, bar->maximum()));
    const int before = bar->value();
    sendWheel(plot, centerOf(plot), -120);
    QCoreApplication::processEvents();
    QCOMPARE(bar->value(), before);
  }

  void geometryCtrlRightTogglesPanelTicksOnly() {
    GeometryPlotView view;
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    const xfoil::geometry::AirfoilGeometry geometry = diamond();
    view.setGeometry(geometry);
    SurfaceGeometry panel;
    for (const xfoil::Point2D& point : geometry.coordinates()) {
      panel.points.push_back(point);
    }
    view.setPanelGeometry(panel);
    view.setShowPaneling(true);
    QCustomPlot* plot = canvas(view);
    const QCPRange beforeX = plot->xAxis->range();
    const QPoint center = centerOf(plot);
    QCPCurve* contour =
        plot->findChild<QCPCurve*>(QStringLiteral("geometryContour"));
    QCPCurve* ticks =
        plot->findChild<QCPCurve*>(QStringLiteral("panelTicks"));
    QVERIFY(contour != nullptr);
    QVERIFY(ticks != nullptr);
    QCOMPARE(contour->scatterStyle().shape(), QCPScatterStyle::ssNone);
    QVERIFY(ticks->visible());

    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::RightButton,
              Qt::RightButton, Qt::ControlModifier);
    sendMouse(plot, QEvent::MouseButtonRelease, center, Qt::RightButton,
              Qt::NoButton, Qt::ControlModifier);
    QVERIFY(!view.panelNodesVisible());
    QVERIFY(!ticks->visible());
    QCOMPARE(contour->scatterStyle().shape(), QCPScatterStyle::ssNone);
    QCOMPARE(plot->xAxis->range().lower, beforeX.lower);
    QCOMPARE(plot->xAxis->range().upper, beforeX.upper);

    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::RightButton,
              Qt::RightButton, Qt::ControlModifier);
    sendMouse(plot, QEvent::MouseButtonRelease, center, Qt::RightButton,
              Qt::NoButton, Qt::ControlModifier);
    QVERIFY(view.panelNodesVisible());
    QVERIFY(ticks->visible());
    QCOMPARE(contour->scatterStyle().shape(), QCPScatterStyle::ssNone);
  }

  void instancesAreIndependent() {
    ScalarPlotView first;
    ScalarPlotView second;
    first.setKind(GuiScalarPlotKind::clAlpha);
    second.setKind(GuiScalarPlotKind::clAlpha);
    first.resize(480, 320);
    second.resize(480, 320);
    first.show();
    second.show();
    QCoreApplication::processEvents();
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.5), point(2, 2.0, 1.0)};
    first.setPoints(points, std::size_t{0}, true);
    second.setPoints(points, std::size_t{0}, true);
    QCustomPlot* firstPlot = canvas(first);
    QCustomPlot* secondPlot = canvas(second);
    const QCPRange secondBefore = secondPlot->xAxis->range();
    const QPoint center = centerOf(firstPlot);
    sendMouse(firstPlot, QEvent::MouseButtonPress, center, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(firstPlot, QEvent::MouseMove, center + QPoint(40, 0),
              Qt::NoButton, Qt::LeftButton);
    sendMouse(firstPlot, QEvent::MouseButtonRelease, center + QPoint(40, 0),
              Qt::LeftButton, Qt::NoButton);
    QCOMPARE(secondPlot->xAxis->range().lower, secondBefore.lower);
    QCOMPARE(secondPlot->xAxis->range().upper, secondBefore.upper);
  }

  void airfoilPanAndWheelChangeRangesWithoutAspectLock() {
    GeometryPlotView view;
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    view.setGeometry(diamond());
    QCustomPlot* plot = canvas(view);
    QVERIFY(scaleDifference(plot) > 1.0);

    const QPoint center = centerOf(plot);
    const QCPRange beforeX = plot->xAxis->range();
    const QCPRange beforeY = plot->yAxis->range();
    sendMouse(plot, QEvent::MouseButtonPress, center, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, center + QPoint(50, 30), Qt::NoButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, center + QPoint(50, 30),
              Qt::LeftButton, Qt::NoButton);
    QVERIFY(plot->xAxis->range() != beforeX);
    QVERIFY(plot->yAxis->range() != beforeY);
    QVERIFY(scaleDifference(plot) > 1.0);

    const QCPRange afterPanX = plot->xAxis->range();
    const QCPRange afterPanY = plot->yAxis->range();
    sendWheel(plot, center, 120);
    QVERIFY(plot->xAxis->range().size() < afterPanX.size());
    QVERIFY(plot->yAxis->range().size() < afterPanY.size());
    QVERIFY(scaleDifference(plot) > 1.0);
  }
};

int runGuiPlotInteractionTests(int argc, char** argv) {
  GuiPlotInteractionTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiPlotInteractionTests.moc"

// End: tests/gui/GuiPlotInteractionTests.cpp

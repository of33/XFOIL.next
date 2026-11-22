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

// File: tests/gui/GuiScalarPlotTests.cpp
// Full enabled curves, marker state, selected-NC dashes, filtering and
// click/drag point selection for scalar plots.

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPoint>
#include <QTest>

#include "plots/ScalarPlotView.h"
#include "qcustomplot.h"

namespace {

using xfoil::gui::GuiPointEntry;
using xfoil::gui::GuiScalarPlotKind;
using xfoil::gui::ScalarComparisonSeries;
using xfoil::gui::ScalarPlotView;

GuiPointEntry point(std::size_t sourceIndex, double alpha, double cl,
                    double cd = 0.02, double cm = 0.0,
                    bool converged = true, bool enabled = true) {
  GuiPointEntry entry;
  entry.sourceIndex = sourceIndex;
  entry.alpha = alpha;
  entry.converged = converged;
  entry.enabled = enabled;
  entry.convergenceStatus =
      converged ? xfoil::ConvergenceStatus::converged
                : xfoil::ConvergenceStatus::unconverged;
  entry.polar.alpha = xfoil::Degrees{alpha};
  entry.polar.liftCoefficient = cl;
  entry.polar.dragCoefficient = cd;
  entry.polar.momentCoefficient = cm;
  entry.polar.convergence = entry.convergenceStatus;
  return entry;
}

QCustomPlot* canvas(ScalarPlotView& view) {
  QCustomPlot* plot = view.findChild<QCustomPlot*>();
  Q_ASSERT(plot != nullptr);
  return plot;
}

QCPGraph* graph(QCustomPlot* plot, const char* name) {
  QCPGraph* value = plot->findChild<QCPGraph*>(QString::fromLatin1(name));
  Q_ASSERT(value != nullptr);
  return value;
}

QCPCurve* curve(QCustomPlot* plot, const char* name) {
  QCPCurve* value = plot->findChild<QCPCurve*>(QString::fromLatin1(name));
  Q_ASSERT(value != nullptr);
  return value;
}

void verifySelectedRedFilled(QCPGraph* marker) {
  QVERIFY(marker != nullptr);
  QCOMPARE(marker->scatterStyle().shape(), QCPScatterStyle::ssDisc);
  QCOMPARE(marker->scatterStyle().pen().color(), QColor(205, 45, 45));
  QCOMPARE(marker->scatterStyle().brush().color(), QColor(205, 45, 45));
  QVERIFY(marker->visible());
}

bool markerDataContains(QCPGraph* marker, double key, double value) {
  const auto data = marker->data();
  for (auto iterator = data->constBegin(); iterator != data->constEnd();
       ++iterator) {
    if (iterator->key == key && iterator->value == value) {
      return true;
    }
  }
  return false;
}

void sendMouse(QWidget* widget, QEvent::Type type, const QPoint& position,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QMouseEvent event(type, QPointF(position),
                    QPointF(widget->mapToGlobal(position)), button,
                    buttons, modifiers);
  QApplication::sendEvent(widget, &event);
}

QPoint markerPixel(QCustomPlot* plot, double x, double y) {
  return QPoint(static_cast<int>(std::lround(plot->xAxis->coordToPixel(x))),
                static_cast<int>(std::lround(plot->yAxis->coordToPixel(y))));
}

}  // namespace

class GuiScalarPlotTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void fullEnabledCurveAndMarkerContract() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.5),
        point(2, 2.0, 1.0, 0.02, 0.0, false),
        point(3, 3.0, 1.5, 0.02, 0.0, true, false),
        point(4, 4.0, 0.8)};
    view.setPoints(points, std::size_t{1}, true);

    QCOMPARE(view.visiblePointCount(), std::size_t{3});
    QCOMPARE(view.solidPointCount(), std::size_t{3});
    QCOMPARE(view.dashedPointCount(), std::size_t{0});
    QCOMPARE(view.unselectedMarkerCount(), std::size_t{2});
    QCOMPARE(view.selectedMarkerCount(), std::size_t{1});

    QCustomPlot* plot = canvas(view);
    QCPGraph* unselected =
        graph(plot, "unselectedMarkers");
    QCPGraph* selected = graph(plot, "selectedMarker");
    QCOMPARE(unselected->scatterStyle().shape(), QCPScatterStyle::ssCircle);
    QCOMPARE(unselected->scatterStyle().brush().style(), Qt::NoBrush);
    verifySelectedRedFilled(selected);
  }

  void selectedNcIsRedFilledAndUsesDashedNeighbors() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.8, 0.02, 0.0, false),
        point(2, 2.0, 0.4)};
    view.setPoints(points, std::size_t{1}, true);
    QCOMPARE(view.visiblePointCount(), std::size_t{3});
    QCOMPARE(view.solidPointCount(), std::size_t{2});
    QCOMPARE(view.dashedPointCount(), std::size_t{3});
    QCOMPARE(view.unselectedMarkerCount(), std::size_t{2});
    QCOMPARE(view.selectedMarkerCount(), std::size_t{1});
    QCustomPlot* plot = canvas(view);
    QCOMPARE(curve(plot, "solidBefore")->dataCount(), 1);
    QCOMPARE(curve(plot, "solidAfter")->dataCount(), 1);
    QCPCurve* dashed = curve(plot, "ncDashed");
    QCOMPARE(dashed->pen().style(), Qt::DashLine);
    QVERIFY(dashed->visible());
    QCPGraph* selected = graph(plot, "selectedMarker");
    verifySelectedRedFilled(selected);

    view.setPoints(points, std::size_t{0}, false);
    QCOMPARE(view.visiblePointCount(), std::size_t{2});
    QCOMPARE(view.solidPointCount(), std::size_t{2});
    QCOMPARE(view.dashedPointCount(), std::size_t{0});
    QVERIFY(!dashed->visible());
    QCOMPARE(curve(plot, "solidBefore")->dataCount(), 2);
    QCOMPARE(curve(plot, "solidAfter")->dataCount(), 0);
    QCOMPARE(view.unselectedMarkerCount(), std::size_t{1});
    QCOMPARE(view.selectedMarkerCount(), std::size_t{1});
    verifySelectedRedFilled(graph(plot, "selectedMarker"));
    QVERIFY(!markerDataContains(graph(plot, "unselectedMarkers"),
                                1.0, 0.8));
  }

  void selectedNcAppearsInEveryScalarKind() {
    const std::vector<GuiScalarPlotKind> kinds = {
        GuiScalarPlotKind::clAlpha, GuiScalarPlotKind::clCd,
        GuiScalarPlotKind::cmAlpha, GuiScalarPlotKind::cmCl,
        GuiScalarPlotKind::efficiencyCl};
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.8, 0.02, 0.0, false),
        point(2, 2.0, 0.4)};
    for (GuiScalarPlotKind kind : kinds) {
      ScalarPlotView view;
      view.setKind(kind);
      view.resize(640, 480);
      view.show();
      QCoreApplication::processEvents();
      view.setPoints(points, std::size_t{1}, true);
      QCOMPARE(view.visiblePointCount(), std::size_t{3});
      QCOMPARE(view.dashedPointCount(), std::size_t{3});
      QCOMPARE(view.unselectedMarkerCount(), std::size_t{2});
      QCOMPARE(view.selectedMarkerCount(), std::size_t{1});
      verifySelectedRedFilled(graph(canvas(view), "selectedMarker"));

      view.setPoints(points, std::size_t{0}, false);
      QCOMPARE(view.visiblePointCount(), std::size_t{2});
      QCOMPARE(view.solidPointCount(), std::size_t{2});
      QCOMPARE(view.dashedPointCount(), std::size_t{0});
      QCOMPARE(view.unselectedMarkerCount(), std::size_t{1});
      QCOMPARE(view.selectedMarkerCount(), std::size_t{1});
      verifySelectedRedFilled(graph(canvas(view), "selectedMarker"));
    }
  }

  void singlePointHasOneMarkerAndNoArtificialLine() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {point(0, 2.0, 0.7)};
    view.setPoints(points, std::size_t{0}, true);
    QCOMPARE(view.visiblePointCount(), std::size_t{1});
    QCOMPARE(view.solidPointCount(), std::size_t{1});
    QCOMPARE(view.dashedPointCount(), std::size_t{0});
    QCOMPARE(view.unselectedMarkerCount(), std::size_t{0});
    QCOMPARE(view.selectedMarkerCount(), std::size_t{1});
  }

  void curvesConnectPointsInSourceOrderDespiteUnsortedX() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::cmCl);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 2.0),
        point(2, 2.0, 1.0)};
    view.setPoints(points, std::size_t{0}, true);
    QCPCurve* line = curve(canvas(view), "solidBefore");
    QCOMPARE(line->dataCount(), 3);
    const auto data = line->data();
    auto iterator = data->constBegin();
    QCOMPARE(iterator->t, 0.0);
    QCOMPARE(iterator->key, 0.0);
    ++iterator;
    QCOMPARE(iterator->t, 1.0);
    QCOMPARE(iterator->key, 2.0);
    ++iterator;
    QCOMPARE(iterator->t, 2.0);
    QCOMPARE(iterator->key, 1.0);
  }

  void clickWithinRadiusSelectsNearestVisibleMarker() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.5), point(2, 2.0, 1.0)};
    view.setPoints(points, std::size_t{0}, true);
    std::optional<std::size_t> selected;
    view.setSelectionCallback(
        [&selected](std::size_t source) { selected = source; });

    QCustomPlot* plot = canvas(view);
    const QPoint target = markerPixel(plot, 2.0, 1.0);
    sendMouse(plot, QEvent::MouseButtonPress, target, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, target, Qt::LeftButton,
              Qt::NoButton);
    QVERIFY(selected.has_value());
    QCOMPARE(*selected, std::size_t{2});

    selected.reset();
    sendMouse(plot, QEvent::MouseButtonPress, QPoint(4, 4),
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, QPoint(4, 4),
              Qt::LeftButton, Qt::NoButton);
    QVERIFY(!selected.has_value());
  }

  void clickTieUsesSmallerSourceIndex() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {
        point(5, 1.0, 0.5), point(6, 1.0, 0.5)};
    view.setPoints(points, std::size_t{5}, true);
    std::optional<std::size_t> selected;
    view.setSelectionCallback(
        [&selected](std::size_t source) { selected = source; });
    QCustomPlot* plot = canvas(view);
    const QPoint target = markerPixel(plot, 1.0, 0.5);
    sendMouse(plot, QEvent::MouseButtonPress, target, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, target, Qt::LeftButton,
              Qt::NoButton);
    QVERIFY(selected.has_value());
    QCOMPARE(*selected, std::size_t{5});
  }

  void dragDoesNotSelect() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.5)};
    view.setPoints(points, std::size_t{0}, true);
    bool selected = false;
    view.setSelectionCallback(
        [&selected](std::size_t) { selected = true; });
    QCustomPlot* plot = canvas(view);
    const QPoint target = markerPixel(plot, 1.0, 0.5);
    sendMouse(plot, QEvent::MouseButtonPress, target, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, target + QPoint(40, 25),
              Qt::NoButton, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, target + QPoint(40, 25),
              Qt::LeftButton, Qt::NoButton);
    QVERIFY(!selected);
  }

  void comparisonCurvesAreIndependentAndLazilyAllocated() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    QCustomPlot* plot = canvas(view);
    QCOMPARE(plot->plottableCount(), 5);
    QCOMPARE(view.comparisonCurveCount(), std::size_t{0});

    const std::vector<ScalarComparisonSeries> comparisons = {
        ScalarComparisonSeries{"First", {{0.0, 0.0}, {1.0, 0.4}, {2.0, 0.8}}},
        ScalarComparisonSeries{"Second", {{0.0, 0.1}, {2.0, 0.5}}}};
    view.setComparisons(comparisons, true);
    QCOMPARE(view.comparisonCurveCount(), std::size_t{2});
    QCOMPARE(view.comparisonPointCount(), std::size_t{5});
    QCOMPARE(plot->plottableCount(), 7);
    QCOMPARE(view.visiblePointCount(), std::size_t{0});
    QCOMPARE(view.unselectedMarkerCount(), std::size_t{0});

    view.setComparisons({comparisons.front()}, false);
    QCOMPARE(view.comparisonCurveCount(), std::size_t{1});
    QCOMPARE(view.comparisonPointCount(), std::size_t{3});
    QCOMPARE(plot->plottableCount(), 6);

    view.setComparisons({}, false);
    QCOMPARE(view.comparisonCurveCount(), std::size_t{0});
    QCOMPARE(view.comparisonPointCount(), std::size_t{0});
    QCOMPARE(plot->plottableCount(), 5);
  }

  void disabledAndUnselectedNcMarkersAreNotClickable() {
    ScalarPlotView view;
    view.setKind(GuiScalarPlotKind::clAlpha);
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();

    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0), point(1, 1.0, 0.5, 0.02, 0.0, false),
        point(2, 2.0, 1.0, 0.02, 0.0, true, false)};
    view.setPoints(points, std::size_t{0}, true);
    bool selected = false;
    view.setSelectionCallback(
        [&selected](std::size_t) { selected = true; });
    QCustomPlot* plot = canvas(view);
    const QPoint nc = markerPixel(plot, 1.0, 0.5);
    sendMouse(plot, QEvent::MouseButtonPress, nc, Qt::LeftButton,
              Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, nc, Qt::LeftButton,
              Qt::NoButton);
    QVERIFY(!selected);
    QCOMPARE(view.visiblePointCount(), std::size_t{1});
    QCOMPARE(view.dashedPointCount(), std::size_t{0});
  }
};

int runGuiScalarPlotTests(int argc, char** argv) {
  GuiScalarPlotTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiScalarPlotTests.moc"

// End: tests/gui/GuiScalarPlotTests.cpp

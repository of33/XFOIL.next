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

// File: tests/gui/GeometryPlotViewTests.cpp
// Offscreen contour, panel-tick, clear, bounds and instance tests.

#include <cmath>
#include <limits>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <QtTest/QTest>

#include "plots/GeometryPlotView.h"
#include "qcustomplot.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::SurfaceGeometry;
using xfoil::gui::GeometryPlotView;
using xfoil::geometry::AirfoilGeometry;

AirfoilGeometry diamond() {
  const std::vector<xfoil::Point2D> points = {
      {1.0, 0.0},   {0.5, 0.08}, {0.0, 0.0},
      {0.5, -0.08}, {1.0, 0.0}};
  AirfoilGeometry geometry;
  const auto assigned = geometry.assign("diamond", points);
  Q_ASSERT(assigned.success());
  (void)assigned;
  return geometry;
}

SurfaceGeometry panelFor(const AirfoilGeometry& geometry) {
  SurfaceGeometry panel;
  for (const xfoil::Point2D& point : geometry.coordinates()) {
    panel.points.push_back(point);
  }
  return panel;
}

QCustomPlot* plotOf(GeometryPlotView& view) {
  QCustomPlot* plot = view.findChild<QCustomPlot*>();
  Q_ASSERT(plot != nullptr);
  return plot;
}

QCPCurve* curveOf(QCustomPlot* plot, const char* name) {
  QCPCurve* curve = plot->findChild<QCPCurve*>(QString::fromLatin1(name));
  Q_ASSERT(curve != nullptr);
  return curve;
}

}  // namespace

class GeometryPlotViewTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void drawsContourPanelAndTicks() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    const AirfoilGeometry geometry = diamond();
    const SurfaceGeometry panel = panelFor(geometry);
    view.setGeometry(geometry);
    view.setPanelGeometry(panel);

    QCOMPARE(view.geometryPointCount(), std::size_t{5});
    QCOMPARE(view.panelPointCount(), std::size_t{5});
    QVERIFY(view.panelNodesVisible());

    QCustomPlot* plot = plotOf(view);
    QCOMPARE(plot->graphCount(), 0);
    QCOMPARE(plot->plottableCount(), 3);
    QCPCurve* geometryCurve = curveOf(plot, "geometryContour");
    QCPCurve* panelContour = curveOf(plot, "panelContour");
    QCPCurve* panelTicks = curveOf(plot, "panelTicks");
    QCOMPARE(geometryCurve->dataCount(), 6);
    QCOMPARE(panelContour->dataCount(), 6);
    QVERIFY(view.panelTickCount() > 0);
    QCOMPARE(panelTicks->dataCount(),
             static_cast<int>(view.panelTickCount() * 3));
    QVERIFY(geometryCurve->visible());
    QVERIFY(!panelContour->visible());
    QVERIFY(!panelTicks->visible());

    view.setShowPaneling(true);
    QVERIFY(panelContour->visible());
    QVERIFY(panelTicks->visible());
    const auto contour = geometryCurve->data();
    auto iterator = contour->constBegin();
    QCOMPARE(iterator->t, 0.0);
    QCOMPARE(iterator->key, 1.0);
    ++iterator;
    QCOMPARE(iterator->key, 0.5);
    ++iterator;
    QCOMPARE(iterator->key, 0.0);
    QCOMPARE(plot->xAxis->label(), QStringLiteral("x/c"));
    QCOMPARE(plot->yAxis->label(), QStringLiteral("y/c"));
    QCOMPARE(plot->xAxis->range().lower, 0.0);
    QCOMPARE(plot->xAxis->range().upper, 1.0);
    QVERIFY(plot->yAxis->range().lower <= -0.09);
    QVERIFY(plot->yAxis->range().upper >= 0.09);
  }

  void panelTicksToggleAndClear() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    const AirfoilGeometry geometry = diamond();
    view.setGeometry(geometry);
    view.setPanelGeometry(panelFor(geometry));
    QCustomPlot* plot = plotOf(view);

    QCPCurve* geometryCurve = curveOf(plot, "geometryContour");
    QCPCurve* panelContour = curveOf(plot, "panelContour");
    QCPCurve* panelTicks = curveOf(plot, "panelTicks");
    view.setShowPaneling(true);
    view.showPanelNodes(false);
    QVERIFY(!view.panelNodesVisible());
    QVERIFY(!panelTicks->visible());
    view.showPanelNodes(true);
    QVERIFY(panelTicks->visible());

    view.clearPanel();
    QCOMPARE(view.panelPointCount(), std::size_t{0});
    QCOMPARE(panelContour->dataCount(), 0);
    QCOMPARE(panelTicks->dataCount(), 0);
    QVERIFY(!panelContour->visible());
    QVERIFY(!panelTicks->visible());
    QCOMPARE(view.geometryPointCount(), std::size_t{5});
    QVERIFY(geometryCurve->visible());

    view.clear();
    QCOMPARE(view.geometryPointCount(), std::size_t{0});
    QCOMPARE(view.panelPointCount(), std::size_t{0});
    QCOMPARE(geometryCurve->dataCount(), 0);
    QVERIFY(!geometryCurve->visible());
  }

  void panelingOverlayDefaultAndToggle() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    QVERIFY(!view.showPaneling());
    const AirfoilGeometry geometry = diamond();
    view.setGeometry(geometry);
    view.setPanelGeometry(panelFor(geometry));
    QCustomPlot* plot = plotOf(view);
    QCPCurve* panelContour = curveOf(plot, "panelContour");
    QCPCurve* ticks = curveOf(plot, "panelTicks");
    QVERIFY(!panelContour->visible());
    QVERIFY(!ticks->visible());
    QVERIFY(view.panelNodesVisible());
    QVERIFY(view.panelTickCount() > 0);
    QCOMPARE(ticks->dataCount(),
             static_cast<int>(view.panelTickCount() * 3));

    view.setShowPaneling(true);
    QVERIFY(panelContour->visible());
    QVERIFY(ticks->visible());
    view.setShowPaneling(false);
    QVERIFY(!panelContour->visible());
    QVERIFY(!ticks->visible());
    view.showPanelNodes(false);
    QVERIFY(!view.panelNodesVisible());
    view.setShowPaneling(true);
    QVERIFY(panelContour->visible());
    QVERIFY(!ticks->visible());
    QVERIFY(view.panelTickCount() > 0);
  }

  void panelingOverlayUsesRedAndOutwardTicks() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    const AirfoilGeometry geometry = diamond();
    view.setGeometry(geometry);
    view.setPanelGeometry(panelFor(geometry));
    view.setShowPaneling(true);

    QCustomPlot* plot = plotOf(view);
    QCPCurve* panelContour = curveOf(plot, "panelContour");
    QCPCurve* ticks = curveOf(plot, "panelTicks");
    const QColor contourColor = panelContour->pen().color();
    const QColor tickColor = ticks->pen().color();
    QVERIFY(contourColor.red() > 180);
    QVERIFY(contourColor.green() < 80);
    QVERIFY(contourColor.blue() < 80);
    QVERIFY(tickColor.red() > 180);
    QVERIFY(tickColor.green() < 80);
    QVERIFY(tickColor.blue() < 80);

    bool sawUpperOutward = false;
    bool sawLowerOutward = false;
    bool sawLeadingEdgeOutward = false;
    const auto data = ticks->data();
    for (int index = 0; index + 1 < data->size(); index += 3) {
      const double startX = data->at(index)->key;
      const double startY = data->at(index)->value;
      const double endX = data->at(index + 1)->key;
      const double endY = data->at(index + 1)->value;
      const double length =
          std::hypot(endX - startX, endY - startY);
      QVERIFY(std::abs(length - 0.005) <= 1.0e-12);
      if (startY > 0.0 && endY > startY) {
        sawUpperOutward = true;
      }
      if (startY < 0.0 && endY < startY) {
        sawLowerOutward = true;
      }
      if (std::abs(startX) <= 1.0e-12 && endX < startX) {
        sawLeadingEdgeOutward = true;
      }
    }
    QVERIFY(sawUpperOutward);
    QVERIFY(sawLowerOutward);
    QVERIFY(sawLeadingEdgeOutward);
  }

  void degeneratePanelNodesProduceNoTicks() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    SurfaceGeometry panel;
    panel.points = {
        {1.0, 0.0}, {1.0, 0.0}, {0.5, 0.08}, {0.0, 0.0},
        {0.5, -0.08}, {1.0, 0.0}};
    view.setPanelGeometry(panel);
    view.setShowPaneling(true);
    QCustomPlot* plot = plotOf(view);
    QCPCurve* ticks = curveOf(plot, "panelTicks");
    QVERIFY(view.panelTickCount() < panel.points.size());
    QCOMPARE(ticks->dataCount(),
             static_cast<int>(view.panelTickCount() * 3));
  }

  void boundsUseXContractAndYMargin() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    AirfoilGeometry geometry;
    const std::vector<xfoil::Point2D> points = {
        {0.02, 0.12}, {0.98, 0.30}, {0.40, -0.05}};
    QVERIFY(geometry.assign("bounds", points).success());
    view.setGeometry(geometry);
    QCustomPlot* plot = plotOf(view);

    QCOMPARE(plot->xAxis->range().lower, 0.0);
    QCOMPARE(plot->xAxis->range().upper, 1.0);
    QVERIFY(std::abs(plot->yAxis->range().lower - (-0.06)) <= 1.0e-12);
    QVERIFY(std::abs(plot->yAxis->range().upper - 0.31) <= 1.0e-12);

    view.resize(320, 480);
    QCoreApplication::processEvents();
    QCOMPARE(plot->xAxis->range().lower, 0.0);
    QCOMPARE(plot->xAxis->range().upper, 1.0);
    QVERIFY(std::abs(plot->yAxis->range().lower - (-0.06)) <= 1.0e-12);
    QVERIFY(std::abs(plot->yAxis->range().upper - 0.31) <= 1.0e-12);

    view.clear();
    QCOMPARE(plot->xAxis->range().lower, 0.0);
    QCOMPARE(plot->xAxis->range().upper, 1.0);
    QCOMPARE(plot->yAxis->range().lower, -0.01);
    QCOMPARE(plot->yAxis->range().upper, 0.01);
  }

  void twoViewsStayIndependent() {
    GeometryPlotView first;
    GeometryPlotView second;
    first.resize(480, 320);
    second.resize(480, 320);
    first.show();
    second.show();
    QCoreApplication::processEvents();

    const AirfoilGeometry geometry = diamond();
    const SurfaceGeometry panel = panelFor(geometry);
    first.setGeometry(geometry);
    first.setPanelGeometry(panel);

    QCOMPARE(first.geometryPointCount(), std::size_t{5});
    QCOMPARE(first.panelPointCount(), std::size_t{5});
    QCOMPARE(second.geometryPointCount(), std::size_t{0});
    QCOMPARE(second.panelPointCount(), std::size_t{0});

    second.setGeometry(geometry);
    first.clear();
    QCOMPARE(first.geometryPointCount(), std::size_t{0});
    QCOMPARE(second.geometryPointCount(), std::size_t{5});
  }
};

int runGeometryPlotViewTests(int argc, char** argv) {
  GeometryPlotViewTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GeometryPlotViewTests.moc"

// End: tests/gui/GeometryPlotViewTests.cpp

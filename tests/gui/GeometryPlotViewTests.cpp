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
// Offscreen contour, panel-node, clear, aspect and instance tests.

#include <cmath>
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

}  // namespace

class GeometryPlotViewTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void drawsContourPanelAndNodes() {
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
    QCOMPARE(plot->graphCount(), 3);
    QCOMPARE(plot->graph(0)->dataCount(), 6);
    QCOMPARE(plot->graph(1)->dataCount(), 6);
    QCOMPARE(plot->graph(2)->dataCount(), 5);
    QVERIFY(plot->graph(0)->visible());
    QVERIFY(plot->graph(1)->visible());
    QVERIFY(plot->graph(2)->visible());
    QCOMPARE(plot->xAxis->label(), QStringLiteral("x/c"));
    QCOMPARE(plot->yAxis->label(), QStringLiteral("y/c"));
    QVERIFY(plot->xAxis->range().lower < 0.0);
    QVERIFY(plot->xAxis->range().upper > 1.0);
  }

  void panelNodesToggleAndClear() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    const AirfoilGeometry geometry = diamond();
    view.setGeometry(geometry);
    view.setPanelGeometry(panelFor(geometry));
    QCustomPlot* plot = plotOf(view);

    view.showPanelNodes(false);
    QVERIFY(!view.panelNodesVisible());
    QVERIFY(!plot->graph(2)->visible());
    view.showPanelNodes(true);
    QVERIFY(plot->graph(2)->visible());

    view.clearPanel();
    QCOMPARE(view.panelPointCount(), std::size_t{0});
    QCOMPARE(plot->graph(1)->dataCount(), 0);
    QCOMPARE(plot->graph(2)->dataCount(), 0);
    QVERIFY(!plot->graph(1)->visible());
    QVERIFY(!plot->graph(2)->visible());
    QCOMPARE(view.geometryPointCount(), std::size_t{5});
    QVERIFY(plot->graph(0)->visible());

    view.clear();
    QCOMPARE(view.geometryPointCount(), std::size_t{0});
    QCOMPARE(view.panelPointCount(), std::size_t{0});
    QCOMPARE(plot->graph(0)->dataCount(), 0);
    QVERIFY(!plot->graph(0)->visible());
  }

  void equalAxisScaleIsMaintained() {
    GeometryPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    const AirfoilGeometry geometry = diamond();
    view.setGeometry(geometry);
    view.setPanelGeometry(panelFor(geometry));
    QCustomPlot* plot = plotOf(view);
    plot->replot();

    const QRect rect = plot->axisRect()->rect();
    const double xScale =
        static_cast<double>(rect.width()) / plot->xAxis->range().size();
    const double yScale =
        static_cast<double>(rect.height()) / plot->yAxis->range().size();
    QVERIFY(xScale > 0.0);
    QVERIFY(yScale > 0.0);
    const double tolerance = 1.0e-6 * std::max(xScale, yScale);
    QVERIFY(std::abs(xScale - yScale) <= tolerance);

    view.resize(320, 480);
    QCoreApplication::processEvents();
    plot->replot();
    const QRect resized = plot->axisRect()->rect();
    const double resizedX =
        static_cast<double>(resized.width()) / plot->xAxis->range().size();
    const double resizedY =
        static_cast<double>(resized.height()) / plot->yAxis->range().size();
    const double resizedTolerance =
        1.0e-6 * std::max(resizedX, resizedY);
    QVERIFY(std::abs(resizedX - resizedY) <= resizedTolerance);
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

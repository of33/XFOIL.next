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

// File: tests/gui/GuiDistributionPlotTests.cpp
// Selected-point Cp/Cf presentation, upper/lower split, zero line, marker
// toggle and inviscid message.

#include <cmath>
#include <cstddef>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QMargins>
#include <QPoint>
#include <QTest>

#include "plots/ResultPlotView.h"
#include "qcustomplot.h"

namespace {

using xfoil::gui::ResultPlotView;

QCustomPlot* canvas(ResultPlotView& view) {
  QCustomPlot* plot = view.findChild<QCustomPlot*>();
  Q_ASSERT(plot != nullptr);
  return plot;
}

xfoil::SurfaceDistribution cpDistribution(double offset = 0.0) {
  xfoil::SurfaceDistribution distribution;
  distribution.kind = xfoil::SurfaceDistributionKind::viscous;
  distribution.x = {1.0, 0.5, 0.0, 0.5, 1.0};
  distribution.y = {0.0, 0.08, 0.0, -0.08, 0.0};
  distribution.cp = {-0.5 + offset, -1.0 + offset, -2.0 + offset,
                     -0.8 + offset, 0.1 + offset};
  return distribution;
}

xfoil::SkinFrictionDistribution cfDistribution() {
  xfoil::SkinFrictionDistribution distribution;
  distribution.upper.side = xfoil::SurfaceSide::upper;
  distribution.upper.x = {0.0, 0.5, 1.0};
  distribution.upper.y = {0.0, 0.02, 0.0};
  distribution.upper.s = {0.0, 0.5, 1.0};
  distribution.upper.cf = {0.004, 0.002, -0.001};
  distribution.lower.side = xfoil::SurfaceSide::lower;
  distribution.lower.x = {0.0, 0.5, 1.0};
  distribution.lower.y = {0.0, -0.02, 0.0};
  distribution.lower.s = {0.0, 0.6, 1.2};
  distribution.lower.cf = {0.003, 0.0015, 0.0005};
  return distribution;
}

}  // namespace

class GuiDistributionPlotTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void cpSplitsUpperLowerAndUsesNegativeCp() {
    ResultPlotView view;
    view.setAxisTitles(QStringLiteral("x/c"), QStringLiteral("-Cp"));
    view.setCpDistribution(cpDistribution());
    QCOMPARE(view.upperPointCount(), std::size_t{3});
    QCOMPARE(view.lowerPointCount(), std::size_t{3});
    QVERIFY(!view.zeroLineVisible());

    QCustomPlot* plot = canvas(view);
    QCOMPARE(plot->graphCount(), 2);
    QCOMPARE(plot->graph(0)->dataCount(), 3);
    QCOMPARE(plot->graph(1)->dataCount(), 3);
    QCOMPARE(plot->graph(0)->data()->constBegin()->key, 0.0);
    QCOMPARE(plot->graph(0)->data()->constBegin()->value, 2.0);
    QCOMPARE((plot->graph(0)->data()->constEnd() - 1)->key, 1.0);
    QCOMPARE((plot->graph(0)->data()->constEnd() - 1)->value, 0.5);
    QCOMPARE(plot->graph(1)->data()->constBegin()->value, 2.0);
    QCOMPARE((plot->graph(1)->data()->constEnd() - 1)->value, -0.1);
    QCOMPARE(plot->yAxis->label(), QStringLiteral("-Cp"));
    QVERIFY(plot->xAxis->range().contains(0.0));
    QVERIFY(plot->xAxis->range().contains(1.0));
    QVERIFY(plot->yAxis->range().contains(-0.1));
    QVERIFY(plot->yAxis->range().contains(2.0));
    QVERIFY(std::abs(plot->yAxis->range().upper - 2.1) < 1.0e-12);
  }

  void cpLegendIsTopRightAndMarginsAreCompact() {
    ResultPlotView view;
    view.resize(640, 480);
    view.show();
    QCoreApplication::processEvents();
    view.setCpDistribution(cpDistribution());
    QCustomPlot* plot = canvas(view);
    QVERIFY(plot->legend->visible());
    QCOMPARE(plot->axisRect()->insetLayout()->insetAlignment(0),
             Qt::AlignTop | Qt::AlignRight);
    QVERIFY(plot->legend->margins() == QMargins(2, 2, 2, 2));
    const QMargins margins = plot->axisRect()->margins();
    QVERIFY(margins.left() < 82);
    QVERIFY(margins.top() <= 6);
    QVERIFY(margins.right() <= 8);
    QVERIFY(margins.bottom() < 56);
    QVERIFY(plot->axisRect()->width() > 0);
    QVERIFY(plot->axisRect()->height() > 0);
  }

  void cpSelectionReplacementLeavesNoStaleCurve() {
    ResultPlotView view;
    view.setCpDistribution(cpDistribution());
    view.setCpDistribution(cpDistribution(1.0));
    QCOMPARE(view.upperPointCount(), std::size_t{3});
    QCOMPARE(view.lowerPointCount(), std::size_t{3});
    QCustomPlot* plot = canvas(view);
    QCOMPARE(plot->graph(0)->data()->constBegin()->value, 1.0);
    QCOMPARE((plot->graph(0)->data()->constEnd() - 1)->value, -0.5);
    QCOMPARE(plot->graph(0)->dataCount(), 3);
  }

  void cfUsesSAndPreservesSignWithZeroLine() {
    ResultPlotView view;
    view.setAxisTitles(QStringLiteral("s"), QStringLiteral("Cf"));
    view.setCfDistribution(cfDistribution());
    QCOMPARE(view.upperPointCount(), std::size_t{3});
    QCOMPARE(view.lowerPointCount(), std::size_t{3});
    QVERIFY(view.zeroLineVisible());
    QCustomPlot* plot = canvas(view);
    const auto upper = plot->graph(0)->data();
    QCOMPARE(upper->constBegin()->key, 0.0);
    QCOMPARE((upper->constEnd() - 1)->key, 1.0);
    QCOMPARE((upper->constEnd() - 1)->value, -0.001);
    QCOMPARE(plot->xAxis->label(), QStringLiteral("s"));
    QCOMPARE(plot->yAxis->label(), QStringLiteral("Cf"));
    QVERIFY(plot->xAxis->range().contains(0.0));
    QVERIFY(plot->xAxis->range().contains(1.2));
    QVERIFY(plot->yAxis->range().contains(-0.001));
    QVERIFY(plot->yAxis->range().contains(0.004));
    QVERIFY(plot->yAxis->range().contains(0.0));
    auto* zeroLine = plot->findChild<QCPItemStraightLine*>(
        QStringLiteral("zeroLine"));
    QVERIFY(zeroLine != nullptr);
    QCOMPARE(zeroLine->point1->coords().x(), 0.0);
    QCOMPARE(zeroLine->point2->coords().x(), 1.2);
  }

  void inviscidMessageLeavesPlotsEmpty() {
    ResultPlotView view;
    view.clear();
    view.setMessage(
        QStringLiteral("No skin-friction distribution for inviscid point"));
    QCOMPARE(view.messageText(),
             QStringLiteral(
                 "No skin-friction distribution for inviscid point"));
    QCOMPARE(view.upperPointCount(), std::size_t{0});
    QCOMPARE(view.lowerPointCount(), std::size_t{0});
    QVERIFY(!view.zeroLineVisible());
  }

  void sampleMarkersDefaultOffAndToggleIndependently() {
    ResultPlotView first;
    ResultPlotView second;
    first.resize(480, 320);
    second.resize(480, 320);
    first.show();
    second.show();
    QCoreApplication::processEvents();
    first.setCpDistribution(cpDistribution());
    second.setCpDistribution(cpDistribution());
    QVERIFY(!first.sampleMarkersEnabled());
    QVERIFY(!second.sampleMarkersEnabled());

    QCustomPlot* plot = canvas(first);
    QTest::mouseClick(plot, Qt::RightButton, Qt::ControlModifier,
                      QPoint(100, 100));
    QVERIFY(first.sampleMarkersEnabled());
    QVERIFY(!second.sampleMarkersEnabled());
    QCOMPARE(plot->graph(0)->scatterStyle().shape(),
             QCPScatterStyle::ssCircle);
    QCOMPARE(plot->graph(1)->scatterStyle().shape(),
             QCPScatterStyle::ssCircle);
    QCOMPARE(plot->graph(1)->scatterStyle().pen().color(),
             QColor(198, 93, 27));
    QTest::mouseClick(plot, Qt::RightButton, Qt::ControlModifier,
                      QPoint(100, 100));
    QVERIFY(!first.sampleMarkersEnabled());
    QCOMPARE(plot->graph(0)->scatterStyle().shape(),
             QCPScatterStyle::ssNone);
    QCOMPARE(plot->graph(1)->scatterStyle().shape(),
             QCPScatterStyle::ssNone);
  }

  void rightClickWithoutControlDoesNotToggleMarkers() {
    ResultPlotView view;
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();
    view.setCpDistribution(cpDistribution());
    QCustomPlot* plot = canvas(view);
    QTest::mouseClick(plot, Qt::RightButton, Qt::NoModifier,
                      QPoint(100, 100));
    QVERIFY(!view.sampleMarkersEnabled());
  }
};

int runGuiDistributionPlotTests(int argc, char** argv) {
  GuiDistributionPlotTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiDistributionPlotTests.moc"

// End: tests/gui/GuiDistributionPlotTests.cpp

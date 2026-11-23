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

// File: tests/gui/XFoilWidgetG6dTests.cpp
// G6d memory projections: canonical Cp text, report view and active-plot
// PNG/PDF/SVG rendering. No file path or file dialog enters the widget.

#include <string>

#include <QImage>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTabWidget>
#include <QtTest/QTest>

#include "app/GuiFileAdapters.h"
#include "plots/ResultPlotView.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::AnalysisConfig;
using xfoil::LiftTarget;
using xfoil::PanelingOptions;
using xfoil::PolarData;
using xfoil::PolarPoint;
using xfoil::gui::XFoilWidget;

void configureAndAnalyze(XFoilWidget& widget) {
  QVERIFY(widget.setNaca("0012").isOk());
  PanelingOptions paneling;
  paneling.panelCount = 80;
  QVERIFY(widget.generatePanel(paneling).isOk());
  AnalysisConfig config;
  config.viscous = false;
  config.maximumIterations = 50;
  QVERIFY(widget.setConfiguration(config).isOk());
  widget.setTarget(LiftTarget{0.5});
  widget.analyze();
  QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
  QVERIFY(widget.currentPolar().has_value());
}

PolarData comparisonPolar() {
  PolarData polar;
  polar.metadata.name = "Imported";
  PolarPoint point;
  point.alpha = xfoil::Degrees{2.0};
  point.liftCoefficient = 0.5;
  point.dragCoefficient = 0.02;
  point.momentCoefficient = -0.1;
  point.convergence = xfoil::ConvergenceStatus::converged;
  polar.points.push_back(point);
  return polar;
}

}  // namespace

class XFoilWidgetG6dTests : public QObject {
  Q_OBJECT

 private slots:
  void emptyWidgetRejectsCpAndStillHasAPlotSurface() {
    XFoilWidget widget;
    QVERIFY(!widget.hasCurrentCp());
    QVERIFY(!widget.selectedPressureExport().has_value());
    QVERIFY(widget.hasActivePlot());
  }

  void currentCpUsesCanonicalTextAndValidPoint() {
    XFoilWidget widget;
    configureAndAnalyze(widget);
    QTRY_VERIFY_WITH_TIMEOUT(widget.hasCurrentCp(), 5000);
    const auto data = widget.selectedPressureExport();
    QVERIFY(data.has_value());
    const auto cp = xfoil::gui::app::encodePressureDumpText(*data);
    QVERIFY(cp.isOk());
    QVERIFY(cp.value().starts_with("#      x          Cp"));
    QVERIFY(cp.value().find("nan") == std::string::npos);
    QVERIFY(cp.value().find("inf") == std::string::npos);
    QVERIFY(cp.value().size() > 100);
  }

  void reportViewIsStructuredReadOnlyAndUpdates() {
    XFoilWidget widget;
    auto* report = widget.findChild<QPlainTextEdit*>("reportView");
    QVERIFY(report != nullptr);
    QVERIFY(report->isReadOnly());
    QVERIFY(report->toPlainText().contains("OPERATING POINT"));

    configureAndAnalyze(widget);
    QVERIFY(widget.addComparisonPolar(comparisonPolar()).isOk());
    const QString text = report->toPlainText();
    QVERIFY(text.contains("OPERATING POINT"));
    QVERIFY(text.contains("Alpha:"));
    QVERIFY(text.contains("SEQUENCE / POLAR"));
    QVERIFY(text.contains("COMPARISON POLARS"));
    QVERIFY(text.contains("Stored: 1"));
    QVERIFY(text.contains("Imported"));
    QVERIFY(text.contains("not a file format"));
  }

  void activePlotRendersPngPdfAndVectorSvg() {
    XFoilWidget widget;
    configureAndAnalyze(widget);
    auto* tabs = widget.findChild<QTabWidget*>("resultPlotTabs");
    QVERIFY(tabs != nullptr);
    QSignalSpy changed(&widget, &XFoilWidget::activePlotChanged);
    tabs->setCurrentWidget(widget.findChild<QWidget*>("cpAlphaPage"));
    QCOMPARE(changed.count(), 1);

    auto* viewWidget = widget.findChild<QWidget*>("cpAlphaPlot");
    QVERIFY(viewWidget != nullptr);
    auto* view = dynamic_cast<xfoil::gui::ResultPlotView*>(viewWidget);
    QVERIFY(view != nullptr);
    auto* plot = view->findChild<QCustomPlot*>();
    QVERIFY(plot != nullptr);
    view->setSampleMarkersEnabled(true);
    plot->xAxis->setRange(0.05, 0.55);
    plot->yAxis->setRange(-1.20, 0.20);
    plot->graph(0)->setSelection(QCPDataSelection(QCPDataRange(0, 1)));
    const QCPRange xRange = plot->xAxis->range();
    const QCPRange yRange = plot->yAxis->range();
    const int graphCount = plot->graphCount();
    const int graphDataCount = plot->graph(0)->dataCount();
    const int tabIndex = tabs->currentIndex();
    const auto stateBefore = widget.state();
    const std::size_t polarPointsBefore = widget.currentPolar()->points.size();
    const bool markersBefore = view->sampleMarkersEnabled();

    const auto png = widget.activePlotPng();
    QVERIFY(png.isOk());
    const QByteArray pngBytes = QByteArray::fromStdString(png.value());
    QVERIFY(pngBytes.startsWith("\x89PNG\r\n\x1a\n"));
    const QImage image = QImage::fromData(pngBytes, "PNG");
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(1200, 800));

    const auto pdf = widget.activePlotPdf();
    QVERIFY(pdf.isOk());
    QVERIFY(pdf.value().starts_with("%PDF"));
    QVERIFY(pdf.value().size() > 1000);

    const auto svg = widget.activePlotSvg();
    QVERIFY(svg.isOk());
    QVERIFY(svg.value().find("<svg") != std::string::npos);
    QVERIFY(svg.value().find("<image") == std::string::npos);
    QVERIFY(svg.value().find("XFOIL.next analysis plot") !=
            std::string::npos);
    QCOMPARE(tabs->currentIndex(), tabIndex);
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("cpAlphaPage"));
    QCOMPARE(plot->xAxis->range().lower, xRange.lower);
    QCOMPARE(plot->xAxis->range().upper, xRange.upper);
    QCOMPARE(plot->yAxis->range().lower, yRange.lower);
    QCOMPARE(plot->yAxis->range().upper, yRange.upper);
    QCOMPARE(plot->graphCount(), graphCount);
    QCOMPARE(plot->graph(0)->dataCount(), graphDataCount);
    QCOMPARE(plot->selectedPlottables().size(), 1);
    QCOMPARE(plot->graph(0)->selection().dataRange(0),
             QCPDataRange(0, 1));
    QCOMPARE(widget.state(), stateBefore);
    QCOMPARE(widget.currentPolar()->points.size(), polarPointsBefore);
    QCOMPARE(view->sampleMarkersEnabled(), markersBefore);
  }
};

int runXFoilWidgetG6dTests(int argc, char** argv) {
  XFoilWidgetG6dTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetG6dTests.moc"

// End: tests/gui/XFoilWidgetG6dTests.cpp

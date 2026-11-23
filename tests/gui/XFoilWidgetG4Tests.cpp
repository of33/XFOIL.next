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

// File: tests/gui/XFoilWidgetG4Tests.cpp
// G4 widget integration: selected point, curves, matrix, filtering,
// Cp/Cf details, click synchronization and no extra analysis.

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include "plots/ResultPlotView.h"
#include "plots/ScalarPlotView.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AlphaTarget;
using xfoil::AnalysisConfig;
using xfoil::Degrees;
using xfoil::PanelingOptions;
using xfoil::gui::ResultPlotView;
using xfoil::gui::ScalarPlotView;
using xfoil::gui::XFoilGuiState;
using xfoil::gui::XFoilWidget;

void configureSequence(XFoilWidget& widget, double end = 3.0) {
  QVERIFY(widget.setNaca("0012").isOk());
  QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
  widget.setSequence(
      AlphaSequence{Degrees{0.0}, Degrees{end}, Degrees{1.0}});
}

QComboBox* combo(XFoilWidget& widget, const char* name) {
  QComboBox* value =
      widget.findChild<QComboBox*>(QString::fromLatin1(name));
  Q_ASSERT(value != nullptr);
  return value;
}

ScalarPlotView* scalar(XFoilWidget& widget, const char* name) {
  QWidget* base = widget.findChild<QWidget*>(QString::fromLatin1(name));
  auto* value = dynamic_cast<ScalarPlotView*>(base);
  Q_ASSERT(value != nullptr);
  return value;
}

ResultPlotView* distribution(XFoilWidget& widget, const char* name) {
  QWidget* base = widget.findChild<QWidget*>(QString::fromLatin1(name));
  auto* value = dynamic_cast<ResultPlotView*>(base);
  Q_ASSERT(value != nullptr);
  return value;
}

void sendClick(QWidget* widget, const QPoint& point) {
  QMouseEvent press(QEvent::MouseButtonPress, QPointF(point),
                    QPointF(widget->mapToGlobal(point)), Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(widget, &press);
  QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point),
                      QPointF(widget->mapToGlobal(point)),
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(widget, &release);
}

}  // namespace

class XFoilWidgetG4Tests : public QObject {
  Q_OBJECT

 private slots:
  void sequencePopulatesSelectionPlotsAndMatrix() {
    XFoilWidget widget;
    configureSequence(widget, 3.0);
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QCOMPARE(started.count(), 1);
    QVERIFY(widget.state() == XFoilGuiState::results);

    QComboBox* operating = combo(widget, "operatingPointCombo");
    QCOMPARE(operating->count(), 3);
    QCOMPARE(operating->currentIndex(), 0);
    QCOMPARE(combo(widget, "deactivatePointCombo")->count(), 3);
    QCOMPARE(combo(widget, "activatePointCombo")->count(), 0);
    auto* convergenceIcon =
        widget.findChild<QLabel*>("currentPointConvergenceIcon");
    QVERIFY(convergenceIcon != nullptr);
    QVERIFY(convergenceIcon->styleSheet().contains(
        QStringLiteral("#2e7d32")));

    for (const char* name :
         {"clAlphaPlot", "clCdPlot", "cmAlphaPlot", "cmClPlot",
          "efficiencyClPlot"}) {
      ScalarPlotView* view = scalar(widget, name);
      QCOMPARE(view->visiblePointCount(), std::size_t{3});
      QCOMPARE(view->solidPointCount(), std::size_t{3});
      QCOMPARE(view->dashedPointCount(), std::size_t{0});
      QCOMPARE(view->selectedMarkerCount(), std::size_t{1});
      QCOMPARE(view->selectedSourceIndex().value(), std::size_t{0});
    }

    auto* cp = distribution(widget, "cpPlot");
    auto* cpDetail = distribution(widget, "cpAlphaPlot");
    auto* cf = distribution(widget, "cfAlphaPlot");
    QVERIFY(cp != nullptr);
    QVERIFY(cpDetail != nullptr);
    QVERIFY(cf != nullptr);
    QVERIFY(cp->upperPointCount() > 0);
    QVERIFY(cp->lowerPointCount() > 0);
    QCOMPARE(cp->upperPointCount(), cpDetail->upperPointCount());
    QCOMPARE(cp->lowerPointCount(), cpDetail->lowerPointCount());
    QCPGraph* cpUpper = cp->findChild<QCPGraph*>("upperDistribution");
    QCPGraph* detailUpper =
        cpDetail->findChild<QCPGraph*>("upperDistribution");
    QVERIFY(cpUpper != nullptr);
    QVERIFY(detailUpper != nullptr);
    QCOMPARE(cpUpper->dataCount(), detailUpper->dataCount());
    QCOMPARE(cpUpper->data()->constBegin()->value,
             detailUpper->data()->constBegin()->value);
    QVERIFY(cf->upperPointCount() > 0);
    QVERIFY(cf->zeroLineVisible());

    QVERIFY(widget.findChild<QLabel*>("cdValue")->text() !=
            QStringLiteral("n/a"));
    QVERIFY(widget.findChild<QLabel*>("clValue")->text() !=
            QStringLiteral("n/a"));
    QVERIFY(widget.findChild<QLabel*>("cmValue")->text() !=
            QStringLiteral("n/a"));
    QVERIFY(widget.findChild<QLabel*>("efficiencyValue")->text() !=
            QStringLiteral("n/a"));
    QVERIFY(widget.findChild<QLabel*>("cd0Value")->text() !=
            QStringLiteral("n/a"));

    const QString diagnostics =
        widget.findChild<QPlainTextEdit*>("setupDiagnosticsView")
            ->toPlainText();
    QVERIFY(diagnostics.contains(QStringLiteral("Highest computed CL")));
    QVERIFY(diagnostics.contains(QStringLiteral("Highest computed CL/CD")));
    QLabel* clMax = widget.findChild<QLabel*>("clMaxValue");
    QLabel* efficiencyMax =
        widget.findChild<QLabel*>("efficiencyMaxValue");
    QVERIFY(clMax != nullptr);
    QVERIFY(efficiencyMax != nullptr);
    QVERIFY(clMax->toolTip().contains(QStringLiteral("Highest computed CL")));
    QVERIFY(efficiencyMax->toolTip().contains(
        QStringLiteral("Highest computed CL/CD")));
  }

  void selectingACombinationPointSynchronizesWithoutSolving() {
    XFoilWidget widget;
    configureSequence(widget, 3.0);
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QCOMPARE(started.count(), 1);

    QComboBox* operating = combo(widget, "operatingPointCombo");
    operating->setCurrentIndex(1);
    QCOMPARE(operating->currentData().toULongLong(), qulonglong{1});
    for (const char* name :
         {"clAlphaPlot", "clCdPlot", "cmAlphaPlot", "cmClPlot",
          "efficiencyClPlot"}) {
      ScalarPlotView* view = scalar(widget, name);
      QCOMPARE(view->selectedSourceIndex().value(), std::size_t{1});
      QCOMPARE(view->selectedMarkerCount(), std::size_t{1});
    }
    QCOMPARE(started.count(), 1);
  }

  void scalarPlotClickSelectsAndSynchronizesDropdowns() {
    XFoilWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    configureSequence(widget, 3.0);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QCoreApplication::processEvents();

    ScalarPlotView* view = scalar(widget, "clAlphaPlot");
    QCustomPlot* plot = view->findChild<QCustomPlot*>();
    QVERIFY(plot != nullptr);
    QCPGraph* markers = plot->findChild<QCPGraph*>(
        QStringLiteral("unselectedMarkers"));
    QVERIFY(markers != nullptr);
    QVERIFY(markers->dataCount() > 0);
    const auto& data = *markers->data();
    const QCPGraphData first = *data.constBegin();
    const QPoint pixel(
        static_cast<int>(std::lround(
            plot->xAxis->coordToPixel(first.key))),
        static_cast<int>(std::lround(
            plot->yAxis->coordToPixel(first.value))));
    sendClick(plot, pixel);

    QComboBox* operating = combo(widget, "operatingPointCombo");
    QCOMPARE(operating->currentData().toULongLong(), qulonglong{1});
    for (const char* name :
         {"clAlphaPlot", "clCdPlot", "cmAlphaPlot", "cmClPlot",
          "efficiencyClPlot"}) {
      QCOMPARE(scalar(widget, name)->selectedSourceIndex().value(),
               std::size_t{1});
    }
  }

  void deactivateAndActivateRestoreEnabledSourceOrder() {
    XFoilWidget widget;
    configureSequence(widget, 3.0);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);

    QComboBox* operating = combo(widget, "operatingPointCombo");
    QComboBox* deactivate = combo(widget, "deactivatePointCombo");
    QComboBox* activate = combo(widget, "activatePointCombo");
    QLabel* clMax = widget.findChild<QLabel*>("clMaxValue");
    QVERIFY(clMax != nullptr);
    QCOMPARE(deactivate->count(), 3);
    const QString unfilteredClMax = clMax->text();
    deactivate->setCurrentIndex(2);
    QCOMPARE(operating->count(), 2);
    QCOMPARE(activate->count(), 1);
    QCOMPARE(operating->currentData().toULongLong(), qulonglong{0});
    QVERIFY(clMax->text() != unfilteredClMax);
    activate->setCurrentIndex(0);
    QCOMPARE(operating->count(), 3);
    QCOMPARE(activate->count(), 0);
    QCOMPARE(clMax->text(), unfilteredClMax);

    deactivate->setCurrentIndex(0);
    QCOMPARE(operating->count(), 2);
    QCOMPARE(activate->count(), 1);
    QCOMPARE(operating->currentData().toULongLong(), qulonglong{1});
    for (const char* name :
         {"clAlphaPlot", "clCdPlot", "cmAlphaPlot", "cmClPlot",
          "efficiencyClPlot"}) {
      QCOMPARE(scalar(widget, name)->visiblePointCount(), std::size_t{2});
    }

    activate->setCurrentIndex(0);
    QCOMPARE(operating->count(), 3);
    QCOMPARE(activate->count(), 0);
    QCOMPARE(operating->currentData().toULongLong(), qulonglong{1});
    QCOMPARE(operating->itemData(0).toULongLong(), qulonglong{0});
    QCOMPARE(operating->itemData(1).toULongLong(), qulonglong{1});
    QCOMPARE(operating->itemData(2).toULongLong(), qulonglong{2});
  }

  void inviscidPointClearsCfAndShowsMessage() {
    XFoilWidget widget;
    QVERIFY(widget.setNaca("0012").isOk());
    QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
    widget.setTarget(AlphaTarget{Degrees{2.0}});
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    auto* cf = distribution(widget, "cfAlphaPlot");
    QVERIFY(cf != nullptr);
    QVERIFY(cf->upperPointCount() > 0);
    QVERIFY(cf->lowerPointCount() > 0);

    AnalysisConfig config;
    config.viscous = false;
    QVERIFY(widget.setConfiguration(config).isOk());
    widget.setTarget(AlphaTarget{Degrees{2.0}});
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.state() == XFoilGuiState::results);

    cf = distribution(widget, "cfAlphaPlot");
    QVERIFY(cf != nullptr);
    QCOMPARE(cf->upperPointCount(), std::size_t{0});
    QCOMPARE(cf->lowerPointCount(), std::size_t{0});
    QVERIFY(!cf->zeroLineVisible());
    QCOMPARE(cf->messageText(),
             QStringLiteral(
                 "No skin-friction distribution for inviscid point"));
    QCOMPARE(widget.findChild<QLabel*>("cdfValue")->text(),
             QStringLiteral("n/a"));
  }

  void setupChangeClearsResultsAndPlots() {
    XFoilWidget widget;
    configureSequence(widget, 3.0);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QCOMPARE(combo(widget, "operatingPointCombo")->count(), 3);
    QVERIFY(scalar(widget, "clAlphaPlot")->visiblePointCount() > 0);

    widget.setTarget(AlphaTarget{Degrees{2.0}});
    QCOMPARE(combo(widget, "operatingPointCombo")->count(), 0);
    QCOMPARE(scalar(widget, "clAlphaPlot")->visiblePointCount(),
             std::size_t{0});
    QCOMPARE(widget.findChild<QLabel*>("clValue")->text(),
             QStringLiteral("n/a"));
    QCOMPARE(distribution(widget, "cpPlot")->upperPointCount(),
             std::size_t{0});
  }
};

int runXFoilWidgetG4Tests(int argc, char** argv) {
  XFoilWidgetG4Tests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetG4Tests.moc"

// End: tests/gui/XFoilWidgetG4Tests.cpp

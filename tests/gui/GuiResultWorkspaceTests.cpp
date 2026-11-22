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

// File: tests/gui/GuiResultWorkspaceTests.cpp
// G2a layout, placeholder, matrix, convergence and plot-shell tests.

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest/QTest>

#include "model/GuiResultSummary.h"
#include "plots/GeometryPlotView.h"
#include "plots/ResultPlotView.h"
#include "plots/ScalarPlotView.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::gui::ResultPlotView;
using xfoil::gui::XFoilGuiState;
using xfoil::gui::XFoilWidget;

QLayout* layoutOf(QWidget* widget, const char* name) {
  QLayout* layout = widget->findChild<QLayout*>(QString::fromLatin1(name));
  Q_ASSERT(layout != nullptr);
  return layout;
}

ResultPlotView* resultPlot(QWidget& widget, const QString& name) {
  auto* base = widget.findChild<QWidget*>(name);
  Q_ASSERT(base != nullptr);
  auto* plot = dynamic_cast<ResultPlotView*>(base);
  Q_ASSERT(plot != nullptr);
  return plot;
}

xfoil::gui::ScalarPlotView* scalarPlot(QWidget& widget,
                                       const QString& name) {
  auto* base = widget.findChild<QWidget*>(name);
  Q_ASSERT(base != nullptr);
  auto* plot = dynamic_cast<xfoil::gui::ScalarPlotView*>(base);
  Q_ASSERT(plot != nullptr);
  return plot;
}

QCustomPlot* canvas(QWidget& view) {
  QCustomPlot* plot = view.findChild<QCustomPlot*>();
  Q_ASSERT(plot != nullptr);
  return plot;
}

QStringList widgetNames(QLayout* layout) {
  QStringList names;
  for (int index = 0; index < layout->count(); ++index) {
    if (QLayoutItem* item = layout->itemAt(index); item->widget() != nullptr) {
      names.push_back(item->widget()->objectName());
    }
  }
  return names;
}

}  // namespace

class GuiResultWorkspaceTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void definitionsAreOneByFourInWorkflowOrder() {
    XFoilWidget widget;
    auto* scroll = widget.findChild<QScrollArea*>("definitionsScrollArea");
    QVERIFY(scroll != nullptr);
    auto* row = widget.findChild<QWidget*>("definitionsRow");
    QVERIFY(row != nullptr);
    QCOMPARE(scroll->widget(), row);

    auto* rowLayout = dynamic_cast<QHBoxLayout*>(
        layoutOf(row, "definitionsRowLayout"));
    QVERIFY(rowLayout != nullptr);
    QCOMPARE(rowLayout->count(), 4);
    QVERIFY(widgetNames(rowLayout) ==
            (QStringList{"geometryGroup", "panelingGroup",
                         "operatingGroup", "targetGroup"}));
    for (int index = 0; index < rowLayout->count(); ++index) {
      QCOMPARE(rowLayout->itemAt(index)->widget()->sizePolicy()
                   .horizontalStretch(),
               1);
    }
  }

  void workspaceHasThreeColumnsWithFixedRightColumn() {
    XFoilWidget widget;
    auto* workspace = widget.findChild<QWidget*>("resultWorkspace");
    QVERIFY(workspace != nullptr);
    auto* workspaceLayout = dynamic_cast<QHBoxLayout*>(
        layoutOf(workspace, "resultWorkspaceLayout"));
    QVERIFY(workspaceLayout != nullptr);
    QCOMPARE(workspaceLayout->count(), 3);
    QVERIFY(widgetNames(workspaceLayout) ==
            (QStringList{"leftPlotColumn", "resultPlotTabs",
                         "resultsColumn"}));

    auto* left = widget.findChild<QWidget*>("leftPlotColumn");
    auto* middle = widget.findChild<QTabWidget*>("resultPlotTabs");
    auto* right = widget.findChild<QFrame*>("resultsColumn");
    QVERIFY(left != nullptr);
    QVERIFY(middle != nullptr);
    QVERIFY(right != nullptr);
    QCOMPARE(left->sizePolicy().horizontalStretch(), 1);
    QCOMPARE(middle->sizePolicy().horizontalStretch(), 1);
    QCOMPARE(right->minimumWidth(), 340);
    QCOMPARE(right->maximumWidth(), 340);

    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    QCOMPARE(right->width(), 340);
    const int firstWidth = left->width();
    const int secondWidth = middle->width();
    QVERIFY(std::abs(firstWidth - secondWidth) <= 1);

    widget.resize(960, 640);
    QCoreApplication::processEvents();
    QCOMPARE(right->width(), 340);
    QVERIFY(std::abs(left->width() - middle->width()) <= 1);
    QVERIFY(right->minimumSizeHint().width() <= right->width());
  }

  void resultsColumnOrderEndsWithAnalyze() {
    XFoilWidget widget;
    auto* right = widget.findChild<QFrame*>("resultsColumn");
    QVERIFY(right != nullptr);
    QLayout* layout = right->layout();
    QVERIFY(layout != nullptr);

    QStringList widgetItems;
    int analyzeIndex = -1;
    for (int index = 0; index < layout->count(); ++index) {
      QLayoutItem* item = layout->itemAt(index);
      if (item->widget() == nullptr) {
        continue;
      }
      widgetItems.push_back(item->widget()->objectName());
      if (item->widget()->objectName() == QStringLiteral("analyzeButton")) {
        analyzeIndex = index;
      }
    }
    QVERIFY(widgetItems ==
            (QStringList{"pointSelectionGroup", "resultMatrixGroup",
                         "setupDiagnosticsGroup", "polarManagerGroup",
                         "analyzeButton"}));
    QVERIFY(analyzeIndex == layout->count() - 1);
  }

  void leftColumnHasCpAboveAirfoilAtTwoToOne() {
    XFoilWidget widget;
    auto* cpSection = widget.findChild<QWidget*>("cpSection");
    auto* airfoilSection = widget.findChild<QWidget*>("airfoilSection");
    QVERIFY(cpSection != nullptr);
    QVERIFY(airfoilSection != nullptr);
    QCOMPARE(cpSection->sizePolicy().verticalStretch(), 2);
    QCOMPARE(airfoilSection->sizePolicy().verticalStretch(), 1);

    QCOMPARE(widget.findChild<QLabel*>("cpTitleLabel")->text(),
             QStringLiteral("Cp"));
    QCOMPARE(widget.findChild<QLabel*>("airfoilTitleLabel")->text(),
             QStringLiteral("Airfoil"));

    auto* cp = resultPlot(widget, QStringLiteral("cpPlot"));
    QCustomPlot* plot = canvas(*cp);
    QCOMPARE(plot->yAxis->label(), QStringLiteral("-Cp"));
    QCOMPARE(plot->graphCount(), 2);
    QCOMPARE(plot->graph(0)->name(), QStringLiteral("Upper"));
    QCOMPARE(plot->graph(1)->name(), QStringLiteral("Lower"));
    QCOMPARE(plot->graph(0)->dataCount(), 0);
    QCOMPARE(plot->graph(1)->dataCount(), 0);

    auto* airfoil = dynamic_cast<xfoil::gui::GeometryPlotView*>(
        widget.findChild<QWidget*>("airfoilPlot"));
    QVERIFY(airfoil != nullptr);
  }

  void middleTabsAreSevenInOrderAndEmpty() {
    XFoilWidget widget;
    auto* tabs = widget.findChild<QTabWidget*>("resultPlotTabs");
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 7);
    const QStringList titles = {
        QStringLiteral("CL(α)"), QStringLiteral("CL(CD)"),
        QStringLiteral("CM(α)"), QStringLiteral("CM(CL)"),
        QStringLiteral("CL/CD(CL)"), QStringLiteral("Cp"),
        QStringLiteral("Cf")};
    const QStringList pages = {
        QStringLiteral("clAlphaPage"), QStringLiteral("clCdPage"),
        QStringLiteral("cmAlphaPage"), QStringLiteral("cmClPage"),
        QStringLiteral("efficiencyClPage"), QStringLiteral("cpAlphaPage"),
        QStringLiteral("cfAlphaPage")};
    const QStringList plots = {
        QStringLiteral("clAlphaPlot"), QStringLiteral("clCdPlot"),
        QStringLiteral("cmAlphaPlot"), QStringLiteral("cmClPlot"),
        QStringLiteral("efficiencyClPlot"), QStringLiteral("cpAlphaPlot"),
        QStringLiteral("cfAlphaPlot")};
    for (int index = 0; index < 7; ++index) {
      QCOMPARE(tabs->tabText(index), titles.at(index));
      QCOMPARE(tabs->widget(index)->objectName(), pages.at(index));
      QWidget* view = widget.findChild<QWidget*>(plots.at(index));
      QVERIFY(view != nullptr);
      QCustomPlot* plot = canvas(*view);
      QVERIFY(plot->xAxis->label().isEmpty() == false);
      QVERIFY(plot->yAxis->label().isEmpty() == false);
      if (index < 5) {
        auto* scalar = scalarPlot(widget, plots.at(index));
        QCOMPARE(scalar->kind(),
                 index == 0 ? xfoil::gui::GuiScalarPlotKind::clAlpha
                 : index == 1 ? xfoil::gui::GuiScalarPlotKind::clCd
                 : index == 2 ? xfoil::gui::GuiScalarPlotKind::cmAlpha
                 : index == 3 ? xfoil::gui::GuiScalarPlotKind::cmCl
                              : xfoil::gui::GuiScalarPlotKind::efficiencyCl);
        QCOMPARE(plot->graphCount(), 2);
        QCOMPARE(plot->plottableCount(), 5);
      } else {
        QCOMPARE(plot->graphCount(), 2);
      }
    }
  }

  void resultMatrixIsFourByThreeAndStartsUnavailable() {
    XFoilWidget widget;
    auto* group = widget.findChild<QGroupBox*>("resultMatrixGroup");
    QVERIFY(group != nullptr);
    auto* grid = dynamic_cast<QGridLayout*>(
        layoutOf(group, "resultMatrixLayout"));
    QVERIFY(grid != nullptr);
    QCOMPARE(grid->rowCount(), 4);
    QCOMPARE(grid->columnCount(), 3);

    const QStringList values = {
        QStringLiteral("cdValue"), QStringLiteral("cdfValue"),
        QStringLiteral("cdpValue"), QStringLiteral("clValue"),
        QStringLiteral("cmValue"), QStringLiteral("efficiencyValue"),
        QStringLiteral("cd0Value"), QStringLiteral("cl0Value"),
        QStringLiteral("cm0Value"), QStringLiteral("alpha0Value"),
        QStringLiteral("clMaxValue"),
        QStringLiteral("efficiencyMaxValue")};
    for (const QString& name : values) {
      auto* label = widget.findChild<QLabel*>(name);
      QVERIFY2(label != nullptr, qPrintable(name));
      QCOMPARE(label->text(), QStringLiteral("n/a"));
    }

    const xfoil::gui::GuiResultSummary summary;
    QCOMPARE(QString::fromStdString(
                 xfoil::gui::formatResultValue(summary.cd)),
             QStringLiteral("n/a"));
    QCOMPARE(QString::fromStdString(
                 xfoil::gui::formatResultValue(std::optional<double>{1.5})),
             QStringLiteral("1.500000"));
  }

  void pointSelectionAndConvergenceStartNeutral() {
    XFoilWidget widget;
    auto* operating = widget.findChild<QComboBox*>("operatingPointCombo");
    auto* deactivate = widget.findChild<QComboBox*>("deactivatePointCombo");
    auto* activate = widget.findChild<QComboBox*>("activatePointCombo");
    QVERIFY(operating != nullptr);
    QVERIFY(deactivate != nullptr);
    QVERIFY(activate != nullptr);
    QCOMPARE(operating->count(), 0);
    QCOMPARE(deactivate->count(), 0);
    QCOMPARE(activate->count(), 0);
    QCOMPARE(widget.findChild<QLabel*>("currentPointConvergenceLabel")->text(),
             QStringLiteral("No point selected"));
    QVERIFY(!widget.findChild<QLabel*>("currentPointConvergenceIcon")
                 ->text()
                 .isEmpty());
  }

  void diagnosticsAreReadOnlyAndAnalyzeBecomesAvailable() {
    XFoilWidget widget;
    auto* diagnostics =
        widget.findChild<QPlainTextEdit*>("setupDiagnosticsView");
    QVERIFY(diagnostics != nullptr);
    QVERIFY(diagnostics->isReadOnly());
    QCOMPARE(diagnostics->placeholderText(),
             QStringLiteral("No setup diagnostics"));

    auto* analyze = widget.findChild<QPushButton*>("analyzeButton");
    QVERIFY(analyze != nullptr);
    QVERIFY(!analyze->isEnabled());

    QVERIFY(widget.setNaca("2412").isOk());
    QVERIFY(widget.generatePanel(xfoil::PanelingOptions{}).isOk());
    QVERIFY(analyze->isEnabled());
    QVERIFY(widget.isReadyToAnalyze());
    QVERIFY(!widget.isRunning());
  }

  void newObjectNamesAreSemantic() {
    XFoilWidget widget;
    const QRegularExpression generic(
        QStringLiteral("^(frame|label|comboBox|widget|groupBox|pushButton)_?\\d+$"));
    for (QWidget* child : widget.findChildren<QWidget*>()) {
      QVERIFY2(!generic.match(child->objectName()).hasMatch(),
               qPrintable(child->objectName()));
    }
  }
};

int runGuiResultWorkspaceTests(int argc, char** argv) {
  GuiResultWorkspaceTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiResultWorkspaceTests.moc"

// End: tests/gui/GuiResultWorkspaceTests.cpp

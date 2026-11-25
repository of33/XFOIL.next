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
#include <QDoubleSpinBox>
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
#include <QSpinBox>
#include <QSplitter>
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

  void workspaceIsAHorizontalSplitterWithThreeColumns() {
    XFoilWidget widget;
    auto* workspace = widget.findChild<QWidget*>("resultWorkspace");
    QVERIFY(workspace != nullptr);
    auto* workspaceLayout = dynamic_cast<QHBoxLayout*>(
        layoutOf(workspace, "resultWorkspaceLayout"));
    QVERIFY(workspaceLayout != nullptr);
    QCOMPARE(workspaceLayout->count(), 1);

    auto* splitter = widget.findChild<QSplitter*>("resultSplitter");
    QVERIFY(splitter != nullptr);
    QCOMPARE(splitter->orientation(), Qt::Horizontal);
    QCOMPARE(splitter->count(), 3);
    QVERIFY(!splitter->childrenCollapsible());
    QStringList splitterNames;
    for (int index = 0; index < splitter->count(); ++index) {
      splitterNames.push_back(splitter->widget(index)->objectName());
    }
    QVERIFY(splitterNames ==
            (QStringList{"leftPlotColumn", "resultPlotTabs",
                         "resultsColumn"}));

    auto* left = widget.findChild<QWidget*>("leftPlotColumn");
    auto* middle = widget.findChild<QTabWidget*>("resultPlotTabs");
    auto* right = widget.findChild<QFrame*>("resultsColumn");
    QVERIFY(left != nullptr);
    QVERIFY(middle != nullptr);
    QVERIFY(right != nullptr);
    QCOMPARE(right->minimumWidth(), 280);

    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    const int total = left->width() + middle->width() + right->width() +
                      splitter->handleWidth() * 2;
    QVERIFY(total > 0);
    QVERIFY(std::abs(left->width() / static_cast<double>(total) - 0.35) < 0.03);
    QVERIFY(std::abs(middle->width() / static_cast<double>(total) - 0.35) < 0.03);
    QVERIFY(std::abs(right->width() / static_cast<double>(total) - 0.30) < 0.03);

    widget.resize(960, 620);
    QCoreApplication::processEvents();
    QVERIFY(right->width() >= right->minimumWidth());
    QVERIFY(left->width() > 0);
    QVERIFY(middle->width() > 0);
  }

  void mainSplitterKeepsDefinitionsAtTargetHeight() {
    XFoilWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    auto* splitter = widget.findChild<QSplitter*>("mainSplitter");
    auto* frame = widget.findChild<QWidget*>("definitionsFrame");
    auto* scroll =
        widget.findChild<QScrollArea*>("definitionsScrollArea");
    auto* row = widget.findChild<QWidget*>("definitionsRow");
    QVERIFY(splitter != nullptr);
    QVERIFY(frame != nullptr);
    QVERIFY(scroll != nullptr);
    QVERIFY(row != nullptr);
    QCOMPARE(splitter->orientation(), Qt::Vertical);
    QCOMPARE(splitter->count(), 2);
    QVERIFY(!splitter->childrenCollapsible());
    QCOMPARE(splitter->widget(0)->objectName(),
             QStringLiteral("definitionsFrame"));
    QCOMPARE(splitter->widget(1)->objectName(),
             QStringLiteral("resultWorkspace"));
    const int frameHeight = frame->sizeHint().height();
    QCOMPARE(frame->maximumHeight(), frameHeight);
    QCOMPARE(frame->height(), frameHeight);
    QCOMPARE(scroll->height(), row->sizeHint().height());
    QCOMPARE(row->height(), row->sizeHint().height());
    QCOMPARE(splitter->sizes().at(0), frameHeight);
    QVERIFY(splitter->sizes().at(1) >= 429);

    widget.resize(1280, 900);
    QCoreApplication::processEvents();
    QCOMPARE(splitter->sizes().at(0), frameHeight);
    QVERIFY(splitter->sizes().at(1) > 429);
  }

  void comparisonPolarManagerScrollsIndependently() {
    XFoilWidget widget;
    auto* scroll =
        widget.findChild<QScrollArea*>("comparisonPolarsScrollArea");
    QVERIFY(scroll != nullptr);
    QVERIFY(scroll->widgetResizable());
    auto* group = widget.findChild<QGroupBox*>("polarManagerGroup");
    QVERIFY(group != nullptr);
    QCOMPARE(scroll->widget(), group);
  }

  void definitionTabsShareHeightAndNcritIsExpert() {
    XFoilWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    auto* paneling = widget.findChild<QTabWidget*>("panelingSettingsTabs");
    auto* conditions =
        widget.findChild<QTabWidget*>("conditionSettingsTabs");
    auto* target = widget.findChild<QTabWidget*>("analysisTargetTabs");
    QVERIFY(paneling != nullptr);
    QVERIFY(conditions != nullptr);
    QVERIFY(target != nullptr);
    QCOMPARE(conditions->height(), target->height());
    QVERIFY(paneling->height() > conditions->height());
    const int frame = 2 * paneling->style()->pixelMetric(
                                QStyle::PM_DefaultFrameWidth, nullptr, paneling);
    const int expectedPanelingHeight =
        paneling->tabBar()->sizeHint().height() +
        paneling->widget(1)->sizeHint().height() + frame + 2;
    QCOMPARE(paneling->height(), expectedPanelingHeight);
    QVERIFY(paneling->widget(0)->sizeHint().height() <=
            paneling->widget(1)->sizeHint().height());
    QCOMPARE(target->count(), 4);
    QCOMPARE(target->tabText(0), QStringLiteral("Alpha"));
    QCOMPARE(target->tabText(1), QStringLiteral("Alpha Sequence"));
    QCOMPARE(target->tabText(2), QStringLiteral("CL"));
    QCOMPARE(target->tabText(3), QStringLiteral("CL Sequence"));

    auto* ncrit = widget.findChild<QDoubleSpinBox*>("ncritSpin");
    auto* iterations = widget.findChild<QSpinBox*>("iterationsSpin");
    auto* expert = widget.findChild<QWidget*>("conditionExpertPage");
    auto* basic = widget.findChild<QWidget*>("conditionBasicPage");
    QVERIFY(ncrit != nullptr);
    QVERIFY(iterations != nullptr);
    QVERIFY(expert != nullptr);
    QVERIFY(basic != nullptr);
    QVERIFY(expert->isAncestorOf(ncrit));
    QVERIFY(!basic->isAncestorOf(ncrit));
    QVERIFY(expert->isAncestorOf(iterations));
    QVERIFY(!basic->isAncestorOf(iterations));
    auto* limitsRow = ncrit->parentWidget();
    QVERIFY(limitsRow != nullptr);
    QCOMPARE(limitsRow->objectName(), QStringLiteral("expertLimitsRow"));
    QCOMPARE(iterations->parentWidget(), limitsRow);
    QVERIFY(ncrit->maximumWidth() <= 80);
    QVERIFY(iterations->maximumWidth() <= 80);
  }

  void resultsColumnContainsMatrixAndInfoTabs() {
    XFoilWidget widget;
    auto* right = widget.findChild<QFrame*>("resultsColumn");
    QVERIFY(right != nullptr);
    QLayout* layout = right->layout();
    QVERIFY(layout != nullptr);
    QVERIFY(widgetNames(layout) ==
            (QStringList{"resultMatrixGroup", "resultInfoTabs"}));

    auto* action = widget.findChild<QFrame*>("analysisActionFrame");
    QVERIFY(action != nullptr);
    auto* analyze = widget.findChild<QPushButton*>("analyzeButton");
    QVERIFY(analyze != nullptr);
    QVERIFY(action->isAncestorOf(analyze));
  }

  void leftColumnHasCpAboveAirfoilAtThreeToTwo() {
    XFoilWidget widget;
    auto* cpSection = widget.findChild<QWidget*>("cpSection");
    auto* airfoilSection = widget.findChild<QWidget*>("airfoilSection");
    auto* splitter = widget.findChild<QSplitter*>("leftPlotSplitter");
    QVERIFY(cpSection != nullptr);
    QVERIFY(airfoilSection != nullptr);
    QVERIFY(splitter != nullptr);
    QCOMPARE(splitter->orientation(), Qt::Vertical);
    QCOMPARE(splitter->count(), 2);
    QVERIFY(!splitter->childrenCollapsible());
    QVERIFY(splitter->widget(0) == cpSection);
    QVERIFY(splitter->widget(1) == airfoilSection);

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
    }
    for (int index = 0; index < 7; ++index) {
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
    auto* report = widget.findChild<QPlainTextEdit*>("reportView");
    QVERIFY(report != nullptr);
    QVERIFY(report->isReadOnly());
    QVERIFY(report->toPlainText().contains("OPERATING POINT"));
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
    QCOMPARE(grid->verticalSpacing(), 6);
    QVERIFY(grid->horizontalSpacing() >= 6);

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
    auto* tabs = widget.findChild<QTabWidget*>("resultInfoTabs");
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Diagnostics"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Report"));
    QCOMPARE(tabs->tabText(2), QStringLiteral("Comparison Polars"));
    QCOMPARE(tabs->currentIndex(), 0);
    auto* report = widget.findChild<QPlainTextEdit*>("reportView");
    QVERIFY(report != nullptr);
    QVERIFY(tabs->isAncestorOf(report));
    QCOMPARE(tabs->widget(1)->objectName(), QStringLiteral("reportPage"));
    QVERIFY(report->isReadOnly());

    auto* analyze = widget.findChild<QPushButton*>("analyzeButton");
    QVERIFY(analyze != nullptr);
    QVERIFY(!analyze->isEnabled());

    QVERIFY(widget.setNaca("2412").isOk());
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

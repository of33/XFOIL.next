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

// File: tests/gui/XFoilWidgetG6cTests.cpp
// G6c manager integration, current/imported snapshot ownership, eight-entry
// limit, comparison scalar curves, explicit rename/visibility/removal and reset.

#include <cstddef>
#include <string>

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStyledItemDelegate>
#include <QtTest/QTest>

#include "plots/ComparisonStyle.h"
#include "plots/ScalarPlotView.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::AnalysisConfig;
using xfoil::LiftTarget;
using xfoil::PanelingOptions;
using xfoil::PolarData;
using xfoil::PolarPoint;
using xfoil::gui::ScalarPlotView;
using xfoil::gui::XFoilWidget;

PolarData polar(std::string name, double cl = 0.5) {
  PolarData value;
  value.metadata.name = std::move(name);
  for (int index = 0; index < 3; ++index) {
    PolarPoint point;
    point.alpha = xfoil::Degrees{static_cast<double>(index)};
    point.liftCoefficient = cl + 0.1 * index;
    point.dragCoefficient = 0.02 + 0.001 * index;
    point.momentCoefficient = -0.1;
    point.convergence = xfoil::ConvergenceStatus::converged;
    value.points.push_back(point);
  }
  return value;
}

QListWidget* polarList(XFoilWidget& widget) {
  auto* list = widget.findChild<QListWidget*>("polarListView");
  Q_ASSERT(list != nullptr);
  return list;
}

ScalarPlotView* scalar(XFoilWidget& widget, const char* name) {
  auto* base = widget.findChild<QWidget*>(QString::fromLatin1(name));
  Q_ASSERT(base != nullptr);
  auto* view = dynamic_cast<ScalarPlotView*>(base);
  Q_ASSERT(view != nullptr);
  return view;
}

void configurePoint(XFoilWidget& widget) {
  QVERIFY(widget.setNaca("0012").isOk());
  PanelingOptions paneling;
  paneling.panelCount = 80;
  QVERIFY(widget.generatePanel(paneling).isOk());
  AnalysisConfig config;
  config.viscous = false;
  config.maximumIterations = 50;
  QVERIFY(widget.setConfiguration(config).isOk());
  widget.setTarget(LiftTarget{0.5});
}

}  // namespace

class XFoilWidgetG6cTests : public QObject {
  Q_OBJECT

 private slots:
  void initialManagerIsEmptyAndFileActionFree() {
    XFoilWidget widget;
    QCOMPARE(polarList(widget)->count(), 0);
    QCOMPARE(widget.findChild<QLabel*>("polarManagerStatusLabel")->text(),
             QStringLiteral("0 / 8 comparisons"));
    QVERIFY(!widget.selectedComparisonPolar().has_value());
    for (const char* name :
         {"clAlphaPlot", "clCdPlot", "cmAlphaPlot", "cmClPlot",
          "efficiencyClPlot"}) {
      QCOMPARE(scalar(widget, name)->comparisonCurveCount(), std::size_t{0});
    }
  }

  void importedComparisonIsOwnedAndDrawnInAllFivePlots() {
    XFoilWidget widget;
    QSignalSpy changed(&widget, &XFoilWidget::comparisonPolarsChanged);
    const auto id = widget.addComparisonPolar(polar("Imported", 0.4));
    QVERIFY(id.isOk());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(polarList(widget)->count(), 1);
    QCOMPARE(polarList(widget)->item(0)->text(), QStringLiteral("Imported"));
    QCOMPARE(polarList(widget)->item(0)
                 ->data(xfoil::gui::kComparisonStyleIndexRole)
                 .toInt(),
             0);
    QVERIFY(dynamic_cast<QStyledItemDelegate*>(
                polarList(widget)->itemDelegate()) != nullptr);
    QCOMPARE(widget.findChild<QLabel*>("polarManagerStatusLabel")->text(),
             QStringLiteral("1 / 8 comparisons"));
    QVERIFY(widget.selectedComparisonPolar().has_value());
    QCOMPARE(widget.selectedComparisonPolar()->metadata.name,
             std::string{"Imported"});
    for (const char* name :
         {"clAlphaPlot", "clCdPlot", "cmAlphaPlot", "cmClPlot",
          "efficiencyClPlot"}) {
      QCOMPARE(scalar(widget, name)->comparisonCurveCount(), std::size_t{1});
      QCOMPARE(scalar(widget, name)->comparisonPointCount(), std::size_t{3});
    }
  }

  void renameVisibilityAndRemoveAreInstanceLocal() {
    XFoilWidget widget;
    QVERIFY(widget.addComparisonPolar(polar("Draft")).isOk());
    auto* list = polarList(widget);
    list->setCurrentRow(0);
    auto* nameEdit = widget.findChild<QLineEdit*>("polarNameEdit");
    QCOMPARE(nameEdit->text(), QStringLiteral("Draft"));
    nameEdit->setText(QStringLiteral("Renamed"));
    widget.findChild<QPushButton*>("renamePolarButton")->click();
    QCOMPARE(list->item(0)->text(), QStringLiteral("Renamed"));
    QCOMPARE(widget.selectedComparisonPolar()->metadata.name,
             std::string{"Renamed"});

    list->item(0)->setCheckState(Qt::Unchecked);
    QCOMPARE(scalar(widget, "clAlphaPlot")->comparisonCurveCount(),
             std::size_t{0});
    list->item(0)->setCheckState(Qt::Checked);
    QCOMPARE(scalar(widget, "clAlphaPlot")->comparisonCurveCount(),
             std::size_t{1});

    widget.findChild<QPushButton*>("removePolarButton")->click();
    QCOMPARE(list->count(), 0);
    QVERIFY(!widget.selectedComparisonPolar().has_value());
    QCOMPARE(widget.findChild<QLabel*>("polarManagerStatusLabel")->text(),
             QStringLiteral("0 / 8 comparisons"));
  }

  void ninthComparisonIsRejectedAndResetClearsManager() {
    XFoilWidget widget;
    for (std::size_t index = 0; index < 8; ++index) {
      QVERIFY(widget.addComparisonPolar(
                  polar("P" + std::to_string(index)))
                  .isOk());
    }
    const auto ninth = widget.addComparisonPolar(polar("Ninth"));
    QVERIFY(ninth.isErr());
    QCOMPARE(ninth.error().code, std::string{"gui.polarLimitReached"});
    QCOMPARE(polarList(widget)->count(), 8);
    QCOMPARE(scalar(widget, "clAlphaPlot")->comparisonCurveCount(),
             std::size_t{8});

    QVERIFY(widget.resetWorkflow().isOk());
    QCOMPARE(polarList(widget)->count(), 0);
    QCOMPARE(scalar(widget, "clAlphaPlot")->comparisonCurveCount(),
             std::size_t{0});
    QCOMPARE(widget.findChild<QLabel*>("polarManagerStatusLabel")->text(),
             QStringLiteral("0 / 8 comparisons"));
  }

  void currentResultIsNotStoredUntilExplicitAdd() {
    XFoilWidget widget;
    configurePoint(widget);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.currentPolar().has_value());
    QVERIFY(widget.currentPolar()->points.size() > 0);
    QCOMPARE(polarList(widget)->count(), 0);
    QCOMPARE(widget.findChild<QLabel*>("polarManagerStatusLabel")->text(),
             QStringLiteral("0 / 8 comparisons"));

    const auto added = widget.addCurrentComparisonPolar();
    QVERIFY(added.isOk());
    QCOMPARE(polarList(widget)->count(), 1);
    QCOMPARE(widget.selectedComparisonPolar()->points.size(),
             widget.currentPolar()->points.size());
    QCOMPARE(scalar(widget, "clAlphaPlot")->comparisonCurveCount(),
             std::size_t{1});
  }
};

int runXFoilWidgetG6cTests(int argc, char** argv) {
  XFoilWidgetG6cTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetG6cTests.moc"

// End: tests/gui/XFoilWidgetG6cTests.cpp

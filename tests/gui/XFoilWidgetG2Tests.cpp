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

// File: tests/gui/XFoilWidgetG2Tests.cpp
// G2 host setter, UI binding, validation, invalidation and isolation tests.

#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QFontMetrics>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>
#include <QtTest/QTest>

#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::Degrees;
using xfoil::LiftTarget;
using xfoil::PanelingOptions;
using xfoil::gui::XFoilGuiState;
using xfoil::gui::XFoilWidget;
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

template <class T>
T* child(XFoilWidget& widget, const char* name) {
  T* value = widget.findChild<T*>(QString::fromLatin1(name));
  Q_ASSERT(value != nullptr);
  return value;
}

QStringList directoryEntries(const QString& path) {
  return QDir(path).entryList(
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
      QDir::Name);
}

}  // namespace

class XFoilWidgetG2Tests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void startsEmptyWithPublicDefaults() {
    XFoilWidget widget;
    const AnalysisConfig configuration;
    const PanelingOptions paneling;

    QVERIFY(widget.state() == XFoilGuiState::empty);
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(!widget.geometry().has_value());
    QVERIFY(!widget.panelGeometry().has_value());
    QCOMPARE(widget.panelingOptions().panelCount, paneling.panelCount);
    QCOMPARE(configuration.reynoldsNumber, 100000.0);
    QCOMPARE(widget.configuration().reynoldsNumber, 500000.0);
    QCOMPARE(widget.configuration().machNumber, configuration.machNumber);
    QCOMPARE(widget.configuration().viscous, configuration.viscous);
    QCOMPARE(widget.configuration().maximumIterations,
             configuration.maximumIterations);
    QVERIFY(std::holds_alternative<xfoil::AlphaTarget>(widget.target()));

    QVERIFY(child<QRadioButton>(widget, "geometryImportRadio") != nullptr);
    QVERIFY(child<QRadioButton>(widget, "geometryNacaRadio") != nullptr);
    QVERIFY(child<QRadioButton>(widget, "geometryNacsRadio") != nullptr);
    QVERIFY(child<QRadioButton>(widget, "geometryImportRadio")->isChecked());
    QVERIFY(child<QLineEdit>(widget, "nacaDesignationEdit") != nullptr);
    QVERIFY(child<QPushButton>(widget, "applyGeometryButton") != nullptr);
    QVERIFY(child<QSpinBox>(widget, "panelCountSpin") != nullptr);
    QVERIFY(child<QPushButton>(widget, "generatePanelButton") != nullptr);
    QVERIFY(child<QDoubleSpinBox>(widget, "reynoldsSpin") != nullptr);
    auto* targetTabs = child<QTabWidget>(widget, "analysisTargetTabs");
    QCOMPARE(targetTabs->count(), 4);
    QCOMPARE(targetTabs->currentIndex(), 0);
    QCOMPARE(widget.findChild<QGroupBox*>("panelingGroup")->title(),
             QStringLiteral("Paneling: no panel"));
    QCOMPARE(widget.findChild<QLabel*>("airfoilNameDisplayLabel")->text(),
             QStringLiteral("No geometry"));
    QVERIFY(widget.findChild<QLabel*>("definitionsPlaceholder") == nullptr);
    QVERIFY(widget.findChild<QLabel*>("geometryPlaceholder") == nullptr);
    QCOMPARE(widget.findChild<QLabel*>("cdValue")->text(),
             QStringLiteral("n/a"));
    QCOMPARE(widget.findChild<QLabel*>("efficiencyMaxValue")->text(),
             QStringLiteral("n/a"));
    auto* diagnostics =
        widget.findChild<QPlainTextEdit*>("setupDiagnosticsView");
    QVERIFY(diagnostics != nullptr);
    QVERIFY(diagnostics->isReadOnly());
    QVERIFY(diagnostics->toPlainText().isEmpty());
    auto* analyze = widget.findChild<QPushButton*>("analyzeButton");
    QVERIFY(analyze != nullptr);
    QVERIFY(!analyze->isEnabled());
    QCOMPARE(widget.findChild<QLabel*>("currentPointConvergenceLabel")->text(),
             QStringLiteral("No point selected"));
  }

  void programmaticSettersUpdateControlsAndPaths() {
    XFoilWidget widget;
    QSignalSpy geometrySpy(&widget, &XFoilWidget::geometryChanged);
    QSignalSpy panelSpy(&widget, &XFoilWidget::panelingChanged);
    QSignalSpy configurationSpy(&widget,
                                &XFoilWidget::configurationChanged);
    QSignalSpy targetSpy(&widget, &XFoilWidget::targetChanged);
    QSignalSpy stateSpy(&widget, &XFoilWidget::stateChanged);
    QSignalSpy validationSpy(&widget, &XFoilWidget::validationChanged);

    QVERIFY(widget.setNaca("2412").isOk());
    QCOMPARE(geometrySpy.count(), 1);
    QCOMPARE(panelSpy.count(), 1);
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.geometry().has_value());
    QVERIFY(widget.panelGeometry().has_value());
    QVERIFY(child<QRadioButton>(widget, "geometryNacaRadio")->isChecked());
    QCOMPARE(child<QLineEdit>(widget, "nacaDesignationEdit")->text(),
             QStringLiteral("2412"));
    QVERIFY(widget.findChild<QLabel*>("airfoilNameDisplayLabel")
                ->text()
                .contains(QStringLiteral("2412")));
    QVERIFY(widget.findChild<QGroupBox*>("panelingGroup")
                ->title()
                .contains(QStringLiteral("panel points")));

    QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
    QCOMPARE(panelSpy.count(), 2);
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.panelGeometry().has_value());
    QVERIFY(widget.isReadyToAnalyze());
    QVERIFY(validationSpy.count() >= 1);
    QVERIFY(validationSpy.last().at(0).toBool());

    AnalysisConfig configuration;
    configuration.reynoldsNumber = 2.0e6;
    configuration.machNumber = 0.1;
    configuration.viscous = false;
    configuration.maximumIterations = 80;
    configuration.transition.nCritical = 8.0;
    configuration.transition.topTripX = 0.8;
    configuration.transition.bottomTripX = 0.9;
    QVERIFY(widget.setConfiguration(configuration).isOk());
    QCOMPARE(configurationSpy.count(), 1);
    QCOMPARE(child<QDoubleSpinBox>(widget, "reynoldsSpin")->value(),
             2.0e6);
    QCOMPARE(child<QDoubleSpinBox>(widget, "machSpin")->value(), 0.1);
    QCOMPARE(child<QCheckBox>(widget, "viscousCheck")->isChecked(), false);
    QCOMPARE(child<QSpinBox>(widget, "iterationsSpin")->value(), 80);
    QCOMPARE(child<QDoubleSpinBox>(widget, "ncritSpin")->value(), 8.0);
    QCOMPARE(child<QDoubleSpinBox>(widget, "xtrUpperSpin")->value(), 0.8);
    QCOMPARE(child<QDoubleSpinBox>(widget, "xtrLowerSpin")->value(), 0.9);

    widget.setTarget(LiftTarget{0.55});
    QCOMPARE(targetSpy.count(), 1);
    QCOMPARE(child<QTabWidget>(widget, "analysisTargetTabs")->currentIndex(),
             2);
    QCOMPARE(child<QDoubleSpinBox>(widget, "clTargetSpin")->value(), 0.55);
    QVERIFY(std::holds_alternative<LiftTarget>(widget.target()));

    AlphaSequence sequence;
    sequence.start = Degrees{-1.0};
    sequence.endExclusive = Degrees{3.0};
    sequence.step = Degrees{0.5};
    widget.setSequence(sequence);
    QCOMPARE(targetSpy.count(), 2);
    QCOMPARE(child<QTabWidget>(widget, "analysisTargetTabs")->currentIndex(),
             1);
    QCOMPARE(widget.sequence().endExclusive.value, 3.0);

    QVERIFY(stateSpy.count() >= 2);
  }

  void uiControlsUseTheSameControllerPath() {
    XFoilWidget widget;

    auto* source = child<QRadioButton>(widget, "geometryNacaRadio");
    auto* designation = child<QLineEdit>(widget, "nacaDesignationEdit");
    auto* apply = child<QPushButton>(widget, "applyGeometryButton");
    source->setChecked(true);
    designation->setText(QStringLiteral("0012"));
    apply->click();
    QVERIFY(widget.geometry().has_value());
    QVERIFY(widget.panelGeometry().has_value());
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.isReadyToAnalyze());

    auto* panelCount = child<QSpinBox>(widget, "panelCountSpin");
    panelCount->setValue(180);
    QVERIFY(!widget.panelGeometry().has_value());
    QVERIFY(!widget.isReadyToAnalyze());
    child<QPushButton>(widget, "generatePanelButton")->click();
    QVERIFY(widget.panelGeometry().has_value());
    QCOMPARE(widget.panelingOptions().panelCount, std::size_t{180});
    QVERIFY(widget.isReadyToAnalyze());

    child<QDoubleSpinBox>(widget, "reynoldsSpin")->setValue(3.0e6);
    QCOMPARE(widget.configuration().reynoldsNumber, 3.0e6);
    child<QCheckBox>(widget, "viscousCheck")->setChecked(false);
    QCOMPARE(widget.configuration().viscous, false);

    auto* mode = child<QTabWidget>(widget, "analysisTargetTabs");
    mode->setCurrentIndex(2);
    child<QDoubleSpinBox>(widget, "clTargetSpin")->setValue(0.4);
    QVERIFY(std::holds_alternative<LiftTarget>(widget.target()));
    QCOMPARE(std::get<LiftTarget>(widget.target()).cl, 0.4);

    mode->setCurrentIndex(1);
    child<QDoubleSpinBox>(widget, "sequenceStepSpin")->setValue(0.0);
    QVERIFY(widget.state() == XFoilGuiState::error);
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(child<QPlainTextEdit>(widget, "setupDiagnosticsView")
                ->toPlainText()
                .contains(QStringLiteral("sequence.zeroStep")));

    child<QDoubleSpinBox>(widget, "sequenceStepSpin")->setValue(1.0);
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.isReadyToAnalyze());
  }

  void geometryAndPanelInvalidationIsAtomic() {
    XFoilWidget widget;
    QVERIFY(widget.setNaca("0012").isOk());
    QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
    const std::optional<AirfoilGeometry> firstGeometry = widget.geometry();
    QVERIFY(firstGeometry.has_value());
    QVERIFY(widget.isReadyToAnalyze());

    QVERIFY(widget.setNaca("2412").isOk());
    QVERIFY(widget.panelGeometry().has_value());
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.isReadyToAnalyze());

    QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
    QVERIFY(widget.isReadyToAnalyze());

    PanelingOptions changed;
    changed.panelCount = 200;
    widget.setPanelingOptions(changed);
    QVERIFY(!widget.panelGeometry().has_value());
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(widget.state() == XFoilGuiState::geometry);

    const auto failed = widget.setNaca("not-a-naca");
    QVERIFY(failed.isErr());
    QVERIFY(widget.state() == XFoilGuiState::error);
    QVERIFY(widget.geometry().has_value());
    QVERIFY(!widget.geometry()->name().empty());
    QVERIFY(QString::fromStdString(
                std::string(widget.geometry()->name()))
                .startsWith(QStringLiteral("NACA")));
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(child<QPlainTextEdit>(widget, "setupDiagnosticsView")
                ->toPlainText()
                .contains(QString::fromStdString(failed.error().code)));

    QVERIFY(widget.setNaca("2412").isOk());
    QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
    QVERIFY(widget.isReadyToAnalyze());
  }

  void memoryGeometryUsesHostSetter() {
    XFoilWidget widget;
    QVERIFY(widget.setGeometry(diamond()).isOk());
    QVERIFY(widget.geometry().has_value());
    QVERIFY(widget.state() == XFoilGuiState::geometry);
    QVERIFY(child<QRadioButton>(widget, "geometryImportRadio")->isChecked());
    QCOMPARE(child<QLabel>(widget, "airfoilNameDisplayLabel")->text(),
             QStringLiteral("diamond"));
    QVERIFY(!child<QPushButton>(widget, "applyGeometryButton")->isEnabled());
  }

  void nacsAutopanelsWithoutImportNotice() {
    XFoilWidget widget;
    QVERIFY(widget.setNacs("0012").isOk());
    QVERIFY(widget.geometry().has_value());
    QVERIFY(widget.panelGeometry().has_value());
    QVERIFY(widget.isReadyToAnalyze());
    QVERIFY(widget.findChild<QRadioButton*>("geometryNacsRadio")->isChecked());
    QVERIFY(widget.findChild<QLabel*>("geometryImportNoticeLabel")->isHidden());
  }

  void importNoticeIsViewStateAndClears() {
    XFoilWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    std::vector<xfoil::Point2D> points = {
        {1.0, 0.0}, {0.5, -0.08}, {0.0, 0.0},
        {0.5, 0.08}, {1.0, 0.0}};
    AirfoilGeometry clockwise;
    QVERIFY(clockwise.assign("clockwise", points).success());
    QVERIFY(widget.applyImportedGeometry(clockwise, true).isOk());
    auto* notice =
        widget.findChild<QLabel*>("geometryImportNoticeLabel");
    QVERIFY(notice != nullptr);
    QVERIFY(!notice->isHidden());
    QCOMPARE(notice->text(), QStringLiteral(
        "Clockwise point order detected and reversed to "
        "counter-clockwise."));
    QVERIFY(child<QRadioButton>(widget, "geometryImportRadio")->isChecked());
    auto* scroll = child<QScrollArea>(widget, "definitionsScrollArea");
    QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);

    points = {{1.0, 0.0}, {0.5, 0.08}, {0.0, 0.0},
              {0.5, -0.08}, {1.0, 0.0}};
    AirfoilGeometry counterClockwise;
    QVERIFY(counterClockwise.assign("ccw", points).success());
    QVERIFY(widget.applyImportedGeometry(counterClockwise, false).isOk());
    QVERIFY(notice->isHidden());
    QVERIFY(notice->text().isEmpty());

    QVERIFY(widget.applyImportedGeometry(clockwise, true).isOk());
    QVERIFY(!notice->isHidden());
    QVERIFY(widget.setNaca("0012").isOk());
    QVERIFY(notice->isHidden());
  }

  void geometrySourceRadiosRequestImportAndRestoreOnCancel() {
    XFoilWidget widget;
    widget.resize(800, 600);
    widget.show();
    QCoreApplication::processEvents();
    QVERIFY(widget.setNaca("0012").isOk());
    QVERIFY(child<QRadioButton>(widget, "geometryNacaRadio")->isChecked());

    QSignalSpy requested(&widget, &XFoilWidget::airfoilImportRequested);
    QTest::mouseClick(child<QRadioButton>(widget, "geometryImportRadio"),
                      Qt::LeftButton);
    QCOMPARE(requested.count(), 1);
    QVERIFY(child<QRadioButton>(widget, "geometryImportRadio")->isChecked());
    QVERIFY(!child<QRadioButton>(widget, "geometryNacaRadio")->isChecked());
    QVERIFY(!child<QLineEdit>(widget, "nacaDesignationEdit")->isEnabled());
    QVERIFY(!child<QPushButton>(widget, "applyGeometryButton")->isEnabled());

    widget.cancelAirfoilImportRequest();
    QVERIFY(child<QRadioButton>(widget, "geometryNacaRadio")->isChecked());
    QVERIFY(child<QLineEdit>(widget, "nacaDesignationEdit")->isEnabled());

    requested.clear();
    QVERIFY(widget.setGeometry(diamond()).isOk());
    QVERIFY(child<QRadioButton>(widget, "geometryImportRadio")->isChecked());
    QCOMPARE(requested.count(), 0);
  }

  void geometryHeaderAndTwoColumnSourceBody() {
    XFoilWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();

    auto* title = child<QLabel>(widget, "geometryTitleLabel");
    auto* geometryGroup = child<QGroupBox>(widget, "geometryGroup");
    QVERIFY(title != nullptr);
    QCOMPARE(title->text(), QStringLiteral("Geometry:"));
    auto* header = child<QHBoxLayout>(widget, "geometryHeaderLayout");
    auto* nameStack = child<QStackedWidget>(widget, "airfoilNameStack");
    QVERIFY(header && nameStack);
    QVERIFY(header->indexOf(title) >= 0);
    QVERIFY(header->indexOf(nameStack) > header->indexOf(title));
    QVERIFY(geometryGroup->title().isEmpty());
    QVERIFY(widget.findChild<QLabel*>("geometryHeaderLabel") == nullptr);
    const int untitledHint = geometryGroup->minimumSizeHint().height();
    const int layoutHint = geometryGroup->layout()->minimumSize().height();
    QVERIFY2(untitledHint - layoutHint <= 18,
             qPrintable(QStringLiteral("group hint=%1 layout hint=%2")
                            .arg(untitledHint).arg(layoutHint)));

    auto* load = child<QPushButton>(widget, "loadAirfoilButton");
    auto* block = child<QWidget>(widget, "nacaGeometryBlock");
    auto* designation = child<QLineEdit>(widget, "nacaDesignationEdit");
    auto* create = child<QPushButton>(widget, "applyGeometryButton");
    QVERIFY(load && block && designation && create);
    QCOMPARE(load->text(), QStringLiteral("Load Airfoil"));
    QVERIFY(load->geometry().right() <= block->geometry().left());
    QVERIFY(load->isEnabled());
    QVERIFY(!designation->isEnabled());
    QVERIFY(!create->isEnabled());
    QVERIFY(designation->maxLength() >= 5);
    const QFontMetrics metrics(designation->font());
    QVERIFY(designation->width() >=
            metrics.horizontalAdvance(QStringLiteral("00000")));
    QVERIFY(designation->width() < 150);

    QSignalSpy requested(&widget, &XFoilWidget::airfoilImportRequested);
    QTest::mouseClick(load, Qt::LeftButton);
    QCOMPARE(requested.count(), 1);

    child<QRadioButton>(widget, "geometryNacaRadio")->click();
    QVERIFY(!load->isEnabled());
    QVERIFY(designation->isEnabled());
    QVERIFY(create->isEnabled());
    child<QRadioButton>(widget, "geometryImportRadio")->click();
    QVERIFY(load->isEnabled());
    QVERIFY(!designation->isEnabled());
    QVERIFY(!create->isEnabled());

    QVERIFY(widget.setNaca("0012").isOk());
    int nameLabels = 0;
    for (QLabel* label : widget.findChildren<QLabel*>()) {
      if (label->text().contains(QStringLiteral("NACA 0012"))) {
        ++nameLabels;
      }
    }
    QCOMPARE(nameLabels, 1);
  }

  void geometryNameEditingCommitsAndCancels() {
    XFoilWidget widget;
    widget.resize(800, 600);
    widget.show();
    QCoreApplication::processEvents();
    QVERIFY(widget.setNaca("0012").isOk());
    auto* display = child<QLabel>(widget, "airfoilNameDisplayLabel");
    auto* edit = child<QLineEdit>(widget, "airfoilNameEdit");
    auto* stack = child<QStackedWidget>(widget, "airfoilNameStack");
    QCOMPARE(stack->currentWidget(), display);

    QTest::mouseClick(display, Qt::LeftButton);
    QCOMPARE(stack->currentWidget(), edit);
    edit->setText(QStringLiteral("My Wing"));
    QTest::keyClick(edit, Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(QString::fromStdString(std::string(widget.geometry()->name())),
             QStringLiteral("My Wing"));
    QCOMPARE(display->text(), QStringLiteral("My Wing"));
    QCOMPARE(stack->currentWidget(), display);
    QVERIFY(!widget.isReadyToAnalyze() ||
            widget.panelGeometry().has_value());

    QTest::mouseClick(display, Qt::LeftButton);
    edit->setText(QStringLiteral("Discarded"));
    QTest::keyClick(edit, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(QString::fromStdString(std::string(widget.geometry()->name())),
             QStringLiteral("My Wing"));
    QCOMPARE(display->text(), QStringLiteral("My Wing"));
    QCOMPARE(stack->currentWidget(), display);

    QTest::mouseClick(display, Qt::LeftButton);
    edit->setText(QStringLiteral("Focus Commit"));
    child<QLineEdit>(widget, "nacaDesignationEdit")->setFocus();
    QCoreApplication::processEvents();
    QCOMPARE(QString::fromStdString(std::string(widget.geometry()->name())),
             QStringLiteral("Focus Commit"));
    QCOMPARE(display->text(), QStringLiteral("Focus Commit"));
    QCOMPARE(stack->currentWidget(), display);

    AirfoilGeometry renamed = *widget.geometry();
    renamed.setName("Programmatic");
    QVERIFY(widget.applyImportedGeometry(renamed, false).isOk());
    QCOMPARE(display->text(), QStringLiteral("Programmatic"));
  }

  void twoWidgetsRemainIndependent() {
    XFoilWidget first;
    XFoilWidget second;

    QVERIFY(first.setNaca("2412").isOk());
    QVERIFY(first.generatePanel(PanelingOptions{}).isOk());
    QVERIFY(first.isReadyToAnalyze());

    QVERIFY(!second.geometry().has_value());
    QVERIFY(!second.panelGeometry().has_value());
    QVERIFY(!second.isReadyToAnalyze());
    QVERIFY(second.state() == XFoilGuiState::empty);
  }


  void stateChangesDoNotRebuildTheWidgetHierarchy() {
    XFoilWidget widget;
    QStringList before;
    for (QObject* child : widget.findChildren<QObject*>()) {
      before.push_back(child->objectName());
    }

    QVERIFY(widget.setNaca("2412").isOk());
    QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
    widget.setTarget(LiftTarget{0.5});
    widget.setPanelingOptions(PanelingOptions{});
    QVERIFY(widget.state() == XFoilGuiState::geometry);

    QStringList after;
    for (QObject* child : widget.findChildren<QObject*>()) {
      after.push_back(child->objectName());
    }
    std::sort(before.begin(), before.end());
    std::sort(after.begin(), after.end());
    QCOMPARE(after, before);
  }

  void operationsTouchNoWorkingDirectoryFiles() {
    const QString cwd = QDir::currentPath();
    const QStringList before = directoryEntries(cwd);
    {
      XFoilWidget widget;
      QVERIFY(widget.setNaca("2412").isOk());
      QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
      QVERIFY(widget.setConfiguration(AnalysisConfig{}).isOk());
      widget.setTarget(LiftTarget{0.3});
    }
    QCOMPARE(QDir::currentPath(), cwd);
    QCOMPARE(directoryEntries(cwd), before);
  }
};

int runXFoilWidgetG2Tests(int argc, char** argv) {
  XFoilWidgetG2Tests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetG2Tests.moc"

// End: tests/gui/XFoilWidgetG2Tests.cpp

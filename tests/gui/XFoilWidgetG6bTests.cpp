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

// File: tests/gui/XFoilWidgetG6bTests.cpp
// G6b Basic/Expert controls, owning host setup transaction, repeated
// reinitialization, reset, busy behavior, CL sequence and panel overlay.

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QtTest/QTest>

#include "plots/GeometryPlotView.h"
#include "qcustomplot.h"
#include "xfoil/gui/AnalysisWorkflowSetup.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AlphaTarget;
using xfoil::AnalysisConfig;
using xfoil::Degrees;
using xfoil::LiftSequence;
using xfoil::LiftTarget;
using xfoil::MachDependency;
using xfoil::PanelingOptions;
using xfoil::ReynoldsDependency;
using xfoil::gui::AnalysisWorkflowRequest;
using xfoil::gui::AnalysisWorkflowSetup;
using xfoil::gui::AnalysisWorkflowViewOptions;
using xfoil::gui::MemoryGeometrySetup;
using xfoil::gui::NacaGeometrySetup;
using xfoil::gui::NacsGeometrySetup;
using xfoil::gui::XFoilGuiState;
using xfoil::gui::XFoilWidget;

template <class T>
T* child(XFoilWidget& widget, const char* name) {
  T* value = widget.findChild<T*>(QString::fromLatin1(name));
  Q_ASSERT(value != nullptr);
  return value;
}

QDoubleSpinBox* spin(XFoilWidget& widget, const char* name) {
  return child<QDoubleSpinBox>(widget, name);
}

QSpinBox* integerSpin(XFoilWidget& widget, const char* name) {
  return child<QSpinBox>(widget, name);
}

QComboBox* combo(XFoilWidget& widget, const char* name) {
  return child<QComboBox>(widget, name);
}

QCheckBox* check(XFoilWidget& widget, const char* name) {
  return child<QCheckBox>(widget, name);
}

PanelingOptions fullPaneling() {
  PanelingOptions options;
  options.panelCount = 180;
  options.bunchingParameter = 1.25;
  options.teToLeDensityRatio = 0.35;
  options.refinementDensityRatio = 0.22;
  options.upperRefinementStart = 0.15;
  options.upperRefinementEnd = 0.65;
  options.lowerRefinementStart = 0.2;
  options.lowerRefinementEnd = 0.7;
  return options;
}

AnalysisConfig fullConfiguration() {
  AnalysisConfig configuration;
  configuration.reynoldsNumber = 2.5e6;
  configuration.machNumber = 0.12;
  configuration.viscous = false;
  configuration.reynoldsDependency =
      ReynoldsDependency::inverseSquareRootLift;
  configuration.machDependency = MachDependency::inverseLift;
  configuration.transition.nCritical = 7.5;
  configuration.transition.topTripX = 0.7;
  configuration.transition.bottomTripX = 0.8;
  configuration.maximumIterations = 70;
  configuration.autoInitializeBoundaryLayer = false;
  configuration.viscousAcceleration = 0.02;
  return configuration;
}

AnalysisWorkflowSetup nacaSetup(std::string designation = "2412",
                                 AnalysisWorkflowRequest request =
                                     LiftTarget{0.5}) {
  AnalysisWorkflowSetup setup;
  setup.geometry = NacaGeometrySetup{std::move(designation)};
  setup.paneling = fullPaneling();
  setup.configuration = fullConfiguration();
  setup.request = std::move(request);
  return setup;
}

AnalysisWorkflowSetup longSequenceSetup() {
  auto setup = nacaSetup("0012",
                         AlphaSequence{Degrees{0.0}, Degrees{40.0},
                                        Degrees{0.05}});
  setup.paneling.panelCount = 80;
  setup.configuration.viscous = false;
  setup.configuration.maximumIterations = 50;
  return setup;
}

void configureLongRun(XFoilWidget& widget) {
  QVERIFY(widget.applySetup(longSequenceSetup()).isOk());
  QVERIFY(widget.isReadyToAnalyze());
}

}  // namespace

class XFoilWidgetG6bTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void basicExpertStructureAndPublicDefaults() {
    XFoilWidget widget;
    auto* panelingTabs =
        child<QTabWidget>(widget, "panelingSettingsTabs");
    auto* conditionTabs =
        child<QTabWidget>(widget, "conditionSettingsTabs");
    QCOMPARE(panelingTabs->count(), 2);
    QCOMPARE(conditionTabs->count(), 2);
    QVERIFY(widget.findChild<QWidget*>("panelingBasicPage") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("panelingExpertPage") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("conditionBasicPage") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("conditionExpertPage") != nullptr);

    const PanelingOptions paneling;
    QCOMPARE(integerSpin(widget, "panelCountSpin")->value(),
             static_cast<int>(paneling.panelCount));
    QCOMPARE(spin(widget, "bunchingSpin")->value(),
             paneling.bunchingParameter);
    QCOMPARE(spin(widget, "teToLeSpin")->value(),
             paneling.teToLeDensityRatio);
    QCOMPARE(spin(widget, "refinementSpin")->value(),
             paneling.refinementDensityRatio);
    QCOMPARE(spin(widget, "upperRefinementStartSpin")->value(),
             paneling.upperRefinementStart);
    QCOMPARE(spin(widget, "upperRefinementEndSpin")->value(),
             paneling.upperRefinementEnd);
    QCOMPARE(spin(widget, "lowerRefinementStartSpin")->value(),
             paneling.lowerRefinementStart);
    QCOMPARE(spin(widget, "lowerRefinementEndSpin")->value(),
             paneling.lowerRefinementEnd);

    const AnalysisConfig configuration;
    QCOMPARE(spin(widget, "reynoldsSpin")->value(),
             configuration.reynoldsNumber);
    QCOMPARE(spin(widget, "machSpin")->value(),
             configuration.machNumber);
    QCOMPARE(check(widget, "viscousCheck")->isChecked(),
             configuration.viscous);
    QCOMPARE(integerSpin(widget, "iterationsSpin")->value(),
             configuration.maximumIterations);
    QCOMPARE(spin(widget, "ncritSpin")->value(),
             configuration.transition.nCritical);
    QCOMPARE(spin(widget, "xtrUpperSpin")->value(),
             configuration.transition.topTripX);
    QCOMPARE(spin(widget, "xtrLowerSpin")->value(),
             configuration.transition.bottomTripX);
    QCOMPARE(combo(widget, "reynoldsDependencyCombo")->currentData().toInt(),
             static_cast<int>(configuration.reynoldsDependency));
    QCOMPARE(combo(widget, "machDependencyCombo")->currentData().toInt(),
             static_cast<int>(configuration.machDependency));
    QCOMPARE(check(widget, "autoInitializeBoundaryLayerCheck")->isChecked(),
             configuration.autoInitializeBoundaryLayer);
    QCOMPARE(spin(widget, "viscousAccelerationSpin")->value(),
             configuration.viscousAcceleration);
    QVERIFY(check(widget, "showPanelingCheckBox")->isChecked() == false);
    QVERIFY(check(widget, "showPanelNodesCheck")->isChecked());
  }

  void applySetupBeforeShowRoundTripsAllFields() {
    XFoilWidget widget;
    QVERIFY(!widget.isVisible());
    const AnalysisWorkflowSetup setup =
        nacaSetup("2412", LiftSequence{0.2, 0.8, 0.2});
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    QVERIFY(widget.applySetup(setup).isOk());
    QCOMPARE(started.count(), 0);
    QVERIFY(widget.isReadyToAnalyze());
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.geometry().has_value());
    QVERIFY(widget.panelGeometry().has_value());
    QCOMPARE(widget.panelingOptions().panelCount,
             setup.paneling.panelCount);
    QCOMPARE(widget.panelingOptions().upperRefinementStart, 0.15);
    QCOMPARE(widget.configuration().reynoldsNumber, 2.5e6);
    QCOMPARE(widget.configuration().machDependency,
             MachDependency::inverseLift);
    QCOMPARE(widget.configuration().autoInitializeBoundaryLayer, false);
    QCOMPARE(widget.clSequence().endExclusive, 0.8);
    QCOMPARE(combo(widget, "targetModeCombo")->currentText(),
             QStringLiteral("CL sequence"));
    const auto current = widget.currentSetup();
    QVERIFY(current.has_value());
    QVERIFY(std::holds_alternative<NacaGeometrySetup>(current->geometry));
    QCOMPARE(std::get<NacaGeometrySetup>(current->geometry).designation,
             std::string("2412"));
    QVERIFY(std::holds_alternative<LiftSequence>(current->request));
    QCOMPARE(std::get<LiftSequence>(current->request).step, 0.2);
  }

  void uiControlsFeedTheSameModelPath() {
    XFoilWidget widget;
    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    integerSpin(widget, "panelCountSpin")->setValue(160);
    spin(widget, "upperRefinementStartSpin")->setValue(0.3);
    spin(widget, "lowerRefinementEndSpin")->setValue(0.9);
    QCOMPARE(widget.panelingOptions().panelCount, std::size_t{160});
    QCOMPARE(widget.panelingOptions().upperRefinementStart, 0.3);
    QCOMPARE(widget.panelingOptions().lowerRefinementEnd, 0.9);
    child<QPushButton>(widget, "generatePanelButton")->click();
    QVERIFY(widget.isReadyToAnalyze());

    spin(widget, "reynoldsSpin")->setValue(3.0e6);
    combo(widget, "reynoldsDependencyCombo")->setCurrentIndex(2);
    check(widget, "autoInitializeBoundaryLayerCheck")->setChecked(false);
    spin(widget, "viscousAccelerationSpin")->setValue(0.03);
    QCOMPARE(widget.configuration().reynoldsNumber, 3.0e6);
    QCOMPARE(widget.configuration().reynoldsDependency,
             ReynoldsDependency::inverseLift);
    QCOMPARE(widget.configuration().autoInitializeBoundaryLayer, false);
    QCOMPARE(widget.configuration().viscousAcceleration, 0.03);

    combo(widget, "targetModeCombo")->setCurrentIndex(3);
    QVERIFY(widget.findChild<QWidget*>("clSequenceFields")->isEnabled());
    QVERIFY(!widget.findChild<QWidget*>("sequenceFields")->isEnabled());
    spin(widget, "clSequenceStartSpin")->setValue(0.1);
    spin(widget, "clSequenceEndSpin")->setValue(0.7);
    spin(widget, "clSequenceStepSpin")->setValue(0.15);
    QCOMPARE(widget.clSequence().start, 0.1);
    QCOMPARE(widget.clSequence().endExclusive, 0.7);
    QCOMPARE(widget.clSequence().step, 0.15);
    QVERIFY(widget.isReadyToAnalyze());
  }

  void memoryAndNacsGeometrySetups() {
    XFoilWidget source;
    QVERIFY(source.setNaca("0012").isOk());
    QVERIFY(source.geometry().has_value());
    AnalysisWorkflowSetup memory = nacaSetup();
    memory.geometry = MemoryGeometrySetup{*source.geometry()};
    XFoilWidget first;
    QVERIFY(first.applySetup(memory).isOk());
    QCOMPARE(combo(first, "geometrySourceCombo")->currentText(),
             QStringLiteral("Memory"));
    QVERIFY(first.geometry().has_value());
    QVERIFY(first.currentSetup().has_value());
    QVERIFY(std::holds_alternative<MemoryGeometrySetup>(
        first.currentSetup()->geometry));

    AnalysisWorkflowSetup nacs = nacaSetup();
    nacs.geometry = NacsGeometrySetup{"0012"};
    XFoilWidget second;
    QVERIFY(second.applySetup(nacs).isOk());
    QCOMPARE(combo(second, "geometrySourceCombo")->currentText(),
             QStringLiteral("NACS"));
    QVERIFY(std::holds_alternative<NacsGeometrySetup>(
        second.currentSetup()->geometry));
  }

  void allFourRequestModesApplyWithoutStart() {
    XFoilWidget widget;
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    const std::vector<AnalysisWorkflowRequest> requests = {
        AlphaTarget{Degrees{2.0}}, LiftTarget{0.4},
        AlphaSequence{Degrees{0.0}, Degrees{3.0}, Degrees{1.0}},
        LiftSequence{0.2, 0.8, 0.2}};
    const QStringList expected = {
        QStringLiteral("Alpha point"), QStringLiteral("CL point"),
        QStringLiteral("Alpha sequence"), QStringLiteral("CL sequence")};
    for (std::size_t index = 0; index < requests.size(); ++index) {
      auto setup = nacaSetup();
      setup.request = requests[index];
      QVERIFY(widget.applySetup(setup).isOk());
      QCOMPARE(combo(widget, "targetModeCombo")->currentText(),
               expected[static_cast<int>(index)]);
      QVERIFY(widget.isReadyToAnalyze());
    }
    QCOMPARE(started.count(), 0);
  }

  void applyDoesNotAutostartAndClearsOldResults() {
    XFoilWidget widget;
    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    widget.analyze();
    QCOMPARE(started.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.currentOperatingPoint().has_value());

    auto replacement = nacaSetup("0012", AlphaTarget{Degrees{1.0}});
    QVERIFY(widget.applySetup(replacement).isOk());
    QCOMPARE(started.count(), 1);
    QVERIFY(!widget.currentOperatingPoint().has_value());
    QVERIFY(!widget.currentSequence().has_value());
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(widget.isReadyToAnalyze());
  }

  void repeatedReinitializationIsAtomicAndComplete() {
    XFoilWidget widget;
    for (int iteration = 0; iteration < 20; ++iteration) {
      auto setup = nacaSetup(iteration % 2 == 0 ? "0012" : "2412");
      setup.paneling.panelCount = 100 + static_cast<std::size_t>(iteration);
      setup.configuration.reynoldsNumber = 1.0e6 + iteration;
      QVERIFY(widget.applySetup(setup).isOk());
      QVERIFY(widget.currentSetup().has_value());
      QCOMPARE(widget.panelingOptions().panelCount, setup.paneling.panelCount);
      QCOMPARE(widget.configuration().reynoldsNumber,
               setup.configuration.reynoldsNumber);
      QVERIFY(widget.isReadyToAnalyze());
    }
  }

  void failedSetupPreservesCompleteOldState() {
    XFoilWidget widget;
    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    const auto oldSetup = widget.currentSetup();
    const auto oldGeometry = widget.geometry();
    const auto oldPanel = widget.panelGeometry();
    const bool oldReady = widget.isReadyToAnalyze();
    const XFoilGuiState oldState = widget.state();

    QSignalSpy geometry(&widget, &XFoilWidget::geometryChanged);
    QSignalSpy paneling(&widget, &XFoilWidget::panelingChanged);
    QSignalSpy configuration(&widget, &XFoilWidget::configurationChanged);
    QSignalSpy target(&widget, &XFoilWidget::targetChanged);
    QSignalSpy applied(&widget, &XFoilWidget::setupApplied);
    QSignalSpy failed(&widget, &XFoilWidget::setupFailed);

    auto invalid = nacaSetup("not-a-naca");
    const auto result = widget.applySetup(invalid);
    QVERIFY(result.isErr());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(geometry.count(), 0);
    QCOMPARE(paneling.count(), 0);
    QCOMPARE(configuration.count(), 0);
    QCOMPARE(target.count(), 0);
    QCOMPARE(applied.count(), 0);
    QCOMPARE(widget.isReadyToAnalyze(), oldReady);
    QCOMPARE(widget.state(), oldState);
    QVERIFY(widget.geometry().has_value());
    QCOMPARE(widget.geometry()->name(), oldGeometry->name());
    QVERIFY(widget.panelGeometry().has_value());
    QCOMPARE(widget.panelGeometry()->points.size(),
             oldPanel->points.size());
    QCOMPARE(widget.currentSetup()->configuration.reynoldsNumber,
             oldSetup->configuration.reynoldsNumber);
    QVERIFY(widget.findChild<QPlainTextEdit*>("setupDiagnosticsView")
                ->toPlainText()
                .contains(QString::fromStdString(result.error().code)));
  }

  void invalidRequestFailsAtomically() {
    XFoilWidget widget;
    auto setup = nacaSetup("0012", LiftSequence{0.2, 0.8, 0.0});
    const auto result = widget.applySetup(setup);
    QVERIFY(result.isErr());
    QCOMPARE(QString::fromStdString(result.error().code),
             QStringLiteral("sequence.zeroStep"));
    QVERIFY(!widget.geometry().has_value());
    QVERIFY(widget.state() == XFoilGuiState::empty);
  }

  void busyRejectsApplyAndResetButAllowsViewOptions() {
    XFoilWidget widget;
    configureLongRun(widget);
    widget.analyze();
    QVERIFY(widget.isRunning());

    const auto applied = widget.applySetup(nacaSetup());
    QVERIFY(applied.isErr());
    QCOMPARE(QString::fromStdString(applied.error().code),
             QStringLiteral("gui.busy"));
    const auto reset = widget.resetWorkflow();
    QVERIFY(reset.isErr());
    QCOMPARE(QString::fromStdString(reset.error().code),
             QStringLiteral("gui.busy"));

    AnalysisWorkflowViewOptions options;
    options.showPaneling = true;
    widget.setViewOptions(options);
    QVERIFY(widget.viewOptions().showPaneling);

    widget.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
  }

  void resetWorkflowReturnsToEmptyAndKeepsViewOptions() {
    XFoilWidget widget;
    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    AnalysisWorkflowViewOptions options;
    options.showPaneling = true;
    options.showPanelNodes = false;
    widget.setViewOptions(options);
    QVERIFY(widget.resetWorkflow().isOk());
    QVERIFY(widget.state() == XFoilGuiState::empty);
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(!widget.geometry().has_value());
    QVERIFY(!widget.panelGeometry().has_value());
    QVERIFY(!widget.currentSetup().has_value());
    QVERIFY(widget.viewOptions().showPaneling);
    QVERIFY(!widget.viewOptions().showPanelNodes);
    QCOMPARE(integerSpin(widget, "panelCountSpin")->value(),
             static_cast<int>(PanelingOptions{}.panelCount));
    QCOMPARE(spin(widget, "reynoldsSpin")->value(),
             AnalysisConfig{}.reynoldsNumber);
    QVERIFY(!widget.analyzeAction()->isEnabled());
  }

  void applyEmitsCommittedSequenceInOrder() {
    XFoilWidget widget;
    QStringList order;
    QObject::connect(&widget, &XFoilWidget::geometryChanged, &widget,
                     [&order] { order.push_back("geometryChanged"); });
    QObject::connect(&widget, &XFoilWidget::panelingChanged, &widget,
                     [&order] { order.push_back("panelingChanged"); });
    QObject::connect(&widget, &XFoilWidget::configurationChanged, &widget,
                     [&order] { order.push_back("configurationChanged"); });
    QObject::connect(&widget, &XFoilWidget::targetChanged, &widget,
                     [&order] { order.push_back("targetChanged"); });
    QObject::connect(&widget, &XFoilWidget::setupApplied, &widget,
                     [&order] { order.push_back("setupApplied"); });
    QObject::connect(&widget, &XFoilWidget::stateChanged, &widget,
                     [&order](XFoilGuiState) {
                       order.push_back("stateChanged");
                     });
    QObject::connect(&widget, &XFoilWidget::validationChanged, &widget,
                     [&order](bool) { order.push_back("validationChanged"); });

    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    QVERIFY(order.size() >= 7);
    QCOMPARE(order.at(0), QStringLiteral("geometryChanged"));
    QCOMPARE(order.at(1), QStringLiteral("panelingChanged"));
    QCOMPARE(order.at(2), QStringLiteral("configurationChanged"));
    QCOMPARE(order.at(3), QStringLiteral("targetChanged"));
    QCOMPARE(order.at(4), QStringLiteral("setupApplied"));
    QCOMPARE(order.at(5), QStringLiteral("stateChanged"));
    QCOMPARE(order.at(6), QStringLiteral("validationChanged"));
  }

  void twoWidgetsKeepSetupAndViewOptionsIndependent() {
    XFoilWidget first;
    XFoilWidget second;
    QVERIFY(first.applySetup(nacaSetup("0012")).isOk());
    QVERIFY(second.applySetup(nacaSetup("2412")).isOk());
    first.setViewOptions({true, false});
    QVERIFY(first.viewOptions().showPaneling);
    QVERIFY(!second.viewOptions().showPaneling);
    QVERIFY(second.viewOptions().showPanelNodes);
    QVERIFY(first.geometry()->name() != second.geometry()->name());
    QVERIFY(first.isReadyToAnalyze());
    QVERIFY(second.isReadyToAnalyze());
  }

  void clSequenceUsesPublicDetailedApiWithoutGuiLoop() {
    XFoilWidget widget;
    auto setup = nacaSetup("0012", LiftSequence{0.2, 0.6, 0.2});
    setup.configuration.viscous = false;
    setup.paneling.panelCount = 100;
    QVERIFY(widget.applySetup(setup).isOk());
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    widget.analyze();
    QCOMPARE(started.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.state() == XFoilGuiState::results);
    QVERIFY(widget.currentSequence().has_value());
    QCOMPARE(widget.currentSequence()->points.size(), std::size_t{2});
    QVERIFY(widget.currentSequence()->points.front().polar.alpha.value <
            widget.currentSequence()->points.back().polar.alpha.value);
  }

  void minimumSizeKeepsDefinitionsReadableWithoutClipping() {
    XFoilWidget widget;
    widget.resize(960, 640);
    widget.show();
    QCoreApplication::processEvents();
    auto* scroll = child<QScrollArea>(widget, "definitionsScrollArea");
    QVERIFY(scroll->horizontalScrollBar()->maximum() == 0);
    auto* row = child<QWidget>(widget, "definitionsRow");
    QVERIFY(row->height() > 0);
    QVERIFY(row->width() <= scroll->viewport()->width() + 1);
  }

  void clSequenceValidationReachesUiAndModel() {
    XFoilWidget widget;
    auto setup = nacaSetup("0012", LiftSequence{0.2, 0.8, 0.0});
    const auto failed = widget.applySetup(setup);
    QVERIFY(failed.isErr());
    QCOMPARE(QString::fromStdString(failed.error().code),
             QStringLiteral("sequence.zeroStep"));

    QVERIFY(widget.applySetup(nacaSetup("0012")).isOk());
    combo(widget, "targetModeCombo")->setCurrentIndex(3);
    spin(widget, "clSequenceStepSpin")->setValue(0.0);
    QVERIFY(widget.state() == XFoilGuiState::error);
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(widget.findChild<QPlainTextEdit*>("setupDiagnosticsView")
                ->toPlainText()
                .contains(QStringLiteral("sequence.zeroStep")));
  }

  void defaultOverlayIsOffAndToggleIsViewOnly() {
    XFoilWidget widget;
    QVERIFY(widget.applySetup(nacaSetup("0012")).isOk());
    QSignalSpy geometry(&widget, &XFoilWidget::geometryChanged);
    QSignalSpy paneling(&widget, &XFoilWidget::panelingChanged);
    QSignalSpy configuration(&widget, &XFoilWidget::configurationChanged);
    QSignalSpy target(&widget, &XFoilWidget::targetChanged);

    auto* view = static_cast<xfoil::gui::GeometryPlotView*>(
        child<QWidget>(widget, "airfoilPlot"));
    QVERIFY(!view->showPaneling());
    auto* plot = view->findChild<QCustomPlot*>();
    QVERIFY(plot != nullptr);
    auto* contour = plot->findChild<QCPCurve*>("panelContour");
    auto* ticks = plot->findChild<QCPCurve*>("panelTicks");
    auto* nodes = plot->findChild<QCPGraph*>("panelNodes");
    QVERIFY(contour != nullptr && ticks != nullptr && nodes != nullptr);
    QVERIFY(!contour->visible());
    QVERIFY(!ticks->visible());

    AnalysisWorkflowViewOptions options;
    options.showPaneling = true;
    widget.setViewOptions(options);
    QVERIFY(view->showPaneling());
    QVERIFY(contour->visible());
    QVERIFY(ticks->visible());
    QVERIFY(ticks->dataCount() > 0);
    QVERIFY(nodes->visible());
    QVERIFY(contour->pen().color().red() > 180);
    QVERIFY(contour->pen().color().green() < 80);
    QCOMPARE(geometry.count(), 0);
    QCOMPARE(paneling.count(), 0);
    QCOMPARE(configuration.count(), 0);
    QCOMPARE(target.count(), 0);
    QVERIFY(widget.isReadyToAnalyze());
  }

  void viewOptionsCanChangeDuringAnalysis() {
    XFoilWidget widget;
    configureLongRun(widget);
    widget.analyze();
    QVERIFY(widget.isRunning());
    AnalysisWorkflowViewOptions options;
    options.showPaneling = true;
    options.showPanelNodes = false;
    widget.setViewOptions(options);
    QVERIFY(widget.viewOptions().showPaneling);
    QVERIFY(!widget.viewOptions().showPanelNodes);
    QVERIFY(widget.findChild<QCheckBox*>("showPanelingCheckBox")
                ->isChecked());
    widget.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
  }
};

int runXFoilWidgetG6bTests(int argc, char** argv) {
  XFoilWidgetG6bTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetG6bTests.moc"

// End: tests/gui/XFoilWidgetG6bTests.cpp

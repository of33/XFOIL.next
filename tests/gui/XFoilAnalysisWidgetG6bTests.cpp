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

// File: tests/gui/XFoilAnalysisWidgetG6bTests.cpp
// G6b Basic/Expert controls, owning host setup transaction, repeated
// reinitialization, reset, busy behavior, CL sequence and panel overlay.

#include <array>
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
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QtTest/QTest>

#include "plots/GeometryPlotView.h"
#include "qcustomplot.h"
#include "xfoil/gui/AnalysisWorkflowSetup.h"
#include "xfoil/gui/XFoilAnalysisWidget.h"

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
using xfoil::gui::XFoilAnalysisWidget;

template <class T>
T* child(XFoilAnalysisWidget& widget, const char* name) {
  T* value = widget.findChild<T*>(QString::fromLatin1(name));
  Q_ASSERT(value != nullptr);
  return value;
}

QDoubleSpinBox* spin(XFoilAnalysisWidget& widget, const char* name) {
  return child<QDoubleSpinBox>(widget, name);
}

QSpinBox* integerSpin(XFoilAnalysisWidget& widget, const char* name) {
  return child<QSpinBox>(widget, name);
}

QComboBox* combo(XFoilAnalysisWidget& widget, const char* name) {
  return child<QComboBox>(widget, name);
}

QCheckBox* check(XFoilAnalysisWidget& widget, const char* name) {
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

void configureLongRun(XFoilAnalysisWidget& widget) {
  QVERIFY(widget.applySetup(longSequenceSetup()).isOk());
  QVERIFY(widget.isReadyToAnalyze());
}

}  // namespace

class XFoilAnalysisWidgetG6bTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void basicExpertStructureAndPublicDefaults() {
    XFoilAnalysisWidget widget;
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
    QCOMPARE(configuration.reynoldsNumber, 100000.0);
    QCOMPARE(spin(widget, "reynoldsSpin")->value(), 500000.0);
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
    QVERIFY(widget.viewOptions().showPanelNodes);
  }

  void applySetupBeforeShowRoundTripsAllFields() {
    XFoilAnalysisWidget widget;
    QVERIFY(!widget.isVisible());
    const AnalysisWorkflowSetup setup =
        nacaSetup("2412", LiftSequence{0.2, 0.8, 0.2});
    QSignalSpy started(&widget, &XFoilAnalysisWidget::analysisStarted);
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
    QCOMPARE(widget.findChild<QTabWidget*>("analysisTargetTabs")
                 ->currentIndex(),
             3);
    const auto current = widget.currentSetup();
    QVERIFY(current.has_value());
    QVERIFY(std::holds_alternative<NacaGeometrySetup>(current->geometry));
    QCOMPARE(std::get<NacaGeometrySetup>(current->geometry).designation,
             std::string("2412"));
    QVERIFY(std::holds_alternative<LiftSequence>(current->request));
    QCOMPARE(std::get<LiftSequence>(current->request).step, 0.2);
  }

  void uiControlsFeedTheSameModelPath() {
    XFoilAnalysisWidget widget;
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

    widget.findChild<QTabWidget*>("analysisTargetTabs")->setCurrentIndex(3);
    spin(widget, "clSequenceStartSpin")->setValue(0.1);
    spin(widget, "clSequenceEndSpin")->setValue(0.7);
    spin(widget, "clSequenceStepSpin")->setValue(0.15);
    QCOMPARE(widget.clSequence().start, 0.1);
    QCOMPARE(widget.clSequence().endExclusive, 0.7);
    QCOMPARE(widget.clSequence().step, 0.15);
    QVERIFY(widget.isReadyToAnalyze());
  }

  void memoryAndNacsGeometrySetups() {
    XFoilAnalysisWidget source;
    QVERIFY(source.setNaca("0012").isOk());
    QVERIFY(source.geometry().has_value());
    AnalysisWorkflowSetup memory = nacaSetup();
    memory.geometry = MemoryGeometrySetup{*source.geometry()};
    XFoilAnalysisWidget first;
    QVERIFY(first.applySetup(memory).isOk());
    QVERIFY(first.findChild<QRadioButton*>("geometryImportRadio")
                ->isChecked());
    QVERIFY(first.geometry().has_value());
    QVERIFY(first.currentSetup().has_value());
    QVERIFY(std::holds_alternative<MemoryGeometrySetup>(
        first.currentSetup()->geometry));

    AnalysisWorkflowSetup nacs = nacaSetup();
    nacs.geometry = NacsGeometrySetup{"0012"};
    XFoilAnalysisWidget second;
    QVERIFY(second.applySetup(nacs).isOk());
    QVERIFY(second.findChild<QRadioButton*>("geometryNacsRadio")
                ->isChecked());
    QVERIFY(std::holds_alternative<NacsGeometrySetup>(
        second.currentSetup()->geometry));
  }

  void allFourRequestModesApplyWithoutStart() {
    XFoilAnalysisWidget widget;
    QSignalSpy started(&widget, &XFoilAnalysisWidget::analysisStarted);
    const std::vector<AnalysisWorkflowRequest> requests = {
        AlphaTarget{Degrees{2.0}}, LiftTarget{0.4},
        AlphaSequence{Degrees{0.0}, Degrees{3.0}, Degrees{1.0}},
        LiftSequence{0.2, 0.8, 0.2}};
    const std::array<int, 4> expectedTabs = {0, 2, 1, 3};
    for (std::size_t index = 0; index < requests.size(); ++index) {
      auto setup = nacaSetup();
      setup.request = requests[index];
      QVERIFY(widget.applySetup(setup).isOk());
      QCOMPARE(widget.findChild<QTabWidget*>("analysisTargetTabs")
                   ->currentIndex(),
               expectedTabs[index]);
      QVERIFY(widget.isReadyToAnalyze());
    }
    QCOMPARE(started.count(), 0);
  }

  void applyDoesNotAutostartAndClearsOldResults() {
    XFoilAnalysisWidget widget;
    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    QSignalSpy started(&widget, &XFoilAnalysisWidget::analysisStarted);
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
    XFoilAnalysisWidget widget;
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
    XFoilAnalysisWidget widget;
    QVERIFY(widget.applySetup(nacaSetup()).isOk());
    const auto oldSetup = widget.currentSetup();
    const auto oldGeometry = widget.geometry();
    const auto oldPanel = widget.panelGeometry();
    const bool oldReady = widget.isReadyToAnalyze();
    const XFoilGuiState oldState = widget.state();

    QSignalSpy geometry(&widget, &XFoilAnalysisWidget::geometryChanged);
    QSignalSpy paneling(&widget, &XFoilAnalysisWidget::panelingChanged);
    QSignalSpy configuration(&widget, &XFoilAnalysisWidget::configurationChanged);
    QSignalSpy target(&widget, &XFoilAnalysisWidget::targetChanged);
    QSignalSpy applied(&widget, &XFoilAnalysisWidget::setupApplied);
    QSignalSpy failed(&widget, &XFoilAnalysisWidget::setupFailed);

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
    XFoilAnalysisWidget widget;
    auto setup = nacaSetup("0012", LiftSequence{0.2, 0.8, 0.0});
    const auto result = widget.applySetup(setup);
    QVERIFY(result.isErr());
    QCOMPARE(QString::fromStdString(result.error().code),
             QStringLiteral("sequence.zeroStep"));
    QVERIFY(!widget.geometry().has_value());
    QVERIFY(widget.state() == XFoilGuiState::empty);
  }

  void busyRejectsApplyAndResetButAllowsViewOptions() {
    XFoilAnalysisWidget widget;
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
    XFoilAnalysisWidget widget;
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
    XFoilAnalysisWidget widget;
    QStringList order;
    QObject::connect(&widget, &XFoilAnalysisWidget::geometryChanged, &widget,
                     [&order] { order.push_back("geometryChanged"); });
    QObject::connect(&widget, &XFoilAnalysisWidget::panelingChanged, &widget,
                     [&order] { order.push_back("panelingChanged"); });
    QObject::connect(&widget, &XFoilAnalysisWidget::configurationChanged, &widget,
                     [&order] { order.push_back("configurationChanged"); });
    QObject::connect(&widget, &XFoilAnalysisWidget::targetChanged, &widget,
                     [&order] { order.push_back("targetChanged"); });
    QObject::connect(&widget, &XFoilAnalysisWidget::setupApplied, &widget,
                     [&order] { order.push_back("setupApplied"); });
    QObject::connect(&widget, &XFoilAnalysisWidget::stateChanged, &widget,
                     [&order](XFoilGuiState) {
                       order.push_back("stateChanged");
                     });
    QObject::connect(&widget, &XFoilAnalysisWidget::validationChanged, &widget,
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

  void applySetupAfterShowAndRepeatedViewOptionsStaySideEffectFree() {
    XFoilAnalysisWidget widget;
    AnalysisWorkflowViewOptions beforeShow;
    beforeShow.showPaneling = true;
    beforeShow.showPanelNodes = false;
    widget.setViewOptions(beforeShow);
    QVERIFY(widget.viewOptions().showPaneling);
    QVERIFY(!widget.viewOptions().showPanelNodes);
    QVERIFY(!widget.geometry().has_value());
    QVERIFY(widget.state() == XFoilGuiState::empty);

    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    AnalysisWorkflowViewOptions afterShow;
    afterShow.showPaneling = false;
    afterShow.showPanelNodes = true;
    widget.setViewOptions(afterShow);
    QVERIFY(!widget.viewOptions().showPaneling);
    QVERIFY(widget.viewOptions().showPanelNodes);

    QSignalSpy started(&widget, &XFoilAnalysisWidget::analysisStarted);
    auto setup = nacaSetup("0012", AlphaTarget{Degrees{3.0}});
    QVERIFY(widget.applySetup(setup).isOk());
    QCOMPARE(started.count(), 0);
    QVERIFY(widget.isReadyToAnalyze());
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QCOMPARE(widget.panelingOptions().panelCount, setup.paneling.panelCount);
    QCOMPARE(widget.configuration().reynoldsNumber,
             setup.configuration.reynoldsNumber);
    QCOMPARE(std::get<AlphaTarget>(widget.target()).alpha.value, 3.0);
    const auto accepted = widget.currentSetup();
    QVERIFY(accepted.has_value());

    QSignalSpy geometry(&widget, &XFoilAnalysisWidget::geometryChanged);
    QSignalSpy paneling(&widget, &XFoilAnalysisWidget::panelingChanged);
    QSignalSpy configuration(&widget,
                             &XFoilAnalysisWidget::configurationChanged);
    QSignalSpy target(&widget, &XFoilAnalysisWidget::targetChanged);
    QSignalSpy failed(&widget, &XFoilAnalysisWidget::setupFailed);

    const auto rejected = widget.applySetup(nacaSetup("not-a-naca"));
    QVERIFY(rejected.isErr());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(geometry.count(), 0);
    QCOMPARE(paneling.count(), 0);
    QCOMPARE(configuration.count(), 0);
    QCOMPARE(target.count(), 0);
    QCOMPARE(started.count(), 0);
    QVERIFY(widget.isReadyToAnalyze());
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QCOMPARE(widget.currentSetup()->configuration.reynoldsNumber,
             accepted->configuration.reynoldsNumber);
    QCOMPARE(std::get<AlphaTarget>(widget.target()).alpha.value, 3.0);
  }

  void resetFromInitialStateIsSafeAndDoesNotStart() {
    XFoilAnalysisWidget widget;
    QSignalSpy started(&widget, &XFoilAnalysisWidget::analysisStarted);
    QVERIFY(widget.resetWorkflow().isOk());
    QVERIFY(widget.resetWorkflow().isOk());
    QCOMPARE(started.count(), 0);
    QVERIFY(widget.state() == XFoilGuiState::empty);
    QVERIFY(!widget.geometry().has_value());
    QVERIFY(!widget.panelGeometry().has_value());
    QVERIFY(!widget.currentSetup().has_value());
    QVERIFY(!widget.isRunning());
    QCOMPARE(widget.findChild<QProgressBar*>("analysisProgressBar")->value(),
             0);
    QVERIFY(widget.findChild<QLabel*>("analysisCurrentPointLabel")
                ->text()
                .isEmpty());
    QVERIFY(!widget.analyzeAction()->isEnabled());
  }

  void resetAfterResultsAndComparisonClearsStateAndAllowsReanalysis() {
    XFoilAnalysisWidget widget;
    const auto setup = nacaSetup("0012", AlphaTarget{Degrees{2.0}});
    QVERIFY(widget.applySetup(setup).isOk());
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.currentOperatingPoint().has_value());
    QVERIFY(widget.addCurrentComparisonPolar().isOk());
    auto* polarList = widget.findChild<QListWidget*>("polarListView");
    QVERIFY(polarList != nullptr);
    QCOMPARE(polarList->count(), 1);

    QVERIFY(widget.resetWorkflow().isOk());
    QCOMPARE(polarList->count(), 0);
    QVERIFY(!widget.selectedComparisonPolar().has_value());
    QVERIFY(!widget.currentPolar().has_value());
    QVERIFY(!widget.currentOperatingPoint().has_value());
    QVERIFY(!widget.currentSequence().has_value());
    QVERIFY(widget.state() == XFoilGuiState::empty);
    QVERIFY(!widget.currentSetup().has_value());
    QVERIFY(!widget.isReadyToAnalyze());
    QCOMPARE(widget.findChild<QProgressBar*>("analysisProgressBar")->value(),
             0);
    QVERIFY(widget.findChild<QLabel*>("analysisCurrentPointLabel")
                ->text()
                .isEmpty());
    QVERIFY(widget.findChild<QLabel*>("analysisIterationLabel")
                ->text()
                .isEmpty());
    QVERIFY(!widget.analyzeAction()->isEnabled());
    QVERIFY(!widget.cancelAction()->isEnabled());

    const auto retry = nacaSetup("0012", AlphaTarget{Degrees{1.5}});
    QVERIFY(widget.applySetup(retry).isOk());
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.currentOperatingPoint().has_value());
    QCOMPARE(widget.currentOperatingPoint()->alpha.value, 1.5);
  }

  void resetAfterCancelledJobClearsProgressAndAllowsReanalysis() {
    XFoilAnalysisWidget widget;
    configureLongRun(widget);
    QSignalSpy cancelled(&widget, &XFoilAnalysisWidget::analysisCancelled);
    widget.analyze();
    QVERIFY(widget.isRunning());
    widget.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(cancelled.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);

    QVERIFY(widget.resetWorkflow().isOk());
    QVERIFY(widget.state() == XFoilGuiState::empty);
    QVERIFY(!widget.currentSetup().has_value());
    QVERIFY(!widget.currentOperatingPoint().has_value());
    QVERIFY(!widget.currentSequence().has_value());
    QCOMPARE(widget.findChild<QProgressBar*>("analysisProgressBar")->value(),
             0);
    QVERIFY(widget.findChild<QLabel*>("analysisCurrentPointLabel")
                ->text()
                .isEmpty());
    QVERIFY(widget.findChild<QLabel*>("analysisIterationLabel")
                ->text()
                .isEmpty());

    const auto retry = nacaSetup("0012", AlphaTarget{Degrees{2.0}});
    QVERIFY(widget.applySetup(retry).isOk());
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.currentOperatingPoint().has_value());
  }

  void twoWidgetsKeepSetupAndViewOptionsIndependent() {
    XFoilAnalysisWidget first;
    XFoilAnalysisWidget second;
    QCOMPARE(spin(first, "reynoldsSpin")->value(), 500000.0);
    QCOMPARE(spin(second, "reynoldsSpin")->value(), 500000.0);
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
    XFoilAnalysisWidget widget;
    auto setup = nacaSetup("0012", LiftSequence{0.2, 0.6, 0.2});
    setup.configuration.viscous = false;
    setup.paneling.panelCount = 100;
    QVERIFY(widget.applySetup(setup).isOk());
    QSignalSpy started(&widget, &XFoilAnalysisWidget::analysisStarted);
    widget.analyze();
    QCOMPARE(started.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.state() == XFoilGuiState::results);
    QVERIFY(widget.currentSequence().has_value());
    QCOMPARE(widget.currentSequence()->points.size(), std::size_t{2});
    QVERIFY(widget.currentSequence()->points.front().polar.alpha.value <
            widget.currentSequence()->points.back().polar.alpha.value);
  }

  void panelingBasicControlsAndExpertAutoInitialize() {
    XFoilAnalysisWidget widget;
    auto* basic = child<QWidget>(widget, "panelingBasicPage");
    auto* expert = child<QWidget>(widget, "conditionExpertPage");
    auto* show = check(widget, "showPanelingCheckBox");
    auto* generate = child<QPushButton>(widget, "generatePanelButton");
    auto* autoInit =
        check(widget, "autoInitializeBoundaryLayerCheck");
    QVERIFY(basic && expert && show && generate && autoInit);
    QVERIFY(basic->isAncestorOf(show));
    QVERIFY(basic->isAncestorOf(generate));
    QVERIFY(expert->isAncestorOf(autoInit));
    QVERIFY(widget.findChild<QLabel*>("autoInitializeLabel") == nullptr);
    QCOMPARE(autoInit->text(),
             QStringLiteral("Auto-initialize boundary layer"));
    int explicitLabels = 0;
    for (QLabel* label : widget.findChildren<QLabel*>()) {
      if (label->text().contains(QStringLiteral("BL auto-initialize"))) {
        ++explicitLabels;
      }
    }
    QCOMPARE(explicitLabels, 0);

    auto* operating = child<QGroupBox>(widget, "operatingGroup");
    auto* target = child<QGroupBox>(widget, "targetGroup");
    auto* conditionTabs = child<QTabWidget>(widget, "conditionSettingsTabs");
    auto* targetTabs = child<QTabWidget>(widget, "analysisTargetTabs");
    QVERIFY(operating->layout() != nullptr);
    QVERIFY(target->layout() != nullptr);
    QCOMPARE(operating->layout()->count(), 1);
    QCOMPARE(target->layout()->count(), 1);
    QCOMPARE(operating->layout()->itemAt(0)->widget(), conditionTabs);
    QCOMPARE(target->layout()->itemAt(0)->widget(), targetTabs);
  }

  void minimumSizeKeepsDefinitionsReadableWithoutClipping() {
    XFoilAnalysisWidget widget;
    QCOMPARE(widget.minimumWidth(), 960);
    QCOMPARE(widget.minimumHeight(), 620);
    widget.resize(960, 620);
    widget.show();
    QCoreApplication::processEvents();
    auto* scroll = child<QScrollArea>(widget, "definitionsScrollArea");
    QVERIFY(scroll->horizontalScrollBar()->maximum() == 0);
    QVERIFY(scroll->verticalScrollBar()->maximum() == 0);
    auto* row = child<QWidget>(widget, "definitionsRow");
    QVERIFY(row->height() > 0);
    QVERIFY(row->height() <= scroll->viewport()->height() + 1);
    QVERIFY(row->width() <= scroll->viewport()->width() + 1);
  }

  void clSequenceValidationReachesUiAndModel() {
    XFoilAnalysisWidget widget;
    auto setup = nacaSetup("0012", LiftSequence{0.2, 0.8, 0.0});
    const auto failed = widget.applySetup(setup);
    QVERIFY(failed.isErr());
    QCOMPARE(QString::fromStdString(failed.error().code),
             QStringLiteral("sequence.zeroStep"));

    QVERIFY(widget.applySetup(nacaSetup("0012")).isOk());
    widget.findChild<QTabWidget*>("analysisTargetTabs")->setCurrentIndex(3);
    spin(widget, "clSequenceStepSpin")->setValue(0.0);
    QVERIFY(widget.state() == XFoilGuiState::error);
    QVERIFY(!widget.isReadyToAnalyze());
    QVERIFY(widget.findChild<QPlainTextEdit*>("setupDiagnosticsView")
                ->toPlainText()
                .contains(QStringLiteral("sequence.zeroStep")));
  }

  void defaultOverlayIsOffAndToggleIsViewOnly() {
    XFoilAnalysisWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    QVERIFY(widget.applySetup(nacaSetup("0012")).isOk());
    QSignalSpy geometry(&widget, &XFoilAnalysisWidget::geometryChanged);
    QSignalSpy paneling(&widget, &XFoilAnalysisWidget::panelingChanged);
    QSignalSpy configuration(&widget, &XFoilAnalysisWidget::configurationChanged);
    QSignalSpy target(&widget, &XFoilAnalysisWidget::targetChanged);

    auto* view = static_cast<xfoil::gui::GeometryPlotView*>(
        child<QWidget>(widget, "airfoilPlot"));
    QVERIFY(!view->showPaneling());
    QVERIFY(!view->sampleMarkersEnabled());
    auto* plot = view->findChild<QCustomPlot*>();
    QVERIFY(plot != nullptr);
    auto* geometryContour = plot->findChild<QCPCurve*>("geometryContour");
    auto* contour = plot->findChild<QCPCurve*>("panelContour");
    auto* ticks = plot->findChild<QCPCurve*>("panelTicks");
    QVERIFY(contour != nullptr && ticks != nullptr);
    QVERIFY(geometryContour != nullptr);
    QVERIFY(plot->findChild<QCPGraph*>("panelNodes") == nullptr);
    QVERIFY(!contour->visible());
    QVERIFY(!ticks->visible());
    QCOMPARE(geometryContour->scatterStyle().shape(), QCPScatterStyle::ssNone);

    QTest::mouseClick(plot, Qt::RightButton, Qt::ControlModifier,
                      plot->axisRect()->rect().center());
    QVERIFY(view->sampleMarkersEnabled());
    QCOMPARE(geometryContour->scatterStyle().shape(),
             QCPScatterStyle::ssCircle);
    QVERIFY(!widget.viewOptions().showPaneling);
    QVERIFY(!check(widget, "showPanelingCheckBox")->isChecked());
    QVERIFY(!contour->visible());
    QVERIFY(!ticks->visible());
    QCOMPARE(geometry.count(), 0);
    QCOMPARE(paneling.count(), 0);
    QCOMPARE(configuration.count(), 0);
    QCOMPARE(target.count(), 0);

    QTest::mouseClick(plot, Qt::RightButton, Qt::ControlModifier,
                      plot->axisRect()->rect().center());
    QVERIFY(!view->sampleMarkersEnabled());
    QCOMPARE(geometryContour->scatterStyle().shape(), QCPScatterStyle::ssNone);

    AnalysisWorkflowViewOptions options;
    options.showPaneling = true;
    widget.setViewOptions(options);
    QVERIFY(view->showPaneling());
    QVERIFY(contour->visible());
    QVERIFY(ticks->visible());
    QVERIFY(ticks->dataCount() > 0);
    QVERIFY(contour->pen().color().red() > 180);
    QVERIFY(contour->pen().color().green() < 80);
    QCOMPARE(geometry.count(), 0);
    QCOMPARE(paneling.count(), 0);
    QCOMPARE(configuration.count(), 0);
    QCOMPARE(target.count(), 0);
    QVERIFY(widget.isReadyToAnalyze());
  }

  void ux3LayoutContracts() {
    XFoilAnalysisWidget widget;
    widget.resize(1280, 800);
    widget.show();
    QCoreApplication::processEvents();
    auto* geometry = widget.findChild<QGroupBox*>("geometryGroup");
    auto* paneling = widget.findChild<QGroupBox*>("panelingGroup");
    auto* operating = widget.findChild<QGroupBox*>("operatingGroup");
    auto* target = widget.findChild<QGroupBox*>("targetGroup");
    auto* panelTabs = widget.findChild<QTabWidget*>("panelingSettingsTabs");
    auto* condTabs = widget.findChild<QTabWidget*>("conditionSettingsTabs");
    auto* targetTabs = widget.findChild<QTabWidget*>("analysisTargetTabs");
    auto* load = widget.findChild<QPushButton*>("loadAirfoilButton");
    auto* designation = widget.findChild<QLineEdit*>("nacaDesignationEdit");
    QVERIFY(geometry && paneling && operating && target);
    QVERIFY(panelTabs && condTabs && targetTabs);
    QVERIFY(load && designation);
    QCOMPARE(geometry->width(), operating->width());
    QCOMPARE(geometry->width(), target->width());
    const double ratio = static_cast<double>(paneling->width()) /
                         static_cast<double>(geometry->width());
    QVERIFY(ratio >= 0.83 && ratio <= 0.87);
    const int frame = 2 * panelTabs->style()->pixelMetric(
                                QStyle::PM_DefaultFrameWidth,
                                nullptr, panelTabs);
    QCOMPARE(panelTabs->height(),
             panelTabs->tabBar()->sizeHint().height() +
                 panelTabs->widget(1)->sizeHint().height() + frame + 2);
    QCOMPARE(condTabs->height(), targetTabs->height());
    QVERIFY(panelTabs->height() > condTabs->height());
    QVERIFY(load->width() >= 110 && load->width() <= 130);
    QVERIFY(designation->width() >= 80 && designation->width() <= 150);
    QCOMPARE(spin(widget, "reynoldsSpin")->value(), 500000.0);
    auto* scroll = widget.findChild<QScrollArea*>("definitionsScrollArea");
    QVERIFY(scroll != nullptr);
    QCOMPARE(scroll->verticalScrollBar()->maximum(), 0);
    QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
  }

  void viewOptionsCanChangeDuringAnalysis() {
    XFoilAnalysisWidget widget;
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

int runXFoilAnalysisWidgetG6bTests(int argc, char** argv) {
  XFoilAnalysisWidgetG6bTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilAnalysisWidgetG6bTests.moc"

// End: tests/gui/XFoilAnalysisWidgetG6bTests.cpp

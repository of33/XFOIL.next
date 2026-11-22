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

// File: src/gui/XFoilWidget.cpp
// G3 widget controller. The widget owns one QThread and one worker-owned
// Session. Programmatic setters are short synchronous worker calls outside
// a job; analysis is queued and all results return as owning values. No
// file system or application ownership is introduced.

#include "xfoil/gui/XFoilWidget.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>
#include <QThread>

#include "model/GuiDefinitionModel.h"
#include "model/GuiResultSelectionModel.h"
#include "model/GuiResultSummary.h"
#include "plots/GeometryPlotView.h"
#include "plots/GuiPointPresentation.h"
#include "plots/ResultPlotView.h"
#include "plots/ScalarPlotView.h"
#include "ui_XFoilWidget.h"
#include "worker/GuiAnalysisWorker.h"
#include "worker/GuiJobTracker.h"
#include "xfoil/Diagnostic.h"
#include "xfoil/Limits.h"

namespace xfoil::gui {
namespace {

void ensureGuiMetaTypes() {
  static const int state =
      qRegisterMetaType<XFoilGuiState>("XFoilGuiState");
  static const int progress =
      qRegisterMetaType<xfoil::ProgressEvent>("xfoil::ProgressEvent");
  static const int point = qRegisterMetaType<xfoil::OperatingPointData>(
      "xfoil::OperatingPointData");
  static const int sequence =
      qRegisterMetaType<xfoil::DetailedSequenceResult>(
          "xfoil::DetailedSequenceResult");
  static const int error = qRegisterMetaType<xfoil::Error>("xfoil::Error");
  static const int finish = qRegisterMetaType<GuiAnalysisFinish>(
      "xfoil::gui::GuiAnalysisFinish");
  (void)state;
  (void)progress;
  (void)point;
  (void)sequence;
  (void)error;
  (void)finish;
}

Error busyError() {
  return Error::make(ErrorCategory::invalidState, "gui.busy",
                     "An analysis is already running or the worker is in use",
                     "worker");
}

Error wrongThreadError() {
  return Error::make(ErrorCategory::invalidState, "gui.wrongThread",
                     "GUI setup calls must run on the widget thread",
                     "worker");
}

Error workerUnavailableError() {
  return Error::make(ErrorCategory::invalidState,
                     "gui.workerUnavailable",
                     "The analysis worker session is not available",
                     "worker");
}

QString phaseName(ProgressPhase phase) {
  switch (phase) {
    case ProgressPhase::geometry:
      return QStringLiteral("geometry");
    case ProgressPhase::paneling:
      return QStringLiteral("paneling");
    case ProgressPhase::initialization:
      return QStringLiteral("initialization");
    case ProgressPhase::inviscidSolve:
      return QStringLiteral("inviscid solve");
    case ProgressPhase::viscousIteration:
      return QStringLiteral("viscous iteration");
    case ProgressPhase::sequence:
      return QStringLiteral("alpha sequence");
    case ProgressPhase::none:
      break;
  }
  return QStringLiteral("analysis");
}

QString formatProgress(const ProgressEvent& event) {
  QString text = QStringLiteral("Running: %1").arg(phaseName(event.phase));
  if (event.sequenceIndex.has_value()) {
    text += QStringLiteral(", point %1")
                .arg(static_cast<qulonglong>(*event.sequenceIndex + 1));
  }
  if (event.iteration.has_value()) {
    text += QStringLiteral(", iteration %1")
                .arg(static_cast<qulonglong>(*event.iteration + 1));
  }
  if (event.total.has_value() && *event.total > 0) {
    text += QStringLiteral(" (%1/%2)")
                .arg(static_cast<qulonglong>(event.completed))
                .arg(static_cast<qulonglong>(*event.total));
  } else if (event.completed > 0) {
    text += QStringLiteral(" (%1)")
                .arg(static_cast<qulonglong>(event.completed));
  }
  return text;
}

QString severityName(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::info:
      return QStringLiteral("info");
    case DiagnosticSeverity::warning:
      return QStringLiteral("warning");
    case DiagnosticSeverity::error:
      return QStringLiteral("error");
  }
  return QStringLiteral("info");
}

QString formatDiagnostic(const Diagnostic& diagnostic) {
  QString text = QStringLiteral("[%1]").arg(severityName(diagnostic.severity));
  if (!diagnostic.code.empty()) {
    text += QStringLiteral(" %1")
                .arg(QString::fromStdString(diagnostic.code));
  }
  if (!diagnostic.message.empty()) {
    text += QStringLiteral(": %1")
                .arg(QString::fromStdString(diagnostic.message));
  }
  if (diagnostic.iteration.has_value()) {
    text += QStringLiteral(" (iteration %1)")
                .arg(static_cast<qulonglong>(*diagnostic.iteration + 1));
  }
  return text;
}

QString formatError(const Error& error) {
  const QString code = QString::fromStdString(error.code);
  const QString message = QString::fromStdString(error.message);
  if (code.isEmpty()) {
    return message;
  }
  return QStringLiteral("%1: %2").arg(code, message);
}

QString targetSummary(const GuiDefinitionModel& model) {
  if (model.mode() == AnalysisMode::alphaSequence) {
    const AlphaSequence& sequence = model.sequence();
    return QStringLiteral("Alpha sequence %1 -> %2, step %3 deg (end exclusive)")
        .arg(sequence.start.value, 0, 'g', 8)
        .arg(sequence.endExclusive.value, 0, 'g', 8)
        .arg(sequence.step.value, 0, 'g', 8);
  }
  if (model.mode() == AnalysisMode::clSequence) {
    const LiftSequence& sequence = model.clSequence();
    return QStringLiteral("CL sequence %1 -> %2, step %3 (end exclusive)")
        .arg(sequence.start, 0, 'g', 8)
        .arg(sequence.endExclusive, 0, 'g', 8)
        .arg(sequence.step, 0, 'g', 8);
  }
  if (model.mode() == AnalysisMode::clPoint) {
    return QStringLiteral("CL point %1")
        .arg(std::get<LiftTarget>(model.target()).cl, 0, 'g', 8);
  }
  return QStringLiteral("Alpha point %1 deg")
      .arg(std::get<AlphaTarget>(model.target()).alpha.value, 0, 'g', 8);
}

PanelingOptions panelingFromControls(const Ui::XFoilWidget& ui) {
  PanelingOptions options;
  options.panelCount =
      static_cast<std::size_t>(ui.panelCountSpin->value());
  options.bunchingParameter = ui.bunchingSpin->value();
  options.teToLeDensityRatio = ui.teToLeSpin->value();
  options.refinementDensityRatio = ui.refinementSpin->value();
  options.upperRefinementStart = ui.upperRefinementStartSpin->value();
  options.upperRefinementEnd = ui.upperRefinementEndSpin->value();
  options.lowerRefinementStart = ui.lowerRefinementStartSpin->value();
  options.lowerRefinementEnd = ui.lowerRefinementEndSpin->value();
  return options;
}

AnalysisConfig configurationFromControls(const Ui::XFoilWidget& ui) {
  AnalysisConfig configuration;
  configuration.reynoldsNumber = ui.reynoldsSpin->value();
  configuration.machNumber = ui.machSpin->value();
  configuration.viscous = ui.viscousCheck->isChecked();
  configuration.maximumIterations = ui.iterationsSpin->value();
  configuration.transition.nCritical = ui.ncritSpin->value();
  configuration.transition.topTripX = ui.xtrUpperSpin->value();
  configuration.transition.bottomTripX = ui.xtrLowerSpin->value();
  configuration.reynoldsDependency = static_cast<ReynoldsDependency>(
      ui.reynoldsDependencyCombo->currentData().toInt());
  configuration.machDependency = static_cast<MachDependency>(
      ui.machDependencyCombo->currentData().toInt());
  configuration.autoInitializeBoundaryLayer =
      ui.autoInitializeBoundaryLayerCheck->isChecked();
  configuration.viscousAcceleration = ui.viscousAccelerationSpin->value();
  return configuration;
}

AlphaSequence sequenceFromControls(const Ui::XFoilWidget& ui) {
  return AlphaSequence{Degrees{ui.sequenceStartSpin->value()},
                       Degrees{ui.sequenceEndSpin->value()},
                       Degrees{ui.sequenceStepSpin->value()}};
}

LiftSequence clSequenceFromControls(const Ui::XFoilWidget& ui) {
  return LiftSequence{ui.clSequenceStartSpin->value(),
                      ui.clSequenceEndSpin->value(),
                      ui.clSequenceStepSpin->value()};
}

AnalysisTarget targetFromControls(const Ui::XFoilWidget& ui,
                                  AnalysisMode mode) {
  if (mode == AnalysisMode::clPoint) {
    return LiftTarget{ui.clTargetSpin->value()};
  }
  return AlphaTarget{Degrees{ui.alphaTargetSpin->value()}};
}

void setComboIndex(QComboBox* combo, int index) {
  const QSignalBlocker blocker(combo);
  combo->setCurrentIndex(index);
}

void setComboData(QComboBox* combo, int data) {
  const QSignalBlocker blocker(combo);
  const int index = combo->findData(data);
  combo->setCurrentIndex(index);
}

void setDoubleValue(QDoubleSpinBox* spin, double value) {
  const QSignalBlocker blocker(spin);
  spin->setValue(value);
}

void setIntegerValue(QSpinBox* spin, int value) {
  const QSignalBlocker blocker(spin);
  spin->setValue(value);
}

void setCheckState(QCheckBox* check, bool checked) {
  const QSignalBlocker blocker(check);
  check->setChecked(checked);
}

int geometrySourceIndex(GeometrySource source) {
  switch (source) {
    case GeometrySource::memory:
      return 0;
    case GeometrySource::naca:
      return 1;
    case GeometrySource::nacs:
      return 2;
    case GeometrySource::none:
      return 0;
  }
  return 0;
}

}  // namespace

class XFoilWidget::Impl {
 public:
  explicit Impl(XFoilWidget* owner) : owner_(owner) {
    ensureGuiMetaTypes();
    ui.setupUi(owner_);
    populateChoiceControls();
    initializeControlValues();
    connectControls();
    createActions();
    startWorker();
  }

  ~Impl() { shutdown(); }

  void initialize() {
    configureResultViews();
    updateResultMatrix();
    ui.airfoilPlot->setShowPaneling(viewOptions_.showPaneling);
    ui.airfoilPlot->showPanelNodes(viewOptions_.showPanelNodes);
    if (workerReady_) {
      (void)owner_->setConfiguration(configurationFromControls(ui));
    }
    refreshPointSelection();
    refreshUi();
  }

  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry) {
    if (const Result<void> validation = validateSetupCall();
        validation.isErr()) {
      return validation;
    }
    invalidateAnalysis();
    const std::optional<Result<void>> invoked =
        invokeWorker([this, &geometry]() {
          return worker_->setGeometry(geometry);
        });
    if (!invoked.has_value()) {
      return failUnavailable(workerUnavailableError());
    }
    const Result<void> result = *invoked;
    if (result.isErr()) {
      model_.fail(result.error());
      refreshUi();
      return result;
    }

    const bool hadPanel = model_.panel().has_value();
    model_.acceptGeometry(geometry);
    updateGeometryControls();
    ui.airfoilPlot->setGeometry(geometry);
    ui.airfoilPlot->clearPanel();
    refreshUi();
    Q_EMIT owner_->geometryChanged();
    if (hadPanel) {
      Q_EMIT owner_->panelingChanged();
    }
    return Result<void>::ok();
  }

  [[nodiscard]]
  Result<void> setNaca(std::string_view designation,
                       bool sharpTrailingEdge) {
    if (const Result<void> validation = validateSetupCall();
        validation.isErr()) {
      return validation;
    }
    invalidateAnalysis();
    const std::optional<Result<geometry::AirfoilGeometry>> invoked =
        invokeWorker([this, designation, sharpTrailingEdge]() {
          return worker_->setNaca(designation, sharpTrailingEdge);
        });
    if (!invoked.has_value()) {
      return failUnavailable(workerUnavailableError());
    }
    const Result<geometry::AirfoilGeometry> result = *invoked;
    if (result.isErr()) {
      model_.fail(result.error());
      refreshUi();
      return Result<void>::fail(result.error());
    }

    geometry::AirfoilGeometry geometry = result.value();
    const bool hadPanel = model_.panel().has_value();
    model_.acceptNaca(geometry, std::string(designation),
                      sharpTrailingEdge);
    updateGeometryControls();
    ui.airfoilPlot->setGeometry(geometry);
    ui.airfoilPlot->clearPanel();
    refreshUi();
    Q_EMIT owner_->geometryChanged();
    if (hadPanel) {
      Q_EMIT owner_->panelingChanged();
    }
    return Result<void>::ok();
  }

  void setPanelingOptions(const PanelingOptions& options) {
    if (validateSetupCall().isErr()) {
      return;
    }
    invalidateAnalysis();
    model_.setPanelingOptions(options);
    updatePanelingControls(options);
    ui.airfoilPlot->clearPanel();
    refreshUi();
    Q_EMIT owner_->panelingChanged();
  }

  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(const PanelingOptions& options) {
    if (const Result<void> validation = validateSetupCall();
        validation.isErr()) {
      return Result<SurfaceGeometry>::fail(validation.error());
    }
    invalidateAnalysis();
    const std::optional<Result<SurfaceGeometry>> invoked =
        invokeWorker([this, &options]() {
          return worker_->generatePanel(options);
        });
    if (!invoked.has_value()) {
      return failUnavailable<SurfaceGeometry>(workerUnavailableError());
    }
    const Result<SurfaceGeometry> result = *invoked;
    if (result.isErr()) {
      model_.fail(result.error());
      refreshUi();
      return result;
    }

    const SurfaceGeometry panel = result.value();
    model_.acceptPanel(panel, options);
    updatePanelingControls(options);
    ui.airfoilPlot->setPanelGeometry(panel);
    ui.airfoilPlot->showPanelNodes(ui.showPanelNodesCheck->isChecked());
    refreshUi();
    Q_EMIT owner_->panelingChanged();
    return Result<SurfaceGeometry>::ok(panel);
  }

  [[nodiscard]]
  Result<void> setConfiguration(const AnalysisConfig& configuration) {
    if (const Result<void> validation = validateSetupCall();
        validation.isErr()) {
      return validation;
    }
    invalidateAnalysis();
    const std::optional<Result<void>> invoked =
        invokeWorker([this, &configuration]() {
          return worker_->setConfiguration(configuration);
        });
    if (!invoked.has_value()) {
      return failUnavailable(workerUnavailableError());
    }
    const Result<void> result = *invoked;
    if (result.isErr()) {
      model_.fail(result.error());
      refreshUi();
      return result;
    }

    model_.acceptConfiguration(configuration);
    updateConfigurationControls(configuration);
    refreshUi();
    Q_EMIT owner_->configurationChanged();
    return Result<void>::ok();
  }

  void setTarget(const AnalysisTarget& target) {
    if (validateSetupCall().isErr()) {
      return;
    }
    invalidateAnalysis();
    const Result<void> result = model_.setTarget(target);
    if (result.isOk()) {
      updateTargetControls();
    }
    refreshUi();
    Q_EMIT owner_->targetChanged();
  }

  void setSequence(const AlphaSequence& sequence) {
    if (validateSetupCall().isErr()) {
      return;
    }
    invalidateAnalysis();
    const Result<void> result = model_.setSequence(sequence);
    if (result.isOk()) {
      updateTargetControls();
    }
    refreshUi();
    Q_EMIT owner_->targetChanged();
  }

  void setClSequence(const LiftSequence& sequence) {
    if (validateSetupCall().isErr()) {
      return;
    }
    invalidateAnalysis();
    const Result<void> result = model_.setClSequence(sequence);
    if (result.isOk()) {
      updateTargetControls();
    }
    refreshUi();
    Q_EMIT owner_->targetChanged();
  }

  [[nodiscard]]
  Result<void> applySetup(const AnalysisWorkflowSetup& setup) {
    if (const Result<void> validation = validateSetupCall();
        validation.isErr()) {
      return validation;
    }
    const std::optional<Result<GuiAppliedSetup>> invoked =
        invokeWorker([this, &setup]() {
          return worker_->applySetup(setup);
        });
    if (!invoked.has_value()) {
      return failUnavailable(workerUnavailableError());
    }
    const Result<GuiAppliedSetup> applied = *invoked;
    if (applied.isErr()) {
      setupError_ = applied.error();
      refreshUi();
      Q_EMIT owner_->setupFailed(applied.error());
      return Result<void>::fail(applied.error());
    }

    invalidateAnalysis();
    setupError_.reset();
    model_.reset();
    if (const auto* memory =
            std::get_if<MemoryGeometrySetup>(&setup.geometry)) {
      model_.acceptGeometry(memory->geometry);
    } else if (const auto* naca =
                   std::get_if<NacaGeometrySetup>(&setup.geometry)) {
      model_.acceptNaca(applied.value().geometry, naca->designation, false);
    } else if (const auto* nacs =
                   std::get_if<NacsGeometrySetup>(&setup.geometry)) {
      model_.acceptNaca(applied.value().geometry, nacs->designation, true);
    }
    model_.acceptPanel(applied.value().panel, setup.paneling);
    model_.acceptConfiguration(setup.configuration);
    if (const auto* alpha = std::get_if<AlphaTarget>(&setup.request)) {
      (void)model_.setTarget(*alpha);
    } else if (const auto* lift = std::get_if<LiftTarget>(&setup.request)) {
      (void)model_.setTarget(*lift);
    } else if (const auto* sequence =
                   std::get_if<AlphaSequence>(&setup.request)) {
      (void)model_.setSequence(*sequence);
    } else if (const auto* sequence =
                   std::get_if<LiftSequence>(&setup.request)) {
      (void)model_.setClSequence(*sequence);
    }

    updateGeometryControls();
    updatePanelingControls(setup.paneling);
    updateConfigurationControls(setup.configuration);
    updateTargetControls();
    ui.airfoilPlot->setGeometry(applied.value().geometry);
    ui.airfoilPlot->setPanelGeometry(applied.value().panel);
    ui.airfoilPlot->setShowPaneling(viewOptions_.showPaneling);
    ui.airfoilPlot->showPanelNodes(viewOptions_.showPanelNodes);

    Q_EMIT owner_->geometryChanged();
    Q_EMIT owner_->panelingChanged();
    Q_EMIT owner_->configurationChanged();
    Q_EMIT owner_->targetChanged();
    Q_EMIT owner_->setupApplied();
    refreshUi();
    return Result<void>::ok();
  }

  [[nodiscard]]
  Result<void> resetWorkflow() {
    if (const Result<void> validation = validateSetupCall();
        validation.isErr()) {
      return validation;
    }
    const std::optional<Result<void>> invoked =
        invokeWorker([this]() { return worker_->resetSession(); });
    if (!invoked.has_value()) {
      return failUnavailable(workerUnavailableError());
    }
    if (invoked->isErr()) {
      return *invoked;
    }

    invalidateAnalysis();
    setupError_.reset();
    model_.reset();
    ui.airfoilPlot->clear();
    updateGeometryControls();
    updatePanelingControls(model_.panelingOptions());
    updateConfigurationControls(model_.configuration());
    updateTargetControls();
    ui.airfoilPlot->setShowPaneling(viewOptions_.showPaneling);
    ui.airfoilPlot->showPanelNodes(viewOptions_.showPanelNodes);

    Q_EMIT owner_->geometryChanged();
    Q_EMIT owner_->panelingChanged();
    Q_EMIT owner_->configurationChanged();
    Q_EMIT owner_->targetChanged();
    refreshUi();
    return Result<void>::ok();
  }

  void setViewOptions(const AnalysisWorkflowViewOptions& options) {
    if (QThread::currentThread() != owner_->thread()) {
      return;
    }
    viewOptions_ = options;
    updatingControls_ = true;
    setCheckState(ui.showPanelingCheckBox, options.showPaneling);
    setCheckState(ui.showPanelNodesCheck, options.showPanelNodes);
    updatingControls_ = false;
    ui.airfoilPlot->setShowPaneling(options.showPaneling);
    ui.airfoilPlot->showPanelNodes(options.showPanelNodes);
  }

  [[nodiscard]]
  AnalysisWorkflowViewOptions viewOptions() const {
    return viewOptions_;
  }

  [[nodiscard]]
  std::optional<AnalysisWorkflowSetup> currentSetup() const {
    if (!model_.isReady() || !model_.geometry().has_value()) {
      return std::nullopt;
    }
    AnalysisWorkflowSetup setup;
    switch (model_.geometrySource()) {
      case GeometrySource::memory:
        setup.geometry = MemoryGeometrySetup{*model_.geometry()};
        break;
      case GeometrySource::naca:
        setup.geometry = NacaGeometrySetup{model_.designation()};
        break;
      case GeometrySource::nacs:
        setup.geometry = NacsGeometrySetup{model_.designation()};
        break;
      case GeometrySource::none:
        return std::nullopt;
    }
    setup.paneling = model_.panelingOptions();
    setup.configuration = model_.configuration();
    switch (model_.mode()) {
      case AnalysisMode::alphaPoint:
        setup.request = std::get<AlphaTarget>(model_.target());
        break;
      case AnalysisMode::clPoint:
        setup.request = std::get<LiftTarget>(model_.target());
        break;
      case AnalysisMode::alphaSequence:
        setup.request = model_.sequence();
        break;
      case AnalysisMode::clSequence:
        setup.request = model_.clSequence();
        break;
    }
    return setup;
  }

  void analyze() {
    if (QThread::currentThread() != owner_->thread() ||
        !workerReady_ || worker_ == nullptr || thread_ == nullptr ||
        !thread_->isRunning() || tracker_.isRunning() ||
        !model_.isReady()) {
      return;
    }

    const Result<GuiRunSpec> spec = model_.makeRunSpec();
    if (spec.isErr()) {
      return;
    }
    const std::optional<GuiJobKey> key = tracker_.begin();
    if (!key.has_value()) {
      return;
    }

    analysisOutcome_ = AnalysisOutcome::running;
    analysisStatusText_ = QStringLiteral("Starting");
    analysisDiagnostics_.clear();
    currentOperatingPoint_.reset();
    currentSequence_.reset();
    progressFraction_.reset();
    clearResultData();
    activeToken_ = std::make_shared<CancellationToken>();

    GuiAnalysisJob job{std::move(spec.value()), key->jobId,
                       key->generationId, activeToken_, {}};
    if (!queueJob(std::move(job))) {
      tracker_.finish(key->jobId, key->generationId);
      activeToken_.reset();
      analysisOutcome_ = AnalysisOutcome::error;
      analysisStatusText_ = QStringLiteral("Analysis failed");
      analysisDiagnostics_.push_back(formatError(workerUnavailableError()));
      refreshUi();
      Q_EMIT owner_->analysisFailed(workerUnavailableError());
      return;
    }

    refreshUi();
    Q_EMIT owner_->analysisStarted();
  }

  void cancel() {
    if (QThread::currentThread() != owner_->thread() ||
        !tracker_.isRunning() || !activeToken_) {
      return;
    }
    activeToken_->requestCancellation();
    analysisStatusText_ = QStringLiteral("Cancelling");
    refreshUi();
  }

  [[nodiscard]]
  bool isRunning() const {
    return tracker_.isRunning();
  }

  [[nodiscard]]
  QAction* analyzeAction() { return analyzeAction_; }

  [[nodiscard]]
  QAction* cancelAction() { return cancelAction_; }

  [[nodiscard]]
  std::optional<geometry::AirfoilGeometry> geometry() const {
    return model_.geometry();
  }

  [[nodiscard]]
  std::optional<SurfaceGeometry> panelGeometry() const {
    return model_.panel();
  }

  [[nodiscard]]
  PanelingOptions panelingOptions() const {
    return model_.panelingOptions();
  }

  [[nodiscard]]
  AnalysisConfig configuration() const {
    return model_.configuration();
  }

  [[nodiscard]]
  AnalysisTarget target() const {
    return model_.target();
  }

  [[nodiscard]]
  AlphaSequence sequence() const {
    return model_.sequence();
  }

  [[nodiscard]]
  LiftSequence clSequence() const {
    return model_.clSequence();
  }

  [[nodiscard]]
  XFoilGuiState state() const {
    if (tracker_.isRunning()) {
      return XFoilGuiState::running;
    }
    switch (analysisOutcome_) {
      case AnalysisOutcome::none:
        return model_.state();
      case AnalysisOutcome::running:
        return XFoilGuiState::running;
      case AnalysisOutcome::results:
        return XFoilGuiState::results;
      case AnalysisOutcome::cancelled:
        return XFoilGuiState::cancelled;
      case AnalysisOutcome::error:
        return XFoilGuiState::error;
    }
    return model_.state();
  }

  [[nodiscard]]
  bool isReadyToAnalyze() const {
    return model_.isReady();
  }

  [[nodiscard]]
  std::optional<OperatingPointData> currentOperatingPoint() const {
    return currentOperatingPoint_;
  }

  [[nodiscard]]
  std::optional<DetailedSequenceResult> currentSequence() const {
    return currentSequence_;
  }

  void shutdown() {
    if (shutdown_) {
      return;
    }
    shutdown_ = true;
    if (activeToken_) {
      activeToken_->requestCancellation();
    }
    if (worker_ != nullptr) {
      QObject::disconnect(worker_, nullptr, nullptr, nullptr);
    }
    if (thread_ != nullptr) {
      thread_->quit();
      thread_->wait();
    }
    delete worker_;
    worker_ = nullptr;
    delete thread_;
    thread_ = nullptr;
    workerReady_ = false;
    activeToken_.reset();
  }

 private:
  enum class AnalysisOutcome {
    none,
    running,
    results,
    cancelled,
    error
  };

  template <typename Function>
  [[nodiscard]]
  auto invokeWorker(Function&& function)
      -> std::optional<decltype(function())> {
    if (!workerReady_ || worker_ == nullptr || thread_ == nullptr ||
        !thread_->isRunning() ||
        QThread::currentThread() != owner_->thread()) {
      return std::nullopt;
    }
    std::optional<decltype(function())> result;
    const bool invoked = QMetaObject::invokeMethod(
        worker_,
        [&function, &result]() { result.emplace(function()); },
        Qt::BlockingQueuedConnection);
    if (!invoked) {
      return std::nullopt;
    }
    return result;
  }

  template <typename T>
  [[nodiscard]]
  Result<T> failUnavailable(Error error) {
    model_.fail(error);
    refreshUi();
    return Result<T>::fail(std::move(error));
  }

  [[nodiscard]]
  Result<void> failUnavailable(Error error) {
    model_.fail(error);
    refreshUi();
    return Result<void>::fail(std::move(error));
  }

  [[nodiscard]]
  Result<void> validateSetupCall() const {
    if (QThread::currentThread() != owner_->thread()) {
      return Result<void>::fail(wrongThreadError());
    }
    if (!workerReady_ || worker_ == nullptr || thread_ == nullptr ||
        !thread_->isRunning()) {
      return Result<void>::fail(workerUnavailableError());
    }
    if (tracker_.isRunning()) {
      return Result<void>::fail(busyError());
    }
    return Result<void>::ok();
  }

  [[nodiscard]]
  bool queueJob(GuiAnalysisJob job) {
    if (worker_ == nullptr) {
      return false;
    }
    GuiAnalysisWorker* const worker = worker_;
    return QMetaObject::invokeMethod(
        worker_,
        [worker, job = std::move(job)]() mutable {
          worker->run(std::move(job));
        },
        Qt::QueuedConnection);
  }

  void invalidateAnalysis() {
    tracker_.invalidate();
    setupError_.reset();
    analysisOutcome_ = AnalysisOutcome::none;
    currentOperatingPoint_.reset();
    currentSequence_.reset();
    analysisDiagnostics_.clear();
    progressFraction_.reset();
    activeToken_.reset();
    analysisStatusText_ = QStringLiteral("Idle");
    clearResultData();
  }

  void startWorker() {
    thread_ = new QThread(owner_);
    worker_ = new GuiAnalysisWorker();
    worker_->moveToThread(thread_);

    QObject::connect(
        worker_, &GuiAnalysisWorker::progressReady, owner_,
        [this](qulonglong jobId, quint64 generationId,
               const ProgressEvent& event) {
          onProgress(jobId, generationId, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        worker_, &GuiAnalysisWorker::operatingPointReady, owner_,
        [this](qulonglong jobId, quint64 generationId,
               const OperatingPointData& point) {
          onOperatingPoint(jobId, generationId, point);
        },
        Qt::QueuedConnection);
    QObject::connect(
        worker_, &GuiAnalysisWorker::sequenceReady, owner_,
        [this](qulonglong jobId, quint64 generationId,
               const DetailedSequenceResult& sequence) {
          onSequence(jobId, generationId, sequence);
        },
        Qt::QueuedConnection);
    QObject::connect(
        worker_, &GuiAnalysisWorker::finished, owner_,
        [this](qulonglong jobId, quint64 generationId,
               GuiAnalysisFinish finish, Error error) {
          onFinished(jobId, generationId, finish, std::move(error));
        },
        Qt::QueuedConnection);

    thread_->start();
    workerReady_ = true;
    const std::optional<Result<void>> initialized =
        invokeWorker([this]() { return worker_->initialize(); });
    if (!initialized.has_value()) {
      workerReady_ = false;
      model_.fail(workerUnavailableError());
      return;
    }
    if (initialized->isErr()) {
      workerReady_ = false;
      model_.fail(initialized->error());
    }
  }

  void onProgress(qulonglong jobId, quint64 generationId,
                  const ProgressEvent& event) {
    if (!tracker_.accepts(jobId, generationId)) {
      return;
    }
    updateProgress(event);
    Q_EMIT owner_->progressChanged(event);
  }

  void onOperatingPoint(qulonglong jobId, quint64 generationId,
                        const OperatingPointData& point) {
    if (!tracker_.accepts(jobId, generationId)) {
      return;
    }
    currentOperatingPoint_ = point;
    currentSequence_.reset();
    Q_EMIT owner_->operatingPointReady(point);
  }

  void onSequence(qulonglong jobId, quint64 generationId,
                  const DetailedSequenceResult& sequence) {
    if (!tracker_.accepts(jobId, generationId)) {
      return;
    }
    currentSequence_ = sequence;
    currentOperatingPoint_.reset();
    Q_EMIT owner_->sequenceReady(sequence);
  }

  void onFinished(qulonglong jobId, quint64 generationId,
                  GuiAnalysisFinish finish, Error error) {
    if (!tracker_.accepts(jobId, generationId)) {
      return;
    }
    tracker_.finish(jobId, generationId);
    activeToken_.reset();
    progressFraction_.reset();

    const bool havePoint = currentOperatingPoint_.has_value();
    const bool haveSequence =
        currentSequence_.has_value() &&
        !currentSequence_->points.empty();
    if (finish == GuiAnalysisFinish::completed && !havePoint &&
        !haveSequence) {
      finish = GuiAnalysisFinish::failed;
      error = Error::make(ErrorCategory::internal,
                          "gui.emptyAnalysisResult",
                          "The analysis completed without a result",
                          "analysis");
    }

    if (finish == GuiAnalysisFinish::completed) {
      analysisOutcome_ = AnalysisOutcome::results;
      analysisDiagnostics_.clear();
      appendResultDiagnostics();
      if (havePoint) {
        analysisStatusText_ =
            QStringLiteral("Analysis complete (alpha %1°)")
                .arg(currentOperatingPoint_->alpha.value, 0, 'g', 8);
      } else {
        analysisStatusText_ =
            QStringLiteral("Sequence complete (%1 points)")
                .arg(static_cast<qulonglong>(
                    currentSequence_->points.size()));
      }
      populateResultsFromCurrent();
      return;
    }

    if (finish == GuiAnalysisFinish::cancelled) {
      analysisOutcome_ = AnalysisOutcome::cancelled;
      analysisStatusText_ = QStringLiteral("Cancelled");
      appendResultDiagnostics();
      analysisDiagnostics_.push_back(QStringLiteral("Analysis cancelled."));
      populateResultsFromCurrent();
      Q_EMIT owner_->analysisCancelled();
      return;
    }

    analysisOutcome_ = AnalysisOutcome::error;
    analysisStatusText_ = QStringLiteral("Analysis failed");
    appendResultDiagnostics();
    analysisDiagnostics_.push_back(formatError(error));
    populateResultsFromCurrent();
    Q_EMIT owner_->analysisFailed(error);
  }

  void appendResultDiagnostics() {
    if (currentOperatingPoint_.has_value()) {
      for (const Diagnostic& diagnostic :
           currentOperatingPoint_->diagnostics) {
        analysisDiagnostics_.push_back(formatDiagnostic(diagnostic));
      }
    }
    if (currentSequence_.has_value()) {
      for (const Diagnostic& diagnostic :
           currentSequence_->diagnostics) {
        analysisDiagnostics_.push_back(formatDiagnostic(diagnostic));
      }
    }
  }

  void updateProgress(const ProgressEvent& event) {
    analysisStatusText_ = formatProgress(event);
    if (event.total.has_value() && *event.total > 0) {
      const double fraction =
          event.fraction.has_value()
              ? std::clamp(*event.fraction, 0.0, 1.0)
              : std::clamp(static_cast<double>(event.completed) /
                               static_cast<double>(*event.total),
                           0.0, 1.0);
      progressFraction_ = fraction;
    } else {
      progressFraction_.reset();
    }
    refreshProgressBar();
    ui.analysisStatusLabel->setText(analysisStatusText_);
  }

  void refreshProgressBar() {
    const bool running = tracker_.isRunning();
    ui.analysisProgressBar->setVisible(running);
    if (!running) {
      ui.analysisProgressBar->setRange(0, 1);
      ui.analysisProgressBar->setValue(0);
      return;
    }
    if (progressFraction_.has_value()) {
      ui.analysisProgressBar->setRange(0, 100);
      ui.analysisProgressBar->setValue(
          static_cast<int>(std::lround(*progressFraction_ * 100.0)));
    } else {
      ui.analysisProgressBar->setRange(0, 0);
    }
  }

  void createActions() {
    analyzeAction_ = new QAction(QStringLiteral("Analyze"), owner_);
    analyzeAction_->setObjectName(QStringLiteral("analyzeAction"));
    analyzeAction_->setToolTip(
        QStringLiteral("Starts one analysis in the widget worker."));
    cancelAction_ = new QAction(QStringLiteral("Cancel"), owner_);
    cancelAction_->setObjectName(QStringLiteral("cancelAction"));
    cancelAction_->setToolTip(
        QStringLiteral("Requests cancellation of the active analysis."));
    QObject::connect(analyzeAction_, &QAction::triggered, owner_,
                     [this](bool) { analyze(); });
    QObject::connect(cancelAction_, &QAction::triggered, owner_,
                     [this](bool) { cancel(); });
  }

  void populateChoiceControls() {
    ui.geometrySourceCombo->addItem(
        QStringLiteral("Memory"),
        static_cast<int>(GeometrySource::memory));
    ui.geometrySourceCombo->addItem(
        QStringLiteral("NACA"),
        static_cast<int>(GeometrySource::naca));
    ui.geometrySourceCombo->addItem(
        QStringLiteral("NACS"),
        static_cast<int>(GeometrySource::nacs));

    ui.targetModeCombo->addItem(
        QStringLiteral("Alpha point"),
        static_cast<int>(AnalysisMode::alphaPoint));
    ui.targetModeCombo->addItem(
        QStringLiteral("CL point"),
        static_cast<int>(AnalysisMode::clPoint));
    ui.targetModeCombo->addItem(
        QStringLiteral("Alpha sequence"),
        static_cast<int>(AnalysisMode::alphaSequence));
    ui.targetModeCombo->addItem(
        QStringLiteral("CL sequence"),
        static_cast<int>(AnalysisMode::clSequence));

    ui.reynoldsDependencyCombo->addItem(
        QStringLiteral("Constant"),
        static_cast<int>(ReynoldsDependency::constant));
    ui.reynoldsDependencyCombo->addItem(
        QStringLiteral("Inverse sqrt(CL)"),
        static_cast<int>(ReynoldsDependency::inverseSquareRootLift));
    ui.reynoldsDependencyCombo->addItem(
        QStringLiteral("Inverse CL"),
        static_cast<int>(ReynoldsDependency::inverseLift));
    ui.machDependencyCombo->addItem(
        QStringLiteral("Constant"),
        static_cast<int>(MachDependency::constant));
    ui.machDependencyCombo->addItem(
        QStringLiteral("Inverse sqrt(CL)"),
        static_cast<int>(MachDependency::inverseSquareRootLift));
    ui.machDependencyCombo->addItem(
        QStringLiteral("Inverse CL"),
        static_cast<int>(MachDependency::inverseLift));
  }

  void initializeControlValues() {
    const PanelingOptions paneling;
    ui.panelCountSpin->setRange(
        2, static_cast<int>(xfoil::limits::getpanPanelLimit));
    ui.panelCountSpin->setKeyboardTracking(false);
    ui.bunchingSpin->setRange(0.0, 1000.0);
    ui.bunchingSpin->setDecimals(4);
    ui.bunchingSpin->setKeyboardTracking(false);
    ui.teToLeSpin->setRange(0.0, 1000.0);
    ui.teToLeSpin->setDecimals(4);
    ui.teToLeSpin->setKeyboardTracking(false);
    ui.refinementSpin->setRange(0.0, 1000.0);
    ui.refinementSpin->setDecimals(4);
    ui.refinementSpin->setKeyboardTracking(false);
    for (QDoubleSpinBox* spin :
         {ui.upperRefinementStartSpin, ui.upperRefinementEndSpin,
          ui.lowerRefinementStartSpin, ui.lowerRefinementEndSpin}) {
      spin->setRange(0.0, 1.0);
      spin->setDecimals(4);
      spin->setSingleStep(0.05);
      spin->setKeyboardTracking(false);
    }
    setIntegerValue(ui.panelCountSpin,
                    static_cast<int>(paneling.panelCount));
    setDoubleValue(ui.bunchingSpin, paneling.bunchingParameter);
    setDoubleValue(ui.teToLeSpin, paneling.teToLeDensityRatio);
    setDoubleValue(ui.refinementSpin, paneling.refinementDensityRatio);
    setDoubleValue(ui.upperRefinementStartSpin,
                   paneling.upperRefinementStart);
    setDoubleValue(ui.upperRefinementEndSpin, paneling.upperRefinementEnd);
    setDoubleValue(ui.lowerRefinementStartSpin,
                   paneling.lowerRefinementStart);
    setDoubleValue(ui.lowerRefinementEndSpin, paneling.lowerRefinementEnd);

    const AnalysisConfig configuration;
    ui.reynoldsSpin->setRange(0.0, 1.0e12);
    ui.reynoldsSpin->setDecimals(0);
    ui.reynoldsSpin->setSingleStep(1000.0);
    ui.reynoldsSpin->setKeyboardTracking(false);
    ui.machSpin->setRange(0.0, 0.999999);
    ui.machSpin->setDecimals(6);
    ui.machSpin->setSingleStep(0.01);
    ui.machSpin->setKeyboardTracking(false);
    ui.iterationsSpin->setRange(1, 1000000);
    ui.iterationsSpin->setKeyboardTracking(false);
    ui.ncritSpin->setRange(0.001, 100.0);
    ui.ncritSpin->setDecimals(3);
    ui.ncritSpin->setSingleStep(0.1);
    ui.ncritSpin->setKeyboardTracking(false);
    ui.xtrUpperSpin->setRange(0.0, 1.0);
    ui.xtrUpperSpin->setDecimals(6);
    ui.xtrUpperSpin->setSingleStep(0.05);
    ui.xtrUpperSpin->setKeyboardTracking(false);
    ui.xtrLowerSpin->setRange(0.0, 1.0);
    ui.xtrLowerSpin->setDecimals(6);
    ui.xtrLowerSpin->setSingleStep(0.05);
    ui.xtrLowerSpin->setKeyboardTracking(false);
    ui.viscousAccelerationSpin->setRange(0.0, 100.0);
    ui.viscousAccelerationSpin->setDecimals(6);
    ui.viscousAccelerationSpin->setSingleStep(0.01);
    ui.viscousAccelerationSpin->setKeyboardTracking(false);
    setDoubleValue(ui.reynoldsSpin, configuration.reynoldsNumber);
    setDoubleValue(ui.machSpin, configuration.machNumber);
    setCheckState(ui.viscousCheck, configuration.viscous);
    setIntegerValue(ui.iterationsSpin, configuration.maximumIterations);
    setDoubleValue(ui.ncritSpin, configuration.transition.nCritical);
    setDoubleValue(ui.xtrUpperSpin, configuration.transition.topTripX);
    setDoubleValue(ui.xtrLowerSpin, configuration.transition.bottomTripX);
    setDoubleValue(ui.viscousAccelerationSpin,
                   configuration.viscousAcceleration);
    setCheckState(ui.autoInitializeBoundaryLayerCheck,
                  configuration.autoInitializeBoundaryLayer);
    setComboData(ui.reynoldsDependencyCombo,
                 static_cast<int>(configuration.reynoldsDependency));
    setComboData(ui.machDependencyCombo,
                 static_cast<int>(configuration.machDependency));

    ui.alphaTargetSpin->setRange(-360.0, 360.0);
    ui.alphaTargetSpin->setDecimals(6);
    ui.alphaTargetSpin->setSingleStep(1.0);
    ui.alphaTargetSpin->setKeyboardTracking(false);
    ui.clTargetSpin->setRange(-100.0, 100.0);
    ui.clTargetSpin->setDecimals(6);
    ui.clTargetSpin->setSingleStep(0.1);
    ui.clTargetSpin->setKeyboardTracking(false);
    ui.sequenceStartSpin->setRange(-360.0, 360.0);
    ui.sequenceStartSpin->setDecimals(6);
    ui.sequenceStartSpin->setSingleStep(1.0);
    ui.sequenceStartSpin->setKeyboardTracking(false);
    ui.sequenceEndSpin->setRange(-360.0, 360.0);
    ui.sequenceEndSpin->setDecimals(6);
    ui.sequenceEndSpin->setSingleStep(1.0);
    ui.sequenceEndSpin->setKeyboardTracking(false);
    ui.sequenceStepSpin->setRange(-360.0, 360.0);
    ui.sequenceStepSpin->setDecimals(6);
    ui.sequenceStepSpin->setSingleStep(0.5);
    ui.sequenceStepSpin->setKeyboardTracking(false);

    for (QDoubleSpinBox* spin :
         {ui.clSequenceStartSpin, ui.clSequenceEndSpin,
          ui.clSequenceStepSpin}) {
      spin->setRange(-100.0, 100.0);
      spin->setDecimals(6);
      spin->setSingleStep(0.1);
      spin->setKeyboardTracking(false);
    }

    const AlphaSequence& sequence = model_.sequence();
    const LiftSequence& clSequence = model_.clSequence();
    setDoubleValue(ui.alphaTargetSpin, 0.0);
    setDoubleValue(ui.clTargetSpin, 0.0);
    setDoubleValue(ui.sequenceStartSpin, sequence.start.value);
    setDoubleValue(ui.sequenceEndSpin, sequence.endExclusive.value);
    setDoubleValue(ui.sequenceStepSpin, sequence.step.value);
    setDoubleValue(ui.clSequenceStartSpin, clSequence.start);
    setDoubleValue(ui.clSequenceEndSpin, clSequence.endExclusive);
    setDoubleValue(ui.clSequenceStepSpin, clSequence.step);
    setCheckState(ui.showPanelingCheckBox, false);
    setCheckState(ui.showPanelNodesCheck, true);

    setComboIndex(ui.geometrySourceCombo, 0);
    setComboIndex(ui.targetModeCombo, 0);
    updateGeometrySourceControls();
    updateTargetControlState();
  }

  void connectControls() {
    QObject::connect(ui.geometrySourceCombo,
            &QComboBox::currentIndexChanged, owner_,
            [this](int) { updateGeometrySourceControls(); });
    QObject::connect(ui.applyGeometryButton, &QPushButton::clicked, owner_,
            [this]() { applyGeometryFromControls(); });

    for (QDoubleSpinBox* spin :
         {ui.bunchingSpin, ui.teToLeSpin, ui.refinementSpin,
          ui.upperRefinementStartSpin, ui.upperRefinementEndSpin,
          ui.lowerRefinementStartSpin, ui.lowerRefinementEndSpin}) {
      connect(spin, &QDoubleSpinBox::valueChanged, owner_,
              [this](double) { panelingControlsChanged(); });
    }
    QObject::connect(ui.panelCountSpin, &QSpinBox::valueChanged, owner_,
            [this](int) { panelingControlsChanged(); });
    QObject::connect(ui.generatePanelButton, &QPushButton::clicked, owner_,
            [this]() { generatePanelFromControls(); });
    QObject::connect(ui.showPanelingCheckBox, &QCheckBox::toggled, owner_,
            [this](bool checked) {
              AnalysisWorkflowViewOptions options = viewOptions_;
              options.showPaneling = checked;
              owner_->setViewOptions(options);
            });
    QObject::connect(ui.showPanelNodesCheck, &QCheckBox::toggled, owner_,
            [this](bool checked) {
              AnalysisWorkflowViewOptions options = viewOptions_;
              options.showPanelNodes = checked;
              owner_->setViewOptions(options);
            });

    for (QDoubleSpinBox* spin :
         {ui.reynoldsSpin, ui.machSpin, ui.ncritSpin, ui.xtrUpperSpin,
          ui.xtrLowerSpin, ui.viscousAccelerationSpin}) {
      connect(spin, &QDoubleSpinBox::valueChanged, owner_,
              [this](double) { configurationControlsChanged(); });
    }
    QObject::connect(ui.iterationsSpin, &QSpinBox::valueChanged, owner_,
            [this](int) { configurationControlsChanged(); });
    QObject::connect(ui.viscousCheck, &QCheckBox::toggled, owner_,
            [this](bool) { configurationControlsChanged(); });
    QObject::connect(ui.autoInitializeBoundaryLayerCheck,
            &QCheckBox::toggled, owner_,
            [this](bool) { configurationControlsChanged(); });
    for (QComboBox* combo :
         {ui.reynoldsDependencyCombo, ui.machDependencyCombo}) {
      QObject::connect(combo, &QComboBox::currentIndexChanged, owner_,
              [this](int) { configurationControlsChanged(); });
    }

    QObject::connect(ui.targetModeCombo, &QComboBox::currentIndexChanged, owner_,
            [this](int) { targetModeChanged(); });
    for (QDoubleSpinBox* spin :
         {ui.alphaTargetSpin, ui.clTargetSpin, ui.sequenceStartSpin,
          ui.sequenceEndSpin, ui.sequenceStepSpin, ui.clSequenceStartSpin,
          ui.clSequenceEndSpin, ui.clSequenceStepSpin}) {
      connect(spin, &QDoubleSpinBox::valueChanged, owner_,
              [this](double) { targetFieldsChanged(); });
    }
    QObject::connect(ui.analyzeButton, &QPushButton::clicked, owner_,
            [this]() { analyze(); });
    QObject::connect(ui.cancelButton, &QPushButton::clicked, owner_,
            [this]() { cancel(); });

    QObject::connect(ui.operatingPointCombo,
            &QComboBox::currentIndexChanged, owner_,
            [this](int index) { operatingPointChanged(index); });
    QObject::connect(ui.deactivatePointCombo,
            &QComboBox::currentIndexChanged, owner_,
            [this](int index) { deactivatePointChanged(index); });
    QObject::connect(ui.activatePointCombo,
            &QComboBox::currentIndexChanged, owner_,
            [this](int index) { activatePointChanged(index); });
  }

  void applyGeometryFromControls() {
    const GeometrySource source = static_cast<GeometrySource>(
        ui.geometrySourceCombo->currentData().toInt());
    if (source != GeometrySource::naca &&
        source != GeometrySource::nacs) {
      return;
    }

    const std::string designation =
        ui.nacaDesignationEdit->text().trimmed().toStdString();
    (void)setNaca(designation, source == GeometrySource::nacs);
  }

  void panelingControlsChanged() {
    if (updatingControls_) {
      return;
    }
    owner_->setPanelingOptions(panelingFromControls(ui));
  }

  void generatePanelFromControls() {
    (void)owner_->generatePanel(panelingFromControls(ui));
  }

  void configurationControlsChanged() {
    if (updatingControls_) {
      return;
    }
    (void)owner_->setConfiguration(configurationFromControls(ui));
  }

  void targetModeChanged() {
    if (updatingControls_) {
      return;
    }
    const AnalysisMode mode = static_cast<AnalysisMode>(
        ui.targetModeCombo->currentData().toInt());
    if (mode == AnalysisMode::alphaSequence) {
      owner_->setSequence(sequenceFromControls(ui));
      return;
    }
    if (mode == AnalysisMode::clSequence) {
      owner_->setClSequence(clSequenceFromControls(ui));
      return;
    }
    owner_->setTarget(targetFromControls(ui, mode));
  }

  void targetFieldsChanged() {
    if (updatingControls_) {
      return;
    }
    const AnalysisMode mode = static_cast<AnalysisMode>(
        ui.targetModeCombo->currentData().toInt());
    if (mode == AnalysisMode::alphaSequence) {
      owner_->setSequence(sequenceFromControls(ui));
      return;
    }
    if (mode == AnalysisMode::clSequence) {
      owner_->setClSequence(clSequenceFromControls(ui));
      return;
    }
    owner_->setTarget(targetFromControls(ui, mode));
  }

  void updateGeometrySourceControls() {
    const GeometrySource source = static_cast<GeometrySource>(
        ui.geometrySourceCombo->currentData().toInt());
    const bool textual =
        source == GeometrySource::naca || source == GeometrySource::nacs;
    ui.nacaDesignationLabel->setEnabled(textual);
    ui.nacaDesignationEdit->setEnabled(textual);
    ui.applyGeometryButton->setEnabled(textual);
  }

  void updateTargetControlState() {
    const AnalysisMode mode = model_.mode();
    ui.alphaTargetSpin->setEnabled(mode == AnalysisMode::alphaPoint);
    ui.clTargetSpin->setEnabled(mode == AnalysisMode::clPoint);
    ui.sequenceFields->setEnabled(mode == AnalysisMode::alphaSequence);
    ui.clSequenceFields->setEnabled(mode == AnalysisMode::clSequence);
    setComboIndex(ui.targetModeCombo, static_cast<int>(mode));
  }

  void updateGeometryControls() {
    updatingControls_ = true;
    const GeometrySource source = model_.geometrySource();
    setComboIndex(ui.geometrySourceCombo, geometrySourceIndex(source));
    {
      const QSignalBlocker blocker(ui.nacaDesignationEdit);
      ui.nacaDesignationEdit->setText(
          QString::fromStdString(model_.designation()));
    }
    updatingControls_ = false;
    updateGeometrySourceControls();
  }

  void updatePanelingControls(const PanelingOptions& options) {
    updatingControls_ = true;
    setIntegerValue(ui.panelCountSpin,
                    static_cast<int>(options.panelCount));
    setDoubleValue(ui.bunchingSpin, options.bunchingParameter);
    setDoubleValue(ui.teToLeSpin, options.teToLeDensityRatio);
    setDoubleValue(ui.refinementSpin, options.refinementDensityRatio);
    setDoubleValue(ui.upperRefinementStartSpin,
                   options.upperRefinementStart);
    setDoubleValue(ui.upperRefinementEndSpin, options.upperRefinementEnd);
    setDoubleValue(ui.lowerRefinementStartSpin,
                   options.lowerRefinementStart);
    setDoubleValue(ui.lowerRefinementEndSpin, options.lowerRefinementEnd);
    updatingControls_ = false;
  }

  void updateConfigurationControls(
      const AnalysisConfig& configuration) {
    updatingControls_ = true;
    setDoubleValue(ui.reynoldsSpin, configuration.reynoldsNumber);
    setDoubleValue(ui.machSpin, configuration.machNumber);
    setCheckState(ui.viscousCheck, configuration.viscous);
    setIntegerValue(ui.iterationsSpin,
                    configuration.maximumIterations);
    setDoubleValue(ui.ncritSpin, configuration.transition.nCritical);
    setDoubleValue(ui.xtrUpperSpin,
                   configuration.transition.topTripX);
    setDoubleValue(ui.xtrLowerSpin,
                   configuration.transition.bottomTripX);
    setDoubleValue(ui.viscousAccelerationSpin,
                   configuration.viscousAcceleration);
    setCheckState(ui.autoInitializeBoundaryLayerCheck,
                  configuration.autoInitializeBoundaryLayer);
    setComboData(ui.reynoldsDependencyCombo,
                 static_cast<int>(configuration.reynoldsDependency));
    setComboData(ui.machDependencyCombo,
                 static_cast<int>(configuration.machDependency));
    updatingControls_ = false;
  }

  void updateTargetControls() {
    updatingControls_ = true;
    const AnalysisMode mode = model_.mode();
    setComboIndex(ui.targetModeCombo, static_cast<int>(mode));
    if (std::holds_alternative<AlphaTarget>(model_.target())) {
      setDoubleValue(ui.alphaTargetSpin,
                     std::get<AlphaTarget>(model_.target()).alpha.value);
    }
    if (std::holds_alternative<LiftTarget>(model_.target())) {
      setDoubleValue(ui.clTargetSpin,
                     std::get<LiftTarget>(model_.target()).cl);
    }
    const AlphaSequence& sequence = model_.sequence();
    setDoubleValue(ui.sequenceStartSpin, sequence.start.value);
    setDoubleValue(ui.sequenceEndSpin, sequence.endExclusive.value);
    setDoubleValue(ui.sequenceStepSpin, sequence.step.value);
    const LiftSequence& clSequence = model_.clSequence();
    setDoubleValue(ui.clSequenceStartSpin, clSequence.start);
    setDoubleValue(ui.clSequenceEndSpin, clSequence.endExclusive);
    setDoubleValue(ui.clSequenceStepSpin, clSequence.step);
    updatingControls_ = false;
    updateTargetControlState();
  }

  void configureResultViews() {
    ui.cpPlot->setAxisTitles(QStringLiteral("x/c"), QStringLiteral("-Cp"));
    ui.cpAlphaPlot->setAxisTitles(QStringLiteral("x/c"),
                                  QStringLiteral("-Cp"));
    ui.cfAlphaPlot->setAxisTitles(QStringLiteral("s"),
                                  QStringLiteral("Cf"));

    ui.clAlphaPlot->setKind(GuiScalarPlotKind::clAlpha);
    ui.clCdPlot->setKind(GuiScalarPlotKind::clCd);
    ui.cmAlphaPlot->setKind(GuiScalarPlotKind::cmAlpha);
    ui.cmClPlot->setKind(GuiScalarPlotKind::cmCl);
    ui.efficiencyClPlot->setKind(GuiScalarPlotKind::efficiencyCl);

    for (ScalarPlotView* view :
         {ui.clAlphaPlot, ui.clCdPlot, ui.cmAlphaPlot, ui.cmClPlot,
          ui.efficiencyClPlot}) {
      view->setSelectionCallback(
          [this](std::size_t sourceIndex) {
            selectPointFromPlot(sourceIndex);
          });
    }
  }

  void updateResultMatrix() {
    ui.cdValue->setText(QString::fromStdString(
        formatResultValue(summary_.cd)));
    ui.cdfValue->setText(QString::fromStdString(
        formatResultValue(summary_.cdf)));
    ui.cdpValue->setText(QString::fromStdString(
        formatResultValue(summary_.cdp)));
    ui.clValue->setText(QString::fromStdString(
        formatResultValue(summary_.cl)));
    ui.cmValue->setText(QString::fromStdString(
        formatResultValue(summary_.cm)));
    ui.efficiencyValue->setText(QString::fromStdString(
        formatResultValue(summary_.efficiency)));
    ui.cd0Value->setText(QString::fromStdString(
        formatResultValue(summary_.cd0)));
    ui.cl0Value->setText(QString::fromStdString(
        formatResultValue(summary_.cl0)));
    ui.cm0Value->setText(QString::fromStdString(
        formatResultValue(summary_.cm0)));
    ui.alpha0Value->setText(QString::fromStdString(
        formatResultValue(summary_.alpha0)));
    ui.clMaxValue->setText(QString::fromStdString(
        formatResultValue(summary_.clMaximum)));
    ui.efficiencyMaxValue->setText(QString::fromStdString(
        formatResultValue(summary_.efficiencyMaximum)));

    const QString clHint =
        summary_.clMaximumProvisional && summary_.clMaximum.has_value()
            ? QString::fromStdString(std::string(kProvisionalClMaxHint))
            : QString();
    const QString efficiencyHint =
        summary_.efficiencyMaximumProvisional &&
                summary_.efficiencyMaximum.has_value()
            ? QString::fromStdString(
                  std::string(kProvisionalEfficiencyMaxHint))
            : QString();
    ui.cd0Value->setToolTip(QStringLiteral(
        "Minimum finite CD over enabled converged points."));
    ui.cl0Value->setToolTip(QStringLiteral(
        "CL at alpha = 0 (exact or interpolated)."));
    ui.cm0Value->setToolTip(QStringLiteral(
        "CM at zero lift, using the same interpolation pair as alpha0."));
    ui.alpha0Value->setToolTip(QStringLiteral(
        "Alpha where CL = 0, using the same pair as CM0; smallest "
        "absolute crossing when several exist."));
    ui.clMaxValue->setToolTip(
        clHint.isEmpty()
            ? QStringLiteral("Quadratic CL maximum between bounded "
                             "falling-flank samples.")
            : clHint);
    ui.efficiencyMaxValue->setToolTip(
        efficiencyHint.isEmpty()
            ? QStringLiteral("Quadratic (CL/CD) maximum between bounded "
                             "falling-flank samples.")
            : efficiencyHint);
  }

  void refreshPointSelection() {
    const QSignalBlocker operatingBlocker(ui.operatingPointCombo);
    const QSignalBlocker deactivateBlocker(ui.deactivatePointCombo);
    const QSignalBlocker activateBlocker(ui.activatePointCombo);
    ui.operatingPointCombo->clear();
    ui.deactivatePointCombo->clear();
    ui.activatePointCombo->clear();

    for (const GuiPointEntry& point : selectionModel_.enabledPoints()) {
      ui.operatingPointCombo->addItem(
          QString::fromStdString(formatOperatingPointLabel(point)),
          static_cast<qulonglong>(point.sourceIndex));
      ui.deactivatePointCombo->addItem(
          QString::fromStdString(formatOperatingPointLabel(point)),
          static_cast<qulonglong>(point.sourceIndex));
    }
    for (const GuiPointEntry& point : selectionModel_.disabledPoints()) {
      ui.activatePointCombo->addItem(
          QString::fromStdString(formatOperatingPointLabel(point)),
          static_cast<qulonglong>(point.sourceIndex));
    }

    ui.operatingPointCombo->setEnabled(
        ui.operatingPointCombo->count() > 0);
    ui.deactivatePointCombo->setEnabled(
        ui.deactivatePointCombo->count() > 0);
    ui.activatePointCombo->setEnabled(
        ui.activatePointCombo->count() > 0);

    const std::optional<GuiPointEntry> current =
        selectionModel_.currentPoint();
    if (current.has_value()) {
      const int index = ui.operatingPointCombo->findData(
          static_cast<qulonglong>(current->sourceIndex));
      if (index >= 0) {
        ui.operatingPointCombo->setCurrentIndex(index);
      }
    } else {
      ui.operatingPointCombo->setCurrentIndex(-1);
    }
    ui.deactivatePointCombo->setCurrentIndex(-1);
    ui.activatePointCombo->setCurrentIndex(-1);
    updatePointConvergence(current);
  }

  void updatePointConvergence(
      const std::optional<GuiPointEntry>& point) {
    GuiConvergenceState state = GuiConvergenceState::noPoint;
    if (point.has_value()) {
      switch (point->convergenceStatus) {
        case ConvergenceStatus::converged:
          state = GuiConvergenceState::converged;
          break;
        case ConvergenceStatus::cancelled:
          state = GuiConvergenceState::cancelled;
          break;
        case ConvergenceStatus::failed:
          state = GuiConvergenceState::failed;
          break;
        case ConvergenceStatus::notRun:
        case ConvergenceStatus::unconverged:
          state = point->converged
                      ? GuiConvergenceState::converged
                      : GuiConvergenceState::notConverged;
          break;
      }
    }
    const GuiConvergenceText convergence = formatConvergence(state);
    ui.currentPointConvergenceIcon->setText(
        QString::fromStdString(convergence.icon));
    ui.currentPointConvergenceLabel->setText(
        QString::fromStdString(convergence.label));
  }

  [[nodiscard]]
  GuiPointEntry pointEntryFromOperatingPoint(
      const OperatingPointData& point, std::size_t sourceIndex) const {
    GuiPointEntry entry;
    entry.sourceIndex = sourceIndex;
    entry.alpha = point.alpha.value;
    entry.converged = point.convergence == ConvergenceStatus::converged;
    entry.enabled = true;
    entry.convergenceStatus = point.convergence;
    entry.operatingPoint = point;
    entry.polar.alpha = point.alpha;
    entry.polar.liftCoefficient = point.liftCoefficient;
    entry.polar.dragCoefficient = point.dragCoefficient;
    entry.polar.dragCoefficientPressure = point.dragCoefficientPressure;
    entry.polar.momentCoefficient = point.momentCoefficient;
    entry.polar.convergence = point.convergence;
    entry.polar.diagnostics = point.diagnostics;
    entry.polar.dragCoefficientFriction =
        point.dragCoefficientFriction;
    return entry;
  }

  [[nodiscard]]
  GuiPointEntry pointEntryFromSequencePoint(
      const DetailedSequencePoint& point, std::size_t sourceIndex) const {
    GuiPointEntry entry;
    entry.sourceIndex = sourceIndex;
    entry.alpha = point.polar.alpha.value;
    entry.converged =
        point.polar.convergence == ConvergenceStatus::converged;
    entry.enabled = true;
    entry.convergenceStatus = point.polar.convergence;
    if (point.operatingPoint.has_value() &&
        point.operatingPoint->convergence != ConvergenceStatus::notRun) {
      entry.convergenceStatus = point.operatingPoint->convergence;
      entry.converged = entry.convergenceStatus ==
                        ConvergenceStatus::converged;
    }
    entry.polar = point.polar;
    entry.operatingPoint = point.operatingPoint;
    return entry;
  }

  void populateResultsFromCurrent() {
    std::vector<GuiPointEntry> points;
    if (currentOperatingPoint_.has_value()) {
      points.push_back(
          pointEntryFromOperatingPoint(*currentOperatingPoint_, 0));
    } else if (currentSequence_.has_value()) {
      points.reserve(currentSequence_->points.size());
      for (std::size_t index = 0;
           index < currentSequence_->points.size(); ++index) {
        points.push_back(pointEntryFromSequencePoint(
            currentSequence_->points[index], index));
      }
    }
    selectionModel_.setPoints(std::move(points));
    refreshResultPresentation(true);
  }

  void clearResultData() {
    selectionModel_.clear();
    summary_ = GuiResultSummary{};
    updateResultMatrix();
    refreshPointSelection();
    for (ScalarPlotView* view :
         {ui.clAlphaPlot, ui.clCdPlot, ui.cmAlphaPlot, ui.cmClPlot,
          ui.efficiencyClPlot}) {
      view->setPoints({}, std::nullopt, false);
    }
    ui.cpPlot->clear();
    ui.cpAlphaPlot->clear();
    ui.cfAlphaPlot->clear();
  }

  void refreshResultPresentation(bool fitPlots) {
    const std::optional<GuiPointEntry> selected =
        selectionModel_.currentPoint();
    summary_ =
        computeGuiResultSummary(selectionModel_.points(), selected);
    updateResultMatrix();
    refreshPointSelection();

    const std::optional<std::size_t> selectedIndex =
        selectionModel_.currentSourceIndex();
    for (ScalarPlotView* view :
         {ui.clAlphaPlot, ui.clCdPlot, ui.cmAlphaPlot, ui.cmClPlot,
          ui.efficiencyClPlot}) {
      view->setPoints(selectionModel_.points(), selectedIndex, fitPlots);
    }

    if (selected.has_value() && selected->operatingPoint.has_value()) {
      const OperatingPointData& point = *selected->operatingPoint;
      if (point.surface.has_value()) {
        ui.cpPlot->setCpDistribution(*point.surface);
        ui.cpAlphaPlot->setCpDistribution(*point.surface);
      } else {
        ui.cpPlot->clear();
        ui.cpAlphaPlot->clear();
      }

      if (point.viscous && point.skinFriction.has_value()) {
        ui.cfAlphaPlot->setCfDistribution(*point.skinFriction);
      } else {
        ui.cfAlphaPlot->clear();
        if (!point.viscous) {
          ui.cfAlphaPlot->setMessage(
              QStringLiteral(
                  "No skin-friction distribution for inviscid point"));
        }
      }
    } else {
      ui.cpPlot->clear();
      ui.cpAlphaPlot->clear();
      ui.cfAlphaPlot->clear();
    }
    refreshUi();
  }

  void selectPointFromPlot(std::size_t sourceIndex) {
    if (!selectionModel_.select(sourceIndex)) {
      return;
    }
    refreshResultPresentation(false);
  }

  void operatingPointChanged(int index) {
    if (index < 0) {
      return;
    }
    const std::size_t sourceIndex = static_cast<std::size_t>(
        ui.operatingPointCombo->itemData(index).toULongLong());
    if (selectionModel_.select(sourceIndex)) {
      refreshResultPresentation(false);
    }
  }

  void deactivatePointChanged(int index) {
    if (index < 0) {
      return;
    }
    const std::size_t sourceIndex = static_cast<std::size_t>(
        ui.deactivatePointCombo->itemData(index).toULongLong());
    if (selectionModel_.deactivate(sourceIndex)) {
      refreshResultPresentation(false);
    } else {
      refreshPointSelection();
    }
  }

  void activatePointChanged(int index) {
    if (index < 0) {
      return;
    }
    const std::size_t sourceIndex = static_cast<std::size_t>(
        ui.activatePointCombo->itemData(index).toULongLong());
    if (selectionModel_.activate(sourceIndex)) {
      refreshResultPresentation(false);
    } else {
      refreshPointSelection();
    }
  }

  void refreshUi() {
    const XFoilGuiState state = this->state();
    const bool ready = model_.isReady();
    const bool running = tracker_.isRunning();

    ui.workflowStateLabel->setText(
        QStringLiteral("Workflow: %1")
            .arg(QString::fromLatin1(toString(state))));
    ui.statusLabel->setText(ready ? QStringLiteral("Ready to run")
                                  : QStringLiteral("Not ready"));
    ui.analysisStatusLabel->setText(analysisStatusText_);

    QString diagnostics;
    if (setupError_.has_value()) {
      diagnostics = formatError(*setupError_);
    } else if (model_.error().has_value()) {
      diagnostics = formatError(*model_.error());
    }
    const auto appendDiagnostic = [&diagnostics](const QString& line) {
      if (line.isEmpty()) {
        return;
      }
      if (!diagnostics.isEmpty()) {
        diagnostics += QLatin1Char('\n');
      }
      diagnostics += line;
    };
    for (const QString& line : analysisDiagnostics_) {
      appendDiagnostic(line);
    }
    const std::optional<GuiPointEntry> selectedPoint =
        selectionModel_.currentPoint();
    if (selectedPoint.has_value() &&
        selectedPoint->convergenceStatus != ConvergenceStatus::converged) {
      appendDiagnostic(QStringLiteral(
          "Selected point is not converged. Direct values are shown; "
          "derived summary values exclude it."));
    }
    if (summary_.clMaximumProvisional &&
        summary_.clMaximum.has_value()) {
      appendDiagnostic(
          QString::fromStdString(std::string(kProvisionalClMaxHint)));
    }
    if (summary_.efficiencyMaximumProvisional &&
        summary_.efficiencyMaximum.has_value()) {
      appendDiagnostic(QString::fromStdString(
          std::string(kProvisionalEfficiencyMaxHint)));
    }
    ui.setupDiagnosticsView->setPlainText(diagnostics);

    ui.analyzeButton->setEnabled(ready && !running);
    ui.cancelButton->setEnabled(running);
    ui.definitionsScrollArea->setEnabled(!running);
    if (analyzeAction_ != nullptr) {
      analyzeAction_->setEnabled(ready && !running);
    }
    if (cancelAction_ != nullptr) {
      cancelAction_->setEnabled(running);
    }
    refreshProgressBar();

    if (model_.geometry().has_value()) {
      const std::string_view name = model_.geometry()->name();
      ui.geometryNameLabel->setText(
          name.empty()
              ? QStringLiteral("Unnamed geometry")
              : QString::fromStdString(std::string(name)));
      ui.geometryStatusLabel->setText(
          model_.geometrySource() == GeometrySource::naca
              ? QStringLiteral("NACA geometry ready")
              : model_.geometrySource() == GeometrySource::nacs
                    ? QStringLiteral("NACS geometry ready")
                    : QStringLiteral("Memory geometry ready"));
    } else {
      ui.geometryNameLabel->setText(QStringLiteral("No geometry"));
      ui.geometryStatusLabel->setText(QStringLiteral("Not set"));
    }

    if (model_.panel().has_value()) {
      ui.panelStatusLabel->setText(
          QStringLiteral("%1 panel points")
              .arg(static_cast<qulonglong>(model_.panel()->points.size())));
    } else {
      ui.panelStatusLabel->setText(
          model_.geometry().has_value()
              ? QStringLiteral("Not generated")
              : QStringLiteral("No geometry"));
    }

    ui.targetStatusLabel->setText(targetSummary(model_));
    updateTargetControlState();

    if (state != lastState_) {
      lastState_ = state;
      Q_EMIT owner_->stateChanged(state);
    }
    if (ready != lastReady_) {
      lastReady_ = ready;
      Q_EMIT owner_->validationChanged(ready);
    }
  }

  XFoilWidget* owner_{};
  Ui::XFoilWidget ui;
  GuiDefinitionModel model_;
  GuiResultSelectionModel selectionModel_;
  GuiResultSummary summary_;
  QThread* thread_{nullptr};
  GuiAnalysisWorker* worker_{nullptr};
  GuiJobTracker tracker_;
  QAction* analyzeAction_{nullptr};
  QAction* cancelAction_{nullptr};
  std::shared_ptr<CancellationToken> activeToken_;
  std::optional<OperatingPointData> currentOperatingPoint_;
  std::optional<DetailedSequenceResult> currentSequence_;
  std::vector<QString> analysisDiagnostics_;
  std::optional<double> progressFraction_;
  std::optional<Error> setupError_;
  AnalysisWorkflowViewOptions viewOptions_;
  AnalysisOutcome analysisOutcome_{AnalysisOutcome::none};
  QString analysisStatusText_{QStringLiteral("Idle")};
  bool updatingControls_{false};
  bool workerReady_{false};
  bool shutdown_{false};
  XFoilGuiState lastState_{XFoilGuiState::empty};
  bool lastReady_{false};
};

XFoilWidget::XFoilWidget(QWidget* parent)
    : QWidget(parent), impl_(std::make_unique<Impl>(this)) {
  impl_->initialize();
}

XFoilWidget::~XFoilWidget() = default;

Result<void> XFoilWidget::setGeometry(
    const geometry::AirfoilGeometry& geometry) {
  return impl_->setGeometry(geometry);
}

Result<void> XFoilWidget::setNaca(std::string_view designation) {
  return impl_->setNaca(designation, false);
}

Result<void> XFoilWidget::setNacs(std::string_view designation) {
  return impl_->setNaca(designation, true);
}

void XFoilWidget::setPanelingOptions(const PanelingOptions& options) {
  impl_->setPanelingOptions(options);
}

Result<SurfaceGeometry> XFoilWidget::generatePanel(
    const PanelingOptions& options) {
  return impl_->generatePanel(options);
}

Result<void> XFoilWidget::setConfiguration(
    const AnalysisConfig& configuration) {
  return impl_->setConfiguration(configuration);
}

void XFoilWidget::setTarget(const AnalysisTarget& target) {
  impl_->setTarget(target);
}

void XFoilWidget::setSequence(const AlphaSequence& sequence) {
  impl_->setSequence(sequence);
}

void XFoilWidget::setClSequence(const LiftSequence& sequence) {
  impl_->setClSequence(sequence);
}

Result<void> XFoilWidget::applySetup(const AnalysisWorkflowSetup& setup) {
  return impl_->applySetup(setup);
}

Result<void> XFoilWidget::resetWorkflow() {
  return impl_->resetWorkflow();
}

void XFoilWidget::setViewOptions(
    const AnalysisWorkflowViewOptions& options) {
  impl_->setViewOptions(options);
}

AnalysisWorkflowViewOptions XFoilWidget::viewOptions() const {
  return impl_->viewOptions();
}

std::optional<AnalysisWorkflowSetup> XFoilWidget::currentSetup() const {
  return impl_->currentSetup();
}

std::optional<geometry::AirfoilGeometry> XFoilWidget::geometry() const {
  return impl_->geometry();
}

std::optional<SurfaceGeometry> XFoilWidget::panelGeometry() const {
  return impl_->panelGeometry();
}

PanelingOptions XFoilWidget::panelingOptions() const {
  return impl_->panelingOptions();
}

AnalysisConfig XFoilWidget::configuration() const {
  return impl_->configuration();
}

AnalysisTarget XFoilWidget::target() const {
  return impl_->target();
}

AlphaSequence XFoilWidget::sequence() const {
  return impl_->sequence();
}

LiftSequence XFoilWidget::clSequence() const {
  return impl_->clSequence();
}

XFoilGuiState XFoilWidget::state() const {
  return impl_->state();
}

bool XFoilWidget::isReadyToAnalyze() const {
  return impl_->isReadyToAnalyze();
}

void XFoilWidget::analyze() {
  impl_->analyze();
}

void XFoilWidget::cancel() {
  impl_->cancel();
}

bool XFoilWidget::isRunning() const {
  return impl_->isRunning();
}

QAction* XFoilWidget::analyzeAction() {
  return impl_->analyzeAction();
}

QAction* XFoilWidget::cancelAction() {
  return impl_->cancelAction();
}

std::optional<OperatingPointData> XFoilWidget::currentOperatingPoint() const {
  return impl_->currentOperatingPoint();
}

std::optional<DetailedSequenceResult> XFoilWidget::currentSequence() const {
  return impl_->currentSequence();
}

}  // namespace xfoil::gui

// End: src/gui/XFoilWidget.cpp

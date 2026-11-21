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
// G2 definition surface. Programmatic setters and visible controls share
// this single controller path. The widget owns a backend session but does
// not run an analysis, create an application or touch the file system.

#include "xfoil/gui/XFoilWidget.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>

#include "backend/GuiSessionBackend.h"
#include "model/GuiDefinitionModel.h"
#include "model/GuiResultSelectionModel.h"
#include "model/GuiResultSummary.h"
#include "plots/GeometryPlotView.h"
#include "plots/GuiPointPresentation.h"
#include "plots/ResultPlotView.h"
#include "ui_XFoilWidget.h"
#include "xfoil/Limits.h"

namespace xfoil::gui {
namespace {

void ensureGuiStateMetaType() {
  static const int registered =
      qRegisterMetaType<XFoilGuiState>("XFoilGuiState");
  (void)registered;
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
  return configuration;
}

AlphaSequence sequenceFromControls(const Ui::XFoilWidget& ui) {
  return AlphaSequence{Degrees{ui.sequenceStartSpin->value()},
                       Degrees{ui.sequenceEndSpin->value()},
                       Degrees{ui.sequenceStepSpin->value()}};
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
    ensureGuiStateMetaType();
    ui.setupUi(owner_);
    populateChoiceControls();
    initializeControlValues();
    connectControls();
  }

  void initialize() {
    configureResultViews();
    updateResultMatrix();
    (void)owner_->setConfiguration(configurationFromControls(ui));
    refreshPointSelection();
    refreshUi();
  }

  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry) {
    const Result<void> result = backend_.setGeometry(geometry);
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
    const Result<geometry::AirfoilGeometry> result =
        backend_.setNaca(designation, sharpTrailingEdge);
    if (result.isErr()) {
      model_.fail(result.error());
      refreshUi();
      return Result<void>::fail(result.error());
    }

    geometry::AirfoilGeometry geometry = result.value();
    const bool hadPanel = model_.panel().has_value();
    model_.acceptNaca(geometry, std::string(designation), sharpTrailingEdge);
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
    model_.setPanelingOptions(options);
    updatePanelingControls(options);
    ui.airfoilPlot->clearPanel();
    refreshUi();
    Q_EMIT owner_->panelingChanged();
  }

  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(const PanelingOptions& options) {
    const Result<SurfaceGeometry> result = backend_.generatePanel(options);
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
    const Result<void> result = backend_.setConfiguration(configuration);
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
    const Result<void> result = model_.setTarget(target);
    if (result.isOk()) {
      updateTargetControls();
    }
    refreshUi();
    Q_EMIT owner_->targetChanged();
  }

  void setSequence(const AlphaSequence& sequence) {
    const Result<void> result = model_.setSequence(sequence);
    if (result.isOk()) {
      updateTargetControls();
    }
    refreshUi();
    Q_EMIT owner_->targetChanged();
  }

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
  XFoilGuiState state() const {
    return model_.state();
  }

  [[nodiscard]]
  bool isReadyToAnalyze() const {
    return model_.isReady();
  }

 private:
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
    setIntegerValue(ui.panelCountSpin,
                    static_cast<int>(paneling.panelCount));
    setDoubleValue(ui.bunchingSpin, paneling.bunchingParameter);
    setDoubleValue(ui.teToLeSpin, paneling.teToLeDensityRatio);
    setDoubleValue(ui.refinementSpin, paneling.refinementDensityRatio);

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
    setDoubleValue(ui.reynoldsSpin, configuration.reynoldsNumber);
    setDoubleValue(ui.machSpin, configuration.machNumber);
    setCheckState(ui.viscousCheck, configuration.viscous);
    setIntegerValue(ui.iterationsSpin, configuration.maximumIterations);
    setDoubleValue(ui.ncritSpin, configuration.transition.nCritical);
    setDoubleValue(ui.xtrUpperSpin, configuration.transition.topTripX);
    setDoubleValue(ui.xtrLowerSpin, configuration.transition.bottomTripX);

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

    const AlphaSequence& sequence = model_.sequence();
    setDoubleValue(ui.alphaTargetSpin, 0.0);
    setDoubleValue(ui.clTargetSpin, 0.0);
    setDoubleValue(ui.sequenceStartSpin, sequence.start.value);
    setDoubleValue(ui.sequenceEndSpin, sequence.endExclusive.value);
    setDoubleValue(ui.sequenceStepSpin, sequence.step.value);
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
         {ui.bunchingSpin, ui.teToLeSpin, ui.refinementSpin}) {
      connect(spin, &QDoubleSpinBox::valueChanged, owner_,
              [this](double) { panelingControlsChanged(); });
    }
    QObject::connect(ui.panelCountSpin, &QSpinBox::valueChanged, owner_,
            [this](int) { panelingControlsChanged(); });
    QObject::connect(ui.generatePanelButton, &QPushButton::clicked, owner_,
            [this]() { generatePanelFromControls(); });
    QObject::connect(ui.showPanelNodesCheck, &QCheckBox::toggled, owner_,
            [this](bool checked) {
              ui.airfoilPlot->showPanelNodes(checked);
            });

    for (QDoubleSpinBox* spin :
         {ui.reynoldsSpin, ui.machSpin, ui.ncritSpin, ui.xtrUpperSpin,
          ui.xtrLowerSpin}) {
      connect(spin, &QDoubleSpinBox::valueChanged, owner_,
              [this](double) { configurationControlsChanged(); });
    }
    QObject::connect(ui.iterationsSpin, &QSpinBox::valueChanged, owner_,
            [this](int) { configurationControlsChanged(); });
    QObject::connect(ui.viscousCheck, &QCheckBox::toggled, owner_,
            [this](bool) { configurationControlsChanged(); });

    QObject::connect(ui.targetModeCombo, &QComboBox::currentIndexChanged, owner_,
            [this](int) { targetModeChanged(); });
    for (QDoubleSpinBox* spin :
         {ui.alphaTargetSpin, ui.clTargetSpin, ui.sequenceStartSpin,
          ui.sequenceEndSpin, ui.sequenceStepSpin}) {
      connect(spin, &QDoubleSpinBox::valueChanged, owner_,
              [this](double) { targetFieldsChanged(); });
    }
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
    updatingControls_ = false;
    updateTargetControlState();
  }

  void configureResultViews() {
    ui.cpPlot->setCpMode(true);
    ui.cpPlot->setAxisTitles(QStringLiteral("x/c"), QStringLiteral("-Cp"));
    ui.clAlphaPlot->setAxisTitles(QStringLiteral("α [deg]"), QStringLiteral("CL"));
    ui.clCdPlot->setAxisTitles(QStringLiteral("CD"), QStringLiteral("CL"));
    ui.cmAlphaPlot->setAxisTitles(QStringLiteral("α [deg]"), QStringLiteral("CM"));
    ui.cmClPlot->setAxisTitles(QStringLiteral("CL"), QStringLiteral("CM"));
    ui.efficiencyClPlot->setAxisTitles(QStringLiteral("CL"),
                                       QStringLiteral("CL/CD"));
    ui.cpAlphaPlot->setAxisTitles(QStringLiteral("x/c"),
                                  QStringLiteral("-Cp"));
    ui.cfAlphaPlot->setAxisTitles(QStringLiteral("x/c or s"),
                                  QStringLiteral("Cf"));
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
  }

  void refreshPointSelection() {
    const QSignalBlocker operatingBlocker(ui.operatingPointCombo);
    const QSignalBlocker deactivateBlocker(ui.deactivatePointCombo);
    const QSignalBlocker activateBlocker(ui.activatePointCombo);
    ui.operatingPointCombo->clear();
    ui.deactivatePointCombo->clear();
    ui.activatePointCombo->clear();

    for (const GuiPointEntry& point : selectionModel_.activePoints()) {
      ui.operatingPointCombo->addItem(
          QString::fromStdString(formatOperatingPointLabel(point)),
          static_cast<qulonglong>(point.sourceIndex));
      ui.deactivatePointCombo->addItem(
          QString::fromStdString(formatOperatingPointLabel(point)),
          static_cast<qulonglong>(point.sourceIndex));
    }
    for (const GuiPointEntry& point : selectionModel_.inactivePoints()) {
      ui.activatePointCombo->addItem(
          QString::fromStdString(formatOperatingPointLabel(point)),
          static_cast<qulonglong>(point.sourceIndex));
    }

    ui.operatingPointCombo->setEnabled(ui.operatingPointCombo->count() > 0);
    ui.deactivatePointCombo->setEnabled(
        ui.deactivatePointCombo->count() > 0);
    ui.activatePointCombo->setEnabled(ui.activatePointCombo->count() > 0);

    if (const std::optional<GuiPointEntry> current =
            selectionModel_.currentPoint();
        current.has_value()) {
      const int index = ui.operatingPointCombo->findData(
          static_cast<qulonglong>(current->sourceIndex));
      if (index >= 0) {
        ui.operatingPointCombo->setCurrentIndex(index);
      }
      const GuiConvergenceText convergence = formatConvergence(
          current->converged ? GuiConvergenceState::converged
                             : GuiConvergenceState::notConverged);
      ui.currentPointConvergenceIcon->setText(
          QString::fromStdString(convergence.icon));
      ui.currentPointConvergenceLabel->setText(
          QString::fromStdString(convergence.label));
    } else {
      const GuiConvergenceText convergence =
          formatConvergence(GuiConvergenceState::noPoint);
      ui.currentPointConvergenceIcon->setText(
          QString::fromStdString(convergence.icon));
      ui.currentPointConvergenceLabel->setText(
          QString::fromStdString(convergence.label));
    }
  }

  void refreshUi() {
    const XFoilGuiState state = model_.state();
    const bool ready = model_.isReady();

    ui.workflowStateLabel->setText(
        QStringLiteral("Workflow: %1")
            .arg(QString::fromLatin1(toString(state))));
    ui.statusLabel->setText(ready ? QStringLiteral("Ready to run")
                                  : QStringLiteral("Not ready"));

    if (model_.error().has_value()) {
      ui.setupDiagnosticsView->setPlainText(
          formatError(*model_.error()));
    } else {
      ui.setupDiagnosticsView->clear();
    }
    ui.analyzeButton->setEnabled(ready);

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
  GuiSessionBackend backend_;
  GuiDefinitionModel model_;
  GuiResultSelectionModel selectionModel_;
  GuiResultSummary summary_;
  bool updatingControls_{false};
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

XFoilGuiState XFoilWidget::state() const {
  return impl_->state();
}

bool XFoilWidget::isReadyToAnalyze() const {
  return impl_->isReadyToAnalyze();
}

}  // namespace xfoil::gui

// End: src/gui/XFoilWidget.cpp

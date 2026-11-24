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

// File: include/xfoil/gui/XFoilWidget.h
// Public embeddable GUI surface. Setup is validated and snapshotted in the
// widget model and applied to one worker-owned xfoil::Session. Analysis runs
// asynchronously in exactly one worker thread; results are owning values.
// Definition setters, analyze(), cancel() and the getters are widget-thread
// operations.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>


#include <QMetaType>
#include <QWidget>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/PolarData.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/gui/AnalysisWorkflowSetup.h"
#include "xfoil/gui/Export.h"
#include "xfoil/gui/GuiState.h"

class QAction;

namespace xfoil::gui {

class XFOIL_GUI_API XFoilWidget : public QWidget {
  Q_OBJECT

 public:
  explicit XFoilWidget(QWidget* parent = nullptr);
  ~XFoilWidget() override;

  XFoilWidget(const XFoilWidget&) = delete;
  XFoilWidget& operator=(const XFoilWidget&) = delete;
  XFoilWidget(XFoilWidget&&) = delete;
  XFoilWidget& operator=(XFoilWidget&&) = delete;

  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry);
  // App-owned import transaction: validates the supplied unpaneled geometry,
  // regenerates the panel with the current options and replaces the setup
  // atomically. No analysis is started. The notice is view-only.
  [[nodiscard]]
  Result<void> applyImportedGeometry(
      const geometry::AirfoilGeometry& geometry, bool clockwiseReversed);
  // Restores the source radio selection from the committed model after a
  // canceled or failed import dialog. No dialog is opened by the widget.
  void cancelAirfoilImportRequest();
  [[nodiscard]]
  Result<void> setNaca(std::string_view designation);
  [[nodiscard]]
  Result<void> setNacs(std::string_view designation);

  void setPanelingOptions(const PanelingOptions& options);
  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(const PanelingOptions& options);
  [[nodiscard]]
  Result<void> setConfiguration(const AnalysisConfig& configuration);

  void setTarget(const AnalysisTarget& target);
  void setSequence(const AlphaSequence& sequence);
  void setClSequence(const LiftSequence& sequence);

  // Applies one complete owning setup transaction in the worker. The
  // active session is replaced only after geometry, paneling, configuration
  // and request all validate. The call is synchronous and works before
  // show() or any event-loop iteration. It never starts an analysis.
  [[nodiscard]]
  Result<void> applySetup(const AnalysisWorkflowSetup& setup);
  // Replaces the active worker session with an empty session and clears the
  // workflow to the empty state. Host defaults are not applied.
  [[nodiscard]]
  Result<void> resetWorkflow();
  // View-only options are instance-local and may change during analysis.
  void setViewOptions(const AnalysisWorkflowViewOptions& options);
  [[nodiscard]]
  AnalysisWorkflowViewOptions viewOptions() const;

  [[nodiscard]]
  std::optional<AnalysisWorkflowSetup> currentSetup() const;

  // Explicit comparison-polar operations. The current result is not stored
  // automatically. At most eight comparison polars are kept per widget.
  [[nodiscard]]
  Result<std::uint64_t> addComparisonPolar(const PolarData& polar);
  [[nodiscard]]
  Result<std::uint64_t> addCurrentComparisonPolar();
  [[nodiscard]]
  std::optional<PolarData> currentPolar() const;
  [[nodiscard]]
  std::optional<PolarData> selectedComparisonPolar() const;

  // Memory-only report/export projections. Paths, dialogs and file writes
  // belong to the application host.
  [[nodiscard]]
  std::optional<PressureExportData> selectedPressureExport() const;
  [[nodiscard]]
  bool hasCurrentCp() const noexcept;
  [[nodiscard]]
  bool hasActivePlot() const noexcept;
  [[nodiscard]]
  Result<std::string> activePlotPng() const;
  [[nodiscard]]
  Result<std::string> activePlotPdf() const;
  [[nodiscard]]
  Result<std::string> activePlotSvg() const;

  [[nodiscard]]
  std::optional<geometry::AirfoilGeometry> geometry() const;
  [[nodiscard]]
  std::optional<SurfaceGeometry> panelGeometry() const;
  [[nodiscard]]
  PanelingOptions panelingOptions() const;
  [[nodiscard]]
  AnalysisConfig configuration() const;
  [[nodiscard]]
  AnalysisTarget target() const;
  [[nodiscard]]
  AlphaSequence sequence() const;
  [[nodiscard]]
  LiftSequence clSequence() const;
  [[nodiscard]]
  XFoilGuiState state() const;
  [[nodiscard]]
  bool isReadyToAnalyze() const;

  // Starts one alpha, CL, detailed alpha-sequence or detailed CL-sequence
  // analysis in the widget-owned worker. A call while not ready or while another analysis
  // is active is a deterministic no-op.
  void analyze();
  // Requests cancellation of the active job through its atomic token. A
  // call outside an active job is an idempotent no-op.
  void cancel();
  [[nodiscard]]
  bool isRunning() const;

  [[nodiscard]]
  QAction* analyzeAction();
  [[nodiscard]]
  QAction* cancelAction();

  // Owning copies of the current analysis result. A setup change or a new
  // analysis clears the current values. No widget or session reference is
  // returned.
  [[nodiscard]]
  std::optional<xfoil::OperatingPointData> currentOperatingPoint() const;
  [[nodiscard]]
  std::optional<xfoil::DetailedSequenceResult> currentSequence() const;

 signals:
  void airfoilImportRequested();
  void geometryChanged();
  void panelingChanged();
  void configurationChanged();
  void targetChanged();
  void setupApplied();
  void setupFailed(xfoil::Error error);
  void comparisonPolarsChanged();
  void activePlotChanged();
  void resultSelectionChanged();
  void stateChanged(XFoilGuiState state);
  void validationChanged(bool valid);
  void analysisStarted();
  void progressChanged(xfoil::ProgressEvent event);
  void operatingPointReady(xfoil::OperatingPointData point);
  void sequenceReady(xfoil::DetailedSequenceResult sequence);
  void analysisCancelled();
  void analysisFailed(xfoil::Error error);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xfoil::gui

Q_DECLARE_METATYPE(xfoil::gui::XFoilGuiState)
Q_DECLARE_METATYPE(xfoil::ProgressEvent)
Q_DECLARE_METATYPE(xfoil::OperatingPointData)
Q_DECLARE_METATYPE(xfoil::DetailedSequenceResult)
Q_DECLARE_METATYPE(xfoil::Error)

// End: include/xfoil/gui/XFoilWidget.h

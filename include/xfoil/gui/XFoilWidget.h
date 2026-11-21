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

#include <memory>
#include <optional>
#include <string_view>

#include <QMetaType>
#include <QWidget>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/AnalysisRequest.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/Session.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"
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
  XFoilGuiState state() const;
  [[nodiscard]]
  bool isReadyToAnalyze() const;

  // Starts one alpha, CL or detailed alpha-sequence analysis in the
  // widget-owned worker. A call while not ready or while another analysis
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
  void geometryChanged();
  void panelingChanged();
  void configurationChanged();
  void targetChanged();
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

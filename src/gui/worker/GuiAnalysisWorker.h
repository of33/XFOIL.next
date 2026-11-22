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

// File: src/gui/worker/GuiAnalysisWorker.h
// Worker-owned single xfoil::Session. All setup and solve work happens on
// the worker thread. Results cross the thread boundary as owning values;
// the widget never receives a session or result reference.

#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include <QMetaType>
#include <QObject>

#include "model/GuiRunSpec.h"
#include "xfoil/AnalysisData.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/gui/AnalysisWorkflowSetup.h"
#include "xfoil/Progress.h"
#include "xfoil/Result.h"
#include "xfoil/SequenceResult.h"
#include "xfoil/SessionOptions.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil {
class Session;
}

namespace xfoil::gui {

enum class GuiAnalysisFinish {
  completed,
  cancelled,
  failed
};

struct GuiAppliedSetup {
  geometry::AirfoilGeometry geometry;
  SurfaceGeometry panel;
};

struct GuiAnalysisJob {
  GuiRunSpec spec;
  qulonglong jobId{};
  quint64 generationId{};
  std::shared_ptr<CancellationToken> cancellation;
  ProgressCallback progress;
};

class GuiAnalysisWorker : public QObject {
  Q_OBJECT

 public:
  explicit GuiAnalysisWorker(QObject* parent = nullptr);
  ~GuiAnalysisWorker() override;

  GuiAnalysisWorker(const GuiAnalysisWorker&) = delete;
  GuiAnalysisWorker& operator=(const GuiAnalysisWorker&) = delete;

  [[nodiscard]]
  Result<void> initialize();

  [[nodiscard]]
  Result<void> setGeometry(const geometry::AirfoilGeometry& geometry);
  [[nodiscard]]
  Result<geometry::AirfoilGeometry> setNaca(
      std::string_view designation, bool sharpTrailingEdge);
  [[nodiscard]]
  Result<SurfaceGeometry> generatePanel(
      const PanelingOptions& options);
  [[nodiscard]]
  Result<void> setConfiguration(
      const AnalysisConfig& configuration);

  // Builds and validates a transient candidate session, then replaces the
  // active worker session only after complete success. The candidate is
  // never analyzed. The returned geometry and panel are owning previews.
  [[nodiscard]]
  Result<GuiAppliedSetup> applySetup(
      const AnalysisWorkflowSetup& setup);
  [[nodiscard]]
  Result<void> resetSession();

 public slots:
  void run(GuiAnalysisJob job);

 signals:
  void progressReady(qulonglong jobId, quint64 generationId,
                     xfoil::ProgressEvent event);
  void operatingPointReady(qulonglong jobId, quint64 generationId,
                           xfoil::OperatingPointData point);
  void sequenceReady(qulonglong jobId, quint64 generationId,
                     xfoil::DetailedSequenceResult sequence);
  void finished(qulonglong jobId, quint64 generationId,
                xfoil::gui::GuiAnalysisFinish finish, xfoil::Error error);

 private:
  [[nodiscard]]
  Result<void> requireSession() const;

  std::unique_ptr<Session> session_;
};

}  // namespace xfoil::gui

Q_DECLARE_METATYPE(xfoil::gui::GuiAnalysisFinish)

// End: src/gui/worker/GuiAnalysisWorker.h

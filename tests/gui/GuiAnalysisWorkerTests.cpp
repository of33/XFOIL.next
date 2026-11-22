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

// File: tests/gui/GuiAnalysisWorkerTests.cpp
// Direct worker contract and job/generation identity tests. The worker is
// exercised without a QThread so callback failure and cancellation are
// deterministic.

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QSignalSpy>
#include <QtTest/QTest>

#include "xfoil/gui/AnalysisWorkflowSetup.h"
#include "xfoil/gui/XFoilWidget.h"
#include "model/GuiDefinitionModel.h"
#include "worker/GuiAnalysisWorker.h"
#include "xfoil/AnalysisData.h"
#include "worker/GuiJobTracker.h"

namespace {

using xfoil::AlphaTarget;
using xfoil::AnalysisConfig;
using xfoil::CancellationToken;
using xfoil::Degrees;
using xfoil::PanelingOptions;
using xfoil::ProgressEvent;
using xfoil::ProgressPhase;
using xfoil::gui::GuiAnalysisFinish;
using xfoil::gui::GuiAnalysisJob;
using xfoil::gui::AnalysisWorkflowSetup;
using xfoil::gui::GuiAnalysisWorker;
using xfoil::gui::NacaGeometrySetup;
using xfoil::gui::GuiDefinitionModel;
using xfoil::gui::GuiJobTracker;
using xfoil::gui::GuiRunSpec;
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

xfoil::SurfaceGeometry panelFromDiamond() {
  const AirfoilGeometry geometry = diamond();
  xfoil::SurfaceGeometry panel;
  const auto coordinates = geometry.coordinates();
  panel.points.assign(coordinates.begin(), coordinates.end());
  return panel;
}

GuiRunSpec pointSpec() {
  GuiDefinitionModel model;
  model.acceptGeometry(diamond());
  model.acceptPanel(panelFromDiamond(), PanelingOptions{});
  model.acceptConfiguration(AnalysisConfig{});
  const auto target = model.setTarget(AlphaTarget{Degrees{2.0}});
  Q_ASSERT(target.isOk());
  const auto spec = model.makeRunSpec();
  Q_ASSERT(spec.isOk());
  return spec.value();
}

}  // namespace

class GuiAnalysisWorkerTests : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase() {
    qRegisterMetaType<xfoil::Error>("xfoil::Error");
    qRegisterMetaType<GuiAnalysisFinish>("xfoil::gui::GuiAnalysisFinish");
    qRegisterMetaType<xfoil::OperatingPointData>(
        "xfoil::OperatingPointData");
    qRegisterMetaType<xfoil::DetailedSequenceResult>(
        "xfoil::DetailedSequenceResult");
  }

  void uninitializedWorkerReportsStructuredFailure() {
    GuiAnalysisWorker worker;
    GuiAnalysisJob job{pointSpec(), 9, 2,
                       std::make_shared<CancellationToken>(), {}};
    QSignalSpy finished(&worker, &GuiAnalysisWorker::finished);
    worker.run(job);
    QCOMPARE(finished.count(), 1);
    const QList<QVariant> args = finished.takeFirst();
    QVERIFY(args.at(2).value<GuiAnalysisFinish>() ==
            GuiAnalysisFinish::failed);
    const auto error = qvariant_cast<xfoil::Error>(args.at(3));
    QCOMPARE(QString::fromStdString(error.code),
             QStringLiteral("gui.workerUnavailable"));
  }

  void alphaAndCallbackFailureHaveDistinctOutcomes() {
    GuiAnalysisWorker worker;
    QVERIFY(worker.initialize().isOk());
    QVERIFY(worker.setNaca("0012", false).isOk());
    QVERIFY(worker.generatePanel(PanelingOptions{}).isOk());
    QVERIFY(worker.setConfiguration(AnalysisConfig{}).isOk());
    const GuiRunSpec spec = pointSpec();

    QSignalSpy pointSpy(&worker, &GuiAnalysisWorker::operatingPointReady);
    QSignalSpy finished(&worker, &GuiAnalysisWorker::finished);
    GuiAnalysisJob job{spec, 41, 7,
                       std::make_shared<CancellationToken>(), {}};
    worker.run(std::move(job));
    QCOMPARE(pointSpy.count(), 1);
    QCOMPARE(finished.count(), 1);
    QList<QVariant> args = finished.takeFirst();
    QVERIFY(args.at(2).value<GuiAnalysisFinish>() ==
            GuiAnalysisFinish::completed);

    GuiAnalysisJob failing{spec, 42, 7,
                           std::make_shared<CancellationToken>(), {}};
    failing.progress = [](const ProgressEvent&) {
      throw std::runtime_error("progress failure");
    };
    worker.run(std::move(failing));
    QCOMPARE(finished.count(), 1);
    args = finished.takeFirst();
    QVERIFY(args.at(2).value<GuiAnalysisFinish>() ==
            GuiAnalysisFinish::failed);
    const auto error = qvariant_cast<xfoil::Error>(args.at(3));
    QCOMPARE(QString::fromStdString(error.code),
             QStringLiteral("progress.callbackFailure"));
  }

  void cancellationDuringSequenceKeepsPartialResult() {
    GuiAnalysisWorker worker;
    QVERIFY(worker.initialize().isOk());
    QVERIFY(worker.setNaca("0012", false).isOk());
    QVERIFY(worker.generatePanel(PanelingOptions{}).isOk());
    QVERIFY(worker.setConfiguration(AnalysisConfig{}).isOk());

    GuiDefinitionModel model;
    model.acceptGeometry(diamond());
    const auto panel = worker.generatePanel(PanelingOptions{});
    QVERIFY(panel.isOk());
    model.acceptPanel(panel.value(), PanelingOptions{});
    model.acceptConfiguration(AnalysisConfig{});
    const auto target = model.setSequence(
        xfoil::AlphaSequence{Degrees{0.0}, Degrees{8.0}, Degrees{0.5}});
    QVERIFY(target.isOk());
    const auto spec = model.makeRunSpec();
    QVERIFY(spec.isOk());

    auto token = std::make_shared<CancellationToken>();
    GuiAnalysisJob job{spec.value(), 77, 3, token, {}};
    bool requested = false;
    job.progress = [token, &requested](const ProgressEvent& event) {
      if (!requested && event.phase == ProgressPhase::sequence &&
          event.sequenceIndex.has_value() && *event.sequenceIndex >= 1) {
        requested = true;
        token->requestCancellation();
      }
    };

    QSignalSpy sequenceSpy(&worker, &GuiAnalysisWorker::sequenceReady);
    QSignalSpy finished(&worker, &GuiAnalysisWorker::finished);
    worker.run(std::move(job));
    QVERIFY(requested);
    QVERIFY(sequenceSpy.count() >= 1);
    QCOMPARE(finished.count(), 1);
    const QList<QVariant> args = finished.takeFirst();
    QVERIFY(args.at(2).value<GuiAnalysisFinish>() ==
            GuiAnalysisFinish::cancelled);
    const auto error = qvariant_cast<xfoil::Error>(args.at(3));
    QCOMPARE(QString::fromStdString(error.code),
             QStringLiteral("analysis.cancelled"));
  }

  void applySetupCommitsPreviewWithoutAnalysis() {
    GuiAnalysisWorker worker;
    QVERIFY(worker.initialize().isOk());
    AnalysisWorkflowSetup setup;
    setup.geometry = NacaGeometrySetup{"2412"};
    setup.paneling = PanelingOptions{};
    setup.configuration = AnalysisConfig{};
    setup.request = xfoil::LiftSequence{0.2, 0.8, 0.2};
    QSignalSpy pointSpy(&worker, &GuiAnalysisWorker::operatingPointReady);
    QSignalSpy sequenceSpy(&worker, &GuiAnalysisWorker::sequenceReady);
    const auto applied = worker.applySetup(setup);
    QVERIFY(applied.isOk());
    QVERIFY(!applied.value().geometry.empty());
    QVERIFY(!applied.value().panel.points.empty());
    QCOMPARE(pointSpy.count(), 0);
    QCOMPARE(sequenceSpy.count(), 0);
  }

  void failedApplySetupKeepsPreviousSession() {
    GuiAnalysisWorker worker;
    QVERIFY(worker.initialize().isOk());
    AnalysisWorkflowSetup valid;
    valid.geometry = NacaGeometrySetup{"0012"};
    valid.paneling = PanelingOptions{};
    valid.configuration = AnalysisConfig{};
    valid.request = xfoil::LiftTarget{0.4};
    QVERIFY(worker.applySetup(valid).isOk());

    AnalysisWorkflowSetup invalid = valid;
    invalid.configuration.reynoldsNumber = -1.0;
    QVERIFY(worker.applySetup(invalid).isErr());

    GuiAnalysisJob job{pointSpec(), 123, 4,
                       std::make_shared<CancellationToken>(), {}};
    QSignalSpy finished(&worker, &GuiAnalysisWorker::finished);
    worker.run(std::move(job));
    QCOMPARE(finished.count(), 1);
    const QList<QVariant> args = finished.takeFirst();
    QVERIFY(args.at(2).value<GuiAnalysisFinish>() ==
            GuiAnalysisFinish::completed);
  }

  void trackerRejectsOldJobAndGeneration() {
    GuiJobTracker tracker;
    const auto first = tracker.begin();
    QVERIFY(first.has_value());
    QVERIFY(tracker.isRunning());
    QVERIFY(tracker.accepts(first->jobId, first->generationId));
    QVERIFY(!tracker.begin().has_value());

    tracker.finish(first->jobId, first->generationId);
    QVERIFY(!tracker.isRunning());
    const auto second = tracker.begin();
    QVERIFY(second.has_value());
    QVERIFY(second->jobId > first->jobId);
    QVERIFY(!tracker.accepts(first->jobId, first->generationId));
    QVERIFY(tracker.accepts(second->jobId, second->generationId));

    tracker.invalidate();
    QVERIFY(!tracker.accepts(second->jobId, second->generationId));
    QVERIFY(tracker.isRunning());
    tracker.finish(second->jobId, second->generationId);
    QVERIFY(!tracker.isRunning());
    const auto third = tracker.begin();
    QVERIFY(third.has_value());
    QVERIFY(third->generationId > second->generationId);
  }
};

int runGuiAnalysisWorkerTests(int argc, char** argv) {
  GuiAnalysisWorkerTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiAnalysisWorkerTests.moc"

// End: tests/gui/GuiAnalysisWorkerTests.cpp

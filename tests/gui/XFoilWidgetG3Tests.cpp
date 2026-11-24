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

// File: tests/gui/XFoilWidgetG3Tests.cpp
// G3 integration tests for the widget worker, owning results, progress,
// cancellation, busy no-ops, isolation and shutdown.

#include <cstddef>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QStringList>
#include <QTimer>
#include <QtTest/QTest>

#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AlphaTarget;
using xfoil::Degrees;
using xfoil::LiftTarget;
using xfoil::OperatingPointData;
using xfoil::PanelingOptions;
using xfoil::gui::XFoilGuiState;
using xfoil::gui::XFoilWidget;

void configurePoint(XFoilWidget& widget, double alpha = 2.0) {
  QVERIFY(widget.setNaca("0012").isOk());
  QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
  widget.setTarget(AlphaTarget{Degrees{alpha}});
}

void configureSequence(XFoilWidget& widget, double end = 2.0,
                       double step = 1.0) {
  QVERIFY(widget.setNaca("0012").isOk());
  QVERIFY(widget.generatePanel(PanelingOptions{}).isOk());
  widget.setSequence(
      AlphaSequence{Degrees{0.0}, Degrees{end}, Degrees{step}});
}

void configureLongRun(XFoilWidget& widget) {
  QVERIFY(widget.setNaca("0012").isOk());
  PanelingOptions paneling;
  paneling.panelCount = 80;
  QVERIFY(widget.generatePanel(paneling).isOk());
  widget.setSequence(
      AlphaSequence{Degrees{0.0}, Degrees{40.0}, Degrees{0.1}});
}

QStringList directoryEntries(const QString& path) {
  return QDir(path).entryList(
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
          QDir::System,
      QDir::Name);
}

}  // namespace

class XFoilWidgetG3Tests : public QObject {
  Q_OBJECT

 private slots:
  void analyzeBeforeReadyIsNoOp() {
    XFoilWidget widget;
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    QSignalSpy failed(&widget, &XFoilWidget::analysisFailed);
    QVERIFY(!widget.analyzeAction()->isEnabled());
    widget.analyze();
    widget.analyzeAction()->trigger();
    QCOMPARE(started.count(), 0);
    QCOMPARE(failed.count(), 0);
    QVERIFY(!widget.isRunning());
    QVERIFY(widget.state() == XFoilGuiState::empty);
  }

  void alphaPointRunsAsynchronouslyAndReturnsOwningValue() {
    XFoilWidget widget;
    configurePoint(widget, 2.0);

    auto* analyzeButton =
        widget.findChild<QPushButton*>("analyzeButton");
    auto* progress = widget.findChild<QProgressBar*>("analysisProgressBar");
    auto* status = widget.findChild<QLabel*>("analysisStatusLabel");
    QVERIFY(analyzeButton != nullptr);
    QVERIFY(progress != nullptr);
    QVERIFY(status != nullptr);
    QCOMPARE(analyzeButton->text(), QStringLiteral("Analyze"));
    QCOMPARE(analyzeButton->property("analysisState").toString(),
             QStringLiteral("ready"));
    QVERIFY(analyzeButton->isEnabled());

    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    QSignalSpy pointSpy(&widget, &XFoilWidget::operatingPointReady);
    QSignalSpy failed(&widget, &XFoilWidget::analysisFailed);
    QSignalSpy stateSpy(&widget, &XFoilWidget::stateChanged);

    widget.analyzeAction()->trigger();
    QCOMPARE(started.count(), 1);
    QVERIFY(widget.isRunning());
    QVERIFY(widget.state() == XFoilGuiState::running);
    QCOMPARE(analyzeButton->text(), QStringLiteral("Cancel"));
    QCOMPARE(analyzeButton->property("analysisState").toString(),
             QStringLiteral("running"));
    QVERIFY(analyzeButton->isEnabled());
    QVERIFY(!widget.analyzeAction()->isEnabled());
    QVERIFY(widget.cancelAction()->isEnabled());

    QTRY_VERIFY_WITH_TIMEOUT(pointSpy.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(widget.state() == XFoilGuiState::results);
    QVERIFY(!stateSpy.isEmpty());

    const std::optional<OperatingPointData> point =
        widget.currentOperatingPoint();
    QVERIFY(point.has_value());
    QVERIFY(!widget.currentSequence().has_value());
    QCOMPARE(point->alpha.value, 2.0);
    QVERIFY(!point->surface.has_value() ||
            point->surface->x.size() == point->surface->cp.size());
    QVERIFY(status->text().contains(QStringLiteral("complete")));
    QVERIFY(status->isHidden());
    QVERIFY(status->parentWidget() != nullptr);
    QCOMPARE(progress->toolTip(),
             QStringLiteral("Progress of the current analysis phase"));
    QVERIFY(!progress->isHidden());
    QCOMPARE(progress->value(), 100);
    QCOMPARE(analyzeButton->text(), QStringLiteral("Analyze"));
    QCOMPARE(analyzeButton->property("analysisState").toString(),
             QStringLiteral("ready"));
    QVERIFY(analyzeButton->isEnabled());
    QVERIFY(widget.analyzeAction()->isEnabled());
    QVERIFY(!widget.cancelAction()->isEnabled());
  }

  void clPointRetryKeepsEarlierOwningCopy() {
    XFoilWidget widget;
    configurePoint(widget, 2.0);

    QSignalSpy pointSpy(&widget, &XFoilWidget::operatingPointReady);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(pointSpy.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    const OperatingPointData first = *widget.currentOperatingPoint();

    widget.setTarget(LiftTarget{0.4});
    QVERIFY(widget.state() == XFoilGuiState::configured);
    QVERIFY(!widget.currentOperatingPoint().has_value());
    QCOMPARE(first.alpha.value, 2.0);

    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(pointSpy.count() == 2, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.state() == XFoilGuiState::results);
    const OperatingPointData second = *widget.currentOperatingPoint();
    QVERIFY(second.alpha.value != 0.0);
    QCOMPARE(first.alpha.value, 2.0);
  }

  void detailedSequenceOwnsPointsAndSelectsFirstVisiblePoint() {
    XFoilWidget widget;
    configureSequence(widget, 2.0, 1.0);

    QSignalSpy sequenceSpy(&widget, &XFoilWidget::sequenceReady);
    QSignalSpy pointSpy(&widget, &XFoilWidget::operatingPointReady);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(sequenceSpy.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);

    QCOMPARE(pointSpy.count(), 0);
    QVERIFY(widget.state() == XFoilGuiState::results);
    const std::optional<xfoil::DetailedSequenceResult> sequence =
        widget.currentSequence();
    QVERIFY(sequence.has_value());
    QCOMPARE(sequence->points.size(), std::size_t{2});
    QVERIFY(!widget.currentOperatingPoint().has_value());
    QVERIFY(!sequence->points[0].operatingPoint.has_value() ||
            sequence->points[0].operatingPoint->alpha.value == 0.0);
    auto* operating =
        widget.findChild<QComboBox*>("operatingPointCombo");
    QVERIFY(operating != nullptr);
    QCOMPARE(operating->count(), 2);
    QCOMPARE(operating->currentIndex(), 0);
    QVERIFY(widget.findChild<QLabel*>("currentPointConvergenceLabel")
                ->text()
                .contains(QStringLiteral("Converged")));
  }

  void progressIsDeliveredAndBarRemainsVisibleAtRest() {
    XFoilWidget widget;
    configureSequence(widget);

    QSignalSpy progressSpy(&widget, &XFoilWidget::progressChanged);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(progressSpy.count() > 0, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QVERIFY(widget.state() == XFoilGuiState::results);
    auto* bar = widget.findChild<QProgressBar*>("analysisProgressBar");
    QVERIFY(bar != nullptr);
    QVERIFY(!bar->isHidden());
    QCOMPARE(bar->value(), 100);
  }

  void cancelDuringRunIsControlledAndIdempotent() {
    XFoilWidget widget;
    configureLongRun(widget);

    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    QSignalSpy cancelled(&widget, &XFoilWidget::analysisCancelled);
    QSignalSpy failed(&widget, &XFoilWidget::analysisFailed);
    widget.analyze();
    QCOMPARE(started.count(), 1);
    QVERIFY(widget.isRunning());

    widget.cancel();
    widget.cancel();
    widget.cancelAction()->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(cancelled.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
    QCOMPARE(failed.count(), 0);
    QVERIFY(widget.state() == XFoilGuiState::cancelled);
    QCOMPARE(widget.findChild<QPushButton*>("analyzeButton")->text(),
             QStringLiteral("Analyze"));
  }

  void secondAnalyzeWhileRunningIsNoOp() {
    XFoilWidget widget;
    configureLongRun(widget);

    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    QSignalSpy cancelled(&widget, &XFoilWidget::analysisCancelled);
    widget.analyze();
    widget.analyze();
    QCOMPARE(started.count(), 1);
    QVERIFY(widget.isRunning());
    widget.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(cancelled.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
  }

  void setupCallsDuringRunAreBusyNoOps() {
    XFoilWidget widget;
    configureLongRun(widget);
    const auto previousTarget = widget.target();
    const double previousAlpha =
        std::get<AlphaTarget>(previousTarget).alpha.value;
    const auto previousPaneling = widget.panelingOptions();

    QSignalSpy cancelled(&widget, &XFoilWidget::analysisCancelled);
    widget.analyze();
    QVERIFY(widget.isRunning());

    const auto geometry = widget.setNaca("2412");
    QVERIFY(geometry.isErr());
    QCOMPARE(QString::fromStdString(geometry.error().code),
             QStringLiteral("gui.busy"));
    const auto configuration =
        widget.setConfiguration(xfoil::AnalysisConfig{});
    QVERIFY(configuration.isErr());
    QCOMPARE(QString::fromStdString(configuration.error().code),
             QStringLiteral("gui.busy"));
    widget.setTarget(AlphaTarget{Degrees{7.0}});
    QCOMPARE(std::get<AlphaTarget>(widget.target()).alpha.value,
             previousAlpha);
    widget.setPanelingOptions(PanelingOptions{});
    QCOMPARE(widget.panelingOptions().panelCount,
             previousPaneling.panelCount);

    widget.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(cancelled.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
  }

  void cancelOutsideRunIsNoOp() {
    XFoilWidget widget;
    configurePoint(widget);
    QSignalSpy cancelled(&widget, &XFoilWidget::analysisCancelled);
    QSignalSpy started(&widget, &XFoilWidget::analysisStarted);
    const auto stateBefore = widget.state();
    widget.cancel();
    widget.cancelAction()->trigger();
    QCOMPARE(cancelled.count(), 0);
    QCOMPARE(started.count(), 0);
    QVERIFY(widget.state() == stateBefore);
    QVERIFY(!widget.isRunning());
  }

  void twoWidgetsUseIsolatedWorkers() {
    XFoilWidget first;
    XFoilWidget second;
    configurePoint(first, 1.0);
    configurePoint(second, -1.0);

    QSignalSpy firstPoint(&first, &XFoilWidget::operatingPointReady);
    QSignalSpy secondPoint(&second, &XFoilWidget::operatingPointReady);
    first.analyze();
    second.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(firstPoint.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(secondPoint.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!first.isRunning(), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!second.isRunning(), 30000);
    QCOMPARE(first.currentOperatingPoint()->alpha.value, 1.0);
    QCOMPARE(second.currentOperatingPoint()->alpha.value, -1.0);
  }

  void eventLoopStaysResponsiveWhileWorkerRuns() {
    XFoilWidget widget;
    configureLongRun(widget);

    QSignalSpy cancelled(&widget, &XFoilWidget::analysisCancelled);
    QTimer timer;
    int ticks = 0;
    QObject::connect(&timer, &QTimer::timeout, &timer,
                     [&ticks]() { ++ticks; });
    timer.start(5);
    widget.analyze();
    QTRY_VERIFY_WITH_TIMEOUT(ticks >= 2, 30000);
    QVERIFY(widget.isRunning());
    widget.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(cancelled.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
  }

  void destructionDuringRunJoinsWorker() {
    auto widget = std::make_unique<XFoilWidget>();
    configureLongRun(*widget);
    widget->analyze();
    QVERIFY(widget->isRunning());

    QElapsedTimer timer;
    timer.start();
    widget.reset();
    const qint64 elapsed = timer.elapsed();
    qInfo("G3 worker shutdown latency: %lld ms",
          static_cast<long long>(elapsed));
    QVERIFY(elapsed < 15000);
  }

  void analysisTouchesNoWorkingDirectoryFiles() {
    const QString cwd = QDir::currentPath();
    const QStringList before = directoryEntries(cwd);
    {
      XFoilWidget widget;
      configureSequence(widget);
      widget.analyze();
      QTRY_VERIFY_WITH_TIMEOUT(!widget.isRunning(), 30000);
      QVERIFY(widget.currentSequence().has_value());
    }
    QCOMPARE(QDir::currentPath(), cwd);
    QCOMPARE(directoryEntries(cwd), before);
  }
};

int runXFoilWidgetG3Tests(int argc, char** argv) {
  XFoilWidgetG3Tests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetG3Tests.moc"

// End: tests/gui/XFoilWidgetG3Tests.cpp

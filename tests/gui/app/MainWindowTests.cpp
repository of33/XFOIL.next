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

// File: tests/gui/app/MainWindowTests.cpp
// Thin-shell tests: one widget, app actions, file adapter use, settings,
// recent files, About and deferred close with cancellation.

#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QListWidget>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolBar>
#include <QtTest/QTest>

#include "app/MainWindow.h"
#include "app/ProductInfo.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

using xfoil::gui::XFoilWidget;
using xfoil::gui::app::MainWindow;

struct Harness {
  QString openPath;
  QString savePath;
  QStringList errors;
  QStringList infos;
  QString infoTitle;
  QString infoText;
  int confirmCalls{};
  bool confirmAnswer{false};

  MainWindow::Dependencies dependencies() {
    MainWindow::Dependencies dependencies;
    dependencies.settingsFile = settingsFile;
    dependencies.chooseFile =
        [this](MainWindow::FileDialogMode mode, const QString&, const QString&,
               const QString&) -> std::optional<QString> {
      if (mode == MainWindow::FileDialogMode::open) {
        if (openPath.isEmpty()) {
          return std::nullopt;
        }
        return openPath;
      }
      if (savePath.isEmpty()) {
        return std::nullopt;
      }
      return savePath;
    };
    dependencies.askText =
        [](const QString&, const QString&, const QString&)
        -> std::optional<QString> { return QStringLiteral("name"); };
    dependencies.askInt =
        [](const QString&, const QString&, int, int, int)
        -> std::optional<int> { return 1; };
    dependencies.confirm = [this](const QString&, const QString&) {
      ++confirmCalls;
      return confirmAnswer;
    };
    dependencies.showError = [this](const QString& title,
                                    const QString& text) {
      errors.append(title + QStringLiteral(": ") + text);
    };
    dependencies.showInfo = [this](const QString& title,
                                   const QString& text) {
      infoTitle = title;
      infoText = text;
      infos.append(text);
    };
    return dependencies;
  }

  QString settingsFile;
};

bool writeText(const QString& path, const char* text) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return file.write(text) == static_cast<qint64>(std::char_traits<char>::length(text));
}

QString readText(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

QString writeDiamond(const QTemporaryDir& directory, const char* name) {
  const QString path = directory.filePath(QString::fromLatin1(name));
  if (!writeText(path,
                 "diamond\n"
                 "1.0 0.0\n"
                 "0.5 0.08\n"
                 "0.0 0.0\n"
                 "0.5 -0.08\n"
                 "1.0 0.0\n")) {
    return {};
  }
  return path;
}

void configureLongRun(XFoilWidget& widget) {
  QVERIFY(widget.setNaca("0012").isOk());
  xfoil::PanelingOptions paneling;
  paneling.panelCount = 80;
  QVERIFY(widget.generatePanel(paneling).isOk());
  widget.setSequence(
      xfoil::AlphaSequence{xfoil::Degrees{0.0}, xfoil::Degrees{40.0},
                           xfoil::Degrees{0.1}});
}

}  // namespace

class MainWindowTests : public QObject {
  Q_OBJECT

 private slots:
  void constructsOneCentralWidgetAndAppActions() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(harness.dependencies());

    auto* widget = qobject_cast<XFoilWidget*>(window.centralWidget());
    QVERIFY(widget != nullptr);
    QCOMPARE(window.findChildren<XFoilWidget*>().size(), 1);
    QCOMPARE(window.findChild<QAction*>("openAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("saveAirfoilAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exportDxfAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exportCpAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exportPlotPngAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exportPlotPdfAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exportPlotSvgAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exitAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("aboutAction") != nullptr, true);
    QVERIFY(window.findChild<QMenu*>("fileMenu") != nullptr);
    QVERIFY(window.findChild<QMenu*>("analysisMenu") != nullptr);
    QVERIFY(window.findChild<QMenu*>("helpMenu") != nullptr);
    QVERIFY(window.findChild<QToolBar*>("mainToolBar") != nullptr);

    QVERIFY(!window.findChild<QAction*>("saveAirfoilAction")->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportDxfAction")->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportCpAction")->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportPlotPngAction")->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportPlotPdfAction")->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportPlotSvgAction")->isEnabled());
    QVERIFY(window.windowTitle().startsWith(QStringLiteral("XFOIL.next ")));
    QVERIFY(window.windowTitle().contains(QString::fromLatin1(
        xfoil::app::product::version.data(),
        static_cast<int>(xfoil::app::product::version.size()))));
    QVERIFY(window.close());
  }

  void polarImportAndComparisonExportUseAppAdapter() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath =
        QFileInfo(QStringLiteral(
                      "tests/data/xfoil-6.996/s11/canonical-base.pol"))
            .absoluteFilePath();
    harness.savePath = directory.filePath(QStringLiteral("output.pol"));
    MainWindow window(harness.dependencies());
    auto* widget = qobject_cast<XFoilWidget*>(window.centralWidget());
    QVERIFY(widget != nullptr);

    QVERIFY(!window.findChild<QAction*>("exportCurrentPolarAction")->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportComparisonPolarAction")->isEnabled());
    window.findChild<QAction*>("importPolarAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QCOMPARE(widget->findChild<QListWidget*>("polarListView")->count(), 1);
    QVERIFY(window.findChild<QAction*>("exportComparisonPolarAction")->isEnabled());

    window.findChild<QAction*>("exportComparisonPolarAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    const QString saved = readText(harness.savePath);
    QVERIFY(saved.contains(QStringLiteral("Calculated polar for: NACA 0012")));
    QVERIFY(window.close());
  }

  void cpAndPlotExportsWriteOwningBytes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(harness.dependencies());
    auto* widget = qobject_cast<XFoilWidget*>(window.centralWidget());
    QVERIFY(widget != nullptr);
    QVERIFY(widget->setNaca("0012").isOk());
    xfoil::PanelingOptions paneling;
    paneling.panelCount = 80;
    QVERIFY(widget->generatePanel(paneling).isOk());
    xfoil::AnalysisConfig config;
    config.viscous = false;
    config.maximumIterations = 50;
    QVERIFY(widget->setConfiguration(config).isOk());
    widget->setTarget(xfoil::LiftTarget{0.5});
    widget->analyze();
    QTRY_VERIFY_WITH_TIMEOUT(!widget->isRunning(), 30000);
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QAction*>("exportCpAction")->isEnabled(), 5000);

    harness.savePath = directory.filePath(QStringLiteral("out_cp.txt"));
    window.findChild<QAction*>("exportCpAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    const QString cp = readText(harness.savePath);
    QVERIFY(cp.startsWith(QStringLiteral("#      x          Cp")));

    harness.savePath = directory.filePath(QStringLiteral("plot.png"));
    window.findChild<QAction*>("exportPlotPngAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QFile png(harness.savePath);
    QVERIFY(png.open(QIODevice::ReadOnly));
    QVERIFY(png.readAll().startsWith("\x89PNG\r\n\x1a\n"));

    harness.savePath = directory.filePath(QStringLiteral("plot.pdf"));
    window.findChild<QAction*>("exportPlotPdfAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QFile pdf(harness.savePath);
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    const QByteArray pdfBytes = pdf.readAll();
    QVERIFY(pdfBytes.startsWith("%PDF"));
    QVERIFY(!pdfBytes.contains("C:\\Users"));
    QVERIFY(!pdfBytes.contains("/local/"));
    QVERIFY(!pdfBytes.contains("g6d-accept"));

    harness.savePath = directory.filePath(QStringLiteral("plot.svg"));
    window.findChild<QAction*>("exportPlotSvgAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    const QString svg = readText(harness.savePath);
    QVERIFY(svg.contains(QStringLiteral("<svg")));
    QVERIFY(!svg.contains(QStringLiteral("<image")));
    QVERIFY(!svg.contains(QStringLiteral("C:\\Users")));
    QVERIFY(!svg.contains(QStringLiteral("/local/")));
    QVERIFY(!svg.contains(QStringLiteral("g6d-accept")));
    QVERIFY(window.close());
  }

  void openSaveExportAndAboutUseExistingContracts() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath = writeDiamond(directory, "input.dat");
    harness.savePath = directory.filePath(QStringLiteral("saved.dat"));
    QVERIFY(!harness.openPath.isEmpty());
    MainWindow window(harness.dependencies());
    auto* widget = qobject_cast<XFoilWidget*>(window.centralWidget());
    QVERIFY(widget != nullptr);

    window.findChild<QAction*>("openAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QVERIFY(widget->geometry().has_value());
    QVERIFY(widget->panelGeometry().has_value());
    QVERIFY(window.findChild<QAction*>("saveAirfoilAction")->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportDxfAction")->isEnabled());

    window.findChild<QAction*>("saveAirfoilAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    const QString saved = readText(harness.savePath);
    QVERIFY(saved.startsWith(QStringLiteral("diamond\n")));
    QCOMPARE(saved.count(QLatin1Char('\n')), 6);

    harness.savePath = directory.filePath(QStringLiteral("panel.dxf"));
    window.findChild<QAction*>("exportDxfAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    const QString dxf = readText(harness.savePath);
    QVERIFY(dxf.startsWith(QStringLiteral("999\n")));
    QVERIFY(dxf.contains(QStringLiteral("LINE")));

    window.findChild<QAction*>("aboutAction")->trigger();
    QVERIFY(harness.infoTitle.contains(QStringLiteral("About")));
    QVERIFY(harness.infoText.contains(QStringLiteral("XFOIL.next")));
    QVERIFY(harness.infoText.contains(QStringLiteral("Qt ")));
    QVERIFY(harness.infoText.contains(
        QString::fromLatin1(QCUSTOMPLOT_VERSION_STR)));
    QVERIFY(harness.infoText.contains(QString::fromLatin1(
        xfoil::app::product::version.data(),
        static_cast<int>(xfoil::app::product::version.size()))));
    QVERIFY(window.close());
  }

  void cancellationLeavesStateUnchanged() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath.clear();
    MainWindow window(harness.dependencies());
    auto* widget = qobject_cast<XFoilWidget*>(window.centralWidget());
    QVERIFY(widget != nullptr);

    window.findChild<QAction*>("openAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QVERIFY(!widget->geometry().has_value());
    QVERIFY(window.findChild<QMenu*>("recentFilesMenu")->actions().size() == 1);
    QVERIFY(window.close());
  }

  void failedOpenIsReportedAndNotRecent() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath = directory.filePath(QStringLiteral("missing.dat"));
    MainWindow window(harness.dependencies());

    window.findChild<QAction*>("openAction")->trigger();
    QCOMPARE(harness.errors.size(), 1);
    QVERIFY(window.findChild<QMenu*>("recentFilesMenu")->actions().size() == 1);
    QVERIFY(window.close());
  }

  void recentFilesPersistAcrossWindows() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settings = directory.filePath(QStringLiteral("settings.ini"));
    const QString input = writeDiamond(directory, "recent.dat");
    QVERIFY(!input.isEmpty());

    {
      Harness harness;
      harness.settingsFile = settings;
      harness.openPath = input;
      MainWindow window(harness.dependencies());
      window.findChild<QAction*>("openAction")->trigger();
      QVERIFY(harness.errors.isEmpty());
      QVERIFY(window.findChild<QAction*>("recentFile0Action") != nullptr);
      QVERIFY(window.close());
    }

    Harness second;
    second.settingsFile = settings;
    MainWindow window(second.dependencies());
    QAction* recent = window.findChild<QAction*>("recentFile0Action");
    QVERIFY(recent != nullptr);
    QCOMPARE(recent->data().toString(), QFileInfo(input).absoluteFilePath());
    QVERIFY(window.close());
  }

  void closeWhileRunningRequiresConfirmationAndCancels() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(harness.dependencies());
    window.show();
    auto* widget = qobject_cast<XFoilWidget*>(window.centralWidget());
    QVERIFY(widget != nullptr);
    configureLongRun(*widget);

    QSignalSpy cancelled(widget, &XFoilWidget::analysisCancelled);
    widget->analyzeAction()->trigger();
    QVERIFY(widget->isRunning());

    harness.confirmAnswer = false;
    QVERIFY(!window.close());
    QCOMPARE(harness.confirmCalls, 1);
    QVERIFY(widget->isRunning());

    harness.confirmAnswer = true;
    QVERIFY(!window.close());
    QCOMPARE(harness.confirmCalls, 2);
    QTRY_VERIFY_WITH_TIMEOUT(cancelled.count() == 1, 30000);
    QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 30000);
    QVERIFY(!widget->isRunning());
  }

  void closeWithoutRunWritesSettings() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(harness.dependencies());
    QVERIFY(window.close());
    QVERIFY(QFileInfo::exists(harness.settingsFile));
  }
};

int runMainWindowTests(int argc, char** argv) {
  MainWindowTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "MainWindowTests.moc"

// End: tests/gui/app/MainWindowTests.cpp

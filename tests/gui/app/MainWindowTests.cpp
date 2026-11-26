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
// Thin-shell tests: one Analysis workflow page, app actions, file adapter
// use, settings, recent files, About and deferred close with cancellation.

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QTest>

#include "app/AppIcon.h"
#include "app/MainWindow.h"
#include "app/ProductInfo.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilAnalysisWidget.h"

namespace {

using xfoil::gui::XFoilAnalysisWidget;
using xfoil::gui::app::MainWindow;

QTabWidget* workflowTabs(MainWindow& window) {
  return qobject_cast<QTabWidget*>(window.centralWidget());
}

XFoilAnalysisWidget* analysisWidget(MainWindow& window) {
  QTabWidget* tabs = workflowTabs(window);
  return tabs == nullptr ? nullptr
                         : qobject_cast<XFoilAnalysisWidget*>(tabs->widget(0));
}

struct Harness {
  QString openPath;
  QString savePath;
  QStringList errors;
  QStringList infos;
  QString infoTitle;
  QString infoText;
  QString lastSuggested;
  int chooseCalls{};
  int confirmCalls{};
  bool confirmAnswer{false};

  MainWindow::Dependencies dependencies() {
    MainWindow::Dependencies dependencies;
    dependencies.settingsFile = settingsFile;
    dependencies.chooseFile =
        [this](MainWindow::FileDialogMode mode, const QString&, const QString&,
               const QString& suggested) -> std::optional<QString> {
      lastSuggested = suggested;
      ++chooseCalls;
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

void configureLongRun(XFoilAnalysisWidget& widget) {
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

    auto* tabs = workflowTabs(window);
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->objectName(), QStringLiteral("workflowTabs"));
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Analysis"));
    QVERIFY(tabs->isTabVisible(0));
    QVERIFY(!tabs->tabsClosable());
    auto* widget = analysisWidget(window);
    QVERIFY(widget != nullptr);
    QCOMPARE(tabs->widget(0), widget);
    QVERIFY(tabs->isAncestorOf(widget));
    QCOMPARE(window.findChildren<XFoilAnalysisWidget*>().size(), 1);
    const QIcon productIcon = xfoil::gui::app::applicationIcon();
    QVERIFY(!productIcon.isNull());
    QVERIFY(!window.windowIcon().isNull());
    QCOMPARE(window.windowIcon().pixmap(64, 64).toImage(),
             productIcon.pixmap(64, 64).toImage());
    QCOMPARE(window.findChild<QAction*>("loadAirfoilAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("loadAirfoilAction")->text(),
             QStringLiteral("&Load Airfoil..."));
    auto* saveAirfoil = window.findChild<QAction*>("saveAirfoilAction");
    auto* exportDxf = window.findChild<QAction*>("exportDxfAction");
    auto* exportAirfoilPng =
        window.findChild<QAction*>("exportAirfoilPngAction");
    auto* exportAirfoilPdf =
        window.findChild<QAction*>("exportAirfoilPdfAction");
    auto* exportAirfoilSvg =
        window.findChild<QAction*>("exportAirfoilSvgAction");
    auto* exportPng = window.findChild<QAction*>("exportPlotPngAction");
    auto* exportPdf = window.findChild<QAction*>("exportPlotPdfAction");
    auto* exportSvg = window.findChild<QAction*>("exportPlotSvgAction");
    QVERIFY(saveAirfoil != nullptr);
    QVERIFY(exportDxf != nullptr);
    QVERIFY(exportAirfoilPng != nullptr);
    QVERIFY(exportAirfoilPdf != nullptr);
    QVERIFY(exportAirfoilSvg != nullptr);
    QVERIFY(exportPng != nullptr);
    QVERIFY(exportPdf != nullptr);
    QVERIFY(exportSvg != nullptr);
    QCOMPARE(saveAirfoil->text(), QStringLiteral("&Data..."));
    QCOMPARE(exportDxf->text(), QStringLiteral("DXF..."));
    QCOMPARE(exportAirfoilPng->text(), QStringLiteral("&PNG..."));
    QCOMPARE(exportAirfoilPdf->text(), QStringLiteral("P&DF..."));
    QCOMPARE(exportAirfoilSvg->text(), QStringLiteral("S&VG..."));
    QCOMPARE(exportPng->text(), QStringLiteral("&PNG..."));
    QCOMPARE(exportPdf->text(), QStringLiteral("P&DF..."));
    QCOMPARE(exportSvg->text(), QStringLiteral("S&VG..."));
    QCOMPARE(window.findChild<QAction*>("exportCpAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("exitAction") != nullptr, true);
    QCOMPARE(window.findChild<QAction*>("aboutAction") != nullptr, true);
    auto* fileMenu = window.findChild<QMenu*>("fileMenu");
    auto* airfoilMenu = window.findChild<QMenu*>("exportAirfoilMenu");
    auto* plotMenu = window.findChild<QMenu*>("exportPlotMenu");
    auto* recentMenu = window.findChild<QMenu*>("recentFilesMenu");
    QVERIFY(fileMenu != nullptr);
    QVERIFY(airfoilMenu != nullptr);
    QVERIFY(plotMenu != nullptr);
    QVERIFY(recentMenu != nullptr);
    QCOMPARE(airfoilMenu->title(), QStringLiteral("Export &Airfoil"));
    QCOMPARE(airfoilMenu->actions().size(), 5);
    QCOMPARE(airfoilMenu->actions().at(0), saveAirfoil);
    QCOMPARE(airfoilMenu->actions().at(1), exportDxf);
    QCOMPARE(airfoilMenu->actions().at(2), exportAirfoilPng);
    QCOMPARE(airfoilMenu->actions().at(3), exportAirfoilPdf);
    QCOMPARE(airfoilMenu->actions().at(4), exportAirfoilSvg);
    QCOMPARE(plotMenu->title(), QStringLiteral("Export &Active Plot"));
    QCOMPARE(plotMenu->actions().size(), 3);
    QCOMPARE(plotMenu->actions().at(0), exportPng);
    QCOMPARE(plotMenu->actions().at(1), exportPdf);
    QCOMPARE(plotMenu->actions().at(2), exportSvg);
    QCOMPARE(recentMenu->title(), QStringLiteral("Recent &Airfoils"));
    QCOMPARE(recentMenu->actions().size(), 1);
    QCOMPARE(recentMenu->actions().at(0)->text(),
             QStringLiteral("No recent airfoils"));
    QVERIFY(!fileMenu->actions().contains(saveAirfoil));
    QVERIFY(!fileMenu->actions().contains(exportDxf));
    QVERIFY(!fileMenu->actions().contains(exportAirfoilPng));
    QVERIFY(!fileMenu->actions().contains(exportAirfoilPdf));
    QVERIFY(!fileMenu->actions().contains(exportAirfoilSvg));
    QVERIFY(!fileMenu->actions().contains(exportPng));
    QVERIFY(!fileMenu->actions().contains(exportPdf));
    QVERIFY(!fileMenu->actions().contains(exportSvg));
    const int recentIndex =
        fileMenu->actions().indexOf(recentMenu->menuAction());
    QVERIFY(recentIndex > 0);
    QVERIFY(fileMenu->actions().at(recentIndex - 1)->isSeparator());
    QVERIFY(window.findChild<QMenu*>("analysisMenu") == nullptr);
    QVERIFY(window.findChild<QMenu*>("helpMenu") != nullptr);
    QVERIFY(window.findChild<QToolBar*>("mainToolBar") == nullptr);
    QCOMPARE(window.minimumSize(), QSize(1280, 720));
    QVERIFY(window.size().width() >= 1280);
    QVERIFY(window.size().height() >= 720);

    QVERIFY(!window.findChild<QAction*>("saveAirfoilAction")->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportDxfAction")->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportAirfoilPngAction")
                 ->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportAirfoilPdfAction")
                 ->isEnabled());
    QVERIFY(!window.findChild<QAction*>("exportAirfoilSvgAction")
                 ->isEnabled());
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

  void workflowTabsOwnSingleAnalysisPageAndKeepLayout() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QPointer<XFoilAnalysisWidget> guard;
    {
      Harness harness;
      harness.settingsFile =
          directory.filePath(QStringLiteral("settings.ini"));
      MainWindow window(harness.dependencies());
      auto* tabs = workflowTabs(window);
      auto* widget = analysisWidget(window);
      QVERIFY(tabs != nullptr);
      QVERIFY(widget != nullptr);
      QCOMPARE(tabs->parentWidget(), &window);
      QCOMPARE(tabs->widget(0), widget);
      QVERIFY(tabs->isAncestorOf(widget));
      QCOMPARE(tabs->count(), 1);
      QCOMPARE(tabs->currentWidget(), widget);
      QCOMPARE(window.findChildren<XFoilAnalysisWidget*>().size(), 1);
      QCOMPARE(window.minimumSize(), QSize(1280, 720));
      QCOMPARE(widget->minimumSize(), QSize(960, 620));
      QVERIFY(widget->findChild<QWidget*>("definitionsFrame") != nullptr);
      QVERIFY(widget->findChild<QWidget*>("resultsColumn") != nullptr);
      QVERIFY(window.findChild<QScrollArea*>("definitionsScrollArea") !=
              nullptr);
      QCOMPARE(window.findChildren<QScrollArea*>().size(),
               widget->findChildren<QScrollArea*>().size());

      window.show();
      QCoreApplication::processEvents();
      QVERIFY(tabs->isVisible());
      QVERIFY(widget->isVisible());
      QVERIFY(widget->width() >= widget->minimumWidth());
      QVERIFY(widget->height() >= widget->minimumHeight());
      auto* splitter = widget->findChild<QSplitter*>("mainSplitter");
      QVERIFY(splitter != nullptr);
      QCOMPARE(splitter->sizes().size(), 2);
      QVERIFY(splitter->sizes().at(0) > 0);
      QVERIFY(splitter->sizes().at(1) > 0);

      guard = widget;
      QVERIFY(window.close());
    }
    QVERIFY(guard.isNull());
  }

  void applicationIconIsEmbeddedAndLoadable() {
    QFile resource(QStringLiteral(":/xfoil-next/xfoil-next-icon.png"));
    QVERIFY(resource.open(QIODevice::ReadOnly));
    QVERIFY(resource.size() > 0);
    QCOMPARE(resource.read(8), QByteArray("\x89PNG\r\n\x1a\n", 8));

    const QString previousDirectory = QDir::currentPath();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir::setCurrent(directory.path()));
    const QIcon icon = xfoil::gui::app::applicationIcon();
    QVERIFY(QDir::setCurrent(previousDirectory));
    QVERIFY(!icon.isNull());
    QVERIFY(!icon.availableSizes().isEmpty());
  }

  void applicationIconCanBeAppliedToQApplication() {
    const QIcon previous = qApp->windowIcon();
    qApp->setWindowIcon(QIcon());
    qApp->setWindowIcon(xfoil::gui::app::applicationIcon());
    QVERIFY(!qApp->windowIcon().isNull());
    qApp->setWindowIcon(previous);
  }

  void applicationIconHasWhiteInteriorAndTransparentOutside() {
    const QImage image(QStringLiteral(":/xfoil-next/xfoil-next-icon.png"));
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(512, 512));
    QVERIFY(image.hasAlphaChannel());
    const QPoint corners[] = {{0, 0}, {511, 0}, {0, 511}, {511, 511}};
    for (const QPoint& point : corners) {
      QCOMPARE(qAlpha(image.pixel(point)), 0);
    }
    QCOMPARE(qAlpha(image.pixel(0, 256)), 0);
    QCOMPARE(qAlpha(image.pixel(511, 256)), 0);
    const QColor centre = image.pixelColor(256, 256);
    QVERIFY(centre.alpha() >= 230);
    QVERIFY(centre.red() >= 250);
    QVERIFY(centre.green() >= 250);
    QVERIFY(centre.blue() >= 250);
    int transparent = 0;
    int white = 0;
    int blue = 0;
    int black = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        const QColor colour = image.pixelColor(x, y);
        if (colour.alpha() == 0) {
          ++transparent;
        } else if (colour.alpha() >= 230 && colour.red() >= 250 &&
                   colour.green() >= 250 && colour.blue() >= 250) {
          ++white;
        } else if (colour.blue() > 80 && colour.red() < 80 &&
                   colour.green() < 80) {
          ++blue;
        } else if (colour.red() < 50 && colour.green() < 50 &&
                   colour.blue() < 50) {
          ++black;
        }
      }
    }
    const int total = image.width() * image.height();
    QVERIFY(transparent > total / 100);
    QVERIFY(white > total / 20);
    QVERIFY(blue > 0);
    QVERIFY(black > 0);
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
    harness.savePath = directory.filePath(QStringLiteral("output"));
    MainWindow window(harness.dependencies());
    auto* widget = analysisWidget(window);
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
    QVERIFY(harness.lastSuggested.endsWith(QStringLiteral(".pol")));
    const QString saved = readText(harness.savePath + QStringLiteral(".pol"));
    QVERIFY(saved.contains(QStringLiteral("Calculated polar for: NACA 0012")));

    harness.savePath = directory.filePath(QStringLiteral("upper.POL"));
    window.findChild<QAction*>("exportComparisonPolarAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QVERIFY(harness.lastSuggested.endsWith(QStringLiteral(".pol")));
    QVERIFY(QFile::exists(harness.savePath));

    harness.savePath = directory.filePath(QStringLiteral("foreign.txt"));
    window.findChild<QAction*>("exportComparisonPolarAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QVERIFY(QFile::exists(harness.savePath + QStringLiteral(".pol")));
    QVERIFY(window.close());
  }

  void cpAndPlotExportsWriteOwningBytes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(harness.dependencies());
    auto* widget = analysisWidget(window);
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

    harness.savePath = directory.filePath(QStringLiteral("out_cp"));
    window.findChild<QAction*>("exportCpAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QVERIFY(harness.lastSuggested.endsWith(QStringLiteral(".cp")));
    QVERIFY(harness.lastSuggested.startsWith(QStringLiteral("NACA 0012")));
    const QString cp = readText(harness.savePath + QStringLiteral(".cp"));
    QVERIFY(cp.startsWith(QStringLiteral("#      x          Cp")));

    harness.savePath = directory.filePath(QStringLiteral("out_cp.CP"));
    window.findChild<QAction*>("exportCpAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QVERIFY(QFile::exists(harness.savePath));

    harness.savePath = directory.filePath(QStringLiteral("out_cp.txt"));
    window.findChild<QAction*>("exportCpAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QVERIFY(QFile::exists(harness.savePath + QStringLiteral(".cp")));

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

  void loadAirfoilNormalizesClockwiseAndCancelRestoresSource() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath = directory.filePath(QStringLiteral("clockwise.dat"));
    QVERIFY(writeText(harness.openPath,
                      "clockwise\n"
                      "1.0 0.0\n"
                      "0.5 -0.08\n"
                      "0.0 0.0\n"
                      "0.5 0.08\n"
                      "1.0 0.0\n"));
    MainWindow window(harness.dependencies());
    auto* widget = analysisWidget(window);
    QVERIFY(widget != nullptr);
    QSignalSpy started(widget, &XFoilAnalysisWidget::analysisStarted);

    window.findChild<QAction*>("loadAirfoilAction")->trigger();
    QVERIFY2(harness.errors.empty(),
             harness.errors.join(QStringLiteral("; ")).toUtf8().constData());
    QCOMPARE(started.count(), 0);
    QVERIFY(widget->geometry().has_value());
    QCOMPARE(QString::fromStdString(std::string(widget->geometry()->name())),
             QStringLiteral("clockwise"));
    QVERIFY(widget->panelGeometry().has_value());
    QVERIFY(widget->isReadyToAnalyze());
    auto* notice =
        widget->findChild<QLabel*>("geometryImportNoticeLabel");
    QVERIFY(notice != nullptr);
    QVERIFY(!notice->isHidden());
    QCOMPARE(widget->geometry()->coordinates()[1].y, 0.08);

    QVERIFY(widget->setNaca("0012").isOk());
    QVERIFY(notice->isHidden());
    QVERIFY(widget->findChild<QRadioButton*>("geometryNacaRadio")
                ->isChecked());
    harness.openPath.clear();
    QTest::mouseClick(
        widget->findChild<QRadioButton*>("geometryImportRadio"),
        Qt::LeftButton);
    QVERIFY(widget->findChild<QRadioButton*>("geometryNacaRadio")
                ->isChecked());
    QVERIFY(!widget->findChild<QRadioButton*>("geometryImportRadio")
                 ->isChecked());
    QCOMPARE(QString::fromStdString(std::string(widget->geometry()->name())),
             QStringLiteral("NACA 0012"));

    const QString beforeName =
        QString::fromStdString(std::string(widget->geometry()->name()));
    harness.openPath = directory.filePath(QStringLiteral("degenerate.dat"));
    QVERIFY(writeText(harness.openPath,
                      "degenerate\n"
                      "0.0 0.0\n"
                      "0.5 0.0\n"
                      "1.0 0.0\n"));
    harness.errors.clear();
    window.findChild<QAction*>("loadAirfoilAction")->trigger();
    QVERIFY(!harness.errors.isEmpty());
    QCOMPARE(QString::fromStdString(std::string(widget->geometry()->name())),
             beforeName);
    QVERIFY(notice->isHidden());
    QVERIFY(widget->panelGeometry().has_value());
    QVERIFY(window.close());
  }

  void importRadioUsesSameDialogProvider() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath = directory.filePath(QStringLiteral("radio.dat"));
    QVERIFY(writeText(harness.openPath,
                      "radio\n"
                      "1.0 0.0\n"
                      "0.5 0.08\n"
                      "0.0 0.0\n"
                      "0.5 -0.08\n"
                      "1.0 0.0\n"));
    MainWindow window(harness.dependencies());
    auto* widget = analysisWidget(window);
    QVERIFY(widget != nullptr);
    QVERIFY(widget->setNaca("0012").isOk());
    harness.chooseCalls = 0;

    QTest::mouseClick(
        widget->findChild<QRadioButton*>("geometryImportRadio"),
        Qt::LeftButton);
    QCOMPARE(harness.chooseCalls, 1);
    QVERIFY(harness.errors.isEmpty());
    QCOMPARE(QString::fromStdString(std::string(widget->geometry()->name())),
             QStringLiteral("radio"));
    QVERIFY(widget->findChild<QRadioButton*>("geometryImportRadio")
                ->isChecked());
    harness.chooseCalls = 0;
    QTest::mouseClick(
        widget->findChild<QPushButton*>("loadAirfoilButton"),
        Qt::LeftButton);
    QCOMPARE(harness.chooseCalls, 1);
    QVERIFY(harness.errors.isEmpty());
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
    auto* widget = analysisWidget(window);
    QVERIFY(widget != nullptr);

    window.findChild<QAction*>("loadAirfoilAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QVERIFY(widget->geometry().has_value());
    QVERIFY(widget->panelGeometry().has_value());
    QVERIFY(window.findChild<QAction*>("saveAirfoilAction")->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportDxfAction")->isEnabled());

    QVERIFY(window.findChild<QAction*>("exportAirfoilPngAction")
                ->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportAirfoilPdfAction")
                ->isEnabled());
    QVERIFY(window.findChild<QAction*>("exportAirfoilSvgAction")
                ->isEnabled());

    harness.savePath = directory.filePath(QStringLiteral("saved"));
    window.findChild<QAction*>("saveAirfoilAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QCOMPARE(harness.lastSuggested, QStringLiteral("diamond.dat"));
    QString saved = readText(harness.savePath + QStringLiteral(".dat"));
    QVERIFY(saved.startsWith(QStringLiteral("diamond\n")));
    QCOMPARE(saved.count(QLatin1Char('\n')), 6);

    harness.savePath = directory.filePath(QStringLiteral("saved-upper.DAT"));
    window.findChild<QAction*>("saveAirfoilAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QVERIFY(QFile::exists(harness.savePath));

    harness.savePath = directory.filePath(QStringLiteral("saved-foreign.txt"));
    window.findChild<QAction*>("saveAirfoilAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QVERIFY(QFile::exists(harness.savePath + QStringLiteral(".dat")));
    saved = readText(harness.savePath + QStringLiteral(".dat"));
    QVERIFY(saved.startsWith(QStringLiteral("diamond\n")));

    harness.savePath = directory.filePath(QStringLiteral("panel.dxf"));
    window.findChild<QAction*>("exportDxfAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    const QString dxf = readText(harness.savePath);
    QVERIFY(dxf.startsWith(QStringLiteral("999\n")));
    QVERIFY(dxf.contains(QStringLiteral("LINE")));

    harness.savePath = directory.filePath(QStringLiteral("airfoil.png"));
    window.findChild<QAction*>("exportAirfoilPngAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QFile airfoilPng(harness.savePath);
    QVERIFY(airfoilPng.open(QIODevice::ReadOnly));
    QVERIFY(airfoilPng.readAll().startsWith("\x89PNG\r\n\x1a\n"));

    harness.savePath = directory.filePath(QStringLiteral("airfoil.pdf"));
    window.findChild<QAction*>("exportAirfoilPdfAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    QFile airfoilPdf(harness.savePath);
    QVERIFY(airfoilPdf.open(QIODevice::ReadOnly));
    QVERIFY(airfoilPdf.readAll().startsWith("%PDF"));

    harness.savePath = directory.filePath(QStringLiteral("airfoil.svg"));
    window.findChild<QAction*>("exportAirfoilSvgAction")->trigger();
    QVERIFY(harness.errors.isEmpty());
    const QString airfoilSvg = readText(harness.savePath);
    QVERIFY(airfoilSvg.contains(QStringLiteral("<svg")));
    QVERIFY(!airfoilSvg.contains(QStringLiteral("<image")));

    window.findChild<QAction*>("aboutAction")->trigger();
    QVERIFY(harness.infoTitle.contains(QStringLiteral("About")));
    const QString aboutPrefix =
        QStringLiteral("XFOIL.next ") +
        QString::fromLatin1(xfoil::app::product::version.data(),
                            static_cast<int>(
                                xfoil::app::product::version.size()));
    QVERIFY(harness.infoText.startsWith(aboutPrefix));
    QVERIFY(harness.infoText.contains(QStringLiteral("XFOIL.next")));
    QVERIFY(harness.infoText.contains(QStringLiteral("Qt ")));
    QVERIFY(harness.infoText.contains(
        QString::fromLatin1(QCUSTOMPLOT_VERSION_STR)));
    QVERIFY(harness.infoText.contains(QString::fromLatin1(
        xfoil::app::product::version.data(),
        static_cast<int>(xfoil::app::product::version.size()))));
    QVERIFY(harness.infoText.contains(
        QStringLiteral("Copyright (C) 2026 Olaf Frommann")));
    QVERIFY(harness.infoText.contains(QStringLiteral(
        "Copyright (C) 2026 Olaf Frommann\n\n"
        "https://github.com/of33/XFOIL.next")));
    QVERIFY(harness.infoText.contains(
        QStringLiteral("This software uses:\n")));
    const QString bullet(QChar(0x2022));
    QVERIFY(harness.infoText.contains(bullet + QStringLiteral(" Qt ")));
    QVERIFY(harness.infoText.contains(
        bullet + QStringLiteral(" QCustomPlot ")));
    QVERIFY(harness.infoText.contains(QStringLiteral("XFOIL 6.996")));
    QVERIFY(harness.infoText.contains(QStringLiteral("GPL-3.0-or-later")));
    QCOMPARE(harness.infoText.count(
                 QStringLiteral("https://github.com/of33/XFOIL.next")),
             1);
    QVERIFY(!harness.infoText.contains(QStringLiteral("1.2.1")));
    QVERIFY(window.close());
  }

  void aboutDialogUsesWiderMinimum() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MainWindow::Dependencies dependencies;
    dependencies.settingsFile =
        directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(dependencies);
    window.show();
    int baseWidth = 0;
    int minimumWidth = 0;
    int actualWidth = 0;
    bool found = false;
    bool whiteBackground = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, [&]() {
      for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* box = qobject_cast<QDialog*>(widget);
        if (box == nullptr ||
            box->objectName() != QStringLiteral("aboutDialog")) {
          continue;
        }
        baseWidth = box->property("aboutBaseWidth").toInt();
        minimumWidth = box->minimumWidth();
        actualWidth = box->width();
        auto* label = box->findChild<QLabel*>("aboutTextLabel");
        whiteBackground = label != nullptr &&
            label->palette().color(QPalette::Window) == QColor(Qt::white);
        found = true;
        timer.stop();
        box->accept();
        return;
      }
    });
    timer.start(10);
    window.findChild<QAction*>("aboutAction")->trigger();
    QVERIFY(found);
    QVERIFY(whiteBackground);
    QVERIFY2(baseWidth > 0, "base width missing");
    const int targetWidth = qRound(static_cast<double>(baseWidth) * 1.2);
    QVERIFY2(minimumWidth >= targetWidth || actualWidth >= targetWidth,
             qPrintable(QStringLiteral("base=%1 min=%2 width=%3")
                            .arg(baseWidth).arg(minimumWidth).arg(actualWidth)));
    QVERIFY(window.close());
  }

  void cancellationLeavesStateUnchanged() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Harness harness;
    harness.settingsFile = directory.filePath(QStringLiteral("settings.ini"));
    harness.openPath.clear();
    MainWindow window(harness.dependencies());
    auto* widget = analysisWidget(window);
    QVERIFY(widget != nullptr);

    window.findChild<QAction*>("loadAirfoilAction")->trigger();
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

    window.findChild<QAction*>("loadAirfoilAction")->trigger();
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
      window.findChild<QAction*>("loadAirfoilAction")->trigger();
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
    auto* widget = analysisWidget(window);
    QVERIFY(widget != nullptr);
    configureLongRun(*widget);

    QSignalSpy cancelled(widget, &XFoilAnalysisWidget::analysisCancelled);
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

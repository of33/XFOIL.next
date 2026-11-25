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

// File: tests/gui/XFoilAnalysisWidgetShellTests.cpp
// Offscreen construction, embedding, ownership and global-side-effect
// tests for the public XFoilAnalysisWidget shell.

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMainWindow>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QScrollArea>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest/QTest>

#include "plots/GeometryPlotView.h"
#include "plots/ResultPlotView.h"
#include "xfoil/gui/XFoilAnalysisWidget.h"

namespace {

struct GlobalSnapshot {
  QCoreApplication* application = nullptr;
  QString cwd;
  QString styleSheet;
  qint64 paletteCacheKey = 0;
  QString localeName;
  QStringList cwdEntries;
};

QStringList cwdEntries(const QString& path) {
  return QDir(path).entryList(
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
      QDir::Name);
}

GlobalSnapshot captureGlobalSnapshot() {
  GlobalSnapshot snapshot;
  snapshot.application = QCoreApplication::instance();
  snapshot.cwd = QDir::currentPath();
  snapshot.styleSheet = qApp->styleSheet();
  snapshot.paletteCacheKey = qApp->palette().cacheKey();
  snapshot.localeName = QLocale().name();
  snapshot.cwdEntries = cwdEntries(snapshot.cwd);
  return snapshot;
}

}  // namespace

using xfoil::gui::XFoilAnalysisWidget;

class XFoilAnalysisWidgetShellTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("offscreen"));
    before_ = captureGlobalSnapshot();
  }

  void cleanup() {
    QCOMPARE(QCoreApplication::instance(), before_.application);
    QCOMPARE(QDir::currentPath(), before_.cwd);
    QCOMPARE(qApp->styleSheet(), before_.styleSheet);
    QCOMPARE(qApp->palette().cacheKey(), before_.paletteCacheKey);
    QCOMPARE(QLocale().name(), before_.localeName);
    QCOMPARE(cwdEntries(before_.cwd), before_.cwdEntries);
  }

  void constructsWithoutApplicationOrMainWindow() {
    XFoilAnalysisWidget widget;
    QVERIFY(QApplication::instance() != nullptr);
    QVERIFY(qobject_cast<QMainWindow*>(&widget) == nullptr);
    QVERIFY(widget.findChildren<QMainWindow*>().isEmpty());
    QVERIFY(widget.findChild<QFrame*>("definitionsFrame") != nullptr);
  }

  void shellUsesSemanticStructure() {
    XFoilAnalysisWidget widget;

    QVERIFY(widget.findChild<QFrame*>("definitionsFrame") != nullptr);
    QVERIFY(widget.findChild<QScrollArea*>("definitionsScrollArea") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("definitionsRow") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("resultWorkspace") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("leftPlotColumn") != nullptr);
    QVERIFY(widget.findChild<QFrame*>("resultsColumn") != nullptr);
    QVERIFY(widget.findChild<QSplitter*>("mainSplitter") != nullptr);
    QVERIFY(widget.findChild<QSplitter*>("resultSplitter") != nullptr);
    QVERIFY(widget.findChild<QSplitter*>("leftPlotSplitter") != nullptr);
    QVERIFY(widget.findChild<QFrame*>("analysisActionFrame") != nullptr);
    QVERIFY(widget.findChild<QTabWidget*>("resultInfoTabs") != nullptr);
    QVERIFY(widget.findChild<QScrollArea*>(
                 "comparisonPolarsScrollArea") != nullptr);

    QVERIFY(widget.findChild<QGroupBox*>("geometryGroup") != nullptr);
    QVERIFY(widget.findChild<QGroupBox*>("panelingGroup") != nullptr);
    QVERIFY(widget.findChild<QGroupBox*>("operatingGroup") != nullptr);
    QVERIFY(widget.findChild<QGroupBox*>("targetGroup") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("definitionsPlaceholder") == nullptr);
    QVERIFY(widget.findChild<QWidget*>("airfoilPlot") != nullptr);
    QVERIFY(widget.findChild<QWidget*>("cpPlot") != nullptr);
    QVERIFY(widget.findChild<QPlainTextEdit*>("setupDiagnosticsView") != nullptr);
    QVERIFY(widget.findChild<QPushButton*>("analyzeButton") != nullptr);
    QVERIFY(widget.findChild<QPushButton*>("cancelButton") == nullptr);
    QVERIFY(widget.findChild<QProgressBar*>("analysisProgressBar") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("analysisCurrentPointLabel") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("analysisStatusLabel") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("statusLabel") == nullptr);
    QVERIFY(widget.findChild<QLabel*>("workflowStateLabel") == nullptr);
    QVERIFY(widget.findChild<QRadioButton*>("geometryImportRadio") != nullptr);
    QVERIFY(widget.findChild<QRadioButton*>("geometryNacaRadio") != nullptr);
    QVERIFY(widget.findChild<QRadioButton*>("geometryNacsRadio") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("geometryImportNoticeLabel") != nullptr);
    QVERIFY(widget.findChild<QTabWidget*>("analysisTargetTabs") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("targetStatusLabel") == nullptr);
    QVERIFY(widget.findChild<QLabel*>("panelStatusLabel") == nullptr);
    QVERIFY(widget.findChild<QLabel*>("geometryStatusLabel") == nullptr);
    QVERIFY(widget.findChild<QComboBox*>("targetModeCombo") == nullptr);
    QVERIFY(widget.findChild<QComboBox*>("geometrySourceCombo") == nullptr);
    QCOMPARE(widget.findChild<QCheckBox*>("showPanelingCheckBox")
                 ->accessibleName(),
             QStringLiteral("Show paneling"));
    QCOMPARE(widget.findChild<QPushButton*>("generatePanelButton")->text(),
             QStringLiteral("Generate Panels"));
    QVERIFY(widget.findChild<QTabWidget*>("panelingSettingsTabs") != nullptr);
    QVERIFY(widget.findChild<QTabWidget*>("conditionSettingsTabs") != nullptr);

    QTabWidget* tabs = widget.findChild<QTabWidget*>("resultPlotTabs");
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 7);
    QVERIFY(widget.findChild<QPlainTextEdit*>("reportView") != nullptr);
  }

  void g2StructureUsesSemanticNamesAndPromotedPlot() {
    XFoilAnalysisWidget widget;
    const QStringList required = {
        QStringLiteral("geometryGroup"),
        QStringLiteral("geometryImportRadio"),
        QStringLiteral("loadAirfoilButton"),
        QStringLiteral("geometryNacaRadio"),
        QStringLiteral("geometryNacsRadio"),
        QStringLiteral("geometryImportNoticeLabel"),
        QStringLiteral("nacaDesignationEdit"),
        QStringLiteral("applyGeometryButton"),
        QStringLiteral("panelCountSpin"),
        QStringLiteral("bunchingSpin"),
        QStringLiteral("teToLeSpin"),
        QStringLiteral("refinementSpin"),
        QStringLiteral("generatePanelButton"),
        QStringLiteral("reynoldsSpin"),
        QStringLiteral("machSpin"),
        QStringLiteral("viscousCheck"),
        QStringLiteral("iterationsSpin"),
        QStringLiteral("ncritSpin"),
        QStringLiteral("xtrUpperSpin"),
        QStringLiteral("xtrLowerSpin"),
        QStringLiteral("analysisTargetTabs"),
        QStringLiteral("alphaTargetSpin"),
        QStringLiteral("clTargetSpin"),
        QStringLiteral("sequenceStartSpin"),
        QStringLiteral("sequenceEndSpin"),
        QStringLiteral("sequenceStepSpin"),
        QStringLiteral("airfoilPlot"),
        QStringLiteral("resultsColumn"),
        QStringLiteral("analysisActionFrame"),
        QStringLiteral("setupDiagnosticsView"),
        QStringLiteral("analyzeButton"),
        QStringLiteral("analysisProgressBar"),
        QStringLiteral("analysisCurrentPointLabel"),
        QStringLiteral("analysisIterationLabel"),
        QStringLiteral("analysisStatusLabel"),
        QStringLiteral("resultInfoTabs"),
    };
    for (const QString& name : required) {
      QVERIFY2(widget.findChild<QWidget*>(name) != nullptr,
               qPrintable(name));
    }
    QVERIFY(widget.findChild<QButtonGroup*>("geometrySourceButtonGroup") !=
            nullptr);

    auto* plotWidget = widget.findChild<QWidget*>("airfoilPlot");
    QVERIFY(plotWidget != nullptr);
    auto* plot = dynamic_cast<xfoil::gui::GeometryPlotView*>(plotWidget);
    QVERIFY(plot != nullptr);
    QCOMPARE(plot->objectName(), QStringLiteral("airfoilPlot"));

    auto* cpWidget = widget.findChild<QWidget*>("cpPlot");
    QVERIFY(cpWidget != nullptr);
    QVERIFY(dynamic_cast<xfoil::gui::ResultPlotView*>(cpWidget) != nullptr);

    const QRegularExpression generic(
        QStringLiteral("^(frame|label|groupBox|widget|layoutWidget)_?\\d+$"));
    for (QWidget* child : widget.findChildren<QWidget*>()) {
      const QString name = child->objectName();
      QVERIFY2(!generic.match(name).hasMatch(), qPrintable(name));
    }

    QVERIFY(!widget.findChild<QRadioButton*>(
                 QStringLiteral("geometryImportRadio"))
                 ->accessibleName()
                 .isEmpty());
    QVERIFY(!widget.findChild<QSpinBox*>(QStringLiteral("panelCountSpin"))
                 ->toolTip()
                 .isEmpty());
  }

  void parentOwnershipIsQtManaged() {
    auto* host = new QWidget;
    auto* widget = new XFoilAnalysisWidget(host);
    QPointer<XFoilAnalysisWidget> guard(widget);

    QCOMPARE(widget->parentWidget(), host);
    delete host;

    QVERIFY(guard.isNull());
  }

  void embedsInHostLayout() {
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* widget = new XFoilAnalysisWidget(&host);
    layout->addWidget(widget);

    QCOMPARE(layout->count(), 1);
    QVERIFY(layout->itemAt(0)->widget() == widget);
    QCOMPARE(widget->parentWidget(), &host);

    QTest::ignoreMessage(
        QtWarningMsg,
        "This plugin does not support propagateSizeHints()");
    host.show();
    QCoreApplication::processEvents();
    QVERIFY(widget->isVisible());
  }

  void twoWidgetsRemainIndependent() {
    auto* hostA = new QWidget;
    auto* hostB = new QWidget;
    auto* first = new XFoilAnalysisWidget(hostA);
    auto* second = new XFoilAnalysisWidget(hostB);
    QPointer<XFoilAnalysisWidget> firstGuard(first);
    QPointer<XFoilAnalysisWidget> secondGuard(second);

    QVERIFY(first != second);
    QCOMPARE(first->parentWidget(), hostA);
    QCOMPARE(second->parentWidget(), hostB);
    QVERIFY(first->findChild<QFrame*>("resultsColumn") !=
            second->findChild<QFrame*>("resultsColumn"));

    delete hostA;
    QVERIFY(firstGuard.isNull());
    QVERIFY(!secondGuard.isNull());

    delete hostB;
    QVERIFY(secondGuard.isNull());
  }

 private:
  GlobalSnapshot before_;
};

int runXFoilAnalysisWidgetShellTests(int argc, char** argv) {
  XFoilAnalysisWidgetShellTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilAnalysisWidgetShellTests.moc"

// End: tests/gui/XFoilAnalysisWidgetShellTests.cpp

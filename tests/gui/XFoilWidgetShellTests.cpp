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

// File: tests/gui/XFoilWidgetShellTests.cpp
// Offscreen construction, embedding, ownership and global-side-effect
// tests for the public XFoilWidget shell.

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QLocale>
#include <QMainWindow>
#include <QPalette>
#include <QPointer>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QtTest/QTest>

#include "xfoil/gui/XFoilWidget.h"

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

using xfoil::gui::XFoilWidget;

class XFoilWidgetShellTests : public QObject {
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
    XFoilWidget widget;
    QVERIFY(QApplication::instance() != nullptr);
    QVERIFY(qobject_cast<QMainWindow*>(&widget) == nullptr);
    QVERIFY(widget.findChildren<QMainWindow*>().isEmpty());
    QVERIFY(widget.findChild<QFrame*>("definitionsFrame") != nullptr);
  }

  void shellUsesSemanticStructure() {
    XFoilWidget widget;

    QVERIFY(widget.findChild<QFrame*>("definitionsFrame") != nullptr);
    QVERIFY(widget.findChild<QSplitter*>("workspaceSplitter") != nullptr);
    QVERIFY(widget.findChild<QTabWidget*>("plotTabs") != nullptr);
    QVERIFY(widget.findChild<QFrame*>("resultsPanel") != nullptr);
    QVERIFY(widget.findChild<QFrame*>("statusFrame") != nullptr);

    QVERIFY(widget.findChild<QLabel*>("definitionsPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("geometryPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("cpPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("cfPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("polarPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("resultsPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("diagnosticsPlaceholder") != nullptr);
    QVERIFY(widget.findChild<QLabel*>("statusLabel") != nullptr);

    QTabWidget* tabs = widget.findChild<QTabWidget*>("plotTabs");
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 4);
    QCOMPARE(tabs->widget(0), widget.findChild<QWidget*>("geometryPage"));
    QCOMPARE(tabs->widget(1), widget.findChild<QWidget*>("cpPage"));
    QCOMPARE(tabs->widget(2), widget.findChild<QWidget*>("cfPage"));
    QCOMPARE(tabs->widget(3), widget.findChild<QWidget*>("polarPage"));
  }

  void parentOwnershipIsQtManaged() {
    auto* host = new QWidget;
    auto* widget = new XFoilWidget(host);
    QPointer<XFoilWidget> guard(widget);

    QCOMPARE(widget->parentWidget(), host);
    delete host;

    QVERIFY(guard.isNull());
  }

  void embedsInHostLayout() {
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* widget = new XFoilWidget(&host);
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
    auto* first = new XFoilWidget(hostA);
    auto* second = new XFoilWidget(hostB);
    QPointer<XFoilWidget> firstGuard(first);
    QPointer<XFoilWidget> secondGuard(second);

    QVERIFY(first != second);
    QCOMPARE(first->parentWidget(), hostA);
    QCOMPARE(second->parentWidget(), hostB);
    QVERIFY(first->findChild<QFrame*>("resultsPanel") !=
            second->findChild<QFrame*>("resultsPanel"));

    delete hostA;
    QVERIFY(firstGuard.isNull());
    QVERIFY(!secondGuard.isNull());

    delete hostB;
    QVERIFY(secondGuard.isNull());
  }

 private:
  GlobalSnapshot before_;
};

int runXFoilWidgetShellTests(int argc, char** argv) {
  XFoilWidgetShellTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "XFoilWidgetShellTests.moc"

// End: tests/gui/XFoilWidgetShellTests.cpp

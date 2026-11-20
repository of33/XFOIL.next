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

// File: tests/gui/QCustomPlotSmoke.cpp
// Offscreen smoke for the vendored QCustomPlot 2.1.1 source and the
// Widgets/PrintSupport link. It creates no file and performs no printing.

#include <QApplication>
#include <QDir>
#include <QStringList>
#include <QVector>
#include <QtTest/QTest>

#include "qcustomplot.h"

class QCustomPlotSmoke : public QObject {
  Q_OBJECT

 private slots:
  void constructsGraphAndReplots() {
    QVERIFY(QApplication::instance() != nullptr);

    const QString workingDirectory = QDir::currentPath();
    const QStringList before = QDir(workingDirectory).entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
        QDir::Name);

    QCustomPlot plot;
    QCOMPARE(plot.graphCount(), 0);

    plot.addGraph();
    QCOMPARE(plot.graphCount(), 1);

    QCPGraph* graph = plot.graph(0);
    QVERIFY(graph != nullptr);

    const QVector<double> keys{0.0, 1.0, 2.0, 3.0};
    const QVector<double> values{0.0, 1.0, 0.5, 1.5};
    graph->setData(keys, values);
    QCOMPARE(graph->dataCount(), 4);

    plot.rescaleAxes();
    plot.replot();
    QCOMPARE(graph->dataCount(), 4);

    const QStringList after = QDir(workingDirectory).entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
        QDir::Name);
    QCOMPARE(after, before);
  }
};

int runQCustomPlotSmoke(int argc, char** argv) {
  QCustomPlotSmoke test;
  return QTest::qExec(&test, argc, argv);
}

#include "QCustomPlotSmoke.moc"

// End: tests/gui/QCustomPlotSmoke.cpp

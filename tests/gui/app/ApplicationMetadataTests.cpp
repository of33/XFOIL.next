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

// File: tests/gui/app/ApplicationMetadataTests.cpp
// The standalone GUI identity is separate from the embeddable widget API.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QtTest/QTest>

#include "app/ApplicationMetadata.h"

class ApplicationMetadataTests : public QObject {
  Q_OBJECT

 private slots:
  void setsStandaloneIdentity() {
    xfoil::gui::app::configureApplicationMetadata();
    QCOMPARE(QCoreApplication::organizationName(),
             QStringLiteral("XFOIL.next"));
    QCOMPARE(QCoreApplication::applicationName(),
             QStringLiteral("XFOIL.next GUI"));
    QCOMPARE(QGuiApplication::applicationDisplayName(),
             QStringLiteral("XFOIL.next GUI"));
    QCOMPARE(QCoreApplication::applicationVersion(),
             QStringLiteral("1.2.1"));
  }
};

int runApplicationMetadataTests(int argc, char** argv) {
  ApplicationMetadataTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "ApplicationMetadataTests.moc"

// End: tests/gui/app/ApplicationMetadataTests.cpp

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

// File: tests/gui/app/GuiFileAdaptersTests.cpp
// App-only adapter tests: existing DAT reader, labeled writer and DXF R12
// encoder contracts, including prompts represented as statuses.

#include <clocale>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "app/GuiFileAdapters.h"

namespace {

using xfoil::gui::app::AirfoilFileReadStatus;
using xfoil::gui::app::exportDxfFile;
using xfoil::gui::app::readAirfoilFile;
using xfoil::gui::app::readPolarFile;
using xfoil::gui::app::saveAirfoilFile;
using xfoil::gui::app::saveBinaryFile;
using xfoil::gui::app::savePolarFile;
using xfoil::gui::app::saveTextFile;

bool writeText(const std::filesystem::path& path, const char* text) {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return file.write(text) == static_cast<qint64>(std::char_traits<char>::length(text));
}

QString readText(const std::filesystem::path& path) {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return QString::fromUtf8(file.readAll());
}

std::filesystem::path pathIn(const QTemporaryDir& directory,
                             const char* name) {
  return std::filesystem::path(directory.path().toStdString()) / name;
}

xfoil::geometry::AirfoilGeometry diamond() {
  xfoil::geometry::AirfoilGeometry geometry;
  const std::vector<xfoil::Point2D> points = {
      {1.0, 0.0}, {0.5, 0.08}, {0.0, 0.0}, {0.5, -0.08}, {1.0, 0.0}};
  const auto assigned = geometry.assign("diamond", points);
  Q_ASSERT(assigned.success());
  return geometry;
}

}  // namespace

class GuiFileAdaptersTests : public QObject {
  Q_OBJECT

 private slots:
  void readsLabeledFile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = pathIn(directory, "labeled.dat");
    QVERIFY(writeText(path,
                      "labeled\n"
                      "1.0 0.0\n"
                      "0.5 0.08\n"
                      "0.0 0.0\n"
                      "0.5 -0.08\n"
                      "1.0 0.0\n"));
    const auto result = readAirfoilFile(path);
    QVERIFY2(result.status == AirfoilFileReadStatus::success,
             result.message.c_str());
    QCOMPARE(QString::fromStdString(std::string(result.geometry.name())),
             QStringLiteral("labeled"));
    QCOMPARE(result.geometry.pointCount(), std::size_t{5});
  }

  void genericFileRequestsAndUsesName() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = pathIn(directory, "generic.dat");
    QVERIFY(writeText(path,
                      "0.0 0.0\n"
                      "1.0 0.0\n"
                      "0.5 0.08\n"
                      "0.0 0.0\n"));
    const auto pending = readAirfoilFile(path);
    QVERIFY2(pending.status == AirfoilFileReadStatus::nameRequired,
             pending.message.c_str());

    const auto result = readAirfoilFile(path, 0, "my profile");
    QVERIFY(result.status == AirfoilFileReadStatus::success);
    QCOMPARE(QString::fromStdString(std::string(result.geometry.name())),
             QStringLiteral("my profile"));
  }

  void multiElementFileRequestsSelection() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = pathIn(directory, "multi.dat");
    QVERIFY(writeText(path,
                      "multi\n"
                      "1.0 2.0 3.0 4.0\n"
                      "1.0 0.0\n"
                      "0.5 0.08\n"
                      "0.0 0.0\n"
                      "999.0 999.0\n"
                      "0.0 0.0\n"
                      "0.5 -0.08\n"
                      "1.0 0.0\n"
                      "999.0 999.0\n"));
    const auto pending = readAirfoilFile(path);
    QVERIFY2(pending.status ==
                 AirfoilFileReadStatus::elementSelectionRequired,
             pending.message.c_str());

    const auto second = readAirfoilFile(path, 2);
    QVERIFY(second.status == AirfoilFileReadStatus::success);
    QCOMPARE(second.geometry.pointCount(), std::size_t{3});
  }

  void missingFileReportsError() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto result = readAirfoilFile(pathIn(directory, "missing.dat"));
    QVERIFY(result.status == AirfoilFileReadStatus::error);
    QVERIFY(!result.message.empty());
  }

  void savesLabeledAirfoilAndHonorsOverwrite() {
    const char* const locale = std::setlocale(LC_NUMERIC, nullptr);
    const std::string savedLocale = locale != nullptr ? locale : "C";
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = pathIn(directory, "saved.dat");
    const auto geometry = diamond();

    QVERIFY(saveAirfoilFile(path, geometry, false).success);
    const QString text = readText(path);
    QVERIFY(text.startsWith(QStringLiteral("diamond\n")));
    QCOMPARE(text.count(QLatin1Char('\n')), 6);
    QVERIFY(!text.contains(QLatin1Char(',')));

    const auto refused = saveAirfoilFile(path, geometry, false);
    QVERIFY(!refused.success);
    QVERIFY(!refused.message.empty());

    QVERIFY(saveAirfoilFile(path, geometry, true).success);
    QCOMPARE(readText(path), text);
    const char* const after = std::setlocale(LC_NUMERIC, nullptr);
    QCOMPARE(after != nullptr ? std::string(after) : std::string{"C"},
             savedLocale);
  }

  void exportsDxfAndHonorsOverwrite() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = pathIn(directory, "panel.dxf");
    xfoil::SurfaceGeometry panel;
    panel.points = {{1.0, 0.0}, {0.5, 0.08}, {0.0, 0.0}};

    QVERIFY(exportDxfFile(path, panel, false).success);
    const QString text = readText(path);
    QVERIFY2(text.startsWith(QStringLiteral("999\n")),
             text.left(80).toUtf8().constData());
    QVERIFY(text.contains(QStringLiteral("LINE")));

    const auto refused = exportDxfFile(path, panel, false);
    QVERIFY(!refused.success);
    QVERIFY(exportDxfFile(path, panel, true).success);
    QCOMPARE(readText(path), text);
  }

  void readsAndWritesPolarTextThroughOwningValues() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const char* const locale = std::setlocale(LC_NUMERIC, nullptr);
    const std::string savedLocale = locale != nullptr ? locale : "C";
    const auto fixture = std::filesystem::path(
        "tests/data/xfoil-6.996/s11/canonical-base.pol");
    const auto read = readPolarFile(fixture);
    QVERIFY2(read.isOk(), read.isErr() ? read.error().message.c_str() : "");
    QCOMPARE(read.value().metadata.name, std::string{"NACA 0012"});
    QCOMPARE(read.value().points.size(), std::size_t{2});

    const auto savedPath = pathIn(directory, "output.pol");
    const auto written = savePolarFile(savedPath, read.value(), false);
    QVERIFY2(written.success, written.message.c_str());
    const auto roundtrip = readPolarFile(savedPath);
    QVERIFY2(roundtrip.isOk(),
             roundtrip.isErr() ? roundtrip.error().message.c_str() : "");
    QCOMPARE(roundtrip.value().metadata.name, read.value().metadata.name);
    QCOMPARE(roundtrip.value().points.size(), std::size_t{2});
    QCOMPARE(roundtrip.value().points.front().liftCoefficient, 0.1);
    QVERIFY(!readText(savedPath).contains(QLatin1Char(',')));
    const char* const after = std::setlocale(LC_NUMERIC, nullptr);
    QCOMPARE(after != nullptr ? std::string(after) : std::string{"C"},
             savedLocale);
    QVERIFY(!savePolarFile(savedPath, read.value(), false).success);
    QVERIFY(savePolarFile(savedPath, read.value(), true).success);
    QVERIFY(!readPolarFile(pathIn(directory, "missing.pol")).isOk());
    QVERIFY(!savePolarFile({}, read.value(), true).success);
  }

  void writesTextAndOpaqueBytesWithoutExtensionChanges() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto textPath = pathIn(directory, "report.out");
    const std::string text = "line one\nline two\n";
    QVERIFY(saveTextFile(textPath, text, false).success);
    QCOMPARE(readText(textPath), QString::fromStdString(text));
    QVERIFY(!saveTextFile(textPath, "changed", false).success);
    QVERIFY(saveTextFile(textPath, "changed", true).success);
    QCOMPARE(readText(textPath), QStringLiteral("changed"));

    const auto binaryPath = pathIn(directory, "plot.bin");
    const std::string bytes("\x00\x01\x7f\x80payload", 11);
    QVERIFY(saveBinaryFile(binaryPath, bytes, false).success);
    QFile file(QString::fromStdString(binaryPath.string()));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray(bytes.data(),
                                        static_cast<qsizetype>(bytes.size())));
    QVERIFY(!saveBinaryFile({}, bytes, true).success);
  }

  void emptyTargetsAreRejected() {
    const auto geometry = diamond();
    QVERIFY(!saveAirfoilFile({}, geometry, true).success);
    QVERIFY(!exportDxfFile({}, xfoil::SurfaceGeometry{}, true).success);
  }
};

int runGuiFileAdaptersTests(int argc, char** argv) {
  GuiFileAdaptersTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiFileAdaptersTests.moc"

// End: tests/gui/app/GuiFileAdaptersTests.cpp

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

// File: tests/gui/GuiSessionBackendTests.cpp
// Public-session setup adapter tests. No solve operation is invoked.

#include <string>
#include <vector>

#include <QtTest/QTest>

#include "backend/GuiSessionBackend.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace {

using xfoil::AnalysisConfig;
using xfoil::PanelingOptions;
using xfoil::gui::GuiSessionBackend;
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

}  // namespace

class GuiSessionBackendTests : public QObject {
  Q_OBJECT

 private slots:
  void memoryGeometryPanelAndConfiguration() {
    GuiSessionBackend backend;
    QVERIFY(backend.setGeometry(diamond()).isOk());

    const auto panel = backend.generatePanel(PanelingOptions{});
    QVERIFY(panel.isOk());
    QVERIFY(!panel.value().points.empty());

    QVERIFY(backend.setConfiguration(AnalysisConfig{}).isOk());
  }

  void nacaAndNacsUsePublicSession() {
    GuiSessionBackend backend;
    const auto naca = backend.setNaca("2412", false);
    QVERIFY(naca.isOk());
    QVERIFY(!naca.value().empty());

    const auto nacs = backend.setNaca("0012", true);
    QVERIFY(nacs.isOk());
    QVERIFY(!nacs.value().empty());
    QVERIFY(naca.value().pointCount() == nacs.value().pointCount());
  }

  void invalidInputsAreReportedAndSessionRemainsUsable() {
    GuiSessionBackend backend;
    const auto badNaca = backend.setNaca("not-a-naca", false);
    QVERIFY(badNaca.isErr());
    QVERIFY(!badNaca.error().message.empty());

    QVERIFY(backend.setNaca("2412", false).isOk());

    PanelingOptions invalidPanel;
    invalidPanel.panelCount = 1000;
    QVERIFY(backend.generatePanel(invalidPanel).isErr());

    const auto validPanel = backend.generatePanel(PanelingOptions{});
    QVERIFY(validPanel.isOk());

    AnalysisConfig invalidConfiguration;
    invalidConfiguration.reynoldsNumber = -1.0;
    QVERIFY(backend.setConfiguration(invalidConfiguration).isErr());

    AnalysisConfig validConfiguration;
    validConfiguration.reynoldsNumber = 1.0e6;
    QVERIFY(backend.setConfiguration(validConfiguration).isOk());
  }
};

int runGuiSessionBackendTests(int argc, char** argv) {
  GuiSessionBackendTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiSessionBackendTests.moc"

// End: tests/gui/GuiSessionBackendTests.cpp

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

// File: tests/gui/GuiPolarManagerTests.cpp
// G6c manager ownership, stable IDs, ordering, visibility, selection,
// limit and removal semantics.

#include <cstdint>
#include <string>

#include <QtTest/QTest>

#include "model/GuiPolarManager.h"

namespace {

using xfoil::PolarData;
using xfoil::PolarPoint;
using xfoil::gui::GuiPolarManager;
using xfoil::gui::GuiPolarOrigin;

PolarData polar(std::string name) {
  PolarData value;
  value.metadata.name = std::move(name);
  PolarPoint point;
  point.alpha = xfoil::Degrees{1.0};
  point.liftCoefficient = 0.5;
  point.dragCoefficient = 0.02;
  point.momentCoefficient = -0.1;
  point.convergence = xfoil::ConvergenceStatus::converged;
  value.points.push_back(point);
  return value;
}

std::string entryNames(const GuiPolarManager& manager) {
  std::string names;
  for (const auto& entry : manager.entries()) {
    if (!names.empty()) {
      names += ',';
    }
    names += entry.name;
  }
  return names;
}

}  // namespace

class GuiPolarManagerTests : public QObject {
  Q_OBJECT

 private slots:
  void addUsesStableIdsIndependentOfNames() {
    GuiPolarManager manager;
    const auto first = manager.add(polar("same"), GuiPolarOrigin::imported);
    const auto second = manager.add(polar("same"), GuiPolarOrigin::imported);
    QVERIFY(first.isOk());
    QVERIFY(second.isOk());
    QVERIFY(first.value() != second.value());
    QCOMPARE(manager.entries().size(), std::size_t{2});
    QVERIFY(manager.selectedId().has_value());
    QCOMPARE(*manager.selectedId(), second.value());
  }

  void emptyPolarAndNinthComparisonAreRejected() {
    GuiPolarManager manager;
    const auto empty = manager.add(PolarData{}, GuiPolarOrigin::imported);
    QVERIFY(empty.isErr());
    QCOMPARE(empty.error().code, std::string{"gui.polarEmpty"});

    for (std::size_t index = 0; index < GuiPolarManager::maxComparisons;
         ++index) {
      QVERIFY(manager.add(polar("p" + std::to_string(index)),
                          GuiPolarOrigin::imported)
                  .isOk());
    }
    const auto ninth = manager.add(polar("ninth"), GuiPolarOrigin::imported);
    QVERIFY(ninth.isErr());
    QCOMPARE(ninth.error().code, std::string{"gui.polarLimitReached"});
    QCOMPARE(manager.entries().size(), GuiPolarManager::maxComparisons);
  }

  void renameVisibilityAndOrderAreExplicit() {
    GuiPolarManager manager;
    const auto a = manager.add(polar("A"), GuiPolarOrigin::imported);
    const auto b = manager.add(polar("B"), GuiPolarOrigin::imported);
    QVERIFY(a.isOk());
    QVERIFY(b.isOk());

    QVERIFY(manager.rename(a.value(), "Primary").isOk());
    QCOMPARE(manager.entries().front().name, std::string{"Primary"});
    QCOMPARE(manager.entries().front().polar.metadata.name,
             std::string{"Primary"});
    QVERIFY(!manager.rename(a.value(), "").isOk());

    QVERIFY(manager.setVisible(a.value(), false).isOk());
    QVERIFY(!manager.entries().front().visible);
    QVERIFY(manager.setVisible(a.value(), true).isOk());
    QVERIFY(manager.entries().front().visible);

    QVERIFY(manager.moveDown(a.value()).isOk());
    QCOMPARE(entryNames(manager), std::string{"B,Primary"});
    QVERIFY(manager.moveUp(a.value()).isOk());
    QCOMPARE(entryNames(manager), std::string{"Primary,B"});
  }

  void removeSelectsNeighborAndClearDropsSelection() {
    GuiPolarManager manager;
    const auto a = manager.add(polar("A"), GuiPolarOrigin::currentSnapshot);
    const auto b = manager.add(polar("B"), GuiPolarOrigin::imported);
    QVERIFY(a.isOk());
    QVERIFY(b.isOk());
    QVERIFY(manager.select(b.value()).isOk());
    QVERIFY(manager.remove(b.value()).isOk());
    QCOMPARE(manager.entries().size(), std::size_t{1});
    QVERIFY(manager.selectedId().has_value());
    QCOMPARE(*manager.selectedId(), a.value());
    QVERIFY(!manager.remove(b.value()).isOk());
    manager.clear();
    QVERIFY(manager.entries().empty());
    QVERIFY(!manager.selectedId().has_value());
  }
};

int runGuiPolarManagerTests(int argc, char** argv) {
  GuiPolarManagerTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiPolarManagerTests.moc"

// End: tests/gui/GuiPolarManagerTests.cpp

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

// File: tests/gui/GuiResultSelectionModelTests.cpp
// Solver-independent activate/deactivate, current-selection and formatter
// tests for the G2a point selection model.

#include <cstddef>
#include <optional>
#include <vector>

#include <QtTest/QTest>

#include "model/GuiResultSelectionModel.h"
#include "plots/GuiPointPresentation.h"

namespace {

using xfoil::gui::GuiPointEntry;
using xfoil::gui::GuiResultSelectionModel;
using xfoil::gui::formatConvergence;
using xfoil::gui::formatOperatingPointLabel;
using xfoil::gui::GuiConvergenceState;
using xfoil::gui::GuiConvergenceText;

std::vector<GuiPointEntry> samplePoints() {
  return {
      GuiPointEntry{0, 0.0, true, true},
      GuiPointEntry{1, 2.0, true, true},
      GuiPointEntry{2, 4.0, false, true},
      GuiPointEntry{3, 6.0, true, true},
      GuiPointEntry{4, 8.0, true, true},
  };
}

std::vector<std::size_t> sourceIndices(
    const std::vector<GuiPointEntry>& points) {
  std::vector<std::size_t> result;
  for (const GuiPointEntry& point : points) {
    result.push_back(point.sourceIndex);
  }
  return result;
}

}  // namespace

class GuiResultSelectionModelTests : public QObject {
  Q_OBJECT

 private slots:
  void emptyModelHasNoSelection() {
    GuiResultSelectionModel model;
    QVERIFY(model.empty());
    QCOMPARE(model.size(), std::size_t{0});
    QVERIFY(!model.currentSourceIndex().has_value());
    QVERIFY(!model.currentPoint().has_value());
    QVERIFY(model.activePoints().empty());
    QVERIFY(model.inactivePoints().empty());
    QVERIFY(!model.select(0));
    QVERIFY(!model.deactivate(0));
    QVERIFY(!model.activate(0));
  }

  void onePointIsSelectedDeterministically() {
    GuiResultSelectionModel model(
        {GuiPointEntry{42, 4.0, true, true}});
    QVERIFY(!model.empty());
    QVERIFY(model.currentSourceIndex().has_value());
    QCOMPARE(*model.currentSourceIndex(), std::size_t{42});
    QCOMPARE(model.currentPoint()->alpha, 4.0);
    QCOMPARE(model.activePoints().size(), std::size_t{1});
  }

  void formatterMarksNotConvergedOnly() {
    QCOMPARE(formatOperatingPointLabel(GuiPointEntry{0, 4.0, true, true}),
             std::string{"4.000"});
    QCOMPARE(formatOperatingPointLabel(GuiPointEntry{1, 6.0, false, true}),
             std::string{"6.000 (nc)"});
  }

  void convergenceTextCoversAllDisplayStates() {
    const auto noPoint = formatConvergence(GuiConvergenceState::noPoint);
    const auto converged = formatConvergence(GuiConvergenceState::converged);
    const auto notConverged =
        formatConvergence(GuiConvergenceState::notConverged);
    const auto cancelled = formatConvergence(GuiConvergenceState::cancelled);
    const auto failed = formatConvergence(GuiConvergenceState::failed);

    QCOMPARE(noPoint.label, std::string{"No point selected"});
    QCOMPARE(converged.label, std::string{"Converged"});
    QCOMPARE(notConverged.label, std::string{"Not converged"});
    QCOMPARE(cancelled.label, std::string{"Cancelled"});
    QCOMPARE(failed.label, std::string{"Failed"});
    QVERIFY(!noPoint.icon.empty());
    QVERIFY(!converged.icon.empty());
    QVERIFY(!notConverged.icon.empty());
    QVERIFY(!cancelled.icon.empty());
    QVERIFY(!failed.icon.empty());
  }

  void multiplePointsKeepSourceOrderAndSelection() {
    GuiResultSelectionModel model(samplePoints());
    QCOMPARE(*model.currentSourceIndex(), std::size_t{0});
    QVERIFY(model.select(2));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{2});
    QCOMPARE(model.currentPoint()->alpha, 4.0);
    QVERIFY(!model.currentPoint()->converged);

    QVERIFY(!model.select(99));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{2});
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{0, 1, 2, 3, 4}));
  }

  void deactivatingCurrentSelectsNextActiveAndWraps() {
    GuiResultSelectionModel model(samplePoints());
    QVERIFY(model.select(3));
    QVERIFY(model.deactivate(3));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{4});
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{0, 1, 2, 4}));
    QVERIFY(sourceIndices(model.inactivePoints()) ==
            (std::vector<std::size_t>{3}));

    QVERIFY(model.deactivate(4));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{0});
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{0, 1, 2}));

    QVERIFY(model.deactivate(0));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{1});
  }

  void deactivatingNonCurrentKeepsSelection() {
    GuiResultSelectionModel model(samplePoints());
    QVERIFY(model.select(4));
    QVERIFY(model.deactivate(1));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{4});
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{0, 2, 3, 4}));
  }

  void allDeactivatedClearsSelectionAndReactivationRestoresOrder() {
    GuiResultSelectionModel model(samplePoints());
    for (std::size_t index = 0; index < 5; ++index) {
      QVERIFY(model.deactivate(index));
    }
    QVERIFY(!model.currentSourceIndex().has_value());
    QVERIFY(model.activePoints().empty());
    QVERIFY(sourceIndices(model.inactivePoints()) ==
            (std::vector<std::size_t>{0, 1, 2, 3, 4}));
    QVERIFY(!model.select(2));

    QVERIFY(model.activate(3));
    QVERIFY(model.currentSourceIndex().has_value());
    QCOMPARE(*model.currentSourceIndex(), std::size_t{3});
    QVERIFY(model.activate(1));
    QVERIFY(model.activate(4));
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{1, 3, 4}));
    QVERIFY(sourceIndices(model.inactivePoints()) ==
            (std::vector<std::size_t>{0, 2}));
  }

  void duplicateAlphaUsesSourceIndexIdentity() {
    GuiResultSelectionModel model({
        GuiPointEntry{10, 4.0, true, true},
        GuiPointEntry{11, 4.0, false, true},
        GuiPointEntry{12, 4.0, true, true},
    });
    QVERIFY(model.select(11));
    QCOMPARE(model.currentPoint()->sourceIndex, std::size_t{11});
    QVERIFY(model.deactivate(10));
    QCOMPARE(*model.currentSourceIndex(), std::size_t{11});
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{11, 12}));
    QVERIFY(model.activate(10));
    QVERIFY(sourceIndices(model.activePoints()) ==
            (std::vector<std::size_t>{10, 11, 12}));
  }

  void setPointsRevalidatesCurrentSelection() {
    GuiResultSelectionModel model(samplePoints());
    QVERIFY(model.select(3));
    model.setPoints({GuiPointEntry{3, 6.0, true, true},
                     GuiPointEntry{7, 7.0, true, true}});
    QCOMPARE(*model.currentSourceIndex(), std::size_t{3});
    model.setPoints({GuiPointEntry{7, 7.0, true, true}});
    QCOMPARE(*model.currentSourceIndex(), std::size_t{7});
  }
};

int runGuiResultSelectionModelTests(int argc, char** argv) {
  GuiResultSelectionModelTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiResultSelectionModelTests.moc"

// End: tests/gui/GuiResultSelectionModelTests.cpp

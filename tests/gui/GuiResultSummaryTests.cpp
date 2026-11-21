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

// File: tests/gui/GuiResultSummaryTests.cpp
// Direct, exact, interpolated, sampled-provisional and exclusion tests for
// the Qt-independent GUI result summary.

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include <QtTest/QTest>

#include "model/GuiResultSummary.h"

namespace {

using xfoil::ConvergenceStatus;
using xfoil::gui::GuiPointEntry;
using xfoil::gui::GuiResultProvenance;
using xfoil::gui::GuiResultSummary;
using xfoil::gui::computeGuiResultSummary;
using xfoil::gui::formatResultValue;

GuiPointEntry point(std::size_t sourceIndex, double alpha, double cl,
                    double cd, double cm, bool converged = true,
                    bool enabled = true) {
  GuiPointEntry entry;
  entry.sourceIndex = sourceIndex;
  entry.alpha = alpha;
  entry.converged = converged;
  entry.enabled = enabled;
  entry.convergenceStatus =
      converged ? ConvergenceStatus::converged
                : ConvergenceStatus::unconverged;
  entry.polar.alpha = xfoil::Degrees{alpha};
  entry.polar.liftCoefficient = cl;
  entry.polar.dragCoefficient = cd;
  entry.polar.momentCoefficient = cm;
  entry.polar.convergence = entry.convergenceStatus;
  return entry;
}

}  // namespace

class GuiResultSummaryTests : public QObject {
  Q_OBJECT

 private slots:
  void cl0IsExactAtAlphaZero() {
    const std::vector<GuiPointEntry> points = {
        point(0, -1.0, -0.1, 0.02, 0.01),
        point(1, 0.0, 0.25, 0.015, 0.0),
        point(2, 1.0, 0.6, 0.02, -0.01)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.cl0.has_value());
    QCOMPARE(*summary.cl0, 0.25);
  }

  void cl0InterpolatesWithoutExtrapolation() {
    const std::vector<GuiPointEntry> points = {
        point(0, -2.0, -0.2, 0.03, 0.0),
        point(1, 2.0, 0.6, 0.03, 0.0)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.cl0.has_value());
    QCOMPARE(*summary.cl0, 0.2);

    const std::vector<GuiPointEntry> positive = {
        point(0, 1.0, 0.2, 0.03, 0.0),
        point(1, 2.0, 0.4, 0.03, 0.0)};
    QVERIFY(!computeGuiResultSummary(positive, {}).cl0.has_value());
  }

  void cd0IsMinimumSampledDrag() {
    const std::vector<GuiPointEntry> points = {
        point(0, -1.0, 0.0, 0.04, 0.0),
        point(1, 0.0, 0.2, 0.012, 0.0),
        point(2, 1.0, 0.4, 0.02, 0.0),
        point(3, 2.0, 0.5, -0.1, 0.0)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.cd0.has_value());
    QCOMPARE(*summary.cd0, 0.012);
  }

  void alpha0AndCm0UseTheSameExactPoint() {
    const std::vector<GuiPointEntry> points = {
        point(0, -1.0, -0.2, 0.02, 0.1),
        point(1, 0.0, 0.0, 0.02, -0.2),
        point(2, 1.0, 0.2, 0.02, -0.1)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.alpha0.has_value());
    QVERIFY(summary.cm0.has_value());
    QCOMPARE(*summary.alpha0, 0.0);
    QCOMPARE(*summary.cm0, -0.2);
  }

  void alpha0AndCm0UseTheSameInterpolationPair() {
    const std::vector<GuiPointEntry> points = {
        point(0, -2.0, -0.2, 0.02, 0.1),
        point(1, 2.0, 0.2, 0.02, -0.3)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.alpha0.has_value());
    QVERIFY(summary.cm0.has_value());
    QCOMPARE(*summary.alpha0, 0.0);
    QCOMPARE(*summary.cm0, -0.1);
  }

  void multipleZeroCrossingsChooseSmallestAbsAlpha() {
    const std::vector<GuiPointEntry> points = {
        point(0, -3.0, -0.3, 0.02, 0.1),
        point(1, -2.0, 0.1, 0.02, 0.0),
        point(2, -1.0, -0.1, 0.02, 0.05),
        point(3, 1.0, 0.1, 0.02, -0.05),
        point(4, 2.0, -0.1, 0.02, 0.0),
        point(5, 3.0, 0.3, 0.02, -0.1)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.alpha0.has_value());
    QVERIFY(std::abs(*summary.alpha0) < 1.0e-12);
    QVERIFY(summary.cm0.has_value());
    QVERIFY(std::abs(*summary.cm0) < 1.0e-12);
  }

  void bracketedClMaximumIsQuadraticAndResolved() {
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0, 0.02, 0.0),
        point(1, 1.0, 0.8, 0.02, 0.0),
        point(2, 2.0, 1.0, 0.02, 0.0),
        point(3, 3.0, 0.9, 0.02, 0.0),
        point(4, 4.0, 0.7, 0.02, 0.0)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.clMaximum.has_value());
    QVERIFY(*summary.clMaximum > 1.0);
    QVERIFY(!summary.clMaximumProvisional);
    QVERIFY(summary.clMaximumProvenance ==
            GuiResultProvenance::interpolated);
  }

  void endpointClMaximumIsSampledProvisional() {
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0, 0.02, 0.0),
        point(1, 1.0, 0.5, 0.02, 0.0),
        point(2, 2.0, 1.0, 0.02, 0.0)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.clMaximum.has_value());
    QCOMPARE(*summary.clMaximum, 1.0);
    QVERIFY(summary.clMaximumProvisional);
    QVERIFY(summary.clMaximumProvenance ==
            GuiResultProvenance::sampledProvisional);
  }

  void efficiencyMaximumInterpolatesAndExcludesInvalidCd() {
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.2, 0.02, 0.0),
        point(1, 1.0, 0.6, 0.01, 0.0),
        point(2, 2.0, 0.8, 0.012, 0.0),
        point(3, 3.0, 0.7, 0.02, 0.0),
        point(4, 4.0, 99.0, 0.0, 0.0)};
    const GuiResultSummary summary = computeGuiResultSummary(points, {});
    QVERIFY(summary.efficiencyMaximum.has_value());
    QVERIFY(*summary.efficiencyMaximum > 60.0);
    QVERIFY(!summary.efficiencyMaximumProvisional);
  }

  void ncAndDisabledPointsAreExcludedFromSummary() {
    const std::vector<GuiPointEntry> points = {
        point(0, 0.0, 0.0, 0.02, 0.0),
        point(1, 1.0, 5.0, 0.0, 0.0, false, true),
        point(2, 2.0, 2.0, 0.01, 0.0, true, false),
        point(3, 3.0, 0.5, 0.02, 0.0)};
    const GuiResultSummary summary =
        computeGuiResultSummary(points, points[1]);
    QCOMPARE(summary.cd, 0.0);
    QCOMPARE(summary.cl, 5.0);
    QVERIFY(!summary.efficiency.has_value());
    QCOMPARE(summary.clMaximum.value(), 0.5);
    QVERIFY(summary.clMaximumProvisional);
    QVERIFY(summary.cd0.has_value());
    QCOMPARE(*summary.cd0, 0.02);
  }

  void resultValuesUseFixedSixDecimalPlaces() {
    QCOMPARE(QString::fromStdString(
                 formatResultValue(std::optional<double>{1.5})),
             QStringLiteral("1.500000"));
    QCOMPARE(QString::fromStdString(
                 formatResultValue(std::optional<double>{0.0})),
             QStringLiteral("0.000000"));
    QCOMPARE(QString::fromStdString(
                 formatResultValue(std::optional<double>{1.23456789})),
             QStringLiteral("1.234568"));
    QCOMPARE(QString::fromStdString(
                 formatResultValue(std::optional<double>{-0.0})),
             QStringLiteral("0.000000"));
    QCOMPARE(QString::fromStdString(formatResultValue(
                 std::optional<double>{
                     std::numeric_limits<double>::quiet_NaN()})),
             QStringLiteral("n/a"));
    QCOMPARE(QString::fromStdString(
                 formatResultValue(std::nullopt)),
             QStringLiteral("n/a"));
  }

  void emptyAndSinglePointData() {
    const GuiResultSummary empty = computeGuiResultSummary({}, {});
    QVERIFY(!empty.cl0.has_value());
    QVERIFY(!empty.clMaximum.has_value());
    QVERIFY(!empty.efficiencyMaximum.has_value());

    const std::vector<GuiPointEntry> one = {
        point(0, 2.0, 0.7, 0.02, -0.1)};
    const GuiResultSummary summary =
        computeGuiResultSummary(one, one.front());
    QCOMPARE(summary.cl, 0.7);
    QCOMPARE(summary.cd, 0.02);
    QVERIFY(!summary.cl0.has_value());
    QVERIFY(!summary.alpha0.has_value());
    QVERIFY(summary.clMaximum.has_value());
    QVERIFY(summary.clMaximumProvisional);
    QVERIFY(summary.efficiencyMaximumProvisional);
  }
};

int runGuiResultSummaryTests(int argc, char** argv) {
  GuiResultSummaryTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiResultSummaryTests.moc"

// End: tests/gui/GuiResultSummaryTests.cpp

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

// File: tests/gui/GuiDetailedSequenceContractTests.cpp
// GUI-side detailed-result ownership and plot-role contract. No analysis,
// no worker and no real plot binding is exercised here.

#include <cstddef>
#include <optional>
#include <utility>

#include <QApplication>
#include <QPlainTextEdit>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>

#include "model/GuiDetailedSequenceResult.h"
#include "model/GuiResultSelectionModel.h"
#include "xfoil/gui/XFoilWidget.h"

namespace {

QWidget* plotWidget(QWidget& widget, const QString& name) {
  QWidget* plot = widget.findChild<QWidget*>(name);
  Q_ASSERT(plot != nullptr);
  return plot;
}

xfoil::gui::GuiPointEntry point(std::size_t sourceIndex, double alpha,
                                bool converged, bool enabled) {
  xfoil::gui::GuiPointEntry entry;
  entry.sourceIndex = sourceIndex;
  entry.alpha = alpha;
  entry.converged = converged;
  entry.enabled = enabled;
  return entry;
}

xfoil::DetailedSequencePoint detailPoint(double alpha, bool viscous) {
  xfoil::DetailedSequencePoint point;
  point.polar.alpha = xfoil::Degrees{alpha};
  point.polar.liftCoefficient = 0.4;
  point.polar.dragCoefficient = 0.02;
  point.polar.convergence = xfoil::ConvergenceStatus::converged;
  xfoil::OperatingPointData operating;
  operating.alpha = xfoil::Degrees{alpha};
  operating.liftCoefficient = 0.4;
  operating.dragCoefficient = 0.02;
  operating.viscous = viscous;
  operating.convergence = xfoil::ConvergenceStatus::converged;
  xfoil::SurfaceDistribution surface;
  surface.kind = viscous ? xfoil::SurfaceDistributionKind::viscous
                         : xfoil::SurfaceDistributionKind::inviscid;
  surface.x = {0.0, 0.5, 1.0};
  surface.y = {0.0, 0.02, 0.0};
  surface.cp = {-0.5, -0.2, 0.1};
  operating.surface = std::move(surface);
  if (viscous) {
    xfoil::SkinFrictionDistribution cf;
    cf.upper.side = xfoil::SurfaceSide::upper;
    cf.upper.x = {0.0, 1.0};
    cf.upper.y = {0.0, 0.0};
    cf.upper.s = {0.0, 1.0};
    cf.upper.cf = {0.004, 0.003};
    cf.lower.side = xfoil::SurfaceSide::lower;
    cf.lower.x = {0.0, 1.0};
    cf.lower.y = {0.0, 0.0};
    cf.lower.s = {0.0, 1.0};
    cf.lower.cf = {0.003, 0.002};
    operating.dragCoefficientFriction = 0.0035;
    operating.skinFriction = std::move(cf);
  }
  point.operatingPoint = std::move(operating);
  return point;
}

}  // namespace

class GuiDetailedSequenceContractTests : public QObject {
  Q_OBJECT

 private slots:
  void init() {
    QVERIFY(QApplication::instance() != nullptr);
  }

  void scalarTabsArePointSelectableAndDistributionTabsAreNot() {
    xfoil::gui::XFoilWidget widget;
    auto* tabs = widget.findChild<QTabWidget*>("resultPlotTabs");
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 7);

    const QStringList scalarPlots = {
        QStringLiteral("clAlphaPlot"), QStringLiteral("clCdPlot"),
        QStringLiteral("cmAlphaPlot"), QStringLiteral("cmClPlot"),
        QStringLiteral("efficiencyClPlot")};
    for (const QString& name : scalarPlots) {
      QWidget* plot = plotWidget(widget, name);
      QCOMPARE(plot->property("xfoilPlotRole").toString(),
               QStringLiteral("scalar-sequence"));
      QCOMPARE(plot->property("xfoilPointSelectable").toBool(), true);
      QVERIFY(plot->toolTip().contains(QStringLiteral("Click a point")));
      QVERIFY(plot->toolTip().contains(QStringLiteral("alpha dropdown")));
    }

    QWidget* cp = plotWidget(widget, QStringLiteral("cpAlphaPlot"));
    QCOMPARE(cp->property("xfoilPlotRole").toString(),
             QStringLiteral("distribution-cp"));
    QVERIFY(!cp->property("xfoilPointSelectable").toBool());
    QVERIFY(cp->toolTip().contains(QStringLiteral("selected operating point")));

    QWidget* cf = plotWidget(widget, QStringLiteral("cfAlphaPlot"));
    QCOMPARE(cf->property("xfoilPlotRole").toString(),
             QStringLiteral("distribution-cf"));
    QVERIFY(!cf->property("xfoilPointSelectable").toBool());
    QVERIFY(cf->toolTip().contains(QStringLiteral("inviscid")));

    auto* report = widget.findChild<QPlainTextEdit*>("reportView");
    QVERIFY(report != nullptr);
    QVERIFY(report->isReadOnly());
  }

  void leftCpAndAirfoilHaveExplicitRoles() {
    xfoil::gui::XFoilWidget widget;
    QWidget* cp = plotWidget(widget, QStringLiteral("cpPlot"));
    QCOMPARE(cp->property("xfoilPlotRole").toString(),
             QStringLiteral("distribution-cp"));
    QWidget* airfoil = plotWidget(widget, QStringLiteral("airfoilPlot"));
    QCOMPARE(airfoil->property("xfoilPlotRole").toString(),
             QStringLiteral("geometry"));
    QVERIFY(airfoil->toolTip().contains(QStringLiteral("Ctrl+right-click")));
  }

  void detailedResultIsOwningAndMapsStableSourceIndex() {
    xfoil::gui::GuiDetailedSequenceResult result;
    result.sequence.points.push_back(detailPoint(1.0, false));
    result.sequence.points.push_back(detailPoint(2.0, true));
    result.sequence.points.push_back(detailPoint(3.0, false));
    result.sequence.stopReason = xfoil::SequenceStopReason::completed;

    const xfoil::gui::GuiDetailedSequenceResult copy = result;
    QCOMPARE(copy.sequence.points.size(), std::size_t{3});
    QVERIFY(copy.sequence.points[1].operatingPoint.has_value());
    QVERIFY(copy.sequence.points[1].operatingPoint->skinFriction.has_value());
    result.sequence.points[1].operatingPoint->skinFriction->upper.cf[0] = 99.0;
    result.sequence.points[1].operatingPoint->surface->cp[0] = 99.0;
    QCOMPARE(copy.sequence.points[1].operatingPoint->skinFriction->upper.cf[0],
             0.004);
    QCOMPARE(copy.sequence.points[1].operatingPoint->surface->cp[0], -0.5);

    xfoil::gui::GuiResultSelectionModel selection({point(1, 1.0, true, true),
                                                   point(2, 1.0, false, true),
                                                   point(3, 2.0, true, false)});
    QVERIFY(selection.select(std::size_t{2}));
    QCOMPARE(selection.currentSourceIndex().value(), std::size_t{2});
    const std::optional<std::size_t> source = selection.currentSourceIndex();
    QVERIFY(source.has_value());
    QVERIFY(*source < copy.sequence.points.size());
    QCOMPARE(copy.sequence.points[*source].polar.alpha.value, 3.0);

    QVERIFY(selection.deactivate(std::size_t{2}));
    QCOMPARE(selection.currentSourceIndex().value(), std::size_t{1});
    QCOMPARE(selection.enabledPoints().size(), std::size_t{1});
    QVERIFY(selection.activate(std::size_t{2}));
    QCOMPARE(selection.enabledPoints().size(), std::size_t{2});
    QCOMPARE(selection.points()[1].alpha, 1.0);
    QCOMPARE(selection.points()[1].sourceIndex, std::size_t{2});
  }

  void distributionPlotsRemainEmptyShells() {
    xfoil::gui::XFoilWidget widget;
    const QStringList plots = {
        QStringLiteral("cpPlot"), QStringLiteral("cpAlphaPlot"),
        QStringLiteral("cfAlphaPlot")};
    for (const QString& name : plots) {
      QWidget* plot = plotWidget(widget, name);
      QVERIFY(plot->property("xfoilPlotRole").toString().startsWith(
          QStringLiteral("distribution-")));
      QVERIFY(!plot->property("xfoilPointSelectable").toBool());
    }
  }
};

int runGuiDetailedSequenceContractTests(int argc, char** argv) {
  GuiDetailedSequenceContractTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiDetailedSequenceContractTests.moc"

// End: tests/gui/GuiDetailedSequenceContractTests.cpp

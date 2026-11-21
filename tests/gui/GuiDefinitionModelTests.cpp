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

// File: tests/gui/GuiDefinitionModelTests.cpp
// Owning definition-model, target validation and RunSpec tests.

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <QtTest/QTest>

#include "model/GuiDefinitionModel.h"
#include "model/GuiRunSpec.h"

namespace {

using xfoil::AlphaSequence;
using xfoil::AnalysisConfig;
using xfoil::AnalysisTarget;
using xfoil::Degrees;
using xfoil::LiftTarget;
using xfoil::PanelingOptions;
using xfoil::SurfaceGeometry;
using xfoil::gui::AnalysisMode;
using xfoil::gui::GeometrySource;
using xfoil::gui::GuiDefinitionModel;
using xfoil::gui::GuiNacaRequest;
using xfoil::gui::GuiRunSpec;
using xfoil::gui::XFoilGuiState;
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

SurfaceGeometry panelFor(const AirfoilGeometry& geometry) {
  SurfaceGeometry panel;
  for (const xfoil::Point2D& point : geometry.coordinates()) {
    panel.points.push_back(point);
  }
  return panel;
}

}  // namespace

class GuiDefinitionModelTests : public QObject {
  Q_OBJECT

 private slots:
  void defaultsComeFromPublicTypes() {
    GuiDefinitionModel model;
    const PanelingOptions paneling;
    const AnalysisConfig configuration;

    QCOMPARE(model.state(), XFoilGuiState::empty);
    QVERIFY(!model.isReady());
    QVERIFY(!model.geometry().has_value());
    QVERIFY(!model.panel().has_value());
    QCOMPARE(model.geometrySource(), GeometrySource::none);
    QCOMPARE(model.mode(), AnalysisMode::alphaPoint);
    QCOMPARE(model.panelingOptions().panelCount,
             static_cast<std::size_t>(paneling.panelCount));
    QCOMPARE(model.panelingOptions().bunchingParameter,
             paneling.bunchingParameter);
    QCOMPARE(model.configuration().reynoldsNumber,
             configuration.reynoldsNumber);
    QCOMPARE(model.configuration().machNumber, configuration.machNumber);
    QCOMPARE(model.configuration().viscous, configuration.viscous);
    QCOMPARE(model.configuration().maximumIterations,
             configuration.maximumIterations);
    QCOMPARE(model.configuration().transition.nCritical,
             configuration.transition.nCritical);
    QCOMPARE(std::get<xfoil::AlphaTarget>(model.target()).alpha.value, 0.0);
    QCOMPARE(model.sequence().start.value, 0.0);
    QCOMPARE(model.sequence().endExclusive.value, 5.0);
    QCOMPARE(model.sequence().step.value, 1.0);
    QVERIFY(model.makeRunSpec().isErr());
  }

  void memoryGeometryAndPanelTransitions() {
    GuiDefinitionModel model;
    model.acceptGeometry(diamond());

    QCOMPARE(model.state(), XFoilGuiState::geometry);
    QCOMPARE(model.geometrySource(), GeometrySource::memory);
    QVERIFY(model.geometry().has_value());
    QVERIFY(!model.panel().has_value());
    QVERIFY(!model.isReady());

    model.acceptConfiguration(AnalysisConfig{});
    model.acceptPanel(panelFor(*model.geometry()), PanelingOptions{});

    QCOMPARE(model.state(), XFoilGuiState::configured);
    QVERIFY(model.isReady());
    const auto spec = model.makeRunSpec();
    QVERIFY(spec.isOk());
    QVERIFY(std::holds_alternative<AirfoilGeometry>(spec.value().geometry()));
    QVERIFY(std::holds_alternative<xfoil::AlphaTarget>(
        spec.value().target()));
  }

  void nacaAndNacsRequestIsOwning() {
    GuiDefinitionModel model;
    model.acceptNaca(diamond(), "2412", false);
    model.acceptConfiguration(AnalysisConfig{});
    model.acceptPanel(panelFor(diamond()), PanelingOptions{});

    const auto naca = model.makeRunSpec();
    QVERIFY(naca.isOk());
    QCOMPARE(std::get<GuiNacaRequest>(naca.value().geometry()).designation,
             std::string{"2412"});
    QCOMPARE(std::get<GuiNacaRequest>(naca.value().geometry())
                 .sharpTrailingEdge,
             false);

    GuiDefinitionModel sharpModel;
    sharpModel.acceptNaca(diamond(), "0012", true);
    sharpModel.acceptConfiguration(AnalysisConfig{});
    sharpModel.acceptPanel(panelFor(diamond()), PanelingOptions{});
    const auto nacs = sharpModel.makeRunSpec();
    QVERIFY(nacs.isOk());
    QCOMPARE(std::get<GuiNacaRequest>(nacs.value().geometry())
                 .sharpTrailingEdge,
             true);
  }

  void panelingChangeInvalidatesPanelAndPreview() {
    GuiDefinitionModel model;
    model.acceptGeometry(diamond());
    model.acceptConfiguration(AnalysisConfig{});
    model.acceptPanel(panelFor(diamond()), PanelingOptions{});
    QVERIFY(model.isReady());

    PanelingOptions changed;
    changed.panelCount = 180;
    model.setPanelingOptions(changed);

    QVERIFY(!model.panel().has_value());
    QCOMPARE(model.state(), XFoilGuiState::geometry);
    QVERIFY(!model.isReady());
    QCOMPARE(model.panelingOptions().panelCount,
             std::size_t{180});
  }

  void targetAndSequenceValidation() {
    GuiDefinitionModel model;
    model.acceptGeometry(diamond());
    model.acceptConfiguration(AnalysisConfig{});
    model.acceptPanel(panelFor(diamond()), PanelingOptions{});

    AlphaSequence zeroStep;
    zeroStep.start = Degrees{0.0};
    zeroStep.endExclusive = Degrees{2.0};
    zeroStep.step = Degrees{0.0};
    QVERIFY(model.setSequence(zeroStep).isErr());
    QCOMPARE(model.state(), XFoilGuiState::error);
    QVERIFY(!model.isReady());
    QVERIFY(model.error().has_value());

    AlphaSequence wrongDirection;
    wrongDirection.start = Degrees{2.0};
    wrongDirection.endExclusive = Degrees{0.0};
    wrongDirection.step = Degrees{1.0};
    QVERIFY(model.setSequence(wrongDirection).isErr());

    AlphaSequence tooMany;
    tooMany.start = Degrees{0.0};
    tooMany.endExclusive = Degrees{100001.0};
    tooMany.step = Degrees{1.0};
    QVERIFY(model.setSequence(tooMany).isErr());

    AlphaSequence valid;
    valid.start = Degrees{-2.0};
    valid.endExclusive = Degrees{2.0};
    valid.step = Degrees{1.0};
    QVERIFY(model.setSequence(valid).isOk());
    QCOMPARE(model.state(), XFoilGuiState::configured);
    QVERIFY(model.isReady());
    const auto spec = model.makeRunSpec();
    QVERIFY(spec.isOk());
    const auto& sequence = std::get<AlphaSequence>(spec.value().target());
    QCOMPARE(sequence.endExclusive.value, 2.0);
    QCOMPARE(sequence.step.value, 1.0);

    QVERIFY(model.setTarget(LiftTarget{0.5}).isOk());
    QCOMPARE(model.mode(), AnalysisMode::clPoint);
    QCOMPARE(std::get<LiftTarget>(model.target()).cl, 0.5);
  }

  void runSpecIsCopyableMovableAndStable() {
    static_assert(std::is_copy_constructible_v<GuiRunSpec>);
    static_assert(std::is_move_constructible_v<GuiRunSpec>);
    static_assert(std::is_copy_assignable_v<GuiRunSpec>);
    static_assert(std::is_move_assignable_v<GuiRunSpec>);

    GuiDefinitionModel model;
    model.acceptNaca(diamond(), "2412", false);
    model.acceptConfiguration(AnalysisConfig{});
    model.acceptPanel(panelFor(diamond()), PanelingOptions{});
    const auto result = model.makeRunSpec();
    QVERIFY(result.isOk());
    GuiRunSpec copy = result.value();
    GuiRunSpec moved = std::move(copy);

    QVERIFY(model.setTarget(LiftTarget{0.9}).isOk());
    QCOMPARE(std::get<GuiNacaRequest>(moved.geometry()).designation,
             std::string{"2412"});
    QVERIFY(std::holds_alternative<xfoil::AlphaTarget>(moved.target()));
  }
};

int runGuiDefinitionModelTests(int argc, char** argv) {
  GuiDefinitionModelTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "GuiDefinitionModelTests.moc"

// End: tests/gui/GuiDefinitionModelTests.cpp

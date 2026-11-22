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

// File: tests/gui/GuiLinkCheck.cpp
// Minimal external consumer: public GUI header and archives only, one
// host widget, one embeddable widget, no event loop.

#include <variant>

#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>

#include "xfoil/gui/XFoilWidget.h"

int main(int argc, char** argv) {
  QApplication application(argc, argv);

  QWidget host;
  auto* layout = new QVBoxLayout(&host);
  auto* widget = new xfoil::gui::XFoilWidget(&host);
  layout->addWidget(widget);

  if (widget->parentWidget() != &host || layout->count() != 1) {
    return 1;
  }
  if (!widget->setNaca("2412").isOk()) {
    return 2;
  }
  if (!widget->generatePanel(xfoil::PanelingOptions{}).isOk()) {
    return 3;
  }
  if (!widget->setConfiguration(xfoil::AnalysisConfig{}).isOk()) {
    return 4;
  }
  widget->setTarget(xfoil::LiftTarget{0.5});
  if (!std::holds_alternative<xfoil::LiftTarget>(widget->target())) {
    return 5;
  }
  if (!widget->isReadyToAnalyze()) {
    return 6;
  }

  xfoil::AlphaSequence sequence;
  sequence.start = xfoil::Degrees{-2.0};
  sequence.endExclusive = xfoil::Degrees{2.0};
  sequence.step = xfoil::Degrees{1.0};
  widget->setSequence(sequence);
  if (widget->sequence().endExclusive.value != 2.0 ||
      !widget->isReadyToAnalyze()) {
    return 7;
  }

  if (!widget->geometry().has_value() ||
      !widget->panelGeometry().has_value()) {
    return 8;
  }

  xfoil::gui::AnalysisWorkflowSetup setup;
  setup.geometry = xfoil::gui::NacaGeometrySetup{"0012"};
  setup.paneling = xfoil::PanelingOptions{};
  setup.configuration = xfoil::AnalysisConfig{};
  setup.request = xfoil::LiftSequence{0.2, 0.8, 0.2};
  if (!widget->applySetup(setup).isOk()) {
    return 9;
  }
  if (!widget->currentSetup().has_value() ||
      !std::holds_alternative<xfoil::LiftSequence>(
          widget->currentSetup()->request)) {
    return 10;
  }
  xfoil::gui::AnalysisWorkflowViewOptions viewOptions;
  viewOptions.showPaneling = true;
  widget->setViewOptions(viewOptions);
  if (!widget->viewOptions().showPaneling) {
    return 11;
  }

  QCoreApplication::processEvents();
  return 0;
}

// End: tests/gui/GuiLinkCheck.cpp

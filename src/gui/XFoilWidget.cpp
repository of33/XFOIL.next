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

// File: src/gui/XFoilWidget.cpp
// Layout-only implementation of the public embeddable widget. The private
// implementation owns generated UI children only; it creates no application,
// session, worker, file adapter or global GUI state.

#include "xfoil/gui/XFoilWidget.h"

#include <memory>

#include "ui_XFoilWidget.h"

namespace xfoil::gui {

class XFoilWidget::Impl {
 public:
  explicit Impl(XFoilWidget* owner) {
    ui.setupUi(owner);
    ui.workspaceSplitter->setStretchFactor(0, 3);
    ui.workspaceSplitter->setStretchFactor(1, 1);
  }

  Ui::XFoilWidget ui;
};

XFoilWidget::XFoilWidget(QWidget* parent)
    : QWidget(parent), impl_(std::make_unique<Impl>(this)) {}

XFoilWidget::~XFoilWidget() = default;

}  // namespace xfoil::gui

// End: src/gui/XFoilWidget.cpp

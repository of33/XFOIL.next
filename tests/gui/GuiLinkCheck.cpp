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

  QCoreApplication::processEvents();
  return 0;
}

// End: tests/gui/GuiLinkCheck.cpp

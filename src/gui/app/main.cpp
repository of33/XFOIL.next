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

// File: src/gui/app/main.cpp
// Standalone application entry point. All GUI responsibility stays in
// MainWindow; the embeddable widget remains a host-neutral component.

#include <QApplication>
#include <QCoreApplication>
#include <QString>

#include "app/MainWindow.h"
#include "app/ProductInfo.h"

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  const QString productName = QString::fromLatin1(
      xfoil::app::product::name.data(),
      static_cast<int>(xfoil::app::product::name.size()));
  QCoreApplication::setOrganizationName(productName);
  QCoreApplication::setApplicationName(productName +
                                       QStringLiteral(" GUI"));
  QCoreApplication::setApplicationVersion(QString::fromLatin1(
      xfoil::app::product::version.data(),
      static_cast<int>(xfoil::app::product::version.size())));

  xfoil::gui::app::MainWindow window;
  window.show();
  return application.exec();
}

// End: src/gui/app/main.cpp

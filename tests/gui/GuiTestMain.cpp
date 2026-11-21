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

// File: tests/gui/GuiTestMain.cpp
// One QApplication for the GUI test binary. The test classes are run
// sequentially and own no global application object.

#include <QApplication>

int runQCustomPlotSmoke(int argc, char** argv);
int runGeometryPlotViewTests(int argc, char** argv);
int runGuiDefinitionModelTests(int argc, char** argv);
int runGuiSessionBackendTests(int argc, char** argv);
int runXFoilWidgetG2Tests(int argc, char** argv);
int runXFoilWidgetShellTests(int argc, char** argv);

int main(int argc, char** argv) {
  QApplication application(argc, argv);

  int failures = 0;
  failures += runQCustomPlotSmoke(argc, argv);
  failures += runGeometryPlotViewTests(argc, argv);
  failures += runGuiDefinitionModelTests(argc, argv);
  failures += runGuiSessionBackendTests(argc, argv);
  failures += runXFoilWidgetG2Tests(argc, argv);
  failures += runXFoilWidgetShellTests(argc, argv);
  return failures == 0 ? 0 : 1;
}

// End: tests/gui/GuiTestMain.cpp

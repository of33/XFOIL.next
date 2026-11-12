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

// file: tests/unit/ConsoleTextS18Tests.cpp
// Exact formatter checks for the accepted S18 console text contract.

#include "TestSupport.h"

#include "app/cli/CliFortranText.h"

#include <sstream>
#include <string>

TEST(unit, ConsoleTextS18ViscalLoadsAppendsClfAndCdp) {
  std::ostringstream out;
  xfoil::app::cli::emitViscalIterationLoads(out, 2.0, 0.6465, -0.0889, 0.00585,
                                     0.00414, 0.00004, 0.00075);
  CHECK_EQ(out.str(),
           "       a =  2.000      CL =  0.6465\n"
           "      Cm = -0.0889     CD =  0.00585   =>   CDf =  0.00414"
           "    CDp =  0.00171    CLf =  0.00004   CDp =  0.00075"
           " CDp+CDf =  0.00489\n");
}

TEST(unit, ConsoleTextS18TcbufUsesWidenedFields) {
  std::ostringstream out;
  xfoil::app::cli::emitTcbufPlain(out, 0.120035, 0.2998, 0.02, 0.4005);
  CHECK_EQ(out.str(),
           " Max thickness =   0.120035  at x =   0.2998\n"
           " Max camber    =   0.020000  at x =   0.4005\n");
}

TEST(unit, ConsoleTextS18TsetKeepsLegacyFields) {
  std::ostringstream out;
  xfoil::app::cli::emitThicknessCamberReport(out, 0.120035, 0.2998, 0.02, 0.4005);
  CHECK_EQ(out.str(),
           "\n Max thickness =   0.1200  at x =   0.300\n"
           " Max camber    =   0.0200  at x =   0.401\n\n");
}

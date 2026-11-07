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

// File: src/app/cli/CommandParser.h
// ASKC-compatible line parser of the interactive command interpreter.
// Orientation: fortran-src/userio.f, routine ASKC. No
// numeric interpretation, no output.

#pragma once

#include <string>
#include <string_view>

namespace xfoil::app::cli {

struct ParsedCommand {
  std::string command;
  std::string argument;
  bool empty{};
};

// Splits an input line according to ASKC: leading spaces are
// removed, the command ends at the latest after four characters and at
// a space, '+', '-', '.', ',' or first digit; it is converted to
// uppercase letters. The rest of the line is returned as argument,
// trimmed on the outside. Pure blank lines deliver empty == true.
[[nodiscard]]
ParsedCommand parseCommandLine(
    std::string_view line);

}  // namespace xfoil::app::cli

// End: src/app/cli/CommandParser.h

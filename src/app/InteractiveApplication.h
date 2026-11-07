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

// File: src/app/InteractiveApplication.h
// Thin application layer over the interpreter core. It
// receives the injected streams and starts the interactive
// XFOIL-6.99 command interpreter. The actual state management lives
// in xfoil::app::cli::CommandInterpreter.

#pragma once

#include <iosfwd>

#include "app/cli/CommandInterpreter.h"

namespace xfoil::app {

// Starts the interactive interpreter with injected streams for
// input, normal output and error output. The return value is the exit
// code of the interpreter (0 for Q, QUIT or EOF, 1 for a faulty
// start profile file).
[[nodiscard]]
int runInteractiveApplication(
    std::istream& input,
    std::ostream& output,
    std::ostream& errorOutput,
    const cli::InteractiveStartupOptions& options);

}  // namespace xfoil::app

// End: src/app/InteractiveApplication.h

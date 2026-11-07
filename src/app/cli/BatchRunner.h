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

// File: src/app/cli/BatchRunner.h
// Native batch runner. The runner
// orchestrates exclusively: file invocation, configuration, working
// directory, tee stream, OutputPathPolicy setting, output-existence check,
// results.dat writing and exit status. No numerics, no
// session-state copy, no own command handler, no own
// LOAD/PACC/defaults implementation, no direct core usage; command
// execution happens exclusively through the existing
// CommandInterpreter.

#pragma once

#include <iosfwd>
#include <optional>
#include <string>

namespace xfoil::app::cli {

// Options for calling the batch runner; the command line
// has already checked prefix validity.
struct BatchRunOptions {
  std::string batchPath;  // job file path or "-" for stdin
  std::optional<std::string> profilePath;
  std::string prefix;  // empty = identity
  bool quiet{false};
  std::optional<std::string> defaultsPath;
  std::optional<std::string> workingDirectory;
};

// Exit codes.
inline constexpr int batchExitSuccess = 0;
inline constexpr int batchExitStartupProfileError = 1;
inline constexpr int batchExitArgumentOrFormatError = 2;
inline constexpr int batchExitRunnerFileError = 3;
inline constexpr int batchExitCommandOrSemanticsError = 4;
inline constexpr int batchExitSolverError = 5;
inline constexpr int batchExitNonConvergence = 6;

// Runs exactly one batch job. standardInput serves as the job source only
// when batch path == "-"; the command input of the interpreter is
// the built-up command stream. The return value is the exit code
// (first-error-wins).
[[nodiscard]]
int runBatch(const BatchRunOptions& options,
             std::istream& standardInput,
             std::ostream& standardOutput,
             std::ostream& errorOutput);

}  // namespace xfoil::app::cli

// End: src/app/cli/BatchRunner.h

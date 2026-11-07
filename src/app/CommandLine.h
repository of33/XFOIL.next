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

// File: src/app/CommandLine.h
// Program argument parser of the product program (new design).
// Splits the arguments after argv[0] into mode and options.
// No output, no exit; diagnostics are returned as text.
// The option name --batch selects the native batch job (file or '-' for
// stdin).
// Options are order independent;
// combination rules and prefix validity are checked here, with the
// diagnostic `Invalid batch prefix:...` before each file operation.

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xfoil::app {

enum class ProgramMode {
  interactive,
  batch,
  help,    // --help: usage text on stdout, exit 0
  version  // --version: version line on stdout, exit 0
};

enum class CommandLineError {
  none,
  unknownOption,
  multipleInputFiles,
  duplicateBatchOption,
  duplicateProfileOption,
  duplicatePrefixOption,
  missingOptionArgument,
  profileRequiresBatch,
  prefixRequiresBatch,
  quietRequiresBatch,
  defaultsRequiresBatch,
  workingDirectoryRequiresBatch,
  profileArgumentWithBatch,
  invalidBatchPrefix
};

struct CommandLineOptions {
  ProgramMode mode{ProgramMode::interactive};
  // Interactive: positional profile (unchanged inventory). In batch mode it
  // is always empty (a bare positional profile with --batch is an error).
  std::optional<std::string> inputPath;

  // Batch options (only for mode == batch set/checked):
  std::string batchPath;  // File path or "-" for stdin
  std::optional<std::string> profilePath;
  std::string prefix;  // validated (character set/length); empty = identity
  bool quiet{false};
  std::optional<std::string> defaultsPath;
  std::optional<std::string> workingDirectory;
};

struct CommandLineParseResult {
  CommandLineOptions options;
  CommandLineError error{CommandLineError::none};
  std::string diagnostic;

  [[nodiscard]]
  bool success() const noexcept {
    return error == CommandLineError::none;
  }
};

[[nodiscard]]
CommandLineParseResult parseCommandLine(
    std::span<const std::string_view> arguments);

}  // namespace xfoil::app

// End: src/app/CommandLine.h

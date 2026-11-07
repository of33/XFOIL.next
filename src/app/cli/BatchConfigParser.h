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

// File: src/app/cli/BatchConfigParser.h
// Parser of the native batch job format version 1.
// Pure function operating on the file contents: no filesystem access,
// no shell interpolation, no includes, no variable replacement, no
// loops and no conditions. The parser
// strips the UTF-8 BOM and comments, removes a leading CR per line and
// returns the command range character-exactly (without trimming) to the
// native command interpreter. The runner reports format errors with exit 2
// on stderr; the diagnostic texts are pre-formatted here.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xfoil::app::cli {

// Protection limits.
inline constexpr std::size_t batchMaxFileBytes = 1048576;
inline constexpr std::size_t batchMaxLineChars = 1024;
inline constexpr std::size_t batchMaxCommandLines = 10000;

// Authoritative key set in [job] (version 1, exact): defaults,
// stop-on-error, stop-on-nonconvergence, allow-unavailable-plot-commands.
// There is no prefix, airfoils or quiet key.
struct BatchJobSettings {
  std::optional<std::string> defaultsPath;
  bool stopOnError{true};
  bool stopOnNonConvergence{false};
  bool allowUnavailablePlotCommands{true};
};

// Completely parsed job: settings plus command stream in
// execution order (blank lines preserved, comments removed).
struct BatchJob {
  BatchJobSettings settings;
  std::vector<std::string> commandLines;
};

enum class BatchFormatError {
  none,
  missingMarker,
  unsupportedVersion,
  missingCommandsSection,
  unknownSection,
  unknownKey,
  duplicateKey,
  invalidValue,
  invalidByteOrderMark,
  fileTooLarge,
  lineTooLong,
  tooManyCommands,
  strayBeforeCommands
};

struct BatchParseResult {
  BatchJob job;
  BatchFormatError error{BatchFormatError::none};
  std::string diagnostic;

  [[nodiscard]]
  bool success() const noexcept {
    return error == BatchFormatError::none;
  }
};

// Parses the complete file contents of a batch job. The
// parser performs the file-size boundary checks on the content; reading
// the file itself (and its existence check, exit 3) is done by the BatchRunner.
[[nodiscard]]
BatchParseResult parseBatchConfig(std::string_view content);

}  // namespace xfoil::app::cli

// End: src/app/cli/BatchConfigParser.h

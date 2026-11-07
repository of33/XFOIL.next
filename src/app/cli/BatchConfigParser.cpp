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

// File: src/app/cli/BatchConfigParser.cpp
// Implementation of the batch job parser.
// The first detected format error wins; the diagnostic is exact and
// is output unchanged by the runner on stderr (exit 2).

#include "app/cli/BatchConfigParser.h"

#include <string>

namespace xfoil::app::cli {
namespace {

constexpr std::string_view versionMarker = "XFOIL-BATCH 1";
constexpr std::string_view versionPrefix = "XFOIL-BATCH";
constexpr std::string_view jobSection = "job";
constexpr std::string_view commandsSection = "commands";
constexpr std::string_view byteOrderMark = "\xEF\xBB\xBF";

std::string trimView(std::string_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end &&
         (text[begin] == ' ' || text[begin] == '\t')) {
    ++begin;
  }
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t')) {
    --end;
  }
  return std::string{text.substr(begin, end - begin)};
}

bool isBlank(std::string_view text) {
  return trimView(text).empty();
}

bool isComment(std::string_view text) {
  for (const char symbol : text) {
    if (symbol == ' ' || symbol == '\t') {
      continue;
    }
    return symbol == '#';
  }
  return false;
}

// Returns the section name if the line is a section declaration
// (trimmed line begins with '[' and ends with ']'), otherwise nullopt.
std::optional<std::string> sectionName(std::string_view line) {
  const std::string trimmed = trimView(line);
  if (trimmed.size() < 2 || trimmed.front() != '[' ||
      trimmed.back() != ']') {
    return std::nullopt;
  }
  return trimmed.substr(1, trimmed.size() - 2);
}

enum class ParserState {
  beforeMarker,
  sectionZone,
  commandStream
};

bool parseBoolValue(std::string_view value, bool& parsed) {
  if (value == "true") {
    parsed = true;
    return true;
  }
  if (value == "false") {
    parsed = false;
    return true;
  }
  return false;
}

}  // namespace

BatchParseResult parseBatchConfig(std::string_view content) {
  BatchParseResult result;

  if (content.size() > batchMaxFileBytes) {
    result.error = BatchFormatError::fileTooLarge;
    result.diagnostic = "batch file exceeds 1 MiB limit";
    return result;
  }

  // BOM tolerated only at the start of the file; otherwise error.
  if (content.substr(0, byteOrderMark.size()) == byteOrderMark) {
    content.remove_prefix(byteOrderMark.size());
  }
  if (content.find(byteOrderMark) != std::string_view::npos) {
    result.error = BatchFormatError::invalidByteOrderMark;
    result.diagnostic = "invalid byte order mark";
    return result;
  }

  ParserState state{ParserState::beforeMarker};
  bool inJobSection = false;
  bool jobDefaultsSeen = false;
  bool jobStopOnErrorSeen = false;
  bool jobStopOnNonConvergenceSeen = false;
  bool jobAllowUnavailableSeen = false;

  std::size_t lineBegin = 0;
  while (lineBegin < content.size()) {
    const std::size_t lineEnd = content.find('\n', lineBegin);
    const bool lastLine = lineEnd == std::string_view::npos;
    std::string_view line = content.substr(
        lineBegin,
        lastLine ? content.size() - lineBegin : lineEnd - lineBegin);
    // LF and CRLF accepted; a leading CR per line is removed.
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.size() > batchMaxLineChars) {
      result.error = BatchFormatError::lineTooLong;
      result.diagnostic = "batch line exceeds 1024 characters";
      return result;
    }

    if (state == ParserState::beforeMarker) {
      if (isBlank(line) || isComment(line)) {
        if (lastLine) {
          break;
        }
        lineBegin = lineEnd + 1;
        continue;
      }
      if (line == versionMarker) {
        state = ParserState::sectionZone;
      } else if (line.substr(0, versionPrefix.size()) == versionPrefix) {
        result.error = BatchFormatError::unsupportedVersion;
        result.diagnostic = "unsupported batch format version";
        return result;
      } else {
        result.error = BatchFormatError::missingMarker;
        result.diagnostic =
            "missing batch marker: expected XFOIL-BATCH 1";
        return result;
      }
      if (lastLine) {
        break;
      }
      lineBegin = lineEnd + 1;
      continue;
    }

    if (state == ParserState::sectionZone) {
      if (isBlank(line) || isComment(line)) {
        // Blank lines do not end the section zone; they are
 // irrelevant there (point 12).
        if (lastLine) {
          break;
        }
        lineBegin = lineEnd + 1;
        continue;
      }
      const std::optional<std::string> section = sectionName(line);
      if (section.has_value()) {
        if (*section == std::string{commandsSection}) {
          state = ParserState::commandStream;
          inJobSection = false;
        } else if (*section == std::string{jobSection}) {
          inJobSection = true;
        } else {
          result.error = BatchFormatError::unknownSection;
          result.diagnostic = "unknown batch section: " + *section;
          return result;
        }
        if (lastLine) {
          break;
        }
        lineBegin = lineEnd + 1;
        continue;
      }
      if (!inJobSection) {
        result.error = BatchFormatError::strayBeforeCommands;
        result.diagnostic =
            "unexpected batch line before a section: " + trimView(line);
        return result;
      }
      const std::size_t equals = line.find('=');
      const std::string key =
          trimView(equals == std::string_view::npos ? line
                                                    : line.substr(0, equals));
      const std::string value =
          equals == std::string_view::npos
              ? std::string{}
              : trimView(line.substr(equals + 1));
      if (key == "defaults") {
        if (jobDefaultsSeen) {
          result.error = BatchFormatError::duplicateKey;
          result.diagnostic = "duplicate batch key: defaults";
          return result;
        }
        jobDefaultsSeen = true;
        // No path quoting: the rest of the line after '=' is
 // trimmed; quotation marks are part of the path.
        result.job.settings.defaultsPath = value;
      } else if (key == "stop-on-error") {
        if (jobStopOnErrorSeen) {
          result.error = BatchFormatError::duplicateKey;
          result.diagnostic = "duplicate batch key: stop-on-error";
          return result;
        }
        jobStopOnErrorSeen = true;
        bool parsed = false;
        if (!parseBoolValue(value, parsed)) {
          result.error = BatchFormatError::invalidValue;
          result.diagnostic = "invalid batch value: stop-on-error";
          return result;
        }
        result.job.settings.stopOnError = parsed;
      } else if (key == "stop-on-nonconvergence") {
        if (jobStopOnNonConvergenceSeen) {
          result.error = BatchFormatError::duplicateKey;
          result.diagnostic =
              "duplicate batch key: stop-on-nonconvergence";
          return result;
        }
        jobStopOnNonConvergenceSeen = true;
        bool parsed = false;
        if (!parseBoolValue(value, parsed)) {
          result.error = BatchFormatError::invalidValue;
          result.diagnostic = "invalid batch value: stop-on-nonconvergence";
          return result;
        }
        result.job.settings.stopOnNonConvergence = parsed;
      } else if (key == "allow-unavailable-plot-commands") {
        if (jobAllowUnavailableSeen) {
          result.error = BatchFormatError::duplicateKey;
          result.diagnostic =
              "duplicate batch key: allow-unavailable-plot-commands";
          return result;
        }
        jobAllowUnavailableSeen = true;
        bool parsed = false;
        if (!parseBoolValue(value, parsed)) {
          result.error = BatchFormatError::invalidValue;
          result.diagnostic =
              "invalid batch value: allow-unavailable-plot-commands";
          return result;
        }
        result.job.settings.allowUnavailablePlotCommands = parsed;
      } else {
        // Version 1 is strict; prefix/airfoils/quiet are unknown
 // keys.
        result.error = BatchFormatError::unknownKey;
        result.diagnostic = "unknown batch key: " + key;
        return result;
      }
      if (lastLine) {
        break;
      }
      lineBegin = lineEnd + 1;
      continue;
    }

    // Command range: everything up to end of file; comments removed,
    // blank lines and command text kept character-exact.
    if (isComment(line)) {
      if (lastLine) {
        break;
      }
      lineBegin = lineEnd + 1;
      continue;
    }
    if (result.job.commandLines.size() >= batchMaxCommandLines) {
      result.error = BatchFormatError::tooManyCommands;
      result.diagnostic =
          "batch command limit exceeded: 10000";
      return result;
    }
    result.job.commandLines.emplace_back(line);
    if (lastLine) {
      break;
    }
    lineBegin = lineEnd + 1;
  }

  if (state == ParserState::beforeMarker) {
    // Empty file or only comments: the mandatory marker is missing.
    result.error = BatchFormatError::missingMarker;
    result.diagnostic = "missing batch marker: expected XFOIL-BATCH 1";
    return result;
  }
  if (state != ParserState::commandStream) {
    result.error = BatchFormatError::missingCommandsSection;
    result.diagnostic = "missing batch section: commands";
    return result;
  }

  return result;
}

}  // namespace xfoil::app::cli

// End: src/app/cli/BatchConfigParser.cpp

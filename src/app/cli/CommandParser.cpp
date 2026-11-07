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

// File: src/app/cli/CommandParser.cpp
// Implementation of the ASKC-compatible line parser.
// Semantics as follows, derived from
// fortran-src/userio.f::ASKC:
// - leading spaces are removed,
// - the command ends at a space, '+', '-', '.', ',' or the first
// digit, at the latest after four characters,
// - the command is converted to uppercase letters (only a-z),
// - the argument begins at the found separator (CARGS = LINE(K:128));
// an appended separating comma stays preserved, only outside
// (space) is trimmed (STRIP),
// - if the line begins with a delimiter, the command corresponds to
// the first four characters of the line and the argument to the
// entire trimmed line, as in the ASKC branch K.EQ.1,
// - nothing numeric is interpreted.

#include "app/cli/CommandParser.h"

namespace xfoil::app::cli {
namespace {

bool isCommandTerminator(char character) {
  return character == ' ' || character == '+' || character == '-' ||
         character == '.' || character == ',' ||
         (character >= '0' && character <= '9');
}

char toUpperAscii(char character) {
  if (character >= 'a' && character <= 'z') {
    return static_cast<char>(character - 'a' + 'A');
  }
  return character;
}

std::string upperCase(std::string_view text) {
  std::string result{text};
  for (char& character : result) {
    character = toUpperAscii(character);
  }
  return result;
}

void trimSpaces(std::string& text) {
  std::size_t begin = 0;
  while (begin < text.size() && text[begin] == ' ') {
    ++begin;
  }

  std::size_t end = text.size();
  while (end > begin && text[end - 1] == ' ') {
    --end;
  }

  text = text.substr(begin, end - begin);
}

std::string trimmed(std::string_view text) {
  std::string result{text};
  trimSpaces(result);
  return result;
}

}  // namespace

ParsedCommand parseCommandLine(std::string_view line) {
  ParsedCommand parsed;

  // Remove leading spaces (ASKC loop via LINE(1:1)).
  std::size_t start = 0;
  while (start < line.size() && line[start] == ' ') {
    ++start;
  }

  if (start >= line.size()) {
    parsed.empty = true;
    return parsed;
  }

  const std::string_view stripped = line.substr(start);

  // ASKC searches the first separator in the entire input line. The
  // assignment to the four-character-wide Fortran command field then limits
  // only the command names; the argument part begins at the
  // found separator.
  std::size_t terminator = std::string_view::npos;
  for (std::size_t index = 0; index < stripped.size(); ++index) {
    if (isCommandTerminator(stripped[index])) {
      terminator = index;
      break;
    }
  }

  // ASKC branch K.EQ.1: The line begins with a delimiter. The
  // command keeps the first at most four characters, the argument the
  // complete (trimmed) line.
  if (terminator == 0) {
    parsed.command = upperCase(stripped.substr(0, 4));
    parsed.argument = trimmed(stripped);
    return parsed;
  }

  if (terminator == std::string_view::npos) {
    // No separator: The assignment to CHARACTER*4 takes over at most
    // the first four characters. A remainder beyond that is no separate
    // argument without a separator.
    const std::size_t commandLength =
    stripped.size() < 4 ? stripped.size() : 4;

    parsed.command =
    upperCase(stripped.substr(0, commandLength));
    parsed.argument.clear();
    return parsed;
  }

  // Normal decomposition: command before the delimiter (at most four
  // characters), argument from the delimiter as in the ASKC branch
  // CARGS = LINE(K:128). A leading separating comma is dropped,
  // signs are retained.
  const std::size_t commandLength = terminator < 4 ? terminator : 4;
  parsed.command = upperCase(stripped.substr(0, commandLength));

  // FCPP014: ASKC sets CARGS = LINE(K:128) and STRIP removed only
  // spaces; the argument begins at the separator and keeps a
  // appended comma (e.g. "NACA,0012" -> ",0012"). The downstream
  // numeric parser treats the comma as a separator, as Fortran does.
  parsed.argument = trimmed(stripped.substr(terminator));
  return parsed;
}

}  // namespace xfoil::app::cli

// End: src/app/cli/CommandParser.cpp

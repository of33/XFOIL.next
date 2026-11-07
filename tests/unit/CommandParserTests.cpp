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

// File: tests/unit/CommandParserTests.cpp
// unit tests for the ASKC-compatible line parser.

#include "TestSupport.h"
#include "app/cli/CommandParser.h"

#include <string>

namespace {

xfoil::app::cli::ParsedCommand parse(const std::string& line) {
  return xfoil::app::cli::parseCommandLine(std::string_view{line});
}

}  // namespace

TEST(unit, CommandParserRecognizesEmptyLine) {
  const auto emptyLine = parse("");
  CHECK(emptyLine.empty);
  CHECK_EQ(emptyLine.command, std::string{});
  CHECK_EQ(emptyLine.argument, std::string{});

  const auto blankLine = parse("   ");
  CHECK(blankLine.empty);
  CHECK_EQ(blankLine.command, std::string{});
  CHECK_EQ(blankLine.argument, std::string{});
}

TEST(unit, CommandParserUppercasesCommand) {
  const auto parsed = parse("oper");
  CHECK_FALSE(parsed.empty);
  CHECK_EQ(parsed.command, std::string{"OPER"});
  CHECK_EQ(parsed.argument, std::string{});
}

TEST(unit, CommandParserSeparatesWhitespaceArgument) {
  const auto parsed = parse("alfa 4.0");
  CHECK_EQ(parsed.command, std::string{"ALFA"});
  CHECK_EQ(parsed.argument, std::string{"4.0"});
}

TEST(unit, CommandParserSeparatesAttachedNumber) {
  const auto parsed = parse("alfa4.0");
  CHECK_EQ(parsed.command, std::string{"ALFA"});
  CHECK_EQ(parsed.argument, std::string{"4.0"});
}

TEST(unit, CommandParserPreservesNegativeNumber) {
  const auto parsed = parse("Re-100000");
  CHECK_EQ(parsed.command, std::string{"RE"});
  CHECK_EQ(parsed.argument, std::string{"-100000"});
}

TEST(unit, CommandParserHandlesCommaSeparator) {
  // FCPP014: ASKC sets CARGS = LINE(K:128); the argument starts at the
  // separator and keeps the trailing comma (STRIP removes only
  // spaces). The numeric parser treats the comma as a separator.
  const auto parsed = parse("naca,2412");
  CHECK_EQ(parsed.command, std::string{"NACA"});
  CHECK_EQ(parsed.argument, std::string{",2412"});
}

TEST(unit, CommandParserLimitsCommandToFourCharacters) {
  const auto parsed = parse("xycm .25 0");
  CHECK_EQ(parsed.command, std::string{"XYCM"});
  CHECK_EQ(parsed.argument, std::string{".25 0"});

  const auto longer = parse("operate 5");
  CHECK_EQ(longer.command, std::string{"OPER"});
  CHECK_EQ(longer.argument, std::string{"5"});
}

TEST(unit, CommandParserDoesNotTreatLeadingDotAsMenuAlias) {
  const auto parsed = parse(".OPER");
  CHECK_FALSE(parsed.empty);
  CHECK_NE(parsed.command, std::string{"OPER"});
}

TEST(unit, CommandParserTrimsArgument) {
  const auto parsed = parse("  alfa    4.0   ");
  CHECK_EQ(parsed.command, std::string{"ALFA"});
  CHECK_EQ(parsed.argument, std::string{"4.0"});
}

TEST(unit, CommandParserKeepsQuestionMark) {
  const auto parsed = parse("?");
  CHECK_FALSE(parsed.empty);
  CHECK_EQ(parsed.command, std::string{"?"});
  CHECK_EQ(parsed.argument, std::string{});
}

// End: tests/unit/CommandParserTests.cpp

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

// file: tests/unit/BatchConfigParserTests.cpp
// unit-tests of the Batch job parser after the Test matrix 
// valid minimal file, all supported forms, unknown version,
// missing marker, missing [commands], unknown section, unknown
// key (also prefix/airfoils/quiet), duplicate key, empty file,
// comments only, CRLF/LF, BOM tolerance, boundary limits (line length,
// file count, command count), comment removal in the command range,
// blank lines in the command stream, path values with space/quote chars.

#include "TestSupport.h"
#include "app/cli/BatchConfigParser.h"

#include <string>

namespace {

std::string minimalJob() {
  return "XFOIL-BATCH 1\n[commands]\nNACA 2412\nQUIT\n";
}

}  // namespace

TEST(unit, BatchParserAcceptsMinimalJob) {
  const auto result = xfoil::app::cli::parseBatchConfig(minimalJob());
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{2});
  CHECK_EQ(result.job.commandLines[0], std::string{"NACA 2412"});
  CHECK_EQ(result.job.commandLines[1], std::string{"QUIT"});
  CHECK_FALSE(result.job.settings.defaultsPath.has_value());
  CHECK(result.job.settings.stopOnError);
  CHECK_FALSE(result.job.settings.stopOnNonConvergence);
  CHECK(result.job.settings.allowUnavailablePlotCommands);
}

TEST(unit, BatchParserAcceptsEmptyCommandsSection) {
  const auto result =
      xfoil::app::cli::parseBatchConfig("XFOIL-BATCH 1\n[commands]\n");
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{0});
}

TEST(unit, BatchParserAcceptsAllJobKeys) {
  const std::string text =
      "XFOIL-BATCH 1\n"
      "[job]\n"
      "defaults = xfoil.def\n"
      "stop-on-error = false\n"
      "stop-on-nonconvergence = true\n"
      "allow-unavailable-plot-commands = false\n"
      "[commands]\n"
      "QUIT\n";
  const auto result = xfoil::app::cli::parseBatchConfig(text);
  CHECK(result.success());
  CHECK(result.job.settings.defaultsPath.has_value());
  CHECK_EQ(*result.job.settings.defaultsPath, std::string{"xfoil.def"});
  CHECK_FALSE(result.job.settings.stopOnError);
  CHECK(result.job.settings.stopOnNonConvergence);
  CHECK_FALSE(result.job.settings.allowUnavailablePlotCommands);
}

TEST(unit, BatchParserAllowsCommentsAndBlankLinesBeforeSections) {
  const std::string text =
      "# comment before marker\n"
      "\n"
      "   # indented comment\n"
      "XFOIL-BATCH 1\n"
      "\n"
      "# comment in section zone\n"
      "[job]\n"
      "\n"
      "[commands]\n"
      "QUIT\n";
  const auto result = xfoil::app::cli::parseBatchConfig(text);
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{1});
}

TEST(unit, BatchParserRejectsUnsupportedVersion) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 2\n[commands]\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(xfoil::app::cli::BatchFormatError::
                                unsupportedVersion));
  CHECK_EQ(result.diagnostic,
           std::string{"unsupported batch format version"});
}

TEST(unit, BatchParserRejectsMissingMarker) {
  const auto result =
      xfoil::app::cli::parseBatchConfig("[commands]\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::missingMarker));
}

TEST(unit, BatchParserRejectsEmptyFileAndCommentsOnly) {
  const auto empty = xfoil::app::cli::parseBatchConfig("");
  CHECK_EQ(static_cast<int>(empty.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::missingMarker));
  const auto comments =
      xfoil::app::cli::parseBatchConfig("# only a Comment\n\n");
  CHECK_EQ(static_cast<int>(comments.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::missingMarker));
}

TEST(unit, BatchParserRejectsMissingCommandsSection) {
  const auto withoutSection =
      xfoil::app::cli::parseBatchConfig("XFOIL-BATCH 1\n");
  CHECK_EQ(static_cast<int>(withoutSection.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::missingCommandsSection));
  const auto onlyJob = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[job]\nstop-on-error = true\n");
  CHECK_EQ(static_cast<int>(onlyJob.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::missingCommandsSection));
}

TEST(unit, BatchParserRejectsUnknownSection) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[global]\nfoo = bar\n[commands]\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::unknownSection));
  CHECK_EQ(result.diagnostic, std::string{"unknown batch section: global"});
}

TEST(unit, BatchParserRejectsForbiddenJobKeys) {
  for (const std::string key : {"prefix", "profile", "quiet"}) {
    const auto result = xfoil::app::cli::parseBatchConfig(
        "XFOIL-BATCH 1\n[job]\n" + key + " = x\n[commands]\nQUIT\n");
    CHECK_EQ(static_cast<int>(result.error),
             static_cast<int>(
                 xfoil::app::cli::BatchFormatError::unknownKey));
    CHECK_EQ(result.diagnostic, "unknown batch key: " + key);
  }
}

TEST(unit, BatchParserRejectsDuplicateKeys) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[job]\nstop-on-error = true\n"
      "stop-on-error = false\n[commands]\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::duplicateKey));
  CHECK_EQ(result.diagnostic,
           std::string{"duplicate batch key: stop-on-error"});
}

TEST(unit, BatchParserRejectsInvalidBoolValues) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[job]\nstop-on-error = TRUE\n[commands]\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::invalidValue));
}

TEST(unit, BatchParserAcceptsCrlfAndRemovesCarriageReturns) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\r\n[commands]\r\nNACA 2412\r\nQUIT\r\n");
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{2});
  CHECK_EQ(result.job.commandLines[0], std::string{"NACA 2412"});
}

TEST(unit, BatchParserToleratesLeadingBomOnly) {
  const auto leading = xfoil::app::cli::parseBatchConfig(
      "\xEF\xBB\xBF" + minimalJob());
  CHECK(leading.success());
  CHECK_EQ(leading.job.commandLines.size(), std::size_t{2});
  const auto embedded = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[commands]\n\xEF\xBB\xBF"
      "QUIT\n");
  CHECK_EQ(static_cast<int>(embedded.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::invalidByteOrderMark));
}

TEST(unit, BatchParserRejectsTooLongLine) {
  const std::string longLine(1025, 'x');
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[commands]\n" + longLine + "\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::lineTooLong));
}

TEST(unit, BatchParserAcceptsLineAtLengthLimit) {
  const std::string maxLine(1024, 'x');
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[commands]\n" + maxLine + "\nQUIT\n");
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines[0].size(), std::size_t{1024});
}

TEST(unit, BatchParserRejectsFileOverOneMebibyte) {
  std::string content = "XFOIL-BATCH 1\n[commands]\n";
  content.append(xfoil::app::cli::batchMaxFileBytes, 'q');
  content.push_back('\n');
  const auto result = xfoil::app::cli::parseBatchConfig(content);
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::fileTooLarge));
}

TEST(unit, BatchParserRejectsTooManyCommands) {
  std::string content = "XFOIL-BATCH 1\n[commands]\n";
  for (std::size_t index = 0;
       index < xfoil::app::cli::batchMaxCommandLines + 1; ++index) {
    content += "QUIT\n";
  }
  const auto result = xfoil::app::cli::parseBatchConfig(content);
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::tooManyCommands));
}

TEST(unit, BatchParserAcceptsCommandLimitAtBoundary) {
  std::string content = "XFOIL-BATCH 1\n[commands]\n";
  for (std::size_t index = 0; index < xfoil::app::cli::batchMaxCommandLines;
       ++index) {
    content += "QUIT\n";
  }
  content += "# trailing comment\n";
  const auto result = xfoil::app::cli::parseBatchConfig(content);
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(),
           xfoil::app::cli::batchMaxCommandLines);
}

TEST(unit, BatchParserRemovesCommentsInsideCommands) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[commands]\nQUIT\n# comment\n   # indented\n");
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{1});
  CHECK_EQ(result.job.commandLines[0], std::string{"QUIT"});
}

TEST(unit, BatchParserKeepsBlankLinesAndVerbatimCommandText) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[commands]\nOPER\n\nALFA  4.0 \nQUIT\n");
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{4});
  CHECK_EQ(result.job.commandLines[1], std::string{""});
  CHECK_EQ(result.job.commandLines[2], std::string{"ALFA  4.0 "});
}

TEST(unit, BatchParserKeepsSectionLikeLinesAfterCommandsAsCommands) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[commands]\n[job]\nQUIT\n");
  CHECK(result.success());
  CHECK_EQ(result.job.commandLines.size(), std::size_t{2});
  CHECK_EQ(result.job.commandLines[0], std::string{"[job]"});
}

TEST(unit, BatchParserDefaultsPathKeepsSpacesAndQuotes) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\n[job]\ndefaults =   my \"xfoil\".def  \n"
      "[commands]\nQUIT\n");
  CHECK(result.success());
  CHECK(result.job.settings.defaultsPath.has_value());
  CHECK_EQ(*result.job.settings.defaultsPath,
           std::string{"my \"xfoil\".def"});
}

TEST(unit, BatchParserRejectsStrayLineBeforeSection) {
  const auto result = xfoil::app::cli::parseBatchConfig(
      "XFOIL-BATCH 1\nNACA 2412\n[commands]\nQUIT\n");
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::cli::BatchFormatError::strayBeforeCommands));
}

// End: tests/unit/BatchConfigParserTests.cpp

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

// File: tests/unit/CommandLineTests.cpp
// unit tests for the program argument parser. These tests exercise
// exclusively the pure parsing function without process start and
// without output.

#include "TestSupport.h"
#include "app/CommandLine.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

xfoil::app::CommandLineParseResult parse(
    const std::vector<std::string_view>& arguments) {
  return xfoil::app::parseCommandLine(std::span<const std::string_view>{
      arguments.data(), arguments.size()});
}

}  // namespace

TEST(unit, CommandLineDefaultsToInteractiveMode) {
  const auto result = parse({});
  CHECK(result.success());
  CHECK_EQ(static_cast<int>(result.options.mode),
           static_cast<int>(xfoil::app::ProgramMode::interactive));
  CHECK_FALSE(result.options.inputPath.has_value());
}

TEST(unit, CommandLineAcceptsInteractiveInputFile) {
  const auto result = parse({"profile.dat"});
  CHECK(result.success());
  CHECK_EQ(static_cast<int>(result.options.mode),
           static_cast<int>(xfoil::app::ProgramMode::interactive));
  CHECK(result.options.inputPath.has_value());
  if (result.options.inputPath.has_value()) {
    CHECK_EQ(*result.options.inputPath, std::string{"profile.dat"});
  }
}

TEST(unit, CommandLineRejectsMultipleInputFiles) {
  const auto result = parse({"a.dat", "b.dat"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(xfoil::app::CommandLineError::multipleInputFiles));
  CHECK_EQ(result.diagnostic,
           std::string{"Only one airfoil input file may be specified"});
}

TEST(unit, CommandLineRejectsUnknownOption) {
  const auto result = parse({"--unknown"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(xfoil::app::CommandLineError::unknownOption));
  CHECK_EQ(result.diagnostic, std::string{"Unknown option: --unknown"});
}

TEST(unit, CommandLineAcceptsHelpAndVersion) {
  const auto help = parse({"--help"});
  CHECK(help.success());
  CHECK_EQ(static_cast<int>(help.options.mode),
           static_cast<int>(xfoil::app::ProgramMode::help));
  const auto version = parse({"--version"});
  CHECK(version.success());
  CHECK_EQ(static_cast<int>(version.options.mode),
           static_cast<int>(xfoil::app::ProgramMode::version));
}

TEST(unit, CommandLineAcceptsBatchWithFile) {
  const auto result = parse({"--batch", "job.xfb"});
  CHECK(result.success());
  CHECK_EQ(static_cast<int>(result.options.mode),
           static_cast<int>(xfoil::app::ProgramMode::batch));
  CHECK_EQ(result.options.batchPath, std::string{"job.xfb"});
  CHECK_FALSE(result.options.inputPath.has_value());
}

TEST(unit, CommandLineAcceptsBatchStdin) {
  const auto result = parse({"--batch", "-"});
  CHECK(result.success());
  CHECK_EQ(result.options.batchPath, std::string{"-"});
}

TEST(unit, CommandLineAcceptsBatchProfileOption) {
  const auto result = parse({"--batch", "job.xfb", "--profile", "p.dat"});
  CHECK(result.success());
  CHECK(result.options.profilePath.has_value());
  CHECK_EQ(*result.options.profilePath, std::string{"p.dat"});
}

TEST(unit, CommandLineAcceptsAllBatchOptionsOrderIndependent) {
  const auto forward = parse({"--batch", "job.xfb", "--profile", "p.dat",
                              "--prefix", "run01", "--quiet",
                              "--defaults", "xfoil.def",
                              "--working-directory", "work"});
  CHECK(forward.success());
  CHECK_EQ(forward.options.prefix, std::string{"run01"});
  CHECK(forward.options.quiet);
  CHECK_EQ(*forward.options.defaultsPath, std::string{"xfoil.def"});
  CHECK_EQ(*forward.options.workingDirectory, std::string{"work"});

  const auto backward = parse({"--working-directory", "work", "--defaults",
                               "xfoil.def", "--quiet", "--prefix", "run01",
                               "--profile", "p.dat", "--batch", "job.xfb"});
  CHECK(backward.success());
  CHECK_EQ(backward.options.batchPath, std::string{"job.xfb"});
  CHECK_EQ(backward.options.prefix, std::string{"run01"});
}

TEST(unit, CommandLineAcceptsEmptyPrefix) {
  const auto result = parse({"--batch", "job.xfb", "--prefix", ""});
  CHECK(result.success());
  CHECK_EQ(result.options.prefix, std::string{});
}

TEST(unit, CommandLineRejectsBatchWithoutFile) {
  const auto result = parse({"--batch"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::CommandLineError::missingOptionArgument));
  CHECK_EQ(result.diagnostic,
           std::string{"--batch requires a file argument"});
}

TEST(unit, CommandLineRejectsDuplicateBatch) {
  const auto result = parse({"--batch", "a.xfb", "--batch", "b.xfb"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(xfoil::app::CommandLineError::duplicateBatchOption));
  CHECK_EQ(result.diagnostic,
           std::string{"The --batch option may only be specified once"});
}

TEST(unit, CommandLineRejectsDuplicateProfile) {
  const auto result = parse({"--batch", "job.xfb", "--profile", "a.dat",
                             "--profile", "b.dat"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::CommandLineError::duplicateProfileOption));
}

TEST(unit, CommandLineRejectsDuplicatePrefix) {
  const auto result = parse({"--batch", "job.xfb", "--prefix", "a",
                             "--prefix", "b"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::CommandLineError::duplicatePrefixOption));
}

TEST(unit, CommandLineAcceptsDuplicateQuiet) {
  const auto result = parse({"--batch", "job.xfb", "--quiet", "--quiet"});
  CHECK(result.success());
  CHECK(result.options.quiet);
}

TEST(unit, CommandLineRejectsProfileArgumentWithBatch) {
  const auto result = parse({"--batch", "job.xfb", "profile.dat"});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(
               xfoil::app::CommandLineError::profileArgumentWithBatch));
  CHECK_EQ(result.diagnostic,
           std::string{"a profile argument cannot be combined with --batch"});
}

TEST(unit, CommandLineRejectsBatchOptionsWithoutBatch) {
  const std::vector<std::pair<std::vector<std::string_view>, std::string>>
      cases = {
          {{"--profile", "p.dat"}, "--profile requires --batch"},
          {{"--prefix", "run"}, "--prefix requires --batch"},
          {{"--quiet"}, "--quiet requires --batch"},
          {{"--defaults", "xfoil.def"}, "--defaults requires --batch"},
          {{"--working-directory", "work"},
           "--working-directory requires --batch"},
      };
  for (const auto& [arguments, expectedDiagnostic] : cases) {
    const auto result = parse(arguments);
    CHECK_FALSE(result.success());
    CHECK_EQ(result.diagnostic, expectedDiagnostic);
  }
}

TEST(unit, CommandLineRejectsInvalidPrefixCharacters) {
  for (const std::string& prefix : {std::string{"a/b"}, std::string{"a b"},
                                    std::string{"a+b"}, std::string{"a/b"},
                                    std::string{"ä"}}) {
    const auto result = parse({"--batch", "job.xfb", "--prefix", prefix});
    CHECK_FALSE(result.success());
    CHECK_EQ(static_cast<int>(result.error),
             static_cast<int>(
                 xfoil::app::CommandLineError::invalidBatchPrefix));
    CHECK_EQ(result.diagnostic, "Invalid batch prefix: " + prefix);
  }
}

TEST(unit, CommandLineRejectsPrefixOver128Bytes) {
  const std::string tooLong(129, 'x');
  const auto result = parse({"--batch", "job.xfb", "--prefix", tooLong});
  CHECK_FALSE(result.success());
  CHECK_EQ(static_cast<int>(result.error),
           static_cast<int>(xfoil::app::CommandLineError::invalidBatchPrefix));

  const std::string boundary(128, 'x');
  const auto allowed = parse({"--batch", "job.xfb", "--prefix", boundary});
  CHECK(allowed.success());
}

TEST(unit, CommandLineAcceptsAllowedPrefixCharacters) {
  const auto result = parse({"--batch", "job.xfb",
                             "--prefix", "aZ09._-"});
  CHECK(result.success());
  CHECK_EQ(result.options.prefix, std::string{"aZ09._-"});
}

// End: tests/unit/CommandLineTests.cpp

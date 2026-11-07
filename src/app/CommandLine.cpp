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

// File: src/app/CommandLine.cpp
// Implementation of the program argument parser.
// Contract:
// xfoil.next [airfoils.dat]
// xfoil.next --batch FILE|- [--airfoils FILE] [--prefix PREFIX] [--quiet]
// [--defaults FILE] [--working-directory DIR]
// xfoil.next --help | --version
// The first detected argument error wins; the diagnostic is exact.
// Prefix validity (character set [A-Za-z0-9._-]*, length
// 0-128 bytes) is checked here before each file operation.

#include "app/CommandLine.h"

#include <cstddef>

namespace xfoil::app {
namespace {

constexpr std::size_t maxPrefixBytes = 128;

bool isValidBatchPrefix(std::string_view prefix) {
  if (prefix.size() > maxPrefixBytes) {
    return false;
  }
  for (const char symbol : prefix) {
    const bool allowed = (symbol >= 'a' && symbol <= 'z') ||
                         (symbol >= 'A' && symbol <= 'Z') ||
                         (symbol >= '0' && symbol <= '9') ||
                         symbol == '.' || symbol == '_' || symbol == '-';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

}  // namespace

CommandLineParseResult parseCommandLine(
    std::span<const std::string_view> arguments) {
  CommandLineParseResult result;

  bool batchSeen = false;
  bool profileSeen = false;
  bool prefixSeen = false;
  bool quietSeen = false;

  std::size_t index = 0;
  while (index < arguments.size()) {
    const std::string_view argument = arguments[index];

    if (argument == "--help") {
      result.options.mode = ProgramMode::help;
      return result;
    }
    if (argument == "--version") {
      result.options.mode = ProgramMode::version;
      return result;
    }

    if (argument == "--batch") {
      if (batchSeen) {
        result.error = CommandLineError::duplicateBatchOption;
        result.diagnostic =
            "The --batch option may only be specified once";
        return result;
      }
      batchSeen = true;
      result.options.mode = ProgramMode::batch;
      if (index + 1 >= arguments.size()) {
        result.error = CommandLineError::missingOptionArgument;
        result.diagnostic = "--batch requires a file argument";
        return result;
      }
      result.options.batchPath = std::string{arguments[index + 1]};
      index += 2;
      continue;
    }

    if (argument == "--profile") {
      if (profileSeen) {
        result.error = CommandLineError::duplicateProfileOption;
        result.diagnostic =
            "The --profile option may only be specified once";
        return result;
      }
      profileSeen = true;
      if (index + 1 >= arguments.size()) {
        result.error = CommandLineError::missingOptionArgument;
        result.diagnostic = "--profile requires a file argument";
        return result;
      }
      result.options.profilePath = std::string{arguments[index + 1]};
      index += 2;
      continue;
    }

    if (argument == "--prefix") {
      if (prefixSeen) {
        result.error = CommandLineError::duplicatePrefixOption;
        result.diagnostic =
            "The --prefix option may only be specified once";
        return result;
      }
      prefixSeen = true;
      if (index + 1 >= arguments.size()) {
        result.error = CommandLineError::missingOptionArgument;
        result.diagnostic = "--prefix requires a prefix argument";
        return result;
      }
      const std::string_view prefix = arguments[index + 1];
      if (!isValidBatchPrefix(prefix)) {
        result.error = CommandLineError::invalidBatchPrefix;
        result.diagnostic = "Invalid batch prefix: " + std::string{prefix};
        return result;
      }
      result.options.prefix = std::string{prefix};
      index += 2;
      continue;
    }

    if (argument == "--defaults") {
      if (index + 1 >= arguments.size()) {
        result.error = CommandLineError::missingOptionArgument;
        result.diagnostic = "--defaults requires a file argument";
        return result;
      }
      result.options.defaultsPath = std::string{arguments[index + 1]};
      index += 2;
      continue;
    }

    if (argument == "--working-directory") {
      if (index + 1 >= arguments.size()) {
        result.error = CommandLineError::missingOptionArgument;
        result.diagnostic =
            "--working-directory requires a directory argument";
        return result;
      }
      result.options.workingDirectory =
          std::string{arguments[index + 1]};
      index += 2;
      continue;
    }

    if (argument == "--quiet") {
      quietSeen = true;
      result.options.quiet = true;  // multiple times permitted, idempotent
      ++index;
      continue;
    }

    if (!argument.empty() && argument.front() == '-') {
      result.error = CommandLineError::unknownOption;
      result.diagnostic = "Unknown option: " + std::string{argument};
      return result;
    }

    if (result.options.inputPath.has_value()) {
      result.error = CommandLineError::multipleInputFiles;
      result.diagnostic =
          "Only one airfoil input file may be specified";
      return result;
    }

    result.options.inputPath = std::string{argument};
    ++index;
  }

  if (result.options.mode == ProgramMode::batch) {
    if (result.options.inputPath.has_value()) {
      result.error = CommandLineError::profileArgumentWithBatch;
      result.diagnostic =
          "a profile argument cannot be combined with --batch";
      return result;
    }
    return result;
  }

  if (profileSeen) {
    result.error = CommandLineError::profileRequiresBatch;
    result.diagnostic = "--profile requires --batch";
    return result;
  }
  if (prefixSeen) {
    result.error = CommandLineError::prefixRequiresBatch;
    result.diagnostic = "--prefix requires --batch";
    return result;
  }
  if (quietSeen) {
    result.error = CommandLineError::quietRequiresBatch;
    result.diagnostic = "--quiet requires --batch";
    return result;
  }
  if (result.options.defaultsPath.has_value()) {
    result.error = CommandLineError::defaultsRequiresBatch;
    result.diagnostic = "--defaults requires --batch";
    return result;
  }
  if (result.options.workingDirectory.has_value()) {
    result.error = CommandLineError::workingDirectoryRequiresBatch;
    result.diagnostic = "--working-directory requires --batch";
    return result;
  }

  return result;
}

}  // namespace xfoil::app

// End: src/app/CommandLine.cpp

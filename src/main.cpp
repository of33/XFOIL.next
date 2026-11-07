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

#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "app/CommandLine.h"
#include "app/ProductInfo.h"
#include "app/cli/BatchRunner.h"
#include "app/cli/CommandInterpreter.h"

namespace {

// Usage text (/10, new in): stdout, exit 0.
void printUsage(std::ostream& out) {
  out << "Usage: xfoil.next [OPTION]... [AIRFOILFILE]\n"
      << "  (no option)            interactive XFOIL session, optional\n"
      << "                         airfoil file as startup profile\n"
      << "  --batch FILE           run one batch job from FILE ('-' = stdin)\n"
      << "  --profile FILE         startup profile for the batch session\n"
      << "  --prefix PREFIX        prefix for session output files\n"
      << "  --quiet                suppress batch output on stdout\n"
      << "  --defaults FILE        defaults file loaded via RDEF before the\n"
      << "                         job commands\n"
      << "  --working-directory DIR\n"
      << "                         working directory for the batch job\n"
      << "  --help                 show this help and exit\n"
      << "  --version              show version information and exit\n";
}

}  // namespace

// main is reduced to mode selection: parse the arguments,
// report an argument error with exit code 2, then start the batch or
// interpreter application. The batch branch calls the native
// BatchRunner.
// No solver logic.
int main(int argc, char** argv) {
  std::vector<std::string_view> arguments;
  if (argc > 1) {
    arguments.reserve(static_cast<std::size_t>(argc) - 1);
    for (int index = 1; index < argc; ++index) {
      arguments.emplace_back(argv[index]);
    }
  }

  const auto parseResult = xfoil::app::parseCommandLine(
      std::span<const std::string_view>{arguments});

  if (!parseResult.success()) {
    std::cerr << parseResult.diagnostic << "\n";
    return 2;
  }

  if (parseResult.options.mode == xfoil::app::ProgramMode::help) {
    printUsage(std::cout);
    return 0;
  }

  if (parseResult.options.mode == xfoil::app::ProgramMode::version) {
    // The version line comes exclusively from the central
    // product source; no banner, no session, no file access.
    std::cout << xfoil::app::product::name << " "
              << xfoil::app::product::version << "\n";
    return 0;
  }

  if (parseResult.options.mode == xfoil::app::ProgramMode::batch) {
    xfoil::app::cli::BatchRunOptions batchOptions;
    batchOptions.batchPath = parseResult.options.batchPath;
    batchOptions.profilePath = parseResult.options.profilePath;
    batchOptions.prefix = parseResult.options.prefix;
    batchOptions.quiet = parseResult.options.quiet;
    batchOptions.defaultsPath = parseResult.options.defaultsPath;
    batchOptions.workingDirectory = parseResult.options.workingDirectory;

    return xfoil::app::cli::runBatch(
        batchOptions,
        std::cin,
        std::cout,
        std::cerr);
  }

  xfoil::app::cli::InteractiveStartupOptions interactiveOptions;
  interactiveOptions.inputPath = parseResult.options.inputPath;

  xfoil::app::cli::CommandInterpreter interpreter(
      std::cin,
      std::cout,
      std::cerr);

  return interpreter.run(interactiveOptions);
}

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

// File: src/app/cli/BatchRunner.cpp
// Implementation of the native batch runner.
// The runner itself executes no commands: it builds the command stream,
// changes the working directory, sets OutputPathPolicy and the tee stream,
// checks the runner output files, writes results.dat and derives the
// exit code from the structured RunSummary (first-error-wins).
// Re-parsing stdout/the transcript does not take place.

#include "app/cli/BatchRunner.h"

#include <cmath>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>

#include "app/ProductInfo.h"
#include "app/cli/BatchConfigParser.h"
#include "app/cli/CommandInterpreter.h"
#include "xfoil/io/OutputPathPolicy.h"

namespace xfoil::app::cli {
namespace {

// Tee stream: writes each interpreter output completely
// to the transcript file and, except with --quiet, to stdout. stderr is
// not affected.
class TeeStreambuf : public std::streambuf {
 public:
  TeeStreambuf(std::streambuf* transcript, std::streambuf* standard)
      : transcript_(transcript), standard_(standard) {}

  TeeStreambuf(const TeeStreambuf&) = delete;
  TeeStreambuf& operator=(const TeeStreambuf&) = delete;

 protected:
  int overflow(int character) override {
    if (character == traits_type::eof()) {
      return traits_type::not_eof(character);
    }
    const char symbol = static_cast<char>(character);
    if (transcript_->sputn(&symbol, 1) != 1) {
      return traits_type::eof();
    }
    if (standard_ != nullptr) {
      standard_->sputn(&symbol, 1);
    }
    return character;
  }

  int sync() override {
    const int transcriptSync = transcript_->pubsync();
    const int standardSync =
        standard_ != nullptr ? standard_->pubsync() : 0;
    return (transcriptSync == 0 && standardSync == 0) ? 0 : -1;
  }

 private:
  std::streambuf* transcript_;
  std::streambuf* standard_;
};

// RAII working-directory switch; the destructor
// always restores the previous path.
class ScopedWorkingDirectory {
 public:
  explicit ScopedWorkingDirectory(const std::filesystem::path& directory)
      : previous_(std::filesystem::current_path()) {
    std::filesystem::current_path(directory);
  }

  ScopedWorkingDirectory(const ScopedWorkingDirectory&) = delete;
  ScopedWorkingDirectory& operator=(const ScopedWorkingDirectory&) = delete;

  ~ScopedWorkingDirectory() {
    std::error_code ignored;
    std::filesystem::current_path(previous_, ignored);
  }

 private:
  std::filesystem::path previous_;
};

// Fixed notation %.6f with nan/inf/-inf.
std::string fixed6(double value) {
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::isinf(value)) {
    return value > 0.0 ? "inf" : "-inf";
  }
  char buffer[64];
  const int written = std::snprintf(buffer, sizeof(buffer), "%.6f", value);
  if (written < 0 || static_cast<std::size_t>(written) >= sizeof(buffer)) {
    return "nan";  // defensive; non-finite values are handled above
  }
  return std::string{buffer, static_cast<std::size_t>(written)};
}

// Quoting the attribute values of the geometry comment line: escape
// backslash, quotation mark, line feed and CR.
std::string quoted(const std::string& value) {
  std::string out;
  out.push_back('"');
  for (const char symbol : value) {
    switch (symbol) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        out.push_back(symbol);
        break;
    }
  }
  out.push_back('"');
  return out;
}

void writeGeometryBlock(std::ostream& out, const RunGeometryRecord& record) {
  out << "# geometry name=" << quoted(record.name)
      << " source=" << quoted(record.source) << " points=" << record.points
      << "\n";
}

// Settings comment block in three lines; blpar
// follows the field order of core::boundary_layer::
// BoundaryLayerParameters as name:value pairs with fixed %.6f formatting.
void writeSettingsBlock(std::ostream& out, const RunSettingsSnapshot& snap) {
  out << "# settings re=" << fixed6(snap.re) << " mach=" << fixed6(snap.mach)
      << " matyp=" << snap.machType << " retyp=" << snap.reType
      << " viscous=" << (snap.viscous ? 1 : 0) << " iter="
      << snap.maximumIterations << "\n";
  out << "# settings ncrit_top=" << fixed6(snap.ncritTop)
      << " ncrit_bottom=" << fixed6(snap.ncritBottom)
      << " xtrip_top=" << fixed6(snap.xtripTop)
      << " xtrip_bottom=" << fixed6(snap.xtripBottom) << "\n";
  out << "# settings vaccel=" << fixed6(snap.viscousAcceleration)
      << " waklen=" << fixed6(snap.wakeLength) << " idamp=" << snap.idamp
      << " blpar=sccon:" << fixed6(snap.blpar.sccon)
      << ",gacon:" << fixed6(snap.blpar.gacon)
      << ",gbcon:" << fixed6(snap.blpar.gbcon)
      << ",gccon:" << fixed6(snap.blpar.gccon)
      << ",dlcon:" << fixed6(snap.blpar.dlcon)
      << ",ctrcon:" << fixed6(snap.blpar.ctrcon)
      << ",ctrcex:" << fixed6(snap.blpar.ctrcex)
      << ",duxcon:" << fixed6(snap.blpar.duxcon)
      << ",ctcon:" << fixed6(snap.blpar.ctcon)
      << ",cffac:" << fixed6(snap.blpar.cffac) << "\n";
}

// results.dat version 1. The product metadata line
// is the only permitted extension of the header; name and
// version derive from app/ProductInfo.h. Format version and column header
// remain unchanged.
void writeResultsDat(std::ostream& out, const RunSummary& summary) {
  out << "# XFOIL_BATCH_RESULTS 1\n";
  out << "# product " << product::name << " " << product::version << "\n";
  out << "# index alpha cl cd cdp cm re mach ncrit_top ncrit_bottom"
         " xtr_top xtr_bottom converged iterations\n";
  std::size_t geometryCursor = 0;
  std::size_t settingsCursor = 0;
  for (std::size_t index = 0; index < summary.pointResults.size(); ++index) {
    while (geometryCursor < summary.geometryRecords.size() &&
           summary.geometryRecords[geometryCursor].pointIndex <= index) {
      writeGeometryBlock(out, summary.geometryRecords[geometryCursor]);
      ++geometryCursor;
    }
    while (settingsCursor < summary.settingsChanges.size() &&
           summary.settingsChanges[settingsCursor].first <= index) {
      writeSettingsBlock(out, summary.settingsChanges[settingsCursor].second);
      ++settingsCursor;
    }
    const auto& point = summary.pointResults[index];
    out << (index + 1) << " " << fixed6(point.alpha) << " "
        << fixed6(point.cl) << " " << fixed6(point.cd) << " "
        << fixed6(point.cdp) << " " << fixed6(point.cm) << " "
        << fixed6(point.re) << " " << fixed6(point.mach) << " "
        << fixed6(point.ncritTop) << " " << fixed6(point.ncritBottom) << " "
        << fixed6(point.xtrTop) << " " << fixed6(point.xtrBottom) << " "
        << (point.converged ? 1 : 0) << " " << point.iterations << "\n";
  }
  // After the run: geometry/settings blocks without a following data line
  // (job without points: header plus comments, no data lines).
  while (geometryCursor < summary.geometryRecords.size()) {
    writeGeometryBlock(out, summary.geometryRecords[geometryCursor]);
    ++geometryCursor;
  }
  while (settingsCursor < summary.settingsChanges.size()) {
    writeSettingsBlock(out, summary.settingsChanges[settingsCursor].second);
    ++settingsCursor;
  }
}

}  // namespace

int runBatch(const BatchRunOptions& options, std::istream& standardInput,
             std::ostream& standardOutput, std::ostream& errorOutput) {
  // 1. Read job content (file or stdin).
  std::string content;
  if (options.batchPath == "-") {
    std::ostringstream buffer;
    buffer << standardInput.rdbuf();
    content = buffer.str();
  } else {
    std::ifstream file(options.batchPath, std::ios::binary);
    if (!file) {
      errorOutput << "Cannot open batch file: " << options.batchPath << "\n";
      return batchExitRunnerFileError;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
  }

  // 2. Format check (exit 2).
  const BatchParseResult parsed = parseBatchConfig(content);
  if (!parsed.success()) {
    errorOutput << parsed.diagnostic << "\n";
    return batchExitArgumentOrFormatError;
  }

 // 3. Defaults ambiguity (exit 2).
  if (options.defaultsPath.has_value() &&
      parsed.job.settings.defaultsPath.has_value()) {
    errorOutput << "batch defaults specified twice: "
                   "--defaults and defaults =\n";
    return batchExitArgumentOrFormatError;
  }

 // 4. Effective working directory.
  std::filesystem::path workingDirectory;
  if (options.workingDirectory.has_value()) {
    workingDirectory = std::filesystem::path{*options.workingDirectory};
  } else if (options.batchPath != "-") {
    workingDirectory = std::filesystem::path{options.batchPath}.parent_path();
    if (workingDirectory.empty()) {
      workingDirectory = ".";
    }
  } else {
    workingDirectory = ".";
  }
  std::error_code directoryEc;
  if (!std::filesystem::is_directory(workingDirectory, directoryEc) ||
      directoryEc) {
    errorOutput << "Working directory does not exist: "
                << workingDirectory.string() << "\n";
    return batchExitRunnerFileError;
  }
  ScopedWorkingDirectory directoryGuard(workingDirectory);

  // 5. --defaults readability (runner I/O, exit 3) after the switch;
  // relative paths refer to the working directory.
  if (options.defaultsPath.has_value()) {
    std::ifstream probe(*options.defaultsPath, std::ios::binary);
    if (!probe) {
      errorOutput << "Cannot open defaults file: " << *options.defaultsPath
                  << "\n";
      return batchExitRunnerFileError;
    }
  }

  // 6. Runner output files: prefix directly (not via the policy),
  // existence check before the first command (R7, exit 3).
  const std::string transcriptName = options.prefix + "transcript.txt";
  const std::string resultsName = options.prefix + "results.dat";
  std::error_code existsEc;
  if (std::filesystem::exists(transcriptName, existsEc)) {
    errorOutput << "Batch output file exists: " << transcriptName << "\n";
    return batchExitRunnerFileError;
  }
  if (std::filesystem::exists(resultsName, existsEc)) {
    errorOutput << "Batch output file exists: " << resultsName << "\n";
    return batchExitRunnerFileError;
  }
  std::ofstream transcript(transcriptName, std::ios::binary | std::ios::trunc);
  if (!transcript) {
    errorOutput << "Batch output file not writable: " << transcriptName
                << "\n";
    return batchExitRunnerFileError;
  }

  // 7. Command stream: RDEF line before the job commands when applicable
 // (existing RDEF core path; job commands character-exact).
  std::optional<std::string> defaultsPath;
  if (options.defaultsPath.has_value()) {
    defaultsPath = options.defaultsPath;
  } else if (parsed.job.settings.defaultsPath.has_value()) {
    defaultsPath = parsed.job.settings.defaultsPath;
  }
  std::ostringstream inputBuilder;
  if (defaultsPath.has_value()) {
    inputBuilder << "RDEF " << *defaultsPath << "\n";
  }
  for (const std::string& line : parsed.job.commandLines) {
    inputBuilder << line << "\n";
  }
  std::istringstream commandStream(inputBuilder.str());

  // 8. Interpreter with tee stream, OutputPathPolicy and job error policy.
  TeeStreambuf teeBuffer(transcript.rdbuf(),
                         options.quiet ? nullptr : standardOutput.rdbuf());
  std::ostream tee(&teeBuffer);

  CommandInterpreter interpreter(commandStream, tee, errorOutput);
  xfoil::io::OutputPathPolicy policy;
  policy.prefix = options.prefix;
  interpreter.setOutputPathPolicy(policy);

  CommandInterpreter::RunPolicy runPolicy;
  runPolicy.stopOnError = parsed.job.settings.stopOnError;
  runPolicy.stopOnNonConvergence = parsed.job.settings.stopOnNonConvergence;
  runPolicy.allowUnavailablePlotCommands =
      parsed.job.settings.allowUnavailablePlotCommands;
  interpreter.setRunPolicy(runPolicy);

  InteractiveStartupOptions startup;
  startup.inputPath = options.profilePath;
  startup.loadStartupDefaults = false;
  const int interpreterResult = interpreter.run(startup);

  tee.flush();
  transcript.close();

  const RunSummary& summary = interpreter.lastRunSummary();

 // 9. Always write results.dat (also for geometry errors).
  std::ofstream results(resultsName, std::ios::binary | std::ios::trunc);
  if (!results) {
    errorOutput << "Batch output file not writable: " << resultsName << "\n";
    return batchExitRunnerFileError;
  }
  writeResultsDat(results, summary);
  results.close();

 // 10. Exit code: canonical interpreter aborts (1, 2)
  // override everything; otherwise first-error-wins from the RunSummary.
  if (interpreterResult != batchExitSuccess) {
    return interpreterResult;
  }

  std::optional<std::pair<std::size_t, int>> firstFatal;
  const auto consider = [&](FatalKind kind, int code) {
    const auto& index =
        summary.firstOccurrenceByKind[static_cast<std::size_t>(kind)];
    if (index.has_value() &&
        (!firstFatal.has_value() || index.value() < firstFatal->first)) {
      firstFatal = std::make_pair(index.value(), code);
    }
  };
  consider(FatalKind::unknownCommand, batchExitCommandOrSemanticsError);
  consider(FatalKind::solverError, batchExitSolverError);
  if (parsed.job.settings.stopOnNonConvergence) {
    consider(FatalKind::nonConverged, batchExitNonConvergence);
  }
  if (!parsed.job.settings.allowUnavailablePlotCommands) {
    consider(FatalKind::unavailablePlot, batchExitCommandOrSemanticsError);
  }
  if (firstFatal.has_value()) {
    return firstFatal->second;
  }
  if (!summary.geometryEstablished) {
    errorOutput << "No airfoil geometry established.\n";
    return batchExitCommandOrSemanticsError;
  }
  return batchExitSuccess;
}

}  // namespace xfoil::app::cli

// End: src/app/cli/BatchRunner.cpp

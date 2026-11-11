// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

// File: src/app/cli/CommandInterpreter.h
// Core of the faithful XFOIL-6.99 command interpreter. The interpreter reads command lines from an
// injected input stream, writes output and errors to injected
// streams and never uses std::cin, std::cout or
// std::cerr in this class. This makes deterministic transcript tests
// possible without starting a process.
//
// The interpreter is a pure
// router: it manages only input, menu and prompt state and calls the
// session/core handlers (CoreGeometryHandlers,
// CoreOperHandlers, CoreVparHandlers) and the confirmed
// session use-cases. All domain states
// (geometry buffer, panel contour and parameters, operating state, VPAR,
// analysis, sequences, results, VACCEL) live exclusively in the
// session composition. No dual write, no numeric formula in the
// interpreter.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/AirfoilWriter.h"
#include "app/DatFileReader.h"
#include "app/cli/CommandParser.h"
#include "app/cli/CoreOperHandlers.h"
#include "xfoil/Types.h"
#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli {

struct InteractiveStartupOptions {
  std::optional<std::string> inputPath;
  // Batch mode skips the
  // interactive startup GETDEF stage (no prompt, no implicit
 // xfoil.def). The existing applyStartupDefaults path is only
  // skipped; interactive behavior remains unchanged.
  bool loadStartupDefaults{true};
};

// Menu state of the interpreter. top, oper, vpar and ppar are active; the
// other states correspond to the menu hierarchy of the Fortran reference.
enum class MenuStatus {
  top,
  oper,
  vpar,
  vplo,
  gdes,
  camb,
  cplo,
  mdes,
  qdes,
  ppar,
  plop
};

// Control flow after executing a menu command.
enum class CommandFlow {
  stayInMenu,    // command processed, next prompt of the menu
  leaveToParent, // empty input: back to the parent menu
  endSession     // Q/QUIT or EOF: interpreter-controlled end
};

// Fatal event classes for the
// exit-code derivation of the batch runner (first-error-wins).
enum class FatalKind {
  none,
  unknownCommand,
  unavailablePlot,
  nonConverged,
  solverError
};

// Monitored settings snapshot for the settings comment blocks of
// results.dat (R9). Sources: OperatingConditionState
// and viscous session state.
struct RunSettingsSnapshot {
  double re{};
  double mach{};
  int machType{};
  int reType{};
  bool viscous{};
  int maximumIterations{};
  double ncritTop{};
  double ncritBottom{};
  double xtripTop{};
  double xtripBottom{};
  double viscousAcceleration{};
  double wakeLength{};
  int idamp{};
  xfoil::core::boundary_layer::BoundaryLayerParameters blpar;
};

inline bool operator==(const RunSettingsSnapshot& left,
                       const RunSettingsSnapshot& right) {
  return left.re == right.re && left.mach == right.mach &&
         left.machType == right.machType && left.reType == right.reType &&
         left.viscous == right.viscous &&
         left.maximumIterations == right.maximumIterations &&
         left.ncritTop == right.ncritTop &&
         left.ncritBottom == right.ncritBottom &&
         left.xtripTop == right.xtripTop &&
         left.xtripBottom == right.xtripBottom &&
         left.viscousAcceleration == right.viscousAcceleration &&
         left.wakeLength == right.wakeLength && left.idamp == right.idamp &&
         left.blpar.sccon == right.blpar.sccon &&
         left.blpar.gacon == right.blpar.gacon &&
         left.blpar.gbcon == right.blpar.gbcon &&
         left.blpar.gccon == right.blpar.gccon &&
         left.blpar.dlcon == right.blpar.dlcon &&
         left.blpar.ctrcon == right.blpar.ctrcon &&
         left.blpar.ctrcex == right.blpar.ctrcex &&
         left.blpar.duxcon == right.blpar.duxcon &&
         left.blpar.ctcon == right.blpar.ctcon &&
         left.blpar.cffac == right.blpar.cffac;
}

// Geometry comment block event: source is
// the --airfoils path or the establishing command. pointIndex is the count
// of points already completed at the time of establishment.
struct RunGeometryRecord {
  std::string name;
  std::string source;
  std::size_t points{};
  std::size_t pointIndex{};
};

// Structured run summary. The runner
// evaluates exclusively these fields; re-parsing stdout is
// forbidden.
struct RunSummary {
  bool completed{};
  bool quitRequested{};
  bool eofReached{};
  std::size_t commandsExecuted{};
  std::size_t unknownCommandCount{};
  std::size_t unavailablePlotCount{};
  std::size_t nonConvergedPoints{};
  std::size_t solverErrors{};
  FatalKind firstFatalKind{FatalKind::none};
  std::size_t firstFatalCommandIndex{};
  // Index of the first occurrence per class (1-based on commandsExecuted);
  // nullopt = class not encountered. The runner selects the classes
  // that are fatal per the job policy and forms the exit code.
  std::array<std::optional<std::size_t>, 5> firstOccurrenceByKind{};
  bool geometryEstablished{};
  std::vector<OperPointResult> pointResults;
 // Settings snapshot switch: (point index 0-based, from which the snapshot
  // applies). The first entry applies from point 0.
  std::vector<std::pair<std::size_t, RunSettingsSnapshot>> settingsChanges;
  std::vector<RunGeometryRecord> geometryRecords;
};

class CommandInterpreter {
 public:
  CommandInterpreter(
      std::istream& input,
      std::ostream& output,
      std::ostream& errorOutput);

  ~CommandInterpreter();

  CommandInterpreter(const CommandInterpreter&) = delete;
  CommandInterpreter& operator=(const CommandInterpreter&) = delete;

  // Start: banner, load profile file if applicable,
  // main menu, then prompt loop. Exit code 0 for Q, QUIT or EOF,
  // exit code 1 for a faulty start profile file.
  [[nodiscard]]
  int run(const InteractiveStartupOptions& options);

  // Error policy of the batch jobs. The runner sets
 // it before run; the default never stops (interactive behavior
  // stays canonical: message and continuation).
  struct RunPolicy {
    bool stopOnError{false};
    bool stopOnNonConvergence{false};
    bool allowUnavailablePlotCommands{true};
  };
  void setRunPolicy(const RunPolicy& policy) {
    runPolicy_ = policy;
  }

 // The BatchRunner sets the OutputPathPolicy from
  // --prefix; interactive uses the default (empty = identity).
  void setOutputPathPolicy(const xfoil::io::OutputPathPolicy& policy) {
    session_.outputPaths = policy;
  }

 // Structured summary of the last run
  // (counter based, no text derivation). Valid until the next
 // run call.
  [[nodiscard]]
  const RunSummary& lastRunSummary() const noexcept {
    return summary_;
  }

  // Canonical interpreter abort code (CPWR/DUMP/DMP OPEN error,
  // WDEF/RDEF guard paths): existing exit code 2 contract.
  [[nodiscard]]
  int fatalExitCode() const noexcept {
    return fatalExitCode_;
  }

 private:
  void printBanner();
  void printMainMenu();
  void showPrompt();

  // Loads the start profile file via the C++ DAT reading path
  // and takes it over into the session (buffer + ABCOPY panel contour). A
  // false return ends the start with exit code 1, without prompt and
  // without a partially initialized session.
  bool loadStartupProfile(const std::string& path);

  // Startup GETDEF (xfoil.f 50, CALL GETDEF(LU,'xfoil.def',
  //.TRUE.)). Runs after the banner and before a startup profile is
  // loaded. Missing file: ' File xfoil.def not found'.
  // Present file: LASK prompt; an answer of N/n declines, otherwise the
  // file is read and, on success, echoed back verbatim (WRTDEF(6)).
  void applyStartupDefaults();

  // Reads a raw input line without prompt (for answers whose prompt
  // the handler provides). EOF sets sessionEnded_.
  bool readAnswerLine(std::string& line);

  // --- TOP commands -------------------------------------------------------
  void handleLoad(const std::string& argument);
  void handleNorm();
  void handleSave(AirfoilOutputFormat format, const std::string& argument);
  void handleMsave(const std::string& argument);
  void handleBend();
  void handleWdef(const std::string& argument);
  void handleRdef(const std::string& argument);
  void handleReve();
  void handleDelimiter(const std::string& argument);
  void handleNaca(const std::string& argument, bool sharpTrailingEdge = false);
  void handleXycm(const std::string& argument);
  void handlePcop();
  void handleInte();
  void handleName(const std::string& argument);
  void handleNinc();

  // --- OPER polar commands with prompt chains (xoper.f 978-1178) ----
  void handleOperPdel(const ParsedCommand& parsed);
  void handleOperPsor(const ParsedCommand& parsed);
  void handleOperRdel(const ParsedCommand& parsed);
  void handleOperPget(const ParsedCommand& parsed);
  void handleOperPwrt(const ParsedCommand& parsed);
  void handleOperRget(const ParsedCommand& parsed);
  void handleOperPrem(const ParsedCommand& parsed);
  void handleOperPnam(const ParsedCommand& parsed);
  void handleOperPacc(const ParsedCommand& parsed);
  // OPER file exports CPWR (CPDUMP), DUMP (BLDUMP), DMP
  // (BLDUMP2), BLOU (BLDUMP3, hidden 6.996 command). Prompt chain after
  // the SETFILE prefix requirement, open overwrite (STATUS='UNKNOWN'),
  // no success message.
  void handleOperCpwr(const ParsedCommand& parsed);
  void handleOperDump(const ParsedCommand& parsed);
  void handleOperDmp(const ParsedCommand& parsed);
  void handleOperBlou(const ParsedCommand& parsed);
  bool askYesNo(const std::string& promptText, bool& value);
  // PLRINI/PLXINI (xpol.f 180-556) as interpreter steps; returns
  // whether the respective file is ready for appending.
  void initPolarSaveFile(int slot0);
  void initPolarDumpFile(int slot0);

  CommandFlow executeTopCommand(const ParsedCommand& parsed);

  // --- Paneling and PPAR menu ------------------------------------------
  void handlePane();
  void runPparMenu();

  // --- OPER menu -----------------------------------------------------------
  void runOperMenu();
  CommandFlow executeOperCommand(const ParsedCommand& parsed);

  void printOperHelp();
  void handleNameOper(const std::string& argument);
  void handleNincOper();
  void handleNdecOper();

  // --- VPAR submenu ------------------------------------------------------
  void runVparMenu();
  CommandFlow executeVparCommand(const ParsedCommand& parsed);
  void printVparHelp();

  // --- I/O helper -----------------------------------------------------------
  void showMenuPrompt(std::string_view promptText);
  bool readInputLine(std::string& line);
  bool askReal(const std::string& promptText, double& value);
  bool askInteger(const std::string& promptText, int& value);
  // ASKS from userio.f: prompt + ' s> ', simple line input without
  // repetition; empty input returns an empty string. EOF ends
  // the session in a controlled way (return value false).
  bool askString(const std::string& promptText, std::string& value);
  // File export prompt help (CPDUMP/BLDUMP/BLDUMP2 shared
  // prompt sequence: inline argument, PREFIX default suggestion, otherwise ASKS).
  bool askExportFilename(const std::string& argument,
                         const std::string& extension, std::string& fileName);
  bool writeExportFile(const std::string& fileName, const std::string& text);

  // --- RunSummary recording ----------------------
  void resetRunSummary();
  void noteCommandExecuted();
  void noteUnknownCommand();
  void noteUnavailablePlot();
  void noteSolverError();
  void notePointResult(const OperPointResult& result);
  void finalizePointTracking();
  void noteOperPointStatus(bool analyzed, bool nonConverged,
                           bool missingGeometry);
  void noteSequenceOutcome(bool missingGeometry, bool pointFailure,
                           std::size_t pointsBefore);
  void noteFatalCandidate(FatalKind kind);
  RunSettingsSnapshot currentSettingsSnapshot() const;
  std::uint64_t geometryRevisionKey() const;
  std::size_t geometryPointCount() const;
  void refreshGeometryTracking(const std::string& source);

  std::istream& input_;
  std::ostream& output_;
  std::ostream& errorOutput_;

  MenuStatus menuStatus_{MenuStatus::top};
  bool quitRequested_{};
  bool sessionEnded_{};
  // Canonical runtime abort for an OPEN error of a
  // STATUS='UNKNOWN' file export (CPWR/DUMP/DMP, no ERR branch in
  // Fortran): exit code 2, the session ends.
  int fatalExitCode_{};

  // Last non-empty command; repeat control "!" in the OPER menu.
  std::optional<ParsedCommand> lastCommand_;

  // COMOLD/ARGOLD analogue: only Alfa, CLI, Cl, ASEQ and CSEQ are recorded
  // (set points at the end of the respective original branches in xoper.f).
  // Reset for each OPER entry (original: COMOLD='****').
  std::optional<ParsedCommand> operRepeatCommand_;
 //VELS redo (xoper.f 1662-1663). '!' repeats for
  // COMOLD='VELS' the VELS branch with ARGOLD (raw argument) instead of the
  // point continuation. Reset for every OPER entry.
  bool operVelsRedoActive_{false};

  // Domain overall state: geometry buffer, panel contour and parameters,
  // operating state, VPAR, analysis and results. Sole state source
  // of the interpreter; no second parameter store.
  xfoil::session::SessionComposition session_;

 // Counter and result events of the last run;
 // no change to texts, prompts or fatalExitCode_.
  RunSummary summary_;
  std::uint64_t observedGeometryRevisionKey_{0};
  RunPolicy runPolicy_;
};

}  // namespace xfoil::app::cli

// End: src/app/cli/CommandInterpreter.h

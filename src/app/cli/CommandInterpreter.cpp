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

// File: src/app/cli/CommandInterpreter.cpp
// Implementation of the interpreter core. Banner, main menu and prompt follow
// fortran-src/xfoil.f (formats 1005 and 1100), the OPER and
// VPAR menus follow fortran-src/xoper.f (SUBROUTINE OPER, SUBROUTINE VPAR,
// MRSHOW, NAMMOD) and ASKC/ASKR/ASKI in fortran-src/userio.f.
//
// This interpreter is a pure
// router: it reads input, manages menu and prompt state and calls
// the session/core handlers and the
// confirmed session use-cases. All domain states
// live in the session composition. No dual write, no
// numeric formula and no own solver iteration in the interpreter.

#include "app/cli/CommandInterpreter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/ProductInfo.h"
#include "app/cli/CliFortranText.h"
#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CoreInverseDesignHandlers.h"
#include "app/cli/CoreOperHandlers.h"
#include "app/cli/CoreVparHandlers.h"
#include "app/cli/NumericArguments.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/io/DumpCodec.h"
#include "xfoil/io/ExportFormats.h"
#include "xfoil/io/PolarText.h"
#include "xfoil/io/OutputPathPolicy.h"
#include "xfoil/session/SessionOps.h"

namespace xfoil::app::cli {
namespace {

// Known TOP plot commands: recognized, but not available in this build
// (compatibility contract 1.6, risk register R50).
constexpr std::string_view kPlotCommands[] = {"Z", "U", "BENP"};

// MDES and QDES are dispatched to their menu handlers (see the
// GDES block below), as are INTX and HALF; every TOP command is
// handled.

// OPER plot commands from the canonical list: recognized, reported as
// plot-unavailable only, and they change no state.
// The canonical ASKC aliases (xoper.f 1181-1182, 1224-1225,
// 1472-1473, 1477-1478, 1720-1721) execute the same
// plot-unavailable branch as the long forms.
constexpr std::string_view kOperPlotCommands[] = {
    "Z",    "U",    "HARD", "SIZE", "CPMI", "UEMA", "UEMI", "SEQP",
    "PPLO", "APLO", "PPAX", "CREF", "CPX",  "UEX",  "GRID", "CPIO",
    "CPV",  "CPVI", "BL",   "BLC",  "BLWT", "ANNO", "VPLO", "FREF",
    "PP",   "AP",   "CP",   "UE",   "VP"};

// OPER commands from the canonical list: recognized, reported as
// plot-unavailable, and they change no state.
// CPMN is handled by its own handler.
// CINC/HINC/PSUM/PLIS/PDEL/PSOR/RDEL are enabled; FREF is
// canonically a plot toggle (xoper.f 1463-1469) and belongs to
// the plot list. ASET, BLF and DAMP are handled by their
// own handlers.

// NT/NB, WAKE/W, LAG/GB/CTR/CFAC and REST run through
// CoreVparHandlers -> SessionOps -> session
// composition. Every canonical non-graphical VPAR command is
// reachable; unknown commands run on format 1000.

// FMOM, FNEW and VELS run through
// CoreOperHandlers -> SessionOps -> core MHINGE/PSILIN.

// Plot commands of the PPAR menus: recognized, but without a plot backend
// not available (contract 1.6); no state change, no table output
// (GETPAN: GO TO 12).
constexpr std::string_view kPparPlotCommands[] = {"Z", "U", "H"};

// Prompt from GETPAN: ASKC('Change what ? (<cr> if nothing else)^').
constexpr std::string_view kPparPrompt =
    "Change what ? (<cr> if nothing else)";

bool isOneOf(std::string_view command,
             std::span<const std::string_view> candidates) {
  for (const std::string_view candidate : candidates) {
    if (command == candidate) {
      return true;
    }
  }
  return false;
}

// Fortran A4 output of a CHARACTER*4 command.
[[nodiscard]]
std::string formatFortranA4(std::string_view command) {
  std::string text{command};
  if (text.size() < 4) {
    text.append(4 - text.size(), ' ');
  }
  return text;
}

std::string trimSpaces(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() && text[begin] == ' ') {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && text[end - 1] == ' ') {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

}  // namespace

CommandInterpreter::CommandInterpreter(
    std::istream& input,
    std::ostream& output,
    std::ostream& errorOutput)
    : input_(input), output_(output), errorOutput_(errorOutput) {
 // Interactive original defaults (xfoil.f INIT)
  // exclusively via the confirmed session operation configure:
  // LVISC=false, ITMAX=20, LBLINI=true, VACCEL=0.01, ACRIT=9, XTRIP=1/1,
  // MATYP=RETYP=1, MINF1=REINF1=0. They deliberately differ from
  // batch mode; the batch path remains unchanged. VACCEL is instance-specific
 // (no static field). configure additionally sets the confirmed
  // operating-state flag 'configured'; without this flag every
  // OPER analysis point would be rejected in a controlled way.
  AnalysisConfig interactiveDefaults;
  interactiveDefaults.reynoldsNumber = 0.0;  // REINF1 = 0 (xfoil.f INIT)
  interactiveDefaults.machNumber = 0.0;      // MINF1 = 0 (xfoil.f INIT)
  interactiveDefaults.viscous = false;       // LVISC = false
  interactiveDefaults.reynoldsDependency =
      ReynoldsDependency::constant;          // RETYP = 1
  interactiveDefaults.machDependency =
      MachDependency::constant;              // MATYP = 1
  interactiveDefaults.maximumIterations = 20;  // ITMAX = 20
  interactiveDefaults.autoInitializeBoundaryLayer = true;  // LBLINI = true
  interactiveDefaults.viscousAcceleration = 0.01;          // VACCEL = 0.01
  // TransitionConfig defaults: ACRIT=9, XSTRIP(1/2)=1.
  const auto configured = session::configure(session_, interactiveDefaults);
  static_cast<void>(configured);  // Defaults are valid per construction
}

CommandInterpreter::~CommandInterpreter() = default;

void CommandInterpreter::printBanner() {
  // XFOIL.next product banner. The layout follows
  // Fortran format 1005 from xfoil.f (51 '=' characters, leading space,
  // blank lines); product name and product version derive exclusively from
  // the central source app/ProductInfo.h. The line
  // `Based upon XFOIL Version 6.99` is the original-XFOIL attribution and
  // not a product version.
  output_ << "\n"
             " ===================================================\n"
             " "
          << product::name << " Version " << product::version
          << "\n"
             " Copyright (C) 2026 Olaf Frommann\n"
             "\n"
             " Based upon XFOIL Version 6.99\n"
             " Copyright (C) 2000   Mark Drela, Harold Youngren\n"
             "\n"
             " This software comes with ABSOLUTELY NO WARRANTY,\n"
             "   subject to the GNU General Public License.\n"
             "\n"
             " Caveat computor\n"
             " ===================================================\n";
}

void CommandInterpreter::printMainMenu() {
  // Format 1100 from xfoil.f with the dynamic fields from the session
  // (XCMREF/YCMREF, NPAN). Without set panel parameters the
  // INIT defaults 0.25 / 0.0 / 240 apply.
  const double cmReferenceX = session_.operating.momentReferenceX;
  const double cmReferenceY = session_.operating.momentReferenceY;
  const int panelNodeCount = session_.paneling.parameters.panelCount;

  output_ << "\n"
             "   QUIT    Exit program\n"
             "\n"
             "  .OPER    Direct operating point(s)\n"
             "  .MDES    Complex mapping design routine\n"
             "  .QDES    Surface speed design routine\n"
             "  .GDES    Geometry design routine\n"
             "\n"
             "   SAVE f  Write airfoil to labeled coordinate file\n"
             "   PSAV f  Write airfoil to plain coordinate file\n"
// " USAV f Write airfoil to unformatted coordinate file\n"
             "   ISAV f  Write airfoil to ISES coordinate file\n"
             "   MSAV f  Write airfoil to MSES coordinate file\n"
             "   REVE    Reverse written-airfoil node ordering\n"
             "   DELI i  Change written-airfoil file delimiters\n"
             "\n"
             "   LOAD f  Read buffer airfoil from coordinate file\n"
             "   NACA i  Set NACA 4,5-digit airfoil and buffer airfoil\n"
             "   INTE    Set buffer airfoil by interpolating two airfoils\n"
// " INTX Set buffer airfoil by interpolation/extrapolation\n"
             "   NORM    Buffer airfoil normalization toggle\n"
             "   HALF    Halve the number of points in buffer airfoil\n";
  output_ << "   XYCM rr Change CM reference location, currently "
          << formatFortranF(8, 5, cmReferenceX)
          << formatFortranF(8, 5, cmReferenceY) << "\n";
  output_ << "\n"
             "   BEND    Display structural properties of current airfoil\n"
// " BENP Plot structural properties of current airfoil\n"
// " (plotting unavailable)\n"
             "\n"
             "   PCOP    Set current-airfoil panel nodes directly"
             " from buffer airfoil points\n";
  output_ << "   PANE    Set current-airfoil panel nodes ("
          << std::setw(4) << panelNodeCount << " )"
          " based on curvature\n";
  output_
      << "  .PPAR    Show/change paneling\n"
         "\n"
         "  .PLOP    Plotting options\n"
         "\n"
         "   WDEF f  Write  current-settings file\n"
         "   RDEF f  Reread current-settings file\n"
         "   NAME s  Specify new airfoil name\n"
         "   NINC    Increment name version number\n"
         "\n"
         "   Z       Zoom    | (available in all menus)\n"
         "   U       Unzoom  |\n";
}

void CommandInterpreter::showPrompt() {
  // ASKC, format 1000: FORMAT(/A,' c> ',$) with prompt text ' XFOIL'.
  output_ << "\n"
          << " XFOIL"
          << "   c>  ";
  output_.flush();
}

void CommandInterpreter::showMenuPrompt(std::string_view promptText) {
  // ASKC, format 1000. OPER uses '.OPERi'/'.OPERv' depending on LVISC,
  // VPAR uses '..VPAR' (xoper.f lines 70-82, 2531).
  output_ << "\n"
          << promptText
          << "   c>  ";
  output_.flush();
}

bool CommandInterpreter::loadStartupProfile(const std::string& path) {
  const auto readResult = xfoil::app::DatFileReader::read(path);

  if (!readResult.success()) {
    errorOutput_
        << "Error loading profile file \""
        << path
        << "\": "
        << readResult.errorMessage
        << "\n";
    return false;
  }

  const std::size_t pointCount = readResult.airfoil.coordinates.size();
  if (pointCount < 3) {
    errorOutput_
        << "Error loading profile file \""
        << path
        << "\": Airfoil geometry preparation failed\n";
    return false;
  }

  std::vector<double> x;
  std::vector<double> y;
  x.reserve(pointCount);
  y.reserve(pointCount);
  for (const auto& point : readResult.airfoil.coordinates) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      errorOutput_
          << "Error loading profile file \""
          << path
          << "\": Airfoil geometry preparation failed\n";
      return false;
    }
    x.push_back(point.x);
    y.push_back(point.y);
  }

  // Startup takeover exclusively via the session use-cases:
  // buffer adoption and ABCOPY panel contour.
  // The visible startup output stays in the frozen startup
  // format.
  core::geometry::AirfoilBuffer buffer(
      std::move(x), std::move(y), readResult.airfoil.name);
  const auto buffered = session::setBufferGeometry(session_, buffer);
  if (!buffered.success()) {
    errorOutput_
        << "Error loading profile file \""
        << path
        << "\": Airfoil geometry preparation failed\n";
    return false;
  }
  const auto copied = session::copyBufferToCurrent(session_);
  if (!copied.ok) {
    errorOutput_
        << "Error loading profile file \""
        << path
        << "\": Airfoil geometry preparation failed\n";
    return false;
  }
  session::setGeometryName(session_, readResult.airfoil.name);

  // PREFIX/NPREFIX as at the startup LOAD (xfoil.f 58-60 -> SETFILE).
  {
    std::string prefix = std::filesystem::path(path).filename().string();
    const std::string::size_type dot = prefix.find('.');
    if (dot != std::string::npos) {
      prefix = prefix.substr(0, dot);
    }
    session_.geometry.filePrefix = prefix;
  }

  output_
      << "\nLoading profile file: "
      << path
      << "\n";

  if (!readResult.airfoil.name.empty()) {
    output_
        << "Foil name : "
        << readResult.airfoil.name
        << "\n";
  }

  output_
      << "Number of points : "
      << pointCount
      << "\n";

  return true;
}

CommandFlow CommandInterpreter::executeTopCommand(const ParsedCommand& parsed) {
  const std::string_view command = parsed.command;

  if (command == "?") {
    printMainMenu();
    return CommandFlow::stayInMenu;
  }

  if (command == "QUIT" || command == "Q") {
    quitRequested_ = true;
    return CommandFlow::endSession;
  }

  if (isOneOf(command, kPlotCommands)) {
    noteUnavailablePlot();
    output_ << "Plotting is not available in this build.\n";
    return CommandFlow::stayInMenu;
  }

  // OPER opens the operating menu. Without a valid panel contour TOP stays
  // active (OPER checks N.EQ.0 as in xoper.f lines 51-55).
  if (command == "OPER") {
    if (!session::panelingValid(session_)) {
      output_ << "***  No airfoil available  ***\n";
      return CommandFlow::stayInMenu;
    }
    // Format 5000 (xoper.f 57-58): active polar slot is reported at
    // OPER entry.
    if (session_.polar.activeSlot >= 0) {
      emitPolarActiveAtOperEntry(output_, session_.polar.activeSlot + 1);
    }
    runOperMenu();
    return sessionEnded_ ? CommandFlow::endSession : CommandFlow::stayInMenu;
  }

  // PLOP (xfoil.f 302-306, plutil.f OPLSET): mixed text/flag
  // command, not a plot command. The handler runs
  // the menu loop; the interpreter only handles the entry.
  if (command == "PLOP") {
    const auto status = handlePlotOptionsCore(session_, input_, output_);
    if (status == PlotOptionsCoreStatus::endOfFile) {
      sessionEnded_ = true;
      return CommandFlow::endSession;
    }
    return CommandFlow::stayInMenu;
  }

  // GDES (xfoil.f TOP dispatch, xgdes.f 21-949): the
  // handler runs the complete menu loop including the CAMB
  // submenu, EXEC, OVER and plot/cursor-unavailable paths. The
  // interpreter contains no GDES numerics, no buffer copy and
  // no redo logic.
  if (command == "GDES") {
    const auto status = handleGeometryDesignCore(session_, input_, output_);
    if (status == GeometryDesignCoreStatus::endOfFile) {
      sessionEnded_ = true;
      return CommandFlow::endSession;
    }
    return CommandFlow::stayInMenu;
  }

  // MDES/QDES menu handlers (xmdes.f 30-720, xqdes.f
  // 40-540). Both handlers carry the complete menu loop
  // including initialization, redo, file and commit paths on the
  // shared InverseSessionState. The interpreter contains no
  // inverse numerics, no QSPEC/CN manipulation, no READ/DUMP
  // formatting, no buffer/panel copy and no redo logic.
  if (command == "MDES") {
    const auto status = handleMdesCore(session_, input_, output_);
    if (status == MdesCoreStatus::endOfFile) {
      sessionEnded_ = true;
      return CommandFlow::endSession;
    }
    return CommandFlow::stayInMenu;
  }
  if (command == "QDES") {
    const auto status = handleQdesCore(session_, input_, output_);
    if (status == QdesCoreStatus::endOfFile) {
      sessionEnded_ = true;
      return CommandFlow::endSession;
    }
    return CommandFlow::stayInMenu;
  }

  if (command == "LOAD") {
    handleLoad(parsed.argument);
  } else if (command == "SAVE") {
    handleSave(AirfoilOutputFormat::labeled, parsed.argument);
  } else if (command == "PSAV") {
    handleSave(AirfoilOutputFormat::plain, parsed.argument);
  } else if (command == "USAV") {
    handleSave(AirfoilOutputFormat::integerCoordinates, parsed.argument);
  } else if (command == "ISAV") {
    handleSave(AirfoilOutputFormat::ises, parsed.argument);
  } else if (command == "MSAV") {
    handleMsave(parsed.argument);
  } else if (command == "BEND") {
    handleBend();
  } else if (command == "WDEF") {
    handleWdef(parsed.argument);
  } else if (command == "RDEF") {
    handleRdef(parsed.argument);
  } else if (command == "REVE") {
    handleReve();
  } else if (command == "DELI") {
    handleDelimiter(parsed.argument);
  } else if (command == "NACA") {
    handleNaca(parsed.argument);
  } else if (command == "INTE") {
    handleInte();
  } else if (command == "NORM") {
    handleNorm();
  } else if (command == "XYCM") {
    handleXycm(parsed.argument);
  } else if (command == "PCOP") {
    handlePcop();
  } else if (command == "NAME") {
    handleName(parsed.argument);
  } else if (command == "NINC") {
    handleNinc();
  } else if (command == "PANE") {
    handlePane();
  } else if (command == "PPAR") {
    runPparMenu();
  } else if (command == "INTX") {
    // INTX (xfoil.f 229-230, 2461-2593).
    handleIntxCore(session_, input_, output_);
  } else if (command == "HALF") {
    // HALF (xfoil.f 242-252); prints the GEOPAR text.
    handleHalfCore(session_, output_);
  } else {
    // FCPP015: Fortran FORMAT 1050 (1X,A4,' command not recognized...').
    noteUnknownCommand();
    output_ << " " << formatFortranA4(command)
            << " command not recognized.  Type a \"?\" for list\n";
    return CommandFlow::stayInMenu;
  }

  return sessionEnded_ ? CommandFlow::endSession : CommandFlow::stayInMenu;
}

int CommandInterpreter::run(const InteractiveStartupOptions& options) {
  resetRunSummary();
  printBanner();

  // Canonical order xfoil.f 40-50: banner, CALL INIT,
  // CALL GETDEF(LU,'xfoil.def',.TRUE.), only afterwards, if applicable, LOAD of the
 // start profile. Batch mode skips the
  // startup GETDEF stage in a controlled way (loadStartupDefaults = false); the
  // interactive path stays byte-identical unchanged.
  if (options.loadStartupDefaults) {
    applyStartupDefaults();
    if (sessionEnded_) {
      summary_.quitRequested = quitRequested_;
      summary_.eofReached = sessionEnded_ && !quitRequested_;
      summary_.geometryEstablished =
          !session_.geometry.buffer.x.empty() ||
          session::panelingValid(session_);
      return fatalExitCode_;
    }
  }

  if (options.inputPath.has_value() &&
      !loadStartupProfile(*options.inputPath)) {
    summary_.quitRequested = quitRequested_;
    summary_.eofReached = sessionEnded_ && !quitRequested_;
    summary_.geometryEstablished =
        !session_.geometry.buffer.x.empty() ||
        session::panelingValid(session_);
    return 1;
  }

 // First geometry comment block for job start;
  // source is the --airfoils start profile or empty geometry.
  refreshGeometryTracking(options.inputPath.has_value()
                              ? "profile:" + *options.inputPath
                              : std::string{});
  // First settings snapshot applies from point 0 (block before the first
 // data line, R9).
  summary_.settingsChanges.emplace_back(0, currentSettingsSnapshot());

  printMainMenu();

  std::string line;
  bool topLoopEof = false;
  while (!quitRequested_ && !sessionEnded_) {
    showPrompt();

    if (!readInputLine(line)) {
      // EOF: controlled exit without message (compatibility
      // contract 1.5).
      topLoopEof = true;
      break;
    }

    const ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      continue;
    }

    lastCommand_ = parsed;
    noteCommandExecuted();

    const std::string command{parsed.command};
    if (executeTopCommand(parsed) == CommandFlow::endSession) {
      break;
    }
    refreshGeometryTracking("command:" + command);
  }

  summary_.completed = true;
  summary_.quitRequested = quitRequested_;
  summary_.eofReached = (sessionEnded_ || topLoopEof) && !quitRequested_;
  summary_.geometryEstablished =
      !session_.geometry.buffer.x.empty() || session::panelingValid(session_);
  return fatalExitCode_;
}

// ---
// TOP file and profile commands: handled by session/core handlers.
// ---

void CommandInterpreter::handleLoad(const std::string& argument) {
  std::string path = trimSpaces(argument);
  if (path.empty()) {
    std::string entered;
    if (!askString("Enter filename", entered)) {
      sessionEnded_ = true;
      return;
    }
    path = trimSpaces(entered);
  }

  // Silent pre-reading only to control the interactive follow-up questions
  // (element selection for MSES multi-element files, name query for
  // plain files). The visible AREAD/LOAD output happens
  // exclusively through the handler.
  int selectedElement = 0;
  CoordinateFileReadResult probe =
      xfoil::app::DatFileReader::readCoordinateFile(path, selectedElement);
  while (probe.elementSelectionRequired) {
    int element = 0;
    if (!askInteger("Enter element number", element)) {
      sessionEnded_ = true;
      return;
    }
    selectedElement = element;
    probe = xfoil::app::DatFileReader::readCoordinateFile(path,
                                                          selectedElement);
  }

  std::string requestedName;
  if (probe.success() &&
      probe.format == CoordinateFileFormat::generic) {
    // The header line and the ASKS prompt derive from the handler; the
    // Answer is here only silently collected.
    if (!readAnswerLine(requestedName)) {
      return;
    }
  }

  handleLoadCore(session_, path, requestedName, output_, selectedElement);
}

void CommandInterpreter::handleNorm() {
  handleNormCore(session_, output_);
}

void CommandInterpreter::handleSave(AirfoilOutputFormat format,
                                    const std::string& argument) {
  std::string filenameAnswer;
  std::string overwriteAnswer;
  bool hasFilenameAnswer = false;
  bool hasOverwriteAnswer = false;

  for (;;) {
    int status = 0;
    switch (format) {
      case AirfoilOutputFormat::labeled:
        status = static_cast<int>(handleSaveCore(
            session_, output_, argument, !argument.empty(),
            filenameAnswer, hasFilenameAnswer,
            overwriteAnswer, hasOverwriteAnswer));
        break;
      case AirfoilOutputFormat::plain:
        status = static_cast<int>(handlePsavCore(
            session_, output_, argument, !argument.empty(),
            filenameAnswer, hasFilenameAnswer,
            overwriteAnswer, hasOverwriteAnswer));
        break;
      case AirfoilOutputFormat::integerCoordinates:
        status = static_cast<int>(handleUsavCore(
            session_, output_, argument, !argument.empty(),
            filenameAnswer, hasFilenameAnswer,
            overwriteAnswer, hasOverwriteAnswer));
        break;
      case AirfoilOutputFormat::ises:
        status = static_cast<int>(handleIsavCore(
            session_, output_, argument, !argument.empty(),
            filenameAnswer, hasFilenameAnswer,
            overwriteAnswer, hasOverwriteAnswer));
        break;
    }

    // The prompt/overwrite status values are numbered identically in all
    // four handler branches.
    if (status == 1) {  // filename prompt requested
      if (!readAnswerLine(filenameAnswer)) {
        return;
      }
      hasFilenameAnswer = true;
      continue;
    }
    if (status == 2) {  // overwriteConfirmationRequired
      if (!readAnswerLine(overwriteAnswer)) {
        return;
      }
      hasOverwriteAnswer = true;
      continue;
    }
    return;
  }
}

// TOP-level MSAV (xfoil.f 172-173, SUBROUTINE MSAVE xfoil.f 1430-1544).
// Prompt chain: ASKS filename (only if no inline argument), then
// the kernel reports the element count and the interpreter runs the
// ASKI element prompt. Canonical ASKI semantics (userio.f): an empty
// line keeps the Fortran residual value IEL, invalid numbers are
// queried again without a special message (READ ERR=10 -> GO TO 10). A
// STOP branch ('LOAD: Array overflow') ended the session with
// exit code 0 (canonical STOP).
void CommandInterpreter::handleMsave(const std::string& argument) {
  std::string filenameAnswer;
  bool hasFilenameAnswer = false;
  std::string elementAnswer;
  bool hasElementAnswer = false;
  int elementDefault = 1;

  for (;;) {
    const int status = static_cast<int>(handleMsaveCore(
        session_, output_, argument, !argument.empty(),
        filenameAnswer, hasFilenameAnswer, elementAnswer, hasElementAnswer,
        elementDefault));
    if (status == static_cast<int>(MsaveCoreStatus::filenamePromptRequested)) {
      if (!askString("Enter output filename for element replacement",
                     filenameAnswer)) {
        sessionEnded_ = true;
        return;
      }
      hasFilenameAnswer = true;
      continue;
    }
    if (status == static_cast<int>(MsaveCoreStatus::elementPromptRequested)) {
      for (;;) {
        output_ << "\nEnter element to be replaced by current airfoil   i>  ";
        output_.flush();
        std::string line;
        if (!std::getline(input_, line)) {
          // Canonically Fortran raises a READ error loop on EOF;
          // the C++ CLI ends the session (documented deviation).
          sessionEnded_ = true;
          return;
        }
        if (line.find_first_not_of(" \t") == std::string::npos) {
          elementAnswer.clear();
          hasElementAnswer = true;
          break;
        }
        const auto parsed = parseIntegerArguments(line);
        if (parsed.success() && !parsed.values.empty()) {
          elementAnswer = line;
          hasElementAnswer = true;
          break;
        }
      }
      continue;
    }
    if (status == static_cast<int>(MsaveCoreStatus::fatalStop)) {
      fatalExitCode_ = 0;
      sessionEnded_ = true;
    }
    return;
  }
}

// TOP-level BEND (xfoil.f 265-272, SUBROUTINE BENDUMP xgeom.f 1295-1365):
// numerical report via the existing ijsect kernel; no
// prompts, no file output, no session mutation. The plot part
// lies in the separate command BENP and stays recognized-but-not-available.
void CommandInterpreter::handleBend() {
  handleBendCore(session_, output_);
}

// Startup GETDEF (xfoil.f 50, SUBROUTINE GETDEF xfoil.f 867-1030
// with LASK=.TRUE.). The filename is the 9-character literal 'xfoil.def';
// the format-A descriptions (1900/1050/1600/1800) carry exactly this
// length, not the 80-character padding of a CHARACTER*80 as for
// RDEF. Parser/writer remain io::DefaultsCodec; application central via
// session::applyDefaults or raw part assignment in the error case.
void CommandInterpreter::applyStartupDefaults() {
  static constexpr std::string_view kName = "xfoil.def";
  std::ifstream file(std::string(kName), std::ios::binary);
  if (!file) {
    emitDefaultsNotFound(output_, std::string(kName));
    return;
  }
  emitDefaultsReadPrompt(output_, std::string(kName));
  std::string answer;
  if (!readAnswerLine(answer)) {
    // READ(*,1000) to the file for EOF: canonical READ error without ERR branch
    // (runtime abort); the C++ CLI ends the session (documented
    // deviation, analogous to the remaining ASK paths).
    return;
  }
  if (answer.find('N') != std::string::npos ||
      answer.find('n') != std::string::npos) {
    // Index('Nn',ANSWER).NE.0: CLOSE(LU), RETURN without message.
    return;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  file.close();
  const io::DefaultsParseResult parsed = io::parseDefaults(buffer.str());
  if (parsed.complete) {
    session::applyDefaults(session_, parsed.values,
                           parsed.values.lcolor ? 4 : 2, std::string(kName));
    emitDefaultsReadOk(output_, std::string(kName));
    output_ << io::writeDefaults(parsed.values,
                                 parsed.values.lcolor ? 4 : 2);
    return;
  }
  // Error branch 80: already read lines remain effective, IDEVRP and
  // MRCL/COMSET omitted.
  if (parsed.error == io::DefaultsError::kEndOfFile) {
    // Canonical Fortran runtime abort on EOF (EXIT 2); controlled
    // deviation with exit code 2 and session end.
    session::applyDefaultsPartial(session_, parsed.values, parsed.linesRead);
    emitDefaultsEndOfFile(output_);
    fatalExitCode_ = 2;
    sessionEnded_ = true;
    return;
  }
  session::applyDefaultsPartial(session_, parsed.values, parsed.linesRead);
  emitDefaultsReadError(output_, std::string(kName));
}

// TOP-level WDEF (xfoil.f 309-331). The kernel runs the
// overwrite prompt; the interpreter collects the answer
  // (READ(*,1000), format A: the whole line is the answer;
  // INDEX('Nn',answer) aborts on 'N'/'n').
// and calls the kernel again.
void CommandInterpreter::handleWdef(const std::string& argument) {
  std::string answer;
  bool hasAnswer = false;
  for (;;) {
    const WdefCoreStatus status = handleWdefCore(
        session_, output_, argument, !argument.empty(), answer, hasAnswer);
    if (status == WdefCoreStatus::promptRequested) {
      if (!readAnswerLine(answer)) {
        return;
      }
      hasAnswer = true;
      continue;
    }
    if (status == WdefCoreStatus::openError) {
      // Canonical: OPEN STATUS='UNKNOWN' without ERR branch =
      // runtime abort. Controlled deviation: exit code 2,
      // session end.
      fatalExitCode_ = 2;
      sessionEnded_ = true;
    }
    return;
  }
}

// TOP-level RDEF (xfoil.f 333-345). No prompt; the kernel works
// completely (incl. messages and WRTDEF echo).
void CommandInterpreter::handleRdef(const std::string& argument) {
  const RdefCoreStatus status =
      handleRdefCore(session_, output_, argument, !argument.empty());
  if (status == RdefCoreStatus::endOfFile) {
    // Canonical: Fortran runtime abort (exit code 2). Controlled
    // deviation with exit code 2 and session end.
    fatalExitCode_ = 2;
    sessionEnded_ = true;
  }
}

void CommandInterpreter::handleReve() {
  handleReveCore(session_, output_);
}

void CommandInterpreter::handleDelimiter(const std::string& argument) {
  const auto values = parseIntegerArguments(argument);
  const bool hasValue = values.success() && !values.values.empty();
  int value = hasValue ? values.values[0] : -1;
  bool valueGiven = hasValue;
  // Fortran DELI (xfoil.f:185-204): loop for invalid value
  // (NINPUT=0, GO TO 40 -> menu again + ASKI 'Enter new delimiter').
  for (;;) {
    if (handleDeliCore(session_, output_, value, valueGiven) !=
        DeliCoreStatus::menuRequested) {
      return;
    }
    int entered = -1;
    if (!askInteger("Enter new delimiter^", entered)) {
      sessionEnded_ = true;
      return;
    }
    value = entered;
    valueGiven = true;
  }
}

void CommandInterpreter::handleNaca(const std::string& argument) {
  const auto values = parseIntegerArguments(argument);
  const int designation =
      (values.success() && !values.values.empty()) ? values.values[0] : -1;
  handleNacaCore(session_, output_, designation);
}

void CommandInterpreter::handleXycm(const std::string& argument) {
  const auto values = parseRealArguments(argument);
  const bool hasX = values.success() && values.values.size() >= 1;
  const bool hasY = values.success() && values.values.size() >= 2;
  const double x = hasX ? values.values[0] : 0.0;
  const double y = hasY ? values.values[1] : 0.0;
  if (handleXycmCore(session_, output_, x, y, hasX, hasY) ==
      XycmCoreStatus::promptRequested) {
    double left = session_.operating.momentReferenceX;
    double right = session_.operating.momentReferenceY;
    if (!askReal("Enter new CM reference X^", left)) {
      sessionEnded_ = true;
      return;
    }
    if (!askReal("Enter new CM reference Y^", right)) {
      sessionEnded_ = true;
      return;
    }
    handleXycmCore(session_, output_, left, right, true, true);
  }
}

void CommandInterpreter::handlePcop() {
  handlePcopCore(session_, output_);
}

void CommandInterpreter::handleInte() {
  // The handler runs the complete canonical INTE sequence
  // (source selection F/C, filename, fraction, name) with the injected
  // input stream; the interpreter only calls the command.
  handleInteCore(session_, input_, output_);
}

void CommandInterpreter::handleName(const std::string& argument) {
  std::string answer;
  bool hasAnswer = false;
  for (;;) {
    const auto status = handleNameCore(session_, output_, argument,
                                       !argument.empty(), answer, hasAnswer);
    if (status == NameCoreStatus::promptRequested) {
      if (!readAnswerLine(answer)) {
        return;
      }
      hasAnswer = true;
      continue;
    }
    return;
  }
}

void CommandInterpreter::handleNinc() {
  std::string answer;
  bool hasAnswer = false;
  for (;;) {
    const auto status = handleNincCore(session_, output_, answer, hasAnswer);
    if (status == NincCoreStatus::promptRequested) {
      if (!readAnswerLine(answer)) {
        return;
      }
      hasAnswer = true;
      continue;
    }
    return;
  }
}

// ---
// Paneling (PANE, PPAR menu): handled by session/core handlers.
// ---

void CommandInterpreter::handlePane() {
  handlePaneCore(session_, output_);
}

void CommandInterpreter::runPparMenu() {
  // GETPAN (xfoil.f lines 2129-2284).
  if (!session_.geometry.valid) {
    output_ << " GETPAN: Buffer airfoil not available.\n";
    return;
  }

  menuStatus_ = MenuStatus::ppar;

  bool changed = false;
  bool showTable = true;

  while (!sessionEnded_) {
    if (showTable) {
      handlePparShowCore(session_, output_);
      showTable = false;
    }

    showMenuPrompt(kPparPrompt);

    std::string line;
    if (!readInputLine(line)) {
      sessionEnded_ = true;
      break;
    }

    const ParsedCommand parsed = parseCommandLine(line);

    if (parsed.empty) {
      // VAR = ' ': LCHANGE branch (xfoil.f lines 2196-2210).
      if (!changed) {
        break;
      }
      changed = false;
      applyPparPanelingCore(session_, output_);
      showTable = true;  // GO TO 5
      continue;
    }

    const std::string_view command = parsed.command;

    if (isOneOf(command, kPparPlotCommands)) {
      noteUnavailablePlot();
      output_ << "Plotting is not available in this build.\n";
      continue;
    }

    const auto realArguments = parseRealArguments(parsed.argument);
    bool haveInlineValues = false;
    if (realArguments.success()) {
      haveInlineValues = !realArguments.values.empty();
    } else {
      output_ << realArguments.diagnostic << "\n";
    }

    using Kind = core::paneling::PanelParameterKind;
    Kind kind{};
    bool parameterCommand = true;
    std::vector<double> values;

    if (command == "N") {
      kind = Kind::panelCount;
      if (haveInlineValues) {
        const auto intArguments = parseIntegerArguments(parsed.argument);
        values.push_back((intArguments.success() &&
                          !intArguments.values.empty())
                             ? static_cast<double>(intArguments.values[0])
                             : 0.0);
      } else {
        int value = session_.paneling.parameters.panelCount;
        if (!askInteger("Enter number of panel nodes", value)) {
          sessionEnded_ = true;
          break;
        }
        values.push_back(static_cast<double>(value));
      }
    } else if (command == "P") {
      kind = Kind::bunchingParam;
      double value = session_.paneling.parameters.bunchingParam;
      if (haveInlineValues) {
        value = realArguments.values[0];
      } else if (!askReal("Enter panel bunching parameter (0 to ~1)", value)) {
        sessionEnded_ = true;
        break;
      }
      values.push_back(value);
    } else if (command == "T") {
      kind = Kind::teToLeDensityRatio;
      double value = session_.paneling.parameters.teToLeDensityRatio;
      if (haveInlineValues) {
        value = realArguments.values[0];
      } else if (!askReal("Enter TE/LE panel density ratio", value)) {
        sessionEnded_ = true;
        break;
      }
      values.push_back(value);
    } else if (command == "R") {
      kind = Kind::refinementDensityRatio;
      double value = session_.paneling.parameters.refinementDensityRatio;
      if (haveInlineValues) {
        value = realArguments.values[0];
      } else if (!askReal("Enter refined-area panel density ratio", value)) {
        sessionEnded_ = true;
        break;
      }
      values.push_back(value);
    } else if (command == "XT") {
      kind = Kind::upperRefinementRange;
      double left = session_.paneling.parameters.upperRefinementRange[0];
      double right = session_.paneling.parameters.upperRefinementRange[1];
      if (realArguments.success() && realArguments.values.size() >= 2) {
        left = realArguments.values[0];
        right = realArguments.values[1];
      } else {
        if (!askReal("Enter left   top   side refinement limit", left)) {
          sessionEnded_ = true;
          break;
        }
        if (!askReal("Enter right  top   side refinement limit", right)) {
          sessionEnded_ = true;
          break;
        }
      }
      values = {left, right};
    } else if (command == "XB") {
      kind = Kind::lowerRefinementRange;
      double left = session_.paneling.parameters.lowerRefinementRange[0];
      double right = session_.paneling.parameters.lowerRefinementRange[1];
      if (realArguments.success() && realArguments.values.size() >= 2) {
        left = realArguments.values[0];
        right = realArguments.values[1];
      } else {
        if (!askReal("Enter left  bottom side refinement limit", left)) {
          sessionEnded_ = true;
          break;
        }
        if (!askReal("Enter right bottom side refinement limit", right)) {
          sessionEnded_ = true;
          break;
        }
      }
      values = {left, right};
    } else {
      parameterCommand = false;
    }

    if (!parameterCommand) {
      // Unknown input (xfoil.f lines 2274-2279).
      output_ << "\n ***  Input not recognized  ***\n";
      showTable = true;
      continue;
    }

    const auto status = applyPparParameterCore(session_, kind, values,
                                               output_);
    if (status != PparParameterCoreStatus::rejected) {
      changed = true;
    }
    // GO TO 12: next prompt without repeating the tables.
  }

  menuStatus_ = MenuStatus::top;
}

// ---
// OPER menu: handled by session/core handlers.
// ---

void CommandInterpreter::printOperHelp() {
  // Format 1050 from xoper.f.
  output_ << "\n"
             "   <cr>     Return to Top Level\n"
             "   !        Redo last ALFA,CLI,CL,ASEQ,CSEQ,VELS\n"
             "\n"
             "   Visc r   Toggle Inviscid/Viscous mode\n"
             "  .VPAR     Change BL parameter(s)\n"
             "   Re   r   Change Reynolds number\n"
             "   Mach r   Change Mach number\n"
             "   Type i   Change type of Mach,Re variation with CL\n"
             "   ITER     Change viscous-solution iteration limit\n"
             "   INIT     Toggle BL initialization flag\n"
             "\n"
             "   Alfa r   Prescribe alpha\n"
             "   CLI  r   Prescribe inviscid CL\n"
             "   Cl   r   Prescribe CL\n"
             "   ASeq rrr Prescribe a sequence of alphas\n"
             "   CSeq rrr Prescribe a sequence of CLs\n"
             "\n"
             "   SEQP     Toggle polar/Cp(x) sequence plot display\n"
             "   CINC     Toggle  minimum Cp  inclusion in polar\n"
             "   HINC     Toggle hinge moment inclusion in polar\n"
             "   Pacc i   Toggle auto point accumulation to active polar\n"
             "   PGET f   Read new polar from save file\n"
             "   PWRT i   Write polar to save file\n"
             "   PSUM     Show summary of stored polars\n"
             "   PLIS i   List stored polar(s)\n"
             "   PDEL i   Delete stored polar\n"
             "   PSOR i   Sort stored polar\n"
             "   PPlo ii. Plot stored polar(s)\n"
             "   APlo ii. Plot stored airfoil(s) for each polar\n"
             "   ASET i   Copy stored airfoil into current airfoil\n"
             "   PREM ir. Remove point(s) from stored polar\n"
             "   PNAM i   Change airfoil name of stored polar\n"
             "   PPAX     Change polar plot axis limits\n"
             "\n"
             "   RGET f   Read new reference polar from file\n"
             "   RDEL i   Delete stored reference polar\n"
             "\n"
             "   GRID     Toggle Cp vs x grid overlay\n"
             "   CPIO     Toggle inviscid Cp overlay on viscous Cp(x)\n"
             "   CREF     Toggle reference Cp data overlay\n"
             "   FREF     Toggle reference CL,CD.. data display\n"
             "\n"
             "   CPx      Plot Cp vs x\n"
             "   CPV      Plot airfoil with pressure vectors (gee wiz)\n"
             "  .VPlo     BL variable plots\n"
             "  .ANNO     Annotate current plot\n"
             "   HARD     Hardcopy current plot\n"
             "   SIZE r   Change plot-object size\n"
             "   CPMI r   Change minimum Cp axis annotation\n"
             "\n"
             "   BL   i   Plot boundary layer velocity profiles\n"
             "   BLC      Plot boundary layer velocity profiles at cursor\n"
             "   BLWT r   Change velocity profile scale weight\n"
             "\n"
             "   FMOM     Calculate flap hinge moment and forces\n"
             "   FNEW rr  Set new flap hinge point\n"
             "   VELS rr  Calculate velocity components at a point\n"
             "   DUMP f   Output Ue,Dstar,Theta,Cf vs s,x,y to file\n"
             "   CPWR f   Output x vs Cp to file\n"
             "   CPMN     Report minimum surface Cp\n"
             "   NAME s   Specify new airfoil name\n"
             "   NINC     Increment name version number\n";
}

void CommandInterpreter::runOperMenu() {
  menuStatus_ = MenuStatus::oper;

  // Original: COMOLD='****' for each entry (xoper.f lines 40-64).
  operRepeatCommand_.reset();
  operVelsRedoActive_ = false;

  while (!sessionEnded_) {
    // ASKC prompts (xoper.f 72-80): .OPERv/.OPERva/.OPERi/.OPERia.
    showMenuPrompt(std::string(session_.operating.viscous ? ".OPERv" : ".OPERi") +
                   (session_.polar.accumulating ? "a" : ""));

    std::string line;
    if (!readInputLine(line)) {
      sessionEnded_ = true;
      break;
    }

    const ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      // Empty input: back to TOP.
      break;
    }

    noteCommandExecuted();

    // Repeat control '!' analogous to COMOLD/ARGOLD (xoper.f 85-96).
    if (!parsed.command.empty() && parsed.command.front() == '!') {
      if (operVelsRedoActive_ && operRepeatCommand_) {
        // COMOLD='VELS': VELS branch with ARGOLD repeat
        // (xoper.f 1662-1663, 1643-1665).
        handleOperVelsCore(session_, input_, output_,
                           operRepeatCommand_->argument,
                           !operRepeatCommand_->argument.empty());
      } else {
        const auto bang = handleOperBangCore(
            session_, output_, session_.polar.accumulating);
        if (bang == OperBangCoreStatus::continuedConverged ||
            bang == OperBangCoreStatus::continuedNonConverged) {
 // '!' redo is a completed
          // operating point; event from the session state.
          notePointResult(makeOperPointResult(
              session_, session_.result.lastResult,
              bang == OperBangCoreStatus::continuedConverged));
        }
        accumulateOperPoint(session_, output_, session_.viscous.converged);
      }
      refreshGeometryTracking("command:!");
      continue;
    }

    lastCommand_ = parsed;
    executeOperCommand(parsed);
    refreshGeometryTracking("command:" + std::string{parsed.command});
  }

  menuStatus_ = MenuStatus::top;
}

CommandFlow CommandInterpreter::executeOperCommand(const ParsedCommand& parsed) {
  const std::string_view command = parsed.command;

  if (command == "?") {
    printOperHelp();
    return CommandFlow::stayInMenu;
  }

  if (isOneOf(command, kOperPlotCommands)) {
    noteUnavailablePlot();
    output_ << "Plotting is not available in this build.\n";
    return CommandFlow::stayInMenu;
  }

    // OPER commands handled directly in this menu.
  if (command == "ASET") {
    handleOperAsetCore(session_, input_, output_, parsed.argument,
                       !parsed.argument.empty());
    return CommandFlow::stayInMenu;
  }
  if (command == "BLF") {
    handleOperBlfCore(session_, output_, parsed.argument,
                      !parsed.argument.empty());
    return CommandFlow::stayInMenu;
  }
  if (command == "DAMP") {
    handleOperDampCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "FMOM") {
    handleOperFmomCore(session_, input_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "FNEW") {
    handleOperFnewCore(session_, parsed.argument, !parsed.argument.empty());
    return CommandFlow::stayInMenu;
  }
  if (command == "VELS") {
    const auto vels = handleOperVelsCore(session_, input_, output_,
                                         parsed.argument,
                                         !parsed.argument.empty());
    if (vels.status == OperVelsCoreResult::Status::reported) {
      // COMOLD/ARGOLD (xoper.f 1662-1663): ARGOLD is the
      // raw argument; prompts do not change ARGOLD.
      operRepeatCommand_ = parsed;
      operVelsRedoActive_ = true;
    }
    return CommandFlow::stayInMenu;
  }

  if (command == "CINC") {
    handleOperCincCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "HINC") {
    handleOperHincCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "PSUM") {
    handleOperPsumCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "PLIS") {
    handleOperPlisCore(session_, output_, parsed.argument,
                       !parsed.argument.empty());
    return CommandFlow::stayInMenu;
  }
  if (command == "PDEL") {
    handleOperPdel(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "PSOR") {
    handleOperPsor(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "RDEL") {
    handleOperRdel(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "PGET") {
    handleOperPget(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "PWRT") {
    handleOperPwrt(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "RGET") {
    handleOperRget(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "PREM") {
    handleOperPrem(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "PNAM") {
    handleOperPnam(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "PACC" || command == "P") {
    handleOperPacc(parsed);
    return CommandFlow::stayInMenu;
  }
  // Canonical OPER file exports (xoper.f 1666-1676).
  if (command == "CPWR") {
    handleOperCpwr(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "DUMP") {
    handleOperDump(parsed);
    return CommandFlow::stayInMenu;
  }
  if (command == "DMP") {
    handleOperDmp(parsed);
    return CommandFlow::stayInMenu;
  }

  if (command == "VISC" || command == "V") {
    handleOperViscCore(session_, input_, output_, parsed.argument,
                                session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "VPAR") {
    runVparMenu();
    return CommandFlow::stayInMenu;
  }
  if (command == "RE" || command == "R") {
    handleOperReCore(session_, input_, output_, parsed.argument,
                            session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "MACH" || command == "M") {
    handleOperMachCore(session_, input_, output_, parsed.argument,
                              session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "TYPE" || command == "T") {
    handleOperTypeCore(session_, input_, output_, parsed.argument,
                              session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "ITER") {
    handleOperIterCore(session_, input_, output_, parsed.argument);
    return CommandFlow::stayInMenu;
  }
  if (command == "INIT") {
    handleOperInitCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "CPMN") {
    handleOperCpmnCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "ALFA" || command == "A") {
    const auto status = handleOperAlphaCore(
        session_, input_, output_, parsed.argument,
        session_.polar.accumulating);
    noteOperPointStatus(status == OperAlphaCoreStatus::analyzed,
                        status == OperAlphaCoreStatus::nonConverged,
                        status == OperAlphaCoreStatus::missingGeometry);
    accumulateOperPoint(session_, output_, session_.viscous.converged);
    operRepeatCommand_ = parsed;
    operVelsRedoActive_ = false;
    return CommandFlow::stayInMenu;
  }
  if (command == "CLI") {
    const auto status = handleOperCliCore(
        session_, input_, output_, parsed.argument,
        session_.polar.accumulating);
    noteOperPointStatus(status == OperCliCoreStatus::analyzed,
                        status == OperCliCoreStatus::nonConverged,
                        status == OperCliCoreStatus::missingGeometry);
    accumulateOperPoint(session_, output_, session_.viscous.converged);
    operRepeatCommand_ = parsed;
    operVelsRedoActive_ = false;
    return CommandFlow::stayInMenu;
  }
  if (command == "CL" || command == "C") {
    const auto status = handleOperClCore(
        session_, input_, output_, parsed.argument,
        session_.polar.accumulating);
    noteOperPointStatus(status == OperClCoreStatus::analyzed,
                        status == OperClCoreStatus::nonConverged,
                        status == OperClCoreStatus::missingGeometry);
    accumulateOperPoint(session_, output_, session_.viscous.converged);
    operRepeatCommand_ = parsed;
    operVelsRedoActive_ = false;
    return CommandFlow::stayInMenu;
  }
  if (command == "ASEQ" || command == "AS") {
    const std::size_t pointsBefore = summary_.pointResults.size();
    const auto status = handleOperAseqCore(
        session_, input_, output_, parsed.argument,
        session_.polar.accumulating, &summary_.pointResults);
    noteSequenceOutcome(status == OperAseqCoreStatus::missingGeometry,
                        status == OperAseqCoreStatus::pointFailure,
                        pointsBefore);
    operRepeatCommand_ = parsed;
    operVelsRedoActive_ = false;
    return CommandFlow::stayInMenu;
  }
  if (command == "CSEQ" || command == "CS") {
    const std::size_t pointsBefore = summary_.pointResults.size();
    const auto status = handleOperCseqCore(
        session_, input_, output_, parsed.argument,
        session_.polar.accumulating, &summary_.pointResults);
    noteSequenceOutcome(status == OperCseqCoreStatus::missingGeometry,
                        status == OperCseqCoreStatus::pointFailure,
                        pointsBefore);
    operRepeatCommand_ = parsed;
    operVelsRedoActive_ = false;
    return CommandFlow::stayInMenu;
  }
  if (command == "NAME") {
    handleNameOper(parsed.argument);
    return CommandFlow::stayInMenu;
  }
  if (command == "NINC") {
    handleNincOper();
    return CommandFlow::stayInMenu;
  }
  if (command == "NDEC") {
    handleNdecOper();
    return CommandFlow::stayInMenu;
  }

  // Unknown command, format 8000 from xoper.f.
  noteUnknownCommand();
  output_ << " " << formatFortranA4(command)
          << " command not recognized.  Type a \"?\" for list\n";
  return CommandFlow::stayInMenu;
}

void CommandInterpreter::handleNameOper(const std::string& argument) {
  std::string answer;
  bool hasAnswer = false;
  for (;;) {
    const auto status = handleOperNameCore(session_, output_, argument,
                                           !argument.empty(), answer,
                                           hasAnswer);
    if (status == OperNameCoreStatus::promptRequested) {
      if (!readAnswerLine(answer)) {
        return;
      }
      hasAnswer = true;
      continue;
    }
    return;
  }
}

void CommandInterpreter::handleNincOper() {
  std::string answer;
  bool hasAnswer = false;
  for (;;) {
    const auto status = handleOperNincCore(session_, output_, answer,
                                           hasAnswer);
    if (status == OperNincCoreStatus::promptRequested) {
      if (!readAnswerLine(answer)) {
        return;
      }
      hasAnswer = true;
      continue;
    }
    return;
  }
}

void CommandInterpreter::handleNdecOper() {
  std::string answer;
  bool hasAnswer = false;
  for (;;) {
    const auto status = handleOperNdecCore(session_, output_, answer,
                                           hasAnswer);
    if (status == OperNdecCoreStatus::promptRequested) {
      if (!readAnswerLine(answer)) {
        return;
      }
      hasAnswer = true;
      continue;
    }
    return;
  }
}

// ---
// VPAR submenu: handled by session/core handlers.
// ---

void CommandInterpreter::runVparMenu() {
  menuStatus_ = MenuStatus::vpar;

  // Fortran VPAR (xoper.f 2505-2531, GO TO 10): at entry the
  // parameter table (format 1200) is written before the first prompt;
  // SHOW and each valid change run through the same display.
  handleVparShowCore(session_, output_);

  while (!sessionEnded_) {
    showMenuPrompt("..VPAR");

    std::string line;
    if (!readInputLine(line)) {
      sessionEnded_ = true;
      break;
    }

    const ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      // Empty input: back to OPER.
      break;
    }

    noteCommandExecuted();
    lastCommand_ = parsed;
    executeVparCommand(parsed);
  }

  menuStatus_ = MenuStatus::oper;
}

CommandFlow CommandInterpreter::executeVparCommand(const ParsedCommand& parsed) {
  const std::string_view command = parsed.command;

  if (command == "?") {
    printVparHelp();
    return CommandFlow::stayInMenu;
  }

  if (command == "SHOW") {
    handleVparShowCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
  if (command == "XTR" || command == "X") {
    handleVparXtrCore(session_, input_, output_, parsed.argument,
                          session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "N") {
    handleVparNCritCore(session_, input_, output_, parsed.argument,
                           session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "VACC" || command == "V") {
    handleVparVaccCore(session_, input_, output_, parsed.argument);
    return CommandFlow::stayInMenu;
  }
  if (command == "INIT") {
    handleVparInitCore(session_, output_);
    return CommandFlow::stayInMenu;
  }
    // VPAR menu commands. Order is irrelevant;
  // the 4-character commands (ASKC parser) are disjoint.
  if (command == "NT") {
    handleVparNCritTopCore(session_, input_, output_, parsed.argument,
                           session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "NB") {
    handleVparNCritBotCore(session_, input_, output_, parsed.argument,
                           session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "WAKE" || command == "W") {
    handleVparWakeCore(session_, input_, output_, parsed.argument);
    return CommandFlow::stayInMenu;
  }
  if (command == "LAG") {
    handleVparLagCore(session_, input_, output_, parsed.argument,
                      session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "GB") {
    handleVparGbCore(session_, input_, output_, parsed.argument,
                     session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "CTR") {
    handleVparCtrCore(session_, input_, output_, parsed.argument,
                      session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }
  if (command == "CFAC") {
    handleVparCfacCore(session_, input_, output_, parsed.argument);
    return CommandFlow::stayInMenu;
  }
  if (command == "REST") {
    handleVparRestCore(session_, output_, session_.polar.accumulating);
    return CommandFlow::stayInMenu;
  }

  // Unknown command, format 1000 from VPAR.
  noteUnknownCommand();
  output_ << " " << formatFortranA4(command)
          << " command not recognized.  Type a \"?\" for list\n";
  return CommandFlow::stayInMenu;
}

void CommandInterpreter::printVparHelp() {
  // Format 1050 from SUBROUTINE VPAR in xoper.f.
  output_ << "\n"
             "   <cr>    Return to OPER menu\n"
             "   SHOW    Display viscous parameters\n"
             "   Xtr  rr Change trip positions Xtr/c\n"
             "   N    r  Change critical amplification exponent Ncrit\n"
             "   NT   r  Change Ncrit on Top\n"
             "   NB   r  Change Ncrit on Bot\n"
             "   Vacc r  Change Newton solution acceleration parameter\n"
             "   Wake r  Change wake length/chord\n"
             "   INIT    BL initialization flag toggle\n"
             "\n"
             "   LAG     change lag equation constants\n"
             "   GB      change G-beta constants A,B\n"
             "   CTR     change initial transition-Ctau constants\n"
             "   REST    restore BL calibration to baseline\n";
}

// ---
// I/O helper
// ---

bool CommandInterpreter::readInputLine(std::string& line) {
  if (!std::getline(input_, line)) {
    return false;
  }
  return true;
}

bool CommandInterpreter::readAnswerLine(std::string& line) {
  // Answer without its own prompt; the prompt belongs to the
  // handler. EOF ends the session in a controlled way.
  if (!std::getline(input_, line)) {
    sessionEnded_ = true;
    return false;
  }
  return true;
}

namespace {
// ASKC/ASKR/ASKI/ASKS prompt section before the first '^' (userio.f
// NP = Index(Prompt,'^')-1; without '^' the entire length).
std::string promptBeforeCaret(const std::string& promptText) {
  const std::size_t caret = promptText.find('^');
  return caret == std::string::npos ? promptText : promptText.substr(0, caret);
}
}  // namespace

bool CommandInterpreter::askReal(const std::string& promptText, double& value) {
  // ASKR from userio.f: FORMAT(/A,' r> ',$) without leading blank and
  // without requirement display; empty line preserves the previous value;
  // invalid number queried again without a special message (READ ERR=10 -> GO TO 10).
  const std::string prompt = promptBeforeCaret(promptText);
  for (;;) {
    output_ << "\n" << prompt << "   r>  ";
    output_.flush();

    std::string line;
    if (!std::getline(input_, line)) {
      return false;
    }
    if (trimSpaces(line).empty()) {
      return true;  // empty input keeps the current value
    }
    const auto parsed = parseRealArguments(line);
    if (parsed.success() && !parsed.values.empty()) {
      value = parsed.values[0];
      return true;
    }
  }
}

bool CommandInterpreter::askInteger(const std::string& promptText, int& value) {
  // ASKI from userio.f: FORMAT(/A,' i> ',$) without leading blank;
  // invalid number queried again without a special message (READ ERR=10 -> GO TO 10).
  const std::string prompt = promptBeforeCaret(promptText);
  for (;;) {
    output_ << "\n" << prompt << "   i>  ";
    output_.flush();

    std::string line;
    if (!std::getline(input_, line)) {
      return false;
    }
    const auto parsed = parseIntegerArguments(line);
    if (parsed.success() && !parsed.values.empty()) {
      value = parsed.values[0];
      return true;
    }
  }
}

bool CommandInterpreter::askString(const std::string& promptText,
                                   std::string& value) {
  // ASKS from userio.f: FORMAT(/A,' s> ',$) without leading blank; the
  // answer is adopted literally (without STRIP).
  output_ << "\n" << promptBeforeCaret(promptText) << "   s>  ";
  output_.flush();

  std::string line;
  if (!std::getline(input_, line)) {
    return false;
  }
  value = line;
  return true;
}

// ---
// OPER polar commands with prompt chains (xoper.f 978-1178).
// ---

// PDEL (xoper.f 1081-1135): IP resolution, ASKI loop for invalid
// index, PLRCOP chain with format 1310, IPACT handling, format 1320.
void CommandInterpreter::handleOperPdel(const ParsedCommand& parsed) {
  if (session_.polar.slots.empty()) {
    output_ << " No polars are stored\n";
    return;
  }
  int ip = 0;
  const auto arguments = parseIntegerArguments(parsed.argument);
  if (arguments.success() && !arguments.values.empty()) {
    ip = arguments.values[0];
  } else {
    ip = static_cast<int>(session_.polar.slots.size()) + 1;
  }
  for (;;) {
    if (ip == 0) {
      session::deleteAllPolarSlots(session_);
      break;
    }
    if (ip == -1) {
      return;
    }
    if (ip < -1 || ip > static_cast<int>(session_.polar.slots.size())) {
      emitPolarSummary(output_, session_, 1,
                       static_cast<int>(session_.polar.slots.size()),
                       session_.polar.activeSlot + 1);
      if (!askInteger("Specify polar to delete (0 = all, -1 = abort)", ip)) {
        sessionEnded_ = true;
        return;
      }
      continue;
    }
    const int index0 = ip - 1;
    if (session_.polar.activeSlot == index0) {
      output_ << " Active polar deleted.  Accumulation turned off\n";
    }
    const auto removed = session::deletePolarSlot(session_, index0);
    for (const auto& move : removed.moves) {
      emitPolarMoved(output_, move.first + 1, move.second + 1);
    }
    break;
  }
  if (session_.polar.activeSlot >= 0) {
    emitPolarNowActive(output_, session_.polar.activeSlot + 1);
  }
}

// PSOR (xoper.f 1138-1178): IDSORT = IAL; ASKI loop as in PDEL.
void CommandInterpreter::handleOperPsor(const ParsedCommand& parsed) {
  if (session_.polar.slots.empty()) {
    output_ << " No polars are stored\n";
    return;
  }
  int ip = 0;
  const auto arguments = parseIntegerArguments(parsed.argument);
  if (arguments.success() && !arguments.values.empty()) {
    ip = arguments.values[0];
  } else {
    ip = static_cast<int>(session_.polar.slots.size()) + 1;
  }
  for (;;) {
    if (ip == -1) {
      return;
    }
    if (ip < -1 || ip > static_cast<int>(session_.polar.slots.size())) {
      emitPolarSummary(output_, session_, 1,
                       static_cast<int>(session_.polar.slots.size()),
                       session_.polar.activeSlot + 1);
      if (!askInteger("Specify polar to sort (0 = all, -1 = abort)", ip)) {
        sessionEnded_ = true;
        return;
      }
      continue;
    }
    const int first = (ip == 0) ? 1 : ip;
    const int last = (ip == 0) ? static_cast<int>(session_.polar.slots.size())
                               : ip;
    for (int slot = first; slot <= last; ++slot) {
      static_cast<void>(session::sortPolarSlotByAlpha(session_, slot - 1));
    }
    return;
  }
}

// RDEL (xoper.f 978-1014): PRFCOP chain; canonical WRITE(*,1310) (the
// format 1410 defined in RDEL is dead code), PRFSUM header in the
// prompt loop.
void CommandInterpreter::handleOperRdel(const ParsedCommand& parsed) {
  if (session_.polar.referenceSlots.empty()) {
    output_ << " No reference polars are stored\n";
    return;
  }
  int ir = 0;
  const auto arguments = parseIntegerArguments(parsed.argument);
  if (arguments.success() && !arguments.values.empty()) {
    ir = arguments.values[0];
  } else {
    ir = static_cast<int>(session_.polar.referenceSlots.size()) + 1;
  }
  for (;;) {
    if (ir == 0) {
      session::deleteAllReferenceSlots(session_);
      return;
    }
    if (ir == -1) {
      return;
    }
    if (ir < -1 || ir > static_cast<int>(session_.polar.referenceSlots.size())) {
      std::vector<std::string> labels;
      labels.reserve(session_.polar.referenceSlots.size());
      for (const auto& slot : session_.polar.referenceSlots) {
        labels.push_back(slot.label);
      }
      emitReferencePolarSummary(output_, labels);
      if (!askInteger("Specify ref. polar to delete (0 = all, -1 = abort)",
                      ir)) {
        sessionEnded_ = true;
        return;
      }
      continue;
    }
    const auto removed = session::deleteReferenceSlot(session_, ir - 1);
    for (const auto& move : removed.moves) {
      emitPolarMoved(output_, move.first + 1, move.second + 1);
    }
    return;
  }
}

// ---
// Polar file commands (xoper.f 826-971, 1293-1447).
// ---
namespace {

bool readWholeFile(const std::string& path, std::string& content) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  // No istreambuf_iterator: GCC -O3 (-Wnull-dereference) cannot prove
  // the buffer pointer as non-zero; the rdbuf detour is
  // equivalent and warning-free.
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  content = buffer.str();
  return true;
}

bool writeWholeFile(const std::string& path, const std::string& content) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream << content;
  return stream.good();
}

// OPER index conversion from real input (xoper.f 115-121: GETFLT,
// magnitudes via 2.1E9 set to 2**30).
int toOperInt(double value) {
  if (!(value > -2.1e9 && value < 2.1e9)) {
    return 1 << 30;
  }
  return static_cast<int>(value);
}

}  // namespace

// PGET (xoper.f 826-867): NPX boundary, ASKS filename, POLREAD, echo the
// read polar (POLWRIT LU=6), PFNAME(IP)=FNAME, FORMAT 5500.
void CommandInterpreter::handleOperPget(const ParsedCommand& parsed) {
  if (session_.polar.slots.size() >= core::polar::maxPolarSlots) {
    output_ << "\n Number of polars is at array limit\n"
            << " Delete with PDEL if necessary\n";
    return;
  }
  std::string fileName = io::stripBlanks(parsed.argument);
  if (fileName.empty() && !askString("Enter polar filename", fileName)) {
    sessionEnded_ = true;
    return;
  }
  fileName = io::stripBlanks(fileName);
  std::string content;
  io::ParsedPolar parsedPolar;
  if (!readWholeFile(fileName, content)) {
    parsedPolar.error = true;
  } else {
    parsedPolar = io::readPolarText(content);
  }
  if (parsedPolar.error) {
    output_ << " Polar file READ error\n";
    return;
  }
  session_.polar.elementSelection = parsedPolar.elementSelection;
  const auto added = session::appendPolarSlot(
      session_, polarDatasetFromParsed(parsedPolar), fileName);
  if (added.capacityExceeded) {
    output_ << "\n Number of polars is at array limit\n"
            << " Delete with PDEL if necessary\n";
    return;
  }
  const auto& slot = session_.polar.slots[static_cast<std::size_t>(
      added.slotIndex)];
  output_ << io::writePolarText(makePolarWriteRequest(
      slot, session_.polar.includeCpMin, session_.polar.includeHingeMoment, 1,
      static_cast<int>(slot.points.size()), true,
      session_.polar.elementSelection));
  output_ << "\n Stored as  Polar"
          << io::fortranI(4, added.slotIndex + 1) << "\n";
}

// PWRT (xoper.f 870-931): index selection (0=all, -1=abort), PLRSUM per polar,
// ASKS with PFNAME requirement in square brackets, POLWRIT LU=19, format 1075
// for write errors, otherwise PFNAME(IP)=FNAME.
void CommandInterpreter::handleOperPwrt(const ParsedCommand& parsed) {
  const int npol = static_cast<int>(session_.polar.slots.size());
  if (npol == 0) {
    // Fortran jumps with NPOL=0 into the index question; PLRSUM then prints
    // only the table header.
    emitPolarSummary(output_, session_, 1, 0, session_.polar.activeSlot + 1);
    int ip = 0;
    if (!askInteger("Enter index of polar to write (0=all, -1=abort)", ip)) {
      sessionEnded_ = true;
      return;
    }
    if (ip == -1) {
      return;
    }
    return;
  }
  bool hasArgument = false;
  int ip = 0;
  const auto arguments = parseIntegerArguments(parsed.argument);
  if (arguments.success() && !arguments.values.empty()) {
    hasArgument = true;
    ip = arguments.values[0];
  }
  int first = 0;
  int last = 0;
  for (;;) {
    if (!hasArgument) {
      if (npol == 1) {
        ip = 1;
      } else {
        emitPolarSummary(output_, session_, 1, npol,
                         session_.polar.activeSlot + 1);
        if (!askInteger("Enter index of polar to write (0=all, -1=abort)",
                        ip)) {
          sessionEnded_ = true;
          return;
        }
        if (ip == -1) {
          return;
        }
      }
    }
    if (ip == 0) {
      first = 1;
      last = npol;
      break;
    }
    if (ip >= 1 && ip <= npol) {
      first = ip;
      last = ip;
      break;
    }
    hasArgument = false;  // xoper.f 890: NINPUT=0, GO TO 75
  }
  for (int slot = first; slot <= last; ++slot) {
    emitPolarSummary(output_, session_, slot, slot,
                     session_.polar.activeSlot + 1);
    const std::string saved = io::stripBlanks(
        session_.polar.saveFileNames[static_cast<std::size_t>(slot - 1)]);
    std::string prompt = "Enter polar output filename";
    if (!saved.empty()) {
      prompt = "Enter polar output filename [" + saved + "]";
    }
    std::string fileName;
    if (!askString(prompt, fileName)) {
      sessionEnded_ = true;
      return;
    }
    fileName = io::stripBlanks(fileName);
    if (fileName.empty() && !saved.empty()) {
      fileName = saved;  // xoper.f 906
    }
 // The overwrite query checks the
    // transformed path; the prompt shows the logical name.
    const std::string effective =
        io::applyOutputPathPolicy(session_.outputPaths, fileName);
    if (std::filesystem::exists(effective)) {
      // POLWRIT LQUERY (iopol.f 497-509): OPEN STATUS='OLD' successful ->
      // overwrite query; answer with N/n -> 'Polar file not saved'.
      output_ << "\n Output file exists.  Overwrite?  Y\n";
      std::string answer;
      if (!readAnswerLine(answer)) {
        sessionEnded_ = true;
        return;
      }
      if (answer.find('N') != std::string::npos ||
          answer.find('n') != std::string::npos) {
        output_ << " Polar file not saved\n";
        continue;
      }
    }
    const auto& dataset =
        session_.polar.slots[static_cast<std::size_t>(slot - 1)];
    const std::string text = io::writePolarText(makePolarWriteRequest(
        dataset, session_.polar.includeCpMin, session_.polar.includeHingeMoment,
        1, static_cast<int>(dataset.points.size()), false,
        session_.polar.elementSelection));
    if (!writeWholeFile(effective, text)) {
      output_ << " Polar" << io::fortranI(3, slot) << "  not written\n";
    } else {
      session::setPolarSaveName(session_, slot - 1, fileName);
    }
  }
}

// RGET (xoper.f 934-971): NPX boundary, ASKS filename, OPEN/READ error
// as 'File OPEN error', label read-only query for empty NAMEREF.
void CommandInterpreter::handleOperRget(const ParsedCommand& parsed) {
  if (session_.polar.referenceSlots.size() >= core::polar::maxPolarSlots) {
    output_ << "\n Number of reference polars is at array limit\n"
            << " Delete with RDEL if necessary\n";
    return;
  }
  std::string fileName = io::stripBlanks(parsed.argument);
  if (fileName.empty() && !askString("Enter reference polar filename",
                                     fileName)) {
    sessionEnded_ = true;
    return;
  }
  fileName = io::stripBlanks(fileName);
  std::string content;
  io::ParsedReferencePolar parsedReference;
  if (!readWholeFile(fileName, content)) {
    parsedReference.error = true;
  } else {
    parsedReference = io::readReferencePolarText(content);
  }
  if (parsedReference.error) {
    output_ << " File OPEN error\n";
    return;
  }
  auto dataset = referencePolarFromParsed(parsedReference);
  const auto added = session::appendReferenceSlot(session_, std::move(dataset));
  if (added.capacityExceeded) {
    output_ << "\n Number of reference polars is at array limit\n"
            << " Delete with RDEL if necessary\n";
    return;
  }
  auto& label = session_.polar.referenceSlots[static_cast<std::size_t>(
      added.slotIndex)].label;
  label = io::stripBlanks(label);
  if (label.empty()) {
    if (!askString("Enter label for reference polar", label)) {
      sessionEnded_ = true;
      return;
    }
    label = io::stripBlanks(label);
  } else {
    output_ << "\n" << label << "\n";
  }
}

// PREM (xoper.f 1293-1392): index resolution with PLRSUM/ASKI, POLWRIT echo
// without argument alphas, format 3220 prompt with GETFLT repetition,
// tolerance 5e-4, slot elimination with format 1310/1320.
void CommandInterpreter::handleOperPrem(const ParsedCommand& parsed) {
  if (session_.polar.slots.empty()) {
    output_ << "\n No polars are stored\n";
    return;
  }
  auto argumentValues = parseRealArguments(parsed.argument);
  std::vector<double> values;
  if (argumentValues.success()) {
    values = std::move(argumentValues.values);
  }
  int ip = 0;
  for (;;) {
    if (values.empty()) {
      if (session_.polar.slots.size() == 1) {
        ip = 1;
      } else {
        emitPolarSummary(output_, session_, 1,
                         static_cast<int>(session_.polar.slots.size()),
                         session_.polar.activeSlot + 1);
        if (!askInteger("Enter index of polar to modify", ip)) {
          sessionEnded_ = true;
          return;
        }
      }
    } else {
      ip = toOperInt(values[0]);
    }
    if (ip == 0) {
      return;
    }
    if (ip < 1 || ip > static_cast<int>(session_.polar.slots.size())) {
      output_ << "\n Specified polar airfoil does not exist\n";
      values.clear();
      continue;
    }
    break;
  }
  std::vector<double> alphas;
  if (values.size() >= 2) {
    alphas.assign(values.begin() + 1, values.end());
  } else {
    emitStoredPolarHeader(output_, ip);
    const auto& slot = session_.polar.slots[static_cast<std::size_t>(ip - 1)];
    output_ << io::writePolarText(makePolarWriteRequest(
        slot, session_.polar.includeCpMin, session_.polar.includeHingeMoment, 1,
        static_cast<int>(slot.points.size()), false,
        session_.polar.elementSelection));
    for (;;) {
      output_ << "\n Enter alpha(s) of points to be removed:  ";
      output_.flush();
      std::string line;
      if (!readInputLine(line)) {
        sessionEnded_ = true;
        return;
      }
      const auto parsedValues = parseRealArguments(line);
      if (parsedValues.success() && !parsedValues.values.empty()) {
        alphas = parsedValues.values;
        break;
      }
    }
  }
  const auto removed =
      session::removePolarPoints(session_, ip - 1, alphas);
  for (const auto& move : removed.moves) {
    emitPolarMoved(output_, move.first + 1, move.second + 1);
  }
  if (removed.polarEliminated && session_.polar.activeSlot >= 0) {
    emitPolarNowActive(output_, session_.polar.activeSlot + 1);
  }
}

// PNAM (xoper.f 1395-1447): index resolution, POLWRIT header output
// (IA1=0, IA2=-1), format 3320 prompt, READ line then STRIP.
void CommandInterpreter::handleOperPnam(const ParsedCommand& parsed) {
  if (session_.polar.slots.empty()) {
    output_ << "\n No polars are stored\n";
    return;
  }
  const auto arguments = parseIntegerArguments(parsed.argument);
  bool hasArgument = arguments.success() && !arguments.values.empty();
  int ip = hasArgument ? arguments.values[0] : 0;
  for (;;) {
    if (!hasArgument) {
      if (session_.polar.slots.size() == 1) {
        ip = 1;
      } else {
        emitPolarSummary(output_, session_, 1,
                         static_cast<int>(session_.polar.slots.size()),
                         session_.polar.activeSlot + 1);
        if (!askInteger("Enter index of polar to modify", ip)) {
          sessionEnded_ = true;
          return;
        }
      }
    }
    if (ip == 0) {
      return;
    }
    if (ip < 1 || ip > static_cast<int>(session_.polar.slots.size())) {
      output_ << "\n Specified polar airfoil does not exist\n";
      hasArgument = false;
      continue;
    }
    break;
  }
  emitStoredPolarHeader(output_, ip);
  const auto& slot = session_.polar.slots[static_cast<std::size_t>(ip - 1)];
  output_ << io::writePolarText(makePolarWriteRequest(
      slot, session_.polar.includeCpMin, session_.polar.includeHingeMoment, 0,
      -1, false, session_.polar.elementSelection));
  output_ << "\n Enter new airfoil name of polar:  ";
  output_.flush();
  std::string line;
  if (!readInputLine(line)) {
    sessionEnded_ = true;
    return;
  }
  static_cast<void>(session::renamePolarSlot(session_, ip - 1,
                                             io::stripBlanks(line)));
}

// ---
// PACC (xoper.f 772-823) with PLRSET (xpol.f 22-137), PLRINI (xpol.f
// 180-382) and PLXINI (xpol.f 383-556).
// ---

bool CommandInterpreter::askYesNo(const std::string& promptText, bool& value) {
  // ASKL (userio.f): FORMAT(/A,' y/n> ',$); empty/invalid again.
  for (;;) {
    emitYesNoPrompt(output_, promptText);
    output_.flush();
    std::string line;
    if (!std::getline(input_, line)) {
      return false;
    }
    if (!line.empty()) {
      char first = line[0];
      if (first == 'y') first = 'Y';
      if (first == 'n') first = 'N';
      if (first == 'Y' || first == 'N') {
        value = (first == 'Y');
        return true;
      }
    }
  }
}

void CommandInterpreter::initPolarSaveFile(int slot0) {
  auto& polar = session_.polar;
  std::string& saved = polar.saveFileNames[static_cast<std::size_t>(slot0)];
  saved = io::stripBlanks(saved);
  std::string prompt;
  if (saved.empty()) {
    prompt = "Enter  polar save filename  OR  <return> for no file";
  } else {
    output_ << " Default polar save filename:   " << saved << "\n";
    prompt = "Enter  new filename  OR  \"none\"  OR  <return> for default";
  }
  std::string fileName;
  if (!askString(prompt, fileName)) {
    sessionEnded_ = true;
    return;
  }
  fileName = io::stripBlanks(fileName);
  bool noFile = false;
  if (fileName.empty()) {
    fileName = saved;
    noFile = fileName.empty();
  } else if (fileName.rfind("NONE", 0) == 0 || fileName.rfind("none", 0) == 0) {
    // INDEX('NONEnone',FNAME(1:4)).NE.0 (xpol.f 217).
    noFile = true;
  }
  if (noFile) {
    polar.saveFileOpen = false;
    output_ << "\n Polar save file will NOT be written\n";
    return;
  }
  polar.saveFileOpen = false;
 // Read-before-write and setup use
  // the same transformed path; saveFileNames carries the logical
  // names (prompt requirement and success messages remain logical).
  const std::string effective =
      io::applyOutputPathPolicy(session_.outputPaths, fileName);
  std::string content;
  if (!readWholeFile(effective, content)) {
    // Label 60: create a new file with header.
    auto& slot = polar.slots[static_cast<std::size_t>(slot0)];
    const std::string header = io::writePolarText(makePolarWriteRequest(
        slot, polar.includeCpMin, polar.includeHingeMoment, 0, -1, false,
        polar.elementSelection));
    if (!writeWholeFile(effective, header)) {
      output_ << " New polar save file OPEN error\n";
      return;
    }
    saved = fileName;
    polar.saveFileOpen = true;
    output_ << "\n New polar save file available\n";
    return;
  }
  const io::ParsedPolar parsed = io::readPolarText(content);
  if (parsed.error) {
    output_ << " Old polar save file READ error\n";
    return;
  }
  polar.elementSelection = parsed.elementSelection;
  auto& slot = polar.slots[static_cast<std::size_t>(slot0)];
  slot = polarDatasetFromParsed(parsed);
  saved = fileName;
  // Comparison exact as PLRINI (xpol.f 235-252); canonical REAL(4)
  // equality, here double comparison of the read values.
  const auto& mr = session_.operating.machReynolds;
  const std::string currentName = session_.geometry.name;
  const std::string fileNameLabel = slot.metadata.airfoilName;
  const bool differs =
      currentName.size() != fileNameLabel.size() ||
      currentName != fileNameLabel ||
      slot.metadata.reCl1 != mr.reinf1 || slot.metadata.machCl1 != mr.minf1 ||
      static_cast<int>(slot.metadata.reType) != mr.reType ||
      static_cast<int>(slot.metadata.machType) != mr.machType ||
      slot.metadata.ncrit[0] != session_.operating.criticalReynoldsExponents[0] ||
      slot.metadata.ncrit[1] != session_.operating.criticalReynoldsExponents[1] ||
      slot.metadata.xtrip[0] != session_.operating.topTripX ||
      slot.metadata.xtrip[1] != session_.operating.bottomTripX;
  if (differs) {
    emitSaveParameterComparison(
        output_, currentName, fileNameLabel, mr.reinf1, slot.metadata.reCl1,
        mr.minf1, slot.metadata.machCl1, mr.reType,
        static_cast<int>(slot.metadata.reType), mr.machType,
        static_cast<int>(slot.metadata.machType),
        session_.operating.criticalReynoldsExponents[0], slot.metadata.ncrit[0],
        session_.operating.criticalReynoldsExponents[1], slot.metadata.ncrit[1],
        session_.operating.topTripX, slot.metadata.xtrip[0],
        session_.operating.bottomTripX, slot.metadata.xtrip[1]);
    output_ << "\n Current parameters different from old save file values.\n";
    bool adopt = false;
    if (!askYesNo("Set current parameters to old save file values ?", adopt)) {
      sessionEnded_ = true;
      return;
    }
    if (!adopt) {
      output_ << "\n Old polar save file NOT available for appending\n";
      return;
    }
    session::adoptPolarParameters(session_, slot0);
  }
  output_ << "\n Old polar save file read in ...\n";
  output_ << io::writePolarText(makePolarWriteRequest(
      slot, polar.includeCpMin, polar.includeHingeMoment, 1,
      static_cast<int>(slot.points.size()), true, polar.elementSelection));
  polar.saveFileOpen = true;
  output_ << "\n Old polar save file available for appending\n";
}

void CommandInterpreter::initPolarDumpFile(int slot0) {
  auto& polar = session_.polar;
  std::string& saved = polar.dumpFileNames[static_cast<std::size_t>(slot0)];
  saved = io::stripBlanks(saved);
  std::string prompt;
  if (saved.empty()) {
    prompt = "Enter  polar dump filename  OR  <return> for no file";
  } else {
    output_ << " Default polar dump filename:   " << saved << "\n";
    prompt = "Enter  new filename  OR  \"none\"  OR  <return> for default";
  }
  std::string fileName;
  if (!askString(prompt, fileName)) {
    sessionEnded_ = true;
    return;
  }
  fileName = io::stripBlanks(fileName);
  bool noFile = false;
  if (fileName.rfind("NONE", 0) == 0 || fileName.rfind("none", 0) == 0) {
    noFile = true;
  }
  if (fileName.empty()) {
    noFile = saved.empty();
    if (!noFile) fileName = saved;
  }
  if (noFile) {
    polar.dumpFileOpen = false;
    output_ << "\n Polar dump file will NOT be written\n";
    return;
  }
  polar.dumpFileOpen = false;
 // read and write use the same transformed file.
  const std::string effective =
      io::applyOutputPathPolicy(session_.outputPaths, fileName);
  std::string content;
  if (!readWholeFile(effective, content)) {
    content.clear();
  }
  if (content.empty()) {
    // Label 60: new unformatted header.
    io::DumpHeader header;
    header.name = session_.geometry.name;
    header.code = "XFOIL   ";
    header.version = 6.99;
    header.mach = session_.operating.machReynolds.minf1;
    header.reynMillions = session_.operating.machReynolds.reinf1 / 1.0e6;
    header.acritTop = session_.operating.criticalReynoldsExponents[0];
    header.acritBottom = session_.operating.criticalReynoldsExponents[1];
    header.matyp = session_.operating.machReynolds.machType;
    header.retyp = session_.operating.machReynolds.reType;
    header.iix = 0;
    header.ilex = 0;
    header.itex = 0;
    header.iibx = static_cast<int>(session_.paneling.panel.x.size());
    for (std::size_t i = 0; i < session_.paneling.panel.x.size(); ++i) {
      header.geometry.emplace_back(session_.paneling.panel.x[i],
                                   session_.paneling.panel.y[i]);
    }
    const auto bytes = io::encodeDumpHeader(header);
    std::ofstream stream(effective, std::ios::binary | std::ios::trunc);
    if (!stream) {
      output_ << " OPEN error on polar dump file "
              << io::fortranA(48, fileName) << "\n";
      return;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream.good()) {
      output_ << " OPEN error on polar dump file "
              << io::fortranA(48, fileName) << "\n";
      return;
    }
    saved = fileName;
    polar.dumpFileOpen = true;
    output_ << "\n New polar dump file available\n";
    return;
  }
  const auto decoded = io::decodeDump(
      reinterpret_cast<const std::uint8_t*>(content.data()), content.size());
  if (!decoded.file.has_value()) {
    if (decoded.error == io::DumpError::kMissingRecord ||
        decoded.error == io::DumpError::kUnexpectedRecord) {
      // Empty/truncated file before the first record: canonical
      // END=60 -> new file.
    } else {
      output_ << " Polar dump file READ error\n";
      return;
    }
  }
  if (decoded.file.has_value()) {
    const auto& header = decoded.file->header;
    auto& mr = session_.operating.machReynolds;
    const double fileRe = header.reynMillions * 1.0e6;
    const std::string dumpName = io::stripBlanks(header.name);
    const std::string currentName = session_.geometry.name;
    const bool differs =
        dumpName.size() != currentName.size() || dumpName != currentName ||
        fileRe != mr.reinf1 || header.mach != mr.minf1 ||
        header.retyp != mr.reType || header.matyp != mr.machType ||
        header.acritTop != session_.operating.criticalReynoldsExponents[0] ||
        header.acritBottom != session_.operating.criticalReynoldsExponents[1];
    if (differs) {
      emitDumpParameterComparison(output_, dumpName, currentName, fileRe,
                                  mr.reinf1, header.mach, mr.minf1,
                                  header.retyp, mr.reType, header.matyp,
                                  mr.machType, header.acritTop,
                                  session_.operating.criticalReynoldsExponents[0],
                                  session_.operating.criticalReynoldsExponents[1]);
      output_ << "\n Current parameters different from old dump file values.\n";
      bool adopt = false;
      if (!askYesNo("Set current parameters to old dump file values ?",
                    adopt)) {
        sessionEnded_ = true;
        return;
      }
      if (!adopt) {
        output_ << "\n Old polar dump file NOT available for appending\n";
        return;
      }
      session_.geometry.name = header.name;
      session_.geometry.buffer.name = io::stripBlanks(header.name);
      mr.minf1 = header.mach;
      mr.reinf1 = fileRe;
      mr.reType = header.retyp;
      mr.machType = header.matyp;
      session_.operating.criticalReynoldsExponents = {header.acritTop,
                                                 header.acritBottom};
    }
    saved = fileName;
    polar.dumpFileOpen = true;
    output_ << "\n Old polar dump file available for appending\n";
  }
}

// PACC (xoper.f 772-823).
void CommandInterpreter::handleOperPacc(const ParsedCommand& parsed) {
  auto& polar = session_.polar;
  polar.accumulating = !polar.accumulating;
  if (!polar.accumulating) {
    output_ << "\n Polar accumulation disabled\n";
    polar.activeSlot = -1;
    polar.saveFileOpen = false;
    polar.dumpFileOpen = false;
    return;
  }
  int ip = 0;  // 1-based as Fortran IP
  const auto arguments = parseIntegerArguments(parsed.argument);
  if (arguments.success() && !arguments.values.empty()) {
    ip = std::min(std::max(arguments.values[0], 0),
                  static_cast<int>(polar.slots.size()) + 1);
  } else {
    ip = static_cast<int>(polar.slots.size()) + 1;
    if (static_cast<std::size_t>(ip) <= polar.slots.size()) {
      polar.saveFileNames[static_cast<std::size_t>(ip - 1)] = " ";
      polar.dumpFileNames[static_cast<std::size_t>(ip - 1)] = " ";
    }
  }
  int ipact = 0;
  if (ip > static_cast<int>(polar.slots.size())) {
    if (polar.slots.size() >= core::polar::maxPolarSlots) {
      output_ << "\n Number of polars is at array limit\n"
              << " New polar will not be stored\n";
      ipact = 0;
    } else {
      ipact = static_cast<int>(polar.slots.size()) + 1;
    }
  } else {
    ipact = ip;
  }
  // PLRSET (xpol.f 22-137).
  if (ipact > 0 && ipact <= static_cast<int>(polar.slots.size())) {
    output_ << "\n Existing stored polar is chosen for appending...\n";
    const auto& slot =
        polar.slots[static_cast<std::size_t>(ipact - 1)];
    output_ << io::writePolarText(makePolarWriteRequest(
        slot, polar.includeCpMin, polar.includeHingeMoment, 1,
        static_cast<int>(slot.points.size()), false, polar.elementSelection));
    // Geometry comparison (xpol.f 55-64): N and DSFRAC <= 1e-5.
    const auto& panel = session_.paneling.panel;
    bool differs = panel.x.size() != slot.geometry.size();
    if (!differs && !panel.s.empty()) {
      const double sizref = panel.s.back() - panel.s.front();
      for (std::size_t i = 0; i < panel.x.size(); ++i) {
        const double dx = panel.x[i] - slot.geometry.x[i];
        const double dy = panel.y[i] - slot.geometry.y[i];
        if (std::sqrt(dx * dx + dy * dy) / sizref > 0.00001) {
          differs = true;
          break;
        }
      }
    }
    if (differs) {
      output_ << " Current airfoil differs from airfoil of stored polar\n"
              << "\n   - - - - - - - - - - - - - - - - - - - - - - - - - - - -"
              << "\n    0  abort polar accumulation"
              << "\n    1  compute with current airfoil"
              << "\n    2  compute with stored  airfoil"
              << " (overwrite current airfoil)\n";
      int iopt = -1;
      if (!askInteger("   Select action", iopt)) {
        sessionEnded_ = true;
        return;
      }
      if (iopt == 0) {
        ipact = 0;
      } else if (iopt == 2) {
        static_cast<void>(
            session::applyArchivedPolarGeometry(session_, ipact - 1));
      }
    }
    if (ipact > 0) {
      output_ << "\n Setting current parameters to those of stored polar\n";
      session::adoptPolarParameters(session_, ipact - 1);
    }
  } else if (ipact > 0) {
    const int created = session::createPolarSlotForAccumulation(session_);
    if (created < 0) {
      output_ << "\n Number of polars is at array limit\n"
              << " New polar will not be stored\n";
      ipact = 0;
    } else {
      ipact = created + 1;
      output_ << "\n Polar" << io::fortranI(3, ipact)
              << " newly created for accumulation\n"
              << " Airfoil archived with polar: "
              << io::fortranA(48, polar.slots[static_cast<std::size_t>(created)]
                                          .metadata.airfoilName)
              << "\n";
    }
  }
  if (ipact <= 0) {
    polar.accumulating = false;
    polar.activeSlot = -1;
    return;
  }
  polar.activeSlot = ipact - 1;
  initPolarSaveFile(ipact - 1);
  if (sessionEnded_) return;
  initPolarDumpFile(ipact - 1);
  if (sessionEnded_) return;
  output_ << "\n Polar accumulation enabled\n";
}

// ---
// OPER file exports (xoper.f 1666-1676, 1892-2346). The handler
// collects only argument/prompt, calls session projection and io encoder
// and writes the bytes. Canonical without success message; OPEN with
// STATUS='UNKNOWN' overwrites without comment. An OPEN error leads in
// Fortran to the runtime abort (no ERR branch) -> here exit code 2.
// ---
bool CommandInterpreter::askExportFilename(const std::string& argument,
                                           const std::string& extension,
                                           std::string& fileName) {
  const std::string trimmed = trimSpaces(argument);
  if (!trimmed.empty()) {
    fileName = trimmed;
    return true;
  }
  if (!session_.geometry.filePrefix.empty()) {
    // Format 1100 (xoper.f 1927/2104): default suggestion from PREFIX.
    // FILDEF is CHARACTER*80, the A editing describes 80 characters.
    const std::string fallback = session_.geometry.filePrefix + extension;
    std::string shown = fallback;
    shown.resize(80, ' ');
    output_ << "\n Enter filename:  " << shown << "\n";
    output_.flush();
    std::string line;
    if (!readInputLine(line)) {
      sessionEnded_ = true;
      return false;
    }
    fileName = trimSpaces(line);
    if (fileName.empty()) {
      fileName = fallback;
    }
    return true;
  }
  if (!askString("Enter filename", fileName)) {
    sessionEnded_ = true;
    return false;
  }
  fileName = trimSpaces(fileName);
  return true;
}

bool CommandInterpreter::writeExportFile(const std::string& fileName,
                                         const std::string& text) {
 // CPWR/DUMP/DMP are subject to the OutputPathPolicy;
  // the transformation happens exactly here at the OPEN point; the prompt
  // text keeps the logical (untransformed) name.
  const std::string effective =
      io::applyOutputPathPolicy(session_.outputPaths, fileName);
  std::ofstream stream(effective, std::ios::binary | std::ios::trunc);
  if (!stream) {
    // Fortran contract: OPEN(...,STATUS='UNKNOWN') without ERR branch ->
    // runtime abort with exit code 2.
    fatalExitCode_ = 2;
    sessionEnded_ = true;
    return false;
  }
  stream << text;
  stream.close();
  return true;
}

void CommandInterpreter::handleOperCpwr(const ParsedCommand& parsed) {
  std::string fileName;
  if (!askExportFilename(parsed.argument, ".cp", fileName)) {
    return;
  }
  const io::PressureDumpInput input =
      session::currentPressureDumpInput(session_);
  std::ignore = writeExportFile(fileName, io::pressureDumpText(input));
}

void CommandInterpreter::handleOperDump(const ParsedCommand& parsed) {
  std::string fileName;
  if (!askExportFilename(parsed.argument, ".bl", fileName)) {
    return;
  }
  const io::BoundaryLayerDumpInput input =
      session::currentBoundaryLayerDumpInput(session_);
  std::ignore = writeExportFile(fileName, io::boundaryLayerDumpText(input));
}

void CommandInterpreter::handleOperDmp(const ParsedCommand& parsed) {
  std::string fileName;
  if (!askExportFilename(parsed.argument, ".bl", fileName)) {
    return;
  }
  const io::BoundaryLayerDumpInput input =
      session::currentBoundaryLayerDumpInput(session_);
  std::ignore =
      writeExportFile(fileName, io::boundaryLayerExtendedDumpText(input));
}

// ---
// RunSummary recording. It only observes the
// existing execution points; texts, prompts and
// fatalExitCode_ paths are unchanged. The runner evaluates exclusively these
// structured quantities.
// ---

void CommandInterpreter::resetRunSummary() {
  summary_ = RunSummary{};
  observedGeometryRevisionKey_ = geometryRevisionKey();
}

void CommandInterpreter::noteCommandExecuted() {
  ++summary_.commandsExecuted;
}

void CommandInterpreter::noteUnknownCommand() {
  ++summary_.unknownCommandCount;
  noteFatalCandidate(FatalKind::unknownCommand);
}

void CommandInterpreter::noteUnavailablePlot() {
  ++summary_.unavailablePlotCount;
  noteFatalCandidate(FatalKind::unavailablePlot);
}

void CommandInterpreter::noteSolverError() {
  ++summary_.solverErrors;
  noteFatalCandidate(FatalKind::solverError);
}

void CommandInterpreter::noteFatalCandidate(FatalKind kind) {
  const std::size_t index = summary_.commandsExecuted;
  auto& slot = summary_.firstOccurrenceByKind[static_cast<std::size_t>(kind)];
  if (!slot.has_value()) {
    slot = index;
  }
  if (summary_.firstFatalKind == FatalKind::none) {
    summary_.firstFatalKind = kind;
    summary_.firstFatalCommandIndex = index;
  }
 // stop-on-error/stop-on-nonconvergence end the
  // job at the first fatal event; the default policy never stops
  // (interactive behavior unchanged).
  const bool stopRequested =
      ((kind == FatalKind::unknownCommand || kind == FatalKind::solverError) &&
       runPolicy_.stopOnError) ||
      (kind == FatalKind::nonConverged && runPolicy_.stopOnNonConvergence) ||
      (kind == FatalKind::unavailablePlot &&
       !runPolicy_.allowUnavailablePlotCommands);
  if (stopRequested) {
    sessionEnded_ = true;
  }
}

void CommandInterpreter::notePointResult(const OperPointResult& result) {
  summary_.pointResults.push_back(result);
  finalizePointTracking();
}

void CommandInterpreter::finalizePointTracking() {
  const std::size_t index = summary_.pointResults.size() - 1;
  const auto& result = summary_.pointResults.back();
  if (!result.converged) {
    ++summary_.nonConvergedPoints;
    noteFatalCandidate(FatalKind::nonConverged);
  }
  const RunSettingsSnapshot snapshot = currentSettingsSnapshot();
  if (summary_.settingsChanges.empty() ||
      !(snapshot == summary_.settingsChanges.back().second)) {
    summary_.settingsChanges.emplace_back(index, snapshot);
  }
}

void CommandInterpreter::noteOperPointStatus(bool analyzed, bool nonConverged,
                                             bool missingGeometry) {
  if (analyzed || nonConverged) {
    notePointResult(makeOperPointResult(session_, session_.result.lastResult,
                                        analyzed));
  } else if (missingGeometry) {
    noteSolverError();
  }
}

void CommandInterpreter::noteSequenceOutcome(bool missingGeometry,
                                             bool pointFailure,
                                             std::size_t pointsBefore) {
  if (missingGeometry || pointFailure) {
    noteSolverError();
  }
  // The handler has already appended the point events in execution order;
  // here only counter and settings bookkeeping happens. The
  // watched settings are constant within a sequence.
  for (std::size_t index = pointsBefore;
       index < summary_.pointResults.size(); ++index) {
    if (!summary_.pointResults[index].converged) {
      ++summary_.nonConvergedPoints;
      noteFatalCandidate(FatalKind::nonConverged);
    }
  }
  if (summary_.pointResults.size() > pointsBefore) {
    const RunSettingsSnapshot snapshot = currentSettingsSnapshot();
    if (summary_.settingsChanges.empty() ||
        !(snapshot == summary_.settingsChanges.back().second)) {
      summary_.settingsChanges.emplace_back(pointsBefore, snapshot);
    }
  }
}

RunSettingsSnapshot CommandInterpreter::currentSettingsSnapshot() const {
  const auto& operating = session_.operating;
  RunSettingsSnapshot snapshot;
  snapshot.re = operating.machReynolds.reinf1;
  snapshot.mach = operating.machReynolds.minf1;
  snapshot.machType = operating.machReynolds.machType;
  snapshot.reType = operating.machReynolds.reType;
  snapshot.viscous = operating.viscous;
  snapshot.maximumIterations = operating.maximumIterations;
  snapshot.ncritTop = operating.criticalReynoldsExponents[0];
  snapshot.ncritBottom = operating.criticalReynoldsExponents[1];
  snapshot.xtripTop = operating.topTripX;
  snapshot.xtripBottom = operating.bottomTripX;
  snapshot.viscousAcceleration = operating.viscousAcceleration;
  snapshot.wakeLength = operating.wakeLength;
  snapshot.idamp = session_.viscous.idamp;
  snapshot.blpar = session_.viscous.blpar;
  return snapshot;
}

std::uint64_t CommandInterpreter::geometryRevisionKey() const {
  std::uint64_t key = session_.geometry.bufferRevision;
  key = key * 1000003ULL + session_.paneling.panelRevision;
  key = key * 1000003ULL + session_.geometry.buffer.x.size();
  key = key * 2ULL + (session_.paneling.panelingValid ? 1U : 0U);
  return key;
}

std::size_t CommandInterpreter::geometryPointCount() const {
  if (!session_.geometry.buffer.x.empty()) {
    return session_.geometry.buffer.x.size();
  }
  return session_.paneling.panel.x.size();
}

void CommandInterpreter::refreshGeometryTracking(const std::string& source) {
  const std::uint64_t key = geometryRevisionKey();
  if (!summary_.geometryRecords.empty() &&
      key == observedGeometryRevisionKey_) {
    return;
  }
  observedGeometryRevisionKey_ = key;
  RunGeometryRecord record;
  record.name = io::stripBlanks(session_.geometry.name);
  record.source = source;
  record.points = geometryPointCount();
  record.pointIndex = summary_.pointResults.size();
  summary_.geometryRecords.push_back(std::move(record));
}

}  // namespace xfoil::app::cli

// End: src/app/cli/CommandInterpreter.cpp

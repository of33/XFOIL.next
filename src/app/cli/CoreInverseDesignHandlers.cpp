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

// File: src/app/cli/CoreInverseDesignHandlers.cpp
// Implementation of the MDES menu handlers.
// See CoreInverseDesignHandlers.h for the contract.
// No numerics here: all state/numeric mutations run via the
// session use cases from SessionOps.h.

#include "CoreInverseDesignHandlers.h"

#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "CliFortranText.h"
#include "CommandParser.h"
#include "NumericArguments.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/io/MdesInverseFileCodec.h"
#include "xfoil/io/OutputPathPolicy.h"
#include "xfoil/session/SessionOps.h"

namespace xfoil::app::cli {

namespace {

namespace session = xfoil::session;
namespace io = xfoil::io;
namespace core = xfoil::core;

constexpr double kMdesDtor = std::numbers::pi / 180.0;

std::string pad4(std::string_view command) {
  std::string field{command};
  field.resize(4, ' ');
  return field;
}

// FORMAT 1150 (xmdes.f 722-723).
void emitQOperatingCondition(std::ostream& out, double alfaRad,
                             double clgam) {
  out << "\n Current Q operating condition:"
      << "   alpha = " << formatFortranF(7, 3, alfaRad / kMdesDtor)
      << "     CL = " << formatFortranF(8, 4, clgam) << "\n";
}

// FORMAT 1190 (xmdes.f 724).
void emitQspecInitialized(std::ostream& out) {
  out << "\n Qspec initialized to current Q\n";
}

// FORMAT 1200/1210 (xmdes.f 726-730).
void emitSpectrumCurrent(std::ostream& out, double algam, double clgam,
                         double cmgam) {
  out << "\n Current :  alpha =" << formatFortranF(9, 4, algam / kMdesDtor)
      << "    CL =" << formatFortranF(11, 6, clgam)
      << "    CM =" << formatFortranF(11, 6, cmgam) << "\n";
}

void emitSpectrumColumn(std::ostream& out, const session::MdesQspecColumnReport& c) {
  out << " Qspec" << std::setw(2) << c.kqsp1
      << " :  alpha =" << formatFortranF(9, 4, c.alfa / kMdesDtor)
      << "    CL =" << formatFortranF(11, 6, c.cl)
      << "    CM =" << formatFortranF(11, 6, c.cm) << "\n";
}

// Prompt (xmdes.f 122-126 ASKC '.MDES^'/'.MDESs^', userio.f 180
// Format 1000: leading line break, ' c> ' without completion).
// FORMAT 1300 (xmdes.f 697-699): MDES EXEC buffer note.
void emitMdesNewBuffer(std::ostream& out) {
  out << "\n New buffer airfoil generated"
      << "\n Execute PANE at Top Level to set new current airfoil\n\n";
}

// MAPGEN/GEOPAR diagnostics of the same EXEC run (xmdes.f 619-628,
// 887-970): iteration lines, non-convergence text, FORMAT 1000
// metrics - before the 1300 success text.
void emitMdesExecDiagnostics(std::ostream& out,
                             const session::MdesExecuteResult& exec) {
  if (exec.mapgen.preLoopLefindFailed) {
    emitZlefindFailed(out);
  }
  for (const auto& it : exec.mapgen.iterations) {
    if (it.lefindFailed) {
      emitZlefindFailed(out);
    }
    emitMapgenIteration(out, it.iteration, it.dcnMax);
  }
  if (!exec.mapgen.converged) {
    emitMapgenNotConverged(out);
  }
  if (exec.metricsValid) {
    emitGeoparMetrics(out, exec.metrics.thick, exec.metrics.thickX,
                      exec.metrics.cambr, exec.metrics.cambrX);
  }
}

void emitMdesPrompt(std::ostream& out, bool symmetry) {
  out << "\n" << (symmetry ? ".MDESs" : ".MDES") << "   c>  ";
}

// Format 1050 (xmdes.f 159-176), byte-exact.
void emitMdesHelp(std::ostream& out) {
  out << "\n   <cr>   Return to Top Level"
      << "\n   !      Redo previous command"
      << "\n\n   INIT   Re-initialize mapping"
      << "\n   QSET   Reset Qspec <== Q"
      << "\n   AQ r.. Show/select alpha(s) for Qspec"
      << "\n   CQ r.. Show/select  CL(s)   for Qspec"
      << "\n\n   Symm   Toggle symmetry flag"
      << "\n   TGAP r Set new TE gap"
      << "\n   TANG r Set new TE angle"
      << "\n\n   Modi   Modify Qspec"
      << "\n   MARK   Mark off target segment for smoothing"
      << "\n   SMOO   Smooth Qspec inside target segment"
      << "\n   Filt   Apply Hanning filter to entire Qspec"
      << "\n   SLOP   Toggle modified-Qspec slope matching flag"
      << "\n\n   eXec   Execute  full-inverse calculation"
      << "\n\n   Visc   Qvis overlay toggle"
      << "\n   REFL   Reflected Qspec overlay toggle"
      << "\n   SPEC   Plot mapping coefficient spectrum"
      << "\n\n   Plot   Replot Qspec (line) and Q (symbols)"
      << "\n   Blow   Blowup plot region"
      << "\n   Rese   Reset plot scale and origin"
      << "\n   Wind     Plot window adjust via cursor and keys"
      << "\n\n   SIZE r Change absolute plot-object size"
      << "\n  .ANNO   Annotate plot"
      << "\n   HARD   Hardcopy current plot"
      << "\n\n   PERT   Perturb one Cn and generate geometry\n";
}

// Format 1100 (xmdes.f 710-711): A4 field.
void emitMdesUnknown(std::ostream& out, const std::string& command) {
  out << " Command " << pad4(command)
      << " not recognized.  Type a \" ? \" for list.\n";
}

// '!' without valid Predecessor (xmdes.f 129-131).
void emitMdesPreviousNotValid(std::ostream& out) {
  out << " Previous .MDES command not valid\n";
}

// Entry without contour (xmdes.f 54-57): two WRITE(*,*).
void emitMdesNoAirfoil(std::ostream& out) {
  out << " \n ***  No airfoil available  ***\n";
}

// ASKR (userio.f 57-76): prompt with ' r> ', blank line keeps
// requirement, invalid number reprompt; EOF aborts in a controlled way.
bool askMdesReal(std::istream& in, std::ostream& out,
                 const std::string& promptText, double& value) {
  for (;;) {
    out << "\n" << promptText << "   r>  ";
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    bool blank = true;
    for (const char ch : line) {
      if (ch != ' ' && ch != '\t') {
        blank = false;
        break;
      }
    }
    if (blank) {
      return true;  // LINE.NE.' '-Test: requirement stays
    }
    const auto parsed = parseRealArguments(line, 80);
    if (parsed.success() && !parsed.values.empty()) {
      value = parsed.values[0];
      return true;
    }
  }
}

// AQ/CQ prompt mode (xmdes.f 224-241/257-274): line with GETFLT;
// invalid input reprompt, empty line leaves everything unchanged.
bool readMdesRealList(std::istream& in, std::ostream& out,
                      const std::string& promptText,
                      std::vector<double>& values, bool& eof) {
  for (;;) {
    out << " " << promptText << "  ";
    std::string line;
    if (!std::getline(in, line)) {
      eof = true;
      return false;
    }
    const auto parsed = parseRealArguments(line, 80);
    if (!parsed.success()) {
      continue;  // ERROR branch reprompt (FORMAT 1161/1172 loop)
    }
    values = parsed.values;
    return true;
  }
}

void emitMdesPlotUnavailable(std::ostream& out) {
  out << "Plotting is not available in this build.\n";
}

// Projection of the session dump rows onto the pure IO record
// (CNDUMP field sequence m, real CN, imag CN, real PIQ(m+1), imag PIQ(m+1)).
std::vector<io::CnDumpRow> toCnDumpRows(
    const std::vector<core::export_data::CircleCoefficientDumpRecord>& rows) {
  std::vector<io::CnDumpRow> out;
  out.reserve(rows.size());
  for (const auto& row : rows) {
    out.push_back(io::CnDumpRow{row.m, row.cnReal, row.cnImag, row.piqReal,
                                row.piqImag});
  }
  return out;
}

}  // namespace

MdesCoreStatus handleMdesCore(session::SessionComposition& session,
                              std::istream& in, std::ostream& out) {
  // Entry (xmdes.f 53-57): N==0 -> no menu.
  const auto entry = session::enterMdesMenuState(session);
  if (entry.status == session::InverseMenuEntryStatus::noAirfoil) {
    emitMdesNoAirfoil(out);
    return MdesCoreStatus::returned;
  }

  // Label 1 (xmdes.f 60-107): numerical initialization path.
  const auto init = session::initializeMdesNumerics(session);
  if (!init.isOk()) {
    session::leaveMdesMenuState(session);
    return MdesCoreStatus::returned;
  }
  // SCINIT diagnostics (xmdes.f 1006-1096) before Format 1150: format 1100
  // TE gap/angle, header, IPASS/max(dw) per pass - only if SCINIT
  // actually ran (cache hit: no lines).
  if (init.scinitRan) {
    emitMdesTeGapLine(out, init.scinit.dxte, init.scinit.dyte,
                      init.scinit.teAngleDeg);
    emitScinitHeader(out);
    for (const auto& it : init.scinit.iterations) {
      emitScinitIteration(out, it.pass, it.maxDelta);
    }
  }
  emitQOperatingCondition(out, session.inverse.shared.algam,
                          session.inverse.shared.clgam);
  if (init.qspecRebuilt) {
    emitQspecInitialized(out);
  }

  // COMOLD/ARGOLD (xmdes.f 44, 114-115): separate MDES redo state.
  std::optional<std::pair<std::string, std::string>> redo =
      session.inverse.mdes.redo;

  for (;;) {
    emitMdesPrompt(out, session.inverse.flags.lqSym);
    std::string line;
    if (!std::getline(in, line)) {
      session.inverse.mdes.redo = redo;
      return MdesCoreStatus::endOfFile;
    }
    ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      // '<Return>' (xmdes.f 139-147): LQSYM=false, menu inactive.
      session::leaveMdesMenuState(session);
      session.inverse.mdes.redo = redo;
      return MdesCoreStatus::returned;
    }

    // '!' redo (xmdes.f 127-136).
    if (parsed.command == "!") {
      if (!redo.has_value()) {
        emitMdesPreviousNotValid(out);
        continue;
      }
      parsed = parseCommandLine(redo->first +
                                (redo->second.empty() ? "" : " " + redo->second));
    }

    const std::string cmd = parsed.command;
    const auto reals = parseRealArguments(parsed.argument);
    const int ninput =
        reals.success() ? static_cast<int>(reals.values.size()) : 0;

    bool eofRequested = false;
    bool redoValid = true;

    if (cmd == "?") {
      emitMdesHelp(out);
    } else if (cmd == "INIT") {
      // xmdes.f 199-202: LQSPEC/LSCINI=false, GO TO 1.
      const auto reinit = session::initializeMdesNumerics(session, true);
      if (reinit.isOk()) {
        if (reinit.scinitRan) {
          emitMdesTeGapLine(out, reinit.scinit.dxte, reinit.scinit.dyte,
                            reinit.scinit.teAngleDeg);
          emitScinitHeader(out);
          for (const auto& it : reinit.scinit.iterations) {
            emitScinitIteration(out, it.pass, it.maxDelta);
          }
        }
        emitQOperatingCondition(out, session.inverse.shared.algam,
                                session.inverse.shared.clgam);
        if (reinit.qspecRebuilt) {
          emitQspecInitialized(out);
        }
      }
    } else if (cmd == "QSET") {
      (void)session::mdesQspecReset(session);
    } else if (cmd == "AQ") {
      std::vector<double> alphas;
      bool applied = false;
      if (ninput >= 1) {
        alphas = reals.values;
        for (double& a : alphas) {
          a *= kMdesDtor;  // xmdes.f 220: ALQSP(K) = RINPUT(K)*DTOR
        }
        applied = true;
      } else {
        emitQOperatingCondition(out, session.inverse.shared.algam,
                                session.inverse.shared.clgam);
        out << "\n Current Qspec alphas  =";
        for (const auto& d : session.inverse.shared.qspecs) {
          out << formatFortranF(9, 3, d.al / kMdesDtor);
        }
        out << "\n";
        bool localEof = false;
        if (readMdesRealList(in, out, "New alphas or <return>:", alphas,
                             localEof)) {
          eofRequested = localEof;
          applied = !alphas.empty();  // empty line: GO TO 500
          for (double& a : alphas) {
            a *= kMdesDtor;
          }
        } else {
          eofRequested = true;
        }
      }
      if (!eofRequested && applied) {
        (void)session::mdesSetQspecAlphas(session, alphas);
      }
    } else if (cmd == "CQ") {
      std::vector<double> cls;
      bool applied = false;
      if (ninput >= 1) {
        cls = reals.values;
        applied = true;
      } else {
        emitQOperatingCondition(out, session.inverse.shared.algam,
                                session.inverse.shared.clgam);
        out << "\n Current Qspec CLs  =";
        for (const auto& d : session.inverse.shared.qspecs) {
          out << formatFortranF(8, 4, d.cl);
        }
        out << "\n";
        bool localEof = false;
        if (readMdesRealList(in, out, "New CLs or <return>:", cls, localEof)) {
          eofRequested = localEof;
          applied = !cls.empty();
        } else {
          eofRequested = true;
        }
      }
      if (!eofRequested && applied) {
        (void)session::mdesSetQspecCls(session, cls);
      }
    } else if (cmd == "SYMM" || cmd == "S") {
      bool newState = false;
      const auto status = session::mdesToggleSymmetry(session, newState);
      if (status == session::MdesCommandStatus::ok) {
        if (newState) {
          out << " Qspec symmetry forcing enabled.\n";
        } else {
          out << " Qspec symmetry forcing disabled.\n";
        }
      }
    } else if (cmd == "TGAP") {
      double dx = 0.0;
      double dy = 0.0;
      bool applied = false;
      if (ninput >= 2) {
        dx = reals.values[0];
        dy = reals.values[1];
        applied = true;
      } else {
        out << "\n Current TE gap  dx/c dy/c ="
            << formatFortranF(7, 4, session.inverse.shared.circle.dzte.real())
            << formatFortranF(7, 4, session.inverse.shared.circle.dzte.imag())
            << "\n";
        dx = session.inverse.shared.circle.dzte.real();
        dy = session.inverse.shared.circle.dzte.imag();
        if (askMdesReal(in, out, "Enter new TE gap dx/c", dx) &&
            askMdesReal(in, out, "Enter new TE gap dy/c", dy)) {
          applied = true;
        } else {
          eofRequested = true;
        }
      }
      if (!eofRequested && applied) {
        (void)session::mdesSetTeGap(session, dx, dy);
      }
    } else if (cmd == "TANG") {
      // AGTSET (xmdes.f 756-770): the inline branch requires NINPUT>=2,
      // but only RINPUT(1) is used - canonical quirk.
      double angle = 0.0;
      bool applied = false;
      if (ninput >= 2) {
        angle = reals.values[0];
        applied = true;
      } else {
        out << "\n Current TE angle ="
            << formatFortranF(7, 3,
                              session.inverse.shared.circle.agte * 180.0)
            << " deg.\n";
        angle = session.inverse.shared.circle.agte * 180.0;
        if (askMdesReal(in, out, "Enter new TE angle (deg)", angle)) {
          applied = true;
        } else {
          eofRequested = true;
        }
      }
      if (!eofRequested && applied) {
        (void)session::mdesSetTeAngle(session, angle);
      }
    } else if (cmd == "FILT" || cmd == "F") {
      session::MdesSpectrumReport report;
      if (session::mdesApplyFilter(session, report) ==
          session::MdesCommandStatus::ok) {
        emitSpectrumCurrent(out, report.algam, report.clgam, report.cmgam);
        for (const auto& column : report.columns) {
          emitSpectrumColumn(out, column);
        }
      }
    } else if (cmd == "SMOO") {
      session::MdesSpectrumReport report;
      const auto status = session::mdesSmoothTargetSegment(session, report);
      if (status == session::MdesCommandStatus::ok) {
        emitSpectrumCurrent(out, report.algam, report.clgam, report.cmgam);
        for (const auto& column : report.columns) {
          emitSpectrumColumn(out, column);
        }
      }
    } else if (cmd == "SLOP") {
      if (session::mdesToggleSlopeMatch(session)) {
        out << " Modified Qspec piece will be made tangent at endpoints\n";
      } else {
        out << " Modified Qspec piece will not be made tangent at endpoints\n";
      }
    } else if (cmd == "VISC" || cmd == "V") {
      if (session::mdesToggleQvisOverlay(session)) {
        out << " Qspec & Qvis will be plotted\n";
      } else {
        out << " Only Qspec will be plotted\n";
      }
    } else if (cmd == "REFL") {
      if (session::mdesToggleReflOverlay(session)) {
        out << " Reflected Qspec will be plotted\n";
      } else {
        out << " Reflected Qspec will not be plotted\n";
      }
    } else if (cmd == "PLOT" || cmd == "P" || cmd == "SPEC" ||
               cmd == "BLOW" || cmd == "B" || cmd == "RESE" ||
               cmd == "R" || cmd == "WIND" || cmd == "W" ||
               cmd == "SIZE" || cmd == "ANNO" || cmd == "HARD" ||
               cmd == "MARK" || cmd == "MODI" || cmd == "M" ||
               cmd == "Z" || cmd == "U") {
      // Plot/cursor commands: recognized-but-unavailable (product rule).
      emitMdesPlotUnavailable(out);
    } else if (cmd == "READ") {
      // xmdes.f 416-435 + GETVOV 1769-1826: GETVOV asks for the filenames
      // always per ASKS (no inline branch). OPEN error (ERR=98):
      // message on the open prompt line, menu continues.
      out << "\nEnter V/Vinf vs s data filename   s>  ";
      std::string fname;
      if (!std::getline(in, fname)) {
        session.inverse.mdes.redo = redo;
        return MdesCoreStatus::endOfFile;
      }
      std::ifstream file(fname, std::ios::binary);
      if (!file) {
        // ERR=98: message, no mutation, READ-after run continues.
        out << " GETVOV: File OPEN error.\n";
      } else {
        const auto data = io::decodeGetvovFile(file);
        file.close();
        if (data.truncatedByParseError) {
          // ERR=99 (xmdes.f 1823-1825): message and RETURN without any
          // mutation; the READ-after run (CNCALC/QSPINT) continues.
          out << " GETVOV: File READ error.\n";
        } else {
          core::export_data::VelocityRatioTable table;
          for (const auto& p : data.points) {
            table.s.push_back(p.s);
            table.vov.push_back(p.vov);
          }
          (void)session::mdesReadVelocityDistribution(session, table);
        }
      }
      session::MdesSpectrumReport report;
      if (session::mdesReadPostProcess(session, report) ==
          session::MdesReadPostStatus::ok) {
        emitSpectrumCurrent(out, report.algam, report.clgam, report.cmgam);
        for (const auto& column : report.columns) {
          emitSpectrumColumn(out, column);
        }
      }
    } else if (cmd == "DUMP") {
      // xmdes.f 605-614 + CNDUMP 1747-1766: inline ARG as filename,
      // otherwise ASKS. OPEN STATUS='UNKNOWN': direct overwrite without
      // query. No success text.
      std::string fname = parsed.argument;
      if (fname.empty()) {
        out << "\nEnter Cn output filename   s>  ";
        if (!std::getline(in, fname)) {
          session.inverse.mdes.redo = redo;
          return MdesCoreStatus::endOfFile;
        }
      }
      session::MdesDumpData dump;
      if (session::currentMdesCnDumpData(session, dump) ==
          session::MdesDumpStatus::available) {
 // MDES DUMP is subject to the
        // OutputPathPolicy (silent overwrite, prompt logical).
        const std::string effective =
            io::applyOutputPathPolicy(session.outputPaths, fname);
        std::ofstream file(effective, std::ios::binary | std::ios::trunc);
        if (file) {
          file << io::cnDumpText(toCnDumpRows(dump.rows));
          file.close();
        }
      }
    } else if (cmd == "EXEC" || cmd == "X") {
      // MDES EXEC/X (xmdes.f 616-699). MAPGEN iterations
      // and plot preparation diagnostics are kernel/plot-internal.
      const auto exec = session::executeMdesDesign(session);
      if (exec.isOk()) {
        emitMdesExecDiagnostics(out, exec);
        emitMdesNewBuffer(out);
      }
    } else if (cmd == "PERT") {
      // PERT (xmdes.f 701-708 + PERT routine): table,
      // READ loop (ERR=10 retry, M<=0 retry, M>NC message+retry),
      // then necessarily the same EXEC path.
      (void)session::mdesPerturbPrepareCoefficients(session);
      const auto& cn = session.inverse.shared.circle.coeff.cn;
      const int nc = session.inverse.shared.circle.grid.nc;
      out << "\n Current mapping coefficients...\n"
          << "       n    Re(Cn)      In the(Cn)\n";
      const int rows = std::min(nc, 32);
      for (int m = 1; m <= rows && m + 1 < static_cast<int>(cn.size()); ++m) {
        out << "    " << std::setw(4) << m
            << formatFortranF(12, 6, cn[static_cast<std::size_t>(m)].real())
            << formatFortranF(12, 6, cn[static_cast<std::size_t>(m)].imag())
            << "\n";
      }
      int mode = 0;
      double dcnr = 0.0;
      double dcni = 0.0;
      for (;;) {
        out << "    Enter  n, delta(Cnr), delta(Cni):  ";
        std::string rl;
        if (!std::getline(in, rl)) {
          redoValid = false;
          break;
        }
        std::istringstream ls(rl);
        if (!(ls >> mode >> dcnr >> dcni)) {
          continue;  // ERR=10: Prompt again
        }
        if (mode <= 0) {
          continue;  // GO TO 10
        }
        if (mode > nc) {
          out << " Max number of modes is" << std::setw(12) << nc << "\n";
          continue;
        }
        break;
      }
      if (redoValid) {
        const auto pert =
            session::mdesPerturbCoefficientAndExecute(session, mode, dcnr,
                                                      dcni);
        // PERT-specific TE gap/chord Newton (xmdes.f PERT 1713-1745):
        // WRITE(*,*) ITERCN, DCNMAX per iteration, for non-convergence
        // after 10 iterations 'TE gap,chord did not converge' — before
        // the EXEC path and with its own separate text.
        if (pert.newton.preLoopLefindFailed) {
          emitZlefindFailed(out);
        }
        for (const auto& it : pert.newton.iterations) {
          if (it.lefindFailed) {
            emitZlefindFailed(out);
          }
          emitMapgenIteration(out, it.iteration, it.dcnMax);
        }
        if (pert.coefficientPerturbed && !pert.newton.converged &&
            pert.newton.iterations.size() == 10) {
          emitPertNotConverged(out);
        }
        if (pert.execute.isOk()) {
          emitMdesExecDiagnostics(out, pert.execute);
          emitMdesNewBuffer(out);
        }
      }
    } else {
      emitMdesUnknown(out, cmd);
      redoValid = false;
    }

    // COMOLD/ARGOLD (xmdes.f 114-115, 712): unknown command sets
    // COMOLD='****' (nullopt), successful command stays redo-capable.
    if (eofRequested) {
      session.inverse.mdes.redo = redoValid ? std::make_pair(parsed.command, parsed.argument) : redo;
      return MdesCoreStatus::endOfFile;
    }
    if (redoValid) {
      redo = std::make_pair(parsed.command, parsed.argument);
    } else {
      redo = std::nullopt;
    }
  }
}

// --- QDES menu (xqdes.f 40-540) ------------------------------------------

namespace {

// Format 1150 (xqdes.f 92-94): two lines plus a completion line.
void emitQdesOperatingCondition(std::ostream& out, double alfaRad,
                                double clgam) {
  out << "\n Current Q operating condition:"
      << "\n alpha = " << formatFortranF(8, 3, alfaRad / kMdesDtor)
      << " deg.      CL = " << formatFortranF(8, 4, clgam) << "\n\n";
}

// FORMAT 1155 (xqdes.f 725).
void emitQdesQspecInitialized(std::ostream& out) {
  out << "\n Qspec initialized to current Q.\n\n";
}

// FORMAT 1200 (xqdes.f 726-727).
void emitQdesForces(std::ostream& out, const session::QdesForceReport& r) {
  out << "\n Q    :   CL =" << formatFortranF(11, 6, r.cl)
      << "    CM =" << formatFortranF(11, 6, r.cm)
      << "\n Qspec:   CL =" << formatFortranF(11, 6, r.clq)
      << "    CM =" << formatFortranF(11, 6, r.cmq) << "\n";
}

void emitQdesPrompt(std::ostream& out) {
  out << "\n.QDES   c>  ";
}

// Format 1050 (xqdes.f 154-172), byte-exact.
void emitQdesHelp(std::ostream& out) {
  out << "\n   <cr>   Return to Top Level"
      << "\n\n   QSET   Reset Qspec <== Q"
      << "\n\n   Modi   Modify Qspec"
      << "\n   MARK   Mark off target segment"
      << "\n   SMOO   Smooth Qspec inside target segment"
      << "\n   SLOP   Toggle modified-Qspec slope matching flag"
      << "\n\n   eXec i Execute mixed-inverse calculation"
      << "\n   REST   Restore geometry from buffer airfoil"
      << "\n   CPXX   CPxx endpoint constraint toggle"
      << "\n\n   Visc   Qvis overlay toggle"
      << "\n   REFL   Reflected Qspec overlay toggle"
      << "\n\n   Plot   Plot Qspec (line) and Q (symbols)"
      << "\n   Blow   Blowup plot region"
      << "\n   Rese   Reset plot scale and origin"
      << "\n   Wind     Plot window adjust via cursor and keys"
      << "\n\n   SIZE r Change absolute plot-object size"
      << "\n  .ANNO   Annotate plot"
      << "\n   HARD   Hardcopy current plot\n";
}

void emitQdesUnknown(std::ostream& out, const std::string& command) {
  out << " Command " << pad4(command)
      << " not recognized.  Type a \" ? \" for list.\n";
}

void emitQdesPreviousNotValid(std::ostream& out) {
  out << " Previous .QDES command not valid\n";
}

// EXEC precondition (xqdes.f 411-414).
void emitQdesMustMark(std::ostream& out) {
  out << " ***  Must mark off target segment first  ***\n";
}

}  // namespace

QdesCoreStatus handleQdesCore(session::SessionComposition& session,
                              std::istream& in, std::ostream& out) {
  const auto entry = session::enterQdesMenuState(session);
  if (entry.status == session::InverseMenuEntryStatus::noAirfoil) {
    emitMdesNoAirfoil(out);
    return QdesCoreStatus::returned;
  }
  const auto init = session::initializeQdesNumerics(session);
  if (!init.isOk()) {
    session::leaveQdesMenuState(session);
    return QdesCoreStatus::returned;
  }
  // Chronological point diagnostics of the executed SPECAL run:
  // GGCALC header only for an actual ggCalc run. No VISCAL and no
  // project-specific OFS output.
  for (const auto& ev : init.pointEvents) {
    if (ev.kind == session::OperPointEventKind::influenceSystemCalculation) {
      emitGgCalcHeader(out);
    }
  }
  emitQdesOperatingCondition(out, session.inverse.shared.algam,
                             session.inverse.shared.clgam);
  if (init.qspecRebuilt) {
    emitQdesQspecInitialized(out);
  }

  std::optional<std::pair<std::string, std::string>> redo =
      session.inverse.qdes.redo;
  int niterq = 20;  // QDES-local NITERQ persistence (ASKI requirement)

  for (;;) {
    emitQdesPrompt(out);
    std::string line;
    if (!std::getline(in, line)) {
      session.inverse.qdes.redo = redo;
      return QdesCoreStatus::endOfFile;
    }
    ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      session::leaveQdesMenuState(session);
      session.inverse.qdes.redo = redo;
      return QdesCoreStatus::returned;
    }
    if (parsed.command == "!") {
      if (!redo.has_value()) {
        emitQdesPreviousNotValid(out);
        continue;
      }
      parsed = parseCommandLine(redo->first +
                                (redo->second.empty() ? "" : " " + redo->second));
    }

    const std::string cmd = parsed.command;
    bool redoValid = true;

    if (cmd == "?") {
      emitQdesHelp(out);
    } else if (cmd == "QSET") {
      (void)session::qdesQspecReset(session);
    } else if (cmd == "SMOO") {
      session::QdesForceReport report;
      if (session::qdesSmoothTargetSegment(session, report) ==
          session::QdesCommandStatus::ok) {
        emitQdesForces(out, report);
      }
    } else if (cmd == "VISC" || cmd == "V") {
      if (session::mdesToggleQvisOverlay(session)) {
        out << " Qspec & Qvis will be plotted\n";
      } else {
        out << " Only Qspec will be plotted\n";
      }
    } else if (cmd == "REFL") {
      if (session::mdesToggleReflOverlay(session)) {
        out << " Reflected Qspec will be plotted\n";
      } else {
        out << " Reflected Qspec will not be plotted\n";
      }
    } else if (cmd == "SLOP") {
      if (session::mdesToggleSlopeMatch(session)) {
        out << " Modified Qspec piece will be made tangent at endpoints\n";
      } else {
        out << " Modified Qspec piece will not be made tangent at endpoints\n";
      }
    } else if (cmd == "CPXX") {
      if (session::qdesToggleCpxxConstraint(session)) {
        out << " CPxx will be constrained\n";
      } else {
        out << " CPxx will not be constrained\n";
      }
    } else if (cmd == "EXEC" || cmd == "X") {
      // QDES EXEC/X (xqdes.f 409-505). Guard without LIQSET:
      // canonical refusal without mutation. With an already set
      // target segment (direct session test/harness only) MIXED runs.
      if (!session.inverse.shared.target.liqset) {
        emitQdesMustMark(out);
      } else {
        int iters = niterq;
        bool haveInline = false;
        {
          std::istringstream ls(parsed.argument);
          ls >> iters;
          haveInline = static_cast<bool>(ls) && ls.peek() == EOF;
        }
        if (!haveInline) {
          out << "Enter max number of iterations: ";
          std::string rl;
          if (std::getline(in, rl)) {
            std::istringstream ls(rl);
            ls >> iters;
          }
        }
        niterq = iters;
        out << "\n Current airfoil saved in buffer airfoil\n";
        const auto exec = session::executeQdesMixedDesign(session, iters);
        switch (exec.status) {
          case session::QdesExecuteStatus::stagnationInTarget:
            out << "\n Target segment cannot include stagnation point in "
                   "mixed-inverse.\n";
            break;
          case session::QdesExecuteStatus::ok:
            if (exec.converged) {
              out << "\n New current airfoil generated"
                  << "\n Old buffer  airfoil unchanged\n";
            } else {
              out << " Not quite converged.  Can EXEC again if "
                     "necessary.\n";
            }
            break;
          case session::QdesExecuteStatus::targetNotMarked:
          case session::QdesExecuteStatus::notInitialized:
          case session::QdesExecuteStatus::coreError:
            break;
        }
      }
    } else if (cmd == "REST") {
      // QDES REST (xqdes.f 508-530): silent
      // current commit from the buffer; LSCINI stays unchanged.
      const auto rest = session::restoreQdesCurrentFromBuffer(session);
      if (rest.leadingEdgeNotFound) {
        emitLefindNotFound(out);
      }
    } else if (cmd == "ANNO") {
      // xqdes.f 381-387: without a plot backend LPLOT is always .FALSE. ->
      // canonical ELSE branch.
      out << " No active plot to annotate\n";
    } else if (cmd == "PLOT" || cmd == "P" || cmd == "BLOW" ||
               cmd == "B" || cmd == "RESE" || cmd == "R" ||
               cmd == "WIND" || cmd == "W" || cmd == "SIZE" ||
               cmd == "HARD" || cmd == "MARK" || cmd == "MODI" ||
               cmd == "M" || cmd == "Z" || cmd == "U") {
      emitMdesPlotUnavailable(out);
    } else {
      emitQdesUnknown(out, cmd);
      redoValid = false;
    }

    if (redoValid) {
      redo = std::make_pair(parsed.command, parsed.argument);
    } else {
      redo = std::nullopt;
    }
  }
}

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreInverseDesignHandlers.cpp

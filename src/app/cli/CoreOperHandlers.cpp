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

// File: src/app/cli/CoreOperHandlers.cpp
// Implementation of the OPER
// operating-state handlers. The handlers validate or collect inputs before
// the session mutation, call only SessionOps or confirmed core kernels,
// and format the visible text via CliFortranText. They start
// no analysis.

#include "CoreOperHandlers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <fstream>
#include <numbers>
#include <istream>
#include <limits>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "CliFortranText.h"
#include "NumericArguments.h"
#include "xfoil/core/boundary_layer/BlForceReport.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/polar/PolarOps.h"
#include "xfoil/io/DumpCodec.h"
#include "xfoil/io/OutputPathPolicy.h"
#include "xfoil/io/TextFormat.h"
#include "xfoil/session/SessionOps.h"

namespace xfoil::app::cli {

// Forward declaration of the internal PLRADD storage (defined at end of file).
std::string accumulateOperPointRecord(session::SessionComposition& session,
                                      bool pointConverged);

namespace {

bool isBlankLine(const std::string& line) {
  for (char ch : line) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

bool askRealCore(std::istream& in, std::ostream& out,
                 const std::string& prompt, double& value) {
  // ASKR (userio.f 49-71): a '^' in the prompt marks the input position
  // and is truncated before output (Index(Prompt,'^')-1).
  const std::size_t caret = prompt.find('^');
  const std::string shown =
      (caret == std::string::npos) ? prompt : prompt.substr(0, caret);
  while (true) {
    out << "\n" << shown << "   r>  ";
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    if (isBlankLine(line)) {
      return true;
    }
    std::istringstream stream(line);
    double parsed = 0.0;
    if (stream >> parsed) {
      value = parsed;
      return true;
    }
  }
}

bool parseFortranListInteger(const std::string& line, int& value) {
  std::istringstream stream(line);
  std::string token;
  if (!(stream >> token)) {
    return false;
  }
  while (!token.empty() && token.back() == ',') {
    token.pop_back();
  }
  std::size_t pos = 0;
  if (!token.empty() && (token[0] == '+' || token[0] == '-')) {
    pos = 1;
  }
  if (pos >= token.size()) {
    return false;
  }
  for (; pos < token.size(); ++pos) {
    if (!std::isdigit(static_cast<unsigned char>(token[pos]))) {
      return false;
    }
  }
  try {
    const long long parsed = std::stoll(token);
    if (parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
      return false;
    }
    value = static_cast<int>(parsed);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool askIntegerCore(std::istream& in, std::ostream& out,
                    const std::string& prompt, int& value) {
  while (true) {
    out << "\n" << prompt << "   i>  ";
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    if (isBlankLine(line)) {
      return true;
    }
    int parsed = 0;
    if (parseFortranListInteger(line, parsed)) {
      value = parsed;
      return true;
    }
  }
}

std::vector<double> inlineRealValues(std::string_view argument) {
  const NumericArgumentResult<double> parsed = parseRealArguments(argument);
  if (!parsed.success()) {
    // GETFLT sets N=0 and ERROR=.TRUE. on a read error; OPER does not
    // check ERROR and returns to the prompt/menu path.
    return {};
  }
  return parsed.values;
}

// xoper.f 115-121: OPER reads inline numbers as real and forms IINPUT from them.
// Values with absolute value above 2.1E9 are set to 2**30.
int toFortranInt(double value) {
  if (!(value > -2.1e9 && value < 2.1e9)) {
    return 1 << 30;
  }
  return static_cast<int>(value);
}

}  // namespace

OperViscCoreStatus handleOperViscCore(
    session::SessionComposition& session, std::istream& in, std::ostream& out,
    std::string_view argument, bool polarAccumulationActive) {
  if (polarAccumulationActive) {
    emitPolarAccumulationBlocked(out);
    return OperViscCoreStatus::polarAccumulationBlocked;
  }

  const std::vector<double> values = inlineRealValues(argument);

  if (session.operating.viscous) {
    session::setViscousMode(session, false);
    return OperViscCoreStatus::toggledOff;
  }

  double reinf1 = session.operating.machReynolds.reinf1;
  if (!values.empty()) {
    reinf1 = values[0];
  } else if (reinf1 == 0.0) {
    if (!askRealCore(in, out, "Enter Reynolds number", reinf1)) {
      return OperViscCoreStatus::inputAborted;
    }
  }

  session::setReynoldsBaseNumber(session, reinf1);
  session::setViscousMode(session, true);
  emitMrShow(out, true, true, session.operating.machReynolds.machType,
             session.operating.machReynolds.minf1,
             session.operating.machReynolds.reType,
             session.operating.machReynolds.reinf1);
  return OperViscCoreStatus::toggledOn;
}

OperReCoreStatus handleOperReCore(session::SessionComposition& session,
                                  std::istream& in, std::ostream& out,
                                  std::string_view argument,
                                  bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return OperReCoreStatus::polarAccumulationBlocked;
  }

  const std::vector<double> values = inlineRealValues(argument);
  double reinf1 = session.operating.machReynolds.reinf1;

  if (values.empty()) {
    out << "\n";
    emitCurrently(out);
    emitMrShow(out, false, true, session.operating.machReynolds.machType,
               session.operating.machReynolds.minf1,
               session.operating.machReynolds.reType,
               session.operating.machReynolds.reinf1);
    if (!askRealCore(in, out, "Enter new Reynolds number", reinf1)) {
      return OperReCoreStatus::inputAborted;
    }
  } else {
    reinf1 = values[0];
  }

  session::setReynoldsNumber(session, reinf1);
  return OperReCoreStatus::applied;
}

OperMachCoreStatus handleOperMachCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument,
                                      bool polarAccumulationActive) {
  if (polarAccumulationActive) {
    emitPolarAccumulationBlocked(out);
    return OperMachCoreStatus::polarAccumulationBlocked;
  }

  std::vector<double> values = inlineRealValues(argument);
  double minf1 = session.operating.machReynolds.minf1;
  bool haveValue = false;

  while (!haveValue) {
    if (!values.empty()) {
      minf1 = values[0];
      if (minf1 >= 1.0) {
        emitSupersonicFreestreamNotAllowed(out);
        values.clear();
        continue;
      }
      haveValue = true;
    } else {
      out << "\n";
      emitCurrently(out);
      emitMrShow(out, true, false, session.operating.machReynolds.machType,
                 minf1, session.operating.machReynolds.reType,
                 session.operating.machReynolds.reinf1);
      if (!askRealCore(in, out, "Enter Mach number", minf1)) {
        return OperMachCoreStatus::inputAborted;
      }
      if (minf1 >= 1.0) {
        emitSupersonicFreestreamNotAllowed(out);
        continue;
      }
      haveValue = true;
    }
  }

  session::setMachNumber(session, minf1);

  const double minf = session.viscous.viscals.minf;
  if (minf > 0.0) {
    const core::inviscid::CompressibilityState comp =
        core::inviscid::comset(minf, session.operating.qinf);
    const double qinf = session.operating.qinf;
    emitSonicCp(out, comp.cpstar, qinf == 0.0 ? 0.0 : comp.qstar / qinf);
  }

  // Fortran Mach runs CPCALC/CLCALC/CDCALC immediately after MRCL/COMSET on
  // the preserved point (xoper.f 328-333), without a new solver run. The
  // session models this as post-processing of the current valid point;
  // without a valid point lastResult stays unchanged (no invented
  // values). No SPECAL/SPECCL/VISCAL.
  session::refreshExistingPointAfterMachChange(session);

  return OperMachCoreStatus::applied;
}

OperTypeCoreStatus handleOperTypeCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument,
                                      bool polarAccumulationActive) {
  if (polarAccumulationActive) {
    emitPolarAccumulationBlocked(out);
    return OperTypeCoreStatus::polarAccumulationBlocked;
  }

  std::vector<double> values = inlineRealValues(argument);
  int ityp = 0;
  bool haveValue = false;

  while (!haveValue) {
    if (!values.empty()) {
      ityp = toFortranInt(values[0]);
      if (ityp < 1 || ityp > 3) {
        values.clear();
        continue;
      }
      haveValue = true;
    } else {
      emitTypeTable(out);
      if (!askIntegerCore(in, out, "Enter type of Mach,Re variation with CL",
                          ityp)) {
        return OperTypeCoreStatus::inputAborted;
      }
      if (ityp < 1 || ityp > 3) {
        continue;
      }
      haveValue = true;
    }
  }

  int machType = 1;
  int reType = 1;
  switch (ityp) {
    case 2:
      machType = 2;
      reType = 2;
      break;
    case 3:
      machType = 1;
      reType = 3;
      break;
    default:
      machType = 1;
      reType = 1;
      break;
  }

  session::setMachReynoldsTypes(session, machType, reType);
  emitMrShow(out, true, true, machType, session.operating.machReynolds.minf1,
             reType, session.operating.machReynolds.reinf1);
  return OperTypeCoreStatus::applied;
}

OperIterCoreStatus handleOperIterCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument) {
  std::vector<double> values = inlineRealValues(argument);
  int itmax = session.operating.maximumIterations;
  int displayedLimit = itmax;
  bool haveValue = false;

  while (!haveValue) {
    if (!values.empty()) {
      itmax = toFortranInt(values[0]);
      if (itmax < 1) {
        displayedLimit = itmax;
        values.clear();
        continue;
      }
      haveValue = true;
    } else {
      emitCurrentIterationLimit(out, displayedLimit);
      itmax = displayedLimit;
      if (!askIntegerCore(in, out, "Enter new iteration limit", itmax)) {
        return OperIterCoreStatus::inputAborted;
      }
      displayedLimit = itmax;
      if (itmax < 1) {
        continue;
      }
      haveValue = true;
    }
  }

  session::setIterationLimit(session, itmax);
  return OperIterCoreStatus::applied;
}

namespace {

// Canonical order of the numeric output blocks of an
// OPER single point, taken exclusively from the chronological
// event stream of the point core (GGCALC, OFS format 333, XYWAKE,
// QDCALC, VISCAL header, iterations format 2000/2010/2020,
// completion format 9922, non-convergence, note). No
// prediction from pre-state flags; each event is captured at its
// actual execution site.
void emitOperPointText(std::ostream& out, bool viscous,
                       const session::OperPointDiagnostics& diag,
                       bool polarAccumulationActive,
                       bool continueHintActive = true) {
  using session::OperPointEventKind;
  using core::boundary_layer::ViscousEventKind;
  using core::boundary_layer::ViscousEventPhase;

  std::size_t pos = 0;
  // Point and viscous prerequisite events before the VISCAL header:
  // GGCALC, OFS marker, XYWAKE, QDCALC (preHead).
  for (; pos < diag.events.size(); ++pos) {
    const auto& e = diag.events[pos];
    if (e.kind == OperPointEventKind::viscous &&
        e.viscous.phase != ViscousEventPhase::preHead) {
      break;
    }
    switch (e.kind) {
      case OperPointEventKind::influenceSystemCalculation:
        emitGgcalcProgress(out);
        break;
      case OperPointEventKind::specifiedPointOutput:
        emitOfsSpecial(out, toDegrees(Radians{diag.specalSnapshot.alpha}).value,
                       diag.specalSnapshot.cl, diag.specalSnapshot.cdp,
                       diag.specalSnapshot.cm);
        break;
      case OperPointEventKind::wakeTrajectoryCalculation:
        emitXywakeProgress(out);
        break;
      case OperPointEventKind::specalConvergenceFailure:
        emitSpecalConvergenceFailed(out);
        break;
      case OperPointEventKind::specclConvergenceFailure:
        emitSpecclConvergenceFailed(out);
        break;
      case OperPointEventKind::viscous:
        if (e.viscous.kind == ViscousEventKind::sourceInfluenceCalculation) {
          emitQdcalcProgress(out);
        } else if (e.viscous.kind ==
                   ViscousEventKind::stagnationPointNotFound) {
          emitStfindStagnationNotFound(out);
        }
        break;
    }
  }
  if (!viscous) {
    return;
  }
  emitSolvingBlSystem(out);
  // Canonical BL events per iteration: only the init block and the
  // MRCHUE messages (phase initMarch), then the unconditional blank line
  // from xbl.f:90, then MRCHDU messages and transition lines (phase
  // postMarch), only afterwards the iteration lines format 2000/2010/2020
  // (whose leading '/' the next blank line produces).
  std::size_t evPos = pos;
  int iter = 0;
  for (const auto& rec : diag.iterations) {
    ++iter;
    const int it0 = iter - 1;
    while (evPos < diag.events.size() &&
           diag.events[evPos].kind == OperPointEventKind::viscous &&
           diag.events[evPos].viscous.iteration == it0 &&
           diag.events[evPos].viscous.phase == ViscousEventPhase::initMarch) {
      emitViscousEvent(out, diag.events[evPos].viscous);
      ++evPos;
    }
    out << "\n";  // Fortran: WRITE(*,*) Blank line (xbl.f:90)
    while (evPos < diag.events.size() &&
           diag.events[evPos].kind == OperPointEventKind::viscous &&
           diag.events[evPos].viscous.iteration == it0 &&
           diag.events[evPos].viscous.phase == ViscousEventPhase::postMarch) {
      emitViscousEvent(out, diag.events[evPos].viscous);
      ++evPos;
    }
    const double alphaDeg = toDegrees(Radians{rec.alfa}).value;
    if (rec.rlx < 1.0) {
      emitViscalIterationWithRelaxation(out, iter, rec.rmsbl, rec.rmxbl,
                                        rec.vmxbl, rec.imxbl + 1,
                                        rec.ismxbl + 1, rec.rlx);
    } else {
      emitViscalIterationWithoutRelaxation(out, iter, rec.rmsbl, rec.rmxbl,
                                           rec.vmxbl, rec.imxbl + 1,
                                           rec.ismxbl + 1);
    }
    emitViscalIterationLoads(out, alphaDeg, rec.cl, rec.cm, rec.cd, rec.cdf);
  }
  if (!diag.viscalConverged) {
    emitViscalConvergenceFailed(out);
  }
  if (diag.completionValid) {
    emitViscalCompletion(out, diag.completion.acrit, diag.completion.hkmax,
                         diag.completion.cd, diag.completion.psep,
                         diag.completion.patt, diag.completion.delp,
                         diag.completion.xoctr);
  }
  if (!polarAccumulationActive && continueHintActive &&
      !diag.viscalConverged) {
    emitContinueIteratingHint(out);
  }
}

}  // namespace

OperAlphaCoreStatus handleOperAlphaCore(
    session::SessionComposition& session, std::istream& in, std::ostream& out,
    std::string_view argument, bool polarAccumulationActive) {
  if (!session::panelingValid(session)) {
    // C++ session contract: without a valid panel contour no analysis and no
    // update of the session numeric state. Fortran never reaches Alfa/CLI/Cl
    // without airfoil (OPER aborts with 'No airfoil available').
    return OperAlphaCoreStatus::missingGeometry;
  }

  std::vector<double> values = inlineRealValues(argument);
  // xoper.f 421-422: requirement is the current COMMON alpha (CR09). The
  // session maintains alpha in viscous.viscals.alfa; lastResult.alpha is
  // invalidated on geometry/panel change, but alpha stays preserved
  // (xfoil.f 2078-2089 sets only flags).
  double adeg = toDegrees(Radians{session.viscous.viscals.alfa}).value;
  if (values.empty()) {
    // xoper.f 421-422: requirement is the current alpha; a blank line keeps
    // it (ASKR leaves RINPUT unchanged).
    if (!askRealCore(in, out, "Enter angle of attack (deg)", adeg)) {
      return OperAlphaCoreStatus::inputAborted;
    }
  } else {
    adeg = values[0];
  }

  const bool viscous = session.operating.viscous;
  const AnalysisResult result =
      session::runOperAlphaPoint(session, Degrees{adeg}, nullptr);
  if (result.status == AnalysisStatus::missingGeometry) {
    return OperAlphaCoreStatus::missingGeometry;
  }
  emitOperPointText(out, viscous, session.result.lastDiagnostics,
                    polarAccumulationActive);
  if (viscous) {
    return result.converged ? OperAlphaCoreStatus::analyzed
                            : OperAlphaCoreStatus::nonConverged;
  }
  return OperAlphaCoreStatus::analyzed;
}

OperCliCoreStatus handleOperCliCore(session::SessionComposition& session,
                                    std::istream& in, std::ostream& out,
                                    std::string_view argument,
                                    bool polarAccumulationActive) {
  if (!session::panelingValid(session)) {
    // C++ session contract: without a valid panel contour no analysis and no
    // update of the session numeric state. Fortran never reaches Alfa/CLI/Cl
    // without airfoil (OPER aborts with 'No airfoil available').
    return OperCliCoreStatus::missingGeometry;
  }

  std::vector<double> values = inlineRealValues(argument);
  double clspec = session.viscous.lastClspec;
  if (values.empty()) {
    // xoper.f 480-481: Requirement is the retained CLSPEC.
    if (!askRealCore(in, out, "Enter inviscid lift coefficient", clspec)) {
      return OperCliCoreStatus::inputAborted;
    }
  } else {
    clspec = values[0];
  }

  const bool viscous = session.operating.viscous;
  // XYWAKE/QDCALC lines cannot be determined before SPECCL; they
  // derive exclusively from the event stream of the use cases.
  const AnalysisResult result =
      session::runOperCliPoint(session, clspec, nullptr);
  if (result.status == AnalysisStatus::missingGeometry) {
    return OperCliCoreStatus::missingGeometry;
  }
  emitOperPointText(out, viscous, session.result.lastDiagnostics,
                    polarAccumulationActive);
  if (viscous) {
    return result.converged ? OperCliCoreStatus::analyzed
                            : OperCliCoreStatus::nonConverged;
  }
  return OperCliCoreStatus::analyzed;
}

OperClCoreStatus handleOperClCore(session::SessionComposition& session,
                                  std::istream& in, std::ostream& out,
                                  std::string_view argument,
                                  bool polarAccumulationActive) {
  if (!session::panelingValid(session)) {
    // C++ session contract: without a valid panel contour no analysis and no
    // update of the session numeric state. Fortran never reaches Alfa/CLI/Cl
    // without airfoil (OPER aborts with 'No airfoil available').
    return OperClCoreStatus::missingGeometry;
  }

  std::vector<double> values = inlineRealValues(argument);
  double clspec = session.viscous.lastClspec;
  if (values.empty()) {
    // xoper.f 515-516: Requirement is the retained CLSPEC.
    if (!askRealCore(in, out, "Enter lift coefficient", clspec)) {
      return OperClCoreStatus::inputAborted;
    }
  } else {
    clspec = values[0];
  }

  const bool viscous = session.operating.viscous;
  // XYWAKE/QDCALC lines cannot be determined before SPECCL; they
  // derive exclusively from the event stream of the use cases.
  const AnalysisResult result =
      session::runOperClPoint(session, clspec, nullptr);
  if (result.status == AnalysisStatus::missingGeometry) {
    return OperClCoreStatus::missingGeometry;
  }
  emitOperPointText(out, viscous, session.result.lastDiagnostics,
                    polarAccumulationActive);
  if (viscous) {
    return result.converged ? OperClCoreStatus::analyzed
                            : OperClCoreStatus::nonConverged;
  }
  return OperClCoreStatus::analyzed;
}

// OPER INIT (xoper.f 394-401): exclusively the confirmed
// session operation (toggleBoundaryLayerInitialization, identical to
// VPAR INIT xoper.f 2649-2653); no solver, no field mutation,
// COMOLD/ARGOLD and thus the '!' continuation remain untouched.
OperInitCoreStatus handleOperInitCore(session::SessionComposition& session,
                                      std::ostream& out) {
  if (!session::panelingValid(session)) {
    return OperInitCoreStatus::missingGeometry;
  }
  session::toggleBoundaryLayerInitialization(session);
  if (session.viscous.viscals.lblini) {
    emitVparInitInitialized(out);
    return OperInitCoreStatus::initializedAssumed;
  }
  emitVparInitWillInitialize(out);
  return OperInitCoreStatus::willInitialize;
}

// '!' (xoper.f 85-92): repeat dispatch with LRECALC. No parser routing;
// the point requirement is not solved again, SPECAL/SPECCL omitted.
OperBangCoreStatus handleOperBangCore(session::SessionComposition& session,
                                      std::ostream& out,
                                      bool polarAccumulationActive) {
  using session::OperPointOrigin;
  const OperPointOrigin origin = session.viscous.lastPointOrigin;
  if (origin == OperPointOrigin::none) {
    // COMOLD='****' (xoper.f 86-88).
    emitPreviousOperCommandNotValid(out);
    return OperBangCoreStatus::noPreviousCommand;
  }
  if (!session.operating.viscous) {
    // Inviscid: the repeated branch skips the point requirement because
    // of LRECALC, LVISC=false causes VISCAL to be omitted -> no text
    // (binary `t_bang_inviscid`).
    return OperBangCoreStatus::noRepeatOutput;
  }
  if (!session.viscous.lastPointContinuable) {
    // Continuation state unusable (e.g. geometry/panel change).
    return OperBangCoreStatus::notContinuable;
  }
  const AnalysisResult result = session::continueViscousPoint(session, nullptr);
  emitOperPointText(out, true, session.result.lastDiagnostics,
                    polarAccumulationActive);
  return result.converged ? OperBangCoreStatus::continuedConverged
                          : OperBangCoreStatus::continuedNonConverged;
}

OperAseqCoreStatus handleOperAseqCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument,
                                      bool polarAccumulationActive,
                                      std::vector<OperPointResult>*
                                          pointResults) {
  if (!session::panelingValid(session)) {
    return OperAseqCoreStatus::missingGeometry;
  }
  const std::vector<double> values = inlineRealValues(argument);
  double first = session.operating.aseqFirstDeg;
  double last = session.operating.aseqLastDeg;
  double step = session.operating.aseqStepDeg;
  if (values.size() >= 3) {
    first = values[0];
    last = values[1];
    step = values[2];
  } else {
    if (values.size() >= 1) {
      first = values[0];
    } else if (!askRealCore(in, out, "Enter first alfa value (deg)", first)) {
      return OperAseqCoreStatus::inputAborted;
    }
    if (values.size() >= 2) {
      last = values[1];
    } else if (!askRealCore(in, out, "Enter last  alfa value (deg)", last)) {
      return OperAseqCoreStatus::inputAborted;
    }
    if (!askRealCore(in, out, "Enter alfa increment   (deg)", step)) {
      return OperAseqCoreStatus::inputAborted;
    }
  }
  // Fortran-local, also preserved on halt (next ASEQ prompt).
  session.operating.aseqFirstDeg = first;
  session.operating.aseqLastDeg = last;
  session.operating.aseqStepDeg = step;

  const bool viscous = session.operating.viscous;
  // PLRADD position (xoper.f 440-447): storage runs within the
  // point loop on the state of the respective point; the associated
  // messages are output only after the point text.
  std::deque<std::string> polarMessages;
  const session::OperSequenceRun run = session::runOperAlphaSequenceDetailed(
      session, Degrees{first}, Degrees{last}, Degrees{step}, nullptr,
      [&](const AnalysisResult& point) {
        if (pointResults != nullptr) {
          pointResults->push_back(
              makeOperPointResult(session, point, point.converged));
        }
        polarMessages.push_back(
            accumulateOperPointRecord(session, point.converged));
      });
  for (const auto& rec : run.points) {
    // The OPER sequence canonically writes no continuation hint
    // (xoper.f 664-755 contains no corresponding WRITE).
    emitOperPointText(out, viscous, rec.diagnostics, polarAccumulationActive,
                      /*continueHintActive=*/false);
    if (!polarMessages.empty()) {
      const std::string msg = std::move(polarMessages.front());
      polarMessages.pop_front();
      if (!msg.empty()) {
        out << msg;
      }
    }
  }
  if (run.stopReason ==
      session::OperSequenceStopReason::consecutiveNonConvergenceLimit) {
    emitSequenceHalted(out, static_cast<int>(run.iseqexAtStop),
                       run.lastConvergedAlphaDeg, run.lastConvergedCl);
    return OperAseqCoreStatus::haltedNonConvergence;
  }
  if (run.stopReason == session::OperSequenceStopReason::pointFailure) {
    return OperAseqCoreStatus::pointFailure;
  }
  return OperAseqCoreStatus::completed;
}

OperCseqCoreStatus handleOperCseqCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument,
                                      bool polarAccumulationActive,
                                      std::vector<OperPointResult>*
                                          pointResults) {
  if (!session::panelingValid(session)) {
    return OperCseqCoreStatus::missingGeometry;
  }
  const std::vector<double> values = inlineRealValues(argument);
  double first = session.operating.cseqFirstCl;
  double last = session.operating.cseqLastCl;
  double step = session.operating.cseqStepCl;
  if (values.size() >= 3) {
    first = values[0];
    last = values[1];
    step = values[2];
  } else {
    if (values.size() >= 1) {
      first = values[0];
    } else if (!askRealCore(in, out, "Enter first CL value", first)) {
      return OperCseqCoreStatus::inputAborted;
    }
    if (values.size() >= 2) {
      last = values[1];
    } else if (!askRealCore(in, out, "Enter last  CL value", last)) {
      return OperCseqCoreStatus::inputAborted;
    }
    if (!askRealCore(in, out, "Enter CL increment", step)) {
      return OperCseqCoreStatus::inputAborted;
    }
  }
  session.operating.cseqFirstCl = first;
  session.operating.cseqLastCl = last;
  session.operating.cseqStepCl = step;

  const bool viscous = session.operating.viscous;
  // PLRADD position (xoper.f 440-447), as at the ASEQ path.
  std::deque<std::string> polarMessages;
  const session::OperSequenceRun run = session::runOperClSequenceDetailed(
      session, first, last, step, session.result.lastResult.alpha, nullptr,
      [&](const AnalysisResult& point) {
        if (pointResults != nullptr) {
          pointResults->push_back(
              makeOperPointResult(session, point, point.converged));
        }
        polarMessages.push_back(
            accumulateOperPointRecord(session, point.converged));
      });
  for (const auto& rec : run.points) {
    // The OPER sequence canonically writes no continuation hint
    // (xoper.f 664-755 contains no corresponding WRITE).
    emitOperPointText(out, viscous, rec.diagnostics, polarAccumulationActive,
                      /*continueHintActive=*/false);
    if (!polarMessages.empty()) {
      const std::string msg = std::move(polarMessages.front());
      polarMessages.pop_front();
      if (!msg.empty()) {
        out << msg;
      }
    }
  }
  if (run.stopReason ==
      session::OperSequenceStopReason::consecutiveNonConvergenceLimit) {
    emitSequenceHalted(out, static_cast<int>(run.iseqexAtStop),
                       run.lastConvergedAlphaDeg, run.lastConvergedCl);
    return OperCseqCoreStatus::haltedNonConvergence;
  }
  if (run.stopReason == session::OperSequenceStopReason::pointFailure) {
    return OperCseqCoreStatus::pointFailure;
  }
  return OperCseqCoreStatus::completed;
}

// OPER Name (xoper.f 1725-1733): identical to the TOP-level name path
// (xfoil.f 344-350) on the same canonical name field. With non-empty
// inline argument: Name = COMARG, STRIP, silently. Without argument:
// NAMMOD(Name,0,-1) always writes prompt + READ. Empty answer applies
// the NAMDEF requirement, non-empty answer overwrites it.
OperNameCoreStatus handleOperNameCore(session::SessionComposition& session,
                                      std::ostream& out,
                                      std::string_view argument,
                                      bool hasArgument,
                                      std::string_view answer,
                                      bool hasAnswer) {
  if (hasArgument && !io::stripBlanks(argument).empty()) {
    session::setGeometryName(session, std::string(argument));
    return OperNameCoreStatus::applied;
  }
  const std::string nameDefault =
      session::geometryNameVersionDefault(session, 0, -1);
  if (!hasAnswer) {
    emitNamePrompt(out, nameDefault);
    return OperNameCoreStatus::promptRequested;
  }
  if (io::stripBlanks(answer).empty()) {
    session::incrementGeometryName(session, 0, -1);
  } else {
    session::setGeometryName(session, std::string(answer));
  }
  return OperNameCoreStatus::applied;
}

// OPER NINC (xoper.f 1735-1738): NAMMOD(NAME,1,1).
OperNincCoreStatus handleOperNincCore(session::SessionComposition& session,
                                      std::ostream& out,
                                      std::string_view answer,
                                      bool hasAnswer) {
  const std::string nameDefault =
      session::geometryNameVersionDefault(session, 1, 1);
  if (!hasAnswer) {
    emitNamePrompt(out, nameDefault);
    return OperNincCoreStatus::promptRequested;
  }
  if (io::stripBlanks(answer).empty()) {
    session::incrementGeometryName(session, 1, 1);
  } else {
    session::setGeometryName(session, std::string(answer));
  }
  return OperNincCoreStatus::applied;
}

// OPER NDEC (xoper.f 1740-1743): NAMMOD(Name,-1,1). For KMODP<1
// (e.g. [0], [1], [99]+decrement to 0) NAMMOD keeps the
// original name including tag (xoper.f 1846,1873-1884).
OperNdecCoreStatus handleOperNdecCore(session::SessionComposition& session,
                                      std::ostream& out,
                                      std::string_view answer,
                                      bool hasAnswer) {
  const std::string nameDefault =
      session::geometryNameVersionDefault(session, -1, 1);
  if (!hasAnswer) {
    emitNamePrompt(out, nameDefault);
    return OperNdecCoreStatus::promptRequested;
  }
  if (io::stripBlanks(answer).empty()) {
    session::incrementGeometryName(session, -1, 1);
  } else {
    session::setGeometryName(session, std::string(answer));
  }
  return OperNdecCoreStatus::applied;
}

// OPER CPMN (xoper.f 1678-1686): pure read-only query of the retained
// FCPMIN snapshots; LVISC selects format 1769, otherwise 1779. Without a
// snapshot Fortran prints the COMMON start values 0.
OperCpmnCoreStatus handleOperCpmnCore(
    const session::SessionComposition& session, std::ostream& out) {
  const session::CpMinimumSnapshot& snap = session.operating.cpMinimum;
  if (session.operating.viscous) {
    emitCpmnViscous(out, snap.cpmni, snap.xcpmni, snap.cpmnv, snap.xcpmnv);
  } else {
    emitCpmnInviscid(out, snap.cpmni, snap.xcpmni);
  }
  return snap.hasBeenUpdatedByFcpmin ? OperCpmnCoreStatus::available
                                     : OperCpmnCoreStatus::noCurrentPoint;
}

// ---
// Polar presentation and Polar commands.
// ---

io::PolarWriteRequest makePolarWriteRequest(
    const core::polar::PolarDataset& slot, bool includeCpMin,
    bool includeHingeMoment, int ia1, int ia2, bool echoHeader) {
  io::PolarWriteRequest request;
  request.lhead = true;
  request.ia1 = ia1;
  request.ia2 = ia2;
  // NIPOL0 base set (xfoil.f 656-662); CINC/HINC hang on in canonical
  // order (xoper.f 909-916). JPOL = JTN, JTI (xfoil.f 664-666).
  request.ipol = {io::kIal, io::kIcl, io::kIcd, io::kIcp, io::kIcm};
  if (includeCpMin) {
    request.ipol.push_back(io::kImc);
  }
  if (includeHingeMoment) {
    request.ipol.push_back(io::kIch);
  }
  request.jpol = {io::kJtn, io::kJti};
  // NAMEPOL/CODEPOL are CHARACTER*48 (XFOIL.INC 32); all POLWRIT calls
  // in xoper.f passed these fields, the A edit fills to 48.
  request.code = echoHeader ? io::fortranA(48, slot.metadata.code)
                            : slot.metadata.code;
  request.version = slot.metadata.version;
  request.name = io::fortranA(48, slot.metadata.airfoilName);
  request.nbl = slot.metadata.elementCount;
  request.iretyp = static_cast<int>(slot.metadata.reType);
  request.imatyp = static_cast<int>(slot.metadata.machType);
  request.mach1 = slot.metadata.machCl1;
  request.reyn1 = slot.metadata.reCl1;
  request.acrit = slot.metadata.ncrit;
  request.xtrip = slot.metadata.xtrip;
  request.ptrat = slot.metadata.ptrat;
  request.etap = slot.metadata.etap;
  for (std::size_t i = 0; i < slot.points.size(); ++i) {
    const auto& point = slot.points[i];
    io::PolarPoint row;
    row.cpol[io::kIal - 1] = point.alpha;
    row.cpol[io::kIcl - 1] = point.cl;
    row.cpol[io::kIcd - 1] = point.cd;
    row.cpol[io::kIcm - 1] = point.cm;
    row.cpol[io::kIcw - 1] = point.cdWave;
    row.cpol[io::kIcv - 1] = point.cdVisc;
    row.cpol[io::kIcp - 1] = point.cdPres;
    row.cpol[io::kIma - 1] = point.mach;
    row.cpol[io::kIre - 1] = point.re;
    row.cpol[io::kIch - 1] = point.hinge;
    row.cpol[io::kImc - 1] = point.cpMin;
    row.cpol[io::kIcdh - 1] = point.cdh;
    row.cpol[io::kIcmdot - 1] = point.cmDot;
    for (std::size_t side = 0; side < 2; ++side) {
      const auto& data = slot.sideData[i][side];
      row.side[side][io::kJnc - 1] = data.ncrit;
      row.side[side][io::kJtp - 1] = data.xtrip;
      row.side[side][io::kJtn - 1] = data.xtr;
      row.side[side][io::kJti - 1] = data.itr;
    }
    request.points.push_back(row);
  }
  return request;
}

void emitPolarSummary(std::ostream& out,
                      const session::SessionComposition& session,
                      int first1, int last1, int ipact1) {
  emitPolarSummaryHeader(out);
  for (int ip = first1; ip <= last1; ++ip) {
    const auto& slot =
        session.polar.slots[static_cast<std::size_t>(ip - 1)];
    const auto encoding =
        core::polar::encodeReMantissaExponent(slot.metadata.reCl1);
    const auto reTag = [](core::polar::ReType type) {
      switch (type) {
        case core::polar::ReType::invSqrtCl: return std::string("/sqCL");
        case core::polar::ReType::invCl: return std::string("/CL  ");
        case core::polar::ReType::fixed: break;
      }
      return std::string("     ");
    };
    const auto machTag = [](core::polar::MachType type) {
      switch (type) {
        case core::polar::MachType::invSqrtCl: return std::string("/sqCL");
        case core::polar::MachType::invCl: return std::string("/CL  ");
        case core::polar::MachType::fixed: break;
      }
      return std::string("     ");
    };
    const std::string fileName =
        io::stripBlanks(session.polar.saveFileNames[static_cast<std::size_t>(
            ip - 1)]);
    // LPFILE (canonical: only for the opened PACC file) stays ' '
    // until PACC activation.
    const char accumulationMark = (ip == ipact1) ? '>' : ' ';
    emitPolarSummaryRow(out, accumulationMark, ip, slot.metadata.airfoilName,
                        encoding.mantissa, encoding.exponent,
                        reTag(slot.metadata.reType), slot.metadata.machCl1,
                        machTag(slot.metadata.machType),
                        slot.metadata.ncrit[0], slot.metadata.ncrit[1],
                        slot.metadata.xtrip[0], slot.metadata.xtrip[1], ' ',
                        fileName);
  }
}

OperCincCoreStatus handleOperCincCore(session::SessionComposition& session,
                                      std::ostream& out) {
  session::togglePolarIncludeCpMin(session);
  if (session.polar.includeCpMin) {
    emitCincEnabled(out);
    return OperCincCoreStatus::toggledOn;
  }
  emitCincDisabled(out);
  return OperCincCoreStatus::toggledOff;
}

OperHincCoreStatus handleOperHincCore(session::SessionComposition& session,
                                      std::ostream& out) {
  session::togglePolarIncludeHingeMoment(session);
  if (session.polar.includeHingeMoment) {
    // xoper.f 1699-1706: note only if LFLAP is not defined
    // (-C: LFLAP is held in session.design.flap.lFlap).
    emitHincEnabled(out);
    if (!session.design.flap.lFlap) {
      emitHincFlapUndefinedNote(out);
    }
    return OperHincCoreStatus::toggledOn;
  }
  emitHincDisabled(out);
  return OperHincCoreStatus::toggledOff;
}

OperPsumCoreStatus handleOperPsumCore(const session::SessionComposition& session,
                                      std::ostream& out) {
  if (session.polar.slots.empty()) {
    // xoper.f 1018-1020: WRITE(*,*) blank + 'No polars are stored'.
    out << "\n No polars are stored\n";
    return OperPsumCoreStatus::noPolars;
  }
  emitPolarSummary(out, session, 1, static_cast<int>(session.polar.slots.size()),
                   session.polar.activeSlot + 1);
  return OperPsumCoreStatus::shown;
}

// OPER DAMP (xoper.f 1744-1750): toggles IDAMP with canonical
// message; arguments are ignored, no invalidation.
OperDampCoreStatus handleOperDampCore(session::SessionComposition& session,
                                      std::ostream& out) {
  const int idamp = session::toggleAmplificationModel(session);
  if (idamp == 1) {
    emitDampModified(out);
  } else {
    emitDampOriginal(out);
  }
  return OperDampCoreStatus::applied;
}

// OPER ASET (xoper.f 1252-1289). Selection follows the canonical
// control flow: NPOL==0 -> message; inline number -> IP=IINPUT(1),
// an unparseable argument counts as NINPUT=0 (prompt path);
// NPOL==1 -> IP=1 without PLRSUM/prompt; otherwise PLRSUM + ASKI.
// IP==0 -> silent return. IP outside [1,NPOL] -> message,
// NINPUT=0, retry. Confirmation: Index('Nn',ANSWER) (CHARACTER*1) ->
// abort for 'N'/'n', otherwise (blank, 'Y', other) APCOPY.
OperAsetCoreStatus handleOperAsetCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument,
                                      bool hasArgument) {
  const int npol = static_cast<int>(session.polar.slots.size());
  if (npol == 0) {
    emitAsetNoPolars(out);
    return OperAsetCoreStatus::noPolars;
  }

  int ip = 0;
  bool inlineGiven = hasArgument;
  if (inlineGiven) {
    const std::vector<double> values = inlineRealValues(argument);
    if (values.empty()) {
      inlineGiven = false;  // GETFLT error corresponds to NINPUT=0
    } else {
      ip = toFortranInt(values[0]);
    }
  }

  for (;;) {
    if (!inlineGiven && npol > 1) {
      emitPolarSummary(out, session, 1, npol, session.polar.activeSlot + 1);
      if (!askIntegerCore(in, out, "Enter index of polar airfoil to set",
                          ip)) {
        return OperAsetCoreStatus::inputAborted;
      }
    } else if (!inlineGiven && npol == 1) {
      ip = 1;
    }
    if (ip == 0) {
      return OperAsetCoreStatus::abortedByZero;
    }
    if (ip < 1 || ip > npol) {
      emitAsetIndexInvalid(out);
      inlineGiven = false;  // xoper.f 1272: NINPUT = 0
      continue;
    }
    break;
  }

  emitAsetConfirm(out);
  std::string line;
  if (!std::getline(in, line)) {
    return OperAsetCoreStatus::inputAborted;
  }
  const char ans = line.empty() ? ' ' : line[0];
  if (ans == 'N' || ans == 'n') {
    emitAsetNoAction(out);
    return OperAsetCoreStatus::cancelled;
  }

  const core::Status committed =
      session::applyArchivedPolarGeometry(session, ip - 1);
  if (!committed.isOk()) {
    return OperAsetCoreStatus::slotGeometryUnavailable;
  }
  return OperAsetCoreStatus::applied;
}

// Minimum condition for FBLGET (dplot.f 332-347): both sides
// must have valid BL station fields with IBLTE >= 2 (1-based,
// here 0-based iblte >= 1) and IPAN indices within the
// current contour itself; additionally QINF != 0 (divisions in
// FBLGET). This is exactly the data access of FBLGET - no
// convergence, accumulation or consideration flag.
bool blfBoundaryLayerDataUsable(
    const session::SessionComposition& session) {
  const auto& panel = session.paneling.panel;
  const auto& viscals = session.viscous.viscals;
  const auto& mapping = viscals.mapping;
  if (!session.paneling.panelingValid || panel.x.size() < 2 ||
      !mapping.valid) {
    return false;
  }
  if (session.operating.qinf == 0.0) {
    return false;
  }
  const std::size_t n = panel.x.size();
  for (int is = 0; is < 2; ++is) {
    const auto& side = is == 0 ? mapping.top : mapping.bottom;
    if (side.iblte < 1 || side.ipan.size() <= static_cast<std::size_t>(side.iblte)) {
      return false;
    }
    if (viscals.dstr.top.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.dstr.bottom.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.thet.top.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.thet.bottom.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.uedg.top.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.uedg.bottom.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.arcs.top.size() <= static_cast<std::size_t>(side.iblte) ||
        viscals.arcs.bottom.size() <= static_cast<std::size_t>(side.iblte)) {
      return false;
    }
    for (int s0 = 1; s0 <= side.iblte; ++s0) {
      const int panelIndex = side.ipan[static_cast<std::size_t>(s0)];
      if (panelIndex < 0 || static_cast<std::size_t>(panelIndex) >= n) {
        return false;
      }
    }
  }
  return true;
}

// OPER BLF (xoper.f 1547-1600). DPR = RINPUT(1) for an inline number,
// otherwise 0.01; an unparseable argument corresponds to NINPUT=0 ->
// default; extra arguments are canonically ignored (only
// RINPUT(1) is read). XOC is canonically fixed at 0.4 for both
// sides. The report is entirely mutation-free.
OperBlfCoreStatus handleOperBlfCore(
    const session::SessionComposition& session, std::ostream& out,
    std::string_view argument, bool hasArgument) {
  double dpr = 0.01;
  if (hasArgument) {
    const std::vector<double> values = inlineRealValues(argument);
    if (!values.empty()) {
      dpr = values[0];
    }
  }

  if (!blfBoundaryLayerDataUsable(session)) {
    emitBlfNoData(out);
    return OperBlfCoreStatus::blDataUnavailable;
  }

  const auto& panel = session.paneling.panel;
  const auto& viscals = session.viscous.viscals;
  core::boundary_layer::BlForceReportInputs inputs;
  inputs.x = panel.x;
  inputs.y = panel.y;
  inputs.s = panel.s;
  inputs.xp = panel.xp;
  inputs.yp = panel.yp;
  inputs.nx = panel.xn;
  inputs.ny = panel.yn;
  inputs.ipan = {std::span<const int>(viscals.mapping.top.ipan),
                 std::span<const int>(viscals.mapping.bottom.ipan)};
  inputs.iblte = {viscals.mapping.top.iblte, viscals.mapping.bottom.iblte};
  inputs.dstr = {std::span<const double>(viscals.dstr.top),
                 std::span<const double>(viscals.dstr.bottom)};
  inputs.thet = {std::span<const double>(viscals.thet.top),
                 std::span<const double>(viscals.thet.bottom)};
  inputs.uedg = {std::span<const double>(viscals.uedg.top),
                 std::span<const double>(viscals.uedg.bottom)};
  inputs.xssi = {std::span<const double>(viscals.arcs.top),
                 std::span<const double>(viscals.arcs.bottom)};
  inputs.xssitr = viscals.xssitr;
  inputs.reinf = viscals.reinf;
  inputs.minf = viscals.minf;
  inputs.qinf = session.operating.qinf;
  inputs.tklam = viscals.tklam;
  inputs.gamm1 = core::inviscid::kGamm1;
  inputs.cffac = session.viscous.blpar.cffac;

  const double doff = 1.0e-5 * (panel.s.back() - panel.s.front());
  double xprOut[2] = {0.0, 0.0};
  double fprOut[2] = {0.0, 0.0};

  for (int ipr = 0; ipr < 2; ++ipr) {
    double spr = ipr == 0
                     ? panel.sle + (panel.s.front() - panel.sle) * 0.4
                     : panel.sle + (panel.s.back() - panel.sle) * 0.4;
    const double xpri = panel.xle + (panel.xte - panel.xle) * 0.4;
    core::math::sinvrt(spr, xpri, panel.x, panel.xp, panel.s);
    const double xpr =
        core::math::seval(spr, panel.x, panel.xp, panel.s) +
        doff * core::math::deval(spr, panel.y, panel.yp, panel.s);
    const double ypr =
        core::math::seval(spr, panel.y, panel.yp, panel.s) -
        doff * core::math::deval(spr, panel.x, panel.xp, panel.s);
    xprOut[ipr] = xpr;

    const auto report =
        core::boundary_layer::blForceReport(inputs, xpr, ypr, dpr);
    if (!report.isOk()) {
      emitBlfNoData(out);
      return OperBlfCoreStatus::blDataUnavailable;
    }
    const auto& side = report.value().side;
    emitBlfStationLine(out, side.x0, side.y0, side.ds, side.rtheta, side.hk);
    for (std::size_t k = 0; k < side.yy.size(); k += 2) {
      emitBlfProfileSample(out, side.yy[k] / 0.0078, side.uu[k]);
    }
    if (side.yintExceeded) {
      emitBlfYintExceeded(out);
    }
    fprOut[ipr] = side.fint;
  }

  out << "\n";
  for (int ipr = 0; ipr < 2; ++ipr) {
    emitBlfReportLine(out, xprOut[ipr], fprOut[ipr] * 1.0e4);
  }
  return OperBlfCoreStatus::reported;
}

// OPER FMOM (xoper.f 1617-1627): MHINGE with following
// Format 1500 report. For LFLAP=false MHINGE first runs
// GETXYF (xgdes.f 1880-1906): x prompt only for sentinel -999.0,
// upper/lower side report, y prompt only for -999.0, y/t path for
// exact 999.0; only after a successful dialog are XOF/YOF/LFLAP
// set. Integration happens via the shared
// hinge-load use case (core inviscid::mhinge; CPI for LVISC=false,
// CPV for LVISC=true).
OperFmomCoreStatus handleOperFmomCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out) {
  auto& flap = session.design.flap;
  if (!flap.lFlap) {
    double xof = flap.xof;
    double yof = flap.yof;
    if (xof == -999.0) {
      if (!askRealCore(in, out, "Enter flap hinge x location^", xof)) {
        return OperFmomCoreStatus::inputAborted;
      }
    }
    if (!session.paneling.panelingValid) {
      return OperFmomCoreStatus::geometryUnavailable;
    }
    const auto hinge = session::operFlapHinge(session, xof, yof);
    emitFlapSurfaces(out, hinge.topy, hinge.boty);
    if (yof == -999.0) {
      if (!askRealCore(in, out,
                       "Enter flap hinge y location (or 999 to specify y/t)^",
                       yof)) {
        return OperFmomCoreStatus::inputAborted;
      }
    }
    if (yof == 999.0) {
      double yrel = 0.0;
      if (!askRealCore(in, out, "Enter flap hinge relative y/t location^",
                       yrel)) {
        return OperFmomCoreStatus::inputAborted;
      }
      // y/t blend via the same core geometry share (GETXYF
      // 1902-1905); no formula duplication in the interpreter.
      yof = session::operFlapHinge(session, xof, 999.0, yrel).yf;
    }
    flap.xof = xof;
    flap.yof = yof;
    flap.lFlap = true;
  }
  const auto loads = session::computeCurrentHingeLoads(session);
  if (!loads.isOk()) {
    return OperFmomCoreStatus::geometryUnavailable;
  }
  emitFmomReport(out, flap.xof, flap.yof, loads.value().hmom,
                 loads.value().hfx, loads.value().hfy);
  return OperFmomCoreStatus::reported;
}

// OPER FNEW (xoper.f 1629-1641): sets exclusively the
// current flap state. Canonical argument assignment: >=2 reals ->
// XOF/YOF; exactly 1 real -> XOF, YOF=-999.0; 0 reals (incl. an
// unparseable argument, GETFLT NINPUT=0) -> both -999.0; always
// LFLAP=false. No output, no further mutation.
void handleOperFnewCore(session::SessionComposition& session,
                        std::string_view argument, bool hasArgument) {
  std::vector<double> values;
  if (hasArgument) {
    values = inlineRealValues(argument);
  }
  double xof = -999.0;
  double yof = -999.0;
  if (values.size() >= 2) {
    xof = values[0];
    yof = values[1];
  } else if (values.size() == 1) {
    xof = values[0];
  }
  session::setCurrentFlapSentinels(session, xof, yof);
}

// OPER VELS (xoper.f 1643-1665): velocity report at a
// free point via the existing core inviscid::psiLin
// (PSILIN I=0, GEOLIN=false, SIGLIN=true). redoArgument carries the
// canonical ARGOLD raw value (xoper.f 1662-1663) for the
// OPER redo infrastructure; the handler itself mutates nothing.
OperVelsCoreResult handleOperVelsCore(
    const session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument, bool hasArgument) {
  OperVelsCoreResult result;
  std::vector<double> values;
  if (hasArgument) {
    values = inlineRealValues(argument);
  }
  double x = 0.0;
  double y = 0.0;
  if (values.size() >= 2) {
    x = values[0];
    y = values[1];
  } else if (values.size() == 1) {
    x = values[0];
    if (!askRealCore(in, out, "Enter y^", y)) {
      result.status = OperVelsCoreResult::Status::inputAborted;
      return result;
    }
  } else {
    if (!askRealCore(in, out, "Enter x^", x)) {
      result.status = OperVelsCoreResult::Status::inputAborted;
      return result;
    }
    if (!askRealCore(in, out, "Enter y^", y)) {
      result.status = OperVelsCoreResult::Status::inputAborted;
      return result;
    }
  }
  const auto velocity = session::evaluateVelocityAtPoint(session, x, y);
  if (!velocity.isOk()) {
    result.status = OperVelsCoreResult::Status::geometryUnavailable;
    return result;
  }
  emitVelsReport(out, velocity.value().u, velocity.value().v,
                 velocity.value().q, velocity.value().cp);
  result.redoArgument = std::string(argument);
  result.status = OperVelsCoreResult::Status::reported;
  return result;
}

OperPlisCoreStatus handleOperPlisCore(session::SessionComposition& session,
                                      std::ostream& out,
                                      std::string_view argument,
                                      bool hasArgument) {
  if (session.polar.slots.empty()) {
    out << "\n No polars are stored\n";
    return OperPlisCoreStatus::noPolars;
  }
  int first = 1;
  int last = static_cast<int>(session.polar.slots.size());
  if (hasArgument) {
    const std::vector<double> values = inlineRealValues(argument);
    if (!values.empty()) {
      const int ip = toFortranInt(values[0]);
      if (ip != 0) {
        if (ip < 1 || ip > last) {
          // xoper.f 1046-1048: message, no prompt, back to the menu.
          out << "\n Specified stored polar does not exist\n";
          return OperPlisCoreStatus::invalidIndex;
        }
        first = ip;
        last = ip;
      }
    }
  }
  for (int ip = first; ip <= last; ++ip) {
    emitStoredPolarHeader(out, ip);
    const auto& slot = session.polar.slots[static_cast<std::size_t>(ip - 1)];
    const io::PolarWriteRequest request = makePolarWriteRequest(
        slot, session.polar.includeCpMin, session.polar.includeHingeMoment, 1,
        static_cast<int>(slot.points.size()), false);
    out << io::writePolarText(request);
  }
  return OperPlisCoreStatus::listed;
}

core::polar::PolarDataset polarDatasetFromParsed(
    const io::ParsedPolar& parsed) {
  core::polar::PolarDataset dataset;
  dataset.metadata.airfoilName = parsed.name;
  dataset.metadata.code = parsed.code;
  dataset.metadata.version = parsed.version;
  dataset.metadata.reType =
      static_cast<core::polar::ReType>(parsed.iretyp == 0 ? 1 : parsed.iretyp);
  dataset.metadata.machType =
      static_cast<core::polar::MachType>(parsed.imatyp == 0 ? 1
                                                            : parsed.imatyp);
  dataset.metadata.reCl1 = parsed.reyn1;
  dataset.metadata.machCl1 = parsed.mach1;
  dataset.metadata.ncrit = parsed.acrit;
  dataset.metadata.xtrip = parsed.xtrip;
  dataset.metadata.ptrat = parsed.ptrat;
  dataset.metadata.etap = parsed.etap;
  dataset.metadata.elementCount = parsed.nbl;
  for (const auto& row : parsed.points) {
    core::polar::PolarPoint point;
    point.alpha = row.cpol[io::kIal - 1];
    point.cl = row.cpol[io::kIcl - 1];
    point.cd = row.cpol[io::kIcd - 1];
    point.cm = row.cpol[io::kIcm - 1];
    point.cdWave = row.cpol[io::kIcw - 1];
    point.cdVisc = row.cpol[io::kIcv - 1];
    point.cdPres = row.cpol[io::kIcp - 1];
    point.mach = row.cpol[io::kIma - 1];
    point.re = row.cpol[io::kIre - 1];
    point.hinge = row.cpol[io::kIch - 1];
    point.cpMin = row.cpol[io::kImc - 1];
    point.cdh = row.cpol[io::kIcdh - 1];
    point.cmDot = row.cpol[io::kIcmdot - 1];
    std::array<core::polar::PolarSideData, core::sideCount> sides{};
    for (std::size_t side = 0; side < 2; ++side) {
      sides[side].ncrit = row.side[side][io::kJnc - 1];
      sides[side].xtrip = row.side[side][io::kJtp - 1];
      sides[side].xtr = row.side[side][io::kJtn - 1];
      sides[side].itr = row.side[side][io::kJti - 1];
    }
    dataset.points.push_back(point);
    dataset.sideData.push_back(sides);
  }
  return dataset;
}

core::polar::ReferencePolarDataset referencePolarFromParsed(
    const io::ParsedReferencePolar& parsed) {
  core::polar::ReferencePolarDataset dataset;
  dataset.label = parsed.label;
  for (std::size_t block = 0; block < core::polar::referenceBlockCount;
       ++block) {
    for (const auto& xy : parsed.blocks[block]) {
      core::polar::ReferencePolarPoint point;
      point.x = xy[0];
      point.y = xy[1];
      dataset.blocks[block].push_back(point);
    }
  }
  return dataset;
}

// PLRADD file branch (xpol.f 614-638): format 1200/1210/1220 line of the
// last appended point on save-file append.
std::string accumulateOperPointRecord(session::SessionComposition& session,
                                      bool pointConverged);

OperPointResult makeOperPointResult(
    const session::SessionComposition& session,
    const AnalysisResult& point, bool converged) {
 // Value sources are exclusively the
  // structured session state of the completed point.
  const auto& viscals = session.viscous.viscals;
  OperPointResult result;
  result.alpha = point.alpha.value;
  result.cl = viscals.cl;
  result.cd = viscals.cd;
  result.cdp = viscals.cdp;
  result.cm = viscals.cm;
  result.re = viscals.reinf;
  result.mach = viscals.minf;
  result.ncritTop = session.operating.criticalReynoldsExponents[0];
  result.ncritBottom = session.operating.criticalReynoldsExponents[1];
  result.xtrTop = viscals.xoctr[0];
  result.xtrBottom = viscals.xoctr[1];
  result.converged = converged;
  result.iterations = point.iterations;
  return result;
}

void appendPolarSavePoint(session::SessionComposition& session, int slot0) {
  const auto& slot = session.polar.slots[static_cast<std::size_t>(slot0)];
  io::PolarWriteRequest request = makePolarWriteRequest(
      slot, session.polar.includeCpMin, session.polar.includeHingeMoment,
      static_cast<int>(slot.points.size()),
      static_cast<int>(slot.points.size()), false);
  // POLWRIT call PLRADD (xpol.f 617-619) with LHEAD=0: only the
  // format line of the new point, no header.
  request.lhead = false;
  const std::string line = io::writePolarText(request);
 // saveFileNames carries the logical names;
  // the prefix transformation happens exactly here at the OPEN point.
  const std::string effective = io::applyOutputPathPolicy(
      session.outputPaths,
      session.polar.saveFileNames[static_cast<std::size_t>(slot0)]);
  // PUB-W2: byte-contract export - binary mode keeps canonical LF
  // line endings on every platform (text mode would rewrite LF to
  // CRLF on Windows).
  std::ofstream stream(effective, std::ios::app | std::ios::binary);
  if (stream) {
    stream << line;
  }
}

// PLXADD (xpol.f 645-720): append an unformatted record to the dump
// file. Data projection in the core, codec in io, wiring here.
// The return value is true only if the complete canonical PLXADD
// point group was written successfully (PLXADD reports
// "Point written to dump file" exclusively after CLOSE without an I/O error,
// xpol.f 724-726).
bool appendPolarDumpPoint(session::SessionComposition& session, int slot0) {
  const auto& viscals = session.viscous.viscals;
  const auto& panel = session.paneling.panel;
  const auto& wake = session.viscous.wake;
  core::export_data::PlxaddInputs in;
  in.viscous = session.operating.viscous;
  in.minf = viscals.minf;
  in.qinf = session.operating.qinf;
  // PLXADD writes Alfa/DTOR (xpol.f): degrees, not radians.
  in.alfaRad = viscals.alfa * 180.0 / std::numbers::pi;
  in.cl = viscals.cl;
  in.cd = viscals.cd;
  in.cm = viscals.cm;
  in.xoctrTop = viscals.xoctr[0];
  in.xoctrBottom = viscals.xoctr[1];
  in.sst = viscals.sst;
  // Fortran COMMON field X(1..N+NW): contour followed by the wake nodes
  // written by XYWAKE; CPV(1..N+NW) from CPCALC (xoper.f 329).
  in.contourCount = panel.x.size();
  in.x = panel.x;
  in.x.insert(in.x.end(), wake.x.begin(), wake.x.end());
  in.xp = panel.xp;
  in.xp.resize(in.x.size(), 0.0);
  in.s = panel.s;
  in.s.insert(in.s.end(), wake.s.begin(), wake.s.end());
  in.cpv = session.viscous.cpv;
  in.cpv.resize(in.x.size(), 0.0);
  for (int side = 0; side < 2; ++side) {
    const auto& map = side == 0 ? viscals.mapping.top : viscals.mapping.bottom;
    auto& dst = in.sides[side];
    // Core-side 0-based: IBLPAN (sole producer) returns NBL/IBLTE>=2,
    // the 0-based equivalent is >=1. The value 0 means "IBLPAN has
    // never run" and corresponds to the Fortran COMMON initial zero (xpol.f PLXADD
    // then writes NSIDE=(2,2), IBLTE=(0,0)).
    dst.nbl = map.nbl == 0 ? 0u : static_cast<std::size_t>(map.nbl) + 1;
    dst.iblte = map.iblte == 0 ? 0u : static_cast<std::size_t>(map.iblte) + 1;
    // Core-side 1-based Fortran indexing: index 0 is a PLACEHOLDER.
    dst.ipan.clear();
    dst.ipan.reserve(map.ipan.size() + 1);
    dst.ipan.push_back(0);
    for (const int index : map.ipan) {
      dst.ipan.push_back(index + 1);
    }
    const auto& thet = side == 0 ? viscals.thet.top : viscals.thet.bottom;
    const auto& dstr = side == 0 ? viscals.dstr.top : viscals.dstr.bottom;
    const auto& ctau = side == 0 ? viscals.ctau.top : viscals.ctau.bottom;
    const auto& tau = side == 0 ? viscals.tau.top : viscals.tau.bottom;
    // PLACEHOLDER plus range insert at the end: avoids
    // insert(begin, value) with rvalue (GCC -O3 -Wnull-dereference
    // false positive in the allocator path).
    dst.thet.clear();
    dst.thet.reserve(thet.size() + 1);
    dst.thet.push_back(0.0);
    dst.thet.insert(dst.thet.end(), thet.begin(), thet.end());
    dst.dstr.clear();
    dst.dstr.reserve(dstr.size() + 1);
    dst.dstr.push_back(0.0);
    dst.dstr.insert(dst.dstr.end(), dstr.begin(), dstr.end());
    dst.ctau.clear();
    dst.ctau.reserve(ctau.size() + 1);
    dst.ctau.push_back(0.0);
    dst.ctau.insert(dst.ctau.end(), ctau.begin(), ctau.end());
    dst.tau.clear();
    dst.tau.reserve(tau.size() + 1);
    dst.tau.push_back(0.0);
    dst.tau.insert(dst.tau.end(), tau.begin(), tau.end());
  }
  const auto projected = core::export_data::projectPlxaddPoint(in);
  if (!projected.isOk()) {
    return false;
  }

  const auto bytes = io::encodeDumpPoint(io::toDumpPoint(projected.value()));
 // read and write use the same transformed file;
  // dumpFileNames carries the logical names.
  const std::string effective = io::applyOutputPathPolicy(
      session.outputPaths,
      session.polar.dumpFileNames[static_cast<std::size_t>(slot0)]);
  std::ofstream stream(effective, std::ios::app | std::ios::binary);
  if (!stream) {
    return false;
  }
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  stream.close();
  return stream.good();
}

std::string accumulateOperPointRecord(session::SessionComposition& session,
                                      bool pointConverged) {
 // Guard xoper.f 440: IF (LPACC .AND. (LVCONV .OR. .NOT.LVISC)).
  if (!session.polar.accumulating) {
    return {};
  }
  if (!(pointConverged || !session.operating.viscous)) {
    return {};
  }
  std::ostringstream msg;
  const auto added = session::addPolarPointFromSession(session);
  switch (added.outcome) {
    case session::PolarAddOutcome::noActivePolar:
      // xpol.f 556-557.
      msg << " No active polar is declared. Point not stored.\n";
      break;
    case session::PolarAddOutcome::storageFull:
      // xpol.f 560-561.
      msg << " Polar storage arrays full. Point not stored\n";
      break;
    case session::PolarAddOutcome::added:
      msg << "\n Point added to stored polar"
          << io::fortranI(3, added.slotIndex + 1) << "\n";
      // PLRADD file branch (xpol.f 614-638) and PLXADD (xpol.f 650-653):
      // without an opened save/dump file the canonical messages.
      if (session.polar.saveFileOpen) {
        appendPolarSavePoint(session, added.slotIndex);
        msg << " Point written to save file  "
            << io::fortranA(
                   48, session.polar.saveFileNames[static_cast<std::size_t>(
                           added.slotIndex)])
            << "\n";
      } else {
        msg << " Save file unspecified or not available\n";
      }
      if (session.polar.dumpFileOpen) {
        // Success message only after successfully writing the complete
        // PLXADD point group (xpol.f 724-726); projection or IO error
        // suppresses the message, without changing session state or filename
        // (no invented partial reset).
        if (appendPolarDumpPoint(session, added.slotIndex)) {
          msg << " Point written to dump file "
              << io::fortranA(
                     48, session.polar.dumpFileNames[static_cast<std::size_t>(
                             added.slotIndex)])
              << "\n";
        }
      } else {
        msg << " Dump file unspecified or not available\n";
      }
      break;
  }
  return msg.str();
}

void accumulateOperPoint(session::SessionComposition& session,
                         std::ostream& out, bool pointConverged) {
  const std::string msg = accumulateOperPointRecord(session, pointConverged);
  if (!msg.empty()) {
    out << msg;
  }
}

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreOperHandlers.cpp

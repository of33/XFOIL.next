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

// File: src/app/cli/CoreVparHandlers.cpp
// Implementation of the VPAR
// state handlers. The handlers validate or collect inputs before the
// session mutation, call only SessionOps or the confirmed core BLPAR
// constants, and format the visible text via CliFortranText.
// They start no analysis and use no
// static VACCEL.

#include "CoreVparHandlers.h"

#include <cctype>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "CliFortranText.h"
#include "NumericArguments.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/session/SessionOps.h"

namespace xfoil::app::cli {
namespace {

bool isBlankLine(const std::string& line) {
  for (char ch : line) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

// ASKR (userio.f Format (/A,' r> ',$)): leading line break, prompt,
// then ' r> ' without closing line break. Empty line counts as
// canonical ASKR requirement (value stays unchanged); EOF aborts.
bool askRealCore(std::istream& in, std::ostream& out,
                 const std::string& prompt, double& value) {
  while (true) {
    out << "\n" << prompt << "   r>  ";
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

// GETFLT (userio.f): reads inline numbers; a read error returns N=0 and
// ERROR=.TRUE.; VPAR does not check ERROR and stays in the prompt path.
std::vector<double> inlineRealValues(std::string_view argument) {
  const NumericArgumentResult<double> parsed = parseRealArguments(argument);
  if (!parsed.success()) {
    return {};
  }
  return parsed.values;
}

}  // namespace

VparShowCoreStatus handleVparShowCore(const session::SessionComposition& session,
                                      std::ostream& out) {
  const session::OperatingConditionState& op = session.operating;
  // SHOW reads the authoritative session-bound
  // BLPAR state (COMMON/BLPAR), not the default factory.
  const core::boundary_layer::BoundaryLayerParameters blpar =
      session.viscous.blpar;
  emitVparDisplay(out, op.topTripX, op.bottomTripX,
                  op.criticalReynoldsExponents[0], op.criticalReynoldsExponents[1],
                  op.viscousAcceleration,
                  op.wakeLength, blpar);
  return VparShowCoreStatus::displayed;
}

VparXtrCoreStatus handleVparXtrCore(session::SessionComposition& session,
                                    std::istream& in, std::ostream& out,
                                    std::string_view argument,
                                    bool polarAccumulationActive) {
  // xoper.f 2571-2574: LPACC.AND.LVISC blocked with text 2100, without change.
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparXtrCoreStatus::polarAccumulationBlocked;
  }

  const std::vector<double> values = inlineRealValues(argument);
  double top = session.operating.topTripX;
  double bottom = session.operating.bottomTripX;

  if (values.size() >= 2) {
    // xoper.f 2575-2577: two inline values set both sides.
    top = values[0];
    bottom = values[1];
  } else {
    // xoper.f 2578-2581: otherwise both prompts (a single inline value is
    // incomplete and is discarded). Transactional: both values are collected
    // before mutation; an abort at the second prompt does not mutate.
    if (!askRealCore(in, out, "Enter top    side Xtrip/c", top)) {
      return VparXtrCoreStatus::inputAborted;
    }
    if (!askRealCore(in, out, "Enter bottom side Xtrip/c", bottom)) {
      return VparXtrCoreStatus::inputAborted;
    }
  }

  session::setTripPositions(session, top, bottom);
  return VparXtrCoreStatus::applied;
}

VparNCritCoreStatus handleVparNCritCore(session::SessionComposition& session,
                                        std::istream& in, std::ostream& out,
                                        std::string_view argument,
                                        bool polarAccumulationActive) {
  // xoper.f 2586-2589: LPACC.AND.LVISC blocked with text 2100.
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparNCritCoreStatus::polarAccumulationBlocked;
  }

  const std::vector<double> values = inlineRealValues(argument);
  double ncrit = session.operating.criticalReynoldsExponents[0];

  if (!values.empty()) {
    // xoper.f 2590-2592: a value sets both sides.
    ncrit = values[0];
  } else {
    // xoper.f 2594-2595: one prompt, both pages equal.
    if (!askRealCore(in, out, "Enter critical amplification ratio", ncrit)) {
      return VparNCritCoreStatus::inputAborted;
    }
  }

  session::setCriticalAmplificationBoth(session, ncrit);
  return VparNCritCoreStatus::applied;
}

// NT (xoper.f 2600-2610). Guard before prompt and
// mutation; an inline value sets only ACRIT(1), otherwise a prompt;
// invalid inline argument (GETFLT, NINPUT=0) falls into the
// prompt path; only LVCONV invalidated.
VparNCritCoreStatus handleVparNCritTopCore(
    session::SessionComposition& session, std::istream& in, std::ostream& out,
    std::string_view argument, bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparNCritCoreStatus::polarAccumulationBlocked;
  }
  const std::vector<double> values = inlineRealValues(argument);
  double top = session.operating.criticalReynoldsExponents[0];
  if (!values.empty()) {
    top = values[0];
  } else if (!askRealCore(in, out,
                          "Enter top-surface critical amplification", top)) {
    return VparNCritCoreStatus::inputAborted;
  }
  session::setCriticalAmplificationTop(session, top);
  return VparNCritCoreStatus::applied;
}

// NB (xoper.f 2613-2623): analogous to NT for ACRIT(2) (bottom).
VparNCritCoreStatus handleVparNCritBotCore(
    session::SessionComposition& session, std::istream& in, std::ostream& out,
    std::string_view argument, bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparNCritCoreStatus::polarAccumulationBlocked;
  }
  const std::vector<double> values = inlineRealValues(argument);
  double bottom = session.operating.criticalReynoldsExponents[1];
  if (!values.empty()) {
    bottom = values[0];
  } else if (!askRealCore(in, out,
                          "Enter bot-surface critical amplification",
                          bottom)) {
    return VparNCritCoreStatus::inputAborted;
  }
  session::setCriticalAmplificationBottom(session, bottom);
  return VparNCritCoreStatus::applied;
}

// WAKE/W (xoper.f 2635-2646). No LPACC guard
// (Fortran does not block WAKE during polar accumulation). WAKLEN0 is
// a dead write-only store and is not reproduced. An empty
// ASKR answer preserves the old value, but the four
// flag invalidations must occur as in Fortran.
VparWakeCoreStatus handleVparWakeCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument) {
  const std::vector<double> values = inlineRealValues(argument);
  double wakeLength = session.operating.wakeLength;
  if (!values.empty()) {
    wakeLength = values[0];
  } else if (!askRealCore(in, out, "Enter wake length/chord", wakeLength)) {
    return VparWakeCoreStatus::inputAborted;
  }
  session::setWakeLength(session, wakeLength);
  return VparWakeCoreStatus::applied;
}

// LAG (xoper.f 2656-2670). Guard 2100 for
// LPACC.AND.LVISC before prompt and mutation. Inline only for NINPUT.GE.3;
// otherwise complete prompt branch (partial inline values are
// discarded). Each field is written immediately after its prompt
// (ASKR targets 2662-2664); EOF at the second or third
// prompt leaves the already written fields in place. No
// flag invalidation.
VparLagCoreStatus handleVparLagCore(session::SessionComposition& session,
                                    std::istream& in, std::ostream& out,
                                    std::string_view argument,
                                    bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparLagCoreStatus::polarAccumulationBlocked;
  }
  const std::vector<double> values = inlineRealValues(argument);
  if (values.size() >= 3) {
    // xoper.f 2659-2661: SCCON, DUXCON, DLCON from RINPUT(1..3).
    session::setShearLagConstant(session, values[0]);
    session::setUxEqWeight(session, values[1]);
    session::setWakeDissipationLengthFactor(session, values[2]);
    return VparLagCoreStatus::applied;
  }
  double sccon = session.viscous.blpar.sccon;
  if (!askRealCore(in, out, "Enter shear lag constant", sccon)) {
    return VparLagCoreStatus::inputAborted;
  }
  session::setShearLagConstant(session, sccon);
  double duxcon = session.viscous.blpar.duxcon;
  if (!askRealCore(in, out, "Enter shear lag UxEQ weight", duxcon)) {
    return VparLagCoreStatus::inputAborted;
  }
  session::setUxEqWeight(session, duxcon);
  double dlcon = session.viscous.blpar.dlcon;
  if (!askRealCore(in, out, "Enter wake 1/dissipation-length factor",
                   dlcon)) {
    return VparLagCoreStatus::inputAborted;
  }
  session::setWakeDissipationLengthFactor(session, dlcon);
  return VparLagCoreStatus::applied;
}

// GB (xoper.f 2672-2685). Inline only for NINPUT.GE.2. CTCON is always
// recomputed from the current session GACON after both values
// (2684, outside of the IF/ELSE — also for an empty
// prompt answer); for EOF before the second prompt this
// step remains, and the already written GACON stays.
VparGbCoreStatus handleVparGbCore(session::SessionComposition& session,
                                  std::istream& in, std::ostream& out,
                                  std::string_view argument,
                                  bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparGbCoreStatus::polarAccumulationBlocked;
  }
  const std::vector<double> values = inlineRealValues(argument);
  if (values.size() >= 2) {
    // xoper.f 2676-2678: GACON, GBCON from RINPUT(1..2).
    session::setGbConstantA(session, values[0]);
    session::setGbConstantBAndCtcon(session, values[1]);
    return VparGbCoreStatus::applied;
  }
  double gacon = session.viscous.blpar.gacon;
  if (!askRealCore(in, out, "Enter G-beta constant A", gacon)) {
    return VparGbCoreStatus::inputAborted;
  }
  session::setGbConstantA(session, gacon);
  double gbcon = session.viscous.blpar.gbcon;
  if (!askRealCore(in, out, "Enter G-beta constant B", gbcon)) {
    return VparGbCoreStatus::inputAborted;
  }
  session::setGbConstantBAndCtcon(session, gbcon);
  return VparGbCoreStatus::applied;
}

// CTR (xoper.f 2687-2700). Inline only for NINPUT.GE.2; otherwise both
// prompts. CTRCON/CTRCEX progressively; no flag invalidation.
VparCtrCoreStatus handleVparCtrCore(session::SessionComposition& session,
                                    std::istream& in, std::ostream& out,
                                    std::string_view argument,
                                    bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparCtrCoreStatus::polarAccumulationBlocked;
  }
  const std::vector<double> values = inlineRealValues(argument);
  if (values.size() >= 2) {
    // xoper.f 2691-2693: CTRCON, CTRCEX from RINPUT(1..2).
    session::setInitialCtauConstant(session, values[0]);
    session::setInitialCtauExponent(session, values[1]);
    return VparCtrCoreStatus::applied;
  }
  double ctrcon = session.viscous.blpar.ctrcon;
  if (!askRealCore(in, out, "Enter initial-Ctau constant", ctrcon)) {
    return VparCtrCoreStatus::inputAborted;
  }
  session::setInitialCtauConstant(session, ctrcon);
  double ctrcex = session.viscous.blpar.ctrcex;
  if (!askRealCore(in, out, "Enter initial-Ctau exponent", ctrcex)) {
    return VparCtrCoreStatus::inputAborted;
  }
  session::setInitialCtauExponent(session, ctrcex);
  return VparCtrCoreStatus::applied;
}

// CFAC (xoper.f 2701-2706). No LPACC guard (Fortran does not block
// CFAC during polar accumulation); an inline value or a prompt;
// an invalid inline argument falls back to the prompt path; an empty
// answer preserves the old value. Only CFFAC; no flag invalidation.
VparCfacCoreStatus handleVparCfacCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument) {
  const std::vector<double> values = inlineRealValues(argument);
  if (!values.empty()) {
    session::setBoundaryLayerCfScale(session, values[0]);
    return VparCfacCoreStatus::applied;
  }
  double cffac = session.viscous.blpar.cffac;
  if (!askRealCore(in, out, "Enter Cf scaling factor", cffac)) {
    return VparCfacCoreStatus::inputAborted;
  }
  session::setBoundaryLayerCfScale(session, cffac);
  return VparCfacCoreStatus::applied;
}

// REST (xoper.f 2709-2715). Guard LPACC.AND.LVISC (2710) before BLPINI (2714);
// blocked case: format 2100 (1763-1764), GO TO 500, no mutation.
// Successful case: CALL BLPINI (xbl.f 1578-1596) via the session
// use case; no output, no prompt. Fortran parses COMARG via
// GETINT/GETFLT (2536-2541) but REST ignores it; the handler therefore
// takes no argument. No flag invalidation, no redo.
VparRestCoreStatus handleVparRestCore(session::SessionComposition& session,
                                      std::ostream& out,
                                      bool polarAccumulationActive) {
  if (polarAccumulationActive && session.operating.viscous) {
    emitPolarAccumulationBlocked(out);
    return VparRestCoreStatus::polarAccumulationBlocked;
  }
  session::restoreBoundaryLayerParameters(session);
  return VparRestCoreStatus::applied;
}

VparVaccCoreStatus handleVparVaccCore(session::SessionComposition& session,
                                      std::istream& in, std::ostream& out,
                                      std::string_view argument) {
  // xoper.f 2626-2632: no LPACC protection, no invalidation. Fortran
  // does not validate the value (negative/zero/non-finite are stored).
  const std::vector<double> values = inlineRealValues(argument);
  double vaccel = session.operating.viscousAcceleration;

  if (!values.empty()) {
    vaccel = values[0];
  } else {
    if (!askRealCore(in, out, "Enter viscous acceleration parameter", vaccel)) {
      return VparVaccCoreStatus::inputAborted;
    }
  }

  session::setViscousAcceleration(session, vaccel);
  return VparVaccCoreStatus::applied;
}

VparInitCoreStatus handleVparInitCore(session::SessionComposition& session,
                                      std::ostream& out) {
  // xoper.f 2649-2653: toggle LBLINI, then message depending on the new value;
  // on disabling additionally LIPAN=false (in the session operation).
  session::toggleBoundaryLayerInitialization(session);
  if (session.viscous.viscals.lblini) {
    emitVparInitInitialized(out);
    return VparInitCoreStatus::initializedAssumed;
  }
  emitVparInitWillInitialize(out);
  return VparInitCoreStatus::willInitialize;
}

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreVparHandlers.cpp

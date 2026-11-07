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

// File: src/app/cli/CoreVparHandlers.h
// VPAR submenu handlers. These free
// functions model the canonical VPAR submenu semantics for display,
// XTR, N, VACC and INIT on the session/core layers. They start no analysis, do not call SPECAL/SPECCL/VISCAL, use
// no static VACCEL and contain no numerical BL formula. The
// text formatting lives in CliFortranText.h.
//
// The handlers are the productive VPAR command route.
//
// Fortran anchoring (xoper.f VPAR, 2490-2727):
// Display format 1200 (2508-2526), entry and SHOW (GO TO 10).
// XTR 2569-2582: LPACC.AND.LVISC guard; >=2 inline values set
// XSTRIP(1/2), otherwise two prompts; LVCONV=.FALSE..
// N 2585-2597: LPACC.AND.LVISC protection; >=1 inline value sets
// ACRIT(1)=ACRIT(2), otherwise a prompt; LVCONV=.FALSE..
// VACC 2626-2632: no protection; >=1 inline value sets VACCEL, otherwise a
// prompt; no invalidation.
// INIT 2649-2653: toggles LBLINI; on disabling LIPAN=false; text.
// NT/NB/WAKE/LAG/GB/CTR/CFAC/REST are not mapped here.

#pragma once

#include <istream>
#include <ostream>
#include <string_view>

#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli {

// SHOW (xoper.f 2565-2566 GO TO 10) or VPAR entry: provides the
// parameter display (format 1200). Reads only the session operating state
// and the confirmed core BLPAR constants; no mutation, no analysis.
enum class VparShowCoreStatus {
  displayed,
};

VparShowCoreStatus handleVparShowCore(const xfoil::session::SessionComposition&
                                          session,
                                      std::ostream& out);

// XTR (xoper.f 2569-2582). polarAccumulationActive models LPACC; for
// LPACC.AND.LVISC the guard text 2100 appears without change. Both values
// are collected before each mutation (transactional); an abort at the second
// prompt does not mutate. A single inline value is incomplete and
// leads to both prompts (Fortran checks NINPUT.GE.2).
enum class VparXtrCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

VparXtrCoreStatus handleVparXtrCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// N (xoper.f 2585-2597). A value sets both sides (ACRIT(1)=ACRIT(2)).
// LPACC.AND.LVISC protection as for XTR.
enum class VparNCritCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

VparNCritCoreStatus handleVparNCritCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// NT/NB according to
// xoper.f 2600-2623. NT sets only ACRIT(1) (top), NB only ACRIT(2)
// (bottom); LPACC&LVISC guard before prompt and mutation; only LVCONV is
// invalidated.
VparNCritCoreStatus handleVparNCritTopCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);
VparNCritCoreStatus handleVparNCritBotCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// WAKE/W according to
// xoper.f 2635-2646. Both 4-character forms ('WAKE', 'W ')
// run the same use case; no LPACC guard; no redo. Sets
// WAKLEN (empty ASKR answer preserves the old value, the four
// flag invalidations occur nevertheless) and invalidates via
// SessionOps::setWakeLength exactly LWAKE/LBLINI/LVCONV/LIPAN.
enum class VparWakeCoreStatus {
  applied,
  inputAborted,
};

VparWakeCoreStatus handleVparWakeCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument);

// LAG/GB/CTR according to
// xoper.f 2656-2700. LPACC.AND.LVISC guard (format 2100) before prompt and
// mutation; blocked cases consume no value input and do not mutate
// (also no CTCON write for GB). The inline branch applies only with the
// full number of values (LAG NINPUT.GE.3, GB/CTR NINPUT.GE.2); too few or
// invalid inline values (GETFLT NINPUT=0) fall into the complete
// prompt branch — already accepted inline values are discarded. The
// prompt branch writes each field immediately after its prompt
// (canonical partial mutation on EOF; no atomic rollback).
// CFAC (2701-2706) has no guard. No flag invalidation;
// return to the VPAR menu; no redo.
enum class VparLagCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

VparLagCoreStatus handleVparLagCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

enum class VparGbCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

VparGbCoreStatus handleVparGbCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

enum class VparCtrCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

VparCtrCoreStatus handleVparCtrCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

enum class VparCfacCoreStatus {
  applied,
  inputAborted,
};

VparCfacCoreStatus handleVparCfacCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument);

// REST (xoper.f 2709-2715). Guard LPACC.AND.LVISC before BLPINI call;
// for the blockade: format 2100, no mutation, no input usage.
// Successful path: CALL BLPINI (xbl.f 1578-1596) via the
// session use case, no output, no prompt, no argument usage
// (Fortran ignores COMARG; 'REST 1.0' and 'RESTX' still fall
// back). No redo, no flag invalidation. Domain-specific separation from
// QDES REST and GDES/MDES RESE.
enum class VparRestCoreStatus {
  applied,
  polarAccumulationBlocked,
};

VparRestCoreStatus handleVparRestCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    bool polarAccumulationActive = false);

// VACC (xoper.f 2626-2632). No LPACC protection, no invalidation. Fortran
// validates nothing: negative, zero and non-finite values are
// stored unchanged.
enum class VparVaccCoreStatus {
  applied,
  inputAborted,
};

VparVaccCoreStatus handleVparVaccCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument);

// INIT (xoper.f 2649-2653). Toggles LBLINI; when disabling, additionally
// sets LIPAN=false. No argument, no analysis, no solver.
enum class VparInitCoreStatus {
  initializedAssumed,  // LBLINI now true
  willInitialize,      // LBLINI now false (LIPAN=false)
};

VparInitCoreStatus handleVparInitCore(xfoil::session::SessionComposition&
                                          session,
                                      std::ostream& out);

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreVparHandlers.h

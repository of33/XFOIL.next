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

// File: src/app/cli/CoreOperHandlers.h
// OPER operating-state handlers. These free
// functions model the canonical OPER state semantics for VISC, Re, Mach,
// TYPE and ITER on the session/core layers.
// They start no analysis, do not call SPECAL/SPECCL/VISCAL, and contain
// no second numerical MRCL/COMSET implementation. Text formatting
// lives in CliFortranText.h.
//
// The handlers are the productive OPER command route.

#pragma once

#include <istream>
#include <ostream>
#include <string_view>
#include <vector>

#include "xfoil/AnalysisResult.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/io/PolarText.h"
#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli {

// VISC (xoper.f 197-216): toggles LVISC. On enabling, an optional
// argument REINF1 may be set; for REINF1==0 and without an argument the
// canonical Reynolds prompt follows. MRSHOW appears only on enabling.
enum class OperViscCoreStatus {
  toggledOff,
  toggledOn,
  polarAccumulationBlocked,
  inputAborted,
};

OperViscCoreStatus handleOperViscCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// Re (xoper.f 278-297): sets REINF1 and runs MRCL(1.0). Without an argument
// Currently..., MRSHOW(.FALSE.,.TRUE.) and the Reynolds prompt appear.
enum class OperReCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

OperReCoreStatus handleOperReCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// Mach (xoper.f 299-334): sets MINF1, runs MRCL(1.0) and COMSET and
// shows the Sonic Cp line for MINF>0. Supersonic values are rejected with the
// canonical text and queried again.
enum class OperMachCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

OperMachCoreStatus handleOperMachCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// TYPE (xoper.f 336-376): sets MATYP/RETYP from the canonical TYPE menu,
// runs MRCL(1.0), COMSET and MRSHOW. Invalid values lead back to the
// table.
enum class OperTypeCoreStatus {
  applied,
  polarAccumulationBlocked,
  inputAborted,
};

OperTypeCoreStatus handleOperTypeCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// ITER (xoper.f 379-392): sets ITMAX. Only values >=1 are valid; invalid
// inline values show the candidates in the current-limit line and lead
// back to the prompt. No analysis, no invalidation.
enum class OperIterCoreStatus {
  applied,
  inputAborted,
};

OperIterCoreStatus handleOperIterCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument);

// ---: OPER single-point handlers (ALFA/CLI/CL) -----------------------
// Used by the native OPER command dispatcher. They perform exactly one
// session single-point operation (runOperAlphaPoint/runOperCliPoint/
// runOperClPoint), borrow no solver logic and produce no second numerical implementation. The emitted
// progress/completion texts are the canonical WRITE(*,*) lines from
// CliFortranText.h. The OPER menu prompt and the '!' continuation
// remain the responsibility of the productive interpreter.

// Alfa (xoper.f 416-449): SPECAL (LALFA=true), for LVISC VISCAL(ITMAX).
// Without an inline number the ASKR prompt follows with the current alpha as the default.
enum class OperAlphaCoreStatus {
  analyzed,
  nonConverged,
  missingGeometry,
  inputAborted,
};

OperAlphaCoreStatus handleOperAlphaCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// CLI (xoper.f 475-503): SPECCL (LALFA=true), no SPECAL, for LVISC
// VISCAL(ITMAX). Without an inline number the ASKR prompt follows with the
// current CLSPEC as the default.
enum class OperCliCoreStatus {
  analyzed,
  nonConverged,
  missingGeometry,
  inputAborted,
};

OperCliCoreStatus handleOperCliCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// Cl (xoper.f 510-538): SPECCL (LALFA=false), no SPECAL, for LVISC
// VISCAL(ITMAX). Without an inline number the ASKR prompt follows with the
// current CLSPEC as the default.
enum class OperClCoreStatus {
  analyzed,
  nonConverged,
  missingGeometry,
  inputAborted,
};

OperClCoreStatus handleOperClCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false);

// '!' in the OPER menu (xoper.f 85-92): COMAND=COMOLD, COMARG=ARGOLD,
// LRECALC=.TRUE.; the repeated Alfa/CLI/Cl branch provides the
// point result and SPECAL/SPECCL and, for LVISC, runs only
// VISCAL(ITMAX) again on the retained state. Without a
// previous point command: 'Previous.OPER command not valid'
// (xoper.f 86-88). For inviscid LVISC=false no text arises
// (binary `t_bang_inviscid`).
enum class OperBangCoreStatus {
  continuedConverged,
  continuedNonConverged,
  noRepeatOutput,       // inviscid repeat: no single text
  noPreviousCommand,    // COMOLD='****'
  notContinuable,       // continuation state unusable due to mutation
};

OperBangCoreStatus handleOperBangCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    bool polarAccumulationActive = false);

// OPER INIT (xoper.f 394-401): toggles LBLINI; on disabling
// additionally LIPAN=false. No LVCONV/wake/DIJ/result change,
// no deletion of station fields, no analysis; COMOLD/ARGOLD
// are retained (INIT does not set COMOLD, xoper.f 471/506/541).
// Text and state contract identical to VPAR INIT
// (xoper.f 2649-2653). Without geometry OPER itself aborts
// earlier (xoper.f 49-53); the handler only reports
// missingGeometry.
enum class OperInitCoreStatus {
  initializedAssumed,  // new LBLINI state: true
  willInitialize,      // new LBLINI state: false (LIPAN=false)
  missingGeometry,
};

OperInitCoreStatus handleOperInitCore(
    xfoil::session::SessionComposition& session, std::ostream& out);

// Structured point-result record
// for the run summary of the batch runner. Created at the
// point-completion step from the session state; the runner reads
// exclusively these events (no re-parsing of stdout).
struct OperPointResult {
  double alpha{};        // Degrees (AnalysisResult.alpha)
  double cl{};           // Viscal state.cl
  double cd{};           // Viscal state.cd
  double cdp{};          // Viscal state.cdp (ICP)
  double cm{};           // Viscal state.cm
  double re{};           // Viscal state.reinf (IRE)
  double mach{};         // Viscal state.minf (IMA)
  double ncritTop{};     // operating.criticalReynoldsExponents[0]
  double ncritBottom{};  // operating.criticalReynoldsExponents[1]
  double xtrTop{};       // Viscal state.xoctr[0] (JTN; 1 = throughout laminar)
  double xtrBottom{};    // Viscal state.xoctr[1]
  bool converged{};      // LVCONV this point
  int iterations{};      // AnalysisResult.iterations
};

// Creates the point record from the session state of the completed
// point (callback-side per sequence point; after handler return value for
// ALFA/CLI/CL/'!').
[[nodiscard]]
OperPointResult makeOperPointResult(
    const xfoil::session::SessionComposition& session,
    const xfoil::AnalysisResult& point, bool converged);

// ASEQ/AS (xoper.f 545-610, 664-760): alpha1 alpha2 dAA; missing values
// are collected in canonical prompt order; step sign
// is enforced from AA2-AA1; NPOINT = INT((AA2-AA1)/DAA+0.5)+1,
// DAA=0 -> one point. Per point SPECAL + VISCAL(ITMAX+5); ISEQEX/NSEQEX=4
// with halt text format 1150.
enum class OperAseqCoreStatus {
  completed,
  haltedNonConvergence,
  inputAborted,
  missingGeometry,
  pointFailure,
};

OperAseqCoreStatus handleOperAseqCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false,
    std::vector<OperPointResult>* pointResults = nullptr);

// CSEQ/CS likewise with Cl specifications; per point SPECCL (start at the retained
// alpha) then SPECAL, LALFA=false.
enum class OperCseqCoreStatus {
  completed,
  haltedNonConvergence,
  inputAborted,
  missingGeometry,
  pointFailure,
};

OperCseqCoreStatus handleOperCseqCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument,
    bool polarAccumulationActive = false,
    std::vector<OperPointResult>* pointResults = nullptr);

// OPER Name/NINC/NDEC (xoper.f 1725-1743): mutate the same canonical
// name field (XFOIL.INC CHARACTER*48) as the TOP-level commands; NAMEPOL
// stays untouched. Name with an inline argument is silent (Name=COMARG,
// STRIP); without an argument NAMMOD always calls prompt+READ. NINC/NDEC
// call NAMMOD(1,1)/NAMMOD(-1,1).
enum class OperNameCoreStatus {
  applied,
  promptRequested,
};

OperNameCoreStatus handleOperNameCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view argument, bool hasArgument, std::string_view answer,
    bool hasAnswer);

enum class OperNincCoreStatus {
  applied,
  promptRequested,
};

OperNincCoreStatus handleOperNincCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view answer, bool hasAnswer);

enum class OperNdecCoreStatus {
  applied,
  promptRequested,
};

OperNdecCoreStatus handleOperNdecCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view answer, bool hasAnswer);

// OPER CPMN (xoper.f 1678-1686): prints the canonical
// FCPMIN COMMON snapshot (CPMNI/XCPMNI/CPMNV/XCPMNV) of the last
// point command; no new computation, no session mutation. Without a
// snapshot (no point or invalidation) the values correspond to the
// COMMON start 0.
enum class OperCpmnCoreStatus {
  available,
  noCurrentPoint,
};

OperCpmnCoreStatus handleOperCpmnCore(
    const xfoil::session::SessionComposition& session, std::ostream& out);

// ---
// Polar presentation and Polar commands (xoper.f 1017-1178,
// xpol.f 755-836).
// ---

// POLWRIT write request from a stored slot. ia1/ia2 are
// 1-based (PLIS: 1..NAPOL; PNAM header output: 0/-1). Columns:
// NIPOL0 base set IAL,ICL,ICD,ICP,ICM (xfoil.f 656-662) plus IMC for
// LCMINP and I for LHMOMP in canonical order (xoper.f 908-916);
// JPOL = JTN,JTI (xfoil.f 664-666). echoHeader: stdout echoes (PGET/PLIS/
// PNAM/PREM) passed through CODEPOL as CHARACTER*48 (padded to 48),
// file writer PWRT uses the literal 'XFOIL' instead (xoper.f 924).
xfoil::io::PolarWriteRequest makePolarWriteRequest(
    const xfoil::core::polar::PolarDataset& slot, bool includeCpMin,
    bool includeHingeMoment, int ia1, int ia2, bool echoHeader,
    const xfoil::core::polar::PolarElementSelection& elementSelection);

// PLRSUM (xpol.f 755): header plus one line per slot. ipact1 is the
// active slot 1-based (0 = none); LPFILE is canonical only set with the
// PACC file management and is currently passed as ' '.
void emitPolarSummary(std::ostream& out,
                      const xfoil::session::SessionComposition& session,
                      int first1, int last1, int ipact1);

// OPER CINC (xoper.f 1689-1696): toggles LCMINP.
enum class OperCincCoreStatus { toggledOn, toggledOff };
OperCincCoreStatus handleOperCincCore(xfoil::session::SessionComposition& session,
                                      std::ostream& out);

// OPER HINC (xoper.f 1698-1707): toggles LHMOMP; the note about the
// missing hinge position is canonical when LFLAP is false
// (session.design.flap.lFlap).
enum class OperHincCoreStatus { toggledOn, toggledOff };
OperHincCoreStatus handleOperHincCore(xfoil::session::SessionComposition& session,
                                      std::ostream& out);

// OPER PLIS (xoper.f 1027-1078): lists stored polars with
// POLWRIT echo on stdout.
enum class OperPlisCoreStatus { listed, noPolars, invalidIndex };
OperPlisCoreStatus handleOperPlisCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view argument, bool hasArgument);

// OPER PSUM (xoper.f 1017-1024): PLRSUM(1,NPOL,IPACT).
enum class OperPsumCoreStatus { shown, noPolars };
OperPsumCoreStatus handleOperPsumCore(
    const xfoil::session::SessionComposition& session, std::ostream& out);

// OPER DAMP
// (xoper.f 1744-1750). Arguments are canonically ignored; the
// handler toggles via session::toggleAmplificationModel and
// emits the canonical message.
enum class OperDampCoreStatus { applied };
OperDampCoreStatus handleOperDampCore(
    xfoil::session::SessionComposition& session, std::ostream& out);

// OPER ASET
// (xoper.f 1252-1289) takes the stored profile contour of a
// polar slot into the current contour (APCOPY xpol.f 139-176 via the
// existing session use case applyArchivedPolarGeometry).
// Selection/confirmation control in the handler, commit/invalidation in
// SessionOps.
enum class OperAsetCoreStatus {
  applied,
  noPolars,
  abortedByZero,             // IP==0: silent return to the menu
  cancelled,                 // confirmation N/n
  inputAborted,              // EOF on selection or confirmation prompt
  slotGeometryUnavailable,   // slot without stored contour
};
OperAsetCoreStatus handleOperAsetCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument, bool hasArgument);

// OPER BLF
// (xoper.f 1547-1600) as a mutation-free text report via the
// core kernel core::boundary_layer::blForceReport (dplot.f FBLGET).
// Without usable BL station data the handler refuses mutation-free
// with the approved safety diagnostic (controlled
// deviation from the Fortran crash path, user decision).
enum class OperBlfCoreStatus {
  reported,
  blDataUnavailable,
};
OperBlfCoreStatus handleOperBlfCore(
    const xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view argument, bool hasArgument);

// OPER FMOM
// (xoper.f 1617-1627 + MHINGE xoper.f 2350-2487 + GETXYF
// xgdes.f 1880-1906). For LFLAP=false the handler runs the
// canonical GETXYF dialog (sentinel -999, y/t path 999), sets
// LFLAP/XOF/YOF only after a successful dialog, and then integrates
// via the shared hinge-load use case (core inviscid::mhinge;
// CPI for LVISC=false, CPV for LVISC=true). Format 1500 report
// byte-exact; no plot text.
enum class OperFmomCoreStatus {
  reported,
  geometryUnavailable,
  inputAborted,
};
OperFmomCoreStatus handleOperFmomCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out);

// OPER FNEW (xoper.f 1629-1641). Two inline values ->
// XOF/YOF; one value -> XOF, YOF=-999; no value (also not
// parseable) -> both -999; always LFLAP=false. No output, no
// further mutation.
void handleOperFnewCore(xfoil::session::SessionComposition& session,
                        std::string_view argument, bool hasArgument);

// OPER VELS (xoper.f 1643-1665). Inline values or
// ASKR prompts 'Enter x^'/'Enter y^'; PSILIN via the existing
// core (psiLin, I=0, GEOLIN=false, SIGLIN=true); FORMAT 1800
// byte-exact; mutation-free. redoArgument carries the canonical
// ARGOLD value (raw argument) for the OPER redo infrastructure.
struct OperVelsCoreResult {
  enum class Status {
    reported,
    geometryUnavailable,
    inputAborted,
  };
  Status status{Status::reported};
  std::string redoArgument;
};
OperVelsCoreResult handleOperVelsCore(
    const xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out, std::string_view argument, bool hasArgument);

// PLRADD/PLXADD on the four OPER point call steps (xoper.f 440-443,
// 497-499, 532-534, 688-690): guard LPACC .AND. (LVCONV .OR. .NOT.LVISC),
// then point storage and the canonical messages. pointConverged is
// LVCONV after VISCAL for this point.
void accumulateOperPoint(xfoil::session::SessionComposition& session,
                         std::ostream& out, bool pointConverged);

// POLREAD->PolarDataset (xoper.f 843-849). ParsedPolar.cpol is already
// indexed after PINDEX; the adoption is a pure field mapping.
// NXYPOL(IP)=0 (xoper.f 854) corresponds to an empty geometry snapshot.
xfoil::core::polar::PolarDataset polarDatasetFromParsed(
    const xfoil::io::ParsedPolar& parsed);

// POLREF->reference slot (xoper.f 952-953).
xfoil::core::polar::ReferencePolarDataset referencePolarFromParsed(
    const xfoil::io::ParsedReferencePolar& parsed);

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreOperHandlers.h

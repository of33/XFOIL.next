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

// File: src/app/cli/CoreGeometryHandlers.h
// Internal preparation stage G2: session/core-based
// geometry/file handlers. These free functions feed the visible
// CLI output exclusively from structured core/session results
// and call only session use cases or confirmed core kernels. They
// produce no second runtime state and contain no solver
// orchestration. Text formatting stays in the CLI (CliFortranText.h).
//
// These handlers are the productive command route: the
// CommandInterpreter dispatches to them.

#pragma once

#include <filesystem>
#include <istream>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include "xfoil/Types.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli {

// NACA command (xfoil.f NACA, naca.f): sets the buffer from the NACA
// designation, provides thickness/camber (GEOPAR on the buffer contour), the
// buffer message (format 1200), the trailing-edge message (PANGEN format 1090,
// SHARP = DSTE < 0.0001*CHORD) and the parameter table (format 1100).
// designation < 0 or invalid designation deliver the controlled
// original messages; the session state then stays unchanged.
void handleNacaCore(xfoil::session::SessionComposition& session,
                    std::ostream& out, int designation);

// Current airfoil of the session panel contour (Fortran X(1..N)/Y(1..N)) as
// a coordinate list for SAVE/PSAV/ISAV.
// Empty list if no valid panel contour exists.
// Read-only access to the confirmed session state; no mutation,
// no solver orchestration.
[[nodiscard]]
std::vector<xfoil::Point2D> currentAirfoilCoordinatesCore(
    const xfoil::session::SessionComposition& session);

// Result of the PCOP core path (ABCOPY). applied: current
// panel contour was set from the buffer (origin abcopy,
// panelSourceRevision == bufferRevision). bufferUnavailable: no valid
// buffer (canonical ABCOPY buffer message). capacityExceeded: buffer points
// exceed the ABCOPY boundary (NB > IQX-5 = 596); canonical
// four-line rejection. For each error value the session state stays
// unchanged (no partial mutation).
enum class PcopCoreStatus {
  applied,
  bufferUnavailable,
  capacityExceeded,
};

// PCOP command (xfoil.f:288 CALL ABCOPY(.TRUE.)): carries the buffer points
// (XB,YB) directly as the current panel contour (X,Y) and provides the ABCOPY
// confirmation (format 1200) with the resulting node count. Direction
// is buffer->panel (not panel->buffer). Uses the confirmed session
// use case copyBufferToCurrent (origin abcopy, panelSourceRevision ==
// bufferRevision, analysis/wake invalidation according to ABCOPY flag block);
// no solver orchestration, no PANGEN, no second copy.
// Without a valid buffer or on capacity exceedance the
// canonical error message appears and the state stays unchanged.
PcopCoreStatus handlePcopCore(xfoil::session::SessionComposition& session,
                              std::ostream& out);

// Result of the TOP-level PANE core path. applied: panel contour
// newly produced from the buffer (origin pangen, panelSourceRevision==
// bufferRevision). bufferUnavailable: no valid buffer (canonical
// PANGEN buffer message). panelingFailed: PANGEN core error (e.g.
// capacity limit or core precondition); state unchanged.
enum class PaneCoreStatus {
  applied,
  bufferUnavailable,
  panelingFailed,
};

// PANE command (xfoil.f: CALL PANGEN(.TRUE.)): produces the current
// panel contour from buffer and session panel parameters and provides the
// PANGEN(.TRUE.) output (Newton limit diagnostics, trailing-edge message
// format 1090, SHOPAR parameter table format 1100, no CANG). Uses the
// confirmed session use case applyPaneling; no
// solver orchestration, no second numerical path. On errors (buffer
// missing, capacity limit, core precondition) the contour stays unchanged.
PaneCoreStatus handlePaneCore(xfoil::session::SessionComposition& session,
                              std::ostream& out);

// PPAR entry (xfoil.f GETPAN format 1000): shows the current
// panel parameters of the session. Read-only access to
// session.panelingParameters; no mutation.
void handlePparShowCore(const xfoil::session::SessionComposition& session,
                        std::ostream& out);

// Result of the PPAR parameter command (GETPAN parameter branch).
// applied: value stored, no diagnostics. appliedClamped: NPAN limited
// by IQX-6, format-1200 message output. rejected: controlled
// rejection (wrong number of values or NPAN<2); state unchanged, no
// text (the interactive ASKI/ASKR diagnostics live in the menu loop).
enum class PparParameterCoreStatus {
  applied,
  appliedClamped,
  rejected,
};

// PPAR parameter command (xfoil.f GETPAN N/P/T/R/XT/XB): sets a
// panel parameter via the confirmed session use case setPanelParameter
// (incl. canonical clamp NPAN>595). Canonical (GETPAN GO TO 12): no
// panel regeneration, no tables from pressure, no analysis invalidation;
// effect only at the next PANGEN/apply. For NPAN>595 the
// format-1200 message is output. XT/XB are transactional value pairs.
PparParameterCoreStatus applyPparParameterCore(
    xfoil::session::SessionComposition& session,
    xfoil::core::paneling::PanelParameterKind kind,
    std::span<const double> values, std::ostream& out);

// Result of the PPAR apply (empty PPAR input -> PANGEN). applied: panel
// contour newly produced from the buffer (origin pangen).
// bufferUnavailable: no valid buffer (canonical PANGEN buffer message).
// panelingFailed: PANGEN core error (e.g. capacity limit); state unchanged.
enum class PparApplyCoreStatus {
  applied,
  bufferUnavailable,
  panelingFailed,
};

// PPAR apply (xfoil.f GETPAN: empty input with LCHANGE -> PANGEN(.FALSE.) +
// CANG). The confirmed session use case applyPaneling orchestrates exclusively
// (PANGEN from the session buffer, origin pangen,
// panelSourceRevision==bufferRevision, analysis/wake invalidation) and
// formats the canonical diagnostics. No second numerical path: identical
// to TOP-level PANE, only without SHOPAR parameter table and additionally CANG angle.
PparApplyCoreStatus applyPparPanelingCore(
    xfoil::session::SessionComposition& session, std::ostream& out);

// Result of the LOAD core path. success: buffer and current
// panel contour were set. The error values correspond to the canonical
// LOAD abort branches; for each error value the session state stays
// unchanged (no partial mutation).
enum class LoadCoreStatus {
  success,
  fileOpenError,
  bufferOverflow,
  elementNotFound,
  readError,
  invalidGeometry,
  normalizationFailed,
};

// LOAD command (xfoil.f LOAD, aread.f AREAD, xgeom.f NORM/GEOPAR, xgdes.f
// ABCOPY). Orchestrates
// exclusively existing layers:
// 1. file via the DatFileReader (readCoordinateFile),
// 2. traversal direction via the signed area of the confirmed core
// kernel aeCalc (itype 1; identical shoelace formula as Fortran LOAD),
// for clockwise a pure point reversal,
// 3. for LNORM the confirmed core transformation norm,
// 4. buffer via setBufferGeometry, current contour via
// copyBufferToCurrent (ABCOPY, origin abcopy),
// 5. LE/TE/chord from the confirmed abcopy panel contour, thickness/camber
// from the confirmed core kernel geopar (geometryMetrics),
// 6. airfoil name and LCLOCK (setWriteOrientation) in the session.
// The handler does not run PANGEN.
// The complete canonical LOAD text output happens via CliFortranText.
// requestedName is the name determined by the caller for plain files
// (ASKS); for labeled/MSES files it is not used.
LoadCoreStatus handleLoadCore(xfoil::session::SessionComposition& session,
                              const std::filesystem::path& path,
                              const std::string& requestedName,
                              std::ostream& out, int selectedElement = 0);

// Result of the TOP-level NORM core path. The canonical
// TOP-level NORM command (xfoil.f:233-239) toggles LNORM
// (LNORM =.NOT.LNORM), the policy for normalization of future
// load operations. enabledAfter/disabledAfter correspond to the new LNORM
// state and choose the canonical list-directed message. There is no
// error branch: NORM changes exclusively the session policy
// (geometry.normalizeOnLoad) and no existing geometry/analysis state.
enum class NormCoreStatus {
  enabledAfter,   // LNORM now .TRUE. -> 'Loaded airfoil will be normalized'
  disabledAfter,  // LNORM now .FALSE. -> "Loaded airfoil won't be normalized"
};

// TOP-level NORM command (xfoil.f:233-239). Reads the current session policy value (session::normalizeOnLoad),
// sets the negated value via session::setNormalizeOnLoad and emits the
// canonical toggle message via CliFortranText. No numeric
// coordinate transformation, no buffer/panel change, no ABCOPY/PANGEN,
// no analysis invalidation, no second policy copy.
NormCoreStatus handleNormCore(xfoil::session::SessionComposition& session,
                              std::ostream& out);

// Result of the TOP-level REVE core path. The canonical
// TOP-level REVE command (xfoil.f:176-182) toggles LCLOCK
// (LCLOCK =.NOT.LCLOCK), the write orientation for SAVE/PSAV/USAV/
// ISAV. clockwiseAfter/counterclockwiseAfter correspond to the new LCLOCK
// state and choose the canonical list-directed message. There is no
// error branch: REVE changes exclusively the write-orientation
// policy (geometry.writeClockwise) and no coordinates of buffer or
// panel, no origin, no revision, no geometry/analysis state.
enum class ReveCoreStatus {
  clockwiseAfter,       // LCLOCK now .TRUE. -> '... written in clockwise order'
  counterclockwiseAfter,  // LCLOCK now .FALSE. -> '... written in counterclockwise order'
};

// TOP-level REVE command (xfoil.f:176-182). Reads the current session writing-orientation value
// (session::writeClockwise), sets the negated value via
// session::setWriteOrientation and emits the canonical toggle message via
// CliFortranText. REVE is pure output policy (LCLOCK controls only
// the write order in SAVE/PSAV/ISAV/MSAV, xfoil.f:1384/1506); it performs
// no coordinate reversal of buffer or panel, no ABCOPY/PANGEN,
// no analysis invalidation, no second policy copy. The
// productive REVE route (CommandInterpreter::handleReve) stays untouched.
ReveCoreStatus handleReveCore(xfoil::session::SessionComposition& session,
                              std::ostream& out);

// Result of the TOP-level DELI core path. The canonical
// TOP-level DELI command (xfoil.f:185-204) is not a toggle; it sets the
// output delimiter KDELIM (0=blank, 1=comma, 2=tab) from an argument.
// applied: a valid argument (0..2) was adopted silently (no output).
// menuRequested: no argument or an invalid value (<0 or >2); the
// canonical menu (format 2100) was printed, and the value follows via
// the CLI prompt (ASKI). KDELIM is pure output policy
// (SAVE/PSAV/ISAV/MSAV, xfoil.f:1335/1404); DELI changes no geometry,
// no origin, no revision, no analysis state, and neither NORM nor REVE policy.
enum class DeliCoreStatus {
  applied,        // valid argument set (silently)
  menuRequested,  // menu printed; value follows via the prompt
};

// TOP-level DELI command (xfoil.f:185-204). Valid argument (0..2): adopts the value via
// session::setDelimiter and prints nothing. Without an argument or for an
// invalid value: prints the canonical menu via CliFortranText (with the
// current KDELIM) and requires a following prompt value. Reads the current
// session delimiter. No file write, no buffer/panel/metric/analysis
// change, no NORM/REVE change, no second delimiter copy.
// The productive DELI route (CommandInterpreter::handleDelimiter)
// stays untouched.
DeliCoreStatus handleDeliCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, int value, bool hasValue);

// Result of the TOP-level XYCM core path. The canonical
// TOP-level XYCM command (xfoil.f:255-262) sets the CM reference point
// XCMREF/YCMREF. applied: two valid (finite) values were adopted via
// session::setMomentReference (canonically silent, no output).
// promptRequested: fewer than two values or invalid input;
// Fortran then asks for both values per ASKR (a single argument is
// ignored). The ASKR prompts live in the CLI layer; the handler emits
// no text. XYCM changes only the moment reference point:
// no existing result is invalidated or converted, no
// geometry/panel/solver state change. The new point only takes effect in the
// next CLCALC (xoper.f SPECAL/SPECCL); only CM itself changes, Cl/CDP/XCP
// stay equal (xfoil.f CLCALC 1146-1157).
enum class XycmCoreStatus {
  applied,          // both values finite -> set (silent)
  promptRequested,  // <2 values or invalid -> CLI asks for X and Y (ASKR)
};

// TOP-level XYCM command (xfoil.f:255-262). With two finite values
// (negative ones included), both are adopted together via
// session::setMomentReference (transactional, no partial mutation).
// A value missing or not finite: no mutation,
// promptRequested. The current reference point is not read for the mutation; only
// the session use case writes it. No CM formula, no
// AnalysisResult change, no second XCMREF/YCMREF copy, no
// geometry/panel/solver mutation. The productive XYCM route
// (CommandInterpreter::handleXycm) stays untouched.
XycmCoreStatus handleXycmCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, double x, double y,
                              bool hasX, bool hasY);

// Result of the TOP-level NAME core path. applied: a
// non-empty inline argument or a prompt answer was adopted via
// session::setGeometryName. promptRequested: name without
// inline argument and without prompt answer; the canonical NAMMOD requirement was
// printed via CliFortranText, and the answer follows structured. A name
// with inline argument is applied silently, as in Fortran. There is no
// rejection error:
// Fortran truncates the CHARACTER*48 name field and calls STRIP.
enum class NameCoreStatus {
  applied,          // inline argument or prompt answer adopted
  promptRequested,  // name without argument; prompt printed, answer follows
};

// TOP-level NAME command (xfoil.f:344-350). With a non-empty argument: Name = COMARG, STRIP, silently.
// Without an argument: NAMMOD(Name,0,-1) with a prompt requirement; an empty
// answer keeps the current airfoil name, a non-empty answer is adopted as
// the new airfoil name. Only the airfoil
// namespace changes (session::setGeometryName); no buffer coordinates, panel contour,
// revisions, origin, metrics, analysis/wake/BL states, moment
// reference, NORM/REVE/DELI policy, or OPER/polar names.
NameCoreStatus handleNameCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view answer,
                              bool hasAnswer);

// Result of the TOP-level NINC core path. applied: the
// prompt answer was adopted; an empty answer applies the NAMMOD
// requirement. promptRequested: no answer; the NAMMOD requirement was
// output. NINC is not silent: Fortran NAMMOD always writes the prompt.
enum class NincCoreStatus {
  applied,          // Prompt answer adopted (empty: NAMMOD requirement)
  promptRequested,  // no Answer; Prompt output
};

// TOP-level NINC command (xfoil.f:353-355). Calls exclusively the session operations
// geometryNameVersionDefault/incrementGeometryName or setGeometryName;
// no second NAMMOD/NINC logic in the CLI. Increments an existing bracket
// version number [n] by 1 modulo 100; without a valid bracket it defaults to [1].
// Overlong NAMDEF/answer values are truncated to the CHARACTER*48 name
// field, then STRIP. Changes only the profile name, no numeric or
// analysis/file-policy states. 
NincCoreStatus handleNincCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view answer,
                              bool hasAnswer);

// Result of the TOP-level SAVE core path. written: the current
// panel contour was successfully written. filenamePromptRequested: SAVE
// without filename and without prompt answer; the ASKS prompt was output.
// overwriteConfirmationRequired: the target file exists and the answer is missing;
// the overwrite prompt was output. notSavedDeclined: existing
// file rejected with 'N'/'n'; the target file stays unchanged.
// badFilename: empty/invalid path, directory or OPEN/write error;
// canonical Fortran message, no session mutation.
enum class SaveCoreStatus {
  written,
  filenamePromptRequested,
  overwriteConfirmationRequired,
  notSavedDeclined,
  badFilename,
};

// TOP-level SAVE command (xfoil.f:156-157, SUBROUTINE SAVE IFTYP=1).
// SAVE writes exclusively the current
// panel contour (Fortran X/Y/N), not the buffer (XB/YB/NB). The handler reads
// airfoil name (geometryNameHeader), write orientation (LCLOCK) and delimiter
// (KDELIM) from the session and calls the existing AirfoilWriter
// with the explicit labeled format. File path, overwrite answer and
// text remain structured in the CLI layer. SAVE changes no
// session state: no buffer/panel/policy/analysis/wake/BL mutation.
//
SaveCoreStatus handleSaveCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer);

// Result of the TOP-level PSAV core path. written: the current
// panel contour was written without header. filenamePromptRequested,
// overwriteConfirmationRequired, notSavedDeclined and badFilename correspond to
// the canonical file-open/overwrite contract of SUBROUTINE
// SAVE, but with IFTYP=0 and without profile-name header.
enum class PsavCoreStatus {
  written,
  filenamePromptRequested,
  overwriteConfirmationRequired,
  notSavedDeclined,
  badFilename,
};

// TOP-level PSAV command (xfoil.f:160-161, SAVE(0,COMARG)).
// PSAV writes exclusively the current
// panel contour X/Y/N in the plain format: no name line, no ISPARS line,
// no point count. REVE and DELI act as for SAVE. The handler reads only
// source contour and output-path policy from the session and calls the existing
// AirfoilWriter with the explicit plain format. PSAV changes no
// session state.
PsavCoreStatus handlePsavCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer);

// Result of the TOP-level USAV core path. written: the current
// panel contour was written without header as integer coordinates.
// filenamePromptRequested, overwriteConfirmationRequired, notSavedDeclined and
// badFilename correspond to the canonical file-open/overwrite contract
// of SUBROUTINE SAVE, but with IFTYP=-1 and without profile-name header.
enum class UsavCoreStatus {
  written,
  filenamePromptRequested,
  overwriteConfirmationRequired,
  notSavedDeclined,
  badFilename,
};

// TOP-level USAV command (xfoil.f:164-165, SAVE(-1,COMARG)).
// USAV writes exclusively the current
// panel contour X/Y/N in the integerCoordinates format: no name line, no
// ISPARS line, no point count, Format(1X,I12,I12). REVE acts via LCLOCK;
// DELI/KDELIM does not act in the IFTYP=-1 branch. The handler reads only source contour
// and write orientation from the session and calls the existing
// AirfoilWriter with the explicit integerCoordinates format. USAV changes
// no session state.
UsavCoreStatus handleUsavCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer);

// Result of the TOP-level ISAV core path. written: the current
// panel contour was written with airfoil name and ISPARS extra line in the ISES format.
// filenamePromptRequested, overwriteConfirmationRequired,
// notSavedDeclined and badFilename correspond to the canonical
// file-open/overwrite contract of SUBROUTINE SAVE, but with
// IFTYP=2.
enum class IsavCoreStatus {
  written,
  filenamePromptRequested,
  overwriteConfirmationRequired,
  notSavedDeclined,
  badFilename,
};

// TOP-level ISAV command (xfoil.f:168-169, SAVE(2,COMARG)).
// ISAV writes the current panel contour
// X/Y/N in the ISES format: first line Name(1:NNAME), second line
// ISPARS(1:K) right truncated, afterwards coordinates in the SAVE format with
// KDELIM. REVE/LCLOCK and DELI/KDELIM act. The handler reads only
// source contour, airfoil name, ISPARS and output-path policy from the session and calls
// the existing AirfoilWriter with the explicit ises format. ISAV
// changes no session state.
IsavCoreStatus handleIsavCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer);

// Result of the TOP-level MSAV core path (SUBROUTINE MSAVE). written: the
// MSES file was written with the current panel contour as element.
// filenamePromptRequested / elementPromptRequested: the next
// interactive prompt follows (ASKS filename or ASKI element number);
// the interpreter collects the answer and calls the kernel again.
// notWritten: a canonical abort branch (OPEN error, READ error,
// invalid element number) without file change. fatalStop:
// element overflow, canonical STOP 'LOAD: Array overflow' with
// program termination.
enum class MsaveCoreStatus {
  written,
  filenamePromptRequested,
  elementPromptRequested,
  notWritten,
  fatalStop,
};

// TOP-level MSAV command (xfoil.f 172-173, CALL MSAVE(COMARG);
// SUBROUTINE MSAVE xfoil.f 1430-1544). MSAV replaces one element of an
// existing MSES multi-element file with the current panel contour
// X/Y/N (LCLOCK order) and writes the file anew
// (REWIND without an additional overwrite prompt; STATUS='OLD' requires a
// present file). The handler reads only the source contour and LCLOCK from the
// session; the airfoil name and ISPARS come from the file, not from the
// session. DELI/KDELIM has no effect (fixed format-A/1100 branches).
// Session state is not changed.
MsaveCoreStatus handleMsaveCore(xfoil::session::SessionComposition& session,
                                std::ostream& out, std::string_view argument,
                                bool hasArgument,
                                std::string_view filenameAnswer,
                                bool hasFilenameAnswer,
                                std::string_view elementAnswer,
                                bool hasElementAnswer, int& elementDefault);

// Result of the TOP-level BEND core path (SUBROUTINE BENDUMP). reported:
// the numeric report was written. noAirfoil: canonical
// N=0 abort branch ('*** No airfoil available ***'). BEND is a
// pure report path without file output and without session mutation; the
// plot part lives exclusively in the separate command BENP.
enum class BendCoreStatus {
  reported,
  noAirfoil,
};

// TOP-level BEND command (xfoil.f 265-272, CALL BENDUMP(N,X,Y);
// SUBROUTINE BENDUMP xgeom.f 1295-1365). Numerical kernel is the
// existing kernel core::geometry::ijsect; the report lines are written in
// canonical Format(1X,A,G14.6) order. No
// session change, no file output, no plot.
BendCoreStatus handleBendCore(xfoil::session::SessionComposition& session,
                              std::ostream& out);

// Result of the TOP-level RDEF core path (SUBROUTINE GETDEF with
// LASK=.FALSE.). applied: the file was read completely and the
// defaults were applied. notFound: OPEN STATUS='OLD' failed
// (' File <FILNAM> not found'). readError: READ error in a
// line (' File <FILNAM> read error' + ' Settings may have been
// changed'); the already read lines remain canonically effective
// (partial mutation), IDEVRP and MRCL/COMSET are not applied.
enum class RdefCoreStatus {
  applied,
  notFound,
  readError,
  // EOF before the last expected line: canonically no ERR branch,
  // but Fortran runtime error 'End of file' with program abort
  // (verified with gfortran). Controlled deviation: message without
  // backtrace, session end with exit code 2.
  endOfFile,
};

// TOP-level RDEF command (xfoil.f 333-345, CALL GETDEF(LU,FNAME,.FALSE.)).
// No prompt; FNAME = COMARG or 'xfoil.def' (CHARACTER*64 according to
// XFOIL.INC 31; the messages carry the string in its full
// 64-character length). The parser is exclusively io::parseDefaults;
// application is central via session::applyDefaults (complete) or
// raw partial assignment (error case).
RdefCoreStatus handleRdefCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument);

// Result of the TOP-level WDEF core path (SUBROUTINE WDEF branch
// xfoil.f 309-331). written: the file was written (in the
// canonical Fortran flow also after the 'No action taken' rejection,
// since the rejection branch without RETURN still falls through on OPEN
// STATUS='UNKNOWN'). promptRequested: the file exists and the
// overwrite prompt (format 701) is issued. openError: OPEN
// STATUS='UNKNOWN' failed. The canonical Fortran path aborts here; this
// implementation ends the session as a controlled deviation.
enum class WdefCoreStatus {
  written,
  promptRequested,
  openError,
};

// TOP-level WDEF command (xfoil.f 309-331). FNAME = COMARG or
// 'xfoil.def', then CALL STRIP. The writer is exclusively
// io::writeDefaults from session::defaults (INIT start values or last
// read/applied values). The canonical rejection peculiarity
// (answer N/n: 'No action taken', afterwards writing still happens)
// is replicated faithfully to the original.
WdefCoreStatus handleWdefCore(xfoil::session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view answer,
                              bool hasAnswer);

// Result of the INTE core path. success: result buffer was
// set. The error values correspond to the canonical INTE abort branches;
// for each error value the session state stays unchanged.
enum class InteCoreStatus {
  success,
  fileOpenError,
  bufferOverflow,
  readError,
  invalidGeometry,
  interpolationFailed,
  invalidSelection,
  inputAborted,
};

// INTE command (xfoil.f INTE, xgeom.f INTER, aread.f AREAD, xgeom.f
// SCALC/SEGSPLD/GEOPAR). Orchestrates the interactive original flow with
// an injected input stream: source menu, per source the selection line
// (F/C; numeric polar sources are canonically 'Invalid
// response' abort for NPOL=0), for F the filename prompt with AREAD, for C the
// current contour (N<=1: 'No current airfoil available' with renewed
// selection), source names, fraction prompt (ASKR, requirement 0.5), INTER/GEOPAR
// display, name prompt (ASKS) and buffer adoption without ABCOPY/PANGEN.
// frac corresponds to the canonical ASKR value (Fortran accepted every
// real value incl. extrapolation; no range check).
InteCoreStatus handleInteCore(xfoil::session::SessionComposition& session,
                              std::istream& in, std::ostream& out);

// Result of the TOP-level INTX core path (xfoil.f INTX,
// xgeom.f INTERX/SCALC/SEGSPLD/LEFIND/GEOPAR, aread.f AREAD).
// success: interpolation completed, buffer set, name set.
// The error values correspond to the canonical INTX abort branches.
// Partial-mutation contract (canonical): the buffer mutation already happens
// before the name prompt (xfoil.f 2566-2580); an EOF at the name prompt
// leaves the mutated buffer in place (inputAborted without rollback).
// On source/interpolation errors the buffer stays unchanged.
enum class IntxCoreStatus {
  success,
  fileOpenError,
  bufferOverflow,
  readError,
  invalidGeometry,
  interpolationFailed,
  invalidSelection,
  inputAborted,
};

// TOP-level INTX (xfoil.f 229-230, SUBROUTINE INTX xfoil.f 2461-2593),
// sequence: source menu (format 1100/1200
// with polar slots for NPOL>0), per source selection (F/C/polar index;
// READ with ERR=90/END=90 -> 'Invalid response. No action taken.'),
// for F filename+AREAD (ITYPE=0: silent return), for C guard
// N<=1 with reprompt, polar source via the authoritative polar slot
// (CPOLXY contour, not the point list), source names, FRAC prompt
// (ASKR, requirement 0.5, unvalidated incl. extrapolation), INTERX commit
// as a pure buffer mutation (panel/current/CPI unchanged),
// GEOPAR thickness/camber, name prompt (ASKS, canonically only after the
// buffer mutation), success and hint texts. No paneling,
// no ABCOPY, no numerics in the handler.
IntxCoreStatus handleIntxCore(xfoil::session::SessionComposition& session,
                              std::istream& in, std::ostream& out);

// Result of the TOP-level HALF core path (xfoil.f 242-252,
// xgeom.f HALF). applied: buffer contour decimated, SCALC/SEGSPL/GEOPAR
// refreshed. bufferUnavailable: no valid buffer with N >= 2.
// decimationFailed: core precondition (degenerate contour); state
// unchanged. HALF is canonically silent and changes exclusively
// the buffer: no paneling, no invalidation, no name.
enum class HalfCoreStatus {
  applied,
  bufferUnavailable,
  decimationFailed,
};

// TOP-level HALF (xfoil.f 242-252).
// Orchestrated exclusively by the session use case
// applyHalfBufferDecimation (core kernel core::geometry::half with
// vertex-preserving in-place index logic, afterwards SCALC/SEGSPL/GEOPAR).
// GEOPAR format 1000 (xgeom.f 377-379) is handed over; no panel/
// analysis/CPI mutation.
HalfCoreStatus handleHalfCore(xfoil::session::SessionComposition& session,
                              std::ostream& out);

// Result of the PLOP menu path (TOP PLOP -> OPLSET,
// plutil.f 21-176). returned: return to the TOP menu via '0'/empty;
// endOfFile: input stream at end (canonical READ error without ERR branch;
// controlled session termination as in the remaining ASK paths).
enum class PlotOptionsCoreStatus {
  returned,
  endOfFile,
};

// TOP-level PLOP (xfoil.f 302-306) via OPLSET. The handler
// prints the menu table (format 2000), reads options as ASKC and applies
// each option exclusively via the session operations of the
// design/defaults states: G (IDEV), S/A/P/M/F/W (numeric
// DefaultsSessionState values), B (LCURS), O (LLAND plus XPAGE/YPAGE swap),
// C (IDEVRP 4/2), L (ICOLS with clamp 1..10). Unknown options report
// ' *** Item not recognized ***' without state change. There is no
// drawing, no cursor interaction, no axis calculation, no
// geometry/panel/analysis mutation.
PlotOptionsCoreStatus handlePlotOptionsCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out);

// Result of the GDES menu path (TOP GDES -> SUBROUTINE GDES,
// xgdes.f 21-949). returned: return to the TOP menu via a blank line;
// endOfFile: input stream at end (canonical READ abort; controlled
// session termination as in the remaining ASK paths).
enum class GeometryDesignCoreStatus {
  returned,
  endOfFile,
};

// TOP-level GDES (xfoil.f TOP dispatch, xgdes.f).
// The handler runs the menu loop (entry text, prompt
// '.GDES c>'/'.GDESs c>', help '?', unknown command, '<Return>'
// exit with warning, '!' redo via COMOLD/ARGOLD) and calls for the
// direct, non-cursor-bound commands exclusively the
// session use cases from SessionOps.h. Plot/cursor commands remain
// recognized-but-unavailable. No numeric logic in the handler, no geometry in the
// interpreter, no plot backend.
GeometryDesignCoreStatus handleGeometryDesignCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out);

// CAMB submenu (xtcam.f). The handler runs the
// menu loop (entry with LGSYM/LPLCAM messages, rotation warning,
// format-1200 help, block-100/200 pass, prompt '..CAMB c>',
// help '?', unknown command format 8000, '<Return>' exit with
// LGPARM restoration, '!' redo via CAMB COMOLD/ARGOLD - exclusively
// SCAL sets it, canonical peculiarity) and calls for TFAC/TSET/HIGH,
// SETC/INPC/INPP/CLR/SCAL/ADD/DCPL/SLOP exclusively the
// session use cases from SessionOps.h. Plot/cursor commands (MODC/MODP/
// CPLI/BLOW/RESE/SIZE/ANNO/HARD/Z/U) remain recognized-but-unavailable.
// No numeric logic in the handler,
// no geometry in the interpreter, no plot backend.
enum class CamberDesignCoreStatus {
  returned,
  endOfFile,
};
CamberDesignCoreStatus handleCamberDesignCore(
    xfoil::session::SessionComposition& session, std::istream& in,
    std::ostream& out);

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreGeometryHandlers.h

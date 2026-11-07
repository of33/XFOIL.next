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

// File: include/xfoil/session/SessionOps.h
// Session/use-case operations.
// Each operation orchestrates only the confirmed core kernels in
// canonical Fortran order and the documented read/mutated
// domain states, invalidations, error behavior and Fortran path.
//
// Fortran path per operation:
// setGeometry xfoil.f LOAD-buffer path + xgdes.f ABCOPY (without PANE)
// configure xfoil.f INIT operating parameters (MRCL inputs)
// initializeAnalysis xfoil.f INIT (LBLINI/LIPAN reset, GGCALC)
// runSpecAlpha xoper.f SPECAL + VISCAL (xoper.f 2892-3116)
// + xoper.f FCPMIN (OPER VISC share)
// runSpecCl xoper.f SPECCL + VISCAL (LALFA=false branch)
// analyzePolar: OPER alpha-sequence orchestration of the point operation
// resetBoundaryLayer xoper.f INIT (LBLINI=false, LIPAN=false, LVCONV=false)
//
// Dependencies: xfoil::core, the neutral value-type header
// xfoil/SessionTypes.h (GeometrySetResult) and documented public
// value-type headers (AnalysisConfig/AnalysisResult/Types/CancellationToken/
// AirfoilGeometry). No dependency on the public facade
// (xfoil/XFoilSession.h), no app, CLI or test include.
// No second numerical implementation: the xcp diagnostic quantity (no
// canonical Fortran field) comes from the confirmed
// 0-based core kernel clCalc (LoadResult.centerOfPressureX,
// in-loop term, bit-identical to the previous public
// API path — proof tests/core/XcpDerivationTests.cpp).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/SessionTypes.h"
#include "xfoil/Types.h"
#include "xfoil/core/Result.h"
#include "xfoil/core/boundary_layer/Coupling.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/geometry_design/CamberSession.h"
#include "xfoil/core/geometry_design/GeometryDesignOps.h"
#include "xfoil/core/geometry_design/ThicknessCamberOps.h"
#include "xfoil/core/inverse/CircleGeometry.h"
#include "xfoil/core/paneling/PanelParameters.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/io/ExportFormats.h"
#include "xfoil/session/SessionState.h"

namespace xfoil::session {

// Simple status for session operations without their own result object.
// OK==false: the session state was not changed.
struct SessionOperationStatus {
  bool ok{true};
  std::string message;
};

// Result of applyPaneling (PANGEN): panel contour plus the structured
// PANGEN diagnostics the CLI needs for visible output
// (sharp LE, TE gap, corner node, Newton limit). OK==false: the current
// panel contour stays unchanged.
struct PanelingApplyResult {
  bool ok{false};
  std::string message;
  core::paneling::PangenResult pangen;
};

// Fortran LOAD/buffer path: validates the geometry and adopts it into
// GeometrySessionState, then derives the current panel geometry via
// core::paneling::abcopy from it (canonical without PANE). For any error,
// the existing geometry, panel geometry, BL state and configuration
// remain unchanged (no partial mutation). Invalidated: PanelingSessionState,
// viscous session state (influence system, BL state, convergence),
// AnalysisResultState.
[[nodiscard]]
GeometrySetResult setGeometry(SessionComposition& session,
                              geometry::AirfoilGeometry geometry);

// Fortran INIT operating parameters: validates the configuration and adopts
// Mach/Re (MATYP/RETYP/MINF1/REINF1), viscosity, iteration limit, BL
// reinitialization choice, VACCEL (instance-specific), XTR/NCRIT. For a
// validation error the previous configuration remains unchanged. No
// solver computation. Invalidated: viscous session state (influence system
// because of the MINF dependency, BL state according to Fortran INIT/SETBL
// LBLINI semantics, convergence), AnalysisResultState.
[[nodiscard]]
ConfigValidationResult configure(SessionComposition& session,
                                 const AnalysisConfig& config);

// Fortran INIT: resets the BL initialization state (LBLINI=false,
// LIPAN=false) and prepares the inviscid base state (GGCALC influence
// system). Returns true if the analysis is ready. Error behavior:
// without geometry, false without partial mutation.
bool initializeAnalysis(SessionComposition& session);

// Fortran SPECAL + VISCAL: viscous or inviscid single point with
// prescribed alpha. Reads: GeometrySessionState, PanelingSessionState,
// OperatingConditionState, viscous session state. Mutates: viscous session
// state (BL state per point according to Fortran LIPAN/LBLINI reset),
// AnalysisResultState. Abort: the token is evaluated at the canonical
// abort points (before the analysis, per polar point); the
// Newton loop itself is a confirmed atomic core path.
[[nodiscard]]
AnalysisResult runSpecAlpha(SessionComposition& session, Degrees alpha,
                            const CancellationToken* cancellation);

// Fortran SPECCL + VISCAL (LALFA=false branch): single point with prescribed
// inviscid Cl. Otherwise the same state and error semantics as runSpecAlpha.
[[nodiscard]]
AnalysisResult runSpecCl(SessionComposition& session, double clSpec,
                         Degrees startAlpha,
                         const CancellationToken* cancellation);

// ---: OPER single-point contracts (ALFA/CLI/CL) ---------------------------
// Only the expressed canonical contract. All
// three use the same factored point kernel, but with the OPER
// single-point warm start (PointExecutionPolicy::operSinglePointWarmStart):
// LIPAN/LBLINI are preserved across single points, wake/LVCONV are reset
// only for |Alfa-AWAKE|, |Alfa-AVISC| or |MINF-MVISC| > 1e-5
// (invalidation after the inviscid solution, xoper.f 430-432/487-489/523-525),
// VISCAL(ITMAX) (xoper.f 435/492/527), no NSEQEX. Does not change the
// API/batch cold path (runSpecAlpha/runSpecCl/runAlphaSequence) or the
// OPER sequence warm start (runOperAlphaSequence/runOperClSequence).

// Fortran OPER Alfa (xoper.f 416-437): SPECAL, LALFA=true, for LVISC
// VISCAL(ITMAX). Target angle of attack given.
[[nodiscard]]
AnalysisResult runOperAlphaPoint(SessionComposition& session, Degrees alpha,
                                 const CancellationToken* cancellation);

// Fortran OPER CLI (xoper.f 475-507): SPECCL, LALFA=true, no SPECAL afterwards,
// for LVISC VISCAL(ITMAX). SPECCL Newton starts at Alfa=0.0 (Fortran sets
// Alfa=0.0 before SPECCL); the SPECCL result alpha is updated.
[[nodiscard]]
AnalysisResult runOperCliPoint(SessionComposition& session, double clSpec,
                               const CancellationToken* cancellation);

// Fortran OPER Cl (xoper.f 510-542): SPECCL, LALFA=false, no SPECAL afterwards,
// for LVISC VISCAL(ITMAX). SPECCL Newton starts at Alfa=0.0; the
// SPECCL result alpha is updated.
[[nodiscard]]
AnalysisResult runOperClPoint(SessionComposition& session, double clSpec,
                              const CancellationToken* cancellation);

// Fortran LRECALC continuation (xoper.f 85-92/416-542): after a
// non-converged viscous single point, this operation restarts only
// VISCAL(ITMAX) on the preserved point, wake, mapping, DIJ and BL state,
// without calling SPECAL/SPECCL again and without a cold start. It
// restores the inviscid point state (TECALC/LALFA/CLSPEC) persisted
// by the last SPECAL/SPECCL point. A non-continuable state
// (inviscid, no previous viscous point, no valid wake) returns
// a controlled error status without partial mutation. Abort is instance-specific.
// Does not change the contracts of runSpecAlpha/runSpecCl/runAlphaSequence/
// runClSequence or of the batch/API paths.
[[nodiscard]]
AnalysisResult continueViscousPoint(SessionComposition& session,
                                    const CancellationToken* cancellation);

// OPER alpha sequence (exclusive end boundary, Fortran NSEQEX abort, limit
// 100000): runs runSpecAlpha per point. A requested abort returns
// a cancelled result and terminates; a cancelled result supplied by
// runSpecAlpha is retained and also terminates.
[[nodiscard]]
std::vector<AnalysisResult> runAlphaSequence(SessionComposition& session,
                                             Degrees start,
                                             Degrees endExclusive, Degrees step,
                                             const CancellationToken*
                                                 cancellation);

// Fortran INIT (xoper.f): invalidates BL state and mapping (LBLINI=false,
// LIPAN=false), resets convergence. No partial mutation of the geometry,
// panel or operating state.
void resetBoundaryLayer(SessionComposition& session);

// ---: geometry/panel producers --------------------------------------------

// Fortran LOAD-buffer adoption without ABCOPY: adopts only the
// buffer contour, increments bufferRevision, computes no panel contour and
// invalidates panel, influence matrix, wake, result and BL. Panel parameters
// are retained. Error (empty/point-count limit) without partial mutation.
[[nodiscard]]
GeometrySetResult setBufferGeometry(SessionComposition& session,
                                    core::geometry::AirfoilBuffer buffer);

// Pure buffer mutation according to the
// canonical INTX-/HALF contract. Fortran INTX (xfoil.f 2566-2580) and
// TOP-HALF (xfoil.f 242-252) write exclusively XB/YB/NB and
// re-derive SB/XBP/YBP and the buffer GEOPAR values; X/XP/Y/YP/S/N
// (panel contour), LCONF, analysis, wake, BL, mapping, CPI/CPV, polar and
// inverse states remain unchanged. No ABCOPY, no PANGEN.
// The airfoil name is set by INTX only after the buffer mutation via ASKS
// (canonical order; separate setGeometryName call).
// If the GDES working buffer is active (design.work), its
// derived quantities are refreshed as in Fortran (shared SB/XBP/YBP).
struct BufferOnlyGeometryResult {
  bool ok{false};
  std::size_t pointCount{0};
  // SCALC/SEGSPL/GEOPAR of the new buffer (Fortran GEOPAR output source;
  // the core writes nothing to stdout).
  core::geometry::GeometryMetrics bufferMetrics;
};

// INTX: two-airfoil interpolation via the confirmed core kernel
// interX (xgeom.f INTERX). The source preprocessing corresponds to the
// Fortran INTX call (SCALC, SEGSPLD with end derivatives -999,-999,
// LEFIND). FRAC is unvalidated (extrapolation permitted). Errors
// (too short sources, capacity limit, core exception) without
// buffer mutation.
[[nodiscard]]
BufferOnlyGeometryResult applyIntxBufferInterpolation(
    SessionComposition& session,
    std::vector<double> x0, std::vector<double> y0,
    std::vector<double> x1, std::vector<double> y1, double frac);

// HALF: corner-preserving decimation of the buffer contour via the
// confirmed core kernel half (xgeom.f HALF, in-place index logic),
// followed by SCALC/SEGSPL/GEOPAR. Error (no buffer, N < 2,
// core exception) without buffer mutation.
[[nodiscard]]
BufferOnlyGeometryResult applyHalfBufferDecimation(
    SessionComposition& session);

// Fortran ABCOPY (xgdes.f): sets the current panel contour directly from the
// buffer contour, origin abcopy, panelSourceRevision = current
// buffer revision; invalidates influence matrix, wake, result and BL. Buffer
// remains preserved. Error without partial mutation.
[[nodiscard]]
SessionOperationStatus copyBufferToCurrent(SessionComposition& session);

// Fortran GDES 'eXec'/'X' (xgdes.f 197-202) calls ABCOPY(LCONF=.TRUE.)
// (xgdes.f 961-1035). This use-case encapsulation is the product
// route for EXEC; it checks the
// canonical ABCOPY preconditions before the commit (NB <= 1: buffer not available;
// NB > IQX-5 = 365: capacity limit) and only then runs the
// confirmed copyBufferToCurrent commit. Success sets LGSAME and
// transfers LBFLAP/XBF/YBF -> LFLAP/XOF/YOF (xgdes.f 982-988).
// Error without any partial mutation: panel contour, analysis, revisions and
// design buffer remain unchanged.
struct ExecuteGeometryDesignResult {
  bool ok{false};
  bool bufferUnavailable{false};  // NB <= 1 (xgdes.f 965-967)
  bool capacityExceeded{false};   // NB > IQX-5 (xgdes.f 968-972)
  std::size_t bufferPointCount{0};
  std::size_t maxPointCount{0};   // IQX-5
};
[[nodiscard]]
ExecuteGeometryDesignResult executeGeometryDesignBuffer(
    SessionComposition& session);

// Fortran PANGEN (xfoil.f): produces the current panel contour from buffer and
// session panel parameters, origin pangen; invalidates influence matrix,
// wake, result and BL. Does not change the buffer. Error (buffer missing,
// too many panels) without partial mutation; the PANGEN diagnostics are
// returned.
[[nodiscard]]
PanelingApplyResult applyPaneling(SessionComposition& session);

// Fortran GETPAN parameter adoption: stores the panel parameters after
// validation. An existing panel contour is not regenerated
// (canonical PPAR semantics: effect only at the next PANGEN/Apply).
// Error (panelCount < 2) without partial mutation.
[[nodiscard]]
SessionOperationStatus setPanelingParameters(
    SessionComposition& session,
    const core::paneling::PanelingParameters& parameters);

// Fortran GETPAN parameter command (N/P/T/R/XT/XB) via the confirmed
// core kernel applyPanelParameter (incl. clamp on IQX-6 for N). Stores the
// new parameter state; no panel regeneration.
[[nodiscard]]
SessionOperationStatus setPanelParameter(
    SessionComposition& session, core::paneling::PanelParameterKind kind,
    std::span<const double> values);

// Read operations for the later A3/analysis consumer side and CLI
// formatting. currentPanelGeometry returns exclusively the current
// valid panel contour (no ABCOPY re-derivation).
const core::geometry::AirfoilBuffer& currentBufferGeometry(
    const SessionComposition& session);
const core::paneling::PanelGeometry& currentPanelGeometry(
    const SessionComposition& session);
const core::paneling::PanelingParameters& panelingParameters(
    const SessionComposition& session);
PanelingOrigin panelingOrigin(const SessionComposition& session);
bool panelingValid(const SessionComposition& session);

// Geometry metrics (THICKB/CAMBRB/LE/Chord) exclusively from the
// confirmed core kernel geopar on the current valid panel contour.
// Returns nullopt if no valid panel contour is present.
std::optional<core::geometry::GeometryMetrics> geometryMetrics(
    const SessionComposition& session);

// Airfoil name (NAMMOD/Name): no numerical state, no invalidation.
// The NAMMOD version logic (applyVersionTag) lives in the confirmed kernel and
// is called by the presenting CLI loop; the result is stored here
// as the session name. The final name field carries the canonical
// CHARACTER*48 truncation and STRIP trimming.
void setGeometryName(SessionComposition& session, const std::string& name);

// NAMMOD prompt requirement (NAMDEF) as CHARACTER*48 field: only truncated
// to 48 characters, not yet STRIP-trimmed. kDel/kMod0 correspond to NAMMOD:
// name without argument (0,-1), NINC (1,1). On a read error of the bracket content,
// the operation returns the canonical Fortran requirement (current name).
[[nodiscard]] std::string geometryNameVersionDefault(
    const SessionComposition& session, int kDel, int kMod0);

// Applies the NAMMOD requirement for an empty prompt answer. Changes only the
// profile name field (name/buffer name/geometry name), no panel, metric,
// analysis or output-path policy state.
void incrementGeometryName(SessionComposition& session, int kDel, int kMod0);

// Canonical SAVE/PSAV/ISAV header field Name(1:NNAME). Before the first
// name assignment it returns Fortran 32 spaces (INIT: NNAME=32); after a
// STRIP assignment the field may be empty. The operation reads only the
// session state and changes nothing.
[[nodiscard]] std::string geometryNameHeader(const SessionComposition& session);

// Fortran XYCM: CM reference point; invalidated only load-dependent results,
// not geometry or paneling.
void setMomentReference(SessionComposition& session, double x, double y);

// ---: GDES-/CAMB design state machine --------------------------------------
//
// The following operations prepare the shared design state
// (design session state) beforehand. They mutate exclusively the
// design session state: no buffer mutation, no panel contour, no
// analysis invalidation, no bufferRevision change, no ABCOPY/PANGEN.
//
// Canonical entry (xgdes.f 21-51): GDES checks NB==0
// ('*** No airfoil available ***'), sets LPLCAM=.FALSE. and works
// then directly on the existing buffer (COMMON CR14). There is
// no geometry snapshot and no copy: the working geometry stays
// session.geometry.buffer; the commit boundary is ABCOPY.

enum class GeometryDesignInitStatus {
  initialized,  // entry according to Fortran (NB >= 1)
  noAirfoil,    // NB == 0: Fortran message, no state transition
};

struct GeometryDesignInitResult {
  GeometryDesignInitStatus status{GeometryDesignInitStatus::noAirfoil};
};

// Fortran GDES entry (xgdes.f 43-51): initializes the design state
// (menu.active, LPLCAM=.FALSE., entryBufferRevision), leaves LGSAME/LGSYM
// and all remaining design flags unchanged (canonical) and changes
// neither buffer, panel, analysis nor revisions. Error case (no contour)
// without any partial mutation.
[[nodiscard]]
GeometryDesignInitResult initializeGeometryDesign(SessionComposition& session);

// Fortran GDES exit per <cr> (xgdes.f 72-84): sets LGSYM=.FALSE. and
// ends the menu level; for .NOT.LGSAME the CLI later prints the
// warning 'Buffer airfoil is not identical to current airfoil'. Buffer,
// panel, analysis and revisions remain unchanged (no commit).
// Return value: lgsame for the canonical warning decision.
struct DesignExitResult {
  bool lgsame{false};
};
[[nodiscard]]
DesignExitResult leaveGeometryDesignWithoutCommit(SessionComposition& session);

// CAMB entry safeguard (xtcam.f 41): LGPARSAVE = LGPARM before the
// CAMB block; CAMB exit (xtcam.f 178-184) restores LGPARM and
// keeps LPLCAM active. This operation only models the state frame;
// the CAMB block-100 path (CamberSession initialization) is not part of it.
void savePlotParameterForCamberEntry(SessionComposition& session);
void restorePlotParameterAfterCamberExit(SessionComposition& session);

// ---: CAMB sub-menu use cases ----------------------------------------------
//
// CAMB (xtcam.f) works exclusively on the shared
// design session state: GDES working buffer (geometry.buffer + design.work),
// CAMB domain state (design.camber as CamberSession) and the flags
// already maintained here (LPLCAM, LGSYM, LDCPLOT, LCSLOP, LGPARM,
// LGPARSAVE). No second geometry memory, no panel/analysis
// invalidation, no TOP buffer commit.

// CAMB entry (xgdes.f 487-492 + xtcam.f 43-75):
// * LGSYM deactivation with message (xgdes.f 488-490),
// * LGPARSAVE = LGPARM (xtcam.f 41),
// * LPLCAM enable message for inactive LPLCAM (xtcam.f 53-57),
// * rotation check |AROT| > 1 deg from SBLE/SEVAL/XB (xtcam.f 61-71),
// * LDCPLOT = .FALSE., LGPARM = .NOT.LDCPLOT (xtcam.f 74-75),
// * CAMB 100 block with LCLEAR =.TRUE. (CamberSession initialization).
struct CamberEntryResult {
  bool noBuffer{false};            // no active GDES working buffer
  bool symmetryDisabled{false};    // 'Disabling symmetry enforcement.'
  bool enablingCamberPlot{false};  // 'Enabling camber,thickness plotting'
  bool rotationWarning{false};     // 'Warning: CAMB does not work well...'
  double arot{0.0};                // 'Current chordline angle: ' AROT
};
[[nodiscard]]
CamberEntryResult initializeCamberDesign(SessionComposition& session);

// CAMB exit per <Return> (xtcam.f 181-188): LGPARM = LGPARSAVE;
// LPLCAM stays active; buffer/panel/analysis unchanged.
void leaveCamberDesign(SessionComposition& session);

// Block-200 report data (xtcam.f 136-156): TCBUF (GETCAM + GETMAX on
// the working buffer) and added-camber incidence report from the
// CAMB domain state. Pure read access, no numerics repetition.
struct CamberBlock200Report {
  bool camberActive{false};
  core::geometry_design::ThicknessCamberDistribution distribution;
  core::geometry_design::MaxPoint camberMax;   // GETMAX XCM/YCM -> CYMAX/CXMAX
  core::geometry_design::MaxPoint thicknessMax;// GETMAX XTK/YTK -> TYMAX/TXMAX
  // GETMAX Newton failure (Diagnostic::limitReached) leads in Fortran
  // to the canonical diagnostic line before the max-values block.
  bool camberMaxFailed{false};
  bool thicknessMaxFailed{false};
  double aleDeg{0.0};            // Added-camber incidence for LE (F6.2)
  double ateDeg{0.0};            // Added-camber incidence for TE (F6.2)
  double maxAddedCamberYc{0.0};  // F8.4
  double maxAddedCamberXc{0.0};  // F7.3
};
[[nodiscard]]
CamberBlock200Report camberBlock200Report(const SessionComposition& session);

// CAMB 'SETC' (xtcam.f 378-394): added camber := current camberline,
// LDCPLOT = .FALSE.
void camberSetCamberline(SessionComposition& session);

// CAMB 'INPC' (xtcam.f 397-443): explicit x/c,y/c points (keyboard-
// READ equivalent), x/c clamp, SORTDUP/FIXDUP, LDCPLOT =.FALSE.
void camberInputCamberPoints(
    SessionComposition& session,
    std::span<const core::geometry_design::CamberInputPoint> points);

// CAMB 'INPP' (xtcam.f 446-526): x/c,DCp points on the existing
// added-loading append, CPCAM, CLX/CMX, LDCPLOT = .TRUE.
[[nodiscard]]
core::geometry_design::LoadingIntegrals camberInputLoadingPoints(
    SessionComposition& session,
    std::span<const core::geometry_design::CamberInputPoint> points);

// CAMB 'CLR' (xtcam.f 591-595): LCLEAR = .TRUE., LDCPLOT = .FALSE.,
// 100 block again.
void camberClear(SessionComposition& session);

// CAMB 'SCAL' (xtcam.f 598-640): added-camber/loading scale,
// recomputes CLX/CMX. COMOLD/ARGOLD are handled by the handler.
[[nodiscard]]
core::geometry_design::LoadingIntegrals camberScale(
    SessionComposition& session, double scal);

// CAMB 'ADD' (xtcam.f 643-683): applies added camber to the GDES working buffer
// (SOPPS/SEVAL, YB update, SCALC/SEGSPL x2, GEOPAR), then
// LCLEAR =.TRUE. The return value holds the GEOPAR metrics and the SOPPS failures
// of the ADD point loop for the CLI pressure output. Current panel contour and
// analysis remain unchanged.
struct CamberAddResult {
  core::geometry::GeometryMetrics metrics;
  std::size_t addLoopOppositePointFailures{0};
};
[[nodiscard]]
core::Result<CamberAddResult> camberAdd(SessionComposition& session);

// CAMB 'RDAC' (xtcam.f 330-372): adopts the x/c,y/c pairs read by the io codec
// (xfoil::io::decodeAddedCamberFile) into the
// CAMB state. Transactional with respect to the file: the reader has
// already parsed the file completely; the session mutation happens
// only here. Fortran partial-mutation contract: on a parse error in the
// middle of the file, the handler returns the points read up to that point;
// the kernel applies them canonically. noPointsFound corresponds to the
// Fortran branch NCADD <= 1 / x span 0 (zero camber + message,
// menu continues).
using CamberRdacStatus = core::geometry_design::RdacCamberStatus;
[[nodiscard]]
CamberRdacStatus camberReadAddedCamber(
    SessionComposition& session,
    std::span<const core::geometry_design::CamberInputPoint> points);

// CAMB 'WRTT'/'WRTC' (xtcam.f 257-328): pure readout of the current
// thickness/camber distribution of the GDES working buffer (GETCAM chain as
// in the block-200 report) for the io codec. Does not mutate the session.
struct CamberDistributionProjection {
  std::string name;      // canonical airfoil name (CHARACTER*48 reference)
  double xbch{0.0};      // CAMB chord length XBCH
  std::vector<double> x; // XTK or XCM
  std::vector<double> y; // YTK or YCM
};
[[nodiscard]]
std::optional<CamberDistributionProjection> camberThicknessProjection(
    const SessionComposition& session);
[[nodiscard]]
std::optional<CamberDistributionProjection> camberCamberProjection(
    const SessionComposition& session);

// CAMB 100 block with LCLEAR = .FALSE. after TFAC/TSET/HIGH (xtcam.f 242-254
// -> GO TO 100): renews the geometry reference/XCAM grid, added state stays.
void camberRefresh(SessionComposition& session);

// ---: PLOP menu state (OPLSET, plutil.f 21-176) ---------------
//
// Each operation changes exactly the COMMON values addressed by OPLSET and
// invalidates nothing. The numerical values live in the DefaultsSessionState
// (single storage location for the xfoil.def fields), the OPLSET extra fields in the
// design session state.plot. No plot backend, no axis computation.
void togglePlotGraphics(SessionComposition& session);   // OPLSET 'G': IDEV 0<->1
void setPlotSize(SessionComposition& session, double size);          // SIZE
void setPlotAspect(SessionComposition& session, double aspect);      // PLOTAR
void setPlotPage(SessionComposition& session, double xpage, double ypage);
void setPlotMargins(SessionComposition& session, double xmarg, double ymarg);
void setPlotFont(SessionComposition& session, double font);          // CH
void setPlotScreenFraction(SessionComposition& session, double scrnfr);
void togglePlotBlowupInput(SessionComposition& session);             // LCURS
// OPLSET 'O': LLAND clamp and XPAGE/YPAGE swap (canonical coupled).
void togglePlotOrientation(SessionComposition& session);
// OPLSET 'C': LCOLOR clamp; IDEVRP = 4 for color, otherwise 2 (OPLSET 148-150).
void togglePlotColor(SessionComposition& session);
// OPLSET 'L': ICOLS(1..2) with canonical clamp on 1..10 (OPLSET 168-169).
void setPlotLineColors(SessionComposition& session, int top, int bottom);

// ---: GDES working-buffer use cases ----------------------------------------
//
// Shared contract (xgdes.f): each direct numerical GDES command
// mutates only the buffer geometry (XB/YB), the derived
// working quantities (SB/XBP/YBP, GEOPAR parameters), LGSAME and - where
// Fortran sets it - LGSYM/flap state. The buffer revision increases on
// successful mutation. The panel contour, panel provenance,
// panelSourceRevision, wake, mapping, BL, system cache and lastResult
// remain completely preserved: they depend on the unchanged current
// panel contour; only EXEC/ABCOPY swaps it.
// Error cases leave the session state unchanged. No
// ABCOPY, no PANGEN, no analysis invalidation.
// Text/prompt handling lives in the CLI, not in the session.
struct GeometryDesignOpResult {
  bool ok{false};
  std::string message;  // canonical message for a controlled abort
};

// Fortran GDES 'GSET' (xgdes.f 175-194): buffer := current panel contour
// (NB=N, copy, LGSAME=.TRUE., SCALC/SEGSPL x2, GEOPAR; for active
// LGSYM additionally ZERCAM). Source is the current contour, not the
// buffer (GSET is not PCOP). Error without partial mutation.
// Return value: metrics of the GSET's own GEOPAR output (incl.
// SOPPS counter) and ZERCAM sub-results, if LGSYM was active.
struct GeometryDesignGsetResult {
  bool ok{false};
  std::string message;  // canonical message for controlled abort
  core::geometry::GeometryMetrics metricsAfterGeopar;
  std::optional<core::geometry_design::ZerCamberResult> zercam;
};
[[nodiscard]]
GeometryDesignGsetResult gdesSetBufferFromCurrent(
    SessionComposition& session);

// Fortran GDES 'SYMM' (xgdes.f 204-211): LGSYM toggle; for activation
// ZERCAM on the working buffer. Return value: new LGSYM state and,
// for activation, the ZERCAM sub-results (THKCAM SOPPS counter and
// metrics of the first GEOPAR print; the second lies in the working buffer).
struct GeometryDesignSymmetryResult {
  bool ok{false};
  bool lgSym{false};
  std::optional<core::geometry_design::ZerCamberResult> zercam;
};
[[nodiscard]]
GeometryDesignSymmetryResult gdesToggleSymmetry(
    SessionComposition& session);

// Fortran GDES 'ADEG'/'ARAD' (xgdes.f 215-247): rotation around the
// origin; canonical without SCALC (only SEGSPL x2), APX angle -= ARAD,
// LGSAME unchanged.
[[nodiscard]]
GeometryDesignOpResult gdesRotate(SessionComposition& session, double arad);

// Fortran GDES 'TRAN'/'T' (xgdes.f 250-274): shift, LGSAME=false.
[[nodiscard]]
GeometryDesignOpResult gdesTranslate(SessionComposition& session, double delx,
                                     double dely);

// Fortran GDES 'SCAL'/'S' (xgdes.f 277-334): scaling around the
// origin; a negative factor product reverses the point order,
// SCALC/SEGSPL x2/GEOPAR, LGSAME=false.
[[nodiscard]]
GeometryDesignOpResult gdesScale(SessionComposition& session, double xxfac,
                                 double yyfac);

// Fortran GDES 'LINS' (xgdes.f 337-386): linearly variable
// y-scaling; |XOC1-XOC2| < 1E-5 or degenerate chord without mutation.
[[nodiscard]]
GeometryDesignOpResult gdesLinearYScale(SessionComposition& session,
                                        double xoc1, double yfac1,
                                        double xoc2, double yfac2);

// Fortran GDES 'DERO' (xgdes.f 389-414): derotation; return value ARAD
// (CLI prints 'Rotating buffer airfoil by...').
[[nodiscard]]
core::Result<double> gdesDerotate(SessionComposition& session);

// Fortran GDES 'UNIT' (xgdes.f 763-781): NORM to unit chord.
[[nodiscard]]
GeometryDesignOpResult gdesNormalize(SessionComposition& session);

// Fortran GDES 'NMOV' (xgdes.f 723-756): normal displacement.
[[nodiscard]]
GeometryDesignOpResult gdesMoveNormals(SessionComposition& session,
                                       double deln);

// Fortran GDES 'TGAP' (xgdes.f 1218-1308): new TE column, DOC clamp
// 0..1 (the CLI handles the canonical ASKR prompts).
[[nodiscard]]
GeometryDesignOpResult gdesSetTrailingEdgeGap(SessionComposition& session,
                                              double gapnew, double doc);

// Fortran GDES 'LERA' (xgdes.f 1312-1373): LE radius scaling,
// DOC clamp >= 0.001; return value new LE radius (CLI prints
// 'New LE radius = ...').
[[nodiscard]]
core::Result<double> gdesScaleLeadingEdgeRadius(SessionComposition& session,
                                                double rfac, double doc);

// Fortran GDES 'TFAC' (xgdes.f 436-450 -> TCSCAL xtcam.f 899-950):
// thickness/camber scale; optional factors correspond to the
// ASKR specifications 1.0. Return value before/after for the CLI text.
[[nodiscard]]
core::Result<core::geometry_design::ScaleResult> gdesScaleThicknessCamber(
    SessionComposition& session, std::optional<double> tfac,
    std::optional<double> cfac);

// Fortran GDES 'TSET' (xgdes.f 453-467 -> TCSET xtcam.f 953-1024):
// target values (nullopt == sentinel 999 '<ret> to skip');
// applyQuestionable corresponds to the canonical ASKL confirmation for
// |Factor| > 100; false leads to 'No action taken' without mutation.
[[nodiscard]]
core::Result<core::geometry_design::SetTargetsResult>
    gdesSetThicknessCamberTargets(SessionComposition& session,
                                  std::optional<double> tnew,
                                  std::optional<double> cnew,
                                  bool applyQuestionable);

// Fortran GDES 'HIGH' (xgdes.f 470-484 -> HIPNT xtcam.f 1093-1250):
// high point shift; thpnt/chpnt <= 0 correspond to the canonical
// "keep current maximum location". Return value incl. AROT warning value.
[[nodiscard]]
core::geometry_design::HighPointResult gdesMoveHighPoints(
    SessionComposition& session, double thpnt, double chpnt);

// Fortran GDES 'FLAP'/'F' (xgdes.f 609-611, 1433-1849):
// flap deflection of the work buffer; sets XBF/YBF/LBFLAP, LGSAME=false
// and canonically disables LGSYM (symmetryDisabled).
[[nodiscard]]
core::Result<core::geometry_design::FlapResult> gdesDeflectFlap(
    SessionComposition& session, double xbf, double ybf, double ddef);

// Fortran GETXYF (xgdes.f 1880-1908) as its numerical part:
// TOPS/BOTS/TOPY/BOTY at the working buffer (xf == -999 corresponds to the
// canonical prompt case; the prompt lies in the CLI). No mutation.
[[nodiscard]]
core::geometry_design::FlapHingeResult gdesFlapHinge(
    const SessionComposition& session, double xf, double yf);

// Fortran TCBUF (xtcam.f 879-896) as a pure report: current thickness/
// camber distribution and maxima of the working buffer. No mutation.
[[nodiscard]]
core::geometry_design::ThicknessCamberReport gdesThicknessCamberReport(
    const SessionComposition& session);

// Fortran GDES 'CADD' (xgdes.f 503-598): corner refinement with
// explicit values or canonical defaults (ATOL=0.5*AMAX, ISPL=1,
// XRF = BBox +/- 10 %).
[[nodiscard]]
core::Result<core::geometry_design::CornerAddResult> gdesAddCornerPoints(
    SessionComposition& session, double atol, int ispl, double xrf1,
    double xrf2);
[[nodiscard]]
core::Result<core::geometry_design::CornerAddResult> gdesAddCornerPoints(
    SessionComposition& session);

// Reports on the working buffer (no mutation, no revision):
// GDES 'BEND' (xgdes.f 495-496 -> BENDUMP xgeom.f 1295-1365) uses the
// buffer coordinates, not the current panel contour.
[[nodiscard]]
core::geometry::SectionBending gdesBendReport(
    const SessionComposition& session);
// GDES 'CANG' (xgdes.f 499-500, IPRINT=2) and 'CLIS' (xgdes.f 601-602).
[[nodiscard]]
core::geometry::CornerAngleReport gdesCangReport(
    const SessionComposition& session);
[[nodiscard]]
core::geometry::CurvatureReport gdesClisReport(
    const SessionComposition& session);

// ---: inverse session state (MDES/QDES)
//
// Authority: xmdes.f 53-106/139-147, xqdes.f 52-102/132-140, XFOIL.INC
// CR07/CI01/Cl01, CIRCLE.INC; S0 state machine. The
// InverseSessionState holds the numerical COMMON data shared by MDES and QDES
// exactly once; menu/redo states are separate. No
// inverse numerics are executed: the numerical INIT paths
// (EIWSET/SCINIT/MAPGAM/CNCALC/QSPCIR in MDES, SPECAL/Q capture/GAMQSP
// in QDES) are not called or started here.

// Resets the inverse session state to the INIT defaults
// (xfoil.f 466-533/704: LEIW/LSCINI/LQSPEC/LQSYM/LQVDES/LQREFL/LCPXX
// false, LQSLOP true, FFILT 0.0, NC1 257; COMMON zeros for
// NSP/NQSP/IACQSP/KQTARG/IQ1/IQ2/LIQSET/QDOF0..3). Corresponds to
// a session reset and a new session. Other domain states (geometry,
// paneling, analysis, polar, design, defaults) remain unchanged.
void resetInverseDesignState(SessionComposition& session);

// Canonical panel/geometry change kinds with their Fortran-specific
// different inverse invalidation. Menu/redo states remain
// unchanged in all cases (Fortran SAVE local; no blanket
// menu invalidation). Pure buffer changes without a new current contour
// (GDES commands, CAMB ADD without EXEC, INTE, TOP LOAD buffer part)
// do not invalidate the inverse numerical state (current stays);
// they only increase the buffer revision and are not mapped via this
// operation.
enum class InversePanelChange {
  bufferToCurrent,     // ABCOPY/PCOP/GDES-EXEC: LSCINI=.FALSE.
                       // (xgdes.f 1027); LQSPEC/target segment remain
  panelingGenerated,   // PANGEN/PPAR apply/NACA: LSCINI=.FALSE. and
                       // LQSPEC=.FALSE. (xfoil.f 2087-2088)
  panelInvalidated,    // buffer adoption without a new contour
                       // (setBufferGeometry,-Panel invalidation):
                       // binding to the discarded contour is outdated;
                       // LQSPEC semantics stay unchanged
  polarArchivedCopy,   // APCOPY: LSCINI=.FALSE. (xpol.f 172)
  mixedInverseExec,    // QDES EXEC: LSCINI=.FALSE. (xqdes.f 499);
                       // QDES EXEC contract; currently no callers
};
void invalidateInverseStateAfterPanelChange(SessionComposition& session,
                                            InversePanelChange change);

// Entry/exit state frame. The entry operations check
// only the canonical precondition (N==0 -> '*** No airfoil available
// ***' without any state transition), set menu/redo state and
// run the numerics-free entry prechecks. Error cases
// leave the session state unchanged (no partial mutation).
enum class InverseMenuEntryStatus {
  entered,    // state frame set according to Fortran
  noAirfoil,  // no valid panel contour (Fortran N==0)
};
struct InverseMenuEntryResult {
  InverseMenuEntryStatus status{InverseMenuEntryStatus::noAirfoil};
};

// MDES entry limits (xmdes.f 53-92 without the numerical parts
// EIWSET/SCINIT/MAPGAM/CNCALC/QSPCIR, which are numerical):
// * Precondition: valid panel contour (N /= 0),
// * NSP /= NC1 -> LQSPEC=.FALSE. and IQ1/IQ2 full range 1..NC1
// (0-based 0..NC1-1); LIQSET stays unchanged (xmdes.f 68-72),
// * NQSP == 0 -> IACQSP=1, NQSP=1, ALQSP(1)=ALFA (xmdes.f 79-83; ALFA
// from the existing session operating state viscous.viscals.alfa),
// * Menu active, redo reset (COMOLD='****', xmdes.f 49/114).
// LSYM/NTQSPL (plot flags) remain at the plot limit and are not updated.
[[nodiscard]]
InverseMenuEntryResult enterMdesMenuState(SessionComposition& session);

// MDES exit per <Return> (xmdes.f 139-147): LQSYM=.FALSE.; menu
// inactive. QSPEC/CN/IQ1/IQ2/NQSP/AGTE/DZTE are retained; redo is
// not deleted (the Fortran SAVE locals persist until the next
// entry and are neutralized there by COMAND='****').
void leaveMdesMenuState(SessionComposition& session);

// QDES entry limits (xqdes.f 52-71 without SPECAL, Q capture and
// GAMQSP, which are numerical):
// * Precondition: valid panel contour (N /= 0),
// * NSP /= N -> LQSPEC=.FALSE. and LIQSET=.FALSE. (xqdes.f 68-71),
// * Menu active, redo reset (xqdes.f 48/112).
// The SPECAL analysis precondition (xqdes.f 65) is a numerical part
// and is not checked or executed here.
[[nodiscard]]
InverseMenuEntryResult enterQdesMenuState(SessionComposition& session);

// QDES exit per <Return> (xqdes.f 132-140): LQSYM=.FALSE. (shared
// COMMON flag, also written by MDES SYMM); menu inactive.
void leaveQdesMenuState(SessionComposition& session);

// ---: numerical MDES initialization path ----------------------------------
//
// Runs the canonical numerical INIT chain of the MDES entry
// (xmdes.f 63-107 label 1) with its guard conditions on the current
// session panel contour and commits the result atomically into the
// shared InverseSessionState:
// * NSP /= NC1 -> LQSPEC absent, IQ1/IQ2 full range (xmdes.f 65-68),
// * EIWSET(NC1) only for LEIW=.FALSE. (xmdes.f 71-72; cache contract),
// * NQSP == 0 -> IACQSP=1, NQSP=1, ALQSP(1)=ALFA (xmdes.f 75-79),
// * SCINIT(N,X,XP,Y,YP,S,SLE) only for LSCINI=.FALSE., afterwards
// LSCINI=.TRUE. and LQSPEC=.FALSE. (xmdes.f 81-87),
// * MAPGAM(1,ALGAM,CLGAM,CMGAM) always (xmdes.f 90-91),
// * CNCALC(QGAMM)+QSPCIR only for LQSPEC=.FALSE. (xmdes.f 93-97).
// Kernels: core::inverse::setCircleGrid/initMappingCoordinate/mapGam/
// calcCoefficients/qSpecCirc (Fortran-confirmed). No
// second SCINIT-/MAPGAM orchestration, no CLI texts, no streams,
// no file paths, no panel/buffer/analysis mutation.
// Error cases: controlled status without any partial mutation of the
// shared inverse state. Fortran STOP paths (EIWSET
// array overflow xmdes.f 1621) are returned as capacityError in a controlled way;
// the canonical numerical soft paths (QCCALC
// Cl non-convergence xmdes.f 1597 only writes and returns as
// Fortran) set no error back.
enum class MdesNumericsInitStatus {
  initialized,   // SCINIT ran (LSCINI was not current): full path
  refreshed,     // mapping current: only MAPGAM (CNCALC/QSPCIR did not run)
  noAirfoil,     // no valid current panel contour (xmdes.f 53-57)
  capacityError, // NC1 > ICX (EIWSET: Array overflow, xmdes.f 1621)
  coreError,     // core error (quantity/argument conflict)
};
struct MdesNumericsInitResult {
  MdesNumericsInitStatus status{MdesNumericsInitStatus::noAirfoil};
  std::string message;
  // qspecRebuilt: CNCALC+QSPCIR ran (LQSPEC was .FALSE.) ->
  // format-1190 'Qspec initialized to current Q' path (xmdes.f 103-106).
  bool qspecRebuilt{false};
  // scinitRan + scinit: SCINIT ran in this call (LSCINI was
  //.FALSE. or INIT command); diagnostic values at the canonical
  // execution time (format 1100 + IPASS/max(dw)). For
  // cache hits (SCINIT not run) both remain empty.
  bool scinitRan{false};
  core::inverse::ScinitDiagnostics scinit;
  [[nodiscard]] bool isOk() const {
    return status == MdesNumericsInitStatus::initialized ||
           status == MdesNumericsInitStatus::refreshed;
  }
};
// forceMapping corresponds to the INIT command (xmdes.f 199-202:
// LQSPEC=.FALSE., LSCINI=.FALSE., GO TO 1): SCINIT/CNCALC/QSPCIR are
// forced. Without the parameter, the canonical label-1 path runs with the
// existing validity flags.
MdesNumericsInitResult initializeMdesNumerics(SessionComposition& session,
                                              bool forceMapping = false);

// File I/O policy (LCLOCK/KDELIM/LNORM): lives in the session presentation
// state, no numerical invalidation.
void setWriteOrientation(SessionComposition& session, bool clockwise);
void setDelimiter(SessionComposition& session, int delimiter);
void setNormalizeOnLoad(SessionComposition& session, bool normalizeOnLoad);
int delimiter(const SessionComposition& session);
bool writeClockwise(const SessionComposition& session);
bool normalizeOnLoad(const SessionComposition& session);
const std::string& isesDomainParameters(const SessionComposition& session);
void setIsesDomainParameters(SessionComposition& session,
                             const std::string& parameters);

// ---: runtime/VPAR producers -----------------------------------------------

// Fortran VISC: viscosity mode; sets only LVISC and LVCONV=false
// (xoper.f 204, 215). LBLINI, LIPAN, LWAKE, LWDIJ, wake, mapping, DIJ,
// influence matrix, AVISC/MVISC and lastResult are retained.
void setViscousMode(SessionComposition& session, bool viscous);
// Fortran VISC with optional argument: sets only REINF1 and LVCONV=false
// (xoper.f 208, 215); no MRCL, no derived REINF update.
void setReynoldsBaseNumber(SessionComposition& session, double reynolds);
// Fortran Re: sets REINF1, derives MRCL(1.0) and sets LVCONV=false
// (xoper.f 286, 295-296). No BL, wake, mapping or system invalidation.
void setReynoldsNumber(SessionComposition& session, double reynolds);
// Fortran Mach: sets MINF1, derives MRCL(1.0) and COMSET and sets
// LVCONV=false (xoper.f 308, 322-333). The influence matrix remains preserved;
// GGCALC is not MINF-dependent.
void setMachNumber(SessionComposition& session, double mach);
// Fortran TYPE: sets MATYP/RETYP, derives MRCL(1.0) and COMSET and sets
// LVCONV=false (xoper.f 357-376). The influence matrix remains preserved.
void setMachReynoldsTypes(SessionComposition& session, int machType,
                          int reType);
// Fortran ITER: stores ITMAX unchanged; the canonical CLI validates
// ITMAX>=1 before the call (xoper.f 382-391). No invalidation.
void setIterationLimit(SessionComposition& session, int maximumIterations);
// Fortran Mach post-processing of an already present valid point
// (xoper.f 328-333): CPCALC/CLCALC/CDCALC on the retained point, wake,
// mapping and BL state, without a new solver run and without SPECAL/SPECCL/
// VISCAL. Updates Cl/CM/CDP (CLCALC) and Cd/CDF (CDCALC) in the Viscals
// state and the public load quantities of lastResult. Cp (CPCALC)
// is not persisted: the session holds no Cp fields; the non-
// graphical consumer path (CPMN text, cavitation) derives CPI/CPV/CPMIN
// on demand via currentCpMinimum from the preserved QINV/QVIS/MINF/LVISC
// (see CpObservation). LVCONV=false is already handled by setMachNumber.
// Executed only for a valid point (panel valid and GAM filled); otherwise
// no-op without invented values. Returns true if an existing point was
// updated.
bool refreshExistingPointAfterMachChange(SessionComposition& session);
// On-demand Cp derivation from the current session state. Pure,
// non-mutating read query without
// persistence of a second Cp state and without solver/SPECAL/VISCAL
// execution. It reconstructs the canonical CPCALC side effect
// (xoper.f 328-329) and the Cp minimum selection FCPMIN (xoper.f 1768-1801)
// directly from the preserved state:
// cpInviscid = core::inviscid::cpCalc(QINV, QINF, MINF) (CPI, xoper.f 328)
// cpViscous = core::inviscid::cpCalc(QVIS, QINF, MINF) (CPV, xoper.f 329,
// only LVISC; otherwise CPI copy, since FCPMIN for inviscid
// Computation CPMNV=CPMNI sets, xoper.f 1794-1797)
// minimum = core::boundary_layer::fcpmin(LVISC, X+WAKE, CPI, CPV)
// Inputs: paneling.panel (X/Y), viscous.wake (X), viscous.viscals
// (QINV/QVIS/MINF), operating (QINF/LVISC). No mutation of QINV/QVIS,
// BL, wake, panel or result. Without a valid point (panel invalid or
// GAM empty) valid==false and no values are returned (no
// Fortran COMMON artifact such as cpCalc(0)=1.0236 is invented). valid==false
// for a controlled core denominator state (The<=0). The value is identical
// to the canonical CPCALC exit of VISCAL (xoper.f 3026-3027, Viscal.cpp)
// and to the CPCALC side effect of Mach (xoper.f 328-329); proof
// tests/unit/CoreOperHandlerTests.cpp against
// tests/data/core-c11a-state-reference/c11a_mach_cp_harness_reference.txt.
struct CpObservation {
  bool valid{false};
  std::vector<double> cpInviscid;  // CPI via QINV (n+1+nw)
  std::vector<double> cpViscous;   // CPV via QVIS (n+1+nw) for LVISC
  core::boundary_layer::CpMinResult minimum;  // FCPMIN (CPMN/CPMNI/CPMNV/X...)
};
CpObservation currentCpMinimum(const SessionComposition& session);
// Fortran INIT choice of BL new initialization; no invalidation.
void setBoundaryLayerInitializationMode(SessionComposition& session,
                                        bool autoInitialize);
// Fortran VACCEL (instance-specific, no static VACCEL).
void setViscousAcceleration(SessionComposition& session, double vaccel);
// Fortran VPAR N (NCRIT) and XTR (XSTRIP): transition; invalidates
// BL/convergence. Not used by the VPAR handlers: this operation
// combines NCRIT and XSTRIP and invalidates via invalidateOperatingDependents
// (LBLINI/LIPAN/convergence/result), which corresponds to no single canonical
// VPAR command. The VPAR handlers use the narrow,
// individually Fortran-anchored operations below.
void setTransitionParameters(SessionComposition& session, double nCritical,
                             double topTripX, double bottomTripX);
// Fortran VPAR WAKE/W (xoper.f 2635-2646,/B1): sets
// WAKLEN and invalidated exclusively LWAKE, LBLINI, LVCONV, LIPAN
// (wakeValid, viscals.lblini, viscals.lvconv+converged,
// viscals.mapping.valid). No array clearing, no lastResult or
// LRECALC intervention; wake/panel regeneration only in the canonical
// VISCAL path. Instance-specific.
void setWakeLength(SessionComposition& session, double wakeLength);

// ---: narrow VPAR state operations -----------------------------------------
// Fortran XTR (xoper.f 2569-2582): sets exclusively XSTRIP(1)=top,
// XSTRIP(2)=bottom and LVCONV=.FALSE. (invalidateViscousConvergenceOnly).
// LBLINI, LIPAN/mapping, LWAKE, LWDIJ, AVISC/MVISC, BL station fields,
// influence matrix, iteration limit and lastResult are retained. Values
// are stored unsorted and unbounded (Fortran does not validate/clamp
// them; values >= 1 mean "no trip on this side", only the
// VISCAL kernel evaluates XSTRIP). Instance-specific.
void setTripPositions(SessionComposition& session, double topTripX,
                      double bottomTripX);
// Fortran N (xoper.f 2585-2597): sets ACRIT(1)=ACRIT(2)=nCritical (one value
// for both sides) and LVCONV=.FALSE. (invalidateViscousConvergenceOnly).
// Mapping/wake/station fields/influence matrix/lastResult are retained.
// The session holds ACRIT as a single value (both sides equal); NT/NB
// (per side) are not mapped here. Instance-specific.
void setCriticalAmplificationBoth(SessionComposition& session, double nCritical);
// per-side ACRIT(1)/ACRIT(2) as the xfoil.def line
// 'REtype Re/10^6 Ncrit1 Ncrit2' (GETDEF xfoil.f 935). Invalidation as
// for NCRIT: only LVCONV=false.
void setCriticalAmplificationSides(SessionComposition& session, double top,
                                   double bottom);
// Fortran VPAR NT
// (xoper.f 2600-2610) sets exclusively ACRIT(1) (top) and
// LVCONV=.FALSE.; for the other side, BLPAR, XSTRIP, wake/mapping/panel and
// result states are retained. NB (xoper.f 2613-2623) is analogous
// for ACRIT(2) (bottom). Instance-specific.
void setCriticalAmplificationTop(SessionComposition& session, double top);
void setCriticalAmplificationBottom(SessionComposition& session,
                                    double bottom);

// Fortran OPER DAMP
// (xoper.f 1744-1750) toggles exclusively the session-bound
// IDAMP (xfoil.def default 0): IDAMP==0 -> 1, otherwise -> 0
// (canonical ELSE branch for unexpected internal values). No
// invalidation, no solver effect except the IDAMP transport in
// dampl/dampl2 (xbl.f 77). Returns the new IDAMP value.
int toggleAmplificationModel(SessionComposition& session);

// Fortran VPAR LAG
// (xoper.f 2656-2670) writes exclusively SCCON, DUXCON, DLCON in
// the session-bound BLPAR state (Session composition.viscous.blpar).
// The three fields correspond to the three ASKR targets 2662-2664; the
// handler writes them individually in Fortran order (canonical
// partial mutation for a prompt abort). No flag invalidation: no
// validity flag and no further BLPAR field is touched; the
// effect appears only in the next BL/VISCAL evaluation.
void setShearLagConstant(SessionComposition& session, double sccon);
void setUxEqWeight(SessionComposition& session, double duxcon);
void setWakeDissipationLengthFactor(SessionComposition& session,
                                    double dlcon);
// Fortran VPAR GB (xoper.f 2672-2685): GACON and GBCON incrementally;
// CTCON = 0.5/(GACON**2*GBCON) is then always recomputed from the current
// session GACON (2684; canonical bracketing bit-identical to
// 0.5/(gacon*gacon*gbcon), harness proof with six value pairs). GCCON and
// remaining BLPAR fields unchanged; no flag invalidation.
void setGbConstantA(SessionComposition& session, double gacon);
void setGbConstantBAndCtcon(SessionComposition& session, double gbcon);
// Fortran VPAR CTR (xoper.f 2687-2700): only CTRCON, CTRCEX; no
// flag invalidation.
void setInitialCtauConstant(SessionComposition& session, double ctrcon);
void setInitialCtauExponent(SessionComposition& session, double ctrcex);
// Fortran VPAR CFAC (xoper.f 2701-2706): only CFFAC; in Fortran without
// LPACC guard; no flag invalidation.
void setBoundaryLayerCfScale(SessionComposition& session, double cffac);
// Fortran VPAR REST (xoper.f 2709-2715): CALL BLPINI (xbl.f 1578-1596)
// provides exclusively the ten BLPAR fields (BLPAR.INC) at the
// canonical BLPINI defaults; no flags, no solution or
// wake/BL/mapping state, no polar state change. Default source
// is exclusively blparConstants; no second
// default list. The guard LPACC.AND.LVISC lives in the CLI handler before this
// call. Domain-specific separation from QDES REST and GDES/MDES reset.
void restoreBoundaryLayerParameters(SessionComposition& session);

// central defaults application (GETDEF completion xfoil.f 936-947).
// Startup and RDEF use exclusively this path. Only the canonical
// effective product fields are applied: panel parameters
// (PPAR contract: effect only at the next PANGEN), MATYP/RETYP/MINF1/
// REINF1 (=RMILL*1e6) with MRCL(1.0)/COMSET and LVCONV=false (TYPE/Mach/Re
// contract), VACCEL, ACRIT(1..2) and XSTRIP(1..2) with LVCONV=false
// (NCRIT/XTR contract). Pure plot fields (size/plotAR/CH/ScrnFr,
// Xpage/Ypage/Xmargn/Ymargn, Lcolor/Lcursor, CPmax/CPmin/CPdel,
// XoffAir/ScalAir/BLUwt, polar axis boundaries) are carried in the
// DefaultsSessionState losslessly, but are not applied as product state
// and enable no plot backend. The complete record
// incl. IDEVRP and origin is stored in session.defaults.
void applyDefaults(SessionComposition& session,
                   const xfoil::io::DefaultsValues& values, int idevrp,
                   const std::string& sourceFile);

// Raw partial application on a GETDEF READ error (xfoil.f 905-936,
// ERR=80). Each successful READ assigns the COMMON fields immediately;
// the error branch performs no MRCL/COMSET and no IDEVRP update.
// Therefore only the lines already read are adopted as raw field
// assignments — without setters, without invalidation.
void applyDefaultsPartial(SessionComposition& session,
                          const xfoil::io::DefaultsValues& values,
                          int linesRead);
// Fortran VPAR INIT (xoper.f 2649-2653, content-identical to OPER INIT 394-401):
// toggles LBLINI (viscals.lblini); if LBLINI becomes false while doing so,
// additionally LIPAN=false (mapping.valid=false). LVCONV, LWAKE, LWDIJ, AVISC/MVISC,
// BL station fields, influence matrix, iteration limit and lastResult remain
// preserved. No deletion of station fields, no solver, no analysis.
// The API/batch auto-new initialization (operating.autoInitializeBoundaryLayer,
// the negation of LBLINI) is a separate use case and is
// not changed here. Instance-specific.
void toggleBoundaryLayerInitialization(SessionComposition& session);

// Fortran CSEQ (xoper.f): Cl sequence as a general session operation; runs
// runSpecCl per point with warm start. End semantics and abort control
// (NSEQEX) remain the task of the presenting CLI loop; this operation
// returns the point sequence.
[[nodiscard]]
std::vector<AnalysisResult> runClSequence(
    SessionComposition& session, double startCl, double endCl, double stepCl,
    Degrees startAlpha, const CancellationToken* cancellation);

// Fortran OPER ASEQ/CSEQ with canonical warm-start semantics
// (xoper.f 545-760). Point count from the NPOINT formula, step sign from
// start/end enforced. In contrast to runAlphaSequence/runClSequence
// (API/batch cold path per point), these operations use the
// OPER sequence point kernel: LWAKE/LIPAN/LBLINI/LVCONV/LWDIJ remain
// preserved and are reset only for |Alfa-AWAKE|, |Alfa-AVISC| or |MINF-MVISC|
// > 1e-5 (xoper.f 675-677). NSEQEX stop after 4
// non-converged VISCAL points (xoper.f 738-753, xfoil.f:698). Does not
// change the cold path of runSpecAlpha/runSpecCl/runAlphaSequence/runClSequence.
[[nodiscard]]
std::vector<AnalysisResult> runOperAlphaSequence(
    SessionComposition& session, Degrees start, Degrees end, Degrees step,
    const CancellationToken* cancellation);

[[nodiscard]]
std::vector<AnalysisResult> runOperClSequence(
    SessionComposition& session, double startCl, double endCl, double stepCl,
    Degrees startAlpha, const CancellationToken* cancellation);

// Sequence handler detail contract (xoper.f 545-760): pointwise
// diagnostic streams and a domain-specific named stop reason instead of implicit
// result edges. ALAST/CLAST correspond to the Fortran locals
// (xoper.f 661-662, 748-750); they feed the halt text format 1150.
enum class OperSequenceStopReason {
  completed,                        // NPOINT points processed
  consecutiveNonConvergenceLimit,   // ISEQEX >= NSEQEX (xoper.f 739-745)
  cancelled,                        // C++ contract, no Fortran NSEQEX
  pointFailure,                     // controlled point error (core)
};

struct OperSequencePointRecord {
  AnalysisResult result;
  OperPointDiagnostics diagnostics;
  double requestedValue{0.0};  // AA1+DAA*(IPOINT-1) or Cl1+DCL*(...)
  double startAlpha{0.0};      // alfa for the start point (degrees)
  double resultingAlpha{0.0};  // alfa after the point (degrees)
};

struct OperSequenceRun {
  std::vector<OperSequencePointRecord> points;
  OperSequenceStopReason stopReason{OperSequenceStopReason::completed};
  std::size_t iseqexAtStop{0};
  double lastConvergedAlphaDeg{0.0};  // Fortran ALAST
  double lastConvergedCl{0.0};        // Fortran CLAST
};

// afterPoint is called immediately after each point, as long as the
// session state corresponds exactly to this point (PLRADD position
// xoper.f 440-447 inside the point loop). The return value tells
// the application to proceed with the polar storage afterwards (the
// guard LVCONV/.NOT.LVISC lives in the application, xoper.f 440).
using OperPointCompletedHook = std::function<void(const AnalysisResult&)>;

[[nodiscard]]
OperSequenceRun runOperAlphaSequenceDetailed(
    SessionComposition& session, Degrees start, Degrees end, Degrees step,
    const CancellationToken* cancellation,
    const OperPointCompletedHook& afterPoint = {});

[[nodiscard]]
OperSequenceRun runOperClSequenceDetailed(
    SessionComposition& session, double startCl, double endCl, double stepCl,
    Degrees startAlpha, const CancellationToken* cancellation,
    const OperPointCompletedHook& afterPoint = {});

// ---
// Polar session operations (xoper.f 1017-1448, xpol.f).
// Consistent 0-based; the CLI layer maps the canonical 1-based
// command indexing (0 = all, -1 = abort). No text, no
// prompts, no file access in these operations.
// ---

// PDEL (xoper.f 1081-1135): deletes slot index0 and shifts the following
// slots up (PLRCOP JK chain). If the deleted slot is the active one,
// accumulation is switched off (LPACC=false, IPACT=0). moves contains the
// canonical shifts (from, to) 0-based in Fortran order.
struct PolarSlotRemovalResult {
  bool deleted{false};
  std::vector<std::pair<int, int>> moves;
  int activeSlotAfter{-1};
  bool accumulationDisabled{false};
};
[[nodiscard]] PolarSlotRemovalResult deletePolarSlot(SessionComposition& session,
                                                     int index0);

// PDEL with IP=0: deletes all slots and filenames, disables accumulation.
void deleteAllPolarSlots(SessionComposition& session);

// PSOR (xoper.f 1138-1178, PLRSRT xpol.f 733): sorts a slot
// by alpha (IDSORT = IAL).
[[nodiscard]] core::Status sortPolarSlotByAlpha(SessionComposition& session,
                                                int index0);

// PNAM (xoper.f 1395-1447): sets NAMEPOL(IP) to the stripped names.
[[nodiscard]] core::Status renamePolarSlot(SessionComposition& session,
                                           int index0,
                                           const std::string& name);

// PREM (xoper.f 1293-1392): for each alpha in ascending
// processing order removes the first point with |alpha - a| < 5e-4.
// If the slot becomes empty it is eliminated as for PDEL and the
// processing ends (canonical GO TO 500).
struct PolarPointRemovalResult {
  int removedPoints{0};
  bool polarEliminated{false};
  std::vector<std::pair<int, int>> moves;
  int activeSlotAfter{-1};
};
[[nodiscard]] PolarPointRemovalResult removePolarPoints(
    SessionComposition& session, int index0,
    const std::vector<double>& alphas);

// CINC/HINC (xoper.f 1689-1707): toggles LCMINP/LHMOMP.
void togglePolarIncludeCpMin(SessionComposition& session);
void togglePolarIncludeHingeMoment(SessionComposition& session);

// RDEL (xoper.f 978-1014, PRFCOP xpol.f 885): deletes a reference slot and
// moves on. IR=0 variant: deleteAllReferenceSlots.
struct ReferencePolarRemovalResult {
  bool deleted{false};
  std::vector<std::pair<int, int>> moves;
};
[[nodiscard]] ReferencePolarRemovalResult deleteReferenceSlot(
    SessionComposition& session, int index0);
void deleteAllReferenceSlots(SessionComposition& session);

// PGET (xoper.f 826-867): holds a polar read by the CLI as
// new slot at the end (IP = NPOL+1) and sets PFNAME(IP) to the filenames.
// NXYPOL(IP)=0 corresponds to an empty geometry snapshot (xoper.f 854).
// Capacity check against NPX; on overflow no mutation.
struct PolarSlotIngestResult {
  bool capacityExceeded{false};
  int slotIndex{-1};
};
PolarSlotIngestResult appendPolarSlot(SessionComposition& session,
                                      core::polar::PolarDataset slot,
                                      const std::string& saveFileName);

// RGET (xoper.f 934-971): holds a read reference polar as new
// slot IR = NPOLREF+1. Capacity check against NPX.
struct ReferencePolarIngestResult {
  bool capacityExceeded{false};
  int slotIndex{-1};
};
ReferencePolarIngestResult appendReferenceSlot(
    SessionComposition& session, core::polar::ReferencePolarDataset slot);

// PWRT/RGET success: PFNAME(IP) = FNAME (xoper.f 929/864).
void setPolarSaveName(SessionComposition& session, int index0,
                      const std::string& name);

// PLRSET adoption (xpol.f 92-107): Name/RETYP/MATYP/MINF1/REINF1/
// ACRIT(1..2)/XSTRIP(1..2) from the stored slot.
void adoptPolarParameters(SessionComposition& session, int index0);

// PLRSET new slot (xpol.f 109-135): NAPOL=0, NAMEPOL from session name,
// parameters from operating state, contour archive from current panel contour.
// Returns the 0-based slot index (-1 at the capacity limit).
int createPolarSlotForAccumulation(SessionComposition& session);

// APCOPY (xpol.f 139-177): reconstruct the panel contour from the archived
// polar contour (SCALC/SEGSPL/NCALC/LEFIND/TECALC in the core) and reset the
// canonically invalidated session states
// (LGAMU/LQINU/LWAKE/LQAIJ/LADIJ/LWDIJ/LIPAN/LVCONV, LBLINI for
// N change). Name is set to NAMEPOL(IP).
[[nodiscard]] core::Status applyArchivedPolarGeometry(
    SessionComposition& session, int index0);

// PLRADD (xpol.f 550-608) on the OPER point call sites (xoper.f
// 441/498/533/689): stores the current point in the active slot. The
// canonical messages ("No active polar...", "Polar storage arrays
// full...", "Point added...") remain the CLI's concern.
enum class PolarAddOutcome {
  added,          // Point stored
  noActivePolar,  // IPACT == 0
  storageFull,    // NAPOL == NAX
};
struct PolarAddResult {
  PolarAddOutcome outcome{PolarAddOutcome::noActivePolar};
  int slotIndex{-1};
};
PolarAddResult addPolarPointFromSession(SessionComposition& session);

// ---: flap/hinge/VELS use case ---------------------------------------------
// FNEW (xoper.f 1629-1641): sets exclusively the current flap
// state (xof/yof set to the 2-, 1- or 0-value sentinel, always
// lFlap=false). No geometry, Cp, solution or buffer flap
// mutation; XBF/YBF/LBFLAP remain unchanged.
void setCurrentFlapSentinels(SessionComposition& session, double xof,
                             double yof);

// GETXYF geometry part (xgdes.f 1887-1892) on the current contour:
// SINVRT/SEVAL at the hinge x position; yf==999.0 corresponds to the
// y/t path with explicit yrel. Mutation-free geometry part of the
// FMOM path; prompt control lives in the CLI.
core::geometry_design::FlapHingeResult operFlapHinge(
    const SessionComposition& session, double xf, double yf,
    double yrel = 0.0);

// Shared hinge-load use case for FMOM and polar hinge: MHINGE
// (xoper.f 2350-2487) on the LFLAP=true branch
// (tops=xof, bots=S(N)-xof, xoper.f 2367-2368); CPI for
// LVISC=false, CPV for LVISC=true. Numerics exclusively in the
// existing core inviscid::mhinge. Error for missing valid
// Cp provenance or missing contour.
core::Result<core::inviscid::HingeMomentResult> computeCurrentHingeLoads(
    const SessionComposition& session);

// VELS (xoper.f 1643-1665): PSILIN (xpanel.f 99) twice at a
// free point (I=0, GEOLIN=false, SIGLIN=true); u from PSI_NI with
// normal (0,1), v from PSI_NI with normal (-1,0);
// q = SQRT(u^2+v^2), cp = 1-(u^2+v^2). Numerics exclusively in the
// existing core inviscid::psiLin. Mutation-free.
struct VelocityAtPointResult {
  double u{0.0};
  double v{0.0};
  double q{0.0};
  double cp{0.0};
};
core::Result<VelocityAtPointResult> evaluateVelocityAtPoint(
    const SessionComposition& session, double x, double y);

// OPER file exports. The projection of the session states onto
// the encoder inputs (CPWR: CPDUMP xoper.f 2265-2346; DUMP: BLDUMP
// xoper.f 1892-2050; DMP: BLDUMP2 xoper.f 2053-2262). No text,
// no file paths, no mutation of the session state.
io::PressureDumpInput currentPressureDumpInput(const SessionComposition& session);
io::BoundaryLayerDumpInput currentBoundaryLayerDumpInput(
    const SessionComposition& session);

// ---: MDES menu commands ----------------------------------------------------
// All operations mutate exclusively InverseSessionState
// (shared MDES/QDES numerical state exactly once, separate redo states).
// No geometry/panel mutation, no analysis invalidation, no text.
// Canonical sources: xmdes.f 205-540 (QSET/AQ/CQ/SYMM/TGAP/TANG/VISC/
// REFL/FILT/SLOP/SMOO), kernels exclusively core::inverse.

enum class MdesCommandStatus {
  ok,              // Command executed
  notInitialized,  // numerical INIT state missing (xmdes.f label 1 needed)
  invalidState,    // canonically valid, but not an executable state
};

// Format-1200/1210 records (text formatting remains the handlers'
// concern): current mapped Q plus per Qspec column ALQSP/CLQSP/CMQSP.
struct MdesQspecColumnReport {
  int kqsp1{0};  // Fortran KQSP (1-based)
  double alfa{0.0};
  double cl{0.0};
  double cm{0.0};
};
struct MdesSpectrumReport {
  double algam{0.0};
  double clgam{0.0};
  double cmgam{0.0};
  std::vector<MdesQspecColumnReport> columns;
};

// QSET (xmdes.f 205-209): CNCALC(QGAMM,.FALSE.), CNSYMM for LQSYM,
// QSPCIR via all NQSP columns. No text in Fortran.
MdesCommandStatus mdesQspecReset(SessionComposition& session);

// AQ (xmdes.f 217-247): ALQSP(1..n)=specifications (rad), NQSP=min(n,IPX),
// IACQSP=1, QSPCIR. Empty requirement list = canonical <return> path without
// mutation (here mapped by passing an empty list).
MdesCommandStatus mdesSetQspecAlphas(SessionComposition& session,
                                     const std::vector<double>& alphasRad);

// CQ (xmdes.f 250-283): CLQSP(1..n)=Specifications, IACQSP=2, QSPCIR.
MdesCommandStatus mdesSetQspecCls(SessionComposition& session,
                                  const std::vector<double>& cls);

// SYMM/S (xmdes.f 286-307): LQSYM toggle; for enabling CNSYMM +
// QSPCIR (numerical), for disabling only the flag. newState = new
// LQSYM state; for an uninitialized numerical state the
// flag is not set (notInitialized).
MdesCommandStatus mdesToggleSymmetry(SessionComposition& session,
                                     bool& newState);

// TGAP (xmdes.f 310, DZTSET 733-753): sets DZTE=(dx,dy); no
// further mapping/coefficient mutations (canonical).
MdesCommandStatus mdesSetTeGap(SessionComposition& session, double dx,
                               double dy);

// TANG (xmdes.f 313, AGTSET 756-770): AGTE = Degrees/180 set.
MdesCommandStatus mdesSetTeAngle(SessionComposition& session,
                                 double angleDeg);

// VISC/V (xmdes.f 316-328): LQVDES toggle (only flag; plot share
// recognized-but-unavailable). Return value = new state.
bool mdesToggleQvisOverlay(SessionComposition& session);

// REFL (xmdes.f 331-341): LQREFL toggle (only flag). Return value = new
// state.
bool mdesToggleReflOverlay(SessionComposition& session);

// SLOP (xmdes.f 508-518): LQSLOP toggle (only flag; NTQSPL is
// a plot character count and is not updated). Return value = new state.
bool mdesToggleSlopeMatch(SessionComposition& session);

// FILT/F (xmdes.f 475-505): CNFILT(0.2) fixed, PIQSUM, QSPCIR; afterwards
// per column QSPINT (CMQSP always, CLQSP only for IACQSP=1). Report as
// FORMAT 1200/1210.
MdesCommandStatus mdesApplyFilter(SessionComposition& session,
                                  MdesSpectrumReport& report);

// SMOO (xmdes.f 443-472): SMOOQ(IQ1,IQ2,KQTARG) in the target column,
// CNCALC(LQSYM), QSPCIR; afterwards QSPINT update as FILT.
// kqtarg must designate an active column (Fortran KQTARG>=1).
MdesCommandStatus mdesSmoothTargetSegment(SessionComposition& session,
                                          MdesSpectrumReport& report);

// ---: MDES file contracts --------------------------------------------------

// READ (xmdes.f 416-435 + GETVOV 1769-1826): the decoded
// (s, V/Vinf) raw values are resampled into column KQSP=1 via the core kernel
// resampleVelocityRatioTable (normalization, SORT pair removal, SPLIND,
// SEVAL, QINCOM, SPLQSP). NQSP/IACQSP
// remain unchanged. Canonical error contract: OPEN errors and
// parse errors (ERR=99) make GETVOV return without any mutation;
// only EOF (END=11) processes the pairs read up to that point. Empty
// or too short tables are numerically undefined in Fortran
// (KK<2: division by 0 / SIGSEGV); the kernel returns a structured
// error without mutation.
enum class MdesReadStatus {
  ok,              // Column 1 updated
  notInitialized,  // numerical INIT state missing
  invalidTable,    // Table empty/too short or quantity conflict
  coreError,       // core error in the resampling
};
MdesReadStatus mdesReadVelocityDistribution(
    SessionComposition& session,
    const core::export_data::VelocityRatioTable& table);

// READ post-processing (xmdes.f 420-433), which in Fortran also runs after OPEN/
// READ errors: CNCALC(QSPEC(1,1),.FALSE.), CNSYMM for LQSYM,
// QSPINT with CLQSP(1) and CMQSP(1) update (both always, in
// contrast to FILT/SMOO). Report = format-1200/1210 data.
enum class MdesReadPostStatus {
  ok,
  notInitialized,
  coreError,
};
MdesReadPostStatus mdesReadPostProcess(SessionComposition& session,
                                       MdesSpectrumReport& report);

// DUMP (xmdes.f 605-614 + CNDUMP 1747-1766): pure read-only query. Lines
// m=0..MC with CN(m) and PIQ(m+1), m=MC+1..NC-1 with CN=0 (canonical).
// No session mutation, no kernel repetition. notInitialized if
// the circle grid is not initialized (NC/MC undetermined).
enum class MdesDumpStatus {
  available,
  notInitialized,
};
struct MdesDumpData {
  std::vector<core::export_data::CircleCoefficientDumpRecord> rows;
};
MdesDumpStatus currentMdesCnDumpData(const SessionComposition& session,
                                     MdesDumpData& data);

// ---: QDES menu and non-committing QDES numerics -------------
// Shared InverseSessionState: no QDES copy of the MDES state.
// Canonical sources: xqdes.f 52-107 (entry), 192-300/400-425
// (QSET/SMOO/SLOP/VISC/REFL/CPXX). REST (xqdes.f 508-530) mutates X/Y
// from XB/YB (geometry commit).

enum class QdesNumericsInitStatus {
  captured,    // Q capture ran (GAMQSP only for .NOT.LQSPEC)
  noAirfoil,   // no valid panel contour (xqdes.f 52-57)
  coreError,   // SPECAL/capture/GAMQSP core error
};
struct QdesNumericsInitResult {
  QdesNumericsInitStatus status{QdesNumericsInitStatus::noAirfoil};
  // qspecRebuilt: GAMQSP(1) ran (LQSPEC was .FALSE.) -> format-1155
  // 'Qspec initialized to current Q.' path (xqdes.f 96-102).
  bool qspecRebuilt{false};
  std::string message;
  // pointEvents/specalSnapshot: chronological event stream of the
  // executed inviscid point path (GGCALC only on cache invalid,
  // OFS format-333 values at the actual SPECAL time point). No
  // VISCAL event (QDES entry runs SPECAL exclusively).
  std::vector<OperPointEventRecord> pointEvents;
  SpecifiedPointSnapshot specalSnapshot;
  [[nodiscard]] bool isOk() const {
    return status == QdesNumericsInitStatus::captured;
  }
};
// Entry (xqdes.f 65-102): SPECAL for current Alfa (confirmed
// core point kernel runSpecAlpha), Q capture from viscous.viscals.gam
// (confirmed kernel captureCurrentSpeed), GAMQSP(1) only for
// missing target Q (confirmed core kernel gamQSpec). No second
// SPECAL/QISET/mapping implementation.
QdesNumericsInitResult initializeQdesNumerics(SessionComposition& session);

enum class QdesCommandStatus {
  ok,
  notInitialized,
  coreError,
};

// QSET (xqdes.f 192-197): exclusively GAMQSP(1) (core kernel gamQSpec):
// ALQSP/CLQSP/CMQSP(1) = ALGAM/CLGAM/CMGAM, QSPEC(:,1) = QGAMM,
// QDOFs zeroed, SPLQSP; IQ1/IQ2 full range only for .NOT.LIQSET.
// Not identical to MDES QSET (there CNCALC/QSPCIR) - a separate
// use case despite the identical name.
QdesCommandStatus qdesQspecReset(SessionComposition& session);

// Format-1200 data of the QDES SMOO (xqdes.f 415-424): current
// analysis Cl/CM plus CLQSP/CMQSP of the smoothed column 1.
struct QdesForceReport {
  double cl{0.0};
  double cm{0.0};
  double clq{0.0};
  double cmq{0.0};
};

// SMOO (xqdes.f 280-296): SMOOQ(IQ1,IQ2,1) + SPLQSP on column 1,
// afterwards CLCALC(N,X,Y,QSPEC(:,1),W1,...) with CLQSP(1)/CMQSP(1)-
// update. Canonical peculiarity: the GAM_A argument preserves the
// COMMON scratch W1; GAM_A flows exclusively into Cl_ALF/Cl_MSQ
// (xfoil.f CLCALC), not into Cl/CM - the force values are
// reproducible. Without MARK, IQ1/IQ2 are the full range (canonical,
// xqdes.f 280 comment); a textual segment grammar does
// not exist and is not invented.
QdesCommandStatus qdesSmoothTargetSegment(SessionComposition& session,
                                          QdesForceReport& report);

// CPXX (xqdes.f 400-407): LCPXX toggle (pure state flag; acts
// only on later MIXED/EXEC). Return value = new state.
bool qdesToggleCpxxConstraint(SessionComposition& session);

// ---: commit and boundary contracts -----------------------------
// Four separate domain use cases (no shared "inverse commit"):
// MDES EXEC (buffer commit), MDES PERT+EXEC, QDES EXEC (current commit
// via MIXED), QDES REST (current commit from buffer without LSCINI
// reset). All four are driven by the production MDES/QDES menu handlers.

enum class MdesExecuteStatus {
  ok,
  notInitialized,  // circle-grid/QSPEC binding missing
  coreError,
};
struct MdesExecuteResult {
  MdesExecuteStatus status{MdesExecuteStatus::notInitialized};
  std::size_t bufferPointCount{0};  // NB = NC (MAPGEN returns N=NC)
  std::string message;
 // mapgen: iteration diagnostics of the same MAPGEN run that
  // produced the buffer contour (format 'ITERCN DCNMAX' + non-convergence).
  core::inverse::MapgenDiagnostics mapgen;
  // metrics: GEOPAR metrics (THICK/XTHICK/CAMBR/XCAMBR, FORMAT 1000)
  // from the same EXEC sequence (xmdes.f 622-628); metricsValid only after
  // a successful commit.
  core::geometry::GeometryMetrics metrics;
  bool metricsValid{false};
  [[nodiscard]] bool isOk() const {
    return status == MdesExecuteStatus::ok;
  }
};
// MDES EXEC/X (xmdes.f 616-699): MAPGEN via the shared
// circle state (confirmed core kernel mapGenerate; mutates CN/PIQ/ZC/SC),
// commit the generated contour into the buffer (XB/YB/SB/XBP/YBP, NB=NC,
// GEOPAR buffer metrics). The current panel contour and the
// analysis state remain canonically unchanged (text 1300 points to
// PANE); LGSAME=false. Error without partial mutation.
MdesExecuteResult executeMdesDesign(SessionComposition& session);

enum class MdesPerturbStatus {
  ok,             // Perturbation + subsequent EXEC (COMAND='EXEC')
  notInitialized,
  invalidIndex,   // M outside 1..MC (Fortran READ guard 1<=M<=NC)
  coreError,
};
struct MdesPerturbResult {
  MdesPerturbStatus status{MdesPerturbStatus::notInitialized};
  // Canonical partial mutation: the coefficient perturbation stands
  // for a following EXEC core error (Fortran sets CN before
  // MAPGEN and never rolls back).
  bool coefficientPerturbed{false};
  // PERT's own TE-gap/chord Newton (xmdes.f PERT 1713-1745, NCN=1):
  // iteration records (ITERCN/DCNMAX) and convergence status of the same
  // run that sets CN at the end of the PERT loop. Fortran
  // non-convergence after 10 iterations is domain-specific, not a core error;
  // the text reads 'TE gap,chord did not converge' (not the
  // MAPGEN text).
  core::inverse::MapgenDiagnostics newton;
  MdesExecuteResult execute;
  std::string message;
};
// MDES PERT (xmdes.f 701-708 + PERT 1655ff): CNCALC state reset beforehand,
// single coefficient CN(M) += (dCnr,dCni) (confirmed core kernel
// perturbCoefficient), PERT TE-gap Newton (numerically identical to the
// MAPGEN Newton, NCN=1, source evidenced: MAPGEN's identity pre-fill
// is overwritten by the TE-gap block before CGAUSS), afterwards
// necessarily the same MDES EXEC path.
MdesPerturbResult mdesPerturbCoefficientAndExecute(SessionComposition& session,
                                                   int m, double dcnr,
                                                   double dcni);

// PERT (xmdes.f PERT routine start): CNCALC(QSPEC(1,1),
//.FALSE.) on the shared circle state (confirmed core kernel
// calcCoefficients) - Fortran prints the coefficients table only
// after this step. Error without mutation.
SessionOperationStatus mdesPerturbPrepareCoefficients(
    SessionComposition& session);

enum class QdesExecuteStatus {
  ok,
  targetNotMarked,   // LIQSET=false -> canonical refusal
  stagnationInTarget,// xqdes.f 416-427: segment contains stagnation point
  notInitialized,
  coreError,
};
struct QdesExecuteResult {
  QdesExecuteStatus status{QdesExecuteStatus::notInitialized};
  // converged: MIXED convergence (format-2000 threshold). Fortran
  // commits the contour regardless (text branch 'Not quite
  // converged'); only a core error prevents the commit.
  bool converged{false};
  std::size_t panelPointCount{0};
  std::string message;
  [[nodiscard]] bool isOk() const {
    return status == QdesExecuteStatus::ok;
  }
};
// QDES EXEC/X (xqdes.f 409-505): guard without LIQSET without any
// mutation; stagnation point check; save current->buffer (NB=N);
// MIXED (confirmed core kernel mixedInverse) on the candidate;
// SCALC/SPLIND/NCALC/LEFIND/TECALC/APCALC via the confirmed
// abcopy kernel; commit the new current contour; invalidations
// according to xqdes.f 494-503 (LSCINI=false, LBLINI stays); QGAMM/SSPEC/
// NSP re-capture; LQSPEC/LIQSET/IQ1/IQ2 are retained.
QdesExecuteResult executeQdesMixedDesign(SessionComposition& session,
                                         int niterq);

enum class QdesRestoreStatus {
  ok,
  bufferTooSmall,  // NB < N: Fortran reads outdated XB values, not representable
  coreError,
};
struct QdesRestoreResult {
  QdesRestoreStatus status{QdesRestoreStatus::coreError};
  std::size_t pointCount{0};
  std::string message;
  // leadingEdgeNotFound: typed event of the LEFIND core path
  // (LeStatus::limitReached) at the actual execution time of
  // the geometry post-processing; no rates from the final geometry.
  bool leadingEdgeNotFound{false};
  [[nodiscard]] bool isOk() const {
    return status == QdesRestoreStatus::ok;
  }
};
// QDES REST (xqdes.f 508-530): X/Y <- XB/YB (first N buffer points),
// SCALC/SPLIND/NCALC/LEFIND/TECALC/APCALC (confirmed abcopy kernel),
// LGAMU/LQINU=false, LGSAME=true, LSCINI/LQSPEC/LIQSET/target segment
// and wake/BL flags remain canonically unchanged. Separate
// use case, not via the EXEC/ABCOPY wrapper.
QdesRestoreResult restoreQdesCurrentFromBuffer(SessionComposition& session);

}  // namespace xfoil::session

// End: include/xfoil/session/SessionOps.h

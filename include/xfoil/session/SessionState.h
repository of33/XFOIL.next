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

// File: include/xfoil/session/SessionState.h
// Domain-separated session part states: each part state holds existing
// xfoil::core value types or pure session value types. No plot or
// cursor states. The
// DesignSessionState carries the GDES/CAMB design state; the
// InverseSessionState carries the shared MDES/QDES state. The
// PolarSessionState carries the polar state (Fortran
// CPOL/CPOLSD/CPOLXY/CPOLREF slots, NPOL, IPACT, LPACC, LCMINP, LHMOMP,
// PFNAME).
//
// No raw pointers, no global state, no dependency on app, CLI or
// tests, and no dependency from the public facade (xfoil/XFoil.h). The
// header is self-sufficiently compilable: every value-stored member type
// is fully included (AnalysisConfig, AnalysisResult, AirfoilGeometry,
// AirfoilBuffer, PanelingParameters, PanelGeometry, MachReynoldsSettings,
// InfluenceSystem, Viscals). The numerical kernels live exclusively in
// xfoil::core; this layer coordinates state and core operations
// (no second numerical implementation).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/Types.h"
#include "xfoil/core/boundary_layer/BoundaryLayerParameters.h"
#include "xfoil/core/boundary_layer/Viscal.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry_design/CamberSession.h"
#include "xfoil/core/inverse/CircleGeometry.h"
#include "xfoil/core/inverse/QSpecKernels.h"
#include "xfoil/core/inviscid/InviscidTypes.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/TrailingEdge.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/io/OutputPathPolicy.h"

namespace xfoil::session {

// Provenance of the current valid panel contour. Explicit, so that ABCOPY
// and PANGEN are not confused and a PANGEN contour can never be replaced
// afterwards by ABCOPY.
enum class PanelingOrigin {
  none,    // no valid panel contour
  abcopy,  // copied directly from the buffer contour (Fortran ABCOPY)
  pangen,  // paneling from buffer + panel parameters (Fortran PANGEN)
};

// Buffer geometry and name (Fortran X/Y/NB buffer, ANAME). The current
// panel contour lives in the PanelingSessionState (Fortran X/XP/Y/YP/S/N
// are assigned by PANE/PPAR). The CM reference point and the write order
// are the Fortran XCMREF/YCMREF and LCLOCK session values. bufferRevision
// counts every adoption into the buffer (instance-specific, monotonic)
// and serves the panel provenance as a dependency marker. The file I/O
// policy (write direction LCLOCK, NORM for LOAD, KDELIM, ISPARS) is
// session state.
struct GeometrySessionState {
  std::string name;
  // In Fortran, NNAME is 32 before the first NAME/LOAD/NACA/INTE
  // assignment, and a name is padded to 32 characters (xfoil.f INIT). A
  // later STRIP assignment can produce an empty name with NNAME=0. This
  // flag distinguishes the canonical initial state from an explicitly
  // emptied name; it changes no numerical geometry.
  bool nameExplicitlySet{false};
  geometry::AirfoilGeometry airfoil;     // modern geometry value (API contract)
  core::geometry::AirfoilBuffer buffer;  // Fortran buffer contour (XB/YB/NB)
  bool valid{false};                     // geometry set
  std::uint64_t bufferRevision{0};       // monotonic, instance-specific revision
  bool writeClockwise{false};            // Fortran LCLOCK (REVE/LOAD)
  bool normalizeOnLoad{false};           // Fortran LNORM (NORM)
  int delimiter{0};                      // Fortran KDELIM (DELI): 0/1/2
  // Fortran PREFIX/NPREFIX (SETFILE xfoil.f 1231-1237): file extension of
  // the last loaded contour without the dot; empty means NPREFIX 0. Used
  // only for the default suggestion in filename prompts (CPWR/DUMP/DMP/MSAV...).
  std::string filePrefix;
  std::string isesDomainParameters{" -2.0  3.0  -2.5  3.5"};  // Fortran ISPARS
  std::optional<core::geometry::GeometryMetrics> metrics;     // from core GEOPAR
};

// Panel parameters and current panel geometry (Fortran NPAN/CVPAR/CTERAT/
// CTRRAT/XSREF/XPREF, PANE/PPAR). panelingValid == true: the current
// panel geometry was derived from the buffer geometry (ABCOPY or PANGEN).
// Buffer changes invalidate the state. origin carries the explicit
// provenance; panelSourceRevision is the buffer revision from which the
// current contour was produced.
struct PanelingSessionState {
  core::paneling::PanelingParameters parameters;  // Fortran GETPAN parameters
  core::paneling::PanelGeometry panel;            // Fortran X/XP/Y/YP/S/N
  bool panelingValid{false};                      // Fortran LCONF semantics
  PanelingOrigin origin{PanelingOrigin::none};    // none/abcopy/pangen
  std::uint64_t panelSourceRevision{0};           // buffer revision of the panel
  // Monotonic, instance-specific count of every replacement or
  // invalidation of the current panel contour (ABCOPY/PCOP, PANGEN/PPAR
  // apply, APCOPY, buffer adoption without a new contour).
  // panelSourceRevision does not distinguish two panel contours on the
  // same buffer revision (e.g. APCOPY or a later QDES-EXEC on an
  // unchanged buffer); the InverseSessionState therefore binds its
  // numerical provenance additionally to this contour revision
  // (source: SCINIT(N,X,XP,Y,YP,S,SLE), xmdes.f 87).
  std::uint64_t panelRevision{0};
};

// Operating state (Fortran MINF1/REINF1/MATYP/RETYP, QINF, ALFA, LVISC,
// ITER limit, INIT/LBLINI choice, VACCEL, XTR/NCRIT, WAKLEN). VACCEL is
// instance-specific (no static VACCEL). The CM reference fields and
// the hinge location are INIT defaults; XYCM/FNEW set them later.
// Fortran COMMON CPMNI/XCPMNI/CPMNV/XCPMNV (XFOIL.INC:92, written only in
// FCPMIN xoper.f 1768-1801): live in the COMMON display state of the last
// FCPMIN call. They are not cleared by geometry/panel change, Mach, Re,
// TYPE, INIT or VPAR; CPMN (xoper.f 1678-1686) prints them regardless of
// domain freshness. The start values are the COMMON zeros.
// hasBeenUpdatedByFcpmin is internal bookkeeping only and does not
// suppress Fortran output.
struct CpMinimumSnapshot {
  bool hasBeenUpdatedByFcpmin{false};
  double cpmni{0.0};   // Fortran CPMNI
  double xcpmni{0.0};  // Fortran XCPMNI
  double cpmnv{0.0};   // Fortran CPMNV
  double xcpmnv{0.0};  // Fortran XCPMNV
};

struct OperatingConditionState {
  core::inviscid::MachReynoldsSettings machReynolds;  // MATYP/RETYP/MINF1/REINF1
  bool configured{false};              // configuration validated and set
  double qinf{1.0};                    // Fortran QINF (INIT 1.0)
  bool alphaSpecified{true};           // Fortran LALFA (last request type)
  bool viscous{true};                  // Fortran LVISC
  int maximumIterations{100};          // Fortran ITER
  bool autoInitializeBoundaryLayer{true};  // INIT automatic re-initialization
  double viscousAcceleration{0.01};    // Fortran VACCEL (INIT, instance-specific)
  std::array<double, 2> criticalReynoldsExponents{9.0,
                                                  9.0};  // Fortran ACRIT(1..2)
  double topTripX{1.0};                // Fortran XSTRIP(1)
  double bottomTripX{1.0};             // Fortran XSTRIP(2)
  double wakeLength{1.0};              // Fortran WAKLEN (INIT)
  double momentReferenceX{0.25};       // Fortran XCMREF (INIT)
  double momentReferenceY{0.0};        // Fortran YCMREF (INIT)
  // ASEQ/CSEQ prompt values (Fortran OPER locals AA1/AA2/DAA and
  // Cl1/Cl2/DCL, xoper.f 552-610): persisted within an OPER session;
  // empty prompt answers keep the previous value.
  double aseqFirstDeg{0.0};
  double aseqLastDeg{0.0};
  double aseqStepDeg{0.0};
  double cseqFirstCl{0.0};
  double cseqLastCl{0.0};
  double cseqStepCl{0.0};
  CpMinimumSnapshot cpMinimum;
};

// Inviscid influence system (GGCALC result, geometry dependent) and the
// persistent viscous BL state (Fortran XFOIL.INC/XBL.INC session shares,
// which VISCAL carries on between points). converged is Fortran LVCONV;
// the BL state carries the mapping, XSSI, WGAP, ISYS, DIJ, the station
// fields and the transition indices (0-based).
// Provenance of the last OPER point command (COMOLD in xoper.f 24/471/506/541).
// '!' sets COMAND=COMOLD and LRECALC=.TRUE. (xoper.f 85-92); the
// corresponding alpha/CLI/CL branch then runs only VISCAL(ITMAX).
enum class OperPointOrigin {
  none,  // COMOLD = '****'
  alpha, // ALFA/A (xoper.f 416-471)
  cli,   // CLI (xoper.f 474-506)
  cl,    // CL/C (xoper.f 509-541)
};

struct ViscousSessionState {
  core::inviscid::InfluenceSystem system;  // Fortran AIJ/BIJ/GAMU/QINVU basis
  bool systemValid{false};                 // GGCALC result current
  double systemMach{0.0};                  // MINF of the system calculation
  double systemQinf{0.0};                  // QINF of the system calculation
  core::boundary_layer::ViscalState viscals;  // Fortran BL session state
  // Authoritative, session-bound BLPAR state
  // (Fortran COMMON/BLPAR, BLPAR.INC; initialization BLPINI, xbl.f
  // 1578-1596). Persisted across OPER/VPAR menu cycles, profile changes,
  // PANE and inviscid/viscous computations; not reset by VPAR INIT;
  // reset only by VPAR REST.
  core::boundary_layer::BoundaryLayerParameters blpar =
      core::boundary_layer::blparConstants();
  // Authoritative, session-bound IDAMP state
  // (Fortran COMMON/CI05, XFOIL.INC:165; INTEGER, initial value 0 from
  // INIT, xfoil.f:685, set only at program start). After INIT the only
  // write site is the OPER DAMP toggle (xoper.f:1744-1751); the only read
  // site is SETBL (xbl.f:77 IDAMPV = IDAMP), where AXSET (xblsys.f:70)
  // selects DAMPL (0) or DAMPL2 (1).
  // No reset on profile change, PANE, VPAR INIT, menu cycles or
  // polar accumulation; no invalidation of stored solutions.
  // New sessions start at 0 (Fortran program start).
  int idamp{0};
  // Fortran CPV (COMMON/CR04, XFOIL.INC:66, n+1+nw): set by VISCAL for
  // each point and kept until the next VISCAL run;
  // PLXADD (xpol.f 665) reads CPV(I) from this COMMON state.
  std::vector<double> cpv;
  // Authoritative, session-bound CPI representation
  // (Fortran COMMON/CR04 CPI(IZX), XFOIL.INC:66). The only producers are
  // the canonical CPCALC results (xfoil.f:1057) of the existing
  // inviscid/viscous core paths: SPECAL/SPECCL completion
  // (xoper.f:2806-2812/2879-2885, length N or N+NW per LVISC branch),
  // VISCAL completion (xoper.f:3026, length N+NW), OPER Mach
  // (xoper.f:328, length N) and QDES-EXEC/MIXED (xqdes.f:488, length N).
  // No interpreter state, no second copy, no recomputation
  // in SessionOps/CLI; the values derive from the core results
  // (SpecPointResult.cp, ViscalResult.cp) or the core kernel cpCalc at
  // the same canonical values. CPI and CPV remain separate.
  // Lifetime as CPV: COMMON persistence until the next canonical
  // producer; validity via the existing point-validity
  // model (valid panel contour + analyzed point).
  std::vector<double> cpi;
  bool converged{false};                   // Fortran LVCONV
  // OPER sequence warm start (xoper.f 675-677/2904-2906):
  // wake geometry and AWAKE/LWAKE stay preserved between sequence
  // points as long as |Alfa-AWAKE| <= 1e-5. The single-point/API/batch
  // path uses the cold path and does not preserve these fields.
  core::inviscid::WakeState wake;          // Fortran XW/YW/NW... (XYWAKE result)
  bool wakeValid{false};                   // Fortran LWAKE
  double awake{0.0};                       // Fortran AWAKE (alpha of the wake computation)
  // LRECALC continuation (xoper.f 85-92/416-542): after a
  // SPECAL/SPECCL point, Fortran keeps the inviscid COMMON quantities
  // until a following '!' reruns only VISCAL(ITMAX) on the preserved
  // state without calling SPECAL/SPECCL again. For this, the TECALC state
  // of the last inviscid point, the LALFA of the last point and the
  // continuability are persisted instance-specifically. A geometry, panel,
  // configuration or viscosity change invalidates it.
  core::inviscid::TrailingEdgeState lastTe{};  // TECALC of the last inviscid point
  core::inviscid::WakeState lastPointWake{};   // wake of the last point (LRECALC, separate from LWAKE)
  bool lastPointWakeValid{false};              // lastPointWake is set
  bool lastPointLalfa{true};                   // LALFA of the last point (input.lalfa)
  double lastClspec{0.0};                      // CLSPEC of the last point (LALFA=false branch)
  bool lastPointContinuable{false};            // '!' continuation is permitted
  // Provenance of the last OPER point (COMOLD equivalent xoper.f 471/506/541):
  // '!' repeats exactly this point type with LRECALC=.TRUE.; none corresponds
  // to COMOLD='****' ('Previous .OPER command not valid', xoper.f 86-88).
  OperPointOrigin lastPointOrigin{OperPointOrigin::none};
};

// Structured numerical single-point diagnostics (no CLI texts,
// no streams). specalSnapshot corresponds to the state immediately after
// SPECAL before VISCAL (Fortran format 333, xoper.f 2816-2818): alfa, cl, cm,
// cdp of the final SPECAL CLCALC. It is set only when SPECAL ran
// (alfa/CSEQ); CLI/CL points produce no OFS state in Fortran and therefore
// keep no snapshot. iterations/completion are the ordered
// core records of the VISCAL point (format 2000/2010/2020 or 9922).
struct SpecifiedPointSnapshot {
  double alpha{0.0};  // Fortran alfa (radians) after SPECAL
  double cl{0.0};     // Fortran Cl after SPECAL CLCALC
  double cm{0.0};     // Fortran CM after SPECAL CLCALC
  double cdp{0.0};    // Fortran CDP after SPECAL CLCALC
  bool valid{false};  // SPECAL ran for this point
};

// Chronological event stream of an OPER single point. The records
// arise at the actual execution sites of the point kernels
// (ggCalc, SPECAL OFS, xywake, VISCAL/QDCALC, BL marching) and are passed
// on in capture order; no retroactive sorting.
// Pure value records without formatted texts.
enum class OperPointEventKind {
  influenceSystemCalculation,      // GGCALC (xpanel.f:1000)
  specifiedPointOutput,            // OFS FORMAT 333 (xoper.f:2816)
  wakeTrajectoryCalculation,       // XYWAKE (xpanel.f:1277)
  specalConvergenceFailure,        // xoper.f:2799
  specclConvergenceFailure,        // xoper.f:2874
  viscous,                         // BL event (ViscousEventRecord)
};

struct OperPointEventRecord {
  OperPointEventKind kind{OperPointEventKind::influenceSystemCalculation};
  double alpha{0.0};  // alpha at the actual execution time
  core::boundary_layer::ViscousEventRecord viscous;  // only for kind==viscous
};

struct OperPointDiagnostics {
  SpecifiedPointSnapshot specalSnapshot;
  // Canonical SPECAL/SPECCL convergence abort of this point
  // (xoper.f:2799/2874); none if the Newton loop converged.
  core::inviscid::SpecifiedPointFailure specFailure{
      core::inviscid::SpecifiedPointFailure::none};
  std::vector<core::boundary_layer::ViscalIterationRecord> iterations;
  std::vector<OperPointEventRecord> events;
  core::boundary_layer::ViscalCompletionRecord completion;
  bool completionValid{false};
  bool viscalRan{false};
  bool viscalConverged{false};
};

// Last analysis result (Fortran Cl/CM/Cd/CDP/CDF/XCP/RMSBL/RMXBL diagnostics,
// ITER). Value type; stays valid after further session calls.
struct AnalysisResultState {
  AnalysisResult lastResult;
  // Numerical single-point diagnostics of the last point. Public
  // API/batch/sequence paths ignore this record; their behavior is
  // unchanged.
  OperPointDiagnostics lastDiagnostics;
};

// Polar session state (Fortran XFOIL.INC CR14/CL01):
// slots corresponds to CPOL/CPOLSD/CPOLXY(:,:,IP) with NPOL = slots.size
// (canonical capacity limit maxPolarSlots = NPX = 12). saveFileNames
// holds PFNAME(IP) per slot (1:1 with slots). activeSlot is IPACT:
// -1 means Fortran IPACT = 0 (no accumulation), otherwise the 0-based
// slot index. accumulating is LPACC, includeCpMin/includeHingeMoment are
// LCMINP/LHMOMP (CINC/HINC). referenceSlots corresponds to
// CPOLREF/LABREF(:,:,1..4,IR) with NPOLREF = referenceSlots.size.
// The canonical 1-based command indexing (PDEL 0=all, -1=abort) remains
// a matter of the CLI layer; this state is consistently 0-based.
struct PolarSessionState {
  std::vector<core::polar::PolarDataset> slots;
  std::vector<std::string> saveFileNames;
  int activeSlot{-1};
  bool accumulating{false};
  bool includeCpMin{false};
  bool includeHingeMoment{false};
  // LPFILE/LPFILX (PLRINI/PLXINI): opened save/dump file of the
  // active polar. dumpFileNames is PFNAMX(IP) parallel to slots.
  bool saveFileOpen{false};
  bool dumpFileOpen{false};
  std::vector<std::string> dumpFileNames;
  std::vector<core::polar::ReferencePolarDataset> referenceSlots;
};

// Canonical defaults record (xfoil.def, GETDEF/WRTDEF
// xfoil.f 867-1030). values carries all 13 positional lines losslessly,
// including the pure plot fields (Size/plotAR/CH/ScrnFr/Xpage/Ypage/
// Xmargn/Ymargn/Lcolor/Lcursor, CPmax/CPmin/CPdel, XoffAir/ScalAir/BLUwt,
// polar axis bounds); these are not interpreted as product state
// and enable no plot backend. idevrp corresponds to Fortran IDEVRP
// (LCOLOR = IDEVRP.EQ.4). loaded/sourceFile document the origin of the
// last applied data set. The start values are the INIT defaults.
inline io::DefaultsValues initDefaultsValues() {
  // INIT assignments xfoil.f 545-660/742-745 (only the fields
  // occurring in xfoil.def).
  io::DefaultsValues v;
  v.npan = 160;
  v.cvpar = 1.0;
  v.cterat = 0.15;
  v.ctrrat = 0.2;
  v.xsref1 = 1.0;
  v.xsref2 = 1.0;
  v.xpref1 = 1.0;
  v.xpref2 = 1.0;
  v.size = 10.0;
  v.plotar = 0.55;
  v.ch = 0.015;
  v.scrnfr = 0.80;
  v.xpage = 11.0;
  v.ypage = 8.5;
  v.xmarg = 0.0;
  v.ymarg = 0.0;
  v.lcolor = true;   // IDEVRP = 4
  v.lcurs = true;
  v.cpmax = 1.0;
  v.cpmin = -2.0;
  v.cpdel = -0.5;
  v.xofair = 0.09;
  v.facair = 0.70;
  v.uprwt = 0.02;
  v.clpol = {0.0, 1.5, 0.5};
  v.cdpol = {0.0, 0.04, 0.01};
  v.alpol = {-4.0, 10.0, 2.0};
  v.cmpol = {-0.25, 0.0, 0.05};
  v.matyp = 1;
  v.minf1 = 0.0;
  v.vaccel = 0.01;
  v.retyp = 1;
  v.rmill = 0.0;
  v.acrit = {9.0, 9.0};
  v.xtrip = {1.0, 1.0};
  return v;
}

struct DefaultsSessionState {
  io::DefaultsValues values = initDefaultsValues();
  int idevrp{4};
  bool loaded{false};
  std::string sourceFile;
};

// GDES/CAMB design session state
// (fortran-src/xgdes.f, xtcam.f, XFOIL.INC/XDES.INC).
//
// Canonical separation (Fortran finding): in Fortran the GDES/CAMB
// working geometry is identical to the TOP buffer (COMMON CR14 XB/YB/NB);
// GDES copies nothing at entry and keeps no stored geometry snapshot. The
// domain commit boundary runs between the buffer (working geometry) and
// the current panel contour (X/Y/N, ABCOPY/PANGEN). This state therefore
// explicitly contains no second geometry: the working geometry stays
// session.geometry.buffer, and a later EXEC carries it over via the
// confirmed copyBufferToCurrent contract. An entry snapshot for RESE does
// not exist in Fortran: GDES 'RESE' only resets the plot frame
// (PLTINI/GOFINI/PLOTG); recovering the geometry from the current contour
// is the command GSET.
//
// Contained are only instance-related design state fields (COMMON
// CL01/CL02, CR14 flap, CR19/CR20 ONAME) and the non-graphical
// plot-options part required for the canonical PLOP menu text (OPLSET,
// plutil.f 21-176). Pure plot frame fields (XOFF/YOFF/XSF/YSF/XWIND/YWIND,
// CR18), cursor positions, hardcopy/device state and plot arrays are
// deliberately not recorded. The numerical plot values SIZE/PLOTAR/CH/
// SCRNFR/XPAGE/YPAGE/XMARG/YMARG/LCURS/LCOLOR have their single storage
// location in the DefaultsSessionState (xfoil.def fields);
// DesignSessionState adds only the OPLSET fields not tracked in xfoil.def.
struct DesignFlapState {
  double xbf{0.0};      // Fortran XBF (buffer flap hinge, FLAP)
  double ybf{0.0};      // Fortran YBF
  bool lbFlap{false};   // Fortran LBFLAP (buffer flap defined)
  double xof{0.0};      // Fortran XOF (current contour, ABCOPY adoption)
  double yof{0.0};      // Fortran YOF
  bool lFlap{false};    // Fortran LFLAP
  // The MHINGE/PSILIN routine names (xoper.f) are not state fields:
  // no COMMON field exists for them.
  // The hinge-moment state (HMOM/HFX/HFY) and psi outputs are
  // Fortran scratch/outputs per call and are derived from the
  // existing core (inviscid::mhinge, inviscid::psiLin).
};

// OPLSET fields that are not in xfoil.def (XFOIL.INC CR13/CR14).
// IDEV/IDEVRP control the menu display values "G raphics-enable flag" and
// "C olor PostScript output?"; LCURS/LLAND/ICOLS appear as text values.
// The numerical OPLSET values (SIZE, PLOTAR, CH, SCRNFR, XPAGE, YPAGE,
// XMARG, YMARG) and LCURS remain in the DefaultsSessionState.values.
struct PlotOptionState {
  int idev{1};                     // INIT: IDEV = 1 (xfoil.f 713)
  int ipslu{0};                    // INIT: IPSLU = 0 (xfoil.f 724)
  bool lland{true};                // INIT: LLAND = .TRUE. (xfoil.f 488)
  std::array<int, 2> icols{8, 3};  // INIT: ICOLS = 8, 3 (xfoil.f 747-748)
};

// Menu/redo state. Fortran GDES keeps COMOLD/ARGOLD as SAVE locals:
// they survive GDES calls within one program run (xgdes.f 18,
// 41-42, 58-70). CAMB resets COMOLD/ARGOLD per call (xtcam.f 29,
// 43-44) and maintains them only in the SCAL branch (xtcam.f 655-656);
// repeated '!' repeats there only SCAL. nullptr corresponds to COMOLD
// == '****' ("Previous ... command not valid").
struct DesignMenuState {
  bool active{false};  // active GDES/CAMB menu level
  std::optional<std::pair<std::string, std::string>> gdesRedo;
  std::optional<std::pair<std::string, std::string>> cambRedo;
};

// GDES working buffer state: the spline quantities SB/XBP/YBP (CR14)
// belonging to the buffer coordinates and the buffer geometry parameters
// (SBLE..CAMBRB) maintained by GEOPAR. No second coordinate store:
// XB/YB remain finally in GeometrySessionState.buffer; this field holds
// only the derived working quantities, so that numerical GDES commands
// (ADEG without SCALC, SEVAL/SINVRT paths) continue the canonical Fortran
// state.
struct DesignBufferWorkState {
  std::vector<double> s;   // Fortran SB
  std::vector<double> xp;  // Fortran XBP
  std::vector<double> yp;  // Fortran YBP
  core::geometry::GeometryMetrics metrics;  // Fortran SBLE..CAMBRB
};

struct DesignSessionState {
  DesignMenuState menu;
  DesignFlapState flap;
  PlotOptionState plot;

  // COMMON flags (XFOIL.INC CL01/CL02, INIT values xfoil.f 489-509).
  bool lgSame{false};    // LGSAME (INIT .FALSE.; ABCOPY/GSET set .TRUE.)
  bool lgSym{false};     // LGSYM (INIT .FALSE.; GDES exit sets .FALSE.)
  bool lgSlop{true};     // LGSLOP (GDES SLOP)
  bool lcSlop{true};     // LCSLOP (CAMB SLOP)
  bool lpLcam{false};    // LPLCAM (GDES entry sets .FALSE.)
  bool ldCplot{false};   // LDCPLOT (CAMB)
  bool lgParm{true};     // LGPARM (CAMB entry saves it in lgParmSave)
  bool lgGrid{true};     // LGGRID
  bool lgTick{true};     // LGTICK
  double gtick{0.0005};  // GTICK (INIT xfoil.f 563; GDES TSIZ)

  // CAMB entry safeguard LGPARSAVE (xtcam.f 41, 184).
  bool lgParmSave{true};

  // ONAME: persistent overlay file name (XFOIL.INC 31/57, OVER xgdes.f
  // 2280-2289). Empty corresponds to Fortran ' '.
  std::string oname;

  // CAMB sub-state (CR19). Filled only by the CAMB block-100 path; it
  // stays empty otherwise. No second airfoil-geometry memory:
  // the camber/loading work arrays are CAMB field state, not a contour.
  std::optional<core::geometry_design::CamberSession> camber;

  // Buffer revision at GDES entry; a later EXEC checks it so that the
  // working geometry is verified against the TOP buffer. No geometry copy.
  std::uint64_t entryBufferRevision{0};

  // Derived working quantities of the GDES buffer (SB/XBP/YBP/GEOPAR).
  // Filled only at GDES entry; empty corresponds to no
  // active design buffer.
  std::optional<DesignBufferWorkState> work;
};

// Inverse session state (MDES/QDES).
//
// Authority: fortran-src/xmdes.f, xqdes.f, XFOIL.INC (CR07/CI01/CL01),
// CIRCLE.INC (CPI01/CPR01/CPC01). In Fortran, MDES and QDES share the
// same COMMON fields (QSPEC/SSPEC/QGAMM/NSP/NQSP/IQ1/IQ2, CIRCLE.INC) and
// must therefore live in exactly one shared InverseSessionState; separate
// are exclusively the per-menu SAVE locals COMOLD/ARGOLD (redo) and the
// active menu level.
// No second numerics, no CLI texts, no streams, no
// filesystem access, no static fields, no reserved
// index-0 cell, consistently 0-based. Fixed Fortran capacities are
// only documented bounds (IPX = limits::qspecDistributionCapacity = 5,
// ICX = core::inverse::kCircleMaxPoints = 257, IQX =
// limits::surfaceSystemCapacity = 601); active lengths (nsp, nqsp) are
// tracked separately from the capacity.
//
// Validity/provenance contract (canonical write sites):
// LEIW (xfoil.f 478 INIT=false, xmdes.f 76 true): the circle-grid unit
// numbers depend exclusively on NC1; geometry changes do not invalidate
// them.
// LSCINI (INIT false xfoil.f 479; ABCOPY false xgdes.f 1027; PANGEN
// false xfoil.f 2087; APCOPY false xpol.f 172; QDES EXEC false
// xqdes.f 499; MDES INIT false xmdes.f 204; MDES entry true
// after SCINIT xmdes.f 88): s(w)/mapping coefficients of the
// current panel contour.
// LQSPEC (QSPCIR true xmdes.f 813; QDES entry true xqdes.f 101;
// PANGEN false xfoil.f 2088; MDES/QDES entry prechecks
// false xmdes.f 69, xqdes.f 69; MDES INIT false xmdes.f 203):
// target Q distributions.
// LIQSET (IQSGET true xqdes.f 1047; QDES entry false for
// NSP.NE.N xqdes.f 70): marked target segment; the canonical
// marking lies in shared.target.liqset (core field).
// Revision binding: provenance.sourcePanelRevision is the panel-contour
// revision (PanelingSessionState.panelRevision) at the time of the
// numerical binding (SCINIT path); provenance.sourceBufferRevision is the
// buffer revision of the panel provenance. No freshness check based on
// point count alone.

// Validity of the circle-grid unit numbers (Fortran LEIW).
enum class CircleGridValidity {
  notInitialized,  // LEIW = .FALSE. (INIT; EIWSET not yet run)
  initialized,     // LEIW = .TRUE. (EIWSET(NC1), dependent only on NC1)
};

// Validity of the circle-level arc length s(w) and the mapping
// coefficients derived from it (Fortran LSCINI).
enum class MappingCoordinateValidity {
  neverInitialized,          // LSCINI = .FALSE., never initialized
  initializedFromPanel,      // LSCINI = .TRUE., from the current panel contour
  staleAfterPanelChange,     // outdated by a geometry/panel change
};

// Validity of the target Q distributions (Fortran LQSPEC).
enum class QSpecValidity {
  absent,                    // LQSPEC = .FALSE., no target Q
  present,                   // LQSPEC = .TRUE. (origin: qspecOrigin)
  fileDataLoadedNotCommitted,  // READ raw data (GETVOV) completely
                               // read, session commit still pending
                               // (transaction contract)
};

// Domain origin of a present target Q.
enum class QSpecOrigin {
  none,       // no target Q present
  currentQ,   // initialized from the current Q (QSPCIR/GAMQSP)
  menuEdits,  // changed by menu commands (AQ/CQ/SYMM/SMOO/FILT/PERT)
  fileRead,   // read from a V/Vinf file (GETVOV)
};

// Shared numerical inverse state: the CR07/CI01/CIRCLE.INC data that
// Fortran shares between MDES and QDES, held exactly once. The menu
// states below contain no numerics.
struct InverseSharedState {
  // CIRCLE.INC CPI01/CPR01/CPC01 complete (NC/MC/MCT/WC/EIW/DWC,
  // CN/PIQ, SC/SCOLD/XCOLD/YCOLD, ZC/ZC_CN, AGTE/DZTE, QIM0/QIMOLD/AG0,
  // CHORDZ/ZLEOLD/ZCOLDW).
  core::inverse::CircleMappingState circle;

  int nc1{257};   // CI01 NC1 (INIT xfoil.f 530-533: min(2**9+1, 257))
  int nsp{0};     // CI01 NSP (active length of the Qspec/QGAMM fields)

  // CR07 current Q capture (active length nsp, 0-based):
  // QGAMM/SSPEC/XSPOC/YSPOC/SSPLE.
  std::vector<double> qgamm;
  std::vector<double> sspec;
  std::vector<double> xspoc;
  std::vector<double> yspoc;
  double ssple{0.0};
  // CR07 ALGAM/CLGAM/CMGAM: operating point of the currently captured Q.
  double algam{0.0};
  double clgam{0.0};
  double cmgam{0.0};

  // Target Q columns: QSPEC(:,K)/QSPECP(:,K) per KQSP plus ALQSP/CLQSP/
  // CMQSP(KQSP) in the al/cl/cm fields. The quantity is always nqsp
  // (capacity limited by IPX = limits::qspecDistributionCapacity).
  // SSPEC stays once above (sspec); the s fields of the distributions
  // stay empty in session memory and are filled from sspec only for
  // kernel calls (SCINIT, MIXED). Likewise the qdof fields of
  // the distributions remain unused: QDOF0..3 are a single COMMON field
  // (below).
  std::vector<core::inverse::QSpecDistribution> qspecs;
  int nqsp{0};    // CI01 NQSP
  int iacqsp{0};  // CI01 IACQSP: 1 = ALQSP given, 2 = CLQSP
  // CI01 KQTARG. Canonical Fortran semantics (COMMON/CR07):
  // KQTARG=0 (MDES without a prior QDES run) points at column QSPEC(:,0),
  // which in COMMON exactly aliases the range QGAMM(IBX). Modeled
  // faithfully as -1 (no target-Q column); QDES entry sets
  // Fortran KQTARG=1 -> 0-based column 0.
  int kqtarg{-1};

  // CI01 IQ1/IQ2 and CL01 LIQSET: shared target segment of both menus
  // (0-based inclusive bounds; liqset is the canonical marking).
  core::inverse::QSpecTargetSegment target;

  // CR07 QDOF0..3: single COMMON field for the mixed-inverse
  // extra degrees of freedom (GAMQSP/SYMQSP zeros, MIXED update).
  double qdof0{0.0};
  double qdof1{0.0};
  double qdof2{0.0};
  double qdof3{0.0};

  // CR07 PSIO: persistent Psi0 offset of the MIXED Newton system
  // (COMMON behavior across several EXEC calls).
  double psio{0.0};

  // CR07 FFILT: inverse-mapping auto-filter level (INIT 0.0, xfoil.f 704;
  // MAPGEN call xmdes.f 619).
  double ffilt{0.0};
};

// Explicit validity and provenance structure (no loose flag group).
struct InverseValidityState {
  CircleGridValidity grid{CircleGridValidity::notInitialized};
  MappingCoordinateValidity mapping{
      MappingCoordinateValidity::neverInitialized};
  QSpecValidity qspec{QSpecValidity::absent};
  QSpecOrigin qspecOrigin{QSpecOrigin::none};
};

// Revision binding to the actual numerical source.
struct InverseProvenanceState {
  std::uint64_t sourcePanelRevision{0};     // panelRevision for SCINIT
  std::uint64_t sourceBufferRevision{0};    // buffer revision of the panel provenance
  PanelingOrigin sourcePanelOrigin{PanelingOrigin::none};  // abcopy/pangen
};

// COMMON CL01 inverse flags: shared memory (Fortran COMMON); the
// writing menus are documented per field. Pure plot flags (LQSPPL,
// LQGRID, LSYM, NTQSPL) are deliberately not tracked (plot boundary).
struct InverseMenuFlags {
  bool lqSym{false};    // LQSYM: MDES SYMM (xmdes.f 292); MDES prompt
                        // variant (.MDESs); the exit of both menus sets
                        // false (xmdes.f 143, xqdes.f 136)
  bool lqSlop{true};    // LQSLOP: INIT true (xfoil.f 502); MDES SLOP
                        // (xmdes.f 509) and QDES SLOP (xqdes.f 311)
  bool lqVdes{false};   // LQVDES: INIT false (xfoil.f 467); MDES VISC
                        // (xmdes.f 320), QDES VISC (xqdes.f 197)
  bool lqRefl{false};   // LQREFL: INIT false (xfoil.f 469); MDES REFL
                        // (xmdes.f 333), QDES REFL (xqdes.f 210)
  bool lcpxx{false};    // LCPXX: INIT false (xfoil.f 466); writing
                        // access only QDES CPXX (xqdes.f 404)
};

// MDES menu/redo state. COMOLD/ARGOLD are separated per menu as
// SAVE locals (xmdes.f 44, 114-115, 126-134); nullopt corresponds to
// COMOLD == '****' ("Previous .MDES command not valid"). Not shared
// with OPER '!'/GDES '!'/CAMB '!'.
struct MdesMenuState {
  bool active{false};
  std::optional<std::pair<std::string, std::string>> redo;
};

// QDES menu/redo state (xqdes.f 42, 112-113, 119-130), separate from the
// MDES redo.
struct QdesMenuState {
  bool active{false};
  std::optional<std::pair<std::string, std::string>> redo;
};

// Shared inverse session state (MDES/QDES): the Fortran CR07/CI01/
// CIRCLE.INC data shared by MDES and QDES is held exactly once.
struct InverseSessionState {
  InverseSharedState shared;
  InverseValidityState validity;
  InverseProvenanceState provenance;
  InverseMenuFlags flags;
  MdesMenuState mdes;
  QdesMenuState qdes;
};

// Session composition: the part states
// remain domain-separated; the use-case operations (SessionOps.h)
// coordinate them with the core kernels.
struct SessionComposition {
  GeometrySessionState geometry;
  PanelingSessionState paneling;
  OperatingConditionState operating;
  ViscousSessionState viscous;
  AnalysisResultState result;
  PolarSessionState polar;
  DefaultsSessionState defaults;
  DesignSessionState design;
  InverseSessionState inverse;  // MDES/QDES
  AnalysisConfig config;  // existing API contract: xfoil::XFoil::config
  // Prefix service for the native output commands.
  // No Fortran-COMMON mirror, but an app service; the BatchRunner sets
  // it before the run from --prefix, and interactive use keeps the
  // default (empty = identity). Exactly one transformation per output
  // file at the application points.
  xfoil::io::OutputPathPolicy outputPaths;
};

}  // namespace session

// End: include/xfoil/session/SessionState.h

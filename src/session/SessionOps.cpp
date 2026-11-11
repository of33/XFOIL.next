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

// File: src/session/SessionOps.cpp
// Implementation of the session/use-case operations. The
// operations orchestrate exclusively the confirmed core kernels in
// canonical Fortran evaluation order (xfoil.f SPECAL/INIT, xoper.f
// SPECAL/SPECCL/VISCAL/FCPMIN, xpanel.f XYWAKE, xgdes.f ABCOPY).
//
// No dependency on the app, CLI or test layers; no 1-based compatibility arrays. The
// xcp diagnostics quantity (no canonical Fortran field) comes exclusively from confirmed
// 0-based core results: the confirmed core kernel clCalc
// carries the additive smoothing terms as LoadResult.centerOfPressureX
// (bit-identical to the previous public API path, proof
// tests/core/XcpDerivationTests.cpp).

#include "xfoil/session/SessionOps.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <complex>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "xfoil/AnalysisConfig.h"
#include "xfoil/Limits.h"
#include "xfoil/core/boundary_layer/Coupling.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/geometry_design/GeometryDesignOps.h"
#include "xfoil/core/geometry_design/ThicknessCamberOps.h"
#include "xfoil/core/inverse/InverseState.h"
#include "xfoil/core/inverse/MixedInverseKernels.h"
#include "xfoil/core/inverse/CircleKernels.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/core/polar/PolarOps.h"
#include "xfoil/core/inviscid/Influence.h"
#include "xfoil/core/inviscid/Loads.h"
#include "xfoil/core/inviscid/SpecifiedPoint.h"
#include "xfoil/core/inviscid/Stagnation.h"
#include "xfoil/core/inviscid/TrailingEdge.h"
#include "xfoil/core/paneling/Abcopy.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/core/paneling/PanelParameters.h"

namespace xfoil::session {

namespace {

constexpr std::size_t kMinimumGeometryPoints = 3;

// Fortran NAME is CHARACTER*48 (XFOIL.INC). Assignment to the field
// truncates to the field length; STRIP only removes leading and
// trailing blanks and preserves internal spaces.
constexpr std::size_t kFortranNameFieldLength = 48;

std::string truncateFortranNameField(std::string_view text) {
  if (text.size() > kFortranNameFieldLength) {
    return std::string(text.substr(0, kFortranNameFieldLength));
  }
  return std::string(text);
}

std::string stripFortranBlanks(std::string_view text) {
  std::size_t first = 0;
  while (first < text.size() && text[first] == ' ') {
    ++first;
  }
  if (first == text.size()) {
    return {};
  }
  std::size_t last = text.size();
  while (last > first && text[last - 1] == ' ') {
    --last;
  }
  return std::string(text.substr(first, last - first));
}

std::string canonicalFortranName(std::string_view text) {
  return stripFortranBlanks(truncateFortranNameField(text));
}

// Fixed upper bound for the count of analysis points in a polar sequence
// (canonical NSEQEX bound of the existing API path).
constexpr std::size_t kMaximumPolarPoints = 100000;

// Fortran IVX (canonical BL capacity, XFOIL.INC).
constexpr std::size_t kBoundaryLayerCapacity = 283;

// Fortran DTOR = PI/180 (xfoil.f INIT with PI = 4*ATAN(1.0)).
constexpr double kDtor = 0.017453292519943295;

// AnalysisConfig from the operating state (only for the validation check
// of the existing API contract).
AnalysisConfig buildAnalysisConfig(const OperatingConditionState& operating) {
  AnalysisConfig config;
  config.reynoldsNumber = operating.machReynolds.reinf1;
  config.machNumber = operating.machReynolds.minf1;
  config.viscous = operating.viscous;
  config.reynoldsDependency =
      static_cast<ReynoldsDependency>(operating.machReynolds.reType);
  config.machDependency =
      static_cast<MachDependency>(operating.machReynolds.machType);
  config.transition.nCritical = operating.criticalReynoldsExponents[0];
  config.transition.topTripX = operating.topTripX;
  config.transition.bottomTripX = operating.bottomTripX;
  config.maximumIterations = operating.maximumIterations;
  config.autoInitializeBoundaryLayer = operating.autoInitializeBoundaryLayer;
  config.viscousAcceleration = operating.viscousAcceleration;
  return config;
}

// XCP diagnostics quantity (no canonical Fortran field) comes exclusively from confirmed 0-based
// core results: the confirmed core kernel clCalc computes the additive
// smoothing terms as LoadResult.centerOfPressureX — the same canonical
// CALL CLCALC as in the Fortran VISCAL loop (xoper.f 3001-3002). No
// 1-based compatibility arrays, no old non-core solver helper.
double centerOfPressureDiagnostic(const core::paneling::PanelGeometry& panel,
                                  const std::vector<double>& gam,
                                  const std::vector<double>& gamA, double alfa,
                                  double minf, double qinf, double xref,
                                  double yref) {
  const auto loaded = core::inviscid::clCalc(panel.x, panel.y, gam, gamA, alfa,
                                             minf, qinf, xref, yref);
  return loaded.centerOfPressureX;
}

// Fortran VISCAL iteration outputs (xoper.f 2990-3003) as structured
// session diagnostics in the existing format class. The station indices
// are emitted in the Fortran convention (1-based).
std::string formatViscalDiagnostics(const core::boundary_layer::ViscalResult&
                                        viscal) {
  std::ostringstream out;
  for (const auto& record : viscal.iterationRecords) {
    if (record.rlx < 1.0) {
      out << "     rms:" << std::scientific << std::setprecision(2)
          << record.rmsbl << "   max:" << std::scientific
          << std::setprecision(2) << record.rmxbl << " at "
          << record.imxbl + 1 << " " << record.ismxbl + 1 << "   rlx:"
          << std::fixed << std::setprecision(3) << record.rlx << "\n";
    } else if (std::fabs(record.rlx - 1.0) < 0.001) {
      out << "     rms:" << std::scientific << std::setprecision(2)
          << record.rmsbl << "   max:" << std::scientific
          << std::setprecision(2) << record.rmxbl << " at "
          << record.imxbl + 1 << " " << record.ismxbl + 1 << "\n";
    }
    out << "     a=" << (viscal.alfa / kDtor) << "    cl=" << viscal.cl
        << "\n     cm=" << viscal.cm << "  cd=" << viscal.cd
        << " => cdf=" << viscal.cdf
        << " cdp=" << (viscal.cd - viscal.cdf) << "\n\n";
  }
  if (viscal.converged) {
    out << "----------CONVERGED----------\n\n";
  }
  return out.str();
}

// Fortran VISCAL input (xoper.f VISCAL arguments) from the operating state.
// iterationAllowance carries the path-dependent bound: single point/batch
// ITMAX, OPER sequence ITMAX+5 (ITMAXS, xoper.f 679), LRECALC continuation ITMAX.
// lalfa is the retained LALFA of the last point; clspec the retained CLSPEC.
core::boundary_layer::ViscalInput buildViscalInput(
    const OperatingConditionState& operating,
    const core::boundary_layer::BoundaryLayerParameters& blpar, int idamp,
    int iterationAllowance, bool lalfa, double clspec) {
  core::boundary_layer::ViscalInput input{};
  input.niter = static_cast<std::size_t>(iterationAllowance);
  input.qinf = operating.qinf;
  input.xref = operating.momentReferenceX;
  input.yref = operating.momentReferenceY;
  input.vaccel = operating.viscousAcceleration;
  input.lalfa = lalfa;
  input.lvisc = operating.viscous;
  input.machType = operating.machReynolds.machType;
  input.reType = operating.machReynolds.reType;
  input.minf1 = operating.machReynolds.minf1;
  input.reinf1 = operating.machReynolds.reinf1;
  input.clspec = clspec;
  input.ivx = kBoundaryLayerCapacity;
  input.acrit = {operating.criticalReynoldsExponents[0],
                 operating.criticalReynoldsExponents[1]};
  input.xstrip = {operating.topTripX, operating.bottomTripX};
  // BLPAR from the authoritative session state (COMMON/BLPAR).
  input.blpar = blpar;
  // IDAMP from the authoritative session state (COMMON/CI05).
  // SETBL passes it on per call via IDAMPV (xbl.f:77); AXSET selects
  // DAMPL (0) or DAMPL2 (1) (xblsys.f:70). No core default fallback.
  input.idamp = idamp;
  return input;
}


// Shared VISCAL execution plus FCPMIN and result update.
// reused by SPECAL-/SPECCL points and LRECALC continuations without
// duplicating the orchestration. Return value false for a controlled core error
// (the result was then filled via mapCoreError). The callers have viscals
// (gam/gamA/alfa/minf/reinf/cl) already set.
bool applyViscousStage(SessionComposition& session,
                       const core::inviscid::WakeState& wakeState,
                       const core::inviscid::TrailingEdgeState& te,
                       const core::boundary_layer::ViscalInput& input,
                       AnalysisResult& result) {
  core::paneling::PanelGeometry& panel = session.paneling.panel;
  const core::inviscid::InfluenceSystem& system = session.viscous.system;
  core::boundary_layer::ViscalState& viscals = session.viscous.viscals;
  auto mapCoreError = [&result](const std::string& message) {
    result.status = AnalysisStatus::invalidAnalysisSettings;
    result.successful = false;
    result.diagnostic = message;
  };
  try {
    const core::boundary_layer::ViscalResult viscal =
        core::boundary_layer::viscal(input, panel, system, wakeState, te,
                                     viscals);

    // Fortran 682: ADEG = Alfa/DTOR after VISCAL.
    result.alpha = toDegrees(Radians{viscals.alfa});
    session.viscous.converged = viscal.converged;
    // canonical result transfer of the VISCAL completion
    // CPCALC call (xoper.f:3026-3027): CPI from viscal.cp, CPV from
    // viscal.cpv (both length N+NW). Bit-identical adoption of the
    // core result, no recomputation.
    session.viscous.cpi = viscal.cp;
    session.viscous.cpv = viscal.cpv;

    // Fortran FCPMIN (xoper.f:1768, OPER VISC part) at the loop end.
    std::vector<double> xFull = panel.x;
    xFull.insert(xFull.end(), wakeState.x.begin(), wakeState.x.end());
    const auto cpmin =
        core::boundary_layer::fcpmin(true, xFull, viscal.cp, viscal.cpv);
    static_cast<void>(cpmin);

    result.status = viscal.converged ? AnalysisStatus::converged
                                     : AnalysisStatus::unconverged;
    result.iterations = static_cast<int>(viscal.iterations);
    result.converged = viscal.converged;
    result.successful = true;
    result.liftCoefficient = viscal.cl;
    result.dragCoefficient = viscal.cd;
    result.momentCoefficient = viscal.cm;
    result.centerOfPressureX = centerOfPressureDiagnostic(
        panel, viscals.gam, viscals.gamA, viscals.alfa, viscals.minf,
        session.operating.qinf, session.operating.momentReferenceX,
        session.operating.momentReferenceY);
    result.rmsResidual = viscal.rmsbl;
    result.maximumResidual =
        viscal.iterationRecords.empty()
            ? 0.0
            : viscal.iterationRecords.back().rmxbl;
    result.diagnostic = formatViscalDiagnostics(viscal);
    // Structured numerical VISCAL records for the CLI layer
    // (formats 2000/2010/2020 and 9922); no renewed solver run.
    session.result.lastDiagnostics.iterations = viscal.iterationRecords;
    // VISCAL events (QDCALC preHead, BL marching) are appended to the
    // point event stream; the capture order is preserved.
    for (const auto& ev : viscal.events) {
      session.result.lastDiagnostics.events.push_back(
          OperPointEventRecord{OperPointEventKind::viscous, 0.0, ev});
    }
    session.result.lastDiagnostics.completion = viscal.completion;
    session.result.lastDiagnostics.completionValid = viscal.completionValid;
    session.result.lastDiagnostics.viscalRan = true;
    session.result.lastDiagnostics.viscalConverged = viscal.converged;
    return true;
  } catch (const std::invalid_argument& error) {
    // Controlled core error (STFIND/IBLPAN/IBLSYS/NITER): structured
    // result status instead of an exception (existing API semantics).
    mapCoreError(error.what());
    return false;
  }
}

// Internal execution contract of the point kernels (single-point contract
// kernel). It separates the domain-specific differing
// state contracts cleanly and without a public API change:
// coldStart - API/batch (confirmed cold path):
// LIPAN/LBLINI/LVCONV/LWAKE per point false,
// wake new per point, VISCAL(NITER)=ITER.
// operSinglePointWarmStart - OPER single point ALFA/CLI/CL (xoper.f
// 416-542): LIPAN/LBLINI stay preserved,
// wake/LVCONV are reset only at the 1e-5 threshold (invalidation
// happens after the inviscid solution, whose Alfa/MINF are
// authoritative); the wake is reused,
// VISCAL(NITER)=ITMAX (xoper.f 435/492/527),
// no NSEQEX.
// operSequenceWarmStart - OPER ASEQ/CSEQ (xoper.f 660-753): flags
// stay preserved and are reset only at the
// 1e-5 threshold (invalidation
// before SPECAL, ASEQ uses the before-point MINF), wake
// is reused, VISCAL(NITER)=ITMAX+5
// (ITMAXS, xoper.f 679), NSEQEX in the
// sequence orchestration.
enum class PointExecutionPolicy {
  coldStart,
  operSinglePointWarmStart,
  operSequenceWarmStart,
};

// Internal point requirement/orchestration kind. It replaces the
// domain-specific insufficient boolean coupling "specifiedCl" and separates the
// canonically different inviscid kernels and LALFA contracts without
// boolean combinations at the call sites:
// specifiedAlpha - SPECAL, LALFA=true (ALFA, ASEQ, runSpecAlpha).
// specifiedClFixedAlpha - SPECCL, LALFA=true, no SPECAL afterwards
// (OPER single command CLI, xoper.f 475-507).
// specifiedClCoupledViscous - SPECCL, LALFA=false, no SPECAL afterwards
// (OPER single command CL, xoper.f 510-542).
// operSequenceSpecifiedCl - SPECCL then SPECAL, LALFA=false (CSEQ,
// xoper.f 672/678; API runSpecCl, unchanged).
enum class PointSpecificationPolicy {
  specifiedAlpha,
  specifiedClFixedAlpha,
  specifiedClCoupledViscous,
  operSequenceSpecifiedCl,
};

// COMOLD origin of the last OPER point (xoper.f 471/506/541).
OperPointOrigin pointOriginFor(PointSpecificationPolicy specPolicy) {
  switch (specPolicy) {
    case PointSpecificationPolicy::specifiedAlpha:
      return OperPointOrigin::alpha;
    case PointSpecificationPolicy::specifiedClCoupledViscous:
      return OperPointOrigin::cli;
    case PointSpecificationPolicy::specifiedClFixedAlpha:
    case PointSpecificationPolicy::operSequenceSpecifiedCl:
      return OperPointOrigin::cl;
  }
  return OperPointOrigin::none;
}


// Fortran SPECAL + VISCAL (or SPECCL with prescribed Cl): single point on
// the confirmed core path. Read: geometry/paneling/operating/viscous.
// Mutates: viscous (BL state per point according to the execution contract),
// result.
// Fortran FCPMIN call after each successful point command
// (xoper.f 437/494/528/684): the canonical COMMON snapshot
// CPMNI/XCPMNI/CPMNV/XCPMNV is set at the point in time and later
// printed unchanged by CPMN (Mach calls no FCPMIN,
// xoper.f 299-333). currentCpMinimum is the confirmed pure query.
void updateCpMinimumSnapshot(SessionComposition& session) {
  const CpObservation obs = currentCpMinimum(session);
  session.operating.cpMinimum.hasBeenUpdatedByFcpmin = true;
  if (obs.valid) {
    session.operating.cpMinimum.cpmni = obs.minimum.cpmni;
    session.operating.cpMinimum.xcpmni = obs.minimum.xcpmni;
    session.operating.cpMinimum.cpmnv = obs.minimum.cpmnv;
    session.operating.cpMinimum.xcpmnv = obs.minimum.xcpmnv;
  }
}

AnalysisResult runSpecifiedPoint(SessionComposition& session, Degrees alpha,
                                 double clSpec,
                                 PointSpecificationPolicy specPolicy,
                                 const CancellationToken* cancellation,
                                 PointExecutionPolicy policy,
                                 std::optional<double> alfaStartRadians = {}) {
  AnalysisResult result;
  result.alpha = alpha;
  // The numerical single-point diagnostics belong to the point; each point start
  // resets them (API/batch/sequence paths ignore them afterwards).
  session.result.lastDiagnostics = OperPointDiagnostics{};

  // Prerequisites in the order of the existing public API:
  // geometry, configuration, validation, abort.
  if (!session.geometry.valid || !session.paneling.panelingValid) {
    result.status = AnalysisStatus::missingGeometry;
    result.successful = false;
    session.result.lastResult = result;
    return result;
  }

  if (!session.operating.configured) {
    result.status = AnalysisStatus::missingConfiguration;
    result.successful = false;
    session.result.lastResult = result;
    return result;
  }

  const ConfigValidationResult validation =
      validate(buildAnalysisConfig(session.operating));
  if (!validation.success()) {
    result.status = AnalysisStatus::invalidConfiguration;
    result.successful = false;
    result.diagnostic = validation.message;
    session.result.lastResult = result;
    return result;
  }

  if (cancellation != nullptr && cancellation->isCancellationRequested()) {
    result.status = AnalysisStatus::cancelled;
    result.successful = false;
    session.result.lastResult = result;
    return result;
  }

  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  const OperatingConditionState& operating = session.operating;

  // Fortran SPECAL/SPECCL preparation: GGCALC influence system
  // (geometry- and QINF-dependent; GGCALC uses no MINF, xpanel.f:988).
  if (!session.viscous.systemValid ||
      session.viscous.systemQinf != operating.qinf) {
    const std::vector<double> sig0(panel.x.size(), 0.0);
    auto system = core::inviscid::ggCalc(panel, sig0, 0.0, operating.qinf);
    if (!system.isOk()) {
      result.status = AnalysisStatus::invalidAnalysisSettings;
      result.successful = false;
      result.diagnostic = system.error().message;
      session.result.lastResult = result;
      return result;
    }
    session.viscous.system = std::move(system.value());
    session.viscous.systemValid = true;
    session.viscous.systemQinf = operating.qinf;
    // Fortran: GGCALC writes at routine entry
    // 'Calculating unit vorticity distributions ...' (xpanel.f:1000);
    // the event arises only for the actual call from
    // SPECAL/SPECCL (xoper.f:2740/2832 gate).
    session.result.lastDiagnostics.events.push_back(OperPointEventRecord{
        OperPointEventKind::influenceSystemCalculation,
        toRadians(alpha).value, {}});
  }
  const core::inviscid::InfluenceSystem& system = session.viscous.system;

  const core::inviscid::MachReynoldsSettings settings{
      operating.machReynolds.machType, operating.machReynolds.reType,
      operating.machReynolds.minf1, operating.machReynolds.reinf1};

  // Fortran SPECCL/SPECAL start from the raw COMMON alfa
  // (CR09, radians). A degrees roundtrip (Alfa/DTOR, ADEG*DTOR) changes
  // near-zero values by up to 1 ULP. The sequence driver therefore
  // returns the raw radians; without that requirement the previous
  // degrees conversion stays.
  const double alfaRadians =
      alfaStartRadians.has_value() ? *alfaStartRadians
                                   : toRadians(alpha).value;

  // Single-point/API/batch cold path (existing semantics): LIPAN=false,
  // LBLINI=false, LVCONV=false, LWAKE=false per point. Station fields and DIJ
  // stay preserved. The OPER sequence contract preserves these flags and only
  // resets them conditionally per xoper.f 675-677.
  if (policy == PointExecutionPolicy::coldStart) {
    session.viscous.viscals.mapping.valid = false;
    session.viscous.viscals.lblini = false;
    session.viscous.viscals.lvconv = false;
    session.viscous.converged = false;
  }
  // For the ASEQ 1e-5 comparison the MINF of the previous point is authoritative
  // (xoper.f 677: MINF before SPECAL). CSEQ used the SPECCL MINF.
  const double previousMinf = session.viscous.viscals.minf;

  auto mapCoreError = [&result](const std::string& message) {
    result.status = AnalysisStatus::invalidAnalysisSettings;
    result.successful = false;
    result.diagnostic = message;
  };

  // Fortran SPECAL/SPECCL (inviscid kernel). The specification kind controls the
  // inviscid kernel and whether SPECAL follows SPECCL:
  // specifiedAlpha - only SPECAL (Alfa/ASEQ, xoper.f 428/678).
  // specifiedClFixedAlpha - only SPECCL, LALFA=true (CLI, xoper.f 484).
  // specifiedClCoupledViscous - only SPECCL, LALFA=false (Cl, xoper.f 519).
  // operSequenceSpecifiedCl - SPECCL then SPECAL (CSEQ, xoper.f 672/678;
  // API runSpecCl, unchanged contract).
  double specclMinf = 0.0;  // SPECCL MINF for the 1e-5 invalidation
  auto computeSpec = [&](double alfaStart)
      -> core::Result<core::inviscid::SpecPointResult> {
    if (specPolicy == PointSpecificationPolicy::specifiedAlpha) {
      return core::inviscid::specAlpha(system, panel, settings, alfaStart,
                                       operating.qinf,
                                       operating.momentReferenceX,
                                       operating.momentReferenceY);
    }
    auto speccl =
        core::inviscid::specCl(system, panel, settings, clSpec, alfaStart,
                               operating.qinf, operating.momentReferenceX,
                               operating.momentReferenceY);
    if (!speccl.isOk()) {
      return speccl;  // SPECCL error is mapped below via spec.isOk
    }
    specclMinf = speccl.value().minf;
    if (specPolicy == PointSpecificationPolicy::operSequenceSpecifiedCl) {
      // xoper.f 678: SPECAL for the Alfa achieved by SPECCL (CSEQ/API).
      return core::inviscid::specAlpha(
          system, panel, settings, speccl.value().alfa, operating.qinf,
          operating.momentReferenceX, operating.momentReferenceY);
    }
    // CLI/Cl single command: SPECCL is the point, no SPECAL afterwards.
    return speccl;
  };
  core::Result<core::inviscid::SpecPointResult> spec =
      computeSpec(alfaRadians);
  if (!spec.isOk()) {
    mapCoreError(spec.error().message);
    session.result.lastResult = result;
    return result;
  }

  // Alpha reported by the inviscid branch (the viscous branch overwrites
  // it with the VISCAL-converged alfa, xoper.f 682 ADEG=Alfa/DTOR).
  result.alpha = toDegrees(Radians{spec.value().alfa});

  // Fortran: canonical convergence abort in SPECAL/SPECCL. The message
  // is written at the error point and then falls through to the
  // completion block (xoper.f:2798-2800/2873-2875): SPECAL writes
  // on to OFS afterwards, SPECCL does not; both continue the point
  // normally (invalidation, then if applicable VISCAL). The event is
  // captured at the actual digit before the OFS marker.
  if (spec.value().failure != core::inviscid::SpecifiedPointFailure::none) {
    session.result.lastDiagnostics.specFailure = spec.value().failure;
    const auto kind =
        spec.value().failure ==
                core::inviscid::SpecifiedPointFailure::
                    specalMachConvergenceFailure
            ? OperPointEventKind::specalConvergenceFailure
            : OperPointEventKind::specclConvergenceFailure;
    session.result.lastDiagnostics.events.push_back(
        OperPointEventRecord{kind, spec.value().alfa, {}});
  }

  // OFS snapshot at the canonical time point: only SPECAL writes format 333
  // (xoper.f 2816-2818); CLI/Cl single commands (SPECCL without a following
  // SPECAL) produce no OFS state in Fortran and therefore get no
  // snapshot. CSEQ (SPECCL then SPECAL) preserves the SPECAL snapshot.
  if (specPolicy == PointSpecificationPolicy::specifiedAlpha ||
      specPolicy == PointSpecificationPolicy::operSequenceSpecifiedCl) {
    session.result.lastDiagnostics.specalSnapshot = SpecifiedPointSnapshot{
        spec.value().alfa, spec.value().cl, spec.value().cm,
        spec.value().cdp, true};
    // Fortran: SPECAL writes Format 333 at routine end (xoper.f:2816);
    // the marker event secures the chronological position between
    // GGCALC and XYWAKE. The values remain in the snapshot.
    session.result.lastDiagnostics.events.push_back(OperPointEventRecord{
        OperPointEventKind::specifiedPointOutput, spec.value().alfa, {}});
  }

  // OPER warm start: conditional invalidation. The OPER single point (xoper.f
  // 430-432/487-489/523-525) invalidates after the inviscid solution, whose
  // Alfa/MINF are authoritative (checkMinf = SPECAL-/SPECCL MINF). The OPER
  // sequence (xoper.f 675-677) invalidates before SPECAL: ASEQ uses the
  // before-point MINF, CSEQ the SPECCL MINF.
  if (policy != PointExecutionPolicy::coldStart) {
    double checkMinf = 0.0;
    if (policy == PointExecutionPolicy::operSequenceWarmStart) {
      checkMinf = (specPolicy == PointSpecificationPolicy::operSequenceSpecifiedCl)
                      ? specclMinf
                      : previousMinf;
    } else {
      checkMinf = spec.value().minf;
    }
    if (std::fabs(spec.value().alfa - session.viscous.awake) > 1.0e-5) {
      session.viscous.wakeValid = false;
    }
    if (std::fabs(spec.value().alfa - session.viscous.viscals.avisc) > 1.0e-5 ||
        std::fabs(checkMinf - session.viscous.viscals.mvisc) > 1.0e-5) {
      session.viscous.viscals.lvconv = false;
    }
  }

  // Fortran XYWAKE (xpanel.f): API/batch cold path new per point (LWAKE=false).
  // OPER warm start (single and sequence): wake only new for |Alfa-AWAKE|>1e-5;
  // otherwise preserved (xoper.f 430/487/523, 675, xpanel.f 1346-1347).
  core::inviscid::WakeState wakeState;
  if (policy != PointExecutionPolicy::coldStart &&
      session.viscous.wakeValid) {
    wakeState = session.viscous.wake;
    session.viscous.lastPointWake = wakeState;
    session.viscous.lastPointWakeValid = true;
  } else {
    auto wake = core::inviscid::xywake(
        panel, spec.value().gam, spec.value().gamA, system.gamu1, system.gamu2,
        spec.value().alfa, operating.wakeLength, operating.qinf);
    if (!wake.isOk()) {
      mapCoreError(wake.error().message);
      session.result.lastResult = result;
      return result;
    }
    wakeState = std::move(wake.value());
    // Fortran: XYWAKE writes 'Calculating wake trajectory ...' at routine
    // entry (xpanel.f:1277). The canonical WRITE sits in VISCAL
    // (xoper.f:2903-2905, !LWAKE); for inviscid points VISCAL does not
    // run, so the event stays there.
    // The alpha is the value produced by the inviscid path (SPECAL/SPECCL).
    if (operating.viscous) {
      session.result.lastDiagnostics.events.push_back(OperPointEventRecord{
          OperPointEventKind::wakeTrajectoryCalculation, spec.value().alfa,
          {}});
    }
    if (policy != PointExecutionPolicy::coldStart) {
      session.viscous.wake = wakeState;
      session.viscous.wakeValid = true;
      session.viscous.awake = spec.value().alfa;
    }
    // Continuation wake (LRECALC): the wake of the last point is
    // persisted per instance and separately from the OPER sequence wake
    // (LWAKE/AWAKE), so that continueViscousPoint reuses it without a renewed
    // SPECAL/XYWAKE. The cold path does not use LWAKE/AWAKE.
    session.viscous.lastPointWake = wakeState;
    session.viscous.lastPointWakeValid = true;
    // Fortran XYWAKE sets LWDIJ=.FALSE. (xpanel.f:1350): the wake-dependent
    // DIJ block must be recomputed with the new wake geometry; the
    // airfoil DIJ block (LADIJ) stays preserved. VISCAL sets LWDIJ after the
    // DIJ computation back to true, so that a LRECALC continuation
    // preserves the wake DIJ block.
    session.viscous.viscals.dijWakeValid = false;
  }

  // Fortran VISCAL session state: inviscid basis per point, BL state
  // persistent (Fortran COMMON behavior between the points).
  core::boundary_layer::ViscalState& viscals = session.viscous.viscals;
  viscals.alfa = spec.value().alfa;
  viscals.minf = spec.value().minf;
  viscals.reinf = spec.value().reinf;
  viscals.cl = spec.value().cl;
  // Fortran CLCALC (SPECAL/SPECCL) sets the COMMON quantities Cl, CM, CDP;
  // PLRADD (xpol.f 570-596) reads exactly these COMMON values, also for
  // inviscid points. VISCAL overwrites CM/CDP later for a viscous
  // point; the earlier adoption corresponds to the Fortran order.
  viscals.cm = spec.value().cm;
  viscals.cdp = spec.value().cdp;
  viscals.gam = spec.value().gam;
  viscals.gamA = spec.value().gamA;
  // canonical result transfer of the SPECAL-/SPECCL
  // completion CPCALC calls (xoper.f:2806-2812/2879-2885). The kernel
  // carries exactly the Fortran branch in SpecPointResult.cp: length N
  // (inviscid) or N+NW from QINV (LVISC). A later VISCAL run
  // overwrites CPI at its own completion call (xoper.f:3026),
  // exactly as Fortran overwrites the COMMON fields.
  session.viscous.cpi = spec.value().cp;

  const bool viscous = operating.viscous;

  try {
    if (viscous) {
      // Fortran: API/batch cold path and OPER single point use VISCAL(ITMAX);
      // the OPER sequence uses VISCAL(ITMAX+5) (ITMAXS, xoper.f 679).
      const int iterationAllowance =
          policy == PointExecutionPolicy::operSequenceWarmStart
              ? operating.maximumIterations + 5
              : operating.maximumIterations;
      const bool lalfa =
          specPolicy == PointSpecificationPolicy::specifiedAlpha ||
          specPolicy == PointSpecificationPolicy::specifiedClFixedAlpha;
      const double viscalClspec =
          specPolicy == PointSpecificationPolicy::specifiedAlpha ? 0.0
                                                                 : clSpec;
      const core::boundary_layer::ViscalInput input = buildViscalInput(
          operating, session.viscous.blpar, session.viscous.idamp,
          iterationAllowance, lalfa, viscalClspec);
      if (!applyViscousStage(session, wakeState, spec.value().te, input,
                             result)) {
        session.result.lastResult = result;
        return result;
      }
      // LRECALC continuation (xoper.f 85-92/416-542): persist the inviscid
      // point state (TECALC, LALFA, CLSPEC) per instance, so that
      // a following '!' restarts only VISCAL(ITMAX) on this preserved state
      // without calling SPECAL/SPECCL again.
      session.viscous.lastTe = spec.value().te;
      session.viscous.lastPointLalfa = lalfa;
      session.viscous.lastClspec = viscalClspec;
      session.viscous.lastPointContinuable = true;
      session.viscous.lastPointOrigin = pointOriginFor(specPolicy);
    } else {
      // Inviscid point: no VISCAL and no viscous iteration
      // (existing API semantics). Cd stays without BL 0 (canonical: CDCALC
      // only in the VISCAL loop).
      result.status = AnalysisStatus::converged;
      result.successful = true;
      result.converged = true;
      result.iterations = 0;
      result.liftCoefficient = spec.value().cl;
      result.dragCoefficient = 0.0;
      result.momentCoefficient = spec.value().cm;
      result.centerOfPressureX = centerOfPressureDiagnostic(
          panel, spec.value().gam, spec.value().gamA, spec.value().alfa,
          spec.value().minf, operating.qinf, operating.momentReferenceX,
          operating.momentReferenceY);
      result.rmsResidual = 0.0;
      result.maximumResidual = 0.0;
      result.diagnostic = std::string();
      // Fortran: an inviscid OPER point holds the inviscid
      // COMMON quantities (GAM/GAM_A/Alfa/TECALC); a following '!'
      // repeats the point type with LRECALC and starts VISCAL on this
      // preserved state as soon as LVISC is set (xoper.f 85-92,
      // 416-435; binary `u_bang_inviscid_then_visc`: XYWAKE/QDCALC/
      // Initializing BL without SPECAL). The wake stays uncomputed
      // (lastPointWakeValid=false); VISCAL fetches it later if needed.
      session.viscous.lastTe = spec.value().te;
      session.viscous.lastPointLalfa =
          specPolicy == PointSpecificationPolicy::specifiedAlpha ||
          specPolicy == PointSpecificationPolicy::specifiedClFixedAlpha;
      session.viscous.lastClspec =
          specPolicy == PointSpecificationPolicy::specifiedAlpha
              ? 0.0
              : clSpec;
      session.viscous.lastPointContinuable = true;
      session.viscous.lastPointOrigin = pointOriginFor(specPolicy);
      session.viscous.lastPointWakeValid = false;
    }
  } catch (const std::invalid_argument& error) {
    // Controlled core error (STFIND/IBLPAN/IBLSYS/NITER): structured
    // result status instead of an exception (existing API semantics).
    mapCoreError(error.what());
    session.result.lastResult = result;
    return result;
  }

  updateCpMinimumSnapshot(session);
  session.result.lastResult = result;
  return result;
}

}  // namespace

// Fortran LRECALC continuation (xoper.f 85-92/416-542): after a
// non-converged viscous single point it restarts only VISCAL(ITMAX) on
// the preserved point, wake, mapping, DIJ and BL state, without
// calling SPECAL/SPECCL again and without a cold-start reset. Reuses
// the confirmed VISCAL orchestration (applyViscousStage). No
// second solver implementation, no test-case logic.
// A non-continuable state (no geometry/panel/configuration, inviscid
// mode, no preceding viscous point, no valid wake) returns
// a controlled error status without partial mutation of the solution state.
AnalysisResult continueViscousPoint(SessionComposition& session,
                                    const CancellationToken* cancellation) {
  AnalysisResult result;

  if (!session.geometry.valid || !session.paneling.panelingValid) {
    result.status = AnalysisStatus::missingGeometry;
    result.successful = false;
    session.result.lastResult = result;
    return result;
  }
  if (!session.operating.configured) {
    result.status = AnalysisStatus::missingConfiguration;
    result.successful = false;
    session.result.lastResult = result;
    return result;
  }
  if (!session.operating.viscous || !session.viscous.lastPointContinuable) {
    // No continuable point state ('!' without a previous OPER point or
    // inviscid mode). Controlled status.
    result.status = AnalysisStatus::invalidAnalysisSettings;
    result.successful = false;
    result.diagnostic = "No viscous point available to continue";
    session.result.lastResult = result;
    return result;
  }
  if (cancellation != nullptr && cancellation->isCancellationRequested()) {
    result.status = AnalysisStatus::cancelled;
    result.successful = false;
    session.result.lastResult = result;
    return result;
  }

  // Fortran LRECALC: SPECAL/SPECCL drop; the inviscid COMMON quantities
  // (gam/gamA/alfa/minf/reinf/cl and TECALC) from the last point stay
  // preserved. Wake and DIJ stay preserved (the 1e-5 resets lie within
  // the .NOT.LRECALC branch). Iteration limit = ITMAX (xoper.f 435/492/527).
  const core::boundary_layer::ViscalInput input = buildViscalInput(
      session.operating, session.viscous.blpar, session.viscous.idamp,
      session.operating.maximumIterations, session.viscous.lastPointLalfa,
      session.viscous.lastClspec);

  // Fortran LRECALC: SPECAL/SPECCL/GGCALC omitted. If the wake of the
  // preserved point was never computed (inviscid point, then VISC+!), VISCAL
  // fetches it for !LWAKE via XYWAKE afterwards (xoper.f 2903-2905; binary
  // `u_bang_inviscid_then_visc`): XYWAKE event, but no SPECAL.
  session.result.lastDiagnostics = OperPointDiagnostics{};

  auto mapCoreError = [&result](const std::string& message) {
    result.status = AnalysisStatus::invalidAnalysisSettings;
    result.successful = false;
    result.diagnostic = message;
  };

  core::inviscid::WakeState wake = session.viscous.lastPointWake;
  if (!session.viscous.lastPointWakeValid) {
    const auto& panel = session.paneling.panel;
    const auto& system = session.viscous.system;
    auto wakeRes = core::inviscid::xywake(
        panel, session.viscous.viscals.gam, session.viscous.viscals.gamA,
        system.gamu1, system.gamu2, session.viscous.viscals.alfa,
        session.operating.wakeLength, session.operating.qinf);
    if (!wakeRes.isOk()) {
      mapCoreError(wakeRes.error().message);
      session.result.lastResult = result;
      return result;
    }
    wake = std::move(wakeRes.value());
    session.result.lastDiagnostics.events.push_back(OperPointEventRecord{
        OperPointEventKind::wakeTrajectoryCalculation,
        session.viscous.viscals.alfa, {}});
    session.viscous.wake = wake;
    session.viscous.wakeValid = true;
    session.viscous.awake = session.viscous.viscals.alfa;
    session.viscous.lastPointWake = wake;
    session.viscous.lastPointWakeValid = true;
    session.viscous.viscals.dijWakeValid = false;
  }

  if (!applyViscousStage(session, wake,
                         session.viscous.lastTe, input, result)) {
    session.result.lastResult = result;
    return result;
  }

  updateCpMinimumSnapshot(session);
  session.result.lastResult = result;
  return result;
}

// ---: inverse session state -------------------------------------------------
//
// No inverse numerics: the numerical INIT paths (EIWSET/SCINIT/
// MAPGAM/CNCALC/QSPCIR, SPECAL/Q capture/GAMQSP) are separate.
// These operations only set validity, origin,
// redo and menu states according to the canonical Fortran write sites
// (xmdes.f 53-106/139-147, xqdes.f 52-102/132-140, xfoil.f 466-533/704,
// xgdes.f 1027, xpol.f 172, xqdes.f 499).

void resetInverseDesignState(SessionComposition& session) {
  // INIT defaults xfoil.f 466-533/704 plus COMMON zeroes; corresponds to
  // a session reset and a new session.
  session.inverse = InverseSessionState{};
}

void invalidateInverseStateAfterPanelChange(SessionComposition& session,
                                            InversePanelChange change) {
  auto& inv = session.inverse;
  const bool wasBoundToPanel =
      inv.validity.mapping ==
      MappingCoordinateValidity::initializedFromPanel;
  switch (change) {
    case InversePanelChange::bufferToCurrent:     // xgdes.f 1027
    case InversePanelChange::polarArchivedCopy:   // xpol.f 172
    case InversePanelChange::mixedInverseExec:    // xqdes.f 499
      // LSCINI=.FALSE.: the binding of the circle-level arc length to the
      // old contour is outdated. LQSPEC, target segment, menu and
      // redo states stay canonically preserved.
      if (wasBoundToPanel) {
        inv.validity.mapping =
            MappingCoordinateValidity::staleAfterPanelChange;
      }
      break;
    case InversePanelChange::panelingGenerated:   // xfoil.f 2087-2088
      if (wasBoundToPanel) {
        inv.validity.mapping =
            MappingCoordinateValidity::staleAfterPanelChange;
      }
      // PANGEN additionally sets LQSPEC=.FALSE.
      inv.validity.qspec = QSpecValidity::absent;
      inv.validity.qspecOrigin = QSpecOrigin::none;
      break;
    case InversePanelChange::panelInvalidated:
      // Panel invalidation for buffer adoption without a new contour:
      // the current contour to which the numerical binding belonged
      // no longer exists; the LQSPEC flag semantics stay
      // unchanged (Fortran LOAD changes no inverse flags).
      if (wasBoundToPanel) {
        inv.validity.mapping =
            MappingCoordinateValidity::staleAfterPanelChange;
      }
      break;
  }
  // Menu/redo states remain unchanged in all cases.
}

InverseMenuEntryResult enterMdesMenuState(SessionComposition& session) {
  InverseMenuEntryResult result;
  // xmdes.f 53-57: N==0 -> message, RETURN, no state transition.
  if (!session.paneling.panelingValid ||
      session.paneling.panel.x.empty()) {
    return result;
  }
  auto& inv = session.inverse;
  // xmdes.f 68-72: NSP.NE.NC1 -> LQSPEC=.FALSE., IQ1=1, IQ2=NC1
  // (0-based 0..NC1-1); LIQSET stays unchanged.
  if (inv.shared.nsp != inv.shared.nc1) {
    inv.validity.qspec = QSpecValidity::absent;
    inv.validity.qspecOrigin = QSpecOrigin::none;
    inv.shared.target.iq1 = 0;
    inv.shared.target.iq2 = inv.shared.nc1 - 1;
  }
  // xmdes.f 79-83: NQSP==0 -> IACQSP=1, ALQSP(1)=Alfa, NQSP=1. Alfa is
  // the existing session operating state (viscous.viscals.alfa,
  // INIT default 0.0). The QSPEC column itself stays empty: its
  // filling happens only in the numerical INIT path (QSPCIR).
  if (inv.shared.nqsp == 0) {
    inv.shared.iacqsp = 1;
    inv.shared.nqsp = 1;
    inv.shared.qspecs.assign(1, core::inverse::QSpecDistribution{});
    inv.shared.qspecs[0].al = session.viscous.viscals.alfa;
  }
  // Menu state: COMAND='****' (xmdes.f 49) and COMOLD=COMAND before the
  // first prompt (xmdes.f 114) make the redo at entry ineffective.
  inv.mdes.active = true;
  inv.mdes.redo.reset();
  result.status = InverseMenuEntryStatus::entered;
  return result;
}

void leaveMdesMenuState(SessionComposition& session) {
  // xmdes.f 139-147: LPLOT/LQSPPL/CLRZOOM are plot boundaries; the
  // non-graphical part is LQSYM=.FALSE.
  session.inverse.mdes.active = false;
  session.inverse.flags.lqSym = false;
}

InverseMenuEntryResult enterQdesMenuState(SessionComposition& session) {
  InverseMenuEntryResult result;
  // xqdes.f 52-56: N==0 -> message, RETURN, no state transition.
  if (!session.paneling.panelingValid ||
      session.paneling.panel.x.empty()) {
    return result;
  }
  auto& inv = session.inverse;
  // xqdes.f 68-71: NSP.NE.N -> LQSPEC=.FALSE., LIQSET=.FALSE.
  const int n = static_cast<int>(session.paneling.panel.x.size());
  if (inv.shared.nsp != n) {
    inv.validity.qspec = QSpecValidity::absent;
    inv.validity.qspecOrigin = QSpecOrigin::none;
    inv.shared.target.liqset = false;
  }
  // SPECAL (xqdes.f 65), NSP=N and the Q capture (xqdes.f 79-89) are
  // numerical parts are separate; the limits branch sets only menu/redo.
  inv.qdes.active = true;
  inv.qdes.redo.reset();
  result.status = InverseMenuEntryStatus::entered;
  return result;
}

void leaveQdesMenuState(SessionComposition& session) {
  // xqdes.f 132-140: LQSYM=.FALSE. (shared COMMON flag), menu inactive.
  session.inverse.qdes.active = false;
  session.inverse.flags.lqSym = false;
}

MdesNumericsInitResult initializeMdesNumerics(SessionComposition& session,
                                              bool forceMapping) {
  // canonical numerical MDES INIT chain (xmdes.f 63-107)
  // on the current session panel contour, atomic commit into the
  // shared InverseSessionState. Exclusively core::inverse kernels.
  MdesNumericsInitResult result;
  auto& inv = session.inverse;

  // xmdes.f 53-57: N==0 -> '*** No airfoil available ***', no
  // state transition.
  if (!session.paneling.panelingValid || session.paneling.panel.x.empty()) {
    result.status = MdesNumericsInitStatus::noAirfoil;
    result.message = "MDES INIT: no airfoil available";
    return result;
  }

  // candidates in neutral core value types; committed only after success.
  core::inverse::FullInverseState fs;
  fs.circle = inv.shared.circle;
  const auto& panel = session.paneling.panel;
  fs.panel.x = panel.x;
  fs.panel.xp = panel.xp;
  fs.panel.y = panel.y;
  fs.panel.yp = panel.yp;
  fs.panel.s = panel.s;
  fs.panel.n = static_cast<int>(panel.x.size());
  fs.panel.sle = panel.sle;
  fs.panel.xle = panel.xle;
  fs.panel.yle = panel.yle;
  fs.panel.xte = panel.xte;
  fs.panel.yte = panel.yte;
  // MINF/QINF/Alfa: current session operating quantities (COMMON CR09).
  fs.minf = session.viscous.viscals.minf;
  fs.qinf = session.operating.qinf;
  fs.qgamm = inv.shared.qgamm;
  fs.sspec = inv.shared.sspec;
  fs.xspoc = inv.shared.xspoc;
  fs.yspoc = inv.shared.yspoc;
  fs.ssple = inv.shared.ssple;
  fs.nsp = inv.shared.nsp;
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  int nqsp = inv.shared.nqsp;
  int iacqsp = inv.shared.iacqsp;
  InverseValidityState validity = inv.validity;
  int iq1 = inv.shared.target.iq1;
  int iq2 = inv.shared.target.iq2;

  // xmdes.f 65-68: target Q does not come from the current NSP.
  if (fs.nsp != inv.shared.nc1) {
    validity.qspec = QSpecValidity::absent;
    validity.qspecOrigin = QSpecOrigin::none;
    iq1 = 0;
    iq2 = inv.shared.nc1 - 1;
  }

  // xmdes.f 71-72: EIWSET(NC1) only for LEIW=.FALSE. (cache contract:
  // NC1 is INIT-constant 257; no cache due to same point count).
  if (validity.grid == CircleGridValidity::notInitialized) {
    auto grid = core::inverse::setCircleGrid(inv.shared.nc1);
    if (grid.isErr()) {
      // Fortran STOP 'EIWSET: array overflow' (xmdes.f 1621) is here
      // handled in a controlled way without any partial mutation.
      result.status = MdesNumericsInitStatus::capacityError;
      result.message = "EIWSET: array overflow. Increase ICX.";
      return result;
    }
    fs.circle.grid = std::move(grid.value());
    core::inverse::initState(fs.circle);
    validity.grid = CircleGridValidity::initialized;
  }

  // xmdes.f 75-79: Qspec alpha never set -> current Alfa.
  if (nqsp == 0) {
    iacqsp = 1;
    nqsp = 1;
    qspecs.assign(static_cast<std::size_t>(nqsp),
                  core::inverse::QSpecDistribution{});
    qspecs[0].al = session.viscous.viscals.alfa;
  }
  if (static_cast<int>(qspecs.size()) != nqsp) {
    qspecs.resize(static_cast<std::size_t>(nqsp));
  }

  // xmdes.f 81-87: SCINIT only for LSCINI=.FALSE.; afterwards LSCINI=.TRUE.
  // and LQSPEC=.FALSE. (kernel: core::inverse::initMappingCoordinate).
  bool scinitRan = false;
  if (forceMapping ||
      validity.mapping != MappingCoordinateValidity::initializedFromPanel) {
    auto st = core::inverse::initMappingCoordinate(
        fs.circle, fs.panel.x, fs.panel.xp, fs.panel.y, fs.panel.yp,
        fs.panel.s, fs.panel.sle, &result.scinit);
    if (st.isErr()) {
      result.status = MdesNumericsInitStatus::coreError;
      result.message = "SCINIT: core error for the mapping initialization";
      return result;
    }
    validity.mapping = MappingCoordinateValidity::initializedFromPanel;
    validity.qspec = QSpecValidity::absent;
    validity.qspecOrigin = QSpecOrigin::none;
    scinitRan = true;
    result.scinitRan = true;
  }

  // xmdes.f 90-91: MAPGAM(1,ALGAM,CLGAM,CMGAM) always (kernel mapGam;
  // sets QGAMM/SSPEC/XSPOC/YSPOC/SSPLE/NSP in the candidates).
  const double alfa = session.viscous.viscals.alfa;
  double clgam = inv.shared.clgam;
  double cmgam = inv.shared.cmgam;
  const auto mg = core::inverse::mapGam(fs, 1, alfa, clgam, cmgam);
  if (mg.isErr()) {
    result.status = MdesNumericsInitStatus::coreError;
    result.message = "MAPGAM: core error for the Q mapping";
    return result;
  }
  clgam = mg.value().cl;
  cmgam = mg.value().cm;

  // xmdes.f 93-97: CNCALC(QGAMM)+QSPCIR only for LQSPEC=.FALSE.
  // (kernels calcCoefficients/qSpecCirc; QSPCIR runs over all
  // NQSP columns, KQSP=1..NQSP).
  if (validity.qspec != QSpecValidity::present) {
    auto st = core::inverse::calcCoefficients(fs.circle, fs.qgamm, false);
    if (st.isErr()) {
      result.status = MdesNumericsInitStatus::coreError;
      result.message = "CNCALC: core error for the coefficient computation";
      return result;
    }
    for (int k = 0; k < nqsp; ++k) {
      st = core::inverse::qSpecCirc(fs, qspecs[static_cast<std::size_t>(k)],
                                    iacqsp);
      if (st.isErr()) {
        result.status = MdesNumericsInitStatus::coreError;
        result.message = "QSPCIR: core error for the target-Q generation";
        return result;
      }
    }
    validity.qspec = QSpecValidity::present;
    validity.qspecOrigin = QSpecOrigin::currentQ;
    result.qspecRebuilt = true;
  }

  // Success: atomic commit of the candidates.
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qgamm = std::move(fs.qgamm);
  inv.shared.sspec = std::move(fs.sspec);
  inv.shared.xspoc = std::move(fs.xspoc);
  inv.shared.yspoc = std::move(fs.yspoc);
  inv.shared.ssple = fs.ssple;
  inv.shared.nsp = fs.nsp;
  inv.shared.algam = alfa;
  inv.shared.clgam = clgam;
  inv.shared.cmgam = cmgam;
  inv.shared.qspecs = std::move(qspecs);
  inv.shared.nqsp = nqsp;
  inv.shared.iacqsp = iacqsp;
  inv.shared.target.iq1 = iq1;
  inv.shared.target.iq2 = iq2;
  // LIQSET/KQTARG/QDOF/FFILT/menu/redo states remain unchanged
  // (Fortran INIT label 1 does not write these fields).
  inv.validity = validity;
  inv.provenance.sourcePanelRevision = session.paneling.panelRevision;
  inv.provenance.sourceBufferRevision = session.paneling.panelSourceRevision;
  inv.provenance.sourcePanelOrigin = session.paneling.origin;

  result.status = scinitRan ? MdesNumericsInitStatus::initialized
                            : MdesNumericsInitStatus::refreshed;
  return result;
}

namespace {

// CR09 operating-point scalars (XFOIL.INC 86-91/158: ALFA, MINF, REINF,
// MINF_CL, TKLAM, TKL_MSQ, CL, CM, CD, CDP, CDF, MVISC, AVISC).
// Fortran PANGEN (xfoil.f 2078-2089) and ABCOPY (xgdes.f 1019-1028)
// set in their flag blocks exclusively flags (LGAMU, LQINU,
// LWAKE, LIPAN, LBLINI, LVCONV, LSCINI, LQSPEC, ...), never CR09-
// values; a direct Fortran run (OPER Alfa -> PANE -> OPER Alfa with
// empty input) preserves Alfa losslessly (OFS line alfa unchanged).
// The session invalidates the BL-/cache-fields of the Viscal state for
// geometry/panel changes, but these operating scalars must stay preserved.
struct Cr09OperatingScalars {
  double alfa{0.0};
  double minf{0.0};
  double reinf{0.0};
  double minfCl{0.0};
  double tklam{0.0};
  double tkMsq{0.0};
  double cl{0.0};
  double cm{0.0};
  double cd{0.0};
  double cdp{0.0};
  double cdf{0.0};
  double clf{0.0};
  double avisc{0.0};
  double mvisc{0.0};
};

Cr09OperatingScalars captureCr09OperatingScalars(
    const core::boundary_layer::ViscalState& viscals) {
  Cr09OperatingScalars s;
  s.alfa = viscals.alfa;
  s.minf = viscals.minf;
  s.reinf = viscals.reinf;
  s.minfCl = viscals.minfCl;
  s.tklam = viscals.tklam;
  s.tkMsq = viscals.tkMsq;
  s.cl = viscals.cl;
  s.cm = viscals.cm;
  s.cd = viscals.cd;
  s.cdp = viscals.cdp;
  s.cdf = viscals.cdf;
  s.clf = viscals.clf;
  s.avisc = viscals.avisc;
  s.mvisc = viscals.mvisc;
  return s;
}

void resetViscalsPreservingCr09(SessionComposition& session) {
  const Cr09OperatingScalars s =
      captureCr09OperatingScalars(session.viscous.viscals);
  session.viscous.viscals = core::boundary_layer::ViscalState{};
  session.viscous.viscals.alfa = s.alfa;
  session.viscous.viscals.minf = s.minf;
  session.viscous.viscals.reinf = s.reinf;
  session.viscous.viscals.minfCl = s.minfCl;
  session.viscous.viscals.tklam = s.tklam;
  session.viscous.viscals.tkMsq = s.tkMsq;
  session.viscous.viscals.cl = s.cl;
  session.viscous.viscals.cm = s.cm;
  session.viscous.viscals.cd = s.cd;
  session.viscous.viscals.cdp = s.cdp;
  session.viscous.viscals.cdf = s.cdf;
  session.viscous.viscals.clf = s.clf;
  session.viscous.viscals.avisc = s.avisc;
  session.viscous.viscals.mvisc = s.mvisc;
}

}  // namespace

GeometrySetResult setGeometry(SessionComposition& session,
                              geometry::AirfoilGeometry geometry) {
  GeometrySetResult result;

  // Fortran LOAD buffer path: validation before each partial mutation.
  if (geometry.pointCount() < kMinimumGeometryPoints) {
    result.error = GeometrySetError::emptyGeometry;
    result.message = "Analysis geometry is empty or incomplete";
    return result;
  }

  if (geometry.pointCount() > limits::maxInputCoordinates) {
    result.error = GeometrySetError::tooManyPoints;
    result.message = "Geometry exceeds the point limit";
    return result;
  }

  const std::size_t count = geometry.pointCount();
  std::vector<double> bufferX(count, 0.0);
  std::vector<double> bufferY(count, 0.0);
  for (std::size_t index = 0; index < count; ++index) {
    bufferX[index] = geometry.coordinates()[index].x;
    bufferY[index] = geometry.coordinates()[index].y;
  }

  const core::geometry::AirfoilBuffer buffer(bufferX, bufferY,
                                             std::string(geometry.name()));

  // Fortran ABCOPY (xgdes.f): current panel contour directly from the
  // buffer contour (canonical without PANE).
  auto paneling = core::paneling::abcopy(buffer);
  if (!paneling.isOk()) {
    result.error = GeometrySetError::panelInitializationFailed;
    result.message = paneling.error().message;
    return result;
  }

  // Only after successful kernel derivation is the visible session state
  // changed (no partial mutation on error).
  session.geometry.airfoil = std::move(geometry);
  session.geometry.name = session.geometry.airfoil.name();
  session.geometry.nameExplicitlySet = true;
  session.geometry.buffer = std::move(buffer);
  session.geometry.valid = true;
  ++session.geometry.bufferRevision;

  session.paneling.panel = std::move(paneling.value().panel);
  session.paneling.panelingValid = true;
  session.paneling.origin = PanelingOrigin::abcopy;
  session.paneling.panelSourceRevision = session.geometry.bufferRevision;
  // new panel contour (ABCOPY path of the API geometry adoption)
  // raises the contour revision and invalidates the inverse binding like
  // ABCOPY (xgdes.f 1027: LSCINI=.FALSE.).
  ++session.paneling.panelRevision;
  invalidateInverseStateAfterPanelChange(
      session, InversePanelChange::bufferToCurrent);

  // Fortran invalidation: buffer change invalidates panel, influence,
  // BL and result state (canonical invalidation rules). The CR09
  // operating scalars (Alfa/MINF/REINF/...) remain as in Fortran
  // preserved (xgdes.f 1019-1028 sets only flags).
  session.viscous.system = core::inviscid::InfluenceSystem{};
  session.viscous.systemValid = false;
  resetViscalsPreservingCr09(session);
  session.viscous.converged = false;
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;

  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status = session.operating.configured
      ? AnalysisStatus::notAnalyzed
      : AnalysisStatus::missingConfiguration;

  return result;
}

ConfigValidationResult configure(SessionComposition& session,
                                 const AnalysisConfig& config) {
  const ConfigValidationResult validation = validate(config);
  if (!validation.success()) {
    return validation;
  }

  session.operating.configured = true;
  session.operating.machReynolds = core::inviscid::MachReynoldsSettings{
      static_cast<int>(config.machDependency),
      static_cast<int>(config.reynoldsDependency), config.machNumber,
      config.reynoldsNumber};
  session.operating.viscous = config.viscous;
  session.operating.maximumIterations = config.maximumIterations;
  session.operating.autoInitializeBoundaryLayer =
      config.autoInitializeBoundaryLayer;
  session.operating.viscousAcceleration = config.viscousAcceleration;
  session.operating.criticalReynoldsExponents = {config.transition.nCritical,
                                            config.transition.nCritical};
  session.operating.topTripX = config.transition.topTripX;
  session.operating.bottomTripX = config.transition.bottomTripX;
  session.config = config;  // existing API contract: xfoil::XFoil::config
  // Fortran QINF stays at the INIT value;
  // Mach/Re/variation type do not invalidate the influence system
  // (GGCALC uses no MINF), but do invalidate BL state and convergence per
  // the Fortran INIT/SETBL LBLINI semantics (reset per point).
  session.viscous.viscals.lblini = false;
  session.viscous.viscals.lvconv = false;
  session.viscous.converged = false;
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;

  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status = session.geometry.valid
      ? AnalysisStatus::notAnalyzed
      : AnalysisStatus::missingGeometry;

  return validation;
}

bool initializeAnalysis(SessionComposition& session) {
  // Fortran INIT: resets the BL initialization state and provides the
  // inviscid influence system. Returns false without geometry.
  if (!session.geometry.valid || !session.paneling.panelingValid) {
    return false;
  }
  session.viscous.viscals.lblini = false;
  session.viscous.viscals.mapping.valid = false;
  session.viscous.viscals.lvconv = false;
  session.viscous.converged = false;
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;

  if (!session.viscous.systemValid) {
    const std::vector<double> sig0(session.paneling.panel.x.size(), 0.0);
    auto system = core::inviscid::ggCalc(session.paneling.panel, sig0, 0.0,
                                         session.operating.qinf);
    if (!system.isOk()) {
      return false;
    }
    session.viscous.system = std::move(system.value());
    session.viscous.systemValid = true;
    session.viscous.systemQinf = session.operating.qinf;
  }
  return true;
}

 AnalysisResult runSpecAlpha(SessionComposition& session, Degrees alpha,
                             const CancellationToken* cancellation) {
   return runSpecifiedPoint(session, alpha, 0.0,
                            PointSpecificationPolicy::specifiedAlpha,
                            cancellation, PointExecutionPolicy::coldStart);
 }

 AnalysisResult runSpecCl(SessionComposition& session, double clSpec,
                          Degrees startAlpha,
                          const CancellationToken* cancellation) {
   return runSpecifiedPoint(session, startAlpha, clSpec,
                            PointSpecificationPolicy::operSequenceSpecifiedCl,
                            cancellation, PointExecutionPolicy::coldStart);
 }

// ---: OPER single-point contracts (ALFA/CLI/CL) ----------------------------
// Only the expressed canonical contract. All
 // three use the same factored point kernel with
 // PointExecutionPolicy::operSinglePointWarmStart (LIPAN/LBLINI preserved,
 // wake/LVCONV invalidated only at the 1e-5 threshold, VISCAL(ITMAX), no NSEQEX).

 // Fortran OPER Alfa (xoper.f 416-437): SPECAL, LALFA=true, for LVISC
 // VISCAL(ITMAX). The prescribed alpha is the target angle of attack.
 AnalysisResult runOperAlphaPoint(SessionComposition& session, Degrees alpha,
                                  const CancellationToken* cancellation) {
   return runSpecifiedPoint(session, alpha, 0.0,
                            PointSpecificationPolicy::specifiedAlpha,
                            cancellation,
                            PointExecutionPolicy::operSinglePointWarmStart);
 }

 // Fortran OPER CLI (xoper.f 475-507): SPECCL, LALFA=true, no SPECAL afterwards,
 // for LVISC VISCAL(ITMAX). Fortran sets Alfa=0.0 before SPECCL (Newton start);
 // the SPECCL result alpha is updated as the current session state.
 AnalysisResult runOperCliPoint(SessionComposition& session, double clSpec,
                                const CancellationToken* cancellation) {
   return runSpecifiedPoint(session, Degrees{0.0}, clSpec,
                            PointSpecificationPolicy::specifiedClFixedAlpha,
                            cancellation,
                            PointExecutionPolicy::operSinglePointWarmStart);
 }

 // Fortran OPER Cl (xoper.f 510-542): SPECCL, LALFA=false, no SPECAL afterwards,
 // for LVISC VISCAL(ITMAX). Fortran sets Alfa=0.0 before SPECCL (Newton start);
 // the SPECCL result alpha is updated as the current session state.
 AnalysisResult runOperClPoint(SessionComposition& session, double clSpec,
                               const CancellationToken* cancellation) {
   return runSpecifiedPoint(session, Degrees{0.0}, clSpec,
                            PointSpecificationPolicy::specifiedClCoupledViscous,
                            cancellation,
                            PointExecutionPolicy::operSinglePointWarmStart);
 }

std::vector<AnalysisResult> runAlphaSequence(SessionComposition& session,
                                             Degrees start,
                                             Degrees endExclusive, Degrees step,
                                             const CancellationToken*
                                                 cancellation) {
  std::vector<AnalysisResult> results;

  const auto makeRangeError = [](const char* message) {
    AnalysisResult error;
    error.status = AnalysisStatus::invalidConfiguration;
    error.successful = false;
    error.diagnostic = message;
    return error;
  };

  const bool validSequence =
      std::isfinite(start.value) && std::isfinite(endExclusive.value) &&
      std::isfinite(step.value) && step.value != 0.0 &&
      ((step.value > 0.0 && start.value < endExclusive.value) ||
       (step.value < 0.0 && start.value > endExclusive.value));

  if (!validSequence) {
    session.result.lastResult = makeRangeError("Invalid polar range");
    results.push_back(session.result.lastResult);
    return results;
  }

  const bool ascending = step.value > 0.0;

  for (double alpha = start.value;
       ascending ? (alpha < endExclusive.value) : (alpha > endExclusive.value);) {
    if (results.size() >= kMaximumPolarPoints) {
      session.result.lastResult = makeRangeError("Polar point limit exceeded");
      session.result.lastResult.alpha = Degrees{alpha};
      results.push_back(session.result.lastResult);
      break;
    }

    if (cancellation != nullptr && cancellation->isCancellationRequested()) {
      AnalysisResult cancelled;
      cancelled.alpha = Degrees{alpha};
      cancelled.status = AnalysisStatus::cancelled;
      cancelled.successful = false;
      session.result.lastResult = cancelled;
      results.push_back(session.result.lastResult);
      break;
    }

    results.push_back(
        runSpecifiedPoint(session, Degrees{alpha}, 0.0,
                          PointSpecificationPolicy::specifiedAlpha, cancellation,
                          PointExecutionPolicy::coldStart));

    if (results.back().status == AnalysisStatus::cancelled) {
      break;
    }

    const double next = alpha + step.value;
    if (next == alpha) {
      session.result.lastResult = makeRangeError("Polar sequence stalled");
      session.result.lastResult.alpha = Degrees{alpha};
      results.push_back(session.result.lastResult);
      break;
    }
    alpha = next;
  }

  return results;
}

void resetBoundaryLayer(SessionComposition& session) {
  // Fortran INIT (xoper.f): LBLINI=false, LIPAN=false, LVCONV=false.
  // Mapping and convergence are invalidated; geometry, panel and
  // operating state remain unchanged.
  session.viscous.viscals.lblini = false;
  session.viscous.viscals.mapping.valid = false;
  session.viscous.viscals.lvconv = false;
  session.viscous.converged = false;
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;
}

// ---: geometry/panel producers --------------------------------------------

namespace {

// Complete analysis invalidation after geometry/panel change
// ("buffer changed" / "ABCOPY or PANGEN"):
// Influence matrix, BL state, convergence and result.
void invalidateGeometryDependents(SessionComposition& session) {
  session.viscous.system = core::inviscid::InfluenceSystem{};
  session.viscous.systemValid = false;
  // CR09 operating-point scalars stay preserved (xfoil.f 2078-2089 sets
  // only flags); only BL-/cache-fields are reset.
  resetViscalsPreservingCr09(session);
  session.viscous.converged = false;
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;
  // Fortran does not delete the FCPMIN COMMON snapshot (CPMNI/XCPMNI/CPMNV/
  // XCPMNV) on geometry/panel change (XFOIL.INC:92, only the
  // write site FCPMIN xoper.f 1768-1801). CPMN keeps the value.
  // Fortran PANGEN (xfoil.f:2080) and ABCOPY (xgdes.f:1021) also set
  // LWAKE = .FALSE. in the same flag block as LGAMU/LVCONV; the
  // wake-cache validity depends on the panel contour (XYWAKE reads
  // X/Y/GAM), so it must be invalidated for every geometry/panel change.
  session.viscous.wakeValid = false;
  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status = session.operating.configured
      ? AnalysisStatus::notAnalyzed
      : AnalysisStatus::missingConfiguration;
}


// ABCOPY-specific invalidation (xgdes.f:1019-1028 plus 975). Fortran
// sets the ABCOPY flag block LGAMU/LQINU/LWAKE/LQAIJ/LADIJ/LWDIJ/LIPAN/
// LVCONV/LSCINI back, but does not delete the BL station fields and
// sets LBLINI only if the panel point count itself changes (xgdes.f:975
// IF(N.NE.NB) LBLINI=.FALSE., evaluated before N=NB). pointCountChanged
// denotes exactly this canonical fact (old panel point count unequal to
// buffer point count); for the same point count LBLINI (warm start) and
// the station-field content are preserved, because the core Viscal solution
// resizes the station fields to fit the panel count (no OOB).
void invalidateAfterAbcopy(SessionComposition& session,
                           bool pointCountChanged) {
  // LGAMU/LQINU/LQAIJ: the inviscid influence system is panel dependent.
  session.viscous.system = core::inviscid::InfluenceSystem{};
  session.viscous.systemValid = false;
 // LADIJ: airfoil DIJ block to rebuild (dij.empty == .NOT.LADIJ).
  session.viscous.viscals.dij.clear();
  session.viscous.viscals.dijStride = 0;
  // LWDIJ: wake-dependent DIJ block.
  session.viscous.viscals.dijWakeValid = false;
  // LIPAN: BL mapping to rebuild.
  session.viscous.viscals.mapping.valid = false;
  // LVCONV: convergence status.
  session.viscous.viscals.lvconv = false;
  session.viscous.converged = false;
  // LWAKE: wake cache validity.
  session.viscous.wakeValid = false;
  // LBLINI: reset only on point-count change; otherwise warm start
  // preserved. Station fields stay preserved in both cases.
  if (pointCountChanged) {
    session.viscous.viscals.lblini = false;
  }
  // LRECALC continuation is not permitted after a panel change.
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;
  // ABCOPY/PCOP/GDES-EXEC replace the current panel contour:
  // raise the contour revision and mark the inverse binding outdated
  // after xgdes.f 1027 (LSCINI=.FALSE.); LQSPEC/target segment/menu/redo
  // remain unchanged.
  ++session.paneling.panelRevision;
  invalidateInverseStateAfterPanelChange(
      session, InversePanelChange::bufferToCurrent);
  // last analysis result.
  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status = session.operating.configured
      ? AnalysisStatus::notAnalyzed
      : AnalysisStatus::missingConfiguration;
}

// Fortran xoper.f VISC/Re/Mach/TYPE: the commands explicitly set only
// LVCONV=.FALSE. (xoper.f 215, 296, 333, 376). LBLINI, LIPAN, LWAKE, LWDIJ,
// wake geometry, influence matrix, BL station fields, AVISC/MVISC and the
// last analysis result stay preserved unless another
// canonical path explicitly changes them.
void invalidateViscousConvergenceOnly(SessionComposition& session) {
  session.viscous.viscals.lvconv = false;
  session.viscous.converged = false;
}

// Fortran MRCL(1.0, MINF_Cl, REINF_Cl) in xoper.f Re/Mach/TYPE and INIT.
// The session separates base inputs (operating.machReynolds) and derived
// values (viscous.viscals.minf/reinf/minfCl). No second numerical
// implementation: the confirmed core kernel MRCL is used.
void applyMrclAtUnitCl(SessionComposition& session) {
  const core::inviscid::MachReynoldsSettings settings{
      session.operating.machReynolds.machType,
      session.operating.machReynolds.reType,
      session.operating.machReynolds.minf1,
      session.operating.machReynolds.reinf1};
  const auto result = core::inviscid::mrcl(settings, 1.0);
  if (result.isOk()) {
    session.viscous.viscals.minf = result.value().minf;
    session.viscous.viscals.reinf = result.value().reinf;
    session.viscous.viscals.minfCl = result.value().mCls;
  }
}

// Fortran COMSET after MRCL for Mach/TYPE (xoper.f 323, 375; xfoil.f 1029).
// The derived compressibility parameters are stored per instance in the
// existing Viscal state; CPSTAR/QSTAR remain text quantities of the
// CLI layer and are not derived as a second numerical source.
void applyComsetFromCurrentMinf(SessionComposition& session) {
  const core::inviscid::CompressibilityState comp = core::inviscid::comset(
      session.viscous.viscals.minf, session.operating.qinf);
  session.viscous.viscals.tklam = comp.tklam;
  session.viscous.viscals.tkMsq = comp.tkMsq;
}

// Metrics from the confirmed core kernel geopar on the current panel contour.
std::optional<core::geometry::GeometryMetrics> metricsFromPanel(
    const SessionComposition& session) {
  if (!session.paneling.panelingValid) {
    return std::nullopt;
  }
  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  return core::geometry::geopar(panel.x, panel.xp, panel.y, panel.yp, panel.s);
}

}  // namespace

GeometrySetResult setBufferGeometry(SessionComposition& session,
                                    core::geometry::AirfoilBuffer buffer) {
  GeometrySetResult result;
  if (buffer.x.empty() || buffer.x.size() != buffer.y.size()) {
    result.error = GeometrySetError::emptyGeometry;
    result.message = "Buffer geometry is empty or incomplete";
    return result;
  }
  if (buffer.x.size() > limits::maxInputCoordinates) {
    result.error = GeometrySetError::tooManyPoints;
    result.message = "Buffer exceeds the point limit";
    return result;
  }

  // Buffer adoption without ABCOPY/PANGEN: panel is invalidated, the
  // buffer revision increased; panel parameters stay preserved. The airfoil name
  // canonically carries the CHARACTER*48 truncation and STRIP trimming.
  buffer.name = canonicalFortranName(buffer.name);
  session.geometry.buffer = std::move(buffer);
  session.geometry.name = session.geometry.buffer.name;
  session.geometry.nameExplicitlySet = true;
  session.geometry.valid = true;
  ++session.geometry.bufferRevision;
  session.geometry.metrics = std::nullopt;

  session.paneling.panel = core::paneling::PanelGeometry{};
  session.paneling.panelingValid = false;
  session.paneling.origin = PanelingOrigin::none;
  session.paneling.panelSourceRevision = 0;
  // Invalidating the current contour counts as a
  // contour revision change; the inverse binding is outdated (LQSPEC
  // flag semantics stay; Fortran LOAD changes no inverse flags).
  ++session.paneling.panelRevision;
  invalidateInverseStateAfterPanelChange(
      session, InversePanelChange::panelInvalidated);

  invalidateGeometryDependents(session);
  return result;
}

// ---: INTX-/HALF buffer commit --------------------------------------------
//
// Canonical contract: INTX (xfoil.f 2566-2580) and TOP-HALF
// (xfoil.f 242-252) write only XB/YB/NB and lead SB/XBP/YBP and
// recompute the buffer GEOPAR values. panel contour (X/XP/Y/YP/S/N),
// validity, analysis, wake, BL, mapping, CPI/CPV, polar, flap and
// inverse states remain unchanged; no ABCOPY/PANGEN occurs.
namespace {

// Fortran INTX source preprocessing (xfoil.f 2552-2557): SCALC,
// SEGSPLD(-999,-999) for x and y, LEFIND.
core::geometry::SplineProfile makeIntxSourceProfile(std::vector<double> x,
                                                    std::vector<double> y) {
  core::geometry::SplineProfile p;
  p.x = std::move(x);
  p.y = std::move(y);
  p.s.resize(p.x.size());
  p.xp.resize(p.x.size());
  p.yp.resize(p.x.size());
  core::math::scalc(p.x, p.y, p.s);
  core::math::segsplD(p.x, p.s, p.xp, -999.0, -999.0);
  core::math::segsplD(p.y, p.s, p.yp, -999.0, -999.0);
  p.sle = core::geometry::lefind(p.x, p.xp, p.y, p.yp, p.s).sle;
  return p;
}

// Buffer commit without panel/analysis invalidation: only buffer coordinates,
// buffer revision and (if active) the derived quantities of the
// GDES work buffer are updated. Fortran keeps SB/XBP/YBP itself
// between TOP buffer and GDES work buffer; an active work buffer
// stays otherwise consistent because it is refreshed as in Fortran.
void commitBufferOnlyGeometry(SessionComposition& session,
                              std::vector<double> x, std::vector<double> y) {
  session.geometry.buffer.x = std::move(x);
  session.geometry.buffer.y = std::move(y);
  session.geometry.valid = true;
  ++session.geometry.bufferRevision;
  if (session.design.work.has_value()) {
    DesignBufferWorkState& work = *session.design.work;
    work.s.resize(session.geometry.buffer.x.size());
    work.xp.resize(session.geometry.buffer.x.size());
    work.yp.resize(session.geometry.buffer.x.size());
    core::math::scalc(session.geometry.buffer.x, session.geometry.buffer.y,
                      work.s);
    core::math::segspl(session.geometry.buffer.x, work.s, work.xp);
    core::math::segspl(session.geometry.buffer.y, work.s, work.yp);
    work.metrics = core::geometry::geopar(session.geometry.buffer.x, work.xp,
                                          session.geometry.buffer.y, work.yp,
                                          work.s);
  }
}

// SCALC/SEGSPL/GEOPAR on the new buffer contour (Fortran order
// xfoil.f 2572-2580 or 244-252); the kernel does not write to stdout.
core::geometry::GeometryMetrics refreshBufferDerived(
    const std::vector<double>& x, const std::vector<double>& y,
    std::vector<double>& s, std::vector<double>& xp, std::vector<double>& yp) {
  s.resize(x.size());
  xp.resize(x.size());
  yp.resize(x.size());
  core::math::scalc(x, y, s);
  core::math::segspl(x, s, xp);
  core::math::segspl(y, s, yp);
  return core::geometry::geopar(x, xp, y, yp, s);
}

}  // namespace

BufferOnlyGeometryResult applyIntxBufferInterpolation(
    SessionComposition& session,
    std::vector<double> x0, std::vector<double> y0,
    std::vector<double> x1, std::vector<double> y1, double frac) {
  BufferOnlyGeometryResult result;
  if (x0.size() != y0.size() || x1.size() != y1.size()) {
    return result;
  }
  // LEFIND requires N >= 3; interX requires N >= 2 (checkProfile).
  if (x0.size() < 3 || x1.size() < 3) {
    return result;
  }
  // Result N = N0; canonical buffer capacity IBX.
  if (x0.size() > limits::maxInputCoordinates) {
    return result;
  }
  try {
    const core::geometry::SplineProfile p0 =
        makeIntxSourceProfile(std::move(x0), std::move(y0));
    const core::geometry::SplineProfile p1 =
        makeIntxSourceProfile(std::move(x1), std::move(y1));
    const core::geometry::AirfoilCoordinates interpolated =
        core::geometry::interX(p0, p1, frac);
    std::vector<double> s;
    std::vector<double> xp;
    std::vector<double> yp;
    result.bufferMetrics = refreshBufferDerived(interpolated.x,
                                                interpolated.y, s, xp, yp);
    commitBufferOnlyGeometry(session, interpolated.x, interpolated.y);
    result.ok = true;
    result.pointCount = interpolated.x.size();
  } catch (const std::exception&) {
    // core precondition (e.g. degenerate contour): no partial mutation.
    return BufferOnlyGeometryResult{};
  }
  return result;
}

BufferOnlyGeometryResult applyHalfBufferDecimation(
    SessionComposition& session) {
  BufferOnlyGeometryResult result;
  if (!session.geometry.valid || session.geometry.buffer.x.size() < 2 ||
      session.geometry.buffer.x.size() != session.geometry.buffer.y.size()) {
    return result;
  }
  std::vector<double> x = session.geometry.buffer.x;
  std::vector<double> y = session.geometry.buffer.y;
  std::size_t n = x.size();
  try {
    std::vector<double> s(x.size());
    core::math::scalc(x, y, s);
    // Fortran HALF (xgeom.f 1755-1791): in-place, corner-preserving.
    core::geometry::half(s, x, y, n);
    x.resize(n);
    y.resize(n);
    std::vector<double> ds;
    std::vector<double> xp;
    std::vector<double> yp;
    result.bufferMetrics = refreshBufferDerived(x, y, ds, xp, yp);
    commitBufferOnlyGeometry(session, std::move(x), std::move(y));
    result.ok = true;
    result.pointCount = n;
  } catch (const std::exception&) {
    return BufferOnlyGeometryResult{};
  }
  return result;
}

SessionOperationStatus copyBufferToCurrent(SessionComposition& session) {
  SessionOperationStatus status;
  if (!session.geometry.valid) {
    status.ok = false;
    status.message = "ABCOPY: Buffer airfoil not available.";
    return status;
  }
  auto paneling = core::paneling::abcopy(session.geometry.buffer);
  if (!paneling.isOk()) {
    status.ok = false;
    status.message = paneling.error().message;
    return status;
  }
  // Fortran ABCOPY (xgdes.f:975) evaluates IF(N.NE.NB) before the assignment
  // N=NB (977) and before the coordinate copy: N is the previous
  // current panel point count, NB the (unchanged) buffer point count. A
  // genuine buffer replacement (setBufferGeometry) empties the panel contour
  // beforehand (validity false, point count 0), so N != NB holds and
  // LBLINI is correctly reset by the new geometry rather than wrongly
  // staying preserved. ABCOPY on an intact panel contour with the same point
  // count, by contrast, preserves the canonical warm start.
  const std::size_t oldPanelCount =
      session.paneling.panelingValid ? session.paneling.panel.x.size() : 0;
  const bool pointCountChanged =
      (oldPanelCount != session.geometry.buffer.x.size());
  session.paneling.panel = std::move(paneling.value().panel);
  session.paneling.panelingValid = true;
  session.paneling.origin = PanelingOrigin::abcopy;
  session.paneling.panelSourceRevision = session.geometry.bufferRevision;
  session.geometry.metrics = metricsFromPanel(session);
  invalidateAfterAbcopy(session, pointCountChanged);
  return status;
}

ExecuteGeometryDesignResult executeGeometryDesignBuffer(
    SessionComposition& session) {
  ExecuteGeometryDesignResult result;
  const std::size_t nb =
      session.geometry.valid ? session.geometry.buffer.x.size() : 0;
  result.bufferPointCount = nb;
  result.maxPointCount =
      static_cast<std::size_t>(core::paneling::kMaxAbcopyNodeCount);
  // xgdes.f 965-967: NB <= 1 => 'ABCOPY: Buffer airfoil not available.'
  if (!session.geometry.valid || nb <= 1) {
    result.bufferUnavailable = true;
    return result;
  }
  // xgdes.f 968-972: NB > IQX-5 => four-line capacity message,
  // no mutation.
  if (nb > result.maxPointCount) {
    result.capacityExceeded = true;
    return result;
  }
  const SessionOperationStatus status = copyBufferToCurrent(session);
  if (!status.ok) {
    // Defensive: copyBufferToCurrent reports the same preconditions.
    result.capacityExceeded = true;
    return result;
  }
  // xgdes.f 982: LGSAME = .TRUE.; 984-988: LBFLAP/XBF/YBF ->
  // LFLAP/XOF/YOF.
  session.design.lgSame = true;
  if (session.design.flap.lbFlap) {
    session.design.flap.xof = session.design.flap.xbf;
    session.design.flap.yof = session.design.flap.ybf;
    session.design.flap.lFlap = true;
  }
  result.ok = true;
  return result;
}

PanelingApplyResult applyPaneling(SessionComposition& session) {
  PanelingApplyResult result;
  if (!session.geometry.valid) {
    result.ok = false;
    result.message = "PANGEN: buffer airfoil not available.";
    return result;
  }
  auto pangen =
      core::paneling::pangen(session.geometry.buffer, session.paneling.parameters);
  if (!pangen.isOk()) {
    result.ok = false;
    result.message = pangen.error().message;
    return result;
  }
  result.pangen = std::move(pangen.value());
  session.paneling.panel = std::move(result.pangen.panel);
  session.paneling.panelingValid = true;
  session.paneling.origin = PanelingOrigin::pangen;
  session.paneling.panelSourceRevision = session.geometry.bufferRevision;
  session.geometry.metrics = metricsFromPanel(session);
  // PANGEN (PANE/PPAR apply/NACA) replaced the panel contour:
  // raise the contour revision; inverse invalidation after xfoil.f
  // 2087-2088 (LSCINI=.FALSE. and LQSPEC=.FALSE.).
  ++session.paneling.panelRevision;
  invalidateInverseStateAfterPanelChange(
      session, InversePanelChange::panelingGenerated);
  invalidateGeometryDependents(session);
  result.ok = true;
  return result;
}

SessionOperationStatus setPanelingParameters(
    SessionComposition& session,
    const core::paneling::PanelingParameters& parameters) {
  SessionOperationStatus status;
  if (parameters.panelCount < 2) {
    status.ok = false;
    status.message = "Number of panel nodes must be at least 2";
    return status;
  }
  // Canonical PPAR semantics: parameters are stored, the existing
  // panel contour is not yet regenerated (effect only for PANGEN/apply).
  session.paneling.parameters = parameters;
  return status;
}

SessionOperationStatus setPanelParameter(
    SessionComposition& session, core::paneling::PanelParameterKind kind,
    std::span<const double> values) {
  SessionOperationStatus status;
  auto applied = core::paneling::applyPanelParameter(
      session.paneling.parameters, kind, values);
  if (!applied.isOk()) {
    status.ok = false;
    status.message = applied.error().message;
    return status;
  }
  session.paneling.parameters = applied.value().parameters;
  return status;
}

const core::geometry::AirfoilBuffer& currentBufferGeometry(
    const SessionComposition& session) {
  return session.geometry.buffer;
}

const core::paneling::PanelGeometry& currentPanelGeometry(
    const SessionComposition& session) {
  return session.paneling.panel;
}

const core::paneling::PanelingParameters& panelingParameters(
    const SessionComposition& session) {
  return session.paneling.parameters;
}

PanelingOrigin panelingOrigin(const SessionComposition& session) {
  return session.paneling.origin;
}

bool panelingValid(const SessionComposition& session) {
  return session.paneling.panelingValid;
}

std::optional<core::geometry::GeometryMetrics> geometryMetrics(
    const SessionComposition& session) {
  if (session.geometry.metrics.has_value()) {
    return session.geometry.metrics;
  }
  return metricsFromPanel(session);
}

void setGeometryName(SessionComposition& session, const std::string& name) {
  const std::string canonical = canonicalFortranName(name);
  session.geometry.name = canonical;
  session.geometry.buffer.name = canonical;
  session.geometry.airfoil.setName(canonical);
  session.geometry.nameExplicitlySet = true;
}

std::string geometryNameHeader(const SessionComposition& session) {
  if (session.geometry.name.empty() && !session.geometry.nameExplicitlySet) {
    // Fortran INIT: Name = 32 spaces, NNAME = 32 (xfoil.f:767-768).
    return std::string(32, ' ');
  }
  return session.geometry.name;
}

std::string geometryNameVersionDefault(const SessionComposition& session,
                                       int kDel, int kMod0) {
  const core::Result<std::string> tagged =
      core::polar::applyVersionTag(session.geometry.name, kDel, kMod0);
  if (tagged.isOk()) {
    return truncateFortranNameField(tagged.value());
  }
  // Fortran NAMMOD jumps to the prompt on a READ error of the bracket content and
  // reuses NAMDEF = Name(1:NNAME); the session keeps this default.
  return truncateFortranNameField(session.geometry.name);
}

void incrementGeometryName(SessionComposition& session, int kDel, int kMod0) {
  setGeometryName(session, geometryNameVersionDefault(session, kDel, kMod0));
}

void setMomentReference(SessionComposition& session, double x, double y) {
  session.operating.momentReferenceX = x;
  session.operating.momentReferenceY = y;
  // XYCM invalidates only load-dependent results.
  session.result.lastResult = AnalysisResult{};
  session.result.lastResult.status =
      session.geometry.valid && session.paneling.panelingValid
          ? AnalysisStatus::notAnalyzed
          : AnalysisStatus::missingGeometry;
}

// ---: GDES-/CAMB design state machine
//
// Fortran GDES entry (xgdes.f 43-51): NB==0 reports '*** No airfoil
// available ***' and leaves GDES without any state transition. For
// NB >= 1 the entry sets only LPLCAM=.FALSE.; LGSAME, LGSYM
// and all remaining COMMON states remain unchanged, and nothing
// is copied: the working geometry is and stays the buffer (CR14).
GeometryDesignInitResult initializeGeometryDesign(SessionComposition& session) {
  GeometryDesignInitResult result;
  if (!session.geometry.valid || session.geometry.buffer.x.empty()) {
    result.status = GeometryDesignInitStatus::noAirfoil;
    return result;  // no partial mutation
  }
  // Working buffer state built from the current TOP buffer
  // (canonical: SCALC/SEGSPL x2, GEOPAR; xgdes.f holds SB/XBP/YBP/
  // GEOPAR parameters as COMMON CR14).
  DesignBufferWorkState work;
  work.s.resize(session.geometry.buffer.x.size());
  work.xp.resize(session.geometry.buffer.x.size());
  work.yp.resize(session.geometry.buffer.x.size());
  core::math::scalc(session.geometry.buffer.x, session.geometry.buffer.y,
                    work.s);
  core::math::segspl(session.geometry.buffer.x, work.s, work.xp);
  core::math::segspl(session.geometry.buffer.y, work.s, work.yp);
  // Fortran holds SBLE..CAMBRB (COMMON CR14) after SETGEOM/NACA based
  // on the BUFFERS (245 points), not the panels. PANGEN does not change
  // these buffer metrics. The working state derives the
  // buffer metrics from the same buffer (SCALC/SEGSPL above), so that
  // SBLE-based paths (CAMB AROT, GEOPAR prints) continue the canonical
  // state update.
  work.metrics = session.geometry.buffer.x.size() >= 3
      ? core::geometry::geopar(session.geometry.buffer.x, work.xp,
                               session.geometry.buffer.y, work.yp, work.s)
      : core::geometry::GeometryMetrics{};
  session.design.work = std::move(work);
  session.design.menu.active = true;
  session.design.lpLcam = false;
  session.design.entryBufferRevision = session.geometry.bufferRevision;
  result.status = GeometryDesignInitStatus::initialized;
  return result;
}

// Fortran GDES exit via <cr> (xgdes.f 72-84): LPLOT=false (pure
// plot-frame state, not modeled here), LGSYM=false, LGEOPL=false
// (plot flag), warning for .NOT.LGSAME. Buffer/panel/analysis unchanged.
DesignExitResult leaveGeometryDesignWithoutCommit(
    SessionComposition& session) {
  DesignExitResult result;
  result.lgsame = session.design.lgSame;
  session.design.menu.active = false;
  session.design.lgSym = false;
  return result;
}

// CAMB entry/exit LGPARSAVE (xtcam.f 41, 184). The CAMB 100-block
// (CamberSession initialization) is separate; here only the flag save.
void savePlotParameterForCamberEntry(SessionComposition& session) {
  session.design.lgParmSave = session.design.lgParm;
}

void restorePlotParameterAfterCamberExit(SessionComposition& session) {
  session.design.lgParm = session.design.lgParmSave;
}

// ---: PLOP menu state (OPLSET, plutil.f 21-176) ---------------
//
// OPLSET 'G': IDEV.EQ.0 -> IDEV=1 otherwise IDEV=0 (plutil.f 84-89). The
// menu line shows LGRAPH = IDEV.GE.1.
void togglePlotGraphics(SessionComposition& session) {
  session.design.plot.idev = session.design.plot.idev == 0 ? 1 : 0;
}

void setPlotSize(SessionComposition& session, double size) {
  session.defaults.values.size = size;
}

void setPlotAspect(SessionComposition& session, double aspect) {
  session.defaults.values.plotar = aspect;
}

void setPlotPage(SessionComposition& session, double xpage, double ypage) {
  session.defaults.values.xpage = xpage;
  session.defaults.values.ypage = ypage;
}

void setPlotMargins(SessionComposition& session, double xmarg, double ymarg) {
  session.defaults.values.xmarg = xmarg;
  session.defaults.values.ymarg = ymarg;
}

void setPlotFont(SessionComposition& session, double font) {
  session.defaults.values.ch = font;
}

void setPlotScreenFraction(SessionComposition& session, double scrnfr) {
  session.defaults.values.scrnfr = scrnfr;
}

void togglePlotBlowupInput(SessionComposition& session) {
  session.defaults.values.lcurs = !session.defaults.values.lcurs;
}

// OPLSET 'O' (plutil.f 138-145): LLAND clamp and XPAGE/YPAGE swap;
// the CLI emits the message 'Swapping X,Y page dimensions'.
void togglePlotOrientation(SessionComposition& session) {
  session.design.plot.lland = !session.design.plot.lland;
  const double xpage = session.defaults.values.xpage;
  session.defaults.values.xpage = session.defaults.values.ypage;
  session.defaults.values.ypage = xpage;
}

// OPLSET 'C' (plutil.f 147-150): LCOLOR clamp; IDEVRP = 4 or 2.
// LCOLOR is the derived display value IDEVRP.EQ.4 (WRTDEF 985).
void togglePlotColor(SessionComposition& session) {
  const bool lcolor = session.defaults.idevrp != 4;
  session.defaults.idevrp = lcolor ? 4 : 2;
  session.defaults.values.lcolor = lcolor;
}

// OPLSET 'L' (plutil.f 152-172): ICOLS with clamp MAX(1,MIN(10,...)).
void setPlotLineColors(SessionComposition& session, int top, int bottom) {
  session.design.plot.icols[0] = std::max(1, std::min(10, top));
  session.design.plot.icols[1] = std::max(1, std::min(10, bottom));
}

// ---: GDES working buffer use cases ---------------------------------------
//
// The working geometry stays the TOP buffer (GeometrySessionState.buffer);
// the derived spline quantities (SB/XBP/YBP) and GEOPAR parameters lie
// in design session state.work. Each successful command raises the
// buffer revision and leaves panel contour, panel provenance,
// panelSourceRevision, wake, mapping, BL, system cache and lastResult
// unchanged (canonical: activation only via EXEC/ABCOPY).
namespace {

core::geometry_design::GeometryDesignBuffer assembleDesignBuffer(
    const SessionComposition& session) {
  core::geometry_design::GeometryDesignBuffer wb;
  wb.x = session.geometry.buffer.x;
  wb.y = session.geometry.buffer.y;
  if (session.design.work.has_value()) {
    wb.s = session.design.work->s;
    wb.xp = session.design.work->xp;
    wb.yp = session.design.work->yp;
    wb.metrics = session.design.work->metrics;
  } else {
    wb.s.resize(wb.x.size());
    wb.xp.resize(wb.x.size());
    wb.yp.resize(wb.x.size());
    core::math::scalc(wb.x, wb.y, wb.s);
    core::math::segspl(wb.x, wb.s, wb.xp);
    core::math::segspl(wb.y, wb.s, wb.yp);
    wb.metrics =
        core::geometry::geopar(wb.x, wb.xp, wb.y, wb.yp, wb.s);
  }
  wb.lgSame = session.design.lgSame;
  wb.lgSym = session.design.lgSym;
  wb.xbf = session.design.flap.xbf;
  wb.ybf = session.design.flap.ybf;
  wb.lbFlap = session.design.flap.lbFlap;
  return wb;
}

// Copy back: coordinates into the TOP buffer, derived quantities into
// the working state, flags in the design state. No panel/
// analysis/wake/mapping/result invalidation, no ABCOPY/PANGEN.
void syncDesignBuffer(SessionComposition& session,
                      const core::geometry_design::GeometryDesignBuffer& wb,
                      bool geometryChanged) {
  session.geometry.buffer.x = wb.x;
  session.geometry.buffer.y = wb.y;
  session.geometry.valid = true;
  DesignBufferWorkState work;
  work.s = wb.s;
  work.xp = wb.xp;
  work.yp = wb.yp;
  work.metrics = wb.metrics;
  session.design.work = std::move(work);
  session.design.lgSame = wb.lgSame;
  session.design.lgSym = wb.lgSym;
  session.design.flap.xbf = wb.xbf;
  session.design.flap.ybf = wb.ybf;
  session.design.flap.lbFlap = wb.lbFlap;
  if (geometryChanged) {
    ++session.geometry.bufferRevision;
  }
}

GeometryDesignOpResult designOpFailure(const char* message) {
  GeometryDesignOpResult result;
  result.message = message;
  return result;
}

bool designWorkAvailable(const SessionComposition& session) {
  return session.design.work.has_value() && session.geometry.valid &&
         !session.geometry.buffer.x.empty();
}

}  // namespace

GeometryDesignGsetResult gdesSetBufferFromCurrent(
    SessionComposition& session) {
  GeometryDesignGsetResult result;
  if (!designWorkAvailable(session)) {
    result.message = "GDES: no design buffer active";
    return result;
  }
  if (!session.paneling.panelingValid || session.paneling.panel.x.empty()) {
    result.message = "GSET: no current airfoil available";
    return result;
  }
  auto wb = assembleDesignBuffer(session);
  const auto status = core::geometry_design::setBufferFromAirfoil(
      wb, session.paneling.panel.x, session.paneling.panel.y);
  if (!status.isOk()) {
    result.message = status.error().message;
    return result;
  }
  result.ok = true;
  result.metricsAfterGeopar = status.value().metricsAfterGeopar;
  result.zercam = std::move(status.value().zercam);
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

GeometryDesignSymmetryResult gdesToggleSymmetry(
    SessionComposition& session) {
  GeometryDesignSymmetryResult result;
  if (!designWorkAvailable(session)) {
    result.lgSym = session.design.lgSym;
    return result;
  }
  auto wb = assembleDesignBuffer(session);
  const bool enabled = !wb.lgSym;
  const auto status = core::geometry_design::setSymmetry(wb, enabled);
  if (!status.isOk()) {
    result.lgSym = session.design.lgSym;
    return result;
  }
  result.ok = true;
  result.zercam = std::move(status.value());
  // ZERCAM changes the geometry only when enabled.
  syncDesignBuffer(session, wb, /*geometryChanged=*/enabled);
  result.lgSym = wb.lgSym;
  return result;
}

GeometryDesignOpResult gdesRotate(SessionComposition& session, double arad) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  core::geometry_design::rotateBuffer(wb, arad);
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

GeometryDesignOpResult gdesTranslate(SessionComposition& session, double delx,
                                     double dely) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  core::geometry_design::translateBuffer(wb, delx, dely);
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

GeometryDesignOpResult gdesScale(SessionComposition& session, double xxfac,
                                 double yyfac) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  core::geometry_design::scaleBuffer(wb, xxfac, yyfac);
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

GeometryDesignOpResult gdesLinearYScale(SessionComposition& session,
                                        double xoc1, double yfac1, double xoc2,
                                        double yfac2) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  const core::Status status = core::geometry_design::linearYScale(
      wb, xoc1, yfac1, xoc2, yfac2);
  if (!status.isOk()) {
    return designOpFailure(status.error().message.c_str());
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

core::Result<double> gdesDerotate(SessionComposition& session) {
  if (!designWorkAvailable(session)) {
    return core::Result<double>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result = core::geometry_design::derotateBuffer(wb);
  if (!result.isOk()) {
    return result;
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

GeometryDesignOpResult gdesNormalize(SessionComposition& session) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  const core::Status status = core::geometry_design::normalizeBuffer(wb);
  if (!status.isOk()) {
    return designOpFailure(status.error().message.c_str());
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

GeometryDesignOpResult gdesMoveNormals(SessionComposition& session,
                                       double deln) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  const core::Status status = core::geometry_design::moveSurfaceNormals(wb, deln);
  if (!status.isOk()) {
    return designOpFailure(status.error().message.c_str());
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

GeometryDesignOpResult gdesSetTrailingEdgeGap(SessionComposition& session,
                                              double gapnew, double doc) {
  if (!designWorkAvailable(session)) {
    return designOpFailure("GDES: no design buffer active");
  }
  auto wb = assembleDesignBuffer(session);
  const core::Status status =
      core::geometry_design::setTrailingEdgeGap(wb, gapnew, doc);
  if (!status.isOk()) {
    return designOpFailure(status.error().message.c_str());
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return GeometryDesignOpResult{true, ""};
}

core::Result<double> gdesScaleLeadingEdgeRadius(SessionComposition& session,
                                                double rfac, double doc) {
  if (!designWorkAvailable(session)) {
    return core::Result<double>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result =
      core::geometry_design::scaleLeadingEdgeRadius(wb, rfac, doc);
  if (!result.isOk()) {
    return result;
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

core::Result<core::geometry_design::ScaleResult> gdesScaleThicknessCamber(
    SessionComposition& session, std::optional<double> tfac,
    std::optional<double> cfac) {
  if (!designWorkAvailable(session)) {
    return core::Result<core::geometry_design::ScaleResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result =
      core::geometry_design::scaleThicknessCamber(wb, tfac, cfac);
  if (!result.isOk()) {
    return result;
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

core::Result<core::geometry_design::SetTargetsResult>
    gdesSetThicknessCamberTargets(SessionComposition& session,
                                  std::optional<double> tnew,
                                  std::optional<double> cnew,
                                  bool applyQuestionable) {
  if (!designWorkAvailable(session)) {
    return core::Result<core::geometry_design::SetTargetsResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result = core::geometry_design::setThicknessCamberTargets(
      wb, tnew, cnew, applyQuestionable);
  if (!result.isOk()) {
    return result;
  }
  // 'Questionable input' -> 'No action taken': no buffer change,
  // no revision change.
  const bool mutated = !result.value().abortedQuestionable;
  if (mutated) {
    syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  }
  return result;
}

core::geometry_design::HighPointResult gdesMoveHighPoints(
    SessionComposition& session, double thpnt, double chpnt) {
  if (!designWorkAvailable(session)) {
    return core::geometry_design::HighPointResult{};
  }
  auto wb = assembleDesignBuffer(session);
  auto result = core::geometry_design::moveHighPoints(wb, thpnt, chpnt);
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

// ---: CAMB sub-menu use cases (xtcam.f) -------------

CamberEntryResult initializeCamberDesign(SessionComposition& session) {
  CamberEntryResult result;
  if (!designWorkAvailable(session)) {
    result.noBuffer = true;
    return result;  // no partial mutation
  }
  // xgdes.f 488-490: LGSYM deactivation with message.
  if (session.design.lgSym) {
    session.design.lgSym = false;
    result.symmetryDisabled = true;
  }
  // xtcam.f 41: LGPARSAVE = LGPARM.
  session.design.lgParmSave = session.design.lgParm;
  // xtcam.f 53-57: LPLCAM enable message (GOFINI is a plot frame, here
  // not modeled).
  if (!session.design.lpLcam) {
    session.design.lpLcam = true;
    result.enablingCamberPlot = true;
  }
  // xtcam.f 61-71: chord-axis angle AROT from SBLE/SEVAL and
  // XB(1)/XB(NB); warning for |AROT| > 1 deg.
  const auto& w = *session.design.work;
  const auto& b = session.geometry.buffer;
  const double xle = core::math::seval(w.metrics.sle, b.x, w.xp, w.s);
  const double yle = core::math::seval(w.metrics.sle, b.y, w.yp, w.s);
  const double xte = 0.5 * (b.x.front() + b.x.back());
  const double yte = 0.5 * (b.y.front() + b.y.back());
  constexpr double kPi = 3.14159265358979323846;
  result.arot = std::atan2(yle - yte, xte - xle) / (kPi / 180.0);
  result.rotationWarning = std::fabs(result.arot) > 1.0;
  // xtcam.f 74-75: LDCPLOT = .FALSE., LGPARM = .NOT.LDCPLOT.
  session.design.ldCplot = false;
  session.design.lgParm = true;
  // xtcam.f 81-131 (LCLEAR = .TRUE.): CAMB 100-block.
  session.design.camber = core::geometry_design::initCamberSession(
      assembleDesignBuffer(session), session.operating.momentReferenceX);
  return result;
}

void leaveCamberDesign(SessionComposition& session) {
  // xtcam.f 181-188: LGPARM = LGPARSAVE; LPLCAM stays active;
  // Buffer/panel/analysis unchanged.
  session.design.lgParm = session.design.lgParmSave;
}

CamberBlock200Report camberBlock200Report(const SessionComposition& session) {
  CamberBlock200Report report;
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return report;
  }
  report.camberActive = true;
  const auto wb = assembleDesignBuffer(session);
  // TCBUF (xtcam.f 886-893): GETCAM + GETMAX on the working buffer.
  report.distribution = core::geometry_design::getCamberThickness(
      wb.x, wb.xp, wb.y, wb.yp, wb.s);
  std::vector<double> ycmp(report.distribution.ncm, 0.0);
  const auto camberMax = core::geometry_design::getMaxPoint(
      report.distribution.xcm, report.distribution.ycm, ycmp);
  report.camberMax = camberMax.max;
  report.camberMaxFailed =
      camberMax.status == core::math::Diagnostic::limitReached;
  std::vector<double> ytkp(report.distribution.ntk, 0.0);
  const auto thicknessMax = core::geometry_design::getMaxPoint(
      report.distribution.xtk, report.distribution.ytk, ytkp);
  report.thicknessMax = thicknessMax.max;
  report.thicknessMaxFailed =
      thicknessMax.status == core::math::Diagnostic::limitReached;
  // xtcam.f 142-153: added-camber incidence report from the CAMB state.
  const auto& c = *session.design.camber;
  constexpr double kPi = 3.14159265358979323846;
  report.aleDeg = std::atan(core::math::deval(c.xcam.front(), c.ycam, c.ycamp,
                                              c.xcam)) /
                  (kPi / 180.0);
  report.ateDeg = std::atan(core::math::deval(c.xcam.back(), c.ycam, c.ycamp,
                                              c.xcam)) /
                  (kPi / 180.0);
  const double chrd = c.xcam.back() - c.xcam.front();
  for (std::size_t k = 0; k < c.ncam; ++k) {
    if (std::fabs(c.ycam[k]) > std::fabs(report.maxAddedCamberYc)) {
      report.maxAddedCamberYc = c.ycam[k];
      report.maxAddedCamberXc = c.xcam[k];
    }
  }
  if (chrd != 0.0) {
    report.maxAddedCamberYc = report.maxAddedCamberYc / chrd;
    report.maxAddedCamberXc = report.maxAddedCamberXc / chrd;
  }
  return report;
}

void camberSetCamberline(SessionComposition& session) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return;
  }
  auto wb = assembleDesignBuffer(session);
  core::geometry_design::setAddedCamberFromCamberline(*session.design.camber,
                                                      wb);
  session.design.ldCplot = false;  // xtcam.f 393
}

void camberInputCamberPoints(
    SessionComposition& session,
    std::span<const core::geometry_design::CamberInputPoint> points) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return;
  }
  core::geometry_design::inputAddedCamberPoints(*session.design.camber,
                                                points);
  session.design.ldCplot = false;  // xtcam.f 442
}

core::geometry_design::LoadingIntegrals camberInputLoadingPoints(
    SessionComposition& session,
    std::span<const core::geometry_design::CamberInputPoint> points) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return {};
  }
  const auto integrals =
      core::geometry_design::inputAddedLoadingPoints(*session.design.camber,
                                                     points);
  session.design.ldCplot = true;  // xtcam.f 525
  return integrals;
}

void camberClear(SessionComposition& session) {
  if (!designWorkAvailable(session)) {
    return;
  }
  // xtcam.f 591-595: LCLEAR = .TRUE. -> 100 block again.
  session.design.camber = core::geometry_design::initCamberSession(
      assembleDesignBuffer(session), session.operating.momentReferenceX);
  session.design.ldCplot = false;
}

core::geometry_design::LoadingIntegrals camberScale(
    SessionComposition& session, double scal) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return {};
  }
  return core::geometry_design::scaleAddedCamber(*session.design.camber,
                                                 scal);
}

core::Result<CamberAddResult> camberAdd(SessionComposition& session) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return core::Result<CamberAddResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "CAMB: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  const auto status =
      core::geometry_design::applyAddedCamber(*session.design.camber, wb);
  if (!status.isOk()) {
    return core::Result<CamberAddResult>::fail(status.error());
  }
  // ADD changed only the GDES working buffer (buffer contract);
  // current panel contour and analysis remain untouched.
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  session.design.ldCplot = false;  // xtcam.f 680
  CamberAddResult result;
  result.metrics = wb.metrics;
  result.addLoopOppositePointFailures = status.value().oppositePointFailures;
  return core::Result<CamberAddResult>::ok(result);
}

void camberRefresh(SessionComposition& session) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return;
  }
  core::geometry_design::refreshCamberSession(assembleDesignBuffer(session),
                                              *session.design.camber);
}

CamberRdacStatus camberReadAddedCamber(
    SessionComposition& session,
    std::span<const core::geometry_design::CamberInputPoint> points) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return CamberRdacStatus::noPointsFound;
  }
  // xtcam.f 342-371: preprocessing + interpolation in the kernel; only here
  // mutates the session (the file was already completely read by the io codec
  // before).
  const auto status =
      core::geometry_design::readAddedCamber(*session.design.camber, points);
  session.design.ldCplot = false;  // xtcam.f 370
  return status;
}

std::optional<CamberDistributionProjection> camberThicknessProjection(
    const SessionComposition& session) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return std::nullopt;
  }
  const auto wb = assembleDesignBuffer(session);
  const auto d = core::geometry_design::getCamberThickness(
      wb.x, wb.xp, wb.y, wb.yp, wb.s);
  CamberDistributionProjection p;
  p.name = session.geometry.name;
  p.xbch = session.design.camber->xbch;
  p.x = std::move(d.xtk);
  p.y = std::move(d.ytk);
  return p;
}

std::optional<CamberDistributionProjection> camberCamberProjection(
    const SessionComposition& session) {
  if (!designWorkAvailable(session) || !session.design.camber.has_value()) {
    return std::nullopt;
  }
  const auto wb = assembleDesignBuffer(session);
  const auto d = core::geometry_design::getCamberThickness(
      wb.x, wb.xp, wb.y, wb.yp, wb.s);
  CamberDistributionProjection p;
  p.name = session.geometry.name;
  p.xbch = session.design.camber->xbch;
  p.x = std::move(d.xcm);
  p.y = std::move(d.ycm);
  return p;
}

core::Result<core::geometry_design::FlapResult> gdesDeflectFlap(
    SessionComposition& session, double xbf, double ybf, double ddef) {
  if (!designWorkAvailable(session)) {
    return core::Result<core::geometry_design::FlapResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result = core::geometry_design::deflectFlap(wb, xbf, ybf, ddef);
  if (!result.isOk()) {
    return result;
  }
  // RDEF == 0: canonical early return without geometry change; the
  // hinge values remain stored (GETXYF/FLAP set
  // XBF/YBF before the return).
  const bool mutated = result.value().nbAfter != result.value().nbBefore || ddef != 0.0;
  syncDesignBuffer(session, wb, /*geometryChanged=*/mutated);
  return result;
}

core::geometry_design::FlapHingeResult gdesFlapHinge(
    const SessionComposition& session, double xf, double yf) {
  auto wb = assembleDesignBuffer(session);
  return core::geometry_design::flapHinge(wb, xf, yf);
}

core::geometry_design::ThicknessCamberReport gdesThicknessCamberReport(
    const SessionComposition& session) {
  auto wb = assembleDesignBuffer(session);
  return core::geometry_design::reportThicknessCamber(wb);
}

core::Result<core::geometry_design::CornerAddResult> gdesAddCornerPoints(
    SessionComposition& session, double atol, int ispl, double xrf1,
    double xrf2) {
  if (!designWorkAvailable(session)) {
    return core::Result<core::geometry_design::CornerAddResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result =
      core::geometry_design::addCornerPoints(wb, atol, ispl, xrf1, xrf2);
  if (!result.isOk()) {
    return result;
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

core::Result<core::geometry_design::CornerAddResult> gdesAddCornerPoints(
    SessionComposition& session) {
  if (!designWorkAvailable(session)) {
    return core::Result<core::geometry_design::CornerAddResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "GDES: no design buffer active"});
  }
  auto wb = assembleDesignBuffer(session);
  auto result = core::geometry_design::addCornerPoints(wb);
  if (!result.isOk()) {
    return result;
  }
  syncDesignBuffer(session, wb, /*geometryChanged=*/true);
  return result;
}

core::geometry::SectionBending gdesBendReport(
    const SessionComposition& session) {
  return core::geometry::ijsect(session.geometry.buffer.x,
                                session.geometry.buffer.y);
}

core::geometry::CornerAngleReport gdesCangReport(
    const SessionComposition& session) {
  return core::geometry::cang(session.geometry.buffer.x,
                              session.geometry.buffer.y);
}

core::geometry::CurvatureReport gdesClisReport(
    const SessionComposition& session) {
  if (!session.design.work.has_value()) {
    return core::geometry::CurvatureReport{};
  }
  return core::geometry::clis(session.geometry.buffer.x,
                              session.design.work->xp,
                              session.geometry.buffer.y,
                              session.design.work->yp, session.design.work->s);
}

void setWriteOrientation(SessionComposition& session, bool clockwise) {
  session.geometry.writeClockwise = clockwise;
}

void setDelimiter(SessionComposition& session, int delimiter) {
  session.geometry.delimiter = delimiter;
}

void setNormalizeOnLoad(SessionComposition& session, bool normalize) {
  session.geometry.normalizeOnLoad = normalize;
}

int delimiter(const SessionComposition& session) {
  return session.geometry.delimiter;
}

bool writeClockwise(const SessionComposition& session) {
  return session.geometry.writeClockwise;
}

bool normalizeOnLoad(const SessionComposition& session) {
  return session.geometry.normalizeOnLoad;
}

const std::string& isesDomainParameters(const SessionComposition& session) {
  return session.geometry.isesDomainParameters;
}

void setIsesDomainParameters(SessionComposition& session,
                             const std::string& parameters) {
  session.geometry.isesDomainParameters = parameters;
}

// ---: runtime/VPAR producers ---------------------------------------------

void setViscousMode(SessionComposition& session, bool viscous) {
  session.operating.viscous = viscous;
  invalidateViscousConvergenceOnly(session);
}

// Fortran VISC sets only REINF1 (xoper.f 208) and LVCONV (215), but no
// MRCL. The derived values MINF/REINF remain until the next
// Re/Mach/TYPE/analysis call unchanged.
void setReynoldsBaseNumber(SessionComposition& session, double reynolds) {
  session.operating.machReynolds.reinf1 = reynolds;
  invalidateViscousConvergenceOnly(session);
}

void setReynoldsNumber(SessionComposition& session, double reynolds) {
  session.operating.machReynolds.reinf1 = reynolds;
  applyMrclAtUnitCl(session);
  invalidateViscousConvergenceOnly(session);
}

void setMachNumber(SessionComposition& session, double mach) {
  session.operating.machReynolds.minf1 = mach;
  applyMrclAtUnitCl(session);
  applyComsetFromCurrentMinf(session);
  invalidateViscousConvergenceOnly(session);
}

void setMachReynoldsTypes(SessionComposition& session, int machType,
                          int reType) {
  session.operating.machReynolds.machType = machType;
  session.operating.machReynolds.reType = reType;
  applyMrclAtUnitCl(session);
  applyComsetFromCurrentMinf(session);
  invalidateViscousConvergenceOnly(session);
}

void setIterationLimit(SessionComposition& session, int maximumIterations) {
  session.operating.maximumIterations = maximumIterations;
}

// Fortran Mach post-processing (xoper.f 328-333). Only CLCALC and CDCALC
// on the preserved point state; no SPECAL/SPECCL/VISCAL, no new
// gamma solution. The same confirmed core kernel as in the LVCONV branch of
// VISCAL (xoper.f 2948-2961, Viscal.cpp). Cp (CPCALC) is not persisted.
bool refreshExistingPointAfterMachChange(SessionComposition& session) {
  if (!session.paneling.panelingValid) {
    return false;
  }
  core::boundary_layer::ViscalState& viscals = session.viscous.viscals;
  if (viscals.gam.empty() || viscals.gamA.empty()) {
    // No valid analyzed point (GAM empty): no invented values.
    return false;
  }
  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  const OperatingConditionState& operating = session.operating;

  // CLCALC (xoper.f 330): Cl/CM/CDP from the retained GAM for the new MINF.
  const core::inviscid::LoadResult load = core::inviscid::clCalc(
      panel.x, panel.y, viscals.gam, viscals.gamA, viscals.alfa, viscals.minf,
      operating.qinf, operating.momentReferenceX, operating.momentReferenceY);
  viscals.cl = load.cl;
  viscals.cm = load.cm;
  viscals.cdp = load.cdp;

  // CDCALC (xoper.f 332): Cd (wake extrapolation for the new TKLAM) and CDF
  // (friction from the retained TAU). For an inviscid or uninitialized
  // BL the kernel returns Cd=0/CDF=0 (xfoil.f 1190-1194, 1197-1205).
  const core::boundary_layer::CdCalcResult cdc = core::boundary_layer::cdcalc(
      viscals.alfa, operating.qinf, viscals.tklam, operating.viscous,
      viscals.lblini, viscals.mapping, viscals.thet, viscals.dstr, viscals.uedg,
      viscals.tau, panel.x, panel.y);
  viscals.cd = cdc.cd;
  viscals.cdf = cdc.cdf;
  viscals.clf = cdc.clf;

  // canonical Mach CPCALC call (xoper.f:328-329).
  // CPI: CPCALC(N,QINV,QINF,MINF,CPI) overwrites exactly the first N
  // entries; the wake range (N+1..N+NW) stays as in the Fortran
  // COMMON. QINV comes from the preserved point (viscous
  // viscals state) or is losslessly reconstructed from the alpha=0/90 basis
  // (QISET, identical to the SPECAL QINV; the same path as
  // currentCpMinimum). CPV: CPCALC(N+NW,QVIS,...) only for LVISC and
  // resolved QVIS. cpCalc is the same confirmed core kernel (CPCALC,
  // xfoil.f:1057); no formula in SessionOps. For TE<=0 (canonical
  // CPCALC warning) the retained state stays unchanged in a controlled way.
  const std::size_t nPanel = panel.x.size();
  std::vector<double> qinvN;
  if (!viscals.qinv.empty()) {
    const std::size_t take = std::min(nPanel, viscals.qinv.size());
    qinvN.assign(viscals.qinv.begin(),
                 viscals.qinv.begin() + static_cast<std::ptrdiff_t>(take));
  } else if (session.viscous.systemValid) {
    std::vector<double> qinvFull;
    std::vector<double> qinvAFull;
    core::inviscid::qiset(session.viscous.system.qinvu1,
                          session.viscous.system.qinvu2, viscals.alfa,
                          qinvFull, qinvAFull);
    const std::size_t take = std::min(nPanel, qinvFull.size());
    qinvN.assign(qinvFull.begin(),
                 qinvFull.begin() + static_cast<std::ptrdiff_t>(take));
  }
  if (qinvN.size() == nPanel) {
    const auto cpiNew =
        core::inviscid::cpCalc(qinvN, operating.qinf, viscals.minf);
    if (cpiNew.isOk()) {
      std::vector<double>& cpi = session.viscous.cpi;
      if (cpi.size() < nPanel) {
        cpi.resize(nPanel, 0.0);
      }
      std::copy(cpiNew.value().begin(), cpiNew.value().end(), cpi.begin());
    }
  }
  if (operating.viscous && !viscals.qvis.empty()) {
    const auto cpvNew =
        core::inviscid::cpCalc(viscals.qvis, operating.qinf, viscals.minf);
    if (cpvNew.isOk()) {
      session.viscous.cpv = cpvNew.value();
    }
  }

  // Public final quantities of the present result update;
  // status/convergence/iterations/alpha/RMS remain a point snapshot (Mach is
  // no new analysis; LVCONV=false already stands in viscous.converged).
  AnalysisResult& result = session.result.lastResult;
  result.liftCoefficient = load.cl;
  result.dragCoefficient = cdc.cd;
  result.momentCoefficient = load.cm;
  result.centerOfPressureX = load.centerOfPressureX;
  return true;
}

// On-demand Cp derivation.
// Pure read query: reads the preserved point/wake/operating state and
// reconstructs CPI/CPV/CPMIN via the confirmed kernels cpCalc (CPCALC,
// xfoil.f 1057-1085) and fcpmin (FCPMIN, xoper.f 1768-1801). Corresponds to the
// canonical CPCALC side effect of Mach (xoper.f 328-329) and the
// CPCALC exit of VISCAL (xoper.f 3026-3027, Viscal.cpp 508-515). No
// session mutation, no solver/SPECAL/VISCAL execution, no
// persistence of a second Cp state. Without a valid point (panel
// invalid or GAM empty) valid==false is provided without invented values;
// the Fortran COMMON artifact cpCalc(QINV=0)=1.0236 (harness scenario A)
// is deliberately not output.
CpObservation currentCpMinimum(const SessionComposition& session) {
  CpObservation obs;
  if (!session.paneling.panelingValid) {
    return obs;
  }
  const core::boundary_layer::ViscalState& viscals = session.viscous.viscals;
  if (viscals.gam.empty() || viscals.gamA.empty()) {
    return obs;  // no valid analyzed point (GAM empty)
  }
  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  const double qinf = session.operating.qinf;
  const double minf = viscals.minf;
  const bool lvisc = session.operating.viscous;

  // QINV provisioning: the viscous point holds QINV/QVIS (n+1+nw, wake
  // included) in the viscals state. The inviscid single path does not persist
  // QINV; it is losslessly reconstructible from the preserved alpha=0/90 basis
  // (QISET, xpanel.f; identical to the QINV exit of the
  // SPECAL kernel, which produces the CPI of harness scenario B).
  std::vector<double> qinvSource;
  if (!viscals.qinv.empty()) {
    qinvSource = viscals.qinv;
  } else {
    if (!session.viscous.systemValid) {
      return obs;  // no valid unit basis -> controlled: no value
    }
    std::vector<double> qinvA;
    core::inviscid::qiset(session.viscous.system.qinvu1,
                          session.viscous.system.qinvu2, viscals.alfa,
                          qinvSource, qinvA);
  }

  auto cpiResult = core::inviscid::cpCalc(qinvSource, qinf, minf);
  if (cpiResult.isErr()) {
    return obs;  // controlled core denominator state (DEN <= 0)
  }
  obs.cpInviscid = std::move(cpiResult.value());

  if (lvisc) {
    if (viscals.qvis.empty()) {
      return obs;  // no resolved QVIS field (e.g. '!' before the first VISCAL)
    }
    auto cpvResult = core::inviscid::cpCalc(viscals.qvis, qinf, minf);
    if (cpvResult.isErr()) {
      return obs;
    }
    obs.cpViscous = std::move(cpvResult.value());
  } else {
    // FCPMIN sets CPMNV=CPMNI and XCPMNV=XCPMNI for inviscid computation
    // (xoper.f 1794-1797); the CPV field is an irrelevant Fortran leftover there.
    // The CPI copy provides the same length as xFull/CPI safe.
    obs.cpViscous = obs.cpInviscid;
  }

  // the x field must fit the Cp length: the viscous path holds
  // QINV/QVIS including wake (N+NW); the inviscid QISET reconstruction
  // returns only the panel contour (N). Fortran FCPMIN runs to N+NW,
  // but uses exactly the X field belonging to the point.
  std::vector<double> xFull = panel.x;
  if (xFull.size() < obs.cpInviscid.size()) {
    xFull.insert(xFull.end(), session.viscous.wake.x.begin(),
                 session.viscous.wake.x.end());
  }
  if (xFull.size() > obs.cpInviscid.size()) {
    obs.cpInviscid.clear();
    obs.cpViscous.clear();
    return obs;  // controlled: no mixed contour/wake length
  }
  obs.minimum = core::boundary_layer::fcpmin(lvisc, xFull, obs.cpInviscid,
                                             obs.cpViscous);
  obs.valid = true;
  return obs;
}

void setBoundaryLayerInitializationMode(SessionComposition& session,
                                        bool autoInitialize) {
  session.operating.autoInitializeBoundaryLayer = autoInitialize;
}

void setViscousAcceleration(SessionComposition& session, double vaccel) {
  session.operating.viscousAcceleration = vaccel;
}

void setTransitionParameters(SessionComposition& session, double nCritical,
                             double topTripX, double bottomTripX) {
  session.operating.criticalReynoldsExponents = {nCritical, nCritical};
  session.operating.topTripX = topTripX;
  session.operating.bottomTripX = bottomTripX;
  // Fortran VPAR XTR/N (xoper.f 2569-2597): only XSTRIP/ACRIT and
  // LVCONV=.FALSE.; wake/DIJ/BL fields and the continuation state of the
  // last point stay preserved.
  invalidateViscousConvergenceOnly(session);
}

// Fortran VPAR WAKE/W (xoper.f 2635-2646) writes
// WAKLEN and invalidated exclusively LWAKE, LBLINI, LVCONV, LIPAN.
// WAKLEN0 (2637) is a readerless dead store and is not
// replicated. No wake/BL/IPAN arrays are cleared, no
// lastResult/LRECALC state is touched and no panel regeneration
// is triggered; regeneration happens later in the canonical
// VISCAL path (xoper.f 2904/2914).
void setWakeLength(SessionComposition& session, double wakeLength) {
  session.operating.wakeLength = wakeLength;
  session.viscous.wakeValid = false;        // LWAKE
  session.viscous.viscals.lblini = false;   // LBLINI
  session.viscous.viscals.lvconv = false;   // LVCONV
  session.viscous.converged = false;        // LVCONV alias state
  session.viscous.viscals.mapping.valid = false;  // LIPAN (IBLPAN pointer)
}

// Fortran XTR (xoper.f 2569-2582): only XSTRIP(1/2) and LVCONV=.FALSE..
// invalidateViscousConvergenceOnly is the confirmed LVCONV-only
// invalidation (xoper.f 2582); LBLINI/LIPAN/wake/DIJ/station fields/
// influence matrix/lastResult stay preserved.
void setTripPositions(SessionComposition& session, double topTripX,
                      double bottomTripX) {
  session.operating.topTripX = topTripX;
  session.operating.bottomTripX = bottomTripX;
  invalidateViscousConvergenceOnly(session);
}

// Fortran N (xoper.f 2585-2597): ACRIT(1)=ACRIT(2)=nCritical, LVCONV=.FALSE..
void setCriticalAmplificationBoth(SessionComposition& session,
                                  double nCritical) {
  session.operating.criticalReynoldsExponents = {nCritical, nCritical};
  invalidateViscousConvergenceOnly(session);
}

// xfoil.def sets ACRIT(1)/ACRIT(2) independent (GETDEF
// xfoil.f 935). Invalidation identical to NCRIT (LVCONV=false).
void setCriticalAmplificationSides(SessionComposition& session, double top,
                                   double bottom) {
  session.operating.criticalReynoldsExponents = {top, bottom};
  invalidateViscousConvergenceOnly(session);
}

// VPAR NT (xoper.f 2600-2610) writes only
// ACRIT(1), VPAR NB (2613-2623) only ACRIT(2); each only LVCONV=false
// (invalidateViscousConvergenceOnly, confirmed LVCONV-only contract
// as for XTR/N). The counterpart page stays bit-identical.
void setCriticalAmplificationTop(SessionComposition& session, double top) {
  session.operating.criticalReynoldsExponents[0] = top;
  invalidateViscousConvergenceOnly(session);
}

void setCriticalAmplificationBottom(SessionComposition& session,
                                    double bottom) {
  session.operating.criticalReynoldsExponents[1] = bottom;
  invalidateViscousConvergenceOnly(session);
}

// OPER DAMP (xoper.f 1744-1750). Canonical
// toggle: IF(IDAMP.EQ.0) IDAMP=1 ELSE IDAMP=0. Unexpected internal
// values thus run back to 0. No further mutation.
int toggleAmplificationModel(SessionComposition& session) {
  session.viscous.idamp = (session.viscous.idamp == 0) ? 1 : 0;
  return session.viscous.idamp;
}

// ---: VPAR LAG/GB/CTR/CFAC ------------------------------------------------
// Fortran VPAR LAG (xoper.f 2656-2670) writes exclusively
// SCCON, DUXCON, DLCON (COMMON/BLPAR, BLPAR.INC). The three ASKR targets
// are written stepwise (2662-2664: CALL ASKR(...,SCCON),
// CALL ASKR(...,DUXCON), CALL ASKR(...,DLCON)); the handler therefore
// calls exactly these single assignments in Fortran order. No
// Flag invalidation: LVISC/LVCONV/LBLINI/LWAKE/LIPAN, wake and BL arrays,
// lastResult and all remaining BLPAR fields remain unchanged; the
// the effect arises only in the next actual BL/VISCAL
// evaluation. Instance-specific (session composition.viscous.blpar).
void setShearLagConstant(SessionComposition& session, double sccon) {
  session.viscous.blpar.sccon = sccon;  // SCCON
}
void setUxEqWeight(SessionComposition& session, double duxcon) {
  session.viscous.blpar.duxcon = duxcon;  // DUXCON
}
void setWakeDissipationLengthFactor(SessionComposition& session,
                                    double dlcon) {
  session.viscous.blpar.dlcon = dlcon;  // DLCON
}

// Fortran VPAR GB (xoper.f 2672-2685): GACON (2678/2681) and GBCON
// (2679/2682) are written progressively; CTCON is then
// always recomputed (2684, outside the IF/ELSE — also for
// an empty prompt answer). The canonical operation structure is
// CTCON = 0.5/(GACON**2 * GBCON); the REAL(8) build evaluates GACON**2 as
// GACON*GACON, then (GACON*GACON)*GBCON and finally the division.
// Bit-level proof against a focused Fortran harness (-fdefault-real-8) for
// six value pairs: identical to 0.5/(gacon*gacon*gbcon). GCCON and the
// remaining BLPAR fields are preserved; no flag invalidation.
// setGbConstantBAndCtcon combines the two Fortran assignments 2679/2682
// (GBCON) and 2684 (CTCON) — CTCON exactly once, from the then
  // current session GACON.
void setGbConstantA(SessionComposition& session, double gacon) {
  session.viscous.blpar.gacon = gacon;  // GACON
}
void setGbConstantBAndCtcon(SessionComposition& session, double gbcon) {
  session.viscous.blpar.gbcon = gbcon;  // GBCON
  const double gacon = session.viscous.blpar.gacon;
  session.viscous.blpar.ctcon = 0.5 / (gacon * gacon * gbcon);  // CTCON
}

// Fortran VPAR CTR (xoper.f 2687-2700): CTRCON and CTRCEX progressively
// (2693/2696, 2694/2697); no flag invalidation, no further
// BLPAR changes.
void setInitialCtauConstant(SessionComposition& session, double ctrcon) {
  session.viscous.blpar.ctrcon = ctrcon;  // CTRCON
}
void setInitialCtauExponent(SessionComposition& session, double ctrcex) {
  session.viscous.blpar.ctrcex = ctrcex;  // CTRCEX
}

// Fortran VPAR CFAC (xoper.f 2701-2706): only CFFAC; no LPACC guard in
// Fortran; no flag invalidation. Effect via the existing
// CFO-/CFT path (xblsys.f 2502, blu.f 121) only in the next
// turbulent computation.
void setBoundaryLayerCfScale(SessionComposition& session, double cffac) {
  session.viscous.blpar.cffac = cffac;  // CFFAC
}

// Fortran VPAR REST (xoper.f 2709-2715): CALL BLPINI (xbl.f 1578-1596).
// BLPINI writes exclusively the ten COMMON/BLPAR fields
// (BLPAR.INC) in assignment order SCCON GACON GBCON GCCON DLCON
// CTRCON CTRCEX DUXCON CTCON CFFAC; CTCON = 0.5/(GACON**2*GBCON) with the
// previously set defaults (bit-identical to the factory structure;
// harness-proven). No flag invalidation, no solution/wake/BL/
// mapping state change, no prompt, no output.
void restoreBoundaryLayerParameters(SessionComposition& session) {
  session.viscous.blpar = core::boundary_layer::blparConstants();
}

  // central defaults application. Startup (GETDEF LASK=.TRUE.) and
// RDEF (GETDEF LASK=.FALSE.) use exclusively this path; the
// field assignments are concentrated here, not spread across CLI handlers.
void applyDefaults(SessionComposition& session,
                   const xfoil::io::DefaultsValues& values, int idevrp,
                   const std::string& sourceFile) {
  // Panel parameters (GETDEF xfoil.f 905-906): PPAR contract, effect only
  // at the next PANGEN; no panel regeneration.
  core::paneling::PanelingParameters parameters =
      session.paneling.parameters;
  parameters.panelCount = values.npan;
  parameters.bunchingParam = values.cvpar;
  parameters.teToLeDensityRatio = values.cterat;
  parameters.refinementDensityRatio = values.ctrrat;
  parameters.upperRefinementRange = {values.xsref1, values.xsref2};
  parameters.lowerRefinementRange = {values.xpref1, values.xpref2};
  static_cast<void>(setPanelingParameters(session, parameters));

  // Mach/Reynolds (GETDEF xfoil.f 934-941): MINF1 directly, REINF1 =
  // RMILL*1e6, then MATYP/RETYP with MRCL(1.0) and COMSET (TYPE contract,
  // LVCONV=false).
  session.operating.machReynolds.minf1 = values.minf1;
  session.operating.machReynolds.reinf1 = values.rmill * 1.0e6;
  setMachReynoldsTypes(session, values.matyp, values.retyp);

  // VACCEL (GETDEF xfoil.f 934): pure value setting (VACCEL handler contract).
  setViscousAcceleration(session, values.vaccel);

  // ACRIT/XSTRIP page by page (GETDEF xfoil.f 935-936): NCRIT/XTR contract
  // (LVCONV=false).
  setCriticalAmplificationSides(session, values.acrit[0], values.acrit[1]);
  setTripPositions(session, values.xtrip[0], values.xtrip[1]);

  // Complete record incl. plot fields and IDEVRP (LCOLOR).
  session.defaults.values = values;
  session.defaults.idevrp = idevrp;
  session.defaults.loaded = true;
  session.defaults.sourceFile = sourceFile;
}

// GETDEF error branch (xfoil.f 905-936 READ ERR=80, label 80).
// Raw field assignment of the lines already read; no MRCL/COMSET,
// no IDEVRP, no invalidation (Fortran changes only the COMMON values).
void applyDefaultsPartial(SessionComposition& session,
                          const xfoil::io::DefaultsValues& values,
                          int linesRead) {
  core::paneling::PanelingParameters& paneling =
      session.paneling.parameters;
  if (linesRead >= 1) {
    paneling.panelCount = values.npan;
    paneling.bunchingParam = values.cvpar;
    paneling.teToLeDensityRatio = values.cterat;
    paneling.refinementDensityRatio = values.ctrrat;
  }
  if (linesRead >= 2) {
    paneling.upperRefinementRange = {values.xsref1, values.xsref2};
    paneling.lowerRefinementRange = {values.xpref1, values.xpref2};
  }
  if (linesRead >= 12) {
    session.operating.machReynolds.machType = values.matyp;
    session.operating.machReynolds.minf1 = values.minf1;
    session.operating.viscousAcceleration = values.vaccel;
  }
  if (linesRead >= 13) {
    session.operating.machReynolds.reType = values.retyp;
    session.operating.machReynolds.reinf1 = values.rmill * 1.0e6;
    session.operating.criticalReynoldsExponents = {values.acrit[0],
                                                   values.acrit[1]};
  }
  if (linesRead >= 14) {
    session.operating.topTripX = values.xtrip[0];
    session.operating.bottomTripX = values.xtrip[1];
  }
  session.defaults.values = values;
}

// Fortran VPAR INIT (xoper.f 2649-2653): toggles LBLINI; on disabling
// (LBLINI false) additionally sets LIPAN=false. No LVCONV, no wake/DIJ,
// no station-field deletion, no result change.
void toggleBoundaryLayerInitialization(SessionComposition& session) {
  session.viscous.viscals.lblini = !session.viscous.viscals.lblini;
  if (!session.viscous.viscals.lblini) {
    session.viscous.viscals.mapping.valid = false;
  }
}

std::vector<AnalysisResult> runClSequence(
    SessionComposition& session, double startCl, double endCl, double stepCl,
    Degrees startAlpha, const CancellationToken* cancellation) {
  std::vector<AnalysisResult> results;
  if (stepCl == 0.0) {
    return results;
  }
  const bool ascending = stepCl > 0.0;
  double cl = startCl;
  std::size_t count = 0;
  Degrees alpha = startAlpha;
  while (ascending ? (cl <= endCl) : (cl >= endCl)) {
    if (count >= kMaximumPolarPoints) {
      break;
    }
    AnalysisResult point =
        runSpecCl(session, cl, alpha, cancellation);
    results.push_back(point);
    if (point.status == AnalysisStatus::cancelled) {
      break;
    }
    if (point.successful) {
      alpha = point.alpha;  // warm start: alpha of the reached point
    }
    const double next = cl + stepCl;
    if (next == cl) {
      break;
    }
    cl = next;
    ++count;
  }
  return results;
}

namespace {

// Fortran ASEQ/CSEQ point count (xoper.f 576-580/596-600):
// NPOINT = INT((END-START)/STEP + 0.5) + 1, step sign enforced from
// start/end. -> exactly one point at the start.
std::size_t operSequencePointCount(double start, double end, double step,
                                   double& signedStep) {
  if (step == 0.0) {
    signedStep = 0.0;
    return 1;
  }
  signedStep = (end < start) ? -std::fabs(step) : std::fabs(step);
  const double raw = (end - start) / signedStep + 0.5;
  long count = static_cast<long>(raw) + 1;  // Fortran INT truncates toward zero
  if (count < 1) {
    count = 1;
  }
  return static_cast<std::size_t>(count);
}

// Fortran NSEQEX (xfoil.f:698) and ISEQEX counter (xoper.f 738-753):
// counts consecutively unconverged VISCAL points; halt at
// NSEQEX. Counts only in viscous mode (LVISC). An unconverged
// VISCAL point carries status=unconverged and converged=false (unsuccessful in the
// convergence, but not in the error path).
constexpr std::size_t kOperSequenceNonConvergenceLimit = 4;

bool viscousNonConverged(const SessionComposition& session,
                         const AnalysisResult& point) {
  return session.operating.viscous && !point.converged;
}

}  // namespace

OperSequenceRun runOperAlphaSequenceDetailed(
    SessionComposition& session, Degrees start, Degrees end, Degrees step,
    const CancellationToken* cancellation,
    const OperPointCompletedHook& afterPoint) {
  OperSequenceRun run;
  double signedStep = 0.0;
  const std::size_t npoint =
      operSequencePointCount(start.value, end.value, step.value, signedStep);

  // Fortran ALAST/CLAST (xoper.f 661-662): start values are the current
  // session state before the sequence.
  double alast = session.result.lastResult.alpha.value;
  double clast = session.result.lastResult.liftCoefficient;
  run.lastConvergedAlphaDeg = alast;
  run.lastConvergedCl = clast;

  std::size_t iseqex = 0;
  for (std::size_t i = 0; i < npoint; ++i) {
    if (cancellation != nullptr && cancellation->isCancellationRequested()) {
      AnalysisResult cancelled;
      cancelled.alpha = Degrees{start.value + signedStep * static_cast<double>(i)};
      cancelled.status = AnalysisStatus::cancelled;
      cancelled.successful = false;
      session.result.lastResult = cancelled;
      run.points.push_back(OperSequencePointRecord{
          cancelled, session.result.lastDiagnostics,
          start.value + signedStep * static_cast<double>(i),
          cancelled.alpha.value, cancelled.alpha.value});
      run.stopReason = OperSequenceStopReason::cancelled;
      run.iseqexAtStop = iseqex;
      return run;
    }
    const Degrees alpha{start.value + signedStep * static_cast<double>(i)};
    AnalysisResult point = runSpecifiedPoint(
        session, alpha, 0.0, PointSpecificationPolicy::specifiedAlpha,
        cancellation, PointExecutionPolicy::operSequenceWarmStart);
    run.points.push_back(OperSequencePointRecord{
        point, session.result.lastDiagnostics, alpha.value, alpha.value,
        point.alpha.value});
    // PLRADD position (xoper.f 440-447): immediately after the point,
    // but only for domain-specific performed points.
    if (afterPoint && point.successful) {
      afterPoint(point);
    }
    if (point.status == AnalysisStatus::cancelled) {
      run.stopReason = OperSequenceStopReason::cancelled;
      run.iseqexAtStop = iseqex;
      return run;
    }
    if (point.status == AnalysisStatus::missingGeometry ||
        point.status == AnalysisStatus::missingConfiguration ||
        point.status == AnalysisStatus::invalidAnalysisSettings) {
      run.stopReason = OperSequenceStopReason::pointFailure;
      run.iseqexAtStop = iseqex;
      return run;
    }
    if (viscousNonConverged(session, point)) {
      ++iseqex;
      if (iseqex >= kOperSequenceNonConvergenceLimit) {
        run.stopReason =
            OperSequenceStopReason::consecutiveNonConvergenceLimit;
        run.iseqexAtStop = iseqex;
        run.lastConvergedAlphaDeg = alast;
        run.lastConvergedCl = clast;
        return run;
      }
    } else if (point.successful) {
      iseqex = 0;
      alast = point.alpha.value;
      clast = point.liftCoefficient;
    }
  }
  run.stopReason = OperSequenceStopReason::completed;
  run.iseqexAtStop = iseqex;
  run.lastConvergedAlphaDeg = alast;
  run.lastConvergedCl = clast;
  return run;
}

std::vector<AnalysisResult> runOperAlphaSequence(
    SessionComposition& session, Degrees start, Degrees end, Degrees step,
    const CancellationToken* cancellation) {
  std::vector<AnalysisResult> results;
  OperSequenceRun run =
      runOperAlphaSequenceDetailed(session, start, end, step, cancellation);
  for (const auto& rec : run.points) {
    results.push_back(rec.result);
  }
  return results;
}

OperSequenceRun runOperClSequenceDetailed(
    SessionComposition& session, double startCl, double endCl, double stepCl,
    Degrees startAlpha, const CancellationToken* cancellation,
    const OperPointCompletedHook& afterPoint) {
  OperSequenceRun run;
  double signedStep = 0.0;
  const std::size_t npoint =
      operSequencePointCount(startCl, endCl, stepCl, signedStep);

  double alast = session.result.lastResult.alpha.value;
  double clast = session.result.lastResult.liftCoefficient;
  run.lastConvergedAlphaDeg = alast;
  run.lastConvergedCl = clast;

  Degrees alpha = startAlpha;
  // Fortran CSEQ starts each SPECCL on the raw COMMON alfa
  // of the previous point (xoper.f 672, CR09 radians). The carrier passes this
  // radian value directly; the degrees field stays pure bookkeeping/diagnostics.
  double alfaRad = session.viscous.viscals.alfa;
  std::size_t iseqex = 0;
  for (std::size_t i = 0; i < npoint; ++i) {
    if (cancellation != nullptr && cancellation->isCancellationRequested()) {
      AnalysisResult cancelled;
      cancelled.alpha = alpha;
      cancelled.status = AnalysisStatus::cancelled;
      cancelled.successful = false;
      session.result.lastResult = cancelled;
      run.points.push_back(OperSequencePointRecord{
          cancelled, session.result.lastDiagnostics,
          startCl + signedStep * static_cast<double>(i), alpha.value,
          alpha.value});
      run.stopReason = OperSequenceStopReason::cancelled;
      run.iseqexAtStop = iseqex;
      return run;
    }
    const double clSpec = startCl + signedStep * static_cast<double>(i);
    const double alphaBefore = alpha.value;
    AnalysisResult point = runSpecifiedPoint(
        session, alpha, clSpec,
        PointSpecificationPolicy::operSequenceSpecifiedCl, cancellation,
        PointExecutionPolicy::operSequenceWarmStart, alfaRad);
    run.points.push_back(OperSequencePointRecord{
        point, session.result.lastDiagnostics, clSpec, alphaBefore,
        point.alpha.value});
    // PLRADD position (xoper.f 440-447): immediately after the point,
    // but only for domain-specific performed points.
    if (afterPoint && point.successful) {
      afterPoint(point);
    }
    if (point.status == AnalysisStatus::cancelled) {
      run.stopReason = OperSequenceStopReason::cancelled;
      run.iseqexAtStop = iseqex;
      return run;
    }
    if (point.status == AnalysisStatus::missingGeometry ||
        point.status == AnalysisStatus::missingConfiguration ||
        point.status == AnalysisStatus::invalidAnalysisSettings) {
      run.stopReason = OperSequenceStopReason::pointFailure;
      run.iseqexAtStop = iseqex;
      return run;
    }
    // Fortran: the next SPECCL starts at the current Alfa (xoper.f 672 uses
    // the COMMON Alfa of the previous point, independent of convergence).
    alpha = point.alpha;
    alfaRad = session.viscous.viscals.alfa;
    // ISEQEX (xoper.f 738-753): raises for LVISC and.NOT.LVCONV, otherwise
    // (converged or inviscid) reset.
    if (viscousNonConverged(session, point)) {
      ++iseqex;
      if (iseqex >= kOperSequenceNonConvergenceLimit) {
        run.stopReason =
            OperSequenceStopReason::consecutiveNonConvergenceLimit;
        run.iseqexAtStop = iseqex;
        run.lastConvergedAlphaDeg = alast;
        run.lastConvergedCl = clast;
        return run;
      }
    } else if (point.converged) {
      iseqex = 0;
      alast = point.alpha.value;
      clast = point.liftCoefficient;
    }
  }
  run.stopReason = OperSequenceStopReason::completed;
  run.iseqexAtStop = iseqex;
  run.lastConvergedAlphaDeg = alast;
  run.lastConvergedCl = clast;
  return run;
}

std::vector<AnalysisResult> runOperClSequence(
    SessionComposition& session, double startCl, double endCl, double stepCl,
    Degrees startAlpha, const CancellationToken* cancellation) {
  std::vector<AnalysisResult> results;
  OperSequenceRun run = runOperClSequenceDetailed(session, startCl, endCl,
                                                  stepCl, startAlpha,
                                                  cancellation);
  for (const auto& rec : run.points) {
    results.push_back(rec.result);
  }
  return results;
}

// ---
// Polar session operations (xoper.f 978-1448, xpol.f 733-885).
// 0-based; no texts/prompts/files. The CLI layer sets the
// canonical 1-based indexing and leads the prompt chains.
// ---

// PLRCOP upward chain (xoper.f 1120-1128/1374-1379/1008-1012): slots from
// index0+1 around a position after the shift above; IPACT carried along as in
// Fortran. saveFileNames (PFNAME) remain 1:1 parallel to slots.
namespace {

std::vector<std::pair<int, int>> shiftPolarSlotsUp(SessionComposition& session,
                                                   int index0) {
  auto& polar = session.polar;
  std::vector<std::pair<int, int>> moves;
  const int count = static_cast<int>(polar.slots.size());
  for (int j = index0 + 1; j < count; ++j) {
    // copySlot always succeeds for valid indices (self-copy
    // allowed, quantities unchanged); the status is canonically not
    // evaluated (PLRCOP has no error branch).
    std::ignore = core::polar::copySlot(
        polar.slots[static_cast<std::size_t>(j)],
        polar.slots[static_cast<std::size_t>(j - 1)]);
    polar.saveFileNames[static_cast<std::size_t>(j - 1)] =
        polar.saveFileNames[static_cast<std::size_t>(j)];
    if (polar.dumpFileNames.size() == polar.slots.size()) {
      polar.dumpFileNames[static_cast<std::size_t>(j - 1)] =
          polar.dumpFileNames[static_cast<std::size_t>(j)];
    }
    moves.emplace_back(j, j - 1);
    if (polar.activeSlot == j) {
      polar.activeSlot = j - 1;
    }
  }
  return moves;
}

}  // namespace

PolarSlotRemovalResult deletePolarSlot(SessionComposition& session,
                                       int index0) {
  PolarSlotRemovalResult result;
  auto& polar = session.polar;
  const int count = static_cast<int>(polar.slots.size());
  if (index0 < 0 || index0 >= count) {
    return result;
  }
  if (polar.activeSlot == index0) {
    // xoper.f 1114-1118: active polar deleted -> accumulation off.
    polar.activeSlot = -1;
    polar.accumulating = false;
    result.accumulationDisabled = true;
  }
  result.moves = shiftPolarSlotsUp(session, index0);
  polar.slots.pop_back();
  if (polar.saveFileNames.size() == polar.slots.size()) {
    polar.saveFileNames.pop_back();
  }
  if (polar.dumpFileNames.size() == polar.slots.size()) {
    polar.dumpFileNames.pop_back();
  }
  result.deleted = true;
  result.activeSlotAfter = polar.activeSlot;
  return result;
}

void deleteAllPolarSlots(SessionComposition& session) {
  // xoper.f 1096-1100: NPOL=0, IPACT=0, LPACC=false.
  session.polar.slots.clear();
  session.polar.saveFileNames.clear();
  session.polar.dumpFileNames.clear();
  session.polar.activeSlot = -1;
  session.polar.accumulating = false;
}

core::Status sortPolarSlotByAlpha(SessionComposition& session, int index0) {
  auto& polar = session.polar;
  const int count = static_cast<int>(polar.slots.size());
  if (index0 < 0 || index0 >= count) {
    return core::Status::fail(core::Error::make(
        core::ErrorCode::outOfRange, "Polar slot index out of range"));
  }
  // PSOR sets IDSORT = IAL (xoper.f 1153); PLRSRT with HSORT/ASORT kernels.
  return core::polar::sortPoints(
      polar.slots[static_cast<std::size_t>(index0)],
      core::polar::SortKey::alpha);
}

core::Status renamePolarSlot(SessionComposition& session, int index0,
                             const std::string& name) {
  auto& polar = session.polar;
  const int count = static_cast<int>(polar.slots.size());
  if (index0 < 0 || index0 >= count) {
    return core::Status::fail(core::Error::make(
        core::ErrorCode::outOfRange, "Polar slot index out of range"));
  }
  // PNAM (xoper.f 1446-1447): NAMEPOL(IP) = READ line, then STRIP. The
  // CLI returns the stripped name; the session stores it directly.
  polar.slots[static_cast<std::size_t>(index0)].metadata.airfoilName = name;
  return core::Status::ok();
}

PolarPointRemovalResult removePolarPoints(SessionComposition& session,
                                          int index0,
                                          const std::vector<double>& alphas) {
  PolarPointRemovalResult result;
  auto& polar = session.polar;
  const int count = static_cast<int>(polar.slots.size());
  if (index0 < 0 || index0 >= count) {
    return result;
  }
  auto& slot = polar.slots[static_cast<std::size_t>(index0)];
  // PREM (xoper.f 1353-1392): per alpha removes the first hit within the
  // 3-digit print tolerance 5e-4; points and page data
  // recompute in parallel.
  for (double alpha : alphas) {
    bool removed = false;
    for (std::size_t ia = 0; ia < slot.points.size(); ++ia) {
      if (std::abs(slot.points[ia].alpha - alpha) < 0.0005) {
        const bool haveElementData =
            slot.elementData.size() == slot.points.size();
        slot.points.erase(slot.points.begin() +
                          static_cast<std::ptrdiff_t>(ia));
        slot.sideData.erase(slot.sideData.begin() +
                            static_cast<std::ptrdiff_t>(ia));
        if (haveElementData) {
          slot.elementData.erase(
              slot.elementData.begin() + static_cast<std::ptrdiff_t>(ia));
        }
        ++result.removedPoints;
        removed = true;
        break;
      }
    }
    if (!removed) {
      continue;
    }
    if (slot.points.empty()) {
      // xoper.f 1372-1386: last point removed -> remove the slot and
      // leave PREM (GO TO 500).
      result.moves = shiftPolarSlotsUp(session, index0);
      polar.slots.pop_back();
      if (polar.saveFileNames.size() == polar.slots.size()) {
        polar.saveFileNames.pop_back();
      }
      if (polar.dumpFileNames.size() == polar.slots.size()) {
        polar.dumpFileNames.pop_back();
      }
      result.polarEliminated = true;
      result.activeSlotAfter = polar.activeSlot;
      return result;
    }
  }
  result.activeSlotAfter = polar.activeSlot;
  return result;
}

void togglePolarIncludeCpMin(SessionComposition& session) {
  session.polar.includeCpMin = !session.polar.includeCpMin;
}

void togglePolarIncludeHingeMoment(SessionComposition& session) {
  session.polar.includeHingeMoment = !session.polar.includeHingeMoment;
}

ReferencePolarRemovalResult deleteReferenceSlot(SessionComposition& session,
                                                int index0) {
  ReferencePolarRemovalResult result;
  auto& polar = session.polar;
  const int count = static_cast<int>(polar.referenceSlots.size());
  if (index0 < 0 || index0 >= count) {
    return result;
  }
  // RDEL (xoper.f 1008-1013): PRFCOP chain upward.
  for (int j = index0 + 1; j < count; ++j) {
    std::ignore = core::polar::copyReferenceSlot(
        polar.referenceSlots[static_cast<std::size_t>(j)],
        polar.referenceSlots[static_cast<std::size_t>(j - 1)]);
    result.moves.emplace_back(j, j - 1);
  }
  polar.referenceSlots.pop_back();
  result.deleted = true;
  return result;
}

void deleteAllReferenceSlots(SessionComposition& session) {
  // RDEL with IR=0: NPOLREF = 0 (xoper.f 992-994).
  session.polar.referenceSlots.clear();
}

PolarSlotIngestResult appendPolarSlot(SessionComposition& session,
                                      core::polar::PolarDataset slot,
                                      const std::string& saveFileName) {
  PolarSlotIngestResult result;
  auto& polar = session.polar;
  if (polar.slots.size() >= core::polar::maxPolarSlots) {
    // xoper.f 827-832: array boundary reached, no mutation.
    result.capacityExceeded = true;
    return result;
  }
  result.slotIndex = static_cast<int>(polar.slots.size());
  polar.slots.push_back(std::move(slot));
  polar.saveFileNames.push_back(saveFileName);
  polar.dumpFileNames.emplace_back();
  return result;
}

ReferencePolarIngestResult appendReferenceSlot(
    SessionComposition& session, core::polar::ReferencePolarDataset slot) {
  ReferencePolarIngestResult result;
  auto& polar = session.polar;
  if (polar.referenceSlots.size() >= core::polar::maxPolarSlots) {
    // xoper.f 935-940: NPOLREF >= NPX.
    result.capacityExceeded = true;
    return result;
  }
  result.slotIndex = static_cast<int>(polar.referenceSlots.size());
  polar.referenceSlots.push_back(std::move(slot));
  return result;
}

void setPolarSaveName(SessionComposition& session, int index0,
                      const std::string& name) {
  auto& polar = session.polar;
  if (index0 < 0 ||
      static_cast<std::size_t>(index0) >= polar.saveFileNames.size()) {
    return;
  }
  polar.saveFileNames[static_cast<std::size_t>(index0)] = name;
}

void adoptPolarParameters(SessionComposition& session, int index0) {
  // PLRSET path 20 (xpol.f 92-107).
  const auto& slot = session.polar.slots[static_cast<std::size_t>(index0)];
  setGeometryName(session, slot.metadata.airfoilName);
  auto& mr = session.operating.machReynolds;
  mr.reType = static_cast<int>(slot.metadata.reType);
  mr.machType = static_cast<int>(slot.metadata.machType);
  mr.minf1 = slot.metadata.machCl1;
  mr.reinf1 = slot.metadata.reCl1;
  session.operating.criticalReynoldsExponents = {slot.metadata.ncrit[0],
                                            slot.metadata.ncrit[1]};
  session.operating.topTripX = slot.metadata.xtrip[0];
  session.operating.bottomTripX = slot.metadata.xtrip[1];
}

int createPolarSlotForAccumulation(SessionComposition& session) {
  // PLRSET new slot (xpol.f 109-135).
  auto& polar = session.polar;
  if (polar.slots.size() >= core::polar::maxPolarSlots) {
    return -1;
  }
  core::polar::PolarDataset slot;
  slot.metadata.airfoilName = session.geometry.name;
  slot.metadata.reType =
      static_cast<core::polar::ReType>(session.operating.machReynolds.reType);
  slot.metadata.machType = static_cast<core::polar::MachType>(
      session.operating.machReynolds.machType);
  // xpol.f 117-121: REYNP1 = REINF1 for LVISC, otherwise 0; MACHP1 = MINF1.
  slot.metadata.reCl1 = session.operating.viscous
                            ? session.operating.machReynolds.reinf1
                            : 0.0;
  slot.metadata.machCl1 = session.operating.machReynolds.minf1;
  slot.metadata.ncrit = {session.operating.criticalReynoldsExponents[0],
                         session.operating.criticalReynoldsExponents[1]};
  slot.metadata.xtrip = {session.operating.topTripX,
                         session.operating.bottomTripX};
  // xpol.f 127-131: NXYPOL(IP) = N, CPOLXY = current panel contour.
  const auto& panel = session.paneling.panel;
  slot.geometry.x = panel.x;
  slot.geometry.y = panel.y;
  polar.slots.push_back(std::move(slot));
  polar.saveFileNames.emplace_back();
  polar.dumpFileNames.emplace_back();
  return static_cast<int>(polar.slots.size()) - 1;
}

core::Status applyArchivedPolarGeometry(SessionComposition& session,
                                        int index0) {
  // APCOPY (xpol.f 139-177).
  auto& polar = session.polar;
  if (index0 < 0 || static_cast<std::size_t>(index0) >= polar.slots.size()) {
    return core::Status::fail(core::Error::make(
        core::ErrorCode::outOfRange, "Polar slot index out of range"));
  }
  const auto& slot = polar.slots[static_cast<std::size_t>(index0)];
  const auto& panel = session.paneling.panel;
  const auto& gam = session.viscous.viscals.gam;
  const auto& gamA = session.viscous.viscals.gamA;
  const bool panelEmpty = panel.x.empty();
  core::geometry::TrailingEdgeInputs teInputs;
  teInputs.x0 = panelEmpty ? 0.0 : panel.x.front();
  teInputs.y0 = panelEmpty ? 0.0 : panel.y.front();
  teInputs.xp0 = panelEmpty ? 0.0 : panel.xp.front();
  teInputs.yp0 = panelEmpty ? 0.0 : panel.yp.front();
  teInputs.xn = panelEmpty ? 0.0 : panel.x.back();
  teInputs.yn = panelEmpty ? 0.0 : panel.y.back();
  teInputs.xpn = panelEmpty ? 0.0 : panel.xp.back();
  teInputs.ypn = panelEmpty ? 0.0 : panel.yp.back();
  teInputs.gam0 = gam.empty() ? 0.0 : gam.front();
  teInputs.gamn = gam.empty() ? 0.0 : gam.back();
  teInputs.gama0 = gamA.empty() ? 0.0 : gamA.front();
  teInputs.gaman = gamA.empty() ? 0.0 : gamA.back();
  const auto reconstructed = core::polar::restoreArchivedGeometry(
      slot.geometry, panel.x.size(), teInputs);
  if (!reconstructed.isOk()) {
    return core::Status::fail(reconstructed.error());
  }
  const auto& rec = reconstructed.value();
  auto& mutablePanel = session.paneling.panel;
  mutablePanel.x = rec.x;
  mutablePanel.y = rec.y;
  mutablePanel.s = rec.s;
  mutablePanel.xp = rec.xp;
  mutablePanel.yp = rec.yp;
  mutablePanel.xn = rec.nx;
  mutablePanel.yn = rec.ny;
  mutablePanel.apanel = rec.apanel;
  mutablePanel.sle = rec.sle;
  mutablePanel.xle = rec.xle;
  mutablePanel.yle = rec.yle;
  mutablePanel.xte = rec.xte;
  mutablePanel.yte = rec.yte;
  mutablePanel.chord = rec.chord;
  session.paneling.panelingValid = true;
  session.paneling.origin = PanelingOrigin::abcopy;
  session.paneling.panelSourceRevision = session.geometry.bufferRevision;
  // APCOPY replaced the panel contour with the archived
  // polar contour: raise the contour revision; inverse invalidation after
  // xpol.f 172 (LSCINI=.FALSE.).
  ++session.paneling.panelRevision;
  invalidateInverseStateAfterPanelChange(
      session, InversePanelChange::polarArchivedCopy);
  setGeometryName(session, slot.metadata.airfoilName);
  // APCOPY invalidations (xpol.f 167-175): LGAMU/LQINU/LWAKE/LQAIJ/
  // LADIJ/LWDIJ/LIPAN/LVCONV false; LBLINI false only on N change.
  session.viscous.systemValid = false;
  session.viscous.wakeValid = false;
  session.viscous.converged = false;
  session.viscous.viscals.dijWakeValid = false;
  session.viscous.viscals.dij.clear();
  session.viscous.viscals.mapping.valid = false;
  if (rec.resetBoundaryLayerInit) {
    session.viscous.viscals.lblini = false;
  }
  return core::Status::ok();
}

PolarAddResult addPolarPointFromSession(SessionComposition& session) {
  PolarAddResult result;
  auto& polar = session.polar;
  if (polar.activeSlot < 0) {
    // xpol.f 556-557: IP == 0 -> no active polar.
    result.outcome = PolarAddOutcome::noActivePolar;
    return result;
  }
  const auto& viscals = session.viscous.viscals;
  const bool viscous = session.operating.viscous;
  core::polar::PolarPointInput input;
  // PLRADD (xpol.f 570-596): ADEG = Alfa/DTOR; Cd/CDV/Re only for LVISC;
  // XOCTR/TINDEX only for LVISC otherwise 0; CPMN = CPMNV for LVISC otherwise
  // CPMNI (FCPMIN xoper.f 1795-1800); HMOM only for LFLAP (:
  // PLRADD 601-605 calls MHINGE per polar record and stores HMOM in
  // CPOL(I); canonically 0 without LFLAP).
  input.alphaDeg = viscals.alfa / kDtor;
  input.cl = viscals.cl;
  input.cd = viscals.cd;
  input.cm = viscals.cm;
  input.cdPres = viscals.cdp;
  input.cpMin = viscous ? session.operating.cpMinimum.cpmnv
                        : session.operating.cpMinimum.cpmni;
  input.mach = viscals.minf;
  input.viscous = viscous;
  input.reInf = viscals.reinf;
  input.acrit = {session.operating.criticalReynoldsExponents[0],
                 session.operating.criticalReynoldsExponents[1]};
  input.xtrip = {session.operating.topTripX, session.operating.bottomTripX};
  if (viscous) {
    input.xoctr = viscals.xoctr;
    input.tindex = viscals.tindex;
  }
  // PLRADD 601-605: IF(LFLAP) CALL MHINGE; CPOL(I)=HMOM; otherwise 0.
  // the same shared hinge-load use case as FMOM
  // (core inviscid::mhinge, LFLAP=true estimation path).
  if (session.design.flap.lFlap) {
    const auto hinge = computeCurrentHingeLoads(session);
    if (hinge.isOk()) {
      input.hingeMoment = hinge.value().hmom;
    }
  }
  const core::Status status = core::polar::addPoint(
      polar.slots[static_cast<std::size_t>(polar.activeSlot)], input);
  if (status.isErr()) {
    // xpol.f 560-561: NAPOL(IP) == NAX, no partial mutation.
    result.outcome = PolarAddOutcome::storageFull;
    return result;
  }
  result.outcome = PolarAddOutcome::added;
  result.slotIndex = polar.activeSlot;
  return result;
}

// ---
// flap/hinge/VELS use case.
// ---

// FNEW (xoper.f 1629-1641): the only mutation is the current flap
// state. Fortran sets XOF/YOF from RINPUT(1)/(2) or -999.0 and
// LFLAP always .FALSE.; prompts follow later in GETXYF (FMOM).
void setCurrentFlapSentinels(SessionComposition& session, double xof,
                             double yof) {
  session.design.flap.xof = xof;
  session.design.flap.yof = yof;
  session.design.flap.lFlap = false;
}

core::geometry_design::FlapHingeResult operFlapHinge(
    const SessionComposition& session, double xf, double yf, double yrel) {
  const auto& panel = session.paneling.panel;
  return core::geometry_design::flapHingeOnContour(
      panel.x, panel.xp, panel.y, panel.yp, panel.s, xf, yf, yrel);
}

core::Result<core::inviscid::HingeMomentResult> computeCurrentHingeLoads(
    const SessionComposition& session) {
  const auto& panel = session.paneling.panel;
  if (!session.paneling.panelingValid || panel.x.size() < 2) {
    return core::Result<core::inviscid::HingeMomentResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "MHINGE: no valid current contour"});
  }
  const std::size_t n = panel.x.size();
  const bool viscous = session.operating.viscous;
  const auto& cpi = session.viscous.cpi;
  const auto& cpv = session.viscous.cpv;
  // Fortran reads CPI/CPV from COMMON; after INIT (without an analyzed
  // point) the field is zero (confirmed CLI oracle value HMOM=0).
  // A non-empty distribution that does not fit the contour has
  // stale provenance and is rejected (validity contract).
  const std::vector<double> zeroCp(n, 0.0);
  const std::span<const double> cpInv =
      cpi.empty()   ? std::span<const double>(zeroCp)
      : cpi.size() >= n ? std::span<const double>(cpi)
                        : std::span<const double>{};
  const std::span<const double> cpVis =
      cpv.empty()     ? std::span<const double>(zeroCp)
      : cpv.size() >= n ? std::span<const double>(cpv)
                        : std::span<const double>{};
  if (cpInv.empty() || (viscous && cpVis.empty())) {
    return core::Result<core::inviscid::HingeMomentResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "MHINGE: stale Cp provenance"});
  }
  const auto& flap = session.design.flap;
  // LFLAP=true estimation path (xoper.f 2367-2368).
  const core::inviscid::HingeArcGuess guess{
      flap.xof, panel.s[n - 1] - flap.xof};
  const core::inviscid::HingeMomentResult result = core::inviscid::mhinge(
      panel.x, panel.xp, panel.y, panel.yp, panel.s, cpInv, cpVis, viscous,
      flap.xof, flap.yof, guess);
  return core::Result<core::inviscid::HingeMomentResult>::ok(result);
}

core::Result<VelocityAtPointResult> evaluateVelocityAtPoint(
    const SessionComposition& session, double x, double y) {
  const auto& panel = session.paneling.panel;
  if (!session.paneling.panelingValid || panel.x.size() < 2) {
    return core::Result<VelocityAtPointResult>::fail(
        core::Error{core::ErrorCode::invalidState,
                    "PSILIN: no valid current contour"});
  }
  const std::size_t n = panel.x.size();
  const auto& viscals = session.viscous.viscals;
  // Fortran COMMON GAM/GAM_A/GAMU1/GAMU2/SIG are zero after INIT; without an
  // analyzed point PSILIN returns only the
  // free-stream part (confirmed CLI oracle u=1, v=0).
  const std::vector<double> zeroGam(n, 0.0);
  const std::span<const double> gam =
      viscals.gam.size() >= n ? std::span<const double>(viscals.gam)
                              : std::span<const double>(zeroGam);
  const std::span<const double> gamA =
      viscals.gamA.size() >= n ? std::span<const double>(viscals.gamA)
                               : std::span<const double>(zeroGam);
  const auto& system = session.viscous.system;
  const std::span<const double> gamu1 =
      session.viscous.systemValid && system.gamu1.size() >= n
          ? std::span<const double>(system.gamu1)
          : std::span<const double>(zeroGam);
  const std::span<const double> gamu2 =
      session.viscous.systemValid && system.gamu2.size() >= n
          ? std::span<const double>(system.gamu2)
          : std::span<const double>(zeroGam);
  const std::vector<double> sig(n, 0.0);  // SIG canonically stays 0.

  // TECALC parameters (xfoil.f 2098, 2287-2312): paneling calls TECALC directly
  // after paneling; DSTE/ANTE/ASTE/SHARP are therefore already available
  // before the first analysis. Fortran TECALC uses the
  // panel contour itself (GAM parts are zero before the first solution).
  const core::inviscid::TrailingEdgeState te = core::inviscid::tecalcFull(
      panel.x[0], panel.y[0], panel.xp[0], panel.yp[0],
      panel.x[n - 1], panel.y[n - 1], panel.xp[n - 1], panel.yp[n - 1],
      panel.s[n - 1] - panel.s[0], 0.0, 0.0, 0.0, 0.0);

  core::inviscid::PsiLinContext ctx;
  ctx.x = panel.x;
  ctx.y = panel.y;
  ctx.s = panel.s;
  ctx.nx = panel.xn;
  ctx.ny = panel.yn;
  ctx.apanel = panel.apanel;
  ctx.gam = gam;
  ctx.gamA = gamA;
  ctx.gamu1 = gamu1;
  ctx.gamu2 = gamu2;
  ctx.sig = sig;
  ctx.sharp = te.sharp;
  ctx.dste = te.dste;
  ctx.ante = te.ante;
  ctx.aste = te.aste;
  ctx.qinf = session.operating.qinf;
  ctx.image = false;
  ctx.yimage = 0.0;

  const double alfa = viscals.alfa;
  core::inviscid::PsiLinInputs in;
  in.node = 0;
  in.xi = x;
  in.yi = y;
  in.nodeOnAirfoil = false;  // Fortran I=0: free point
  in.siglin = true;
  in.geolin = false;
  in.alfa = alfa;

  in.nxi = -1.0;
  in.nyi = 0.0;
  const double v = core::inviscid::psiLin(in, ctx).psiNi;
  in.nxi = 0.0;
  in.nyi = 1.0;
  const double u = core::inviscid::psiLin(in, ctx).psiNi;

  VelocityAtPointResult out;
  out.u = u;
  out.v = v;
  const double sum = u * u + v * v;
  out.q = std::sqrt(sum);
  out.cp = 1.0 - sum;
  return core::Result<VelocityAtPointResult>::ok(out);
}

// ---
// OPER file export projections (CPWR/DUMP/DMP). The encoders in
// io/ExportFormats carry the canonical format semantics; here only
// the existing session state is mapped onto the encoder inputs
// (1-based Fortran indices with PLACEHOLDER entry as in PLXADD).
// ---
namespace {

// Fortran COMMON field with 1-based indexing: index 0 is
// a placeholder, the values follow 0-based from the session state.
std::vector<double> withFortranPlaceholder(const std::vector<double>& values) {
  std::vector<double> out;
  out.reserve(values.size() + 1);
  out.push_back(0.0);
  out.insert(out.end(), values.begin(), values.end());
  return out;
}

std::vector<int> ipanWithFortranPlaceholder(
    const std::vector<int>& zeroBasedPanelIndices) {
  std::vector<int> out;
  out.reserve(zeroBasedPanelIndices.size() + 1);
  out.push_back(0);
  for (const int index : zeroBasedPanelIndices) {
    out.push_back(index + 1);
  }
  return out;
}

}  // namespace

io::PressureDumpInput currentPressureDumpInput(
    const SessionComposition& session) {
  // CPDUMP (xoper.f 2265-2346): N lines X(I) against the
  // compressibility-corrected Cp from GAM (COMMON). Without an
  // analyzed point GAM contains the COMMON zero state.
  io::PressureDumpInput input;
  const auto& panel = session.paneling.panel;
  input.n = static_cast<int>(panel.x.size());
  input.delimiter = static_cast<io::DumpDelimiter>(
      session.geometry.delimiter == 0 ? 0
      : session.geometry.delimiter == 1 ? 1
      : session.geometry.delimiter == 2 ? 2 : 3);
  input.minf = session.viscous.viscals.minf;
  input.qinf = session.operating.qinf;
  input.x = panel.x;
  input.gam = session.viscous.viscals.gam;
  if (input.gam.size() != panel.x.size()) {
    input.gam.assign(panel.x.size(), 0.0);
  }
  return input;
}

io::BoundaryLayerDumpInput currentBoundaryLayerDumpInput(
    const SessionComposition& session) {
  // BLDUMP/BLDUMP2 (xoper.f 1892-2262): airfoil and wake lines from
  // COMMON GAM plus BL station fields; LIPAN/LVISC/LWAKE as
  // taken faithfully from the session state.
  io::BoundaryLayerDumpInput input;
  const auto& panel = session.paneling.panel;
  const auto& viscals = session.viscous.viscals;
  const auto& wake = session.viscous.wake;
  input.n = static_cast<int>(panel.x.size());
  input.nw = static_cast<int>(wake.nw);
  input.delimiter = static_cast<io::DumpDelimiter>(
      session.geometry.delimiter == 0 ? 0
      : session.geometry.delimiter == 1 ? 1
      : session.geometry.delimiter == 2 ? 2 : 3);
  input.lipan = session.paneling.panelingValid;
  input.lvisc = session.operating.viscous;
  // Fortran LWAKE is set only by XYWAKE inside VISCAL
  // (xpanel.f 1346, call xoper.f 2904); the inviscid path does not
  // run through VISCAL. lastPointWakeValid models exactly this state.
  input.lwake = session.viscous.lastPointWakeValid;
  input.minf = viscals.minf;
  input.qinf = session.operating.qinf;
  input.gamm1 = 0.4;  // Fortran GAMM1 (Air)
  input.reinf = viscals.reinf;
  input.s = panel.s;
  input.x = panel.x;
  input.y = panel.y;
  input.s.insert(input.s.end(), wake.s.begin(), wake.s.end());
  input.x.insert(input.x.end(), wake.x.begin(), wake.x.end());
  input.y.insert(input.y.end(), wake.y.begin(), wake.y.end());
  input.gam = viscals.gam;
  if (input.gam.size() != panel.x.size()) {
    input.gam.assign(panel.x.size(), 0.0);
  }
  for (int side = 0; side < 2; ++side) {
    const auto& map = side == 0 ? viscals.mapping.top : viscals.mapping.bottom;
    input.iblte[static_cast<std::size_t>(side)] = map.iblte + 1;
    input.nbl[static_cast<std::size_t>(side)] = map.nbl + 1;
    input.ipan[static_cast<std::size_t>(side)] =
        ipanWithFortranPlaceholder(map.ipan);
    const auto& arcs = side == 0 ? viscals.arcs.top : viscals.arcs.bottom;
    input.xssi[static_cast<std::size_t>(side)] = withFortranPlaceholder(arcs);
    const auto& dstr = side == 0 ? viscals.dstr.top : viscals.dstr.bottom;
    const auto& thet = side == 0 ? viscals.thet.top : viscals.thet.bottom;
    const auto& tstr = side == 0 ? viscals.tstr.top : viscals.tstr.bottom;
    const auto& tau = side == 0 ? viscals.tau.top : viscals.tau.bottom;
    const auto& dis = side == 0 ? viscals.dis.top : viscals.dis.bottom;
    const auto& uedg = side == 0 ? viscals.uedg.top : viscals.uedg.bottom;
    const auto& ctau = side == 0 ? viscals.ctau.top : viscals.ctau.bottom;
    const auto& dssr = side == 0 ? viscals.dssr.top : viscals.dssr.bottom;
    input.dstr[static_cast<std::size_t>(side)] = withFortranPlaceholder(dstr);
    input.thet[static_cast<std::size_t>(side)] = withFortranPlaceholder(thet);
    input.tstr[static_cast<std::size_t>(side)] = withFortranPlaceholder(tstr);
    input.tau[static_cast<std::size_t>(side)] = withFortranPlaceholder(tau);
    input.dis[static_cast<std::size_t>(side)] = withFortranPlaceholder(dis);
    input.uedg[static_cast<std::size_t>(side)] = withFortranPlaceholder(uedg);
    input.ctau[static_cast<std::size_t>(side)] = withFortranPlaceholder(ctau);
    input.dssr[static_cast<std::size_t>(side)] = withFortranPlaceholder(dssr);
  }
  return input;
}

// ---: MDES menu commands --------------------------------------------------

namespace {

// The numerical INIT state (xmdes.f label 1) must exist before QSET/AQ/CQ/SYMM/
// FILT/SMOO/TGAP/TANG; the MDES menu canonically only runs
// after label 1 (xmdes.f 60-107).
bool mdesNumericsReady(const SessionComposition& session) {
  const auto& inv = session.inverse;
  return inv.validity.grid == CircleGridValidity::initialized &&
         inv.validity.mapping ==
             MappingCoordinateValidity::initializedFromPanel &&
         inv.shared.nsp > 0 && !inv.shared.circle.grid.wc.empty();
}

// Non-mutating view of the shared state for the core::inverse kernels
// (FullInverseState without panel part; qCCalc/qspIntegrate need
// exclusively circle/minf/qinf/sspec).
core::inverse::FullInverseState mdesSharedView(
    const SessionComposition& session) {
  core::inverse::FullInverseState fs;
  fs.circle = session.inverse.shared.circle;
  fs.minf = session.viscous.viscals.minf;
  fs.qinf = session.operating.qinf;
  fs.qgamm = session.inverse.shared.qgamm;
  fs.sspec = session.inverse.shared.sspec;
  fs.ssple = session.inverse.shared.ssple;
  fs.nsp = session.inverse.shared.nsp;
  return fs;
}

// QSPCIR (xmdes.f 771-789) over all active columns; sets
// LQSPEC=.TRUE. (here: validity present/currentQ).
void mdesQspecCircAll(core::inverse::FullInverseState& fs,
                      std::vector<core::inverse::QSpecDistribution>& qspecs,
                      int iacqsp) {
  for (auto& d : qspecs) {
    (void)core::inverse::qSpecCirc(fs, d, iacqsp);
  }
}

// QSPINT follow-up pass after FILT/SMOO (xmdes.f 489-497/457-465):
// CMQSP(KQSP) always from QSPINT, CLQSP(KQSP) only for IACQSP=1.
void mdesQspintPass(const core::inverse::FullInverseState& fs,
                    std::vector<core::inverse::QSpecDistribution>& qspecs,
                    int iacqsp, MdesSpectrumReport& report) {
  for (std::size_t k = 0; k < qspecs.size(); ++k) {
    auto& d = qspecs[k];
    const auto sp = core::inverse::qspIntegrate(fs.circle, d.al, d.q,
                                                fs.qinf, fs.minf);
    if (sp.isErr()) {
      continue;
    }
    d.cm = sp.value().cm;
    if (iacqsp == 1) {
      d.cl = sp.value().cl;
    }
    report.columns.push_back(
        MdesQspecColumnReport{static_cast<int>(k) + 1, d.al, d.cl, d.cm});
  }
}

}  // namespace

MdesCommandStatus mdesQspecReset(SessionComposition& session) {
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  auto& inv = session.inverse;
  auto fs = mdesSharedView(session);
  auto st = core::inverse::calcCoefficients(fs.circle, fs.qgamm, false);
  if (st.isErr()) {
    return MdesCommandStatus::invalidState;
  }
  if (inv.flags.lqSym) {
    core::inverse::coeffSymmetrize(fs.circle.coeff);
  }
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  mdesQspecCircAll(fs, qspecs, inv.shared.iacqsp);
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return MdesCommandStatus::ok;
}

MdesCommandStatus mdesSetQspecAlphas(SessionComposition& session,
                                     const std::vector<double>& alphasRad) {
  if (alphasRad.empty()) {
    // Canonical <return> path (xmdes.f 236): no mutation.
    return MdesCommandStatus::ok;
  }
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  auto& inv = session.inverse;
  const int n = static_cast<int>(
      std::min(alphasRad.size(), core::qspecDistributionCapacity));
  auto fs = mdesSharedView(session);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  qspecs.resize(static_cast<std::size_t>(n));
  for (int k = 0; k < n; ++k) {
    qspecs[static_cast<std::size_t>(k)].al =
        alphasRad[static_cast<std::size_t>(k)];
  }
  inv.shared.iacqsp = 1;
  mdesQspecCircAll(fs, qspecs, inv.shared.iacqsp);
  inv.shared.nqsp = n;
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return MdesCommandStatus::ok;
}

MdesCommandStatus mdesSetQspecCls(SessionComposition& session,
                                  const std::vector<double>& cls) {
  if (cls.empty()) {
    return MdesCommandStatus::ok;
  }
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  auto& inv = session.inverse;
  const int n = static_cast<int>(
      std::min(cls.size(), core::qspecDistributionCapacity));
  auto fs = mdesSharedView(session);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  qspecs.resize(static_cast<std::size_t>(n));
  for (int k = 0; k < n; ++k) {
    qspecs[static_cast<std::size_t>(k)].cl = cls[static_cast<std::size_t>(k)];
  }
  inv.shared.iacqsp = 2;
  mdesQspecCircAll(fs, qspecs, inv.shared.iacqsp);
  inv.shared.nqsp = n;
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return MdesCommandStatus::ok;
}

MdesCommandStatus mdesToggleSymmetry(SessionComposition& session,
                                     bool& newState) {
  auto& inv = session.inverse;
  newState = !inv.flags.lqSym;
  if (!newState) {
    inv.flags.lqSym = false;
    return MdesCommandStatus::ok;
  }
  if (!mdesNumericsReady(session)) {
    inv.flags.lqSym = false;
    newState = false;
    return MdesCommandStatus::notInitialized;
  }
  auto fs = mdesSharedView(session);
  core::inverse::coeffSymmetrize(fs.circle.coeff);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  mdesQspecCircAll(fs, qspecs, inv.shared.iacqsp);
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qspecs = std::move(qspecs);
  inv.flags.lqSym = true;
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return MdesCommandStatus::ok;
}

MdesCommandStatus mdesSetTeGap(SessionComposition& session, double dx,
                               double dy) {
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  // DZTSET (xmdes.f 733-753): exclusively DZTE; no further
  // mapping/coefficient mutations.
  session.inverse.shared.circle.dzte = std::complex<double>(dx, dy);
  return MdesCommandStatus::ok;
}

MdesCommandStatus mdesSetTeAngle(SessionComposition& session,
                                 double angleDeg) {
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  // AGTSET (xmdes.f 756-770): AGTE = degrees/180.
  session.inverse.shared.circle.agte = angleDeg / 180.0;
  return MdesCommandStatus::ok;
}

bool mdesToggleQvisOverlay(SessionComposition& session) {
  session.inverse.flags.lqVdes = !session.inverse.flags.lqVdes;
  return session.inverse.flags.lqVdes;
}

bool mdesToggleReflOverlay(SessionComposition& session) {
  session.inverse.flags.lqRefl = !session.inverse.flags.lqRefl;
  return session.inverse.flags.lqRefl;
}

bool mdesToggleSlopeMatch(SessionComposition& session) {
  session.inverse.flags.lqSlop = !session.inverse.flags.lqSlop;
  return session.inverse.flags.lqSlop;
}

MdesCommandStatus mdesApplyFilter(SessionComposition& session,
                                  MdesSpectrumReport& report) {
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  auto& inv = session.inverse;
  if (inv.shared.nqsp == 0 || inv.shared.qspecs.empty()) {
    return MdesCommandStatus::invalidState;
  }
  auto fs = mdesSharedView(session);
  // CNFILT with canonical fixed CFILT=0.2 (xmdes.f 477-478), PIQSUM,
  // QSPCIR.
  core::inverse::coeffFilter(fs.circle.coeff, 0.2);
  const auto st = core::inverse::piqSum(fs.circle.grid, fs.circle.coeff);
  if (st.isErr()) {
    return MdesCommandStatus::invalidState;
  }
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  mdesQspecCircAll(fs, qspecs, inv.shared.iacqsp);
  report = MdesSpectrumReport{};
  report.algam = inv.shared.algam;
  report.clgam = inv.shared.clgam;
  report.cmgam = inv.shared.cmgam;
  mdesQspintPass(fs, qspecs, inv.shared.iacqsp, report);
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return MdesCommandStatus::ok;
}

MdesCommandStatus mdesSmoothTargetSegment(SessionComposition& session,
                                          MdesSpectrumReport& report) {
  if (!mdesNumericsReady(session)) {
    return MdesCommandStatus::notInitialized;
  }
  auto& inv = session.inverse;
  const int kq = inv.shared.kqtarg;
  if (kq >= inv.shared.nqsp ||
      (kq >= 0 &&
       static_cast<std::size_t>(kq) >= inv.shared.qspecs.size())) {
    return MdesCommandStatus::invalidState;
  }
  auto fs = mdesSharedView(session);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  if (kq < 0) {
    // Canonical COMMON/CR07 alias: Fortran KQTARG=0 addresses
    // QSPEC(:,0), which exactly overlays the range QGAMM(IBX).
    // SMOO thus smooths QGAMM in place (xmdes.f 445-446), CNCALC
    // runs on the same smoothed values, QSPCIR writes the
    // actual columns 1..NQSP.
    core::inverse::QSpecDistribution d;
    d.s = fs.sspec;
    d.q = fs.qgamm;
    d.qp.assign(d.q.size(), 0.0);
    const auto stA = core::inverse::smooQ(d, inv.shared.target.iq1,
                                          inv.shared.target.iq2,
                                          inv.flags.lqSlop);
    if (stA.isErr()) {
      return MdesCommandStatus::invalidState;
    }
    fs.qgamm = std::move(d.q);
    const auto stB =
        core::inverse::calcCoefficients(fs.circle, fs.qgamm,
                                        inv.flags.lqSym);
    if (stB.isErr()) {
      return MdesCommandStatus::invalidState;
    }
  } else {
    auto& d = qspecs[static_cast<std::size_t>(kq)];
    const auto st1 = core::inverse::smooQ(d, inv.shared.target.iq1,
                                          inv.shared.target.iq2,
                                          inv.flags.lqSlop);
    if (st1.isErr()) {
      return MdesCommandStatus::invalidState;
    }
    const auto st2 =
        core::inverse::calcCoefficients(fs.circle, d.q, inv.flags.lqSym);
    if (st2.isErr()) {
      return MdesCommandStatus::invalidState;
    }
  }
  mdesQspecCircAll(fs, qspecs, inv.shared.iacqsp);
  report = MdesSpectrumReport{};
  report.algam = inv.shared.algam;
  report.clgam = inv.shared.clgam;
  report.cmgam = inv.shared.cmgam;
  mdesQspintPass(fs, qspecs, inv.shared.iacqsp, report);
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qgamm = std::move(fs.qgamm);
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return MdesCommandStatus::ok;
}

MdesReadStatus mdesReadVelocityDistribution(
    SessionComposition& session,
    const core::export_data::VelocityRatioTable& table) {
  if (!mdesNumericsReady(session)) {
    return MdesReadStatus::notInitialized;
  }
  auto& inv = session.inverse;
  if (inv.shared.qspecs.empty()) {
    return MdesReadStatus::invalidTable;
  }
  auto fs = mdesSharedView(session);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  // KQSP=1 (xmdes.f 417): exclusively the first column.
  auto& d = qspecs[0];
  d.s = fs.sspec;
  const auto stResample = core::export_data::resampleVelocityRatioTable(
      table, fs.qinf, session.viscous.viscals.tklam, d);
  if (stResample.isErr()) {
    return MdesReadStatus::invalidTable;
  }
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::fileRead;
  return MdesReadStatus::ok;
}

MdesReadPostStatus mdesReadPostProcess(SessionComposition& session,
                                       MdesSpectrumReport& report) {
  if (!mdesNumericsReady(session)) {
    return MdesReadPostStatus::notInitialized;
  }
  auto& inv = session.inverse;
  if (inv.shared.qspecs.empty()) {
    return MdesReadPostStatus::notInitialized;
  }
  auto fs = mdesSharedView(session);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  auto& d = qspecs[0];
  const auto stCn = core::inverse::calcCoefficients(fs.circle, d.q, false);
  if (stCn.isErr()) {
    return MdesReadPostStatus::coreError;
  }
  if (inv.flags.lqSym) {
    core::inverse::coeffSymmetrize(fs.circle.coeff);
  }
  // QSPINT (xmdes.f 430-431): CLQSP(1) and CMQSP(1) are both
  // updated (no IACQSP reservation as for FILT/SMOO).
  const auto sp =
      core::inverse::qspIntegrate(fs.circle, d.al, d.q, fs.qinf, fs.minf);
  if (sp.isErr()) {
    return MdesReadPostStatus::coreError;
  }
  d.cl = sp.value().cl;
  d.cm = sp.value().cm;
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  report = MdesSpectrumReport{};
  report.algam = inv.shared.algam;
  report.clgam = inv.shared.clgam;
  report.cmgam = inv.shared.cmgam;
  report.columns.push_back(MdesQspecColumnReport{1, d.al, d.cl, d.cm});
  return MdesReadPostStatus::ok;
}

MdesDumpStatus currentMdesCnDumpData(const SessionComposition& session,
                                     MdesDumpData& data) {
  const auto& circle = session.inverse.shared.circle;
  if (session.inverse.validity.grid != CircleGridValidity::initialized ||
      circle.grid.nc <= 0) {
    return MdesDumpStatus::notInitialized;
  }
  const int nc = circle.grid.nc;
  const int mc = circle.grid.mc;
  std::vector<double> cnRe(static_cast<std::size_t>(mc) + 1);
  std::vector<double> cnIm(static_cast<std::size_t>(mc) + 1);
  for (int m = 0; m <= mc; ++m) {
    const auto& c = circle.coeff.cn[static_cast<std::size_t>(m)];
    cnRe[static_cast<std::size_t>(m)] = c.real();
    cnIm[static_cast<std::size_t>(m)] = c.imag();
  }
  // The projection kernel holds PIQ 1-based (Fortran PIQ(1..NC));
  // the session core stores PIQ(1..NC) in piq[0..nc-1].
  std::vector<double> piqRe(static_cast<std::size_t>(nc) + 1);
  std::vector<double> piqIm(static_cast<std::size_t>(nc) + 1);
  for (int m = 0; m < nc; ++m) {
    const auto& p = circle.coeff.piq[static_cast<std::size_t>(m)];
    piqRe[static_cast<std::size_t>(m) + 1] = p.real();
    piqIm[static_cast<std::size_t>(m) + 1] = p.imag();
  }
  const auto rows = core::export_data::circleCoefficientDumpRows(
      cnRe, cnIm, piqRe, piqIm, mc, nc);
  if (rows.isErr()) {
    return MdesDumpStatus::notInitialized;
  }
  data.rows = rows.value();
  return MdesDumpStatus::available;
}

QdesNumericsInitResult initializeQdesNumerics(SessionComposition& session) {
  QdesNumericsInitResult result;
  if (!session.paneling.panelingValid) {
    result.status = QdesNumericsInitStatus::noAirfoil;
    result.message = "QDES entry: no valid panel contour";
    return result;
  }
  // xqdes.f 65: SPECAL for the current Alfa (confirmed point kernel).
  // QDES calls exclusively SPECAL, never VISCAL; the Q capture comes from
  // the inviscid GAM. The confirmed point kernel is therefore run with an
  // inviscid configuration, without permanently changing the session
  // configuration.
  const double alfaRad = session.viscous.viscals.alfa;
  const bool viscousConfig = session.operating.viscous;
  session.operating.viscous = false;
  const AnalysisResult point = runSpecAlpha(
      session, Degrees{alfaRad * 180.0 / std::numbers::pi}, nullptr);
  session.operating.viscous = viscousConfig;
  if (point.status != AnalysisStatus::converged &&
      point.status != AnalysisStatus::unconverged) {
    result.status = QdesNumericsInitStatus::coreError;
    result.message = "SPECAL: analysis not available";
    return result;
  }
  // Chronological point diagnostics of the executed SPECAL run:
  // GGCALC event only for an actual ggCalc run (cache invalid),
  // OFS values from the real SpecifiedPointSnapshot; no VISCAL.
  for (const auto& ev : session.result.lastDiagnostics.events) {
    if (ev.kind == OperPointEventKind::influenceSystemCalculation ||
        ev.kind == OperPointEventKind::specifiedPointOutput) {
      result.pointEvents.push_back(ev);
    }
  }
  result.specalSnapshot = session.result.lastDiagnostics.specalSnapshot;
  auto& inv = session.inverse;
  auto fs = mdesSharedView(session);
  const auto& panel = session.paneling.panel;
  fs.panel.x = panel.x;
  fs.panel.xp = panel.xp;
  fs.panel.y = panel.y;
  fs.panel.yp = panel.yp;
  fs.panel.s = panel.s;
  fs.panel.n = static_cast<int>(panel.x.size());
  fs.panel.sle = panel.sle;
  fs.panel.xle = panel.xle;
  fs.panel.yle = panel.yle;
  fs.panel.xte = panel.xte;
  fs.panel.yte = panel.yte;
  const auto st = core::inverse::captureCurrentSpeed(fs,
                                                     session.viscous.viscals.gam);
  if (st.isErr()) {
    result.status = QdesNumericsInitStatus::coreError;
    result.message = "Q capture: core error";
    return result;
  }
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  int iq1 = inv.shared.target.iq1;
  int iq2 = inv.shared.target.iq2;
  if (inv.validity.qspec != QSpecValidity::present || qspecs.empty()) {
    // xqdes.f 96-102: NQSP=1, KQTARG=1, GAMQSP(1).
    qspecs.assign(1, core::inverse::QSpecDistribution{});
    qspecs[0].s = fs.sspec;
    qspecs[0].q.assign(fs.sspec.size(), 0.0);
    qspecs[0].qp.assign(fs.sspec.size(), 0.0);
    inv.shared.nqsp = 1;
    inv.shared.kqtarg = 0;
    const auto stG = core::inverse::gamQSpec(
        qspecs[0], fs.qgamm, alfaRad, point.liftCoefficient,
        point.momentCoefficient, inv.shared.target.liqset, iq1, iq2);
    if (stG.isErr()) {
      result.status = QdesNumericsInitStatus::coreError;
      result.message = "GAMQSP: core error";
      return result;
    }
    result.qspecRebuilt = true;
  }
  inv.shared.circle = std::move(fs.circle);
  inv.shared.qgamm = std::move(fs.qgamm);
  inv.shared.sspec = std::move(fs.sspec);
  inv.shared.xspoc = std::move(fs.xspoc);
  inv.shared.yspoc = std::move(fs.yspoc);
  inv.shared.ssple = fs.ssple;
  inv.shared.nsp = fs.nsp;
  inv.shared.qspecs = std::move(qspecs);
  inv.shared.target.iq1 = iq1;
  inv.shared.target.iq2 = iq2;
  inv.shared.algam = alfaRad;
  inv.shared.clgam = point.liftCoefficient;
  inv.shared.cmgam = point.momentCoefficient;
  if (result.qspecRebuilt) {
    inv.validity.qspec = QSpecValidity::present;
    inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  }
  inv.provenance.sourcePanelRevision = session.paneling.panelSourceRevision;
  result.status = QdesNumericsInitStatus::captured;
  return result;
}

QdesCommandStatus qdesQspecReset(SessionComposition& session) {
  auto& inv = session.inverse;
  if (inv.shared.qspecs.empty() || inv.shared.nsp <= 0) {
    return QdesCommandStatus::notInitialized;
  }
  auto fs = mdesSharedView(session);
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  int iq1 = inv.shared.target.iq1;
  int iq2 = inv.shared.target.iq2;
  const auto st = core::inverse::gamQSpec(
      qspecs[0], fs.qgamm, inv.shared.algam, inv.shared.clgam,
      inv.shared.cmgam, inv.shared.target.liqset, iq1, iq2);
  if (st.isErr()) {
    return QdesCommandStatus::coreError;
  }
  inv.shared.target.iq1 = iq1;
  inv.shared.target.iq2 = iq2;
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::currentQ;
  return QdesCommandStatus::ok;
}

QdesCommandStatus qdesSmoothTargetSegment(SessionComposition& session,
                                          QdesForceReport& report) {
  auto& inv = session.inverse;
  if (inv.shared.qspecs.empty() || inv.shared.nsp <= 0) {
    return QdesCommandStatus::notInitialized;
  }
  std::vector<core::inverse::QSpecDistribution> qspecs = inv.shared.qspecs;
  auto& d = qspecs[0];  // KQSP=1 (xqdes.f 282)
  const auto st1 = core::inverse::smooQ(d, inv.shared.target.iq1,
                                        inv.shared.target.iq2,
                                        inv.flags.lqSlop);
  if (st1.isErr()) {
    return QdesCommandStatus::coreError;
  }
  const auto st2 = core::inverse::splQSpec(d);
  if (st2.isErr()) {
    return QdesCommandStatus::coreError;
  }
  // CLCALC (xqdes.f 293-296): GAM_A = COMMON scratch W1; GAM_A flows
  // only in Cl_ALF/Cl_MSQ (xfoil.f CLCALC), not in Cl/CM. As
  // GAM_A is handed over as QSPECP here - the Cl/CM values are
  // canonically independent of it.
  const auto& panel = session.paneling.panel;
  const auto load = core::inviscid::clCalc(
      panel.x, panel.y, d.q, d.qp, session.viscous.viscals.alfa,
      session.viscous.viscals.minf, session.operating.qinf,
      session.operating.momentReferenceX, session.operating.momentReferenceY);
  d.cl = load.cl;
  d.cm = load.cm;
  inv.shared.qspecs = std::move(qspecs);
  inv.validity.qspec = QSpecValidity::present;
  inv.validity.qspecOrigin = QSpecOrigin::menuEdits;
  report.cl = session.result.lastResult.liftCoefficient;
  report.cm = session.result.lastResult.momentCoefficient;
  report.clq = d.cl;
  report.cmq = d.cm;
  return QdesCommandStatus::ok;
}

bool qdesToggleCpxxConstraint(SessionComposition& session) {
  session.inverse.flags.lcpxx = !session.inverse.flags.lcpxx;
  return session.inverse.flags.lcpxx;
}

// ---: commit and boundary contracts ----------------------------

MdesExecuteResult executeMdesDesign(SessionComposition& session) {
  MdesExecuteResult result;
  auto& inv = session.inverse;
  if (!session.paneling.panelingValid ||
      inv.validity.mapping !=
          MappingCoordinateValidity::initializedFromPanel ||
      inv.validity.qspec != QSpecValidity::present ||
      inv.shared.qspecs.empty()) {
    result.status = MdesExecuteStatus::notInitialized;
    result.message = "MDES EXEC: inverse state not initialized";
    return result;
  }
  // MAPGEN on the shared circle state (kernel mutates CN/PIQ/ZC/SC).
  core::inverse::CircleMappingState circle = inv.shared.circle;
  std::vector<double> x;
  std::vector<double> y;
  const auto st = core::inverse::mapGenerate(circle, inv.shared.ffilt, x, y,
                                             &result.mapgen);
  if (st.isErr()) {
    result.status = MdesExecuteStatus::coreError;
    result.message = "MAPGEN: core error";
    return result;
  }
  // Commit: exclusively buffer (XB/YB/SB/XBP/YBP, NB=NC) and
  // the shared circle state; panel contour, analysis and wake remain
  // canonically unchanged (xmdes.f 619-699).
  inv.shared.circle = std::move(circle);
  session.geometry.buffer =
      core::geometry::AirfoilBuffer(std::move(x), std::move(y),
                                    session.geometry.buffer.name);
  ++session.geometry.bufferRevision;
  session.design.lgSame = false;
  result.bufferPointCount = session.geometry.buffer.x.size();
  // xmdes.f 622-628: SCALC/SPLIND (SPLIND edge -999) + GEOPAR on the
  // new buffer - the same canonical sequence, whose GEOPAR result
  // carries both state (buffer metrics) and text (Format 1000).
  const auto& bx = session.geometry.buffer.x;
  const auto& by = session.geometry.buffer.y;
  std::vector<double> bs(bx.size());
  std::vector<double> bxp(bx.size());
  std::vector<double> byp(bx.size());
  core::math::scalc(bx, by, bs);
  core::math::splind(bx, bs, bxp, -999.0, -999.0);
  core::math::splind(by, bs, byp, -999.0, -999.0);
  result.metrics = core::geometry::geopar(bx, bxp, by, byp, bs);
  result.metricsValid = true;
  result.status = MdesExecuteStatus::ok;
  return result;
}

SessionOperationStatus mdesPerturbPrepareCoefficients(
    SessionComposition& session) {
  SessionOperationStatus status;
  auto& inv = session.inverse;
  if (!session.paneling.panelingValid ||
      inv.validity.mapping !=
          MappingCoordinateValidity::initializedFromPanel ||
      inv.validity.qspec != QSpecValidity::present ||
      inv.shared.qspecs.empty()) {
    status.ok = false;
    status.message = "MDES PERT: inverse state not initialized";
    return status;
  }
  core::inverse::CircleMappingState circle = inv.shared.circle;
  const auto st = core::inverse::calcCoefficients(circle,
                                                  inv.shared.qspecs[0].q,
                                                  false);
  if (st.isErr()) {
    status.ok = false;
    status.message = "CNCALC: core error";
    return status;
  }
  inv.shared.circle = std::move(circle);
  return status;
}

MdesPerturbResult mdesPerturbCoefficientAndExecute(
    SessionComposition& session, int m, double dcnr, double dcni) {
  MdesPerturbResult result;
  auto& inv = session.inverse;
  if (!session.paneling.panelingValid ||
      inv.validity.mapping !=
          MappingCoordinateValidity::initializedFromPanel ||
      inv.validity.qspec != QSpecValidity::present ||
      inv.shared.qspecs.empty()) {
    result.status = MdesPerturbStatus::notInitialized;
    result.message = "MDES PERT: inverse state not initialized";
    return result;
  }
  core::inverse::CircleMappingState circle = inv.shared.circle;
  const auto st = core::inverse::perturbCoefficient(circle, m, dcnr, dcni);
  if (st.isErr()) {
    result.status = MdesPerturbStatus::invalidIndex;
    result.message = st.error().message;
    return result;
  }
  // Canonical order: CN is already perturbed (Fortran never rolls
  // back). PERT first runs the same geometry/TE-gap
  // Newton as MAPGEN (kernel mapGenerate, NCN=1; iteration records and
  // convergence status are returned canonically). Execution must then
  // continue through the EXEC path with a renewed MAPGEN (COMAND='EXEC').
  std::vector<double> px;
  std::vector<double> py;
  const auto stMap = core::inverse::mapGenerate(circle, inv.shared.ffilt, px,
                                                py, &result.newton);
  if (stMap.isErr()) {
    // CN perturbation stays canonically preserved (no rollback).
    inv.shared.circle = std::move(circle);
    result.coefficientPerturbed = true;
    result.status = MdesPerturbStatus::coreError;
    result.message = "PERT: core error in the geometry/gap Newton";
    return result;
  }
  // xmdes.f 702-708: after PERT follows CALL QSPCIR (over all
  // NQSP columns, QCCALC In/Out + SPLQSP), only then COMAND='EXEC'.
  // QSPCIR writes the target-Q columns from the changed CN state
  // further; without this step a second PERT would compute with an outdated
  // QSPEC.
  inv.shared.circle = std::move(circle);
  {
    const auto fs = mdesSharedView(session);
    for (auto& d : inv.shared.qspecs) {
      (void)core::inverse::qSpecCirc(fs, d, inv.shared.iacqsp);
    }
  }
  result.coefficientPerturbed = true;
  result.execute = executeMdesDesign(session);
  switch (result.execute.status) {
    case MdesExecuteStatus::ok:
      result.status = MdesPerturbStatus::ok;
      break;
    case MdesExecuteStatus::coreError:
      result.status = MdesPerturbStatus::coreError;
      result.message = result.execute.message;
      break;
    case MdesExecuteStatus::notInitialized:
      result.status = MdesPerturbStatus::notInitialized;
      result.message = result.execute.message;
      break;
  }
  return result;
}

QdesExecuteResult executeQdesMixedDesign(SessionComposition& session,
                                         int niterq) {
  QdesExecuteResult result;
  auto& inv = session.inverse;
  const auto& panel = session.paneling.panel;
  const int n = static_cast<int>(panel.x.size());
  if (!session.paneling.panelingValid || n <= 0) {
    result.status = QdesExecuteStatus::notInitialized;
    result.message = "QDES EXEC: no valid panel contour";
    return result;
  }
  if (!inv.shared.target.liqset) {
    // xqdes.f 411-414: canonical refusal without any mutation.
    result.status = QdesExecuteStatus::targetNotMarked;
    return result;
  }
  if (inv.shared.qspecs.empty() ||
      static_cast<int>(inv.shared.qspecs[0].q.size()) != n) {
    result.status = QdesExecuteStatus::notInitialized;
    result.message = "QDES EXEC: target Q does not fit the current contour";
    return result;
  }
  // xqdes.f 416-427: stagnation point check in the target segment (QGAMM sign).
  for (int i = inv.shared.target.iq1; i < inv.shared.target.iq2; ++i) {
    if (inv.shared.qgamm[static_cast<std::size_t>(i)] >= 0.0 &&
        inv.shared.qgamm[static_cast<std::size_t>(i + 1)] < 0.0) {
      result.status = QdesExecuteStatus::stagnationInTarget;
      return result;
    }
  }
  if (n + 5 > core::inverse::kMixedIqx) {
    result.status = QdesExecuteStatus::coreError;
    result.message = "QDES EXEC: capacity limit IQX exceeded";
    return result;
  }
  // Candidate: MIXED mutates x/y/s/gam locally; the commit happens only
  // after a successful kernel run.
  core::inverse::MixedInverseState ms;
  ms.x = panel.x;
  ms.y = panel.y;
  ms.s = panel.s;
  ms.xp = panel.xp;
  ms.yp = panel.yp;
  ms.nx = panel.xn;
  ms.ny = panel.yn;
  ms.apanel = panel.apanel;
  ms.gam = session.viscous.viscals.gam;
  ms.gamA = session.viscous.viscals.gamA;
  // GAMU1/GAMU2/SIG: inviscid base state (SIG=0, GAMU from the
  // influence system, provided quantity-consistent; otherwise zero).
  const auto& sys = session.viscous.system;
  ms.gamu1 = sys.gamu1.size() == static_cast<std::size_t>(n)
                 ? std::vector<double>(sys.gamu1.begin(), sys.gamu1.end())
                 : std::vector<double>(static_cast<std::size_t>(n), 0.0);
  ms.gamu2 = sys.gamu2.size() == static_cast<std::size_t>(n)
                 ? std::vector<double>(sys.gamu2.begin(), sys.gamu2.end())
                 : std::vector<double>(static_cast<std::size_t>(n), 0.0);
  ms.sig.assign(static_cast<std::size_t>(n), 0.0);
  ms.qf0.assign(static_cast<std::size_t>(n), 0.0);
  ms.qf1 = ms.qf0;
  ms.qf2 = ms.qf0;
  ms.qf3 = ms.qf0;
  ms.qspec = inv.shared.qspecs[0].q;
  ms.n = n;
  ms.iq1 = inv.shared.target.iq1;
  ms.iq2 = inv.shared.target.iq2;
  ms.lcpXX = inv.flags.lcpxx;
  ms.sharp = panel.sharpTe;
  ms.xte = panel.xte;
  ms.yte = panel.yte;
  ms.chord = panel.chord;
  const auto last = static_cast<std::size_t>(n - 1);
  const auto teg = core::paneling::teGeometry(
      panel.x[0], panel.y[0], panel.xp[0], panel.yp[0], panel.x[last],
      panel.y[last], panel.xp[last], panel.yp[last], panel.chord);
  ms.dste = teg.dste;
  ms.ante = teg.ante;
  ms.aste = teg.aste;
  ms.alfa = session.viscous.viscals.alfa;
  ms.minf = session.viscous.viscals.minf;
  ms.qinf = session.operating.qinf;
  ms.psio = inv.shared.psio;
  ms.qdof0 = inv.shared.qdof0;
  ms.qdof1 = inv.shared.qdof1;
  ms.qdof2 = inv.shared.qdof2;
  ms.qdof3 = inv.shared.qdof3;
  const auto st = core::inverse::mixedInverse(ms, niterq);
  if (st.isErr()) {
    // kernel error: no partial commit (Fortran crash path controlled).
    result.status = QdesExecuteStatus::coreError;
    result.message = std::string("MIXED: ") + st.error().message;
    return result;
  }
  // Save Current->Buffer (xqdes.f 431-439): buffer = old contour, NB=N.
  const std::vector<double> oldX = panel.x;
  const std::vector<double> oldY = panel.y;
  // Geometry post-processing SCALC/SPLIND/NCALC/LEFIND/TECALC/APCALC
  // via the confirmed abcopy kernel on the new contour.
  core::geometry::AirfoilBuffer newBuffer(ms.x, ms.y,
                                          session.geometry.buffer.name);
  auto paneling = core::paneling::abcopy(newBuffer);
  if (!paneling.isOk()) {
    result.status = QdesExecuteStatus::coreError;
    result.message = paneling.error().message;
    return result;
  }
  // Commit: new current contour; buffer = the saved old contour.
  session.geometry.buffer =
      core::geometry::AirfoilBuffer(oldX, oldY, session.geometry.buffer.name);
  ++session.geometry.bufferRevision;
  const std::uint64_t newBufferRev = session.geometry.bufferRevision;
  session.paneling.panel = std::move(paneling.value().panel);
  session.paneling.panelingValid = true;
  session.paneling.origin = PanelingOrigin::abcopy;
  session.paneling.panelSourceRevision = newBufferRev;
  ++session.paneling.panelRevision;
  session.geometry.metrics = metricsFromPanel(session);
  // Invalidations xqdes.f 494-503: LGAMU/LQINU (system), wake, DIJ,
  // BL mapping, LVCONV; LBLINI stays preserved (ccc branch).
  session.viscous.system = core::inviscid::InfluenceSystem{};
  session.viscous.systemValid = false;
  session.viscous.viscals.dij.clear();
  session.viscous.viscals.dijStride = 0;
  session.viscous.viscals.dijWakeValid = false;
  session.viscous.viscals.mapping.valid = false;
  session.viscous.viscals.lvconv = false;
  session.viscous.converged = false;
  session.viscous.wakeValid = false;
  session.viscous.lastPointContinuable = false;
  session.viscous.lastPointWakeValid = false;
  session.design.lgSame = false;
  // New inviscid surface state (xqdes.f 476-491).
  session.viscous.viscals.gam = ms.gam;
  session.viscous.viscals.qinv = ms.gam;
  // canonical QDES EXEC CPCALC call (xqdes.f:488):
  // CPCALC(N,QINV(=GAM),QINF,MINF,CPI) with the existing core kernel;
  // CPI is kept so the new contour has the same provenance as in Fortran.
  const auto cpiAfterQdes = core::inviscid::cpCalc(
      session.viscous.viscals.qinv, session.operating.qinf,
      session.viscous.viscals.minf);
  if (cpiAfterQdes.isOk()) {
    session.viscous.cpi = cpiAfterQdes.value();
  }
  // Re-capture of QGAMM/SSPEC/SSPLE/NSP and ALGAM=Alfa.
  const double snorm = session.paneling.panel.s.back();
  inv.shared.nsp = n;
  inv.shared.qgamm = ms.gam;
  inv.shared.sspec.assign(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    inv.shared.sspec[static_cast<std::size_t>(i)] =
        session.paneling.panel.s[static_cast<std::size_t>(i)] / snorm;
  }
  inv.shared.ssple = session.paneling.panel.sle / snorm;
  inv.shared.algam = session.viscous.viscals.alfa;
  // MIXED updates the COMMON fields.
  inv.shared.psio = ms.psio;
  inv.shared.qdof0 = ms.qdof0;
  inv.shared.qdof1 = ms.qdof1;
  inv.shared.qdof2 = ms.qdof2;
  inv.shared.qdof3 = ms.qdof3;
  // LSCINI=false (xqdes.f 501): the binding of the circle-level arc
  // length to the old contour is discarded; LQSPEC, LIQSET, IQ1/IQ2,
  // menu/redo states stay preserved.
  invalidateInverseStateAfterPanelChange(
      session, InversePanelChange::mixedInverseExec);
  if (session.inverse.validity.mapping ==
      MappingCoordinateValidity::neverInitialized) {
    // QDES entry never runs SCINIT; LSCINI=false is still
    // canonically set (xqdes.f 501).
    session.inverse.validity.mapping =
        MappingCoordinateValidity::staleAfterPanelChange;
  }
  result.converged = ms.converged;
  result.panelPointCount = session.paneling.panel.x.size();
  result.status = QdesExecuteStatus::ok;
  return result;
}

QdesRestoreResult restoreQdesCurrentFromBuffer(SessionComposition& session) {
  QdesRestoreResult result;
  auto& inv = session.inverse;
  const int n = static_cast<int>(session.paneling.panel.x.size());
  if (!session.paneling.panelingValid || n <= 0 || !session.geometry.valid ||
      static_cast<int>(session.geometry.buffer.x.size()) < n) {
    // NB < N: Fortran would read outdated XB stored values; not
    // deterministically representable.
    result.status = QdesRestoreStatus::bufferTooSmall;
    result.message = "REST: buffer contains fewer points than the contour";
    return result;
  }
  // X/Y <- XB/YB (first N buffer points), then SCALC/SPLIND/NCALC/
  // LEFIND/TECALC/APCALC via the confirmed abcopy kernel.
  std::vector<double> bx(session.geometry.buffer.x.begin(),
                         session.geometry.buffer.x.begin() + n);
  std::vector<double> by(session.geometry.buffer.y.begin(),
                         session.geometry.buffer.y.begin() + n);
  core::geometry::AirfoilBuffer truncated(std::move(bx), std::move(by),
                                          session.geometry.buffer.name);
  auto paneling = core::paneling::abcopy(truncated);
  if (!paneling.isOk()) {
    result.status = QdesRestoreStatus::coreError;
    result.message = paneling.error().message;
    return result;
  }
  // LEFIND event at the actual execution time of the
  // geometry post-processing (limitReached == Fortran warning branch).
  result.leadingEdgeNotFound =
      paneling.value().leStatus == core::geometry::LeStatus::limitReached;
  session.paneling.panel = std::move(paneling.value().panel);
  session.paneling.panelingValid = true;
  session.paneling.origin = PanelingOrigin::abcopy;
  session.paneling.panelSourceRevision = session.geometry.bufferRevision;
  ++session.paneling.panelRevision;
  session.geometry.metrics = metricsFromPanel(session);
  // xqdes.f 526-529: LGAMU/LQINU=false (system cache new), LGSAME=true.
  // LWAKE/LQAIJ/LADIJ/LWDIJ/LIPAN/LVCONV/LBLINI remain canonically
  // unchanged; analysis result and wake cache are not erased.
  session.viscous.system = core::inviscid::InfluenceSystem{};
  session.viscous.systemValid = false;
  session.design.lgSame = true;
  // LSCINI/LQSPEC/LIQSET/target segment/menu/redo: no change.
  (void)inv;
  result.pointCount = session.paneling.panel.x.size();
  result.status = QdesRestoreStatus::ok;
  return result;
}

}  // namespace xfoil::session

// End: src/session/SessionOps.cpp

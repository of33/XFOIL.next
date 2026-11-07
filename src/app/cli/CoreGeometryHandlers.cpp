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

// File: src/app/cli/CoreGeometryHandlers.cpp
// Session/core-based
// geometry/file handlers (see header).

#include "app/cli/CoreGeometryHandlers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "app/AirfoilWriter.h"
#include "app/DatFileReader.h"
#include "app/cli/CliFortranText.h"
#include "app/cli/CommandParser.h"
#include "app/cli/NumericArguments.h"
#include "xfoil/core/geometry/GeometryParameters.h"
#include "xfoil/core/geometry/GeometryTypes.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/core/paneling/Pangen.h"
#include "xfoil/io/AddedCamberCodec.h"
#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/io/MsesMultielementCodec.h"
#include "xfoil/io/OutputPathPolicy.h"
#include "xfoil/io/TextFormat.h"
#include "xfoil/io/ThicknessCamberCodec.h"
#include "xfoil/session/SessionOps.h"

namespace xfoil::app::cli {

namespace {

// Fortran PANGEN SHARP criterion (xfoil.f:2307): SHARP = DSTE < 0.0001*CHORD.
// DSTE is the TE gap (PangenResult.teGap); CHORD the chord length of the
// panel contour. No test-case value, but the canonical threshold.
constexpr double kSharpTrailingEdgeGapFactor = 0.0001;

// Domain-specific typed PANGEN output contract. The three callers have
// explicitly different visible contracts; an unclear
// boolean flag is forbidden. topLevelPane and nacaGeneration
// use PANGEN(.TRUE.) incl. the SHOPAR parameter table (format 1100),
// pparApply uses PANGEN(.FALSE.) without table; for this reason the
// CANG corner-angle line (FORMAT 1200).
enum class PangenOutputContext {
  topLevelPane,
  pparApply,
  nacaGeneration,
};

// CANG corner angle (only set in the pparApply context; computed by the
// handler via core::geometry::cang, only formatted here).
struct PangenCornerReport {
  bool present{false};
  double angle{0.0};
  int index{0};
  double x{0.0};
  double y{0.0};
};

// Shared PANGEN output: Newton-limit diagnostics + trailing-edge message
// (format 1090, SHARP = DSTE < 0.0001*CHORD). The context-dependent
// completion (SHOPAR table vs. CANG angle) is selected via the typed
// PangenOutputContext; no duplication of the diagnostics/TE lines.
void emitPangenOutput(std::ostream& out,
                      const session::PanelingApplyResult& paneled,
                      const session::SessionComposition& session,
                      PangenOutputContext context,
                      const PangenCornerReport& corner = {}) {
  emitPangenDiagnostics(out, paneled.pangen);
  const auto panelMetrics = session::geometryMetrics(session);
  const double chord = panelMetrics ? panelMetrics->chord : 0.0;
  const bool sharp =
      paneled.pangen.teGap < kSharpTrailingEdgeGapFactor * chord;
  emitTrailingEdge(out, sharp, paneled.pangen.teGap);
  switch (context) {
    case PangenOutputContext::topLevelPane:
    case PangenOutputContext::nacaGeneration: {
      const core::paneling::PanelingParameters& params =
          session::panelingParameters(session);
      emitPanelingParameters(out, params.panelCount, params.bunchingParam,
                             params.teToLeDensityRatio,
                             params.refinementDensityRatio,
                             params.upperRefinementRange[0],
                             params.upperRefinementRange[1],
                             params.lowerRefinementRange[0],
                             params.lowerRefinementRange[1]);
      break;
    }
    case PangenOutputContext::pparApply:
      if (corner.present) {
        emitMaxPanelCornerAngle(out, corner.angle, corner.index, corner.x,
                                corner.y);
      }
      break;
  }
}

}  // namespace

void handleNacaCore(session::SessionComposition& session, std::ostream& out,
                    int designation) {
  // Controlled original messages (as handleNaca / xfoil.f NACA).
  if (designation < 0) {
    out << " This designation not implemented.\n";
    return;
  }
  int itype = 0;
  if (designation <= 25099) {
    itype = 5;
  }
  if (designation <= 9999) {
    itype = 4;
  }
  if (itype == 0) {
    out << " This designation not implemented.\n";
    return;
  }
  if (itype == 5) {
    const int n5 = designation / 10000;
    const int n4 = (designation - n5 * 10000) / 1000;
    const int n3 = (designation - n5 * 10000 - n4 * 1000) / 100;
    const int n543 = 100 * n5 + 10 * n4 + n3;
    if (n543 < 210 || n543 > 250 || n543 % 10 != 0) {
      out << " Illegal 5-digit designation\n"
          << " First three digits must be 210, 220, ... 250\n";
      return;
    }
  }

  const auto naca = core::geometry::nacaAirfoil(designation);
  if (!naca.isOk()) {
    out << " This designation not implemented.\n";
    return;
  }

  // Transactional: preview PANGEN on the candidates buffer (pure
  // core function core::paneling::pangen, no session mutation). A failing
  // PANGEN leaves the complete session state (buffer, panel, name,
  // revisions, analysis) untouched; it does not create a new buffer for
  // the simultaneously old or deleted panel. The commit below uses
  // the same parameters on the same buffer, so applyPaneling afterwards
  // succeeds.
  const auto preview = core::paneling::pangen(naca.value().buffer,
                                              session.paneling.parameters);
  if (!preview.isOk()) {
    // Canonical capacity/PANGEN diagnostics; no partial mutation.
    emitPangenTooManyPanels(out);
    return;
  }

  // Success path: text in Fortran order (GEOPAR Format 1000 before Format
  // 1200 before PANGEN output) and atomic commit of buffer and panel.
  const core::geometry::GeometryMetrics& bufferMetrics = naca.value().metrics;
  emitThicknessCamber(out, bufferMetrics.thick, bufferMetrics.thickX,
                      bufferMetrics.cambr, bufferMetrics.cambrX);

  const auto set = session::setBufferGeometry(session, naca.value().buffer);
  if (!set.success()) {
    // Controlled error; existing state stays unchanged.
    return;
  }
  emitBufferAirfoilSet(out, static_cast<int>(naca.value().buffer.x.size()));

  // Fortran NACA calls PANGEN(.TRUE.) (SHOPAR parameter table).
  const auto paneled = session::applyPaneling(session);
  if (!paneled.ok) {
    // Defensive: the prior visit guarantees success safely; this branch
    // stays free of partial panel mutation (buffer already set, panels
    // remain installed valid as guaranteed by the prior visit).
    emitPangenTooManyPanels(out);
    return;
  }
  emitPangenOutput(out, paneled, session,
                   PangenOutputContext::nacaGeneration);
}

std::vector<xfoil::Point2D> currentAirfoilCoordinatesCore(
    const session::SessionComposition& session) {
  std::vector<xfoil::Point2D> coordinates;
  if (!session.paneling.panelingValid) {
    return coordinates;
  }
  const core::paneling::PanelGeometry& panel = session.paneling.panel;
  const std::size_t count = panel.x.size() < panel.y.size()
                                ? panel.x.size()
                                : panel.y.size();
  coordinates.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    coordinates.push_back(xfoil::Point2D{panel.x[index], panel.y[index]});
  }
  return coordinates;
}

PcopCoreStatus handlePcopCore(session::SessionComposition& session,
                              std::ostream& out) {
  // Canonical PCOP (xfoil.f:288 CALL ABCOPY(.TRUE.)): direction is
  // buffer(XB,YB)->panel(X,Y), not panel->buffer. The handler orchestrates
  // exclusively the confirmed session use case copyBufferToCurrent
  // (core::paneling::abcopy); no second copy, no PANGEN.
  if (!session.geometry.valid) {
    // ABCOPY buffer message (xgdes.f:966 WRITE(*,*)).
    emitAbcopyBufferUnavailable(out);
    return PcopCoreStatus::bufferUnavailable;
  }
  const auto status = session::copyBufferToCurrent(session);
  if (!status.ok) {
    // For a valid buffer (NB>=2) the only ABCOPY rejection is the
    // capacity limit NB > IQX-5 = 365 (xgdes.f:968). copyBufferToCurrent
    // mutates only after a successful core::paneling::abcopy; contour,
    // parameters, buffer and analysis remain unchanged.
    emitAbcopyCapacityError(out, core::paneling::kMaxAbcopyNodeCount,
                            static_cast<int>(session.geometry.buffer.x.size()));
    return PcopCoreStatus::capacityExceeded;
  }
  // FORMAT 1200: leading line break + node count (I4).
  emitAbcopyConfirmation(out,
                         static_cast<int>(session.paneling.panel.x.size()));
  return PcopCoreStatus::applied;
}

PaneCoreStatus handlePaneCore(session::SessionComposition& session,
                              std::ostream& out) {
  // Canonical TOP-level PANE (xfoil.f: CALL PANGEN(.TRUE.)): panel contour
  // from the session buffer and the stored panel parameters via the confirmed
  // applyPaneling producer; no second numerical path, no
  // panel-coordinate copy, no ABCOPY.
  if (!session.geometry.valid) {
    // PANGEN buffer message (xfoil.f WRITE(*,*) 'PANGEN: Buffer airfoil...').
    out << " PANGEN: Buffer airfoil not available.\n";
    return PaneCoreStatus::bufferUnavailable;
  }
  session::PanelingApplyResult paneled;
  try {
    paneled = session::applyPaneling(session);
  } catch (const std::exception&) {
    // Core precondition (e.g. lefind N<3): controlled abort without
    // partial mutation. applyPaneling mutates only after successful PANGEN.
    return PaneCoreStatus::panelingFailed;
  }
  if (!paneled.ok) {
    // PANGEN kernel error (e.g. capacity limit). Previous contour/parameter/
    // analysis remain intact (applyPaneling swaps only on success).
    emitPangenTooManyPanels(out);
    return PaneCoreStatus::panelingFailed;
  }
  // PANGEN(.TRUE.): diagnostics + TE + SHOPAR parameter table, no CANG.
  emitPangenOutput(out, paneled, session, PangenOutputContext::topLevelPane);
  return PaneCoreStatus::applied;
}

void handlePparShowCore(const session::SessionComposition& session,
                        std::ostream& out) {
  const core::paneling::PanelingParameters& params =
      session::panelingParameters(session);
  emitPparTable(out, params.panelCount, params.bunchingParam,
                params.teToLeDensityRatio, params.refinementDensityRatio,
                params.upperRefinementRange[0], params.upperRefinementRange[1],
                params.lowerRefinementRange[0], params.lowerRefinementRange[1]);
}

PparParameterCoreStatus applyPparParameterCore(
    session::SessionComposition& session,
    core::paneling::PanelParameterKind kind, std::span<const double> values,
    std::ostream& out) {
  // Canonical GETPAN parameter branch: stores only the parameters.
  // No PANGEN, no pressure tables (GO TO 12), no analysis invalidation.
  const auto status = session::setPanelParameter(session, kind, values);
  if (!status.ok) {
    // Controlled rejection; parameter state, contour and analysis unchanged.
    // The interactive ASKI/ASKR diagnostics live in the menu loop.
    return PparParameterCoreStatus::rejected;
  }
  if (kind == core::paneling::PanelParameterKind::panelCount &&
      values.size() == 1) {
    // Fortran clamp NPAN > IQX-6 (364): format 1200. Detection without a second
    // threshold copy by comparing requested vs. stored value.
    const int requested = static_cast<int>(values[0]);
    const int stored = session::panelingParameters(session).panelCount;
    if (requested > stored) {
      emitPparPanelCountClamped(out, stored);
      return PparParameterCoreStatus::appliedClamped;
    }
  }
  return PparParameterCoreStatus::applied;
}

PparApplyCoreStatus applyPparPanelingCore(session::SessionComposition& session,
                                          std::ostream& out) {
  // Canonical GETPAN apply (xfoil.f:2196-2202): PANGEN(.FALSE.) from the
  // session buffer, then CANG(X,Y,N,1,...). applyPaneling is the confirmed
  // PANGEN producer (identical to TOP-level PANE); no second numerical path,
  // no panel-coordinate copy.
  if (!session.geometry.valid) {
    // PANGEN buffer message (xfoil.f WRITE(*,*) 'PANGEN: Buffer airfoil...').
    out << " PANGEN: Buffer airfoil not available.\n";
    return PparApplyCoreStatus::bufferUnavailable;
  }
  session::PanelingApplyResult paneled;
  try {
    paneled = session::applyPaneling(session);
  } catch (const std::exception&) {
    // Core precondition (e.g. lefind N<3): controlled abort without
    // partial mutation. applyPaneling mutates only after successful PANGEN.
    // For this there is no canonical Fortran error text (Fortran reads
    // NPAN=2 outside the valid indices); the productive CLI lower bound
    // (kMinPanelingPoints) prevents this path. No invented diagnostics.
    return PparApplyCoreStatus::panelingFailed;
  }
  if (!paneled.ok) {
    // PANGEN kernel error (e.g. capacity limit). applyPaneling swaps the
    // contour only on success; previous contour/parameter/analysis stay intact.
    emitPangenTooManyPanels(out);
    return PparApplyCoreStatus::panelingFailed;
  }
  // CANG on the new panel contour: maximum corner angle, 1-based display.
  // The value is computed via the confirmed core::geometry::cang and
  // only formatted; no new computation/heuristic in the interpreter.
  PangenCornerReport corner;
  const auto& panel = session.paneling.panel;
  if (!panel.x.empty() && !panel.y.empty()) {
    const auto cornerReport = core::geometry::cang(panel.x, panel.y);
    const std::size_t idx = cornerReport.maxIndex;
    corner.present = true;
    corner.angle = cornerReport.maxAngle;
    corner.index = static_cast<int>(idx + 1);
    corner.x = panel.x[idx];
    corner.y = panel.y[idx];
  }
  // PANGEN(.FALSE.): diagnostics + TE + CANG, no SHOPAR parameter table.
  emitPangenOutput(out, paneled, session, PangenOutputContext::pparApply,
                   corner);
  return PparApplyCoreStatus::applied;
}

// STRIP (Fortran STRIP): removes leading and trailing spaces.
std::string stripSpaces(std::string_view text) {
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

// aread.f OPEN error name: FNAME is CHARACTER*128; the name reported by
// AREAD ends at the first space plus two characters (canonical field
// truncation), otherwise at the first character.
std::string areadOpenErrorName(std::string_view name) {
  std::string padded{name};
  padded.resize(128, ' ');
  const std::size_t space = padded.find(' ');
  if (space == std::string::npos) {
    return padded.substr(0, 1);
  }
  return padded.substr(0, space + 2);
}

LoadCoreStatus handleLoadCore(session::SessionComposition& session,
                              const std::filesystem::path& path,
                              const std::string& requestedName,
                              std::ostream& out, int selectedElement) {
  const app::CoordinateFileReadResult read =
      app::DatFileReader::readCoordinateFile(path, selectedElement);

  // AREAD abort branches: no session mutation. For an MSES multi-element
  // file Fortran writes the MSES marker line already before the element query
  // (aread.f:90-95, then ASKI for 999.0/999.0). The handler
  // does not itself run the interactive element prompt, but emits the
  // already confirmed marker-line text before the later CLI loop
  // takes over the element selection.
  if (read.elementSelectionRequired) {
    if (!read.ispars.empty()) {
      emitAirfoilFileInfo(out, AirfoilFileInfoContext::msesMulti, read.name);
    }
    emitLoadElementNotFound(out);
    return LoadCoreStatus::elementNotFound;
  }
  if (!read.success()) {
    switch (read.error) {
      case app::DatReadError::cannotOpen:
        emitLoadFileOpenError(out, areadOpenErrorName(path.string()));
        return LoadCoreStatus::fileOpenError;
      case app::DatReadError::coordinateOverflow:
        emitLoadBufferOverflow(out, static_cast<int>(read.overflowLimit));
        return LoadCoreStatus::bufferOverflow;
      case app::DatReadError::elementNotFound:
        emitLoadElementNotFound(out);
        return LoadCoreStatus::elementNotFound;
      default:
        emitLoadReadError(out);
        return LoadCoreStatus::readError;
    }
  }

  // File type and AREAD marker line (aread.f). The output context is domain-
  // specific: plain, labeled, MSES single and MSES multi. The marker-line name
  // for labeled and MSES files is the raw name line LINE1
  // (aread.f:55, Name=LINE1 also in the MSES branch); plain files
  // show no name. The emitter carries the context-specific
  // field width and blank handling.
  AirfoilFileInfoContext info = AirfoilFileInfoContext::plain;
  if (read.format == app::CoordinateFileFormat::labeled) {
    info = AirfoilFileInfoContext::labeled;
  } else if (read.format == app::CoordinateFileFormat::msesSingle) {
    info = AirfoilFileInfoContext::msesSingle;
  } else if (read.format == app::CoordinateFileFormat::msesMulti) {
    info = AirfoilFileInfoContext::msesMulti;
  }
  const bool hasKennzeileName = info == AirfoilFileInfoContext::labeled ||
                                info == AirfoilFileInfoContext::msesSingle ||
                                info == AirfoilFileInfoContext::msesMulti;
  const std::string_view kennzeileName =
      hasKennzeileName ? std::string_view{read.name} : std::string_view{};
  emitAirfoilFileInfo(out, info, kennzeileName);

  // Airfoil name per file policy (xfoil.f LOAD). For a plain file
  // AREAD/ASKS asks for the name (prompt; the value arrives as requestedName).
  // For labeled and MSES files the session name comes from LINE1 and
  // is treated like Fortran STRIP. The session name is separate from the
  // visible AREAD CHARACTER field: the marker line is only padded or
  // shortened in a way specific to the emitter context.
  std::string name;
  if (info == AirfoilFileInfoContext::plain) {
    emitAsksPrompt(out, "Enter airfoil name");
    name = stripSpaces(requestedName);
  } else {
    name = stripSpaces(read.name);
  }
  if (name.size() > 32) {
    name.resize(32);
  }

  // Coordinate extraction and finiteness check (geometry error before
  // every mutation).
  std::vector<double> x;
  std::vector<double> y;
  x.reserve(read.coordinates.size());
  y.reserve(read.coordinates.size());
  for (const app::Coordinate& point : read.coordinates) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      emitLoadGeometryFailed(out);
      return LoadCoreStatus::invalidGeometry;
    }
    x.push_back(point.x);
    y.push_back(point.y);
  }
  if (x.size() < 3) {
    emitLoadGeometryFailed(out);
    return LoadCoreStatus::invalidGeometry;
  }

  // Traversal direction via the signed area of the confirmed core kernel
  // aeCalc (itype 1). aeCalc returns exactly the LOAD shoelace sum
  // SUM 0.5*(Y(I)+Y(IP))*(X(I)-X(IP)); t is unused for itype 1.
  std::vector<double> thicknessArea(x.size(), 0.0);
  const double area =
      core::geometry::aeCalc(x, y, thicknessArea, /*itype=*/1).area;
  const bool clockwise = area < 0.0;
  if (clockwise) {
    // Fortran LOAD: point reversal for clockwise (pure data operation).
    std::reverse(x.begin(), x.end());
    std::reverse(y.begin(), y.end());
  }
  emitLoadPointCountAndOrdering(out, static_cast<int>(x.size()), clockwise);

  // LOAD normalization (LNORM) via the confirmed core transformation norm.
  bool normalized = false;
  if (session::normalizeOnLoad(session)) {
    std::vector<double> xp(x.size());
    std::vector<double> yp(x.size());
    std::vector<double> s(x.size());
    try {
      core::geometry::norm(x, xp, y, yp, s);
      normalized = true;
    } catch (const std::exception&) {
      emitLoadNormalizationFailed(out);
      return LoadCoreStatus::normalizationFailed;
    }
  }
  if (normalized) {
    emitNormalized(out);
  }

  // Buffer adoption and ABCOPY (canonical without PANGEN). After successful
  // validation these are not missing; an unexpected failure is
  // reported in a controlled way.
  core::geometry::AirfoilBuffer buffer{x, y, name};
  const GeometrySetResult set =
      session::setBufferGeometry(session, std::move(buffer));
  if (!set.success()) {
    emitLoadGeometryFailed(out);
    return LoadCoreStatus::invalidGeometry;
  }
  const session::SessionOperationStatus abcopy =
      session::copyBufferToCurrent(session);
  if (!abcopy.ok) {
    emitLoadGeometryFailed(out);
    return LoadCoreStatus::invalidGeometry;
  }

  // PREFIX/NPREFIX (SETFILE xfoil.f 1231-1237): path name of the loaded
  // file up to the first dot; default suggestion for later
  // filename prompts.
  {
    std::string prefix = path.filename().string();
    const std::string::size_type dot = prefix.find('.');
    if (dot != std::string::npos) {
      prefix = prefix.substr(0, dot);
    }
    session.geometry.filePrefix = prefix;
  }

  // Adopt the ISES-/MSES-domain parameter (ISPARS) (xfoil.f LOAD, aread.f).
  // For an MSES-/ISES-file (ITYPE>=3) ISPARS is the second file line read
  // (aread.f:95 ISPARS=LINE2); it is adopted into the session verbatim,
  // limited to 80 characters, and later written back unchanged by ISAV
  // (right-trimmed). For a normal file (ITYPE<=2) Fortran leaves an
  // already set ISPARS unchanged (xfoil.f:1288 sets a default only when
  // ISPARS==' '; the session value is never empty), so
  // isesDomainParameters stays untouched here.
  if (info == AirfoilFileInfoContext::msesSingle ||
      info == AirfoilFileInfoContext::msesMulti) {
    std::string domain = read.ispars;
    if (domain.size() > 80) {
      domain.resize(80);
    }
    session::setIsesDomainParameters(session, std::move(domain));
  }

  // Characteristic values from confirmed core results: LE/TE/chord from the
  // abcopy panel contour, thickness/camber from geopar (geometryMetrics).
  const core::paneling::PanelGeometry& panel =
      session::currentPanelGeometry(session);
  const std::optional<core::geometry::GeometryMetrics> metrics =
      session::geometryMetrics(session);
  if (metrics) {
    emitThicknessCamber(out, metrics->thick, metrics->thickX, metrics->cambr,
                        metrics->cambrX);
  }
  emitLeTeChord(out, panel.xle, panel.yle, panel.chord, panel.xte, panel.yte);
  emitAbcopyConfirmation(out, static_cast<int>(panel.x.size()));

  // LOAD command branch (xfoil.f:207-217): after ABCOPY, CANG checks the
  // largest panel angle against ANGTOL (xfoil.f INIT 40.0; changes only
  // via GDES/CADD). PANPLT is a plot command and stays unavailable.
  constexpr double kFortranInitAngtol = 40.0;
  if (panel.x.size() >= 3) {
    const core::geometry::CornerAngleReport corners =
        core::geometry::cang(panel.x, panel.y);
    if (std::fabs(corners.maxAngle) > kFortranInitAngtol) {
      emitLoadPoorDistributionWarning(out, corners.maxAngle,
                                      static_cast<int>(corners.maxIndex) + 1);
    }
  }

  // LCLOCK (Fortran LOAD: LCLOCK = area<0) as file-policy update.
  session::setWriteOrientation(session, clockwise);

  return LoadCoreStatus::success;
}

// Reads a coordinate file and transfers it into a pair of vectors.
// Returns false on a read error; the status is then filled.
bool readInteProfile(const std::filesystem::path& path,
                     std::vector<double>& x, std::vector<double>& y,
                     std::string& name, InteCoreStatus& status,
                     std::ostream& out) {
  const app::CoordinateFileReadResult read =
      app::DatFileReader::readCoordinateFile(path);
  if (read.elementSelectionRequired) {
    emitInteFileOpenError(out, areadOpenErrorName(path.string()));
    status = InteCoreStatus::readError;
    return false;
  }
  if (!read.success()) {
    switch (read.error) {
      case app::DatReadError::cannotOpen:
        emitInteFileOpenError(out, areadOpenErrorName(path.string()));
        status = InteCoreStatus::fileOpenError;
        break;
      case app::DatReadError::coordinateOverflow:
        emitLoadBufferOverflow(out, static_cast<int>(read.overflowLimit));
        status = InteCoreStatus::bufferOverflow;
        break;
      default:
        emitLoadReadError(out);
        status = InteCoreStatus::readError;
        break;
    }
    return false;
  }
  x.clear();
  y.clear();
  x.reserve(read.coordinates.size());
  y.reserve(read.coordinates.size());
  for (const app::Coordinate& point : read.coordinates) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      status = InteCoreStatus::invalidGeometry;
      return false;
    }
    x.push_back(point.x);
    y.push_back(point.y);
  }
  name = stripSpaces(read.name);
  return true;
}

// TOP-level NORM (xfoil.f:233-239): LNORM = .NOT.LNORM, pure switching of the
// LOAD normalization policy. Reads the current session value and sets the
// negated value via the confirmed session operation; no numeric
// coordinate transformation, no buffer/panel/analysis mutation, no
// second policy copy.
NormCoreStatus handleNormCore(session::SessionComposition& session,
                              std::ostream& out) {
  const bool enabled = !session::normalizeOnLoad(session);
  session::setNormalizeOnLoad(session, enabled);
  if (enabled) {
    emitNormEnabled(out);
    return NormCoreStatus::enabledAfter;
  }
  emitNormDisabled(out);
  return NormCoreStatus::disabledAfter;
}

// TOP-level REVE (xfoil.f:176-182): LCLOCK = .NOT.LCLOCK, pure switching of the
// write orientation for SAVE/PSAV/ISAV/MSAV. Reads the current session value
// and sets the negated value via the confirmed session operation. REVE is
// exclusively output policy (xfoil.f:1384/1506 only flip at write
// time): no coordinate reversal of buffer or panel, no origin/revision
// change, no analysis invalidation, no second policy copy.
//
ReveCoreStatus handleReveCore(session::SessionComposition& session,
                              std::ostream& out) {
  const bool clockwise = !session::writeClockwise(session);
  session::setWriteOrientation(session, clockwise);
  if (clockwise) {
    emitReveClockwise(out);
    return ReveCoreStatus::clockwiseAfter;
  }
  emitReveCounterclockwise(out);
  return ReveCoreStatus::counterclockwiseAfter;
}

// TOP-level DELI (xfoil.f:185-204): sets KDELIM (0=blank, 1=comma, 2=tab) from
// an argument. A valid argument (0..2) takes over the value silently via the
// confirmed session operation. Without an argument or for an invalid value
// Fortran gives the menu (Format 2100) with the current KDELIM and demands
// a prompt value (ASKI); this handler emits the menu and reports
// menuRequested without changing the state. KDELIM is pure output policy:
// no buffer/panel/metric/analysis/NORM/REVE change.
//
DeliCoreStatus handleDeliCore(session::SessionComposition& session,
                              std::ostream& out, int value, bool hasValue) {
  if (hasValue && value >= 0 && value <= 2) {
    session::setDelimiter(session, value);
    return DeliCoreStatus::applied;
  }
  emitDelimiterMenu(out, session::delimiter(session));
  return DeliCoreStatus::menuRequested;
}

// TOP-level XYCM (xfoil.f:255-262): sets the CM reference point XCMREF/YCMREF.
// Two finite values (including negative ones) are adopted together and
// as a transaction via the confirmed session operation (canonically silent).
// If a value is missing or not finite, no partial mutation happens;
// Fortran then asks for both values per ASKR (a single argument is
// ignored), the prompts live in the CLI layer. XYCM invalidates or
// recomputes no existing result and changes no geometry, panel or
// solver states; the new reference point only takes effect in the next
// CLCALC, where only CM itself changes. 
XycmCoreStatus handleXycmCore(session::SessionComposition& session,
                              std::ostream& out, double x, double y,
                              bool hasX, bool hasY) {
  static_cast<void>(out);  // XYCM argument path is canonically silent (no text)
  if (hasX && hasY && std::isfinite(x) && std::isfinite(y)) {
    session::setMomentReference(session, x, y);
    return XycmCoreStatus::applied;
  }
  return XycmCoreStatus::promptRequested;
}

// TOP-level NAME (xfoil.f:344-350): with a non-empty inline argument Fortran
// sets Name = COMARG and calls STRIP; the path is silent. Without an argument
// Fortran calls NAMMOD(Name,0,-1), which always writes the prompt with the
// current or newly formatted bracket requirement. The handler signature takes
// the prompt answer structured: without an answer only the prompt is
// output and no session mutation is performed; with an empty answer the
// current airfoil name is preserved; with a non-empty answer the answer is
// adopted as the new airfoil name. The canonical CHARACTER*48 truncation and
// STRIP trimming live in session::setGeometryName. No
// numeric geometry, panel, analysis or file-policy mutation. Still
//
NameCoreStatus handleNameCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view answer,
                              bool hasAnswer) {
  if (hasArgument && !stripSpaces(argument).empty()) {
    session::setGeometryName(session, std::string(argument));
    return NameCoreStatus::applied;
  }

  const std::string nameDefault =
      session::geometryNameVersionDefault(session, 0, -1);
  // Prompt exactly once per logical command: in the interactive two-phase
  // flow the first call emits the prompt, the answer call applies
  // the answer without re-emitting it (canonical: prompt, then READ).
  if (!hasAnswer) {
    emitNamePrompt(out, nameDefault);
    return NameCoreStatus::promptRequested;
  }
  if (stripSpaces(answer).empty()) {
    session::incrementGeometryName(session, 0, -1);
  } else {
    session::setGeometryName(session, std::string(answer));
  }
  return NameCoreStatus::applied;
}

// TOP-level NINC (xfoil.f:353-355): NAMMOD(Name,1,1). The session operation
// geometryNameVersionDefault computes the canonical NAMDEF default;
// incrementGeometryName applies it for an empty answer. A non-empty
// answer overwrites the default like the canonical READ(*,1000) name.
// The handler contains no own bracket/increment logic. Still not
//
NincCoreStatus handleNincCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view answer,
                              bool hasAnswer) {
  const std::string nameDefault =
      session::geometryNameVersionDefault(session, 1, 1);
  if (!hasAnswer) {
    emitNamePrompt(out, nameDefault);
    return NincCoreStatus::promptRequested;
  }
  if (stripSpaces(answer).empty()) {
    session::incrementGeometryName(session, 1, 1);
  } else {
    session::setGeometryName(session, std::string(answer));
  }
  return NincCoreStatus::applied;
}

// Shared file-open/overwrite decision for SUBROUTINE SAVE
// (xfoil.f:1349-1366,1418-1419). SAVE, PSAV, USAV and ISAV share the same
// file path/prompt contract; the domain format choice stays with the
// individual handlers. The return value is structured and changes no session.
enum class CoordinateFileRequestAction {
  proceed,
  filenamePromptRequested,
  overwriteConfirmationRequired,
  notSavedDeclined,
  badFilename,
};

struct CoordinateFileRequestDecision {
  std::filesystem::path path;
  bool exists{};
  CoordinateFileRequestAction action{CoordinateFileRequestAction::proceed};
};

CoordinateFileRequestDecision decideCoordinateFileRequest(
    std::ostream& out, const xfoil::io::OutputPathPolicy& policy,
    std::string_view argument, bool hasArgument,
    std::string_view filenameAnswer, bool hasFilenameAnswer,
    std::string_view overwriteAnswer, bool hasOverwriteAnswer) {
  std::string path;
  bool havePath = false;
  if (hasArgument) {
    path = stripSpaces(argument);
    havePath = !path.empty();
  }
  if (!havePath) {
    if (!hasFilenameAnswer) {
      emitAsksPrompt(out, "Enter output filename");
      return {{}, false, CoordinateFileRequestAction::filenamePromptRequested};
    }
    // ASKS takes over the prompt answer without STRIP; leading spaces
    // can therefore be part of the filename. A completely empty or
    // spaces-only answer leads to the OPEN error.
    path = std::string(filenameAnswer);
    if (path.find_first_not_of(' ') == std::string::npos) {
      emitSaveBadFilename(out);
      return {{}, false, CoordinateFileRequestAction::badFilename};
    }
  }

 // the prefix transformation is applied exactly once
  // here, before the exists check (overwrite prompt) and before
  // the later OPEN of the AirfoilWriter instances. Prompt texts remain unchanged.
  const std::filesystem::path target{
      io::applyOutputPathPolicy(policy, path)};
  std::error_code ec;
  const bool exists = std::filesystem::exists(target, ec);
  if (ec || std::filesystem::is_directory(target, ec)) {
    emitSaveBadFilename(out);
    return {target, false, CoordinateFileRequestAction::badFilename};
  }

  if (exists) {
    if (!hasOverwriteAnswer) {
      emitSaveOverwritePrompt(out);
      return {target, exists,
              CoordinateFileRequestAction::overwriteConfirmationRequired};
    }
    const char first =
        overwriteAnswer.empty() ? ' ' : overwriteAnswer.front();
    if (first == 'N' || first == 'n') {
      emitSaveNotSaved(out);
      return {target, exists, CoordinateFileRequestAction::notSavedDeclined};
    }
  }

  return {target, exists, CoordinateFileRequestAction::proceed};
}

// TOP-level SAVE (xfoil.f:156-157, SAVE(1,COMARG)): writes the current
// panel contour X/Y/N, not the buffer. The filename comes for an inline
// argument from the COMARG already trimmed by ASKC; for a missing argument
// the ASKS prompt is emitted and the prompt answer is adopted without
// trimming (xfoil.f ASKS/SAVE). An existing regular file triggers the
// overwrite prompt; 'N'/'n' as first read character declines,
// everything else confirms. Directories and OPEN/write errors correspond to
// the canonical 'Bad filename.' path. The handler changes no
// session state and uses exclusively the existing AirfoilWriter.
//
SaveCoreStatus handleSaveCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer) {
  const CoordinateFileRequestDecision decision = decideCoordinateFileRequest(
      out, session.outputPaths, argument, hasArgument, filenameAnswer,
      hasFilenameAnswer, overwriteAnswer, hasOverwriteAnswer);
  switch (decision.action) {
    case CoordinateFileRequestAction::filenamePromptRequested:
      return SaveCoreStatus::filenamePromptRequested;
    case CoordinateFileRequestAction::overwriteConfirmationRequired:
      return SaveCoreStatus::overwriteConfirmationRequired;
    case CoordinateFileRequestAction::notSavedDeclined:
      return SaveCoreStatus::notSavedDeclined;
    case CoordinateFileRequestAction::badFilename:
      return SaveCoreStatus::badFilename;
    case CoordinateFileRequestAction::proceed:
      break;
  }

  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::labeled;
  switch (session::delimiter(session)) {
    case 1:
      options.delimiter = app::CoordinateDelimiter::comma;
      break;
    case 2:
      options.delimiter = app::CoordinateDelimiter::tab;
      break;
    default:
      options.delimiter = app::CoordinateDelimiter::blank;
      break;
  }
  options.clockwise = session::writeClockwise(session);
  options.name = session::geometryNameHeader(session);
  options.overwriteExisting = decision.exists;

  const std::vector<xfoil::Point2D> coordinates =
      currentAirfoilCoordinatesCore(session);
  const app::AirfoilWriteResult written =
      app::writeAirfoil(decision.path, coordinates, options);
  if (!written.success) {
    emitSaveBadFilename(out);
    return SaveCoreStatus::badFilename;
  }
  return SaveCoreStatus::written;
}

// TOP-level PSAV (xfoil.f:160-161, SAVE(0,COMARG)): writes exclusively
// the current panel contour X/Y/N in plain format. IFTYP=0 suppresses in
// SUBROUTINE SAVE both Name(1:NNAME) and ISPARS; there is no header
// and no point count. REVE/LCLOCK and DELI/KDELIM act as for SAVE. The
// handler reads only source contour and file policy from the session and calls
// the existing AirfoilWriter with explicit plain format. PSAV changes
// no session state. 
PsavCoreStatus handlePsavCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer) {
  const CoordinateFileRequestDecision decision = decideCoordinateFileRequest(
      out, session.outputPaths, argument, hasArgument, filenameAnswer,
      hasFilenameAnswer, overwriteAnswer, hasOverwriteAnswer);
  switch (decision.action) {
    case CoordinateFileRequestAction::filenamePromptRequested:
      return PsavCoreStatus::filenamePromptRequested;
    case CoordinateFileRequestAction::overwriteConfirmationRequired:
      return PsavCoreStatus::overwriteConfirmationRequired;
    case CoordinateFileRequestAction::notSavedDeclined:
      return PsavCoreStatus::notSavedDeclined;
    case CoordinateFileRequestAction::badFilename:
      return PsavCoreStatus::badFilename;
    case CoordinateFileRequestAction::proceed:
      break;
  }

  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::plain;
  switch (session::delimiter(session)) {
    case 1:
      options.delimiter = app::CoordinateDelimiter::comma;
      break;
    case 2:
      options.delimiter = app::CoordinateDelimiter::tab;
      break;
    default:
      options.delimiter = app::CoordinateDelimiter::blank;
      break;
  }
  options.clockwise = session::writeClockwise(session);
  options.overwriteExisting = decision.exists;

  const std::vector<xfoil::Point2D> coordinates =
      currentAirfoilCoordinatesCore(session);
  const app::AirfoilWriteResult written =
      app::writeAirfoil(decision.path, coordinates, options);
  if (!written.success) {
    emitSaveBadFilename(out);
    return PsavCoreStatus::badFilename;
  }
  return PsavCoreStatus::written;
}

// TOP-level USAV (xfoil.f:164-165, SAVE(-1,COMARG)): writes exclusively
// the current panel contour X/Y/N in integer format Format(1X,I12,I12).
// IFTYP=-1 suppresses Name(1:NNAME) and ISPARS; the IFTYP=-1 branch in
// SUBROUTINE SAVE does not use KDELIM. REVE/LCLOCK acts as for SAVE/PSAV.
// The handler reads only source contour and write orientation from the session
// and calls the existing AirfoilWriter with explicit integerCoordinates
// format. USAV changes no session state.
UsavCoreStatus handleUsavCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer) {
  const CoordinateFileRequestDecision decision = decideCoordinateFileRequest(
      out, session.outputPaths, argument, hasArgument, filenameAnswer,
      hasFilenameAnswer, overwriteAnswer, hasOverwriteAnswer);
  switch (decision.action) {
    case CoordinateFileRequestAction::filenamePromptRequested:
      return UsavCoreStatus::filenamePromptRequested;
    case CoordinateFileRequestAction::overwriteConfirmationRequired:
      return UsavCoreStatus::overwriteConfirmationRequired;
    case CoordinateFileRequestAction::notSavedDeclined:
      return UsavCoreStatus::notSavedDeclined;
    case CoordinateFileRequestAction::badFilename:
      return UsavCoreStatus::badFilename;
    case CoordinateFileRequestAction::proceed:
      break;
  }

  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::integerCoordinates;
  options.clockwise = session::writeClockwise(session);
  options.overwriteExisting = decision.exists;

  const std::vector<xfoil::Point2D> coordinates =
      currentAirfoilCoordinatesCore(session);
  const app::AirfoilWriteResult written =
      app::writeAirfoil(decision.path, coordinates, options);
  if (!written.success) {
    emitSaveBadFilename(out);
    return UsavCoreStatus::badFilename;
  }
  return UsavCoreStatus::written;
}

// TOP-level ISAV (xfoil.f:168-169, SAVE(2,COMARG)): writes the current
// panel contour X/Y/N in ISES format. IFTYP=2 writes Name(1:NNAME) first,
// then ISPARS(1:K) right-trimmed via the 80-step backward scan
// (xfoil.f:1369-1382), then coordinates in SAVE format incl. KDELIM.
// REVE/LCLOCK acts as for SAVE/PSAV. The handler reads only source contour,
// airfoil name, ISPARS and file policy from the session and calls the
// existing AirfoilWriter with explicit ises format. ISAV changes
// no session state. 
IsavCoreStatus handleIsavCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view filenameAnswer,
                              bool hasFilenameAnswer,
                              std::string_view overwriteAnswer,
                              bool hasOverwriteAnswer) {
  const CoordinateFileRequestDecision decision = decideCoordinateFileRequest(
      out, session.outputPaths, argument, hasArgument, filenameAnswer,
      hasFilenameAnswer, overwriteAnswer, hasOverwriteAnswer);
  switch (decision.action) {
    case CoordinateFileRequestAction::filenamePromptRequested:
      return IsavCoreStatus::filenamePromptRequested;
    case CoordinateFileRequestAction::overwriteConfirmationRequired:
      return IsavCoreStatus::overwriteConfirmationRequired;
    case CoordinateFileRequestAction::notSavedDeclined:
      return IsavCoreStatus::notSavedDeclined;
    case CoordinateFileRequestAction::badFilename:
      return IsavCoreStatus::badFilename;
    case CoordinateFileRequestAction::proceed:
      break;
  }

  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::ises;
  switch (session::delimiter(session)) {
    case 1:
      options.delimiter = app::CoordinateDelimiter::comma;
      break;
    case 2:
      options.delimiter = app::CoordinateDelimiter::tab;
      break;
    default:
      options.delimiter = app::CoordinateDelimiter::blank;
      break;
  }
  options.clockwise = session::writeClockwise(session);
  options.name = session::geometryNameHeader(session);
  options.isesDomainParameters = session::isesDomainParameters(session);
  options.overwriteExisting = decision.exists;

  const std::vector<xfoil::Point2D> coordinates =
      currentAirfoilCoordinatesCore(session);
  const app::AirfoilWriteResult written =
      app::writeAirfoil(decision.path, coordinates, options);
  if (!written.success) {
    emitSaveBadFilename(out);
    return IsavCoreStatus::badFilename;
  }
  return IsavCoreStatus::written;
}

// TOP-level MSAV (xfoil.f 172-173, SUBROUTINE MSAVE xfoil.f 1430-1544).
// Canonical flow: OPEN STATUS='OLD' (file must exist), parse two
// header lines and elements, report the element count, ASKI for the
// element to replace, range check, replace or append
// with the current panel contour (LCLOCK order), REWIND and
// rewrite the entire file. There is no overwrite prompt;
// DELI/KDELIM has no effect (fixed formats A and 1X,2G15.7).
MsaveCoreStatus handleMsaveCore(session::SessionComposition& session,
                                std::ostream& out, std::string_view argument,
                                bool hasArgument,
                                std::string_view filenameAnswer,
                                bool hasFilenameAnswer,
                                std::string_view elementAnswer,
                                bool hasElementAnswer, int& elementDefault) {
  // IF(FNAME1(1:1) .NE. ' ') THEN FNAME = FNAME1 ELSE ASKS(...)
  std::string path;
  if (hasArgument && !argument.empty() && argument.front() != ' ') {
    path = std::string(argument);
  } else if (hasFilenameAnswer) {
    path = std::string(filenameAnswer);
  } else {
    return MsaveCoreStatus::filenamePromptRequested;
  }
  while (!path.empty() && path.back() == ' ') {
    path.pop_back();
  }

  // OPEN(LU,FILE=FNAME,STATUS='OLD',ERR=9005)
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    out << " Old file OPEN error.  Airfoil not saved.\n";
    return MsaveCoreStatus::notWritten;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  file.close();

  const io::MsesDecodeResult decoded =
      io::decodeMsesMultielement(buffer.str());
  if (decoded.status == io::MsesDecodeStatus::readError) {
    // READ(LU,1000,ERR=9010): missing header line or unreadable data.
    // Canonically Fortran aborts on unreadable data with a runtime error;
    // controlled message as a documented deviation.
    out << " Old file READ error.  Airfoil not saved.\n";
    return MsaveCoreStatus::notWritten;
  }
  if (decoded.status == io::MsesDecodeStatus::overflow) {
    // STOP 'LOAD: Array overflow' (xfoil.f 1461): program end.
    out << "LOAD: Array overflow\n";
    return MsaveCoreStatus::fatalStop;
  }

  io::MsesMultielementFile mses = decoded.file;
  const int nel = static_cast<int>(mses.elements.size());

  // WRITE(*,3010) NEL: FORMAT(/' Specified multielement airfoil has',I2,
  // ' elements.')
  // The message appears exactly once before the ASKI prompt; the
  // interpreter calls the kernel again after the answer.
  if (!hasElementAnswer) {
    out << "\n Specified multielement airfoil has"
        << io::fortranI(2, nel) << " elements.\n";
    elementDefault = decoded.residualElement;
    return MsaveCoreStatus::elementPromptRequested;
  }

  // ASKI: an empty answer keeps the Fortran residual value IEL; valid numbers
  // were already validated by the interpreter.
  int iel = elementDefault;
  if (elementAnswer.find_first_not_of(" \t") != std::string_view::npos) {
    iel = std::atoi(std::string(elementAnswer).c_str());
  }

  if (iel < 1 || iel > nel + 1) {
    out << " Element number inappropriate.  Airfoil not written.\n";
    return MsaveCoreStatus::notWritten;
  }
  if (iel == nel + 1) {
    mses.elements.push_back(io::MsesMultielementElement{});
  }

  // NTMP(IEL) = N; IDIR = LCLOCK ? N-I+1 : I
  const std::vector<xfoil::Point2D> coordinates =
      currentAirfoilCoordinatesCore(session);
  io::MsesMultielementElement& target = mses.elements[static_cast<std::size_t>(
      iel - 1)];
  target.x.clear();
  target.y.clear();
  const bool clockwise = session::writeClockwise(session);
  for (std::size_t i = 0; i < coordinates.size(); ++i) {
    const std::size_t source =
        clockwise ? coordinates.size() - 1 - i : i;
    target.x.push_back(coordinates[source].x);
    target.y.push_back(coordinates[source].y);
  }

  // REWIND + new write; CLOSE truncates the file to the new content
  // (verified with gfortran).
  std::ofstream rewritten(path, std::ios::binary | std::ios::trunc);
  if (!rewritten) {
    out << " Old file OPEN error.  Airfoil not saved.\n";
    return MsaveCoreStatus::notWritten;
  }
  rewritten << io::msesMultielementText(mses);
  if (!rewritten) {
    out << " Old file READ error.  Airfoil not saved.\n";
    return MsaveCoreStatus::notWritten;
  }
  return MsaveCoreStatus::written;
}

// TOP-level BEND (xfoil.f 265-272, SUBROUTINE BENDUMP xgeom.f 1295-1365).
// Numerical kernel is core::geometry::ijsect (port of IJSECT);
// this handler exclusively leads the canonical
// WRITE sequence in Format(1X,A,G14.6). The report divisions
// (AIYY/XBAR etc.) are part of the report texts, not solver numerics.
// BEND changes no session state and writes no file.
BendCoreStatus handleBendCore(session::SessionComposition& session,
                              std::ostream& out) {
  const std::vector<xfoil::Point2D> coordinates =
      currentAirfoilCoordinatesCore(session);
  if (coordinates.empty()) {
    // IF(N.EQ.0): WRITE(*,*) / WRITE(*,*) '*** No airfoil available ***'
    out << "\n\n ***  No airfoil available  ***\n";
    return BendCoreStatus::noAirfoil;
  }

  std::vector<double> x(coordinates.size());
  std::vector<double> y(coordinates.size());
  for (std::size_t i = 0; i < coordinates.size(); ++i) {
    x[i] = coordinates[i].x;
    y[i] = coordinates[i].y;
  }
  const core::geometry::SectionBending b =
      core::geometry::ijsect(x, y);

  const auto line = [&out](const char* label, double value) {
    out << ' ' << label << io::fortranG(14, 6, value) << "\n";
  };
  const auto labelOnly = [&out](const char* text) {
    out << ' ' << text << "\n";
  };

  const double xbar = std::max(std::abs(b.xmax - b.xc),
                               std::abs(b.xmin - b.xc));
  const double ybar = std::max(std::abs(b.ymax - b.yc),
                               std::abs(b.ymin - b.yc));
  const double xbart = std::max(std::abs(b.xmax - b.xct),
                                std::abs(b.xmin - b.xct));
  const double ybart = std::max(std::abs(b.ymax - b.yct),
                                std::abs(b.ymin - b.yct));

  out << "\n";
  line("Area =", b.area);
  line("Slen =", b.slen);
  out << "\n";
  labelOnly("X-bending parameters(solid):");
  line("        Xc =", b.xc);
  line("  max X-Xc =", b.xmax - b.xc);
  line("  min X-Xc =", b.xmin - b.xc);
  line("       Iyy =", b.aiyy);
  line(" Iyy/(X-Xc)=", b.aiyy / xbar);
  out << "\n";
  labelOnly("Y-bending parameters(solid):");
  line("        Yc =", b.yc);
  line("  max Y-Yc =", b.ymax - b.yc);
  line("  min Y-Yc =", b.ymin - b.yc);
  line("       Ixx =", b.aixx);
  line(" Ixx/(Y-Yc)=", b.aixx / ybar);
  out << "\n";
  line("       J   =", b.aj);
  out << "\n\n";
  labelOnly("X-bending parameters(skin):");
  line("         Xc =", b.xct);
  line("   max X-Xc =", b.xmax - b.xct);
  line("   min X-Xc =", b.xmin - b.xct);
  line("      Iyy/t =", b.aiyyt);
  line(" Iyy/t(X-Xc)=", b.aiyyt / xbart);
  out << "\n";
  labelOnly("Y-bending parameters(skin):");
  line("         Yc =", b.yct);
  line("   max Y-Yc =", b.ymax - b.yct);
  line("   min Y-Yc =", b.ymin - b.yct);
  line("      Ixx/t =", b.aixxt);
  line(" Ixx/t(Y-Yc)=", b.aixxt / ybart);
  out << "\n";
  line("      J/t   =", b.ajt);

  return BendCoreStatus::reported;
}

// TOP-level RDEF (xfoil.f 333-345, CALL GETDEF(LU,FNAME,.FALSE.)).
// FNAME is CHARACTER*80; the messages (Format 1900/1800/1600) carry
// the string in full length, i.e. with 80 characters including
// padding blanks.
RdefCoreStatus handleRdefCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument) {
  std::string path;
  if (hasArgument && !argument.empty() && argument.front() != ' ') {
    path = std::string(argument);
  } else {
    path = "xfoil.def";
  }
  // FNAME is CHARACTER*64 (XFOIL.INC 31); FNAME = COMARG or
  // FNAME = 'xfoil.def' padded to 64 characters; the canonical
  // A descriptor (Format 1900/1800/1600) carries the full length.
  std::string displayName = path;
  displayName.resize(64, ' ');

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    emitDefaultsNotFound(out, displayName);
    return RdefCoreStatus::notFound;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  file.close();

  const io::DefaultsParseResult parsed = io::parseDefaults(buffer.str());
  if (parsed.complete) {
    session::applyDefaults(session, parsed.values,
                           parsed.values.lcolor ? 4 : 2, path);
    emitDefaultsReadOk(out, displayName);
    out << io::writeDefaults(parsed.values, parsed.values.lcolor ? 4 : 2);
    return RdefCoreStatus::applied;
  }

  if (parsed.error == io::DefaultsError::kEndOfFile) {
    // EOF without END= is canonically not an ERR=80 branch, but a
    // Fortran runtime error with program abort. The lines already read
    // remain effective up to the abort.
    session::applyDefaultsPartial(session, parsed.values, parsed.linesRead);
    emitDefaultsEndOfFile(out);
    return RdefCoreStatus::endOfFile;
  }

  session::applyDefaultsPartial(session, parsed.values, parsed.linesRead);
  emitDefaultsReadError(out, displayName);
  return RdefCoreStatus::readError;
}

// TOP-level WDEF (xfoil.f 309-331). CALL STRIP removes leading and
// trailing blanks; the messages (Format 701/708) use
// FNAME(1:NFN), i.e. the stripped name.
WdefCoreStatus handleWdefCore(session::SessionComposition& session,
                              std::ostream& out, std::string_view argument,
                              bool hasArgument, std::string_view answer,
                              bool hasAnswer) {
  std::string path;
  if (hasArgument && !argument.empty() && argument.front() != ' ') {
    path = std::string(argument);
  } else {
    path = "xfoil.def";
  }
  const std::size_t first = path.find_first_not_of(" \t");
  if (first == std::string::npos) {
    path.clear();
  } else {
    const std::size_t last = path.find_last_not_of(" \t");
    path = path.substr(first, last - first + 1);
  }

 // WDEF is subject to the OutputPathPolicy; the
  // query checks the transformed path, prompt and error texts
  // show the logical name.
  const std::string effective =
      io::applyOutputPathPolicy(session.outputPaths, path);

  const bool exists = std::filesystem::exists(effective);
  if (exists && !hasAnswer) {
    emitDefaultsOverwritePrompt(out, path);
    return WdefCoreStatus::promptRequested;
  }
  if (hasAnswer && (answer.find('N') != std::string_view::npos ||
                    answer.find('n') != std::string_view::npos)) {
    // Canonical Particularity (xfoil.f 319-323): WRITE(*,*) /
    // WRITE(*,*) 'No action taken', CLOSE(LU) — without RETURN the
    // sequence falls through to label 703 (OPEN STATUS='UNKNOWN') and
    // writes the file anyway.
    emitDefaultsNoActionTaken(out);
  }

  std::ofstream file(effective, std::ios::binary | std::ios::trunc);
  if (!file) {
    // OPEN(LU,FILE=FNAME,STATUS='UNKNOWN') without ERR branch: canonical
    // unhandled I/O runtime error with program abort. Controlled
    // deviation: message + session end with exit code 2.
    out << " File  " << path << "  open error\n";
    return WdefCoreStatus::openError;
  }
  file << io::writeDefaults(session.defaults.values,
                            session.defaults.idevrp);
  if (!file) {
    out << " File  " << path << "  write error\n";
    return WdefCoreStatus::openError;
  }
  emitDefaultsWritten(out, path);
  return WdefCoreStatus::written;
}

// Fortran INTE source spline: SCALC, SEGSPLD(-999,-999) for x and y, LEFIND.
// The end derivatives -999,-999 correspond exactly to the INTE call (open
// spline), not to the natural SEGSPL.
core::geometry::SplineProfile makeInteProfile(std::vector<double> x,
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

// ASKR reader for the INTE fraction prompt (userio.f ASKR): prompt with
// leading line break, an empty line keeps the default, an invalid
// number is re-asked without a special message.
bool askRealInte(std::istream& in, std::ostream& out,
                 const std::string& prompt, double& value) {
  for (;;) {
    out << "\n" << prompt << "   r>  ";
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    if (line.find_first_not_of(" \t") == std::string::npos) {
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

InteCoreStatus handleInteCore(session::SessionComposition& session,
                              std::istream& in, std::ostream& out) {
  // INTE source menu (xfoil.f Format 1100: Name = current contour).
  emitInteMenu(out, session.geometry.name);

  // Per source: selection line (READ(*,1000) CAIR CHARACTER*2, authoritative is
  // the first character), then AREAD (F) or current contour (C).
  std::vector<double> x0;
  std::vector<double> y0;
  std::vector<double> x1;
  std::vector<double> y1;
  std::string name0;
  std::string name1;
  for (int source = 0; source < 2; ++source) {
    std::vector<double>* x = (source == 0) ? &x0 : &x1;
    std::vector<double>* y = (source == 0) ? &y0 : &y1;
    std::string* name = (source == 0) ? &name0 : &name1;
    for (;;) {
      emitInteSourcePrompt(out, source);
      std::string line;
      if (!std::getline(in, line)) {
        return InteCoreStatus::inputAborted;
      }
      const char first = line.empty() ? ' ' : line[0];
      if (first == 'F' || first == 'f') {
        emitInteFilenamePrompt(out);
        std::string filename;
        if (!std::getline(in, filename)) {
          return InteCoreStatus::inputAborted;
        }
        InteCoreStatus status = InteCoreStatus::success;
        if (!readInteProfile(std::filesystem::path{filename}, *x, *y, *name,
                             status, out)) {
          return status;
        }
        break;
      }
      if (first == 'C' || first == 'c') {
        const core::paneling::PanelGeometry& panel =
            session.paneling.panel;
        if (panel.x.size() <= 1) {
          emitInteNoCurrentAirfoil(out);
          continue;
        }
        *x = panel.x;
        *y = panel.y;
        *name = session.geometry.name;
        break;
      }
      // Numeric polar source (NPOL=0) or invalid answer:
      // 90 CONTINUE -> 'Invalid response', INTE abort.
      emitInteInvalidResponse(out);
      return InteCoreStatus::invalidSelection;
    }
  }

  // Source names (NAMEINT CHARACTER*48).
  emitInteAirfoilNames(out, name0, name1);

  // Fraction prompt (canonical one real value; no range check).
  double frac = 0.5;
  if (!askRealInte(in, out, "Specify interpolating fraction  0...1", frac)) {
    return InteCoreStatus::inputAborted;
  }

  // Precondition: LEFIND requires N >= 3, inter requires N >= 2.
  if (x0.size() < 3 || x1.size() < 3) {
    return InteCoreStatus::invalidGeometry;
  }

  // Interpolation via the confirmed core kernel inter.
  core::geometry::AirfoilCoordinates result;
  core::geometry::GeometryMetrics metrics;
  try {
    const core::geometry::SplineProfile p0 = makeInteProfile(x0, y0);
    const core::geometry::SplineProfile p1 = makeInteProfile(x1, y1);
    result = core::geometry::inter(p0, p1, frac);
    // Metrics of the result (SCALC/SEGSPL/geopar as Fortran INTE).
    core::geometry::SplineProfile r;
    r.x = result.x;
    r.y = result.y;
    r.s.resize(r.x.size());
    r.xp.resize(r.x.size());
    r.yp.resize(r.x.size());
    core::math::scalc(r.x, r.y, r.s);
    core::math::segspl(r.x, r.s, r.xp);
    core::math::segspl(r.y, r.s, r.yp);
    metrics = core::geometry::geopar(r.x, r.xp, r.y, r.yp, r.s);
  } catch (const std::exception&) {
    return InteCoreStatus::interpolationFailed;
  }

  // GEOPAR thickness/camber (Format 1000, without leading '/').
  emitThicknessCamber(out, metrics.thick, metrics.thickX, metrics.cambr,
                      metrics.cambrX);

  // New name (ASKS 'Enter new airfoil name').
  emitAsksPrompt(out, "Enter new airfoil name");
  std::string resultName;
  if (!std::getline(in, resultName)) {
    return InteCoreStatus::inputAborted;
  }
  resultName = stripSpaces(resultName);

  // Completion: result only in the buffer, no ABCOPY/PANGEN.
  emitInteResultPlaced(out);

  // Transactional buffer adoption (panel is invalidated, no current
  // contour). Name set via the buffer.
  core::geometry::AirfoilBuffer buffer{std::move(result.x), std::move(result.y),
                                       resultName};
  const GeometrySetResult set =
      session::setBufferGeometry(session, std::move(buffer));
  if (!set.success()) {
    return InteCoreStatus::invalidGeometry;
  }
  return InteCoreStatus::success;
}

// ---: TOP INTX/HALF (xfoil.f 229-252) --------------------------
namespace {

// Reads a coordinate file for INTX (aread.f AREAD with INFO=0).
// Messages correspond to the canonical AREAD 98/99 behavior
// (OPEN error/READ error incl. '*** LOAD NOT COMPLETED ***' via
// the confirmed emitters); return value false for ITYPE == 0.
bool readIntxProfile(const std::filesystem::path& path,
                     std::vector<double>& x, std::vector<double>& y,
                     std::string& name, IntxCoreStatus& status,
                     std::ostream& out) {
  const app::CoordinateFileReadResult read =
      app::DatFileReader::readCoordinateFile(path);
  if (read.elementSelectionRequired) {
    emitLoadFileOpenError(out, areadOpenErrorName(path.string()));
    status = IntxCoreStatus::readError;
    return false;
  }
  if (!read.success()) {
    switch (read.error) {
      case app::DatReadError::cannotOpen:
        emitLoadFileOpenError(out, areadOpenErrorName(path.string()));
        status = IntxCoreStatus::fileOpenError;
        break;
      case app::DatReadError::coordinateOverflow:
        emitLoadBufferOverflow(out, static_cast<int>(read.overflowLimit));
        status = IntxCoreStatus::bufferOverflow;
        break;
      default:
        emitLoadReadError(out);
        status = IntxCoreStatus::readError;
        break;
    }
    return false;
  }
  x.clear();
  y.clear();
  x.reserve(read.coordinates.size());
  y.reserve(read.coordinates.size());
  for (const app::Coordinate& point : read.coordinates) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      status = IntxCoreStatus::invalidGeometry;
      return false;
    }
    x.push_back(point.x);
    y.push_back(point.y);
  }
  name = stripSpaces(read.name);
  return true;
}

// CAIR read contract (xfoil.f INTX READ(*,1000) CHARACTER*2, FORMAT(A)):
// the first two characters of the line; missing characters count as blank.
std::string readCairField(const std::string& line) {
  std::string cair = line.substr(0, 2);
  while (cair.size() < 2) {
    cair.push_back(' ');
  }
  return cair;
}

// READ(CAIR,*,ERR=90) IP: list-directed integer from the CAIR field;
// the rest must be blank (canonical READ error otherwise goes to 90).
bool parseCairIndex(const std::string& cair, int& value) {
  std::istringstream stream(cair);
  int parsed = 0;
  if (!(stream >> parsed)) {
    return false;
  }
  std::string rest;
  stream >> rest;
  if (!rest.empty()) {
    return false;
  }
  value = parsed;
  return true;
}

}  // namespace

IntxCoreStatus handleIntxCore(session::SessionComposition& session,
                              std::istream& in, std::ostream& out) {
  // Source menu (xfoil.f Format 1100 with Name, Format 1200 per polar slot
  // with NXYPOL > 0). NPOL = number of slots.
  emitInteMenu(out, session.geometry.name);
  const int npol = static_cast<int>(session.polar.slots.size());
  for (int ip = 1; ip <= npol; ++ip) {
    const core::polar::PolarDataset& slot =
        session.polar.slots[static_cast<std::size_t>(ip - 1)];
    if (!slot.geometry.x.empty()) {
      emitIntxPolarSlot(out, ip, slot.metadata.airfoilName);
    }
  }

  std::vector<double> x0;
  std::vector<double> y0;
  std::vector<double> x1;
  std::vector<double> y1;
  std::string name0;
  std::string name1;
  for (int source = 0; source < 2; ++source) {
    std::vector<double>* x = (source == 0) ? &x0 : &x1;
    std::vector<double>* y = (source == 0) ? &y0 : &y1;
    std::string* name = (source == 0) ? &name0 : &name1;
    for (;;) {
      emitIntxSourcePrompt(out, source, npol);
      std::string line;
      if (!std::getline(in, line)) {
        // READ(*,1000,END=90): EOF goes to 90 (Invalid response).
        emitIntxInvalidResponse(out);
        return IntxCoreStatus::invalidSelection;
      }
      const std::string cair = readCairField(line);
      if (cair == "  ") {
        // IF (CAIR .EQ. ' ') GO TO 90
        emitIntxInvalidResponse(out);
        return IntxCoreStatus::invalidSelection;
      }
      const char first = cair[0];
      if (first == 'F' || first == 'f') {
        emitInteFilenamePrompt(out);
        std::string filename;
        if (!std::getline(in, filename)) {
          return IntxCoreStatus::inputAborted;
        }
        IntxCoreStatus status = IntxCoreStatus::success;
        if (!readIntxProfile(std::filesystem::path{filename}, *x, *y, *name,
                             status, out)) {
          // AREAD ITYPE == 0: canonical silent return to the main menu.
          return status;
        }
        break;
      }
      if (first == 'C' || first == 'c') {
        const core::paneling::PanelGeometry& panel = session.paneling.panel;
        if (panel.x.size() <= 1) {
          emitInteNoCurrentAirfoil(out);
          continue;  // GO TO 20: renewed selection of the same source
        }
        *x = panel.x;
        *y = panel.y;
        *name = session.geometry.name;
        break;
      }
      int ip = 0;
      if (!parseCairIndex(cair, ip) || ip < 1 || ip > npol) {
        emitIntxInvalidResponse(out);
        return IntxCoreStatus::invalidSelection;
      }
      const core::polar::PolarDataset& slot =
          session.polar.slots[static_cast<std::size_t>(ip - 1)];
      if (slot.geometry.x.empty()) {
        // NXYPOL(IP) <= 0 -> GO TO 90
        emitIntxInvalidResponse(out);
        return IntxCoreStatus::invalidSelection;
      }
      // Fortran INTX copied the polar contour with the loop limit
      // DO I = 1, N (current panel point count) and sets NINT = NXYPOL.
      // For N /= NXYPOL Fortran would read entries of a static array lying
      // outside the active contour (undefined
      // state). Controlled, documented mapping: the complete active
      // contour of the slot (NXYPOL entries) is
      // adopted; the polar state itself stays unchanged.
      *x = slot.geometry.x;
      *y = slot.geometry.y;
      *name = slot.metadata.airfoilName;
      break;
    }
  }

  emitInteAirfoilNames(out, name0, name1);

  double frac = 0.5;
  if (!askRealInte(in, out, "Specify interpolating fraction  0...1", frac)) {
    return IntxCoreStatus::inputAborted;
  }

  // INTERX + SCALC/SEGSPL/GEOPAR + pure buffer mutation (panel, current,
  // CPI/CPV, wake, BL, polar, inverse remain unchanged). Error without
  // buffer mutation.
  const session::BufferOnlyGeometryResult applied =
      session::applyIntxBufferInterpolation(session, std::move(x0),
                                            std::move(y0), std::move(x1),
                                            std::move(y1), frac);
  if (!applied.ok) {
    return IntxCoreStatus::interpolationFailed;
  }

  // GEOPAR output (xgeom.f Format 1000, leading space, no
  // '/'): appears after the FRAC prompt on the same line.
  emitThicknessCamber(out, applied.bufferMetrics.thick,
                      applied.bufferMetrics.thickX, applied.bufferMetrics.cambr,
                      applied.bufferMetrics.cambrX);

  // Name prompt (canonical only after the buffer mutation; EOF aborts
  // in a controlled way; the mutated buffer stays as in Fortran).
  emitAsksPrompt(out, "Enter new airfoil name");
  std::string resultName;
  if (!std::getline(in, resultName)) {
    return IntxCoreStatus::inputAborted;
  }
  session::setGeometryName(session, resultName);

  emitInteResultPlaced(out);
  return IntxCoreStatus::success;
}

namespace {
void emitGeoparMetrics(std::ostream& out,
                       const core::geometry::GeometryMetrics& m);
}  // namespace

HalfCoreStatus handleHalfCore(session::SessionComposition& session,
                              std::ostream& out) {
  // TOP-HALF (xfoil.f 242-252): SCALC/SEGSPL/GEOPAR; GEOPAR writes
  // Format 1000 (xgeom.f 377-379) on the output file. No ABCOPY,
  // no PANGEN, no invalidation.
  const session::BufferOnlyGeometryResult applied =
      session::applyHalfBufferDecimation(session);
  if (!applied.ok) {
    if (!session.geometry.valid ||
        session.geometry.buffer.x.size() < 2) {
      return HalfCoreStatus::bufferUnavailable;
    }
    return HalfCoreStatus::decimationFailed;
  }
  emitGeoparMetrics(out, applied.bufferMetrics);
  return HalfCoreStatus::applied;
}

// ---: PLOP menu (OPLSET, plutil.f 21-176) ----------------------
namespace {

// ASKR emulation (userio.f 1000 Format(/A,' r> ',$)): empty input
// keeps the default (canonical: LINE.EQ.' ' -> RINPUT unchanged),
// an invalid number is re-asked without message (READ ERR=10 -> GO TO 10),
// EOF aborts in a controlled way.
bool askRealOption(std::istream& in, std::ostream& out,
                   const std::string& promptText, double& value) {
  const std::size_t caret = promptText.find('^');
  const std::string prompt = caret == std::string::npos
                                 ? promptText
                                 : promptText.substr(0, caret);
  for (;;) {
    out << "\n" << prompt << "   r>  ";
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    if (stripSpaces(line).empty()) {
      return true;  // empty answer: default stays
    }
    const auto parsed = parseRealArguments(line);
    if (parsed.success() && !parsed.values.empty()) {
      value = parsed.values[0];
      return true;
    }
  }
}

// ASKI integer read (userio.f 98-...): prompt (/A,' i> ',$), READ(*,1001)
// Format(A), internal READ(*,*) conversion with repetition on error.
bool askIntOption(std::istream& in, std::ostream& out,
                  const std::string& promptText, int& value) {
  const std::size_t caret = promptText.find('^');
  const std::string prompt = caret == std::string::npos
                                 ? promptText
                                 : promptText.substr(0, caret);
  for (;;) {
    emitAskiPrompt(out, prompt);
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    const char* begin = line.c_str();
    char* end = nullptr;
    const long parsed = std::strtol(begin, &end, 10);
    if (end != begin && *end == '\0') {
      value = static_cast<int>(parsed);
      return true;
    }
  }
}

// OPLSET 'L' interactive (plutil.f 157-171): color list (FORMAT 2200) plus
// list-directed READ(*,*,ERR=20); a read error prints the list again.
bool askPlotLineColors(std::istream& in, std::ostream& out, int& top,
                       int& bottom) {
  for (;;) {
    emitPlotColorMenu(out);
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    const auto parsed = parseIntegerArguments(line);
    if (parsed.success() && parsed.values.size() >= 2) {
      top = parsed.values[0];
      bottom = parsed.values[1];
      return true;
    }
  }
}

}  // namespace

// TOP-level PLOP (xfoil.f 302-306) -> OPLSET menu loop.
// The handler mutates exclusively design/defaults session state via
// the session operations; no geometry, no panel, no analysis,
// no plot backend.
PlotOptionsCoreStatus handlePlotOptionsCore(
    session::SessionComposition& session, std::istream& in, std::ostream& out) {
  for (;;) {
    // Menu table from the current session values (Format 2000).
    PlotOptionDisplayValues display;
    display.graphics = session.design.plot.idev >= 1;
    display.size = session.defaults.values.size;
    display.aspect = session.defaults.values.plotar;
    display.xpage = session.defaults.values.xpage;
    display.ypage = session.defaults.values.ypage;
    display.xmarg = session.defaults.values.xmarg;
    display.ymarg = session.defaults.values.ymarg;
    display.font = session.defaults.values.ch;
    display.screenFraction = session.defaults.values.scrnfr;
    display.cursor = session.defaults.values.lcurs;
    display.landscape = session.design.plot.lland;
    display.color = session.defaults.idevrp == 4;
    display.colorTop = session.design.plot.icols[0];
    display.colorBottom = session.design.plot.icols[1];
    emitPlotOptionsTable(out, display);

    // ASKC menu prompt (plutil.f 75).
    emitPlotOptionsPrompt(out);
    std::string line;
    if (!std::getline(in, line)) {
      return PlotOptionsCoreStatus::endOfFile;
    }
    const ParsedCommand parsed = parseCommandLine(line);
    const char var = parsed.command.empty() ? ' ' : parsed.command[0];
    if (var == ' ' || var == '0') {
      return PlotOptionsCoreStatus::returned;  // plutil.f 76-77
    }

    // GETINT/GETFLT on the argument (plutil.f 6-13, 63-66): a
    // read error returns NINPUT=0 without abort.
    const auto reals = parseRealArguments(parsed.argument);
    const auto ints = parseIntegerArguments(parsed.argument);
    const int ninput = reals.success()
                           ? static_cast<int>(reals.values.size())
                           : 0;
    const int nint = ints.success() ? static_cast<int>(ints.values.size()) : 0;

    switch (var) {
      case 'G':  // IDEV 0<->1 (plutil.f 79-84)
        session::togglePlotGraphics(session);
        break;
      case 'S': {  // SIZE (plutil.f 86-91)
        double value = session.defaults.values.size;
        if (ninput >= 1) {
          value = reals.values[0];
        } else if (!askRealOption(in, out, "Enter size (in)^", value)) {
          return PlotOptionsCoreStatus::endOfFile;
        }
        session::setPlotSize(session, value);
        break;
      }
      case 'A': {  // PLOTAR (plutil.f 93-98)
        double value = session.defaults.values.plotar;
        if (ninput >= 1) {
          value = reals.values[0];
        } else if (!askRealOption(in, out, "Enter aspect ratio^", value)) {
          return PlotOptionsCoreStatus::endOfFile;
        }
        session::setPlotAspect(session, value);
        break;
      }
      case 'P': {  // XPAGE/YPAGE (plutil.f 100-109)
        double xpage = session.defaults.values.xpage;
        double ypage = session.defaults.values.ypage;
        if (ninput >= 2) {
          xpage = reals.values[0];
          ypage = reals.values[1];
        } else if (ninput >= 1) {
          xpage = reals.values[0];
          if (!askRealOption(in, out, "Enter page Y dimension (in)^", ypage)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
        } else {
          if (!askRealOption(in, out, "Enter page X dimension (in)^", xpage)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
          if (!askRealOption(in, out, "Enter page Y dimension (in)^", ypage)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
        }
        session::setPlotPage(session, xpage, ypage);
        break;
      }
      case 'M': {  // XMARG/YMARG (plutil.f 111-120)
        double xmarg = session.defaults.values.xmarg;
        double ymarg = session.defaults.values.ymarg;
        if (ninput >= 2) {
          xmarg = reals.values[0];
          ymarg = reals.values[1];
        } else if (ninput >= 1) {
          xmarg = reals.values[0];
          if (!askRealOption(in, out, "Enter page Y margin (in)^", ymarg)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
        } else {
          if (!askRealOption(in, out, "Enter page X margin (in)^", xmarg)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
          if (!askRealOption(in, out, "Enter page Y margin (in)^", ymarg)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
        }
        session::setPlotMargins(session, xmarg, ymarg);
        break;
      }
      case 'F': {  // CH (plutil.f 122-127)
        double value = session.defaults.values.ch;
        if (ninput >= 1) {
          value = reals.values[0];
        } else if (!askRealOption(in, out, "Enter character font size^",
                                  value)) {
          return PlotOptionsCoreStatus::endOfFile;
        }
        session::setPlotFont(session, value);
        break;
      }
      case 'W': {  // SCRNFR (plutil.f 129-134)
        double value = session.defaults.values.scrnfr;
        if (ninput >= 1) {
          value = reals.values[0];
        } else if (!askRealOption(in, out, "Enter window/screen size fraction^",
                                  value)) {
          return PlotOptionsCoreStatus::endOfFile;
        }
        session::setPlotScreenFraction(session, value);
        break;
      }
      case 'B':  // LCURS (plutil.f 136)
        session::togglePlotBlowupInput(session);
        break;
      case 'O':  // LLAND clamp, XPAGE/YPAGE swap (plutil.f 138-145)
        session::togglePlotOrientation(session);
        emitPlotOrientationSwap(out);
        break;
      case 'C':  // IDEVRP 4/2 (plutil.f 147-150)
        session::togglePlotColor(session);
        break;
      case 'L': {  // ICOLS (plutil.f 152-171)
        if (nint >= 2) {
          session::setPlotLineColors(session, ints.values[0],
                                     ints.values[1]);
        } else {
          int top = session.design.plot.icols[0];
          int bottom = session.design.plot.icols[1];
          if (!askPlotLineColors(in, out, top, bottom)) {
            return PlotOptionsCoreStatus::endOfFile;
          }
          session::setPlotLineColors(session, top, bottom);
        }
        break;
      }
      default:  // plutil.f 173
        emitPlotOptionUnrecognized(out);
        break;
    }
    // GO TO 1: print the table again (plutil.f 175).
  }
}

// ---: GDES menu (xgdes.f 21-949) -------------------------------
namespace {

constexpr double kGdesPi = 3.14159265358979323846;

// GEOPAR print (xgeom.f 377-379 Format 1000): every GDES operation that
// calls GEOPAR in Fortran prints the max-thickness/camber line from the
// working-buffer metrics. TCCALC calls SOPPS per point; the failures
// (oppositePointFailures) precede the metric print.
void emitGeoparMetrics(std::ostream& out,
                       const core::geometry::GeometryMetrics& m) {
  emitSoppsFailure(out, m.oppositePointFailures);
  emitThicknessCamber(out, m.thick, m.thickX, m.cambr, m.cambrX);
}
void emitGeoparLine(std::ostream& out,
                    const session::SessionComposition& session) {
  if (!session.design.work.has_value()) {
    return;
  }
  emitGeoparMetrics(out, session.design.work->metrics);
}

// TCBUF/TCSCAL/TCSET/HIPNT report (xtcam.f FORMAT 1000/1010): GETCAM
// SOPPS failures first, then 2.0*TYMAX (F8.4) with trailing '/'.
void emitTcbufReport(
    std::ostream& out,
    const core::geometry_design::ThicknessCamberReport& report) {
  emitSoppsFailure(out, report.oppositePointFailures);
  emitThicknessCamberReport(out, 2.0 * report.thickMax.y, report.thickMax.x,
                            report.camberMax.y, report.camberMax.x);
}

// TE gap of the buffer (xgdes.f 1233-1235 GAP).
double bufferTrailingEdgeGap(const session::SessionComposition& session) {
  const auto& b = session.geometry.buffer;
  if (b.x.size() < 2) {
    return 0.0;
  }
  const double dx = b.x.front() - b.x.back();
  const double dy = b.y.front() - b.y.back();
  return std::sqrt(dx * dx + dy * dy);
}

// AROT warning value of HIPNT (xtcam.f 1105-1115): chord angle from the LE
// (spline evaluation at SBLE) and TE midpoint.
double workChordlineAngleDeg(const session::SessionComposition& session) {
  if (!session.design.work.has_value() ||
      session.geometry.buffer.x.size() < 2) {
    return 0.0;
  }
  const auto& b = session.geometry.buffer;
  const auto& w = *session.design.work;
  const double xle = core::math::seval(w.metrics.sle, b.x, w.xp, w.s);
  const double yle = core::math::seval(w.metrics.sle, b.y, w.yp, w.s);
  const double xte = 0.5 * (b.x.front() + b.x.back());
  const double yte = 0.5 * (b.y.front() + b.y.back());
  return std::atan2(yle - yte, xte - xle) * 180.0 / kGdesPi;
}

// READR/READI (userio.f 235/204): READ(*,1000) LINE (A80), GETFLT/GETINT;
// A blank line returns N=0 with ERROR=.FALSE. (the 80 spaces of the A80 line
// are never counted as a number), the requirement value is preserved and
// processing continues. Non-numeric tokens set
// ERROR=.TRUE. -> prompt again (GO TO 70/71/72). EOF aborts.
bool readRealLine(std::istream& in, std::ostream& out,
                  const std::string& promptLine, double& value) {
  for (;;) {
    out << promptLine;
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    const auto parsed = parseRealArguments(line);
    if (!parsed.success()) {
      continue;  // GETFLT: ERROR=.TRUE.
    }
    if (!parsed.values.empty()) {
      value = parsed.values[0];
    }
    return true;
  }
}

bool readIntLine(std::istream& in, std::ostream& out,
                 const std::string& promptLine, int& value) {
  for (;;) {
    out << promptLine;
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    const auto parsed = parseIntegerArguments(line);
    if (!parsed.success()) {
      continue;  // GETINT: ERROR=.TRUE.
    }
    if (!parsed.values.empty()) {
      value = parsed.values[0];
    }
    return true;
  }
}

// LINS prompts (xgdes.f 345-349): Format(/1X,A,$), list-directed READ with
// two values; the ERR branch prints the same prompt again.
bool readLinsPair(std::istream& in, std::ostream& out,
                  const std::string& promptLine, double& a, double& b) {
  for (;;) {
    out << "\n" << promptLine;
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    const auto parsed = parseRealArguments(line);
    if (parsed.success() && parsed.values.size() >= 2) {
      a = parsed.values[0];
      b = parsed.values[1];
      return true;
    }
  }
}

// ASKL (userio.f 73-93): READ(*,1010) CHAR with Format(A) takes the
// first character of the line; y/Y -> true, n/N -> false, anything else
// (including blank line, CHAR=' ') -> GO TO 10, prompt again.
// EOF aborts in a controlled way (OK=false, Fortran: fatal EOF abort).
bool askGdesYesNo(std::istream& in, std::ostream& out,
                  const std::string& promptText, bool& ok) {
  for (;;) {
    emitAsklPrompt(out, promptText);
    std::string line;
    if (!std::getline(in, line)) {
      ok = false;
      return false;
    }
    const char first = line.empty() ? ' ' : line[0];
    if (first == 'y' || first == 'Y') {
      ok = true;
      return true;
    }
    if (first == 'n' || first == 'N') {
      ok = true;
      return false;
    }
  }
}

// CANG full output (IPRINT=2, xgeom.f 1123-1143) with buffer coordinates.
void emitCangListing(std::ostream& out,
                     const session::SessionComposition& session,
                     const core::geometry::CornerAngleReport& report) {
  const auto& b = session.geometry.buffer;
  emitCangHeader(out);
  for (std::size_t i = 0; i < report.angles.size(); ++i) {
    const std::size_t point = i + 1;  // Fortran I = i+2 (1-based)
    emitCangRow(out, static_cast<int>(point + 1), b.x[point], b.y[point],
                report.angles[i]);
  }
  const std::size_t maxPoint = report.maxIndex;
  emitMaxPanelCornerAngle(out, report.maxAngle,
                          static_cast<int>(maxPoint + 1), b.x[maxPoint],
                          b.y[maxPoint]);
}

}  // namespace

GeometryDesignCoreStatus handleGeometryDesignCore(
    session::SessionComposition& session, std::istream& in,
    std::ostream& out) {
  // Entry (xgdes.f 40-51).
  const auto init = session::initializeGeometryDesign(session);
  if (init.status == session::GeometryDesignInitStatus::noAirfoil) {
    emitGdesNoAirfoil(out);
    return GeometryDesignCoreStatus::returned;
  }
  emitGdesEntry(out);

  // COMOLD/ARGOLD (xgdes.f 18, 41-42, 57-58): Fortran SAVE locals
  // outlive GDES calls; the session state models them.
  std::optional<std::pair<std::string, std::string>> redo =
      session.design.menu.gdesRedo;

  for (;;) {
    emitGdesPrompt(out, session.design.lgSym);
    std::string line;
    if (!std::getline(in, line)) {
      session.design.menu.gdesRedo = redo;
      return GeometryDesignCoreStatus::endOfFile;
    }
    ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      // '<Return>'-Exit (xgdes.f 72-84).
      const auto exit = session::leaveGeometryDesignWithoutCommit(session);
      if (!exit.lgsame) {
        emitGdesExitWarning(out);
      }
      session.design.menu.gdesRedo = redo;
      return GeometryDesignCoreStatus::returned;
    }

    // '!' redo (xgdes.f 59-70): COMOLD/ARGOLD are adopted and the
    // command processing runs again.
    if (parsed.command == "!") {
      if (!redo.has_value()) {
        emitGdesPreviousNotValid(out);
        continue;
      }
      parsed = parseCommandLine(redo->first +
                                (redo->second.empty() ? "" : " " + redo->second));
    }

    const std::string& cmd = parsed.command;
    const auto reals = parseRealArguments(parsed.argument);
    const auto ints = parseIntegerArguments(parsed.argument);
    const int ninput =
        reals.success() ? static_cast<int>(reals.values.size()) : 0;
    const int nint =
        ints.success() ? static_cast<int>(ints.values.size()) : 0;

    bool redoValid = true;
    bool eofRequested = false;

    // ASKR template: aborts the menu loop on EOF in a controlled way.
    const auto askReal = [&](const std::string& promptText,
                             double& value) -> bool {
      if (!askRealOption(in, out, promptText, value)) {
        eofRequested = true;
        return false;
      }
      return true;
    };

    if (cmd == "?") {
      emitGdesHelp(out);
    } else if (cmd == "GSET") {
      const auto gset = session::gdesSetBufferFromCurrent(session);
      if (gset.ok) {
        // xgdes.f 188: GEOPAR print; with active LGSYM 194: ZERCAM with
        // THKCAM (SOPPS warnings, GEOPAR) and a second GEOPAR.
        emitGeoparMetrics(out, gset.metricsAfterGeopar);
        if (gset.zercam.has_value()) {
          emitZeroCamber(out);
          emitSoppsFailure(out, gset.zercam->thkcamOppositePointFailures);
          emitGeoparMetrics(out, gset.zercam->metricsAfterThkcam);
          emitGeoparLine(out, session);
        }
      }
    } else if (cmd == "SYMM") {
      const auto sym = session::gdesToggleSymmetry(session);
      if (sym.lgSym) {
        // xgdes.f 207-208 + xtcam.f 855-867: ZERCAM text, THKCAM
        // (SOPPS warnings + GEOPAR), YSYM, second GEOPAR.
        emitSymmetryEnabled(out);
        emitZeroCamber(out);
        if (sym.zercam.has_value()) {
          emitSoppsFailure(out, sym.zercam->thkcamOppositePointFailures);
          emitGeoparMetrics(out, sym.zercam->metricsAfterThkcam);
        }
        emitGeoparLine(out, session);
      } else {
        emitSymmetryDisabled(out);
      }
    } else if (cmd == "ADEG" || cmd == "ARAD") {
      double arad = 0.0;
      if (cmd == "ADEG") {
        double adeg = 0.0;
        if (ninput >= 1) {
          adeg = reals.values[0];
        } else if (!askReal("Enter angle change (deg)^", adeg)) {
          arad = 0.0;
        }
        arad = adeg * kGdesPi / 180.0;
      } else if (ninput >= 1) {
        arad = reals.values[0];
      } else {
        if (!askReal("Enter angle change (rad)^", arad)) {
          arad = 0.0;
        }
      }
      if (!eofRequested) {
        (void)session::gdesRotate(session, arad);
      }
    } else if (cmd == "TRAN" || cmd == "T") {
      double delx = 0.0;
      double dely = 0.0;
      if (ninput >= 2) {
        delx = reals.values[0];
        dely = reals.values[1];
      } else if (ninput >= 1) {
        delx = reals.values[0];
        if (askReal("Enter delta(y)^", dely)) {
          (void)session::gdesTranslate(session, delx, dely);
        }
      } else if (askReal("Enter delta(x)^", delx) &&
                 askReal("Enter delta(y)^", dely)) {
        (void)session::gdesTranslate(session, delx, dely);
      }
    } else if (cmd == "SCAL" || cmd == "S") {
      double xxfac = 1.0;
      double yyfac = 1.0;
      double fac = 1.0;
      if (ninput >= 1) {
        fac = reals.values[0];
        xxfac = fac;
        yyfac = fac;
      } else if (askReal("Enter scale factor (0 for separate x,y scales)^",
                         fac)) {
        xxfac = fac;
        yyfac = fac;
      }
      if (!eofRequested && fac == 0.0) {
        if (ninput >= 3) {
          xxfac = reals.values[1];
          yyfac = reals.values[2];
        } else {
          xxfac = 1.0;
          yyfac = 1.0;
          if (askReal("Enter x scale factor^", xxfac)) {
            (void)askReal("Enter y scale factor^", yyfac);
          }
        }
      }
      if (!eofRequested) {
        (void)session::gdesScale(session, xxfac, yyfac);
        emitGeoparLine(out, session);
      }
    } else if (cmd == "LINS") {
      double xoc1 = 0.0;
      double yfac1 = 0.0;
      double xoc2 = 0.0;
      double yfac2 = 0.0;
      bool applied = false;
      while (!eofRequested && !applied) {
        if (ninput >= 4) {
          xoc1 = reals.values[0];
          yfac1 = reals.values[1];
          xoc2 = reals.values[2];
          yfac2 = reals.values[3];
          applied = true;
        } else if (readLinsPair(in, out,
                                " Location 1...  enter  x/c, y-scale :  ",
                                xoc1, yfac1) &&
                   readLinsPair(in, out,
                                " Location 2...  enter  x/c, y-scale :  ",
                                xoc2, yfac2)) {
          if (std::abs(xoc1 - xoc2) < 1.0e-5) {
            out << " x/c locations 1 and 2 must be different\n";
            continue;
          }
          applied = true;
        } else {
          eofRequested = true;
        }
      }
      if (applied && !eofRequested) {
        (void)session::gdesLinearYScale(session, xoc1, yfac1, xoc2, yfac2);
        emitGeoparLine(out, session);
      }
    } else if (cmd == "DERO") {
      const auto result = session::gdesDerotate(session);
      if (result.isOk()) {
        emitDerotateMessage(out, result.value() * 180.0 / kGdesPi);
        emitGeoparLine(out, session);
      }
    } else if (cmd == "TGAP") {
      double gapnew = 0.0;
      double doc = 1.0;
      bool proceed = true;
      if (ninput >= 2) {
        gapnew = reals.values[0];
        doc = reals.values[1];
      } else if (ninput >= 1) {
        gapnew = reals.values[0];
        proceed = askReal("Enter blending distance/c (0..1)^", doc);
      } else {
        emitCurrentGap(out, bufferTrailingEdgeGap(session));
        proceed = askReal("Enter new gap^", gapnew) &&
                  askReal("Enter blending distance/c (0..1)^", doc);
      }
      if (proceed && !eofRequested) {
        (void)session::gdesSetTrailingEdgeGap(session, gapnew, doc);
        emitGeoparLine(out, session);
      }
    } else if (cmd == "LERA") {
      double rfac = 1.0;
      double doc = 1.0;
      bool proceed = true;
      if (ninput >= 2) {
        rfac = reals.values[0];
        doc = reals.values[1];
      } else if (ninput >= 1) {
        rfac = reals.values[0];
        proceed = askReal("Enter blending distance/c from LE^", doc);
      } else {
        proceed =
            askReal("Enter approx. new/old LE radius scaling ratio^", rfac) &&
            askReal("Enter blending distance/c from LE^", doc);
      }
      if (proceed && !eofRequested) {
        const auto result =
            session::gdesScaleLeadingEdgeRadius(session, rfac, doc);
        if (result.isOk()) {
          // xgdes.f 1355-1365: GEOPAR before 'New LE radius = '.
          emitGeoparLine(out, session);
          emitNewLeRadius(out, result.value());
        }
      }
    } else if (cmd == "TCPL") {
      session.design.lpLcam = !session.design.lpLcam;
    } else if (cmd == "TFAC") {
      if (!session.design.lpLcam) {
        emitEnablingCamberPlot(out);
        session.design.lpLcam = true;
      }
      const auto before = session::gdesThicknessCamberReport(session);
      emitTcbufReport(out, before);
      std::optional<double> tfac;
      std::optional<double> cfac;
      bool proceed = true;
      if (ninput >= 2) {
        tfac = reals.values[0];
        cfac = reals.values[1];
      } else if (ninput >= 1) {
        tfac = reals.values[0];
        if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
          cfac = 1.0;
        } else {
          double value = 1.0;
          if (askReal("Enter new/old camber    scale factor^", value)) {
            cfac = value;
          } else {
            proceed = false;
          }
        }
      } else {
        double t = 1.0;
        if (!askReal("Enter new/old thickness scale factor^", t)) {
          proceed = false;
        } else {
          tfac = t;
          if (session.design.lgSym) {
            out << " Symmetry enforced:  Maintaining zero camber.\n";
            cfac = 1.0;
          } else {
            double c = 1.0;
            if (askReal("Enter new/old camber    scale factor^", c)) {
              cfac = c;
            } else {
              proceed = false;
            }
          }
        }
      }
      if (proceed && !eofRequested) {
        const auto result =
            session::gdesScaleThicknessCamber(session, tfac, cfac);
        if (result.isOk()) {
          // xtcam.f 938-944: THKCAM (SOPPS warnings + GEOPAR print);
          // the second GETCAM pass prints no report, only its
          // SOPPS failures.
          emitSoppsFailure(out, result.value().thkcamOppositePointFailures);
          emitGeoparLine(out, session);
          emitSoppsFailure(out,
                           result.value().after.oppositePointFailures);
        }
      }
    } else if (cmd == "TSET") {
      if (!session.design.lpLcam) {
        emitEnablingCamberPlot(out);
        session.design.lpLcam = true;
      }
      const auto before = session::gdesThicknessCamberReport(session);
      emitTcbufReport(out, before);
      std::optional<double> tnew;
      std::optional<double> cnew;
      bool proceed = true;
      if (ninput >= 2) {
        tnew = reals.values[0];
        cnew = reals.values[1];
      } else if (ninput >= 1) {
        tnew = reals.values[0];
        if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
        } else {
          double c = 999.0;
          if (askReal("Enter new max  camber   <ret> to skip^", c)) {
            if (c != 999.0) {
              cnew = c;
            }
          } else {
            proceed = false;
          }
        }
      } else {
        double t = 999.0;
        if (!askReal("Enter new max thickness <ret> to skip^", t)) {
          proceed = false;
        } else {
          if (t != 999.0) {
            tnew = t;
          }
          if (session.design.lgSym) {
            out << " Symmetry enforced:  Maintaining zero camber.\n";
          } else {
            double c = 999.0;
            if (askReal("Enter new max  camber   <ret> to skip^", c)) {
              if (c != 999.0) {
                cnew = c;
              }
            } else {
              proceed = false;
            }
          }
        }
      }
      if (proceed && !eofRequested) {
        // Canonical factor determination (xtcam.f 997-1000) before THKCAM, so
        // that the 'Questionable input' branch prints at the correct
        // point; the core performs the same computation.
        double tfac = 1.0;
        double cfac = 1.0;
        if (before.thickMax.y != 0.0 && tnew.has_value()) {
          tfac = *tnew / (2.0 * before.thickMax.y);
        }
        if (before.camberMax.y != 0.0 && cnew.has_value()) {
          cfac = *cnew / before.camberMax.y;
        }
        bool questionable =
            std::abs(tfac) > 100.0 || std::abs(cfac) > 100.0;
        bool applyQuestionable = false;
        if (questionable) {
          emitQuestionableScaling(out, tfac, cfac);
          bool askOk = false;
          applyQuestionable = askGdesYesNo(in, out, "Apply scaling factors?^",
                                           askOk);
          if (!askOk) {
            eofRequested = true;
          }
        }
        if (!eofRequested) {
          const auto result = session::gdesSetThicknessCamberTargets(
              session, tnew, cnew, applyQuestionable);
          if (result.isOk()) {
            if (result.value().abortedQuestionable) {
              emitNoActionTaken(out);
            } else if (result.value().after.has_value()) {
              // xtcam.f 1015-1021: THKCAM (SOPPS warnings + GEOPAR);
              // the second GETCAM pass prints no report, only
              // its SOPPS failures.
              emitSoppsFailure(out,
                               result.value().thkcamOppositePointFailures);
              emitGeoparLine(out, session);
              emitSoppsFailure(
                  out, result.value().after->oppositePointFailures);
            }
          }
        }
      }
    } else if (cmd == "HIGH") {
      if (!session.design.lpLcam) {
        emitEnablingCamberPlot(out);
        session.design.lpLcam = true;
      }
      const double arot = workChordlineAngleDeg(session);
      if (std::abs(arot) > 1.0) {
        out << "\n\n Warning: HIGH does not work well on rotated foils\n";
        out << " Current chordline angle: " << arot << "\n";
        out << " Proceeding anyway...\n";
      }
      const auto before = session::gdesThicknessCamberReport(session);
      emitTcbufReport(out, before);
      double thpnt = 0.0;
      double chpnt = 0.0;
      bool proceed = true;
      if (ninput >= 2) {
        thpnt = reals.values[0];
        chpnt = reals.values[1];
      } else if (ninput >= 1) {
        thpnt = reals.values[0];
        if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
        } else if (!askReal("Enter new camber    highpoint x: ^", chpnt)) {
          proceed = false;
        }
      } else {
        if (!askReal("Enter new thickness highpoint x: ^", thpnt)) {
          proceed = false;
        } else if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
        } else if (!askReal("Enter new camber    highpoint x: ^", chpnt)) {
          proceed = false;
        }
      }
      if (proceed && !eofRequested) {
        (void)session::gdesMoveHighPoints(session, thpnt, chpnt);
        emitGeoparLine(out, session);
      }
    } else if (cmd == "BEND") {
      emitBendDump(out, session::gdesBendReport(session));
    } else if (cmd == "CANG") {
      emitCangListing(out, session, session::gdesCangReport(session));
    } else if (cmd == "CLIS") {
      const auto report = session::gdesClisReport(session);
      emitClisHeader(out);
      for (std::size_t i = 0; i < report.rows.size(); ++i) {
        const auto& row = report.rows[i];
        emitClisRow(out, static_cast<int>(i + 1), row.x, row.y, row.s,
                    row.angleDeg, row.curvature);
      }
      const auto& maxRow = report.rows[report.maxIndex];
      emitMaxCurvature(out, report.maxCurvature,
                       static_cast<int>(report.maxIndex + 1), maxRow.x,
                       maxRow.y, maxRow.s);
    } else if (cmd == "CADD") {
      const auto first = session::gdesCangReport(session);
      emitCangListing(out, session, first);
      out << "\n";
      double xMin = session.geometry.buffer.x.front();
      double xMax = session.geometry.buffer.x.front();
      for (const double x : session.geometry.buffer.x) {
        xMin = std::min(xMin, x);
        xMax = std::max(xMax, x);
      }
      double atol = 0.5 * first.maxAngle;
      int ispl = 1;
      double xrf1 = xMin - 0.1 * (xMax - xMin);
      double xrf2 = xMax + 0.1 * (xMax - xMin);
      bool exitMenu = false;
      bool proceed = true;
      // xgdes.f 523-537: NINPUT (GETFLT) controls the skips in the
      // prompt branches 70/71/72. ISPL = IINPUT(2); a failed
      // GETINT (xgdes.f 103-105, IINPUT set to 0 beforehand) returns ISPL=0,
      // leaving the existing arc length S in the inline branch.
      const int isplInline = nint >= 2 ? ints.values[1] : 0;
      if (ninput <= 0 || ninput > 4) {
        proceed = readRealLine(
            in, out,
            " Enter corner angle criterion for refinement (deg):" +
                formatFortranF(8, 3, atol) + "\n",
            atol);
      } else {
        atol = reals.values[0];
      }
      if (proceed && !eofRequested) {
        const auto pad4 = [](int value) {
          std::ostringstream s;
          s << std::setw(4) << value;
          return s.str();
        };
        if (ninput <= 1 || ninput > 4) {
          for (;;) {
            proceed = readIntLine(
                in, out,
                " Enter type of spline parameter (1=uniform, 2=arclength):" +
                    pad4(ispl) + "\n",
                ispl);
            if (!proceed) {
              break;
            }
            if (ispl <= 0) {
              exitMenu = true;  // xgdes.f 548: GO TO 500
              break;
            }
            if (ispl <= 2) {
              break;
            }
            // xgdes.f 549: ISPL > 2 -> prompt again (GO TO 71).
          }
        } else {
          ispl = isplInline;
        }
      }
      if (proceed && !exitMenu && !eofRequested) {
        if (ninput <= 2 || ninput > 4) {
          proceed = readRealLine(
              in, out,
              " Enter refinement x limits:" + formatFortranF(10, 5, xrf1) +
                  formatFortranF(10, 5, xrf2) + "\n",
              xrf1);
        } else {
          xrf1 = reals.values[2];
          xrf2 = reals.values[3];
        }
      }
      if (exitMenu) {
        const auto exit = session::leaveGeometryDesignWithoutCommit(session);
        if (!exit.lgsame) {
          emitGdesExitWarning(out);
        }
        session.design.menu.gdesRedo = redo;
        return GeometryDesignCoreStatus::returned;
      }
      if (proceed && !eofRequested) {
        const auto result =
            session::gdesAddCornerPoints(session, atol, ispl, xrf1, xrf2);
        if (result.isOk()) {
          if (result.value().refineStatus ==
              core::math::Diagnostic::limitReached) {
            // xgeom.f AREFINE 88-89: 'SDOUBLE: Arrays will overflow.
            // No action taken.', NNEW=0. Fortran then writes the
            // negative point count and continues with NB=0 in SEGSPL/GEOPAR
            // further; the core reports the canonical texts but does
            // not run the GEOPAR/CANG report with an empty contour
            // (cpp-safety deviation, no OOB accesses, no
            // invented text).
            emitSdoubleOverflow(out);
            out << " Number of points added: " << std::setw(12)
                << static_cast<int>(result.value().nbAdded) << "\n";
          } else {
          // xgdes.f 569: list-directed WRITE; integer field width 12.
          out << " Number of points added: " << std::setw(12)
              << static_cast<int>(result.value().nbAdded) << "\n";
          emitGeoparLine(out, session);
          const auto after = session::gdesCangReport(session);
          const auto& b = session.geometry.buffer;
          emitMaxPanelCornerAngle(out, after.maxAngle,
                                  static_cast<int>(after.maxIndex + 1),
                                  b.x[after.maxIndex], b.y[after.maxIndex]);
          }
        }
      }
    } else if (cmd == "FLAP" || cmd == "F") {
      double xbf = -999.0;
      double ybf = -999.0;
      if (ninput >= 2) {
        xbf = reals.values[0];
        ybf = reals.values[1];
      }
      bool proceed = true;
      if (xbf == -999.0) {
        proceed = askReal("Enter flap hinge x location^", xbf);
      }
      if (proceed && !eofRequested) {
        const auto hinge = session::gdesFlapHinge(session, xbf, ybf);
        emitFlapSurfaces(out, hinge.topy, hinge.boty);
        if (ybf == -999.0) {
          proceed = askReal(
              "Enter flap hinge y location (or 999 to specify y/t)^", ybf);
        }
        if (proceed && !eofRequested && ybf == 999.0) {
          double yrel = 0.0;
          if (askReal("Enter flap hinge relative y/t location^", yrel)) {
            ybf = hinge.topy * yrel + hinge.boty * (1.0 - yrel);
          } else {
            proceed = false;
          }
        }
        if (proceed && !eofRequested) {
          emitFlapHinge(out, xbf, ybf);
          double ddef = 0.0;
          if (ninput >= 3) {
            ddef = reals.values[2];
          } else {
            proceed =
                askReal("Enter flap deflection in degrees (+ down)^", ddef);
          }
          if (proceed && !eofRequested) {
            const auto result =
                session::gdesDeflectFlap(session, xbf, ybf, ddef);
            if (result.isOk()) {
              const auto& r = result.value();
              // xgdes.f 1469-1470: RDEF == 0 -> early return before the
              // break-point/GEOPAR/symmetry output.
              if (ddef != 0.0) {
                emitFlapBreaks(out, r.xt1, r.yt1, r.xt2, r.yt2, r.xb1,
                               r.yb1, r.xb2, r.yb2);
                // xgdes.f 1795-1804: GEOPAR print before the
                // Symmetry message.
                emitGeoparLine(out, session);
                if (r.symmetryDisabled) {
                  emitFlapDisablingSymmetry(out);
                }
              }
            }
          }
        }
      }
    } else if (cmd == "NMOV") {
      double deln = 0.0;
      bool proceed = true;
      if (ninput >= 1) {
        deln = reals.values[0];
      } else {
        proceed = askReal("Enter normal movement (+ outward)^", deln);
      }
      if (proceed && !eofRequested) {
        (void)session::gdesMoveNormals(session, deln);
        emitGeoparLine(out, session);
      }
    } else if (cmd == "UNIT") {
      (void)session::gdesNormalize(session);
      emitGeoparLine(out, session);
    } else if (cmd == "SLOP") {
      session.design.lgSlop = !session.design.lgSlop;
      if (session.design.lgSlop) {
        out << " Modified segment will be made tangent at endpoints\n";
      } else {
        out << " Modified segment will not be made tangent at endpoints\n";
      }
    } else if (cmd == "NAME") {
      if (!parsed.argument.empty()) {
        session::setGeometryName(session, parsed.argument);
      } else {
        const std::string nameDefault =
            session::geometryNameVersionDefault(session, 0, -1);
        emitNamePrompt(out, nameDefault);
        std::string answer;
        if (!std::getline(in, answer)) {
          session.design.menu.gdesRedo = redo;
          return GeometryDesignCoreStatus::endOfFile;
        }
        if (stripSpaces(answer).empty()) {
          session::incrementGeometryName(session, 0, -1);
        } else {
          session::setGeometryName(session, answer);
        }
      }
    } else if (cmd == "NINC" || cmd == "NDEC") {
      const int kDel = cmd == "NINC" ? 1 : -1;
      const std::string nameDefault =
          session::geometryNameVersionDefault(session, kDel, 1);
      emitNamePrompt(out, nameDefault);
      std::string answer;
      if (!std::getline(in, answer)) {
        session.design.menu.gdesRedo = redo;
        return GeometryDesignCoreStatus::endOfFile;
      }
      if (stripSpaces(answer).empty()) {
        session::incrementGeometryName(session, kDel, 1);
      } else {
        session::setGeometryName(session, answer);
      }
    } else if (cmd == "MODI" || cmd == "M" || cmd == "CORN" ||
               cmd == "ADDP" || cmd == "MOVP" || cmd == "DELP" ||
               cmd == "DIST" || cmd == "D" || cmd == "WIND" || cmd == "W" ||
               cmd == "PLOT" || cmd == "P" || cmd == "INPL" ||
               cmd == "BLOW" || cmd == "B" || cmd == "RESE" || cmd == "R" ||
               cmd == "TSIZ" || cmd == "TICK" || cmd == "GRID" ||
               cmd == "GPAR" || cmd == "SIZE" || cmd == "HARD" ||
               cmd == "CPLO" || cmd == "ANNO" || cmd == "Z" || cmd == "U" ||
               cmd == "SINT") {
      out << "Plotting is not available in this build.\n";
    } else if (cmd == "EXEC" || cmd == "X") {
      // xgdes.f 197-202: EXEC/X -> ABCOPY(LCONF=.TRUE.). The handler
      // orchestrates exclusively the confirmed session use case
      // executeGeometryDesignBuffer (Preconditions NB<=1 and
      // NB>IQX-5, Commit via copyBufferToCurrent, LGSAME and
      // LBFLAP adoption); no second copy, no PANGEN.
      // Error branches mutate nothing.
      const auto exec = session::executeGeometryDesignBuffer(session);
      if (exec.bufferUnavailable) {
        emitAbcopyBufferUnavailable(out);
      } else if (exec.capacityExceeded) {
        emitAbcopyCapacityError(out, static_cast<int>(exec.maxPointCount),
                                static_cast<int>(exec.bufferPointCount));
      } else {
        emitAbcopyConfirmation(
            out, static_cast<int>(session.paneling.panel.x.size()));
      }
    } else if (cmd == "CAMB") {
      // xgdes.f 466-470: CAMB submenu (xtcam.f); after the return
      // the GDES menu loop continues with the saved COMOLD/ARGOLD
      // as is.
      const auto camb = handleCamberDesignCore(session, in, out);
      if (camb == CamberDesignCoreStatus::endOfFile) {
        // xgdes.f 940-941: COMOLD/ARGOLD of the last executed
        // command (CAMB) also stay stored on EOF abort.
        session.design.menu.gdesRedo =
            std::make_pair(parsed.command, parsed.argument);
        return GeometryDesignCoreStatus::endOfFile;
      }
    } else if (cmd == "OVER" || cmd == "O") {
      // xgdes.f 2262-2367 OVER: filename (inline ARG, ONAME default with
      // Format 1100, otherwise ASKS), AREAD(INFO=1) via the existing
      // structured reader, ONAME update only after a
      // successful read (2290-2293), LNORM/NORM or SCALC/SEGSPL,
      // GEOPAR report on the overlay contour. The canonical
      // parameter save/restore sequence is functionally pure in the core:
      // buffer, panel, analysis and metrics of the session are not
      // touched; only ONAME stays persistent. The plot part
      // (PLTINI/PLTAIR) prints no text in Fortran and is still
      // omitted.
      std::string fname;
      if (!parsed.argument.empty()) {
        fname = parsed.argument.substr(0, 64);
      } else if (!session.design.oname.empty()) {
        emitGdesOverlayDefaultPrompt(out, session.design.oname);
        std::string answer;
        if (!std::getline(in, answer)) {
          session.design.menu.gdesRedo = redo;
          return GeometryDesignCoreStatus::endOfFile;
        }
        if (answer.size() > 64) {
          answer.resize(64);
        }
        fname = stripSpaces(answer);
        if (fname.empty()) {
          fname = session.design.oname;
        }
      } else {
        emitAsksPrompt(out, "Enter filename");
        std::string answer;
        if (!std::getline(in, answer)) {
          session.design.menu.gdesRedo = redo;
          return GeometryDesignCoreStatus::endOfFile;
        }
        if (answer.size() > 64) {
          answer.resize(64);
        }
        fname = answer;
        if (fname.empty()) {
          // Fortran quirk: an empty ASKS prompt leaves FNAME blank; AREAD
          // then reads without OPEN of the unopened LU=9 and
          // ends in the READ error (aread.f 11-12, 99); ONAME stays
          // unchanged.
          emitLoadReadError(out);
          continue;
        }
      }
      const std::filesystem::path path{stripSpaces(fname)};
      int selectedElement = 0;
      bool kennzeilePrinted = false;
      auto read =
          app::DatFileReader::readCoordinateFile(
              path, selectedElement,
              static_cast<int>(xfoil::limits::overlayCoordinateCapacity));
      while (read.elementSelectionRequired) {
        // aread.f 90-95, 121-124: MSES marker line before the ASKI
        // element query; the reader stays silent, the handler prints.
        emitAirfoilFileInfo(out, AirfoilFileInfoContext::msesMulti,
                            read.name, 32);
        kennzeilePrinted = true;
        int element = 0;
        if (!askIntOption(in, out, "Enter element number", element)) {
          session.design.menu.gdesRedo = redo;
          return GeometryDesignCoreStatus::endOfFile;
        }
        selectedElement = element;
        read = app::DatFileReader::readCoordinateFile(
              path, selectedElement,
              static_cast<int>(xfoil::limits::overlayCoordinateCapacity));
      }
      if (!read.success()) {
        // ITYPE=0: return to the menu loop without ONAME update.
        if (read.error == app::DatReadError::coordinateOverflow &&
            read.format != app::CoordinateFileFormat::none &&
            !kennzeilePrinted) {
          // aread.f INFO=1: the marker line precedes the traversal message
          // (Format already recognized).
          AirfoilFileInfoContext pre = AirfoilFileInfoContext::plain;
          if (read.format == app::CoordinateFileFormat::labeled) {
            pre = AirfoilFileInfoContext::labeled;
          } else if (read.format == app::CoordinateFileFormat::msesSingle ||
                     read.format == app::CoordinateFileFormat::msesMulti) {
            pre = AirfoilFileInfoContext::msesMulti;
          }
          emitAirfoilFileInfo(out, pre, read.name, 32);
        }
        switch (read.error) {
          case app::DatReadError::cannotOpen:
            emitLoadFileOpenError(out, areadOpenErrorName(path.string()));
            break;
          case app::DatReadError::coordinateOverflow:
            emitLoadBufferOverflow(out, static_cast<int>(read.overflowLimit));
            break;
          case app::DatReadError::elementNotFound:
            emitLoadElementNotFound(out);
            break;
          default:
            emitLoadReadError(out);
            break;
        }
        continue;
      }
      session.design.oname = fname.substr(0, 64);

      AirfoilFileInfoContext info = AirfoilFileInfoContext::plain;
      if (read.format == app::CoordinateFileFormat::labeled) {
        info = AirfoilFileInfoContext::labeled;
      } else if (read.format == app::CoordinateFileFormat::msesSingle) {
        info = AirfoilFileInfoContext::msesSingle;
      } else if (read.format == app::CoordinateFileFormat::msesMulti) {
        info = AirfoilFileInfoContext::msesMulti;
      }
      const bool hasKennzeileName =
          info == AirfoilFileInfoContext::labeled ||
          info == AirfoilFileInfoContext::msesSingle ||
          info == AirfoilFileInfoContext::msesMulti;
      if (!kennzeilePrinted) {
        emitAirfoilFileInfo(
            out, info,
            hasKennzeileName ? std::string_view{read.name}
                             : std::string_view{},
            32);
      }

      std::vector<double> x;
      std::vector<double> y;
      x.reserve(read.coordinates.size());
      y.reserve(read.coordinates.size());
      for (const app::Coordinate& point : read.coordinates) {
        x.push_back(point.x);
        y.push_back(point.y);
      }
      // cpp-safety deviation: Fortran would run into SEGSPL/GEOPAR with
      // N < 2; the core deliberately does not run the GEOPAR report
      // (no session mutation, no invented text).
      if (x.size() >= 2) {
        std::vector<double> xp(x.size(), 0.0);
        std::vector<double> yp(x.size(), 0.0);
        std::vector<double> s(x.size(), 0.0);
        if (session::normalizeOnLoad(session)) {
          // LNORM as TOP-level NORM: the same confirmed core
          // transformation, exactly once, only on the overlay contour.
          core::geometry::norm(x, xp, y, yp, s);
        } else {
          core::math::scalc(x, y, s);
          core::math::segspl(x, s, xp);
          core::math::segspl(y, s, yp);
        }
        const auto metrics = core::geometry::geopar(x, xp, y, yp, s);
        emitLefindNotFound(out, metrics.lefindFailures);
        emitSoppsFailure(out, metrics.oppositePointFailures);
        emitGeoparMetrics(out, metrics);
      }
    } else {
      emitGdesUnknown(out, cmd);
      redoValid = false;  // xgdes.f 946: COMAND = '****'
    }

    if (eofRequested) {
      session.design.menu.gdesRedo = redo;
      return GeometryDesignCoreStatus::endOfFile;
    }

    // COMOLD/ARGOLD for the next '!' update (xgdes.f 57-58);
    // an unknown command deletes the predecessor (COMAND='****').
    if (cmd != "!") {
      if (redoValid) {
        redo = std::make_pair(parsed.command, parsed.argument);
      } else {
        redo = std::nullopt;
      }
    }
  }
}

// Block-200 report (xtcam.f 136-156): header + TCBUF + incidence report.
void emitCambBlock200(std::ostream& out,
                      const session::SessionComposition& session) {
  const auto r = session::camberBlock200Report(session);
  emitCambBlock200Header(out);
  emitSoppsFailure(out, r.distribution.oppositePointFailures);
  // xtcam.f 1130-1131: GETMAX(XCM) before GETMAX(XTK); each failed
  // Newton iteration writes the canonical diagnostics immediately.
  if (r.camberMaxFailed) emitCambGetmaxFailure(out);
  if (r.thicknessMaxFailed) emitCambGetmaxFailure(out);
  emitTcbufPlain(out, 2.0 * r.thicknessMax.y, r.thicknessMax.x,
                 r.camberMax.y, r.camberMax.x);
  emitCambIncidenceReport(out, r.aleDeg, r.ateDeg, r.maxAddedCamberYc,
                          r.maxAddedCamberXc);
}

// INPC/INPP read block (xtcam.f 410-429/472-492): READ(*,1000) LINE;
// a blank line ends it; a faulty line prints 'try again' and re-reads
// the same digit. Two list-directed reals per line.
// Loop limit 2*IQX = 740 (xtcam.f 414/472 DO I=1,2*IQX): after 740
// points the READ chain ends without consuming the next line.
bool readCamberInputPoints(std::istream& in, std::ostream& out,
                           std::vector<core::geometry_design::CamberInputPoint>&
                                points,
                           std::size_t maxPoints) {
  for (;;) {
    if (points.size() >= maxPoints) {
      return true;  // DO loop ends, next line is a menu command
    }
    std::string line;
    if (!std::getline(in, line)) {
      return false;  // EOF: Fortran aborts the READ chain
    }
    if (stripSpaces(line).empty()) {
      return true;  // '<cr> ends input'
    }
    const auto values = parseRealArguments(line);
    if (!values.success() || values.values.size() < 2) {
      emitCambTryAgain(out);
      continue;
    }
    points.push_back({values.values[0], values.values[1]});
  }
}

CamberDesignCoreStatus handleCamberDesignCore(
    session::SessionComposition& session, std::istream& in,
    std::ostream& out) {
  // Entry (xgdes.f 487-492 + xtcam.f 43-77).
  const auto entry = session::initializeCamberDesign(session);
  if (entry.noBuffer) {
    return CamberDesignCoreStatus::returned;
  }
  if (entry.symmetryDisabled) {
    emitCambDisablingSymmetry(out);
  }
  if (entry.enablingCamberPlot) {
    emitCambEnablingCamberPlot(out);
  }
  if (entry.rotationWarning) {
    emitCambRotationWarning(out, entry.arot);
  }
  emitCambHelp(out);
  // 100-block (in the entry use case) falls through to the 200-block.
  emitCambBlock200(out, session);

  // COMOLD/ARGOLD (xtcam.f 44-51, 655-656): reset per CAMB call
  // (local, no SAVE as in GDES); exclusively SCAL sets them
  // (canonical '!' quirk). The session state holds the value only
  // within the active CAMB menus.
  std::optional<std::pair<std::string, std::string>> redo;

  for (;;) {
    emitCambPrompt(out);
    std::string line;
    if (!std::getline(in, line)) {
      session.design.menu.cambRedo = redo;
      return CamberDesignCoreStatus::endOfFile;
    }
    ParsedCommand parsed = parseCommandLine(line);
    if (parsed.empty) {
      // '<Return>'-Exit (xtcam.f 181-188): LGPARM = LGPARSAVE.
      session::leaveCamberDesign(session);
      session.design.menu.cambRedo = std::nullopt;
      return CamberDesignCoreStatus::returned;
    }

    if (parsed.command == "!") {
      if (!redo.has_value()) {
        emitCambPreviousNotValid(out);
        continue;
      }
      parsed = parseCommandLine(redo->first +
                                (redo->second.empty() ? "" : " " + redo->second));
    }

    const std::string& cmd = parsed.command;
    const auto reals = parseRealArguments(parsed.argument);
    const int ninput =
        reals.success() ? static_cast<int>(reals.values.size()) : 0;

    bool eofRequested = false;
    const auto askReal = [&](const std::string& promptText,
                             double& value) -> bool {
      if (!askRealOption(in, out, promptText, value)) {
        eofRequested = true;
        return false;
      }
      return true;
    };
    // GO-TO-100 chain (TFAC/TSET/HIGH/CLR/ADD): the 100-block falls
    // through to the 200-block.
    const auto go100 = [&]() { emitCambBlock200(out, session); };

    if (cmd == "?") {
      emitCambHelp(out);
    } else if (cmd == "TFAC") {
      // TCSCAL (xtcam.f 899-950): TCBUF report, prompt chain, THKCAM.
      const auto before = session::gdesThicknessCamberReport(session);
      emitTcbufReport(out, before);
      std::optional<double> tfac;
      std::optional<double> cfac;
      bool proceed = true;
      if (ninput >= 2) {
        tfac = reals.values[0];
        cfac = reals.values[1];
      } else if (ninput >= 1) {
        tfac = reals.values[0];
        if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
          cfac = 1.0;
        } else {
          double value = 1.0;
          if (askReal("Enter new/old camber    scale factor^", value)) {
            cfac = value;
          } else {
            proceed = false;
          }
        }
      } else {
        double t = 1.0;
        if (!askReal("Enter new/old thickness scale factor^", t)) {
          proceed = false;
        } else {
          tfac = t;
          if (session.design.lgSym) {
            out << " Symmetry enforced:  Maintaining zero camber.\n";
            cfac = 1.0;
          } else {
            double c = 1.0;
            if (askReal("Enter new/old camber    scale factor^", c)) {
              cfac = c;
            } else {
              proceed = false;
            }
          }
        }
      }
      if (proceed && !eofRequested) {
        const auto result =
            session::gdesScaleThicknessCamber(session, tfac, cfac);
        if (result.isOk()) {
          emitSoppsFailure(out, result.value().thkcamOppositePointFailures);
          emitGeoparLine(out, session);
          emitSoppsFailure(out, result.value().after.oppositePointFailures);
          session::camberRefresh(session);
          go100();
        }
      }
    } else if (cmd == "TSET") {
      // TCSET (xtcam.f 953-1024).
      const auto before = session::gdesThicknessCamberReport(session);
      emitTcbufReport(out, before);
      std::optional<double> tnew;
      std::optional<double> cnew;
      bool proceed = true;
      if (ninput >= 2) {
        tnew = reals.values[0];
        cnew = reals.values[1];
      } else if (ninput >= 1) {
        tnew = reals.values[0];
        if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
        } else {
          double c = 999.0;
          if (askReal("Enter new max  camber   <ret> to skip^", c)) {
            if (c != 999.0) {
              cnew = c;
            }
          } else {
            proceed = false;
          }
        }
      } else {
        double t = 999.0;
        if (!askReal("Enter new max thickness <ret> to skip^", t)) {
          proceed = false;
        } else {
          if (t != 999.0) {
            tnew = t;
          }
          if (session.design.lgSym) {
            out << " Symmetry enforced:  Maintaining zero camber.\n";
          } else {
            double c = 999.0;
            if (askReal("Enter new max  camber   <ret> to skip^", c)) {
              if (c != 999.0) {
                cnew = c;
              }
            } else {
              proceed = false;
            }
          }
        }
      }
      if (proceed && !eofRequested) {
        double tfac = 1.0;
        double cfac = 1.0;
        if (before.thickMax.y != 0.0 && tnew.has_value()) {
          tfac = *tnew / (2.0 * before.thickMax.y);
        }
        if (before.camberMax.y != 0.0 && cnew.has_value()) {
          cfac = *cnew / before.camberMax.y;
        }
        const bool questionable =
            std::abs(tfac) > 100.0 || std::abs(cfac) > 100.0;
        bool applyQuestionable = false;
        if (questionable) {
          emitQuestionableScaling(out, tfac, cfac);
          bool askOk = false;
          applyQuestionable =
              askGdesYesNo(in, out, "Apply scaling factors?^", askOk);
          if (!askOk) {
            eofRequested = true;
          }
        }
        if (!eofRequested) {
          const auto result = session::gdesSetThicknessCamberTargets(
              session, tnew, cnew, applyQuestionable);
          if (result.isOk()) {
            if (result.value().abortedQuestionable) {
              emitNoActionTaken(out);
            } else if (result.value().after.has_value()) {
              emitSoppsFailure(out,
                               result.value().thkcamOppositePointFailures);
              emitGeoparLine(out, session);
              emitSoppsFailure(
                  out, result.value().after->oppositePointFailures);
            }
            session::camberRefresh(session);
            go100();
          }
        }
      }
    } else if (cmd == "HIGH") {
      // HIPNT (xtcam.f 1093-1250).
      const double arot = workChordlineAngleDeg(session);
      if (std::abs(arot) > 1.0) {
        out << "\n\n Warning: HIGH does not work well on rotated foils\n";
        out << " Current chordline angle: " << arot << "\n";
        out << " Proceeding anyway...\n";
      }
      const auto before = session::gdesThicknessCamberReport(session);
      emitTcbufReport(out, before);
      double thpnt = 0.0;
      double chpnt = 0.0;
      bool proceed = true;
      if (ninput >= 2) {
        thpnt = reals.values[0];
        chpnt = reals.values[1];
      } else if (ninput >= 1) {
        thpnt = reals.values[0];
        if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
        } else if (!askReal("Enter new camber    highpoint x: ^", chpnt)) {
          proceed = false;
        }
      } else {
        if (!askReal("Enter new thickness highpoint x: ^", thpnt)) {
          proceed = false;
        } else if (session.design.lgSym) {
          out << " Symmetry enforced:  Maintaining zero camber.\n";
        } else if (!askReal("Enter new camber    highpoint x: ^", chpnt)) {
          proceed = false;
        }
      }
      if (proceed && !eofRequested) {
        (void)session::gdesMoveHighPoints(session, thpnt, chpnt);
        emitGeoparLine(out, session);
        session::camberRefresh(session);
        go100();
      }
    } else if (cmd == "SETC") {
      session::camberSetCamberline(session);
      go100();
    } else if (cmd == "INPC") {
      emitCambManualCamberInput(out);
      std::vector<core::geometry_design::CamberInputPoint> points;
      const bool completed = readCamberInputPoints(
          in, out, points, 2 * xfoil::limits::surfaceNodeDimension);
      if (!completed) {
        session.design.menu.cambRedo = redo;
        return CamberDesignCoreStatus::endOfFile;
      }
      // cpp-safety-deviation: Fortran calls SEGSPL with NCADD<2
      // (OOB access); the core declines in a controlled way without changing
      // the CAMB state.
      if (points.size() >= 2) {
        session::camberInputCamberPoints(session, points);
      }
      go100();
    } else if (cmd == "INPP") {
      emitCambManualLoadingInput(out);
      std::vector<core::geometry_design::CamberInputPoint> points;
      const bool completed = readCamberInputPoints(
          in, out, points, 2 * xfoil::limits::surfaceNodeDimension);
      if (!completed) {
        session.design.menu.cambRedo = redo;
        return CamberDesignCoreStatus::endOfFile;
      }
      if (!points.empty()) {
        const auto integrals =
            session::camberInputLoadingPoints(session, points);
        emitCambDeltaClCm(out, integrals.clx, integrals.cmx);
      }
      go100();
    } else if (cmd == "CLR") {
      session::camberClear(session);
      go100();
    } else if (cmd == "SCAL") {
      double scal = 1.0;
      bool proceed = true;
      if (ninput >= 1) {
        scal = reals.values[0];
      } else if (!askReal("Enter camber scaling factor^", scal)) {
        proceed = false;
      }
      if (proceed && !eofRequested) {
        const auto integrals = session::camberScale(session, scal);
        // xtcam.f 633: print only if CLX != 0 and CMX != 0.
        if (integrals.clx != 0.0 && integrals.cmx != 0.0) {
          emitCambDeltaClCm(out, integrals.clx, integrals.cmx);
        }
        // xtcam.f 635-636: COMOLD/ARGOLD are set exclusively here;
        // '!' therefore repeats only SCAL.
        redo = std::make_pair(parsed.command, parsed.argument);
        go100();
      }
    } else if (cmd == "ADD") {
      const auto result = session::camberAdd(session);
      if (result.isOk()) {
        emitSoppsFailure(out, result.value().addLoopOppositePointFailures);
        emitGeoparMetrics(out, result.value().metrics);
        go100();
      }
    } else if (cmd == "DCPL") {
      session.design.ldCplot = !session.design.ldCplot;
      go100();
    } else if (cmd == "SLOP") {
      session.design.lcSlop = !session.design.lcSlop;
      if (session.design.lcSlop) {
        emitCambSlopTangent(out);
      } else {
        emitCambSlopNotTangent(out);
      }
    } else if (cmd == "MODC" || cmd == "MODP" || cmd == "CPLI" ||
               cmd == "BLOW" || cmd == "B" || cmd == "RESE" || cmd == "R" ||
               cmd == "SIZE" || cmd == "ANNO" || cmd == "HARD" ||
               cmd == "Z" || cmd == "U") {
      out << "Plotting is not available in this build.\n";
    } else if (cmd == "RDAC") {
      // xtcam.f 330-372: ASKS filename, OPEN OLD, header skip,
      // list-directed pairs until EOF/parse error; partial points are
      // canonically adopted. OPEN error: message + GO TO 500
      // (menu loop continues).
      out << "\nEnter added camber filename   s>  ";
      std::string fname;
      if (!std::getline(in, fname)) {
        session.design.menu.cambRedo = redo;
        return CamberDesignCoreStatus::endOfFile;
      }
      std::ifstream file(fname, std::ios::binary);
      if (!file) {
        out << "\n Error opening added camber file\n";
        continue;
      }
      const auto data = io::decodeAddedCamberFile(file);
      file.close();
      std::vector<core::geometry_design::CamberInputPoint> points;
      points.reserve(data.points.size());
      for (const auto& p : data.points) {
        points.push_back(core::geometry_design::CamberInputPoint{p.x, p.y});
      }
      const auto rdac = session::camberReadAddedCamber(session, points);
      if (rdac == session::CamberRdacStatus::noPointsFound) {
        // WRITE(*,*) after the open ASKS prompt: the message continues the
        // prompt line (canonical bytes of the oracle cases).
        out << " No added camber points found\n";
      }
      go100();
    } else if (cmd == "WRTT" || cmd == "WRTC") {
      // xtcam.f 257-328: inline filename or ASKS prompt; an existing
      // file leads to the overwrite question (READ(*,1000) answer, abort only
      // for 'N'/'n'); then write via the io codec and GO TO 500.
      const bool thickness = (cmd == "WRTT");
      std::string fname = parsed.argument;
      if (fname.empty()) {
        out << "\nEnter output " << (thickness ? "thickness" : "camber")
            << " filename   s>  ";
        if (!std::getline(in, fname)) {
          session.design.menu.cambRedo = redo;
          return CamberDesignCoreStatus::endOfFile;
        }
      }
 // CAMB WRTT/WRTC are subject to the
      // OutputPathPolicy; the overwrite query checks the transformed
      // path, prompt texts remain logical.
      const std::string effective =
          io::applyOutputPathPolicy(session.outputPaths, fname);
      if (std::filesystem::exists(effective)) {
        // WRITE(*,*) (empty) ends the open prompt line or produces
        // a blank line for an inline filename; then the canonical question.
        out << "\n Output file exists.  Overwrite?  Y\n";
        std::string ans;
        if (!std::getline(in, ans)) {
          session.design.menu.cambRedo = redo;
          return CamberDesignCoreStatus::endOfFile;
        }
        const char first = ans.empty() ? ' ' : ans[0];
        if (first == 'N' || first == 'n') {
          out << " Current " << (thickness ? "thickness" : "camber")
              << " not saved.\n";
          continue;
        }
      }
      const auto projection = thickness
                                  ? session::camberThicknessProjection(session)
                                  : session::camberCamberProjection(session);
      std::ofstream file(effective, std::ios::binary | std::ios::trunc);
      if (!file) {
        out << "\n Error opening " << (thickness ? "thickness" : "camber")
            << " save file\n";
        continue;
      }
      if (projection.has_value()) {
        file << (thickness
                     ? io::thicknessDistributionText(projection->name,
                                                     projection->xbch,
                                                     projection->x,
                                                     projection->y)
                     : io::camberDistributionText(projection->name,
                                                  projection->xbch,
                                                  projection->x,
                                                  projection->y));
      }
      file.close();
    } else {
      emitCambUnknown(out, cmd);
    }

    if (eofRequested) {
      session.design.menu.cambRedo = redo;
      return CamberDesignCoreStatus::endOfFile;
    }
  }
}

}  // namespace xfoil::app::cli

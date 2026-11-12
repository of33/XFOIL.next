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

// file: tests/unit/CoreGeometryHandlerTests.cpp
// Verifies the session/core geometry handlers against the
// byte-exact output of the canonical Fortran processes
// (fortran-bin/xfoil). The handlers are still not wired into the
// production command route; this test uses a local session
// composition and changes neither goldens nor the production
// route.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "app/cli/CliFortranText.h"

#include "app/AirfoilWriter.h"

#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace xfoil;

void useXfoil699PanelDefaults(session::SessionComposition& composition) {
  core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
}


// INTE call helper for the interactively controlled handler: both sources as
// F lines with file path, fraction and result name as answer lines.
app::cli::InteCoreStatus handleInteFF(session::SessionComposition& composition,
                                      std::string_view file0,
                                      std::string_view file1,
                                      std::string_view frac,
                                      std::string_view name,
                                      std::ostream& out) {
  std::istringstream in(std::string("F\n") + std::string(file0) + "\nF\n" +
                        std::string(file1) + "\n" + std::string(frac) + "\n" +
                        std::string(name) + "\n");
  return app::cli::handleInteCore(composition, in, out);
}

std::string readWholeFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return std::string();
  }
  const std::streamoff size = in.tellg();
  if (size < 0) {
    return std::string();
  }
  std::string text(static_cast<std::size_t>(size), '\0');
  in.seekg(0, std::ios::beg);
  in.read(text.data(), size);
  text.resize(static_cast<std::size_t>(in.gcount()));
  return text;
}

}  // namespace

// path to the canonical Fortran reference. Used by the LOAD and the
// PPAR origin tests.
constexpr const char* kLoadReferenceFile =
    "tests/fortran-reference/cases/geometry/load-naca2412-buffer/input/"
    "naca2412_buffer.dat";

// NACA 2412: reference block from fortran-bin/xfoil (NACA 2412 -> PANGEN).
// thickness/camber (GEOPAR buffer), buffer message (format 1200), blunt TE
// (FORMAT 1090, gap 0.00252), parameter table (FORMAT 1100).
TEST(unit, CoreNacaHandlerMatchesFortranNacaBlock) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNacaCore(composition, out, 2412);
  const std::string expected =
      " Max thickness =     0.120034  at x =   0.301\n"
      " Max camber    =     0.020000  at x =   0.398\n"
      "\n Buffer airfoil set using 399 points\n"
      "\n Blunt trailing edge.  Gap =  0.00252\n"
      "\n Paneling parameters used..."
      "\n   Number of panel nodes       240"
      "\n   Panel bunching parameter    1.000"
      "\n   TE/LE panel density ratio   0.300"
      "\n   Refined-area/LE panel density ratio    0.200"
      "\n   Top    side refined area x/c limits  1.000 1.000"
      "\n   Bottom side refined area x/c limits  1.000 1.000\n";
  CHECK_EQ(out.str(), expected);
}

// controlled original message for an invalid designation: the
// session state stays unchanged (no buffer adoption).
TEST(unit, CoreNacaHandlerIllegalDesignationKeepsState) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNacaCore(composition, out, 26000);
  CHECK_EQ(out.str(), std::string(" This designation not implemented.\n"));
  CHECK(!composition.geometry.valid);
  CHECK(!composition.paneling.panelingValid);
}

TEST(unit, CoreNacaHandlerIllegalFiveDigitDesignation) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNacaCore(composition, out, 21112);
  CHECK_EQ(out.str(),
           std::string(" Illegal 5-digit designation\n"
                       " First three digits must be 210, 220, ... 250\n"));
  CHECK(!composition.geometry.valid);
}

// core panel contour (currentAirfoilCoordinatesCore) as SAVE source: the
// labeled coordinate file written from the session panel contour is
// byte-identical to the canonical Fortran output (NACA 2412 -> SAVE).
// reference: tests/data/core-c11a-cli-reference/naca2412_save_labeled.dat
// (from fortran-bin/xfoil).
TEST(unit, CoreCurrentAirfoilCoordinatesMatchesFortranSave) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  const auto coordinates = app::cli::currentAirfoilCoordinatesCore(composition);
  CHECK_EQ(coordinates.size(), 160u);

  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::labeled;
  options.delimiter = app::CoordinateDelimiter::blank;
  options.clockwise = false;  // Fortran NACA: LCLOCK = .FALSE.
  options.name = "NACA 2412";
  options.overwriteExisting = true;

  const testing::ScopedTempDirectory work(
      "CoreCurrentAirfoilCoordinatesMatchesFortranSave");
  const auto tmp = work.makePath("save-test.dat");
  const auto written = app::writeAirfoil(tmp, coordinates, options);
  CHECK(written.success);

  const std::string gotText = readWholeFile(tmp);
  const std::string wantText = readWholeFile(
      "tests/data/core-c11a-cli-reference/naca2412_save_labeled.dat");
  CHECK_EQ(gotText, wantText);
}

// NACA 0012 (symmetric): canonical NACA->PANGEN block from fortran-bin/xfoil.
// max camber is negative zero (-0.000000) as in canonical format 1000.
TEST(unit, CoreNacaHandlerN0012MatchesFortranText) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNacaCore(composition, out, 12);
  const std::string expected =
      " Max thickness =     0.120034  at x =   0.301\n"
      " Max camber    =     0.000000  at x =   0.043\n"
      "\n Buffer airfoil set using 399 points\n"
      "\n Blunt trailing edge.  Gap =  0.00252\n"
      "\n Paneling parameters used..."
      "\n   Number of panel nodes       240"
      "\n   Panel bunching parameter    1.000"
      "\n   TE/LE panel density ratio   0.300"
      "\n   Refined-area/LE panel density ratio    0.200"
      "\n   Top    side refined area x/c limits  1.000 1.000"
      "\n   Bottom side refined area x/c limits  1.000 1.000\n";
  CHECK_EQ(out.str(), expected);
}

// NACA 23012 (five-digit): canonical block from fortran-bin/xfoil.
TEST(unit, CoreNacaHandlerNaca5N23012MatchesFortranText) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNacaCore(composition, out, 23012);
  const std::string expected =
      " Max thickness =     0.120034  at x =   0.301\n"
      " Max camber    =     0.018385  at x =   0.152\n"
      "\n Buffer airfoil set using 399 points\n"
      "\n Blunt trailing edge.  Gap =  0.00252\n"
      "\n Paneling parameters used..."
      "\n   Number of panel nodes       240"
      "\n   Panel bunching parameter    1.000"
      "\n   TE/LE panel density ratio   0.300"
      "\n   Refined-area/LE panel density ratio    0.200"
      "\n   Top    side refined area x/c limits  1.000 1.000"
      "\n   Bottom side refined area x/c limits  1.000 1.000\n";
  CHECK_EQ(out.str(), expected);
  CHECK_EQ(composition.geometry.name, std::string("NACA 23012"));
}

// success-state contract: canonical name, buffer revision incremented
// exactly once, origin pangen, panelSourceRevision == bufferRevision,
// buffer with 399 points, panel derived from this buffer.
TEST(unit, CoreNacaHandlerSetsNameRevisionAndOrigin) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNacaCore(composition, out, 2412);

  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  CHECK(composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 1u);
  CHECK_EQ(composition.geometry.buffer.x.size(), 399u);
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK(!composition.paneling.panel.x.empty());
}

// NACA uses the currently stored panel parameters (not the defaults):
// store only N 200, then NACA -> PANGEN table and panel use 200.
TEST(unit, CoreNacaHandlerUsesCurrentPanelParameters) {
  session::SessionComposition composition;
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  testing::handleLegacyNacaCore(composition, out, 2412);
  CHECK(out.str().find("Number of panel nodes       200") !=
        std::string::npos);
  CHECK_EQ(session::panelingParameters(composition).panelCount, 200);
}

// NACA invalidates dependent analysis/wake states (Fortran PANGEN sets
// LGAMU/LVCONV/LWAKE ... .FALSE.).
TEST(unit, CoreNacaHandlerInvalidatesAnalysisIncludingWake) {
  session::SessionComposition composition;
  std::ostringstream firstOut;
  testing::handleLegacyNacaCore(composition, firstOut, 2412);
  composition.viscous.systemValid = true;
  composition.viscous.wakeValid = true;
  composition.viscous.converged = true;
  composition.result.lastResult.status = xfoil::AnalysisStatus::converged;

  std::ostringstream out;
  testing::handleLegacyNacaCore(composition, out, 2412);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.result.lastResult.status !=
        xfoil::AnalysisStatus::converged);
}

// NACA on an existing session with origin abcopy: the buffer is
// replaced, the buffer revision increases, and the origin switches to pangen.
TEST(unit, CoreNacaHandlerFromAbcopyOriginReplacesBuffer) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  const std::uint64_t revBefore = composition.geometry.bufferRevision;

  std::ostringstream out;
  testing::handleLegacyNacaCore(composition, out, 2412);
  CHECK_EQ(composition.geometry.bufferRevision, revBefore + 1u);
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
}

// transactional behavior: a PANGEN error after a valid NACA generation
// (here enforced by injected capacity parameters) must leave neither a
// new buffer nor a stale or deleted panel behind.
TEST(unit, CoreNacaHandlerPangenFailureNoMutation) {
  session::SessionComposition composition;
  std::ostringstream firstOut;
  testing::handleLegacyNacaCore(composition, firstOut, 2412);  // buffer + panel valid
  const std::vector<double> bufferBefore = composition.geometry.buffer.x;
  const std::vector<double> panelBefore = composition.paneling.panel.x;
  const std::uint64_t revBefore = composition.geometry.bufferRevision;
  composition.paneling.parameters.panelCount = 601;  // test injection (>600)

  std::ostringstream out;
  testing::handleLegacyNacaCore(composition, out, 12);
  CHECK_EQ(out.str(), "Panel: Too many panels. Increase IQX\n");
  // No commit: buffer, panel, name and revision remain at the first NACA.
  CHECK(composition.geometry.buffer.x == bufferBefore);
  CHECK(composition.paneling.panel.x == panelBefore);
  CHECK_EQ(composition.geometry.bufferRevision, revBefore);
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  CHECK(composition.paneling.panelingValid);
}

// independent sessions: NACA in one session leaves the other untouched.
TEST(unit, CoreNacaHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream outA;
  testing::handleLegacyNacaCore(a, outA, 2412);
  CHECK(!b.geometry.valid);
  CHECK_EQ(a.geometry.name, std::string("NACA 2412"));
  CHECK(b.geometry.name.empty());
}

// TE line appears exactly once (no duplicate diagnostics).
TEST(unit, CoreNacaHandlerTeLineAppearsOnce) {
  session::SessionComposition composition;
  std::ostringstream out;
  testing::handleLegacyNacaCore(composition, out, 2412);
  std::size_t count = 0;
  const std::string needle = "trailing edge";
  for (std::size_t pos = out.str().find(needle); pos != std::string::npos;
       pos = out.str().find(needle, pos + needle.size())) {
    ++count;
  }
  CHECK_EQ(static_cast<int>(count), 1);
}

// deterministic full-text output: an identical NACA sequence returns
// byte-identical text.
TEST(unit, CoreNacaHandlerTextDeterministic) {
  auto runOnce = [] {
    session::SessionComposition composition;
    std::ostringstream out;
    testing::handleLegacyNacaCore(composition, out, 2412);
    return out.str();
  };
  const std::string first = runOnce();
  const std::string second = runOnce();
  const std::string third = runOnce();
  CHECK(first == second);
  CHECK(second == third);
}

// PCOP (ABCOPY(.TRUE.)): transfers buffer points as the current contour
// and emits the ABCOPY confirmation (format 1200). Reference from
// fortran-bin/xfoil (NACA 2412 -> PCOP): node count 245.
TEST(unit, CorePcopHandlerMatchesFortranAbcopyConfirmation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  std::ostringstream out;
  app::cli::handlePcopCore(composition, out);
  CHECK_EQ(out.str(),
           std::string("\n Current airfoil nodes set from buffer airfoil "
                       "nodes ( 245 )\n"));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(composition.paneling.panel.x.size(), 245u);
}

// PCOP direction is buffer->panel: the original PANGEN panel contour
// (160) is replaced by the buffer contour (NACA 2412: 245 points);
// origin abcopy, panelSourceRevision == bufferRevision, revisions unchanged.
TEST(unit, CorePcopHandlerFromPangenPanelCopiesBufferToPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);  // panel pangen (160)
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  const std::uint64_t bufferRevBefore = composition.geometry.bufferRevision;
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> bufferY = composition.geometry.buffer.y;

  std::ostringstream out;
  const app::cli::PcopCoreStatus status =
      app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PcopCoreStatus::applied));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRevBefore);
  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.geometry.buffer.y == bufferY);
  CHECK_EQ(composition.paneling.panel.x.size(), bufferX.size());
}

// after PCOP the panel contour equals the buffer (order and point count
// exact; NACA buffer without duplicated points).
TEST(unit, CorePcopHandlerPanelCoordsEqualBufferAfter) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream out;
  app::cli::handlePcopCore(composition, out);
  CHECK(composition.paneling.panel.x == composition.geometry.buffer.x);
  CHECK(composition.paneling.panel.y == composition.geometry.buffer.y);
  CHECK_EQ(composition.paneling.panel.x.size(), 245u);
}

// ABCOPY panel -> PCOP: a contour already identical to the buffer is copied
// back again; the origin stays abcopy and the buffer revision is unchanged.
TEST(unit, CorePcopHandlerFromAbcopyPanel) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);  // origin abcopy
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  const std::uint64_t bufferRevBefore = composition.geometry.bufferRevision;

  std::ostringstream out;
  const app::cli::PcopCoreStatus status =
      app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PcopCoreStatus::applied));
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRevBefore);
}

// panel parameters and airfoil name remain unchanged by PCOP; metrics
// are set from the new panel contour.
TEST(unit, CorePcopHandlerPreservesParamsAndName) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const int npanBefore = session::panelingParameters(composition).panelCount;
  const std::string nameBefore = composition.geometry.name;

  std::ostringstream out;
  app::cli::handlePcopCore(composition, out);
  CHECK_EQ(session::panelingParameters(composition).panelCount, npanBefore);
  CHECK_EQ(composition.geometry.name, nameBefore);
  CHECK(composition.geometry.metrics.has_value());
}

// PCOP invalidates the dependent analysis/wake states according to the
// ABCOPY flag block (LGAMU/LWAKE/LVCONV/... .FALSE.).
TEST(unit, CorePcopHandlerInvalidatesAnalysisDependents) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  composition.viscous.systemValid = true;
  composition.viscous.wakeValid = true;
  composition.viscous.converged = true;
  composition.result.lastResult.status = xfoil::AnalysisStatus::converged;

  std::ostringstream out;
  app::cli::handlePcopCore(composition, out);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.result.lastResult.status !=
        xfoil::AnalysisStatus::converged);
}

// after PCOP the panel contour is valid and tied to the buffer revision;
// a subsequent PANE continues from the (unchanged) buffer.
TEST(unit, CorePcopHandlerThenPaneUsesBuffer) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream pcopOut;
  app::cli::handlePcopCore(composition, pcopOut);  // panel = buffer (245)
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream paneOut;
  const app::cli::PaneCoreStatus paneStatus =
      app::cli::handlePaneCore(composition, paneOut);
  CHECK_EQ(static_cast<int>(paneStatus),
           static_cast<int>(app::cli::PaneCoreStatus::applied));
  CHECK_EQ(composition.paneling.panel.x.size(), 200u);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// repeated PCOP: the second call succeeds, the panel stays equal to the
// buffer, origin abcopy, buffer revision unchanged.
TEST(unit, CorePcopHandlerRepeated) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream firstOut;
  app::cli::handlePcopCore(composition, firstOut);
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;

  std::ostringstream out;
  const app::cli::PcopCoreStatus status =
      app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PcopCoreStatus::applied));
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK(composition.paneling.panel.x == composition.geometry.buffer.x);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
}

// No buffer: canonical ABCOPY buffer message, no mutation.
TEST(unit, CorePcopHandlerNoBufferNoMutation) {
  session::SessionComposition composition;  // fresh: no buffer
  std::ostringstream out;
  const app::cli::PcopCoreStatus status =
      app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PcopCoreStatus::bufferUnavailable));
  CHECK_EQ(out.str(), " ABCOPY: Buffer airfoil not available.\n");
  CHECK(!composition.paneling.panelingValid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(composition.paneling.panel.x.empty());
}

// capacity exceedance (NB > IQX-5 = 596, test injection): canonical
// four-line rejection, panel contour unchanged.
TEST(unit, CorePcopHandlerCapacityNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);  // panel 160
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  // Stretch the buffer above the new limit (only the capacity check matters).
  const std::size_t limit = static_cast<std::size_t>(
      core::paneling::kMaxAbcopyNodeCount);
  while (composition.geometry.buffer.x.size() <= limit) {
    composition.geometry.buffer.x.push_back(
        1.0 + 0.001 * static_cast<double>(
                        composition.geometry.buffer.x.size()));
    composition.geometry.buffer.y.push_back(0.0);
  }

  std::ostringstream out;
  const app::cli::PcopCoreStatus status =
      app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PcopCoreStatus::capacityExceeded));
  const std::string expected =
      " Maximum number of panel nodes  :          596\n"
      " Number of buffer airfoil points:          597\n"
      " Current airfoil cannot be set.\n"
      " Try executing PANE at Top Level instead.\n";
  CHECK_EQ(out.str(), expected);
  CHECK(composition.paneling.panel.x == panelXBefore);
}

// independent sessions: PCOP in one session leaves the other untouched.
TEST(unit, CorePcopHandlerIndependentSessions) {
  session::SessionComposition a;
  useXfoil699PanelDefaults(a);
  session::SessionComposition b;
  useXfoil699PanelDefaults(b);
  std::ostringstream nacaA;
  std::ostringstream nacaB;
  testing::handleLegacyNacaCore(a, nacaA, 2412);
  testing::handleLegacyNacaCore(b, nacaB, 2412);
  std::ostringstream out;
  app::cli::handlePcopCore(a, out);
  CHECK_EQ(static_cast<int>(b.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(b.paneling.panel.x.size(), 160u);
}

// deterministic text output: two independent identical PCOP runs are
// byte-identical.
TEST(unit, CorePcopHandlerTextDeterministic) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream nacaA;
  std::ostringstream nacaB;
  testing::handleLegacyNacaCore(a, nacaA, 2412);
  testing::handleLegacyNacaCore(b, nacaB, 2412);
  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handlePcopCore(a, outA);
  app::cli::handlePcopCore(b, outB);
  CHECK_EQ(outA.str(), outB.str());
}

// handler level of golden case file-inte-pcop-quit: NACA 2412 -> INTE
// (buffer 160) -> PCOP returns " (160)" (INTER point count, not NACA-245).
TEST(unit, CorePcopHandlerAfterInteMatchesGoldenCount) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream inteSink;
  handleInteFF(composition, "tests/data/core-c11a-cli-reference/inte_n0012.dat", "tests/data/core-c11a-cli-reference/inte_n2412.dat", "0.5", "inte", inteSink);
  CHECK_EQ(composition.geometry.buffer.x.size(), 160u);

  std::ostringstream out;
  const app::cli::PcopCoreStatus status =
      app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PcopCoreStatus::applied));
  CHECK_EQ(out.str(),
           std::string("\n Current airfoil nodes set from buffer airfoil "
                       "nodes ( 160 )\n"));
  CHECK_EQ(composition.paneling.panel.x.size(), 160u);
}

// PCOP from PANGEN panel (160) with buffer 245: N!=NB -> LBLINI false.
TEST(unit, CorePcopHandlerResetsLbliniFromPangenPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);  // panel pangen 160
  composition.viscous.viscals.lblini = true;  // test injection

  std::ostringstream out;
  app::cli::handlePcopCore(composition, out);
  CHECK_FALSE(composition.viscous.viscals.lblini);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.viscals.mapping.valid);
}

// PCOP from ABCOPY panel (buffer equal 245==245): N==NB -> LBLINI preserved.
TEST(unit, CorePcopHandlerPreservesLbliniFromAbcopyPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream firstOut;
  app::cli::handlePcopCore(composition, firstOut);  // panel = buffer 245
  composition.viscous.viscals.lblini = true;  // test injection

  std::ostringstream out;
  app::cli::handlePcopCore(composition, out);  // N==NB -> preserve
  CHECK(composition.viscous.viscals.lblini);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.viscals.mapping.valid);
}

// PANE (PANGEN(.TRUE.)): emits only the trailing-edge message (format
// 1090) and the parameter table (format 1100). Reference from
// fortran-bin/xfoil (NACA 2412 -> PANE, second PANGEN pass).
TEST(unit, CorePaneHandlerMatchesFortranPangenBlock) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  std::ostringstream out;
  app::cli::handlePaneCore(composition, out);
  const std::string expected =
      "\n Blunt trailing edge.  Gap =  0.00252\n"
      "\n Paneling parameters used..."
      "\n   Number of panel nodes       240"
      "\n   Panel bunching parameter    1.000"
      "\n   TE/LE panel density ratio   0.300"
      "\n   Refined-area/LE panel density ratio    0.200"
      "\n   Top    side refined area x/c limits  1.000 1.000"
      "\n   Bottom side refined area x/c limits  1.000 1.000\n";
  CHECK_EQ(out.str(), expected);
}

// PANE after N 200: the SHOPAR table shows 200, the TE gap stays
// airfoil-determined (0.00252), no CANG line. Reference from
// fortran-bin/xfoil (NACA 2412 -> PPAR -> N 200 -> Apply -> PANE).
TEST(unit, CorePaneHandlerAfterN200MatchesFortranText) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::applied));
  const std::string expected =
      "\n Blunt trailing edge.  Gap =  0.00252\n"
      "\n Paneling parameters used..."
      "\n   Number of panel nodes       200"
      "\n   Panel bunching parameter    1.000"
      "\n   TE/LE panel density ratio   0.150"
      "\n   Refined-area/LE panel density ratio    0.200"
      "\n   Top    side refined area x/c limits  1.000 1.000"
      "\n   Bottom side refined area x/c limits  1.000 1.000\n";
  CHECK_EQ(out.str(), expected);
}

// PANE (PANGEN(.TRUE.)) explicitly contains no CANG corner-angle line
// (which belongs only to PPAR Apply with PANGEN(.FALSE.)).
TEST(unit, CorePaneHandlerNoCangLine) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream out;
  app::cli::handlePaneCore(composition, out);
  CHECK(out.str().find("Maximum panel corner angle") == std::string::npos);
  CHECK(out.str().find("Paneling parameters used...") != std::string::npos);
}

// TE message appears exactly once per PANE call.
TEST(unit, CorePaneHandlerTeLineAppearsOnce) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream out;
  app::cli::handlePaneCore(composition, out);
  const std::string marker = "trailing edge.";
  const std::size_t first = out.str().find(marker);
  CHECK(first != std::string::npos);
  CHECK(out.str().find(marker, first + marker.size()) == std::string::npos);
}

// panel point count and TE gap against the Fortran PSAV reference
// (panel-coord/pane-default-psav: 160 nodes, TE gap 0.00252). The
// complete coordinate comparison lives in tests/core/PanelingTests.cpp
// (paneling_reference.txt PANE160) for the same PANGEN producer.
TEST(unit, CorePaneHandlerPanelCountAndGapMatchFortran) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream out;
  app::cli::handlePaneCore(composition, out);
  CHECK_EQ(composition.paneling.panel.x.size(), 160u);
  const double gap =
      composition.paneling.panel.y.front() - composition.paneling.panel.y.back();
  CHECK_NEAR(gap, 0.00252, 1e-5, 0.0);
}

// success-state contract: origin pangen, panelSourceRevision == bufferRevision,
// buffer and buffer revision unchanged, parameters preserved.
TEST(unit, CorePaneHandlerSetsPangenOriginAndRevision) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);  // origin abcopy
  const std::uint64_t bufferRevBefore = composition.geometry.bufferRevision;
  const std::size_t bufferPoints = composition.geometry.buffer.x.size();

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::applied));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRevBefore);
  CHECK_EQ(composition.geometry.buffer.x.size(), bufferPoints);
  CHECK_EQ(session::panelingParameters(composition).panelCount, 160);
}

// PANGEN uses the buffer, not the old panel contour: after ABCOPY (245
// buffer points as panel), PANE with NPAN=200 produces a PANGEN contour
// whose point count does not match the ABCOPY contour.
TEST(unit, CorePaneHandlerUsesBufferNotOldPanel) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::size_t abcopyPanelPoints = composition.paneling.panel.x.size();
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  app::cli::handlePaneCore(composition, out);
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK(composition.paneling.panel.x.size() != abcopyPanelPoints);
  CHECK_EQ(composition.paneling.panel.x.size(), 200u);
}

// complete analysis/wake invalidation for PANE (Fortran PANGEN sets
// LGAMU/LVCONV/LWAKE/... .FALSE.).
TEST(unit, CorePaneHandlerInvalidatesAnalysisIncludingWake) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  composition.viscous.systemValid = true;
  composition.viscous.wakeValid = true;
  composition.viscous.awake = 2.0;
  composition.viscous.converged = true;
  composition.result.lastResult.status = xfoil::AnalysisStatus::converged;

  std::ostringstream out;
  app::cli::handlePaneCore(composition, out);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.result.lastResult.status !=
        xfoil::AnalysisStatus::converged);
}

// initial origin none (INTE buffer without panel): PANE creates a PANGEN contour.
TEST(unit, CorePaneHandlerFromNoneOrigin) {
  session::SessionComposition composition;
  std::ostringstream inteOut;
  handleInteFF(composition, "tests/data/core-c11a-cli-reference/inte_n0012.dat", "tests/data/core-c11a-cli-reference/inte_n2412.dat", "0.5", "inte", inteOut);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::none));

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::applied));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// initial origin abcopy: PANE replaces the ABCOPY contour, origin pangen.
TEST(unit, CorePaneHandlerFromAbcopyOrigin) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::applied));
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// repeated PANE with unchanged parameters: origin stays pangen,
// panelSourceRevision == bufferRevision, buffer revision unchanged.
TEST(unit, CorePaneHandlerRenewedPangenStable) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream firstOut;
  app::cli::handlePaneCore(composition, firstOut);
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::applied));
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
}

// No buffer: canonical PANGEN buffer message, no mutation.
TEST(unit, CorePaneHandlerNoBufferNoMutation) {
  session::SessionComposition composition;  // fresh: no buffer
  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::bufferUnavailable));
  CHECK_EQ(out.str(), " PANGEN: Buffer airfoil not available.\n");
  CHECK(!composition.paneling.panelingValid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(composition.paneling.panel.x.empty());
}

// invalid panel parameter (panelCount<2, set directly): PANGEN core error
// without partial mutation; contour and parameters remain unchanged.
TEST(unit, CorePaneHandlerInvalidParamsNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  composition.paneling.parameters.panelCount = 1;  // test injection

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::panelingFailed));
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK_EQ(composition.paneling.parameters.panelCount, 1);
}

// capacity limit (panelCount>600, set directly): canonical
// too-many-panels diagnostic without partial mutation.
TEST(unit, CorePaneHandlerCapacityNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  composition.paneling.parameters.panelCount = 601;  // test injection (>600)

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::panelingFailed));
  CHECK_EQ(out.str(), "Panel: Too many panels. Increase IQX\n");
  CHECK(composition.paneling.panel.x == panelXBefore);
}

// error after an already valid PANGEN panel: the existing valid panel
// contour is not deleted by the error.
TEST(unit, CorePaneHandlerErrorAfterValidPangenKeepsPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream firstOut;
  app::cli::handlePaneCore(composition, firstOut);  // valid pangen-panel
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  const auto originBefore = composition.paneling.origin;
  composition.paneling.parameters.panelCount = 601;  // next call fails (>600)

  std::ostringstream out;
  const app::cli::PaneCoreStatus status =
      app::cli::handlePaneCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PaneCoreStatus::panelingFailed));
  CHECK(composition.paneling.panelingValid);
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(originBefore));
}

// independent sessions: PANE in one session leaves the other untouched.
TEST(unit, CorePaneHandlerIndependentSessions) {
  session::SessionComposition a;
  useXfoil699PanelDefaults(a);
  session::SessionComposition b;
  useXfoil699PanelDefaults(b);
  std::ostringstream nacaA;
  std::ostringstream nacaB;
  testing::handleLegacyNacaCore(a, nacaA, 2412);
  testing::handleLegacyNacaCore(b, nacaB, 2412);
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      a, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  app::cli::handlePaneCore(a, out);
  CHECK_EQ(session::panelingParameters(b).panelCount, 160);
  CHECK_EQ(static_cast<int>(b.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// deterministic text output: two independent identical PANE runs are
// byte-identical.
TEST(unit, CorePaneHandlerTextDeterministic) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream nacaA;
  std::ostringstream nacaB;
  testing::handleLegacyNacaCore(a, nacaA, 2412);
  testing::handleLegacyNacaCore(b, nacaB, 2412);
  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handlePaneCore(a, outA);
  app::cli::handlePaneCore(b, outB);
  CHECK_EQ(outA.str(), outB.str());
}

// PPAR entry (GETPAN format 1000): shows the session panel parameters.
// reference from fortran-bin/xfoil (NACA 2412 -> PPAR).
TEST(unit, CorePparShowHandlerMatchesFortranTable) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  std::ostringstream out;
  app::cli::handlePparShowCore(composition, out);
  const std::string expected =
      "\n    Present paneling parameters..."
      "\n  N  i   Number of panel nodes       240"
      "\n  P  r   Panel bunching parameter    1.000"
      "\n  T  r   TE/LE panel density ratio   0.300"
      "\n  R  r   Refined area/LE  panel density ratio   0.200"
      "\n  XT rr  Top    side refined area x/c limits    1.000 1.000"
      "\n  XB rr  Bottom side refined area x/c limits    1.000 1.000"
      "\n  Z oom\n  U nzoom\n  H ardcopy\n";
  CHECK_EQ(out.str(), expected);
}

// PPAR parameter command N (GETPAN): sets NPAN, without reprinted tables
// and without panel regeneration (canonical: GO TO 12; only effective for
// PANGEN/Apply). panel contour, origin and revisions remain unchanged.
TEST(unit, CorePparParameterNChangesNoReprintNoPangen) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);  // origin pangen

  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  const std::vector<double> panelYBefore = composition.paneling.panel.y;
  const std::uint64_t bufferRevBefore = composition.geometry.bufferRevision;
  const std::uint64_t panelSrcRevBefore =
      composition.paneling.panelSourceRevision;

  const double value = 200.0;
  std::ostringstream out;
  const app::cli::PparParameterCoreStatus status =
      app::cli::applyPparParameterCore(
          composition, core::paneling::PanelParameterKind::panelCount,
          std::span<const double>(&value, 1), out);

  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK(out.str().empty());  // no Reprint, no diagnostics
  CHECK_EQ(session::panelingParameters(composition).panelCount, 200);
  // contour and state unchanged (no PANGEN, no invalidation).
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK(composition.paneling.panel.y == panelYBefore);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRevBefore);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelSrcRevBefore);
}

// N above IQX-6 (595): canonical clamp to 595 plus format-1200 message
// (xfoil.f:2218-2221), otherwise no panel regeneration.
TEST(unit, CorePparParameterNClampEmitsFortranMessage) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  const double value = 600.0;
  std::ostringstream out;
  const app::cli::PparParameterCoreStatus status =
      app::cli::applyPparParameterCore(
          composition, core::paneling::PanelParameterKind::panelCount,
          std::span<const double>(&value, 1), out);

  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparParameterCoreStatus::appliedClamped));
  const std::string expected =
      "  Number of panel nodes reduced to array limit: 595\n";
  CHECK_EQ(out.str(), expected);
  CHECK_EQ(session::panelingParameters(composition).panelCount, 595);
}

// N < 2: controlled rejection without partial mutation (core contract).
TEST(unit, CorePparParameterNTooSmallRejectedNoMutation) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;

  const double value = 1.0;
  std::ostringstream out;
  const app::cli::PparParameterCoreStatus status =
      app::cli::applyPparParameterCore(
          composition, core::paneling::PanelParameterKind::panelCount,
          std::span<const double>(&value, 1), out);

  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparParameterCoreStatus::rejected));
  CHECK(out.str().empty());
  CHECK_EQ(session::panelingParameters(composition).panelCount, 160);
  CHECK(composition.paneling.panel.x == panelXBefore);
}

// P/T/R: each real value is stored exactly, without range checking and
// without text (Fortran ASKR accepts every real value unchanged).
TEST(unit, CorePparParameterPTeRStoreValuesNoText) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  const double p = 0.5;
  const double t = 0.25;
  const double r = 0.3;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::applyPparParameterCore(
               composition, core::paneling::PanelParameterKind::bunchingParam,
               std::span<const double>(&p, 1), out)),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK_EQ(static_cast<int>(app::cli::applyPparParameterCore(
               composition,
               core::paneling::PanelParameterKind::teToLeDensityRatio,
               std::span<const double>(&t, 1), out)),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK_EQ(static_cast<int>(app::cli::applyPparParameterCore(
               composition,
               core::paneling::PanelParameterKind::refinementDensityRatio,
               std::span<const double>(&r, 1), out)),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK(out.str().empty());
  CHECK(session::panelingParameters(composition).bunchingParam == 0.5);
  CHECK(session::panelingParameters(composition).teToLeDensityRatio == 0.25);
  CHECK(session::panelingParameters(composition).refinementDensityRatio == 0.3);
}

// XT/XB: exactly two values, stored unsorted (Fortran keeps the order
// unchanged), without text.
TEST(unit, CorePparParameterXtXbStorePairsUnsorted) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  const double xt[2] = {0.8, 0.2};  // deliberate descending
  const double xb[2] = {0.9, 0.1};
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::applyPparParameterCore(
               composition,
               core::paneling::PanelParameterKind::upperRefinementRange,
               std::span<const double>(xt, 2), out)),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK_EQ(static_cast<int>(app::cli::applyPparParameterCore(
               composition,
               core::paneling::PanelParameterKind::lowerRefinementRange,
               std::span<const double>(xb, 2), out)),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK(out.str().empty());
  const auto& params = session::panelingParameters(composition);
  CHECK(params.upperRefinementRange[0] == 0.8);
  CHECK(params.upperRefinementRange[1] == 0.2);
  CHECK(params.lowerRefinementRange[0] == 0.9);
  CHECK(params.lowerRefinementRange[1] == 0.1);
}

// XT with only one value: transactional value-pair check, no partial mutation
// (no value of the pair is stored prematurely).
TEST(unit, CorePparParameterXtPartialPairNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const auto before = session::panelingParameters(composition);

  const double single = 0.5;
  std::ostringstream out;
  const app::cli::PparParameterCoreStatus status =
      app::cli::applyPparParameterCore(
          composition, core::paneling::PanelParameterKind::upperRefinementRange,
          std::span<const double>(&single, 1), out);

  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparParameterCoreStatus::rejected));
  CHECK(out.str().empty());
  const auto& after = session::panelingParameters(composition);
  CHECK(after.upperRefinementRange[0] == before.upperRefinementRange[0]);
  CHECK(after.upperRefinementRange[1] == before.upperRefinementRange[1]);
}

// origin none: parameters can be stored without contour generation;
// origin stays none, panel stays empty, no revision updated.
TEST(unit, CorePparParameterOriginNoneStoresNoContour) {
  session::SessionComposition composition;  // fresh: no contour
  const double value = 200.0;
  std::ostringstream out;
  const app::cli::PparParameterCoreStatus status =
      app::cli::applyPparParameterCore(
          composition, core::paneling::PanelParameterKind::panelCount,
          std::span<const double>(&value, 1), out);

  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK_EQ(session::panelingParameters(composition).panelCount, 200);
  CHECK(!composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::none));
  CHECK_EQ(composition.paneling.panelSourceRevision, 0u);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(composition.paneling.panel.x.empty());
}

// origin abcopy: the ABCOPY contour stays valid and is not converted
// into a PANGEN contour; parameters change, contour/revision unchanged.
TEST(unit, CorePparParameterOriginAbcopyKeepsContour) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  const std::uint64_t bufferRevBefore = composition.geometry.bufferRevision;
  const std::uint64_t panelSrcRevBefore =
      composition.paneling.panelSourceRevision;

  const double p = 0.5;
  std::ostringstream out;
  const app::cli::PparParameterCoreStatus status =
      app::cli::applyPparParameterCore(
          composition, core::paneling::PanelParameterKind::bunchingParam,
          std::span<const double>(&p, 1), out);

  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparParameterCoreStatus::applied));
  CHECK(session::panelingParameters(composition).bunchingParam == 0.5);
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRevBefore);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelSrcRevBefore);
}

// Several changes in sequence: all values are stored cumulatively,
// throughout without text and without contour change.
TEST(unit, CorePparParameterMultipleChangesAccumulate) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;

  const double n = 250.0;
  const double p = 0.7;
  const double xt[2] = {0.3, 0.7};
  std::ostringstream out;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), out);
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::bunchingParam,
      std::span<const double>(&p, 1), out);
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::upperRefinementRange,
      std::span<const double>(xt, 2), out);

  CHECK(out.str().empty());
  const auto& params = session::panelingParameters(composition);
  CHECK_EQ(params.panelCount, 250);
  CHECK(params.bunchingParam == 0.7);
  CHECK(params.upperRefinementRange[0] == 0.3);
  CHECK(params.upperRefinementRange[1] == 0.7);
  CHECK(composition.paneling.panel.x == panelXBefore);
}

// independent sessions: a change in one session leaves the other untouched.
TEST(unit, CorePparParameterIndependentSessions) {
  session::SessionComposition a;
  useXfoil699PanelDefaults(a);
  session::SessionComposition b;
  useXfoil699PanelDefaults(b);
  std::ostringstream nacaA;
  std::ostringstream nacaB;
  testing::handleLegacyNacaCore(a, nacaA, 2412);
  testing::handleLegacyNacaCore(b, nacaB, 2412);

  const double value = 300.0;
  std::ostringstream out;
  app::cli::applyPparParameterCore(
      a, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&value, 1), out);

  CHECK_EQ(session::panelingParameters(a).panelCount, 300);
  CHECK_EQ(session::panelingParameters(b).panelCount, 160);
}

// deterministic text output: an identical command sequence returns
// byte-identical text (clamp message).
TEST(unit, CorePparParameterTextDeterministic) {
  auto runOnce = [] {
    session::SessionComposition composition;
    std::ostringstream nacaSink;
    testing::handleLegacyNacaCore(composition, nacaSink, 2412);
    const double value = 600.0;
    std::ostringstream out;
    app::cli::applyPparParameterCore(
        composition, core::paneling::PanelParameterKind::panelCount,
        std::span<const double>(&value, 1), out);
    return out.str();
  };
  const std::string first = runOnce();
  const std::string second = runOnce();
  const std::string third = runOnce();
  CHECK(first == second);
  CHECK(second == third);
  CHECK_EQ(first, "  Number of panel nodes reduced to array limit: 595\n");
}

// --- PPAR Apply/PANGEN producer ------------------------------

// PPAR Apply with default parameters (N=240): canonical text =
// PANGEN(.FALSE.) (TE line, no SHOPAR table) + CANG corner angle.
// reference from fortran-bin/xfoil (NACA 2412 -> PPAR -> N 160 -> Apply).
TEST(unit, CorePparApplyDefaultMatchesFortranText) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::applied));
  const std::string expected =
      "\n Blunt trailing edge.  Gap =  0.00252\n"
      "\n Maximum panel corner angle =  4.900   at  i,x,y  = 126"
      "   0.0007  -0.0045\n";
  CHECK_EQ(out.str(), expected);
}

// PPAR Apply after N 200: corner angle against reference case
// panel-coord/ppar-n200-psav (5.803 at index 105).
TEST(unit, CorePparApplyAfterN200MatchesFortranText) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::applied));
  CHECK(out.str().find(
            " Maximum panel corner angle =  5.803   at  i,x,y  = 105"
            "   0.0006  -0.0043\n") != std::string::npos);
  CHECK(out.str().find("Paneling parameters used...") == std::string::npos);
}

// success-state contract: origin pangen, panelSourceRevision == bufferRevision,
// buffer and buffer revision unchanged, parameters preserved.
TEST(unit, CorePparApplySetsPangenOriginAndRevision) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);  // origin abcopy
  const std::uint64_t bufferRevBefore = composition.geometry.bufferRevision;
  const std::size_t bufferPoints = composition.geometry.buffer.x.size();

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::applied));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRevBefore);
  CHECK_EQ(composition.geometry.buffer.x.size(), bufferPoints);
  CHECK_EQ(session::panelingParameters(composition).panelCount, 160);
}

// PANGEN uses the buffer, not the old panel contour: after ABCOPY (245
// buffer points as panel), Apply with NPAN=200 yields a PANGEN contour
// whose point count does not match the ABCOPY contour.
TEST(unit, CorePparApplyUsesBufferNotOldPanel) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::size_t abcopyPanelPoints = composition.paneling.panel.x.size();
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  app::cli::applyPparPanelingCore(composition, out);
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK(composition.paneling.panel.x.size() != abcopyPanelPoints);
  CHECK(!composition.paneling.panel.x.empty());
}

// complete analysis/wake invalidation for Apply (Fortran PANGEN sets
// LGAMU/LVCONV/LWAKE/... .FALSE.).
TEST(unit, CorePparApplyInvalidatesAnalysisDependents) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  // analysis/wake state set artificially (only to test invalidation).
  composition.viscous.systemValid = true;
  composition.viscous.wakeValid = true;
  composition.viscous.awake = 2.0;
  composition.viscous.converged = true;
  composition.result.lastResult.status = xfoil::AnalysisStatus::converged;

  std::ostringstream out;
  app::cli::applyPparPanelingCore(composition, out);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
  CHECK_FALSE(composition.viscous.converged);
  CHECK(composition.result.lastResult.status !=
        xfoil::AnalysisStatus::converged);
}

// initial origin none (INTE buffer without panel): Apply creates a PANGEN contour.
TEST(unit, CorePparApplyFromNoneOrigin) {
  session::SessionComposition composition;
  std::ostringstream inteOut;
  handleInteFF(composition, "tests/data/core-c11a-cli-reference/inte_n0012.dat", "tests/data/core-c11a-cli-reference/inte_n2412.dat", "0.5", "inte", inteOut);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::none));

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::applied));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// repeated Apply with unchanged parameters: origin stays pangen,
// panelSourceRevision == bufferRevision, buffer revision unchanged.
TEST(unit, CorePparApplyRenewedPangenStable) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  std::ostringstream firstOut;
  app::cli::applyPparPanelingCore(composition, firstOut);
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::applied));
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
}

// No buffer: canonical PANGEN buffer message, no mutation.
TEST(unit, CorePparApplyNoBufferNoMutation) {
  session::SessionComposition composition;  // fresh: no buffer
  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::bufferUnavailable));
  CHECK_EQ(out.str(), " PANGEN: Buffer airfoil not available.\n");
  CHECK(!composition.paneling.panelingValid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(composition.paneling.panel.x.empty());
}

// invalid panel parameter (panelCount<2, set directly): PANGEN core error
// without partial mutation; contour and parameters remain unchanged.
TEST(unit, CorePparApplyInvalidParamsNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  composition.paneling.parameters.panelCount = 1;  // test injection

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::panelingFailed));
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK_EQ(composition.paneling.parameters.panelCount, 1);
}

// capacity limit (panelCount>600, set directly): canonical
// too-many-panels diagnostic without partial mutation.
TEST(unit, CorePparApplyCapacityNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  composition.paneling.parameters.panelCount = 601;  // test injection (>600)

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::panelingFailed));
  CHECK_EQ(out.str(), "Panel: Too many panels. Increase IQX\n");
  CHECK(composition.paneling.panel.x == panelXBefore);
}

// independent sessions: Apply in one session leaves the other untouched.
TEST(unit, CorePparApplyIndependentSessions) {
  session::SessionComposition a;
  useXfoil699PanelDefaults(a);
  session::SessionComposition b;
  useXfoil699PanelDefaults(b);
  std::ostringstream nacaA;
  std::ostringstream nacaB;
  testing::handleLegacyNacaCore(a, nacaA, 2412);
  testing::handleLegacyNacaCore(b, nacaB, 2412);
  const double n = 200.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      a, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  app::cli::applyPparPanelingCore(a, out);
  CHECK_EQ(session::panelingParameters(b).panelCount, 160);
  CHECK_EQ(static_cast<int>(b.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// Small-number audit (without changing the production CLI contract): for
// NPAN=2 Fortran reads outside valid indices in LEFIND (S(3)/S(2)) and
// returns a degenerate must-not result; the core guards this with the
// lefind N<3 precondition. The handler does not expose the precondition
// but returns a structured failure without partial mutation. No
// canonical Fortran error text is invented.
TEST(unit, CorePparApplySmallCountN2GuardedNoMutation) {
  session::SessionComposition composition;
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  const double n = 2.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::panelingFailed));
  CHECK(out.str().empty());
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK_EQ(composition.paneling.parameters.panelCount, 2);
}

// Small-number audit NPAN=4: Fortran corner angle 87.097 at index 2.
TEST(unit, CorePparApplySmallCountN4MatchesFortran) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream nacaSink;
  testing::handleLegacyNacaCore(composition, nacaSink, 2412);
  const double n = 4.0;
  std::ostringstream paramSink;
  app::cli::applyPparParameterCore(
      composition, core::paneling::PanelParameterKind::panelCount,
      std::span<const double>(&n, 1), paramSink);

  std::ostringstream out;
  const app::cli::PparApplyCoreStatus status =
      app::cli::applyPparPanelingCore(composition, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PparApplyCoreStatus::applied));
  CHECK(out.str().find(
            " Maximum panel corner angle = 93.403   at  i,x,y  =   3"
            "   0.1181  -0.0396\n") != std::string::npos);
}

// deterministic full-text output: an identical Apply sequence returns
// byte-identical text.
TEST(unit, CorePparApplyTextDeterministic) {
  auto runOnce = [] {
    session::SessionComposition composition;
    std::ostringstream nacaSink;
    testing::handleLegacyNacaCore(composition, nacaSink, 2412);
    std::ostringstream out;
    app::cli::applyPparPanelingCore(composition, out);
    return out.str();
  };
  const std::string first = runOnce();
  const std::string second = runOnce();
  const std::string third = runOnce();
  CHECK(first == second);
  CHECK(second == third);
  CHECK(first.find("Maximum panel corner angle") != std::string::npos);
}

// --- LOAD core path -------------------------------------------

// complete LOAD text output byte-exact against the canonical Fortran
// reference geometry/load-naca2412-buffer (plain file, LNORM off):
// header line (aread.f), ASKS name prompt, FORMAT 1010, GEOPAR FORMAT
// 1000 (thickness/camber), format 1050 (LE/TE/chord), ABCOPY format 1200.
TEST(unit, CoreLoadHandlerMatchesFortranLoadBlock) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status = app::cli::handleLoadCore(
      composition, kLoadReferenceFile, "naca2412buf", out);
  CHECK_EQ(static_cast<int>(status), static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected =
      "\n Plain airfoil file\n"
      "\nEnter airfoil name   s>  "
      "\n Number of input coordinate points: 245\n Counterclockwise ordering\n"
      " Max thickness =     0.120032  at x =   0.297\n"
      " Max camber    =     0.019999  at x =   0.403\n"
      "\n  LE  x,y  =  -0.00000  -0.00000  |   Chord =   1.00000\n"
      "  TE  x,y  =   1.00000   0.00000  |\n"
      "\n Current airfoil nodes set from buffer airfoil nodes ( 245 )\n";
  CHECK_EQ(out.str(), expected);
}

// state contract of a successful LOAD: buffer = loaded contour,
// revision incremented exactly once, ABCOPY contour, origin abcopy,
// panelSourceRevision == bufferRevision, panel parameters preserved, name set.
TEST(unit, CoreLoadHandlerSetsBufferAbcopyAndRevision) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream out;
  const app::cli::LoadCoreStatus status = app::cli::handleLoadCore(
      composition, kLoadReferenceFile, "naca2412buf", out);
  CHECK_EQ(static_cast<int>(status), static_cast<int>(app::cli::LoadCoreStatus::success));

  CHECK(composition.geometry.valid);
  CHECK_EQ(composition.geometry.buffer.x.size(), 245u);
  CHECK_EQ(composition.geometry.buffer.y.size(), 245u);
  CHECK_EQ(composition.geometry.bufferRevision, 1u);
  CHECK_EQ(composition.geometry.name, std::string("naca2412buf"));

  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
  CHECK_EQ(composition.paneling.panel.x.size(), 245u);

  // panel parameters are preserved by LOAD (canonical semantics).
  CHECK_EQ(session::panelingParameters(composition).panelCount, 160);

  // geometry metrics (geopar) are available.
  CHECK(session::geometryMetrics(composition).has_value());
}

// normalization on/off: LNORM shows the format-1020 message and the
// contour is normalized to the unit chord; off removes the message again.
TEST(unit, CoreLoadHandlerNormalizationToggle) {
  session::SessionComposition off;
  std::ostringstream offOut;
  app::cli::handleLoadCore(off, kLoadReferenceFile, "naca2412buf", offOut);
  CHECK(offOut.str().find("Airfoil has been normalized") == std::string::npos);

  session::SessionComposition on;
  session::setNormalizeOnLoad(on, true);
  std::ostringstream onOut;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(on, kLoadReferenceFile, "naca2412buf", onOut);
  CHECK_EQ(static_cast<int>(status), static_cast<int>(app::cli::LoadCoreStatus::success));
  CHECK(onOut.str().find("\n Airfoil has been normalized\n") != std::string::npos);
  const auto metrics = session::geometryMetrics(on);
  CHECK(metrics.has_value());
  if (metrics) {
    CHECK(metrics->chord > 0.999 && metrics->chord < 1.001);
  }
}

// missing file: controlled error without any session mutation.
TEST(unit, CoreLoadHandlerMissingFileKeepsState) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status = app::cli::handleLoadCore(
      composition, "tests/data/definitely-missing-airfoil.dat", "x", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::fileOpenError));
  CHECK(out.str().find("*** LOAD NOT COMPLETED ***") != std::string::npos);
  CHECK(!composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(!composition.paneling.panelingValid);
  CHECK_EQ(composition.paneling.panelSourceRevision, 0u);
}

// independent sessions: a LOAD in A does not change B.
TEST(unit, CoreLoadHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  app::cli::handleLoadCore(a, kLoadReferenceFile, "naca2412buf", out);
  CHECK(a.geometry.valid);
  CHECK_EQ(a.geometry.bufferRevision, 1u);
  CHECK(!b.geometry.valid);
  CHECK_EQ(b.geometry.bufferRevision, 0u);
  CHECK(!b.paneling.panelingValid);
}

// --- INTE core path -------------------------------------------

constexpr const char* kInteFile0 =
    "tests/data/core-c11a-cli-reference/inte_n0012.dat";
constexpr const char* kInteFile1 =
    "tests/data/core-c11a-cli-reference/inte_n2412.dat";
constexpr const char* kInteStdoutRef =
    "tests/data/core-c11a-cli-reference/inte_stdout_ref.txt";
constexpr const char* kInteResultRef =
    "tests/data/core-c11a-cli-reference/inte_result_ref.dat";

std::string readFileText(const char* path) {
  return readWholeFile(path);
}

// complete INTE text output byte-exact against the canonical Fortran
// reference (source menu, two F sources, fraction 0.5, GEOPAR
// thickness/camber, new name, buffer/PCOP note).
TEST(unit, CoreInteHandlerMatchesFortranText) {
  session::SessionComposition composition;
  std::ostringstream loadSink;
  app::cli::handleLoadCore(composition, kInteFile0, "0012src", loadSink);

  std::ostringstream out;
  const app::cli::InteCoreStatus status = handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "inte05", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::InteCoreStatus::success));
  CHECK_EQ(out.str(), readFileText(kInteStdoutRef));
}

// result coordinates: the buffer set by INTE matches the canonical
// Fortran interpolation (PSAV reference, 160 points).
TEST(unit, CoreInteHandlerResultCoordinatesMatchFortran) {
  session::SessionComposition composition;
  std::ostringstream loadSink;
  app::cli::handleLoadCore(composition, kInteFile0, "0012src", loadSink);
  std::ostringstream inteSink;
  handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "inte05", inteSink);

  CHECK_EQ(composition.geometry.buffer.x.size(), 160u);
  CHECK_EQ(composition.geometry.buffer.y.size(), 160u);

  std::vector<xfoil::Point2D> coords;
  for (std::size_t i = 0; i < composition.geometry.buffer.x.size(); ++i) {
    coords.push_back({composition.geometry.buffer.x[i],
                      composition.geometry.buffer.y[i]});
  }
  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::plain;
  options.delimiter = app::CoordinateDelimiter::blank;
  options.clockwise = composition.geometry.writeClockwise;
  options.name = "inte05";
  options.overwriteExisting = true;
  const testing::ScopedTempDirectory work(
      "CoreInteHandlerResultCoordinatesMatchFortran");
  const auto tmp = work.makePath("inte-result.dat");
  const auto written = app::writeAirfoil(tmp, coords, options);
  CHECK(written.success);
  const std::string got = readFileText(tmp.string().c_str());
  CHECK_EQ(got, readFileText(kInteResultRef));
}

// state contract: INTE writes only the buffer. The existing current panel,
// panel provenance and analysis state remain unchanged until PCOP/PANE.
TEST(unit, CoreInteHandlerPreservesCurrentPanelAndAnalysis) {
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream loadSink;
  app::cli::handleLoadCore(composition, kInteFile0, "0012src", loadSink);
  CHECK(composition.paneling.panelingValid);  // LOAD established an ABCOPY contour
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;
  const std::uint64_t panelRevisionBefore = composition.paneling.panelRevision;
  const std::uint64_t panelSourceBefore = composition.paneling.panelSourceRevision;
  const std::vector<double> panelXBefore = composition.paneling.panel.x;
  const std::vector<double> panelYBefore = composition.paneling.panel.y;
  const std::size_t panelCountBefore = composition.paneling.panel.x.size();
  composition.viscous.systemValid = true;
  composition.viscous.wakeValid = true;
  composition.viscous.converged = true;
  composition.viscous.viscals.lblini = true;
  composition.result.lastResult.status = xfoil::AnalysisStatus::converged;

  std::ostringstream out;
  handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "inte05", out);

  CHECK_EQ(composition.geometry.bufferRevision, revisionBefore + 1u);
  CHECK_EQ(composition.geometry.name, std::string("inte05"));
  // canonical INTE: buffer only, the current contour stays installed.
  CHECK(composition.paneling.panelingValid);
  CHECK(composition.paneling.panel.x == panelXBefore);
  CHECK(composition.paneling.panel.y == panelYBefore);
  CHECK_EQ(composition.paneling.panel.x.size(), panelCountBefore);
  CHECK_EQ(composition.paneling.panelRevision, panelRevisionBefore);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelSourceBefore);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  // analysis state is preserved until PCOP/PANE.
  CHECK(composition.viscous.systemValid);
  CHECK(composition.viscous.wakeValid);
  CHECK(composition.viscous.converged);
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.result.lastResult.status == xfoil::AnalysisStatus::converged);
  // panel parameters are retained.
  CHECK_EQ(session::panelingParameters(composition).panelCount, 160);
}

// edge cases fraction 0 and 1: deterministic point count (N = airfoil 0),
// no session mutation error.
TEST(unit, CoreInteHandlerFractionEndpoints) {
  for (const char* frac : {"0", "1"}) {
    session::SessionComposition composition;
    std::ostringstream loadSink;
    app::cli::handleLoadCore(composition, kInteFile0, "0012src", loadSink);
    std::ostringstream out;
    std::istringstream in(std::string("F\n") + kInteFile0 + "\nF\n" +
                          kInteFile1 + "\n" + frac + "\ninteE\n");
    const app::cli::InteCoreStatus status =
        app::cli::handleInteCore(composition, in, out);
    CHECK_EQ(static_cast<int>(status),
             static_cast<int>(app::cli::InteCoreStatus::success));
    CHECK_EQ(composition.geometry.buffer.x.size(), 160u);
  }
}

// missing file: controlled error without session mutation.
TEST(unit, CoreInteHandlerMissingFileKeepsState) {
  session::SessionComposition composition;
  std::ostringstream loadSink;
  app::cli::handleLoadCore(composition, kInteFile0, "0012src", loadSink);
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;
  const std::string nameBefore = composition.geometry.name;
  const std::size_t bufferBefore = composition.geometry.buffer.x.size();

  std::ostringstream out;
  const app::cli::InteCoreStatus status = handleInteFF(composition, kInteFile0, "tests/data/definitely-missing.dat", "0.5", "x", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::InteCoreStatus::fileOpenError));
  CHECK_EQ(composition.geometry.bufferRevision, revisionBefore);
  CHECK_EQ(composition.geometry.name, nameBefore);
  CHECK_EQ(composition.geometry.buffer.x.size(), bufferBefore);
}

// independent sessions.
TEST(unit, CoreInteHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream loadA;
  app::cli::handleLoadCore(a, kInteFile0, "0012src", loadA);
  std::ostringstream out;
  handleInteFF(a, kInteFile0, kInteFile1, "0.5", "inte05", out);
  CHECK_EQ(a.geometry.name, std::string("inte05"));
  CHECK_EQ(b.geometry.bufferRevision, 0u);
  CHECK(b.geometry.name.empty());
}

// ===================== TOP-level NORM (not routed) =====================
// TOP-level NORM toggles LNORM (xfoil.f:233-239): the policy for
// normalizing future LOAD operations. It changes no existing geometry,
// no buffer/panel and no analysis.

// canonical initial value: LNORM is .FALSE. after INIT (xfoil.f:495).
TEST(unit, CoreNormHandlerInitialPolicyOff) {
  session::SessionComposition composition;
  CHECK_FALSE(session::normalizeOnLoad(composition));
}

// toggle sequence: the first NORM switches on, the second off; text byte-exact.
TEST(unit, CoreNormHandlerTogglesOnThenOff) {
  session::SessionComposition composition;
  std::ostringstream on;
  CHECK_EQ(static_cast<int>(app::cli::handleNormCore(composition, on)),
           static_cast<int>(app::cli::NormCoreStatus::enabledAfter));
  CHECK(session::normalizeOnLoad(composition));
  CHECK_EQ(on.str(), std::string(" Loaded airfoil will  be normalized\n"));

  std::ostringstream off;
  CHECK_EQ(static_cast<int>(app::cli::handleNormCore(composition, off)),
           static_cast<int>(app::cli::NormCoreStatus::disabledAfter));
  CHECK_FALSE(session::normalizeOnLoad(composition));
  CHECK_EQ(off.str(), std::string(" Loaded airfoil won't be normalized\n"));
}

// NORM without loaded geometry: only the policy itself changes.
TEST(unit, CoreNormHandlerNoGeometryOnlyPolicyChanges) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNormCore(composition, out);
  CHECK(session::normalizeOnLoad(composition));
  CHECK_FALSE(composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(composition.geometry.buffer.x.empty());
  CHECK_FALSE(composition.paneling.panelingValid);
  CHECK_EQ(composition.paneling.panelSourceRevision, 0u);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
}

// NORM for a valid buffer/ABCOPY contour: geometry/analysis state
// remains unchanged except for the policy (no coordinate transformation).
TEST(unit, CoreNormHandlerPreservesAbcopyState) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  CHECK(composition.geometry.valid);
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));

  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const std::size_t bufferN = composition.geometry.buffer.x.size();
  const std::size_t panelN = composition.paneling.panel.x.size();
  const double bufferX0 = composition.geometry.buffer.x.front();
  const double panelX0 = composition.paneling.panel.x.front();
  const std::string name = composition.geometry.name;
  const bool clockwise = composition.geometry.writeClockwise;
  const int delim = composition.geometry.delimiter;
  const int panelCount = composition.paneling.parameters.panelCount;
  const double bunching = composition.paneling.parameters.bunchingParam;
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  app::cli::handleNormCore(composition, out);

  CHECK(session::normalizeOnLoad(composition));
  CHECK(composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.buffer.x.size(), bufferN);
  CHECK(composition.geometry.buffer.x.front() == bufferX0);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(composition.paneling.panel.x.size(), panelN);
  CHECK(composition.paneling.panel.x.front() == panelX0);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(composition.geometry.writeClockwise, clockwise);
  CHECK_EQ(composition.geometry.delimiter, delim);
  CHECK_EQ(composition.paneling.parameters.panelCount, panelCount);
  CHECK(composition.paneling.parameters.bunchingParam == bunching);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK(static_cast<bool>(metrics) == static_cast<bool>(metricsAfter));
  if (metrics && metricsAfter) {
    CHECK(metrics->chord == metricsAfter->chord);
  }
}

// NORM for a valid PANGEN panel contour: origin and panel are retained.
TEST(unit, CoreNormHandlerPreservesPangenPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaOut;
  testing::handleLegacyNacaCore(composition, nacaOut, 2412);  // panel pangen
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  const std::size_t panelN = composition.paneling.panel.x.size();
  const double panelX0 = composition.paneling.panel.x.front();

  std::ostringstream out;
  app::cli::handleNormCore(composition, out);

  CHECK(session::normalizeOnLoad(composition));
  CHECK_EQ(composition.paneling.panel.x.size(), panelN);
  CHECK(composition.paneling.panel.x.front() == panelX0);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// NORM via the handler drives the LOAD normalization (the policy is consumed);
// second toggle off -> no normalization. The handler never normalizes itself.
TEST(unit, CoreNormHandlerDrivesLoadNormalization) {
  session::SessionComposition composition;
  std::ostringstream normOn;
  app::cli::handleNormCore(composition, normOn);  // LNORM on
  std::ostringstream loadOn;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOn);
  CHECK(loadOn.str().find("\n Airfoil has been normalized\n") !=
        std::string::npos);
  const auto mOn = session::geometryMetrics(composition);
  CHECK(mOn && mOn->chord > 0.999 && mOn->chord < 1.001);

  std::ostringstream normOff;
  app::cli::handleNormCore(composition, normOff);  // LNORM off
  std::ostringstream loadOff;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOff);
  CHECK(loadOff.str().find("Airfoil has been normalized") == std::string::npos);
  CHECK_FALSE(session::normalizeOnLoad(composition));
}

// NACA and INTE ignore the LOAD normalization policy (canonical without
// NORM); the policy stays unchanged.
TEST(unit, CoreNormHandlerNacaInteIgnorePolicy) {
  session::SessionComposition composition;
  std::ostringstream normOn;
  app::cli::handleNormCore(composition, normOn);  // LNORM on
  std::ostringstream nacaOut;
  testing::handleLegacyNacaCore(composition, nacaOut, 2412);
  CHECK(nacaOut.str().find("Airfoil has been normalized") == std::string::npos);
  CHECK(session::normalizeOnLoad(composition));

  std::ostringstream inteOut;
  handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "inte05", inteOut);
  CHECK(inteOut.str().find("Airfoil has been normalized") == std::string::npos);
  CHECK(session::normalizeOnLoad(composition));
}

// independent sessions: NORM in A does not change B.
TEST(unit, CoreNormHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  app::cli::handleNormCore(a, out);
  CHECK(session::normalizeOnLoad(a));
  CHECK_FALSE(session::normalizeOnLoad(b));
}

// deterministic text output: same toggle in independent sessions.
TEST(unit, CoreNormHandlerDeterministicText) {
  session::SessionComposition c1;
  session::SessionComposition c2;
  std::ostringstream e1;
  std::ostringstream e2;
  app::cli::handleNormCore(c1, e1);
  app::cli::handleNormCore(c2, e2);
  CHECK_EQ(e1.str(), e2.str());
  std::ostringstream d1;
  std::ostringstream d2;
  app::cli::handleNormCore(c1, d1);
  app::cli::handleNormCore(c2, d2);
  CHECK_EQ(d1.str(), d2.str());
}

// ===================== TOP-level REVE (not routed) =====================
// TOP-level REVE toggles LCLOCK (xfoil.f:176-182): the write
// orientation for SAVE/PSAV/USAV/ISAV. It reverses no buffer or panel
// coordinates and changes no origin/revision/analysis state.

// canonical initial value: LCLOCK is .FALSE. (COMMON zero; session default).
TEST(unit, CoreReveHandlerInitialOrientationCounterclockwise) {
  session::SessionComposition composition;
  CHECK_FALSE(session::writeClockwise(composition));
}

// toggle sequence: the first REVE switches to clockwise, the second back; text byte-exact.
TEST(unit, CoreReveHandlerTogglesClockwiseThenBack) {
  session::SessionComposition composition;
  std::ostringstream cw;
  CHECK_EQ(static_cast<int>(app::cli::handleReveCore(composition, cw)),
           static_cast<int>(app::cli::ReveCoreStatus::clockwiseAfter));
  CHECK(session::writeClockwise(composition));
  CHECK_EQ(cw.str(),
           std::string(" Airfoil will be written in clockwise order\n"));

  std::ostringstream ccw;
  CHECK_EQ(static_cast<int>(app::cli::handleReveCore(composition, ccw)),
           static_cast<int>(app::cli::ReveCoreStatus::counterclockwiseAfter));
  CHECK_FALSE(session::writeClockwise(composition));
  CHECK_EQ(ccw.str(),
           std::string(" Airfoil will be written in counterclockwise order\n"));
}

// REVE when the write orientation is already set (writeClockwise=true):
// toggles to counterclockwise.
TEST(unit, CoreReveHandlerFromClockwiseToCounterclockwise) {
  session::SessionComposition composition;
  session::setWriteOrientation(composition, true);
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleReveCore(composition, out)),
           static_cast<int>(app::cli::ReveCoreStatus::counterclockwiseAfter));
  CHECK_FALSE(session::writeClockwise(composition));
}

// REVE without loaded geometry: only the write orientation changes.
TEST(unit, CoreReveHandlerNoGeometryOnlyPolicyChanges) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleReveCore(composition, out);
  CHECK(session::writeClockwise(composition));
  CHECK_FALSE(composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK(composition.geometry.buffer.x.empty());
  CHECK_FALSE(composition.paneling.panelingValid);
  CHECK_EQ(composition.paneling.panelSourceRevision, 0u);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
}

// REVE for a valid buffer/ABCOPY contour: all coordinates, origin,
// revisions, parameters, name, metrics and the NORM policy remain unchanged;
// only writeClockwise flips (REVE does not reverse any coordinates).
TEST(unit, CoreReveHandlerPreservesBufferAndPanelCoordinates) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  CHECK(composition.geometry.valid);
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));

  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> bufferY = composition.geometry.buffer.y;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> panelY = composition.paneling.panel.y;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const std::string name = composition.geometry.name;
  const bool normPolicy = session::normalizeOnLoad(composition);
  const int delim = composition.geometry.delimiter;
  const int panelCount = composition.paneling.parameters.panelCount;
  const auto metrics = session::geometryMetrics(composition);
  const bool writeBefore = session::writeClockwise(composition);

  std::ostringstream out;
  app::cli::handleReveCore(composition, out);

  CHECK(session::writeClockwise(composition) != writeBefore);
  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.geometry.buffer.y == bufferY);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK(composition.paneling.panel.y == panelY);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(session::normalizeOnLoad(composition), normPolicy);
  CHECK_EQ(composition.geometry.delimiter, delim);
  CHECK_EQ(composition.paneling.parameters.panelCount, panelCount);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK(static_cast<bool>(metrics) == static_cast<bool>(metricsAfter));
  if (metrics && metricsAfter) {
    CHECK(metrics->chord == metricsAfter->chord);
  }
}

// REVE for a valid PANGEN panel contour: origin and panel are retained.
TEST(unit, CoreReveHandlerPreservesPangenPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaOut;
  testing::handleLegacyNacaCore(composition, nacaOut, 2412);  // panel pangen
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
  const std::vector<double> panelX = composition.paneling.panel.x;

  std::ostringstream out;
  app::cli::handleReveCore(composition, out);

  CHECK(session::writeClockwise(composition));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// REVE twice: write orientation back, coordinates unchanged.
TEST(unit, CoreReveHandlerTwiceRestoresOrientation) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const bool writeBefore = session::writeClockwise(composition);

  std::ostringstream o1;
  std::ostringstream o2;
  app::cli::handleReveCore(composition, o1);
  app::cli::handleReveCore(composition, o2);

  CHECK_EQ(session::writeClockwise(composition), writeBefore);
  CHECK(composition.geometry.buffer.x == bufferX);
}

// Orthogonality: REVE leaves the NORM policy untouched and vice versa.
TEST(unit, CoreReveAndNormPoliciesAreOrthogonal) {
  session::SessionComposition composition;
  std::ostringstream n;
  app::cli::handleNormCore(composition, n);  // LNORM on
  const bool norm = session::normalizeOnLoad(composition);
  std::ostringstream r;
  app::cli::handleReveCore(composition, r);  // LCLOCK on
  CHECK(session::normalizeOnLoad(composition) == norm);
  CHECK(session::writeClockwise(composition));
  std::ostringstream n2;
  app::cli::handleNormCore(composition, n2);  // LNORM from
  CHECK_FALSE(session::normalizeOnLoad(composition));
  CHECK(session::writeClockwise(composition));  // LCLOCK stays on
}

// independent sessions: REVE in A does not change B.
TEST(unit, CoreReveHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  app::cli::handleReveCore(a, out);
  CHECK(session::writeClockwise(a));
  CHECK_FALSE(session::writeClockwise(b));
}

// deterministic text output: same toggle in independent sessions.
TEST(unit, CoreReveHandlerDeterministicText) {
  session::SessionComposition c1;
  session::SessionComposition c2;
  std::ostringstream e1;
  std::ostringstream e2;
  app::cli::handleReveCore(c1, e1);
  app::cli::handleReveCore(c2, e2);
  CHECK_EQ(e1.str(), e2.str());
  std::ostringstream d1;
  std::ostringstream d2;
  app::cli::handleReveCore(c1, d1);
  app::cli::handleReveCore(c2, d2);
  CHECK_EQ(d1.str(), d2.str());
}

// after REVE, PANE uses the same contour (REVE did not change the geometry).
TEST(unit, CoreReveHandlerSubsequentPaneUsesSameContour) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);
  std::ostringstream paneA;
  app::cli::handlePaneCore(a, paneA);
  std::ostringstream reve;
  app::cli::handleReveCore(b, reve);  // only write orientation
  std::ostringstream paneB;
  app::cli::handlePaneCore(b, paneB);
  CHECK(a.paneling.panel.x == b.paneling.panel.x);
  CHECK(a.paneling.panel.y == b.paneling.panel.y);
}

// ===================== TOP-level DELI (not routed) =====================
// TOP-level DELI sets KDELIM (0=blank, 1=comma, 2=tab) from an argument
// (xfoil.f:185-204). A valid argument is adopted silently; without an
// argument or with an invalid one -> menu (format 2100) + prompt. Pure
// output policy: no geometry/analysis/NORM/REVE change.

namespace {
app::CoordinateDelimiter sessionDelimiterToWriter(int d) {
  if (d == 1) return app::CoordinateDelimiter::comma;
  if (d == 2) return app::CoordinateDelimiter::tab;
  return app::CoordinateDelimiter::blank;
}
}  // namespace

// canonical initial value: KDELIM = 0 (blank) (xfoil.f:454).
TEST(unit, CoreDeliHandlerInitialDelimiterBlank) {
  session::SessionComposition composition;
  CHECK_EQ(session::delimiter(composition), 0);
}

// valid argument takes over silently (no menu output).
TEST(unit, CoreDeliHandlerArgumentSetsSilently) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleDeliCore(composition, out, 1, true)),
           static_cast<int>(app::cli::DeliCoreStatus::applied));
  CHECK_EQ(session::delimiter(composition), 1);
  CHECK(out.str().empty());

  std::ostringstream out2;
  app::cli::handleDeliCore(composition, out2, 2, true);
  CHECK_EQ(session::delimiter(composition), 2);
  CHECK(out2.str().empty());

  std::ostringstream out3;
  app::cli::handleDeliCore(composition, out3, 0, true);  // back to the initial value
  CHECK_EQ(session::delimiter(composition), 0);
  CHECK(out3.str().empty());
}

// invalid argument: Menu with current value, no mutation.
TEST(unit, CoreDeliHandlerInvalidArgumentRequestsMenu) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleDeliCore(composition, out, 3, true)),
           static_cast<int>(app::cli::DeliCoreStatus::menuRequested));
  CHECK_EQ(session::delimiter(composition), 0);  // unchanged
  CHECK(out.str().find("currently, delimiter = 0") != std::string::npos);
}

// without an argument: menu with the current value, no mutation.
TEST(unit, CoreDeliHandlerNoArgumentRequestsMenu) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleDeliCore(composition, out, 0, false)),
           static_cast<int>(app::cli::DeliCoreStatus::menuRequested));
  CHECK_EQ(session::delimiter(composition), 0);
  CHECK(out.str().find("currently, delimiter = 0") != std::string::npos);
}

// menu byte-exact (format 2100) for current value 0 and 2.
TEST(unit, CoreDeliHandlerMenuByteExact) {
  session::SessionComposition composition;
  std::ostringstream out0;
  app::cli::handleDeliCore(composition, out0, 0, false);
  CHECK_EQ(out0.str(),
           std::string("\n  --------------------------\n"
                       "   0  blank\n"
                       "   1  comma\n"
                       "   2  tab\n"
                       "\n  currently, delimiter = 0\n"));

  std::ostringstream set2;
  app::cli::handleDeliCore(composition, set2, 2, true);  // KDELIM=2
  std::ostringstream out2;
  app::cli::handleDeliCore(composition, out2, 0, false);
  CHECK_EQ(out2.str(),
           std::string("\n  --------------------------\n"
                       "   0  blank\n"
                       "   1  comma\n"
                       "   2  tab\n"
                       "\n  currently, delimiter = 2\n"));
}

// DELI without loaded geometry: only the delimiter changes.
TEST(unit, CoreDeliHandlerNoGeometryOnlyPolicyChanges) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleDeliCore(composition, out, 1, true);
  CHECK_EQ(session::delimiter(composition), 1);
  CHECK_FALSE(composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK_FALSE(composition.paneling.panelingValid);
  CHECK_FALSE(composition.viscous.systemValid);
  CHECK_FALSE(composition.viscous.wakeValid);
}

// DELI for ABCOPY-contour: all except delimiter unchanged.
TEST(unit, CoreDeliHandlerPreservesAbcopyState) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const std::string name = composition.geometry.name;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  app::cli::handleDeliCore(composition, out, 1, true);

  CHECK_EQ(session::delimiter(composition), 1);
  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK(static_cast<bool>(metrics) == static_cast<bool>(metricsAfter));
}

// DELI for a PANGEN contour: origin and panel are retained.
TEST(unit, CoreDeliHandlerPreservesPangenPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaOut;
  testing::handleLegacyNacaCore(composition, nacaOut, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  std::ostringstream out;
  app::cli::handleDeliCore(composition, out, 2, true);
  CHECK_EQ(session::delimiter(composition), 2);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// Orthogonality: DELI leaves the NORM and REVE policies untouched.
TEST(unit, CoreDeliHandlerIndependentOfNormAndReve) {
  session::SessionComposition composition;
  std::ostringstream s;
  app::cli::handleNormCore(composition, s);  // LNORM on
  app::cli::handleReveCore(composition, s);  // LCLOCK on
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  std::ostringstream out;
  app::cli::handleDeliCore(composition, out, 1, true);
  CHECK_EQ(session::delimiter(composition), 1);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
}

// the writer correctly consumes every session state set via DELI.
TEST(unit, CoreDeliHandlerWriterConsumesEachState) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  std::vector<xfoil::Point2D> coords;
  for (std::size_t i = 0; i < composition.geometry.buffer.x.size(); ++i) {
    coords.push_back({composition.geometry.buffer.x[i],
                      composition.geometry.buffer.y[i]});
  }
  const std::array<int, 3> states{0, 1, 2};
  const testing::ScopedTempDirectory work(
      "CoreDeliHandlerWriterConsumesEachState");
  for (int state : states) {
    std::ostringstream d;
    app::cli::handleDeliCore(composition, d, state, true);
    CHECK_EQ(session::delimiter(composition), state);
    app::AirfoilWriteOptions options;
    options.format = app::AirfoilOutputFormat::plain;
    options.delimiter = sessionDelimiterToWriter(session::delimiter(composition));
    options.clockwise = composition.geometry.writeClockwise;
    options.overwriteExisting = true;
    const auto tmp =
        work.makePath("deli-state-" + std::to_string(state) + ".dat");
    const auto written = app::writeAirfoil(tmp, coords, options);
    CHECK(written.success);
    const std::string text = readFileText(tmp.string().c_str());
    const std::size_t nl = text.find('\n');
    const std::string firstLine = text.substr(0, nl);
    if (state == 1) {
      CHECK(firstLine.find(',') != std::string::npos);
      CHECK(firstLine.find('\t') == std::string::npos);
    } else if (state == 2) {
      CHECK(firstLine.find('\t') != std::string::npos);
      CHECK(firstLine.find(',') == std::string::npos);
    } else {
      CHECK(firstLine.find(',') == std::string::npos);
      CHECK(firstLine.find('\t') == std::string::npos);
    }
  }
}

// REVE plus DELI: correct order (backward pass) and separation.
TEST(unit, CoreDeliHandlerRevePlusDeliOrderAndSeparation) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  std::vector<xfoil::Point2D> coords;
  for (std::size_t i = 0; i < composition.geometry.buffer.x.size(); ++i) {
    coords.push_back({composition.geometry.buffer.x[i],
                      composition.geometry.buffer.y[i]});
  }
  std::ostringstream d;
  app::cli::handleDeliCore(composition, d, 1, true);   // comma
  std::ostringstream r;
  app::cli::handleReveCore(composition, r);            // clockwise
  CHECK(session::writeClockwise(composition));
  CHECK_EQ(session::delimiter(composition), 1);

  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::plain;
  options.delimiter = sessionDelimiterToWriter(session::delimiter(composition));
  options.clockwise = composition.geometry.writeClockwise;
  options.overwriteExisting = true;
  const testing::ScopedTempDirectory work(
      "CoreDeliHandlerRevePlusDeliOrderAndSeparation");
  const auto tmp = work.makePath("deli-reve.dat");
  const auto written = app::writeAirfoil(tmp, coords, options);
  CHECK(written.success);
  const std::string text = readFileText(tmp.string().c_str());
  const std::size_t nl = text.find('\n');
  const std::string firstLine = text.substr(0, nl);
  CHECK(firstLine.find(',') != std::string::npos);  // comma-separation
  // backward pass: first written x coordinate == last buffer x.
  const double firstWrittenX = std::stod(firstLine.substr(0, firstLine.find(',')));
  CHECK(firstWrittenX == composition.geometry.buffer.x.back());
}

// independent sessions: DELI in A does not change B.
TEST(unit, CoreDeliHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  app::cli::handleDeliCore(a, out, 2, true);
  CHECK_EQ(session::delimiter(a), 2);
  CHECK_EQ(session::delimiter(b), 0);
}

// deterministic menu output: same state in independent sessions.
TEST(unit, CoreDeliHandlerDeterministicText) {
  session::SessionComposition c1;
  session::SessionComposition c2;
  std::ostringstream e1;
  std::ostringstream e2;
  app::cli::handleDeliCore(c1, e1, 0, false);
  app::cli::handleDeliCore(c2, e2, 0, false);
  CHECK_EQ(e1.str(), e2.str());
}

// ===================== TOP-level XYCM (not routed) =====================
// TOP-level XYCM sets the CM reference point XCMREF/YCMREF (xfoil.f:255-262).
// two finite values are adopted silently; otherwise a prompt (ASKR) follows.
// only CM depends on the reference point (CLCALC 1146-1157); no existing
// result is invalidated or converted; no geometry/solver mutation.

// canonical initial values: XCMREF=0.25, YCMREF=0.0 (xfoil.f:669-670).
TEST(unit, CoreXycmHandlerInitialReference) {
  session::SessionComposition composition;
  CHECK_NEAR(composition.operating.momentReferenceX, 0.25, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, 0.0, 0.0, 0.0);
}

// two valid values are adopted silently.
TEST(unit, CoreXycmHandlerAppliesTwoValuesSilently) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(
               app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, true)),
           static_cast<int>(app::cli::XycmCoreStatus::applied));
  CHECK_NEAR(composition.operating.momentReferenceX, 0.5, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, 0.0, 0.0, 0.0);
  CHECK(out.str().empty());
}

// negative and nontrivial values are accepted.
TEST(unit, CoreXycmHandlerNegativeAndNontrivialValues) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleXycmCore(composition, out, -0.5,
                                                     -0.25, true, true)),
           static_cast<int>(app::cli::XycmCoreStatus::applied));
  CHECK_NEAR(composition.operating.momentReferenceX, -0.5, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, -0.25, 0.0, 0.0);
}

// identical repetition: values remain; applied again.
TEST(unit, CoreXycmHandlerIdenticalRepeat) {
  session::SessionComposition composition;
  std::ostringstream o1;
  std::ostringstream o2;
  app::cli::handleXycmCore(composition, o1, 0.35, -0.05, true, true);
  app::cli::handleXycmCore(composition, o2, 0.35, -0.05, true, true);
  CHECK_NEAR(composition.operating.momentReferenceX, 0.35, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, -0.05, 0.0, 0.0);
}

// missing second value: no partial mutation, prompt requested.
TEST(unit, CoreXycmHandlerMissingSecondValueNoMutation) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(
               app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, false)),
           static_cast<int>(app::cli::XycmCoreStatus::promptRequested));
  CHECK_NEAR(composition.operating.momentReferenceX, 0.25, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, 0.0, 0.0, 0.0);
}

// invalid first value (NaN): no partial mutation.
TEST(unit, CoreXycmHandlerInvalidFirstValueNoMutation) {
  session::SessionComposition composition;
  std::ostringstream out;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_EQ(static_cast<int>(
               app::cli::handleXycmCore(composition, out, nan, 0.0, true, true)),
           static_cast<int>(app::cli::XycmCoreStatus::promptRequested));
  CHECK_NEAR(composition.operating.momentReferenceX, 0.25, 0.0, 0.0);
}

// invalid second value (Infinity): no partial mutation.
TEST(unit, CoreXycmHandlerInvalidSecondValueNoMutation) {
  session::SessionComposition composition;
  std::ostringstream out;
  const double inf = std::numeric_limits<double>::infinity();
  CHECK_EQ(static_cast<int>(
               app::cli::handleXycmCore(composition, out, 0.5, inf, true, true)),
           static_cast<int>(app::cli::XycmCoreStatus::promptRequested));
  CHECK_NEAR(composition.operating.momentReferenceX, 0.25, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, 0.0, 0.0, 0.0);
}

// XYCM without loaded geometry: only the reference point changes.
TEST(unit, CoreXycmHandlerNoGeometry) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleXycmCore(composition, out, 0.4, 0.1, true, true);
  CHECK_NEAR(composition.operating.momentReferenceX, 0.4, 0.0, 0.0);
  CHECK_FALSE(composition.geometry.valid);
  CHECK_FALSE(composition.paneling.panelingValid);
}

// XYCM for ABCOPY-contour: all except reference point unchanged.
TEST(unit, CoreXycmHandlerPreservesAbcopyState) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const std::string name = composition.geometry.name;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, true);

  CHECK_NEAR(composition.operating.momentReferenceX, 0.5, 0.0, 0.0);
  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK(static_cast<bool>(metrics) == static_cast<bool>(metricsAfter));
}

// XYCM for a PANGEN contour: origin and panel are retained.
TEST(unit, CoreXycmHandlerPreservesPangenPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaOut;
  testing::handleLegacyNacaCore(composition, nacaOut, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  std::ostringstream out;
  app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, true);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// XYCM invalidates an existing result according to the existing session
// contract (setMomentReference sets lastResult to notAnalyzed), without
// recomputing or repaneling itself.
TEST(unit, CoreXycmHandlerInvalidatesStoredResultPerSessionContract) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult r1 =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(r1.successful);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(xfoil::AnalysisStatus::converged));

  std::ostringstream out;
  app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, true);

  CHECK_NEAR(composition.operating.momentReferenceX, 0.5, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, 0.0, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(xfoil::AnalysisStatus::notAnalyzed));
  CHECK(!composition.result.lastResult.successful);
  CHECK(composition.geometry.valid);
  CHECK(composition.paneling.panelingValid);
}

// the next inviscid point uses the new reference point and returns CM
// directly according to It24.16 Fortran (xycm_reference.txt, NACA 2412,
// NPAN=160, alpha=4 degrees, inviscid). Cl stays reference-independent and
// unchanged. The complete canonical transformation (not dCM = Cl*dX) is:
// dCM = dX*(CL*cos(alfa) + CDP*sin(alfa)) + dY*(CL*sin(alfa) - CDP*cos(alfa)).
// the core reproduces the reference values bit-faithfully (threshold 1e-12).
TEST(unit, CoreXycmHandlerNextPointMatchesFortran) {
  // reference-CM from tests/data/core-xycm-reference/xycm_reference.txt.
  constexpr double kCl = 0.73757352558247768481;
  constexpr double kCm00 = -0.24555188803562483235;   // (0.00, 0.00)
  constexpr double kCm250 = -0.61626823037021875862e-01;  // (0.25, 0.00)
  constexpr double kCm251 = -0.56372264931503171526e-01;  // (0.25, 0.10)
  constexpr double kCmM51 = -0.60814745992731189528;      // (-0.50, 0.10)

  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  xfoil::AnalysisConfig cfg;
  cfg.viscous = false;  // inviscid branch as in the harness
  CHECK(session::configure(composition, cfg).success());

  std::ostringstream out;
  // (a) initial reference (0.25, 0.00) = session INIT.
  const xfoil::AnalysisResult rA =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(rA.successful);
  CHECK_NEAR(rA.liftCoefficient, kCl, 1e-12, 1e-12);
  CHECK_NEAR(rA.momentCoefficient, kCm250, 1e-12, 1e-12);

  // (b) X change with Y=0: (0.25,0) -> (0,0).
  app::cli::handleXycmCore(composition, out, 0.0, 0.0, true, true);
  const xfoil::AnalysisResult rB =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(rB.successful);
  CHECK_NEAR(rB.liftCoefficient, kCl, 1e-12, 1e-12);
  CHECK_NEAR(rB.momentCoefficient, kCm00, 1e-12, 1e-12);

  // (c) Y change with X=0.25: (0,0) -> (0.25,0.1).
  app::cli::handleXycmCore(composition, out, 0.25, 0.1, true, true);
  const xfoil::AnalysisResult rC =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(rC.successful);
  CHECK_NEAR(rC.liftCoefficient, kCl, 1e-12, 1e-12);
  CHECK_NEAR(rC.momentCoefficient, kCm251, 1e-12, 1e-12);

  // (d) combined X/Y change: (0.25,0.1) -> (-0.5,0.1).
  app::cli::handleXycmCore(composition, out, -0.5, 0.1, true, true);
  const xfoil::AnalysisResult rD =
      session::runSpecAlpha(composition, xfoil::Degrees{4.0}, nullptr);
  CHECK(rD.successful);
  CHECK_NEAR(rD.liftCoefficient, kCl, 1e-12, 1e-12);
  CHECK_NEAR(rD.momentCoefficient, kCmM51, 1e-12, 1e-12);
}

// XYCM changes only the reference point and the lastResult status:
// panel coordinates, panel origin, wake, mapping and BL state remain
// preserved; the argument-path output is silent.
TEST(unit, CoreXycmHandlerNoGeometryPanelWakeMappingBlMutation) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult r1 =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(r1.successful);

  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> panelY = composition.paneling.panel.y;
  const std::size_t panelCount = composition.paneling.panel.x.size();
  const auto origin = composition.paneling.origin;
  const bool systemValid = composition.viscous.systemValid;
  const double systemMach = composition.viscous.systemMach;
  const double systemQinf = composition.viscous.systemQinf;
  const bool wakeValid = composition.viscous.wakeValid;
  const double awake = composition.viscous.awake;
  const bool converged = composition.viscous.converged;

  std::ostringstream out;
  app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, true);

  CHECK(out.str().empty());  // argument path is canonical silently
  CHECK_NEAR(composition.operating.momentReferenceX, 0.5, 0.0, 0.0);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK(composition.paneling.panel.y == panelY);
  CHECK_EQ(composition.paneling.panel.x.size(), panelCount);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_NEAR(composition.viscous.systemMach, systemMach, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.systemQinf, systemQinf, 0.0, 0.0);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_NEAR(composition.viscous.awake, awake, 0.0, 0.0);
  CHECK_EQ(composition.viscous.converged, converged);
}

// viscous next point: the new reference point acts on CM, not on
// Cl/Cd (CLCALC contract; the exact transformation is evidenced at core
// level in XycmReferenceTests against It24.16).
TEST(unit, CoreXycmHandlerViscousNextPointContract) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult r1 =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(r1.successful);

  std::ostringstream out;
  app::cli::handleXycmCore(composition, out, 0.5, 0.0, true, true);
  const xfoil::AnalysisResult r2 =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(r2.successful);
  CHECK_NEAR(r2.liftCoefficient, r1.liftCoefficient, 1e-12, 0.0);
  CHECK_NEAR(r2.dragCoefficient, r1.dragCoefficient, 1e-12, 0.0);
  CHECK(std::fabs(r2.momentCoefficient - r1.momentCoefficient) > 1e-6);
}

// independent sessions: XYCM in A does not change B.
TEST(unit, CoreXycmHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  app::cli::handleXycmCore(a, out, 0.5, 0.0, true, true);
  CHECK_NEAR(a.operating.momentReferenceX, 0.5, 0.0, 0.0);
  CHECK_NEAR(b.operating.momentReferenceX, 0.25, 0.0, 0.0);
}

// --- TOP-level name/NINC ---

namespace {

std::string expectedNamePrompt(std::string_view nameDefault) {
  std::ostringstream out;
  app::cli::emitNamePrompt(out, nameDefault);
  return out.str();
}

}  // namespace

// name with a simple inline argument: canonically silent, only the airfoil name changes.
TEST(unit, CoreNameHandlerInlineSimpleNoText) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::NameCoreStatus status = app::cli::handleNameCore(
      composition, out, "Simple", true, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NameCoreStatus::applied));
  CHECK(out.str().empty());
  CHECK_EQ(composition.geometry.name, std::string("Simple"));
  CHECK_EQ(composition.geometry.buffer.name, std::string("Simple"));
}

// name preserves internal spaces; leading and trailing ones are STRIP-trimmed.
TEST(unit, CoreNameHandlerInlineSpacesPreserveInternal) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "   A  B   ", true, "", false);
  CHECK(out.str().empty());
  CHECK_EQ(composition.geometry.name, std::string("A  B"));
}

// name with only a space as argument corresponds to COMARG=' ': NAMMOD
// prompt, no mutation.
TEST(unit, CoreNameHandlerBlankInlineArgumentRequestsPromptNoMutation) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::string before = composition.geometry.name;

  std::ostringstream out;
  const app::cli::NameCoreStatus status =
      app::cli::handleNameCore(composition, out, "   ", true, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NameCoreStatus::promptRequested));
  CHECK_EQ(out.str(), expectedNamePrompt(before));
  CHECK_EQ(composition.geometry.name, before);
}

// name without an argument with an empty answer: the prompt requirement stays, name unchanged.
TEST(unit, CoreNameHandlerNoArgumentBlankAnswerKeepsCurrent) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "", false, "", false);
  const app::cli::NameCoreStatus status =
      app::cli::handleNameCore(composition, out, "", false, "", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NameCoreStatus::applied));
  CHECK_EQ(out.str(), expectedNamePrompt("NACA 2412"));
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
}

// name without an argument with a non-empty answer adopts the answer.
TEST(unit, CoreNameHandlerNoArgumentAnswerSetsNewName) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "", false, "", false);
  app::cli::handleNameCore(composition, out, "", false, "New Name", true);
  CHECK_EQ(out.str(), expectedNamePrompt("NACA 2412"));
  CHECK_EQ(composition.geometry.name, std::string("New Name"));
}

// name without an argument reformats an existing bracketed form (Fortran
// reference file-io/name-ninc-top-level: A [007] -> A [7]).
TEST(unit, CoreNameHandlerNoArgumentReformatsBracketDefault) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A [007]", true, "", false);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "", false, "", false);
  app::cli::handleNameCore(composition, out, "", false, "", true);
  CHECK_EQ(out.str(), expectedNamePrompt("A [7]"));
  CHECK_EQ(composition.geometry.name, std::string("A [7]"));
}

// name passes through CHARACTER*48 and is truncated to 48 characters before STRIP.
TEST(unit, CoreNameHandlerOverlongTruncatesToFortranField) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, std::string(60, 'A'), true, "",
                           false);
  CHECK_EQ(composition.geometry.name, std::string(48, 'A'));
  CHECK_EQ(composition.geometry.buffer.name, std::string(48, 'A'));
}

// name preserves upper/lower case and special characters (LC2UC applies
// only to commands, not to arguments).
TEST(unit, CoreNameHandlerCaseAndSpecialCharactersPreserved) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "from!@#[]", true, "", false);
  CHECK_EQ(composition.geometry.name, std::string("from!@#[]"));
}

// name without geometry changes only the airfoil name field.
TEST(unit, CoreNameHandlerNoGeometrySetsNameOnly) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::NameCoreStatus status =
      app::cli::handleNameCore(composition, out, "NoGeom", true, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NameCoreStatus::applied));
  CHECK(out.str().empty());
  CHECK_EQ(composition.geometry.name, std::string("NoGeom"));
  CHECK(!composition.geometry.valid);
  CHECK(!composition.paneling.panelingValid);
}

// name for an ABCOPY panel: panel coordinates, origin and revisions remain.
TEST(unit, CoreNameHandlerPreservesAbcopyState) {
  session::SessionComposition composition;
  std::ostringstream loadOut;
  app::cli::handleLoadCore(composition, kLoadReferenceFile, "naca2412buf",
                           loadOut);
  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const auto origin = composition.paneling.origin;
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "Renamed", true, "", false);

  CHECK_EQ(composition.geometry.name, std::string("Renamed"));
  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK(static_cast<bool>(metrics) == static_cast<bool>(metricsAfter));
}

// name for a PANGEN panel: origin pangen and contour are retained.
TEST(unit, CoreNameHandlerPreservesPangenPanel) {
  session::SessionComposition composition;
  std::ostringstream nacaOut;
  testing::handleLegacyNacaCore(composition, nacaOut, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "Renamed", true, "", false);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::pangen));
}

// name changes no numerical, analysis, wake, BL or policy state
// and leaves lastResult unchanged as in Fortran.
TEST(unit, CoreNameHandlerNoNumericalAnalysisWakeBlPolicyMutation) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult before =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(before.successful);

  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> panelY = composition.paneling.panel.y;
  const auto origin = composition.paneling.origin;
  const bool systemValid = composition.viscous.systemValid;
  const double systemMach = composition.viscous.systemMach;
  const double systemQinf = composition.viscous.systemQinf;
  const bool wakeValid = composition.viscous.wakeValid;
  const double awake = composition.viscous.awake;
  const bool converged = composition.viscous.converged;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const double xref = composition.operating.momentReferenceX;
  const double yref = composition.operating.momentReferenceY;

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "Renamed", true, "", false);

  CHECK(composition.paneling.panel.x == panelX);
  CHECK(composition.paneling.panel.y == panelY);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_NEAR(composition.viscous.systemMach, systemMach, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.systemQinf, systemQinf, 0.0, 0.0);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_NEAR(composition.viscous.awake, awake, 0.0, 0.0);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  CHECK_NEAR(composition.operating.momentReferenceX, xref, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, yref, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             before.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(composition.result.lastResult.momentCoefficient,
             before.momentCoefficient, 0.0, 0.0);
}

// the writer path reads the new airfoil name from the session (no SAVE
// command, only the existing write core).
TEST(unit, CoreNameHandlerWriterConsumesNewSessionName) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "A B C", true, "", false);

  const std::vector<xfoil::Point2D> coordinates =
      app::cli::currentAirfoilCoordinatesCore(composition);
  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::labeled;
  options.name = composition.geometry.name;
  const testing::ScopedTempDirectory work(
      "CoreNameHandlerWriterConsumesNewSessionName");
  const auto tmp = work.makePath("name-writer.dat");
  const auto written = app::writeAirfoil(tmp, coordinates, options);
  CHECK(written.success);
  std::ifstream file(tmp);
  std::string firstLine;
  std::getline(file, firstLine);
  CHECK_EQ(firstLine, std::string("A B C"));
}

// NINC without brackets appends [1], not based on the last digit.
TEST(unit, CoreNincHandlerNoBracketAppendsOne) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A1", true, "", false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  const app::cli::NincCoreStatus status =
      app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NincCoreStatus::applied));
  CHECK_EQ(out.str(), expectedNamePrompt("A1 [1]"));
  CHECK_EQ(composition.geometry.name, std::string("A1 [1]"));
}

// NINC with an internal space and repeated NINC: [1], [2].
TEST(unit, CoreNincHandlerRepeatedIncrements) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B", true, "", false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(composition.geometry.name, std::string("A B [1]"));
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(composition.geometry.name, std::string("A B [2]"));
}

// NINC with brackets: 9 -> 10 and 10 -> 11 (Fortran reference name_carry.dat).
TEST(unit, CoreNincHandlerBracketCarryNineToTen) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A [9]", true, "", false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(out.str(), expectedNamePrompt("A [10]"));
  CHECK_EQ(composition.geometry.name, std::string("A [10]"));
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(composition.geometry.name, std::string("A [11]"));
}

// NINC at 99: MOD(99+1,100)=0, NAMDEF stays the original name.
TEST(unit, CoreNincHandlerNinetyNineKeepsOriginalDefault) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A [99]", true, "", false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(out.str(), expectedNamePrompt("A [99]"));
  CHECK_EQ(composition.geometry.name, std::string("A [99]"));
}

// NINC with leading zeros: list-directed READ, no leading zeros in the tag.
TEST(unit, CoreNincHandlerLeadingZerosNormalize) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A [007]", true, "", false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(out.str(), expectedNamePrompt("A [8]"));
  CHECK_EQ(composition.geometry.name, std::string("A [8]"));
}

// NINC with a multi-digit number: [12] -> [13].
TEST(unit, CoreNincHandlerMultipleDigitsIncrement) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A [12]", true, "", false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(composition.geometry.name, std::string("A [13]"));
}

// NINC at maximum field length: the NAMMOD requirement is truncated to
// CHARACTER*48 and contains no visible version tag (Fortran reference
// name_max_ninc.dat).
TEST(unit, CoreNincHandlerAtMaximumFieldLengthTruncatesTag) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, std::string(48, 'A'), true, "",
                           false);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(out.str(), expectedNamePrompt(std::string(48, 'A')));
  CHECK_EQ(composition.geometry.name, std::string(48, 'A'));
}

// NINC without geometry: the canonical requirement " [1]" is STRIP-trimmed
// before the name is set (Fortran reference state/name-ninc-no-geometry).
TEST(unit, CoreNincHandlerNoGeometry) {
  session::SessionComposition composition;
  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  const app::cli::NincCoreStatus status =
      app::cli::handleNincCore(composition, out, "", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NincCoreStatus::applied));
  CHECK_EQ(out.str(), expectedNamePrompt(" [1]"));
  CHECK_EQ(composition.geometry.name, std::string("[1]"));
  CHECK(!composition.geometry.valid);
}

// NINC without prompt answer: only prompt, no mutation.
TEST(unit, CoreNincHandlerNoAnswerNoMutation) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::string before = composition.geometry.name;

  std::ostringstream out;
  const app::cli::NincCoreStatus status =
      app::cli::handleNincCore(composition, out, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::NincCoreStatus::promptRequested));
  CHECK_EQ(out.str(), expectedNamePrompt("NACA 2412 [1]"));
  CHECK_EQ(composition.geometry.name, before);
}

// NINC with non-empty prompt answer overwrites the requirement.
TEST(unit, CoreNincHandlerAnswerOverridesDefault) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "Override", true);
  CHECK_EQ(out.str(), expectedNamePrompt("NACA 2412 [1]"));
  CHECK_EQ(composition.geometry.name, std::string("Override"));
}

// NINC via runs CHARACTER*48-answer before STRIP.
TEST(unit, CoreNincHandlerOverlongAnswerTruncates) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, std::string(60, 'B'), true);
  CHECK_EQ(composition.geometry.name, std::string(48, 'B'));
}

// NINC changes no numerical, analysis, wake, BL or policy state.
TEST(unit, CoreNincHandlerNoNumericalAnalysisWakeBlPolicyMutation) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult before =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(before.successful);

  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::vector<double> panelY = composition.paneling.panel.y;
  const auto origin = composition.paneling.origin;
  const bool systemValid = composition.viscous.systemValid;
  const double systemMach = composition.viscous.systemMach;
  const double systemQinf = composition.viscous.systemQinf;
  const bool wakeValid = composition.viscous.wakeValid;
  const double awake = composition.viscous.awake;
  const bool converged = composition.viscous.converged;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const double xref = composition.operating.momentReferenceX;
  const double yref = composition.operating.momentReferenceY;

  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);

  CHECK(composition.paneling.panel.x == panelX);
  CHECK(composition.paneling.panel.y == panelY);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_NEAR(composition.viscous.systemMach, systemMach, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.systemQinf, systemQinf, 0.0, 0.0);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_NEAR(composition.viscous.awake, awake, 0.0, 0.0);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  CHECK_NEAR(composition.operating.momentReferenceX, xref, 0.0, 0.0);
  CHECK_NEAR(composition.operating.momentReferenceY, yref, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             before.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(composition.result.lastResult.momentCoefficient,
             before.momentCoefficient, 0.0, 0.0);
}

// the writer path reads the NINC names from the session.
TEST(unit, CoreNincHandlerWriterConsumesIncrementedName) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  std::ostringstream out;
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);

  const std::vector<xfoil::Point2D> coordinates =
      app::cli::currentAirfoilCoordinatesCore(composition);
  app::AirfoilWriteOptions options;
  options.format = app::AirfoilOutputFormat::labeled;
  options.name = composition.geometry.name;
  const testing::ScopedTempDirectory work(
      "CoreNincHandlerWriterConsumesIncrementedName");
  const auto tmp = work.makePath("ninc-writer.dat");
  const auto written = app::writeAirfoil(tmp, coordinates, options);
  CHECK(written.success);
  std::ifstream file(tmp);
  std::string firstLine;
  std::getline(file, firstLine);
  CHECK_EQ(firstLine, std::string("A B C [1]"));
}

// name/NINC change NORM-, REVE- and DELI-policy not.
TEST(unit, CoreNameNincHandlersPreserveFilePolicies) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  session::setNormalizeOnLoad(composition, true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 2);

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "Name", true, "", false);
  app::cli::handleNincCore(composition, out, "", false);
  app::cli::handleNincCore(composition, out, "", true);

  CHECK_EQ(session::normalizeOnLoad(composition), true);
  CHECK_EQ(session::writeClockwise(composition), true);
  CHECK_EQ(session::delimiter(composition), 2);
}

// TOP-level name changes no OPER/polar names: the session holds only the
// airfoil name; runtime/configuration state stays unchanged.
TEST(unit, CoreNameHandlerDoesNotTouchOperatingOrPolarState) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const auto operatingBefore = composition.operating;
  const auto configBefore = composition.config;

  std::ostringstream out;
  app::cli::handleNameCore(composition, out, "Profile", true, "", false);

  CHECK_NEAR(composition.operating.qinf, operatingBefore.qinf, 0.0, 0.0);
  CHECK_NEAR(composition.operating.machReynolds.reinf1,
             operatingBefore.machReynolds.reinf1, 0.0, 0.0);
  CHECK_NEAR(composition.operating.machReynolds.minf1,
             operatingBefore.machReynolds.minf1, 0.0, 0.0);
  CHECK_EQ(composition.operating.viscous, operatingBefore.viscous);
  CHECK_EQ(composition.config.viscous, configBefore.viscous);
}

// independent sessions: Name/NINC in A does not change B.
TEST(unit, CoreNameNincHandlerIndependentSessions) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  app::cli::handleNameCore(a, out, "A", true, "", false);
  app::cli::handleNincCore(a, out, "", true);
  CHECK_EQ(a.geometry.name, std::string("A [1]"));
  CHECK(b.geometry.name.empty());
}

// deterministic text output: two fresh sessions deliver an identical
// NAMMOD-prompt.
TEST(unit, CoreNameNincHandlerDeterministicText) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handleNincCore(a, outA, "", false);
  app::cli::handleNincCore(b, outB, "", false);
  CHECK_EQ(outA.str(), outB.str());
}

// --- TOP-level SAVE ---

namespace {

// RAII test working directory per test run, with a directory below
// "build/test-data". The relative path names checked in the contract
// remain unchanged, but files appear process-isolated instead of in a
// shared build directory.
class ScopedBuildTestDataWorkspace {
 public:
  explicit ScopedBuildTestDataWorkspace(const std::string& tag)
      : work_("geom-" + tag, true) {
    std::error_code error;
    std::filesystem::create_directories(work_.path() / "build" / "test-data",
                                        error);
  }

  ScopedBuildTestDataWorkspace(const ScopedBuildTestDataWorkspace&) = delete;
  ScopedBuildTestDataWorkspace& operator=(const ScopedBuildTestDataWorkspace&) =
      delete;

 private:
  testing::ScopedTempDirectory work_;
};

// RAII target file in its own unique test directory
// (absolute path; no shared build/test-data path,
// no manual cleanup).
class TestSavePath {
 public:
  explicit TestSavePath(const std::string& fileName)
      : work_("save-" + fileName),
        path_(work_.makePath("build/test-data/" + fileName)) {
    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);
  }

  TestSavePath(const TestSavePath&) = delete;
  TestSavePath& operator=(const TestSavePath&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

 private:
  testing::ScopedTempDirectory work_;
  std::filesystem::path path_;
};

std::string readBinaryFile(const std::filesystem::path& path) {
  return readWholeFile(path);
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

std::size_t countLines(const std::string& text) {
  std::size_t lines = 0;
  for (const char character : text) {
    if (character == '\n') {
      ++lines;
    }
  }
  return lines;
}

std::string firstLine(const std::string& text) {
  const std::size_t newline = text.find('\n');
  return newline == std::string::npos ? text : text.substr(0, newline);
}

std::string expectedSaveOverwritePrompt() {
  std::ostringstream out;
  app::cli::emitSaveOverwritePrompt(out);
  return out.str();
}

std::string expectedSaveBadFilename() {
  std::ostringstream out;
  app::cli::emitSaveBadFilename(out);
  return out.str();
}

std::string expectedSaveFilenamePrompt() {
  std::ostringstream out;
  app::cli::emitAsksPrompt(out, "Enter output filename");
  return out.str();
}

constexpr const char* kSaveNameReference =
    "tests/fortran-reference/cases/file-io/save-reve-delimiter-name/generated/"
    "save_name.dat";
constexpr const char* kSaveReveCommaReference =
    "tests/fortran-reference/cases/file-io/save-reve-delimiter-name/generated/"
    "save_reve_comma.dat";
constexpr const char* kSaveTabReference =
    "tests/fortran-reference/cases/file-io/save-reve-delimiter-name/generated/"
    "save_tab.dat";
constexpr const char* kSaveNoGeometryInitialReference =
    "tests/fortran-reference/cases/file-io/save-no-geometry-name/generated/"
    "nogeom_initial.dat";
constexpr const char* kSaveNoGeometryBlankReference =
    "tests/fortran-reference/cases/file-io/save-no-geometry-name/generated/"
    "nogeom_blank.dat";

}  // namespace

// SAVE writes the current panel contour, not the buffer.
TEST(unit, CoreSaveHandlerWritesCurrentPanelNotBuffer) {
  TestSavePath file("save-panel.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK_EQ(composition.geometry.buffer.x.size(), 245u);
  CHECK_EQ(composition.paneling.panel.x.size(), 160u);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK(out.str().empty());

  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(firstLine(contents), std::string("NACA 2412"));
  CHECK_EQ(countLines(contents), composition.paneling.panel.x.size() + 1);
}

// SAVE header and file bytes match the Fortran reference after name/NINC.
TEST(unit, CoreSaveHandlerNameHeaderMatchesFortranReference) {
  TestSavePath file("save-name-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), readBinaryFile(kSaveNameReference));
}

// REVE and DELI comma produce a byte-identical Fortran file.
TEST(unit, CoreSaveHandlerReveCommaMatchesFortranReference) {
  TestSavePath file("save-reve-comma-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 1);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kSaveReveCommaReference));
}

// DELI tab after REVE produces a byte-identical Fortran file.
TEST(unit, CoreSaveHandlerTabMatchesFortranReference) {
  TestSavePath file("save-tab-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 2);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), readBinaryFile(kSaveTabReference));
}

// SAVE without geometry before any name assignment writes the INIT field
// with 32 spaces and no coordinates.
TEST(unit, CoreSaveHandlerNoGeometryInitialNameMatchesFortran) {
  TestSavePath file("save-nogeom-initial.dat");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK(out.str().empty());
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kSaveNoGeometryInitialReference));
}

// name with an empty answer produces an explicit empty namespace (empty line).
TEST(unit, CoreSaveHandlerNoGeometryExplicitBlankNameMatchesFortran) {
  TestSavePath file("save-nogeom-blank.dat");
  session::SessionComposition composition;
  std::ostringstream nameOut;
  app::cli::handleNameCore(composition, nameOut, "", false, "", true);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kSaveNoGeometryBlankReference));
}

// SAVE with maximum name length writes 48 characters in the header.
TEST(unit, CoreSaveHandlerMaxNameLengthHeader) {
  TestSavePath file("save-max-name.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, std::string(60, 'A'), true, "",
                           false);

  std::ostringstream out;
  app::cli::handleSaveCore(composition, out, file.path().string(), true, "",
                           false, "", false);
  CHECK_EQ(firstLine(readBinaryFile(file.path())), std::string(48, 'A'));
}

// existing file without an answer: prompt, no mutation, file stays.
TEST(unit, CoreSaveHandlerExistingFileRequestsOverwriteNoMutation) {
  TestSavePath file("save-overwrite-pending.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::overwriteConfirmationRequired));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with N/n: rejection, file stays unchanged.
TEST(unit, CoreSaveHandlerOverwriteDeclineKeepsFile) {
  TestSavePath file("save-overwrite-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::SaveCoreStatus query = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::SaveCoreStatus::overwriteConfirmationRequired));
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "N", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::notSavedDeclined));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt() +
                          std::string(" Current airfoil not saved.\n"));
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with Y: replaced exactly once.
TEST(unit, CoreSaveHandlerOverwriteAcceptReplacesFile) {
  TestSavePath file("save-overwrite-accept.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::SaveCoreStatus query = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::SaveCoreStatus::overwriteConfirmationRequired));
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "Y", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK(readBinaryFile(file.path()).find("original") == std::string::npos);
}

// empty answer for the overwrite prompt is confirmed canonically (default Y).
TEST(unit, CoreSaveHandlerOverwriteBlankAnswerAccepts) {
  TestSavePath file("save-overwrite-blank.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));
  CHECK(readBinaryFile(file.path()).find("original") == std::string::npos);
}

// directory as target: canonical bad-filename path without overwrite prompt.
TEST(unit, CoreSaveHandlerDirectoryTargetBadFilename) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreSaveHandlerDirectoryTargetBadFilename");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "save-dir-target";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  CHECK(!error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  std::filesystem::remove_all(dir, error);
}

// SAVE without filenames: ASKS prompt; an empty answer leads to bad filename.
TEST(unit, CoreSaveHandlerFilenamePromptAndBlankAnswer) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream promptOut;
  const app::cli::SaveCoreStatus promptStatus = app::cli::handleSaveCore(
      composition, promptOut, "", false, "", false, "", false);
  CHECK_EQ(static_cast<int>(promptStatus),
           static_cast<int>(app::cli::SaveCoreStatus::filenamePromptRequested));
  CHECK_EQ(promptOut.str(), expectedSaveFilenamePrompt());

  std::ostringstream badOut;
  const app::cli::SaveCoreStatus badStatus = app::cli::handleSaveCore(
      composition, badOut, "", false, "", true, "", false);
  CHECK_EQ(static_cast<int>(badStatus),
           static_cast<int>(app::cli::SaveCoreStatus::badFilename));
  CHECK_EQ(badOut.str(), expectedSaveBadFilename());
}

// SAVE changes no session state on success.
TEST(unit, CoreSaveHandlerSuccessLeavesSessionStateUnchanged) {
  TestSavePath file("save-state-success.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult before =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(before.successful);

  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const auto origin = composition.paneling.origin;
  const std::string name = composition.geometry.name;
  const bool systemValid = composition.viscous.systemValid;
  const bool wakeValid = composition.viscous.wakeValid;
  const bool converged = composition.viscous.converged;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const double xref = composition.operating.momentReferenceX;

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::written));

  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  CHECK_NEAR(composition.operating.momentReferenceX, xref, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             before.liftCoefficient, 0.0, 0.0);
}

// SAVE changes no session state on overwrite rejection.
TEST(unit, CoreSaveHandlerDeclineLeavesSessionStateUnchanged) {
  TestSavePath file("save-state-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;

  std::ostringstream out;
  const app::cli::SaveCoreStatus query = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::SaveCoreStatus::overwriteConfirmationRequired));
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, file.path().string(), true, "", false, "N", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::notSavedDeclined));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// SAVE changes no session state on bad-filename error.
TEST(unit, CoreSaveHandlerBadFilenameLeavesSessionStateUnchanged) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreSaveHandlerBadFilenameLeavesSessionStateUnchanged");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "save-state-bad-dir";
  std::error_code error;
  std::filesystem::create_directories(dir, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;

  std::ostringstream out;
  const app::cli::SaveCoreStatus status = app::cli::handleSaveCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::SaveCoreStatus::badFilename));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  std::filesystem::remove_all(dir, error);
}

// independent sessions with different output-path policies.
TEST(unit, CoreSaveHandlerIndependentSessionsDifferentPolicies) {
  TestSavePath fileA("save-independent-a.dat");
  TestSavePath fileB("save-independent-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);
  session::setDelimiter(a, 1);
  session::setWriteOrientation(a, true);

  std::ostringstream out;
  app::cli::handleSaveCore(a, out, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handleSaveCore(b, out, fileB.path().string(), true, "", false, "",
                           false);

  const std::string contentsA = readBinaryFile(fileA.path());
  const std::string contentsB = readBinaryFile(fileB.path());
  CHECK(contentsA.find(',') != std::string::npos);
  CHECK(contentsB.find(',') == std::string::npos);
  CHECK_EQ(session::delimiter(b), 0);
  CHECK_EQ(session::writeClockwise(b), false);
}

// deterministic file bytes and text output.
TEST(unit, CoreSaveHandlerDeterministicBytesAndText) {
  TestSavePath fileA("save-deterministic-a.dat");
  TestSavePath fileB("save-deterministic-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);

  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handleSaveCore(a, outA, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handleSaveCore(b, outB, fileB.path().string(), true, "", false, "",
                           false);
  CHECK_EQ(readBinaryFile(fileA.path()), readBinaryFile(fileB.path()));
  CHECK_EQ(outA.str(), outB.str());
}

// --- TOP-level PSAV ---

namespace {

constexpr const char* kPsavNameReference =
    "tests/fortran-reference/cases/file-io/psav-reve-delimiter-name/generated/"
    "psav_name.dat";
constexpr const char* kPsavReveCommaReference =
    "tests/fortran-reference/cases/file-io/psav-reve-delimiter-name/generated/"
    "psav_reve_comma.dat";
constexpr const char* kPsavTabReference =
    "tests/fortran-reference/cases/file-io/psav-reve-delimiter-name/generated/"
    "psav_tab.dat";
constexpr const char* kPsavNoGeometryReference =
    "tests/fortran-reference/cases/file-io/psav-no-geometry/generated/"
    "psav_nogeom.dat";
constexpr const char* kPsavInteNoCurrentReference =
    "tests/fortran-reference/cases/file-io/psav-inte-buffer-no-current/"
    "generated/psav_after_inte_nocurrent.dat";

}  // namespace

// PSAV writes the current panel contour without a header, not the buffer.
TEST(unit, CorePsavHandlerWritesCurrentPanelNotBuffer) {
  TestSavePath file("psav-panel.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK_EQ(composition.geometry.buffer.x.size(), 245u);
  CHECK_EQ(composition.paneling.panel.x.size(), 160u);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK(out.str().empty());

  const std::string contents = readBinaryFile(file.path());
  CHECK(firstLine(contents).find("NACA") == std::string::npos);
  CHECK(firstLine(contents).find("1.000000") != std::string::npos);
  CHECK_EQ(countLines(contents), composition.paneling.panel.x.size());
}

// PSAV carries no header after name/NINC; the file bytes match the
// canonical Fortran reference.
TEST(unit, CorePsavHandlerNameNincProducesNoHeader) {
  TestSavePath file("psav-name-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  const std::string contents = readBinaryFile(file.path());
  CHECK(firstLine(contents).find("A B C") == std::string::npos);
  CHECK(contents.find("A B C") == std::string::npos);
  CHECK_EQ(contents, readBinaryFile(kPsavNameReference));
}

// REVE and DELI comma produce a byte-identical Fortran PSAV file.
TEST(unit, CorePsavHandlerReveCommaMatchesFortranReference) {
  TestSavePath file("psav-reve-comma-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 1);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kPsavReveCommaReference));
}

// DELI tab after REVE produces a byte-identical Fortran PSAV file.
TEST(unit, CorePsavHandlerTabMatchesFortranReference) {
  TestSavePath file("psav-tab-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 2);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), readBinaryFile(kPsavTabReference));
}

// PSAV without geometry writes a completely empty file.
TEST(unit, CorePsavHandlerNoGeometryWritesEmptyFile) {
  TestSavePath file("psav-nogeom.dat");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK(out.str().empty());
  CHECK_EQ(readBinaryFile(file.path()), std::string());
  CHECK_EQ(readBinaryFile(file.path()), readBinaryFile(kPsavNoGeometryReference));
}

// INTE from two disk sources fills only the buffer; lacking a current
// contour, PSAV writes an empty file.
TEST(unit, CorePsavHandlerInteBufferWithoutCurrentWritesEmptyFile) {
  TestSavePath file("psav-inte-nocurrent.dat");
  session::SessionComposition composition;
  std::ostringstream inteOut;
  const app::cli::InteCoreStatus inteStatus = handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "", inteOut);
  CHECK_EQ(static_cast<int>(inteStatus),
           static_cast<int>(app::cli::InteCoreStatus::success));
  CHECK(composition.geometry.valid);
  CHECK(!composition.paneling.panelingValid);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), std::string());
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kPsavInteNoCurrentReference));
}

// SAVE and PSAV remain typed as domain-specific separate contracts.
TEST(unit, CorePsavHandlerSaveContractRemainsLabeled) {
  TestSavePath saveFile("psav-save-contract-save.dat");
  TestSavePath psavFile("psav-save-contract-psav.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleSaveCore(composition, out, saveFile.path().string(), true, "",
                           false, "", false);
  app::cli::handlePsavCore(composition, out, psavFile.path().string(), true, "",
                           false, "", false);

  const std::string saveContents = readBinaryFile(saveFile.path());
  const std::string psavContents = readBinaryFile(psavFile.path());
  CHECK_EQ(firstLine(saveContents), std::string("NACA 2412"));
  CHECK(firstLine(psavContents).find("NACA") == std::string::npos);
  CHECK_EQ(saveContents, std::string("NACA 2412\n") + psavContents);
}

// the inline argument corresponds to COMARG trimmed by ASKC/STRIP.
TEST(unit, CorePsavHandlerInlineArgumentTrimsSpaces) {
  TestSavePath file("psav-inline-trim.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  const std::string argument =
      "  " + file.path().string() + "   ";
  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, argument, true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK(out.str().empty());
  CHECK(std::filesystem::exists(file.path()));
}

// the ASKS prompt answer is not trimmed; spaces inside the file name are
// retained. The canonical ASKS answer returns the complete path; here a
// space in the last path component and a trailing space lead to the exact
// target files.
TEST(unit, CorePsavHandlerPromptAnswerKeepsSpaces) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CorePsavHandlerPromptAnswerKeepsSpaces");
  std::error_code error;
  std::filesystem::create_directories("build/test-data", error);
  CHECK(!error);

  const std::filesystem::path leading =
      std::filesystem::path("build/test-data") / " psav-leading.dat";
  const std::filesystem::path trailing =
      std::filesystem::path("build/test-data") / "psav-trailing.dat ";
  std::filesystem::remove(leading, error);
  std::filesystem::remove(trailing, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream leadingOut;
  const app::cli::PsavCoreStatus leadingStatus = app::cli::handlePsavCore(
      composition, leadingOut, "", false, "build/test-data/ psav-leading.dat",
      true, "", false);
  CHECK_EQ(static_cast<int>(leadingStatus),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK(std::filesystem::exists(leading));

  std::ostringstream trailingOut;
  const app::cli::PsavCoreStatus trailingStatus = app::cli::handlePsavCore(
      composition, trailingOut, "", false, "build/test-data/psav-trailing.dat ",
      true, "", false);
  CHECK_EQ(static_cast<int>(trailingStatus),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK(std::filesystem::exists(trailing));

  std::filesystem::remove(leading, error);
  std::filesystem::remove(trailing, error);
}

// existing file without an answer: prompt, no mutation, file stays.
TEST(unit, CorePsavHandlerExistingFileRequestsOverwriteNoMutation) {
  TestSavePath file("psav-overwrite-pending.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::overwriteConfirmationRequired));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with N/n: rejection, file stays unchanged.
TEST(unit, CorePsavHandlerOverwriteDeclineKeepsFile) {
  TestSavePath file("psav-overwrite-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::PsavCoreStatus query = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::PsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "n", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::notSavedDeclined));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt() +
                          std::string(" Current airfoil not saved.\n"));
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with Y: replaced exactly once.
TEST(unit, CorePsavHandlerOverwriteAcceptReplacesFile) {
  TestSavePath file("psav-overwrite-accept.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::PsavCoreStatus query = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::PsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "Y", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK(readBinaryFile(file.path()).find("original") == std::string::npos);
}

// READ(*,1000) reads the first character; a leading space is not 'N'
// and is confirmed canonically.
TEST(unit, CorePsavHandlerOverwriteLeadingSpaceAnswerAccepts) {
  TestSavePath file("psav-overwrite-leading-space.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, " N", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));
  CHECK(readBinaryFile(file.path()).find("original") == std::string::npos);
}

// directory as target: canonical bad-filename path without overwrite prompt.
TEST(unit, CorePsavHandlerDirectoryTargetBadFilename) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CorePsavHandlerDirectoryTargetBadFilename");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "psav-dir-target";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  CHECK(!error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  std::filesystem::remove_all(dir, error);
}

// path not writable in a missing directory: bad filename, no
// session mutation.
TEST(unit, CorePsavHandlerMissingDirectoryBadFilename) {
  const std::filesystem::path target =
      std::filesystem::path("build/test-data") /
      "psav-missing-dir/psav.dat";
  std::error_code error;
  std::filesystem::remove_all(target.parent_path(), error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, target.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK(!std::filesystem::exists(target, error));
}

// PSAV without filenames: ASKS prompt; an empty answer leads to bad filename.
TEST(unit, CorePsavHandlerFilenamePromptAndBlankAnswer) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream promptOut;
  const app::cli::PsavCoreStatus promptStatus = app::cli::handlePsavCore(
      composition, promptOut, "", false, "", false, "", false);
  CHECK_EQ(static_cast<int>(promptStatus),
           static_cast<int>(app::cli::PsavCoreStatus::filenamePromptRequested));
  CHECK_EQ(promptOut.str(), expectedSaveFilenamePrompt());

  std::ostringstream badOut;
  const app::cli::PsavCoreStatus badStatus = app::cli::handlePsavCore(
      composition, badOut, "", false, "", true, "", false);
  CHECK_EQ(static_cast<int>(badStatus),
           static_cast<int>(app::cli::PsavCoreStatus::badFilename));
  CHECK_EQ(badOut.str(), expectedSaveBadFilename());
}

// PSAV changes no session state on success.
TEST(unit, CorePsavHandlerSuccessLeavesSessionStateUnchanged) {
  TestSavePath file("psav-state-success.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult before =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(before.successful);

  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const auto origin = composition.paneling.origin;
  const std::string name = composition.geometry.name;
  const bool nameExplicit = composition.geometry.nameExplicitlySet;
  const bool systemValid = composition.viscous.systemValid;
  const bool wakeValid = composition.viscous.wakeValid;
  const bool converged = composition.viscous.converged;
  const auto viscals = composition.viscous.viscals;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const double xref = composition.operating.momentReferenceX;
  const int panelCount = session::panelingParameters(composition).panelCount;
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::written));

  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(composition.geometry.nameExplicitlySet, nameExplicit);
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(composition.viscous.viscals.qinv.size(), viscals.qinv.size());
  CHECK_EQ(composition.viscous.viscals.dij.size(), viscals.dij.size());
  CHECK_EQ(composition.viscous.viscals.dijWakeValid, viscals.dijWakeValid);
  CHECK(composition.viscous.viscals.itran == viscals.itran);
  CHECK_NEAR(composition.viscous.viscals.alfa, viscals.alfa, 0.0, 0.0);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  CHECK_EQ(session::panelingParameters(composition).panelCount, panelCount);
  CHECK_NEAR(composition.operating.momentReferenceX, xref, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             before.liftCoefficient, 0.0, 0.0);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK_EQ(static_cast<bool>(metrics), static_cast<bool>(metricsAfter));
  if (metrics && metricsAfter) {
    CHECK_NEAR(metrics->chord, metricsAfter->chord, 0.0, 0.0);
  }
}

// PSAV changes no session state on overwrite rejection.
TEST(unit, CorePsavHandlerDeclineLeavesSessionStateUnchanged) {
  TestSavePath file("psav-state-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;

  std::ostringstream out;
  const app::cli::PsavCoreStatus query = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::PsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, file.path().string(), true, "", false, "N", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::notSavedDeclined));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// PSAV changes no session state on bad-filename error.
TEST(unit, CorePsavHandlerBadFilenameLeavesSessionStateUnchanged) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CorePsavHandlerBadFilenameLeavesSessionStateUnchanged");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "psav-state-bad-dir";
  std::error_code error;
  std::filesystem::create_directories(dir, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;

  std::ostringstream out;
  const app::cli::PsavCoreStatus status = app::cli::handlePsavCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::PsavCoreStatus::badFilename));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  std::filesystem::remove_all(dir, error);
}

// independent sessions with different output-path policies.
TEST(unit, CorePsavHandlerIndependentSessionsDifferentPolicies) {
  TestSavePath fileA("psav-independent-a.dat");
  TestSavePath fileB("psav-independent-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);
  session::setDelimiter(a, 1);
  session::setWriteOrientation(a, true);

  std::ostringstream out;
  app::cli::handlePsavCore(a, out, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handlePsavCore(b, out, fileB.path().string(), true, "", false, "",
                           false);

  const std::string contentsA = readBinaryFile(fileA.path());
  const std::string contentsB = readBinaryFile(fileB.path());
  CHECK(contentsA.find(',') != std::string::npos);
  CHECK(contentsB.find(',') == std::string::npos);
  CHECK_EQ(session::delimiter(b), 0);
  CHECK_EQ(session::writeClockwise(b), false);
}

// deterministic file bytes and text output.
TEST(unit, CorePsavHandlerDeterministicBytesAndText) {
  TestSavePath fileA("psav-deterministic-a.dat");
  TestSavePath fileB("psav-deterministic-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);

  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handlePsavCore(a, outA, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handlePsavCore(b, outB, fileB.path().string(), true, "", false, "",
                           false);
  CHECK_EQ(readBinaryFile(fileA.path()), readBinaryFile(fileB.path()));
  CHECK_EQ(outA.str(), outB.str());
}

// --- TOP-level USAV ---

namespace {

constexpr const char* kUsavAfterNacaReference =
    "tests/fortran-reference/cases/file-io/usav-source-pcop/generated/"
    "after_naca.dat";
constexpr const char* kUsavAfterPcopReference =
    "tests/fortran-reference/cases/file-io/usav-source-pcop/generated/"
    "after_pcop.dat";
constexpr const char* kUsavNameReference =
    "tests/fortran-reference/cases/file-io/usav-reve-name-deli/generated/"
    "usav_name.dat";
constexpr const char* kUsavReveReference =
    "tests/fortran-reference/cases/file-io/usav-reve-name-deli/generated/"
    "usav_reve.dat";
constexpr const char* kUsavDelimReference =
    "tests/fortran-reference/cases/file-io/usav-reve-name-deli/generated/"
    "usav_delim.dat";
constexpr const char* kUsavNoGeometryReference =
    "tests/fortran-reference/cases/file-io/usav-no-geometry/generated/"
    "usav_nogeom.dat";
constexpr const char* kUsavInteNoCurrentReference =
    "tests/fortran-reference/cases/file-io/usav-inte-buffer-no-current/"
    "generated/usav_after_inte_nocurrent.dat";

}  // namespace

// USAV writes the current panel contour, not the buffer.
TEST(unit, CoreUsavHandlerWritesCurrentPanelNotBuffer) {
  TestSavePath file("usav-panel.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK_EQ(composition.geometry.buffer.x.size(), 245u);
  CHECK_EQ(composition.paneling.panel.x.size(), 160u);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK(out.str().empty());

  const std::string contents = readBinaryFile(file.path());
  CHECK(firstLine(contents).find("NACA") == std::string::npos);
  CHECK(firstLine(contents).find("1") != std::string::npos);
  CHECK_EQ(countLines(contents), composition.paneling.panel.x.size());
  CHECK_EQ(contents, readBinaryFile(kUsavAfterNacaReference));
}

// PCOP copies the current contour to the buffer; USAV follows the current
// contour and then writes 245 integer lines.
TEST(unit, CoreUsavHandlerPcopSourceMatchesFortranReference) {
  TestSavePath afterNaca("usav-source-naca.dat");
  TestSavePath afterPcop("usav-source-pcop.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleUsavCore(composition, out, afterNaca.path().string(), true,
                           "", false, "", false);
  CHECK_EQ(readBinaryFile(afterNaca.path()),
           readBinaryFile(kUsavAfterNacaReference));

  const app::cli::PcopCoreStatus pcop = app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(pcop),
           static_cast<int>(app::cli::PcopCoreStatus::applied));
  CHECK_EQ(composition.paneling.panel.x.size(), 245u);

  app::cli::handleUsavCore(composition, out, afterPcop.path().string(), true,
                           "", false, "", false);
  CHECK_EQ(readBinaryFile(afterPcop.path()),
           readBinaryFile(kUsavAfterPcopReference));
}

// name/NINC produce no header and no metadata in the USAV file.
TEST(unit, CoreUsavHandlerNameNincProducesNoHeader) {
  TestSavePath file("usav-name-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  const std::string contents = readBinaryFile(file.path());
  CHECK(contents.find("A B C") == std::string::npos);
  CHECK_EQ(contents, readBinaryFile(kUsavNameReference));
}

// REVE reverses the USAV write order.
TEST(unit, CoreUsavHandlerReveMatchesFortranReference) {
  TestSavePath file("usav-reve-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), readBinaryFile(kUsavReveReference));
}

// DELI has no effect in the IFTYP=-1 branch; comma after REVE stays byte-identical
// to the REVE reference.
TEST(unit, CoreUsavHandlerDelimiterIsIgnored) {
  TestSavePath file("usav-delim-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 1);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  const std::string contents = readBinaryFile(file.path());
  CHECK(contents.find(',') == std::string::npos);
  CHECK(contents.find('\t') == std::string::npos);
  CHECK_EQ(contents, readBinaryFile(kUsavDelimReference));
  CHECK_EQ(contents, readBinaryFile(kUsavReveReference));
}

// USAV without geometry writes a completely empty file.
TEST(unit, CoreUsavHandlerNoGeometryWritesEmptyFile) {
  TestSavePath file("usav-nogeom.dat");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK(out.str().empty());
  CHECK_EQ(readBinaryFile(file.path()), std::string());
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kUsavNoGeometryReference));
}

// INTE from two disk sources fills only the buffer; lacking a current
// contour, USAV writes an empty file.
TEST(unit, CoreUsavHandlerInteBufferWithoutCurrentWritesEmptyFile) {
  TestSavePath file("usav-inte-nocurrent.dat");
  session::SessionComposition composition;
  std::ostringstream inteOut;
  const app::cli::InteCoreStatus inteStatus = handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "", inteOut);
  CHECK_EQ(static_cast<int>(inteStatus),
           static_cast<int>(app::cli::InteCoreStatus::success));
  CHECK(composition.geometry.valid);
  CHECK(!composition.paneling.panelingValid);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), std::string());
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kUsavInteNoCurrentReference));
}

// SAVE, PSAV and USAV remain typed as domain-specific separate contracts.
TEST(unit, CoreUsavHandlerSaveAndPsavContractsRemainSeparate) {
  TestSavePath saveFile("usav-contract-save.dat");
  TestSavePath psavFile("usav-contract-psav.dat");
  TestSavePath usavFile("usav-contract-usav.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleSaveCore(composition, out, saveFile.path().string(), true, "",
                           false, "", false);
  app::cli::handlePsavCore(composition, out, psavFile.path().string(), true, "",
                           false, "", false);
  app::cli::handleUsavCore(composition, out, usavFile.path().string(), true, "",
                           false, "", false);

  const std::string saveContents = readBinaryFile(saveFile.path());
  const std::string psavContents = readBinaryFile(psavFile.path());
  const std::string usavContents = readBinaryFile(usavFile.path());
  CHECK_EQ(firstLine(saveContents), std::string("NACA 2412"));
  CHECK(firstLine(psavContents).find("NACA") == std::string::npos);
  CHECK(firstLine(psavContents).find("1.000000") != std::string::npos);
  CHECK(firstLine(usavContents).find("1.000000") == std::string::npos);
  CHECK(firstLine(usavContents).find("1") != std::string::npos);
  CHECK_EQ(usavContents, readBinaryFile(kUsavAfterNacaReference));
}

// the ASKS prompt answer is not trimmed; spaces in the filenames remain.
TEST(unit, CoreUsavHandlerPromptAnswerKeepsSpaces) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreUsavHandlerPromptAnswerKeepsSpaces");
  std::error_code error;
  std::filesystem::create_directories("build/test-data", error);
  CHECK(!error);

  const std::filesystem::path leading =
      std::filesystem::path("build/test-data") / " usav-leading.dat";
  const std::filesystem::path trailing =
      std::filesystem::path("build/test-data") / "usav-trailing.dat ";
  std::filesystem::remove(leading, error);
  std::filesystem::remove(trailing, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream leadingOut;
  const app::cli::UsavCoreStatus leadingStatus = app::cli::handleUsavCore(
      composition, leadingOut, "", false, "build/test-data/ usav-leading.dat",
      true, "", false);
  CHECK_EQ(static_cast<int>(leadingStatus),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK(std::filesystem::exists(leading));

  std::ostringstream trailingOut;
  const app::cli::UsavCoreStatus trailingStatus = app::cli::handleUsavCore(
      composition, trailingOut, "", false, "build/test-data/usav-trailing.dat ",
      true, "", false);
  CHECK_EQ(static_cast<int>(trailingStatus),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK(std::filesystem::exists(trailing));

  std::filesystem::remove(leading, error);
  std::filesystem::remove(trailing, error);
}

// existing file without an answer: prompt, no mutation, file stays.
TEST(unit, CoreUsavHandlerExistingFileRequestsOverwriteNoMutation) {
  TestSavePath file("usav-overwrite-pending.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::overwriteConfirmationRequired));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with N/n: rejection, file stays unchanged.
TEST(unit, CoreUsavHandlerOverwriteDeclineKeepsFile) {
  TestSavePath file("usav-overwrite-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::UsavCoreStatus query = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::UsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "n", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::notSavedDeclined));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt() +
                          std::string(" Current airfoil not saved.\n"));
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with Y: replaced exactly once.
TEST(unit, CoreUsavHandlerOverwriteAcceptReplacesFile) {
  TestSavePath file("usav-overwrite-accept.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::UsavCoreStatus query = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::UsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "Y", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK(readBinaryFile(file.path()).find("original") == std::string::npos);
}

// directory as target: canonical bad-filename path without overwrite prompt.
TEST(unit, CoreUsavHandlerDirectoryTargetBadFilename) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreUsavHandlerDirectoryTargetBadFilename");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "usav-dir-target";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  CHECK(!error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  std::filesystem::remove_all(dir, error);
}

// path not writable in a missing directory: bad filename, no
// session mutation.
TEST(unit, CoreUsavHandlerMissingDirectoryBadFilename) {
  const std::filesystem::path target =
      std::filesystem::path("build/test-data") /
      "usav-missing-dir/usav.dat";
  std::error_code error;
  std::filesystem::remove_all(target.parent_path(), error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, target.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK(!std::filesystem::exists(target, error));
}

// USAV without filenames: ASKS prompt; an empty answer leads to bad filename.
TEST(unit, CoreUsavHandlerFilenamePromptAndBlankAnswer) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream promptOut;
  const app::cli::UsavCoreStatus promptStatus = app::cli::handleUsavCore(
      composition, promptOut, "", false, "", false, "", false);
  CHECK_EQ(static_cast<int>(promptStatus),
           static_cast<int>(app::cli::UsavCoreStatus::filenamePromptRequested));
  CHECK_EQ(promptOut.str(), expectedSaveFilenamePrompt());

  std::ostringstream badOut;
  const app::cli::UsavCoreStatus badStatus = app::cli::handleUsavCore(
      composition, badOut, "", false, "", true, "", false);
  CHECK_EQ(static_cast<int>(badStatus),
           static_cast<int>(app::cli::UsavCoreStatus::badFilename));
  CHECK_EQ(badOut.str(), expectedSaveBadFilename());
}

// USAV changes no session state on success.
TEST(unit, CoreUsavHandlerSuccessLeavesSessionStateUnchanged) {
  TestSavePath file("usav-state-success.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult before =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(before.successful);

  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const auto origin = composition.paneling.origin;
  const std::string name = composition.geometry.name;
  const bool nameExplicit = composition.geometry.nameExplicitlySet;
  const bool systemValid = composition.viscous.systemValid;
  const bool wakeValid = composition.viscous.wakeValid;
  const bool converged = composition.viscous.converged;
  const auto viscals = composition.viscous.viscals;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const double xref = composition.operating.momentReferenceX;
  const int panelCount = session::panelingParameters(composition).panelCount;
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::written));

  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(composition.geometry.nameExplicitlySet, nameExplicit);
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(composition.viscous.viscals.qinv.size(), viscals.qinv.size());
  CHECK_EQ(composition.viscous.viscals.dij.size(), viscals.dij.size());
  CHECK_EQ(composition.viscous.viscals.dijWakeValid, viscals.dijWakeValid);
  CHECK(composition.viscous.viscals.itran == viscals.itran);
  CHECK_NEAR(composition.viscous.viscals.alfa, viscals.alfa, 0.0, 0.0);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  CHECK_EQ(session::panelingParameters(composition).panelCount, panelCount);
  CHECK_NEAR(composition.operating.momentReferenceX, xref, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             before.liftCoefficient, 0.0, 0.0);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK_EQ(static_cast<bool>(metrics), static_cast<bool>(metricsAfter));
  if (metrics && metricsAfter) {
    CHECK_NEAR(metrics->chord, metricsAfter->chord, 0.0, 0.0);
  }
}

// USAV changes no session state on overwrite rejection.
TEST(unit, CoreUsavHandlerDeclineLeavesSessionStateUnchanged) {
  TestSavePath file("usav-state-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;

  std::ostringstream out;
  const app::cli::UsavCoreStatus query = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::UsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, file.path().string(), true, "", false, "N", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::notSavedDeclined));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// USAV changes no session state on bad-filename error.
TEST(unit, CoreUsavHandlerBadFilenameLeavesSessionStateUnchanged) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreUsavHandlerBadFilenameLeavesSessionStateUnchanged");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "usav-state-bad-dir";
  std::error_code error;
  std::filesystem::create_directories(dir, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;

  std::ostringstream out;
  const app::cli::UsavCoreStatus status = app::cli::handleUsavCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::UsavCoreStatus::badFilename));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  std::filesystem::remove_all(dir, error);
}

// independent sessions with different policies.
TEST(unit, CoreUsavHandlerIndependentSessionsDifferentPolicies) {
  TestSavePath fileA("usav-independent-a.dat");
  TestSavePath fileB("usav-independent-b.dat");
  session::SessionComposition a;
  useXfoil699PanelDefaults(a);
  session::SessionComposition b;
  useXfoil699PanelDefaults(b);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);
  session::setDelimiter(a, 1);
  session::setWriteOrientation(a, true);

  std::ostringstream out;
  app::cli::handleUsavCore(a, out, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handleUsavCore(b, out, fileB.path().string(), true, "", false, "",
                           false);

  const std::string contentsA = readBinaryFile(fileA.path());
  const std::string contentsB = readBinaryFile(fileB.path());
  CHECK(contentsA.find(',') == std::string::npos);
  CHECK(contentsA.find('\t') == std::string::npos);
  CHECK_EQ(contentsA, readBinaryFile(kUsavReveReference));
  CHECK_EQ(contentsB, readBinaryFile(kUsavNameReference));
  CHECK_EQ(session::delimiter(b), 0);
  CHECK_EQ(session::writeClockwise(b), false);
}

// deterministic file bytes and text output.
TEST(unit, CoreUsavHandlerDeterministicBytesAndText) {
  TestSavePath fileA("usav-deterministic-a.dat");
  TestSavePath fileB("usav-deterministic-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);

  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handleUsavCore(a, outA, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handleUsavCore(b, outB, fileB.path().string(), true, "", false, "",
                           false);
  CHECK_EQ(readBinaryFile(fileA.path()), readBinaryFile(fileB.path()));
  CHECK_EQ(outA.str(), outB.str());
}

// --- TOP-level ISAV ---

namespace {

constexpr const char* kIsavAfterNacaReference =
    "tests/fortran-reference/cases/file-io/isav-source-pcop/generated/"
    "after_naca.dat";
constexpr const char* kIsavAfterPcopReference =
    "tests/fortran-reference/cases/file-io/isav-source-pcop/generated/"
    "after_pcop.dat";
constexpr const char* kIsavNameReference =
    "tests/fortran-reference/cases/file-io/isav-reve-delimiter-name/generated/"
    "isav_name.dat";
constexpr const char* kIsavReveCommaReference =
    "tests/fortran-reference/cases/file-io/isav-reve-delimiter-name/generated/"
    "isav_reve_comma.dat";
constexpr const char* kIsavTabReference =
    "tests/fortran-reference/cases/file-io/isav-reve-delimiter-name/generated/"
    "isav_tab.dat";
constexpr const char* kIsavNoGeometryReference =
    "tests/fortran-reference/cases/file-io/isav-no-geometry/generated/"
    "isav_nogeom.dat";
constexpr const char* kIsavInteNoCurrentReference =
    "tests/fortran-reference/cases/file-io/isav-inte-buffer-no-current/"
    "generated/isav_after_inte_nocurrent.dat";
constexpr const char* kIsavMsesDomainReference =
    "tests/fortran-reference/cases/file-io/isav-mses-domain-parameters/"
    "generated/isav_after_mses.dat";
constexpr const char* kMsesDomainInputFile =
    "tests/fortran-reference/cases/file-io/isav-mses-domain-parameters/"
    "input/isav_mses_domain.dat";
constexpr const char* kMsesDomainStdout =
    "tests/fortran-reference/cases/file-io/isav-mses-domain-parameters/"
    "stdout.raw.txt";
constexpr const char* kMsesEmptyNameInput =
    "tests/fortran-reference/cases/file-io/mses-load-empty-name/"
    "input/mses_empty_name.dat";
constexpr const char* kMsesEmptyNameStdout =
    "tests/fortran-reference/cases/file-io/mses-load-empty-name/"
    "stdout.raw.txt";
constexpr const char* kMsesInternalSpacesInput =
    "tests/fortran-reference/cases/file-io/mses-load-name-internal-spaces/"
    "input/mses_internal_spaces.dat";
constexpr const char* kMsesInternalSpacesStdout =
    "tests/fortran-reference/cases/file-io/mses-load-name-internal-spaces/"
    "stdout.raw.txt";
constexpr const char* kMsesName48Input =
    "tests/fortran-reference/cases/file-io/mses-load-name-48/"
    "input/mses_name_48.dat";
constexpr const char* kMsesName48Stdout =
    "tests/fortran-reference/cases/file-io/mses-load-name-48/"
    "stdout.raw.txt";
constexpr const char* kMsesNameLongInput =
    "tests/fortran-reference/cases/file-io/mses-load-name-long/"
    "input/mses_name_long.dat";
constexpr const char* kMsesNameLongStdout =
    "tests/fortran-reference/cases/file-io/mses-load-name-long/"
    "stdout.raw.txt";
constexpr const char* kMsesMultiInput =
    "tests/fortran-reference/cases/file-io/mses-load-multi-element/"
    "input/mses_multi.dat";
constexpr const char* kMsesMultiStdout =
    "tests/fortran-reference/cases/file-io/mses-load-multi-element/"
    "stdout.raw.txt";

std::string lineWith(const std::string& text, const std::string& needle) {
  const std::size_t start = text.find(needle);
  if (start == std::string::npos) {
    return std::string();
  }
  const std::size_t end = text.find('\n', start);
  if (end == std::string::npos) {
    return text.substr(start);
  }
  return text.substr(start, end - start + 1);
}

std::string secondLine(const std::string& text) {
  const std::size_t first = text.find('\n');
  if (first == std::string::npos) {
    return std::string();
  }
  const std::size_t second = text.find('\n', first + 1);
  return second == std::string::npos
             ? text.substr(first + 1)
             : text.substr(first + 1, second - first - 1);
}

}  // namespace

// ISAV writes the current panel contour with header and ISPARS line.
TEST(unit, CoreIsavHandlerWritesCurrentPanelWithHeaders) {
  TestSavePath file("isav-panel.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  CHECK_EQ(composition.geometry.buffer.x.size(), 245u);
  CHECK_EQ(composition.paneling.panel.x.size(), 160u);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK(out.str().empty());

  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(firstLine(contents), std::string("NACA 2412"));
  CHECK_EQ(secondLine(contents), std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(countLines(contents), composition.paneling.panel.x.size() + 2);
  CHECK_EQ(contents, readBinaryFile(kIsavAfterNacaReference));
}

// PCOP copies the current contour to the buffer; ISAV follows the current
// contour and then writes 245 coordinate lines.
TEST(unit, CoreIsavHandlerPcopSourceMatchesFortranReference) {
  TestSavePath afterNaca("isav-source-naca.dat");
  TestSavePath afterPcop("isav-source-pcop.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleIsavCore(composition, out, afterNaca.path().string(), true,
                           "", false, "", false);
  CHECK_EQ(readBinaryFile(afterNaca.path()),
           readBinaryFile(kIsavAfterNacaReference));

  const app::cli::PcopCoreStatus pcop = app::cli::handlePcopCore(composition, out);
  CHECK_EQ(static_cast<int>(pcop),
           static_cast<int>(app::cli::PcopCoreStatus::applied));
  CHECK_EQ(composition.paneling.panel.x.size(), 245u);

  app::cli::handleIsavCore(composition, out, afterPcop.path().string(), true,
                           "", false, "", false);
  CHECK_EQ(readBinaryFile(afterPcop.path()),
           readBinaryFile(kIsavAfterPcopReference));
}

// name/NINC produce the canonical header before the ISPARS line.
TEST(unit, CoreIsavHandlerNameNincHeaderMatchesFortranReference) {
  TestSavePath file("isav-name-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(firstLine(contents), std::string("A B C [1]"));
  CHECK_EQ(secondLine(contents), std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(contents, readBinaryFile(kIsavNameReference));
}

// ISAV with maximum name length writes 48 characters in the header.
TEST(unit, CoreIsavHandlerMaxNameLengthHeader) {
  TestSavePath file("isav-max-name.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, std::string(60, 'A'), true, "",
                           false);

  std::ostringstream out;
  app::cli::handleIsavCore(composition, out, file.path().string(), true, "",
                           false, "", false);
  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(firstLine(contents), std::string(48, 'A'));
  CHECK_EQ(secondLine(contents), std::string(" -2.0  3.0  -2.5  3.5"));
}

// REVE and DELI comma produce a byte-identical Fortran ISAV file.
TEST(unit, CoreIsavHandlerReveCommaMatchesFortranReference) {
  TestSavePath file("isav-reve-comma-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 1);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kIsavReveCommaReference));
}

// DELI tab after REVE produces a byte-identical Fortran ISAV file.
TEST(unit, CoreIsavHandlerTabMatchesFortranReference) {
  TestSavePath file("isav-tab-ref.dat");
  session::SessionComposition composition;
  useXfoil699PanelDefaults(composition);
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  app::cli::handleNameCore(composition, sink, "A B C", true, "", false);
  app::cli::handleNincCore(composition, sink, "", true);
  session::setWriteOrientation(composition, true);
  session::setDelimiter(composition, 2);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()), readBinaryFile(kIsavTabReference));
}

// ISAV without geometry writes the INIT namespace and ISPARS line.
TEST(unit, CoreIsavHandlerNoGeometryWritesHeadersOnly) {
  TestSavePath file("isav-nogeom.dat");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK(out.str().empty());
  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(firstLine(contents), std::string(32, ' '));
  CHECK_EQ(secondLine(contents), std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(countLines(contents), 2u);
  CHECK_EQ(contents, readBinaryFile(kIsavNoGeometryReference));
}

// INTE from two disk sources fills only the buffer; ISAV writes an empty
// header line, the ISPARS line and no coordinates.
TEST(unit, CoreIsavHandlerInteBufferWithoutCurrentWritesHeadersOnly) {
  TestSavePath file("isav-inte-nocurrent.dat");
  session::SessionComposition composition;
  std::ostringstream inteOut;
  const app::cli::InteCoreStatus inteStatus = handleInteFF(composition, kInteFile0, kInteFile1, "0.5", "", inteOut);
  CHECK_EQ(static_cast<int>(inteStatus),
           static_cast<int>(app::cli::InteCoreStatus::success));
  CHECK(composition.geometry.valid);
  CHECK(!composition.paneling.panelingValid);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(firstLine(contents), std::string());
  CHECK_EQ(secondLine(contents), std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(contents, readBinaryFile(kIsavInteNoCurrentReference));
}

// ISPARS is written literally and right-truncated; changed session
// values replace the initial default.
TEST(unit, CoreIsavHandlerWritesDomainParametersVerbatim) {
  TestSavePath file("isav-domain.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  session::setIsesDomainParameters(composition,
                                   " -1.0  2.0  -1.5  2.5   ");

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  const std::string contents = readBinaryFile(file.path());
  CHECK_EQ(secondLine(contents), std::string(" -1.0  2.0  -1.5  2.5"));
  CHECK_EQ(secondLine(contents), secondLine(readBinaryFile(kIsavMsesDomainReference)));
  CHECK_NE(contents, readBinaryFile(kIsavAfterNacaReference));
}

// MSES/ISES LOAD adopts airfoil name (LINE1), ISPARS (LINE2) and
// geometry transactionally into the session (aread.f, xfoil.f LOAD).
TEST(unit, CoreLoadMsesAdoptsNameDomainAndBuffer) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesDomainInputFile, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  CHECK_EQ(composition.geometry.name, std::string("MSESFOIL"));
  CHECK_EQ(session::isesDomainParameters(composition),
           std::string(" -1.0  2.0  -1.5  2.5"));
  CHECK_EQ(composition.geometry.buffer.x.size(), 5u);
  CHECK_EQ(composition.paneling.panel.x.size(), 5u);
  CHECK_EQ(composition.geometry.bufferRevision, 1u);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(session::PanelingOrigin::abcopy));
  CHECK_EQ(composition.paneling.panelSourceRevision,
           composition.geometry.bufferRevision);
}

// Central proof: canonical MSES file -> handleLoadCore -> session ->
// handleIsavCore -> output file, byte-identical against the ISAV reference
// file produced by the canonical binary (header, ISPARS, coordinates, bytes).
TEST(unit, CoreLoadMsesThenIsavMatchesFortranFile) {
  TestSavePath file("isav-roundtrip-mses.dat");
  session::SessionComposition composition;
  std::ostringstream loadOut;
  const app::cli::LoadCoreStatus load =
      app::cli::handleLoadCore(composition, kMsesDomainInputFile, "", loadOut);
  CHECK_EQ(static_cast<int>(load),
           static_cast<int>(app::cli::LoadCoreStatus::success));

  std::ostringstream isavOut;
  const app::cli::IsavCoreStatus isav = app::cli::handleIsavCore(
      composition, isavOut, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(isav),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK_EQ(readBinaryFile(file.path()),
           readBinaryFile(kIsavMsesDomainReference));
}

// two independent LOAD->ISAV round trips deliver byte-identical files
// (determinism of file type detection, ISPARS adoption and writer).
TEST(unit, CoreLoadMsesRoundtripDeterministicBytes) {
  TestSavePath first("isav-roundtrip-mses-1.dat");
  TestSavePath second("isav-roundtrip-mses-2.dat");
  std::string firstBytes;
  std::string secondBytes;
  for (int run = 0; run < 2; ++run) {
    session::SessionComposition composition;
    std::ostringstream loadOut;
    app::cli::handleLoadCore(composition, kMsesDomainInputFile, "", loadOut);
    std::ostringstream isavOut;
    const std::string target =
        (run == 0) ? first.path().string() : second.path().string();
    app::cli::handleIsavCore(composition, isavOut, target, true, "", false, "",
                             false);
    if (run == 0) {
      firstBytes = readBinaryFile(first.path());
    } else {
      secondBytes = readBinaryFile(second.path());
    }
  }
  CHECK_EQ(firstBytes, secondBytes);
  CHECK_EQ(firstBytes, readBinaryFile(kIsavMsesDomainReference));
}

// A plain (simple) file after an MSES LOAD leaves the already set
// ISPARS unchanged (xfoil.f:1288 sets a default only for ISPARS==' ';
// the session value is never empty).
TEST(unit, CoreLoadNormalAfterMsesKeepsDomainParameters) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleLoadCore(
               composition, kMsesDomainInputFile, "", out)),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string msesDomain = session::isesDomainParameters(composition);
  CHECK_EQ(msesDomain, std::string(" -1.0  2.0  -1.5  2.5"));

  CHECK_EQ(static_cast<int>(app::cli::handleLoadCore(
               composition, kLoadReferenceFile, "plainname", out)),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  CHECK_EQ(session::isesDomainParameters(composition), msesDomain);
}

// second line with only three numbers: labeled file (ITYPE 2), no
// ISPARS adoption; domain parameters remain at the initial default.
TEST(unit, CoreLoadThreeNumberSecondLineIsLabeledNotMses) {
  TestSavePath file("mses-three-num-line.dat");
  writeTextFile(file.path(),
                "THREENAME\n"
                " 1.0 0.0 0.5\n"
                " 0.0 0.1\n"
                "-1.0 0.0\n"
                " 0.0 -0.1\n"
                " 1.0 0.0\n");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, file.path(), "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  CHECK_EQ(session::isesDomainParameters(composition),
           std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(composition.geometry.name, std::string("THREENAME"));
}

// name line without a numerical second line: AREAD abort (ITYPE 0), no
// session mutation (name, ISPARS, buffer, revision unchanged).
TEST(unit, CoreLoadInvalidSecondLineNoMutation) {
  TestSavePath file("mses-invalid-domain.dat");
  writeTextFile(file.path(),
                "BADNAME\n"
                "not numeric\n"
                " 1.0 0.0\n"
                " 0.0 0.1\n"
                "-1.0 0.0\n");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, file.path(), "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::readError));
  CHECK(composition.geometry.name.empty());
  CHECK_EQ(session::isesDomainParameters(composition),
           std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK(composition.geometry.buffer.x.empty());
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
}

// two independent sessions with different MSES files carry
// different domain parameters and names.
TEST(unit, CoreLoadMsesIndependentSessions) {
  TestSavePath fileA("mses-indep-a.dat");
  writeTextFile(fileA.path(),
                "AFOIL\n"
                " -3.0  4.0  -3.5  4.5\n"
                " 1.0 0.0\n"
                " 0.0 0.1\n"
                "-1.0 0.0\n"
                " 0.0 -0.1\n"
                " 1.0 0.0\n");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream out;
  CHECK_EQ(static_cast<int>(app::cli::handleLoadCore(a, fileA.path(), "", out)),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  CHECK_EQ(static_cast<int>(app::cli::handleLoadCore(
               b, kMsesDomainInputFile, "", out)),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  CHECK_EQ(session::isesDomainParameters(a),
           std::string(" -3.0  4.0  -3.5  4.5"));
  CHECK_EQ(session::isesDomainParameters(b),
           std::string(" -1.0  2.0  -1.5  2.5"));
  CHECK_EQ(a.geometry.name, std::string("AFOIL"));
  CHECK_EQ(b.geometry.name, std::string("MSESFOIL"));
}

// the MSES header line of the handler is byte-identical to the
// canonical Fortran raw output (including leading space, two
// space after 'name:', CHARACTER*48 padding and newline).
TEST(unit, CoreLoadMsesKennzeileMatchesFortranRawLine) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesDomainInputFile, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected =
      lineWith(readBinaryFile(kMsesDomainStdout), " MSES airfoil file");
  CHECK(!expected.empty());
  CHECK_EQ(lineWith(out.str(), " MSES airfoil file"), expected);
}

// empty MSES name: the header line carries 48 blanks; the session name
// stays empty after STRIP.
TEST(unit, CoreLoadMsesEmptyNameKennzeileMatchesFortranRawLine) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesEmptyNameInput, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected =
      lineWith(readBinaryFile(kMsesEmptyNameStdout), " MSES airfoil file");
  CHECK(!expected.empty());
  CHECK_EQ(lineWith(out.str(), " MSES airfoil file"), expected);
  CHECK(composition.geometry.name.empty());
}

// internal spaces in the MSES name are preserved in the CHARACTER field;
// STRIP removes only leading and trailing spaces for the session name.
TEST(unit, CoreLoadMsesInternalSpacesKennzeileMatchesFortranRawLine) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status = app::cli::handleLoadCore(
      composition, kMsesInternalSpacesInput, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected = lineWith(readBinaryFile(kMsesInternalSpacesStdout),
                                        " MSES airfoil file");
  CHECK(!expected.empty());
  CHECK_EQ(lineWith(out.str(), " MSES airfoil file"), expected);
  CHECK_EQ(composition.geometry.name, std::string("A B  C"));
}

// exactly 48 characters: the CHARACTER*48 field fills the header line up to the
// newline, without additional blank fill.
TEST(unit, CoreLoadMsesName48KennzeileMatchesFortranRawLine) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesName48Input, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected =
      lineWith(readBinaryFile(kMsesName48Stdout), " MSES airfoil file");
  CHECK(!expected.empty());
  CHECK_EQ(lineWith(out.str(), " MSES airfoil file"), expected);
}

// Longer than 48 characters: the visible AREAD header line contains only
// the first 48 characters of the CHARACTER*48 field.
TEST(unit, CoreLoadMsesNameLongKennzeileMatchesFortranRawLine) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesNameLongInput, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected =
      lineWith(readBinaryFile(kMsesNameLongStdout), " MSES airfoil file");
  CHECK(!expected.empty());
  CHECK_EQ(lineWith(out.str(), " MSES airfoil file"), expected);
}

// MSES multi-element: the header line appears before the element selection, even
// though the handler does not issue the ASKI prompt itself.
TEST(unit, CoreLoadMsesMultiKennzeileMatchesFortranRawLine) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesMultiInput, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::elementNotFound));
  const std::string expected =
      lineWith(readBinaryFile(kMsesMultiStdout), " MSES airfoil file");
  CHECK(!expected.empty());
  CHECK_EQ(lineWith(out.str(), " MSES airfoil file"), expected);
  CHECK(!composition.geometry.valid);
  CHECK_EQ(composition.geometry.bufferRevision, 0u);
  CHECK_EQ(session::isesDomainParameters(composition),
           std::string(" -2.0  3.0  -2.5  3.5"));
}

// visible AREAD field and session name remain domain-specific separate: the
// header line carries the 48-character field, the session name follows the
// existing STRIP/32-character contract.
TEST(unit, CoreLoadMsesTextSeparatesVisibleFieldAndSessionName) {
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, kMsesNameLongInput, "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string line = lineWith(out.str(), " MSES airfoil file");
  CHECK_EQ(line.size(), 77u);
  CHECK_EQ(line, lineWith(readBinaryFile(kMsesNameLongStdout),
                          " MSES airfoil file"));
  CHECK_EQ(composition.geometry.name.size(), 32u);
  CHECK_EQ(composition.geometry.name,
           std::string("LONGNAME") + std::string(24, 'X'));
}

// Pure text formatting changes no session values: three identical header-line runs
// deliver the same byte sequence and the same session state.
TEST(unit, CoreLoadMsesTextDeterministicThreeRuns) {
  std::string first;
  std::string second;
  std::string third;
  for (int run = 0; run < 3; ++run) {
    session::SessionComposition composition;
    std::ostringstream out;
    app::cli::handleLoadCore(composition, kMsesDomainInputFile, "", out);
    const std::string line = lineWith(out.str(), " MSES airfoil file");
    CHECK_EQ(composition.geometry.name, std::string("MSESFOIL"));
    CHECK_EQ(session::isesDomainParameters(composition),
             std::string(" -1.0  2.0  -1.5  2.5"));
    if (run == 0) {
      first = line;
    } else if (run == 1) {
      second = line;
    } else {
      third = line;
    }
  }
  CHECK_EQ(first, second);
  CHECK_EQ(second, third);
  CHECK_EQ(first, lineWith(readBinaryFile(kMsesDomainStdout),
                           " MSES airfoil file"));
}

// the labeled text of the handlers stays deliberately
// at the existing product/golden contract and is not aligned
// to the MSES text.
TEST(unit, CoreLoadLabeledTextRemainsUnchangedProductContract) {
  TestSavePath file("labeled-text-contract.dat");
  writeTextFile(file.path(),
                "TEST PROFILE\n"
                " 1.0 0.0 0.5\n"
                " 0.0 0.1\n"
                "-1.0 0.0\n"
                " 0.0 -0.1\n"
                " 1.0 0.0\n");
  session::SessionComposition composition;
  std::ostringstream out;
  const app::cli::LoadCoreStatus status =
      app::cli::handleLoadCore(composition, file.path(), "", out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::LoadCoreStatus::success));
  const std::string expected =
      " Labeled airfoil file.  Name:  TEST PROFILE" + std::string(36, ' ') +
      "\n";
  CHECK_EQ(lineWith(out.str(), " Labeled airfoil file"), expected);
}

// SAVE, PSAV, USAV and ISAV remain typed as domain-specific separate contracts.
TEST(unit, CoreIsavHandlerOtherWriteContractsRemainSeparate) {
  TestSavePath saveFile("isav-contract-save.dat");
  TestSavePath psavFile("isav-contract-psav.dat");
  TestSavePath usavFile("isav-contract-usav.dat");
  TestSavePath isavFile("isav-contract-isav.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  app::cli::handleSaveCore(composition, out, saveFile.path().string(), true, "",
                           false, "", false);
  app::cli::handlePsavCore(composition, out, psavFile.path().string(), true, "",
                           false, "", false);
  app::cli::handleUsavCore(composition, out, usavFile.path().string(), true, "",
                           false, "", false);
  app::cli::handleIsavCore(composition, out, isavFile.path().string(), true, "",
                           false, "", false);

  const std::string saveContents = readBinaryFile(saveFile.path());
  const std::string psavContents = readBinaryFile(psavFile.path());
  const std::string usavContents = readBinaryFile(usavFile.path());
  const std::string isavContents = readBinaryFile(isavFile.path());
  CHECK_EQ(firstLine(saveContents), std::string("NACA 2412"));
  CHECK(firstLine(psavContents).find("NACA") == std::string::npos);
  CHECK(firstLine(usavContents).find("1.000000") == std::string::npos);
  CHECK_EQ(firstLine(isavContents), std::string("NACA 2412"));
  CHECK_EQ(secondLine(isavContents), std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK_EQ(isavContents,
           std::string("NACA 2412\n -2.0  3.0  -2.5  3.5\n") + psavContents);
}

// the ASKS prompt answer is not trimmed; spaces in the filenames remain.
TEST(unit, CoreIsavHandlerPromptAnswerKeepsSpaces) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreIsavHandlerPromptAnswerKeepsSpaces");
  std::error_code error;
  std::filesystem::create_directories("build/test-data", error);
  CHECK(!error);

  const std::filesystem::path leading =
      std::filesystem::path("build/test-data") / " isav-leading.dat";
  const std::filesystem::path trailing =
      std::filesystem::path("build/test-data") / "isav-trailing.dat ";
  std::filesystem::remove(leading, error);
  std::filesystem::remove(trailing, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream leadingOut;
  const app::cli::IsavCoreStatus leadingStatus = app::cli::handleIsavCore(
      composition, leadingOut, "", false, "build/test-data/ isav-leading.dat",
      true, "", false);
  CHECK_EQ(static_cast<int>(leadingStatus),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK(std::filesystem::exists(leading));

  std::ostringstream trailingOut;
  const app::cli::IsavCoreStatus trailingStatus = app::cli::handleIsavCore(
      composition, trailingOut, "", false, "build/test-data/isav-trailing.dat ",
      true, "", false);
  CHECK_EQ(static_cast<int>(trailingStatus),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK(std::filesystem::exists(trailing));

  std::filesystem::remove(leading, error);
  std::filesystem::remove(trailing, error);
}

// existing file without an answer: prompt, no mutation, file stays.
TEST(unit, CoreIsavHandlerExistingFileRequestsOverwriteNoMutation) {
  TestSavePath file("isav-overwrite-pending.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::overwriteConfirmationRequired));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with N/n: rejection, file stays unchanged.
TEST(unit, CoreIsavHandlerOverwriteDeclineKeepsFile) {
  TestSavePath file("isav-overwrite-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::IsavCoreStatus query = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::IsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "n", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::notSavedDeclined));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt() +
                          std::string(" Current airfoil not saved.\n"));
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// existing file with Y: replaced exactly once.
TEST(unit, CoreIsavHandlerOverwriteAcceptReplacesFile) {
  TestSavePath file("isav-overwrite-accept.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::IsavCoreStatus query = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::IsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "Y", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));
  CHECK_EQ(out.str(), expectedSaveOverwritePrompt());
  CHECK(readBinaryFile(file.path()).find("original") == std::string::npos);
}

// directory as target: canonical bad-filename path without overwrite prompt.
TEST(unit, CoreIsavHandlerDirectoryTargetBadFilename) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreIsavHandlerDirectoryTargetBadFilename");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "isav-dir-target";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  CHECK(!error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  std::filesystem::remove_all(dir, error);
}

// path not writable in a missing directory: bad filename, no
// session mutation.
TEST(unit, CoreIsavHandlerMissingDirectoryBadFilename) {
  const std::filesystem::path target =
      std::filesystem::path("build/test-data") /
      "isav-missing-dir/isav.dat";
  std::error_code error;
  std::filesystem::remove_all(target.parent_path(), error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string domain = session::isesDomainParameters(composition);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, target.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::badFilename));
  CHECK_EQ(out.str(), expectedSaveBadFilename());
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(session::isesDomainParameters(composition), domain);
  CHECK(!std::filesystem::exists(target, error));
}

// ISAV without filenames: ASKS prompt; an empty answer leads to bad filename.
TEST(unit, CoreIsavHandlerFilenamePromptAndBlankAnswer) {
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);

  std::ostringstream promptOut;
  const app::cli::IsavCoreStatus promptStatus = app::cli::handleIsavCore(
      composition, promptOut, "", false, "", false, "", false);
  CHECK_EQ(static_cast<int>(promptStatus),
           static_cast<int>(app::cli::IsavCoreStatus::filenamePromptRequested));
  CHECK_EQ(promptOut.str(), expectedSaveFilenamePrompt());

  std::ostringstream badOut;
  const app::cli::IsavCoreStatus badStatus = app::cli::handleIsavCore(
      composition, badOut, "", false, "", true, "", false);
  CHECK_EQ(static_cast<int>(badStatus),
           static_cast<int>(app::cli::IsavCoreStatus::badFilename));
  CHECK_EQ(badOut.str(), expectedSaveBadFilename());
}

// ISAV changes no session state on success.
TEST(unit, CoreIsavHandlerSuccessLeavesSessionStateUnchanged) {
  TestSavePath file("isav-state-success.dat");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  session::setIsesDomainParameters(composition,
                                   " -1.0  2.0  -1.5  2.5");
  CHECK(session::configure(composition, xfoil::AnalysisConfig{}).success());
  const xfoil::AnalysisResult before =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(before.successful);

  const std::vector<double> bufferX = composition.geometry.buffer.x;
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::uint64_t panelRev = composition.paneling.panelSourceRevision;
  const auto origin = composition.paneling.origin;
  const std::string name = composition.geometry.name;
  const bool nameExplicit = composition.geometry.nameExplicitlySet;
  const std::string domain = session::isesDomainParameters(composition);
  const bool systemValid = composition.viscous.systemValid;
  const bool wakeValid = composition.viscous.wakeValid;
  const bool converged = composition.viscous.converged;
  const auto viscals = composition.viscous.viscals;
  const bool norm = session::normalizeOnLoad(composition);
  const bool cw = session::writeClockwise(composition);
  const int delim = session::delimiter(composition);
  const double xref = composition.operating.momentReferenceX;
  const int panelCount = session::panelingParameters(composition).panelCount;
  const auto metrics = session::geometryMetrics(composition);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::written));

  CHECK(composition.geometry.buffer.x == bufferX);
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(static_cast<int>(composition.paneling.origin),
           static_cast<int>(origin));
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(composition.geometry.nameExplicitlySet, nameExplicit);
  CHECK_EQ(session::isesDomainParameters(composition), domain);
  CHECK_EQ(composition.viscous.systemValid, systemValid);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_EQ(composition.viscous.converged, converged);
  CHECK_EQ(composition.viscous.viscals.qinv.size(), viscals.qinv.size());
  CHECK_EQ(composition.viscous.viscals.dij.size(), viscals.dij.size());
  CHECK_EQ(composition.viscous.viscals.dijWakeValid, viscals.dijWakeValid);
  CHECK(composition.viscous.viscals.itran == viscals.itran);
  CHECK_NEAR(composition.viscous.viscals.alfa, viscals.alfa, 0.0, 0.0);
  CHECK_EQ(session::normalizeOnLoad(composition), norm);
  CHECK_EQ(session::writeClockwise(composition), cw);
  CHECK_EQ(session::delimiter(composition), delim);
  CHECK_EQ(session::panelingParameters(composition).panelCount, panelCount);
  CHECK_NEAR(composition.operating.momentReferenceX, xref, 0.0, 0.0);
  CHECK_EQ(static_cast<int>(composition.result.lastResult.status),
           static_cast<int>(before.status));
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             before.liftCoefficient, 0.0, 0.0);
  const auto metricsAfter = session::geometryMetrics(composition);
  CHECK_EQ(static_cast<bool>(metrics), static_cast<bool>(metricsAfter));
  if (metrics && metricsAfter) {
    CHECK_NEAR(metrics->chord, metricsAfter->chord, 0.0, 0.0);
  }
}

// ISAV changes no session state on overwrite rejection.
TEST(unit, CoreIsavHandlerDeclineLeavesSessionStateUnchanged) {
  TestSavePath file("isav-state-decline.dat");
  writeTextFile(file.path(), "original\n");
  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;
  const std::string domain = session::isesDomainParameters(composition);

  std::ostringstream out;
  const app::cli::IsavCoreStatus query = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(query),
           static_cast<int>(app::cli::IsavCoreStatus::overwriteConfirmationRequired));
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, file.path().string(), true, "", false, "N", true);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::notSavedDeclined));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(session::isesDomainParameters(composition), domain);
  CHECK_EQ(readBinaryFile(file.path()), std::string("original\n"));
}

// ISAV changes no session state on bad-filename error.
TEST(unit, CoreIsavHandlerBadFilenameLeavesSessionStateUnchanged) {
 // own, unique test working directory: the
  // checked relative build/test-data paths remain unchanged.
  const ScopedBuildTestDataWorkspace workspace("CoreIsavHandlerBadFilenameLeavesSessionStateUnchanged");
  const std::filesystem::path dir =
      std::filesystem::path("build/test-data") / "isav-state-bad-dir";
  std::error_code error;
  std::filesystem::create_directories(dir, error);

  session::SessionComposition composition;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(composition, sink, 2412);
  const std::vector<double> panelX = composition.paneling.panel.x;
  const std::uint64_t bufferRev = composition.geometry.bufferRevision;
  const std::string name = composition.geometry.name;
  const std::string domain = session::isesDomainParameters(composition);

  std::ostringstream out;
  const app::cli::IsavCoreStatus status = app::cli::handleIsavCore(
      composition, out, dir.string(), true, "", false, "", false);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(app::cli::IsavCoreStatus::badFilename));
  CHECK(composition.paneling.panel.x == panelX);
  CHECK_EQ(composition.geometry.bufferRevision, bufferRev);
  CHECK_EQ(composition.geometry.name, name);
  CHECK_EQ(session::isesDomainParameters(composition), domain);
  std::filesystem::remove_all(dir, error);
}

// independent sessions with different domain parameters and policies.
TEST(unit, CoreIsavHandlerIndependentSessionsDifferentDomainParameters) {
  TestSavePath fileA("isav-independent-a.dat");
  TestSavePath fileB("isav-independent-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);
  session::setIsesDomainParameters(a, " -1.0  2.0  -1.5  2.5");
  session::setDelimiter(a, 1);
  session::setWriteOrientation(a, true);

  std::ostringstream out;
  app::cli::handleIsavCore(a, out, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handleIsavCore(b, out, fileB.path().string(), true, "", false, "",
                           false);

  const std::string contentsA = readBinaryFile(fileA.path());
  const std::string contentsB = readBinaryFile(fileB.path());
  CHECK_EQ(secondLine(contentsA), std::string(" -1.0  2.0  -1.5  2.5"));
  CHECK_EQ(secondLine(contentsB), std::string(" -2.0  3.0  -2.5  3.5"));
  CHECK(contentsA.find(',') != std::string::npos);
  CHECK(contentsB.find(',') == std::string::npos);
  CHECK_EQ(session::delimiter(b), 0);
  CHECK_EQ(session::writeClockwise(b), false);
  CHECK_EQ(session::isesDomainParameters(b), std::string(" -2.0  3.0  -2.5  3.5"));
}

// deterministic file bytes and text output.
TEST(unit, CoreIsavHandlerDeterministicBytesAndText) {
  TestSavePath fileA("isav-deterministic-a.dat");
  TestSavePath fileB("isav-deterministic-b.dat");
  session::SessionComposition a;
  session::SessionComposition b;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(a, sink, 2412);
  testing::handleLegacyNacaCore(b, sink, 2412);

  std::ostringstream outA;
  std::ostringstream outB;
  app::cli::handleIsavCore(a, outA, fileA.path().string(), true, "", false, "",
                           false);
  app::cli::handleIsavCore(b, outB, fileB.path().string(), true, "", false, "",
                           false);
  CHECK_EQ(readBinaryFile(fileA.path()), readBinaryFile(fileB.path()));
  CHECK_EQ(outA.str(), outB.str());
}

// --- PLOP menu (OPLSET, plutil.f 21-176) ----------------------

namespace {

// canonical menu block from design-state/plop-menu-return (format 2000,
// plutil.f 61-73): the point record depends on the open prompt line, '//'
// produces the empty record, and blowup/orientation carry the 10-character
// Fortran strings.
const std::string kPlotMenuTable =
    " ...............................................\n"
    "\n"
    "  G raphics-enable flag:        T\n"
    "  S ize of plot object          10.00\"\n"
    "  A spect ratio of plot object   0.5500\n"
    "  P age dimensions              11.00 x  8.50\"\n"
    "  M argins from page edges       0.00\",  0.00\"\n"
    "  F ont size (relative)          0.0150\n"
    "  W indow/screen size fraction   0.8000\n"
    "  B lowup input method:        Cursor    \n"
    "  O rientation of plot:        Landscape \n"
    "  C olor PostScript output?     T\n"
    "  L ine colors for top,bottom    8  3\n";

const std::string kPlotMenuPrompt =
    "\n      Option, Value   (or <Return>)    c>  ";

}  // namespace

TEST(unit, CorePlotOptionsMenuReturnMatchesFortranBytes) {
  session::SessionComposition session;
  std::istringstream in("0\n");
  std::ostringstream out;
  const auto status = app::cli::handlePlotOptionsCore(session, in, out);
  CHECK(status == app::cli::PlotOptionsCoreStatus::returned);
  CHECK_EQ(out.str(), kPlotMenuTable + kPlotMenuPrompt);
  // No state change, no geometry/panel mutation.
  CHECK_EQ(session.design.plot.idev, 1);
  CHECK_NEAR(session.defaults.values.size, 10.0, 0.0, 0.0);
  CHECK_EQ(session.geometry.bufferRevision, 0u);
}

TEST(unit, CorePlotOptionsTogglesUpdateStateAndReprintedTable) {
  session::SessionComposition session;
  std::istringstream in("g\ns 12\nb\no\nc\nl 3 4\n0\n");
  std::ostringstream out;
  const auto status = app::cli::handlePlotOptionsCore(session, in, out);
  CHECK(status == app::cli::PlotOptionsCoreStatus::returned);
  CHECK_EQ(session.design.plot.idev, 0);
  CHECK_NEAR(session.defaults.values.size, 12.0, 0.0, 0.0);
  CHECK_FALSE(session.defaults.values.lcurs);
  CHECK_FALSE(session.design.plot.lland);
  CHECK_NEAR(session.defaults.values.xpage, 8.5, 0.0, 0.0);
  CHECK_NEAR(session.defaults.values.ypage, 11.0, 0.0, 0.0);
  CHECK_EQ(session.defaults.idevrp, 2);
  CHECK_EQ(session.design.plot.icols[0], 3);
  CHECK_EQ(session.design.plot.icols[1], 4);
  const std::string text = out.str();
  CHECK(text.find("  G raphics-enable flag:        F\n") != std::string::npos);
  CHECK(text.find("  S ize of plot object          12.00\"\n") !=
        std::string::npos);
  CHECK(text.find("  B lowup input method:        Keyboard  \n") !=
        std::string::npos);
  CHECK(text.find("\n\n Swapping X,Y page dimensions\n") != std::string::npos);
  CHECK(text.find("  O rientation of plot:        Portrait  \n") !=
        std::string::npos);
  CHECK(text.find("  C olor PostScript output?     F\n") != std::string::npos);
  CHECK(text.find("  P age dimensions               8.50 x 11.00\"\n") !=
        std::string::npos);
  CHECK(text.find("  L ine colors for top,bottom    3  4\n") !=
        std::string::npos);
  // Seven table printouts (start + 6 options) plus exit prompt.
  std::size_t tables = 0;
  for (std::size_t pos = text.find(" ...."); pos != std::string::npos;
       pos = text.find(" ....", pos + 1)) {
    ++tables;
  }
  CHECK_EQ(tables, 7u);
}

TEST(unit, CorePlotOptionsUnknownOptionLeavesStateUnchanged) {
  session::SessionComposition session;
  std::istringstream in("x\n0\n");
  std::ostringstream out;
  const auto status = app::cli::handlePlotOptionsCore(session, in, out);
  CHECK(status == app::cli::PlotOptionsCoreStatus::returned);
  CHECK(out.str().find(" *** Item not recognized ***\n") != std::string::npos);
  CHECK_EQ(session.design.plot.idev, 1);
  CHECK_NEAR(session.defaults.values.size, 10.0, 0.0, 0.0);
}

TEST(unit, CorePlotOptionsInteractivePromptKeepsDefaultOnEmpty) {
  session::SessionComposition session;
  std::istringstream in("s\n\n0\n");
  std::ostringstream out;
  const auto status = app::cli::handlePlotOptionsCore(session, in, out);
  CHECK(status == app::cli::PlotOptionsCoreStatus::returned);
  CHECK_NEAR(session.defaults.values.size, 10.0, 0.0, 0.0);
  CHECK(out.str().find("\nEnter size (in)   r>  ") != std::string::npos);
}

TEST(unit, CorePlotOptionsColorMenuMatchesFortranBytes) {
  session::SessionComposition session;
  std::istringstream in("l\n3 4\n0\n");
  std::ostringstream out;
  const auto status = app::cli::handlePlotOptionsCore(session, in, out);
  CHECK(status == app::cli::PlotOptionsCoreStatus::returned);
  CHECK_EQ(session.design.plot.icols[0], 3);
  CHECK_EQ(session.design.plot.icols[1], 4);
  const std::string expected =
      "\n  1  black  "
      "\n  2  white  "
      "\n  3  red    "
      "\n  4  orange "
      "\n  5  yellow "
      "\n  6  green  "
      "\n  7  cyan   "
      "\n  8  blue   "
      "\n  9  violet "
      "\n 10  magenta"
      "\n Select top,bottom line-plot colors\n";
  CHECK(out.str().find(expected) != std::string::npos);
}

TEST(unit, CorePlotOptionsEndOfFileStopsWithoutCrash) {
  session::SessionComposition session;
  std::istringstream in("g\n");
  std::ostringstream out;
  const auto status = app::cli::handlePlotOptionsCore(session, in, out);
  CHECK(status == app::cli::PlotOptionsCoreStatus::endOfFile);
  CHECK_EQ(session.design.plot.idev, 0);
}

TEST(unit, CorePlotOptionsDeterministicBytesAcrossRuns) {
  session::SessionComposition a;
  session::SessionComposition b;
  std::istringstream inA("g\ns 12\no\nl\n3 4\nx\n0\n");
  std::istringstream inB("g\ns 12\no\nl\n3 4\nx\n0\n");
  std::ostringstream outA;
  std::ostringstream outB;
  const auto statusA = app::cli::handlePlotOptionsCore(a, inA, outA);
  const auto statusB = app::cli::handlePlotOptionsCore(b, inB, outB);
  CHECK(statusA == app::cli::PlotOptionsCoreStatus::returned);
  CHECK(statusB == app::cli::PlotOptionsCoreStatus::returned);
  CHECK_EQ(outA.str(), outB.str());
}

TEST(unit, CorePlotOptionsLeaveGeometryAndPanelUntouched) {
  session::SessionComposition session;
  std::ostringstream sink;
  testing::handleLegacyNacaCore(session, sink, 4412);
  const std::uint64_t revisionBefore = session.geometry.bufferRevision;
  std::istringstream in("g\no\n0\n");
  std::ostringstream out;
  (void)app::cli::handlePlotOptionsCore(session, in, out);
  CHECK_EQ(session.geometry.bufferRevision, revisionBefore);
  CHECK_EQ(session.design.plot.idev, 0);
  CHECK_FALSE(session.design.plot.lland);
}

// End: tests/unit/CoreGeometryHandlerTests.cpp

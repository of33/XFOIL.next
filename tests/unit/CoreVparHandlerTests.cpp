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

// File: tests/unit/CoreVparHandlerTests.cpp
// permanent tests of the VPAR state
// handlers display/XTR/N/VACC/INIT. The display is
// byte-exact against the raw stdout references of the canonical Fortran processes
// (oper-state/vpar-*). State updates, invalidation,
// transactionality on abort, instance scoping and absence of analysis-
// and panel mutations are checked directly. The production CLI route stays
// untouched.

#include "TestSupport.h"

#include "app/cli/CoreVparHandlers.h"

#include "app/DatFileReader.h"
#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/geometry/AirfoilGeometry.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using namespace xfoil::app::cli;

std::string readBinaryFile(const std::filesystem::path& path) {
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

// Extracts the contiguous FORMAT-1200 display block (from the first
// ' Xtr/c =' line through the ' CtiniK=' line) from a
// Fortran stdout reference.
std::string extractDisplayBlock(const std::string& text) {
  // anchor on the top-page line of the last display (both Xtr lines
  // start with ' Xtr/c =', hence via the unique ' top side').
  const std::size_t topKey = text.rfind("    top    side");
  if (topKey == std::string::npos) {
    return std::string();
  }
  const std::size_t lineBreak = text.rfind('\n', topKey);
  const std::size_t start = (lineBreak == std::string::npos) ? 0 : lineBreak + 1;
  const std::size_t endKey = text.find(" CtiniK=", topKey);
  if (endKey == std::string::npos) {
    return std::string();
  }
  const std::size_t end = text.find('\n', endKey);
  if (end == std::string::npos) {
    return text.substr(start);
  }
  return text.substr(start, end - start + 1);
}

xfoil::geometry::AirfoilGeometry makeClarkYGeometry() {
  const auto read = xfoil::app::DatFileReader::read("sample/CLARK_Y.dat");
  CHECK(read.success());
  xfoil::geometry::AirfoilGeometry geometry;
  const auto assigned = geometry.assign(
      read.airfoil.name,
      std::span<const xfoil::Point2D>(read.airfoil.coordinates));
  CHECK(assigned.success());
  return geometry;
}

// Fresh session with canonical INIT operating defaults (XSTRIP 1/1,
  // ACRIT 9, VACCEL 0.01, WAKLEN 1, LBLINI false), without analysis.
session::SessionComposition makeVparDefaultSession() {
  session::SessionComposition session;
  CHECK(session::setGeometry(session, makeClarkYGeometry()).success());
  CHECK(session::copyBufferToCurrent(session).ok);
  xfoil::AnalysisConfig config;
  config.viscous = false;
  config.reynoldsNumber = 0.0;
  config.maximumIterations = 30;
  CHECK(session::configure(session, config).success());
  CHECK(session::initializeAnalysis(session));
  return session;
}

// converged viscous single point (Clark Y, Re=1e6, alpha 2, Mach 0):
// contains lblini=true, mapping.valid=true, lvconv/converged=true and a
// set lastResult. Serves as the before state for the invalidation matrix.
session::SessionComposition makeConvergedViscousSession() {
  session::SessionComposition session;
  CHECK(session::setGeometry(session, makeClarkYGeometry()).success());
  xfoil::AnalysisConfig config;
  config.viscous = true;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 30;
  CHECK(session::configure(session, config).success());
  CHECK(session::initializeAnalysis(session));
  const xfoil::AnalysisResult point =
      session::runSpecAlpha(session, xfoil::Degrees{2.0}, nullptr);
  CHECK(point.successful);
  return session;
}

struct StateSnapshot {
  bool lblini{};
  bool mapping{};
  bool lvconv{};
  bool converged{};
  bool wakeValid{};
  bool systemValid{};
  int maximumIterations{};
  AnalysisStatus status{};
  std::uint64_t bufferRevision{};
  std::uint64_t panelSourceRevision{};
  session::PanelingOrigin origin{};
};

StateSnapshot snapshot(const session::SessionComposition& s) {
  return StateSnapshot{
      s.viscous.viscals.lblini,
      s.viscous.viscals.mapping.valid,
      s.viscous.viscals.lvconv,
      s.viscous.converged,
      s.viscous.wakeValid,
      s.viscous.systemValid,
      s.operating.maximumIterations,
      s.result.lastResult.status,
      s.geometry.bufferRevision,
      s.paneling.panelSourceRevision,
      s.paneling.origin};
}

constexpr const char* kDefaultDisplayRef =
    "tests/fortran-reference/cases/oper-state/vpar-default-display/"
    "stdout.raw.txt";
constexpr const char* kSequenceRef =
    "tests/fortran-reference/cases/oper-state/vpar-sequence/stdout.raw.txt";
constexpr const char* kXtrPromptRef =
    "tests/fortran-reference/cases/oper-state/vpar-prompts/stdout.raw.txt";

}  // namespace

TEST(unit, CoreVparShowDefaultDisplayMatchesFortranBytes) {
  auto session = makeVparDefaultSession();
  std::ostringstream out;
  const auto status = handleVparShowCore(session, out);
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(VparShowCoreStatus::displayed));

  const std::string reference = readBinaryFile(kDefaultDisplayRef);
  const std::string block = extractDisplayBlock(reference);
  CHECK(!block.empty());
  // the handler emits the block with a leading newline (break after prompt).
  CHECK_EQ(out.str(), std::string("\n") + block);
  // the reference contains the same block byte-identically.
  CHECK(reference.find(block) != std::string::npos);
}

TEST(unit, CoreVparShowAfterPreparedValuesMatchesFortranSequenceBytes) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream scratch;
  handleVparXtrCore(session, in, scratch, "0.5 0.6");
  handleVparNCritCore(session, in, scratch, "12");
  handleVparVaccCore(session, in, scratch, "0.05");

  std::ostringstream out;
  handleVparShowCore(session, out);
  const std::string reference = readBinaryFile(kSequenceRef);
  const std::string block = extractDisplayBlock(reference);
  CHECK(!block.empty());
  CHECK_EQ(out.str(), std::string("\n") + block);
}

TEST(unit, CoreVparXtrInlineSetsBothSidesInvalidatesConvergenceOnly) {
  auto session = makeConvergedViscousSession();
  const StateSnapshot before = snapshot(session);
  CHECK(before.lvconv);
  CHECK(before.converged);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleVparXtrCore(session, in, out, "0.5 0.6");
  CHECK_EQ(static_cast<int>(status), static_cast<int>(VparXtrCoreStatus::applied));
  CHECK_EQ(out.str(), std::string());

  CHECK_NEAR(session.operating.topTripX, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, 0.6, 0.0, 0.0);
  // LVCONV-only-invalidation (xoper.f 2582).
  CHECK_FALSE(session.viscous.viscals.lvconv);
  CHECK_FALSE(session.viscous.converged);
  // all other preserve.
  const StateSnapshot after = snapshot(session);
  CHECK_EQ(after.lblini, before.lblini);
  CHECK_EQ(after.mapping, before.mapping);
  CHECK_EQ(after.wakeValid, before.wakeValid);
  CHECK_EQ(after.systemValid, before.systemValid);
  CHECK_EQ(after.maximumIterations, before.maximumIterations);
  CHECK_EQ(static_cast<int>(after.status), static_cast<int>(before.status));
  CHECK_EQ(after.bufferRevision, before.bufferRevision);
  CHECK_EQ(after.panelSourceRevision, before.panelSourceRevision);
  CHECK_EQ(static_cast<int>(after.origin), static_cast<int>(before.origin));
}

TEST(unit, CoreVparXtrPromptCollectsBothValues) {
  auto session = makeVparDefaultSession();
  std::istringstream in("0.4\n0.7\n");
  std::ostringstream out;
  const auto status = handleVparXtrCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status), static_cast<int>(VparXtrCoreStatus::applied));
  CHECK(out.str().find("Enter top    side Xtrip/c   r>  ") != std::string::npos);
  CHECK(out.str().find("Enter bottom side Xtrip/c   r>  ") !=
        std::string::npos);
  CHECK_NEAR(session.operating.topTripX, 0.4, 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, 0.7, 0.0, 0.0);

  const std::string reference = readBinaryFile(kXtrPromptRef);
  CHECK(reference.find("Enter top    side Xtrip/c   r>  ") != std::string::npos);
  CHECK(reference.find("Enter bottom side Xtrip/c   r>  ") != std::string::npos);
}

TEST(unit, CoreVparXtrSingleInlineValuePromptsBoth) {
  auto session = makeVparDefaultSession();
  std::istringstream in("0.4\n0.7\n");
  std::ostringstream out;
  const auto status = handleVparXtrCore(session, in, out, "0.5");
  CHECK_EQ(static_cast<int>(status), static_cast<int>(VparXtrCoreStatus::applied));
  // A single inline value is incomplete (NINPUT.GE.2 false) and is
  // discarded; both sides are then set via prompts.
  CHECK(out.str().find("Enter top    side Xtrip/c   r>  ") != std::string::npos);
  CHECK_NEAR(session.operating.topTripX, 0.4, 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, 0.7, 0.0, 0.0);
}

TEST(unit, CoreVparXtrAbortLeavesNoPartialMutation) {
  auto session = makeVparDefaultSession();
  const double topBefore = session.operating.topTripX;
  const double bottomBefore = session.operating.bottomTripX;
  const bool lvconvBefore = session.viscous.viscals.lvconv;
  std::istringstream in("0.4\n");  // second prompt preserves EOF
  std::ostringstream out;
  const auto status = handleVparXtrCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(VparXtrCoreStatus::inputAborted));
  CHECK_NEAR(session.operating.topTripX, topBefore, 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, bottomBefore, 0.0, 0.0);
  CHECK_EQ(session.viscous.viscals.lvconv, lvconvBefore);
}

TEST(unit, CoreVparNCritInlineSetsBothSidesInvalidatesConvergenceOnly) {
  auto session = makeConvergedViscousSession();
  const StateSnapshot before = snapshot(session);
  std::istringstream in;
  std::ostringstream out;
  const auto status = handleVparNCritCore(session, in, out, "12");
  CHECK_EQ(static_cast<int>(status),
           static_cast<int>(VparNCritCoreStatus::applied));
  CHECK_EQ(out.str(), std::string());
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 12.0, 0.0, 0.0);
  CHECK_FALSE(session.viscous.viscals.lvconv);
  CHECK_FALSE(session.viscous.converged);
  const StateSnapshot after = snapshot(session);
  CHECK_EQ(after.lblini, before.lblini);
  CHECK_EQ(after.mapping, before.mapping);
  CHECK_EQ(after.wakeValid, before.wakeValid);
  CHECK_EQ(static_cast<int>(after.status), static_cast<int>(before.status));
}

TEST(unit, CoreVparNCritPromptSetsBothSides) {
  auto session = makeVparDefaultSession();
  std::istringstream in("11\n");
  std::ostringstream out;
  const auto status = handleVparNCritCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status), static_cast<int>(VparNCritCoreStatus::applied));
  CHECK(out.str().find("Enter critical amplification ratio   r>  ") !=
        std::string::npos);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 11.0, 0.0, 0.0);
}

TEST(unit, CoreVparVaccInlineNoInvalidation) {
  auto session = makeConvergedViscousSession();
  const StateSnapshot before = snapshot(session);
  std::istringstream in;
  std::ostringstream out;
  const auto status = handleVparVaccCore(session, in, out, "0.05");
  CHECK_EQ(static_cast<int>(status), static_cast<int>(VparVaccCoreStatus::applied));
  CHECK_EQ(out.str(), std::string());
  CHECK_NEAR(session.operating.viscousAcceleration, 0.05, 0.0, 0.0);
  // VACC invalidated nothing (xoper.f 2626-2632).
  const StateSnapshot after = snapshot(session);
  CHECK_EQ(after.lvconv, before.lvconv);
  CHECK_EQ(after.converged, before.converged);
  CHECK_EQ(after.lblini, before.lblini);
  CHECK_EQ(after.mapping, before.mapping);
  CHECK_EQ(static_cast<int>(after.status), static_cast<int>(before.status));
}

TEST(unit, CoreVparVaccPromptAndAcceptsNonPositive) {
  auto session = makeVparDefaultSession();
  std::istringstream in("0.02\n");
  std::ostringstream out;
  const auto status = handleVparVaccCore(session, in, out, "");
  CHECK_EQ(static_cast<int>(status), static_cast<int>(VparVaccCoreStatus::applied));
  CHECK(out.str().find("Enter viscous acceleration parameter   r>  ") !=
        std::string::npos);
  CHECK_NEAR(session.operating.viscousAcceleration, 0.02, 0.0, 0.0);

  // Fortran validated VACC not: negative and Zero be unchanged
  // stored unchanged.
  std::istringstream in2;
  std::ostringstream out2;
  handleVparVaccCore(session, in2, out2, "-1");
  CHECK_NEAR(session.operating.viscousAcceleration, -1.0, 0.0, 0.0);
  handleVparVaccCore(session, in2, out2, "0");
  CHECK_NEAR(session.operating.viscousAcceleration, 0.0, 0.0, 0.0);
}

TEST(unit, CoreVparShowNonFiniteVaccShowsAsterisks) {
  auto session = makeVparDefaultSession();
  session.operating.viscousAcceleration =
      std::numeric_limits<double>::infinity();
  std::ostringstream out;
  handleVparShowCore(session, out);
  CHECK(out.str().find(" Vacc      ="
                       "        ********") != std::string::npos ||
        out.str().find(" Vacc      =********") != std::string::npos);
}

TEST(unit, CoreVparInitToggleAndRepeat) {
  auto session = makeConvergedViscousSession();
  const StateSnapshot before = snapshot(session);
  CHECK(before.lblini);
  CHECK(before.mapping);

  std::ostringstream out;
  const auto first = handleVparInitCore(session, out);
  CHECK_EQ(static_cast<int>(first),
           static_cast<int>(VparInitCoreStatus::willInitialize));
  CHECK_FALSE(session.viscous.viscals.lblini);
  CHECK_FALSE(session.viscous.viscals.mapping.valid);  // LIPAN=false
  CHECK(out.str().find(" BLs will be initialized on next point") !=
        std::string::npos);

  std::ostringstream out2;
  const auto second = handleVparInitCore(session, out2);
  CHECK_EQ(static_cast<int>(second),
           static_cast<int>(VparInitCoreStatus::initializedAssumed));
  CHECK(session.viscous.viscals.lblini);
  CHECK(out2.str().find(" BLs are assumed to be initialized") !=
        std::string::npos);

  // LVCONV/Result by INIT not changed (xoper.f 2649-2653).
  CHECK_EQ(session.viscous.viscals.lvconv, before.lvconv);
  CHECK_EQ(static_cast<int>(session.result.lastResult.status),
           static_cast<int>(before.status));
}

TEST(unit, CoreVparXtrNCritGuardedDuringPolarAccumulation) {
  auto session = makeConvergedViscousSession();
  const StateSnapshot before = snapshot(session);
  const double topBefore = session.operating.topTripX;
  const double ncritBefore = session.operating.criticalReynoldsExponents[0];
  std::istringstream in;
  std::ostringstream out;
  const auto xtr = handleVparXtrCore(session, in, out, "0.5 0.6", true);
  CHECK_EQ(static_cast<int>(xtr),
           static_cast<int>(VparXtrCoreStatus::polarAccumulationBlocked));
  const auto n = handleVparNCritCore(session, in, out, "12", true);
  CHECK_EQ(static_cast<int>(n),
           static_cast<int>(VparNCritCoreStatus::polarAccumulationBlocked));
  CHECK(out.str().find(" * Polar is being accumulated.") != std::string::npos);
  CHECK(out.str().find(" * Cannot change its parameters in midstream.") !=
        std::string::npos);
  // No change to trip position, Ncrit or convergence.
  CHECK_NEAR(session.operating.topTripX, topBefore, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], ncritBefore, 0.0, 0.0);
  const StateSnapshot after = snapshot(session);
  CHECK_EQ(after.lvconv, before.lvconv);
}

TEST(unit, CoreVparNoGeometryPanelMutationAcrossCommands) {
  auto session = makeConvergedViscousSession();
  const std::uint64_t bufferRev = session.geometry.bufferRevision;
  const std::uint64_t panelRev = session.paneling.panelSourceRevision;
  const std::size_t panelCount = session.paneling.panel.x.size();
  std::istringstream in;
  std::ostringstream out;
  handleVparXtrCore(session, in, out, "0.5 0.6");
  handleVparNCritCore(session, in, out, "12");
  handleVparVaccCore(session, in, out, "0.05");
  handleVparInitCore(session, out);
  CHECK_EQ(session.geometry.bufferRevision, bufferRev);
  CHECK_EQ(session.paneling.panelSourceRevision, panelRev);
  CHECK_EQ(session.paneling.panel.x.size(), panelCount);
}

TEST(unit, CoreVparPreparedValuesConsumedByControlPoint) {
  // Control proof: a viscous single point consumes the prepared
  // trip positions. A point with trip 0.5/0.6 gives a different Cd than a
  // point without trip (XSTRIP 1.0). The alfa handler is not used here.
  session::SessionComposition tripped = makeVparDefaultSession();
  tripped.operating.viscous = true;
  tripped.operating.machReynolds.reinf1 = 1000000.0;
  std::istringstream in;
  std::ostringstream out;
  handleVparXtrCore(tripped, in, out, "0.5 0.6");
  handleVparNCritCore(tripped, in, out, "9");
  const xfoil::AnalysisResult trippedPoint =
      session::runSpecAlpha(tripped, xfoil::Degrees{2.0}, nullptr);

  session::SessionComposition untripped = makeVparDefaultSession();
  untripped.operating.viscous = true;
  untripped.operating.machReynolds.reinf1 = 1000000.0;
  handleVparNCritCore(untripped, in, out, "9");
  const xfoil::AnalysisResult untrippedPoint =
      session::runSpecAlpha(untripped, xfoil::Degrees{2.0}, nullptr);

  CHECK(trippedPoint.successful);
  CHECK(untrippedPoint.successful);
  CHECK(trippedPoint.dragCoefficient != untrippedPoint.dragCoefficient);
}

TEST(unit, CoreVparVaccInstanceIndependent) {
  session::SessionComposition a = makeVparDefaultSession();
  session::SessionComposition b = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  handleVparVaccCore(a, in, out, "0.05");
  handleVparVaccCore(b, in, out, "0.5");
  CHECK_NEAR(a.operating.viscousAcceleration, 0.05, 0.0, 0.0);
  CHECK_NEAR(b.operating.viscousAcceleration, 0.5, 0.0, 0.0);
}

TEST(unit, CoreVparIndependentSessionsTransition) {
  session::SessionComposition a = makeVparDefaultSession();
  session::SessionComposition b = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  handleVparXtrCore(a, in, out, "0.5 0.6");
  handleVparNCritCore(a, in, out, "12");
  CHECK_NEAR(b.operating.topTripX, 1.0, 0.0, 0.0);
  CHECK_NEAR(b.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
}

TEST(unit, CoreVparShowTextDeterministic) {
  auto session = makeVparDefaultSession();
  std::ostringstream first;
  std::ostringstream second;
  handleVparShowCore(session, first);
  handleVparShowCore(session, second);
  CHECK_EQ(first.str(), second.str());
}

TEST(unit, CoreVparShowReflectsSessionBlparState) {
  // SHOW reads the authoritative session-bound
  // BLPAR-state (COMMON/BLPAR), not the default factory.
  auto session = makeVparDefaultSession();
  session.viscous.blpar.sccon = 6.0;
  session.viscous.blpar.ctrcex = 4.0;
  std::ostringstream out;
  handleVparShowCore(session, out);
  CHECK(out.str().find("Klag  =  6.0000") != std::string::npos);
  CHECK(out.str().find("CtiniX=  4.0000") != std::string::npos);
  // default SHOW stays unchanged when the state is not mutated.
  auto fresh = makeVparDefaultSession();
  std::ostringstream out2;
  handleVparShowCore(fresh, out2);
  CHECK(out2.str().find("Klag  =  5.6000") != std::string::npos);
}

// NT/NB according to
// xoper.f 2600-2623.

TEST(unit, CoreVparNcritTopInlineSetsTopOnly) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparNCritTopCore(session, in, out, "8.5") ==
        VparNCritCoreStatus::applied);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 8.5, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 9.0, 0.0, 0.0);
  CHECK(out.str().empty());
}

TEST(unit, CoreVparNcritBotInlineSetsBottomOnly) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparNCritBotCore(session, in, out, "7.25") ==
        VparNCritCoreStatus::applied);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 7.25, 0.0, 0.0);
  CHECK(out.str().empty());
}

TEST(unit, CoreVparNcritTopPromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("8.5\n");
  std::ostringstream out;
  CHECK(handleVparNCritTopCore(session, in, out, "") ==
        VparNCritCoreStatus::applied);
  CHECK_EQ(out.str(),
           "\nEnter top-surface critical amplification   r>  ");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 8.5, 0.0, 0.0);
}

TEST(unit, CoreVparNcritBotPromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("7.25\n");
  std::ostringstream out;
  CHECK(handleVparNCritBotCore(session, in, out, "") ==
        VparNCritCoreStatus::applied);
  CHECK_EQ(out.str(), "\nEnter bot-surface critical amplification   r>  ");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 7.25, 0.0, 0.0);
}

TEST(unit, CoreVparNcritTopInvalidInlineFallsToPrompt) {
  // GETFLT (userio.f) returns NINPUT=0 for 'abc'; VPAR then takes the
  // prompt path (xoper.f 2605-2609).
  auto session = makeVparDefaultSession();
  std::istringstream in("8.5\n");
  std::ostringstream out;
  CHECK(handleVparNCritTopCore(session, in, out, "abc") ==
        VparNCritCoreStatus::applied);
  CHECK(out.str().find("Enter top-surface critical amplification") !=
        std::string::npos);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 8.5, 0.0, 0.0);
}

TEST(unit, CoreVparNcritTopBlankPromptKeepsValueLikeAskr) {
  // ASKR (userio.f 64-66): an empty answer leaves the value unchanged.
  auto session = makeVparDefaultSession();
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleVparNCritTopCore(session, in, out, "") ==
        VparNCritCoreStatus::applied);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
}

TEST(unit, CoreVparNcritTopPolarGuardBeforePromptAndMutation) {
  auto session = makeVparDefaultSession();
  session.operating.viscous = true;  // LVISC
  std::istringstream in("8.5\n");
  std::ostringstream out;
  CHECK(handleVparNCritTopCore(session, in, out, "", true) ==
        VparNCritCoreStatus::polarAccumulationBlocked);
  // guard text byte-exact (FORMAT 2100), no prompt, no mutation,
  // no consumed prompt input.
  CHECK_EQ(out.str(),
           "\n * Polar is being accumulated.\n"
           " * Cannot change its parameters in midstream.\n");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
}

TEST(unit, CoreVparNcritBotPolarGuardBeforePromptAndMutation) {
  auto session = makeVparDefaultSession();
  session.operating.viscous = true;
  std::istringstream in("7.25\n");
  std::ostringstream out;
  CHECK(handleVparNCritBotCore(session, in, out, "7.25", true) ==
        VparNCritCoreStatus::polarAccumulationBlocked);
  CHECK_EQ(out.str(),
           "\n * Polar is being accumulated.\n"
           " * Cannot change its parameters in midstream.\n");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 9.0, 0.0, 0.0);
}

TEST(unit, CoreVparNcritGuardRequiresBothLpaccAndLvisc) {
  // LPACC=true, LVISC=false: Fortran (xoper.f 2601/2614) allows it.
  auto session = makeVparDefaultSession();
  session.operating.viscous = false;
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparNCritTopCore(session, in, out, "8.5", true) ==
        VparNCritCoreStatus::applied);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 8.5, 0.0, 0.0);
}

TEST(unit, CoreVparNcritTopThenBotAreIndependent) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  handleVparNCritTopCore(session, in, out, "8.5");
  handleVparNCritBotCore(session, in, out, "7.25");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 8.5, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 7.25, 0.0, 0.0);
  // A later N sets both sides (xoper.f 2590-2596), independently of NT/NB.
  handleVparNCritCore(session, in, out, "12");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 12.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 12.0, 0.0, 0.0);
  // NT/NB can be set separately again afterwards.
  handleVparNCritTopCore(session, in, out, "8.0");
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 8.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 12.0, 0.0, 0.0);
}

TEST(unit, CoreVparNcritTopInvalidatesConvergenceOnly) {
  auto session = makeVparDefaultSession();
  // before defined reference states.
  session.viscous.converged = true;
  session.viscous.viscals.lvconv = true;
  session.viscous.wakeValid = true;
  session.viscous.viscals.mapping.valid = true;
  session.viscous.viscals.lblini = true;
  const auto blparBefore = session.viscous.blpar;
  const auto xtripBefore = std::array{session.operating.topTripX,
                                      session.operating.bottomTripX};
  const double waklenBefore = session.operating.wakeLength;
  const double vaccelBefore = session.operating.viscousAcceleration;

  std::istringstream in;
  std::ostringstream out;
  handleVparNCritTopCore(session, in, out, "8.5");

  // LVCONV invalidation (confirmed C++ representation).
  CHECK(!session.viscous.converged);
  CHECK(!session.viscous.viscals.lvconv);
  // unchanged (xoper.f 2600-2610 writes only ACRIT(1) and LVCONV).
  CHECK(session.viscous.wakeValid);
  CHECK(session.viscous.viscals.mapping.valid);
  CHECK(session.viscous.viscals.lblini);
  CHECK_NEAR(session.viscous.blpar.sccon, blparBefore.sccon, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.cffac, blparBefore.cffac, 0.0, 0.0);
  CHECK_NEAR(session.operating.topTripX, xtripBefore[0], 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, xtripBefore[1], 0.0, 0.0);
  CHECK_NEAR(session.operating.wakeLength, waklenBefore, 0.0, 0.0);
  CHECK_NEAR(session.operating.viscousAcceleration, vaccelBefore, 0.0, 0.0);
}

TEST(unit, CoreVparNcritIndependentSessions) {
  auto a = makeVparDefaultSession();
  auto b = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  handleVparNCritTopCore(a, in, out, "8.5");
  handleVparNCritBotCore(a, in, out, "7.25");
  CHECK_NEAR(b.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
  CHECK_NEAR(b.operating.criticalReynoldsExponents[1], 9.0, 0.0, 0.0);
}

// WAKE/W according to
// xoper.f 2635-2646.

TEST(unit, CoreVparWakeInlineSetsLengthAndInvalidatesFourFlags) {
  auto session = makeVparDefaultSession();
  session.viscous.wakeValid = true;
  session.viscous.viscals.lblini = true;
  session.viscous.viscals.lvconv = true;
  session.viscous.converged = true;
  session.viscous.viscals.mapping.valid = true;
  const auto blparBefore = session.viscous.blpar;
  const auto acritBefore = session.operating.criticalReynoldsExponents;
  const auto xtripBefore = std::array{session.operating.topTripX,
                                      session.operating.bottomTripX};
  const double vaccelBefore = session.operating.viscousAcceleration;

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparWakeCore(session, in, out, "2.0") ==
        VparWakeCoreStatus::applied);
  CHECK_NEAR(session.operating.wakeLength, 2.0, 0.0, 0.0);
  // exactly LWAKE/LBLINI/LVCONV/LIPAN are invalidated.
  CHECK(!session.viscous.wakeValid);
  CHECK(!session.viscous.viscals.lblini);
  CHECK(!session.viscous.viscals.lvconv);
  CHECK(!session.viscous.converged);
  CHECK(!session.viscous.viscals.mapping.valid);
  // unchanged (xoper.f 2635-2646 writes nothing else).
  CHECK_NEAR(session.viscous.blpar.sccon, blparBefore.sccon, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.cffac, blparBefore.cffac, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0],
             acritBefore[0], 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1],
             acritBefore[1], 0.0, 0.0);
  CHECK_NEAR(session.operating.topTripX, xtripBefore[0], 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, xtripBefore[1], 0.0, 0.0);
  CHECK_NEAR(session.operating.viscousAcceleration, vaccelBefore, 0.0, 0.0);
  CHECK(out.str().empty());
}

TEST(unit, CoreVparWakeAliasParity) {
  auto a = makeVparDefaultSession();
  auto b = makeVparDefaultSession();
  a.viscous.wakeValid = true;
  b.viscous.wakeValid = true;
  a.viscous.viscals.mapping.valid = true;
  b.viscous.viscals.mapping.valid = true;
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparWakeCore(a, in, out, "2.0") ==
        VparWakeCoreStatus::applied);
  CHECK(handleVparWakeCore(b, in, out, "2.0") ==
        VparWakeCoreStatus::applied);
  CHECK_NEAR(a.operating.wakeLength, b.operating.wakeLength, 0.0, 0.0);
  CHECK_EQ(a.viscous.wakeValid, b.viscous.wakeValid);
  CHECK_EQ(a.viscous.viscals.lblini, b.viscous.viscals.lblini);
  CHECK_EQ(a.viscous.viscals.lvconv, b.viscous.viscals.lvconv);
  CHECK_EQ(a.viscous.viscals.mapping.valid, b.viscous.viscals.mapping.valid);
}

TEST(unit, CoreVparWakePromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n");
  std::ostringstream out;
  CHECK(handleVparWakeCore(session, in, out, "") ==
        VparWakeCoreStatus::applied);
  CHECK_EQ(out.str(), "\nEnter wake length/chord   r>  ");
  CHECK_NEAR(session.operating.wakeLength, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparWakeInvalidInlineFallsToPrompt) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n");
  std::ostringstream out;
  CHECK(handleVparWakeCore(session, in, out, "abc") ==
        VparWakeCoreStatus::applied);
  CHECK(out.str().find("Enter wake length/chord") != std::string::npos);
  CHECK_NEAR(session.operating.wakeLength, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparWakeBlankPromptKeepsValueButInvalidatesFlags) {
  // ASKR preserves the old value; the four flag invalidations
  // always occur after the IF/ELSE in Fortran (xoper.f 2643-2646).
  auto session = makeVparDefaultSession();
  session.viscous.wakeValid = true;
  session.viscous.viscals.lblini = true;
  session.viscous.viscals.lvconv = true;
  session.viscous.converged = true;
  session.viscous.viscals.mapping.valid = true;
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleVparWakeCore(session, in, out, "") ==
        VparWakeCoreStatus::applied);
  CHECK_NEAR(session.operating.wakeLength, 1.0, 0.0, 0.0);
  CHECK(!session.viscous.wakeValid);
  CHECK(!session.viscous.viscals.lblini);
  CHECK(!session.viscous.viscals.lvconv);
  CHECK(!session.viscous.converged);
  CHECK(!session.viscous.viscals.mapping.valid);
}

TEST(unit, CoreVparWakeHasNoPolarGuard) {
  // Fortran does not block WAKE for LPACC.AND.LVISC (xoper.f
  // 2635-2646 without guard). LVISC=true and running polar accumulation
  // must not prevent the command.
  auto session = makeVparDefaultSession();
  session.operating.viscous = true;
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparWakeCore(session, in, out, "2.0") ==
        VparWakeCoreStatus::applied);
  CHECK(out.str().find("Polar is being accumulated") ==
        std::string::npos);
  CHECK_NEAR(session.operating.wakeLength, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparWakeDoesNotClearWakeArraysImmediately) {
  // invalidation is not a physical delete (Fortran does not clear
  // the COMMON arrays).
  auto session = makeVparDefaultSession();
  session.viscous.wake.nw = 5;
  session.viscous.wake.x.assign(5, 1.5);
  std::istringstream in;
  std::ostringstream out;
  handleVparWakeCore(session, in, out, "0.5");
  CHECK_EQ(session.viscous.wake.x.size(), std::size_t{5});
  CHECK_NEAR(session.viscous.wake.x[0], 1.5, 0.0, 0.0);
  CHECK(!session.viscous.wakeValid);
}

// --- VPAR LAG/GB/CTR/CFAC ---------------
// Fortran anchor xoper.f 2656-2706. The prompt texts and SHOW values
// are confirmed byte- or value-exactly against the triple deterministic
// Fortran oracle runs (LC_ALL=C, fortran-bin/xfoil).

namespace {

// bit representation of all ten BLPAR fields in BLPAR.INC order.
std::array<std::uint64_t, 10> blparBits(
    const xfoil::core::boundary_layer::BoundaryLayerParameters& p) {
  const double fields[10] = {p.sccon, p.gacon, p.gbcon, p.gccon, p.dlcon,
                             p.ctrcon, p.ctrcex, p.duxcon, p.ctcon,
                             p.cffac};
  std::array<std::uint64_t, 10> bits{};
  for (std::size_t i = 0; i < 10; ++i) {
    std::memcpy(&bits[i], &fields[i], sizeof(double));
  }
  return bits;
}

bool bitsEqual(const std::array<std::uint64_t, 10>& a,
               const std::array<std::uint64_t, 10>& b,
               std::initializer_list<std::size_t> changed) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const bool expectedChanged =
        std::find(changed.begin(), changed.end(), i) != changed.end();
    if ((a[i] != b[i]) != expectedChanged) {
      return false;
    }
  }
  return true;
}

uint64_t doubleBits(double v) {
  uint64_t u{};
  std::memcpy(&u, &v, sizeof(double));
  return u;
}

}  // namespace

// LAG inline (xoper.f 2659-2661): only SCCON, DUXCON, DLCON; the seven
// remaining BLPAR fields bit-identical; no flag invalidation.
TEST(unit, CoreVparLagInlineSetsThreeFieldsOnly) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  const StateSnapshot flagsBefore = snapshot(session);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  // BLPAR.INC order: 0 SCCON, 7 DUXCON, 4 DLCON.
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {0, 4, 7}));
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.duxcon, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 1.5, 0.0, 0.0);
  CHECK_EQ(out.str(), "");
  // No validity flags or operating state changed.
  const StateSnapshot after = snapshot(session);
  CHECK_EQ(after.lblini, flagsBefore.lblini);
  CHECK_EQ(after.mapping, flagsBefore.mapping);
  CHECK_EQ(after.lvconv, flagsBefore.lvconv);
  CHECK_EQ(after.converged, flagsBefore.converged);
  CHECK_EQ(after.wakeValid, flagsBefore.wakeValid);
  CHECK_EQ(after.systemValid, flagsBefore.systemValid);
}

// LAG prompt texts byte-exact against the Fortran oracle (lag_prompt):
// three ASKR prompts in the order SCCON, DUXCON, DLCON.
TEST(unit, CoreVparLagPromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("6.0\n0.5\n1.5\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "") ==
        VparLagCoreStatus::applied);
  const std::string expected =
      "\nEnter shear lag constant   r>  "
      "\nEnter shear lag UxEQ weight   r>  "
      "\nEnter wake 1/dissipation-length factor   r>  ";
  CHECK_EQ(out.str(), expected);
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.duxcon, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 1.5, 0.0, 0.0);
}

// partial inline (NINPUT=2 < 3): both values are discarded and all
// three prompts appear (xoper.f 2658 checks NINPUT.GE.3). Oracle
// lag_partial: answers 7.0/0.6/1.6 -> SHOW 7.0000/0.60/1.6000.
TEST(unit, CoreVparLagPartialInlineDiscardedPromptsAllThree) {
  auto session = makeVparDefaultSession();
  std::istringstream in("7.0\n0.6\n1.6\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "6.0 0.5") ==
        VparLagCoreStatus::applied);
  CHECK_EQ(out.str(),
           "\nEnter shear lag constant   r>  "
           "\nEnter shear lag UxEQ weight   r>  "
           "\nEnter wake 1/dissipation-length factor   r>  ");
  CHECK_NEAR(session.viscous.blpar.sccon, 7.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.duxcon, 0.6, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 1.6, 0.0, 0.0);
}

// invalid inline argument (GETFLT error -> NINPUT=0): prompt path.
TEST(unit, CoreVparLagInvalidInlineFallsToPrompt) {
  auto session = makeVparDefaultSession();
  std::istringstream in("6.0\n0.5\n1.5\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "abc") ==
        VparLagCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 1.5, 0.0, 0.0);
}

// An empty ASKR answer preserves the previous value at each position
// (userio.f ASKR LINE.NE.' ' test); oracle lag_blank1/2/3.
TEST(unit, CoreVparLagBlankPromptKeepsValueAtEachPosition) {
  {
    auto session = makeVparDefaultSession();
    std::istringstream in("\n0.5\n1.5\n");
    std::ostringstream out;
    CHECK(handleVparLagCore(session, in, out, "") ==
          VparLagCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.sccon, 5.6, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.duxcon, 0.5, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.dlcon, 1.5, 0.0, 0.0);
  }
  {
    auto session = makeVparDefaultSession();
    std::istringstream in("6.0\n\n1.5\n");
    std::ostringstream out;
    CHECK(handleVparLagCore(session, in, out, "") ==
          VparLagCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.duxcon, 1.0, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.dlcon, 1.5, 0.0, 0.0);
  }
  {
    auto session = makeVparDefaultSession();
    std::istringstream in("6.0\n0.5\n\n");
    std::ostringstream out;
    CHECK(handleVparLagCore(session, in, out, "") ==
          VparLagCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.duxcon, 0.5, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.dlcon, 0.9, 0.0, 0.0);
  }
}

// Surplus inline values: only RINPUT(1..3) are used, the fourth is
// ignored (oracle lag_extra: SHOW as in lag_inline).
TEST(unit, CoreVparLagExtraInlineValuesIgnored) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "6.0 0.5 1.5 9.9") ==
        VparLagCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.duxcon, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 1.5, 0.0, 0.0);
  CHECK_EQ(out.str(), "");
}

// canonical partial mutation: EOF after the first prompt still writes
// SCCON back (the ASKR assignment at 2662 already happened); DUXCON/
// DLCON are unchanged; no atomic teardown.
TEST(unit, CoreVparLagEofAtSecondPromptKeepsPartialMutation) {
  auto session = makeVparDefaultSession();
  std::istringstream in("6.0\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "") ==
        VparLagCoreStatus::inputAborted);
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.duxcon, 1.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 0.9, 0.0, 0.0);
}

TEST(unit, CoreVparLagEofAtThirdPromptKeepsFirstTwo) {
  auto session = makeVparDefaultSession();
  std::istringstream in("6.0\n0.5\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "") ==
        VparLagCoreStatus::inputAborted);
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.duxcon, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.dlcon, 0.9, 0.0, 0.0);
}

TEST(unit, CoreVparLagEofAtFirstPromptNoMutation) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "") ==
        VparLagCoreStatus::inputAborted);
  const auto def = xfoil::core::boundary_layer::blparConstants();
  CHECK(bitsEqual(blparBits(def), blparBits(session.viscous.blpar), {}));
}

// guard (xoper.f 2657-2660): LPACC.AND.LVISC blocks before prompt and
// mutation; no prompt output, no consumption of the input, no
// mutation. Oracle lag_guard: SHOW shows the defaults.
TEST(unit, CoreVparLagPolarGuardBeforePromptAndMutation) {
  auto session = makeConvergedViscousSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in("6.0\n0.5\n1.5\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "6.0 0.5 1.5", true) ==
        VparLagCoreStatus::polarAccumulationBlocked);
  CHECK_EQ(out.str(),
           "\n * Polar is being accumulated.\n"
           " * Cannot change its parameters in midstream.\n");
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {}));
  // input was not consumed (istream still full).
  std::string rest;
  CHECK(static_cast<bool>(in >> rest));
  CHECK_EQ(rest, std::string("6.0"));
}

// the guard requires both flags: LPACC without LVISC, or LVISC without
// LPACC, allows the mutation (oracle: guard cases only for viscous computation).
TEST(unit, CoreVparLagGuardRequiresBothLpaccAndLvisc) {
  {
    auto session = makeVparDefaultSession();  // viscous=false
    std::istringstream in;
    std::ostringstream out;
    CHECK(handleVparLagCore(session, in, out, "6.0 0.5 1.5", true) ==
          VparLagCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  }
  {
    auto session = makeConvergedViscousSession();  // viscous=true
    std::istringstream in;
    std::ostringstream out;
    CHECK(handleVparLagCore(session, in, out, "6.0 0.5 1.5", false) ==
          VparLagCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  }
}

// GB inline (xoper.f 2676-2678 + 2684): only GACON, GBCON and the
// canonically derived CTCON change; GCCON and the remaining fields
// stay bit-identical. CTCON bits match the demonstrated canonical
// structure 0.5/(gacon*gacon*gbcon) (harness pair 2).
TEST(unit, CoreVparGbInlineSetsGaconGbconAndCtcon) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "7.0 0.8") ==
        VparGbCoreStatus::applied);
  // Indices: 1 GACON, 2 GBCON, 8 CTCON.
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {1, 2, 8}));
  CHECK_NEAR(session.viscous.blpar.gacon, 7.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.gbcon, 0.8, 0.0, 0.0);
  CHECK_EQ(doubleBits(session.viscous.blpar.ctcon),
           doubleBits(0.5 / (7.0 * 7.0 * 0.8)));
  CHECK_EQ(out.str(), "");
}

// GB prompt texts byte-exact (oracle gb_prompt).
TEST(unit, CoreVparGbPromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("7.0\n0.8\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "") == VparGbCoreStatus::applied);
  CHECK_EQ(out.str(),
           "\nEnter G-beta constant A   r>  "
           "\nEnter G-beta constant B   r>  ");
  CHECK_NEAR(session.viscous.blpar.gacon, 7.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.gbcon, 0.8, 0.0, 0.0);
}

// partial inline GB (NINPUT=1 < 2): value discarded, both prompts
// appear (oracle gb_partial: 8.0/0.9, KCt=0.00868).
TEST(unit, CoreVparGbPartialInlineDiscardedPromptsBoth) {
  auto session = makeVparDefaultSession();
  std::istringstream in("8.0\n0.9\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "7.0") ==
        VparGbCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.gacon, 8.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.gbcon, 0.9, 0.0, 0.0);
  CHECK_EQ(doubleBits(session.viscous.blpar.ctcon),
           doubleBits(0.5 / (8.0 * 8.0 * 0.9)));
}

TEST(unit, CoreVparGbInvalidInlineFallsToPrompt) {
  auto session = makeVparDefaultSession();
  std::istringstream in("7.0\n0.8\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "zz") ==
        VparGbCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.gacon, 7.0, 0.0, 0.0);
}

// empty answer at the A prompt: GACON stays, but CTCON is still
// recomputed from (old GACON, new GBCON) (2684 is outside IF/ELSE).
// oracle gb_blank1: A=6.7 B=0.8 KCt=0.01392.
TEST(unit, CoreVparGbBlankAKeepsGaconButRecomputesCtcon) {
  auto session = makeVparDefaultSession();
  std::istringstream in("\n0.8\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "") == VparGbCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.gacon, 6.70, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.gbcon, 0.8, 0.0, 0.0);
  CHECK_EQ(doubleBits(session.viscous.blpar.ctcon),
           doubleBits(0.5 / (6.70 * 6.70 * 0.8)));
}

// empty answer at the B prompt: GBCON stays, CTCON comes from
// (new GACON, old GBCON). Oracle gb_blank2: A=7.0 B=0.75 KCt=0.01361.
TEST(unit, CoreVparGbBlankBKeepsGbconRecomputesCtcon) {
  auto session = makeVparDefaultSession();
  std::istringstream in("7.0\n\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "") == VparGbCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.gacon, 7.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.gbcon, 0.75, 0.0, 0.0);
  CHECK_EQ(doubleBits(session.viscous.blpar.ctcon),
           doubleBits(0.5 / (7.0 * 7.0 * 0.75)));
}

// EOF after the A prompt: GACON is still written (2681 happened),
// GBCON and CTCON unchanged - the CTCON write (2684) is not reached
// on abort.
TEST(unit, CoreVparGbEofAtSecondPromptKeepsGaconNoCtconWrite) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in("7.0\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "") ==
        VparGbCoreStatus::inputAborted);
  CHECK_NEAR(session.viscous.blpar.gacon, 7.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.gbcon, 0.75, 0.0, 0.0);
  // CTCON bit-identical to the default (BLPINI-structure).
  CHECK_EQ(blparBits(session.viscous.blpar)[8], before[8]);
}

TEST(unit, CoreVparGbEofAtFirstPromptNoMutation) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "") ==
        VparGbCoreStatus::inputAborted);
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {}));
}

TEST(unit, CoreVparGbPolarGuardBeforePromptAndMutation) {
  auto session = makeConvergedViscousSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in("7.0\n0.8\n");
  std::ostringstream out;
  CHECK(handleVparGbCore(session, in, out, "7.0 0.8", true) ==
        VparGbCoreStatus::polarAccumulationBlocked);
  CHECK_EQ(out.str(),
           "\n * Polar is being accumulated.\n"
           " * Cannot change its parameters in midstream.\n");
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {}));
  std::string rest;
  CHECK(static_cast<bool>(in >> rest));
  CHECK_EQ(rest, std::string("7.0"));
}

// CTR inline (xoper.f 2691-2693): only CTRCON, CTRCEX.
TEST(unit, CoreVparCtrInlineSetsTwoFieldsOnly) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparCtrCore(session, in, out, "2.0 4.0") ==
        VparCtrCoreStatus::applied);
  // Indices: 5 CTRCON, 6 CTRCEX.
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {5, 6}));
  CHECK_NEAR(session.viscous.blpar.ctrcon, 2.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.ctrcex, 4.0, 0.0, 0.0);
  CHECK_EQ(out.str(), "");
}

TEST(unit, CoreVparCtrPromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n4.0\n");
  std::ostringstream out;
  CHECK(handleVparCtrCore(session, in, out, "") ==
        VparCtrCoreStatus::applied);
  CHECK_EQ(out.str(),
           "\nEnter initial-Ctau constant   r>  "
           "\nEnter initial-Ctau exponent   r>  ");
  CHECK_NEAR(session.viscous.blpar.ctrcon, 2.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.ctrcex, 4.0, 0.0, 0.0);
}

TEST(unit, CoreVparCtrPartialInlineDiscardedPromptsBoth) {
  auto session = makeVparDefaultSession();
  std::istringstream in("3.0\n5.0\n");
  std::ostringstream out;
  CHECK(handleVparCtrCore(session, in, out, "2.0") ==
        VparCtrCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.ctrcon, 3.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.ctrcex, 5.0, 0.0, 0.0);
}

TEST(unit, CoreVparCtrInvalidInlineFallsToPrompt) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n4.0\n");
  std::ostringstream out;
  CHECK(handleVparCtrCore(session, in, out, "q") ==
        VparCtrCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.ctrcon, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparCtrBlankPromptsKeepValues) {
  {
    auto session = makeVparDefaultSession();
    std::istringstream in("\n4.0\n");
    std::ostringstream out;
    CHECK(handleVparCtrCore(session, in, out, "") ==
          VparCtrCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.ctrcon, 1.8, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.ctrcex, 4.0, 0.0, 0.0);
  }
  {
    auto session = makeVparDefaultSession();
    std::istringstream in("2.0\n\n");
    std::ostringstream out;
    CHECK(handleVparCtrCore(session, in, out, "") ==
          VparCtrCoreStatus::applied);
    CHECK_NEAR(session.viscous.blpar.ctrcon, 2.0, 0.0, 0.0);
    CHECK_NEAR(session.viscous.blpar.ctrcex, 3.3, 0.0, 0.0);
  }
}

TEST(unit, CoreVparCtrEofAtSecondPromptKeepsFirst) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n");
  std::ostringstream out;
  CHECK(handleVparCtrCore(session, in, out, "") ==
        VparCtrCoreStatus::inputAborted);
  CHECK_NEAR(session.viscous.blpar.ctrcon, 2.0, 0.0, 0.0);
  CHECK_NEAR(session.viscous.blpar.ctrcex, 3.3, 0.0, 0.0);
}

TEST(unit, CoreVparCtrPolarGuardBeforePromptAndMutation) {
  auto session = makeConvergedViscousSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in("2.0\n4.0\n");
  std::ostringstream out;
  CHECK(handleVparCtrCore(session, in, out, "2.0 4.0", true) ==
        VparCtrCoreStatus::polarAccumulationBlocked);
  CHECK_EQ(out.str(),
           "\n * Polar is being accumulated.\n"
           " * Cannot change its parameters in midstream.\n");
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {}));
  std::string rest;
  CHECK(static_cast<bool>(in >> rest));
  CHECK_EQ(rest, std::string("2.0"));
}

// CFAC (xoper.f 2701-2706): only CFFAC; no guard - it also applies
// during polar accumulation and with LVISC (oracle cfac_pacc:
// no blocking, SHOW unchanged because CFFAC is not shown).
TEST(unit, CoreVparCfacInlineSetsCffacOnlyNoGuard) {
  auto session = makeConvergedViscousSession();
  const auto before = blparBits(session.viscous.blpar);
  const StateSnapshot flagsBefore = snapshot(session);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparCfacCore(session, in, out, "2.0") ==
        VparCfacCoreStatus::applied);
  // index 9 = CFFAC.
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {9}));
  CHECK_NEAR(session.viscous.blpar.cffac, 2.0, 0.0, 0.0);
  CHECK_EQ(out.str(), "");
  const StateSnapshot after = snapshot(session);
  CHECK_EQ(after.lblini, flagsBefore.lblini);
  CHECK_EQ(after.mapping, flagsBefore.mapping);
  CHECK_EQ(after.lvconv, flagsBefore.lvconv);
  CHECK_EQ(after.converged, flagsBefore.converged);
  CHECK_EQ(after.wakeValid, flagsBefore.wakeValid);
}

TEST(unit, CoreVparCfacPromptTextMatchesFortran) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n");
  std::ostringstream out;
  CHECK(handleVparCfacCore(session, in, out, "") ==
        VparCfacCoreStatus::applied);
  CHECK_EQ(out.str(), "\nEnter Cf scaling factor   r>  ");
  CHECK_NEAR(session.viscous.blpar.cffac, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparCfacInvalidInlineFallsToPrompt) {
  auto session = makeVparDefaultSession();
  std::istringstream in("2.0\n");
  std::ostringstream out;
  CHECK(handleVparCfacCore(session, in, out, "x") ==
        VparCfacCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.cffac, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparCfacBlankPromptKeepsValue) {
  auto session = makeVparDefaultSession();
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleVparCfacCore(session, in, out, "") ==
        VparCfacCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.cffac, 1.0, 0.0, 0.0);
}

TEST(unit, CoreVparCfacExtraInlineValuesIgnored) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparCfacCore(session, in, out, "2.0 3.0") ==
        VparCfacCoreStatus::applied);
  CHECK_NEAR(session.viscous.blpar.cffac, 2.0, 0.0, 0.0);
}

TEST(unit, CoreVparCfacEofAtPromptNoMutation) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparCfacCore(session, in, out, "") ==
        VparCfacCoreStatus::inputAborted);
  CHECK(bitsEqual(before, blparBits(session.viscous.blpar), {}));
}

// retry for an invalid prompt answer (userio.f ASKR ERR=10): the same
// prompt repeats without writing a field. Oracle lag_retry: 'abc' ->
// prompt appears twice, values afterwards correct.
TEST(unit, CoreVparLagInvalidPromptAnswerRepeatsPrompt) {
  auto session = makeVparDefaultSession();
  std::istringstream in("abc\n6.0\n0.5\n1.5\n");
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "") ==
        VparLagCoreStatus::applied);
  CHECK_EQ(out.str(),
           "\nEnter shear lag constant   r>  "
           "\nEnter shear lag constant   r>  "
           "\nEnter shear lag UxEQ weight   r>  "
           "\nEnter wake 1/dissipation-length factor   r>  ");
  CHECK_NEAR(session.viscous.blpar.sccon, 6.0, 0.0, 0.0);
}

// four commands in sequence: each field goes to the correct group, all
// others stay bit-identical; no flag invalidation (before/after
// matrix over all ten fields).
TEST(unit, CoreVparLagGbCtrCfacSequenceMutatesOnlyTheirFields) {
  auto session = makeVparDefaultSession();
  const auto before = blparBits(session.viscous.blpar);
  const StateSnapshot flagsBefore = snapshot(session);
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparLagCore(session, in, out, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  CHECK(handleVparGbCore(session, in, out, "7.0 0.8") ==
        VparGbCoreStatus::applied);
  CHECK(handleVparCtrCore(session, in, out, "2.0 4.0") ==
        VparCtrCoreStatus::applied);
  CHECK(handleVparCfacCore(session, in, out, "2.0") ==
        VparCfacCoreStatus::applied);
  const auto after = blparBits(session.viscous.blpar);
  // changed: SCCON(0) GACON(1) GBCON(2) DLCON(4) CTRCON(5) CTRCEX(6)
  // DUXCON(7) CTCON(8) CFFAC(9); unchanged: GCCON(3).
  CHECK(bitsEqual(before, after, {0, 1, 2, 4, 5, 6, 7, 8, 9}));
  CHECK_EQ(after[8], doubleBits(0.5 / (7.0 * 7.0 * 0.8)));
  const StateSnapshot afterFlags = snapshot(session);
  CHECK_EQ(afterFlags.lblini, flagsBefore.lblini);
  CHECK_EQ(afterFlags.mapping, flagsBefore.mapping);
  CHECK_EQ(afterFlags.lvconv, flagsBefore.lvconv);
  CHECK_EQ(afterFlags.converged, flagsBefore.converged);
  CHECK_EQ(afterFlags.wakeValid, flagsBefore.wakeValid);
  CHECK_EQ(afterFlags.systemValid, flagsBefore.systemValid);
  CHECK(afterFlags.status == flagsBefore.status);
}

// SHOW after mutations: the FORMAT-1200 lines show the mutated session
// values; non-addressed values stay byte-identical. Reference:
// gb_inline oracle block (A=7.0000 B=0.8000 KCt=0.01276) and
// ctr_inline oracle block (CtiniK=2.0 CtiniX=4.0).
TEST(unit, CoreVparShowReflectsLagGbCtrMutations) {
  auto session = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream mut;
  CHECK(handleVparLagCore(session, in, mut, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  CHECK(handleVparGbCore(session, in, mut, "7.0 0.8") ==
        VparGbCoreStatus::applied);
  CHECK(handleVparCtrCore(session, in, mut, "2.0 4.0") ==
        VparCtrCoreStatus::applied);
  CHECK(handleVparCfacCore(session, in, mut, "2.0") ==
        VparCfacCoreStatus::applied);
  std::ostringstream show;
  handleVparShowCore(session, show);
  CHECK(show.str().find(" Klag  =  6.0000     Uxwt  =    0.50       "
                        "Kdl =  1.5000") != std::string::npos);
  CHECK(show.str().find(" A     =  7.0000     B     =  0.8000       "
                        "KCt = 0.01276") != std::string::npos);
  CHECK(show.str().find(" CtiniK=  2.0000     CtiniX=  4.0000") !=
        std::string::npos);
  // non-addressed lines (Xtr/Ncrit/Vacc/WakeL) stay at default.
  CHECK(show.str().find(" Xtr/c     =  1.0000    top    side") !=
        std::string::npos);
  CHECK(show.str().find(" WakeL/c   =   1.000") != std::string::npos);
}

// session independence: a mutation in session b does not affect a.
TEST(unit, CoreVparLagGbCtrCfacIndependentSessions) {
  auto a = makeVparDefaultSession();
  auto b = makeVparDefaultSession();
  std::istringstream in;
  std::ostringstream out;
  CHECK(handleVparLagCore(b, in, out, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  CHECK(handleVparGbCore(b, in, out, "7.0 0.8") == VparGbCoreStatus::applied);
  CHECK(handleVparCtrCore(b, in, out, "2.0 4.0") ==
        VparCtrCoreStatus::applied);
  CHECK(handleVparCfacCore(b, in, out, "2.0") ==
        VparCfacCoreStatus::applied);
  const auto def = xfoil::core::boundary_layer::blparConstants();
  CHECK(bitsEqual(blparBits(def), blparBits(a.viscous.blpar), {}));
  CHECK_NEAR(b.viscous.blpar.sccon, 6.0, 0.0, 0.0);
}

// REST according to
// xoper.f 2709-2715; BLPINI xbl.f 1578-1596.

TEST(unit, CoreVparRestAppliedWithoutOutputOnDefaultSession) {
  auto session = makeVparDefaultSession();
  std::ostringstream out;
  CHECK(handleVparRestCore(session, out) == VparRestCoreStatus::applied);
  CHECK(out.str().empty());  // Fortran: no output, no prompt
  CHECK(blparBits(session.viscous.blpar) ==
        blparBits(xfoil::core::boundary_layer::blparConstants()));
}

TEST(unit, CoreVparRestAfterLagGbCtrCfacGcconRestoresAllTen) {
  auto session = makeConvergedViscousSession();
  std::istringstream in;
  std::ostringstream mut;
  CHECK(handleVparLagCore(session, in, mut, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  CHECK(handleVparGbCore(session, in, mut, "7.0 0.8") ==
        VparGbCoreStatus::applied);
  CHECK(handleVparCtrCore(session, in, mut, "2.0 4.0") ==
        VparCtrCoreStatus::applied);
  CHECK(handleVparCfacCore(session, in, mut, "2.0") ==
        VparCfacCoreStatus::applied);
  session.viscous.blpar.gccon = 99.0;  // GCCON has no VPAR set command
  const auto mutated = blparBits(session.viscous.blpar);
  std::ostringstream out;
  CHECK(handleVparRestCore(session, out) == VparRestCoreStatus::applied);
  CHECK(out.str().empty());
  // all ten fields exactly match BLPINI, not only the previously visible
  // ones; CTCON (index 8) carries the default value, not the GB value.
  CHECK(blparBits(session.viscous.blpar) ==
        blparBits(xfoil::core::boundary_layer::blparConstants()));
  CHECK(mutated != blparBits(session.viscous.blpar));
}

TEST(unit, CoreVparRestPolarGuardBeforeMutation) {
  auto session = makeConvergedViscousSession();
  std::istringstream in;
  std::ostringstream mut;
  CHECK(handleVparCfacCore(session, in, mut, "2.0") ==
        VparCfacCoreStatus::applied);  // CFAC without guard: mutation happens before the guard case
  const auto before = blparBits(session.viscous.blpar);
  std::ostringstream out;
  CHECK(handleVparRestCore(session, out, true) ==
        VparRestCoreStatus::polarAccumulationBlocked);
  CHECK_EQ(out.str(),
           "\n * Polar is being accumulated.\n"
           " * Cannot change its parameters in midstream.\n");
  CHECK(blparBits(session.viscous.blpar) == before);  // no mutation
}

TEST(unit, CoreVparRestGuardMatrix) {
  {  // LPACC=false, LVISC=false
    auto session = makeVparDefaultSession();
    std::ostringstream out;
    CHECK(handleVparRestCore(session, out, false) ==
          VparRestCoreStatus::applied);
    CHECK(out.str().empty());
  }
  {  // LPACC=false, LVISC=true
    auto session = makeConvergedViscousSession();
    std::ostringstream out;
    CHECK(handleVparRestCore(session, out, false) ==
          VparRestCoreStatus::applied);
    CHECK(out.str().empty());
  }
  {  // LPACC=true, LVISC=false
    auto session = makeVparDefaultSession();
    std::ostringstream out;
    CHECK(handleVparRestCore(session, out, true) ==
          VparRestCoreStatus::applied);
    CHECK(out.str().empty());
  }
  {  // LPACC=true, LVISC=true
    auto session = makeConvergedViscousSession();
    std::ostringstream out;
    CHECK(handleVparRestCore(session, out, true) ==
          VparRestCoreStatus::polarAccumulationBlocked);
  }
}

TEST(unit, CoreVparRestKeepsOperatingParameters) {
  auto session = makeConvergedViscousSession();
  session::setViscousAcceleration(session, 0.02);
  session::setWakeLength(session, 2.0);
  session::setCriticalAmplificationSides(session, 5.0, 6.0);
  session::setTripPositions(session, 0.5, 0.6);
  std::ostringstream out;
  CHECK(handleVparRestCore(session, out) == VparRestCoreStatus::applied);
  CHECK_NEAR(session.operating.viscousAcceleration, 0.02, 0.0, 0.0);
  CHECK_NEAR(session.operating.wakeLength, 2.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[0], 5.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.criticalReynoldsExponents[1], 6.0, 0.0, 0.0);
  CHECK_NEAR(session.operating.topTripX, 0.5, 0.0, 0.0);
  CHECK_NEAR(session.operating.bottomTripX, 0.6, 0.0, 0.0);
  CHECK(session.operating.viscous);
}

TEST(unit, CoreVparRestIdempotentWithoutExtraOutput) {
  auto session = makeConvergedViscousSession();
  std::istringstream in;
  std::ostringstream mut;
  CHECK(handleVparLagCore(session, in, mut, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  std::ostringstream out1;
  CHECK(handleVparRestCore(session, out1) == VparRestCoreStatus::applied);
  const auto afterFirst = blparBits(session.viscous.blpar);
  const bool lvconv = session.viscous.viscals.lvconv;
  const bool converged = session.viscous.converged;
  std::ostringstream out2;
  CHECK(handleVparRestCore(session, out2) == VparRestCoreStatus::applied);
  CHECK(out2.str().empty());
  CHECK(blparBits(session.viscous.blpar) == afterFirst);
  CHECK_EQ(session.viscous.viscals.lvconv, lvconv);
  CHECK_EQ(session.viscous.converged, converged);
}

TEST(unit, CoreVparRestIndependentSessions) {
  auto a = makeConvergedViscousSession();
  auto b = makeConvergedViscousSession();
  std::istringstream in;
  std::ostringstream mut;
  CHECK(handleVparLagCore(a, in, mut, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  CHECK(handleVparLagCore(b, in, mut, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  std::ostringstream out;
  CHECK(handleVparRestCore(a, out) == VparRestCoreStatus::applied);
  CHECK_NEAR(a.viscous.blpar.sccon, 5.6, 0.0, 0.0);
  CHECK_NEAR(b.viscous.blpar.sccon, 6.0, 0.0, 0.0);
}

TEST(unit, CoreVparShowAfterRestEqualsPreMutationShow) {
  auto session = makeConvergedViscousSession();
  std::ostringstream base;
  handleVparShowCore(session, base);
  std::istringstream in;
  std::ostringstream mut;
  CHECK(handleVparLagCore(session, in, mut, "6.0 0.5 1.5") ==
        VparLagCoreStatus::applied);
  CHECK(handleVparGbCore(session, in, mut, "7.0 0.8") ==
        VparGbCoreStatus::applied);
  CHECK(handleVparCtrCore(session, in, mut, "2.0 4.0") ==
        VparCtrCoreStatus::applied);
  CHECK(handleVparCfacCore(session, in, mut, "2.0") ==
        VparCfacCoreStatus::applied);
  std::ostringstream mutated;
  handleVparShowCore(session, mutated);
  CHECK(mutated.str() != base.str());
  CHECK(mutated.str().find("Klag  =  6.0000") != std::string::npos);
  CHECK(mutated.str().find("KCt = 0.01276") != std::string::npos);
  std::ostringstream out;
  CHECK(handleVparRestCore(session, out) == VparRestCoreStatus::applied);
  std::ostringstream restored;
  handleVparShowCore(session, restored);
  CHECK_EQ(restored.str(), base.str());  // canonical defaults, KCt 0.01485
  CHECK(restored.str().find("KCt = 0.01485") != std::string::npos);
  // CFFAC/GCCON remain canonically invisible; no extra output.
  CHECK(restored.str().find("Cffac") == std::string::npos);
  CHECK(restored.str().find("Gcc") == std::string::npos);
}

// End: tests/unit/CoreVparHandlerTests.cpp


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

// file: tests/unit/CoreIntxHalfHandlerTests.cpp
// TOP handlers for INTX and HALF. Text contracts are verified
// byte-exactly against the LC_ALL=C fortran-bin/xfoil oracle
// (xfoil.f INTX 2461-2593, TOP-HALF 242-252, userio.f ASKS/ASKR,
// aread.f AREAD): source menu with polar slots, selection prompts
// with NPOL suffix, 'No current airfoil available' re-prompt,
// 'invalid response. No action taken.', FRAC prompt with default
// 0.5 and repetition for an invalid number, name prompt only after
// the buffer mutation, success and hint text, and the AREAD open
// error including '*** LOAD NOT COMPLETED ***'. HALF is canonically
// silent.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <sstream>
#include <string>

namespace {

using namespace xfoil;
using app::cli::handleHalfCore;
using app::cli::handleIntxCore;
using app::cli::HalfCoreStatus;
using app::cli::IntxCoreStatus;

// CHARACTER*48 padding as in the Fortran-oracle (name/NAMEPOL/NAMEINT).
std::string pad48(std::string_view text) {
  std::string field(text);
  if (field.size() < 48) {
    field.append(48 - field.size(), ' ');
  }
  return field;
}

void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412);
  CHECK(naca.isOk());
  CHECK(
      session::setBufferGeometry(composition, naca.value().buffer).success());
  xfoil::core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
}

void configureInviscid(session::SessionComposition& composition) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = false;
  CHECK(session::configure(composition, config).success());
}

void addPolarSlot(session::SessionComposition& composition,
                  const std::string& name, std::size_t nside) {
  const auto naca = core::geometry::naca4(2412, nside);
  CHECK(naca.isOk());
  core::polar::PolarDataset slot;
  slot.metadata.airfoilName = name;
  slot.geometry.x = naca.value().xb;
  slot.geometry.y = naca.value().yb;
  composition.polar.slots.push_back(std::move(slot));
}

}  // namespace

// success case current+current with the default fraction: byte-exact
// output sequence according to oracle intx_cc.out.
TEST(unit, CoreIntxHandlerTextContractCurrentCurrent) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);

  std::istringstream in("C\nC\n\nTESTNAME\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) == IntxCoreStatus::success);

  const std::string expected =
      "\n  F  disk file\n"
      "  C  current airfoil  " + pad48("NACA 2412") + "\n"
      "\n  Select source of airfoil \"0\" ( F C ):  \n"
      "  Select source of airfoil \"1\" ( F C ):  \n"
      " airfoil \"0\":  " + pad48("NACA 2412") + "\n"
      " airfoil \"1\":  " + pad48("NACA 2412") + "\n"
      "\nSpecify interpolating fraction  0...1   r>  "
      " Max thickness =     0.120023  at x =   0.305\n"
      " Max camber    =     0.020000  at x =   0.400\n"
      "\nEnter new airfoil name   s>  \n"
      " Result has been placed in buffer airfoil\n"
      " Execute PCOP or PANE to set new current airfoil\n";
  CHECK_EQ(out.str(), expected);
  CHECK_EQ(composition.geometry.name, std::string("TESTNAME"));
  CHECK_EQ(composition.geometry.buffer.x.size(), std::size_t{160});
}

// invalid selection 'X': 'invalid response. No action taken.',
// no buffer mutation.
TEST(unit, CoreIntxHandlerInvalidResponseNoMutation) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  const std::vector<double> bufferBefore = composition.geometry.buffer.x;
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;

  std::istringstream in("X\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) ==
        IntxCoreStatus::invalidSelection);
  CHECK_EQ(out.str(),
           "\n  F  disk file\n"
           "  C  current airfoil  " + pad48("NACA 2412") + "\n"
           "\n  Select source of airfoil \"0\" ( F C ):  "
           "\n Invalid response.  No action taken.\n");
  CHECK_EQ(composition.geometry.buffer.x.size(), bufferBefore.size());
  CHECK_EQ(composition.geometry.bufferRevision, revisionBefore);
}

// empty selection line: canonical also Label 90.
TEST(unit, CoreIntxHandlerBlankResponseIsInvalid) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) ==
        IntxCoreStatus::invalidSelection);
  CHECK(out.str().find(" Invalid response.  No action taken.\n") !=
        std::string::npos);
}

// EOF for the source selection: READ END=90 -> invalid response.
TEST(unit, CoreIntxHandlerEofAtSourceIsInvalidResponse) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  std::istringstream in("C\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) ==
        IntxCoreStatus::invalidSelection);
  CHECK(out.str().find(" Invalid response.  No action taken.\n") !=
        std::string::npos);
}

// 'C' without a current contour: message on the same prompt line,
// then the same source is selected again (oracle intx_nocur2.out).
TEST(unit, CoreIntxHandlerNoCurrentAirfoilReprompts) {
  session::SessionComposition composition;
  std::istringstream in("C\nC\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) ==
        IntxCoreStatus::invalidSelection);
  const std::string expected =
      "\n  F  disk file\n"
      "  C  current airfoil  " + pad48("") + "\n"
      "\n  Select source of airfoil \"0\" ( F C ):  "
      " No current airfoil available\n"
      "\n  Select source of airfoil \"0\" ( F C ):  "
      " No current airfoil available\n"
      "\n  Select source of airfoil \"0\" ( F C ):  "
      "\n Invalid response.  No action taken.\n";
  CHECK_EQ(out.str(), expected);
}

// polar slot list and selection '1' (oracle intx_pc.out): NPOL=1 suffix
// ' (F C 1)', slot contour as source, polar state unchanged.
TEST(unit, CoreIntxHandlerPolarSourceSelection) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  addPolarSlot(composition, "NACA 2412", 123);
  const std::size_t slotCountBefore = composition.polar.slots.size();

  std::istringstream in("1\nC\n0.25\nPOLMIX\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) == IntxCoreStatus::success);
  CHECK(out.str().find("  1  polar airfoil    " + pad48("NACA 2412") + "\n") !=
        std::string::npos);
  CHECK(out.str().find("Select source of airfoil \"0\" ( F C 1 ):  ") !=
        std::string::npos);
  CHECK_EQ(composition.geometry.name, std::string("POLMIX"));
  CHECK_EQ(composition.polar.slots.size(), slotCountBefore);
}

// polar index outside NPOL: invalid response (oracle intx_badpol.out).
TEST(unit, CoreIntxHandlerPolarIndexOutOfRange) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  addPolarSlot(composition, "NACA 2412", 123);
  std::istringstream in("5\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) ==
        IntxCoreStatus::invalidSelection);
  CHECK(out.str().find(" Invalid response.  No action taken.\n") !=
        std::string::npos);
}

// FRAC: an invalid number triggers no special message and the prompt
// repeats; an empty answer keeps default 0.5 (oracle intx_fracretry.out).
TEST(unit, CoreIntxHandlerFracRetryAndDefault) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  std::istringstream in("C\nC\nabc\n\nDEFNAME\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) == IntxCoreStatus::success);
  const std::string prompt =
      "Specify interpolating fraction  0...1   r>  ";
  std::size_t occurrences = 0;
  std::size_t pos = out.str().find(prompt);
  while (pos != std::string::npos) {
    ++occurrences;
    pos = out.str().find(prompt, pos + prompt.size());
  }
  CHECK_EQ(occurrences, std::size_t{2});
}

// Extrapolation (FRAC outside [0,1]) is canonically permitted.
TEST(unit, CoreIntxHandlerExtrapolationAccepted) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  std::istringstream in("C\nC\n-0.25\nEXTRAP\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) == IntxCoreStatus::success);
  CHECK_EQ(composition.geometry.name, std::string("EXTRAP"));
}

// empty result name: the name is blank (STRIP on an empty field), but
// the success and hint texts still appear.
TEST(unit, CoreIntxHandlerEmptyNameBlanksProfileName) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  std::istringstream in("C\nC\n0.5\n\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) == IntxCoreStatus::success);
  CHECK_EQ(composition.geometry.name, std::string(""));
  CHECK(out.str().find(" Result has been placed in buffer airfoil\n") !=
        std::string::npos);
}

// EOF at the name prompt: controlled abort; the buffer mutation that
// already happened stays (canonical order: name only after
// INTERX/GEOPAR), and the name stays unchanged.
TEST(unit, CoreIntxHandlerEofAtNameKeepsBufferMutation) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configureInviscid(composition);
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;
  std::istringstream in("C\nC\n0.5\n");
  std::ostringstream out;
  CHECK(handleIntxCore(composition, in, out) ==
        IntxCoreStatus::inputAborted);
  CHECK(composition.geometry.bufferRevision == revisionBefore + 1);
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  CHECK(out.str().find("Enter new airfoil name   s>  ") !=
        std::string::npos);
  CHECK(out.str().find("Result has been placed") == std::string::npos);
}

// Missing file: AREAD contract with open-error message and
// '*** LOAD NOT COMPLETED ***' (oracle intx_badfile.out); no
// buffer mutation.
TEST(unit, CoreIntxHandlerMissingFileReportsAreadContract) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  const std::uint64_t revisionBefore = composition.geometry.bufferRevision;
  std::istringstream in("F\ntests/data/definitely-missing-p7p1.dat\n");
  std::ostringstream out;
  const IntxCoreStatus status = handleIntxCore(composition, in, out);
  CHECK(status == IntxCoreStatus::fileOpenError ||
        status == IntxCoreStatus::readError);
  CHECK(out.str().find(
            " File OPEN error.  Nonexistent file:  "
            "tests/data/definitely-missing-p7p1.dat  \n") !=
        std::string::npos);
  CHECK(out.str().find(" *** LOAD NOT COMPLETED ***\n") !=
        std::string::npos);
  CHECK_EQ(composition.geometry.bufferRevision, revisionBefore);
}

// HALF: canonically silent, only a buffer mutation, name unchanged.
TEST(unit, CoreHalfHandlerSilentBufferOnly) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  std::ostringstream out;
  {
    std::ostringstream sink;
    CHECK(handleHalfCore(composition, sink) == HalfCoreStatus::applied);
  }
  CHECK_EQ(out.str(), std::string(""));
  CHECK_EQ(composition.geometry.buffer.x.size(), std::size_t{123});
  CHECK_EQ(composition.geometry.name, std::string("NACA 2412"));
  CHECK(composition.paneling.panelingValid);
  CHECK_EQ(composition.paneling.panel.x.size(), std::size_t{160});
}

// HALF without a buffer: bufferUnavailable, no mutation.
TEST(unit, CoreHalfHandlerWithoutBufferReportsUnavailable) {
  session::SessionComposition composition;
  std::ostringstream out;
  {
    std::ostringstream sink;
    CHECK(handleHalfCore(composition, sink) ==
           HalfCoreStatus::bufferUnavailable);
  }
  CHECK_EQ(out.str(), std::string(""));
  CHECK(!composition.geometry.valid);
}

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

// file: tests/unit/SessionPolarOpsTests.cpp
// unit tests for the polar session operations (xoper.f 978-1448,
// xpol.f 733-885). Pure in-memory semantics without file or prompt parts;
// the canonical 1-based indexing is checked here as 0-based.

#include "TestSupport.h"

#include "xfoil/core/polar/PolarTypes.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace xfoil;
using xfoil::session::SessionComposition;

core::polar::PolarDataset makePolar(const std::string& name,
                                    const std::vector<double>& alphas) {
  core::polar::PolarDataset polar;
  polar.metadata.airfoilName = name;
  for (double alpha : alphas) {
    core::polar::PolarPoint point;
    point.alpha = alpha;
    point.cl = alpha * 0.1;
    polar.points.push_back(point);
    polar.sideData.push_back({});
    polar.elementData.emplace_back();
  }
  return polar;
}

core::polar::ReferencePolarDataset makeReferencePolar(
    const std::string& label) {
  core::polar::ReferencePolarDataset reference;
  reference.label = label;
  core::polar::ReferencePolarPoint point;
  point.x = 1.0;
  point.y = 2.0;
  reference.blocks[0].push_back(point);
  return reference;
}

}  // namespace

TEST(unit, SessionPolarDeleteSlotShiftsAndDisablesActive) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0, 1.0}));
  session.polar.slots.push_back(makePolar("B", {2.0}));
  session.polar.slots.push_back(makePolar("C", {3.0}));
  session.polar.saveFileNames = {"fa", "fb", "fc"};
  session.polar.dumpFileNames = {"xa", "xb", "xc"};
  session.polar.activeSlot = 1;
  session.polar.accumulating = true;

  const auto removed = session::deletePolarSlot(session, 1);
  CHECK(removed.deleted);
  CHECK_EQ(session.polar.slots.size(), 2u);
  CHECK_EQ(session.polar.slots[0].metadata.airfoilName, std::string("A"));
  CHECK_EQ(session.polar.slots[1].metadata.airfoilName, std::string("C"));
  CHECK_EQ(session.polar.saveFileNames[1], std::string("fc"));
  CHECK(removed.accumulationDisabled);
  CHECK_EQ(removed.activeSlotAfter, -1);
  CHECK(!session.polar.accumulating);
  CHECK_EQ(removed.moves.size(), 1u);
  CHECK_EQ(removed.moves[0].first, 2);
  CHECK_EQ(removed.moves[0].second, 1);
}

TEST(unit, SessionPolarDeleteSlotKeepsActiveSlotOnShift) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0}));
  session.polar.slots.push_back(makePolar("B", {1.0}));
  session.polar.slots.push_back(makePolar("C", {2.0}));
  session.polar.saveFileNames = {"", "", ""};
  session.polar.dumpFileNames = {"", "", ""};
  session.polar.activeSlot = 2;
  session.polar.accumulating = true;

  const auto removed = session::deletePolarSlot(session, 0);
  CHECK(removed.deleted);
  CHECK(!removed.accumulationDisabled);
  CHECK_EQ(removed.activeSlotAfter, 1);
  CHECK_EQ(session.polar.activeSlot, 1);
  CHECK_EQ(removed.moves.size(), 2u);
}

TEST(unit, SessionPolarDeleteSlotInvalidIndexNoMutation) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0}));
  session.polar.saveFileNames = {"fa"};
  const auto removed = session::deletePolarSlot(session, 5);
  CHECK(!removed.deleted);
  CHECK_EQ(session.polar.slots.size(), 1u);
}

TEST(unit, SessionPolarDeleteAllSlots) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0}));
  session.polar.saveFileNames = {"fa"};
  session.polar.activeSlot = 0;
  session.polar.accumulating = true;
  session::deleteAllPolarSlots(session);
  CHECK_EQ(session.polar.slots.size(), 0u);
  CHECK_EQ(session.polar.saveFileNames.size(), 0u);
  CHECK_EQ(session.polar.activeSlot, -1);
  CHECK(!session.polar.accumulating);
}

TEST(unit, SessionPolarSortSlotByAlpha) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {2.0, -1.0, 0.5}));
  const auto status = session::sortPolarSlotByAlpha(session, 0);
  CHECK(status.isOk());
  const auto& points = session.polar.slots[0].points;
  CHECK_NEAR(points[0].alpha, -1.0, 1e-12, 1e-12);
  CHECK_NEAR(points[1].alpha, 0.5, 1e-12, 1e-12);
  CHECK_NEAR(points[2].alpha, 2.0, 1e-12, 1e-12);
  CHECK(session::sortPolarSlotByAlpha(session, 3).isErr());
}

TEST(unit, SessionPolarRenameSlot) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("OLD", {0.0}));
  CHECK(session::renamePolarSlot(session, 0, "NEW").isOk());
  CHECK_EQ(session.polar.slots[0].metadata.airfoilName, std::string("NEW"));
  CHECK(session::renamePolarSlot(session, 1, "X").isErr());
}

TEST(unit, SessionPolarRemovePointsToleranceAndElimination) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0, 1.0, 2.0}));
  session.polar.slots.push_back(makePolar("B", {5.0}));
  session.polar.saveFileNames = {"fa", "fb"};
  session.polar.dumpFileNames = {"xa", "xb"};
  session.polar.activeSlot = 1;
  session.polar.accumulating = true;

  // 3-digit tolerance 5e-4 (xoper.f 1357): 1.0004 hits, 1.001 not.
  auto result = session::removePolarPoints(session, 0, {1.0004, 1.001});
  CHECK_EQ(result.removedPoints, 1);
  CHECK(!result.polarEliminated);
  CHECK_EQ(session.polar.slots[0].points.size(), 2u);

  // Remove all points -> the slot is eliminated, IPACT shifts (xoper.f
  // 1372-1386), processing ends.
  result = session::removePolarPoints(session, 0, {0.0, 2.0, 9.9});
  CHECK(result.polarEliminated);
  CHECK_EQ(result.removedPoints, 2);
  CHECK_EQ(session.polar.slots.size(), 1u);
  CHECK_EQ(session.polar.slots[0].metadata.airfoilName, std::string("B"));
  CHECK_EQ(result.activeSlotAfter, 0);
  CHECK_EQ(session.polar.activeSlot, 0);
  CHECK(session.polar.accumulating);
}

TEST(unit, SessionPolarCapacityBoundaryNpx) {
  // NPX boundary values: 12 (old limit), 13/19 (between), 20 (new limit);
  // the next append is rejected without partial mutation.
  CHECK_EQ(core::polar::maxPolarSlots, std::size_t{20});
  for (const std::size_t n : {std::size_t{12}, std::size_t{13},
                              std::size_t{19}, std::size_t{20}}) {
    SessionComposition session;
    for (std::size_t i = 0; i < n; ++i) {
      const auto added =
          session::appendPolarSlot(session, makePolar("P", {0.0}), "f");
      CHECK(!added.capacityExceeded);
    }
    CHECK_EQ(session.polar.slots.size(), n);
    const auto next =
        session::appendPolarSlot(session, makePolar("Q", {}), "g");
    if (n < core::polar::maxPolarSlots) {
      CHECK(!next.capacityExceeded);
      CHECK_EQ(session.polar.slots.size(), n + 1);
    } else {
      CHECK(next.capacityExceeded);
      CHECK_EQ(session.polar.slots.size(), n);
    }
  }
}

TEST(unit, SessionPolarIncludeToggles) {
  SessionComposition session;
  CHECK(!session.polar.includeCpMin);
  session::togglePolarIncludeCpMin(session);
  CHECK(session.polar.includeCpMin);
  session::togglePolarIncludeCpMin(session);
  CHECK(!session.polar.includeCpMin);
  session::togglePolarIncludeHingeMoment(session);
  CHECK(session.polar.includeHingeMoment);
}

TEST(unit, SessionPolarAppendAndCapacity) {
  SessionComposition session;
  for (std::size_t i = 0; i < core::polar::maxPolarSlots; ++i) {
    const auto added =
        session::appendPolarSlot(session, makePolar("P", {0.0}), "f");
    CHECK(!added.capacityExceeded);
    CHECK_EQ(added.slotIndex, static_cast<int>(i));
  }
  const auto overflow = session::appendPolarSlot(session, makePolar("Q", {}), "g");
  CHECK(overflow.capacityExceeded);
  CHECK_EQ(session.polar.slots.size(), core::polar::maxPolarSlots);

  session::setPolarSaveName(session, 0, "new");
  CHECK_EQ(session.polar.saveFileNames[0], std::string("new"));

  for (std::size_t i = 0; i < core::polar::maxPolarSlots; ++i) {
    const auto added =
        session::appendReferenceSlot(session, makeReferencePolar("R"));
    CHECK(!added.capacityExceeded);
  }
  const auto refOverflow =
      session::appendReferenceSlot(session, makeReferencePolar("S"));
  CHECK(refOverflow.capacityExceeded);

  const auto removed = session::deleteReferenceSlot(session, 0);
  CHECK(removed.deleted);
  CHECK_EQ(removed.moves.size(), core::polar::maxPolarSlots - 1);
  CHECK_EQ(session.polar.referenceSlots.size(),
           core::polar::maxPolarSlots - 1);
  session::deleteAllReferenceSlots(session);
  CHECK_EQ(session.polar.referenceSlots.size(), 0u);
}

TEST(unit, PolarElementMetadataSessionDeleteCompaction) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0}));
  session.polar.slots.push_back(makePolar("B", {1.0}));
  session.polar.slots.push_back(makePolar("C", {2.0}));
  session.polar.slots[0].elementData[0][0].cx = 10.0;
  session.polar.slots[1].elementData[0][0].cx = 20.0;
  session.polar.slots[2].elementData[0][0].cx = 30.0;
  session.polar.saveFileNames = {"fa", "fb", "fc"};
  session.polar.dumpFileNames = {"xa", "xb", "xc"};
  session.polar.activeSlot = 2;
  session.polar.accumulating = true;

  const auto removed = session::deletePolarSlot(session, 1);
  CHECK(removed.deleted);
  CHECK_EQ(session.polar.slots.size(), 2u);
  CHECK_EQ(session.polar.slots[0].elementData[0][0].cx, 10.0);
  CHECK_EQ(session.polar.slots[1].elementData[0][0].cx, 30.0);
  CHECK_EQ(session.polar.activeSlot, 1);
}

TEST(unit, PolarElementMetadataPointRemovalKeepsParallelFields) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0, 1.0, 2.0}));
  for (std::size_t i = 0; i < 3; ++i) {
    session.polar.slots[0].elementData[i][0].cx =
        10.0 * static_cast<double>(i + 1);
  }

  const auto result = session::removePolarPoints(session, 0, {1.0});
  CHECK_EQ(result.removedPoints, 1);
  CHECK(!result.polarEliminated);
  CHECK_EQ(session.polar.slots[0].points.size(), 2u);
  CHECK_EQ(session.polar.slots[0].elementData.size(), 2u);
  CHECK_EQ(session.polar.slots[0].elementData[0][0].cx, 10.0);
  CHECK_EQ(session.polar.slots[0].elementData[1][0].cx, 30.0);
}

TEST(unit, PolarElementMetadataSlotTwentyAndTwentyOne) {
  SessionComposition session;
  session.polar.elementSelection.count = 2;
  session.polar.elementSelection.columns[0] = core::polar::PolarElementQuantity::cx;
  session.polar.elementSelection.columns[1] =
      core::polar::PolarElementQuantity::cdVisc;

  for (std::size_t i = 0; i < core::polar::maxPolarSlots; ++i) {
    auto polar = makePolar("P", {static_cast<double>(i)});
    polar.elementData[0][4].cdVisc = static_cast<double>(i);
    const auto added = session::appendPolarSlot(session, std::move(polar), "f");
    CHECK(!added.capacityExceeded);
    CHECK_EQ(added.slotIndex, static_cast<int>(i));
  }
  CHECK_EQ(session.polar.slots.size(), core::polar::maxPolarSlots);
  CHECK_EQ(session.polar.slots[19].elementData[0][4].cdVisc, 19.0);

  auto overflow = makePolar("Q", {21.0});
  overflow.elementData[0][4].cdVisc = 99.0;
  const auto rejected =
      session::appendPolarSlot(session, std::move(overflow), "g");
  CHECK(rejected.capacityExceeded);
  CHECK_EQ(session.polar.slots.size(), core::polar::maxPolarSlots);
  CHECK_EQ(session.polar.slots[19].elementData[0][4].cdVisc, 19.0);
  CHECK_EQ(session.polar.elementSelection.count, std::size_t{2});
}

TEST(unit, PolarElementMetadataResetKeepsOtherSessionData) {
  SessionComposition session;
  session.polar.slots.push_back(makePolar("A", {0.0}));
  session.polar.slots[0].elementData[0][0].cy = 3.5;
  session.polar.saveFileNames = {"fa"};
  session.polar.dumpFileNames = {"xa"};
  session.polar.activeSlot = 0;
  session.polar.accumulating = true;
  session.polar.elementSelection.count = 1;
  session.polar.elementSelection.columns[0] =
      core::polar::PolarElementQuantity::cm;
  session.geometry.name = "keep-name";
  session.operating.maximumIterations = 77;

  session::deleteAllPolarSlots(session);
  CHECK(session.polar.slots.empty());
  CHECK(session.polar.saveFileNames.empty());
  CHECK(session.polar.dumpFileNames.empty());
  CHECK_EQ(session.polar.activeSlot, -1);
  CHECK(!session.polar.accumulating);
  CHECK_EQ(session.polar.elementSelection.count, std::size_t{1});
  CHECK(session.polar.elementSelection.columns[0].has_value());
  CHECK(session.polar.elementSelection.columns[0].value() ==
        core::polar::PolarElementQuantity::cm);
  CHECK_EQ(session.geometry.name, std::string("keep-name"));
  CHECK_EQ(session.operating.maximumIterations, 77);
}

TEST(unit, PolarElementMetadataIndependentSessions) {
  SessionComposition first;
  SessionComposition second;
  first.polar.slots.push_back(makePolar("A", {0.0}));
  second.polar.slots.push_back(makePolar("B", {1.0}));
  first.polar.slots[0].elementData[0][0].cx = 1.5;
  second.polar.slots[0].elementData[0][0].cx = 2.5;
  first.polar.elementSelection.count = 1;
  first.polar.elementSelection.columns[0] =
      core::polar::PolarElementQuantity::cy;

  CHECK_EQ(second.polar.slots[0].elementData[0][0].cx, 2.5);
  CHECK_EQ(second.polar.elementSelection.count, std::size_t{0});
  CHECK(!second.polar.elementSelection.columns[0].has_value());
}

TEST(unit, PolarElementMetadataSelectionDefinition) {
  SessionComposition session;
  const auto& selection = session.polar.elementSelection;
  CHECK_EQ(selection.count, std::size_t{0});
  for (const auto& column : selection.columns) {
    CHECK(!column.has_value());
  }
  CHECK_EQ(static_cast<int>(core::polar::PolarElementQuantity::cx), 1);
  CHECK_EQ(static_cast<int>(core::polar::PolarElementQuantity::cy), 2);
  CHECK_EQ(static_cast<int>(core::polar::PolarElementQuantity::cm), 3);
  CHECK_EQ(static_cast<int>(core::polar::PolarElementQuantity::cdVisc), 4);
}

// End: tests/unit/SessionPolarOpsTests.cpp

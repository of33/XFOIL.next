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

// File: include/xfoil/core/polar/PolarOps.h
// Deterministic polar operations, ported directly from
// fortran-src/xpol.f and fortran-src/xoper.f.
//
// Contains no file, path, stream, prompt, session or
// CLI dependency. Conflicts are modeled as structured results,
// not resolved by prompting.

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "xfoil/core/Result.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Transformations.h"
#include "xfoil/core/polar/PolarTypes.h"

namespace xfoil::core::polar {

// ---
// PLRADD (xpol.f:550) - point insertion without file update.
// ---
// Canonical input quantities of the active analysis state.
struct PolarPointInput {
  double alphaDeg{0.0};   // ADEG
  double cl{0.0};         // CL
  double cd{0.0};         // Cd (viscous overall)
  double cm{0.0};         // CM
  double cdPres{0.0};     // CDP
  double cpMin{0.0};      // CPMN
  double mach{0.0};       // MINF
  bool viscous{false};    // LVISC
  double reInf{0.0};      // REINF
  std::array<double, sideCount> acrit{};   // ACRIT(1..2)
  std::array<double, sideCount> xtrip{};   // XSTRIP(1..2)
  std::array<double, sideCount> xoctr{};   // XOCTR(1..2)
  std::array<double, sideCount> tindex{};  // TINDEX(1..2)
  double hingeMoment{0.0};  // HMOM for LFLAP, otherwise 0
};

// Adds a point per PLRADD. For a full polar (NAX=800)
// capacityExceeded is returned without any partial mutation.
[[nodiscard]] Status addPoint(PolarDataset& polar,
                              const PolarPointInput& input);

// ---
// PLRSRT (xpol.f:733) - sorting with canonical permutation.
// ---
// Sort key corresponding to IDSORT (PINDEX columns).
enum class SortKey {
  alpha, cl, cd, cm, cdWave, cdVisc, cdPres, mach, re, hinge, cpMin, cdh,
  cmDot,
};

// Sorts points and side data by the key.
// Uses the validated routines hSort/aSort unchanged.
[[nodiscard]] Status sortPoints(PolarDataset& polar, SortKey key);

// ---
// PLRCOP (xpol.f:838) / PRFCOP (xpol.f:885) - slot copies.
// ---
// Complete copy of src to dst (header, points, side data,
// contour). Self-copy is allowed and leaves the state unchanged.
[[nodiscard]] Status copySlot(const PolarDataset& source,
                              PolarDataset& destination);

[[nodiscard]] Status copyReferenceSlot(const ReferencePolarDataset& source,
                                       ReferencePolarDataset& destination);

// ---
// NAMMOD (xoper.f:1831) - deterministic string kernel for
// bracket version counting. The READ(*) fallback for read errors is
// omitted; here the read error is returned as a structured
// result.
// ---
// Canonical semantics: an existing bracket [n] is increased by kDel
// (MOD 100, Fortran sign); without a bracket, kMod0 is appended
// if kMod0 > 0.
[[nodiscard]] Result<std::string> applyVersionTag(const std::string& name,
                                                  int kDel, int kMod0);

// ---
// PLRSUM (xpol.f:755) - Re mantissa/exponent codec (without stdout).
// ---
struct ReEncoding {
  double mantissa{0.0};
  int exponent{0};
};

// IEXP = INT(LOG10(Re)) clamp 0..9, RMAN = Re / 10^IEXP; Re <= 0 returns
// mantissa 0 (Fortran leaves IEXP unset in this branch; C++ defines
// it as 0 - documented safety deviation).
[[nodiscard]] ReEncoding encodeReMantissaExponent(double re);

// ---
// PLRINI (xpol.f:180) - header/parameter comparison (without ASKS/ASKL).
// ---
// Active session parameters that PLRSET compares or adopts.
struct SessionParameters {
  std::string name;                       // NAME
  ReType reType{ReType::fixed};           // RETYP
  MachType machType{MachType::fixed};     // MATYP
  double minf1{0.0};                      // MINF1
  double reinf1{0.0};                     // REINF1
  std::array<double, sideCount> acrit{};  // ACRIT(1..2)
  std::array<double, sideCount> xtrip{};  // XSTRIP(1..2)
  bool viscous{false};                    // LVISC
};

struct SaveHeaderComparison {
  bool nameDiffers{false};
  bool reDiffers{false};
  bool machDiffers{false};
  bool reTypeDiffers{false};
  bool machTypeDiffers{false};
  bool ncritDiffers{false};
  bool xtripDiffers{false};

  [[nodiscard]] bool matches() const noexcept {
    return !(nameDiffers || reDiffers || machDiffers || reTypeDiffers ||
             machTypeDiffers || ncritDiffers || xtripDiffers);
  }
};

// Compares file header data with current session parameters according to
// the PLRINI comparison list (NAME, REYNP1, MACHP1, IRETYP, IMATYP,
// ACRITP(1..2), XSTRIPP(1..2)).
[[nodiscard]] SaveHeaderComparison compareSaveHeader(
    const PolarMetadata& fileHeader, const SessionParameters& current);

// ---
// PLXINI (xpol.f:383) - dump-header comparison (without ASKS/ASKL).
// ---
// Decoded canonical dump header (PLXINI read order).
struct DumpHeader {
  std::string name;                       // NAMEX
  std::string code;                       // 'XFOIL '
  double version{0.0};                    // VERSION
  double mach{0.0};                       // MACHX
  double re{0.0};                         // REYNX (already *1e6)
  std::array<double, sideCount> ncrit{};  // ACRITX(1..2)
  int matyp{0};                           // MATYPX
  int retyp{0};                           // RETYPX
  std::array<int, 4> indices{};           // IIX, ILEX, ITEX, IIBX
  std::vector<double> x;                  // Contour X(1..N)
  std::vector<double> y;                  // Contour Y(1..N)
};

struct DumpHeaderComparison {
  bool nameDiffers{false};
  bool reDiffers{false};
  bool machDiffers{false};
  bool reTypeDiffers{false};
  bool machTypeDiffers{false};
  bool ncritDiffers{false};

  [[nodiscard]] bool matches() const noexcept {
    return !(nameDiffers || reDiffers || machDiffers || reTypeDiffers ||
             machTypeDiffers || ncritDiffers);
  }
};

// Comparison according to the PLXINI comparison list (NAMEX, REYNX, MACHX,
// RETYPX, MATYPX, ACRITX(1..2)).
[[nodiscard]] DumpHeaderComparison compareDumpHeader(
    const DumpHeader& fileHeader, const PolarMetadata& current);

// ---
// PLRSET (xpol.f:22) - slot initialization and geometry comparison
// (without ASKI/stdout).
// ---
struct SlotSetupResult {
  enum class Kind {
    newSlot,                 // new slot created from session parameters
    adoptStoredParameters,   // parameters of the stored polar adopted
    geometryConflict,        // contour differs (ASKI path)
  };
  Kind kind{Kind::newSlot};
  PolarDataset slot;          // for newSlot: initialized empty slot
  SessionParameters adopted;  // for adoptStoredParameters: adopted values
};

// existing == nullptr corresponds to the Fortran branch "new polar slot".
// The geometry comparison uses the canonical threshold
// DSFRAC = sqrt(dx^2+dy^2)/(S(N)-S(1)) <= 1e-5 for all points.
[[nodiscard]] SlotSetupResult prepareAppendSlot(
    const PolarDataset* existing, const SessionParameters& current,
    const PolarGeometrySnapshot& currentGeometry);

// ---
// APCOPY (xpol.f:139) - geometry reconstruction from the
// co-archived contour, exclusively with validated core kernels.
// ---
struct GeometryReconstruction {
  std::size_t n{0};
  std::vector<double> x, y, s, xp, yp;
  std::vector<double> nx, ny;  // Fortran NX/YN normal arrays
  double sle{0.0};
  double xle{0.0}, yle{0.0};
  double xte{0.0}, yte{0.0};
  double chord{0.0};
  geometry::TecalcResult te{};
  std::vector<double> apanel;
  bool resetBoundaryLayerInit{false};  // LBLINI reset for N change
};

// teInputs contains the canonical required final TE/GAM values of the
// active panel state (GAM(1),GAM(N),GAM_A(1),GAM_A(N)); APCOPY passes
// them on to TECALC. oldN corresponds to Fortran NOLD.
[[nodiscard]] Result<GeometryReconstruction> restoreArchivedGeometry(
    const PolarGeometrySnapshot& snapshot, std::size_t oldN,
    const geometry::TrailingEdgeInputs& teInputs);

}  // namespace xfoil::core::polar

// End: include/xfoil/core/polar/PolarOps.h

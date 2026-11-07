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

// File: include/xfoil/core/polar/PolarTypes.h
// Explicit, 0-based polar and reference-polar value types.
//
// Maps the canonical Fortran storage structure (fortran-src/XFOIL.INC,
// fortran-src/PINDEX.INC) onto domain-specific named fields:
//
// CPOL(IA,IAL..ICMDOT,IP) IA 1..NAX -> points[i] with i 0..NAX-1,
// columns as named fields
// CPOLSD(IA,IS,JNC..JTI,IP) IS 1..ISX -> sideData[i][s] with s 0..1,
// JP as named fields
// CPOLXY(I,1..2,IP) I 1..NXYPOL -> geometry.x/y with i 0..N-1
// CPOLREF(IA,IS,K,IR) K 1..4 -> blocks[k] with k 0..3
//
// No filenames, streams, paths, prompts, session flags, or plot states.
// No 1-based containers, no owning raw pointers.

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "xfoil/core/Limits.h"

namespace xfoil::core::polar {

// PINDEX.INC: IRETYP/IMATYP codes (1 fixed, 2 ~1/sqrt(CL), 3 ~1/CL).
enum class ReType { fixed = 1, invSqrtCl = 2, invCl = 3 };
enum class MachType { fixed = 1, invSqrtCl = 2, invCl = 3 };

// Fortran CPOL(IA,ID,IP), ID = IAL..ICMDOT (PINDEX.INC).
// All columns are domain-specific named; the canonical field order
// corresponds to the column order IAL,ICL,ICD,ICM,ICW,ICV,ICP,IMA,IRE,
// I,IMC,ICDH,ICMDOT.
struct PolarPoint {
  double alpha{0.0};    // IAL alpha (degrees)
  double cl{0.0};       // ICL CL
  double cd{0.0};       // ICD Cd (overall, viscous)
  double cm{0.0};       // ICM CM
  double cdWave{0.0};   // ICW CDw
  double cdVisc{0.0};   // ICV CDv
  double cdPres{0.0};   // ICP CDp
  double mach{0.0};     // IMA Mach
  double re{0.0};       // IRE Re
  double hinge{0.0};    // I Chinge
  double cpMin{0.0};    // IMC Cpmin
  double cdh{0.0};      // ICDH CDh
  double cmDot{0.0};    // ICMDOT Cmdot
};

// Number of canonical polar columns (PINDEX.INC: IPTOT=13).
inline constexpr std::size_t polarColumnCount = 13;

// Fortran CPOLSD(IA,IS,JP,IP), JP = JNC..JTI (PINDEX.INC: JPTOT=4).
struct PolarSideData {
  double ncrit{0.0};  // JNC Ncrit
  double xtrip{0.0};  // JTP trip
  double xtr{0.0};    // JTN transition
  double itr{0.0};    // JTI transition index
};

// Number of canonical side-data columns (PINDEX.INC: JPTOT=4).
inline constexpr std::size_t sideColumnCount = 4;

// Polar header data: canonical parameters of a stored polar.
struct PolarMetadata {
  std::string airfoilName;            // NAMEPOL(IP)
  std::string code{"XFOIL"};          // CODEPOL(IP)
  double version{6.99};               // VERSPOL(IP)
  ReType reType{ReType::fixed};       // IRETYP(IP)
  MachType machType{MachType::fixed}; // IMATYP(IP)
  double reCl1{0.0};                  // REYNP1(IP)
  double machCl1{0.0};                // MACHP1(IP)
  std::array<double, sideCount> ncrit{};   // ACRITP(1..2,IP)
  std::array<double, sideCount> xtrip{};   // XSTRIPP(1..2,IP)
  double ptrat{0.0};                  // PTRATP(IP)
  double etap{0.0};                   // ETAPP(IP)
  int elementCount{1};                // NBLP(IP)
};

// Fortran CPOLXY(I,1..2,IP), I = 1..NXYPOL(IP): archived contour.
struct PolarGeometrySnapshot {
  std::vector<double> x;
  std::vector<double> y;

  [[nodiscard]] std::size_t size() const noexcept {
    return x.size();
  }
};

// A stored polar (corresponds to slot IP, 0-based).
struct PolarDataset {
  PolarMetadata metadata;
  std::vector<PolarPoint> points;
  // points[i].side[s]: CPOLSD(IA=i+1, IS=s+1, JP, IP).
  std::vector<std::array<PolarSideData, sideCount>> sideData;
  PolarGeometrySnapshot geometry;
};

// Fortran CPOLREF(IA,IS,K,IR): reference polar point (x value, y value).
struct ReferencePolarPoint {
  double x{0.0};
  double y{0.0};
};

// Number of reference-polar blocks (POLREF reads 4 blocks).
inline constexpr std::size_t referenceBlockCount = 4;

// A reference-polar slot IR (0-based): LABREF(IR) + 4 blocks.
struct ReferencePolarDataset {
  std::string label;
  std::array<std::vector<ReferencePolarPoint>, referenceBlockCount> blocks;
};

// Canonical capacities (XFOIL.INC: NAX=800, NPX=12, NFX=128).
inline constexpr std::size_t maxPolarPoints = polarPointCapacity;
inline constexpr std::size_t maxPolarSlots = polarSetCapacity;
inline constexpr std::size_t maxReferencePoints = referencePointCapacity;

static_assert(maxPolarPoints == 800);
static_assert(maxPolarSlots == 12);
static_assert(maxReferencePoints == 128);

}  // namespace xfoil::core::polar

// End: include/xfoil/core/polar/PolarTypes.h

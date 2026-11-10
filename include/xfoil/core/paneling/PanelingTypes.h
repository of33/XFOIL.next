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

// File: include/xfoil/core/paneling/PanelingTypes.h
// Value types of the paneling kernels of the new 0-based cores,
// ported from fortran-src/xfoil.f (GETPAN, PANGEN),
// fortran-src/xgdes.f (ABCOPY) and fortran-src/xpanel.f (APCALC, NCALC).
//
// - PanelingParameters: paneling parameters (Fortran NPAN, CVPAR,
// CTERAT, CTRRAT, XSREF1/2, XPREF1/2) with the INIT defaults
// (xfoil.f 6.996 INIT: 240, 1.0, 0.3, 0.2, 1.0/1.0 each).
// - Panel parameter branch: GETPAN parameter branching without menu/prompt.
// - ParameterChangeResult: new parameter state with clamp diagnostics.
// - PanelGeometry: current panel contour (Fortran X,XP,Y,YP,S,N,SLE,
// XLE,YLE,XTE,YTE,CHORD,SHARP plus normals NX/NY and panel angle
// APANEL), 0-based.
// - PangenResult: PANGEN initial group incl. structured diagnostics.
// - AbcopyResult: ABCOPY initial group (buffer -> current contour).
//
// Capacity limits (canonical, separate; derived from core IQX=601):
// - kMaxPanelingPanelCount = IQX-6 = 595: exclusively the GETPAN/PPAR
//   input boundary (Fortran NPAN > IQX-6 -> NPAN = IQX-6, xfoil.f:2218-2219;
//   only occurrence of IQX-6). It does not apply to the direct
//   PANGEN call.
// - kMaxAbcopyNodeCount = IQX-5 = 596: ABCOPY boundary (Fortran
//   NB > IQX-5 -> current contour is not set).
// - kMaxPanelNodeCount = IQX-1 = 600: independent PANGEN safety limit
//   including corner-point insertion (Fortran STOP 'PANEL: Too many
//   panels...' after N = N+1, xfoil.f:2025; only occurrence of IQX-1).
// The pure PANGEN kernel processes NPAN=596..600 directly within the
// 6*IQX Newton arrays, 5*IBX SNEW storage and IZX=IQX+IWX coordinate
// arrays. Above 600 no canonical safety semantics exists.

#pragma once

#include <array>
#include <vector>

#include "xfoil/core/Limits.h"
#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/math/Diagnostic.h"

namespace xfoil::core::paneling {

// GETPAN/PPAR input boundary (Fortran NPAN > IQX-6 -> NPAN = IQX-6,
// xfoil.f:2218-2219). Does not apply to the direct PANGEN call.
inline constexpr int kMaxPanelingPanelCount =
    static_cast<int>(surfaceSystemCapacity - 6);  // 595

// ABCOPY boundary (Fortran NB > IQX-5 -> current contour is not set).
inline constexpr int kMaxAbcopyNodeCount =
    static_cast<int>(surfaceSystemCapacity - 5);  // 596

// Independent PANGEN safety limit including corner-point insertion
// (Fortran STOP after N = N+1, xfoil.f:2025). Above 600 no
// canonical safety semantics exists.
inline constexpr int kMaxPanelNodeCount =
    static_cast<int>(surfaceSystemCapacity - 1);  // 600

// Paneling parameters. Defaults are the Fortran INIT values
// (xfoil.f 6.996 INIT: NPAN=240, CVPAR=1.0, CTERAT=0.3, CTRRAT=0.2,
// XSREF1/2=1.0, XPREF1/2=1.0).
struct PanelingParameters {
  int panelCount{240};                     // Fortran NPAN
  double bunchingParam{1.0};               // Fortran CVPAR
  double teToLeDensityRatio{0.3};          // Fortran CTERAT
  double refinementDensityRatio{0.2};      // Fortran CTRRAT
  std::array<double, 2> upperRefinementRange{1.0, 1.0};  // XSREF1, XSREF2
  std::array<double, 2> lowerRefinementRange{1.0, 1.0};  // XPREF1, XPREF2
};

// GETPAN parameter branching without menu/prompt code.
enum class PanelParameterKind {
  panelCount,              // Fortran 'N'
  bunchingParam,           // Fortran 'P'
  teToLeDensityRatio,      // Fortran 'T'
  refinementDensityRatio,  // Fortran 'R'
  upperRefinementRange,    // Fortran 'XT' (2 values)
  lowerRefinementRange,    // Fortran 'XB' (2 values)
};

// GETPAN initial group: new parameter state with clamp diagnostics
// (Fortran WRITE of the 1200 formats is structured).
struct ParameterChangeResult {
  PanelingParameters parameters;
  bool clamped{false};  // true: panelCount was limited to 595
};

// Current panel contour of the new cores (Fortran X,XP,Y,YP,S,N,SLE,
// XLE,YLE,XTE,YTE,CHORD,SHARP, NX,NY,APANEL). The length invariant
// x/y/s/xp/yp/xn/yn.size==n and apanel.size==n is checked in make;
// a violation throws std::invalid_argument without partial state.
struct PanelGeometry {
  std::vector<double> x;
  std::vector<double> y;
  std::vector<double> s;
  std::vector<double> xp;
  std::vector<double> yp;
  std::vector<double> xn;    // panel normals (Fortran NX, NY)
  std::vector<double> yn;
  std::vector<double> apanel;  // panel angle (Fortran APANEL; panel n-1 = TE)
  double sle{0.0};
  double xle{0.0};
  double yle{0.0};
  double xte{0.0};
  double yte{0.0};
  double chord{0.0};
  bool sharpTe{false};

  // Factory with length-invariant check (x/y/s/xp/yp/xn/yn equal length,
  // apanel equal length). Violations throw std::invalid_argument.
  [[nodiscard]]
  static PanelGeometry make(std::vector<double> x, std::vector<double> y,
                            std::vector<double> s, std::vector<double> xp,
                            std::vector<double> yp, std::vector<double> xn,
                            std::vector<double> yn,
                            std::vector<double> apanel, double sle,
                            double xle, double yle, double xte, double yte,
                            double chord, bool sharpTe);
};

// PANGEN initial group: panel contour plus structured diagnostics
// (Fortran WRITEs are not written to stdout).
struct PangenResult {
  PanelGeometry panel;
  std::vector<math::Diagnostic> diagnostics;  // e.g. Newton limit
  double teGap{0.0};                          // blunt-edge gap (GAP)
  double dsminRatio{0.0};                     // Fortran DSMIN
  double dsmaxRatio{0.0};                     // Fortran DSMAX
  int cornersInserted{0};                     // Fortran NCORN
  bool sharpLeadingEdge{false};               // Fortran IBLE != 0
};

// ABCOPY initial group: current panel contour from the buffer contour
// (duplicated points removed, as in the Fortran). leStatus carries the
// canonical LEFIND completion status (limitReached == Fortran
// 'LEFIND: LE point not found. Continuing...'); the kernel does not
// write to stdout.
struct AbcopyResult {
  PanelGeometry panel;
  geometry::LeStatus leStatus{geometry::LeStatus::completed};
};

}  // namespace xfoil::core::paneling

// End: include/xfoil/core/paneling/PanelingTypes.h

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

// File: src/app/cli/CoreInverseDesignHandlers.h
// MDES menu handlers:
// TOP MDES -> handleMdesCore, TOP QDES -> handleQdesCore.
//
// The handler runs the menu loop of SUBROUTINE MDES
// (fortran-src/xmdes.f 30-720): entry via the numerical label-1 path
// (exclusively session::initializeMdesNumerics), prompt
// '.MDES c>'/'.MDESs c>', help '?', unknown command format 1100,
// '<Return>' exit, '!' redo via COMOLD/ARGOLD (separate
// MDES redo state), EOF as controlled session termination.
//
// Commands: INIT, QSET, AQ, CQ, SYMM/S, TGAP, TANG, FILT/F, SMOO, SLOP,
// VISC/V, REFL are executed exclusively via the session use cases
// from SessionOps.h. No numerics in the handler, no direct
// session-field mutation, no plot backend.
//
// Plot/cursor commands (PLOT/P, SPEC, BLOW/B, RESE/R, WIND/W, SIZE,
// ANNO, HARD, Z, U, MARK, MODI/M) remain recognized-but-unavailable
// ('Plotting is not available in this build.'). READ/DUMP,
// EXEC/X and PERT (PERT jumps canonically into EXEC, xmdes.f
// 701-708) are implemented in the handler.

#pragma once

#include <iosfwd>

#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli {

enum class MdesCoreStatus {
  returned,   // '<Return>' exit to the top level
  endOfFile,  // input stream at end (canonical READ abort;
              // controlled handling as in the remaining ASK paths)
};

enum class QdesCoreStatus {
  returned,
  endOfFile,
};

MdesCoreStatus handleMdesCore(xfoil::session::SessionComposition& session,
                              std::istream& in, std::ostream& out);

// QDES menu (SUBROUTINE QDES, xqdes.f 40-540),
// (TOP QDES -> handleQdesCore).
// The handler runs the menu loop (entry with SPECAL
// initialization exclusively via session::initializeQdesNumerics,
// prompt '.QDES c>', help format 1050, unknown command format
// 1100, '<Return>' exit, '!' redo via the separate
// QDES redo state, EOF controlled) and calls for QSET, SMOO, VISC/
// V, REFL, SLOP, CPXX exclusively the session use cases from
// SessionOps.h. EXEC/X canonically shows the LIQSET refusal
// ('*** Must mark off target segment first ***'); with a set
// target segment EXEC remains inactive without partial activation. REST
// (xqdes.f 508-530) mutates X/Y from XB/YB (geometry commit) and
// stays recognized but unexecuted. Plot/cursor
// commands remain recognized-but-unavailable; ANNO holds the
// canonical 'No active plot to annotate' branch (without a plot
// backend LPLOT is always .FALSE.). No numerics in the handler.
QdesCoreStatus handleQdesCore(xfoil::session::SessionComposition& session,
                              std::istream& in, std::ostream& out);

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreInverseDesignHandlers.h

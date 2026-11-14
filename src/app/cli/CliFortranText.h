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

// File: src/app/cli/CliFortranText.h
// Emitter precursor: reproduces the visible
// Fortran text lines exclusively from structured core/session results.
// Header-only, so that both the
// command interpreter and the unit tests use the same source.
// The byte-exact output preserves the canonical Fortran-interface text
// (ABCOPY confirmation format 1200, PANGEN convergence/capacity
// messages) and emitDiagnostics (appended '\n' for a missing line end).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include "xfoil/core/boundary_layer/BoundaryLayerTypes.h"
#include "xfoil/core/boundary_layer/Marching.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/paneling/PanelingTypes.h"
#include "xfoil/io/TextFormat.h"

namespace xfoil::app::cli {

// Fortran Fw.d: fixed-value output; if the value does not fit the width,
// Fortran writes only '*' (verified with gfortran). Identical to the
// file-local helper of the productive CommandInterpreter; here it is
// provided for the core handlers in the CliFortranText layer.
[[nodiscard]]
inline std::string formatFortranF(int width, int decimals, double value) {
  if (!std::isfinite(value)) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(decimals) << std::setw(width)
         << value;
  std::string text = stream.str();
  if (static_cast<int>(text.size()) > width) {
    // Fortran drops the leading zero if the field would
    // otherwise overflow (F8.6 of -0.014939 -> '-.014939').
    std::string trimmed = text;
    if (trimmed.rfind("-0.", 0) == 0) {
      trimmed = "-" + trimmed.substr(2);
    } else if (trimmed.rfind("0.", 0) == 0) {
      trimmed = trimmed.substr(1);
    }
    if (static_cast<int>(trimmed.size()) <= width) {
      return trimmed;
    }
    return std::string(static_cast<std::size_t>(width), '*');
  }
  return text;
}

// Fortran Gw.d following the behavior verified against gfortran: after
// rounding to d significant digits, fixed-format output is used when the
// decimal exponent EE (value = m * 10^EE, 0.1 <= m < 1) lies between 0
// and d; the fixed format then reserves the last four field characters as
// if an exponent were omitted. Otherwise the scientific format
// "0.ddddE+ee" appears, always with a two-digit exponent.
[[nodiscard]]
inline std::string formatFortranG(int width, int digits, double value) {
  if (!std::isfinite(value)) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  if (digits < 1 || digits > 17) {
    return std::string(static_cast<std::size_t>(width), '*');
  }

  const double magnitude = std::fabs(value);
  char buffer[64];

  if (magnitude == 0.0) {
    std::snprintf(buffer, sizeof buffer, "%.*f", digits - 1, 0.0);
    std::ostringstream stream;
    stream << std::setw(width - 4) << std::right << buffer << "    ";
    return stream.str();
  }

  std::snprintf(buffer, sizeof buffer, "%.*e", digits - 1, magnitude);
  const std::string normalized(buffer);
  const std::size_t exponentPosition = normalized.find('e');
  const double mantissa =
      std::strtod(normalized.substr(0, exponentPosition).c_str(), nullptr);
  const int baseExponent =
      std::atoi(normalized.substr(exponentPosition + 1).c_str());
  const int ee = baseExponent + 1;

  std::ostringstream stream;
  if (ee >= 0 && ee <= digits) {
    const int fractionDigits = digits - ee;
    if (fractionDigits > 0) {
      std::snprintf(buffer, sizeof buffer, "%.*f", fractionDigits, magnitude);
    } else {
      std::snprintf(buffer, sizeof buffer, "%.0f.", magnitude);
    }
    stream << std::setw(width - 4) << std::right
           << (value < 0.0 ? "-" + std::string(buffer) : std::string(buffer))
           << "    ";
  } else {
    std::snprintf(buffer, sizeof buffer, "%.*fE%c%02d", digits, mantissa / 10.0,
                  ee >= 0 ? '+' : '-', ee >= 0 ? ee : -ee);
    stream << std::setw(width) << std::right
           << (value < 0.0 ? "-" + std::string(buffer) : std::string(buffer));
  }

  std::string text = stream.str();
  if (static_cast<int>(text.size()) > width) {
    return std::string(static_cast<std::size_t>(width), '*');
  }
  return text;
}

// Fortran A output of a fixed-length string: short values are
// padded with spaces, long ones truncated to the field length.
[[nodiscard]]
inline std::string padFortranField(std::string_view text, std::size_t width) {
  std::string field{text};
  if (field.size() > width) {
    field.resize(width);
  }
  field.resize(width, ' ');
  return field;
}

// ABCOPY confirmation (xgdes.f Format 1200):
// leading line break, brackets around the node count with field width 4.
inline void emitAbcopyConfirmation(std::ostream& out, int nodeCount) {
  out << "\n Current airfoil nodes set from buffer airfoil nodes ("
      << std::setw(4) << nodeCount << " )\n";
}

// ABCOPY buffer message (xgdes.f:966 WRITE(*,*) 'ABCOPY: Buffer airfoil
// not available.'): list-directed, leading space.
inline void emitAbcopyBufferUnavailable(std::ostream& out) {
  out << " ABCOPY: Buffer airfoil not available.\n";
}

// ABCOPY capacity rejection (xgdes.f:969-972, four WRITE(*,*)-Lines).
// The integers are list-directed (gfortran: right-justified field width 12);
// the literal strings each end with a space before the value.
inline void emitAbcopyCapacityError(std::ostream& out, int maxNodes,
                                    int bufferPoints) {
  out << " Maximum number of panel nodes  : " << std::setw(12) << maxNodes
      << "\n";
  out << " Number of buffer airfoil points: " << std::setw(12) << bufferPoints
      << "\n";
  out << " Current airfoil cannot be set.\n";
  out << " Try executing PANE at Top Level instead.\n";
}

// PANGEN diagnostics (xfoil.f PANGEN): the only convergence message
// actually written in the CLI path is the Newton-limit message.
// The sharp LE-/TE messages are only TRACE and are not emitted.
inline void emitPangenDiagnostics(std::ostream& out,
                                  const core::paneling::PangenResult& result) {
  for (const core::math::Diagnostic diagnostic : result.diagnostics) {
    if (diagnostic == core::math::Diagnostic::limitReached) {
      out << "Paneling convergence failed.  Continuing anyway...\n";
      return;
    }
  }
}

// PANGEN capacity limit (xfoil.f:2025 STOP 'PANEL: Too many panels...').
// emitDiagnostics depends on the missing
// line end, so this one ends with a trailing '\n'.
inline void emitPangenTooManyPanels(std::ostream& out) {
  out << "Panel: Too many panels. Increase IQX\n";
}

// NACA buffer message (xfoil.f format 1200): leading line break,
// node count with field width 4.
inline void emitBufferAirfoilSet(std::ostream& out, int nodeCount) {
  out << "\n Buffer airfoil set using" << std::setw(4) << nodeCount
      << " points\n";
}

// PANGEN trailing-edge message (xfoil.f:2107/2110 FORMAT 1090 = /1X,A,F9.5).
// Fortran decides via SHARP; in the blunt case the gap GAP follows as
// F9.5. The canonical Fortran process (fortran-bin/xfoil) prints these
// lines after NACA/PANE. The emission follows
// the canonical behavior, not a formatting change.
inline void emitTrailingEdge(std::ostream& out, bool sharp, double gap) {
  out << "\n ";
  if (sharp) {
    out << "Sharp trailing edge\n";
  } else {
    out << "Blunt trailing edge.  Gap =" << std::fixed << std::setprecision(5)
        << std::setw(9) << gap << "\n";
  }
}

// PANGEN parameter table (xfoil.f:2116 Format 1100), only for SHOPAR.
// Field widths exactly as Fortran: NPAN I4, CVPAR/CTERAT/CTRRAT F6.3,
// XSREF1/2 and XPREF1/2 per 2F6.3.
inline void emitPanelingParameters(std::ostream& out, int npan, double cvpar,
                                   double cterat, double ctrrat, double xsref1,
                                   double xsref2, double xpref1,
                                   double xpref2) {
  out << "\n Paneling parameters used..."
      << "\n   Number of panel nodes      " << std::setw(4) << npan
      << "\n   Panel bunching parameter   " << std::fixed
      << std::setprecision(3) << std::setw(6) << cvpar
      << "\n   TE/LE panel density ratio  " << std::setw(6) << cterat
      << "\n   Refined-area/LE panel density ratio   " << std::setw(6)
      << ctrrat
      << "\n   Top    side refined area x/c limits " << std::setw(6) << xsref1
      << std::setw(6) << xsref2
       << "\n   Bottom side refined area x/c limits " << std::setw(6) << xpref1
       << std::setw(6) << xpref2 << "\n";
}

// CANG Format 1200 (xgeom.f:1157): maximum panel corner angle. It is
// output by the GETPAN apply via CANG(X,Y,N,1,...) after PANGEN(.FALSE.),
// not by PANE. index is the 1-based Fortran IMAX display.
inline void emitMaxPanelCornerAngle(std::ostream& out, double angle,
                                    int index, double x, double y) {
  out << "\n Maximum panel corner angle =" << formatFortranF(7, 3, angle)
      << "   at  i,x,y  = " << std::setw(3) << index << formatFortranF(9, 4, x)
      << formatFortranF(9, 4, y) << "\n";
}

// GEOPAR thickness/camber line (xgeom.f:378 Format 1000): THICK F12.6 on
// XT F7.3, CAMBR F12.6 on XC F7.3. The values derive from the core geopar
// (G2 extends the structured output with these quantities).
inline void emitThicknessCamber(std::ostream& out, double thickness,
                                double thicknessX, double camber,
                                double camberX) {
  out << " Max thickness = " << std::fixed << std::setprecision(6)
      << std::setw(12) << thickness << "  at x = " << std::setprecision(3)
      << std::setw(7) << thicknessX << "\n"
      << " Max camber    = " << std::setprecision(6) << std::setw(12) << camber
      << "  at x = " << std::setprecision(3) << std::setw(7) << camberX
      << "\n";
}

// GETPAN parameter table (xfoil.f Format 1000) at the PPAR entry.
// NPAN I4; CVPAR/CTERAT/CTRRAT F6.3; XSREF1/2 and XPREF1/2 per 2F6.3.
inline void emitPparTable(std::ostream& out, int npan, double cvpar,
                          double cterat, double ctrrat, double xsref1,
                          double xsref2, double xpref1, double xpref2) {
  out << "\n    Present paneling parameters..."
      << "\n  N  i   Number of panel nodes      " << std::setw(4) << npan
      << "\n  P  r   Panel bunching parameter   " << std::fixed
      << std::setprecision(3) << std::setw(6) << cvpar
      << "\n  T  r   TE/LE panel density ratio  " << std::setw(6) << cterat
      << "\n  R  r   Refined area/LE  panel density ratio  " << std::setw(6)
      << ctrrat
      << "\n  XT rr  Top    side refined area x/c limits   " << std::setw(6)
      << xsref1 << std::setw(6) << xsref2
      << "\n  XB rr  Bottom side refined area x/c limits   " << std::setw(6)
      << xpref1 << std::setw(6) << xpref2
      << "\n  Z oom\n  U nzoom\n  H ardcopy\n";
}

// GETPAN Format 1200 (xfoil.f:2218-2221): NPAN via IQX-6 (364) limited.
// 1X + literal with leading space, I4 of the limited value. It is only
// triggered by the parameter change itself, not by the pressure tables.
inline void emitPparPanelCountClamped(std::ostream& out, int limitedValue) {
  out << "  Number of panel nodes reduced to array limit:" << std::setw(4)
      << limitedValue << "\n";
}

// ---: LOAD text contract (xfoil.f LOAD / aread.f / xgeom.f GEOPAR) --

// Domain-specific separated AREAD output context (aread.f INFO branches). The
// context is deliberately not a boolean flag: plain, labeled, MSES single
// and MSES multi have different file types, even if single and
// multi use the same visible marker line text.
enum class AirfoilFileInfoContext {
  plain,       // ITYPE 1: plain coordinate file without names
  labeled,     // ITYPE 2: labeled coordinate file
  msesSingle,  // ITYPE 3: MSES single-element file
  msesMulti    // ITYPE 4: MSES multi-element file
};

// AREAD identification line (aread.f). The name is rendered differently
// per context:
// - labeled: previous product/golden contract with three spaces and
// field width 32;
// - MSES single/multi: canonical Fortran text with two spaces after
// 'Name:' and CHARACTER*48 padding, byte-confirmed by
// tests/fortran-reference/cases/file-io/isav-MSES-domain-parameters/
// stdout.raw.txt and the MSES text reference cases.
inline void emitAirfoilFileInfo(std::ostream& out,
                                AirfoilFileInfoContext context,
                                std::string_view name,
                                std::size_t nameFieldWidth = 48) {
  switch (context) {
    case AirfoilFileInfoContext::labeled:
      out << "\n Labeled airfoil file.  Name:  "
          << padFortranField(name, nameFieldWidth) << "\n";
      break;
    case AirfoilFileInfoContext::msesSingle:
    case AirfoilFileInfoContext::msesMulti:
      out << "\n MSES airfoil file.  Name:  "
          << padFortranField(name, nameFieldWidth) << "\n";
      break;
    case AirfoilFileInfoContext::plain:
      out << "\n Plain airfoil file\n";
      break;
  }
}

// ASKS prompt (userio.f Format (/A,' s> ',$)): leading line break,
// prompt text, then ' s> ' without closing line break. The
// leading '/' of the next WRITE ends the prompt line.
inline void emitAsksPrompt(std::ostream& out, std::string_view promptText) {
  out << "\n" << promptText << "   s>  ";
}

// LOAD point count and traversal direction (xfoil.f format 1010/1011). The
// leading '/' ends the preceding prompt line; the two lines
// appear without a blank line in between.
inline void emitLoadPointCountAndOrdering(std::ostream& out, int count,
                                          bool clockwise) {
  out << "\n Number of input coordinate points:" << std::setw(4) << count
      << "\n " << (clockwise ? "Clockwise ordering" : "Counterclockwise ordering")
      << "\n";
}

// LOAD normalization message (xfoil.f format 1020), only for LNORM.
inline void emitNormalized(std::ostream& out) {
  out << "\n Airfoil has been normalized\n";
}

// TOP-level NORM toggle (xfoil.f:233-239): toggles the LNORM switch with
// a list-directed WRITE(*,*) message (no leading line break, since the
// prompt with '$' does not advance). 'will be' carries two spaces as
// in the original; "won't" corresponds to the Fortran '' escape.
inline void emitNormEnabled(std::ostream& out) {
  out << " Loaded airfoil will  be normalized\n";
}

inline void emitNormDisabled(std::ostream& out) {
  out << " Loaded airfoil won't be normalized\n";
}

// TOP-level REVE toggle (xfoil.f:176-182): toggles LCLOCK, the write
// orientation (SAVE/PSAV/ISAV/MSAV); list-directed WRITE(*,*) message
// (no leading line break, since the prompt with '$' does not advance).
// REVE does not reverse coordinates; LCLOCK only controls the write order.
inline void emitReveClockwise(std::ostream& out) {
  out << " Airfoil will be written in clockwise order\n";
}

inline void emitReveCounterclockwise(std::ostream& out) {
  out << " Airfoil will be written in counterclockwise order\n";
}

// TOP-level DELI menu (xfoil.f Format 2100), only on the path without a
// valid argument (no argument or invalid value): leading line break,
// separator line, the three allowed delimiter states, blank line, then the
// current value as list-directed I2 (field width 2). The ASKI prompt
// 'Enter new delimiter' is added by the CLI layer (ask helper), not
// here. DELI with a valid argument (0..2) acts silently and does not call
// this menu.
inline void emitDelimiterMenu(std::ostream& out, int currentDelimiter) {
  out << "\n  --------------------------\n"
         "   0  blank\n"
         "   1  comma\n"
         "   2  tab\n"
      << "\n  currently, delimiter =" << std::setw(2) << currentDelimiter
      << "\n";
}

// LOAD LE-/TE-/chord line (xfoil.f Format 1050): leading line break,
// LE 2F10.5, chord F10.5, TE 2F10.5. Values from the confirmed core
// (abcopy panel contour), not from a new interpreter computation.
inline void emitLeTeChord(std::ostream& out, double xle, double yle,
                          double chord, double xte, double yte) {
  out << "\n  LE  x,y  =" << formatFortranF(10, 5, xle)
      << formatFortranF(10, 5, yle) << "  |   Chord ="
      << formatFortranF(10, 5, chord) << "\n"
      << "  TE  x,y  =" << formatFortranF(10, 5, xte)
      << formatFortranF(10, 5, yte) << "  |\n";
}

// LOAD error messages (xfoil.f/aread.f), each with a leading blank line
// and the completion line ' *** LOAD NOT COMPLETED ***'.
inline void emitLoadNotCompleted(std::ostream& out) {
  out << " *** LOAD NOT COMPLETED ***\n";
}

inline void emitLoadFileOpenError(std::ostream& out, std::string_view name) {
  out << "\n File OPEN error.  Nonexistent file:  " << name << "\n";
  emitLoadNotCompleted(out);
}

inline void emitLoadBufferOverflow(std::ostream& out, int limit) {
  out << "\n Buffer array size exceeded\n"
      << " Maximum number of points: " << std::setw(4) << limit << "\n";
  emitLoadNotCompleted(out);
}

// AREFINE overflow (xgeom.f 1688-1689, label 90): list-directed WRITE,
// NNEW=0. CADD returns the text when the kernel reports limitReached.
inline void emitSdoubleOverflow(std::ostream& out) {
  out << " SDOUBLE:  Arrays will overflow.  No action taken.\n";
}

inline void emitLoadElementNotFound(std::ostream& out) {
  out << "\n Element not found\n";
  emitLoadNotCompleted(out);
}

inline void emitLoadReadError(std::ostream& out) {
  out << "\n File READ error.  Unrecognizable file format\n";
  emitLoadNotCompleted(out);
}

inline void emitLoadNormalizationFailed(std::ostream& out) {
  out << "\n Airfoil normalization failed\n";
  emitLoadNotCompleted(out);
}

inline void emitLoadGeometryFailed(std::ostream& out) {
  out << "\n Airfoil geometry preparation failed\n";
  emitLoadNotCompleted(out);
}

// ---: INTE text contract (xfoil.f INTE, userio.f ASKS/ASKR, xgeom.f
// GEOPAR FORMAT 1000) -----------------------------------------------

// INTE source menu (xfoil.f Format 1100): ' F disk file' and
// ' C current airfoil ' followed by the current name in field width 48
// (CHARACTER*48, A output). Polar lines (Format 1200) omitted for NPOL=0.
inline void emitInteMenu(std::ostream& out, std::string_view currentName) {
  out << "\n  F  disk file\n  C  current airfoil  "
      << padFortranField(currentName, 48) << "\n";
}

// INTE source selection (xfoil.f FORMAT 2100 = /' Select source of airfoil "',
// I1, A, $). Leading line break, index, prompt ' " (F C): ' without
// closing line break (NPOL=0 branch, NPR=12).
inline void emitInteSourcePrompt(std::ostream& out, int index) {
  out << "\n  Select source of airfoil \"" << index << "\" ( F C ):  ";
}

// INTE filename prompt (ASKS 'Enter filename^', userio.f Format
// (/A,' s> ',$)): leading line break, without closing
// line break.
inline void emitInteFilenamePrompt(std::ostream& out) {
  out << "\nEnter filename   s>  ";
}

// INTE source names (xfoil.f WRITE(*,*) 'airfoil "0": ', NAMEINT(1) etc.):
// list-directed output with leading space, name in CHARACTER*48.
inline void emitInteAirfoilNames(std::ostream& out, std::string_view name0,
                                 std::string_view name1) {
  out << "\n airfoil \"0\":  " << padFortranField(name0, 48)
      << "\n airfoil \"1\":  " << padFortranField(name1, 48) << "\n";
}

// INTE fraction prompt (ASKR 'Specify interpolating fraction 0...1^',
// userio.f Format (/A,' r> ',$)): leading line break, without
// closing line break; the following GEOPAR line (Format 1000,
// leading space, no '/') appears on the same line.
inline void emitInteFractionPrompt(std::ostream& out) {
  out << "\nSpecify interpolating fraction  0...1   r>  ";
}

// INTE completion (xfoil.f WRITE(*,*)): blank line, buffer message and note
// on PCOP/PANE (INTE produces no current contour, no ABCOPY).
inline void emitInteResultPlaced(std::ostream& out) {
  out << "\n Result has been placed in buffer airfoil\n"
      << " Execute PCOP or PANE to set new current airfoil\n";
}

// INTE file-open error (aread.f, IPRNT branch): leading blank line and
// the OPEN error message; INTE returns without 'LOAD NOT COMPLETED'.
inline void emitInteFileOpenError(std::ostream& out, std::string_view name) {
  out << "\n File OPEN error.  Nonexistent file:  " << name << "\n";
}

// INTE selection 'C' without a current contour (xfoil.f INTE, WRITE(*,*)
// 'No current airfoil available'): list-directed with leading
// space; then a renewed selection (GO TO 20).
inline void emitInteNoCurrentAirfoil(std::ostream& out) {
  out << " No current airfoil available\n";
}

// INTE invalid selection (xfoil.f INTE, 90 CONTINUE): blank line and
// list-directed 'Invalid response'; INTE returns to the main menu.
inline void emitInteInvalidResponse(std::ostream& out) {
  out << "\n Invalid response\n";
}

// INTX: polar slot line of the source selection (xfoil.f INTX
// FORMAT 1200 = 1X,I2,' polar airfoil ', A); NAMEPOL in CHARACTER*48.
inline void emitIntxPolarSlot(std::ostream& out, int slotIndex,
                              std::string_view name) {
  out << " " << (slotIndex < 10 ? " " : "") << slotIndex
      << "  polar airfoil    " << padFortranField(name, 48) << "\n";
}

// INTX source selection (xfoil.f FORMAT 2100 = /' Select source of
// airfoil "', I1, A, $). The prompt choices depend on NPOL
// (xfoil.f 2487-2499): NPOL=0 '" (F C): ', NPOL=1 '" (F C 1): ',
// NPOL=2 '" (F C 1 2): ', otherwise '" (F C 1 2..): '.
inline void emitIntxSourcePrompt(std::ostream& out, int index, int npol) {
  out << "\n  Select source of airfoil \"" << index << "\"";
  if (npol <= 0) {
    out << " ( F C )";
  } else if (npol == 1) {
    out << " ( F C 1 )";
  } else if (npol == 2) {
    out << " ( F C 1 2 )";
  } else {
    out << " ( F C 1 2.. )";
  }
  out << ":  ";
}

// INTX invalid selection/EOF for the source selection (xfoil.f INTX
// READ(*,1000,ERR=90,END=90), 90 CONTINUE): blank line and list-directed
// 'Invalid response. No action taken.' (two spaces after the
// period); INTX returns to the main menu.
inline void emitIntxInvalidResponse(std::ostream& out) {
  out << "\n Invalid response.  No action taken.\n";
}

// LOAD command warning for an excessive panel angle (xfoil.f:212-215,
// Format 1081 after ABCOPY+CANG of the LOAD branch): list-directed leading
// space, AMAX in F7.1, IMAX in I4. PANPLT is a plot command and stays
// unavailable in this build.
inline void emitLoadPoorDistributionWarning(std::ostream& out, double amax,
                                            int imax) {
  out << "\n WARNING: Poor input coordinate distribution\n"
      << "          Excessive panel angle"
      << std::fixed << std::setprecision(1) << std::setw(7) << amax
      << "  at i =" << std::setw(4) << imax << "\n"
      << "          Repaneling with PANE and/or PPAR suggested\n"
      << "           (doing GDES,CADD before repaneling _may_\n"
      << "            improve excessively coarse LE spacing\n";
}

// ---: TOP-level NAME/NINC text contract (xoper.f NAMMOD FORMAT 1040) -

// ---: TOP-level SAVE text contract (xfoil.f SAVE) --------------------

// SAVE overwrite prompt (xfoil.f:1357-1358): two list-directed
// WRITE(*,*) lines. The first writes a blank line, the second the
// literal with leading space. The canonical answer is 'Y'; an answer
// whose first read character is not 'N'/'n' is treated as confirmed.
inline void emitSaveOverwritePrompt(std::ostream& out) {
  out << "\n Output file exists.  Overwrite?  Y\n";
}

// SAVE rejection after 'N'/'n' (xfoil.f:1363, list-directed WRITE(*,*)).
inline void emitSaveNotSaved(std::ostream& out) {
  out << " Current airfoil not saved.\n";
}

// SAVE OPEN error (xfoil.f:1418-1419): two list-directed WRITE(*,*) lines.
// After an ASKS prompt without closing line break the
// first message on the same line; the leading space comes from the
// list-directed field.
inline void emitSaveBadFilename(std::ostream& out) {
  out << " Bad filename.\n"
      << " Current airfoil not saved.\n";
}

// NAMMOD prompt (xoper.f:1881-1882): leading line break via '/',
// literal with leading space, two spaces after the colon,
// then NAMDEF as a CHARACTER*48 A-field. The field width 48 corresponds to the
// canonical name field (XFOIL.INC); short requested values are space-
// padded, overlong ones truncated to 48.
inline void emitNamePrompt(std::ostream& out, std::string_view nameDefault) {
  out << "\n Enter airfoil name or <return> for default:  "
      << padFortranField(nameDefault, 48) << "\n";
}

// ---: OPER operating-state texts (xoper.f OPER/MRSHOW) ---------------

// LPACC guard text (xoper.f format 2100): two WRITE lines with leading
// space; the leading '/' produces the line break after the prompt.
inline void emitPolarAccumulationBlocked(std::ostream& out) {
  out << "\n * Polar is being accumulated.\n"
      << " * Cannot change its parameters in midstream.\n";
}

// OPER DAMP (xoper.f 1746/1749): list-directed WRITE(*,*)
// with leading space; the prompt advance is already written in the CLI
// path by the OPER prompt with advance.
inline void emitDampModified(std::ostream& out) {
  out << " Modified amplification used\n";
}

inline void emitDampOriginal(std::ostream& out) {
  out << " Original amplification used\n";
}

// OPER ASET (xoper.f 1252-1289). NPOL==0 branch (1253-1256):
// blank + list-directed 'No polar airfoils are stored'.
inline void emitAsetNoPolars(std::ostream& out) {
  out << "\n No polar airfoils are stored\n";
}

// ASKI prompt (userio.f FORMAT 1000 = /A,' i> ',$): leading
// line break, without closing advance.
inline void emitAsetIndexPrompt(std::ostream& out) {
  out << "\nEnter index of polar airfoil to set   i>  ";
}

// Invalid index (xoper.f 1270-1273): blank + message, then a retry
// with NINPUT=0.
inline void emitAsetIndexInvalid(std::ostream& out) {
  out << "\n Specified polar airfoil does not exist\n";
}

// Confirmation prompt (xoper.f 1276-1277): blank + text with a
// default answer ' Y'.
inline void emitAsetConfirm(std::ostream& out) {
  out << "\n Current airfoil will be overwritten.  Proceed?  Y\n";
}

// Abort with N/n (xoper.f 1285-1286).
inline void emitAsetNoAction(std::ostream& out) {
  out << " No action taken\n";
}

// --- OPER BLF / FBLGET (xoper.f 1547-1600, dplot.f 320-503) --

// FORMAT 9100 (dplot.f 428-430): station diagnostic line.
inline void emitBlfStationLine(std::ostream& out, double x0, double y0,
                               double ds, double rtheta, double hk) {
  out << " x y =" << io::fortranF(8, 4, x0) << io::fortranF(8, 4, y0)
      << "    Delta* =" << io::fortranG(12, 4, ds)
      << "    Rtheta =" << io::fortranF(10, 2, rtheta)
      << "    Hk =" << io::fortranF(9, 4, hk) << "\n";
}

// Format '(1x,5f10.6)' with a two-item list (dplot.f 469):
// YY(K)/0.0078 and UU(K).
inline void emitBlfProfileSample(std::ostream& out, double yyScaled,
                                 double uu) {
  out << " " << io::fortranF(10, 6, yyScaled) << io::fortranF(10, 6, uu)
      << "\n";
}

// List-directed 'yint > ye' (dplot.f 494).
inline void emitBlfYintExceeded(std::ostream& out) {
  out << " yint > ye\n";
}

// FORMAT 7720 (xoper.f 1600): final report line.
inline void emitBlfReportLine(std::ostream& out, double xpr, double fprScaled) {
  out << " xBL, Fint =" << io::fortranF(7, 3, xpr)
      << io::fortranF(12, 6, fprScaled) << "\n";
}

// Controlled BLF deviation (user decision):
// canonical Fortran runs into a memory access error without BL station
// data; C++ refuses mutation-free with this compact,
// deterministic text. Applies exclusively to BLF.
inline void emitBlfNoData(std::ostream& out) {
  out << " Boundary-layer data are not available.\n";
}

// --- OPER FMOM/FNEW/VELS (xoper.f 1617-1665) -------------------

// Format 1500 (xoper.f 1620-1626): FMOM hinge report. The
// superscript '2' lines are canonical unit annotations.
inline void emitFmomReport(std::ostream& out, double xof, double yof,
                           double hmom, double hfx, double hfy) {
  out << "\n Flap hinge x,y :" << io::fortranF(8, 4, xof)
      << io::fortranF(8, 4, yof) << "\n"
      << "                                           2  2\n"
      << " Hinge moment/span = " << io::fortranF(8, 6, hmom)
      << "  x  1/2 rho V  c \n"
      << "                                           2   \n"
      << " x-Force     /span = " << io::fortranF(8, 6, hfx)
      << "  x  1/2 rho V  c \n"
      << "                                           2   \n"
      << " y-Force     /span = " << io::fortranF(8, 6, hfy)
      << "  x  1/2 rho V  c \n\n";
}

// FORMAT 1800 (xoper.f 1658-1659): VELS report (leading blank-
// line by format slash, closing slash as a blank line).
inline void emitVelsReport(std::ostream& out, double u, double v, double q,
                           double cp) {
  out << "\n u/Uinf = " << io::fortranF(8, 4, u)
      << "   v/Uinf = " << io::fortranF(8, 4, v) << "\n"
      << " q/Uinf = " << io::fortranF(8, 4, q)
      << "   Cp     = " << io::fortranF(8, 4, cp) << "\n\n";
}

// WRITE(*,*) 'Currently...' (xoper.f Re/Mach): list-directed with leading
// Space.
inline void emitCurrently(std::ostream& out) {
  out << " Currently...\n";
}

// WRITE(*,*) 'Supersonic freestream not allowed' (xoper.f Mach).
inline void emitSupersonicFreestreamNotAllowed(std::ostream& out) {
  out << " Supersonic freestream not allowed\n";
}

// MRSHOW (xoper.f 1805-1827): for LM or LR first a blank line, then the
// M- and/or Re- line with Format 1100/1200 and an optional dependency
// suffix. The values are the canonical base inputs MINF1/REINF1.
inline void emitMrShow(std::ostream& out, bool showMach, bool showReynolds,
                       int machType, double minf1, int reType, double reinf1) {
  if (showMach || showReynolds) {
    out << "\n";
  }
  if (showMach) {
    std::string suffix;
    if (machType == 2) {
      suffix = " / sqrt(CL)";
    } else if (machType == 3) {
      suffix = " / CL";
    }
    out << " M  =" << formatFortranF(10, 4, minf1) << suffix << "\n";
  }
  if (showReynolds) {
    std::string suffix;
    if (reType == 2) {
      suffix = " / sqrt(CL)";
    } else if (reType == 3) {
      suffix = " / CL";
    }
    out << " Re =" << formatFortranG(12, 4, reinf1) << suffix << "\n";
  }
}

// TYPE menu table (xoper.f Format 1105). The leading '/' produces the
// line break after the OPER prompt; the table ends without a blank line,
// since the following ASKI prompt brings its own leading '/'.
inline void emitTypeTable(std::ostream& out) {
  out << "\n"
         " Type   parameters held constant       varying      fixed   \n"
         " ----   ------------------------       -------   -----------\n"
         "   1    M, Re..   lift     chord, vel.\n"
         "   2    M sqrt(CL), Re sqrt(CL)..   vel.     chord, lift\n"
         "   3    M, Re CL..   chord    lift, vel.\n";
}

// WRITE(*,*) 'Current iteration limit:', ITMAX (xoper.f ITER). The integer
// field is list-directed with field width 12.
inline void emitCurrentIterationLimit(std::ostream& out, int limit) {
  out << " Current iteration limit:" << std::setw(12) << limit << "\n";
}

// sonic Cp line after Mach (xoper.f Format 1300): leading '/' before the
// literal with leading space, F10.2 and F10.3, closing '/'.
inline void emitSonicCp(std::ostream& out, double cpstar,
                        double qstarOverQinf) {
  out << "\n Sonic Cp =" << formatFortranF(10, 2, cpstar)
      << "      Sonic Q/Qinf =" << formatFortranF(10, 3, qstarOverQinf)
      << "\n\n";
}

// ---: VPAR text contract (xoper.f VPAR FORMAT 1200, INIT WRITE(*,*)) --

// VPAR parameter display (xoper.f VPAR Format 1200, 2517-2526). The leading
// '/' produces the line break after the preceding prompt. The calibration
// constants (Klag/Uxwt/Kdl, A/B/KCt, CtiniK/CtiniX) derive from the
// authoritative session-bound BLPAR state, not from
// a renewed default fetch. The
// turbulence levels TURB = 100*EXP(-(Ncrit+8.43)/2.4) (xoper.f 2506-2507) are
// pure display conversion and are computed here, not in the handler.
inline void emitVparDisplay(std::ostream& out, double topTripX,
                            double bottomTripX, double ncritTop,
                            double ncritBottom, double vaccel, double waklen,
                            const core::boundary_layer::BoundaryLayerParameters&
                                blpar) {
  // xoper.f 2506-2511: TURB(1) from ACRIT(1), TURB(2) from ACRIT(2).
  const double turbTop = 100.0 * std::exp(-(ncritTop + 8.43) / 2.4);
  const double turbBot = 100.0 * std::exp(-(ncritBottom + 8.43) / 2.4);
  out << "\n"
      << " Xtr/c     =" << formatFortranF(8, 4, topTripX) << "    top    side\n"
      << " Xtr/c     =" << formatFortranF(8, 4, bottomTripX)
      << "    bottom side\n"
      << " NcritT    =" << formatFortranF(8, 2, ncritTop) << "   ("
      << formatFortranF(6, 3, turbTop) << " % turb. level )\n"
      << " NcritB    =" << formatFortranF(8, 2, ncritBottom) << "   ("
      << formatFortranF(6, 3, turbBot) << " % turb. level )\n"
      << " Vacc      =" << formatFortranF(8, 4, vaccel) << "\n"
      << " WakeL/c   =" << formatFortranF(8, 3, waklen) << "\n"
      << "\n"
      << " Klag  =" << formatFortranF(8, 4, blpar.sccon) << "     Uxwt  ="
      << formatFortranF(8, 2, blpar.duxcon) << "       Kdl ="
      << formatFortranF(8, 4, blpar.dlcon) << "\n"
      << " A     =" << formatFortranF(8, 4, blpar.gacon) << "     B     ="
      << formatFortranF(8, 4, blpar.gbcon) << "       KCt ="
      << formatFortranF(8, 5, blpar.ctcon) << "\n"
      << " CtiniK=" << formatFortranF(8, 4, blpar.ctrcon) << "     CtiniX="
      << formatFortranF(8, 4, blpar.ctrcex) << "\n";
}

// VPAR INIT messages (xoper.f 2651-2652, list-directed WRITE(*,*), leading
// space, no leading line break, since the prompt with '$' does not
// advance).
inline void emitVparInitInitialized(std::ostream& out) {
  out << " BLs are assumed to be initialized\n";
}

inline void emitVparInitWillInitialize(std::ostream& out) {
  out << " BLs will be initialized on next point\n";
}

// ---: OPER single-point progress texts (ALFA/CLI/CL) ----------------

// GGCALC (xpanel.f 1000): WRITE(*,*) list-directed with leading space.
inline void emitGgcalcProgress(std::ostream& out) {
  out << " Calculating unit vorticity distributions ...\n";
}

// XYWAKE (xpanel.f 1277): WRITE(*,*) list-directed.
inline void emitXywakeProgress(std::ostream& out) {
  out << " Calculating wake trajectory ...\n";
}

// QDCALC (xpanel.f 1156): WRITE(*,*) list-directed.
inline void emitQdcalcProgress(std::ostream& out) {
  out << " Calculating source influence matrix ...\n";
}

// VISCAL header (xoper.f 2968-2969): WRITE(*,*) blank line, then
// 'Solving BL system ...'.
inline void emitSolvingBlSystem(std::ostream& out) {
  out << "\n Solving BL system ...\n";
}

// VISCAL non-convergence (xoper.f 3023): WRITE(*,*) 'VISCAL: Convergence
// failed' (two spaces after the colon).
inline void emitViscalConvergenceFailed(std::ostream& out) {
  out << " VISCAL:  Convergence failed\n";
}

// SPECAL convergence abort (xoper.f 2799): WRITE(*,*) list-directed,
// two spaces after the colon; fallthrough to the point continuation.
inline void emitSpecalConvergenceFailed(std::ostream& out) {
  out << " SPECAL:  Minf convergence failed\n";
}

// SPECCL convergence abort (xoper.f 2874): WRITE(*,*) list-directed,
// two spaces after the colon; no specified-point text.
inline void emitSpecclConvergenceFailed(std::ostream& out) {
  out << " SPECCL:  CL convergence failed\n";
}

// STFIND stagnation point warning (xpanel.f 1366): WRITE(*,*) list-directed,
// single line; Fortran returns to I=N/2 and continues.
inline void emitStfindStagnationNotFound(std::ostream& out) {
  out << " STFIND: Stagnation point not found. Continuing ...\n";
}

// '!' without a valid previous OPER command (xoper.f 86-88):
// WRITE(*,*) list-directed.
inline void emitPreviousOperCommandNotValid(std::ostream& out) {
  out << " Previous .OPER command not valid\n";
}

// ASEQ/CSEQ NSEQEX halt (xoper.f 741-745, FORMAT 1150): leading
// blank line, I3 counter, F8.3 ALAST, F10.5 CLAST.
inline void emitSequenceHalted(std::ostream& out, int iseqex,
                               double alastDeg, double clast) {
  out << "\n Sequence halted since previous" << io::fortranI(3, iseqex)
      << " points did not converge"
      << "\n Last-converged  alpha =" << io::fortranF(8, 3, alastDeg)
      << "    CL =" << io::fortranF(10, 5, clast) << "\n";
}

// OPER Alfa/CLI/Cl re-run (xoper.f 446/503/538): only for LVISC,
// .NOT.LPACC and .NOT.LVCONV.
inline void emitContinueIteratingHint(std::ostream& out) {
  out << " Type \"!\" to continue iterating\n";
}

// ---: numeric single-point diagnostic formatters --------------------
// Byte-exact reproduction of the canonical formats from structured
// value data sets. All field widths follow the Fortran FORMATs directly;
// the formatting uses the existing io::fortranF/fortranE/fortranI
// helpers (asterisk fill on overflow, Fortran E exponent).

// ---C-D: MDES/QDES diagnostic texts (byte-exact) -------------------

// gfortran list-directed default REAL(8) field: 17 significant digits,
// large E exponent with three digits, right-justified field width 25.
// gfortran list-directed REAL(8) (WRITE(*,*)): field 25 characters.
// Decimal form for -1 <= decimal exponent <= 16 (17 significant
// digits, positional; >=17 digits before the point without digits after,
// but with a closing point), otherwise E form
// 'd.ddddddddddddddddE+sss'. Decimal: 5 trailing spaces,
// leading space = 20 - length (min. 2 for length > 19);
// E form: right-justified in 25. Reference evidence: ld.f..ld5.f.
inline std::string fortranListReal25(double value) {
  const bool negative = std::signbit(value);
  const double mag = std::fabs(value);
  int exp10 = 0;
  if (mag != 0.0) {
    exp10 = static_cast<int>(std::floor(std::log10(mag)));
    // readjust for log10 rounding cases
    double check = std::pow(10.0, exp10);
    if (mag < check) {
      --exp10;
    } else if (mag >= check * 10.0) {
      ++exp10;
    }
  }
  char buf[64];
  if (mag != 0.0 && (exp10 < -1 || exp10 > 16)) {
    std::snprintf(buf, sizeof(buf), "%.16e", value);
    std::string text(buf);
    const auto epos = text.find('e');
    if (epos != std::string::npos) {
      text[epos] = 'E';
      const std::string expPart = text.substr(epos + 1);
      const char sign = expPart[0];
      const std::string digits = expPart.substr(1);
      text = text.substr(0, epos + 1) + sign +
             std::string(std::size_t{3} - digits.size(), '0') + digits;
    }
    if (static_cast<int>(text.size()) >= 25) {
      return text;
    }
    return std::string(std::size_t{25} - text.size(), ' ') + text;
  }
  // Decimal form with 17 significant digits.
  std::string core;
  if (mag == 0.0) {
    core = negative ? "-0.0000000000000000" : "0.0000000000000000";
  } else if (exp10 >= 16) {
    std::snprintf(buf, sizeof(buf), "%.0f.", mag);
    core = buf;
    if (negative) {
      core = "-" + core;
    }
  } else if (exp10 >= 0) {
    const int decimals = 16 - exp10;
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, mag);
    core = buf;
    if (negative) {
      core = "-" + core;
    }
  } else {
    std::snprintf(buf, sizeof(buf), "%.17f", mag);
    core = buf;
    if (negative) {
      core = "-" + core;
    }
  }
  // Subsequent rounding can shift the leading digits before the decimal
  // point (e.g. 9.99...e15 -> 17 leading digits): re-center.
  {
    const auto dotp = core.find('.');
    const std::size_t intDigits =
        dotp == std::string::npos
            ? core.size() - (negative ? 1 : 0)
            : dotp - (negative ? 1 : 0);
    if (intDigits > 17) {
      std::snprintf(buf, sizeof(buf), "%.0f.", mag);
      core = buf;
      if (negative) {
        core = "-" + core;
      }
    } else if (dotp != std::string::npos && intDigits != 0 &&
               exp10 >= 0 &&
               intDigits - 1 + (core.size() - dotp - 1) > 17) {
      const int decimals = 16 - static_cast<int>(intDigits - 1);
      std::snprintf(buf, sizeof(buf), "%.*f", decimals, mag);
      core = buf;
      if (negative) {
        core = "-" + core;
      }
    }
  }
  const std::size_t len = core.size();
  const std::size_t lead = (len <= 19) ? (20 - len) : std::size_t{2};
  return std::string(lead, ' ') + core + "     ";
}

// Format 1100 (xmdes.f SCINIT): leading and closing '/'
// blank lines; REAL(DZTE), IMAG(DZTE) F7.4; AGTE*180 F7.3.
inline void emitMdesTeGapLine(std::ostream& out, double dxte, double dyte,
                              double teAngleDeg) {
  out << "\n Current TE gap  dx dy =" << io::fortranF(7, 4, dxte)
      << io::fortranF(7, 4, dyte) << "    TE angle ="
      << io::fortranF(7, 3, teAngleDeg) << " deg.\n\n";
}

// SCINIT header (xmdes.f): WRITE(*,*) 'Initializing mapping coordinate
//...' -> list-directed, leading space.
inline void emitScinitHeader(std::ostream& out) {
  out << " Initializing mapping coordinate ...\n";
}

// SCINIT iteration line (xmdes.f): WRITE(*,*) IPASS,
// ' max(dw) =', DSCMAX. Integer list-directed field width 12,
// Real list-directed field width 25.
inline void emitScinitIteration(std::ostream& out, int pass,
                                double maxDelta) {
  out << " " << std::setw(11) << pass << " " << "     max(dw) ="
      << " " << fortranListReal25(maxDelta) << "\n";
}

// MAPGEN iteration line (xmdes.f): WRITE(*,*) ITERCN, DCNMAX.
inline void emitMapgenIteration(std::ostream& out, int iteration,
                                double dcnMax) {
  out << " " << std::setw(11) << iteration << " "
      << fortranListReal25(dcnMax) << "\n";
}

// MAPGEN non-convergence (xmdes.f): WRITE(*,*) + WRITE(*,*) message.
inline void emitMapgenNotConverged(std::ostream& out) {
  out << "\n MAPGEN: Geometric constraints not fully converged\n";
}

// PERT non-convergence (xmdes.f PERT 1743): WRITE(*,*)
// 'TE gap,chord did not converge' — list-directed, leading
// space, without a preceding blank line (unlike MAPGEN).
inline void emitPertNotConverged(std::ostream& out) {
  out << " TE gap,chord did not converge\n";
}

// ZLEFIND warning (xmdes.f ZLEFIND): WRITE(*,*)
// 'ZLEFIND: LE location failed.' — list-directed, leading
// space. It is written by ZCNORM within the MAPGEN/PERT Newton
// loop, before its ITERCN/DCNMAX line.
inline void emitZlefindFailed(std::ostream& out) {
  out << " ZLEFIND: LE location failed.\n";
}

// GEOPAR FORMAT 1000 (xgeom.f 377-379): THICK/XTHICK/CAMBR/XCAMBR.
inline void emitGeoparMetrics(std::ostream& out, double thick,
                              double thickX, double cambr, double cambrX) {
  out << " Max thickness = " << io::fortranF(12, 6, thick)
      << "  at x = " << io::fortranF(7, 3, thickX) << "\n"
      << " Max camber    = " << io::fortranF(12, 6, cambr)
      << "  at x = " << io::fortranF(7, 3, cambrX) << "\n";
}

// GGCALC header (xpanel.f 1000): list-directed string.
inline void emitGgCalcHeader(std::ostream& out) {
  out << " Calculating unit vorticity distributions ...\n";
}

// LEFIND warning (xgeom.f 84): list-directed string.
inline void emitLefindNotFound(std::ostream& out) {
  out << " LEFIND:  LE point not found.  Continuing...\n";
}

// Format 2000 (xoper.f 3103-3105): RLX < 1.0. Leading '/' produces the
// blank line before the iteration line. Station/page 1-based as in the
// write list (IMXBL, ISMXBL).
inline void emitViscalIterationWithRelaxation(std::ostream& out, int iter,
                                              double rmsbl, double rmxbl,
                                              char marker, int station,
                                              int side, double rlx) {
  out << "\n " << io::fortranI(3, iter) << "   rms: "
      << io::fortranE(10, 4, rmsbl) << "   max: "
      << io::fortranE(10, 4, rmxbl) << "   " << marker << " at "
      << io::fortranI(4, station) << io::fortranI(3, side) << "   RLX:"
      << io::fortranF(6, 3, rlx) << "\n";
}

// Format 2010 (xoper.f 3106-3107): RLX == 1.0, without the RLX part.
inline void emitViscalIterationWithoutRelaxation(std::ostream& out, int iter,
                                                 double rmsbl, double rmxbl,
                                                 char marker, int station,
                                                 int side) {
  out << "\n " << io::fortranI(3, iter) << "   rms: "
      << io::fortranE(10, 4, rmsbl) << "   max: "
      << io::fortranE(10, 4, rmxbl) << "   " << marker << " at "
      << io::fortranI(4, station) << io::fortranI(3, side) << "\n";
}

// Format 2020 (xoper.f 3108-3111): two lines without a leading '/';
// CDp = CDPDIF = Cd - CDF per the write list (xoper.f 3012).
inline void emitViscalIterationLoads(std::ostream& out, double alphaDeg,
                                     double cl, double cm, double cd,
                                     double cdf) {
  out << "    "
      << "   a =" << io::fortranF(7, 3, alphaDeg)
      << "      CL =" << io::fortranF(8, 4, cl) << "\n"
      << "    "
      << "  Cm =" << io::fortranF(8, 4, cm)
      << "     CD =" << io::fortranF(9, 5, cd)
      << "   =>   CDf =" << io::fortranF(9, 5, cdf)
      << "    CDp =" << io::fortranF(9, 5, cd - cdf) << "\n";
}

// Format 9922 (xoper.f 3060): completion block xoper.f 3029-3057; the
// doubling of PSEP/PATT/DELP corresponds to the write list (3058-3059).
inline void emitViscalCompletion(std::ostream& out, double acrit,
                                 double hkmax, double cd, double psep,
                                 double patt, double delp, double xoctr) {
  out << " " << io::fortranF(10, 3, acrit) << io::fortranF(10, 4, hkmax)
      << io::fortranF(11, 6, cd) << io::fortranF(11, 6, 2.0 * psep)
      << io::fortranF(11, 6, 2.0 * patt) << io::fortranF(11, 6, 2.0 * delp)
      << io::fortranF(10, 4, xoctr) << "     #\n";
}

// ---: viscous BL event formatters ----------------------------------
// Byte-exact reproduction of the canonical WRITE digits from xbl.f. The
// page/station/ITRAN fields are lifted here from 0-based to the
// Fortran 1-based written values.

// xbl.f:84-85: WRITE(*,*) blank line, then 'Initializing BL ...'.
inline void emitBoundaryLayerInitialization(std::ostream& out) {
  out << "\n Initializing BL ...\n";
}

// xbl.f:561: WRITE(*,*) ' side ', IS, ' ...' (list-directed; the
// integer fields follow the observed byte form: 8 + 13 + 5 characters).
inline void emitMarchingSideStart(std::ostream& out, int side) {
  out << "    side" << io::fortranI(13, side + 1) << "  ...\n";
}

// FORMAT 1300 (xbl.f:723-724): MRCHUE Inverse mode.
inline void emitMrchueInverseMode(std::ostream& out, int station,
                                  double hk) {
  out << " MRCHUE: Inverse mode at" << io::fortranI(4, station + 1)
      << "     Hk =" << io::fortranF(8, 3, hk) << "\n";
}

// FORMAT 1350 (xbl.f:779-781): MRCHUE Convergence failed.
inline void emitMrchueConvergenceFailure(std::ostream& out, int station,
                                         int side, double residual) {
  out << " MRCHUE: Convergence failed at" << io::fortranI(4, station + 1)
      << "  side" << io::fortranI(2, side + 1) << "    Res ="
      << io::fortranE(12, 4, residual) << "\n";
}

// FORMAT 1350 (xbl.f:1102-1104): MRCHDU Convergence failed.
inline void emitMrchduConvergenceFailure(std::ostream& out, int station,
                                         int side, double residual) {
  out << " MRCHDU: Convergence failed at" << io::fortranI(4, station + 1)
      << "  side" << io::fortranI(2, side + 1) << "    Res ="
      << io::fortranE(12, 4, residual) << "\n";
}

// TRCHEK2 label 100 (xblsys.f:423-425): WRITE(*,*) message +
// FORMAT 6700 1X,'x:',3F9.5,' N:',3F7.3,' Nx:',F8.3,' dN:',E10.3.
inline void emitTrchek2ConvergenceFailed(std::ostream& out, double x1,
                                         double xt, double x2, double ampl1,
                                         double amplt, double ampl2,
                                         double ax, double da2) {
  out << " TRCHEK2: N2 convergence failed.\n"
      << " x:" << io::fortranF(9, 5, x1) << io::fortranF(9, 5, xt)
      << io::fortranF(9, 5, x2) << "  N:" << io::fortranF(7, 3, ampl1)
      << io::fortranF(7, 3, amplt) << io::fortranF(7, 3, ampl2)
      << "  Nx:" << io::fortranF(8, 3, ax) << "   dN:"
      << io::fortranE(10, 3, da2) << "\n";
}

// SETBL Xtr warning (xbl.f:228): list-directed with default REAL
// field width 24; NaN appears with a leading sign field " NaN".
inline std::string fortranListReal24(double value) {
  if (std::isnan(value)) {
    // Canonical Fortran build (-fdefault-real-8): list-directed
    // NaN field width 26 = 22 blanks + ' NaN'.
    return std::string(21, ' ') + " NaN";
  }
  if (std::isinf(value)) {
    const std::string text = value > 0 ? "Infinity" : "-Infinity";
    return std::string(std::size_t{24} - text.size(), ' ') + text;
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*g", 17, value);
  const std::string text(buf);
  if (static_cast<int>(text.size()) >= 24) {
    return text;
  }
  return std::string(std::size_t{24} - text.size(), ' ') + text;
}

inline void emitSetblXtrWarning(std::ostream& out, double ampl1,
                                double ampl2) {
  out << " SETBL: Xtr???  n1 n2: "
      << " " << fortranListReal24(ampl1)
      << " " << fortranListReal24(ampl2) << "\n";
}

// FORMAT 1779 (xoper.f 1684-1685): inviscid Cp minimum.
inline void emitCpmnInviscid(std::ostream& out, double cpmni, double xcpmni) {
  out << "  Minimum Inviscid Cp =" << io::fortranF(8, 4, cpmni)
      << "   at x =" << io::fortranF(8, 4, xcpmni) << "\n";
}

// Format 1769 (xoper.f 1679-1681): viscous and inviscid Cp minimum.
inline void emitCpmnViscous(std::ostream& out, double cpmni, double xcpmni,
                            double cpmnv, double xcpmnv) {
  out << "  Minimum Inviscid Cp =" << io::fortranF(8, 4, cpmni)
      << "   at x =" << io::fortranF(8, 4, xcpmni) << "\n"
      << "  Minimum Viscous  Cp =" << io::fortranF(8, 4, cpmnv)
      << "   at x =" << io::fortranF(8, 4, xcpmnv) << "\n";
}

// Format 9200 (xbl.f:508): free transition.
inline void emitTransitionFree(std::ostream& out, int side, double xOverC,
                               int transitionStation) {
  out << " Side" << io::fortranI(2, side + 1)
      << "  free  transition at x/c = " << io::fortranF(7, 4, xOverC)
      << io::fortranI(5, transitionStation + 1) << "\n";
}

// FORMAT 9100 (xbl.f:505): forced transition.
inline void emitTransitionForced(std::ostream& out, int side, double xOverC,
                                 int transitionStation) {
  out << " Side" << io::fortranI(2, side + 1)
      << " forced transition at x/c = " << io::fortranF(7, 4, xOverC)
      << io::fortranI(5, transitionStation + 1) << "\n";
}

// Dispatcher: one event record -> exactly one canonical line.
inline void emitViscousEvent(std::ostream& out,
                             const core::boundary_layer::ViscousEventRecord& ev) {
  using core::boundary_layer::ViscousEventKind;
  switch (ev.kind) {
    case ViscousEventKind::boundaryLayerInitialization:
      emitBoundaryLayerInitialization(out);
      break;
    case ViscousEventKind::marchingSideStart:
      emitMarchingSideStart(out, ev.side);
      break;
    case ViscousEventKind::mrchueInverseMode:
      emitMrchueInverseMode(out, ev.station, ev.hk);
      break;
    case ViscousEventKind::mrchueConvergenceFailure:
      emitMrchueConvergenceFailure(out, ev.station, ev.side, ev.residual);
      break;
    case ViscousEventKind::mrchduConvergenceFailure:
      emitMrchduConvergenceFailure(out, ev.station, ev.side, ev.residual);
      break;
    case ViscousEventKind::transitionFree:
      emitTransitionFree(out, ev.side, ev.xOverC, ev.transitionStation);
      break;
    case ViscousEventKind::transitionForced:
      emitTransitionForced(out, ev.side, ev.xOverC, ev.transitionStation);
      break;
    case ViscousEventKind::sourceInfluenceCalculation:
      emitQdcalcProgress(out);
      break;
    case ViscousEventKind::stagnationPointNotFound:
      emitStfindStagnationNotFound(out);
      break;
    case ViscousEventKind::trchek2ConvergenceFailed:
      emitTrchek2ConvergenceFailed(out, ev.trchekX1, ev.trchekXt, ev.trchekX2,
                                   ev.trchekAmpl1, ev.trchekAmplt,
                                   ev.trchekAmpl2, ev.trchekAx, ev.trchekDa2);
      break;
    case ViscousEventKind::setblXtrWarning:
      emitSetblXtrWarning(out, ev.trchekAmpl1, ev.trchekAmpl2);
      break;
  }
}

// ---
// Polar presentation (xpol.f 755-836, xoper.f 1064-1135).
// ---

// FORMAT 1100 (xpol.f 757/760-764): PLRSUM table header (1X,A,A).
inline void emitPolarSummaryHeader(std::ostream& out) {
  out << "\n"
      << " "
      << "       airfoil                    Re           Mach     "
      << "  NcritT  NcritB  XtripT  XtripB       file\n"
      << " "
      << "      ------------------------  ------------  ----------"
      << "  ------  ------  ------  ------    -------------------\n";
}

// FORMAT 1200 (xpol.f 799-801): PLRSUM data line.
inline void emitPolarSummaryRow(std::ostream& out, char accumulationMark,
                                int slot1Based, const std::string& name,
                                double reMantissa, int reExponent,
                                const std::string& reTypeTag, double mach,
                                const std::string& machTypeTag,
                                double ncritTop, double ncritBottom,
                                double xtripTop, double xtripBottom,
                                char fileMark, const std::string& fileName) {
  out << " " << accumulationMark << io::fortranI(3, slot1Based) << "  "
      << io::fortranA(24, name) << io::fortranF(7, 3, reMantissa) << "e"
      << io::fortranI(1, reExponent) << io::fortranA(5, reTypeTag)
      << io::fortranF(7, 3, mach) << io::fortranA(5, machTypeTag)
      << io::fortranF(8, 2, ncritTop) << io::fortranF(8, 2, ncritBottom)
      << io::fortranF(8, 3, xtripTop) << io::fortranF(8, 3, xtripBottom)
      << "  " << fileMark << " " << fileName << "\n";
}

// Format 1100/1200 (xpol.f 811-828): PRFSUM header and data line.
inline void emitReferencePolarSummary(std::ostream& out,
                                      const std::vector<std::string>& labels) {
  out << "\n"
      << "       reference polar                          \n"
      << "      ------------------------------------------\n";
  for (std::size_t i = 0; i < labels.size(); ++i) {
    out << "  " << io::fortranI(3, static_cast<long>(i) + 1) << "  "
        << io::fortranA(48, labels[i]) << "\n";
  }
}

// Format 3100 (xoper.f 1065-1067): PLIS/PREM/PNAM polar header. The
// format contains two '/' slashes: blank, separator line, polar line.
inline void emitStoredPolarHeader(std::ostream& out, int slot1Based) {
  out << "\n"
      << " " + std::string(62, '=') + "\n"
      << " Polar" << io::fortranI(3, slot1Based) << "\n";
}

// Format 1310 (xoper.f 1123): PLRCOP shift line. RDEL uses
// canonically the same format label (xoper.f 1010 WRITE(*,1310); the
// Format 1410 defined there is dead code).
inline void emitPolarMoved(std::ostream& out, int from1Based, int to1Based) {
  out << " Polar" << io::fortranI(3, from1Based)
      << "  moved into polar" << io::fortranI(3, to1Based) << "\n";
}

// FORMAT 1320 (xoper.f 1133-1134).
inline void emitPolarNowActive(std::ostream& out, int slot1Based) {
  out << " Polar" << io::fortranI(3, slot1Based) << "  is now active\n";
}

// FORMAT 5000 (xoper.f 57-58): OPER entry, IPACT /= 0.
inline void emitPolarActiveAtOperEntry(std::ostream& out, int slot1Based) {
  out << "\n  Polar" << io::fortranI(3, slot1Based) << "  is active\n";
}

// CINC/HINC messages (xoper.f 1689-1707, list-directed).
inline void emitCincEnabled(std::ostream& out) {
  out << " Min Cp will be written to polar save file\n";
}
inline void emitCincDisabled(std::ostream& out) {
  out << " Min Cp won't be written to polar save file\n";
}
inline void emitHincEnabled(std::ostream& out) {
  out << " Hinge moment will be written to polar save file\n";
}
// Note only for !LFLAP (xoper.f 1701-1704); LFLAP is held
// session-side, so the note is emitted conditionally.
inline void emitHincFlapUndefinedNote(std::ostream& out) {
  out << "\n"
      << " Note: Flap hinge location not defined\n"
      << "       Set it with FNEW,FMOM commands\n";
}
inline void emitHincDisabled(std::ostream& out) {
  out << " Hinge moment won't be written to polar save file\n";
}

// FORMAT 6600 (xpol.f 271-283): PLRINI value comparison current/save file.
inline void emitSaveParameterComparison(
    std::ostream& out, const std::string& currentName,
    const std::string& fileName, double currentRe, double fileRe,
    double currentMach, double fileMach, int currentRetyp, int fileRetyp,
    int currentMatyp, int fileMatyp, double currentNcritTop,
    double fileNcritTop, double currentNcritBottom, double fileNcritBottom,
    double currentXtripTop, double fileXtripTop, double currentXtripBottom,
    double fileXtripBottom) {
  out << "\n               Current                         Save file\n"
      << "           ------------------              ------------------\n"
      << " name  :   " << io::fortranA(48, currentName)
      << io::fortranA(48, fileName) << "\n"
      << " Re:   " << io::fortranF(12, 0, currentRe)
      << "                    " << io::fortranF(12, 0, fileRe) << "\n"
      << " Mach:   " << io::fortranF(12, 4, currentMach)
      << "                    " << io::fortranF(12, 4, fileMach) << "\n"
      << " Retyp:   " << io::fortranI(7, currentRetyp)
      << "                         " << io::fortranI(7, fileRetyp) << "\n"
      << " Matyp :   " << io::fortranI(7, currentMatyp)
      << "                         " << io::fortranI(7, fileMatyp) << "\n"
      << " NcritT:   " << io::fortranF(12, 4, currentNcritTop)
      << "                    " << io::fortranF(12, 4, fileNcritTop) << "\n"
      << " NcritB:   " << io::fortranF(12, 4, currentNcritBottom)
      << "                    " << io::fortranF(12, 4, fileNcritBottom) << "\n"
      << " xtr T :   " << io::fortranF(12, 4, currentXtripTop)
      << "                    " << io::fortranF(12, 4, fileXtripTop) << "\n"
      << " xtr B :   " << io::fortranF(12, 4, currentXtripBottom)
      << "                    " << io::fortranF(12, 4, fileXtripBottom)
      << "\n";
}

// FORMAT 6600 (xpol.f 471-479): PLXINI value comparison dump file/current.
inline void emitDumpParameterComparison(
    std::ostream& out, const std::string& fileName,
    const std::string& currentName, double fileRe, double currentRe,
    double fileMach, double currentMach, int fileRetyp, int currentRetyp,
    int fileMatyp, int currentMatyp, double fileNcritTop,
    double currentNcritTop, double currentNcritBottom) {
  out << "\n               Dump file                       Current\n"
      << "             ------------                    ------------\n"
      << " name  :   " << io::fortranA(48, fileName)
      << io::fortranA(48, currentName) << "\n"
      << " Re:   " << io::fortranF(12, 0, fileRe)
      << "                    " << io::fortranF(12, 0, currentRe) << "\n"
      << " Mach:   " << io::fortranF(12, 4, fileMach)
      << "                    " << io::fortranF(12, 4, currentMach) << "\n"
      << " Retyp:   " << io::fortranI(7, fileRetyp)
      << "                         " << io::fortranI(7, currentRetyp) << "\n"
      << " Matyp :   " << io::fortranI(7, fileMatyp)
      << "                         " << io::fortranI(7, currentMatyp) << "\n"
      << " NcritT:   " << io::fortranF(12, 4, fileNcritTop)
      << "                    " << io::fortranF(12, 4, currentNcritTop) << "\n"
      // Canonical Fortran typo (xpol.f 471-479, Format 6600
      // of the dump variant): the NcritB line prints ACRITX(1) again.
      << " NcritB:   " << io::fortranF(12, 4, fileNcritTop)
      << "                    " << io::fortranF(12, 4, currentNcritBottom)
      << "\n";
}

// ASKL prompt (userio.f 1000): FORMAT(/A,' y/n> ',$).
inline void emitYesNoPrompt(std::ostream& out, const std::string& prompt) {
  out << "\n" << prompt << " y/n>  ";
}

// --- Defaults texts (GETDEF/WRTDEF/WDEF, xfoil.f 867-1030,
// 309-345) -------------------------------------------------------------

// Format 1900: /' File ',A,' not found'. A describes the
// transfer string in its full length (startup literal 9 characters,
// RDEF-FNAME CHARACTER*80 with 80 characters).
inline void emitDefaultsNotFound(std::ostream& out,
                                 const std::string& displayName) {
  out << "\n File  " << displayName << "  not found\n";
}

// FORMAT 1050 (GETDEF LASK=.TRUE.): /' Read settings from file ',A,
// ' ? Y'.
inline void emitDefaultsReadPrompt(std::ostream& out,
                                   const std::string& displayName) {
  out << "\n  Read settings from file  " << displayName << " ?  Y\n";
}

// FORMAT 1600: /' Default parameters read in from file ',A,':' /
inline void emitDefaultsReadOk(std::ostream& out,
                               const std::string& displayName) {
  out << "\n Default parameters read in from file  " << displayName
      << ":\n\n";
}

// FORMAT 1800: /' File ',A,' read error' /' Settings may have been
// changed'.
inline void emitDefaultsReadError(std::ostream& out,
                                  const std::string& displayName) {
  out << "\n File  " << displayName << "  read error\n"
      << " Settings may have been changed\n";
}

// WDEF existence query, FORMAT 701: /' File ',A,' exists. Overwrite? Y'
inline void emitDefaultsOverwritePrompt(std::ostream& out,
                                        const std::string& displayName) {
  out << "\n  File  " << displayName << "  exists.  Overwrite?  Y\n";
}

// WDEF rejection branch: WRITE(*,*) / WRITE(*,*) 'No action taken'.
inline void emitDefaultsNoActionTaken(std::ostream& out) {
  out << "\n No action taken\n";
}

// WDEF success, FORMAT 708: /' File ',A,' written'
inline void emitDefaultsWritten(std::ostream& out,
                                const std::string& displayName) {
  out << "\n  File  " << displayName << "  written\n";
}

// GETDEF-EOF (xfoil.f 905+ READ(LU,*,ERR=80) without END=): canonical
// Fortran runtime error 'End of file' with backtrace on stderr and
// program abort. Controlled deviation: compact message without
// backtrace/source line, exit code 2.
inline void emitDefaultsEndOfFile(std::ostream& out) {
  out << " Fortran runtime error: End of file\n";
}

// Display values of the OPLSET menus (plutil.f 21-176, format 2000). The
// numeric values derive from the storage location
// DefaultsSessionState.values; graphics is IDEV.GE.1, color is
// IDEVRP.EQ.4, landscape is LLAND, cursor is LCURS.
struct PlotOptionDisplayValues {
  bool graphics{true};
  double size{10.0};
  double aspect{0.55};
  double xpage{11.0};
  double ypage{8.5};
  double xmarg{0.0};
  double ymarg{0.0};
  double font{0.015};
  double screenFraction{0.80};
  bool cursor{true};
  bool landscape{true};
  bool color{true};
  int colorTop{8};
  int colorBottom{3};
};

// OPLSET menu table, Format 2000 (plutil.f 61-73), byte-exact against
// design-state/plop-menu-return. The first record (dotted line) depends
// on the open ASKC prompt line for its own line feed; '//' produces the
// empty record before the G line. The records for Blowup/Orientation carry
// the 10-character Fortran strings 'Cursor '/'Keyboard ' and
// 'Landscape '/'Portrait '.
inline void emitPlotOptionsTable(std::ostream& out,
                                 const PlotOptionDisplayValues& v) {
  out << " ...............................................\n"
      << "\n"
      << "  G raphics-enable flag:       " << io::fortranL2(v.graphics) << "\n"
      << "  S ize of plot object         " << formatFortranF(6, 2, v.size)
      << "\"\n"
      << "  A spect ratio of plot object " << formatFortranF(8, 4, v.aspect)
      << "\n"
      << "  P age dimensions             " << formatFortranF(6, 2, v.xpage)
      << " x" << formatFortranF(6, 2, v.ypage) << "\"\n"
      << "  M argins from page edges     " << formatFortranF(6, 2, v.xmarg)
      << "\"," << formatFortranF(6, 2, v.ymarg) << "\"\n"
      << "  F ont size (relative)        " << formatFortranF(8, 4, v.font)
      << "\n"
      << "  W indow/screen size fraction "
      << formatFortranF(8, 4, v.screenFraction) << "\n"
      << "  B lowup input method:        "
      << (v.cursor ? "Cursor    " : "Keyboard  ") << "\n"
      << "  O rientation of plot:        "
      << (v.landscape ? "Landscape " : "Portrait  ") << "\n"
      << "  C olor PostScript output?    " << io::fortranL2(v.color) << "\n"
      << "  L ine colors for top,bottom  " << io::fortranI(3, v.colorTop)
      << io::fortranI(3, v.colorBottom) << "\n";
}

// OPLSET menu prompt, ASKC Format 1000 (userio.f): advance, text before '^',
// then ' c> ' without a line feed.
inline void emitPlotOptionsPrompt(std::ostream& out) {
  out << "\n      Option, Value   (or <Return>)    c>  ";
}

// OPLSET unknown option, WRITE(*,*) '*** Item not recognized ***'
// (plutil.f 173): list-directed with leading space.
inline void emitPlotOptionUnrecognized(std::ostream& out) {
  out << " *** Item not recognized ***\n";
}

// OPLSET 'O' message (plutil.f 140-141): two list-directed WRITE lines.
inline void emitPlotOrientationSwap(std::ostream& out) {
  out << "\n\n Swapping X,Y page dimensions\n";
}

// OPLSET 'L' color list, FORMAT 2200 (plutil.f 158-161) plus
// WRITE(*,*) 'Select top,bottom line-plot colors' (plutil.f 162).
inline void emitPlotColorMenu(std::ostream& out) {
  out << "\n  1  black  "
      << "\n  2  white  "
      << "\n  3  red    "
      << "\n  4  orange "
      << "\n  5  yellow "
      << "\n  6  green  "
      << "\n  7  cyan   "
      << "\n  8  blue   "
      << "\n  9  violet "
      << "\n 10  magenta"
      << "\n Select top,bottom line-plot colors\n";
}

// ---: GDES menu and command texts (xgdes.f, xgeom.f, xtcam.f) ------

// GDES entry without contour (xgdes.f 40-45): two list-directed WRITE; the
// first record acts as a line feed on the open TOP prompt line.
inline void emitGdesNoAirfoil(std::ostream& out) {
  out << "\n ***  No airfoil available  ***\n";
}

// GDES entry (xgdes.f 49-50): WRITE(*,*) / WRITE(*,*)
// 'You are working with the buffer airfoil'.
inline void emitGdesEntry(std::ostream& out) {
  out << "\n You are working with the buffer airfoil\n";
}

// GDES prompt (xgdes.f 61-65): ASKC('.GDESs^') for active LGSYM, otherwise
// ASKC('.GDES^'); FORMAT 1000 (/A,' c> ',$).
inline void emitGdesPrompt(std::ostream& out, bool symmetry) {
  out << "\n" << (symmetry ? ".GDESs" : ".GDES") << "   c>  ";
}

// GDES help Format 1050 (xgdes.f 113-161), byte-exact.
inline void emitGdesHelp(std::ostream& out) {
  out << "\n   <cr>     Return to Top Level"
      << "\n   !        Redo previous command"
      << "\n\n   GSET     Set buffer  airfoil <== current airfoil"
      << "\n   eXec     Set current airfoil <== buffer  airfoil"
      << "\n   SYMM     Toggle y-symmetry flag"
      << "\n\n   ADEG r   Rotate about origin (degrees)"
      << "\n   ARAD r   Rotate about origin (radians)"
      << "\n   Tran rr  Translate"
      << "\n   Scal r   Scale about origin"
      << "\n   LINS rr. Linearly-varying y scale"
      << "\n   DERO     Derotate (set chord line level)"
      << "\n\n   TGAP rr  Change trailing edge gap"
      << "\n   LERA rr  Change leading edge radius"
      << "\n\n   TCPL     Toggle thickness and camber plotting"
      << "\n   TFAC rr  Scale existing thickness and camber"
      << "\n   TSET rr  Set new thickness and camber"
      << "\n   HIGH rr  Move camber and thickness highpoints"
      << "\n  .CAMB     Modify camber shape directly or via loading"
      << "\n\n   BEND     Display structural properties of buffer airfoil"
      << "\n\n   Flap rrr Deflect trailing edge flap"
      << "\n\n   Modi     Modify contour via cursor"
      << "\n   SLOP     Toggle modified-contour slope matching flag"
      << "\n\n   CORN     Double point with cursor (set sharp corner)"
      << "\n   ADDP     Add    point with cursor or keyboard x,y"
      << "\n   MOVP     Move   point with cursor or keyboard x,y"
      << "\n   DELP     Delete point with cursor"
      << "\n   NMOV r   Move all points in surface-normal direction"
      << "\n\n   UNIT     Normalize buffer airfoil to unit chord"
      << "\n   Dist     Determine distance between 2 cursor points"
      << "\n   CLIS     List curvatures"
      << "\n   CPLO     Plot curvatures"
      << "\n   CANG     List panel corner angles"
      << "\n   CADD ri. Add points at corners exceeding angle threshold"
      << "\n\n   Plot     Replot buffer airfoil"
      << "\n   INPL     Replot buffer airfoil without scaling (in inches)"
      << "\n   Blow     Blowup plot region"
      << "\n   Rese     Reset plot scale and origin"
      << "\n   Wind     Plot window adjust via cursor and keys"
      << "\n\n   TSIZ r   Change tick-mark size"
      << "\n   TICK     Toggle node tick-mark plotting"
      << "\n   GRID     Toggle grid plotting"
      << "\n   GPAR     Toggle geometric parameter plotting"
      << "\n   Over f   Overlay disk file airfoil"
      << "\n\n   SIZE r   Change absolute plot-object size"
      << "\n  .ANNO     Annotate plot"
      << "\n   HARD     Hardcopy current plot"
      << "\n\n   NAME s   Specify new airfoil name"
      << "\n   NINC     Increment name version number\n";
}

// Unknown GDES command, Format 1100 (xgdes.f 944-945): A4 pads up to four
// characters.
inline void emitGdesUnknown(std::ostream& out, const std::string& comand) {
  std::string field = comand;
  field.resize(4, ' ');
  out << " Command " << field
      << " not recognized.  Type a \" ? \" for list.\n";
}

// '!' without a valid predecessor (xgdes.f 63-64).
inline void emitGdesPreviousNotValid(std::ostream& out) {
  out << " Previous .GDES command not valid\n";
}

// Exit warning (xgdes.f 80-82) for .NOT.LGSAME.
inline void emitGdesExitWarning(std::ostream& out) {
  out << "\n Buffer airfoil is not identical to current airfoil\n";
}

// ---: CAMB submenu (xtcam.f) --------------------------------------

// List-directed REAL(8) field (gfortran): 17 decimal places,
// left-aligned in 24 characters, inter-item gap of two spaces after the
// prompt text.
// List-directed REAL(8) output pattern of gfortran: 17 significant
// digits; fixed-point for exponent -3..16, otherwise E form with a
// three-digit exponent. Return value without field width.
inline std::string fortranListReal8Digits(double value) {
  if (value == 0.0) return "0.0000000000000000";
  const double av = std::fabs(value);
  const int e = static_cast<int>(std::floor(std::log10(av)));
  if (e >= -3 && e <= 16) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", 16 - e, value);
    return std::string(buf);
  }
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*e", 16, value);
  std::string s(buf);
  const std::size_t ep = s.find('e');
  std::string mant = s.substr(0, ep);
  int exp = std::stoi(s.substr(ep + 1));
  char out[64];
  std::snprintf(out, sizeof out, "%sE%c%03d", mant.c_str(),
                exp < 0 ? '-' : '+', std::abs(exp));
  return std::string(out);
}

inline std::string formatFortranListReal8(double value) {
  std::string field(24, ' ');
  const std::string digits = fortranListReal8Digits(value);
  if (digits.size() >= field.size()) return digits;
  field.replace(field.size() - digits.size(), digits.size(), digits);
  return field;
}

// Complete list-directed field: fixed-point representations are placed
// right-justified in 22 characters and padded with the gfortran-typical 5
// spaces; E form uses 24 characters without edge trimming.
inline std::string formatFortranListReal8Field(double value) {
  const std::string digits = fortranListReal8Digits(value);
  if (digits.find('E') == std::string::npos) {
    std::string field(22, ' ');
    if (digits.size() >= field.size()) return digits + "     ";
    field.replace(field.size() - digits.size(), digits.size(), digits);
    return field + "     ";
  }
  std::string field(24, ' ');
  if (digits.size() >= field.size()) return digits;
  field.replace(field.size() - digits.size(), digits.size(), digits);
  return field;
}

// CAMB prompt (xtcam.f 168 ASKC('..CAMB^'), FORMAT 1000 (/A,' c> ',$)).
inline void emitCambPrompt(std::ostream& out) {
  out << "\n..CAMB   c>  ";
}

// CAMB help Format 1200 (xtcam.f 206-229), byte-exact. It is output at
// entry (xtcam.f 77) and for '?' (xtcam.f 204-205).
inline void emitCambHelp(std::ostream& out) {
  out << "\n   <cr>    Return to GDES"
      << "\n   TFAC rr Scale existing thickness and camber"
      << "\n   TSET rr Set new thickness and camber"
      << "\n   HIGH rr Move camber and thickness highpoints"
      << "\n   WRTT f  Write airfoil thickness x/c,t/c to file"
      << "\n   WRTC f  Write airfoil camber    x/c,y/c to file"
      << "\n\n   RDAC    Read   added camber  x/c,y/c from file"
      << "\n   SETC    Set    added camber  x/c,y/c from camberline"
      << "\n   INPC    Input  added camber  x/c,y/c from keyboard"
      << "\n   MODC    Modify added camber  x/c,y/c with cursor"
      << "\n   INPP    Input  added loading x/c,DCp from keyboard"
      << "\n   MODP    Modify added loading x/c,DCp with cursor"
      << "\n   SLOP    Toggle modified-camber,dCp slope matching flag"
      << "\n   SCAL r  Scale the added camber"
      << "\n   CLR     Clear the added camber"
      << "\n   ADD     Add added camber to the existing camberline"
      << "\n\n   DCPL    Toggle DCp plot"
      << "\n   CPLI rr Change DCp axis plot limits"
      << "\n\n   Blow    Blowup plot region"
      << "\n   Rese    Reset plot scale and origin"
      << "\n\n   SIZE r   Change absolute plot-object size"
      << "\n  .ANNO     Annotate plot"
      << "\n   HARD     Hardcopy current plot\n";
}

// Unknown CAMB command, Format 8000 (xtcam.f 771-772): A4, then
// ' command not recognized. Type a "?" for list'.
inline void emitCambUnknown(std::ostream& out, const std::string& comand) {
  std::string field = comand;
  field.resize(4, ' ');
  out << " " << field
      << " command not recognized.  Type a \"?\" for list\n";
}

// '!' without a valid predecessor (xtcam.f 172-173).
inline void emitCambPreviousNotValid(std::ostream& out) {
  out << " Previous ..CAMB command not valid\n";
}

// LGSYM deactivation before CAMB (xgdes.f 488-490).
inline void emitCambDisablingSymmetry(std::ostream& out) {
  out << " Disabling symmetry enforcement.\n";
}

// LPLCAM enable message (xtcam.f 54).
inline void emitCambEnablingCamberPlot(std::ostream& out) {
  out << " Enabling camber,thickness plotting\n";
}

// Rotation warning (xtcam.f 67-70): WRITE(*,*) ' ', warning,
// 'Current chordline angle: ' list-directed AROT, 'Proceeding anyway...'.
inline void emitCambRotationWarning(std::ostream& out, double arot) {
  out << "  \n Warning: CAMB does not work well on rotated foils\n"
      << " Current chordline angle:" << formatFortranListReal8Field(arot)
      << "\n Proceeding anyway...\n";
}

// Block-200 header (xtcam.f 138-139): WRITE(*,*) / WRITE(*,*)
// 'Buffer airfoil thickness and camber:'.
inline void emitCambBlock200Header(std::ostream& out) {
  out << "\n Buffer airfoil thickness and camber:\n";
}

// GETMAX diagnostics (xtcam.f 1349-1350 WRITE(*,*)): list-directed
// character field with leading space, without edge trimming.
inline void emitCambGetmaxFailure(std::ostream& out) {
  out << " GETMAX: Newton iteration for max camber/thickness failed.\n";
}

// Added-camber incidence report FORMAT 1100 (xtcam.f 154-156).
inline void emitCambIncidenceReport(std::ostream& out, double aleDeg,
                                    double ateDeg, double maxYc,
                                    double maxXc) {
  out << "\n Added camber line incidence at LE = "
      << formatFortranF(6, 2, aleDeg) << "  deg."
      << "\n Added camber line incidence at TE = "
      << formatFortranF(6, 2, ateDeg) << "  deg."
      << "\n Max added camber y/c = " << formatFortranF(8, 4, maxYc)
      << "  at x/c = " << formatFortranF(7, 3, maxXc) << "\n";
}

// Delta-CL/Delta-CM report FORMAT 1110 (xtcam.f 777-778).
inline void emitCambDeltaClCm(std::ostream& out, double clx, double cmx) {
  out << "\n Delta Cp loading gives delta CL = " << formatFortranF(7, 3, clx)
      << "\n                        delta CM = " << formatFortranF(7, 3, cmx)
      << "\n";
}

// SLOP messages (xtcam.f 688-694), list-directed two-record line.
inline void emitCambSlopTangent(std::ostream& out) {
  out << " Modified segment will be made tangent at endpoints\n";
}
inline void emitCambSlopNotTangent(std::ostream& out) {
  out << " Modified segment will not be made tangent at endpoints\n";
}

// INPC prompt block FORMAT 2000 (xtcam.f 400-403).
inline void emitCambManualCamberInput(std::ostream& out) {
  out << "\n Manual input of camber x/c,y/c:\n"
      << "\n Input x/c, y/c pairs from  x/c = 0  to  x/c = 1\n"
      << " <cr> ends input\n";
}

// INPP prompt block FORMAT 3000 (xtcam.f 449-452).
inline void emitCambManualLoadingInput(std::ostream& out) {
  out << "\n Manual input of loading x/c, DCp:\n"
      << "\n Input x/c, DCp pairs from  x/c = 0  to  x/c = 1\n"
      << " <cr> ends input\n";
}

// INPC/INPP read error (xtcam.f 427/490).
inline void emitCambTryAgain(std::ostream& out) {
  out << " try again\n";
}

// SOPPS failure (xgeom.f 286-287): list-directed WRITE per failure,
// leading space of the records. Counts the kernel (oppositePointFailures).
inline void emitSoppsFailure(std::ostream& out, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    out << " SOPPS: Opposite-point location failed. Continuing...\n";
  }
}

// LEFIND failure (xgeom.f 64): list-directed WRITE per failed
// LEFIND call. Counts the kernel (lefindFailures).
inline void emitLefindNotFound(std::ostream& out, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    out << " LEFIND:  LE point not found.  Continuing...\n";
  }
}

// OVER default prompt (xgdes.f 2277-2279 FORMAT 1100 /' Enter filename: ',A):
// ONAME is CHARACTER*64 and is written including edge
// blanks.
inline void emitGdesOverlayDefaultPrompt(std::ostream& out,
                                         const std::string& oname) {
  std::string field = oname.substr(0, 64);
  field.resize(64, ' ');
  out << "\n Enter filename:  " << field << "\n";
}

// ASKI prompt (userio.f FORMAT 1000 /A,' i> ',$): leading
// line break, prompt without '^', then ' i> ' without line end.
inline void emitAskiPrompt(std::ostream& out, std::string_view promptText) {
  out << "\n" << promptText << "   i>  ";
}

// SYMM messages (xgdes.f 207/210).
inline void emitSymmetryEnabled(std::ostream& out) {
  out << " y-symmetry forcing enabled.\n";
}
inline void emitSymmetryDisabled(std::ostream& out) {
  out << " y-symmetry forcing disabled.\n";
}

// FLAP disabled symmetry (xgdes.f 1801-1804): WRITE(*,*) /
// WRITE(*,*) 'Disabling symmetry enforcement'; list-directed records with
// leading space, the first record is empty.
inline void emitFlapDisablingSymmetry(std::ostream& out) {
  out << "\n\n Disabling symmetry enforcement\n";
}

// DERO FORMAT 1080 (xgdes.f 398-399).
inline void emitDerotateMessage(std::ostream& out, double aradDeg) {
  out << "\nRotating buffer airfoil by " << formatFortranF(8, 3, aradDeg)
      << " deg.\n";
}

// ZERCAM message (xtcam.f 855).
inline void emitZeroCamber(std::ostream& out) {
  out << " Setting current camber to zero.\n";
}

// TCSCAL/TCSET/HIPNT initial report, FORMAT 1000/1010 (xtcam.f
// 946-947/969-970/1138-1139): leading '/' record, trailing '/'.
inline void emitThicknessCamberReport(std::ostream& out, double thickMax,
                                      double thickX, double camberMax,
                                      double camberX) {
  out << "\n Max thickness = " << formatFortranF(8, 4, thickMax)
      << "  at x = " << formatFortranF(7, 3, thickX) << "\n"
      << " Max camber    = " << formatFortranF(8, 4, camberMax)
      << "  at x = " << formatFortranF(7, 3, camberX) << "\n\n";
}

// TCBUF report (xtcam.f 886-893, Format 1000 without leading and
// closing '/' record): the Block-200 header and the leading '/'
// of the incidence report deliver the surrounding blank lines.
inline void emitTcbufPlain(std::ostream& out, double thickMax,
                           double thickX, double camberMax,
                           double camberX) {
  out << " Max thickness = " << formatFortranF(8, 4, thickMax)
      << "  at x = " << formatFortranF(7, 3, thickX) << "\n"
      << " Max camber    = " << formatFortranF(8, 4, camberMax)
      << "  at x = " << formatFortranF(7, 3, camberX) << "\n";
}

// TFAC/TSET/HIGH enable the camber display (xgdes.f 438/455/472).
inline void emitEnablingCamberPlot(std::ostream& out) {
  out << " Enabling camber,thickness plotting\n";
}

// TCSET 'Questionable input...' FORMAT 1100 (xtcam.f 1004-1007).
inline void emitQuestionableScaling(std::ostream& out, double tfac,
                                    double cfac) {
  out << "\n Questionable input..."
      << "\n Implied scaling factors are:" << formatFortranF(13, 2, tfac)
      << " x thickness"
      << "\n                             " << formatFortranF(13, 2, cfac)
      << " x camber   \n";
}

// TCSET abort (xtcam.f 1010).
inline void emitNoActionTaken(std::ostream& out) {
  out << " No action taken\n";
}

// ASKL prompt (userio.f FORMAT 1100: /A,' y/n> ',$).
inline void emitAsklPrompt(std::ostream& out, const std::string& promptText) {
  const std::size_t caret = promptText.find('^');
  const std::string prompt = caret == std::string::npos
                                 ? promptText
                                 : promptText.substr(0, caret);
  out << "\n" << prompt << " y/n>  ";
}

// TGAP 'Current gap =' FORMAT 1000 (xgdes.f 1254-1255).
inline void emitCurrentGap(std::ostream& out, double gap) {
  out << "\n Current gap =" << formatFortranF(9, 5, gap) << "\n";
}

// LERA 'New LE radius = ' FORMAT 1000 (xgdes.f 1364-1365).
inline void emitNewLeRadius(std::ostream& out, double radius) {
  out << "\n New LE radius = " << formatFortranF(7, 5, radius) << "\n";
}

// FLAP GETXYF FORMAT 1000 (xgdes.f 1894-1896).
inline void emitFlapSurfaces(std::ostream& out, double topy, double boty) {
  out << "\n  Top    surface:  y =" << formatFortranF(8, 4, topy)
      << "     y/t = 1.0\n"
      << "  Bottom surface:  y =" << formatFortranF(8, 4, boty)
      << "     y/t = 0.0\n";
}

// FLAP 'Flap hinge: x,y =' FORMAT 1050 (xgdes.f 1460-1461).
inline void emitFlapHinge(std::ostream& out, double xbf, double ybf) {
  out << "\n Flap hinge: x,y =" << formatFortranF(9, 5, xbf)
      << formatFortranF(9, 5, ybf) << "\n";
}

// FLAP break points Format 1100 (xgdes.f 1508-1511), 4X = four spaces.
inline void emitFlapBreaks(std::ostream& out, double xt1, double yt1,
                           double xt2, double yt2, double xb1, double yb1,
                           double xb2, double yb2) {
  out << "\n Top breaks: x,y =  " << formatFortranF(9, 5, xt1)
      << formatFortranF(9, 5, yt1) << "    " << formatFortranF(9, 5, xt2)
      << formatFortranF(9, 5, yt2) << "\n"
      << " Bot breaks: x,y =  " << formatFortranF(9, 5, xb1)
      << formatFortranF(9, 5, yb1) << "    " << formatFortranF(9, 5, xb2)
      << formatFortranF(9, 5, yb2) << "\n";
}

// CANG Format 1050/1100/1200 (xgeom.f 1154-1158). index is 1-based.
inline void emitCangHeader(std::ostream& out) {
  out << "\n  i       x        y      angle\n";
}
inline void emitCangRow(std::ostream& out, int index, double x, double y,
                        double angle) {
  out << " " << std::setw(3) << index << formatFortranF(9, 4, x)
      << formatFortranF(9, 4, y) << formatFortranF(9, 3, angle) << "\n";
}
// emitMaxPanelCornerAngle (Format 1200) already exists above.

// CLIS Format 1050/1100/1200 (xgeom.f 914-919). index is 1-based.
inline void emitClisHeader(std::ostream& out) {
  out << "\n  i        x         y         s       theta        curv\n";
}
inline void emitClisRow(std::ostream& out, int index, double x, double y,
                        double s, double thetaDeg, double curvature) {
  out << " " << std::setw(3) << index << formatFortranF(10, 5, x)
      << formatFortranF(10, 5, y) << formatFortranF(10, 5, s)
      << formatFortranF(11, 3, thetaDeg) << formatFortranF(12, 3, curvature)
      << "\n";
}
inline void emitMaxCurvature(std::ostream& out, double curvature, int index,
                             double x, double y, double s) {
  out << "\n Maximum curvature =" << formatFortranF(14, 3, curvature)
      << "   at  i,x,y,s  = " << std::setw(3) << index
      << formatFortranF(9, 4, x) << formatFortranF(9, 4, y)
      << formatFortranF(9, 4, s) << "\n";
}

// BENDUMP FORMAT 1200 (xgeom.f 1316-1364): list-directed label+G14.6.
// TOP BEND and GDES BEND use the same text.
inline void emitBendDump(std::ostream& out,
                         const core::geometry::SectionBending& b) {
  const auto line = [&out](const char* label, double value) {
    out << ' ' << label << formatFortranG(14, 6, value) << "\n";
  };
  const auto labelOnly = [&out](const char* text) {
    out << ' ' << text << "\n";
  };
  const double xbar = std::max(std::abs(b.xmax - b.xc), std::abs(b.xmin - b.xc));
  const double ybar = std::max(std::abs(b.ymax - b.yc), std::abs(b.ymin - b.yc));
  const double xbart =
      std::max(std::abs(b.xmax - b.xct), std::abs(b.xmin - b.xct));
  const double ybart =
      std::max(std::abs(b.ymax - b.yct), std::abs(b.ymin - b.yct));
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
}

}  // namespace xfoil::app::cli

// End: src/app/cli/CliFortranText.h

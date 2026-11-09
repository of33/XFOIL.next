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

// File: src/app/DatFileReader.h
// Internal header of the application layer: reading
// XFoil DAT airfoil files. No public library API, no output,
// no fixed array bounds.

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "xfoil/Limits.h"
#include "xfoil/Types.h"

namespace xfoil::app {

// Coordinate is an alias for the modern point type.
// Existing code (coordinate.x, coordinate.y, std::vector<Coordinate>)
// remains source-compatible and functional.
using Coordinate = xfoil::Point2D;

struct AirfoilInput {
  std::string name;
  std::vector<Coordinate> coordinates;
};

enum class DatReadError {
  none,
  cannotOpen,
  missingName,
  invalidCoordinate,
  tooFewCoordinates,
  coordinateOverflow,
  elementNotFound
};

// File type corresponding to the ITYPE return value of SUBROUTINE AREAD from
// fortran-src/aread.f. The value msesMulti is set by the caller after
// an element selection has been made (AREAD itself issues the ASKI prompt).
enum class CoordinateFileFormat {
  none,         // ITYPE = 0: read error or no selection completed
  generic,      // ITYPE = 1: plain coordinate file without names
  labeled,      // ITYPE = 2: labeled coordinate file
  msesSingle,   // ITYPE = 3: MSES single-element file with domain line
  msesMulti     // ITYPE = 4: MSES multi-element file (element selected)
};

// Result of the AREAD-compatible reading path. Like the
// existing read path, the reader never writes to stdout or stderr;
// callers format AREAD's original messages from the structured
// data.
struct CoordinateFileReadResult {
  CoordinateFileFormat format{CoordinateFileFormat::none};
  bool elementSelectionRequired{};  // first 999.0/999.0 marker without selection
  DatReadError error{DatReadError::none};
  std::size_t overflowLimit{};      // point limit for coordinateOverflow
  std::string name;                 // raw name line (not trimmed)
  std::string ispars;               // raw MSES domain line, otherwise empty
  std::vector<Coordinate> coordinates;

  [[nodiscard]] bool success() const noexcept {
    return error == DatReadError::none &&
           format != CoordinateFileFormat::none;
  }
};

struct DatReadResult {
  DatReadError error{DatReadError::none};
  AirfoilInput airfoil;
  std::size_t lineNumber{};
  std::string errorMessage;

  [[nodiscard]] bool success() const noexcept {
    return error == DatReadError::none;
  }
};

class DatFileReader {
 public:
  [[nodiscard]]
  static DatReadResult read(const std::filesystem::path& path);

 // AREAD-compatible reading of coordinate files:
  // supports plain, labeled and MSES file formats, skips comment
  // lines starting with '#' or '!', recognizes the 999.0/999.0
  // element marker and checks the point capacity (Fortran IBX).
  // With selectedElement <= 0 it behaves like AREAD up to the first
  // element marker and reports elementSelectionRequired; after a
  // selection the same path is read again with the chosen element
  // number. maxPoints corresponds to the NMAX argument of the
  // respective AREAD call (LOAD: IBX=2404, OVER: 2*IQX=1202).
  [[nodiscard]]
  static CoordinateFileReadResult readCoordinateFile(
      const std::filesystem::path& path,
      int selectedElement = 0,
      int maxPoints = static_cast<int>(xfoil::limits::maxInputCoordinates));
};

}  // namespace xfoil::app

// End: src/app/DatFileReader.h

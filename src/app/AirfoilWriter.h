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

// File: src/app/AirfoilWriter.h
// Safe coordinate-file writing core.
// Reproduces the non-graphical output semantics of SUBROUTINE SAVE in
// fortran-src/xfoil.f (IFTYP = 1 labeled, 0 plain, -1 integer, 2 ISES)
// including the LCLOCK writing direction and KDELIM separators. The core
// never writes to stdout or stderr and does not silently overwrite an
// existing file; the original overwrite prompt is issued by the
// interpreter before the call and confirmed via
// AirfoilWriteOptions::overwriteExisting.

#pragma once

#include <filesystem>
#include <span>
#include <string>

#include "xfoil/Types.h"

namespace xfoil::app {

// Output formats corresponding to the IFTYP branches of SUBROUTINE SAVE.
enum class AirfoilOutputFormat {
  labeled,             // SAVE: name in the first line + coordinates
  plain,               // PSAV: coordinates only
  integerCoordinates,  // USAV: integer rounded coordinates (I12, I12)
  ises                 // ISAV: name + ISES domain line + coordinates
};

// Separator corresponding to KDELIM from fortran-src/xfoil.f.
enum class CoordinateDelimiter {
  blank,  // KDELIM = 0: FORMAT(1X,G15.7,G15.7)
  comma,  // KDELIM = 1: Format(1X,F10.6,A,F10.6) with BSTRIP
  tab     // KDELIM = 2: as comma with tab character
};

struct AirfoilWriteOptions {
  AirfoilOutputFormat format{AirfoilOutputFormat::labeled};
  CoordinateDelimiter delimiter{CoordinateDelimiter::blank};
  bool clockwise{};                 // LCLOCK: reversed write pass
  std::string name;                 // already trimmed airfoil name (Name(1:NNAME))
  std::string isesDomainParameters; // ISPARS; right-hand trimmed written
  // Only set enables replacing an already existing file.
  bool overwriteExisting{};
};

struct AirfoilWriteResult {
  bool success{};
  bool existingFileNotOverwritten{};  // Kernel spared the existing file
  std::string diagnostic;             // pure state info, no stdout text
};

// Writes the passed coordinates in the original counting order (for
// a clockwise backward pass) into the file. The write happens via
// a temporary file with subsequent replace; on error the target
// file remains unchanged. An empty point set writes only the
// header lines of the chosen format.
[[nodiscard]]
AirfoilWriteResult writeAirfoil(
    const std::filesystem::path& path,
    std::span<const xfoil::Point2D> coordinates,
    const AirfoilWriteOptions& options);

}  // namespace xfoil::app

// End: src/app/AirfoilWriter.h

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

// File: src/gui/app/GuiFileAdapters.h
// App-only file adapters. They reuse the accepted DAT reader, airfoil writer
// and DXF encoder; the embeddable XFoilWidget remains file-I/O free.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "xfoil/AnalysisData.h"
#include "xfoil/PolarData.h"
#include "xfoil/Result.h"
#include "xfoil/geometry/AirfoilGeometry.h"

namespace xfoil::gui::app {

enum class AirfoilFileReadStatus {
  success,
  nameRequired,
  elementSelectionRequired,
  error
};

struct AirfoilFileReadResult {
  AirfoilFileReadStatus status{AirfoilFileReadStatus::error};
  geometry::AirfoilGeometry geometry;
  std::string message;
};

// Reads the existing XFOIL coordinate-file formats through DatFileReader.
// Generic coordinate files need a profile name; MSES multi-element files
// need a 1-based element selection. No format is invented here.
[[nodiscard]]
AirfoilFileReadResult readAirfoilFile(
    const std::filesystem::path& path, int selectedElement = 0,
    std::string_view suppliedName = {});

struct FileWriteResult {
  bool success{};
  std::string message;
};

// Writes the current owning geometry with the existing labeled SAVE writer.
// The caller supplies the overwrite decision; no extension is appended.
[[nodiscard]]
FileWriteResult saveAirfoilFile(
    const std::filesystem::path& path,
    const geometry::AirfoilGeometry& geometry, bool overwriteExisting);

// Writes the existing current panel contour through the accepted DXF R12
// encoder. The caller supplies the overwrite decision; no extension is
// appended.
[[nodiscard]]
FileWriteResult exportDxfFile(const std::filesystem::path& path,
                               const SurfaceGeometry& panel,
                               bool overwriteExisting);

// Reads one accepted PolarText file into an owning value. File and path
// handling stays in the application adapter.
[[nodiscard]]
Result<PolarData> readPolarFile(const std::filesystem::path& path);

// Writes the owning polar through the existing PolarText contract. No
// extension is appended and the caller owns the overwrite decision.
[[nodiscard]]
FileWriteResult savePolarFile(const std::filesystem::path& path,
                              const PolarData& polar,
                              bool overwriteExisting);

}  // namespace xfoil::gui::app

// End: src/gui/app/GuiFileAdapters.h

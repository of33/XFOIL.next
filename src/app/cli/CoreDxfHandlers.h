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

// File: src/app/cli/CoreDxfHandlers.h
// S16 DXF command follow-up: hidden top-level command file wiring around the
// accepted S14 encoder. No format or encoder logic lives here.

#pragma once

#include <ostream>
#include <string_view>

#include "xfoil/session/SessionState.h"

namespace xfoil::app::cli {

enum class DxfExportStatus {
  written,
  filenamePromptRequested,
  overwriteConfirmationRequired,
  notWrittenDeclined,
  badFilename,
};

// Hidden top-level DXF command (xfoil.f:189-190 DXFOUT).
// Resolves the logical filename, applies OutputPathPolicy exactly once at
// the open point, handles the overwrite contract, encodes the current panel
// contour through the accepted S14 DXF R12 encoder and writes canonical LF
// bytes. Invalid paths are non-fatal and leave no output file. No session
// geometry, panel, analysis, polar or output state is mutated.
DxfExportStatus handleDxfExportCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view argument, bool hasArgument,
    std::string_view filenameAnswer, bool hasFilenameAnswer,
    std::string_view overwriteAnswer, bool hasOverwriteAnswer);

}  // namespace xfoil::app::cli

// End: src/app/cli/CoreDxfHandlers.h

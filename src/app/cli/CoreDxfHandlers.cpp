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

// File: src/app/cli/CoreDxfHandlers.cpp
// S16 DXF command follow-up. The S14 encoder owns the bytes; this file owns
// only the hidden command's filename, overwrite, path-policy and write
// behavior (xfoil.f:1710-1829).

#include "app/cli/CoreDxfHandlers.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "app/cli/CliFortranText.h"
#include "xfoil/io/DxfExport.h"
#include "xfoil/io/OutputPathPolicy.h"
#include "xfoil/io/TextFormat.h"
#include "xfoil/session/SessionOps.h"

namespace xfoil::app::cli {
namespace {

void emitDxfBadFilename(std::ostream& out) {
  out << " Bad filename.\n"
         " DXF file not written.\n";
}

void emitDxfNotWritten(std::ostream& out) {
  out << " DXF file not written.\n";
}

}  // namespace

DxfExportStatus handleDxfExportCore(
    xfoil::session::SessionComposition& session, std::ostream& out,
    std::string_view argument, bool hasArgument,
    std::string_view filenameAnswer, bool hasFilenameAnswer,
    std::string_view overwriteAnswer, bool hasOverwriteAnswer) {
  std::string logicalName;
  if (hasArgument) {
    logicalName = io::stripBlanks(argument);
  }
  if (logicalName.empty()) {
    if (!hasFilenameAnswer) {
      emitAsksPrompt(out, "Enter output filename");
      return DxfExportStatus::filenamePromptRequested;
    }
    logicalName = io::stripBlanks(filenameAnswer);
    if (logicalName.empty()) {
      emitDxfBadFilename(out);
      return DxfExportStatus::badFilename;
    }
  }

  // Exactly one path-policy transformation at the open point. The prompt
  // and messages keep the logical name.
  const std::filesystem::path target{
      io::applyOutputPathPolicy(session.outputPaths, logicalName)};
  std::error_code error;
  const bool exists = std::filesystem::exists(target, error);
  if (error || std::filesystem::is_directory(target, error)) {
    emitDxfBadFilename(out);
    return DxfExportStatus::badFilename;
  }

  if (exists) {
    if (!hasOverwriteAnswer) {
      out << "\n Output file exists.  Overwrite?  Y\n";
      return DxfExportStatus::overwriteConfirmationRequired;
    }
    const char first =
        overwriteAnswer.empty() ? ' ' : overwriteAnswer.front();
    if (first == 'N' || first == 'n') {
      emitDxfNotWritten(out);
      return DxfExportStatus::notWrittenDeclined;
    }
  }

  const auto& panel = session::currentPanelGeometry(session);
  const auto encoded = io::dxfR12TextChecked(panel.x, panel.y);
  if (encoded.isErr()) {
    emitDxfBadFilename(out);
    return DxfExportStatus::badFilename;
  }

  std::ofstream stream(target, std::ios::binary | std::ios::trunc);
  if (!stream) {
    emitDxfBadFilename(out);
    return DxfExportStatus::badFilename;
  }
  stream << encoded.value();
  stream.close();
  if (!stream) {
    emitDxfNotWritten(out);
    return DxfExportStatus::badFilename;
  }
  return DxfExportStatus::written;
}

}  // namespace xfoil::app::cli

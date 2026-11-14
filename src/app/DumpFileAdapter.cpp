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

// File: src/app/DumpFileAdapter.cpp
// Application-side file adapter for the polar dump append path.
// Only path access, byte writing and file error mapping happen here.
// Append compatibility and byte encoding are provided by xfoil::io.

#include "app/DumpFileAdapter.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace xfoil::app {

core::Status appendDumpPointFile(const std::string& path,
                                 const io::DumpHeader& expected,
                                 const io::DumpPoint& point,
                                 io::DumpRealWidth realWidth) {
  std::ifstream existing(path, std::ios::binary);
  if (!existing) {
    return core::Status::fail(
        core::Error::make(core::ErrorCode::ioError,
                          "Cannot read existing polar dump file"));
  }
  std::ostringstream buffer;
  buffer << existing.rdbuf();
  const std::string contents = buffer.str();
  const io::DumpAppendCheck check =
      io::checkDumpAppend(reinterpret_cast<const std::uint8_t*>(contents.data()),
                          contents.size(), expected, realWidth);
  if (!check.compatible) {
    return core::Status::fail(
        core::Error::make(core::ErrorCode::invalidState, check.message));
  }
  const core::Result<std::vector<std::uint8_t>> encoded =
      io::encodeDumpPointChecked(point, realWidth);
  if (!encoded.isOk()) {
    return core::Status::fail(encoded.error());
  }
  std::ofstream stream(path, std::ios::app | std::ios::binary);
  if (!stream) {
    return core::Status::fail(
        core::Error::make(core::ErrorCode::ioError,
                          "Cannot append to polar dump file"));
  }
  stream.write(reinterpret_cast<const char*>(encoded.value().data()),
               static_cast<std::streamsize>(encoded.value().size()));
  stream.close();
  if (!stream.good()) {
    return core::Status::fail(
        core::Error::make(core::ErrorCode::ioError,
                          "Polar dump append write failed"));
  }
  return core::Status::ok();
}

}  // namespace xfoil::app

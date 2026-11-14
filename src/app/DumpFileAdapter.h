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

// File: src/app/DumpFileAdapter.h
// Application-side file adapter for the polar dump append path.
// The pure validation and byte encoding stay in xfoil::io; this adapter
// only reads the existing file, asks the codec for compatibility and
// bytes, and appends those bytes. Not part of the embedded library
// runtime boundary.

#pragma once

#include <string>

#include "xfoil/core/Result.h"
#include "xfoil/io/DumpCodec.h"

namespace xfoil::app {

// Appends one validated PLXADD group to an existing dump file. The
// existing file is read and checked before it is opened for writing; an
// incompatible or malformed file is left byte-identical.
[[nodiscard]] core::Status appendDumpPointFile(
    const std::string& path, const io::DumpHeader& expected,
    const io::DumpPoint& point,
    io::DumpRealWidth realWidth = io::DumpRealWidth::k8);

}  // namespace xfoil::app

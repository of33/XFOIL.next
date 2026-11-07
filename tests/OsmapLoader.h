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

// File: tests/OsmapLoader.h
// Diagnostics tool for tests: decodes the binary Fortran unformatted
// sequential file osmap.dat and hands the data to the numerical core via
// OrrSommerfeldMap::fromRaw. The production core performs no file-,
// environment- or path-I/O; that responsibility is assigned to.
#pragma once

#include <string>

#include "xfoil/core/Result.h"
#include "xfoil/core/inverse/OrrSommerfeldMap.h"

xfoil::core::Result<xfoil::core::inverse::OrrSommerfeldMap>
loadOsmapForTest(const std::string& path);

// End: tests/OsmapLoader.h

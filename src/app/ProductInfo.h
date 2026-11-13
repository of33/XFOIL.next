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

// File: src/app/ProductInfo.h
// Central internal product and version source of XFOIL.next.
// Header-only, no dynamic initialization, no public header
// below include/xfoil/, no dependency on session or core.
// The banner, --version and the product metadata line of results.dat
// read exclusively from these constants; a second hard-coded
// product version source in productive C++ code is not allowed.
//
// Delimitation: The value 6.99 in the dump header and polar text and the
// format markers `XFOIL file-format version 6.99`, `XFOIL-BATCH 1` and
// `# XFOIL_BATCH_RESULTS 1` are file or protocol versions and are
// deliberately not tied to these sources.

#pragma once

#include <string_view>

namespace xfoil::app::product {

inline constexpr std::string_view name = "XFOIL.next";
inline constexpr std::string_view version = "1.1.0";
inline constexpr std::string_view licenseId = "GPL-3.0-or-later";
inline constexpr std::string_view projectUrl =
    "https://github.com/of33/XFOIL.next";

}  // namespace xfoil::app::product

// End: src/app/ProductInfo.h

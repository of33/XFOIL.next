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

// File: include/xfoil/gui/Export.h
// Public visibility/import-export attribute for the optional GUI component.
// This is deliberately separate from the core XFOIL_API contract. A shared
// GUI artifact is not part of G1; source-based and static use remain the
// supported paths.

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(XFOIL_GUI_BUILD_SHARED)
#    define XFOIL_GUI_API __declspec(dllexport)
#  elif defined(XFOIL_GUI_USE_SHARED)
#    define XFOIL_GUI_API __declspec(dllimport)
#  else
#    define XFOIL_GUI_API
#  endif
#else
#  if defined(XFOIL_GUI_BUILD_SHARED)
#    define XFOIL_GUI_API __attribute__((visibility("default")))
#  else
#    define XFOIL_GUI_API
#  endif
#endif

// End: include/xfoil/gui/Export.h

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

// File: include/xfoil/Diagnostic.h
// Structured public diagnostic records. No streams, no formatted CLI
// transcript, no file or console dependency.

#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace xfoil {

enum class DiagnosticSeverity {
  info,
  warning,
  error
};

struct Diagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::info};
  std::string code;
  std::string message;
  std::optional<std::size_t> iteration;
};

}  // namespace xfoil

// End: include/xfoil/Diagnostic.h

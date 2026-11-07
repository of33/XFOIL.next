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

// File: src/io/OutputPathPolicy.cpp
// Implementation of the prefix transformation. No
// filesystem access, no validation of the prefix (the command
// line performs it before each file operation).

#include "xfoil/io/OutputPathPolicy.h"

namespace xfoil::io {

std::string applyOutputPathPolicy(const OutputPathPolicy& policy,
                                  const std::string& path) {
  // An empty filename stays empty.
  if (path.empty()) {
    return std::string{};
  }
  // Split at the last native separator; directory part including
  // separator, then the base name. PUB-W2: Windows paths use both
  // separators (Win32/cmd.exe accept '/' and '\'); POSIX keeps '/'
  // exclusively so a backslash stays a regular filename character
  // there (unchanged POSIX contract).
#ifdef _WIN32
  const std::string::size_type separator = path.find_last_of("/\\");
#else
  const std::string::size_type separator = path.find_last_of('/');
#endif
  if (separator == std::string::npos) {
    return policy.prefix + path;
  }
  const std::string directory = path.substr(0, separator + 1);
  const std::string base = path.substr(separator + 1);
  // A path ending in a separator is returned unchanged.
  if (base.empty()) {
    return path;
  }
  // Result: d + P + b.
  return directory + policy.prefix + base;
}

}  // namespace xfoil::io

// End: src/io/OutputPathPolicy.cpp

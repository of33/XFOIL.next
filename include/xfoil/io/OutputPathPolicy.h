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

// File: include/xfoil/io/OutputPathPolicy.h
// Central prefix contract for the native output commands.
// Self-contained data type without file, existence-check or solver reference;
// no Fortran COMMON mirror, but an app service injected via
// session::SessionComposition.
// The prefix validity (character set [A-Za-z0-9._-]*, 0-128 bytes)
// is checked by the caller (command line, exit 2); applyOutputPathPolicy sets a
// syntactically valid prefix ahead.

#ifndef XFOIL_IO_OUTPUTPATHPOLICY_H
#define XFOIL_IO_OUTPUTPATHPOLICY_H

#include <string>

namespace xfoil::io {

struct OutputPathPolicy {
  std::string prefix;  // empty means identity
};

// Transformation of directory/name.ext ->
// directory/PREFIXname.ext, likewise for absolute paths. An extension
// is not swapped; the prefix stands directly before the base name. An empty
// path stays empty; a path ending in a separator stays unchanged
// (defensive; the native OPEN error follows unchanged). The split happens at
// the last native directory separator: '/' on POSIX; '/' and '\' on
// Windows (drive letters, UNC and mixed-separator paths are therefore
// prefixed at the base name, e.g. C:\dir\foil.dat ->
// C:\dir\PREFIXfoil.dat). The transformation
// is applied exactly once per output file at the point that
// fixes the final target path (before existence check, read-before-write and
// open).
std::string applyOutputPathPolicy(const OutputPathPolicy& policy,
                                  const std::string& path);

}  // namespace xfoil::io

#endif  // XFOIL_IO_OUTPUTPATHPOLICY_H

// End: include/xfoil/io/OutputPathPolicy.h

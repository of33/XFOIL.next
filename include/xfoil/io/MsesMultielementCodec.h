// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

// File: include/xfoil/io/MsesMultielementCodec.h
// MSES multi-element coordinate file as a text codec (step,
// TOP MSAV). Decoding and encoding correspond exactly to the
// READ/WRITE statements of SUBROUTINE MSAVE from fortran-src/xfoil.f
// (1430-1544): two header lines in Format(A), coordinate pairs in
// Format(1X,2G15.7), element separator ' 999.0 999.0' as list-directed
// WRITE of a character constant. No CLI texts, no session relation,
// no file access.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace xfoil::io {

// One element of the MSES file: N coordinate pairs in file order.
struct MsesMultielementElement {
  std::vector<double> x;
  std::vector<double> y;
};

// Decoded MSES file. name/ispars hold the right-trimmed header content
// Name1(1:NN1) or ISPARS1(1:NI1) determined by MSAVE.
struct MsesMultielementFile {
  std::string name;
  std::string ispars;
  std::vector<MsesMultielementElement> elements;
};

enum class MsesDecodeStatus {
  ok,         // fully decoded
  readError,  // READ error (missing header or unreadable data)
  overflow    // element overflow: Fortran STOP 'LOAD: Array overflow'
};

// Decode result. residualElement is the Fortran remainder of the
// element count IEL after the read loop (1-based, value 1..6):
// - EOF in the middle of an element: IEL of the affected element,
// - after completing all NEX=5 elements: NEX+1 = 6.
// MSAVE returns this value unchanged on ASKI; an empty ASKI answer
// keeps exactly this value.
struct MsesDecodeResult {
  MsesDecodeStatus status{MsesDecodeStatus::readError};
  MsesMultielementFile file;
  int residualElement{1};
};

// Decodes the whole file text per MSAVE read logic.
// Canonical bounds: NEX=5 elements, per element at most 2*IQX+1 = 741
// coordinate pairs (IQX=370, XFOIL.INC). A ninth element or
// an element without a 999.0 marker within the capacity leads to status
// overflow. Unreadable data lines correspond to the canonical
// READ error that Fortran does not handle and are reported as readError
// (documented deviation: Fortran aborts with a runtime error).
[[nodiscard]]
MsesDecodeResult decodeMsesMultielement(std::string_view text);

// Produces the exact file contents (complete bytes, trailing
// line newline as for WRITE). The separator appears only between
// elements, never after the last element.
[[nodiscard]]
std::string msesMultielementText(const MsesMultielementFile& file);

}  // namespace xfoil::io

// End: include/xfoil/io/MsesMultielementCodec.h

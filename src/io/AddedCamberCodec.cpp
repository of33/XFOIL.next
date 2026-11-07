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

// File: src/io/AddedCamberCodec.cpp
// Implementation of the RDAC read contracts (xtcam.f 330-372).

#include "xfoil/io/AddedCamberCodec.h"

#include <cctype>
#include <cstdlib>
#include <istream>
#include <string>
#include <vector>

namespace xfoil::io {
namespace {

// List-directed separators: whitespace and comma.
std::vector<std::string> splitTokens(const std::string& line) {
  std::vector<std::string> tokens;
  std::string token;
  for (char c : line) {
    if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
    } else {
      token.push_back(c);
    }
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  return tokens;
}

// Fortran list-directed real, including D-exponent notation.
bool parseReal(const std::string& token, double& value) {
  if (token.empty()) {
    return false;
  }
  std::string normalized = token;
  for (char& c : normalized) {
    if (c == 'D' || c == 'd') {
      c = 'e';
    }
  }
  const char* begin = normalized.c_str();
  char* end = nullptr;
  const double parsed = std::strtod(begin, &end);
  if (end == begin || *end != '\0') {
    return false;
  }
  value = parsed;
  return true;
}

// Fetches the next token; empty records are skipped.
// Returns false on EOF (END).
bool nextToken(std::istream& in, std::vector<std::string>& tokens,
               std::size_t& index, std::string& token) {
  for (;;) {
    if (index < tokens.size()) {
      token = tokens[index++];
      return true;
    }
    std::string line;
    if (!std::getline(in, line)) {
      return false;
    }
    tokens = splitTokens(line);
    index = 0;
  }
}

}  // namespace

AddedCamberFileData decodeAddedCamberFile(std::istream& in) {
  AddedCamberFileData data;

  // READ(LU,1000,ERR=30,END=30) LINE: discard the header line. EOF or
  // a parse error in the header line leads directly to label 30 in Fortran
  // (NCADD = 0 -> 'No added camber points found').
  std::string header;
  if (!std::getline(in, header)) {
    return data;
  }

  std::vector<std::string> tokens;
  std::size_t index = 0;
  for (std::size_t k = 0; k < kAddedCamberPointLimit; ++k) {
    std::string token;
    if (!nextToken(in, tokens, index, token)) {
      break;  // END=30
    }
    double x = 0.0;
    if (!parseReal(token, x)) {
      data.truncatedByParseError = true;
      break;  // ERR=30
    }
    if (!nextToken(in, tokens, index, token)) {
      break;  // END=30: incomplete pair does not count
    }
    double y = 0.0;
    if (!parseReal(token, y)) {
      data.truncatedByParseError = true;
      break;  // ERR=30
    }
    data.points.push_back(AddedCamberPoint{x, y});
    // READ(*,*) ends the record after the two items; remaining tokens of
    // the current record are ignored.
    index = tokens.size();
  }
  if (data.points.size() == kAddedCamberPointLimit && in.peek() !=
                                                         std::istream::traits_type::eof()) {
    data.truncatedByPointLimit = true;
  }
  return data;
}

}  // namespace xfoil::io

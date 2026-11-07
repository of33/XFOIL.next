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

// File: src/io/MdesInverseFileCodec.cpp
// Implementation of the GETVOV read and CNDUMP write contracts
// (xmdes.f 1769-1826, 1747-1766). Pure data flow.

#include "xfoil/io/MdesInverseFileCodec.h"

#include <cctype>
#include <cstdlib>
#include <istream>
#include <string>
#include <vector>

#include "xfoil/io/TextFormat.h"

namespace xfoil::io {
namespace {

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

GetvovFileData decodeGetvovFile(std::istream& in) {
  GetvovFileData data;
  std::vector<std::string> tokens;
  std::size_t index = 0;
  for (std::size_t k = 0; k < kGetvovPointLimit; ++k) {
    std::string first;
    if (!nextToken(in, tokens, index, first)) {
      break;  // END=11
    }
    double s = 0.0;
    if (!parseReal(first, s)) {
      data.truncatedByParseError = true;  // ERR=99
      break;
    }
    std::string second;
    if (!nextToken(in, tokens, index, second)) {
      // Incomplete last pair: the Fortran READ waits until EOF and
      // takes the END branch; the incomplete pair does not count.
      break;
    }
    double vov = 0.0;
    if (!parseReal(second, vov)) {
      data.truncatedByParseError = true;
      break;
    }
    data.points.push_back(GetvovPoint{s, vov});
  }
  if (!data.truncatedByParseError && data.points.size() == kGetvovPointLimit &&
      in.peek() != std::istream::traits_type::eof()) {
    data.truncatedByPointLimit = true;
  }
  return data;
}

std::string cnDumpText(const std::vector<CnDumpRow>& rows) {
  std::string text;
  for (const CnDumpRow& row : rows) {
    text += " ";
    text += fortranI(3, row.m);
    text += fortranF(11, 6, row.cnReal);
    text += fortranF(11, 6, row.cnImag);
    text += fortranF(11, 6, row.piqReal);
    text += fortranF(11, 6, row.piqImag);
    text += "\n";
  }
  return text;
}

}  // namespace xfoil::io

// End: src/io/MdesInverseFileCodec.cpp

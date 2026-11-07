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

// File: src/io/MsesMultielementCodec.cpp
// Text codec for MSES multi-element files per SUBROUTINE MSAVE
// (fortran-src/xfoil.f 1430-1544). Port rules:
// - list-directed READ(LU,*) of exactly two values per READ call:
// a line remainder after the second value is discarded, an
// incomplete pair continues reading on the next line;
// - blank lines are skipped (list-directed);
// - element boundary: first value of the pair exactly 999.0 (Fortran checks
// only XTMP);
// - canonical bounds: NEX=5 elements, per element at most
// 2*IQX+1 = 741 pairs (IQX=370 from XFOIL.INC);
// - the remainder IEL of the element count is returned as residualElement
// (ASKI default for an empty answer).

#include "xfoil/io/MsesMultielementCodec.h"

#include "xfoil/io/TextFormat.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace xfoil::io {
namespace {

// Canonical MSAVE bounds (xfoil.f 1437-1438: Parameter(NEX=5),
// XTMP(2*IQX,NEX) with IQX=370 from XFOIL.INC).
constexpr int kMaxElements = 5;
constexpr int kMaxPairsPerElement = 2 * 370 + 1;

// Right trim as in the backward loop DO NN1=80,2,-1 in MSAVE:
// last non-empty position from 80 downward, at least 1.
std::string trimLikeMsave(const std::string& line) {
  std::size_t end = line.size();
  if (end > 80) {
    end = 80;
  }
  std::size_t keep = 1;
  for (std::size_t index = end; index >= 2; --index) {
    if (line[index - 1] != ' ') {
      keep = index;
      break;
    }
  }
  return line.substr(0, keep);
}

// List-directed input: separators are spaces, tabs and
// commas; a '!' is canonically not a comment and leads to a
// read error (Fortran does not handle the READ error).
std::vector<std::string> tokenizeListDirectedLine(const std::string& line) {
  std::vector<std::string> tokens;
  std::string token;
  for (const char character : line) {
    if (character == ' ' || character == '\t' || character == ',') {
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
    } else {
      token.push_back(character);
    }
  }
  if (!token.empty()) {
    tokens.push_back(token);
  }
  return tokens;
}

// Pairwise list-directed reader: fetches exactly two values per logical
// READ call; line remainders are discarded, an incomplete pair
// continues on the next line.
class PairReader {
 public:
  explicit PairReader(const std::vector<std::string>& lines) : lines_(lines) {}

  // Returns false for EOF (canonical END=). badData reports unreadable
  // numbers (READ error that Fortran does not handle).
  bool readPair(double& first, double& second, bool& badData) {    first = 0.0;
    second = 0.0;
    int obtained = 0;
    while (obtained < 2) {
      if (tokenIndex_ >= tokens_.size()) {
        if (!nextLine()) {
          return false;
        }
        continue;
      }
      // A READ call consumes only the values needed for its item
      // list; the rest of the line is skipped.
      const std::string token = tokens_[tokenIndex_++];
      char* end = nullptr;
      const double value = std::strtod(token.c_str(), &end);
      if (end == token.c_str() || *end != '\0') {
        badData = true;
        return true;
      }
      if (obtained == 0) {
        first = value;
      } else {
        second = value;
      }
      ++obtained;
    }
    return true;
  }

  // Sets the reader to a line number (0-based); MSAVE begins reading
  // coordinates after both header lines at line 3 (index 2).
  void skipToLine(std::size_t index) {
    lineIndex_ = index;
    tokens_.clear();
    tokenIndex_ = 0;
  }

 private:
  bool nextLine() {
    while (lineIndex_ < lines_.size()) {
      tokens_ = tokenizeListDirectedLine(lines_[lineIndex_++]);
      tokenIndex_ = 0;
      if (!tokens_.empty()) {
        return true;
      }
    }
    return false;
  }

  const std::vector<std::string>& lines_;
  std::size_t lineIndex_{0};
  std::vector<std::string> tokens_;
  std::size_t tokenIndex_{0};
};

}  // namespace

MsesDecodeResult decodeMsesMultielement(std::string_view text) {
  MsesDecodeResult result;

  std::vector<std::string> lines;
  std::string_view remaining = text;
  while (true) {
    const std::size_t newline = remaining.find('\n');
    if (newline == std::string_view::npos) {
      lines.emplace_back(remaining);
      break;
    }
    lines.emplace_back(remaining.substr(0, newline));
    remaining.remove_prefix(newline + 1);
    if (remaining.empty()) {
      break;
    }
  }

  // READ(LU,1000,ERR=9010) Name1 / ISPARS1: both header lines must
  // be present.
  if (lines.size() < 2) {
    result.status = MsesDecodeStatus::readError;
    return result;
  }
  result.file.name = trimLikeMsave(lines[0]);
  result.file.ispars = trimLikeMsave(lines[1]);

  PairReader reader(lines);
  reader.skipToLine(2);
  std::vector<int> counts;
  int residual = 1;

  for (int iel = 1; iel <= kMaxElements; ++iel) {
    std::vector<double> xs;
    std::vector<double> ys;
    bool sentinelFound = false;
    for (int i = 1; i <= kMaxPairsPerElement; ++i) {
      bool badData = false;
      double first = 0.0;
      double second = 0.0;
      if (!reader.readPair(first, second, badData)) {
        // END=56: I==1 -> NEL=IEL-1, otherwise NEL=IEL with NTMP(IEL)=I-1.
        if (i > 1) {
          result.file.elements.push_back({std::move(xs), std::move(ys)});
          counts.push_back(i - 1);
        }
        residual = iel;
        result.residualElement = residual;
        result.status = MsesDecodeStatus::ok;
        return result;
      }
      if (badData) {
        result.status = MsesDecodeStatus::readError;
        return result;
      }
      if (first == 999.0) {
        result.file.elements.push_back({std::move(xs), std::move(ys)});
        counts.push_back(i - 1);
        residual = iel;
        sentinelFound = true;
        break;
      }
      xs.push_back(first);
      ys.push_back(second);
    }
    if (!sentinelFound) {
      // Fortran: STOP 'LOAD: Array overflow'
      result.status = MsesDecodeStatus::overflow;
      return result;
    }
  }

  // All NEX elements read with a marker: NEL=NEX, IEL remainder NEX+1.
  residual = kMaxElements + 1;
  result.residualElement = residual;
  result.status = MsesDecodeStatus::ok;
  return result;
}

std::string msesMultielementText(const MsesMultielementFile& file) {
  std::string out;
  out += file.name;
  out += '\n';
  out += file.ispars;
  out += '\n';
  for (std::size_t iel = 0; iel < file.elements.size(); ++iel) {
    const MsesMultielementElement& element = file.elements[iel];
    const std::size_t count = element.x.size() < element.y.size()
                                  ? element.x.size()
                                  : element.y.size();
    for (std::size_t i = 0; i < count; ++i) {
      out += ' ';
      out += fortranG(15, 7, element.x[i]);
      out += fortranG(15, 7, element.y[i]);
      out += '\n';
    }
    if (iel + 1 < file.elements.size()) {
      // WRITE(LU,*) ' 999.0 999.0': list-directed output prepends a
      // leading blank to a character constant; the constant
      // itself already begins with a blank.
      out += "  999.0  999.0\n";
    }
  }
  return out;
}

}  // namespace xfoil::io

// End: src/io/MsesMultielementCodec.cpp

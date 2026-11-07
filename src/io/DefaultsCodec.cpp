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

#include "xfoil/io/DefaultsCodec.h"

#include <vector>

#include "xfoil/io/TextFormat.h"

namespace xfoil::io {
namespace {

bool readLineValues(std::string_view line, std::vector<double>& out, int expected) {
  const FloatParseResult parsed = parseFloatList(line, static_cast<std::size_t>(expected));
  if (parsed.error || parsed.extracted != expected) return false;
  out = parsed.values;
  return true;
}

bool readLineInts(std::string_view line, std::vector<int>& out, int expected) {
  std::vector<double> reals;
  if (!readLineValues(line, reals, expected)) return false;
  out.clear();
  for (const double v : reals) out.push_back(static_cast<int>(v));
  return true;
}

bool readLineLogical(std::string_view line, std::vector<bool>& out) {
  // READ(LU,*) LCOLOR, LCURS: logical fields T/F.
  std::string text(line.substr(0, 128));
  std::vector<bool> flags;
  std::size_t pos = 0;
  while (pos < text.size() && flags.size() < 2) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == ',')) ++pos;
    if (pos >= text.size()) break;
    const char c = text[pos];
    if (c == 'T' || c == 't' || c == 'F' || c == 'f') {
      flags.push_back(c == 'T' || c == 't');
      ++pos;
    } else {
      return false;
    }
    while (pos < text.size() && text[pos] != ' ' && text[pos] != ',') ++pos;
  }
  if (flags.size() != 2) return false;
  out = flags;
  return true;
}

}  // namespace

DefaultsParseResult parseDefaults(std::string_view text) {
  DefaultsParseResult result;
  std::vector<std::string> lines;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t nl = text.find('\n', pos);
    std::string line(text.substr(pos, nl == std::string_view::npos ? std::string_view::npos
                                                                  : nl - pos));
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
    if (nl == std::string_view::npos) break;
    pos = nl + 1;
  }

  std::vector<double> v;
  std::vector<int> iv;
  std::vector<bool> lv;
  std::size_t lineNo = 0;
  auto fail = [&](int number) {
    result.error = DefaultsError::kBadLine;
    result.firstBadLine = number;
  };
  auto eofFail = [&](int number) {
    result.error = DefaultsError::kEndOfFile;
    result.firstBadLine = number;
  };

  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
  }
  if (readLineInts(lines[lineNo], iv, 1)) {
    result.values.npan = iv[0];
    if (readLineValues(lines[lineNo], v, 4)) {
      result.values.cvpar = v[1];
      result.values.cterat = v[2];
      result.values.ctrrat = v[3];
      ++result.linesRead;
    } else {
      fail(static_cast<int>(lineNo + 1));
      return result;
    }
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 4)) {
    result.values.xsref1 = v[0];
    result.values.xsref2 = v[1];
    result.values.xpref1 = v[2];
    result.values.xpref2 = v[3];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 4)) {
    result.values.size = v[0];
    result.values.plotar = v[1];
    result.values.ch = v[2];
    result.values.scrnfr = v[3];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 4)) {
    result.values.xpage = v[0];
    result.values.ypage = v[1];
    result.values.xmarg = v[2];
    result.values.ymarg = v[3];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineLogical(lines[lineNo], lv)) {
    result.values.lcolor = lv[0];
    result.values.lcurs = lv[1];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 3)) {
    result.values.cpmax = v[0];
    result.values.cpmin = v[1];
    result.values.cpdel = v[2];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 3)) {
    result.values.xofair = v[0];
    result.values.facair = v[1];
    result.values.uprwt = v[2];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  for (int block = 0; block < 4; ++block) {
    ++lineNo;
    if (lineNo >= lines.size()) {
      eofFail(static_cast<int>(lineNo + 1));
      return result;
      }
    if (readLineValues(lines[lineNo], v, 3)) {
      if (block == 0) result.values.clpol = {v[0], v[1], v[2]};
      if (block == 1) result.values.cdpol = {v[0], v[1], v[2]};
      if (block == 2) result.values.alpol = {v[0], v[1], v[2]};
      if (block == 3) result.values.cmpol = {v[0], v[1], v[2]};
      ++result.linesRead;
    } else {
      fail(static_cast<int>(lineNo + 1));
      return result;
    }
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 3)) {
    result.values.matyp = static_cast<int>(v[0]);
    result.values.minf1 = v[1];
    result.values.vaccel = v[2];
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 4)) {
    result.values.retyp = static_cast<int>(v[0]);
    result.values.rmill = v[1];
    result.values.acrit = {v[2], v[3]};
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  ++lineNo;
  if (lineNo >= lines.size()) {
    eofFail(static_cast<int>(lineNo + 1));
    return result;
    }
  if (readLineValues(lines[lineNo], v, 2)) {
    result.values.xtrip = {v[0], v[1]};
    ++result.linesRead;
  } else {
    fail(static_cast<int>(lineNo + 1));
    return result;
  }
  result.complete = true;
  return result;
}

std::string writeDefaults(const DefaultsValues& values, int idevrp) {
  std::string out;
  out += " " + fortranI(5, values.npan) + "    " + fortranF(9, 4, values.cvpar) +
         fortranF(9, 4, values.cterat) + fortranF(9, 4, values.ctrrat) +
         " | Npan    PPanel  TErat  REFrat\n";
  out += " " + fortranF(9, 4, values.xsref1) + fortranF(9, 4, values.xsref2) +
         fortranF(9, 4, values.xpref1) + fortranF(9, 4, values.xpref2) +
         " | XrefS1  XrefS2  XrefP1 XrefP2\n";
  out += " " + fortranF(9, 4, values.size) + fortranF(9, 4, values.plotar) +
         fortranF(9, 4, values.ch) + fortranF(9, 4, values.scrnfr) +
         " | Size    plotAR  CHsize ScrnFr\n";
  out += " " + fortranF(9, 4, values.xpage) + fortranF(9, 4, values.ypage) +
         fortranF(9, 4, values.xmarg) + fortranF(9, 4, values.ymarg) +
         " | Xpage   Ypage   Xmargn Ymargn\n";
  const bool lcolor = idevrp == 4;
  out += " " + fortranL2(lcolor) + "       " + fortranL2(values.lcurs) +
         "                         "
         " | Lcolor  Lcursor\n";
  out += " " + fortranF(9, 4, values.cpmax) + fortranF(9, 4, values.cpmin) +
         fortranF(9, 4, values.cpdel) + "         "
         " | CPmax   CPmin   CPdel\n";
  out += " " + fortranF(9, 4, values.xofair) + fortranF(9, 4, values.facair) +
         fortranF(9, 4, values.uprwt) + "         "
         " | XoffAir ScalAir BLUwt\n";
  out += " " + fortranF(9, 4, values.clpol[0]) + fortranF(9, 4, values.clpol[1]) +
         fortranF(9, 4, values.clpol[2]) + "         "
         " | CLmin   CLmax   CLdel\n";
  out += " " + fortranF(9, 4, values.cdpol[0]) + fortranF(9, 4, values.cdpol[1]) +
         fortranF(9, 4, values.cdpol[2]) + "         "
         " | CDmin   CDmax   CDdel\n";
  out += " " + fortranF(9, 4, values.alpol[0]) + fortranF(9, 4, values.alpol[1]) +
         fortranF(9, 4, values.alpol[2]) + "         "
         " | ALmin   ALmax   ALdel\n";
  out += " " + fortranF(9, 4, values.cmpol[0]) + fortranF(9, 4, values.cmpol[1]) +
         fortranF(9, 4, values.cmpol[2]) + "         "
         " | CMmin   CMmax   CMdel\n";
  out += " " + fortranI(3, values.matyp) + "      " + fortranF(9, 4, values.minf1) +
         fortranF(9, 4, values.vaccel) + "         "
         " | MAtype  Mach    Vaccel\n";
  out += " " + fortranI(3, values.retyp) + "      " + fortranF(9, 4, values.rmill) +
         fortranF(9, 4, values.acrit[0]) + fortranF(9, 4, values.acrit[1]) +
         " | REtype  Re/10^6 Ncrit1 Ncrit2\n";
  out += " " + fortranF(9, 4, values.xtrip[0]) + fortranF(9, 4, values.xtrip[1]) +
         "                  "
         " | XtripT  XtripB\n";
  return out;
}

}  // namespace xfoil::io

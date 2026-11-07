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

#include "xfoil/io/PolarText.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "xfoil/io/TextFormat.h"

namespace xfoil::io {
namespace {

struct NumFormat {
  char kind;  // 'F' or 'E'
  int width;
  int decimals;
};

// CPOLFORM from PINDEX.INC.
const NumFormat kCpolForm[kIptot + 1] = {
    {},        // Index 0 unused
    {'F', 7, 3},   {'F', 9, 4},   {'F', 10, 5},  {'F', 9, 4},   {'F', 10, 5},
    {'F', 10, 5},  {'F', 10, 5},  {'F', 8, 4},   {'E', 11, 3},  {'F', 10, 5},
    {'F', 9, 4},   {'F', 11, 5},  {'F', 9, 5},
};

// CPOLSFORM from PINDEX.INC.
const NumFormat kColsForm[kJptot + 1] = {
    {}, {'F', 7, 3}, {'F', 9, 4}, {'F', 9, 4}, {'F', 9, 4},
};

const char* kCpolName[kIptot + 1] = {"",     "alpha", "CL",    "CD",   "CM",
                                     "CDw",  "CDv",   "CDp",   "Mach", "Re",
                                     "Chinge", "Cpmin", "CDh",   "Cmdot"};

const char* kColsName[kJptot + 1] = {"", "Ncrit", "Xtrip", "Xtr", "Itr"};

std::string formatValue(const NumFormat& fmt, double value) {
  if (fmt.kind == 'E') return fortranE(fmt.width, fmt.decimals, value);
  return fortranF(fmt.width, fmt.decimals, value);
}

// Fortran index: 1-based position or 0.
int fortranIndex(std::string_view line, std::string_view needle) {
  if (needle.empty()) return 0;
  const std::size_t pos = line.find(needle);
  return pos == std::string::npos ? 0 : static_cast<int>(pos) + 1;
}

// Fortran list-directed READ from a substring (READ(LINE(a:b),*,ERR=..)).
bool readListValue(std::string_view text, double& out) {
  const FloatParseResult parsed = parseFloatList(text, 1);
  if (parsed.error || parsed.values.empty()) return false;
  out = parsed.values[0];
  return true;
}

constexpr std::size_t sz(int v) { return static_cast<std::size_t>(v); }

std::string trimRight(std::string_view text) {
  std::size_t end = text.size();
  while (end > 0 && text[end - 1] == ' ') --end;
  return std::string(text.substr(0, end));
}

}  // namespace

ParsedPolar readPolarText(std::string_view text, int maxPoints) {
  ParsedPolar result;
  std::array<int, kIptot + 1> ipol{};      // Column -> data position
  std::array<int, kJptot + 1> ispolTop{};  // JP -> Position (side 1)
  std::array<int, kJptot + 1> ispolBot{};
  bool header = true;
  bool nameSeen = false;
  int kfFor = 0;
  int lineNo = 0;

  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t nl = text.find('\n', pos);
    std::string_view raw = (nl == std::string_view::npos)
                               ? text.substr(pos, text.size() - pos)
                               : text.substr(pos, nl - pos);
    pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    if (raw.size() > 128) raw = raw.substr(0, 128);
    ++lineNo;

    if (header) {
      if (stripBlanks(raw).empty()) continue;
      bool matched = false;

      const int kVersion = fortranIndex(raw, "Version");
      if (kVersion != 0) {
        const std::size_t k1 = raw.find_first_not_of(' ');
        if (k1 != std::string_view::npos && static_cast<int>(k1) + 1 < kVersion) {
          result.code = trimRight(raw.substr(k1, static_cast<std::size_t>(kVersion) - k1 - 1));
          readListValue(raw.substr(std::min<std::size_t>(
                            static_cast<std::size_t>(kVersion) + 6, raw.size())),
                        result.version);
        }
        continue;
      }
      const int kf = fortranIndex(raw, "for:");
      if (kf != 0) {
        kfFor = kf;
        result.name = trimRight(raw.substr(static_cast<std::size_t>(kf) + 4));
        nameSeen = true;
        matched = true;
      }
      const int ke = fortranIndex(raw, "elements");
      if (ke > 0 && nameSeen) {
        double nblValue = 0.0;
        if (ke >= 5 && readListValue(raw.substr(static_cast<std::size_t>(ke) - 5, 4), nblValue)) {
          result.nbl = std::max(1, static_cast<int>(nblValue));
        }
        if (kfFor > 0 && ke >= kfFor + 5) {
          result.name = trimRight(raw.substr(static_cast<std::size_t>(kfFor) + 4,
                                             static_cast<std::size_t>(ke) - 5 -
                                                 static_cast<std::size_t>(kfFor) + 4));
        }
        matched = true;
      }
      const int kr = fortranIndex(raw, "Reynolds number");
      const int km = fortranIndex(raw, "Mach number");
      if (kr != 0) {
        const std::size_t kend =
            (km > kr) ? static_cast<std::size_t>(km) - 1 : raw.size();
        const std::string_view seg = raw.substr(static_cast<std::size_t>(kr) - 1,
                                                kend - static_cast<std::size_t>(kr) + 1);
        if (fortranIndex(seg, "fixed") != 0) result.iretyp = 1;
        else if (fortranIndex(seg, "1/sqrt(CL)") != 0) result.iretyp = 2;
        else if (fortranIndex(seg, "1/CL") != 0) result.iretyp = 3;
        matched = true;
      }
      if (km != 0) {
        const std::size_t kend =
            (kr > km) ? static_cast<std::size_t>(kr) - 1 : raw.size();
        const std::string_view seg = raw.substr(static_cast<std::size_t>(km) - 1,
                                                kend - static_cast<std::size_t>(km) + 1);
        if (fortranIndex(seg, "fixed") != 0) result.imatyp = 1;
        else if (fortranIndex(seg, "1/sqrt(CL)") != 0) result.imatyp = 2;
        else if (fortranIndex(seg, "1/CL") != 0) result.imatyp = 3;
        matched = true;
      }
      const int kx = fortranIndex(raw, "xtrf");
      if (kx != 0) {
        const int kt = fortranIndex(raw, "(top)");
        const int kb = fortranIndex(raw, "(bottom)");
        const int kel = fortranIndex(raw, "element ");
        int n = 1;
        if (kel != 0 && static_cast<std::size_t>(kel) + 11 < raw.size()) {
          double nv = 0.0;
          if (readListValue(raw.substr(static_cast<std::size_t>(kel) + 6, 6), nv)) {
            n = static_cast<int>(nv);
          }
        }
        if (n <= result.nbl) {
          const int is1 = 2 * n - 1;
          const int is2 = 2 * n;
          result.xtrip[0] = 1.0;
          result.xtrip[1] = 1.0;
          double v = 0.0;
          if (kt > 0 && readListValue(raw.substr(static_cast<std::size_t>(kx) + 5,
                                                 static_cast<std::size_t>(kt) - 5 -
                                                     static_cast<std::size_t>(kx) + 5),
                                      v)) {
            result.xtrip[0] = v;
          }
          if (kb > kt && readListValue(raw.substr(static_cast<std::size_t>(kt) + 4,
                                                  static_cast<std::size_t>(kb) - 4 -
                                                      static_cast<std::size_t>(kt) + 4),
                                       v)) {
            result.xtrip[1] = v;
          }
          (void)is1;
          (void)is2;
        }
        matched = true;
      }
      const int kmach = fortranIndex(raw, "Mach =");
      if (kmach != 0) {
        readListValue(raw.substr(std::min<std::size_t>(static_cast<std::size_t>(kmach) + 5,
                                                       raw.size())),
                      result.mach1);
        matched = true;
      }
      const int kre = fortranIndex(raw, "Re =");
      if (kre != 0) {
        double reyn = 0.0;
        if (readListValue(raw.substr(std::min<std::size_t>(static_cast<std::size_t>(kre) + 3,
                                                           raw.size())),
                          reyn)) {
          result.reyn1 = reyn * 1.0e6;
        }
        matched = true;
      }
      const int kn = fortranIndex(raw, "Ncrit =");
      if (kn != 0) {
        const FloatParseResult parsed =
            parseFloatList(raw.substr(std::min<std::size_t>(static_cast<std::size_t>(kn) + 6,
                                                             raw.size())),
                           2);
        if (!parsed.error && !parsed.values.empty()) {
          result.acrit[0] = parsed.values[0];
          result.acrit[1] = parsed.values.size() > 1 ? parsed.values[1] : parsed.values[0];
        }
        matched = true;
      }
      const int kpi = fortranIndex(raw, "pi_p =");
      if (kpi != 0) {
        readListValue(raw.substr(std::min<std::size_t>(static_cast<std::size_t>(kpi) + 5,
                                                       raw.size())),
                      result.ptrat);
        matched = true;
      }
      const int ket = fortranIndex(raw, "eta_p =");
      if (ket != 0) {
        readListValue(raw.substr(std::min<std::size_t>(static_cast<std::size_t>(ket) + 6,
                                                       raw.size())),
                      result.etap);
        matched = true;
      }

      if (!matched && result.ipolOrder.empty()) {
        // Label line: positions determined by the canonical column names.
                std::vector<std::pair<int, int>> marks;  // (position, kind)
        for (int ip = 1; ip <= kIptot; ++ip) {
          const int k = fortranIndex(raw, kCpolName[sz(ip)]);
          if (k > 0) marks.push_back({k, ip});
        }
        for (int jp = 1; jp <= kJptot; ++jp) {
          const std::string topA = std::string("Top ") + kColsName[sz(jp)];
          const std::string topB = std::string("Top_") + kColsName[sz(jp)];
          const int k1 = fortranIndex(raw, topA);
          const int k2 = fortranIndex(raw, topB);
          const int kTop = std::max(k1, k2);
          if (kTop > 0) marks.push_back({kTop, kIptot + jp});
          const std::string botA = std::string("Bot ") + kColsName[sz(jp)];
          const std::string botB = std::string("Bot_") + kColsName[sz(jp)];
          const int k3 = fortranIndex(raw, botA);
          const int k4 = fortranIndex(raw, botB);
          const int kBot = std::max(k3, k4);
          if (kBot > 0) marks.push_back({kBot, kIptot + kJptot + jp});
        }
        std::sort(marks.begin(), marks.end());
        int nipol = 0;
        for (const auto& mark : marks) {
          ++nipol;
          if (mark.second <= kIptot) {
            ipol[static_cast<std::size_t>(mark.second)] = nipol;
            result.columnPresent[static_cast<std::size_t>(mark.second - 1)] = true;
            result.ipolOrder.push_back(mark.second);
          } else if (mark.second <= kIptot + kJptot) {
            ispolTop[static_cast<std::size_t>(mark.second - kIptot)] = nipol;
            result.sidePresent[static_cast<std::size_t>(mark.second - kIptot - 1)] = true;
            if (std::find(result.jpolOrder.begin(), result.jpolOrder.end(),
                          mark.second - kIptot) == result.jpolOrder.end()) {
              result.jpolOrder.push_back(mark.second - kIptot);
            }
          } else {
            ispolBot[static_cast<std::size_t>(mark.second - kIptot - kJptot)] = nipol;
            result.sidePresent[static_cast<std::size_t>(mark.second - kIptot - kJptot - 1)] = true;
            if (std::find(result.jpolOrder.begin(), result.jpolOrder.end(),
                          mark.second - kIptot - kJptot) == result.jpolOrder.end()) {
              result.jpolOrder.push_back(mark.second - kIptot - kJptot);
            }
          }
        }
        matched = true;
      }
      if (fortranIndex(raw, "-----") != 0) header = false;
      continue;
    }

    // Data line
    if (stripBlanks(raw).empty()) continue;
    const FloatParseResult parsed = parseFloatList(raw, kIptot + 2 * kJptot);
    if (parsed.error) {
      result.error = true;
      result.errorLine = lineNo;
      return result;
    }
    if (static_cast<int>(result.points.size()) >= maxPoints) {
      result.error = true;
      result.errorLine = lineNo;
      return result;
    }
    PolarPoint point;
    for (int ip = 1; ip <= kIptot; ++ip) {
      // Canonical behavior reads RINP(0) for a missing column
      // (uninitialized); per the step file this
      // uninitialized state is not replicated; 0.0 is set instead.
      const int at = ipol[sz(ip)];
      point.cpol[sz(ip - 1)] =
          (at >= 1 && at <= static_cast<int>(parsed.values.size())) ? parsed.values[sz(at - 1)] : 0.0;
    }
    for (int jp = 1; jp <= kJptot; ++jp) {
      const int top = ispolTop[sz(jp)];
      const int bot = ispolBot[sz(jp)];
      point.side[0][sz(jp - 1)] =
          (top >= 1 && top <= static_cast<int>(parsed.values.size())) ? parsed.values[sz(top - 1)] : 0.0;
      point.side[1][sz(jp - 1)] =
          (bot >= 1 && bot <= static_cast<int>(parsed.values.size())) ? parsed.values[sz(bot - 1)] : 0.0;
    }
    // Canonical fallbacks if Re/Ma/Ncrit/Xtrip are not present in the data.
    const double acl = std::max(point.cpol[sz(kIcl - 1)], 0.001);
    if (!result.columnPresent[kIre - 1]) {
      if (result.iretyp == 1) point.cpol[sz(kIre - 1)] = result.reyn1;
      else if (result.iretyp == 2) point.cpol[sz(kIre - 1)] = result.reyn1 / std::sqrt(acl);
      else if (result.iretyp == 3) point.cpol[sz(kIre - 1)] = result.reyn1 / acl;
    }
    if (!result.columnPresent[kIma - 1]) {
      if (result.imatyp == 1) point.cpol[sz(kIma - 1)] = result.mach1;
      else if (result.imatyp == 2) point.cpol[sz(kIma - 1)] = result.mach1 / std::sqrt(acl);
      else if (result.imatyp == 3) point.cpol[sz(kIma - 1)] = result.mach1 / acl;
    }
    if (!result.sidePresent[kJnc - 1]) {
      point.side[0][sz(kJnc - 1)] = result.acrit[0];
      point.side[1][sz(kJnc - 1)] = result.acrit[1];
    }
    if (!result.sidePresent[kJtp - 1]) {
      point.side[0][sz(kJtp - 1)] = result.xtrip[0];
      point.side[1][sz(kJtp - 1)] = result.xtrip[1];
    }
    result.points.push_back(point);
  }
  return result;
}

std::string writePolarText(const PolarWriteRequest& request) {
  std::string out;
  if (request.lhead) {
    out += "  \n";
    out += "       " + request.code + "         Version" +
           fortranF(5, 2, request.version) + "\n";
    out += "  \n";
    const std::string nameField =
        request.name.size() >= 48 ? request.name.substr(0, 48)
                                  : request.name + std::string(48 - request.name.size(), ' ');
    if (request.nbl == 1) {
      out += " Calculated polar for: " + nameField + "\n";
    } else {
      out += " Calculated polar for: " + nameField + fortranI(4, request.nbl) + " elements\n";
    }
    out += "  \n";
    auto reynoldsLabel = [](int t) {
      if (t == 1) return std::string(" Reynolds number fixed       ");
      if (t == 2) return std::string(" Reynolds number ~ 1/sqrt(CL)");
      if (t == 3) return std::string(" Reynolds number ~ 1/CL      ");
      return std::string("                             ");
    };
    auto machLabel = [](int t) {
      if (t == 1) return std::string("   Mach number fixed         ");
      if (t == 2) return std::string("   Mach number ~ 1/sqrt(CL)  ");
      if (t == 3) return std::string("   Mach number ~ 1/CL        ");
      return std::string("                             ");
    };
    out += " " + fortranI(1, request.iretyp) + fortranI(2, request.imatyp) +
           reynoldsLabel(request.iretyp) + machLabel(request.imatyp) + "\n";
    out += "  \n";
    for (int n = 1; n <= request.nbl; ++n) {
      const int is1 = 2 * n - 2;
      const int is2 = 2 * n - 1;
      if (request.nbl == 1) {
        out += " xtrf = " + fortranF(7, 3, request.xtrip[sz(is1)]) + " (top)    " +
               fortranF(9, 3, request.xtrip[sz(is2)]) + " (bottom)  \n";
      } else {
        out += " xtrf = " + fortranF(7, 3, request.xtrip[sz(is1)]) + " (top)    " +
               fortranF(9, 3, request.xtrip[sz(is2)]) + " (bottom)     element" +
               fortranI(3, n) + "\n";
      }
    }
    out += " Mach = " + fortranF(7, 3, request.mach1) + "     Re = " +
           fortranF(9, 3, request.reyn1 / 1.0e6) + " e 6     Ncrit = " +
           fortranF(7, 3, request.acrit[0]) + fortranF(7, 3, request.acrit[1]) + "\n";
    if (request.ptrat != 0.0) {
      out += " pi_p = " + fortranF(7, 4, request.ptrat) + "     eta_p = " +
             fortranF(9, 4, request.etap) + "\n";
    }
    out += "  \n";

    std::string lineL = " ";
    std::string lineD = " ";
    for (const int ip : request.ipol) {
      if (ip < 1 || ip > kIptot) continue;
      const NumFormat& fmt = kCpolForm[sz(ip)];
      const std::string name = kCpolName[sz(ip)];
      const int nblank = std::max((fmt.width - static_cast<int>(name.size()) + 2) / 2, 0);
      lineL += std::string(static_cast<std::size_t>(nblank), ' ') + name +
               std::string(static_cast<std::size_t>(fmt.width - static_cast<int>(name.size()) - nblank),
                           ' ');
      lineD += " " + std::string(static_cast<std::size_t>(fmt.width - 1), '-');
    }
    for (const int jp : request.jpol) {
      if (jp < 1 || jp > kJptot) continue;
      const NumFormat& fmt = kColsForm[sz(jp)];
      const std::string name = kColsName[sz(jp)];
      const int nblank = std::max((fmt.width - static_cast<int>(name.size()) - 2) / 2, 0);
      for (int n = 1; n <= request.nbl; ++n) {
        (void)n;
        lineL += std::string(static_cast<std::size_t>(nblank), ' ') + "Top_" + name +
                 std::string(static_cast<std::size_t>(fmt.width - static_cast<int>(name.size()) - 4 - nblank),
                             ' ');
        lineD += " " + std::string(static_cast<std::size_t>(fmt.width - 1), '-');
        lineL += std::string(static_cast<std::size_t>(nblank), ' ') + "Bot_" + name +
                 std::string(static_cast<std::size_t>(fmt.width - static_cast<int>(name.size()) - 4 - nblank),
                             ' ');
        lineD += " " + std::string(static_cast<std::size_t>(fmt.width - 1), '-');
      }
    }
    out += lineL + "\n";
    out += lineD + "\n";
  }

  for (int ia = request.ia1; ia <= request.ia2; ++ia) {
    if (ia < 1 || ia > static_cast<int>(request.points.size())) continue;
    const PolarPoint& point = request.points[static_cast<std::size_t>(ia - 1)];
    std::string line = " ";
    for (const int ip : request.ipol) {
      if (ip < 1 || ip > kIptot) continue;
      line += formatValue(kCpolForm[sz(ip)], point.cpol[sz(ip - 1)]);
    }
    for (const int jp : request.jpol) {
      if (jp < 1 || jp > kJptot) continue;
      for (int side = 0; side < 2; ++side) {
        line += formatValue(kColsForm[sz(jp)], point.side[sz(side)][sz(jp - 1)]);
      }
    }
    out += line + "\n";
  }
  return out;
}

ParsedReferencePolar readReferencePolarText(std::string_view text, int maxPerBlock) {
  ParsedReferencePolar result;
  std::size_t pos = 0;
  bool first = true;
  int block = 0;
  while (pos <= text.size() && block < 4) {
    const std::size_t nl = text.find('\n', pos);
    std::string_view raw = (nl == std::string_view::npos)
                               ? text.substr(pos, text.size() - pos)
                               : text.substr(pos, nl - pos);
    pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    if (first) {
      first = false;
      if (!raw.empty() && raw[0] == '#') {
        result.label = stripBlanks(raw.substr(1));
        continue;
      }
    }
    if (stripBlanks(raw).empty()) continue;
    const FloatParseResult parsed = parseFloatList(raw, 2);
    if (parsed.error || parsed.values.size() < 2) {
      result.error = true;
      return result;
    }
    if (parsed.values[0] == 999.0) {
      ++block;
      continue;
    }
    if (static_cast<int>(result.blocks[sz(block)].size()) >= maxPerBlock) {
      result.error = true;
      return result;
    }
    result.blocks[sz(block)].push_back({parsed.values[0], parsed.values[1]});
  }
  for (int b = 0; b < 4; ++b)
    result.blockSizes[sz(b)] = static_cast<int>(result.blocks[sz(b)].size());
  return result;
}

}  // namespace xfoil::io

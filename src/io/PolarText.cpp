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
#include <string_view>
#include <utility>

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

const NumFormat kElementForm[5] = {
    {}, {'F', 9, 4}, {'F', 9, 4}, {'F', 9, 4}, {'F', 10, 5},
};

const char* kCpolName[kIptot + 1] = {"",     "alpha", "CL",    "CD",   "CM",
                                     "CDw",  "CDv",   "CDp",   "Mach", "Re",
                                     "Chinge", "Cpmin", "CDh",   "Cmdot"};

const char* kColsName[kJptot + 1] = {"", "Ncrit", "Xtrip", "Xtr", "Itr"};

const char* kElementName[5] = {"", "Cx", "Cy", "Cm", "CDv"};

int elementQuantityIndex(core::polar::PolarElementQuantity quantity) {
  return static_cast<int>(quantity);
}

const NumFormat& elementFormat(core::polar::PolarElementQuantity quantity) {
  return kElementForm[elementQuantityIndex(quantity)];
}

std::string formatValue(const NumFormat& fmt, double value) {
  if (fmt.kind == 'E') return fortranE(fmt.width, fmt.decimals, value);
  return fortranF(fmt.width, fmt.decimals, value);
}

constexpr std::size_t sz(int v) { return static_cast<std::size_t>(v); }

std::string trimRight(std::string_view text) {
  std::size_t end = text.size();
  while (end > 0 && text[end - 1] == ' ') --end;
  return std::string(text.substr(0, end));
}

std::vector<std::string_view> tokens(std::string_view line) {
  std::vector<std::string_view> result;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') ++i;
    if (i == line.size()) break;
    const std::size_t begin = i;
    while (i < line.size() && line[i] != ' ' && line[i] != ',') ++i;
    if (i > begin) result.push_back(line.substr(begin, i - begin));
  }
  return result;
}

bool parseElementToken(std::string_view token,
                       core::polar::PolarElementQuantity& quantity,
                       int& elementIndex) {
  static constexpr std::string_view names[5] = {"", "Cx_", "Cy_", "Cm_",
                                                "CDv_"};
  for (int q = 1; q <= 4; ++q) {
    if (token.rfind(names[sz(q)], 0) != 0) continue;
    const std::string_view suffix = token.substr(names[sz(q)].size());
    if (suffix.empty()) return false;
    int value = 0;
    for (char ch : suffix) {
      if (ch < '0' || ch > '9') return false;
      value = value * 10 + (ch - '0');
      if (value > kMaxPolarElements) return false;
    }
    if (value < 1) return false;
    quantity = static_cast<core::polar::PolarElementQuantity>(q);
    elementIndex = value;
    return true;
  }
  return false;
}

int baseIndexForName(std::string_view token) {
  for (int i = 1; i <= kIptot; ++i) {
    if (token == kCpolName[sz(i)]) return i;
  }
  return 0;
}

int sideIndexForName(std::string_view token, bool& top) {
  if (token.rfind("Top_", 0) == 0) {
    top = true;
    const std::string_view name = token.substr(4);
    for (int j = 1; j <= kJptot; ++j) {
      if (name == kColsName[sz(j)]) return j;
    }
    return 0;
  }
  if (token.rfind("Bot_", 0) == 0) {
    top = false;
    const std::string_view name = token.substr(4);
    for (int j = 1; j <= kJptot; ++j) {
      if (name == kColsName[sz(j)]) return j;
    }
    return 0;
  }
  return 0;
}

bool parsePolarNumbers(std::string_view line, std::vector<double>& values) {
  values.clear();
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == ',')) ++i;
    if (i == line.size()) break;
    if (line[i] == '!') break;
    const std::size_t begin = i;
    while (i < line.size() && line[i] != ' ' && line[i] != ',') ++i;
    std::string token(line.substr(begin, i - begin));
    std::replace(token.begin(), token.end(), 'D', 'E');
    std::replace(token.begin(), token.end(), 'd', 'e');
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (end == token.c_str() || *end != '\0') return false;
    values.push_back(value);
  }
  return true;
}

std::string fieldName(const PolarField& field) {
  switch (field.kind) {
    case PolarFieldKind::base:
      return kCpolName[sz(field.baseIndex)];
    case PolarFieldKind::sideTop:
      return std::string("Top_") + kColsName[sz(field.sideIndex)];
    case PolarFieldKind::sideBottom:
      return std::string("Bot_") + kColsName[sz(field.sideIndex)];
    case PolarFieldKind::element:
      return std::string(kElementName[elementQuantityIndex(
                 field.elementQuantity)]) +
             "_" + std::to_string(field.elementIndex);
  }
  return {};
}

const NumFormat& fieldFormat(const PolarField& field) {
  switch (field.kind) {
    case PolarFieldKind::base:
      return kCpolForm[sz(field.baseIndex)];
    case PolarFieldKind::sideTop:
    case PolarFieldKind::sideBottom:
      return kColsForm[sz(field.sideIndex)];
    case PolarFieldKind::element:
      return elementFormat(field.elementQuantity);
  }
  return kCpolForm[0];
}

double baseValue(const PolarPoint& point, int index) {
  return point.cpol[sz(index - 1)];
}

double sideValue(const PolarPoint& point, const PolarField& field) {
  const std::size_t sideRow =
      static_cast<std::size_t>(2 * (field.elementIndex - 1) +
                               (field.kind == PolarFieldKind::sideBottom
                                    ? 1
                                    : 0));
  return point.side[sideRow][sz(field.sideIndex - 1)];
}

double elementValue(const core::polar::PolarElementData& data,
                    core::polar::PolarElementQuantity quantity) {
  switch (quantity) {
    case core::polar::PolarElementQuantity::cx: return data.cx;
    case core::polar::PolarElementQuantity::cy: return data.cy;
    case core::polar::PolarElementQuantity::cm: return data.cm;
    case core::polar::PolarElementQuantity::cdVisc: return data.cdVisc;
  }
  return 0.0;
}

}  // namespace

core::Result<PolarLayout> makePolarLayout(const PolarWriteRequest& request) {
  auto fail = [](std::string message) {
    return core::Result<PolarLayout>::fail(
        core::Error::make(core::ErrorCode::invalidArgument,
                          std::move(message)));
  };
  if (request.nbl < 1 || request.nbl > kMaxPolarElements) {
    return fail("POLWRIT: invalid element count");
  }

  PolarLayout layout;
  layout.nbl = request.nbl;

  std::array<bool, kIptot + 1> seenBase{};
  for (const int ip : request.ipol) {
    if (ip < 1 || ip > kIptot) {
      return fail("POLWRIT: invalid base column index");
    }
    if (seenBase[sz(ip)]) {
      return fail("POLWRIT: duplicate base column");
    }
    seenBase[sz(ip)] = true;
    PolarField field;
    field.kind = PolarFieldKind::base;
    field.baseIndex = ip;
    layout.fields.push_back(field);
  }

  std::array<bool, kJptot + 1> seenSide{};
  for (const int jp : request.jpol) {
    if (jp < 1 || jp > kJptot) {
      return fail("POLWRIT: invalid side column index");
    }
    if (seenSide[sz(jp)]) {
      return fail("POLWRIT: duplicate side column");
    }
    seenSide[sz(jp)] = true;
    for (int n = 1; n <= request.nbl; ++n) {
      PolarField top;
      top.kind = PolarFieldKind::sideTop;
      top.sideIndex = jp;
      top.elementIndex = n;
      layout.fields.push_back(top);
      PolarField bottom = top;
      bottom.kind = PolarFieldKind::sideBottom;
      layout.fields.push_back(bottom);
    }
  }

  const auto& selection = request.elementSelection;
  if (selection.count > core::polar::elementColumnCount) {
    return fail("POLWRIT: element selection count exceeds capacity");
  }
  std::array<bool, core::polar::elementColumnCount + 1> seenQuantity{};
  for (std::size_t k = 0; k < core::polar::elementColumnCount; ++k) {
    if (k >= selection.count) {
      if (selection.columns[k].has_value()) {
        return fail("POLWRIT: element selection has a value beyond count");
      }
      continue;
    }
    if (!selection.columns[k].has_value()) {
      return fail("POLWRIT: element selection has an empty active entry");
    }
    const int q = elementQuantityIndex(*selection.columns[k]);
    if (q < 1 || q > 4 || seenQuantity[sz(q)]) {
      return fail("POLWRIT: invalid or duplicate element quantity");
    }
    seenQuantity[sz(q)] = true;
  }

  for (int n = 1; n <= request.nbl; ++n) {
    for (std::size_t k = 0; k < selection.count; ++k) {
      PolarField field;
      field.kind = PolarFieldKind::element;
      field.elementIndex = n;
      field.elementQuantity = *selection.columns[k];
      layout.fields.push_back(field);
    }
  }
  return core::Result<PolarLayout>::ok(std::move(layout));
}

ParsedPolar readPolarText(std::string_view text, int maxPoints,
                          PolarReadCompatibility compatibility) {
  ParsedPolar result;
  bool header = true;
  bool nameSeen = false;
  bool xtripSeen = false;
  bool layoutFound = false;
  int kfFor = 0;
  int lineNo = 0;

  auto fail = [&result](int line, std::string message) {
    result.error = true;
    result.errorLine = line;
    result.errorMessage = std::move(message);
    return result;
  };

  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t nl = text.find('\n', pos);
    std::string_view raw = (nl == std::string_view::npos)
                               ? text.substr(pos, text.size() - pos)
                               : text.substr(pos, nl - pos);
    pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    ++lineNo;

    if (header) {
      if (trimRight(raw).empty()) continue;
      bool matched = false;

      if (raw.find("-----") != std::string_view::npos) {
        if (!layoutFound) {
          return fail(lineNo, "POLREAD: missing polar column header");
        }
        header = false;
        continue;
      }

      const std::size_t kVersion = raw.find("Version");
      if (kVersion != std::string_view::npos) {
        const std::size_t k1 = raw.find_first_not_of(' ');
        if (k1 != std::string_view::npos && k1 + 1 < kVersion) {
          result.code = trimRight(raw.substr(k1, kVersion - k1 - 1));
          double version = 0.0;
          const std::vector<std::string_view> parts =
              tokens(raw.substr(kVersion + 7));
          if (!parts.empty()) {
            std::string v(parts.front());
            char* end = nullptr;
            version = std::strtod(v.c_str(), &end);
            if (end != v.c_str() && *end == '\0') result.version = version;
          }
        }
        continue;
      }

      const std::size_t kf = raw.find("for:");
      if (kf != std::string_view::npos) {
        kfFor = static_cast<int>(kf);
        result.name = trimRight(raw.substr(kf + 5));
        nameSeen = true;
        matched = true;
      }

      const std::size_t ke = raw.find("elements");
      if (ke != std::string_view::npos && nameSeen) {
        int nbl = 1;
        const std::vector<std::string_view> parts =
            tokens(raw.substr(ke > 4 ? ke - 4 : 0));
        if (!parts.empty()) {
          std::string number(parts.front());
          char* end = nullptr;
          const long value = std::strtol(number.c_str(), &end, 10);
          if (end != number.c_str() && *end == '\0' && value >= 1) {
            nbl = static_cast<int>(value);
          }
        }
        result.nbl = nbl;
        if (nbl > kMaxPolarElements) {
          return fail(lineNo, "POLREAD: element count exceeds capacity");
        }
        if (kfFor > 0 && ke >= static_cast<std::size_t>(kfFor) + 5) {
          if (ke >= static_cast<std::size_t>(kfFor) + 9) {
            result.name = trimRight(raw.substr(
                static_cast<std::size_t>(kfFor) + 5,
                ke - 4 - (static_cast<std::size_t>(kfFor) + 5)));
          }
        }
        matched = true;
      }

      const std::size_t kr = raw.find("Reynolds number");
      const std::size_t km = raw.find("Mach number");
      if (kr != std::string_view::npos) {
        const std::size_t kend =
            (km != std::string_view::npos && km > kr) ? km - 1 : raw.size();
        const std::string_view seg = raw.substr(kr, kend - kr + 1);
        if (seg.find("fixed") != std::string_view::npos) result.iretyp = 1;
        else if (seg.find("1/sqrt(CL)") != std::string_view::npos)
          result.iretyp = 2;
        else if (seg.find("1/CL") != std::string_view::npos)
          result.iretyp = 3;
        matched = true;
      }
      if (km != std::string_view::npos) {
        const std::size_t kend =
            (kr != std::string_view::npos && kr > km) ? kr - 1 : raw.size();
        const std::string_view seg = raw.substr(km, kend - km + 1);
        if (seg.find("fixed") != std::string_view::npos) result.imatyp = 1;
        else if (seg.find("1/sqrt(CL)") != std::string_view::npos)
          result.imatyp = 2;
        else if (seg.find("1/CL") != std::string_view::npos)
          result.imatyp = 3;
        matched = true;
      }

      const std::size_t kx = raw.find("xtrf");
      if (kx != std::string_view::npos) {
        const std::size_t kt = raw.find("(top)");
        const std::size_t kb = raw.find("(bottom)");
        if (!xtripSeen) {
          result.xtrip[0] = 1.0;
          result.xtrip[1] = 1.0;
          if (kt != std::string_view::npos && kt > kx + 7) {
            const std::vector<std::string_view> parts =
                tokens(raw.substr(kx + 7, kt - kx - 7));
            if (!parts.empty()) {
              std::string v(parts.front());
              char* end = nullptr;
              const double value = std::strtod(v.c_str(), &end);
              if (end != v.c_str() && *end == '\0') result.xtrip[0] = value;
            }
          }
          if (kb != std::string_view::npos && kb > kt + 4) {
            const std::vector<std::string_view> parts =
                tokens(raw.substr(kt + 4, kb - kt - 4));
            if (!parts.empty()) {
              std::string v(parts.front());
              char* end = nullptr;
              const double value = std::strtod(v.c_str(), &end);
              if (end != v.c_str() && *end == '\0') result.xtrip[1] = value;
            }
          }
          xtripSeen = true;
        }
        matched = true;
      }

      const std::size_t kmach = raw.find("Mach =");
      if (kmach != std::string_view::npos) {
        const std::vector<std::string_view> parts =
            tokens(raw.substr(kmach + 6));
        if (!parts.empty()) {
          std::string v(parts.front());
          char* end = nullptr;
          const double value = std::strtod(v.c_str(), &end);
          if (end != v.c_str() && *end == '\0') result.mach1 = value;
        }
        matched = true;
      }
      const std::size_t kre = raw.find("Re =");
      if (kre != std::string_view::npos) {
        const std::vector<std::string_view> parts =
            tokens(raw.substr(kre + 4));
        if (!parts.empty()) {
          std::string v(parts.front());
          char* end = nullptr;
          const double value = std::strtod(v.c_str(), &end);
          if (end != v.c_str() && *end == '\0') result.reyn1 = value * 1.0e6;
        }
        matched = true;
      }
      const std::size_t kn = raw.find("Ncrit =");
      if (kn != std::string_view::npos) {
        const std::vector<std::string_view> parts =
            tokens(raw.substr(kn + 7));
        if (!parts.empty()) {
          std::string v(parts[0]);
          char* end = nullptr;
          const double first = std::strtod(v.c_str(), &end);
          if (end != v.c_str() && *end == '\0') {
            result.acrit[0] = first;
            result.acrit[1] = first;
            if (parts.size() > 1) {
              v = std::string(parts[1]);
              end = nullptr;
              const double second = std::strtod(v.c_str(), &end);
              if (end != v.c_str() && *end == '\0') result.acrit[1] = second;
            }
          }
        }
        matched = true;
      }
      const std::size_t kpi = raw.find("pi_p =");
      if (kpi != std::string_view::npos) {
        const std::vector<std::string_view> parts =
            tokens(raw.substr(kpi + 6));
        if (!parts.empty()) {
          std::string v(parts.front());
          char* end = nullptr;
          const double value = std::strtod(v.c_str(), &end);
          if (end != v.c_str() && *end == '\0') result.ptrat = value;
        }
        matched = true;
      }
      const std::size_t ket = raw.find("eta_p =");
      if (ket != std::string_view::npos) {
        const std::vector<std::string_view> parts =
            tokens(raw.substr(ket + 7));
        if (!parts.empty()) {
          std::string v(parts.front());
          char* end = nullptr;
          const double value = std::strtod(v.c_str(), &end);
          if (end != v.c_str() && *end == '\0') result.etap = value;
        }
        matched = true;
      }

      if (!matched && !layoutFound) {
        std::array<bool, kIptot + 1> seenBase{};
        std::array<int, kJptot + 1> topCount{};
        std::array<int, kJptot + 1> botCount{};
        std::vector<std::vector<core::polar::PolarElementQuantity>>
            elementQuantities(static_cast<std::size_t>(result.nbl + 1));

        for (const std::string_view token : tokens(raw)) {
          if (const int ip = baseIndexForName(token); ip > 0) {
            if (seenBase[sz(ip)]) {
              return fail(lineNo, "POLREAD: duplicate base column");
            }
            seenBase[sz(ip)] = true;
            PolarField field;
            field.kind = PolarFieldKind::base;
            field.baseIndex = ip;
            result.layout.fields.push_back(field);
            result.columnPresent[sz(ip - 1)] = true;
            result.ipolOrder.push_back(ip);
            continue;
          }

          bool top = false;
          if (const int jp = sideIndexForName(token, top); jp > 0) {
            int n = 0;
            if (top) {
              if (topCount[sz(jp)] >= result.nbl) {
                return fail(lineNo, "POLREAD: too many side columns");
              }
              n = ++topCount[sz(jp)];
            } else {
              if (botCount[sz(jp)] >= topCount[sz(jp)] ||
                  botCount[sz(jp)] >= result.nbl) {
                return fail(lineNo, "POLREAD: side columns are not Top/Bot pairs");
              }
              n = ++botCount[sz(jp)];
            }
            PolarField field;
            field.kind = top ? PolarFieldKind::sideTop
                             : PolarFieldKind::sideBottom;
            field.sideIndex = jp;
            field.elementIndex = n;
            result.layout.fields.push_back(field);
            result.sidePresent[sz(jp - 1)] = true;
            if (std::find(result.jpolOrder.begin(), result.jpolOrder.end(),
                          jp) == result.jpolOrder.end()) {
              result.jpolOrder.push_back(jp);
            }
            continue;
          }

          core::polar::PolarElementQuantity quantity{};
          int elementIndex = 0;
          if (parseElementToken(token, quantity, elementIndex)) {
            if (elementIndex > result.nbl) {
              return fail(lineNo, "POLREAD: element index exceeds element count");
            }
            const int q = elementQuantityIndex(quantity);
            if (q < 1 || q > 4) {
              return fail(lineNo, "POLREAD: invalid element quantity");
            }
            auto& row = elementQuantities[sz(elementIndex)];
            if (std::find(row.begin(), row.end(), quantity) != row.end()) {
              return fail(lineNo, "POLREAD: duplicate element column");
            }
            row.push_back(quantity);
            PolarField field;
            field.kind = PolarFieldKind::element;
            field.elementIndex = elementIndex;
            field.elementQuantity = quantity;
            result.layout.fields.push_back(field);
            continue;
          }

          return fail(lineNo, "POLREAD: unknown polar column");
        }

        for (int jp = 1; jp <= kJptot; ++jp) {
          if (topCount[sz(jp)] != botCount[sz(jp)] ||
              (topCount[sz(jp)] != 0 && topCount[sz(jp)] != result.nbl)) {
            return fail(lineNo, "POLREAD: incomplete side column pair");
          }
        }

        std::vector<core::polar::PolarElementQuantity> firstSelection;
        bool anyElements = false;
        for (int n = 1; n <= result.nbl; ++n) {
          if (!elementQuantities[sz(n)].empty()) anyElements = true;
          if (elementQuantities[sz(n)].empty()) continue;
          if (firstSelection.empty()) {
            firstSelection = elementQuantities[sz(n)];
          } else if (firstSelection != elementQuantities[sz(n)]) {
            return fail(lineNo, "POLREAD: element columns differ between elements");
          }
        }
        if (anyElements) {
          for (int n = 1; n <= result.nbl; ++n) {
            if (elementQuantities[sz(n)].empty()) {
              return fail(lineNo, "POLREAD: element columns missing for an element");
            }
          }
          result.elementSelection.count = firstSelection.size();
          for (std::size_t k = 0; k < firstSelection.size(); ++k) {
            result.elementSelection.columns[k] = firstSelection[k];
          }
        }

        if (result.columnPresent[sz(kIal - 1)] == false ||
            result.columnPresent[sz(kIcl - 1)] == false ||
            result.columnPresent[sz(kIcd - 1)] == false ||
            result.columnPresent[sz(kIcm - 1)] == false) {
          return fail(lineNo, "POLREAD: missing required base column");
        }

        result.layout.nbl = result.nbl;
        layoutFound = true;
        if (compatibility == PolarReadCompatibility::canonical &&
            result.version == 7.00 &&
            (result.sidePresent[kJnc - 1] || result.sidePresent[kJtp - 1] ||
             result.sidePresent[kJtn - 1] || result.sidePresent[kJti - 1])) {
          return fail(lineNo,
                      "POLREAD: XFOIL 6.996 side order requires explicit "
                      "compatibility mode");
        }
        if (compatibility ==
                PolarReadCompatibility::xfoil6996BrokenSideOrder &&
            result.version != 7.00) {
          return fail(lineNo,
                      "POLREAD: 6.996 compatibility mode requires Version 7.00");
        }
        matched = true;
      }
      continue;
    }

    if (trimRight(raw).empty()) continue;
    if (!layoutFound) {
      return fail(lineNo, "POLREAD: data before polar column header");
    }

    std::vector<double> values;
    if (!parsePolarNumbers(raw, values)) {
      return fail(lineNo, "POLREAD: invalid polar data row");
    }
    if (values.size() != result.layout.fields.size()) {
      return fail(lineNo, values.size() < result.layout.fields.size()
                              ? "POLREAD: truncated polar data row"
                              : "POLREAD: extra polar data values");
    }
    if (static_cast<int>(result.points.size()) >= maxPoints) {
      return fail(lineNo, "POLREAD: polar point capacity exceeded");
    }

    const bool haveElements = result.elementSelection.count > 0;
    if (haveElements) {
      result.elementData.emplace_back();
    }
    PolarPoint point;
    const int baseCount = static_cast<int>(result.ipolOrder.size());
    const int sideCount = static_cast<int>(result.jpolOrder.size()) *
                          result.layout.nbl * 2;
    const int elementBase = baseCount + sideCount;
    int canonicalPosition = 0;
    int elementPosition = 0;
    for (const PolarField& field : result.layout.fields) {
      int position = canonicalPosition++;
      if (compatibility ==
          PolarReadCompatibility::xfoil6996BrokenSideOrder) {
        if (field.kind == PolarFieldKind::sideTop ||
            field.kind == PolarFieldKind::sideBottom) {
          for (int i = 0; i < static_cast<int>(result.jpolOrder.size()); ++i) {
            if (result.jpolOrder[sz(i)] == field.sideIndex) {
              const int sideRow = 2 * (field.elementIndex - 1) +
                                  (field.kind == PolarFieldKind::sideBottom
                                       ? 1
                                       : 0);
              position = baseCount +
                         sideRow * static_cast<int>(result.jpolOrder.size()) +
                         i;
              break;
            }
          }
        } else if (field.kind == PolarFieldKind::element) {
          position = elementBase + elementPosition++;
        }
      }
      if (field.kind == PolarFieldKind::base) {
        point.cpol[sz(field.baseIndex - 1)] = values[sz(position)];
      } else if (field.kind == PolarFieldKind::sideTop ||
                 field.kind == PolarFieldKind::sideBottom) {
        point.side[sz(2 * (field.elementIndex - 1) +
                      (field.kind == PolarFieldKind::sideBottom ? 1 : 0))]
                  [sz(field.sideIndex - 1)] = values[sz(position)];
      } else if (haveElements) {
        switch (field.elementQuantity) {
          case core::polar::PolarElementQuantity::cx:
            result.elementData.back()[sz(field.elementIndex - 1)].cx =
                values[sz(position)];
            break;
          case core::polar::PolarElementQuantity::cy:
            result.elementData.back()[sz(field.elementIndex - 1)].cy =
                values[sz(position)];
            break;
          case core::polar::PolarElementQuantity::cm:
            result.elementData.back()[sz(field.elementIndex - 1)].cm =
                values[sz(position)];
            break;
          case core::polar::PolarElementQuantity::cdVisc:
            result.elementData.back()[sz(field.elementIndex - 1)].cdVisc =
                values[sz(position)];
            break;
        }
      }
    }

    const double acl = std::max(point.cpol[sz(kIcl - 1)], 0.001);
    if (!result.columnPresent[sz(kIre - 1)]) {
      if (result.iretyp == 1) point.cpol[sz(kIre - 1)] = result.reyn1;
      else if (result.iretyp == 2)
        point.cpol[sz(kIre - 1)] = result.reyn1 / std::sqrt(acl);
      else if (result.iretyp == 3)
        point.cpol[sz(kIre - 1)] = result.reyn1 / acl;
    }
    if (!result.columnPresent[sz(kIma - 1)]) {
      if (result.imatyp == 1) point.cpol[sz(kIma - 1)] = result.mach1;
      else if (result.imatyp == 2)
        point.cpol[sz(kIma - 1)] = result.mach1 / std::sqrt(acl);
      else if (result.imatyp == 3)
        point.cpol[sz(kIma - 1)] = result.mach1 / acl;
    }
    if (!result.sidePresent[sz(kJnc - 1)]) {
      point.side[0][sz(kJnc - 1)] = result.acrit[0];
      point.side[1][sz(kJnc - 1)] = result.acrit[1];
    }
    if (!result.sidePresent[sz(kJtp - 1)]) {
      point.side[0][sz(kJtp - 1)] = result.xtrip[0];
      point.side[1][sz(kJtp - 1)] = result.xtrip[1];
    }
    result.points.push_back(point);
  }

  result.layout.nbl = result.nbl;
  return result;
}

core::Result<std::string> writePolarTextChecked(
    const PolarWriteRequest& request) {
  const core::Result<PolarLayout> layoutResult = makePolarLayout(request);
  if (!layoutResult.isOk()) {
    return core::Result<std::string>::fail(layoutResult.error());
  }
  const PolarLayout& layout = layoutResult.value();
  const bool haveElements = layout.nbl > 0 && request.elementSelection.count > 0;
  if (haveElements && request.elementData.size() != request.points.size()) {
    return core::Result<std::string>::fail(core::Error::make(
        core::ErrorCode::sizeMismatch,
        "POLWRIT: element data count does not match point count"));
  }

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
      out += " Calculated polar for: " + nameField +
             fortranI(4, request.nbl) + " elements\n";
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
      if (request.nbl == 1) {
        out += " xtrf = " + fortranF(7, 3, request.xtrip[0]) +
               " (top)    " + fortranF(9, 3, request.xtrip[1]) +
               " (bottom)  \n";
      } else {
        out += " xtrf = " + fortranF(7, 3, request.xtrip[0]) +
               " (top)    " + fortranF(9, 3, request.xtrip[1]) +
               " (bottom)     element" + fortranI(3, n) + "\n";
      }
    }
    out += " Mach = " + fortranF(7, 3, request.mach1) + "     Re = " +
           fortranF(9, 3, request.reyn1 / 1.0e6) + " e 6     Ncrit = " +
           fortranF(7, 3, request.acrit[0]) + fortranF(7, 3, request.acrit[1]) +
           "\n";
    if (request.ptrat != 0.0) {
      out += " pi_p = " + fortranF(7, 4, request.ptrat) + "     eta_p = " +
             fortranF(9, 4, request.etap) + "\n";
    }
    out += "  \n";

    std::string lineL = " ";
    std::string lineD = " ";
    int previousElement = 0;
    for (const PolarField& field : layout.fields) {
      if (field.kind == PolarFieldKind::element &&
          field.elementIndex != previousElement) {
        lineL += "  ";
        lineD += "  ";
        previousElement = field.elementIndex;
      }
      const NumFormat& fmt = fieldFormat(field);
      const std::string name = fieldName(field);
      int lead = 0;
      int trailing = 0;
      if (field.kind == PolarFieldKind::base) {
        lead = std::max((fmt.width - static_cast<int>(name.size()) + 2) / 2, 0);
        trailing = fmt.width - static_cast<int>(name.size()) - lead;
      } else if (field.kind == PolarFieldKind::sideTop ||
                 field.kind == PolarFieldKind::sideBottom) {
        const std::size_t suffixSize =
            std::string_view(kColsName[sz(field.sideIndex)]).size();
        lead = std::max((fmt.width - static_cast<int>(suffixSize) - 2) / 2,
                        0);
        trailing = fmt.width - static_cast<int>(suffixSize) - 4 - lead;
      } else {
        lead = std::max((fmt.width - static_cast<int>(name.size()) + 2) / 2, 0);
        trailing = fmt.width - static_cast<int>(name.size()) - lead;
      }
      lineL += std::string(static_cast<std::size_t>(lead), ' ') + name +
               std::string(static_cast<std::size_t>(std::max(trailing, 0)), ' ');
      lineD += " " + std::string(static_cast<std::size_t>(fmt.width - 1), '-');
    }
    out += lineL + "\n";
    out += lineD + "\n";
  }

  for (int ia = request.ia1; ia <= request.ia2; ++ia) {
    if (ia < 1 || ia > static_cast<int>(request.points.size())) continue;
    const std::size_t pointIndex = static_cast<std::size_t>(ia - 1);
    const PolarPoint& point = request.points[pointIndex];
    std::string line = " ";
    int previousElement = 0;
    for (const PolarField& field : layout.fields) {
      if (field.kind == PolarFieldKind::element &&
          field.elementIndex != previousElement) {
        line += "  ";
        previousElement = field.elementIndex;
      }
      double value = 0.0;
      if (field.kind == PolarFieldKind::base) {
        value = baseValue(point, field.baseIndex);
      } else if (field.kind == PolarFieldKind::sideTop ||
                 field.kind == PolarFieldKind::sideBottom) {
        value = sideValue(point, field);
      } else {
        value = elementValue(
            request.elementData[pointIndex][sz(field.elementIndex - 1)],
            field.elementQuantity);
      }
      line += formatValue(fieldFormat(field), value);
    }
    out += line + "\n";
  }
  return core::Result<std::string>::ok(std::move(out));
}

std::string writePolarText(const PolarWriteRequest& request) {
  const auto checked = writePolarTextChecked(request);
  return checked.isOk() ? checked.value() : std::string{};
}

PolarAppendCheck checkPolarAppend(std::string_view existing,
                                  const PolarWriteRequest& request,
                                  PolarReadCompatibility compatibility) {
  const ParsedPolar parsed = readPolarText(existing, kNax, compatibility);
  if (parsed.error) {
    return {false, "existing polar: " + parsed.errorMessage};
  }
  const auto layoutResult = makePolarLayout(request);
  if (!layoutResult.isOk()) {
    return {false, "requested layout: " + layoutResult.error().message};
  }
  if (parsed.nbl != request.nbl || !(parsed.layout == layoutResult.value())) {
    return {false, "polar append schema differs from existing file"};
  }
  if (parsed.version != request.version) {
    return {false, "polar append version differs from existing file"};
  }
  return {true, {}};
}

ParsedReferencePolar readReferencePolarText(std::string_view text,
                                            int maxPerBlock) {
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

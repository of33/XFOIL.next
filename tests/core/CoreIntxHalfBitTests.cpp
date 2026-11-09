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

// File: tests/core/CoreIntxHalfBitTests.cpp
// bit-level proof for the canonical INTERX/HALF kernels against the
// Fortran oracle tests/data/core-intx-half-reference/6.996-s03/intx_half_reference.txt
// (S03: 6.996 LEFIND/XLFIND; the 6.99 reference remains canonical).
// (harness tests/fortran-reference/harnesses/intx-half/intx_half_harness.f,
// built canonically with -fdefault-real-8 -fdefault-double-8; unqualified
// REAL = REAL(8) = C++ double).
//
// Compared values:
// I3 LE parameters (SLE) per source, XLE from SEVAL
// I8 complete INTERX result geometry per case (N, X, Y)
// H2/H3 HALF index selection (index-marked contours) and result X/Y
// H7 final point-count state of the HALF chains
// bit comparison via IEEE-754 bits; no tolerance increase. On deviation,
// compareBitwise reports the first differing element with bits and ULP
// distance.

#include "TestSupport.h"

#include "xfoil/core/geometry/LeadingEdge.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/core/geometry/Refinement.h"
#include "xfoil/core/math/Spline.h"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace xfoil;
using namespace xfoil::core;

struct IntxCase {
  int n{-1};
  std::vector<double> x;
  std::vector<double> y;
};

struct HalfCase {
  int nin{-1};
  int n{-1};
  std::vector<double> x;
  std::vector<double> y;
  std::vector<int> chainNs;
};

struct Oracle {
  std::vector<double> sle;   // IX SLE0, SLE1, XLE0, XLE1
  double hSle1{0.0};         // IX H SLE1
  double iSle1{0.0};         // IX I SLE1
  std::vector<IntxCase> ix;  // A..I (index 0..8)
  HalfCase j, k, l, m, n, o, p, p2;
  std::vector<int> qChainNs;
};

// Parser for the harness reference format:
// "IX <mark> <tag> = <Value>" (A..I, N/X/Y)
// "IX SLE0 = ..." etc.
// "HF <mark> <tag> = ..." (J..Q, NIN/N/X/Y/STEP)
Oracle loadOracle(const char* path) {
  std::ifstream file(path, std::ios::binary);
  CHECK(static_cast<bool>(file));
  Oracle orc;
  orc.ix.resize(9);
  std::string line;
  auto valueOf = [](const std::string& l) {
    const std::size_t eq = l.find('=');
    std::string v = (eq == std::string::npos) ? std::string{} : l.substr(eq + 1);
    const std::size_t b = v.find_first_not_of(" \t");
    if (b != std::string::npos) {
      v = v.substr(b);
    }
    const std::size_t e = v.find_last_not_of(" \t");
    if (e != std::string::npos) {
      v = v.substr(0, e + 1);
    }
    return v;
  };
  while (std::getline(file, line)) {
    if (line.size() < 4) {
      continue;
    }
    const std::string value = valueOf(line);
    if (line.compare(0, 3, "IX ") == 0) {
      if (line.compare(3, 5, "SLE0 ") == 0) {
        orc.sle.push_back(std::strtod(value.c_str(), nullptr));
      } else if (line.compare(3, 5, "SLE1 ") == 0) {
        orc.sle.push_back(std::strtod(value.c_str(), nullptr));
      } else if (line.compare(3, 5, "XLE0 ") == 0) {
        orc.sle.push_back(std::strtod(value.c_str(), nullptr));
      } else if (line.compare(3, 5, "XLE1 ") == 0) {
        orc.sle.push_back(std::strtod(value.c_str(), nullptr));
      } else if (line.compare(3, 6, "H SLE1") == 0) {
        orc.hSle1 = std::strtod(value.c_str(), nullptr);
      } else if (line.compare(3, 6, "I SLE1") == 0) {
        orc.iSle1 = std::strtod(value.c_str(), nullptr);
      } else {
        const char mark = line[3];
        if (mark < 'A' || mark > 'I') {
          continue;
        }
        IntxCase& c = orc.ix[static_cast<std::size_t>(mark - 'A')];
        if (line.compare(5, 2, "N ") == 0) {
          c.n = std::atoi(value.c_str());
        } else if (line.compare(5, 2, "X ") == 0) {
          c.x.push_back(std::strtod(value.c_str(), nullptr));
        } else if (line.compare(5, 2, "Y ") == 0) {
          c.y.push_back(std::strtod(value.c_str(), nullptr));
        }
      }
      continue;
    }
    if (line.compare(0, 3, "HF ") != 0) {
      continue;
    }
    if (line.compare(3, 2, "Q ") == 0) {
      if (line.compare(5, 5, "STEP ") == 0) {
        orc.qChainNs.push_back(std::atoi(value.c_str()));
      }
      continue;
    }
    const char mark = line[3];
    HalfCase* hc = nullptr;
    switch (mark) {
      case 'J': hc = &orc.j; break;
      case 'K': hc = &orc.k; break;
      case 'L': hc = &orc.l; break;
      case 'M': hc = &orc.m; break;
      case 'N': hc = &orc.n; break;
      case 'O': hc = &orc.o; break;
      case 'P': hc = &orc.p; break;
      default: break;
    }
    if (mark == 'P' && line.compare(4, 2, "2 ") == 0) {
      hc = &orc.p2;
      if (line.compare(6, 2, "N ") == 0) {
        hc->n = std::atoi(value.c_str());
      } else if (line.compare(6, 2, "X ") == 0) {
        hc->x.push_back(std::strtod(value.c_str(), nullptr));
      } else if (line.compare(6, 2, "Y ") == 0) {
        hc->y.push_back(std::strtod(value.c_str(), nullptr));
      }
      continue;
    }
    if (hc == nullptr) {
      continue;
    }
    if (line.compare(5, 4, "NIN ") == 0) {
      hc->nin = std::atoi(value.c_str());
    } else if (line.compare(5, 2, "N ") == 0) {
      hc->n = std::atoi(value.c_str());
    } else if (line.compare(5, 2, "X ") == 0) {
      hc->x.push_back(std::strtod(value.c_str(), nullptr));
    } else if (line.compare(5, 2, "Y ") == 0) {
      hc->y.push_back(std::strtod(value.c_str(), nullptr));
    }
  }
  return orc;
}

// IEEE-754 bit comparison; reports the first deviation with bits and ULP.
bool compareBitwise(const std::vector<double>& actual,
                    const std::vector<double>& expected,
                    const char* what) {
  CHECK_EQ(actual.size(), expected.size());
  if (actual.size() != expected.size()) {
    return false;
  }
  for (std::size_t i = 0; i < actual.size(); ++i) {
    const std::uint64_t a = std::bit_cast<std::uint64_t>(actual[i]);
    const std::uint64_t e = std::bit_cast<std::uint64_t>(expected[i]);
    if (a != e) {
      const std::uint64_t ulp = (a > e) ? (a - e) : (e - a);
      std::ostringstream msg;
      msg << what << " Index " << i << " differs from: actual bits 0x"
          << std::hex << a << " expected bits 0x" << e << std::dec
          << " ULP " << ulp;
      ::testing::reportFailure(__FILE__, __LINE__, msg.str());
      return false;
    }
  }
  return true;
}

// Fortran INTX/INTE source spline: SCALC, SEGSPLD(-999,-999), LEFIND.
geometry::SplineProfile makeProfile(std::vector<double> x,
                                    std::vector<double> y) {
  geometry::SplineProfile p;
  p.x = std::move(x);
  p.y = std::move(y);
  p.s.resize(p.x.size());
  p.xp.resize(p.x.size());
  p.yp.resize(p.x.size());
  math::scalc(p.x, p.y, p.s);
  math::segsplD(p.x, p.s, p.xp, -999.0, -999.0);
  math::segsplD(p.y, p.s, p.yp, -999.0, -999.0);
  p.sle = geometry::lefind(p.x, p.xp, p.y, p.yp, p.s).sle;
  return p;
}

std::vector<double> nacaX(int designation, std::size_t nside) {
  const auto n = geometry::naca4(designation, nside);
  CHECK(n.isOk());
  return n.value().xb;
}

std::vector<double> nacaY(int designation, std::size_t nside) {
  const auto n = geometry::naca4(designation, nside);
  CHECK(n.isOk());
  return n.value().yb;
}

void runHalfChain(std::vector<double>& x, std::vector<double>& y,
                  std::size_t& n) {
  std::vector<double> s(x.size());
  math::scalc(x, y, s);
  geometry::half(s, x, y, n);
  // Fortran N=K after HALF: the active length shrinks; the harness
  // reference contains only the active points.
  x.resize(n);
  y.resize(n);
}

TEST(core, IntxHalfBitAgainstFortranHarness) {
  const Oracle orc = loadOracle(
      "tests/data/core-intx-half-reference/6.996-s03/intx_half_reference.txt");

  // I3: LE parameter and XLE from SEVAL bit-identical.
  CHECK_EQ(orc.sle.size(), std::size_t{4});
  const std::size_t nside123 = 123;
  const std::size_t nside61 = 61;
  const geometry::SplineProfile a =
      makeProfile(nacaX(12, nside123), nacaY(12, nside123));
  const geometry::SplineProfile b123 =
      makeProfile(nacaX(2412, nside123), nacaY(2412, nside123));
  compareBitwise({a.sle}, {orc.sle[0]}, "IX SLE0");
  compareBitwise({b123.sle}, {orc.sle[1]}, "IX SLE1");
  compareBitwise({math::seval(a.sle, a.x, a.xp, a.s)}, {orc.sle[2]},
                 "IX XLE0");
  compareBitwise({math::seval(b123.sle, b123.x, b123.xp, b123.s)},
                 {orc.sle[3]}, "IX XLE1");

  // I8: INTERX-result geometry per case.
  struct IxSpec {
    const geometry::SplineProfile* p0;
    const geometry::SplineProfile* p1;
    double frac;
  };
  const geometry::SplineProfile b61 =
      makeProfile(nacaX(2412, nside61), nacaY(2412, nside61));
  const geometry::SplineProfile c4412 =
      makeProfile(nacaX(4412, nside123), nacaY(4412, nside123));
  const std::vector<double> tagX = {1.0, 0.6, 0.3, 0.3, 0.0, 0.3, 0.6, 1.0};
  const std::vector<double> tagY = {0.0, 0.05, 0.08, 0.08,
                                    0.0, -0.08, -0.05, 0.0};
  const geometry::SplineProfile tag = makeProfile(tagX, tagY);
  compareBitwise({c4412.sle}, {orc.hSle1}, "IX H SLE1");
  compareBitwise({tag.sle}, {orc.iSle1}, "IX I SLE1");

  const IxSpec specs[9] = {
      {&a, &a, 0.0},   {&a, &a, 1.0},   {&a, &a, 0.5},
      {&a, &b123, 0.5}, {&a, &b61, 0.5}, {&a, &b123, -0.25},
      {&a, &b123, 1.25}, {&a, &c4412, 0.5}, {&a, &tag, 0.5},
  };
  const char ixNames[9] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I'};
  for (std::size_t i = 0; i < 9; ++i) {
    const geometry::AirfoilCoordinates got =
        geometry::interX(*specs[i].p0, *specs[i].p1, specs[i].frac);
    CHECK_EQ(got.x.size(), static_cast<std::size_t>(orc.ix[i].n));
    std::ostringstream tagBuf;
    tagBuf << "IX " << ixNames[i];
    const std::string label = tagBuf.str();
    compareBitwise(got.x, orc.ix[i].x, label.c_str());
    compareBitwise(got.y, orc.ix[i].y, (label + " Y").c_str());
  }

  // H7/H3: HALF chain J/K/L (245 -> 123 -> 62 -> 32).
  CHECK_EQ(orc.j.nin, 245);
  std::vector<double> x = nacaX(2412, nside123);
  std::vector<double> y = nacaY(2412, nside123);
  std::size_t n = x.size();
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.j.n);
  compareBitwise(x, orc.j.x, "HF J X");
  compareBitwise(y, orc.j.y, "HF J Y");
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.k.n);
  compareBitwise(x, orc.k.x, "HF K X");
  compareBitwise(y, orc.k.y, "HF K Y");
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.l.n);

  // H7: odd input (121 -> 61).
  x = nacaX(2412, nside61);
  y = nacaY(2412, nside61);
  n = x.size();
  CHECK_EQ(orc.m.nin, 121);
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.m.n);

  // H2: index-marked contour with a corner pair (result X/Y bit-identical;
  // the X values reveal the exact source-index selection).
  x = {0.0, 0.1, 0.2, 0.2, 0.3, 0.4, 0.3, 0.2, 0.1, 0.0, 0.0};
  y = {0.0, 0.01, 0.02, 0.02, 0.01, 0.0, -0.01, -0.02, -0.01, 0.0, 0.0};
  n = x.size();
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.n.n);
  compareBitwise(x, orc.n.x, "HF N X");
  compareBitwise(y, orc.n.y, "HF N Y");

  // H2: several corners.
  x = {0.0, 0.1, 0.2, 0.2, 0.3, 0.4, 0.5, 0.4, 0.4, 0.3, 0.2, 0.1};
  y = {0.0, 0.01, 0.02, 0.02, 0.015, 0.005,
       0.0, -0.005, -0.005, -0.015, -0.02, -0.01};
  n = x.size();
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.o.n);
  compareBitwise(x, orc.o.x, "HF O X");
  compareBitwise(y, orc.o.y, "HF O Y");

  // H7: smallest contour 3 -> 2, then 2 -> 2.
  x = {0.0, 0.5, 1.0};
  y = {0.0, 0.1, 0.0};
  n = x.size();
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.p.n);
  compareBitwise(x, orc.p.x, "HF P X");
  compareBitwise(y, orc.p.y, "HF P Y");
  runHalfChain(x, y, n);
  CHECK_EQ(static_cast<int>(n), orc.p2.n);
  compareBitwise(x, orc.p2.x, "HF P2 X");
  compareBitwise(y, orc.p2.y, "HF P2 Y");

  // H7: repeated HALF until 2.
  x = nacaX(2412, nside123);
  y = nacaY(2412, nside123);
  n = x.size();
  for (int step = 0; step < 8; ++step) {
    runHalfChain(x, y, n);
    CHECK_EQ(static_cast<int>(n), orc.qChainNs[static_cast<std::size_t>(step)]);
  }
}

}  // namespace

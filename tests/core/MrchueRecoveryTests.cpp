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

// File: tests/core/MrchueRecoveryTests.cpp
// Permanent direct proof of the MRCHUE fallback path (label 109,
// xbl.f:809-827: TRCHEK execution, AMI=AMPL2, ITRAN rewriting, second
// BLPRV/BLKIN with the updated AMI).
//
// Case: Clark-Y batch contract, alpha 10.0 (SETBL call 341), upper side,
// Fortran station 22 (C++ ibl=21), Newton path not converged with
// dmax>0.1. Before the fix AMI stayed at the pre-update value
// 1.8671840454999331 instead of the Fortran AMPL2 2.0033858719407260;
// MRCHUE stores CTAU(IBL,IS)=AMI on laminar stations, so the stored CTAU at
// station 22 is the critical intermediate value.
//
// References (build flags -fdefault-real-8
// -fdefault-double-8):
// tests/data/core-c11a-state-reference/c11a_mrchue341_probe_reference.txt
// (c11a_mrchue341_probe.f: stall-region contract with F341*/U* WRITEs,
// LDUMPM for ISET=341)
// tests/data/core-c11a-state-reference/c11a_mrchue341_fixture.txt
// (core before-state at the entry to mrchue from call 341; in the test it
// is first validated bit-exactly against F341/F341M/F341I of the Fortran
// probe)
//
// the test calls mrchue directly without a production test hook and checks
// exact equality (0 ULP). No /tmp artifacts.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "TestSupport.h"

#include "xfoil/core/boundary_layer/Marching.h"

namespace {

namespace bl = xfoil::core::boundary_layer;

constexpr const char* kFixturePath =
    "tests/data/core-c11a-state-reference/c11a_mrchue341_fixture.txt";
constexpr const char* kProbePath =
    "tests/data/core-c11a-state-reference/"
    "c11a_mrchue341_probe_reference.txt";

// Fortran station 22 (1-based) -> C++ 0-based.
constexpr int kStation22 = 21;
// Fortran AMPL2 at station 22 after label 109 (U10/U6, directly
// Fortran-confirmed; the pre-fix value was 1.8671840454999331).
constexpr double kAmpiFortran = 2.0033858719407260;
constexpr double kAmpiPreFix = 1.8671840454999331;

std::vector<std::string> loadLines(const char* path) {
  std::ifstream in(path);
  CHECK(in.good());
  std::vector<std::string> out;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] == '#') {
      continue;
    }
    out.push_back(line);
  }
  return out;
}

std::unordered_map<std::string, std::vector<double>> parseFixture(
    const std::vector<std::string>& lines) {
  std::unordered_map<std::string, std::vector<double>> d;
  for (const auto& l : lines) {
    std::istringstream is(l);
    std::string key;
    is >> key;
    double v = 0.0;
    while (is >> v) {
      d[key].push_back(v);
    }
  }
  return d;
}

struct ProbeData {
  // F341I
  int itran1 = 0, itran2 = 0, iblte1 = 0, iblte2 = 0, nbl1 = 0, nbl2 = 0;
  // F341: index (IS-1)*maxN + (IBL-1) -> 5 primary values
  std::vector<double> preCtau, preThet, preDstr, preUedg, preMass;
  // F341M
  std::vector<double> preIpan, preVti, preXssi;
  // U10 (IS=1): per IBL (1-based) stored after the save
  std::unordered_map<int, std::vector<double>> u10;  // IBL -> 6 values
  // U11 (IS=1, IBL=22): TAU DIS CTQ DELT TSTR
  std::vector<double> u11;
};

ProbeData parseProbe(const std::vector<std::string>& lines) {
  ProbeData p;
  std::string curU10Key;
  int curU10Ibl = 0;
  for (const auto& l : lines) {
    std::istringstream is(l);
    std::string t1, t2;
    is >> t1 >> t2;
    if (t1 == "F341I") {
      const std::string& name = t2;
      double v = 0.0;
      is >> v;
      if (name == "ITRAN1") p.itran1 = static_cast<int>(v);
      if (name == "ITRAN2") p.itran2 = static_cast<int>(v);
      if (name == "IBLTE1") p.iblte1 = static_cast<int>(v);
      if (name == "IBLTE2") p.iblte2 = static_cast<int>(v);
      if (name == "NBL1") p.nbl1 = static_cast<int>(v);
      if (name == "NBL2") p.nbl2 = static_cast<int>(v);
    } else if (t1 == "F341") {
      int ibl = 0;
      double c = 0.0, t = 0.0, d = 0.0, u = 0.0, m = 0.0;
      const int isp = std::stoi(t2);
      is >> ibl >> c >> t >> d >> u >> m;
      if (isp == 1) {
        p.preCtau.push_back(c);
        p.preThet.push_back(t);
        p.preDstr.push_back(d);
        p.preUedg.push_back(u);
        p.preMass.push_back(m);
      }
    } else if (t1 == "F341M") {
      int ibl = 0;
      double ip = 0.0, vt = 0.0, xs = 0.0;
      const int isp = std::stoi(t2);
      is >> ibl >> ip >> vt >> xs;
      if (isp == 1) {
        p.preIpan.push_back(ip);
        p.preVti.push_back(vt);
        p.preXssi.push_back(xs);
      }
    } else if (t1 == "U10") {
      std::string f;
      is >> f;
      if (f == "IBL") {
        is >> curU10Ibl;
        p.u10[curU10Ibl] = std::vector<double>(6, 0.0);
      } else {
        double v = 0.0;
        is >> v;
        int idx = -1;
        if (f == "CTAU") idx = 0;
        if (f == "THET") idx = 1;
        if (f == "DSTR") idx = 2;
        if (f == "UEDG") idx = 3;
        if (f == "MASS") idx = 4;
        if (f == "ITRAN(IS)") idx = 5;
        if (idx >= 0 && curU10Ibl > 0) {
          p.u10[curU10Ibl][static_cast<std::size_t>(idx)] = v;
        }
      }
    } else if (t1 == "U11") {
      std::string f;
      is >> f;
      double v = 0.0;
      is >> v;
      p.u11.push_back(v);  // TAU DIS CTQ DELT TSTR in order
    }
  }
  return p;
}

std::uint64_t bitsOf(double v) {
  std::uint64_t u = 0;
  static_assert(sizeof(double) == sizeof(std::uint64_t));
  std::memcpy(&u, &v, sizeof(u));
  return u;
}

// PUB-W1: the pre-state replay pins the marched BL values against the
// canonical Fortran probe. On Linux the bits are identical and must
// stay identical (contract class kReplayValue degrades to a bitwise
// comparison on exact platforms). On Windows/MSYS2 the recorded run
// showed kernel-level deviations up to rel 1.33e-14 (max 117 ULP);
// the class allows |a-e| <= 1e-14 + 1e-13*|e| there only.
void checkBits(double actual, double expected, const char* what) {
  testing::checkPortableNumeric(actual, expected,
                                testing::PortableNumericClass::kReplayValue,
                                what, "fortran reference", __FILE__,
                                __LINE__);
}

}  // namespace

TEST(core, MrchueRecoveryAlpha10Station22Replay) {
  const auto fx = parseFixture(loadLines(kFixturePath));
  const auto pr = parseProbe(loadLines(kProbePath));

  // 1) Validate the fixture before-state against the canonical Fortran probe.
  CHECK_EQ(static_cast<int>(fx.at("map.top.nbl").front()) + 1, pr.nbl1);
  CHECK_EQ(static_cast<int>(fx.at("map.top.iblte").front()) + 1, pr.iblte1);
  CHECK_EQ(static_cast<int>(fx.at("map.bottom.nbl").front()) + 1, pr.nbl2);
  CHECK_EQ(static_cast<int>(fx.at("map.bottom.iblte").front()) + 1, pr.iblte2);
  CHECK_EQ(static_cast<int>(fx.at("itran").front()) + 1, pr.itran1);
  CHECK_EQ(static_cast<int>(fx.at("itran")[1]) + 1, pr.itran2);
  CHECK_EQ(pr.preCtau.size(), static_cast<std::size_t>(pr.nbl1));
  for (std::size_t i = 0; i < static_cast<std::size_t>(pr.nbl1); ++i) {
    checkBits(fx.at("ctau.top")[i], pr.preCtau[i], "pre CTAU");
    checkBits(fx.at("thet.top")[i], pr.preThet[i], "pre THET");
    checkBits(fx.at("dstr.top")[i], pr.preDstr[i], "pre DSTR");
    checkBits(fx.at("uedg.top")[i], pr.preUedg[i], "pre UEDG");
    checkBits(fx.at("mass.top")[i], pr.preMass[i], "pre MASS");
    // Fortran IPAN(1,1)=0 is unset; C++ uses 0-based panels, so the
    // sentinel stays on station 1 but at 0.
    const int ipFor = static_cast<int>(pr.preIpan[i]);
    CHECK_EQ(static_cast<int>(fx.at("map.top.ipan")[i]),
             ipFor == 0 ? 0 : ipFor - 1);
    checkBits(fx.at("map.top.vti")[i], pr.preVti[i], "pre VTI");
    checkBits(fx.at("xssi0")[i], pr.preXssi[i], "pre XSSI");
  }

  // 2) build MarchInput/MarchSideState from the fixture.
  bl::MarchInput in{};
  in.mapping.top.iblte = static_cast<int>(fx.at("map.top.iblte").front());
  in.mapping.top.nbl = static_cast<int>(fx.at("map.top.nbl").front());
  in.mapping.bottom.iblte = static_cast<int>(fx.at("map.bottom.iblte").front());
  in.mapping.bottom.nbl = static_cast<int>(fx.at("map.bottom.nbl").front());
  in.mapping.iblmax = static_cast<int>(fx.at("map.iblmax").front());
  in.mapping.valid = true;
  auto copyInt = [](const std::vector<double>& v) {
    std::vector<int> out(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
      out[i] = static_cast<int>(v[i]);
    }
    return out;
  };
  in.mapping.top.ipan = copyInt(fx.at("map.top.ipan"));
  in.mapping.top.vti = fx.at("map.top.vti");
  in.mapping.bottom.ipan = copyInt(fx.at("map.bottom.ipan"));
  in.mapping.bottom.vti = fx.at("map.bottom.vti");
  auto xssiTop = fx.at("xssi0");
  auto xssiBot = fx.at("xssi1");
  auto wgap = fx.at("wgap");
  in.xssi = {std::span<const double>(xssiTop), std::span<const double>(xssiBot)};
  in.wgap = std::span<const double>(wgap);
  in.acrit = {fx.at("acrit")[0], fx.at("acrit")[1]};
  in.xiforc = {fx.at("xiforc")[0], fx.at("xiforc")[1]};
  in.ante = fx.at("ante").front();
  const auto& pp = fx.at("props");
  in.props.gambl = pp[0];
  in.props.gm1bl = pp[1];
  in.props.qinfbl = pp[2];
  in.props.tkbl = pp[3];
  in.props.tkblMs = pp[4];
  in.props.rstbl = pp[5];
  in.props.rstblMs = pp[6];
  in.props.hstinv = pp[7];
  in.props.hstinvMs = pp[8];
  in.props.hvrat = pp[9];
  in.props.herat = pp[10];
  in.props.heratMs = pp[11];
  in.props.reybl = pp[12];
  in.props.reyblMs = pp[13];
  in.props.reyblRe = pp[14];
  in.props.dwte = pp[15];
  in.props.idampv = static_cast<int>(fx.at("idampv").front());
  const auto& bp = fx.at("blpar");
  in.blpar.sccon = bp[0];
  in.blpar.gacon = bp[1];
  in.blpar.gbcon = bp[2];
  in.blpar.gccon = bp[3];
  in.blpar.dlcon = bp[4];
  in.blpar.ctrcon = bp[5];
  in.blpar.ctrcex = bp[6];
  in.blpar.duxcon = bp[7];
  in.blpar.ctcon = bp[8];
  in.blpar.cffac = bp[9];
  in.thet.top = fx.at("thet.top");
  in.thet.bottom = fx.at("thet.bottom");
  in.dstr.top = fx.at("dstr.top");
  in.dstr.bottom = fx.at("dstr.bottom");
  in.ctau.top = fx.at("ctau.top");
  in.ctau.bottom = fx.at("ctau.bottom");
  in.uedg.top = fx.at("uedg.top");
  in.uedg.bottom = fx.at("uedg.bottom");
  in.mass.top = fx.at("mass.top");
  in.mass.bottom = fx.at("mass.bottom");
  in.tau.top = fx.at("tau.top");
  in.tau.bottom = fx.at("tau.bottom");
  in.dis.top = fx.at("dis.top");
  in.dis.bottom = fx.at("dis.bottom");
  in.ctq.top = fx.at("ctq.top");
  in.ctq.bottom = fx.at("ctq.bottom");
  in.delt.top = fx.at("delt.top");
  in.delt.bottom = fx.at("delt.bottom");
  in.tstr.top = fx.at("tstr.top");
  in.tstr.bottom = fx.at("tstr.bottom");
  in.itran = {static_cast<int>(fx.at("itran")[0]),
              static_cast<int>(fx.at("itran")[1])};
  in.tforce = {fx.at("tforce")[0] != 0.0, fx.at("tforce")[1] != 0.0};
  in.xssitr = {fx.at("xssitr")[0], fx.at("xssitr")[1]};

  auto buildSide = [&](int i) {
    bl::MarchSideState st{};
    const std::string p = "st" + std::to_string(i) + ".";
    auto g = [&](const char* k) { return fx.at(p + k).front(); };
    st.com1.x = g("com1.x");
    st.com1.ue = g("com1.ue");
    st.com1.t = g("com1.t");
    st.com1.d = g("com1.d");
    st.com1.s = g("com1.s");
    st.com1.ampl = g("com1.ampl");
    st.com1.dw = g("com1.dw");
    st.com1.m = g("com1.m");
    st.com1.mU = g("com1.mU");
    st.com1.mMs = g("com1.mMs");
    st.com1.h = g("com1.h");
    st.com1.hT = g("com1.hT");
    st.com1.hD = g("com1.hD");
    st.com1.hk = g("com1.hk");
    st.com1.hkU = g("com1.hkU");
    st.com1.hkT = g("com1.hkT");
    st.com1.hkD = g("com1.hkD");
    st.com1.hkMs = g("com1.hkMs");
    st.com1.hs = g("com1.hs");
    st.com1.hsU = g("com1.hsU");
    st.com1.hsT = g("com1.hsT");
    st.com1.hsD = g("com1.hsD");
    st.com1.hsMs = g("com1.hsMs");
    st.com1.hsRe = g("com1.hsRe");
    st.com1.hc = g("com1.hc");
    st.com1.hcU = g("com1.hcU");
    st.com1.hcT = g("com1.hcT");
    st.com1.hcD = g("com1.hcD");
    st.com1.hcMs = g("com1.hcMs");
    st.com1.rt = g("com1.rt");
    st.com1.rtU = g("com1.rtU");
    st.com1.rtT = g("com1.rtT");
    st.com1.rtMs = g("com1.rtMs");
    st.com1.rtRe = g("com1.rtRe");
    st.com1.cf = g("com1.cf");
    st.com1.cfU = g("com1.cfU");
    st.com1.cfT = g("com1.cfT");
    st.com1.cfD = g("com1.cfD");
    st.com1.cfMs = g("com1.cfMs");
    st.com1.cfRe = g("com1.cfRe");
    st.com1.di = g("com1.di");
    st.com1.diU = g("com1.diU");
    st.com1.diT = g("com1.diT");
    st.com1.diD = g("com1.diD");
    st.com1.diS = g("com1.diS");
    st.com1.diMs = g("com1.diMs");
    st.com1.diRe = g("com1.diRe");
    st.com1.us = g("com1.us");
    st.com1.usU = g("com1.usU");
    st.com1.usT = g("com1.usT");
    st.com1.usD = g("com1.usD");
    st.com1.usMs = g("com1.usMs");
    st.com1.cq = g("com1.cq");
    st.com1.cqU = g("com1.cqU");
    st.com1.cqT = g("com1.cqT");
    st.com1.cqD = g("com1.cqD");
    st.com1.cqMs = g("com1.cqMs");
    st.com1.cqRe = g("com1.cqRe");
    st.com1.de = g("com1.de");
    st.com1.deU = g("com1.deU");
    st.com1.deT = g("com1.deT");
    st.com1.deD = g("com1.deD");
    st.com1.deMs = g("com1.deMs");
    st.ueEi1 = g("ueEi1");
    st.ueMs1 = g("ueMs1");
    st.ampl1 = g("ampl1");
    st.xt = g("xt");
    st.xtA1 = g("xtA1");
    st.xtT1 = g("xtT1");
    st.xtD1 = g("xtD1");
    st.xtU1 = g("xtU1");
    st.xtX1 = g("xtX1");
    st.xtT2 = g("xtT2");
    st.xtD2 = g("xtD2");
    st.xtU2 = g("xtU2");
    st.xtX2 = g("xtX2");
    st.xtMs = g("xtMs");
    st.xtRe = g("xtRe");
    st.xtXf = g("xtXf");
    const auto& fl = fx.at(p + "flags");
    st.tran = fl[0] != 0.0;
    st.turb = fl[1] != 0.0;
    st.trforc = fl[2] != 0.0;
    return st;
  };
  std::array<bl::MarchSideState, 2> state = {buildSide(0), buildSide(1)};

  // 3) Execute MRCHUE directly.
  const auto diag = bl::mrchue(in, state);

  // 4) entry into the MRCHUE recovery path (not converged, upper side,
  // station 22 = 0-based 21).
  CHECK_FALSE(diag.converged);
  CHECK_EQ(diag.failedSide, 0);

  // 5) AMI takes over AMPL2: on the laminar station 22,
  // CTAU(IBL,IS)=AMI is stored.
  checkBits(in.ctau.top[kStation22], kAmpiFortran, "CTAU(22)=AMI=AMPL2");
  CHECK(bitsOf(in.ctau.top[kStation22]) != bitsOf(kAmpiPreFix));

  // 6) Complete upper-side exit against U10 (all saves).
  for (int ibl = 2; ibl <= pr.nbl1; ++ibl) {
    const auto& u = pr.u10.at(ibl);
    const std::size_t i = static_cast<std::size_t>(ibl - 1);
    checkBits(in.ctau.top[i], u[0], "U10 CTAU");
    checkBits(in.thet.top[i], u[1], "U10 THET");
    checkBits(in.dstr.top[i], u[2], "U10 DSTR");
    checkBits(in.uedg.top[i], u[3], "U10 UEDG");
    checkBits(in.mass.top[i], u[4], "U10 MASS");
  }
  // ITRAN rewriting: final value after marching (U10 at the last save,
  // 1-based) against C++ 0-based.
  CHECK_EQ(in.itran[0] + 1,
           static_cast<int>(pr.u10.at(pr.nbl1)[5]));

  // 7) second BLPRV/BLKIN with updated AMI: the BLVAR fields
  // TAU/DIS/CTQ/DELT/TSTR at station 22 (U11).
  CHECK_EQ(pr.u11.size(), 5u);
  checkBits(in.tau.top[kStation22], pr.u11[0], "U11 TAU");
  checkBits(in.dis.top[kStation22], pr.u11[1], "U11 DIS");
  checkBits(in.ctq.top[kStation22], pr.u11[2], "U11 CTQ");
  checkBits(in.delt.top[kStation22], pr.u11[3], "U11 DELT");
  checkBits(in.tstr.top[kStation22], pr.u11[4], "U11 TSTR");
}

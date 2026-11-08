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

// file: tests/core/IoTests.cpp
// IO integration tests: byte comparison of the parser and serializer
// against the canonical Fortran references under
// tests/data/core-polar-io-reference/ (text serializers and binary dump).

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <span>
#include <string>
#include <vector>

#include "TestSupport.h"
#include "xfoil/core/inverse/OrrSommerfeldMap.h"
#include "xfoil/io/DefaultsCodec.h"
#include "xfoil/io/DumpCodec.h"
#include "xfoil/io/ExportFormats.h"
#include "xfoil/io/OrrSommerfeldCodec.h"
#include "xfoil/core/math/Spline.h"
#include "xfoil/io/PolarText.h"
#include "xfoil/io/TextFormat.h"

namespace io = xfoil::io;
namespace ex = xfoil::core::export_data;
namespace cmath = xfoil::core::math;

namespace {

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<std::uint8_t> readBytes(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}

const char* kPolFixtures[] = {
    "polar-inv-0012/generated/polar-inv-0012.pol",
    "polar-inv-2412/generated/inv2412.pol",
    "polar-inv-clark-y/generated/polar-inv-clark-y.pol",
    "polar-nonconverging/generated/polar-nonconverging.pol",
    "polar-state-ascending/generated/polar-state-ascending.pol",
    "polar-state-cl-driven/generated/polar-state-cl-driven.pol",
    "polar-state-descending/generated/polar-state-descending.pol",
    "polar-visc-0012/generated/polar-visc-0012.pol",
    "polar-visc-2412/generated/visc2412.pol",
    "polar-visc-2412-re3e6/generated/polar-visc-2412-re3e6.pol",
    "polar-visc-clark-y/generated/polar-visc-clark-y.pol",
};

const char* kDumpFixtures[] = {
    "polar-visc-0012/generated/polar-visc-0012.dump",
    "polar-visc-2412/generated/visc2412.dump",
    "polar-visc-2412-re3e6/generated/polar-visc-2412-re3e6.dump",
    "polar-visc-clark-y/generated/polar-visc-clark-y.dump",
};

}  // namespace

TEST(unit, IoTextFormat) {
  CHECK_EQ(io::fortranF(7, 3, 1.0), std::string("  1.000"));
  CHECK_EQ(io::fortranF(9, 4, -0.1), std::string("  -0.1000"));
  CHECK_EQ(io::fortranF(10, 6, 0.006), std::string("  0.006000"));
  CHECK_EQ(io::fortranF(10, 4, std::nan("")), std::string("       NaN"));
  CHECK_EQ(io::fortranF(10, 6, std::numeric_limits<double>::infinity()), std::string("  Infinity"));
  // Fortran E normalizes the mantissa into the range [0.1,1); the
  // following target values are stdout slices taken from the canonical
  // fortran-bin/xfoil (VISCAL FORMAT 2000/2010, xoper.f 3103).
  CHECK_EQ(io::fortranE(11, 3, 3.0e6), std::string("  0.300E+07"));
  CHECK_EQ(io::fortranE(10, 4, 0.12717), std::string("0.1272E+00"));
  CHECK_EQ(io::fortranE(10, 4, -0.9391), std::string("-.9391E+00"));
  CHECK_EQ(io::fortranE(10, 4, 4.5636e-9), std::string("0.4564E-08"));
  CHECK_EQ(io::fortranI(3, 7), std::string("  7"));
  CHECK_EQ(io::fortranI(4, 120), std::string(" 120"));
  CHECK_EQ(io::fortranL2(true), std::string(" T"));
  const auto parsed = io::parseFloatList(" 1.0, 2.5", 0);
  CHECK(!parsed.error);
  CHECK_EQ(parsed.extracted, 2);
  CHECK_EQ(parsed.values[1], 2.5);
  const auto bad = io::parseFloatList("1.0 xx", 2);
  CHECK(bad.error);
}

TEST(unit, IoPolarRoundTripFixtures) {
  for (const char* rel : kPolFixtures) {
    const std::string path = std::string("tests/fortran-reference/cases/polar/") + rel;
    const std::string original = readFile(path);
    const io::ParsedPolar parsed = io::readPolarText(original);
    CHECK(!parsed.error);
    io::PolarWriteRequest req;
    req.lhead = true;
    req.ia1 = 1;
    req.ia2 = static_cast<int>(parsed.points.size());
    req.ipol = parsed.ipolOrder;
    req.jpol = parsed.jpolOrder;
    req.points = parsed.points;
    req.code = parsed.code;
    req.version = parsed.version;
    req.name = parsed.name;
    req.nbl = parsed.nbl;
    req.iretyp = parsed.iretyp;
    req.imatyp = parsed.imatyp;
    req.mach1 = parsed.mach1;
    req.reyn1 = parsed.reyn1;
    req.acrit = parsed.acrit;
    req.xtrip = parsed.xtrip;
    req.ptrat = parsed.ptrat;
    req.etap = parsed.etap;
    const std::string written = io::writePolarText(req);
    CHECK_EQ(written, original);
  }
}

TEST(unit, IoPolarReadMetadata) {
  const std::string text =
      readFile("tests/fortran-reference/cases/polar/polar-visc-0012/generated/polar-visc-0012.pol");
  const io::ParsedPolar parsed = io::readPolarText(text);
  CHECK(!parsed.error);
  CHECK_EQ(parsed.code, std::string("XFOIL"));
  CHECK_EQ(parsed.nbl, 1);
  CHECK_EQ(parsed.iretyp, 1);
  CHECK_EQ(parsed.imatyp, 1);
  CHECK_EQ(parsed.reyn1, 1.0e6);
  CHECK(parsed.points.size() >= 4u);
  CHECK_EQ(parsed.ipolOrder.size(), 5u);
  CHECK_EQ(parsed.jpolOrder.size(), 2u);
}

TEST(unit, IoPolarEmptyAndLimits) {
  const io::ParsedPolar empty = io::readPolarText("");
  CHECK(!empty.error);
  CHECK_EQ(empty.points.size(), 0u);
  std::string many = "  \n       XFOIL         Version 6.99\n  \n"
                     " Calculated polar for: T                                      "
                     "\n  \n 1 1 Reynolds number fixed          Mach number fixed         \n  \n"
                     " xtrf =   1.000 (top)        1.000 (bottom)  \n"
                     " Mach =   0.000     Re =     1.000 e 6     Ncrit =   9.000  9.000\n  \n"
                     "   alpha    CL        CD       CDp       CM\n"
                     "  ------ -------- --------- --------- --------\n";
  for (int i = 0; i < 5; ++i) {
    many += "   1.000   0.1000  0.00100  0.00000   -0.1000\n";
  }
  const io::ParsedPolar limited = io::readPolarText(many, 3);
  CHECK(limited.error);
  CHECK_EQ(limited.errorLine, 16);
}

TEST(unit, IoPolRefParse) {
  const std::string text =
      readFile("tests/fortran-reference/harnesses/polar-io/data/refpolar_input.txt");
  const io::ParsedReferencePolar ref = io::readReferencePolarText(text);
  CHECK(!ref.error);
  CHECK_EQ(ref.label, std::string("CLARK Y flap 0 deg reference polar"));
  CHECK_EQ(ref.blockSizes[0], 3);
  CHECK_EQ(ref.blockSizes[1], 3);
  CHECK_EQ(ref.blockSizes[2], 2);
  CHECK_EQ(ref.blockSizes[3], 2);
  CHECK_EQ(ref.blocks[0][2][0], 1.0);
}

TEST(unit, IoDumpDecodeFixtures) {
  for (const char* rel : kDumpFixtures) {
    const std::string path = std::string("tests/fortran-reference/cases/polar/") + rel;
    const std::vector<std::uint8_t> bytes = readBytes(path);
    const io::DumpDecodeResult decoded = io::decodeDump(bytes.data(), bytes.size());
    CHECK(decoded.file.has_value());
    if (!decoded.file) continue;
    CHECK(!decoded.file->header.name.empty());
    CHECK(!decoded.file->points.empty());
    // Re-encoding must be byte-identical.
    std::vector<std::uint8_t> re;
    re = io::encodeDumpHeader(decoded.file->header);
    for (const auto& point : decoded.file->points) {
      const std::vector<std::uint8_t> enc = io::encodeDumpPoint(point);
      re.insert(re.end(), enc.begin(), enc.end());
    }
    CHECK_EQ(re.size(), bytes.size());
    CHECK(re == bytes);
  }
}

TEST(unit, IoPlxaddEndToEndByteIdentical) {
  const std::vector<std::uint8_t> bytes = readBytes(
      "tests/data/core-polar-io-reference/plxadd_case_dump_reference.bin");
  const io::DumpDecodeResult decoded = io::decodeDump(bytes.data(), bytes.size());
  CHECK(decoded.file.has_value());
  if (!decoded.file) return;
  CHECK_EQ(decoded.file->points.size(), 2u);

 // domain projection (B) from harness inputs.
  ex::PlxaddInputs in;
  in.viscous = true;
  in.minf = 0.3;
  in.qinf = 0.0;
  in.alfaRad = std::numeric_limits<double>::infinity();
  in.cl = 0.3;
  in.cd = 0.012;
  in.cm = -0.1;
  in.xoctrTop = 0.5;
  in.xoctrBottom = 0.4;
  in.x = {1.0, 0.5, 0.0, 0.5, 1.0, 0.5, 1.0};
  const std::vector<double> y = {0.0, 0.05, 0.0, -0.05, 0.0, 0.02, 0.0};
  in.s.assign(7, 0.0);
  in.xp.assign(7, 0.0);
  std::vector<double> yp(7, 0.0);
  cmath::scalc(std::span<const double>(in.x), std::span<const double>(y),
                    std::span<double>(in.s));
  cmath::segspl(std::span<const double>(in.x), std::span<const double>(in.s),
                     std::span<double>(in.xp));
  cmath::segspl(std::span<const double>(y), std::span<const double>(in.s),
                     std::span<double>(yp));
  in.sst = 0.5 * (in.s[0] + in.s[6]);
  in.cpv.resize(7);
  // CPV(I) = 1.0 - GAM(I)**2 as in the Fortran harness
  // (tests/fortran-reference/harnesses/polar-io/plxadd_harness.f:59-60;
  // for -O0 without FMA contraction, only GAM**2 is rounded, then the
  // subtraction follows). The volatile-forced intermediate result rounds
  // the square before the subtraction and prevents FMA contraction, so that
  // the bit pattern of the reference binaries is reproduced exactly,
  // independent of the compiler (GCC and Clang, debug and release).
  for (int i = 1; i <= 7; ++i) {
    const double gam = 0.8 + 0.05 * i;
    volatile double sq = gam * gam;
    in.cpv[static_cast<std::size_t>(i - 1)] = 1.0 - sq;
  }
  for (int is = 0; is < 2; ++is) {
    ex::PlxaddSideInputs& side = in.sides[is];
    side.nbl = (is == 0) ? 5u : 6u;
    side.iblte = 4;
    side.ipan.assign(7, 0);
    side.thet.assign(7, 0.0);
    side.dstr.assign(7, 0.0);
    side.ctau.assign(7, 0.0);
    side.tau.assign(7, 0.0);
    for (std::size_t ibl = 1; ibl <= side.nbl; ++ibl) {
      side.ipan[ibl] = static_cast<int>((ibl - 1) % 5) + 1;
      side.dstr[ibl] = 0.001 * static_cast<double>(ibl);
      side.thet[ibl] = 0.0005 * static_cast<double>(ibl);
      side.ctau[ibl] = 0.00002 * static_cast<double>(ibl);
      side.tau[ibl] = 0.0001 * static_cast<double>(ibl);
    }
  }
  const auto p1 = ex::projectPlxaddPoint(in);
  CHECK(p1.isOk());
  in.cl = 0.45;
  in.cd = 0.015;
  for (int is = 0; is < 2; ++is) {
    for (std::size_t ibl = 1; ibl <= in.sides[is].nbl; ++ibl) {
      in.sides[is].dstr[ibl] = 0.0012 * static_cast<double>(ibl);
      in.sides[is].thet[ibl] = 0.0006 * static_cast<double>(ibl);
      in.sides[is].tau[ibl] = 0.00012 * static_cast<double>(ibl);
    }
  }
  const auto p2 = ex::projectPlxaddPoint(in);
  CHECK(p2.isOk());

  std::vector<std::uint8_t> re = io::encodeDumpHeader(decoded.file->header);
  const std::vector<std::uint8_t> e1 = io::encodeDumpPoint(io::toDumpPoint(p1.value()));
  re.insert(re.end(), e1.begin(), e1.end());
  const std::vector<std::uint8_t> e2 = io::encodeDumpPoint(io::toDumpPoint(p2.value()));
  re.insert(re.end(), e2.begin(), e2.end());
  CHECK_EQ(re.size(), bytes.size());
  CHECK(re == bytes);
}

// Permanent regression for the PLXADD Clang release finding:
// the first domain-specific differing field was Cp (not XX/CF) at the
// stations derived from CPV. The deviation arose solely from FMA
// contraction of the test input CPV(I)=1.0-GAM(I)**2 under Clang -O3 (GCC
// and the Fortran original do not contract at -O0). The production code
// copies CPV(I) unchanged (xpol.f:714). This test pins the exact bit
// pattern of the CPV-derived Cp values, the IPAN=0 null-field contract
// beyond NBL(IS), and the projection determinism of the test harness
// path.
TEST(unit, IoPlxaddCpvBitsAndIpan0NullFields) {
  auto bitsOf = [](double d) {
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof(double));
    return u;
  };

  ex::PlxaddInputs in;
  in.viscous = true;
  in.minf = 0.3;
  in.qinf = 0.0;
  in.alfaRad = std::numeric_limits<double>::infinity();
  in.cl = 0.3;
  in.cd = 0.012;
  in.cm = -0.1;
  in.xoctrTop = 0.5;
  in.xoctrBottom = 0.4;
  in.x = {1.0, 0.5, 0.0, 0.5, 1.0, 0.5, 1.0};
  const std::vector<double> y = {0.0, 0.05, 0.0, -0.05, 0.0, 0.02, 0.0};
  in.s.assign(7, 0.0);
  in.xp.assign(7, 0.0);
  std::vector<double> yp(7, 0.0);
  cmath::scalc(std::span<const double>(in.x), std::span<const double>(y),
               std::span<double>(in.s));
  cmath::segspl(std::span<const double>(in.x), std::span<const double>(in.s),
                std::span<double>(in.xp));
  cmath::segspl(std::span<const double>(y), std::span<const double>(in.s),
                std::span<double>(yp));
  in.sst = 0.5 * (in.s[0] + in.s[6]);
  in.cpv.resize(7);
  // volatile forces rounding of GAM**2 before the subtraction and blocks
  // FMA contraction (compiler-independent, GCC and Clang).
  for (int i = 1; i <= 7; ++i) {
    const double gam = 0.8 + 0.05 * i;
    volatile double sq = gam * gam;
    in.cpv[static_cast<std::size_t>(i - 1)] = 1.0 - sq;
  }
  for (int is = 0; is < 2; ++is) {
    ex::PlxaddSideInputs& side = in.sides[is];
    side.nbl = (is == 0) ? 5u : 6u;
    side.iblte = 4;
    side.ipan.assign(7, 0);
    side.thet.assign(7, 0.0);
    side.dstr.assign(7, 0.0);
    side.ctau.assign(7, 0.0);
    side.tau.assign(7, 0.0);
    for (std::size_t ibl = 1; ibl <= side.nbl; ++ibl) {
      side.ipan[ibl] = static_cast<int>((ibl - 1) % 5) + 1;
      side.dstr[ibl] = 0.001 * static_cast<double>(ibl);
      side.thet[ibl] = 0.0005 * static_cast<double>(ibl);
      side.ctau[ibl] = 0.00002 * static_cast<double>(ibl);
      side.tau[ibl] = 0.0001 * static_cast<double>(ibl);
    }
  }

  const auto p = ex::projectPlxaddPoint(in);
  CHECK(p.isOk());
  if (!p.isOk()) return;
  const io::DumpPoint dp = io::toDumpPoint(p.value());

  // CPV-derived Cp bits (Fortran reference, -O0, without FMA):
  // IBL=3 -> IPAN=3 -> CPV(3)=1-0.95^2 -> 0x3fb8f5c28f5c28f0
  // IBL=5 -> IPAN=5 -> CPV(5)=1-1.05^2 -> 0xbfba3d70a3d70a40
  for (int s = 0; s < 2; ++s) {
    CHECK_EQ(bitsOf(dp.sides[s].cp[2]), UINT64_C(0x3fb8f5c28f5c28f0));
    CHECK_EQ(bitsOf(dp.sides[s].cp[4]), UINT64_C(0xbfba3d70a3d70a40));
  }

  // IPAN=0 beyond NBL(IS): canonical COMMON-zero read case.
  // NSIDE(1)=6, NBL(1)=5 -> IBL=6 (index 5) is unset.
  CHECK_EQ(bitsOf(dp.sides[0].x[5]), UINT64_C(0));
  CHECK_EQ(bitsOf(dp.sides[0].cp[5]), UINT64_C(0));

  // projection determinism: two independent projections with
  // identical input deliver byte-identical records.
  const auto p2 = ex::projectPlxaddPoint(in);
  CHECK(p2.isOk());
  const std::vector<std::uint8_t> e1 = io::encodeDumpPoint(io::toDumpPoint(p.value()));
  const std::vector<std::uint8_t> e2 = io::encodeDumpPoint(io::toDumpPoint(p2.value()));
  CHECK_EQ(e1.size(), e2.size());
  CHECK(e1 == e2);

  // end-to-end against the first canonical Fortran record: the projected
  // and encoded sequence must be byte-identical to the oracle.
  const std::vector<std::uint8_t> bytes = readBytes(
      "tests/data/core-polar-io-reference/plxadd_case_dump_reference.bin");
  const io::DumpDecodeResult decoded = io::decodeDump(bytes.data(), bytes.size());
  CHECK(decoded.file.has_value());
  if (decoded.file && !decoded.file->points.empty()) {
    const std::vector<std::uint8_t> refE1 =
        io::encodeDumpPoint(decoded.file->points[0]);
    CHECK_EQ(refE1.size(), e1.size());
    CHECK(refE1 == e1);
  }
}

TEST(unit, IoDumpCorruptDetection) {
  const std::vector<std::uint8_t> bytes = readBytes(
      "tests/fortran-reference/cases/polar/polar-visc-0012/generated/polar-visc-0012.dump");
  // GCC 16 -O3: prove non-emptiness so the decoder never sees a null
  // data() (also the right runtime guard for a missing reference file).
  CHECK(!bytes.empty());
  if (bytes.empty()) {
    return;
  }
  std::vector<std::uint8_t> truncated(bytes.begin(), bytes.end() - 3);
  const io::DumpDecodeResult t = io::decodeDump(truncated.data(), truncated.size());
  CHECK(!t.file.has_value());
  CHECK(t.error == io::DumpError::kTruncated || t.error == io::DumpError::kMarkerMismatch);
  std::vector<std::uint8_t> corrupted = bytes;
  corrupted[0] = static_cast<std::uint8_t>(corrupted[0] ^ 0xffu);
  const io::DumpDecodeResult c = io::decodeDump(corrupted.data(), corrupted.size());
  CHECK(!c.file.has_value());
}

TEST(unit, IoDefaultsCodec) {
  const std::string text = readFile("tests/fortran-reference/harnesses/polar-io/data/getdef_full.def");
  const io::DefaultsParseResult parsed = io::parseDefaults(text);
  CHECK(parsed.complete);
  CHECK_EQ(parsed.linesRead, 14);  // GETDEF reads exactly 14 positional lines
  CHECK_EQ(parsed.values.npan, 20);
  CHECK_EQ(parsed.values.cvpar, 1.0);
  CHECK_EQ(parsed.values.minf1, 0.0);
  CHECK_EQ(parsed.values.rmill, 3.0);
  CHECK_EQ(parsed.values.acrit[0], 9.0);
  const std::string partial =
      readFile("tests/fortran-reference/harnesses/polar-io/data/getdef_partial.def");
  const io::DefaultsParseResult part = io::parseDefaults(partial);
  CHECK(!part.complete);
  // the file aborts after line 7: EOF without END= is canonically a
  // runtime error (kEndOfFile), not a READ data error (kBadLine).
  CHECK(part.error == io::DefaultsError::kEndOfFile);
  CHECK_EQ(part.linesRead, 7);
  CHECK_EQ(part.firstBadLine, 8);
  CHECK_EQ(part.values.npan, 20);  // already-read values are retained
}

TEST(unit, IoDefaultsWriteMatchesFortran) {
  const std::string text = readFile("tests/fortran-reference/harnesses/polar-io/data/getdef_full.def");
  const io::DefaultsParseResult parsed = io::parseDefaults(text);
  const std::string written = io::writeDefaults(parsed.values, 2);
  std::size_t nlCount = 0;
  for (char c : written) {
    if (c == '\n') ++nlCount;
  }
  CHECK_EQ(nlCount, 14u);  // WRTDEF writes exactly 14 lines
  const std::string reference = readFile("tests/data/core-polar-io-reference/getdef_reference.txt");
  // the WRTDEF echo lines of the harness begin after the header line.
  const std::size_t start = reference.find("Default parameters read in");
  CHECK(start != std::string::npos);
  const std::size_t nl = reference.find('\n', start);
  const std::size_t nl2 = reference.find('\n', nl + 1);
  const std::string echo = reference.substr(nl2 + 1, written.size());
  CHECK_EQ(written, echo);
}

TEST(unit, IoCndumpMatchesFortran) {
  io::CircleCoefficientDumpInput in;
  in.nc = 9;
  in.mc = 3;
  for (int m = 0; m <= 3; ++m) in.cn.push_back({0.1 * m, -0.05 * (m + 1)});
  for (int m = 1; m <= 9; ++m) {
    in.piq.push_back({std::cos(0.7 * m), std::sin(0.7 * m)});
  }
  const std::string written = io::circleCoefficientDumpText(in);
  CHECK_EQ(written, readFile("tests/data/core-polar-io-reference/cndump_reference.txt"));
}

TEST(unit, IoCpdumpMatchesFortran) {
  io::PressureDumpInput in;
  in.n = 6;
  in.minf = 0.3;
  in.qinf = 0.0;  // harness state: QINF unset -> NaN values
  in.x = {1.0, 0.6, 0.0, 0.6, 1.0, 0.6};
  in.gam = {0.55, 0.90, 1.05, 0.95, 0.70, 0.80};
  CHECK_EQ(io::pressureDumpText(in),
           readFile("tests/data/core-polar-io-reference/cpdump_case_reference.cp"));
  in.delimiter = io::DumpDelimiter::kComma;
  CHECK_EQ(io::pressureDumpText(in),
           readFile("tests/data/core-polar-io-reference/cpdump_case_csv_reference.cp"));
  in.delimiter = io::DumpDelimiter::kTab;
  CHECK_EQ(io::pressureDumpText(in),
           readFile("tests/data/core-polar-io-reference/cpdump_case_tab_reference.cp"));
}

TEST(unit, IoBldumpMatchesFortran) {
  io::BoundaryLayerDumpInput in;
  in.n = 6;
  in.nw = 2;
  in.lipan = true;
  in.lvisc = true;
  in.lwake = true;
  in.minf = 0.3;
  in.qinf = 0.0;
  in.gamm1 = 0.4;
  in.s = {0.0, 0.4, 0.8, 1.2, 1.6, 2.0, 2.4, 2.8};
  in.x = {1.0, 0.6, 0.0, 0.6, 1.0, 0.6, 0.0, 0.0};
  in.y = {0.0, 0.05, 0.0, -0.05, 0.0, 0.01, 0.0, 0.0};
  in.gam = {0.55, 0.90, 1.05, 0.95, 0.70, -0.60, -0.65, -0.70};
  in.iblte = {3, 4};
  in.nbl = {4, 6};
  for (int is = 0; is < 2; ++is) {
    for (int ibl = 0; ibl <= 6; ++ibl) {
      in.ipan[static_cast<std::size_t>(is)].push_back((ibl % 5) + 1);
      in.xssi[static_cast<std::size_t>(is)].push_back(0.3 * ibl);
      in.dstr[static_cast<std::size_t>(is)].push_back(0.002 * ibl);
      in.thet[static_cast<std::size_t>(is)].push_back(0.0008 * ibl);
      in.tstr[static_cast<std::size_t>(is)].push_back(0.0003 * ibl);
      in.tau[static_cast<std::size_t>(is)].push_back(0.00015 * ibl);
      in.dis[static_cast<std::size_t>(is)].push_back(0.00002 * ibl);
      in.uedg[static_cast<std::size_t>(is)].push_back(0.85 + 0.03 * ibl);
      in.ctau[static_cast<std::size_t>(is)].push_back(0.00003 * ibl);
    }
  }
  CHECK_EQ(io::boundaryLayerDumpText(in),
           readFile("tests/data/core-polar-io-reference/bldump_case_reference.bl"));
  in.delimiter = io::DumpDelimiter::kComma;
  CHECK_EQ(io::boundaryLayerDumpText(in),
           readFile("tests/data/core-polar-io-reference/bldump_case_csv_reference.bl"));
  in.delimiter = io::DumpDelimiter::kTab;
  CHECK_EQ(io::boundaryLayerDumpText(in),
           readFile("tests/data/core-polar-io-reference/bldump_case_tab_reference.bl"));
  in.delimiter = io::DumpDelimiter::kIllegal;
  CHECK_EQ(io::boundaryLayerDumpText(in),
           readFile("tests/data/core-polar-io-reference/bldump_case_bad_reference.bl"));
}

TEST(unit, IoNammodMatchesFortran) {
  const std::string ref = readFile("tests/data/core-polar-io-reference/nammod_reference.txt");
  std::vector<std::string> expected;
  std::size_t pos = 0;
  while (pos < ref.size()) {
    const std::size_t nl = ref.find('\n', pos);
    const std::string line = ref.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    if (line.rfind("NM ", 0) == 0) {
      std::string v = line.substr(3);
      while (!v.empty() && v.back() == ' ') v.pop_back();
      expected.push_back(v);
    }
    pos = (nl == std::string::npos) ? ref.size() : nl + 1;
  }
  CHECK_EQ(expected.size(), 9u);
  const std::string n0 = io::modifyNameWithVersion("NACA 0012", 0, 1);
  const std::string n1 = io::modifyNameWithVersion("NACA 0012 [3]", 1, -1);
  const std::string n2 = io::modifyNameWithVersion("NACA 0012 [99]", 1, -1);
  const std::string n3 = io::modifyNameWithVersion("NACA 0012 [9]", 1, -1);
  const std::string n4 = io::modifyNameWithVersion("CLARK Y", 0, 0);
  const std::string n5 = io::modifyNameWithVersion("CLARK Y", 0, 12);
  const std::string n6 = io::modifyNameWithVersion("NACA 0012 [10]", -11, -1);
  const std::string n7 = io::modifyNameWithVersion("NACA 0012 [ 5 ]", 4, -1);
  const std::string n8 = io::modifyNameWithVersion("[7]", 1, -1);
  CHECK_EQ(n0, expected[0]);
  CHECK_EQ(n1, expected[1]);
  CHECK_EQ(n2, expected[2]);
  CHECK_EQ(n3, expected[3]);
  CHECK_EQ(n4, expected[4]);
  CHECK_EQ(n5, expected[5]);
  CHECK_EQ(n6, expected[6]);
  CHECK_EQ(n7, expected[7]);
  CHECK_EQ(n8, expected[8]);
}

TEST(unit, IoVelocityRatioTableDecode) {
  const std::string text =
      readFile("tests/fortran-reference/harnesses/polar-io/data/getvov_table.dat");
  const io::VelocityRatioTableDecodeResult decoded = io::decodeVelocityRatioTable(text);
  CHECK(!decoded.error);
  CHECK_EQ(decoded.count, 6u);
  CHECK_EQ(decoded.s.size(), 6u);
  CHECK_NEAR(decoded.s[0], 0.0, 1e-12, 1e-12);
  CHECK_NEAR(decoded.s[1], 0.25, 1e-12, 1e-12);
  CHECK_NEAR(decoded.s[5], 1.0, 1e-12, 1e-12);
  CHECK_NEAR(decoded.vov[0], 0.8, 1e-12, 1e-12);
  CHECK_NEAR(decoded.vov[1], 0.9, 1e-12, 1e-12);
  const io::VelocityRatioTableDecodeResult bad =
      io::decodeVelocityRatioTable("0.0 0.0\n1.0 xx\n");
  CHECK(bad.error);
  CHECK_EQ(bad.errorLine, 2u);
}

TEST(unit, IoOrrSommerfeldDecode) {
  const std::vector<std::uint8_t> bytes = readBytes("fortran-src/xfoil-6.99/osmap.dat");
  io::OsmapDecodeResult decoded = io::decodeOrrSommerfeldRaw(bytes.data(), bytes.size());
  CHECK(decoded.raw.has_value());
  if (!decoded.raw) return;
  CHECK(decoded.raw->complete);
  CHECK(decoded.raw->nr > 0 && decoded.raw->nw > 0 && decoded.raw->nh > 0);
  const auto map = xfoil::core::inverse::OrrSommerfeldMap::fromRaw(std::move(*decoded.raw));
  CHECK(map.isOk());
}

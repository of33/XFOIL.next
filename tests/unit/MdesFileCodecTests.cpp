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

// file: tests/unit/MdesFileCodecTests.cpp
// tests the pure IO codecs (GETVOV read, CNDUMP
// write) and the READ/DUMP session use cases. Fortran anchor:
// canonical CLI oracle (LC_ALL=C fortran-bin/xfoil, three times
// byte-identical), including complete file bytes
// (tests/data/mdes-file-reference/). The test calls the session
// use cases directly, with no numerics
// recomputed in the test.

#include "TestSupport.h"

#include "app/cli/CoreGeometryHandlers.h"
#include "xfoil/core/export_data/ExportData.h"
#include "xfoil/io/MdesInverseFileCodec.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using namespace xfoil;
namespace cli = xfoil::app::cli;
namespace io = xfoil::io;
namespace session = xfoil::session;
namespace core = xfoil::core;

using session::SessionComposition;

constexpr double kF6Abs = 5.0e-7 + 1.0e-12;

void setupPangenAirfoil(SessionComposition& s, int designation) {
  std::ostringstream out;
  cli::handleNacaCore(s, out, designation);
  CHECK(s.paneling.panelingValid);
}

void configureOper(SessionComposition& s) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = 100;
  config.viscous = true;
  CHECK(session::configure(s, config).success());
}

void enterMdes(SessionComposition& s) {
  const auto entry = session::enterMdesMenuState(s);
  CHECK(entry.status == session::InverseMenuEntryStatus::entered);
  CHECK(session::initializeMdesNumerics(s).isOk());
}

std::string readWholeFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return std::string{};
  }
  std::ostringstream buffer;
  buffer << f.rdbuf();
  return buffer.str();
}

}  // namespace

// GETVOV codec: list-directed pairs, empty records, a pair across
// a record boundary, D exponent.
TEST(unit, GetvovCodecBasicAndDExponent) {
  std::istringstream in("0.0 1.25\n\n0.5D0 1.5\n0.75,\n1.0 1.05\n");
  const auto data = io::decodeGetvovFile(in);
  CHECK(!data.truncatedByParseError);
  CHECK_EQ(data.points.size(), 3u);
  CHECK_NEAR(data.points[1].s, 0.5, 0.0, 0.0);
  CHECK_NEAR(data.points[1].vov, 1.5, 0.0, 0.0);
  CHECK_NEAR(data.points[2].s, 0.75, 0.0, 0.0);
  CHECK_NEAR(data.points[2].vov, 1.0, 0.0, 0.0);
}

// GETVOV codec: a parse error aborts reading; already complete pairs
// remain in the codec record, and the handler canonically discards it
// (ERR=99 without mutation).
TEST(unit, GetvovCodecParseErrorFlag) {
  std::istringstream in("0.0 1.0\n0.5 0.0\nxx\n");
  const auto data = io::decodeGetvovFile(in);
  CHECK(data.truncatedByParseError);
  CHECK_EQ(data.points.size(), 2u);
}

// GETVOV codec: IQX boundary (370 pairs).
TEST(unit, GetvovCodecPointLimit) {
  std::ostringstream gen;
  for (int k = 0; k < 371; ++k) {
    gen << (0.5 * k) << " 1.0\n";
  }
  std::istringstream in(gen.str());
  const auto data = io::decodeGetvovFile(in);
  CHECK_EQ(data.points.size(), io::kGetvovPointLimit);
  CHECK(data.truncatedByPointLimit);
}

// CNDUMP-codec: Format 1X,I3,4F11.6 byte-exact.
TEST(unit, CnDumpCodecFormatBytes) {
  const std::string text = io::cnDumpText({io::CnDumpRow{
      0, 0.0, -0.036883, 0.681658, -0.065300}});
  CHECK(text == "   0   0.000000  -0.036883   0.681658  -0.065300\n");
}

// READ use case and after-run against the canonical CLI oracle:
// NACA 2412 / MDES / READ(vov2.dat) returns
// Qspec 1 : alpha = 0.0000 CL = -0.098496 CM = -0.011622
TEST(unit, MdesReadUseCaseMatchesFortranAnchor) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  const auto before = s.inverse.shared.qspecs[0].q;

  core::export_data::VelocityRatioTable table;
  table.s = {0.0, 0.05, 0.1, 0.3, 0.6, 0.9, 1.0};
  table.vov = {0.30, 0.0, 0.85, 1.30, 1.20, 1.05, 1.00};
  CHECK(session::mdesReadVelocityDistribution(s, table) ==
        session::MdesReadStatus::ok);
  CHECK(s.inverse.validity.qspecOrigin == session::QSpecOrigin::fileRead);

  session::MdesSpectrumReport report;
  CHECK(session::mdesReadPostProcess(s, report) ==
        session::MdesReadPostStatus::ok);
  CHECK_EQ(report.columns.size(), 1u);
  CHECK_NEAR(report.columns[0].cl, -0.098496, kF6Abs, kF6Abs);
  CHECK_NEAR(report.columns[0].cm, -0.011622, kF6Abs, kF6Abs);
  CHECK_NEAR(report.clgam, 0.255075, kF6Abs, kF6Abs);
  bool changed = false;
  for (std::size_t i = 0; i < before.size(); ++i) {
    if (before[i] != s.inverse.shared.qspecs[0].q[i]) {
      changed = true;
      break;
    }
  }
  CHECK(changed);
  // No geometry/panel/analysis mutation.
  CHECK(s.paneling.panelingValid);
}

// READ error contracts: empty table without mutation (Fortran KK<2 is
// numerically undefined); an after-run is still possible.
TEST(unit, MdesReadEmptyTableNoMutation) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  const auto before = s.inverse.shared.qspecs[0].q;
  core::export_data::VelocityRatioTable table;
  CHECK(session::mdesReadVelocityDistribution(s, table) ==
        session::MdesReadStatus::invalidTable);
  for (std::size_t i = 0; i < before.size(); ++i) {
    CHECK_NEAR(s.inverse.shared.qspecs[0].q[i], before[i], 0.0, 0.0);
  }
  session::MdesSpectrumReport report;
  CHECK(session::mdesReadPostProcess(s, report) ==
        session::MdesReadPostStatus::ok);
  CHECK_NEAR(report.columns[0].cl, 0.255075, kF6Abs, kF6Abs);
}

// DUMP projection against canonical Fortran file bytes (INIT state
// NACA 2412, 257 lines Format 1X,I3,4F11.6).
TEST(unit, MdesDumpProjectionMatchesFortranBytes) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);

  session::MdesDumpData dump;
  CHECK(session::currentMdesCnDumpData(s, dump) ==
        session::MdesDumpStatus::available);
  CHECK_EQ(dump.rows.size(), 257u);
  const std::string expected = readWholeFile(
      "tests/data/mdes-file-reference/cn_after_init.txt");
  CHECK(!expected.empty());
  CHECK(io::cnDumpText({}) .empty());
  std::vector<io::CnDumpRow> rows;
  for (const auto& r : dump.rows) {
    rows.push_back(io::CnDumpRow{r.m, r.cnReal, r.cnImag, r.piqReal,
                                 r.piqImag});
  }
  CHECK(io::cnDumpText(rows) == expected);
}

// READ -> DUMP: the projected bytes match the canonical
// Fortran dump after the same READ.
TEST(unit, MdesReadThenDumpMatchesFortranBytes) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  core::export_data::VelocityRatioTable table;
  table.s = {0.0, 0.05, 0.1, 0.3, 0.6, 0.9, 1.0};
  table.vov = {0.30, 0.0, 0.85, 1.30, 1.20, 1.05, 1.00};
  CHECK(session::mdesReadVelocityDistribution(s, table) ==
        session::MdesReadStatus::ok);
  session::MdesSpectrumReport report;
  CHECK(session::mdesReadPostProcess(s, report) ==
        session::MdesReadPostStatus::ok);

  session::MdesDumpData dump;
  CHECK(session::currentMdesCnDumpData(s, dump) ==
        session::MdesDumpStatus::available);
  std::vector<io::CnDumpRow> rows;
  for (const auto& r : dump.rows) {
    rows.push_back(io::CnDumpRow{r.m, r.cnReal, r.cnImag, r.piqReal,
                                 r.piqImag});
  }
  CHECK(io::cnDumpText(rows) ==
        readWholeFile(
            "tests/data/mdes-file-reference/cn_after_read_vov2.txt"));
}

// DUMP is a pure read-only query: a second call is identical, with no
// state change.
TEST(unit, MdesDumpIsPureQuery) {
  SessionComposition s;
  setupPangenAirfoil(s, 2412);
  configureOper(s);
  enterMdes(s);
  session::MdesDumpData a;
  session::MdesDumpData b;
  CHECK(session::currentMdesCnDumpData(s, a) ==
        session::MdesDumpStatus::available);
  CHECK(session::currentMdesCnDumpData(s, b) ==
        session::MdesDumpStatus::available);
  CHECK(a.rows.size() == b.rows.size());
  for (std::size_t i = 0; i < a.rows.size(); ++i) {
    CHECK_NEAR(a.rows[i].cnReal, b.rows[i].cnReal, 0.0, 0.0);
    CHECK_NEAR(a.rows[i].piqImag, b.rows[i].piqImag, 0.0, 0.0);
  }
}

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

// File: tests/DumpParitySupport.h
// PUB-W2: Windows-only semantic comparison for PLXADD polar dumps.
// The dump file stores raw IEEE-754 records, so platform differences
// in the kernels appear as bit deviations inside otherwise
// structurally identical files. On Linux the byte-exact comparison is
// kept unchanged; on Windows the records are decoded through the
// product's own DumpCodec and compared with the measured portable
// contracts:
//
// - structure: identical point count, identical side lengths,
//   identical integer fields (nside), header strings byte-identical;
// - header reals (version/mach/reynMillions/acrit): exact - they are
//   written from session state, not from kernels;
// - geometry pairs: kReplayValue (panel generation family, measured
//   max 1.33e-14 relative in PUB-W1);
// - alfaRad: exact (an input quantity);
// - cl/cdTot/zero/cm: absolute 1e-5. Basis: the PUB-W2 Windows batch
//   logs for the same converged polar quantities showed display
//   deviations up to 5.5e-6 (iteration-path family near stall); the
//   bound is ~1.8x the measurement;
// - xtrTop/xtrBottom: absolute 1e-5 (measured 2e-6 plus display
//   rounding);
// - iblte1/iblte2: |difference| <= 2. Basis: iteration paths that
//   differ in iteration count can quantize the transition station one
//   or two BL stations apart; larger shifts indicate a real convergence
//   deviation and must fail with a measurement;
// - side x/thet/dstr arrays: kReplayValue for the BL integrals and
//   panel coordinates (Mrchue replay family; passed unviolated in the
//   PUB-W3 measurement run);
// - cp: abs <= 5e-12 or rel <= 5e-11 (PUB-W3 run measured
//   abs 8.4e-13, rel 8.7e-13; the fourth MSYS2 run measured
//   abs 2.435e-12 / rel 1.71e-11 at point 0 side 2 cp[9]; margins
//   2.0x/2.9x - influence-kernel family; a repeated-run check is part
//   of the PUB-W4 acceptance; replaces the PUB-W2 kUlpBudget768 and
//   the too-tight PUB-W3 2e-12);
// - cf: abs <= 1e-13 or rel <= 2e-9 (measured abs 5.4e-14,
//   rel 8.5e-10; margins 1.9x/2.4x - wall-shear near zero);
// - ctau: abs <= 2e-11 or rel <= 3e-10 (measured abs 9.1e-12,
//   rel 1.5e-10; margins 2.2x/2.0x).
// The cf/ctau relative bounds are deliberately loose for near-zero
// values (ULP explodes there); the absolute bound governs normal
// magnitudes.
//
// Violations are reported with decimal values, bit patterns, ULP
// distance and absolute/relative difference (PORTABLE-NUMERIC style),
// aggregated per field so the per-point report cap cannot hide whole
// field families (PUB-W3), so a focused re-measurement run can tighten
// or keep the bounds.

#ifndef XFOIL_TESTS_DUMPPARITYSUPPORT_H
#define XFOIL_TESTS_DUMPPARITYSUPPORT_H

#include "TestSupport.h"
#include "xfoil/io/DumpCodec.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace testing {

// PUB-W3: absolute/relative bound pair for one dump field group.
struct DumpFieldGroup {
  double absolute;
  double relative;
};

// PUB-W4 diagnostic mode: with XFOIL_DUMP_DIAG=1 the report cap and
// the per-family sample cap are lifted so EVERY violating value is
// listed (used for the focused PACC cp re-measurement runs; normal
// acceptance runs keep the bounded diagnostics).
inline bool dumpDiagnosticMode() {
  const char* value = std::getenv("XFOIL_DUMP_DIAG");
  return value != nullptr && value[0] == '1';
}

// PUB-W3 measured field groups (see the file header for the measured
// maxima and margins).
inline constexpr DumpFieldGroup kDumpCpGroup{5e-12, 5e-11};
inline constexpr DumpFieldGroup kDumpCfGroup{1e-13, 2e-9};
inline constexpr DumpFieldGroup kDumpCtauGroup{2e-11, 3e-10};

inline std::string describeDumpReal(double actual, double expected,
                                    const std::string& what) {
  std::ostringstream msg;
  msg.precision(17);
  const double absolute = std::fabs(actual - expected);
  const double relative =
      expected != 0.0 ? absolute / std::fabs(expected) : absolute;
  msg << what << ": actual " << actual << " expected " << expected
      << " bits 0x" << std::hex << std::bit_cast<std::uint64_t>(actual)
      << " vs 0x" << std::bit_cast<std::uint64_t>(expected) << std::dec
      << " ulp " << portableUlpDistance(actual, expected) << " abs " << absolute
      << " rel " << relative;
  return msg.str();
}

// Returns an empty string when the dumps agree under the Windows
// contract; otherwise a diagnostic message (first violations only).
// PUB-W4: optional ALFA contract override. CSEQ polar dumps carry the
// same near-zero ALFA/DTOR quantity as the CSEQ header pins (proven by
// the fourth run: the dump violation and the pin violation printed the
// identical double 5.8369402123553517e-13), so the CSEQ test passes the
// measured ALFA family here while the PACC dumps keep the strict
// (nullptr) comparison.
inline std::string compareDumpsWindowsPortable(
    const std::vector<std::uint8_t>& actual,
    const std::vector<std::uint8_t>& expected,
    const DumpFieldGroup* alfaGroup = nullptr) {
  auto actualFile = xfoil::io::decodeDump(actual.data(), actual.size());
  auto expectedFile = xfoil::io::decodeDump(expected.data(), expected.size());
  if (!actualFile.file || !expectedFile.file) {
    return "dump decode failed (actual: " + actualFile.message +
           ", expected: " + expectedFile.message + ")";
  }
  const xfoil::io::DumpFile& a = *actualFile.file;
  const xfoil::io::DumpFile& e = *expectedFile.file;
  std::ostringstream problems;
  int reported = 0;
  const bool diagnostic = dumpDiagnosticMode();
  const std::size_t kSamplesPerFamily = diagnostic ? 1000000 : 8;
  const int kReportLimit = diagnostic ? 1000000 : 16;
  const auto note = [&](const std::string& message) {
    if (reported < kReportLimit) {
      if (reported > 0) {
        problems << "\n      ";
      }
      problems << message;
      ++reported;
    } else {
      ++reported;
    }
  };

  // PUB-W3: array fields are aggregated per field family (field name
  // and side, not per polar point) so one noisy point cannot hide the
  // violation families of the other points behind the report cap.
  struct FieldAgg {
    std::string field;
    std::vector<std::string> samples;
    int count = 0;
  };
  std::vector<FieldAgg> fieldAggs;
  const auto noteField = [&](const std::string& field,
                             const std::string& message) {
    for (auto& agg : fieldAggs) {
      if (agg.field == field) {
        ++agg.count;
        if (agg.samples.size() < kSamplesPerFamily) {
          agg.samples.push_back(message);
        }
        return;
      }
    }
    fieldAggs.push_back({field, {message}, 1});
  };

  const auto reals = [&](const std::vector<double>& va,
                    const std::vector<double>& ve, const std::string& what,
                    const std::string& field, PortableNumericClass cls) {
    for (std::size_t i = 0; i < va.size(); ++i) {
      const bool ok =
          std::isfinite(va[i]) && std::isfinite(ve[i]) &&
          withinPortableNumericContract(va[i], ve[i], cls);
      if (!ok) {
        noteField(field, describeDumpReal(va[i], ve[i], what + "[" +
                                                         std::to_string(i) +
                                                         "]"));
      }
    }
  };
  const auto realsGroup = [&](const std::vector<double>& va,
                         const std::vector<double>& ve,
                         const std::string& what, const std::string& field,
                         const DumpFieldGroup& group) {
    for (std::size_t i = 0; i < va.size(); ++i) {
      if (!withinAbsoluteRelativeContract(va[i], ve[i], group.absolute,
                                          group.relative)) {
        noteField(field, describeDumpReal(va[i], ve[i], what + "[" +
                                                         std::to_string(i) +
                                                         "]"));
      }
    }
  };

  if (a.header.name != e.header.name || a.header.code != e.header.code) {
    note("dump header strings differ: " + a.header.name + "/" +
         e.header.name);
  }
  if (a.header.version != e.header.version || a.header.mach != e.header.mach ||
      a.header.reynMillions != e.header.reynMillions ||
      a.header.acritTop != e.header.acritTop ||
      a.header.acritBottom != e.header.acritBottom) {
    note("dump header reals differ exactly (not kernel values)");
  }
  if (a.header.geometry.size() != e.header.geometry.size()) {
    note("dump geometry size differs");
  } else {
    for (std::size_t i = 0; i < a.header.geometry.size(); ++i) {
      if (!withinPortableNumericContract(a.header.geometry[i].first,
                                         e.header.geometry[i].first,
                                         PortableNumericClass::kReplayValue) ||
          !withinPortableNumericContract(a.header.geometry[i].second,
                                         e.header.geometry[i].second,
                                         PortableNumericClass::kReplayValue)) {
        noteField("geometry",
                  describeDumpReal(a.header.geometry[i].first,
                                   e.header.geometry[i].first,
                                   "geometry x[" + std::to_string(i) + "]"));
      }
    }
  }
  if (a.points.size() != e.points.size()) {
    note("dump point count differs: " + std::to_string(a.points.size()) +
         " vs " + std::to_string(e.points.size()));
    return problems.str();
  }
  static const char* const kScalarNames[] = {
      "cl", "cdTot", "zero", "cm", "xtrTop", "xtrBottom"};
  for (std::size_t p = 0; p < a.points.size(); ++p) {
    const xfoil::io::DumpPoint& pa = a.points[p];
    const xfoil::io::DumpPoint& pe = e.points[p];
    const std::string at = "point " + std::to_string(p) + " ";
    const bool alfaOk =
        alfaGroup == nullptr
            ? pa.alfaRad == pe.alfaRad
            : withinAbsoluteRelativeContract(pa.alfaRad, pe.alfaRad,
                                             alfaGroup->absolute,
                                             alfaGroup->relative);
    if (!alfaOk) {
      note(describeDumpReal(pa.alfaRad, pe.alfaRad, at + "alfaRad"));
    }
    const double scalarsA[] = {pa.cl, pa.cdTot, pa.zero, pa.cm, pa.xtrTop,
                               pa.xtrBottom};
    const double scalarsE[] = {pe.cl, pe.cdTot, pe.zero, pe.cm, pe.xtrTop,
                               pe.xtrBottom};
    for (int s = 0; s < 6; ++s) {
      if (!std::isfinite(scalarsA[s]) || !std::isfinite(scalarsE[s]) ||
          std::fabs(scalarsA[s] - scalarsE[s]) > 1e-5) {
        noteField(kScalarNames[s],
                  describeDumpReal(scalarsA[s], scalarsE[s], at + kScalarNames[s]));
      }
    }
    if (pa.nside1 != pe.nside1 || pa.nside2 != pe.nside2) {
      note(at + "nside differs (structural)");
    }
    if (std::abs(pa.iblte1 - pe.iblte1) > 2 ||
        std::abs(pa.iblte2 - pe.iblte2) > 2) {
      note(at + "iblte shift > 2: " + std::to_string(pa.iblte1) + "/" +
           std::to_string(pe.iblte1) + " " + std::to_string(pa.iblte2) + "/" +
           std::to_string(pe.iblte2));
    }
    for (int side = 0; side < 2; ++side) {
      const xfoil::io::DumpSide& sa = pa.sides[side];
      const xfoil::io::DumpSide& se = pe.sides[side];
      const std::string tag =
          at + "side " + std::to_string(side + 1) + " ";
      const auto sizeCheck = [&](const std::vector<double>& va,
                                 const std::vector<double>& ve,
                                 const std::string& field) {
        if (va.size() != ve.size()) {
          note(field + ": size " + std::to_string(va.size()) + " vs " +
               std::to_string(ve.size()) + " (structural)");
          return false;
        }
        return true;
      };
      const auto compare = [&](const std::vector<double>& va,
                               const std::vector<double>& ve,
                               const std::string& what,
                               PortableNumericClass cls) {
        const std::string field = tag + what;
        if (!sizeCheck(va, ve, field)) {
          return;
        }
        reals(va, ve, tag + what, field, cls);
      };
      const auto compareGroup = [&](const std::vector<double>& va,
                                    const std::vector<double>& ve,
                                    const std::string& what,
                                    const DumpFieldGroup& group) {
        const std::string field = tag + what;
        if (!sizeCheck(va, ve, field)) {
          return;
        }
        realsGroup(va, ve, tag + what, field, group);
      };
      compare(sa.x, se.x, "x", PortableNumericClass::kReplayValue);
      compareGroup(sa.cp, se.cp, "cp", kDumpCpGroup);
      compare(sa.thet, se.thet, "thet", PortableNumericClass::kReplayValue);
      compare(sa.dstr, se.dstr, "dstr", PortableNumericClass::kReplayValue);
      compareGroup(sa.cf, se.cf, "cf", kDumpCfGroup);
      compareGroup(sa.ctau, se.ctau, "ctau", kDumpCtauGroup);
    }
  }
  for (const auto& agg : fieldAggs) {
    for (std::size_t i = 0; i < agg.samples.size(); ++i) {
      std::string message = agg.samples[i];
      if (i + 1 == agg.samples.size() && agg.count > 1) {
        message += " (+" + std::to_string(agg.count - 1) + " more in " +
                   agg.field + ")";
      }
      note(message);
    }
  }
  if (reported > kReportLimit) {
    problems << "\n      (and " << (reported - kReportLimit)
             << " further violations)";
  }
  return problems.str();
}

}  // namespace testing

#endif  // XFOIL_TESTS_DUMPPARITYSUPPORT_H

// End: tests/DumpParitySupport.h

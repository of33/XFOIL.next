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

#include "xfoil/io/ExportFormats.h"

#include <cmath>
#include <span>

#include "xfoil/core/math/Sort.h"

#include <cmath>
#include <limits>

#include "xfoil/core/Limits.h"
#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"
#include "xfoil/core/inviscid/Compressibility.h"
#include "xfoil/io/TextFormat.h"

namespace xfoil::io {
namespace {

constexpr std::size_t sz(int v) { return static_cast<std::size_t>(v); }

constexpr std::size_t kIqx = xfoil::core::surfaceSystemCapacity;

char delimiterChar(DumpDelimiter d) {
  switch (d) {
    case DumpDelimiter::kComma: return ',';
    case DumpDelimiter::kTab: return '\t';
    default: return ' ';
  }
}

// BSTRIP: removes spaces while retaining every other character
// (tabs count as content).
std::string bstrip(const std::string& line) {
  std::string out;
  out.reserve(line.size());
  for (char c : line) {
    if (c != ' ') out.push_back(c);
  }
  return out;
}

struct BlRow {
  double s, x, y, ue, ds, th, cf, hk, hs, p, m, k;
};

BlRow computeRow(const BoundaryLayerDumpInput& in, int i, int is, int ibl, double tklam,
                 double hstinv) {
  BlRow row{};
  double ds = 0.0, th = 0.0, ts = 0.0, cf = 0.0, h = 1.0, hs = 2.0;
  if (in.lipan && in.lvisc && ibl < 1) {
    // The harness reads canonical values outside the field; the reference
    // shows DS=TH=TS=0 and CF=NaN.
    ds = 0.0;
    th = 0.0;
    ts = 0.0;
    cf = std::numeric_limits<double>::quiet_NaN();
    h = 1.0;
    hs = 1.0;
  } else if (in.lipan && in.lvisc) {
    ds = in.dstr[sz(is)][static_cast<std::size_t>(ibl)];
    th = in.thet[sz(is)][static_cast<std::size_t>(ibl)];
    ts = in.tstr[sz(is)][static_cast<std::size_t>(ibl)];
    cf = in.tau[sz(is)][static_cast<std::size_t>(ibl)] / (0.5 * in.qinf * in.qinf);
    if (th == 0.0) {
      h = 1.0;
      hs = 1.0;
    } else {
      h = ds / th;
      hs = ts / th;
    }
  }
  const double g = in.gam[static_cast<std::size_t>(i)];
  const double ratio = g / in.qinf;
  const double ue = (ratio * (1.0 - tklam)) / (1.0 - tklam * ratio * ratio);
  const double amsq = ue * ue * hstinv / (in.gamm1 * (1.0 - 0.5 * ue * ue * hstinv));
  const double hk = core::boundary_layer::hkin(h, amsq).value;
  row.s = in.s[static_cast<std::size_t>(i)];
  row.x = in.x[static_cast<std::size_t>(i)];
  row.y = in.y[static_cast<std::size_t>(i)];
  row.ue = ue;
  row.ds = ds;
  row.th = th;
  row.cf = cf;
  row.hk = hk;
  row.hs = hs;
  row.p = th * ue * ue;
  row.m = ds * ue;
  row.k = ts * ue * ue * ue;
  return row;
}

}  // namespace

std::string boundaryLayerDumpText(const BoundaryLayerDumpInput& input) {
  std::string out;
  const char delim = delimiterChar(input.delimiter);
  const bool blank = input.delimiter == DumpDelimiter::kBlank;
  if (blank) {
    out += "#    s        x        y     Ue/Vinf    Dstar     Theta "
           "     Cf       H"
           "       H*        P         m          K          tau         Di\n";
  } else {
    out += std::string("#s") + delim + "x" + delim + "y" + delim + "Ue/Vinf" + delim + "Dstar" +
           delim + "Theta" + delim + "Cf" + delim + "H\n";
  }

  const core::inviscid::CompressibilityState comp = core::inviscid::comset(input.minf, input.qinf);
  const double tklam = comp.tklam;
  const double hstinv = input.gamm1 * std::pow(input.minf / input.qinf, 2) /
                        (1.0 + 0.5 * input.gamm1 * input.minf * input.minf);

  for (int i = 0; i < input.n; ++i) {
    int is = 0;
    if (input.gam[static_cast<std::size_t>(i)] < 0.0) is = 1;
    int ibl = 0;
    if (is == 0) {
      ibl = input.iblte[0] - i;
    } else {
      ibl = input.iblte[1] + i - input.n + 1;
    }
    const BlRow row = computeRow(input, i, is, ibl, tklam, hstinv);
    if (blank) {
      out += " " + fortranF(9, 5, row.s) + fortranF(9, 5, row.x) + fortranF(9, 5, row.y) +
             fortranF(9, 5, row.ue) + fortranF(10, 6, row.ds) + fortranF(10, 6, row.th) +
             fortranF(10, 6, row.cf) + fortranF(10, 4, row.hk) + fortranF(10, 4, row.hs) +
             fortranF(9, 5, row.p) + fortranF(9, 5, row.m) + fortranF(9, 5, row.k) + "\n";
    } else {
      std::string line = " " + fortranF(9, 5, row.s) + delim + fortranF(9, 5, row.x) + delim +
                         fortranF(9, 5, row.y) + delim + fortranF(9, 5, row.ue) + delim +
                         fortranF(10, 6, row.ds) + delim + fortranF(10, 6, row.th) + delim +
                         fortranF(10, 6, row.cf) + delim + fortranF(10, 4, row.hk);
      out += bstrip(line) + "\n";
    }
  }

  if (input.lwake) {
    const int is = 1;
    for (int i = input.n; i < input.n + input.nw; ++i) {
      const int ibl = input.iblte[1] + i - input.n + 1;
      const double ui = input.uedg[sz(is)][static_cast<std::size_t>(ibl)];
      const double ratio = ui / input.qinf;
      const double ue = (ratio * (1.0 - tklam)) / (1.0 - tklam * ratio * ratio);
      const double ds = input.dstr[sz(is)][static_cast<std::size_t>(ibl)];
      const double th = input.thet[sz(is)][static_cast<std::size_t>(ibl)];
      const double h = ds / th;
      const double amsq = ue * ue * hstinv / (input.gamm1 * (1.0 - 0.5 * ue * ue * hstinv));
      const double hk = core::boundary_layer::hkin(h, amsq).value;
      if (blank) {
        out += " " + fortranF(9, 5, input.s[static_cast<std::size_t>(i)]) +
               fortranF(9, 5, input.x[static_cast<std::size_t>(i)]) +
               fortranF(9, 5, input.y[static_cast<std::size_t>(i)]) + fortranF(9, 5, ue) +
               fortranF(10, 6, ds) + fortranF(10, 6, th) + fortranF(10, 6, 0.0) +
               fortranF(10, 4, hk) + "\n";
      } else {
        std::string line = " " + fortranF(9, 5, input.s[static_cast<std::size_t>(i)]) + delim +
                           fortranF(9, 5, input.x[static_cast<std::size_t>(i)]) + delim +
                           fortranF(9, 5, input.y[static_cast<std::size_t>(i)]) + delim +
                           fortranF(9, 5, ue) + delim + fortranF(10, 6, ds) + delim +
                           fortranF(10, 6, th) + delim + fortranF(10, 6, 0.0) + delim +
                           fortranF(10, 4, hk);
        out += bstrip(line) + "\n";
      }
    }
  }
  return out;
}

namespace {

std::string validateBoundaryLayerStations(const BoundaryLayerDumpInput& input,
                                          bool requireDssr, bool requireViscous) {
  if (input.n < 0 || input.nw < 0) return "invalid panel/wake count";
  const std::size_t total = static_cast<std::size_t>(input.n) +
                            static_cast<std::size_t>(input.nw);
  if (input.x.size() < total || input.y.size() < total) {
    return "geometry arrays shorter than N+NW";
  }
  if (!std::isfinite(input.qinf) || input.qinf <= 0.0 ||
      !std::isfinite(input.minf) || input.minf < 0.0 || input.minf >= 1.0 ||
      !std::isfinite(input.reinf) || input.reinf <= 0.0 ||
      !std::isfinite(input.gamm1) || input.gamm1 <= 0.0) {
    return "invalid flow state";
  }
  for (int is = 0; is < 2; ++is) {
    const std::size_t side = static_cast<std::size_t>(is);
    const int nbl = input.nbl[side];
    const int iblte = input.iblte[side];
    if (nbl < 0 || iblte < 0) return "invalid NBL/IBLTE";
    if (requireViscous && (nbl < 2 || iblte < 2 || iblte > nbl)) {
      return "invalid viscous station range";
    }
    const std::size_t need = static_cast<std::size_t>(nbl) + 1;
    for (const auto* vector : {&input.xssi[side], &input.uedg[side],
                               &input.dstr[side], &input.thet[side],
                               &input.tstr[side], &input.tau[side],
                               &input.dis[side], &input.ctau[side]}) {
      if (vector->size() < need) return "station array shorter than NBL";
    }
    if (input.ipan[side].size() < need) return "IPAN array shorter than NBL";
    if (requireDssr && input.dssr[side].size() < need) {
      return "DSSR array shorter than NBL";
    }
    for (int ibl = 2; ibl <= nbl; ++ibl) {
      const std::size_t k = static_cast<std::size_t>(ibl);
      const int ipan = input.ipan[side][k];
      if (ipan < 1 || static_cast<std::size_t>(ipan) > input.x.size()) {
        return "IPAN outside geometry";
      }
      const auto& xs = input.xssi[side][k];
      if (!std::isfinite(xs)) return "non-finite station coordinate";
      if (requireDssr && !std::isfinite(input.dssr[side][k])) {
        return "non-finite DSSR";
      }
      if (!std::isfinite(input.x[static_cast<std::size_t>(ipan - 1)]) ||
          !std::isfinite(input.y[static_cast<std::size_t>(ipan - 1)])) {
        return "non-finite geometry";
      }
    }
  }
  return {};
}

}  // namespace

core::Result<std::string> boundaryLayerExtendedDumpTextChecked(
    const BoundaryLayerDumpInput& input,
    BoundaryLayerExtendedDumpContract contract) {
  const bool corrected =
      contract == BoundaryLayerExtendedDumpContract::Xfoil6996DensityCorrected;
  if (corrected) {
    const std::string error = validateBoundaryLayerStations(input, false, false);
    if (!error.empty()) {
      return core::Result<std::string>::fail(
          core::Error::make(core::ErrorCode::invalidArgument, error));
    }
  }

  std::string out;
  out += "#    s        x        y      Ue/Vinf    Dstar      Theta      Tstar       Cf         CD         H         H*         m          P         Pf        Pp         K/2         tau       Diss    -m du/ds    dP/ds      DV         dm/ds     N     Rtheta\n";
  out += "#    1        2        3        4          5          6          7          8         9          10        11         12         13        14        15         16          17         18        19        20        21          22      23      24   \n";

  const core::inviscid::CompressibilityState comp =
      core::inviscid::comset(input.minf, input.qinf);
  const double tklam = comp.tklam;
  const double hstinv = input.gamm1 * std::pow(input.minf / input.qinf, 2) /
                        (1.0 + 0.5 * input.gamm1 * input.minf * input.minf);
  const double hirat = 1.0 - 0.5 * hstinv * input.qinf * input.qinf;

  std::array<std::vector<double>, 2> ptau, edis;
  for (int is = 0; is < 2; ++is) {
    const std::size_t nbl = static_cast<std::size_t>(input.nbl[sz(is)]);
    ptau[sz(is)].assign(nbl + 1, 0.0);
    edis[sz(is)].assign(nbl + 1, 0.0);
    for (int ibl = 2; ibl <= input.iblte[sz(is)]; ++ibl) {
      const double dx = input.xssi[sz(is)][sz(ibl)] - input.xssi[sz(is)][sz(ibl - 1)];
      ptau[sz(is)][sz(ibl)] = ptau[sz(is)][sz(ibl - 1)] +
                      0.5 * (input.tau[sz(is)][sz(ibl)] + input.tau[sz(is)][sz(ibl - 1)]) * dx;
      edis[sz(is)][sz(ibl)] = edis[sz(is)][sz(ibl - 1)] +
                      0.5 * (input.dis[sz(is)][sz(ibl)] + input.dis[sz(is)][sz(ibl - 1)]) * dx;
    }
  }
  {
    const int ibl = input.iblte[1] + 1;
    ptau[1][sz(ibl)] = ptau[0][sz(input.iblte[0])] + ptau[1][sz(input.iblte[1])];
    edis[1][sz(ibl)] = edis[0][sz(input.iblte[0])] + edis[1][sz(input.iblte[1])];
    for (int i = input.iblte[1] + 2; i <= input.nbl[1]; ++i) {
      const double dx = input.xssi[1][sz(i)] - input.xssi[1][sz(i - 1)];
      ptau[1][sz(i)] = ptau[1][sz(i - 1)] + 0.5 * (input.tau[1][sz(i)] + input.tau[1][sz(i - 1)]) * dx;
      edis[1][sz(i)] = edis[1][sz(i - 1)] + 0.5 * (input.dis[1][sz(i)] + input.dis[1][sz(i - 1)]) * dx;
    }
  }

  for (int is = 0; is < 2; ++is) {
    for (int ibl = 2; ibl <= input.nbl[sz(is)]; ++ibl) {
      int iblm = (ibl == 2) ? ibl : ibl - 1;
      int iblp = (ibl == input.iblte[sz(is)]) ? ibl : ibl + 1;
      if (is == 1) {
        if (ibl == input.iblte[1] + 1) iblm = ibl;
        if (ibl == input.nbl[1]) iblp = ibl;
      }
      const int i = input.ipan[sz(is)][sz(ibl)] - 1;
      double xs = input.xssi[sz(is)][sz(ibl)];
      if (is == 1 && ibl > input.iblte[1]) {
        xs = xs - input.xssi[1][sz(input.iblte[1])] + input.xssi[0][sz(input.iblte[0])];
      }
      double ue, ds, th, ts, cf, cdis, ptaui, pprei, edisi, sqfac, h, hs, hk,
          duds, dmds, dk, ct, rt;
      double rh = 1.0;
      if (input.lipan && input.lvisc) {
        ue = input.uedg[sz(is)][sz(ibl)];
        ds = input.dstr[sz(is)][sz(ibl)];
        th = input.thet[sz(is)][sz(ibl)];
        ts = input.tstr[sz(is)][sz(ibl)];
        cf = input.tau[sz(is)][sz(ibl)] / (0.5 * input.qinf * input.qinf);
        cdis = input.dis[sz(is)][sz(ibl)] / std::pow(input.qinf, 3);
        if (corrected) {
          const double herat = 1.0 - 0.5 * hstinv * ue * ue;
          if (!(hirat > 0.0) || !(herat > 0.0)) {
            return core::Result<std::string>::fail(core::Error::make(
                core::ErrorCode::numericalFailure,
                "invalid compressibility state in BLDUMP2"));
          }
          rh = std::pow(herat / hirat, 1.0 / input.gamm1);
        }
        ptaui = ptau[sz(is)][sz(ibl)];
        pprei = rh * th * ue * ue - ptaui;
        edisi = edis[sz(is)][sz(ibl)];
        const double havg = 0.5 * (ds / th + 1.0);
        sqfac = std::pow(ue, havg);
        if (th == 0.0) {
          h = 1.0;
          hs = 1.0;
        } else {
          h = ds / th;
          hs = ts / th;
        }
        const double amsq = ue * ue * hstinv / (input.gamm1 * (1.0 - 0.5 * ue * ue * hstinv));
        hk = core::boundary_layer::hkin(h, amsq).value;
        duds = (input.uedg[sz(is)][sz(iblp)] - input.uedg[sz(is)][sz(iblm)]) /
               (input.xssi[sz(is)][sz(iblp)] - input.xssi[sz(is)][sz(iblm)]);
        dmds = (input.uedg[sz(is)][sz(iblp)] * input.dstr[sz(is)][sz(iblp)] -
                input.uedg[sz(is)][sz(iblm)] * input.dstr[sz(is)][sz(iblm)]) /
               (input.xssi[sz(is)][sz(iblp)] - input.xssi[sz(is)][sz(iblm)]);
        dk = hk * input.thet[sz(is)][sz(ibl)];
        ct = input.ctau[sz(is)][sz(ibl)];
        rt = rh * ue * th * input.reinf;
        (void)sqfac;
        (void)edisi;
      } else {
        if (corrected &&
            static_cast<std::size_t>(i) >= input.gam.size()) {
          return core::Result<std::string>::fail(core::Error::make(
              core::ErrorCode::invalidArgument,
              "GAM array shorter than IPAN"));
        }
        const double ratio = input.gam[static_cast<std::size_t>(i)] / input.qinf;
        ue = ratio * (1.0 - tklam);
        if (corrected) {
          const double herat = 1.0 - 0.5 * hstinv * ue * ue;
          if (!(hirat > 0.0) || !(herat > 0.0)) {
            return core::Result<std::string>::fail(core::Error::make(
                core::ErrorCode::numericalFailure,
                "invalid compressibility state in BLDUMP2"));
          }
          rh = std::pow(herat / hirat, 1.0 / input.gamm1);
        }
        ds = th = ts = cf = cdis = ptaui = pprei = edisi = duds = dmds = dk = ct = rt = 0.0;
        sqfac = 1.0;
        h = hs = hk = 1.0;
      }
      if (corrected &&
          (!std::isfinite(ue) || !std::isfinite(ds) || !std::isfinite(th) ||
           !std::isfinite(ts) || !std::isfinite(rh) || !std::isfinite(rt))) {
        return core::Result<std::string>::fail(core::Error::make(
            core::ErrorCode::numericalFailure,
            "non-finite value in BLDUMP2 output"));
      }
      out += " " + fortranF(9, 5, xs) + fortranF(9, 5, input.x[static_cast<std::size_t>(i)]) +
             fortranF(9, 5, input.y[static_cast<std::size_t>(i)]) + fortranF(9, 5, ue) +
             fortranF(11, 7, ds) + fortranF(11, 7, th) + fortranF(11, 7, ts) +
             fortranF(11, 7, cf) + fortranF(11, 7, cdis) + fortranF(10, 4, hk) +
             fortranF(10, 4, hs) + fortranF(11, 7, rh * ds * ue) +
             fortranF(11, 7, rh * th * ue * ue) + fortranF(11, 7, ptaui) +
             fortranF(11, 7, pprei) + fortranF(11, 7, 0.5 * rh * ts * ue * ue * ue) +
             fortranF(11, 7, input.tau[sz(is)][sz(ibl)]) + fortranF(11, 7, input.dis[sz(is)][sz(ibl)]) +
             fortranF(11, 7, -rh * ue * ds * duds) +
             fortranF(11, 7, input.tau[sz(is)][sz(ibl)] - rh * ue * ds * duds) +
             fortranF(11, 7, rh * th * ue * ue - 0.5 * (1.0 - ue * ue) * dk) +
             fortranF(11, 7, dmds) + fortranF(11, 7, ct) + fortranF(11, 3, rt) + "\n";
      if (is == 1 && ibl == input.iblte[1]) out += "\n";
    }
    if (is == 0) out += "\n";
  }
  return core::Result<std::string>::ok(std::move(out));
}

std::string boundaryLayerExtendedDumpText(
    const BoundaryLayerDumpInput& input,
    BoundaryLayerExtendedDumpContract contract) {
  const auto result = boundaryLayerExtendedDumpTextChecked(input, contract);
  return result.isOk() ? result.value() : std::string{};
}

core::Result<std::string> boundaryLayerDump3TextChecked(
    const BoundaryLayerDumpInput& input) {
  const bool viscous = input.lipan && input.lvisc;
  const std::string error =
      validateBoundaryLayerStations(input, viscous, viscous);
  if (!error.empty()) {
    return core::Result<std::string>::fail(
        core::Error::make(core::ErrorCode::invalidArgument, error));
  }

  std::string out;
  out += "#      x          y          s          b    "
         "      Cp       Ue/Vinf   rhoe/rhoinf    Me   "
         "      Hk         Rtheta "
         "      m            P            K      "
         "     Cf/2          CD          Ctau    "
         "    delta*       theta        theta*       delta**  \n";
  out += "#      1          2          3          4    "
         "       5          6          7          8    "
         "       9           10   "
         "      11           12           13     "
         "      14           15           16     "
         "      17           18           19           20     \n";

  constexpr double hvrat = 0.35;
  const core::inviscid::CompressibilityState comp =
      core::inviscid::comset(input.minf, input.qinf);
  const double tklam = comp.tklam;
  const double hstinv = input.gamm1 * std::pow(input.minf / input.qinf, 2) /
                        (1.0 + 0.5 * input.gamm1 * input.minf * input.minf);
  const double hirat = 1.0 - 0.5 * hstinv * input.qinf * input.qinf;

  std::array<std::vector<double>, 2> ptau, edis;
  for (int is = 0; is < 2; ++is) {
    const std::size_t nbl = static_cast<std::size_t>(input.nbl[sz(is)]);
    ptau[sz(is)].assign(nbl + 1, 0.0);
    edis[sz(is)].assign(nbl + 1, 0.0);
    for (int ibl = 2; ibl <= input.iblte[sz(is)]; ++ibl) {
      const double dx = input.xssi[sz(is)][sz(ibl)] - input.xssi[sz(is)][sz(ibl - 1)];
      ptau[sz(is)][sz(ibl)] = ptau[sz(is)][sz(ibl - 1)] +
          0.5 * (input.tau[sz(is)][sz(ibl)] + input.tau[sz(is)][sz(ibl - 1)]) * dx;
      edis[sz(is)][sz(ibl)] = edis[sz(is)][sz(ibl - 1)] +
          0.5 * (input.dis[sz(is)][sz(ibl)] + input.dis[sz(is)][sz(ibl - 1)]) * dx;
    }
  }
  {
    const int ibl = input.iblte[1] + 1;
    ptau[1][sz(ibl)] = ptau[0][sz(input.iblte[0])] + ptau[1][sz(input.iblte[1])];
    edis[1][sz(ibl)] = edis[0][sz(input.iblte[0])] + edis[1][sz(input.iblte[1])];
    for (int i = input.iblte[1] + 2; i <= input.nbl[1]; ++i) {
      const double dx = input.xssi[1][sz(i)] - input.xssi[1][sz(i - 1)];
      ptau[1][sz(i)] = ptau[1][sz(i - 1)] +
          0.5 * (input.tau[1][sz(i)] + input.tau[1][sz(i - 1)]) * dx;
      edis[1][sz(i)] = edis[1][sz(i - 1)] +
          0.5 * (input.dis[1][sz(i)] + input.dis[1][sz(i - 1)]) * dx;
    }
  }

  for (int is = 0; is < 2; ++is) {
    for (int ibl = 2; ibl <= input.nbl[sz(is)]; ++ibl) {
      const int i = input.ipan[sz(is)][sz(ibl)] - 1;
      double xs = input.xssi[sz(is)][sz(ibl)];
      if (is == 1 && ibl > input.iblte[1]) {
        xs = xs - input.xssi[1][sz(input.iblte[1])] + input.xssi[0][sz(input.iblte[0])];
      }

      double ue = 0.0;
      double ds = 0.0;
      double th = 0.0;
      double ts = 0.0;
      double dc = 0.0;
      double cf = 0.0;
      double cdis = 0.0;
      double ct = 0.0;
      double h = 1.0;
      double hk = 1.0;
      double rt = 0.0;
      double rhoe = 0.0;
      double me = 0.0;
      double amsq = 0.0;
      double herat = 0.0;

      if (viscous) {
        ue = input.uedg[sz(is)][sz(ibl)];
        ds = input.dstr[sz(is)][sz(ibl)];
        th = input.thet[sz(is)][sz(ibl)];
        ts = input.tstr[sz(is)][sz(ibl)];
        dc = input.dssr[sz(is)][sz(ibl)];
        herat = 1.0 - 0.5 * hstinv * ue * ue;
        amsq = ue * ue * hstinv / (input.gamm1 * herat);
        me = std::sqrt(amsq);
        rhoe = std::pow(herat / hirat, 1.0 / input.gamm1);
        const double amue = std::sqrt(std::pow(herat, 3)) * (1.0 + hvrat) /
                            (herat + hvrat) / input.reinf;
        cf = input.tau[sz(is)][sz(ibl)] / (0.5 * input.qinf * input.qinf);
        cdis = input.dis[sz(is)][sz(ibl)] / std::pow(input.qinf, 3);
        ct = input.ctau[sz(is)][sz(ibl)];
        if (th == 0.0) {
          h = 1.0;
        } else {
          h = ds / th;
        }
        hk = core::boundary_layer::hkin(h, amsq).value;
        rt = rhoe * ue * th / amue;
      } else {
        if (static_cast<std::size_t>(i) >= input.gam.size()) {
          return core::Result<std::string>::fail(core::Error::make(
              core::ErrorCode::invalidArgument,
              "GAM array shorter than IPAN"));
        }
        const double g = input.gam[static_cast<std::size_t>(i)] / input.qinf;
        ue = g * (1.0 - tklam) / (1.0 - tklam * g * g);
        herat = 1.0 - 0.5 * hstinv * ue * ue;
        amsq = ue * ue * hstinv / (input.gamm1 * herat);
        me = std::sqrt(amsq);
        rhoe = std::pow(herat / hirat, 1.0 / input.gamm1);
      }

      double cp = 0.0;
      if (input.minf < 0.05) {
        cp = 1.0 - ue * ue;
      } else {
        const double pe = std::pow(
            1.0 + 0.5 * input.gamm1 * input.minf * input.minf * (1.0 - ue * ue),
            (1.0 + input.gamm1) / input.gamm1);
        cp = (pe - 1.0) / (0.5 * (1.0 + input.gamm1) * input.minf * input.minf);
      }

      if (!std::isfinite(xs) || !std::isfinite(input.x[static_cast<std::size_t>(i)]) ||
          !std::isfinite(input.y[static_cast<std::size_t>(i)]) || !std::isfinite(cp) ||
          !std::isfinite(ue) || !std::isfinite(rhoe) || !std::isfinite(me) ||
          !std::isfinite(hk) || !std::isfinite(rt) || !std::isfinite(ds) ||
          !std::isfinite(th) || !std::isfinite(ts) || !std::isfinite(dc)) {
        return core::Result<std::string>::fail(core::Error::make(
            core::ErrorCode::numericalFailure,
            "non-finite value in BLDUMP3 output"));
      }

      out += " " + fortranF(11, 6, input.x[static_cast<std::size_t>(i)]) +
             fortranF(11, 6, input.y[static_cast<std::size_t>(i)]) +
             fortranF(11, 6, xs) + fortranF(11, 6, 1.0) +
             fortranF(11, 6, cp) + fortranF(11, 6, ue) +
             fortranF(11, 6, rhoe) + fortranF(11, 6, me) +
             fortranF(12, 6, hk) + fortranF(12, 3, rt) +
             fortranF(13, 8, ds * rhoe * ue) +
             fortranF(13, 8, th * rhoe * ue * ue) +
             fortranF(13, 8, 0.5 * ts * rhoe * ue * ue * ue) +
             fortranF(13, 8, 0.5 * cf) + fortranF(13, 8, cdis) +
             fortranF(13, 8, ct) + fortranF(13, 8, ds) +
             fortranF(13, 8, th) + fortranF(13, 8, ts) +
             fortranF(13, 8, dc) + "\n";
      if (is == 1 && ibl == input.iblte[1]) out += "\n";
    }
    if (is == 0) out += "\n";
  }
  return core::Result<std::string>::ok(std::move(out));
}

std::string boundaryLayerDump3Text(const BoundaryLayerDumpInput& input) {
  const auto result = boundaryLayerDump3TextChecked(input);
  return result.isOk() ? result.value() : std::string{};
}

std::string pressureDumpText(const PressureDumpInput& input) {
  std::string out;
  const char delim = delimiterChar(input.delimiter);
  const bool blank = input.delimiter == DumpDelimiter::kBlank;
  if (blank) {
    out += "#      x          Cp  \n";
  } else {
    out += std::string("#x") + delim + "Cp\n";
  }
  const double beta = std::sqrt(1.0 - input.minf * input.minf);
  const double bfac = 0.5 * input.minf * input.minf / (1.0 + beta);
  for (int i = 0; i < input.n; ++i) {
    const double cpinc = 1.0 - std::pow(input.gam[static_cast<std::size_t>(i)] / input.qinf, 2);
    const double den = beta + bfac * cpinc;
    const double cpcom = cpinc / den;
    if (blank) {
      out += " " + fortranF(11, 5, input.x[static_cast<std::size_t>(i)]) +
             fortranF(11, 5, cpcom) + "\n";
    } else {
      std::string line = " " + fortranF(11, 5, input.x[static_cast<std::size_t>(i)]) + delim +
                         fortranF(11, 5, cpcom);
      out += bstrip(line) + "\n";
    }
  }
  return out;
}

std::string circleCoefficientDumpText(const CircleCoefficientDumpInput& input) {
  std::string out;
  for (int m = 0; m <= input.mc; ++m) {
    out += " " + fortranI(3, m) + fortranF(11, 6, input.cn[sz(m)][0]) +
           fortranF(11, 6, input.cn[sz(m)][1]) + fortranF(11, 6, input.piq[sz(m)][0]) +
           fortranF(11, 6, input.piq[sz(m)][1]) + "\n";
  }
  for (int m = input.mc + 1; m <= input.nc - 1; ++m) {
    out += " " + fortranI(3, m) + fortranF(11, 6, 0.0) + fortranF(11, 6, 0.0) +
           fortranF(11, 6, input.piq[sz(m)][0]) + fortranF(11, 6, input.piq[sz(m)][1]) + "\n";
  }
  return out;
}

VelocityRatioTableDecodeResult decodeVelocityRatioTable(std::string_view text) {
  VelocityRatioTableDecodeResult result;
  std::vector<double> w1, w2;
  std::size_t lineNo = 0;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    std::size_t nl = text.find('\n', pos);
    const bool last = nl == std::string_view::npos;
    const std::string_view line = text.substr(pos, last ? text.size() - pos : nl - pos);
    ++lineNo;
    pos = last ? text.size() + 1 : nl + 1;
    if (line.find_first_not_of(" \t\r") == std::string_view::npos) {
      if (last) break;
      continue;
    }
    const FloatParseResult parsed = parseFloatList(line, 2);
    if (parsed.error || parsed.extracted < 2) {
      result.error = true;
      result.errorLine = lineNo;
      return result;
    }
    if (w1.size() >= kIqx) break;
    w1.push_back(parsed.values[0]);
    w2.push_back(parsed.values[1]);
    if (last) break;
  }
  const std::size_t kk = w1.size();
  result.count = kk;
  if (kk == 0) return result;
  const double sstart = w1[0];
  const double sspan = w1[kk - 1] - sstart;
  for (std::size_t k = 0; k < kk; ++k) {
    w1[k] = 1.0 - (w1[k] - sstart) / sspan;
  }
  std::size_t n = kk;
  core::math::sortAndRemovePairs(n, std::span<double>(w1), std::span<double>(w2));
  w1.resize(n);
  w2.resize(n);
  result.s = std::move(w1);
  result.vov = std::move(w2);
  return result;
}

}  // namespace xfoil::io

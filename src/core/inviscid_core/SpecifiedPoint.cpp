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

// File: src/core/inviscid_core/SpecifiedPoint.cpp
// SPECAL/SPECCL kernel of the new 0-based core, ported
// from fortran-src/xoper.f (SPECAL, SPECCL) — only the inviscid numerical
// semantics.
//
// Formulas and operation order exactly as in Fortran:
// - GAM overlay: GAM(I) = COSA*GAMU(I,1) + SINA*GAMU(I,2)
// GAM_A(I) = -SINA*GAMU(I,1) + COSA*GAMU(I,2)
// - SPECAL: TECALC, QISET, CLM=1.0, MRCL/COMSET, CLCALC; Newton ITCL=1..20:
// MSQ_CLM=2*MINF*MINF_CLM, DCLM=(CL-CLM)/(1-CL_MSQ*MSQ_CLM); RLX loop
// IRLX=1..12 (MATYP==1 or MINF==0 or MINF_CLM!=0 -> done, otherwise
// RLX=0.5*RLX); COMSET/CLCALC; |DCLM|<=1e-6 -> done; after 20: Diagnostics;
// final MRCL(CL)/COMSET/CLCALC/CPCALC.
// - SPECCL: MRCL(CLSPEC)/COMSET (Mach fixed); GAM overlay; CLCALC;
// Newton ITAL=1..20: DALFA=(CLSPEC-CL)/CL_ALF, ALFA+=RLX*DALFA;
// GAM overlay new; CLCALC; |DALFA|<=1e-6 -> done; after 20:
// Diagnostics; TECALC/QISET/CPCALC.

#include "xfoil/core/inviscid/SpecifiedPoint.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace xfoil::core::inviscid {

namespace {

// Fortran: GAM overlay from the alpha=0/90 base solutions.
void overlayGam(std::span<const double> gamu1, std::span<const double> gamu2,
                double alfa, std::vector<double>& gam,
                std::vector<double>& gamA) {
  const std::size_t n = gamu1.size();
  const double cosa = std::cos(alfa);
  const double sina = std::sin(alfa);
  gam.assign(n, 0.0);
  gamA.assign(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    gam[i] = cosa * gamu1[i] + sina * gamu2[i];
    gamA[i] = -sina * gamu1[i] + cosa * gamu2[i];
  }
}

  // Fortran: TECALC + QISET for the inviscid kernel.
TrailingEdgeState teAndQiset(const paneling::PanelGeometry& panel,
                             std::span<const double> qinvu1,
                             std::span<const double> qinvu2,
                             std::span<const double> gam,
                             std::span<const double> gamA, double alfa,
                             std::vector<double>& qinv,
                             std::vector<double>& qinvA) {
  const std::size_t n = panel.x.size();
  const TrailingEdgeState te =
      tecalcFull(panel.x[0], panel.y[0], panel.xp[0], panel.yp[0],
                 panel.x[n - 1], panel.y[n - 1], panel.xp[n - 1],
                 panel.yp[n - 1], panel.chord, gam[0], gam[n - 1], gamA[0],
                 gamA[n - 1]);

  // Fortran: QISET (N+NW with NW=0 -> n entries).
  qiset(qinvu1, qinvu2, alfa, qinv, qinvA);
  return te;
}

// Fortran: CPCALC on QINV -> CPI (inviscid branch, NW=0).
Result<std::vector<double>> inviscidCp(std::span<const double> qinv,
                                       double qinf, double minf) {
  return cpCalc(qinv, qinf, minf);
}

// Fortran: LVISC/LFLAP validation (xoper.f:2807-2813, 2880-2886).
void validateViscousExt(const SpecViscousExt& ext, const char* who) {
  if (!ext.lvisc) {
    return;
  }
  if (ext.qinvu1.empty() || ext.qinvu2.size() != ext.qinvu1.size() ||
      ext.qvis.size() != ext.qinvu1.size()) {
    throw std::invalid_argument(
        std::string(who) +
        ": LVISC-Branch needs equal lange wake bases and QVIS");
  }
}

}  // namespace

  // Fortran: SUBROUTINE SPECAL (inviscid kernel).
Result<SpecPointResult> specAlpha(const InfluenceSystem& system,
                                  const paneling::PanelGeometry& panel,
                                  const MachReynoldsSettings& settings,
                                  double alfa, double qinf, double xref,
                                  double yref, const SpecViscousExt& ext) {
  const std::size_t n = panel.x.size();
  if (system.n != n || system.gamu1.size() != n + 1 ||
      system.gamu2.size() != n + 1) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::specAlpha: System-/Panel-Sizes fit "
        "not");
  }
  validateViscousExt(ext, "xfoil::core::inviscid::specAlpha");

  SpecPointResult result;
  result.alfa = alfa;

  // Fortran: GAM overlay.
  std::span<const double> gamu1(system.gamu1.data(), n);
  std::span<const double> gamu2(system.gamu2.data(), n);
  overlayGam(gamu1, gamu2, alfa, result.gam, result.gamA);

  // Fortran: TECALC, QISET.
  result.te = teAndQiset(panel, system.qinvu1, system.qinvu2, result.gam,
                         result.gamA, alfa, result.qinv, result.qinvA);

  // Fortran: CLM = 1.0.
  double clm = 1.0;

  // Fortran: MRCL/COMSET/CLCALC.
  const auto mrclClm = mrcl(settings, clm);
  if (mrclClm.isErr()) {
    return Result<SpecPointResult>::fail(mrclClm.error());
  }
  double minf = mrclClm.value().minf;
  double minfClm = mrclClm.value().mCls;
  static_cast<void>(comset(minf, qinf));
  LoadResult load = clCalc(panel.x, panel.y, result.gam, result.gamA, alfa,
                           minf, qinf, xref, yref);
  double cl = load.cl;

  // Fortran: Newton ITCL=1..20. Cl_MSQ is refreshed per iteration, as
  // in the Fortran original (CLCALC updates Cl_MSQ in COMMON).
  bool converged = false;
  double dclm = 0.0;
  for (int itcl = 0; itcl < kMaxSpecIterations && !converged; ++itcl) {
    const double msqClm = 2.0 * minf * minfClm;
    dclm = (cl - clm) / (1.0 - load.clMsq * msqClm);

    const double clm1 = clm;
    double rlx = 1.0;

    // Fortran: RLX loop IRLX=1..12.
    for (int irlx = 0; irlx < 12; ++irlx) {
      clm = clm1 + rlx * dclm;

      const auto mrclNew = mrcl(settings, clm);
      if (mrclNew.isErr()) {
        return Result<SpecPointResult>::fail(mrclNew.error());
      }
      minf = mrclNew.value().minf;
      minfClm = mrclNew.value().mCls;

      if (settings.machType == 1 || minf == 0.0 || minfClm != 0.0) {
        break;  // Fortran GO TO 91
      }
      rlx = 0.5 * rlx;
    }

    // Fortran: COMSET/CLCALC.
    static_cast<void>(comset(minf, qinf));
    const LoadResult loadNew = clCalc(panel.x, panel.y, result.gam,
                                      result.gamA, alfa, minf, qinf, xref,
                                      yref);
    load = loadNew;
    cl = load.cl;
    result.cm = loadNew.cm;
    result.cdp = loadNew.cdp;

    if (std::abs(dclm) <= kSpecTolerance) {
      converged = true;
    }
  }

  if (!converged) {
    // Fortran: 'SPECAL: Minf convergence failed' (xoper.f:2799);
    // Fall through to the completion block (Label 110).
    result.converged = false;
    result.failure = SpecifiedPointFailure::specalMachConvergenceFailure;
  }

  // Fortran: final MRCL(CL)/COMSET/CLCALC/CPCALC.
  const auto mrclFinal = mrcl(settings, cl);
  if (mrclFinal.isErr()) {
    return Result<SpecPointResult>::fail(mrclFinal.error());
  }
  minf = mrclFinal.value().minf;
  result.minf = minf;
  result.reinf = mrclFinal.value().reinf;
  static_cast<void>(comset(minf, qinf));
  const LoadResult loadFinal = clCalc(panel.x, panel.y, result.gam,
                                      result.gamA, alfa, minf, qinf, xref,
                                      yref);
  result.cl = loadFinal.cl;
  result.cm = loadFinal.cm;
  result.cdp = loadFinal.cdp;
  result.clm = clm;

  // Fortran 2806-2813: final CPCALC block; the LVISC branch sets CPI/CPV
  // on N+NW (QINV wake-inclusive via QISET with the n+1+nw basis), the
  // non-LVISC branch stays on N.
  if (ext.lvisc) {
    std::vector<double> qinvFull;
    std::vector<double> qinvAFull;
    qiset(ext.qinvu1, ext.qinvu2, alfa, qinvFull, qinvAFull);
    result.qinv = std::move(qinvFull);
    result.qinvA = std::move(qinvAFull);
    auto cpv = cpCalc(ext.qvis, qinf, minf);
    auto cpi = cpCalc(result.qinv, qinf, minf);
    if (cpv.isErr() || cpi.isErr()) {
      return Result<SpecPointResult>::fail(cpv.isErr() ? cpv.error()
                                                       : cpi.error());
    }
    result.cp = std::move(cpi.value());
    result.cpv = std::move(cpv.value());
  } else {
    const auto cp = inviscidCp(result.qinv, qinf, minf);
    if (cp.isErr()) {
      return Result<SpecPointResult>::fail(cp.error());
    }
    result.cp = std::move(cp.value());
  }

  // Fortran 2813: IF(LFLAP) CALL MHINGE.
  if (ext.lflap) {
    result.hinge = mhinge(panel.x, panel.xp, panel.y, panel.yp, panel.s,
                          result.cp, result.cpv, ext.lvisc, ext.xof,
                          ext.yof);
  }

  return Result<SpecPointResult>::ok(std::move(result));
}

  // Fortran: SUBROUTINE SPECCL (inviscid kernel).
Result<SpecPointResult> specCl(const InfluenceSystem& system,
                               const paneling::PanelGeometry& panel,
                               const MachReynoldsSettings& settings,
                               double clspec, double alfa, double qinf,
                               double xref, double yref,
                               const SpecViscousExt& ext) {
  const std::size_t n = panel.x.size();
  if (system.n != n || system.gamu1.size() != n + 1 ||
      system.gamu2.size() != n + 1) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::specCl: System-/Panel-Sizes fit "
        "not");
  }
  validateViscousExt(ext, "xfoil::core::inviscid::specCl");

  SpecPointResult result;
  result.alfa = alfa;

  // Fortran: MRCL(CLSPEC)/COMSET (Mach from Cl, fixed).
  const auto mrclSpec = mrcl(settings, clspec);
  if (mrclSpec.isErr()) {
    return Result<SpecPointResult>::fail(mrclSpec.error());
  }
  const double minf = mrclSpec.value().minf;
  result.minf = minf;
  result.reinf = mrclSpec.value().reinf;
  static_cast<void>(comset(minf, qinf));

  // Fortran: GAM overlay with the start Alfa.
  std::span<const double> gamu1(system.gamu1.data(), n);
  std::span<const double> gamu2(system.gamu2.data(), n);
  overlayGam(gamu1, gamu2, alfa, result.gam, result.gamA);

  // Fortran: CLCALC.
  LoadResult load = clCalc(panel.x, panel.y, result.gam, result.gamA, alfa,
                           minf, qinf, xref, yref);
  double cl = load.cl;

  // Fortran: Newton ITAL=1..20. Cl_ALF is refreshed per iteration, as
  // in the Fortran original (CLCALC updates Cl_ALF in COMMON).
  bool converged = false;
  double dalfa = 0.0;
  double curAlfa = alfa;
  for (int ital = 0; ital < kMaxSpecIterations && !converged; ++ital) {
    dalfa = (clspec - cl) / load.clAlf;

    curAlfa += dalfa;

    // Fortran: GAM overlay new.
    overlayGam(gamu1, gamu2, curAlfa, result.gam, result.gamA);

    // Fortran: CLCALC.
    const LoadResult loadNew = clCalc(panel.x, panel.y, result.gam,
                                      result.gamA, curAlfa, minf, qinf, xref,
                                      yref);
    load = loadNew;
    cl = load.cl;
    result.cm = loadNew.cm;
    result.cdp = loadNew.cdp;

    if (std::abs(dalfa) <= kSpecTolerance) {
      converged = true;
    }
  }

  if (!converged) {
    // Fortran: 'SPECCL: CL convergence failed' (xoper.f:2874);
    // Fall through to TECALC/QISET/CPCALC (Label 110).
    result.converged = false;
    result.failure = SpecifiedPointFailure::specclLiftConvergenceFailure;
  }

  result.alfa = curAlfa;
  result.cl = cl;
  result.clm = clspec;

  // Fortran: TECALC/QISET/CPCALC (2878-2886); the LVISC branch uses the
  // n+1+nw basis for QISET (QINV wake-inclusive).
  std::span<const double> gamu1Full(system.gamu1.data(), n);
  std::span<const double> gamu2Full(system.gamu2.data(), n);
  if (ext.lvisc) {
    result.te = teAndQiset(panel, ext.qinvu1, ext.qinvu2, result.gam,
                           result.gamA, curAlfa, result.qinv, result.qinvA);
  } else {
    result.te = teAndQiset(panel, system.qinvu1, system.qinvu2, result.gam,
                           result.gamA, curAlfa, result.qinv, result.qinvA);
  }

  if (ext.lvisc) {
    auto cpv = cpCalc(ext.qvis, qinf, minf);
    auto cpi = cpCalc(result.qinv, qinf, minf);
    if (cpv.isErr() || cpi.isErr()) {
      return Result<SpecPointResult>::fail(cpv.isErr() ? cpv.error()
                                                       : cpi.error());
    }
    result.cp = std::move(cpi.value());
    result.cpv = std::move(cpv.value());
  } else {
    const auto cp = inviscidCp(result.qinv, qinf, minf);
    if (cp.isErr()) {
      return Result<SpecPointResult>::fail(cp.error());
    }
    result.cp = std::move(cp.value());
  }

  // Fortran 2886: IF(LFLAP) CALL MHINGE.
  if (ext.lflap) {
    result.hinge = mhinge(panel.x, panel.xp, panel.y, panel.yp, panel.s,
                          result.cp, result.cpv, ext.lvisc, ext.xof,
                          ext.yof);
  }

  return Result<SpecPointResult>::ok(std::move(result));
}

}  // namespace xfoil::core::inviscid

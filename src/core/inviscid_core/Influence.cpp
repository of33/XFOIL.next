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

// File: src/core/inviscid_core/Influence.cpp
// Influence-coefficient and circulation kernels of the new 0-based core,
// ported directly from fortran-src/xpanel.f (PSILIN, PSWLIN,
// GGCALC, QWCALC, QDCALC, XYWAKE).
//
// Formulas, operation order and special cases exactly as in Fortran:
// - PSILIN: main loop JO=1..N (edge handling, zero-panel skip, SGN
// correction, G1/G2/T1/T2), SIGLIN source blocks (1-0/0-2 half-panels,
// DZDM/DQDM independent of SIG), vortex contribution (DZDG/DQDG, QTAN1/QTAN2),
// TE panel (Label 11, HOPI), freestream (Z_QINF/Z_ALFA), LIMAGE
// image-point system (negated sign). GEOLIN/Z_QDOF -> inverse (not
// ported, geolin=false).
// - PSWLIN: wake-source blocks with inverted ATAN2 sign.
// - GGCALC: AIJ=DZDG, BIJ=-DZDM, Kutta line, SHARP-TE corner condition,
// LUDCMP/BAKSUB, QINVU=GAMU.
// - QWCALC: QINVU(N+1)=QINVU(N), wake PSILIN -> QTAN.
// - QDCALC: BAKSUB on BIJ columns, PSWLIN, PSILIN, CIJ/DIJ products.
// - XYWAKE: NW=N/12+10*INT(WAKLEN), SETEXP, PSILIN normals, trajectory.

#include "xfoil/core/inviscid/Influence.h"

#include "xfoil/core/paneling/PanelGeometryKernels.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "xfoil/core/math/FortranCompat.h"
#include "xfoil/core/math/LinearAlgebra.h"
#include "xfoil/core/math/Spline.h"

namespace xfoil::core::inviscid {

using math::Diagnostic;

double invPi() {
  return 4.0 * std::atan(1.0);
}

namespace {

// Fortran COMMON/CR11/: HOPI = 0.5/PI, QOPI = 0.25/PI (xfoil.f INIT).
double hopi() {
  return 0.5 / invPi();
}

double qopi() {
  return 0.25 / invPi();
}

// Fortran: SIGN(1.0, YY).
double fortranSign(double value, double reference) {
  return reference >= 0.0 ? value : -value;
}

// Fortran: LIMAGE image-point system (xpanel.f, second DO 20 block after the
// RETURN for .NOT.LIMAGE).
// Image coordinates: X + 2*(YIMAGE+Y)*SINA, Y - 2*(YIMAGE+Y)*COSA;
// image angle: APAN = PI - APANEL(JO) + 2*ALFA.
// Vortex/TE contributions with sign inverted relative to the normal path;
// SIGLIN source contributions with the same (positive) sign.
// GEOLIN/Z_QDOF0..3 parts -> inverse kernels (here only GEOLIN=false).
// The G1/T1 computation is unconditional in the Fortran image branch
// (no IO/RS guard) and is ported exactly.
void psiLinImageLoop(const PsiLinInputs& in, const PsiLinContext& ctx,
                     PsiLinResult& result) {
  const std::size_t n = ctx.x.size();
  const double seps = (ctx.s[n - 1] - ctx.s[0]) * 1.0e-5;
  const bool ioOnAirfoil = in.nodeOnAirfoil;
  const double cosa = std::cos(in.alfa);
  const double sina = std::sin(in.alfa);
  const double yimage = ctx.yimage;

  // Fortran: SHARP -> SCS/SDS.
  double scs = 0.0;
  double sds = 0.0;
  if (ctx.sharp) {
    scs = 1.0;
    sds = 0.0;
  } else {
    scs = ctx.ante / ctx.dste;
    sds = ctx.aste / ctx.dste;
  }

  for (std::size_t jo0 = 0; jo0 < n; ++jo0) {
    std::size_t jp0 = jo0 + 1;
    std::size_t jm0 = jo0 >= 1 ? jo0 - 1 : 0;
    std::size_t jq0 = jp0 + 1;

    bool skipNode = false;
    if (jo0 == 0) {
      jm0 = jo0;
    } else if (jo0 == n - 2) {
      jq0 = jp0;
    } else if (jo0 == n - 1) {
      jp0 = 0;
      if ((ctx.x[jo0] - ctx.x[jp0]) * (ctx.x[jo0] - ctx.x[jp0]) +
              (ctx.y[jo0] - ctx.y[jp0]) * (ctx.y[jo0] - ctx.y[jp0]) <
          seps * seps) {
        break;  // Fortran GO TO 22 (all Contributions skipped)
      }
    }
    if (skipNode) {
      break;
    }

    const double dso = std::sqrt((ctx.x[jo0] - ctx.x[jp0]) *
                                     (ctx.x[jo0] - ctx.x[jp0]) +
                                 (ctx.y[jo0] - ctx.y[jp0]) *
                                     (ctx.y[jo0] - ctx.y[jp0]));
    if (dso == 0.0) {
      continue;  // Fortran GO TO 20
    }
    const double dsio = 1.0 / dso;

    // Fortran: image angle and image coordinates.
    const double apan = invPi() - ctx.apanel[jo0] + 2.0 * in.alfa;
    const double xjo = ctx.x[jo0] + 2.0 * (yimage + ctx.y[jo0]) * sina;
    const double yjo = ctx.y[jo0] - 2.0 * (yimage + ctx.y[jo0]) * cosa;
    const double xjp = ctx.x[jp0] + 2.0 * (yimage + ctx.y[jp0]) * sina;
    const double yjp = ctx.y[jp0] - 2.0 * (yimage + ctx.y[jp0]) * cosa;

    const double rx1 = in.xi - xjo;
    const double ry1 = in.yi - yjo;
    const double rx2 = in.xi - xjp;
    const double ry2 = in.yi - yjp;

    const double sx = (xjp - xjo) * dsio;
    const double sy = (yjp - yjo) * dsio;

    const double x1 = sx * rx1 + sy * ry1;
    const double x2 = sx * rx2 + sy * ry2;
    const double yy = sx * ry1 - sy * rx1;

    const double rs1 = rx1 * rx1 + ry1 * ry1;
    const double rs2 = rx2 * rx2 + ry2 * ry2;

    // Fortran: SGN correction.
    const double sgn = ioOnAirfoil ? 1.0 : fortranSign(1.0, yy);

    // Fortran: log(r^2) and arctan — unconditional in the image branch.
    const double g1 = std::log(rs1);
    const double t1 = std::atan2(sgn * x1, sgn * yy) + (0.5 - 0.5 * sgn) * invPi();
    const double g2 = std::log(rs2);
    const double t2 = std::atan2(sgn * x2, sgn * yy) + (0.5 - 0.5 * sgn) * invPi();

    const double x1i = sx * in.nxi + sy * in.nyi;
    const double x2i = sx * in.nxi + sy * in.nyi;
    const double yyi = sx * in.nyi - sy * in.nxi;

    // Fortran: IF(JO.EQ.N) GO TO 21 (TE panel).
    if (jo0 == n - 1) {
      const double psig =
          0.5 * yy * (g1 - g2) + x2 * (t2 - apan) - x1 * (t1 - apan);
      const double pgam =
          0.5 * x1 * g1 - 0.5 * x2 * g2 + x2 - x1 + yy * (t1 - t2);
      const double psigNi = -(t1 - apan) * x1i + (t2 - apan) * x2i +
                            0.5 * (g1 - g2) * yyi;
      const double pgamNi =
          0.5 * g1 * x1i - 0.5 * g2 * x2i + yyi * (t1 - t2);
      const double sigte = 0.5 * scs * (ctx.gam[jp0] - ctx.gam[jo0]);
      const double gamte = -0.5 * sds * (ctx.gam[jp0] - ctx.gam[jo0]);
      const double sigte1 = 0.5 * scs * (ctx.gamu1[jp0] - ctx.gamu1[jo0]);
      const double sigte2 = 0.5 * scs * (ctx.gamu2[jp0] - ctx.gamu2[jo0]);
      const double gamte1 = -0.5 * sds * (ctx.gamu1[jp0] - ctx.gamu1[jo0]);
      const double gamte2 = -0.5 * sds * (ctx.gamu2[jp0] - ctx.gamu2[jo0]);

      result.psi += hopi() * (psig * sigte - pgam * gamte);
      result.dzdg[jo0] += -hopi() * psig * scs * 0.5;
      result.dzdg[jp0] += hopi() * psig * scs * 0.5;
      result.dzdg[jo0] += -hopi() * pgam * sds * 0.5;
      result.dzdg[jp0] += hopi() * pgam * sds * 0.5;
      result.psiNi += hopi() * (psigNi * sigte - pgamNi * gamte);
      result.qtan1 += hopi() * (psigNi * sigte1 - pgamNi * gamte1);
      result.qtan2 += hopi() * (psigNi * sigte2 - pgamNi * gamte2);
      result.dqdg[jo0] +=
          -hopi() * (psigNi * 0.5 * scs + pgamNi * 0.5 * sds);
      result.dqdg[jp0] += hopi() * (psigNi * 0.5 * scs + pgamNi * 0.5 * sds);
      break;
    }

    // Fortran: SIGLIN blocks (1-0/0-2 Half panels, same Sign).
    if (in.siglin) {
      const double x0 = 0.5 * (x1 + x2);
      const double rs0 = x0 * x0 + yy * yy;
      const double g0 = std::log(rs0);
      const double t0 =
          std::atan2(sgn * x0, sgn * yy) + (0.5 - 0.5 * sgn) * invPi();

      const double dxinv = 1.0 / (x1 - x0);
      const double psum = x0 * (t0 - apan) - x1 * (t1 - apan) +
                          0.5 * yy * (g1 - g0);
      const double pdif =
          ((x1 + x0) * psum + rs1 * (t1 - apan) - rs0 * (t0 - apan) +
           (x0 - x1) * yy) * dxinv;

      const double psx1 = -(t1 - apan);
      const double psx0 = t0 - apan;
      const double psyy = 0.5 * (g1 - g0);

      const double pdx1 = ((x1 + x0) * psx1 + psum + 2.0 * x1 * (t1 - apan) -
                           pdif) * dxinv;
      const double pdx0 = ((x1 + x0) * psx0 + psum - 2.0 * x0 * (t0 - apan) +
                           pdif) * dxinv;
      const double pdyy = ((x1 + x0) * psyy +
                           2.0 * (x0 - x1 + yy * (t1 - t0))) * dxinv;

      const double dsm = std::sqrt((ctx.x[jp0] - ctx.x[jm0]) *
                                       (ctx.x[jp0] - ctx.x[jm0]) +
                                   (ctx.y[jp0] - ctx.y[jm0]) *
                                       (ctx.y[jp0] - ctx.y[jm0]));
      const double dsim = 1.0 / dsm;

      const double ssum = (ctx.sig[jp0] - ctx.sig[jo0]) * dsio +
                          (ctx.sig[jp0] - ctx.sig[jm0]) * dsim;
      const double sdif = (ctx.sig[jp0] - ctx.sig[jo0]) * dsio -
                          (ctx.sig[jp0] - ctx.sig[jm0]) * dsim;

      result.psi += qopi() * (psum * ssum + pdif * sdif);

      result.dzdm[jm0] += qopi() * (-psum * dsim + pdif * dsim);
      result.dzdm[jo0] += qopi() * (-psum * dsio - pdif * dsio);
      result.dzdm[jp0] +=
          qopi() * (psum * (dsio + dsim) + pdif * (dsio - dsim));

      const double psni =
          psx1 * x1i + psx0 * (x1i + x2i) * 0.5 + psyy * yyi;
      const double pdni =
          pdx1 * x1i + pdx0 * (x1i + x2i) * 0.5 + pdyy * yyi;
      result.psiNi += qopi() * (psni * ssum + pdni * sdif);
      static_cast<void>(psni * ssum + pdni * sdif);  // QTANM -> Context

      result.dqdm[jm0] += qopi() * (-psni * dsim + pdni * dsim);
      result.dqdm[jo0] += qopi() * (-psni * dsio - pdni * dsio);
      result.dqdm[jp0] +=
          qopi() * (psni * (dsio + dsim) + pdni * (dsio - dsim));

      // 0-2 Half panel
      const double dxinv2 = 1.0 / (x0 - x2);
      const double psum2 = x2 * (t2 - apan) - x0 * (t0 - apan) +
                           0.5 * yy * (g0 - g2);
      const double pdif2 =
          ((x0 + x2) * psum2 + rs0 * (t0 - apan) - rs2 * (t2 - apan) +
           (x2 - x0) * yy) * dxinv2;

      const double psx0b = -(t0 - apan);
      const double psx2 = t2 - apan;
      const double psyyb = 0.5 * (g0 - g2);

      const double pdx0b = ((x0 + x2) * psx0b + psum2 +
                            2.0 * x0 * (t0 - apan) - pdif2) * dxinv2;
      const double pdx2 = ((x0 + x2) * psx2 + psum2 - 2.0 * x2 * (t2 - apan) +
                           pdif2) * dxinv2;
      const double pdyyb = ((x0 + x2) * psyyb +
                            2.0 * (x2 - x0 + yy * (t0 - t2))) * dxinv2;

      const double dsp = std::sqrt((ctx.x[jq0] - ctx.x[jo0]) *
                                       (ctx.x[jq0] - ctx.x[jo0]) +
                                   (ctx.y[jq0] - ctx.y[jo0]) *
                                       (ctx.y[jq0] - ctx.y[jo0]));
      const double dsip = 1.0 / dsp;

      const double ssum2 = (ctx.sig[jq0] - ctx.sig[jo0]) * dsip +
                           (ctx.sig[jp0] - ctx.sig[jo0]) * dsio;
      const double sdif2 = (ctx.sig[jq0] - ctx.sig[jo0]) * dsip -
                           (ctx.sig[jp0] - ctx.sig[jo0]) * dsio;

      result.psi += qopi() * (psum2 * ssum2 + pdif2 * sdif2);

      result.dzdm[jo0] +=
          qopi() * (-psum2 * (dsip + dsio) - pdif2 * (dsip - dsio));
      result.dzdm[jp0] += qopi() * (psum2 * dsio - pdif2 * dsio);
      result.dzdm[jq0] += qopi() * (psum2 * dsip + pdif2 * dsip);

      const double psni2 =
          psx0b * (x1i + x2i) * 0.5 + psx2 * x2i + psyyb * yyi;
      const double pdni2 =
          pdx0b * (x1i + x2i) * 0.5 + pdx2 * x2i + pdyyb * yyi;
      result.psiNi += qopi() * (psni2 * ssum2 + pdni2 * sdif2);
      static_cast<void>(psni2 * ssum2 + pdni2 * sdif2);  // QTANM -> Context

      result.dqdm[jo0] +=
          qopi() * (-psni2 * (dsip + dsio) - pdni2 * (dsip - dsio));
      result.dqdm[jp0] += qopi() * (psni2 * dsio - pdni2 * dsio);
      result.dqdm[jq0] += qopi() * (psni2 * dsip + pdni2 * dsip);
    }

    // Fortran: vortex-panel contribution with inverted sign.
    {
      const double dxinv = 1.0 / (x1 - x2);
      const double psis = 0.5 * x1 * g1 - 0.5 * x2 * g2 + x2 - x1 +
                          yy * (t1 - t2);
      const double psid = ((x1 + x2) * psis +
                           0.5 * (rs2 * g2 - rs1 * g1 + x1 * x1 - x2 * x2)) *
                          dxinv;

      const double psx1 = 0.5 * g1;
      const double psx2 = -0.5 * g2;
      const double psyy = t1 - t2;

      const double pdx1 = ((x1 + x2) * psx1 + psis - x1 * g1 - psid) * dxinv;
      const double pdx2 = ((x1 + x2) * psx2 + psis + x2 * g2 + psid) * dxinv;
      const double pdyy = ((x1 + x2) * psyy - yy * (g1 - g2)) * dxinv;

      const double gsum1 = ctx.gamu1[jp0] + ctx.gamu1[jo0];
      const double gsum2 = ctx.gamu2[jp0] + ctx.gamu2[jo0];
      const double gdif1 = ctx.gamu1[jp0] - ctx.gamu1[jo0];
      const double gdif2 = ctx.gamu2[jp0] - ctx.gamu2[jo0];

      const double gsum = ctx.gam[jp0] + ctx.gam[jo0];
      const double gdif = ctx.gam[jp0] - ctx.gam[jo0];

      result.psi -= qopi() * (psis * gsum + psid * gdif);

      result.dzdg[jo0] -= qopi() * (psis - psid);
      result.dzdg[jp0] -= qopi() * (psis + psid);

      const double psni = psx1 * x1i + psx2 * x2i + psyy * yyi;
      const double pdni = pdx1 * x1i + pdx2 * x2i + pdyy * yyi;
      result.psiNi -= qopi() * (gsum * psni + gdif * pdni);

      result.qtan1 -= qopi() * (gsum1 * psni + gdif1 * pdni);
      result.qtan2 -= qopi() * (gsum2 * psni + gdif2 * pdni);

      result.dqdg[jo0] -= qopi() * (psni - pdni);
      result.dqdg[jp0] -= qopi() * (psni + pdni);
    }
  }
}

}  // namespace

// Fortran: SUBROUTINE PSILIN.
PsiLinResult psiLin(const PsiLinInputs& in, const PsiLinContext& ctx) {
  const std::size_t n = ctx.x.size();
  // GAMU/GAM_A are only needed in the LIMAGE branch (not implemented
  // here) and may be empty; SIG is only used by SIGLIN (Fortran accesses it
  // only in this branch).
  if (ctx.y.size() != n || ctx.s.size() != n || ctx.nx.size() != n ||
      ctx.ny.size() != n || ctx.apanel.size() != n || ctx.gam.size() != n ||
      ctx.gamA.size() != n) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::psiLin: all panel lengths must be equal");
  }
  // SIG is needed only for SIGLIN (Fortran accesses it only in this branch).
  if (in.siglin && ctx.sig.size() != n) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::psiLin: SIG length must correspond to n");
  }
  if (in.geolin && (in.qf0.size() != n || in.qf1.size() != n ||
                    in.qf2.size() != n || in.qf3.size() != n)) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::psiLin: QF length must correspond to n");
  }
  if (n < 1) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::psiLin: at least 1 Node required");
  }

  PsiLinResult result;
  result.dzdg.assign(n, 0.0);
  result.dqdg.assign(n, 0.0);
  result.dzdm.assign(n, 0.0);
  result.dqdm.assign(n, 0.0);
  result.dzdn.assign(n, 0.0);

  // Fortran: SEPS, IO, COSA/SINA.
  const double seps = (ctx.s[n - 1] - ctx.s[0]) * 1.0e-5;
  const bool ioOnAirfoil = in.nodeOnAirfoil;
  const double cosa = std::cos(in.alfa);
  const double sina = std::sin(in.alfa);

  // Fortran: SHARP -> SCS/SDS.
  double scs = 0.0;
  double sds = 0.0;
  if (ctx.sharp) {
    scs = 1.0;
    sds = 0.0;
  } else {
    scs = ctx.ante / ctx.dste;
    sds = ctx.aste / ctx.dste;
  }

  double psi = 0.0;
  double psiNi = 0.0;
  double qtan1 = 0.0;
  double qtan2 = 0.0;
  double zQinf = 0.0;
  double zAlfa = 0.0;
  double zQdof0 = 0.0;
  double zQdof1 = 0.0;
  double zQdof2 = 0.0;
  double zQdof3 = 0.0;

  // TE panel intermediate values (Label 11/12).
  double tePsig = 0.0;
  double tePgam = 0.0;
  double tePsigNi = 0.0;
  double tePgamNi = 0.0;
  double sigte = 0.0;
  double gamte = 0.0;
  double sigte1 = 0.0;
  double sigte2 = 0.0;
  double gamte1 = 0.0;
  double gamte2 = 0.0;
  // TE-panel derivatives and GEOLIN intermediate values (label 11).
  double tePsigx1 = 0.0, tePsigx2 = 0.0, tePsigyy = 0.0;
  double tePgamx1 = 0.0, tePgamx2 = 0.0, tePgamyy = 0.0;
  double teX1o = 0.0, teX1p = 0.0, teX2o = 0.0, teX2p = 0.0, teYyo = 0.0,
         teYyp = 0.0;

  // Fortran: main loop JO=1..N (0-based jo0 = JO-1).
  for (std::size_t jo0 = 0; jo0 < n; ++jo0) {
    std::size_t jp0 = jo0 + 1;
    std::size_t jm0 = jo0 >= 1 ? jo0 - 1 : 0;
    std::size_t jq0 = jp0 + 1;

    // Fortran: Edge handling.
    bool skipTeAndFreestream = false;
    if (jo0 == 0) {
      jm0 = jo0;
    } else if (jo0 == n - 2) {
      jq0 = jp0;
    } else if (jo0 == n - 1) {
      jp0 = 0;
      if ((ctx.x[jo0] - ctx.x[jp0]) * (ctx.x[jo0] - ctx.x[jp0]) +
              (ctx.y[jo0] - ctx.y[jp0]) * (ctx.y[jo0] - ctx.y[jp0]) <
          seps * seps) {
        skipTeAndFreestream = true;  // Fortran GO TO 12
      }
    }
    if (skipTeAndFreestream) {
      break;
    }

    const double dso = std::sqrt((ctx.x[jo0] - ctx.x[jp0]) *
                                     (ctx.x[jo0] - ctx.x[jp0]) +
                                 (ctx.y[jo0] - ctx.y[jp0]) *
                                     (ctx.y[jo0] - ctx.y[jp0]));
    if (dso == 0.0) {
      continue;  // Fortran GO TO 10
    }
    const double dsio = 1.0 / dso;

    const double apan = ctx.apanel[jo0];

    const double rx1 = in.xi - ctx.x[jo0];
    const double ry1 = in.yi - ctx.y[jo0];
    const double rx2 = in.xi - ctx.x[jp0];
    const double ry2 = in.yi - ctx.y[jp0];

    const double sx = (ctx.x[jp0] - ctx.x[jo0]) * dsio;
    const double sy = (ctx.y[jp0] - ctx.y[jo0]) * dsio;

    const double x1 = sx * rx1 + sy * ry1;
    const double x2 = sx * rx2 + sy * ry2;
    const double yy = sx * ry1 - sy * rx1;

    const double rs1 = rx1 * rx1 + ry1 * ry1;
    const double rs2 = rx2 * rx2 + ry2 * ry2;

    // Fortran: SGN correction.
    const double sgn = ioOnAirfoil ? 1.0 : fortranSign(1.0, yy);

    // Fortran: log(r^2) and arctan(x/y).
    // Fortran IO.NE.JO / IO.NE.JP: IO lies in 1..N on the contour; a
    // point outside (IO=0 or IO>N) never matches. in.node is 0-based.
    const bool nodeMatchesJo = ioOnAirfoil && (in.node == jo0);
    const bool nodeMatchesJp = ioOnAirfoil && (in.node == jp0);
    double g1 = 0.0;
    double t1 = 0.0;
    if (!nodeMatchesJo && rs1 > 0.0) {
      g1 = std::log(rs1);
      t1 = std::atan2(sgn * x1, sgn * yy) + (0.5 - 0.5 * sgn) * invPi();
    }
    double g2 = 0.0;
    double t2 = 0.0;
    if (!nodeMatchesJp && rs2 > 0.0) {
      g2 = std::log(rs2);
      t2 = std::atan2(sgn * x2, sgn * yy) + (0.5 - 0.5 * sgn) * invPi();
    }

    const double x1i = sx * in.nxi + sy * in.nyi;
    const double x2i = sx * in.nxi + sy * in.nyi;
    const double yyi = sx * in.nyi - sy * in.nxi;

    // Fortran: GEOLIN intermediate values (dPsi/dn), before the TE branch.
    double x1o = 0.0, x1p = 0.0, x2o = 0.0, x2p = 0.0, yyo = 0.0, yyp = 0.0;
    if (in.geolin) {
      const double nxo = ctx.nx[jo0];
      const double nyo = ctx.ny[jo0];
      const double nxp = ctx.nx[jp0];
      const double nyp = ctx.ny[jp0];
      x1o = -((rx1 - x1 * sx) * nxo + (ry1 - x1 * sy) * nyo) * dsio -
            (sx * nxo + sy * nyo);
      x1p = ((rx1 - x1 * sx) * nxp + (ry1 - x1 * sy) * nyp) * dsio;
      x2o = -((rx2 - x2 * sx) * nxo + (ry2 - x2 * sy) * nyo) * dsio;
      x2p = ((rx2 - x2 * sx) * nxp + (ry2 - x2 * sy) * nyp) * dsio -
            (sx * nxp + sy * nyp);
      yyo = ((rx1 + x1 * sy) * nyo - (ry1 - x1 * sx) * nxo) * dsio -
            (sx * nyo - sy * nxo);
      yyp = -((rx1 - x1 * sy) * nyp - (ry1 + x1 * sx) * nxp) * dsio;
    }

    // Fortran: IF(JO.EQ.N) GO TO 11 (TE panel).
    if (jo0 == n - 1) {

      tePsig = 0.5 * yy * (g1 - g2) + x2 * (t2 - apan) - x1 * (t1 - apan);
      tePgam = 0.5 * x1 * g1 - 0.5 * x2 * g2 + x2 - x1 + yy * (t1 - t2);
      tePsigNi = -(t1 - apan) * x1i + (t2 - apan) * x2i + 0.5 * (g1 - g2) * yyi;
      tePgamNi = 0.5 * g1 * x1i - 0.5 * g2 * x2i + (x2 - x1) * 0.0 +
                 yyi * (t1 - t2);
      tePsigx1 = -(t1 - apan);
      tePsigx2 = (t2 - apan);
      tePsigyy = 0.5 * (g1 - g2);
      tePgamx1 = 0.5 * g1;
      tePgamx2 = -0.5 * g2;
      tePgamyy = t1 - t2;
      teX1o = x1o; teX1p = x1p; teX2o = x2o; teX2p = x2p;
      teYyo = yyo; teYyp = yyp;
      // Fortran: TE panel source and vortex strengths.
      sigte = 0.5 * scs * (ctx.gam[jp0] - ctx.gam[jo0]);
      gamte = -0.5 * sds * (ctx.gam[jp0] - ctx.gam[jo0]);
      sigte1 = 0.5 * scs * (ctx.gamu1[jp0] - ctx.gamu1[jo0]);
      sigte2 = 0.5 * scs * (ctx.gamu2[jp0] - ctx.gamu2[jo0]);
      gamte1 = -0.5 * sds * (ctx.gamu1[jp0] - ctx.gamu1[jo0]);
      gamte2 = -0.5 * sds * (ctx.gamu2[jp0] - ctx.gamu2[jo0]);
      break;  // Fortran GO TO 11
    }

    // Fortran: SIGLIN blocks (1-0/0-2 Half panels).
    if (in.siglin) {
      // 1-0 Half panel
      const double x0 = 0.5 * (x1 + x2);
      const double rs0 = x0 * x0 + yy * yy;
      const double g0 = std::log(rs0);
      const double t0 =
          std::atan2(sgn * x0, sgn * yy) + (0.5 - 0.5 * sgn) * invPi();

      const double dxinv = 1.0 / (x1 - x0);
      const double psum = x0 * (t0 - apan) - x1 * (t1 - apan) +
                          0.5 * yy * (g1 - g0);
      const double pdif =
          ((x1 + x0) * psum + rs1 * (t1 - apan) - rs0 * (t0 - apan) +
           (x0 - x1) * yy) * dxinv;

      const double psx1 = -(t1 - apan);
      const double psx0 = t0 - apan;
      const double psyy = 0.5 * (g1 - g0);

      const double pdx1 = ((x1 + x0) * psx1 + psum + 2.0 * x1 * (t1 - apan) -
                           pdif) * dxinv;
      const double pdx0 = ((x1 + x0) * psx0 + psum - 2.0 * x0 * (t0 - apan) +
                           pdif) * dxinv;
      const double pdyy = ((x1 + x0) * psyy +
                           2.0 * (x0 - x1 + yy * (t1 - t0))) * dxinv;

      const double dsm = std::sqrt((ctx.x[jp0] - ctx.x[jm0]) *
                                       (ctx.x[jp0] - ctx.x[jm0]) +
                                   (ctx.y[jp0] - ctx.y[jm0]) *
                                       (ctx.y[jp0] - ctx.y[jm0]));
      const double dsim = 1.0 / dsm;

      const double ssum = (ctx.sig[jp0] - ctx.sig[jo0]) * dsio +
                          (ctx.sig[jp0] - ctx.sig[jm0]) * dsim;
      const double sdif = (ctx.sig[jp0] - ctx.sig[jo0]) * dsio -
                          (ctx.sig[jp0] - ctx.sig[jm0]) * dsim;

      psi += qopi() * (psum * ssum + pdif * sdif);

      result.dzdm[jm0] += qopi() * (-psum * dsim + pdif * dsim);
      result.dzdm[jo0] += qopi() * (-psum * dsio - pdif * dsio);
      result.dzdm[jp0] += qopi() * (psum * (dsio + dsim) + pdif * (dsio - dsim));

      const double psni = psx1 * x1i + psx0 * (x1i + x2i) * 0.5 + psyy * yyi;
      const double pdni = pdx1 * x1i + pdx0 * (x1i + x2i) * 0.5 + pdyy * yyi;
      psiNi += qopi() * (psni * ssum + pdni * sdif);
      qtan1 += qopi() * (psni * ssum + pdni * sdif) * 0.0;  // QTANM share
      result.dqdm[jm0] += qopi() * (-psni * dsim + pdni * dsim);
      result.dqdm[jo0] += qopi() * (-psni * dsio - pdni * dsio);
      result.dqdm[jp0] += qopi() * (psni * (dsio + dsim) + pdni * (dsio - dsim));

      // 0-2 Half panel
      const double dxinv2 = 1.0 / (x0 - x2);
      const double psum2 = x2 * (t2 - apan) - x0 * (t0 - apan) +
                           0.5 * yy * (g0 - g2);
      const double pdif2 =
          ((x0 + x2) * psum2 + rs0 * (t0 - apan) - rs2 * (t2 - apan) +
           (x2 - x0) * yy) * dxinv2;

      const double psx0b = -(t0 - apan);
      const double psx2 = t2 - apan;
      const double psyyb = 0.5 * (g0 - g2);

      const double pdx0b = ((x0 + x2) * psx0b + psum2 + 2.0 * x0 * (t0 - apan) -
                            pdif2) * dxinv2;
      const double pdx2 = ((x0 + x2) * psx2 + psum2 - 2.0 * x2 * (t2 - apan) +
                           pdif2) * dxinv2;
      const double pdyyb = ((x0 + x2) * psyyb +
                            2.0 * (x2 - x0 + yy * (t0 - t2))) * dxinv2;

      const double dsp = std::sqrt((ctx.x[jq0] - ctx.x[jo0]) *
                                       (ctx.x[jq0] - ctx.x[jo0]) +
                                   (ctx.y[jq0] - ctx.y[jo0]) *
                                       (ctx.y[jq0] - ctx.y[jo0]));
      const double dsip = 1.0 / dsp;

      const double ssum2 = (ctx.sig[jq0] - ctx.sig[jo0]) * dsip +
                           (ctx.sig[jp0] - ctx.sig[jo0]) * dsio;
      const double sdif2 = (ctx.sig[jq0] - ctx.sig[jo0]) * dsip -
                           (ctx.sig[jp0] - ctx.sig[jo0]) * dsio;

      psi += qopi() * (psum2 * ssum2 + pdif2 * sdif2);

      result.dzdm[jo0] += qopi() * (-psum2 * (dsip + dsio) - pdif2 * (dsip - dsio));
      result.dzdm[jp0] += qopi() * (psum2 * dsio - pdif2 * dsio);
      result.dzdm[jq0] += qopi() * (psum2 * dsip + pdif2 * dsip);

      const double psni2 = psx0b * (x1i + x2i) * 0.5 + psx2 * x2i + psyyb * yyi;
      const double pdni2 =
          pdx0b * (x1i + x2i) * 0.5 + pdx2 * x2i + pdyyb * yyi;
      psiNi += qopi() * (psni2 * ssum2 + pdni2 * sdif2);
      result.dqdm[jo0] += qopi() * (-psni2 * (dsip + dsio) - pdni2 * (dsip - dsio));
      result.dqdm[jp0] += qopi() * (psni2 * dsio - pdni2 * dsio);
      result.dqdm[jq0] += qopi() * (psni2 * dsip + pdni2 * dsip);
    }

    // Fortran: Vortex panel contribution.
    {
      const double dxinv = 1.0 / (x1 - x2);
      const double psis = 0.5 * x1 * g1 - 0.5 * x2 * g2 + x2 - x1 +
                          yy * (t1 - t2);
      const double psid = ((x1 + x2) * psis +
                           0.5 * (rs2 * g2 - rs1 * g1 + x1 * x1 - x2 * x2)) *
                          dxinv;

      const double psx1 = 0.5 * g1;
      const double psx2 = -0.5 * g2;
      const double psyy = t1 - t2;

      const double pdx1 = ((x1 + x2) * psx1 + psis - x1 * g1 - psid) * dxinv;
      const double pdx2 = ((x1 + x2) * psx2 + psis + x2 * g2 + psid) * dxinv;
      const double pdyy = ((x1 + x2) * psyy - yy * (g1 - g2)) * dxinv;

      const double gsum1 = ctx.gamu1[jp0] + ctx.gamu1[jo0];
      const double gsum2 = ctx.gamu2[jp0] + ctx.gamu2[jo0];
      const double gdif1 = ctx.gamu1[jp0] - ctx.gamu1[jo0];
      const double gdif2 = ctx.gamu2[jp0] - ctx.gamu2[jo0];

      const double gsum = ctx.gam[jp0] + ctx.gam[jo0];
      const double gdif = ctx.gam[jp0] - ctx.gam[jo0];

      psi += qopi() * (psis * gsum + psid * gdif);

      result.dzdg[jo0] += qopi() * (psis - psid);
      result.dzdg[jp0] += qopi() * (psis + psid);

      const double psni = psx1 * x1i + psx2 * x2i + psyy * yyi;
      const double pdni = pdx1 * x1i + pdx2 * x2i + pdyy * yyi;
      psiNi += qopi() * (gsum * psni + gdif * pdni);

      qtan1 += qopi() * (gsum1 * psni + gdif1 * pdni);
      qtan2 += qopi() * (gsum2 * psni + gdif2 * pdni);

      result.dqdg[jo0] += qopi() * (psni - pdni);
      result.dqdg[jp0] += qopi() * (psni + pdni);

      if (in.geolin) {
        result.dzdn[jo0] +=
            qopi() * gsum * (psx1 * x1o + psx2 * x2o + psyy * yyo) +
            qopi() * gdif * (pdx1 * x1o + pdx2 * x2o + pdyy * yyo);
        result.dzdn[jp0] +=
            qopi() * gsum * (psx1 * x1p + psx2 * x2p + psyy * yyp) +
            qopi() * gdif * (pdx1 * x1p + pdx2 * x2p + pdyy * yyp);
        zQdof0 += qopi() * ((psis - psid) * in.qf0[jo0] +
                            (psis + psid) * in.qf0[jp0]);
        zQdof1 += qopi() * ((psis - psid) * in.qf1[jo0] +
                            (psis + psid) * in.qf1[jp0]);
        zQdof2 += qopi() * ((psis - psid) * in.qf2[jo0] +
                            (psis + psid) * in.qf2[jp0]);
        zQdof3 += qopi() * ((psis - psid) * in.qf3[jo0] +
                            (psis + psid) * in.qf3[jp0]);
      }
    }
  }

  // Fortran: Label 11 — TE-panel contributions.
  {
    psi += hopi() * (tePsig * sigte + tePgam * gamte);
    // Fortran: DZDG(JO=N-1) -=, DZDG(JP=0) += (TE panel JP=1 wrap-around).
    result.dzdg[n - 1] += -hopi() * tePsig * scs * 0.5;
    result.dzdg[0] += hopi() * tePsig * scs * 0.5;
    result.dzdg[n - 1] += hopi() * tePgam * sds * 0.5;
    result.dzdg[0] += -hopi() * tePgam * sds * 0.5;
    psiNi += hopi() * (tePsigNi * sigte + tePgamNi * gamte);
    qtan1 += hopi() * (tePsigNi * sigte1 + tePgamNi * gamte1);
    qtan2 += hopi() * (tePsigNi * sigte2 + tePgamNi * gamte2);
    result.dqdg[n - 1] +=
        -hopi() * (tePsigNi * 0.5 * scs - tePgamNi * 0.5 * sds);
    result.dqdg[0] += hopi() * (tePsigNi * 0.5 * scs - tePgamNi * 0.5 * sds);

    if (in.geolin) {
      result.dzdn[n - 1] +=
          hopi() * (tePsigx1 * teX1o + tePsigx2 * teX2o + tePsigyy * teYyo) *
              sigte +
          hopi() * (tePgamx1 * teX1o + tePgamx2 * teX2o + tePgamyy * teYyo) *
              gamte;
      result.dzdn[0] +=
          hopi() * (tePsigx1 * teX1p + tePsigx2 * teX2p + tePsigyy * teYyp) *
              sigte +
          hopi() * (tePgamx1 * teX1p + tePgamx2 * teX2p + tePgamyy * teYyp) *
              gamte;
      const double dqf0 = in.qf0[0] - in.qf0[n - 1];
      const double dqf1 = in.qf1[0] - in.qf1[n - 1];
      const double dqf2 = in.qf2[0] - in.qf2[n - 1];
      const double dqf3 = in.qf3[0] - in.qf3[n - 1];
      zQdof0 += hopi() * tePsig * 0.5 * dqf0 * scs -
                hopi() * tePgam * 0.5 * dqf0 * sds;
      zQdof1 += hopi() * tePsig * 0.5 * dqf1 * scs -
                hopi() * tePgam * 0.5 * dqf1 * sds;
      zQdof2 += hopi() * tePsig * 0.5 * dqf2 * scs -
                hopi() * tePgam * 0.5 * dqf2 * sds;
      zQdof3 += hopi() * tePsig * 0.5 * dqf3 * scs -
                hopi() * tePgam * 0.5 * dqf3 * sds;
    }
  }

  // Fortran: freestream contributions.
  psi += ctx.qinf * (cosa * in.yi - sina * in.xi);
  psiNi += ctx.qinf * (cosa * in.nyi - sina * in.nxi);
  qtan1 += ctx.qinf * in.nyi;
  qtan2 += -ctx.qinf * in.nxi;
  zQinf += cosa * in.yi - sina * in.xi;
  zAlfa += -ctx.qinf * (sina * in.yi + cosa * in.xi);

  result.psi = psi;
  result.psiNi = psiNi;
  result.qtan1 = qtan1;
  result.qtan2 = qtan2;
  result.zQinf = zQinf;
  result.zAlfa = zAlfa;
  result.zQdof0 = zQdof0;
  result.zQdof1 = zQdof1;
  result.zQdof2 = zQdof2;
  result.zQdof3 = zQdof3;

  // Fortran: LIMAGE image-point system (second DO 20 block after the RETURN
  // for .NOT.LIMAGE).
  if (ctx.image) {
    psiLinImageLoop(in, ctx, result);
  }

  return result;
}

// Fortran: SUBROUTINE PSWLIN.
PswLinResult pswLin(const PswLinInputs& in, const PswLinContext& ctx) {
  const std::size_t nw = ctx.x.size();
  if (ctx.y.size() != nw || ctx.apanel.size() != nw ||
      ctx.sig.size() != nw) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::pswLin: all wake lengths must be equal");
  }

  PswLinResult result;
  result.dzdm.assign(nw, 0.0);
  result.dqdm.assign(nw, 0.0);

  const double cosa = std::cos(in.alfa);
  const double sina = std::sin(in.alfa);
  static_cast<void>(cosa);
  static_cast<void>(sina);

  double psi = 0.0;
  double psiNi = 0.0;

  // Fortran: Loop JO=N+1..N+NW-1 (0-based jo0 = JO-N-1).
  if (nw >= 2) {
    for (std::size_t jo0 = 0; jo0 + 1 < nw; ++jo0) {
      std::size_t jp0 = jo0 + 1;
      std::size_t jm0 = jo0 >= 1 ? jo0 - 1 : 0;
      std::size_t jq0 = jp0 + 1;
      if (jo0 == 0) {
        jm0 = jo0;
      } else if (jo0 == nw - 2) {
        jq0 = jp0;
      }

      const double dso = std::sqrt((ctx.x[jo0] - ctx.x[jp0]) *
                                       (ctx.x[jo0] - ctx.x[jp0]) +
                                   (ctx.y[jo0] - ctx.y[jp0]) *
                                       (ctx.y[jo0] - ctx.y[jp0]));
      const double dsio = 1.0 / dso;

      const double apan = ctx.apanel[jo0];

      const double rx1 = in.xi - ctx.x[jo0];
      const double ry1 = in.yi - ctx.y[jo0];
      const double rx2 = in.xi - ctx.x[jp0];
      const double ry2 = in.yi - ctx.y[jp0];

      const double sx = (ctx.x[jp0] - ctx.x[jo0]) * dsio;
      const double sy = (ctx.y[jp0] - ctx.y[jo0]) * dsio;

      const double x1 = sx * rx1 + sy * ry1;
      const double x2 = sx * rx2 + sy * ry2;
      const double yy = sx * ry1 - sy * rx1;

      const double rs1 = rx1 * rx1 + ry1 * ry1;
      const double rs2 = rx2 * rx2 + ry2 * ry2;

      const double sgn = in.nodeWake ? 1.0 : fortranSign(1.0, yy);

      double g1 = 0.0;
      double t1 = 0.0;
      if (rs1 > 0.0 && !in.nodeWake) {
        g1 = std::log(rs1);
        t1 = std::atan2(sgn * x1, sgn * yy) - (0.5 - 0.5 * sgn) * invPi();
      } else if (rs1 > 0.0) {
        g1 = std::log(rs1);
        t1 = std::atan2(sgn * x1, sgn * yy) - (0.5 - 0.5 * sgn) * invPi();
      }
      double g2 = 0.0;
      double t2 = 0.0;
      if (rs2 > 0.0) {
        g2 = std::log(rs2);
        t2 = std::atan2(sgn * x2, sgn * yy) - (0.5 - 0.5 * sgn) * invPi();
      }

    const double x1i = sx * in.nxi + sy * in.nyi;
    const double x2i = sx * in.nxi + sy * in.nyi;
    const double yyi = sx * in.nyi - sy * in.nxi;


      // Fortran: 1-0 Half panel.
      {
        const double x0 = 0.5 * (x1 + x2);
        const double rs0 = x0 * x0 + yy * yy;
        const double g0 = std::log(rs0);
        const double t0 = std::atan2(sgn * x0, sgn * yy) -
                          (0.5 - 0.5 * sgn) * invPi();

        const double dxinv = 1.0 / (x1 - x0);
        const double psum = x0 * (t0 - apan) - x1 * (t1 - apan) +
                            0.5 * yy * (g1 - g0);
        const double pdif = ((x1 + x0) * psum + rs1 * (t1 - apan) -
                             rs0 * (t0 - apan) + (x0 - x1) * yy) * dxinv;

        const double psx1 = -(t1 - apan);
        const double psx0 = t0 - apan;
        const double psyy = 0.5 * (g1 - g0);

        const double pdx1 = ((x1 + x0) * psx1 + psum + 2.0 * x1 * (t1 - apan) -
                             pdif) * dxinv;
        const double pdx0 = ((x1 + x0) * psx0 + psum - 2.0 * x0 * (t0 - apan) +
                             pdif) * dxinv;
        const double pdyy = ((x1 + x0) * psyy +
                             2.0 * (x0 - x1 + yy * (t1 - t0))) * dxinv;

        const double dsm = std::sqrt((ctx.x[jp0] - ctx.x[jm0]) *
                                         (ctx.x[jp0] - ctx.x[jm0]) +
                                     (ctx.y[jp0] - ctx.y[jm0]) *
                                         (ctx.y[jp0] - ctx.y[jm0]));
        const double dsim = 1.0 / dsm;

        const double ssum = (ctx.sig[jp0] - ctx.sig[jo0]) * dsio +
                            (ctx.sig[jp0] - ctx.sig[jm0]) * dsim;
        const double sdif = (ctx.sig[jp0] - ctx.sig[jo0]) * dsio -
                            (ctx.sig[jp0] - ctx.sig[jm0]) * dsim;

        psi += qopi() * (psum * ssum + pdif * sdif);
        result.dzdm[jm0] += qopi() * (-psum * dsim + pdif * dsim);
        result.dzdm[jo0] += qopi() * (-psum * dsio - pdif * dsio);
        result.dzdm[jp0] +=
            qopi() * (psum * (dsio + dsim) + pdif * (dsio - dsim));

        const double psni = psx1 * x1i + psx0 * (x1i + x2i) * 0.5 + psyy * yyi;
        const double pdni = pdx1 * x1i + pdx0 * (x1i + x2i) * 0.5 + pdyy * yyi;
        psiNi += qopi() * (psni * ssum + pdni * sdif);
        result.dqdm[jm0] += qopi() * (-psni * dsim + pdni * dsim);
        result.dqdm[jo0] += qopi() * (-psni * dsio - pdni * dsio);
        result.dqdm[jp0] +=
            qopi() * (psni * (dsio + dsim) + pdni * (dsio - dsim));
      }

      // Fortran: 0-2 Half panel.
      {
        const double x0 = 0.5 * (x1 + x2);
        const double rs0 = x0 * x0 + yy * yy;
        const double g0 = std::log(rs0);
        const double t0 = std::atan2(sgn * x0, sgn * yy) -
                          (0.5 - 0.5 * sgn) * invPi();

        const double dxinv = 1.0 / (x0 - x2);
        const double psum = x2 * (t2 - apan) - x0 * (t0 - apan) +
                            0.5 * yy * (g0 - g2);
        const double pdif = ((x0 + x2) * psum + rs0 * (t0 - apan) -
                             rs2 * (t2 - apan) + (x2 - x0) * yy) * dxinv;

        const double psx0 = -(t0 - apan);
        const double psx2 = t2 - apan;
        const double psyy = 0.5 * (g0 - g2);

        const double pdx0 = ((x0 + x2) * psx0 + psum + 2.0 * x0 * (t0 - apan) -
                             pdif) * dxinv;
        const double pdx2 = ((x0 + x2) * psx2 + psum - 2.0 * x2 * (t2 - apan) +
                             pdif) * dxinv;
        const double pdyy = ((x0 + x2) * psyy +
                             2.0 * (x2 - x0 + yy * (t0 - t2))) * dxinv;

        const double dsp = std::sqrt((ctx.x[jq0] - ctx.x[jo0]) *
                                         (ctx.x[jq0] - ctx.x[jo0]) +
                                     (ctx.y[jq0] - ctx.y[jo0]) *
                                         (ctx.y[jq0] - ctx.y[jo0]));
        const double dsip = 1.0 / dsp;

        const double ssum = (ctx.sig[jq0] - ctx.sig[jo0]) * dsip +
                            (ctx.sig[jp0] - ctx.sig[jo0]) * dsio;
        const double sdif = (ctx.sig[jq0] - ctx.sig[jo0]) * dsip -
                            (ctx.sig[jp0] - ctx.sig[jo0]) * dsio;

        psi += qopi() * (psum * ssum + pdif * sdif);
        result.dzdm[jo0] +=
            qopi() * (-psum * (dsip + dsio) - pdif * (dsip - dsio));
        result.dzdm[jp0] += qopi() * (psum * dsio - pdif * dsio);
        result.dzdm[jq0] += qopi() * (psum * dsip + pdif * dsip);

        const double psni = psx0 * (x1i + x2i) * 0.5 + psx2 * x2i + psyy * yyi;
        const double pdni =
            pdx0 * (x1i + x2i) * 0.5 + pdx2 * x2i + pdyy * yyi;
        psiNi += qopi() * (psni * ssum + pdni * sdif);
        result.dqdm[jo0] +=
            qopi() * (-psni * (dsip + dsio) - pdni * (dsip - dsio));
        result.dqdm[jp0] += qopi() * (psni * dsio - pdni * dsio);
        result.dqdm[jq0] += qopi() * (psni * dsip + pdni * dsip);
      }

      // Fortran: freestream and wake contribution in QTAN (not used in
      // qdCalc; PSI part canonical).
      static_cast<void>(psi);
      static_cast<void>(psiNi);
    }
  }

  result.psi = psi;
  result.psiNi = psiNi;
  return result;
}

// Fortran: SUBROUTINE GGCALC.
Result<InfluenceSystem> ggCalc(const paneling::PanelGeometry& panel,
                               std::span<const double> sig, double minf,
                               double qinf) {
  static_cast<void>(minf);
  const std::size_t n = panel.x.size();
  if (sig.size() != n) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::ggCalc: SIG length must correspond to n");
  }
  if (n < 1) {
    return Result<InfluenceSystem>::fail(Error::make(
        ErrorCode::invalidArgument,
        "xfoil::core::inviscid::ggCalc: at least 1 Node required"));
  }

  // Fortran: AIJ(N+1)x(N+1) row-major, BIJ, GAMU, QINVU.
  const std::size_t m = n + 1;
  InfluenceSystem system;
  system.n = n;
  system.aij.assign(m * m, 0.0);
  system.aijpiv.assign(m, 0);
  system.bij.assign(m * m, 0.0);
  system.gamu1.assign(m, 0.0);
  system.gamu2.assign(m, 0.0);
  system.qinvu1.assign(m, 0.0);
  system.qinvu2.assign(m, 0.0);

  // Fortran: BWT = 0.1 (distance of the internal control point in front of
  // the sharp TE).
  constexpr double kBwt = 0.1;

  // Fortran: GAM/GAMU are initialized to 0 in GGCALC; the
  // influence coefficients DZDG/DZDM are SIG-independent.
  std::vector<double> gam0(n, 0.0);

  PsiLinContext ctx;
  ctx.x = panel.x;
  ctx.y = panel.y;
  ctx.s = panel.s;
  ctx.nx = panel.xn;
  ctx.ny = panel.yn;
  ctx.apanel = panel.apanel;
  ctx.gam = gam0;
  ctx.gamA = gam0;
  ctx.gamu1 = gam0;
  ctx.gamu2 = gam0;
  ctx.sig = sig;
  // Fortran: PANGEN calls TECALC; the TE state here comes from the
  // computed panel contour (values identical to the COMMON state).
  const paneling::TrailingEdgeGeometry teGeom = paneling::teGeometry(
      panel.x[0], panel.y[0], panel.xp[0], panel.yp[0], panel.x[n - 1],
      panel.y[n - 1], panel.xp[n - 1], panel.yp[n - 1], panel.chord);
  ctx.sharp = teGeom.sharp;
  ctx.dste = teGeom.dste;
  ctx.ante = teGeom.ante;
  ctx.aste = teGeom.aste;
  ctx.qinf = qinf;
  ctx.image = false;
  ctx.yimage = 0.0;

  // Fortran: Loop I=1..N (0-based i0 = I-1).
  for (std::size_t i0 = 0; i0 < n; ++i0) {
    // Fortran: CALL PSILIN(I,X,Y,NX,NY,PSI,PSI_N,.FALSE.,.TRUE.)
    PsiLinInputs in;
    in.node = i0;
    in.xi = panel.x[i0];
    in.yi = panel.y[i0];
    in.nxi = panel.xn[i0];
    in.nyi = panel.yn[i0];
    in.alfa = 0.0;
    in.nodeOnAirfoil = true;
    in.siglin = true;
    const PsiLinResult pl = psiLin(in, ctx);

    // Fortran: RES1 = QINF*Y(I), RES2 = -QINF*X(I).
    const double res1 = qinf * panel.y[i0];
    const double res2 = -qinf * panel.x[i0];

    for (std::size_t j0 = 0; j0 < n; ++j0) {
      system.aij[i0 * m + j0] = pl.dzdg[j0];
      system.bij[i0 * m + j0] = -pl.dzdm[j0];
    }
    system.aij[i0 * m + n] = -1.0;
    system.gamu1[i0] = -res1;
    system.gamu2[i0] = -res2;
  }

  // Fortran: Kutta condition.
  for (std::size_t j0 = 0; j0 < m; ++j0) {
    system.aij[n * m + j0] = 0.0;
  }
  system.aij[n * m + 0] = 1.0;
  system.aij[n * m + n - 1] = 1.0;
  system.gamu1[n] = 0.0;
  system.gamu2[n] = 0.0;
  for (std::size_t j0 = 0; j0 < n; ++j0) {
    system.bij[n * m + j0] = 0.0;
  }

  // Fortran: SHARP -> TE corner condition.
  if (panel.sharpTe) {
    const double ag1 =
        std::atan2(-panel.yp[0], -panel.xp[0]);
    const double ag2 = math::atanC(panel.yp[n - 1], panel.xp[n - 1], ag1);
    const double abis = 0.5 * (ag1 + ag2);
    const double cbis = std::cos(abis);
    const double sbis = std::sin(abis);

    const double ds1 = std::sqrt((panel.x[0] - panel.x[1]) *
                                     (panel.x[0] - panel.x[1]) +
                                 (panel.y[0] - panel.y[1]) *
                                     (panel.y[0] - panel.y[1]));
    const double ds2 = std::sqrt((panel.x[n - 1] - panel.x[n - 2]) *
                                     (panel.x[n - 1] - panel.x[n - 2]) +
                                 (panel.y[n - 1] - panel.y[n - 2]) *
                                     (panel.y[n - 1] - panel.y[n - 2]));
    const double dsmin = std::min(ds1, ds2);

    const double xbis = panel.xte - kBwt * dsmin * cbis;
    const double ybis = panel.yte - kBwt * dsmin * sbis;

    PsiLinInputs in;
    in.node = 0;
    in.xi = xbis;
    in.yi = ybis;
    in.nxi = -sbis;
    in.nyi = cbis;
    in.alfa = 0.0;
    in.nodeOnAirfoil = false;
    in.siglin = true;
    const PsiLinResult pl = psiLin(in, ctx);

    for (std::size_t j0 = 0; j0 < n; ++j0) {
      system.aij[(n - 1) * m + j0] = pl.dqdg[j0];
      system.bij[(n - 1) * m + j0] = -pl.dqdm[j0];
    }
    system.aij[(n - 1) * m + n] = 0.0;
    system.gamu1[n - 1] = -cbis;
    system.gamu2[n - 1] = -sbis;
  }

 // Fortran: LUDCMP/BAKSUB (Kernel, row-major).
  math::ludcmp(system.aij, m, system.aijpiv);
  math::baksub(system.aij, m, system.aijpiv, system.gamu1);
  math::baksub(system.aij, m, system.aijpiv, system.gamu2);

  // Fortran: QINVU(I,k) = GAMU(I,k) only for I=1..N (0-based n entries;
  // GAMU(N+1,*) is the Kutta residual, not part of QINVU).
  system.qinvu1.assign(system.gamu1.begin(), system.gamu1.begin() +
                                                 static_cast<long>(n));
  system.qinvu2.assign(system.gamu2.begin(), system.gamu2.begin() +
                                                 static_cast<long>(n));

  return Result<InfluenceSystem>::ok(std::move(system));
}

// Fortran: SUBROUTINE QWCALC.
WakeBasisResult qwCalc(const InfluenceSystem& system, const WakeState& wake,
                       const paneling::PanelGeometry& panel, double alfa, double qinf) {
  const std::size_t n = system.n;
  const std::size_t nw = wake.nw;
  if (wake.x.size() != nw || wake.y.size() != nw || wake.nx.size() != nw ||
      wake.ny.size() != nw || wake.apanel.size() != nw) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::qwCalc: wake lengths must be equal ");
  }

  WakeBasisResult result;
  result.qinvu1.assign(nw, 0.0);
  result.qinvu2.assign(nw, 0.0);

  PsiLinContext ctx;
  ctx.x = panel.x;
  ctx.y = panel.y;
  ctx.s = panel.s;
  ctx.nx = panel.xn;
  ctx.ny = panel.yn;
  ctx.apanel = panel.apanel;
  const std::vector<double> gam0(n, 0.0);
  ctx.gam = gam0;
  ctx.gamA = gam0;
  ctx.gamu1 = system.gamu1;
  ctx.gamu2 = system.gamu2;
  ctx.sig = std::span<const double>();
  // Fortran: PANGEN calls TECALC; the TE state here comes from the
  // computed panel contour (values identical to the COMMON state).
  const paneling::TrailingEdgeGeometry teGeom = paneling::teGeometry(
      panel.x[0], panel.y[0], panel.xp[0], panel.yp[0], panel.x[n - 1],
      panel.y[n - 1], panel.xp[n - 1], panel.yp[n - 1], panel.chord);
  ctx.sharp = teGeom.sharp;
  ctx.dste = teGeom.dste;
  ctx.ante = teGeom.ante;
  ctx.aste = teGeom.aste;
  ctx.qinf = qinf;
  ctx.image = false;
  ctx.yimage = 0.0;

  // Fortran: first wake point (same as the TE).
  if (nw >= 1) {
    result.qinvu1[0] = system.qinvu1[n - 1];
    result.qinvu2[0] = system.qinvu2[n - 1];
  }

  // Fortran: Loop I=N+2..N+NW (0-based iw0 = I-N-1 >= 1).
  for (std::size_t iw0 = 1; iw0 < nw; ++iw0) {
    PsiLinInputs in;
    in.node = n + iw0;
    in.xi = wake.x[iw0];
    in.yi = wake.y[iw0];
    in.nxi = wake.nx[iw0];
    in.nyi = wake.ny[iw0];
    in.alfa = alfa;
    in.nodeOnAirfoil = false;
    in.siglin = false;
    const PsiLinResult pl = psiLin(in, ctx);
    result.qinvu1[iw0] = pl.qtan1;
    result.qinvu2[iw0] = pl.qtan2;
  }

  return result;
}

// Fortran: SUBROUTINE QDCALC.
SourceInfluenceResult qdCalc(const InfluenceSystem& system,
                             const WakeState& wake,
                             const paneling::PanelGeometry& panel, double alfa,
                             double qinf, std::span<const double> sig) {
  const std::size_t n = system.n;
  const std::size_t nw = wake.nw;
  const std::size_t m = n + 1;
  const std::size_t cols = n + nw;
  const std::size_t rows = n + nw;

  SourceInfluenceResult out;
  out.dij.assign(rows * cols, 0.0);

  // Fortran: BIJ work matrix (N+1) x (N+NW) row-major.
  std::vector<double> bij(m * cols, 0.0);
  for (std::size_t j0 = 0; j0 < n; ++j0) {
    for (std::size_t i0 = 0; i0 < m; ++i0) {
      bij[i0 * cols + j0] = system.bij[i0 * m + j0];
    }
  }

  // Fortran: LADIJ=false: BAKSUB on the BIJ columns (dGam/dSig).
  std::vector<double> col(m, 0.0);
  for (std::size_t j0 = 0; j0 < n; ++j0) {
    for (std::size_t i0 = 0; i0 < m; ++i0) {
      col[i0] = bij[i0 * cols + j0];
    }
    math::baksub(system.aij, m, system.aijpiv, col);
    for (std::size_t i0 = 0; i0 < n; ++i0) {
      out.dij[i0 * cols + j0] = col[i0];
    }
  }

  // Fortran: PSWLIN -> BIJ(I,J) = -DZDM(J) (Wake source columns).
  const std::vector<double> wakeSig(nw, 0.0);
  PswLinContext wctx;
  wctx.x = wake.x;
  wctx.y = wake.y;
  wctx.apanel = wake.apanel;
  wctx.sig = wakeSig;
  for (std::size_t i0 = 0; i0 < n; ++i0) {
    PswLinInputs win;
    win.xi = panel.x[i0];
    win.yi = panel.y[i0];
    win.nxi = panel.xn[i0];
    win.nyi = panel.yn[i0];
    win.alfa = alfa;
    win.nodeWake = false;
    const PswLinResult pw = pswLin(win, wctx);
    for (std::size_t j0 = 0; j0 < nw; ++j0) {
      bij[i0 * cols + n + j0] = -pw.dzdm[j0];
    }
  }

  // Fortran: Kutta and SHARP lines without direct source influence.
  for (std::size_t j0 = n; j0 < cols; ++j0) {
    bij[n * cols + j0] = 0.0;
    if (panel.sharpTe) {
      bij[(n - 1) * cols + j0] = 0.0;
    }
  }

  // Fortran: BAKSUB on the wake columns — in place on BIJ (xpanel.f
  // DO 40, xpanel.f:1185-1187): BAKSUB overwrites BIJ(1..N+1,J); the
  // DO-80 block (xpanel.f:1216-1222) afterwards uses the post-BAKSUB values.
  for (std::size_t j0 = n; j0 < cols; ++j0) {
    for (std::size_t i0 = 0; i0 < m; ++i0) {
      col[i0] = bij[i0 * cols + j0];
    }
    math::baksub(system.aij, m, system.aijpiv, col);
    for (std::size_t i0 = 0; i0 < m; ++i0) {
      bij[i0 * cols + j0] = col[i0];
    }
    for (std::size_t i0 = 0; i0 < n; ++i0) {
      out.dij[i0 * cols + j0] = col[i0];
    }
  }

  // Fortran: DO 70 — dQtan/dGam and dQtan/dSig on the wake points.
  PsiLinContext ctx;
  ctx.x = panel.x;
  ctx.y = panel.y;
  ctx.s = panel.s;
  ctx.nx = panel.xn;
  ctx.ny = panel.yn;
  ctx.apanel = panel.apanel;
  const std::vector<double> gam0(n, 0.0);
  ctx.gam = gam0;
  ctx.gamA = gam0;
  ctx.gamu1 = system.gamu1;
  ctx.gamu2 = system.gamu2;
  ctx.sig = sig;
  // Fortran: PANGEN calls TECALC; the TE state here comes from the
  // computed panel contour (values identical to the COMMON state).
  const paneling::TrailingEdgeGeometry teGeom = paneling::teGeometry(
      panel.x[0], panel.y[0], panel.xp[0], panel.yp[0], panel.x[n - 1],
      panel.y[n - 1], panel.xp[n - 1], panel.yp[n - 1], panel.chord);
  ctx.sharp = teGeom.sharp;
  ctx.dste = teGeom.dste;
  ctx.ante = teGeom.ante;
  ctx.aste = teGeom.aste;
  ctx.qinf = qinf;
  ctx.image = false;
  ctx.yimage = 0.0;

  std::vector<double> cij(nw * n, 0.0);
  for (std::size_t iw0 = 0; iw0 < nw; ++iw0) {
    PsiLinInputs in;
    in.node = n + iw0;
    in.xi = wake.x[iw0];
    in.yi = wake.y[iw0];
    in.nxi = wake.nx[iw0];
    in.nyi = wake.ny[iw0];
    in.alfa = alfa;
    in.nodeOnAirfoil = false;
    in.siglin = true;
    const PsiLinResult pl = psiLin(in, ctx);
    for (std::size_t j0 = 0; j0 < n; ++j0) {
      cij[iw0 * n + j0] = pl.dqdg[j0];
      out.dij[(n + iw0) * cols + j0] = pl.dqdm[j0];
    }

    PswLinInputs win;
    win.xi = wake.x[iw0];
    win.yi = wake.y[iw0];
    win.nxi = wake.nx[iw0];
    win.nyi = wake.ny[iw0];
    win.alfa = alfa;
    win.nodeWake = true;
    const PswLinResult pw = pswLin(win, wctx);
    for (std::size_t j0 = 0; j0 < nw; ++j0) {
      out.dij[(n + iw0) * cols + n + j0] = pw.dqdm[j0];
    }
  }

  // Fortran: DO 80 — sources on the airfoil affect the wake vortex
  // strength, which in turn affects the wake Qtan.
  for (std::size_t iw0 = 0; iw0 < nw; ++iw0) {
    for (std::size_t j0 = 0; j0 < n; ++j0) {
      double sum = 0.0;
      for (std::size_t k0 = 0; k0 < n; ++k0) {
        sum += cij[iw0 * n + k0] * out.dij[k0 * cols + j0];
      }
      out.dij[(n + iw0) * cols + j0] += sum;
    }
    for (std::size_t j0 = n; j0 < cols; ++j0) {
      double sum = 0.0;
      for (std::size_t k0 = 0; k0 < n; ++k0) {
        sum += cij[iw0 * n + k0] * bij[k0 * cols + j0];
      }
      out.dij[(n + iw0) * cols + j0] += sum;
    }
  }

  // Fortran: first wake point has the same velocity as the TE.
  for (std::size_t j0 = 0; j0 < cols; ++j0) {
    out.dij[n * cols + j0] = out.dij[(n - 1) * cols + j0];
  }

  return out;
}

// Fortran: SUBROUTINE XYWAKE.
Result<WakeState> xywake(const paneling::PanelGeometry& panel,
                         std::span<const double> gam,
                         std::span<const double> gamA,
                         std::span<const double> gamu1,
                         std::span<const double> gamu2, double alfa,
                         double waklen, double qinf) {
  const std::size_t n = panel.x.size();
  if (gam.size() != n || gamA.size() != n) {
    throw std::invalid_argument(
        "xfoil::core::inviscid::xywake: GAM/GAM_A lengths must correspond to n");
  }

  // Fortran: NW = N/12 + 10*INT(WAKLEN) (integer division).
  std::size_t nw =
      n / 12 + 10 * static_cast<std::size_t>(static_cast<int>(waklen));

  // Fortran: NW > IWX -> message + NW = IWX (IWX = IQX/8+2 = 48); the
  // routine then sets the flag.
  constexpr std::size_t kIwx = 48;
  if (nw > kIwx) {
    nw = kIwx;
  }

  // Fortran: SETEXP(SNEW(N+1), DS1, WAKLEN*CHORD, NW).
  const double ds1 =
      0.5 * (panel.s[1] - panel.s[0] + panel.s[n - 1] - panel.s[n - 2]);
  std::vector<double> snew(nw, 0.0);
  const Diagnostic setexpDiag = math::setExp(snew, ds1, waklen * panel.chord);
  static_cast<void>(setexpDiag);

  const double xte = panel.xte;
  const double yte = panel.yte;

  WakeState wake;
  wake.nw = nw;
  wake.x.assign(nw, 0.0);
  wake.y.assign(nw, 0.0);
  wake.nx.assign(nw, 0.0);
  wake.ny.assign(nw, 0.0);
  wake.apanel.assign(nw, 0.0);
  wake.s.assign(nw, 0.0);

  PsiLinContext ctx;
  ctx.x = panel.x;
  ctx.y = panel.y;
  ctx.s = panel.s;
  ctx.nx = panel.xn;
  ctx.ny = panel.yn;
  ctx.apanel = panel.apanel;
  ctx.gam = gam;
  ctx.gamA = gamA;
  ctx.gamu1 = gamu1;
  ctx.gamu2 = gamu2;
  ctx.sig = std::span<const double>();
  // Fortran: PANGEN calls TECALC; the TE state here comes from the
  // computed panel contour (values identical to the COMMON state).
  const paneling::TrailingEdgeGeometry teGeom = paneling::teGeometry(
      panel.x[0], panel.y[0], panel.xp[0], panel.yp[0], panel.x[n - 1],
      panel.y[n - 1], panel.xp[n - 1], panel.yp[n - 1], panel.chord);
  ctx.sharp = teGeom.sharp;
  ctx.dste = teGeom.dste;
  ctx.ante = teGeom.ante;
  ctx.aste = teGeom.aste;
  ctx.qinf = qinf;
  ctx.image = false;
  ctx.yimage = 0.0;

  // Fortran: first wake point (shorter distance behind the TE).
  {
    const double sx = 0.5 * (panel.yp[n - 1] - panel.yp[0]);
    const double sy = 0.5 * (panel.xp[0] - panel.xp[n - 1]);
    const double smod = std::sqrt(sx * sx + sy * sy);
    wake.nx[0] = sx / smod;
    wake.ny[0] = sy / smod;
    wake.x[0] = xte - 0.0001 * wake.ny[0];
    wake.y[0] = yte + 0.0001 * wake.nx[0];
    wake.s[0] = panel.s[n - 1];
  }

  // Fortran: stream-function gradient at the first point -> normal and
  // APANEL; then trajectory.
  for (std::size_t iw0 = 0; iw0 < nw; ++iw0) {
    if (iw0 >= 1) {
      const double ds = snew[iw0] - snew[iw0 - 1];
      wake.x[iw0] = wake.x[iw0 - 1] - ds * wake.ny[iw0];
      wake.y[iw0] = wake.y[iw0 - 1] + ds * wake.nx[iw0];
      wake.s[iw0] = wake.s[iw0 - 1] + ds;
      if (iw0 == nw - 1) {
        continue;  // Fortran IF(I.EQ.N+NW) GO TO 10
      }
    }

    PsiLinInputs in;
    in.node = n + iw0;
    in.xi = wake.x[iw0];
    in.yi = wake.y[iw0];
    in.nxi = 1.0;
    in.nyi = 0.0;
    in.alfa = alfa;
    in.nodeOnAirfoil = false;
    in.siglin = false;
    const PsiLinResult px = psiLin(in, ctx);

    PsiLinInputs inY = in;
    inY.nxi = 0.0;
    inY.nyi = 1.0;
    const PsiLinResult py = psiLin(inY, ctx);

    if (iw0 + 1 < nw) {
      wake.nx[iw0 + 1] =
          -px.psiNi / std::sqrt(px.psiNi * px.psiNi + py.psiNi * py.psiNi);
      wake.ny[iw0 + 1] =
          -py.psiNi / std::sqrt(px.psiNi * px.psiNi + py.psiNi * py.psiNi);
    }
    wake.apanel[iw0] = std::atan2(py.psiNi, px.psiNi);
  }

  return Result<WakeState>::ok(std::move(wake));
}

}  // namespace xfoil::core::inviscid

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

// File: src/core/inverse_design/WallProfileKernels.cpp
// Port of B3SOLV and FS from fortran-src/profil.f.

#include "xfoil/core/inverse/WallProfileKernels.h"

#include <algorithm>
#include <cmath>

#include "xfoil/core/boundary_layer/BoundaryLayerCorrelations.h"

namespace xfoil::core::inverse {
namespace {

// block index for A/B/C: K,L=1..3, I=1..n.
inline std::size_t blk(std::size_t k, std::size_t l, std::size_t i) {
  return (i - 1) * 9 + (l - 1) * 3 + (k - 1);
}
// block index for R: K=1..3, L=1..nrhs, I=1..n.
inline std::size_t rhs(std::size_t k, std::size_t l, std::size_t i,
                       std::size_t nrhs) {
  return (i - 1) * 3 * nrhs + (l - 1) * 3 + (k - 1);
}

}  // namespace

Status b3Solv(std::vector<double>& a, std::vector<double>& b,
              std::vector<double>& c, std::vector<double>& r, int n,
              int nrhs) {
  if (n < 1 || nrhs < 1) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "b3Solv: n and nrhs >= 1 required"));
  }
  const std::size_t nn = static_cast<std::size_t>(n);
  const std::size_t nl = static_cast<std::size_t>(nrhs);
  if (a.size() < 9 * nn || b.size() < 9 * nn || c.size() < 9 * nn ||
      r.size() < 3 * nl * nn) {
    return Status::fail(Error::make(ErrorCode::sizeMismatch,
                                    "b3Solv: array size too small"));
  }
  const std::size_t N = nn;
  const std::size_t NRHS = nl;

  for (std::size_t I = 1; I <= N; ++I) {
    const std::size_t IM = I - 1;
    if (I != 1) {
      for (std::size_t K = 1; K <= 3; ++K) {
        for (std::size_t L = 1; L <= 3; ++L) {
          a[blk(K, L, I)] -= (b[blk(K, 1, I)] * c[blk(1, L, IM)] +
                              b[blk(K, 2, I)] * c[blk(2, L, IM)] +
                              b[blk(K, 3, I)] * c[blk(3, L, IM)]);
        }
        for (std::size_t L = 1; L <= NRHS; ++L) {
          r[rhs(K, L, I, NRHS)] -= (b[blk(K, 1, I)] * r[rhs(1, L, IM, NRHS)] +
                                    b[blk(K, 2, I)] * r[rhs(2, L, IM, NRHS)] +
                                    b[blk(K, 3, I)] * r[rhs(3, L, IM, NRHS)]);
        }
      }
    }
    for (std::size_t KPIV = 1; KPIV <= 2; ++KPIV) {
      const std::size_t KP1 = KPIV + 1;
      std::size_t KX = KPIV;
      for (std::size_t K = KP1; K <= 3; ++K) {
        if (std::fabs(a[blk(K, KPIV, I)]) > std::fabs(a[blk(KX, KPIV, I)])) {
          KX = K;
        }
      }
      if (a[blk(KX, KPIV, I)] == 0.0) {
        return Status::fail(Error::make(ErrorCode::invalidState,
                                        "b3Solv: singular A block"));
      }
      const double PIVOT = 1.0 / a[blk(KX, KPIV, I)];
      a[blk(KX, KPIV, I)] = a[blk(KPIV, KPIV, I)];
      for (std::size_t L = KP1; L <= 3; ++L) {
        const double TEMP = a[blk(KX, L, I)] * PIVOT;
        a[blk(KX, L, I)] = a[blk(KPIV, L, I)];
        a[blk(KPIV, L, I)] = TEMP;
      }
      for (std::size_t L = 1; L <= 3; ++L) {
        const double TEMP = c[blk(KX, L, I)] * PIVOT;
        c[blk(KX, L, I)] = c[blk(KPIV, L, I)];
        c[blk(KPIV, L, I)] = TEMP;
      }
      for (std::size_t L = 1; L <= NRHS; ++L) {
        const double TEMP = r[rhs(KX, L, I, NRHS)] * PIVOT;
        r[rhs(KX, L, I, NRHS)] = r[rhs(KPIV, L, I, NRHS)];
        r[rhs(KPIV, L, I, NRHS)] = TEMP;
      }
      for (std::size_t K = KP1; K <= 3; ++K) {
        for (std::size_t L = KP1; L <= 3; ++L) {
          a[blk(K, L, I)] -= a[blk(K, KPIV, I)] * a[blk(KPIV, L, I)];
        }
        c[blk(K, 1, I)] -= a[blk(K, KPIV, I)] * c[blk(KPIV, 1, I)];
        c[blk(K, 2, I)] -= a[blk(K, KPIV, I)] * c[blk(KPIV, 2, I)];
        c[blk(K, 3, I)] -= a[blk(K, KPIV, I)] * c[blk(KPIV, 3, I)];
        for (std::size_t L = 1; L <= NRHS; ++L) {
          r[rhs(K, L, I, NRHS)] -=
              a[blk(K, KPIV, I)] * r[rhs(KPIV, L, I, NRHS)];
        }
      }
    }
    if (a[blk(3, 3, I)] == 0.0) {
      return Status::fail(Error::make(ErrorCode::invalidState,
                                      "b3Solv: singular A block"));
    }
    const double PIVOT = 1.0 / a[blk(3, 3, I)];
    c[blk(3, 1, I)] *= PIVOT;
    c[blk(3, 2, I)] *= PIVOT;
    c[blk(3, 3, I)] *= PIVOT;
    for (std::size_t L = 1; L <= NRHS; ++L) {
      r[rhs(3, L, I, NRHS)] *= PIVOT;
    }
    for (std::size_t KPIV = 2; KPIV >= 1; --KPIV) {
      const std::size_t KP1 = KPIV + 1;
      for (std::size_t K = KP1; K <= 3; ++K) {
        c[blk(KPIV, 1, I)] -= a[blk(KPIV, K, I)] * c[blk(K, 1, I)];
        c[blk(KPIV, 2, I)] -= a[blk(KPIV, K, I)] * c[blk(K, 2, I)];
        c[blk(KPIV, 3, I)] -= a[blk(KPIV, K, I)] * c[blk(K, 3, I)];
        for (std::size_t L = 1; L <= NRHS; ++L) {
          r[rhs(KPIV, L, I, NRHS)] -=
              a[blk(KPIV, K, I)] * r[rhs(K, L, I, NRHS)];
        }
      }
    }
  }
  for (std::size_t I = N - 1; I >= 1; --I) {
    const std::size_t IP = I + 1;
    for (std::size_t L = 1; L <= NRHS; ++L) {
      for (std::size_t K = 1; K <= 3; ++K) {
        r[rhs(K, L, I, NRHS)] -= (r[rhs(1, L, IP, NRHS)] * c[blk(K, 1, I)] +
                                  r[rhs(2, L, IP, NRHS)] * c[blk(K, 2, I)] +
                                  r[rhs(3, L, IP, NRHS)] * c[blk(K, 3, I)]);
      }
    }
    if (I == 1) break;
  }
  return Status::ok();
}

Result<FalknerSkanResult> falknerSkan(int inorm, int ispec, double bspec,
                                      double hspec, int n, double etae,
                                      double geo) {
  if (n < 2 || n > kFsMaxPoints) {
    return Result<FalknerSkanResult>::fail(Error::make(
        ErrorCode::capacityExceeded, "FS: array overflow"));
  }
  if (inorm < 1 || inorm > 3 || ispec < 1 || ispec > 2) {
    return Result<FalknerSkanResult>::fail(Error::make(
        ErrorCode::invalidArgument, "FS: invalid INORM/ISPEC"));
  }
  const std::size_t N = static_cast<std::size_t>(n);
  const std::size_t NRHS = 3;
  const int ITMAX = 40;
  const double PI = 4.0 * std::atan(1.0);

  FalknerSkanResult res;
  res.eta.assign(N, 0.0);
  res.f.assign(N, 0.0);
  res.u.assign(N, 0.0);
  res.s.assign(N, 0.0);
  std::vector<double>& ETA = res.eta;
  std::vector<double>& F = res.f;
  std::vector<double>& U = res.u;
  std::vector<double>& S = res.s;

  double H = 0.0;
  double BU = 0.0;
  if (ispec == 1) {
    H = 2.6;
    BU = bspec;
  } else {
    H = hspec;
    if (H <= 2.17) H = 2.17;
    BU = (0.058 * std::pow(H - 4.0, 2) / (H - 1.0) - 0.068) /
         (6.54 * H - 14.07) * std::pow(H, 2);
    if (H > 4.0) BU = std::min(BU, 0.0);
  }

  double TN = 0.0;
  if (inorm == 3) {
    TN = (6.54 * H - 14.07) / std::pow(H, 2);
  } else {
    TN = 1.0;
  }

  double DETA = 1.0;
  ETA[0] = 0.0;
  for (std::size_t I = 1; I < N; ++I) {
    ETA[I] = ETA[I - 1] + DETA;
    DETA = geo * DETA;
  }
  for (std::size_t I = 0; I < N; ++I) {
    ETA[I] = ETA[I] * etae / ETA[N - 1];
  }

  if (H <= 3.0) {
    double ETJOIN = (inorm == 3) ? 7.3 : 5.0;
    double EFAC = 0.5 * PI / ETJOIN;
    std::size_t IJOIN = N;
    for (std::size_t I = 0; I < N; ++I) {
      U[I] = std::sin(EFAC * ETA[I]);
      F[I] = 1.0 / EFAC * (1.0 - std::cos(EFAC * ETA[I]));
      S[I] = EFAC * std::cos(EFAC * ETA[I]);
      if (ETA[I] > ETJOIN) {
        IJOIN = I;
        break;
      }
    }
    for (std::size_t I = IJOIN + 1; I < N; ++I) {
      U[I] = 1.0;
      F[I] = F[IJOIN] + ETA[I] - ETA[IJOIN];
      S[I] = 0.0;
    }
  } else {
    double ETJOIN1 = 0.0;
    double ETJOIN2 = 8.0;
    if (inorm == 3 && H > 4.0) {
      ETJOIN1 = H - 4.0;
      ETJOIN2 = ETJOIN1 + 8.0;
    }
    std::size_t IJOIN = N;
    for (std::size_t I = 0; I < N; ++I) {
      U[I] = 0.0;
      S[I] = 0.0;
      F[I] = 0.0;
      if (ETA[I] >= ETJOIN1) {
        IJOIN = I;
        break;
      }
    }
    double EFAC = 0.5 * PI / (ETJOIN2 - ETJOIN1);
    std::size_t IJOIN2 = N;
    for (std::size_t I = IJOIN + 1; I < N; ++I) {
      double EBAR = ETA[I] - ETJOIN1;
      U[I] = 0.5 - 0.5 * std::cos(2.0 * EFAC * EBAR);
      F[I] = 0.5 * EBAR - 0.25 / EFAC * std::sin(2.0 * EFAC * EBAR);
      S[I] = EFAC * std::sin(2.0 * EFAC * EBAR);
      if (ETA[I] >= ETJOIN2) {
        IJOIN2 = I;
        break;
      }
    }
    for (std::size_t I = IJOIN2 + 1; I < N; ++I) {
      U[I] = 1.0;
      F[I] = F[IJOIN2] + ETA[I] - ETA[IJOIN2];
      S[I] = 0.0;
    }
  }

  std::vector<double> A(9 * N, 0.0), B(9 * N, 0.0), C(9 * N, 0.0);
  std::vector<double> R(3 * NRHS * N, 0.0);
  double DSI = 0.0;
  double THI = 0.0;

  for (int ITER = 1; ITER <= ITMAX; ++ITER) {
    std::fill(A.begin(), A.end(), 0.0);
    std::fill(B.begin(), B.end(), 0.0);
    std::fill(C.begin(), C.end(), 0.0);
    std::fill(R.begin(), R.end(), 0.0);

    double BETU = 0.0, BETU_BU = 0.0, BETN = 0.0, BETN_BU = 0.0;
    if (inorm == 2) {
      BETU = 2.0 * BU / (BU + 1.0);
      BETU_BU = (2.0 - BETU / (BU + 1.0)) / (BU + 1.0);
      BETN = 1.0;
      BETN_BU = 0.0;
    } else {
      BETU = BU;
      BETU_BU = 1.0;
      BETN = 0.5 * (1.0 + BU);
      BETN_BU = 0.5;
    }

    A[blk(1, 1, 1)] = 1.0;
    A[blk(2, 2, 1)] = 1.0;
    A[blk(3, 2, N)] = 1.0;
    R[rhs(1, 1, 1, NRHS)] = F[0];
    R[rhs(2, 1, 1, NRHS)] = U[0];
    R[rhs(3, 1, N, NRHS)] = U[N - 1] - 1.0;

    for (std::size_t I = 1; I <= N - 1; ++I) {
      const std::size_t Ip = I + 1;
      double dEta = ETA[Ip - 1] - ETA[I - 1];
      R[rhs(1, 1, Ip, NRHS)] =
          F[Ip - 1] - F[I - 1] - 0.5 * dEta * (U[Ip - 1] + U[I - 1]);
      R[rhs(2, 1, Ip, NRHS)] =
          U[Ip - 1] - U[I - 1] - 0.5 * dEta * (S[Ip - 1] + S[I - 1]);
      R[rhs(3, 1, I, NRHS)] =
          S[Ip - 1] - S[I - 1] +
          TN * (BETN * dEta * 0.5 *
                    (F[Ip - 1] * S[Ip - 1] + F[I - 1] * S[I - 1]) +
                BETU * dEta *
                    (1.0 - 0.5 * (U[Ip - 1] * U[Ip - 1] + U[I - 1] * U[I - 1])));
      A[blk(3, 1, I)] = TN * BETN * 0.5 * dEta * S[I - 1];
      C[blk(3, 1, I)] = TN * BETN * 0.5 * dEta * S[Ip - 1];
      A[blk(3, 2, I)] = -TN * BETU * dEta * U[I - 1];
      C[blk(3, 2, I)] = -TN * BETU * dEta * U[Ip - 1];
      A[blk(3, 3, I)] = TN * BETN * 0.5 * dEta * F[I - 1] - 1.0;
      C[blk(3, 3, I)] = TN * BETN * 0.5 * dEta * F[Ip - 1] + 1.0;
      B[blk(1, 1, Ip)] = -1.0;
      A[blk(1, 1, Ip)] = 1.0;
      B[blk(1, 2, Ip)] = -0.5 * dEta;
      A[blk(1, 2, Ip)] = -0.5 * dEta;
      B[blk(2, 2, Ip)] = -1.0;
      A[blk(2, 2, Ip)] = 1.0;
      B[blk(2, 3, Ip)] = -0.5 * dEta;
      A[blk(2, 3, Ip)] = -0.5 * dEta;
      R[rhs(3, 2, I, NRHS)] =
          TN * (BETN_BU * dEta * 0.5 *
                    (F[Ip - 1] * S[Ip - 1] + F[I - 1] * S[I - 1]) +
                BETU_BU * dEta *
                    (1.0 - 0.5 * (U[Ip - 1] * U[Ip - 1] + U[I - 1] * U[I - 1])));
      R[rhs(3, 3, I, NRHS)] =
          (BETN * dEta * 0.5 * (F[Ip - 1] * S[Ip - 1] + F[I - 1] * S[I - 1]) +
           BETU * dEta *
               (1.0 - 0.5 * (U[Ip - 1] * U[Ip - 1] + U[I - 1] * U[I - 1])));
    }

    Status st = b3Solv(A, B, C, R, n, static_cast<int>(NRHS));
    if (st.isErr()) return Result<FalknerSkanResult>::fail(st.error());

    double DSI1 = 0.0, DSI2 = 0.0, DSI3 = 0.0;
    double THI1 = 0.0, THI2 = 0.0, THI3 = 0.0;
    DSI = 0.0;
    THI = 0.0;
    for (std::size_t I = 1; I <= N - 1; ++I) {
      const std::size_t Ip = I + 1;
      double US = U[I - 1] + U[Ip - 1];
      double dEta = ETA[Ip - 1] - ETA[I - 1];
      DSI += (1.0 - 0.5 * US) * dEta;
      double DSI_US = -0.5 * dEta;
      THI += (1.0 - 0.5 * US) * 0.5 * US * dEta;
      double THI_US = (0.5 - 0.5 * US) * dEta;
      DSI1 += DSI_US * (R[rhs(2, 1, I, NRHS)] + R[rhs(2, 1, Ip, NRHS)]);
      DSI2 += DSI_US * (R[rhs(2, 2, I, NRHS)] + R[rhs(2, 2, Ip, NRHS)]);
      DSI3 += DSI_US * (R[rhs(2, 3, I, NRHS)] + R[rhs(2, 3, Ip, NRHS)]);
      THI1 += THI_US * (R[rhs(2, 1, I, NRHS)] + R[rhs(2, 1, Ip, NRHS)]);
      THI2 += THI_US * (R[rhs(2, 2, I, NRHS)] + R[rhs(2, 2, Ip, NRHS)]);
      THI3 += THI_US * (R[rhs(2, 3, I, NRHS)] + R[rhs(2, 3, Ip, NRHS)]);
    }

    double R1 = 0.0, Q11 = 0.0, Q12 = 0.0;
    if (ispec == 1) {
      R1 = bspec - BU;
      Q11 = 1.0;
      Q12 = 0.0;
    } else {
      R1 = DSI - hspec * THI - DSI1 + hspec * THI1;
      Q11 = -DSI2 + hspec * THI2;
      Q12 = -DSI3 + hspec * THI3;
    }

    double R2 = 0.0, Q21 = 0.0, Q22 = 0.0;
    if (inorm == 3) {
      R2 = THI - 1.0 - THI1;
      Q21 = -THI2;
      Q22 = -THI3;
    } else {
      R2 = TN - 1.0;
      Q21 = 0.0;
      Q22 = 1.0;
    }

    double DET = Q11 * Q22 - Q12 * Q21;
    double DBU = -(R1 * Q22 - Q12 * R2) / DET;
    double DTN = -(Q11 * R2 - R1 * Q21) / DET;

    double RMAX = 0.0;
    double RMS = 0.0;
    for (std::size_t I = 1; I <= N; ++I) {
      double DF = -R[rhs(1, 1, I, NRHS)] - DBU * R[rhs(1, 2, I, NRHS)] -
                  DTN * R[rhs(1, 3, I, NRHS)];
      double DU = -R[rhs(2, 1, I, NRHS)] - DBU * R[rhs(2, 2, I, NRHS)] -
                  DTN * R[rhs(2, 3, I, NRHS)];
      double DS = -R[rhs(3, 1, I, NRHS)] - DBU * R[rhs(3, 2, I, NRHS)] -
                  DTN * R[rhs(3, 3, I, NRHS)];
      RMAX = std::max({RMAX, std::fabs(DF), std::fabs(DU), std::fabs(DS)});
      RMS = DF * DF + DU * DU + DS * DS + RMS;
    }
    RMS = std::sqrt(RMS / (3.0 * static_cast<double>(N) + 3.0));
    RMAX = std::max({RMAX, std::fabs(DBU / 1.0), std::fabs(DTN / TN)});

    double RLX = 1.0;
    if (RMAX > 0.5) RLX = 0.5 / RMAX;

    for (std::size_t I = 1; I <= N; ++I) {
      double DF = -R[rhs(1, 1, I, NRHS)] - DBU * R[rhs(1, 2, I, NRHS)] -
                  DTN * R[rhs(1, 3, I, NRHS)];
      double DU = -R[rhs(2, 1, I, NRHS)] - DBU * R[rhs(2, 2, I, NRHS)] -
                  DTN * R[rhs(2, 3, I, NRHS)];
      double DS = -R[rhs(3, 1, I, NRHS)] - DBU * R[rhs(3, 2, I, NRHS)] -
                  DTN * R[rhs(3, 3, I, NRHS)];
      F[I - 1] = F[I - 1] + RLX * DF;
      U[I - 1] = U[I - 1] + RLX * DU;
      S[I - 1] = S[I - 1] + RLX * DS;
    }
    BU = BU + RLX * DBU;
    TN = TN + RLX * DTN;

    if (ITER > 3 && RMS < 1.0e-6) break;
  }

  res.hspec = DSI / THI;
  res.bspec = BU;
  res.delta = std::sqrt(TN);
  return Result<FalknerSkanResult>::ok(std::move(res));
}

namespace {
// Fortran SIGN(1.0, x): +1.0 for x >= 0, otherwise -1.0.
double fsign(double x) { return x < 0.0 ? -1.0 : 1.0; }
}  // namespace

Status prWall(double dstar, double theta, double rt, double ms, double ct,
              double cffac, WallProfileState& st) {
  (void)ct;
  if (theta <= 0.0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "PRWALL: THETA > 0 required"));
  }
  const std::size_t N = 65;
  const double HPI = 1.570796327;
  const double AK = 0.09;
  const double VKAP = 0.40;
  const double VB = 5.0;

  const double HK = dstar / theta;
  const double UO = 1.0;
  double BB = 1.0;

  const boundary_layer::CorrelationValue cftv =
      boundary_layer::cft(HK, rt, ms, cffac);
  double CF = cftv.value;
  double SGN = fsign(CF);
  double UT = SGN * std::sqrt(0.5 * std::fabs(CF));

  double UI = std::min(UT / AK * HPI, 0.90);
  double DO = HK * theta / (1.0 - 0.5 * (UO + UI));

  const double EBK = std::exp(-VB * VKAP);

  std::vector<double> ETA(N + 1, 0.0), UIP(N + 1, 0.0), UIP_DP(N + 1, 0.0),
      G(N + 1, 0.0), G_BB(N + 1, 0.0);

  double B11 = 0.0, B12 = 0.0, B21 = 0.0, B22 = 0.0;
  double DSN_TH = 0.0, DSN_RT = 0.0, DSN_MS = 0.0;
  double THN_TH = 0.0, THN_RT = 0.0, THN_MS = 0.0;
  double DUO = 0.0, DUO_TH = 0.0, DUO_RT = 0.0, DUO_MS = 0.0;
  double DUO_UT = 0.0, DUO_DO = 0.0;

  for (int ITER = 1; ITER <= 40; ++ITER) {
    SGN = fsign(UT);
    const double DP = SGN * UT * rt * DO / theta;
    const double DP_DO = SGN * UT * rt / theta;
    const double DP_UT = SGN * rt * DO / theta;
    const double DP_TH = -DP / theta;
    const double DP_RT = SGN * UT * DO / theta;
    const double DP_MS = 0.0;

    double UPE = std::log(DP) / VKAP + VB;
    double EXU = 0.0, DP_U = 0.0, UK = 0.0;
    for (int ITUP = 1; ITUP <= 5; ++ITUP) {
      UK = UPE * VKAP;
      const double ARG = UK - VB * VKAP;
      EXU = std::exp(ARG);
      const double REZ =
          UPE + EXU - EBK * (1.0 + UK + UK * UK / 2.0 + UK * UK * UK / 6.0) -
          DP;
      DP_U = 1.0 + (EXU - EBK * (1.0 + UK + UK * UK / 2.0)) * VKAP;
      if (std::fabs(REZ / DP) < 1.0e-5) break;
      const double DUPE = -REZ / DP_U;
      UPE = UPE + DUPE;
    }
    const double UPE_DP = 1.0 / DP_U;

    const double DP_UU = (EXU - EBK * (1.0 + UK)) * VKAP * VKAP;
    const double DP_UUU = (EXU - EBK) * VKAP * VKAP * VKAP;

    const double UPD = 1.0 / DP_U;
    const double UPD_DP = (-1.0 / (DP_U * DP_U * DP_U)) * DP_UU;
    const double UPDD = UPD_DP;
    const double UPDD_DP =
        (-1.0 / std::pow(DP_U, 4)) * DP_UUU +
        (3.0 / std::pow(DP_U, 5)) * DP_UU * DP_UU;

    const double DC2 = 0.5 * DP * DP * UPDD - DP * UPD;
    const double DC2_DP = DP * UPDD - UPD + 0.5 * DP * DP * UPDD_DP -
                          DP * UPD_DP;
    const double DC3 = -(DP * DP * UPDD - DP * UPD) / 3.0;
    const double DC3_DP = -(2.0 * DP * UPDD - UPD) / 3.0 -
                          (DP * DP * UPDD_DP - DP * UPD_DP) / 3.0;

    DUO = UO - UT * (UPE + DC2 + DC3);
    const double DUO_DP = -UT * (UPE_DP + DC2_DP + DC3_DP);
    DUO_UT = -(UPE + DC2 + DC3) + DUO_DP * DP_UT;
    DUO_DO = DUO_DP * DP_DO;
    DUO_TH = DUO_DP * DP_TH;
    DUO_RT = DUO_DP * DP_RT;
    DUO_MS = DUO_DP * DP_MS;

    const double BB1 = 3.0 * (BB + 2.0) * (BB + 3.0) / (BB + 7.0);
    const double BB1_BB =
        3.0 * (BB * 2.0 + 5.0) / (BB + 7.0) - BB1 / (BB + 7.0);
    const double BB2 = -5.0 * (BB + 1.0) * (BB + 3.0) / (BB + 7.0);
    const double BB2_BB =
        -5.0 * (BB * 2.0 + 4.0) / (BB + 7.0) - BB2 / (BB + 7.0);
    const double BB3 = 2.0 * (BB + 1.0) * (BB + 2.0) / (BB + 7.0);
    const double BB3_BB =
        2.0 * (BB * 2.0 + 3.0) / (BB + 7.0) - BB3 / (BB + 7.0);

    const double EXUPE = EXU;
    const double DEXU = (EXUPE - EBK) / static_cast<double>(N - 1);

    ETA[1] = 0.0;
    UIP[1] = 0.0;
    UIP_DP[1] = 0.0;
    G[1] = 0.0;
    G_BB[1] = 0.0;

    for (std::size_t I = 2; I <= N; ++I) {
      const double fi = static_cast<double>(I - 1);
      const double fni = static_cast<double>(N - I);
      const double fn1 = static_cast<double>(N - 1);
      const double exu = EBK + (DEXU - 0.75 * DEXU * fni / fn1) * fi;
      const double uk = std::log(exu) + VB * VKAP;
      const double up = uk / VKAP;
      const double yp =
          up + exu - EBK * (1.0 + uk + uk * uk / 2.0 + uk * uk * uk / 6.0);
      const double et = yp / DP;
      UIP[I] = up + DC2 * et * et + DC3 * et * et * et;
      UIP_DP[I] = DC2_DP * et * et + DC3_DP * et * et * et;
      const double etb = std::pow(et, BB);
      const double ale = std::log(et);
      G[I] = (BB1 * et + BB2 * et * et + BB3 * et * et * et) * etb;
      G_BB[I] = (BB1_BB * et + BB2_BB * et * et + BB3_BB * et * et * et) *
                    etb +
                G[I] * ale;
      ETA[I] = et;
    }

    double DSN = 0.0, DSN_DO = 0.0, DSN_UT = 0.0, DSN_BB = 0.0;
    DSN_TH = 0.0;
    DSN_RT = 0.0;
    DSN_MS = 0.0;
    double THN = 0.0, THN_DO = 0.0, THN_UT = 0.0, THN_BB = 0.0;
    THN_TH = 0.0;
    THN_RT = 0.0;
    THN_MS = 0.0;

    for (std::size_t I = 1; I <= N - 1; ++I) {
      const double dEta = ETA[I + 1] - ETA[I];
      const double GA = 0.5 * (G[I + 1] + G[I]);
      const double GA_BB = 0.5 * (G_BB[I + 1] + G_BB[I]);
      const double UIPA = 0.5 * (UIP[I + 1] + UIP[I]);
      const double UIPA_DP = 0.5 * (UIP_DP[I + 1] + UIP_DP[I]);
      const double U = UT * UIPA + DUO * GA;
      const double U_DP = UT * UIPA_DP;
      const double U_DO = DUO_DO * GA + U_DP * DP_DO;
      const double U_UT = UIPA + DUO_UT * GA + U_DP * DP_UT;
      const double U_BB = DUO * GA_BB;
      const double U_TH = DUO_TH * GA + U_DP * DP_TH;
      const double U_RT = DUO_RT * GA + U_DP * DP_RT;
      const double U_MS = DUO_MS * GA + U_DP * DP_MS;
      DSN += (1.0 - U) * dEta;
      DSN_DO -= U_DO * dEta;
      DSN_UT -= U_UT * dEta;
      DSN_BB -= U_BB * dEta;
      DSN_TH -= U_TH * dEta;
      DSN_RT -= U_RT * dEta;
      DSN_MS -= U_MS * dEta;
      THN += (U - U * U) * dEta;
      THN_DO += (1.0 - 2.0 * U) * U_DO * dEta;
      THN_UT += (1.0 - 2.0 * U) * U_UT * dEta;
      THN_BB += (1.0 - 2.0 * U) * U_BB * dEta;
      THN_TH += (1.0 - 2.0 * U) * U_TH * dEta;
      THN_RT += (1.0 - 2.0 * U) * U_RT * dEta;
      THN_MS += (1.0 - 2.0 * U) * U_MS * dEta;
    }

    const double REZ1 = DO * DSN - theta * HK;
    (void)DSN_BB;
    (void)THN_BB;
    const double A11 = DO * DSN_DO + DSN;
    const double A12 = DO * DSN_UT;
    const double REZ2 = DO * THN - theta;
    const double A21 = DO * THN_DO + THN;
    const double A22 = DO * THN_UT;

    if (std::fabs(REZ1 / theta) < 2.0e-5 && std::fabs(REZ2 / theta) < 2.0e-5) {
      break;
    }

    const double DET = A11 * A22 - A12 * A21;
    B11 = A22 / DET;
    B12 = -A12 / DET;
    B21 = -A21 / DET;
    B22 = A11 / DET;

    const double DDO = -(B11 * REZ1 + B12 * REZ2);
    const double DUT = -(B21 * REZ1 + B22 * REZ2);

    const double DMAX = std::max(std::fabs(DDO / DO), std::fabs(DUT / 0.05));
    double RLX = 1.0;
    if (DMAX > 0.5) RLX = 0.5 / DMAX;

    DO = DO + RLX * DDO;
    UT = UT + RLX * DUT;
  }

  const double Z1_HK = -theta;
  const double Z1_TH = DO * DSN_TH - HK;
  const double Z1_RT = DO * DSN_RT;
  const double Z1_MS = DO * DSN_MS;
  const double Z2_HK = 0.0;
  const double Z2_TH = DO * THN_TH - 1.0;
  const double Z2_RT = DO * THN_RT;
  const double Z2_MS = DO * THN_MS;

  const double DO_HK = -(B11 * Z1_HK + B12 * Z2_HK);
  const double DO_TH = -(B11 * Z1_TH + B12 * Z2_TH);
  const double DO_RT = -(B11 * Z1_RT + B12 * Z2_RT);
  const double DO_MS = -(B11 * Z1_MS + B12 * Z2_MS);

  const double UT_HK = -(B21 * Z1_HK + B22 * Z2_HK);
  const double UT_TH = -(B21 * Z1_TH + B22 * Z2_TH);
  const double UT_RT = -(B21 * Z1_RT + B22 * Z2_RT);
  const double UT_MS = -(B21 * Z1_MS + B22 * Z2_MS);
  (void)DO_HK;
  (void)UT_HK;

  CF = SGN * 2.0 * UT * UT;
  const double CF_UT = SGN * 4.0 * UT;
  const double CF_DO = 0.0;
  const double CF_TH = CF_UT * UT_TH + CF_DO * DO_TH;
  const double CF_RT = CF_UT * UT_RT + CF_DO * DO_RT;
  const double CF_MS = CF_UT * UT_MS + CF_DO * DO_MS;

  UI = UO - DUO;
  const double UI_UT = -DUO_UT;
  const double UI_DO = -DUO_DO;
  const double UI_TH = UI_UT * UT_TH + UI_DO * DO_TH - DUO_TH;
  const double UI_RT = UI_UT * UT_RT + UI_DO * DO_RT - DUO_RT;
  const double UI_MS = UI_UT * UT_MS + UI_DO * DO_MS - DUO_MS;

  st.bb = BB;
  st.do_ = DO;
  st.do_th = DO_TH;
  st.do_rt = DO_RT;
  st.do_ms = DO_MS;
  st.ui = UI;
  st.ui_th = UI_TH;
  st.ui_rt = UI_RT;
  st.ui_ms = UI_MS;
  st.cf = CF;
  st.cf_th = CF_TH;
  st.cf_rt = CF_RT;
  st.cf_ms = CF_MS;
  return Status::ok();
}

Status uwWall(double th, double uo, double do_, double ui, double rt,
              double cf, double bb, int n, std::vector<double>& y,
              std::vector<double>& u) {
  (void)ui;
  if (n < 2 || th <= 0.0) {
    return Status::fail(Error::make(ErrorCode::invalidArgument,
                                    "UWALL: invalid input"));
  }
  const std::size_t NE = static_cast<std::size_t>(n);
  const double AK = 0.09;
  (void)AK;
  const double VKAP = 0.40;
  const double VB = 5.0;
  const double EBK = std::exp(-VB * VKAP);

  const double SGN = fsign(cf);
  const double UT = SGN * std::sqrt(0.5 * std::fabs(cf));
  const double DP = SGN * UT * rt * do_ / th;

  double UPE = std::log(DP) / VKAP + VB;
  double EXU = 0.0, DP_U = 0.0, UK = 0.0;
  for (int ITUP = 1; ITUP <= 5; ++ITUP) {
    UK = UPE * VKAP;
    const double ARG = UK - VB * VKAP;
    EXU = std::exp(ARG);
    const double REZ =
        UPE + EXU - EBK * (1.0 + UK + UK * UK / 2.0 + UK * UK * UK / 6.0) - DP;
    DP_U = 1.0 + (EXU - EBK * (1.0 + UK + UK * UK / 2.0)) * VKAP;
    if (std::fabs(REZ / DP) < 1.0e-5) break;
    const double DUPE = -REZ / DP_U;
    UPE = UPE + DUPE;
  }

  const double DP_UU = (EXU - EBK * (1.0 + UK)) * VKAP * VKAP;
  const double UPD = 1.0 / DP_U;
  const double UPDD = (-1.0 / std::pow(DP_U, 3)) * DP_UU;
  const double DC2 = 0.5 * DP * DP * UPDD - DP * UPD;
  const double DC3 = -(DP * DP * UPDD - DP * UPD) / 3.0;
  const double DUO = uo - UT * (UPE + DC2 + DC3);

  const double BB1 = 3.0 * (bb + 2.0) * (bb + 3.0) / (bb + 7.0);
  const double BB2 = -5.0 * (bb + 1.0) * (bb + 3.0) / (bb + 7.0);
  const double BB3 = 2.0 * (bb + 1.0) * (bb + 2.0) / (bb + 7.0);

  const double EXUPE = EXU;
  const double DEXU = (EXUPE - EBK) / static_cast<double>(NE - 1);

  y.assign(static_cast<std::size_t>(n), 0.0);
  u.assign(static_cast<std::size_t>(n), 0.0);
  y[0] = 0.0;
  u[0] = 0.0;
  for (std::size_t I = 2; I <= NE; ++I) {
    const std::size_t k = I - 1;
    const double fi = static_cast<double>(I - 1);
    const double fni = static_cast<double>(NE - I);
    const double fn1 = static_cast<double>(NE - 1);
    const double exu = EBK + (DEXU - 0.75 * DEXU * fni / fn1) * fi;
    const double uk = std::log(exu) + VB * VKAP;
    const double up = uk / VKAP;
    const double yp =
        up + exu - EBK * (1.0 + uk + uk * uk / 2.0 + uk * uk * uk / 6.0);
    const double et = yp / DP;
    const double uip = up + DC2 * et * et + DC3 * et * et * et;
    const double etb = std::pow(et, bb);
    const double g = (BB1 * et + BB2 * et * et + BB3 * et * et * et) * etb;
    y[k] = et * do_;
    u[k] = UT * uip + DUO * g;
  }
  return Status::ok();
}

}  // namespace xfoil::core::inverse

// End: src/core/inverse_design/WallProfileKernels.cpp

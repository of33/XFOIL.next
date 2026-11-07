C XFOIL.next
C
C Copyright (C) 2026 Olaf Frommann
C https://github.com/of33/XFOIL.next
C
C XFOIL.next is based on XFOIL 6.99 by Mark Drela and
C Harold Youngren.
C
C This program is free software: you can redistribute it and/or modify
C it under the terms of the GNU General Public License as published by
C the Free Software Foundation, either version 3 of the License, or
C (at your option) any later version.
C
C This program is distributed in the hope that it will be useful,
C but WITHOUT ANY WARRANTY; without even the implied warranty of
C MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
C GNU General Public License for more details.
C
C You should have received a copy of the GNU General Public License
C along with this program. If not, see <https://www.gnu.org/licenses/>.
C
C SPDX-License-Identifier: GPL-3.0-or-later

C File:
C tests/fortran-reference/harnesses/inverse-design/mdes_panel_glue.f
C Harness glue for MAPGAM/QSPCIR. XFOIL.INC and CIRCLE.INC both define
C     the symbol PI in two COMMON blocks; a single program unit therefore
C     cannot INCLUDE both. These glue units INCLUDE only
C XFOIL.INC and call the canonical routines (LEFIND, MAPGAM, QSPCIR).
C     They change no Fortran source.
      SUBROUTINE SETPANEL(NIN, XIN, YIN, XPIN, YPIN, SIN, SLEOUT)
      INCLUDE 'XFOIL.INC'
      INTEGER NIN, I
      REAL XIN(*),YIN(*),XPIN(*),YPIN(*),SIN(*),SLEOUT
      N = NIN
      DO 1 I = 1, NIN
        X(I) = XIN(I)
        XP(I) = XPIN(I)
        Y(I) = YIN(I)
        YP(I) = YPIN(I)
        S(I) = SIN(I)
    1 CONTINUE
      CALL LEFIND(SLE, X, XP, Y, YP, S, N)
      XTE = 0.5*(X(1) + X(N))
      YTE = 0.5*(Y(1) + Y(N))
      XLE = SEVAL(SLE, X, XP, S, N)
      YLE = SEVAL(SLE, Y, YP, S, N)
      SLEOUT = SLE
      MINF = 0.0
      QINF = 1.0
      RETURN
      END
C
      SUBROUTINE RUNMAP(IAC, ALG, CLG, CMG, NSPO, SP, QG, XS, YS, SPL)
      INCLUDE 'XFOIL.INC'
      INTEGER IAC, I, NSPO
      REAL ALG, CLG, CMG, SP(*), QG(*), XS(*), YS(*), SPL
      CALL MAPGAM(IAC, ALG, CLG, CMG)
      NSPO = NSP
      SPL = SSPLE
      DO 2 I = 1, NSP
        SP(I) = SSPEC(I)
        QG(I) = QGAMM(I)
        XS(I) = XSPOC(I)
        YS(I) = YSPOC(I)
    2 CONTINUE
      RETURN
      END
C
      SUBROUTINE RUNQSP(IACQ, ALIN, CLIN, NSPO, QS, QSP, ALQ, CLQ, CMQ)
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSPO, IACQ
      REAL ALIN, CLIN, QS(*), QSP(*), ALQ, CLQ, CMQ
      NQSP = 1
      IACQSP = IACQ
      ALQSP(1) = ALIN
      CLQSP(1) = CLIN
      CMQSP(1) = 0.0
      CALL QSPCIR
      ALQ = ALQSP(1)
      CLQ = CLQSP(1)
      CMQ = CMQSP(1)
      DO 3 I = 1, NSPO
        QS(I) = QSPEC(I,1)
        QSP(I) = QSPECP(I,1)
    3 CONTINUE
      RETURN
      END

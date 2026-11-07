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

C File: tests/fortran-reference/harnesses/inverse-design/
C qdes_kernels_harness.f
C     Focused Fortran harness for the QDES kernels of the inverse core:
C     SPLQSP, SMOOQ, QINCOM, GAMQSP, SYMQSP.
C
C     Calls only the canonical routines from fortran-src/xqdes.f
C     (extracted byte-faithfully). Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM QDESKERN
      INCLUDE 'XFOIL.INC'
      REAL QC, QINF, TKLAM, QI
      NSP = 21
      NQSP = 1
      KQSP = 1
      LQSLOP = .FALSE.
      DO 1 I = 1, NSP
        SSPEC(I) = FLOAT(I-1) / FLOAT(NSP-1)
        QSPEC(I,KQSP) = 1.0 - 0.5*SSPEC(I)
    1 CONTINUE
      CALL SPLQSP(KQSP)
      DO 10 I = 1, NSP, 4
        WRITE(*,1000) I, QSPEC(I,KQSP), QSPECP(I,KQSP)
1000    FORMAT('SPL',I4,2ES22.12)
   10 CONTINUE
      DO 2 I = 1, NSP
        QSPEC(I,KQSP) = 1.0 - 0.5*SSPEC(I)
    2 CONTINUE
      CALL SPLQSP(KQSP)
      CALL SMOOQ(2, NSP-1, KQSP)
      DO 20 I = 1, NSP, 4
        WRITE(*,1010) I, QSPEC(I,KQSP)
1010    FORMAT('SMQ',I4,ES22.12)
   20 CONTINUE
      DO 3 I = 1, NSP
        QSPEC(I,KQSP) = 1.0 - 0.5*SSPEC(I)
    3 CONTINUE
      LQSLOP = .TRUE.
      CALL SPLQSP(KQSP)
      CALL SMOOQ(3, NSP-2, KQSP)
      DO 30 I = 1, NSP, 4
        WRITE(*,1010) I, QSPEC(I,KQSP)
   30 CONTINUE
      LQSLOP = .FALSE.
      QI = QINCOM(1.0, 1.0, 0.0)
      WRITE(*,1020) QI
1020    FORMAT('QIC',ES22.12)
      QI = QINCOM(1.2, 1.0, 0.3)
      WRITE(*,1020) QI
      QI = QINCOM(0.0, 1.0, 0.5)
      WRITE(*,1020) QI
      DO 4 I = 1, NSP
        QGAMM(I) = COS(3.1415926535898*SSPEC(I))
    4 CONTINUE
      ALGAM = 2.0
      CLGAM = 0.5
      CMGAM = -0.05
      LIQSET = .FALSE.
      CALL GAMQSP(KQSP)
      WRITE(*,1030) ALQSP(KQSP), CLQSP(KQSP), CMQSP(KQSP), IQ1, IQ2
1030    FORMAT('GMH',3ES22.12,2I4)
      DO 40 I = 1, NSP, 4
        WRITE(*,1010) I, QSPEC(I,KQSP)
   40 CONTINUE
      DO 5 I = 1, NSP
        SSPEC(I) = FLOAT(I-1) / FLOAT(NSP-1)
        QSPEC(I,KQSP) = 1.0 + 0.3*SSPEC(I) - 0.1*SSPEC(I)**2
    5 CONTINUE
      CALL SYMQSP(KQSP)
      DO 50 I = 1, NSP, 4
        WRITE(*,1040) I, SSPEC(I), QSPEC(I,KQSP), QSPECP(I,KQSP)
1040    FORMAT('SYQ',I4,3ES22.12)
   50 CONTINUE
      END

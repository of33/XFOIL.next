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

C CPI harness: canonical CPI producer chain (CPCALC from
C SPECAL) with complete CPI output (It24.16) for the
C bit-level and provenance verification of the session-bound CPI state.
C Calls the canonical routines unchanged (NACA4/PANGEN/GGCALC/
C SPECAL/CPCALC); the harness link uses byte-faithfully extracted
C routine blocks (see tools/build-cpi-harnesses.sh).
C
C Scenarios:
C  A: NACA 2412, inviscid, ALFA = 0   (MINF1 = 0)
C  B: NACA 2412, inviscid, ALFA = 2   (MINF1 = 0)
C  C: NACA 2412, Mach 0.3, Alfa = 2   (MINF1 = 0.3, Mach-dependent CPI)
C
C Output per scenario: N, X(I),Y(I) (panel coordinates), Alfa, MINF,
C QINV(I) (direct CPI inputs), CPI(I) complete in It24.16.
C
C Important: fixed form; lines remain <= column 72.
      PROGRAM CPI_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE
      REAL ALFR
      CHARACTER*80 CPMARK
C
C ==== COMMON initialization (as xfoil.f INIT) ====
      PI = 4.0*ATAN(1.0)
      HOPI = 0.50/PI
      QOPI = 0.25/PI
      GAMMA = 1.4
      GAMM1 = GAMMA - 1.0
      XCMREF = 0.25
      YCMREF = 0.0
      WAKLEN = 1.0
      LIMAGE = .FALSE.
      SHARP = .FALSE.
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      XSREF1 = 1.0
      XSREF2 = 1.0
      XPREF1 = 1.0
      XPREF2 = 1.0
      QINF = 1.0
      MINF = 0.0
      ALFA = 0.0
      N = 0
      NW = 0
      DO 1 I=1,IQX
        GAM(I) = 0.0
        GAM_A(I) = 0.0
        SIG(I) = 0.0
        QINV(I) = 0.0
        QINV_A(I) = 0.0
        QINVU(I,1) = 0.0
        QINVU(I,2) = 0.0
        CPI(I) = 0.0
        CPV(I) = 0.0
    1 CONTINUE
      GAMU(N+1,1) = 0.0
      GAMU(N+1,2) = 0.0
C
C ==== Geometry + paneling + base solution (NACA 2412) ====
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      MINF1 = 0.0
      REINF1 = 3.0E6
      MATYP = 1
      RETYP = 1
      CALL GGCALC
C
C ==== Scenario A: ALFA = 0, MINF1 = 0 ====
      ALFA = 0.0
      CALL SPECAL
      CPMARK = 'A'
      GOTO 90
C
C ==== Scenario B: Alfa = 2 Degrees, MINF1 = 0 ====
   80 CONTINUE
      ALFR = 2.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      CPMARK = 'B'
      GOTO 90
C
C ==== Scenario C: Alfa = 2 Degrees, MINF1 = 0.3 (Mach dependency) ====
   85 CONTINUE
      MINF1 = 0.3
      CALL SPECAL
      CPMARK = 'C'
C
C ==== Dump: panel coordinates, operating state, QINV, CPI ====
   90 CONTINUE
      WRITE(*,9100) 'CPI ',CPMARK,' N = ', N
      DO 91 I=1,N
        WRITE(*,9200) 'CPI ',CPMARK,' X = ', X(I)
   91 CONTINUE
      DO 92 I=1,N
        WRITE(*,9200) 'CPI ',CPMARK,' Y = ', Y(I)
   92 CONTINUE
      WRITE(*,9200) 'CPI ',CPMARK,' ALFA = ', ALFA
      WRITE(*,9200) 'CPI ',CPMARK,' MINF = ', MINF
      DO 93 I=1,N
        WRITE(*,9200) 'CPI ',CPMARK,' QINV = ', QINV(I)
   93 CONTINUE
      DO 94 I=1,N
        WRITE(*,9200) 'CPI ',CPMARK,' CPI = ', CPI(I)
   94 CONTINUE
      IF(CPMARK.EQ.'A') GOTO 80
      IF(CPMARK.EQ.'B') GOTO 85
      STOP
 9100 FORMAT(A4,A1,A5,I5)
 9200 FORMAT(A4,A1,A7,ES24.16)
      END

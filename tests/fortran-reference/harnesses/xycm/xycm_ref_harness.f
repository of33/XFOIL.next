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

C Fortran harness for the XYCM moment reference transformation.
C Calls the canonical routines NACA4/PANGEN/TECALC/GGCALC/SPECAL
C (xfoil.f,
C xpanel.f, xoper.f) unchanged and then calls CLCALC (xfoil.f)
C with
C with explicit XREF/YREF on the same force state (GAM/GAM_A from
C SPECAL).
C This carries the harness the complete effect of XCMREF/YCMREF on CM
C and returns the values needed for the analytic transformation;
C CLCALC outputs Cl and CDP.
C
C Canonical build flags: -fdefault-real-8 -fdefault-double-8 (It24.16).
C Output deterministic; lines remain <= column 72.
      PROGRAM XYCM_REF_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE
      REAL ALFR
      REAL CL_ALF, CL_MSQ
      REAL XR, YR
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
C ==== NACA 2412 + PANGEN + GGCALC + SPECAL for Alfa = 4 Degrees ====
      MATYP = 1
      RETYP = 1
      MINF1 = 0.0
      REINF1 = 3.0E6
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      CALL GGCALC
      ALFR = 4.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      CALL TECALC
      WRITE(*,9200) 'ALFA = ', ALFA
      WRITE(*,9200) 'MINF = ', MINF
      WRITE(*,9200) 'QINF = ', QINF
      WRITE(*,9100) 'N = ', N
C
C ==== CLCALC on the same force state for several references ====
      DO 20 K=1,4
        IF(K.EQ.1) THEN
          XR = 0.0 ; YR = 0.0
        ELSEIF(K.EQ.2) THEN
          XR = 0.25 ; YR = 0.0
        ELSEIF(K.EQ.3) THEN
          XR = 0.25 ; YR = 0.1
        ELSE
          XR = -0.5 ; YR = 0.1
        ENDIF
        CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF,XR,YR,
     &              CL,CM,CDP,CL_ALF,CL_MSQ)
        WRITE(*,9200) 'XR = ', XR
        WRITE(*,9200) 'YR = ', YR
        WRITE(*,9200) 'CL = ', CL
        WRITE(*,9200) 'CDP = ', CDP
        WRITE(*,9200) 'CM = ', CM
   20 CONTINUE
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
      END

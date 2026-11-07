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

C Fortran-harness for xfoil::core::geometry - INTER/INTERX/IJSECT/
C AREFINE/SCHECK/HALF (xgeom.f). Calls the canonical routines
C unchanged. Fixed form; lines remain <= column 72.
      PROGRAM INTERP_HARNESS
      INTEGER NSIDE, NB0, NB1, IDES, N, I, NNEW, NDIM
      REAL W1(123), W2(123), W3(123)
      REAL XB(250), YB(250), SB(250), XBP(250), YBP(250)
      REAL XC(250), YC2(250), SC(250), XCP(250), YCP(250)
      REAL XD(250), YD(250)
      REAL XNEW(500), YNEW(500)
      REAL SLE0, SLE1, FRAC
      REAL PEX, AREA, SLEN, XMIN, XMAX, YMIN, YMAX
      REAL XCC, YCC, XCT, YCT, AIXX, AIYY, AIXXT, AIYYT, AJ, AJT
      REAL ATOL, X1, X2, STOL
      REAL XQ(50), YQ(50)
      CHARACTER*48 NAME
      LOGICAL LCHANGE
C
C ==== Airfoils 0: NACA 0012 splined, Airfoils 1: NACA 2412 splined ====
      NSIDE = 123
      IDES = 0012
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB0,NAME)
      CALL SCALC(XB,YB,SB,NB0)
      CALL SEGSPL(XB,XBP,SB,NB0)
      CALL SEGSPL(YB,YBP,SB,NB0)
      CALL LEFIND(SLE0,XB,XBP,YB,YBP,SB,NB0)
C
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XC,YC2,NB1,NAME)
      CALL SCALC(XC,YC2,SC,NB1)
      CALL SEGSPL(XC,XCP,SC,NB1)
      CALL SEGSPL(YC2,YCP,SC,NB1)
      CALL LEFIND(SLE1,XC,XCP,YC2,YCP,SC,NB1)
      WRITE(*,9100) 'Profile NB0 = ', NB0
      WRITE(*,9100) 'Profile NB1 = ', NB1
C
C ==== INTER FRAC=0.5 ====
      FRAC = 0.5
      CALL INTER(XB,XBP,YB,YBP,SB,NB0,SLE0,
     &           XC,XCP,YC2,YCP,SC,NB1,SLE1,
     &           XD,YD,N,FRAC)
      WRITE(*,9100) 'INTER0012-2412 FRAC=0.5 N = ', N
      DO 10 I=1,N
        WRITE(*,9200) '  INTER X = ', XD(I)
        WRITE(*,9200) '  INTER Y = ', YD(I)
   10 CONTINUE
C
C ==== INTER FRAC=0.25 ====
      FRAC = 0.25
      CALL INTER(XB,XBP,YB,YBP,SB,NB0,SLE0,
     &           XC,XCP,YC2,YCP,SC,NB1,SLE1,
     &           XD,YD,N,FRAC)
      WRITE(*,9100) 'INTER0012-2412 FRAC=0.25 N = ', N
      DO 20 I=1,N
        WRITE(*,9200) '  INTER25 X = ', XD(I)
        WRITE(*,9200) '  INTER25 Y = ', YD(I)
   20 CONTINUE
C
C ==== INTERX FRAC=0.5 ====
      FRAC = 0.5
      CALL INTERX(XB,XBP,YB,YBP,SB,NB0,SLE0,
     &            XC,XCP,YC2,YCP,SC,NB1,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9100) 'INTERX0012-2412 FRAC=0.5 N = ', N
      DO 30 I=1,N
        WRITE(*,9200) '  INTERX X = ', XD(I)
        WRITE(*,9200) '  INTERX Y = ', YD(I)
   30 CONTINUE
C
C ==== IJSECT on NACA 0012 buffer ====
      PEX = 16.0
      CALL IJSECT(NB0,XB,YB, PEX,
     &  AREA, SLEN,
     &  XMIN, XMAX, XEXINT,
     &  YMIN, YMAX, YEXINT,
     &  XCC , YCC ,
     &  XCT, YCT,
     &  AIXX , AIYY ,
     &  AIXXT, AIYYT,
     &  AJ   , AJT    )
      WRITE(*,9200) 'IJSECT0012 AREA = ', AREA
      WRITE(*,9200) 'IJSECT0012 SLEN = ', SLEN
      WRITE(*,9200) 'IJSECT0012 XMIN = ', XMIN
      WRITE(*,9200) 'IJSECT0012 XMAX = ', XMAX
      WRITE(*,9200) 'IJSECT0012 YMIN = ', YMIN
      WRITE(*,9200) 'IJSECT0012 YMAX = ', YMAX
      WRITE(*,9200) 'IJSECT0012 XC = ', XCC
      WRITE(*,9200) 'IJSECT0012 YC = ', YCC
      WRITE(*,9200) 'IJSECT0012 XCT = ', XCT
      WRITE(*,9200) 'IJSECT0012 YCT = ', YCT
      WRITE(*,9200) 'IJSECT0012 AIXX = ', AIXX
      WRITE(*,9200) 'IJSECT0012 AIYY = ', AIYY
      WRITE(*,9200) 'IJSECT0012 AIXXT = ', AIXXT
      WRITE(*,9200) 'IJSECT0012 AIYYT = ', AIYYT
      WRITE(*,9200) 'IJSECT0012 AJ = ', AJ
      WRITE(*,9200) 'IJSECT0012 AJT = ', AJT
C
C ==== AREFINE on NACA 2412, NSIDE=61 ====
      NSIDE = 61
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB0,NAME)
      CALL SCALC(XB,YB,SB,NB0)
      CALL SEGSPL(XB,XBP,SB,NB0)
      CALL SEGSPL(YB,YBP,SB,NB0)
      ATOL = 5.0
      NDIM = 500
      X1 = -1.0
      X2 = 2.0
      CALL AREFINE(XB,YB,SB,XBP,YBP,NB0, ATOL,
     &             NDIM,NNEW,XNEW,YNEW, X1,X2)
      WRITE(*,9100) 'AREFINE2412-61 ATOL=5 NNEW = ', NNEW
      DO 40 I=1,NNEW
        WRITE(*,9200) '  AREFINE X = ', XNEW(I)
        WRITE(*,9200) '  AREFINE Y = ', YNEW(I)
   40 CONTINUE
C
C ==== AREFINE Overflow (NDIM=10) ====
      CALL AREFINE(XB,YB,SB,XBP,YBP,NB0, ATOL,
     &             10,NNEW,XNEW,YNEW, X1,X2)
      WRITE(*,9100) 'AREFINE overflow NDIM=10 NNEW = ', NNEW
C
C ==== SCHECK on a constructed point set with a micro segment ====
      N = 8
      XQ(1) = 0.0
      XQ(2) = 0.4
      XQ(3) = 0.401
      XQ(4) = 0.8
      XQ(5) = 1.0
      XQ(6) = 0.8
      XQ(7) = 0.4
      XQ(8) = 0.0
      YQ(1) = 0.0
      YQ(2) = 0.05
      YQ(3) = 0.055
      YQ(4) = 0.02
      YQ(5) = 0.0
      YQ(6) = -0.02
      YQ(7) = -0.04
      YQ(8) = -0.03
      STOL = 0.05
      CALL SCHECK(XQ,YQ,N, STOL, LCHANGE)
      WRITE(*,9100) 'SCHECK N = ', N
      IF(LCHANGE) WRITE(*,*) 'SCHECK LCHANGE = T'
      IF(.NOT.LCHANGE) WRITE(*,*) 'SCHECK LCHANGE = F'
      DO 50 I=1,N
        WRITE(*,9200) '  SCHECK X = ', XQ(I)
        WRITE(*,9200) '  SCHECK Y = ', YQ(I)
   50 CONTINUE
C
C ==== SCHECK with STOL > 0.3 (no change) ====
      N = 8
      XQ(1) = 0.0
      XQ(2) = 0.4
      XQ(3) = 0.401
      XQ(4) = 0.8
      XQ(5) = 1.0
      XQ(6) = 0.8
      XQ(7) = 0.4
      XQ(8) = 0.0
      YQ(1) = 0.0
      YQ(2) = 0.05
      YQ(3) = 0.055
      YQ(4) = 0.02
      YQ(5) = 0.0
      YQ(6) = -0.02
      YQ(7) = -0.04
      YQ(8) = -0.03
      STOL = 0.5
      CALL SCHECK(XQ,YQ,N, STOL, LCHANGE)
      WRITE(*,9100) 'SCHECK bad STOL N = ', N
      IF(LCHANGE) WRITE(*,*) 'SCHECK bad STOL LCHANGE = T'
      IF(.NOT.LCHANGE) WRITE(*,*) 'SCHECK bad STOL LCHANGE = F'
C
C ==== HALF on NACA 2412 buffer ====
      NSIDE = 123
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB0,NAME)
      CALL SCALC(XB,YB,SB,NB0)
      CALL HALF(XB,YB,SB,NB0)
      WRITE(*,9100) 'HALF2412 NNEW = ', NB0
      DO 60 I=1,NB0
        WRITE(*,9200) '  HALF X = ', XB(I)
        WRITE(*,9200) '  HALF Y = ', YB(I)
   60 CONTINUE
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
      END

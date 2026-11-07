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

C Fortran harness for the canonical INTERX and HALF
C routines (xgeom.f) and the required semantics for the INTERX/HALF
C cases. Calls the canonical routines unchanged.
C Canonical build with -fdefault-real-8 -fdefault-double-8
C (unqualified REAL = REAL(8) = C++ double).
C Fixed form; lines remain <= column 72.
      PROGRAM INTX_HALF_HARNESS
      INTEGER NSIDE, NB0, NB1, N, I, NIN, NNEW
      REAL W1(123), W2(123), W3(123)
      REAL XA(250), YA(250), SA(250), XPA(250), YPA(250)
      REAL XB(250), YB(250), SB(250), XPB(250), YPB(250)
      REAL XD(250), YD(250)
      REAL XT(250), YT(250), ST(250), XPT(250), YPT(250)
      REAL SLE0, SLE1, FRAC, XLE0, XLE1
      CHARACTER*48 NAME
C
C==== INTERX-sources prepare ==============================
C
C---- Source A: NACA 0012, NSIDE=123
      NSIDE = 123
      CALL NACA4(0012,W1,W2,W3,NSIDE,XA,YA,NB0,NAME)
      CALL SCALC(XA,YA,SA,NB0)
      CALL SEGSPLD(XA,XPA,SA,NB0,-999.,-999.)
      CALL SEGSPLD(YA,YPA,SA,NB0,-999.,-999.)
      CALL LEFIND(SLE0,XA,XPA,YA,YPA,SA,NB0)
C
C---- Source B: NACA 2412, NSIDE=123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB1,NAME)
      CALL SCALC(XB,YB,SB,NB1)
      CALL SEGSPLD(XB,XPB,SB,NB1,-999.,-999.)
      CALL SEGSPLD(YB,YPB,SB,NB1,-999.,-999.)
      CALL LEFIND(SLE1,XB,XPB,YB,YPB,SB,NB1)
      WRITE(*,9100) 'IX SLE0 = ', SLE0
      WRITE(*,9100) 'IX SLE1 = ', SLE1
      WRITE(*,9100) 'IX XLE0 = ', SEVAL(SLE0,XA,XPA,SA,NB0)
      WRITE(*,9100) 'IX XLE1 = ', SEVAL(SLE1,XB,XPB,SB,NB1)
C
C---- A: identical Airfoils (0012/0012), FRAC=0
      FRAC = 0.0
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX A N = ', N
      DO 10 I=1,N
        WRITE(*,9300) 'IX A X = ', XD(I)
        WRITE(*,9300) 'IX A Y = ', YD(I)
   10 CONTINUE
C
C---- B: identical Airfoils, FRAC=1
      FRAC = 1.0
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX B N = ', N
      DO 20 I=1,N
        WRITE(*,9300) 'IX B X = ', XD(I)
        WRITE(*,9300) 'IX B Y = ', YD(I)
   20 CONTINUE
C
C---- C: identical Airfoils, FRAC=0.5
      FRAC = 0.5
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX C N = ', N
      DO 30 I=1,N
        WRITE(*,9300) 'IX C X = ', XD(I)
        WRITE(*,9300) 'IX C Y = ', YD(I)
   30 CONTINUE
C
C---- D: 0012/2412, FRAC=0.5
      FRAC = 0.5
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XB,XPB,YB,YPB,SB,NB1,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX D N = ', N
      DO 40 I=1,N
        WRITE(*,9300) 'IX D X = ', XD(I)
        WRITE(*,9300) 'IX D Y = ', YD(I)
   40 CONTINUE
C
C---- E: different point counts (0012 NSIDE=123,
C         2412 NSIDE=61), FRAC=0.5
      NSIDE = 61
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB1,NAME)
      CALL SCALC(XB,YB,SB,NB1)
      CALL SEGSPLD(XB,XPB,SB,NB1,-999.,-999.)
      CALL SEGSPLD(YB,YPB,SB,NB1,-999.,-999.)
      CALL LEFIND(SLE1,XB,XPB,YB,YPB,SB,NB1)
      FRAC = 0.5
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XB,XPB,YB,YPB,SB,NB1,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX E N = ', N
      DO 50 I=1,N
        WRITE(*,9300) 'IX E X = ', XD(I)
        WRITE(*,9300) 'IX E Y = ', YD(I)
   50 CONTINUE
C
C---- F: Extrapolation FRAC=-0.25 (0012/2412 NSIDE=123)
      NSIDE = 123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB1,NAME)
      CALL SCALC(XB,YB,SB,NB1)
      CALL SEGSPLD(XB,XPB,SB,NB1,-999.,-999.)
      CALL SEGSPLD(YB,YPB,SB,NB1,-999.,-999.)
      CALL LEFIND(SLE1,XB,XPB,YB,YPB,SB,NB1)
      FRAC = -0.25
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XB,XPB,YB,YPB,SB,NB1,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX F N = ', N
      DO 60 I=1,N
        WRITE(*,9300) 'IX F X = ', XD(I)
        WRITE(*,9300) 'IX F Y = ', YD(I)
   60 CONTINUE
C
C---- G: Extrapolation FRAC=1.25
      FRAC = 1.25
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XB,XPB,YB,YPB,SB,NB1,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX G N = ', N
      DO 70 I=1,N
        WRITE(*,9300) 'IX G X = ', XD(I)
        WRITE(*,9300) 'IX G Y = ', YD(I)
   70 CONTINUE
C
C---- H: different LE position (0012/4412), FRAC=0.5
      CALL NACA4(4412,W1,W2,W3,NSIDE,XB,YB,NB1,NAME)
      CALL SCALC(XB,YB,SB,NB1)
      CALL SEGSPLD(XB,XPB,SB,NB1,-999.,-999.)
      CALL SEGSPLD(YB,YPB,SB,NB1,-999.,-999.)
      CALL LEFIND(SLE1,XB,XPB,YB,YPB,SB,NB1)
      WRITE(*,9100) 'IX H SLE1 = ', SLE1
      FRAC = 0.5
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XB,XPB,YB,YPB,SB,NB1,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX H N = ', N
      DO 80 I=1,N
        WRITE(*,9300) 'IX H X = ', XD(I)
        WRITE(*,9300) 'IX H Y = ', YD(I)
   80 CONTINUE
C
C---- I: corner point source with duplicated station (index marked),
C         FRAC=0.5 against NACA 0012
      NIN = 8
      XT(1) = 1.000
      XT(2) = 0.600
      XT(3) = 0.300
      XT(4) = 0.300
      XT(5) = 0.000
      XT(6) = 0.300
      XT(7) = 0.600
      XT(8) = 1.000
      YT(1) = 0.000
      YT(2) = 0.050
      YT(3) = 0.080
      YT(4) = 0.080
      YT(5) = 0.000
      YT(6) = -0.080
      YT(7) = -0.050
      YT(8) = 0.000
      CALL SCALC(XT,YT,ST,NIN)
      CALL SEGSPLD(XT,XPT,ST,NIN,-999.,-999.)
      CALL SEGSPLD(YT,YPT,ST,NIN,-999.,-999.)
      CALL LEFIND(SLE1,XT,XPT,YT,YPT,ST,NIN)
      WRITE(*,9100) 'IX I SLE1 = ', SLE1
      FRAC = 0.5
      CALL INTERX(XA,XPA,YA,YPA,SA,NB0,SLE0,
     &            XT,XPT,YT,YPT,ST,NIN,SLE1,
     &            XD,YD,N,FRAC)
      WRITE(*,9200) 'IX I N = ', N
      DO 90 I=1,N
        WRITE(*,9300) 'IX I X = ', XD(I)
        WRITE(*,9300) 'IX I Y = ', YD(I)
   90 CONTINUE
C
C==== HALF cases =============================================
C
C---- J: NACA 2412 NSIDE=123 (245 points) -> HALF
      NSIDE = 123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB0,NAME)
      CALL SCALC(XB,YB,SB,NB0)
      WRITE(*,9200) 'HF J NIN = ', NB0
      CALL HALF(XB,YB,SB,NB0)
      WRITE(*,9200) 'HF J N = ', NB0
      DO 100 I=1,NB0
        WRITE(*,9300) 'HF J X = ', XB(I)
        WRITE(*,9300) 'HF J Y = ', YB(I)
  100 CONTINUE
C
C---- K: second HALF (123 -> 62)
      CALL SCALC(XB,YB,SB,NB0)
      CALL HALF(XB,YB,SB,NB0)
      WRITE(*,9200) 'HF K N = ', NB0
      DO 110 I=1,NB0
        WRITE(*,9300) 'HF K X = ', XB(I)
        WRITE(*,9300) 'HF K Y = ', YB(I)
  110 CONTINUE
C
C---- L: third HALF on an even point count (62 -> 32)
      CALL SCALC(XB,YB,SB,NB0)
      CALL HALF(XB,YB,SB,NB0)
      WRITE(*,9200) 'HF L N = ', NB0
C
C---- M: odd point count: NACA 2412 NSIDE=61 (121) -> HALF
      NSIDE = 61
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB0,NAME)
      CALL SCALC(XB,YB,SB,NB0)
      WRITE(*,9200) 'HF M NIN = ', NB0
      CALL HALF(XB,YB,SB,NB0)
      WRITE(*,9200) 'HF M N = ', NB0
C
C---- N: index-marked contour with corner point pair (S(I)==S(I+1))
      NIN = 11
      XT(1) = 0.000
      XT(2) = 0.100
      XT(3) = 0.200
      XT(4) = 0.200
      XT(5) = 0.300
      XT(6) = 0.400
      XT(7) = 0.300
      XT(8) = 0.200
      XT(9) = 0.100
      XT(10) = 0.000
      XT(11) = 0.000
      YT(1) = 0.000
      YT(2) = 0.010
      YT(3) = 0.020
      YT(4) = 0.020
      YT(5) = 0.010
      YT(6) = 0.000
      YT(7) = -0.010
      YT(8) = -0.020
      YT(9) = -0.010
      YT(10) = 0.000
      YT(11) = 0.000
      CALL SCALC(XT,YT,ST,NIN)
      CALL HALF(XT,YT,ST,NIN)
      WRITE(*,9200) 'HF N N = ', NIN
      DO 120 I=1,NIN
        WRITE(*,9300) 'HF N X = ', XT(I)
        WRITE(*,9300) 'HF N Y = ', YT(I)
  120 CONTINUE
C
C---- O: several corner points (duplicate stations for 3/4 and 8/9)
      NIN = 12
      XT(1) = 0.000
      XT(2) = 0.100
      XT(3) = 0.200
      XT(4) = 0.200
      XT(5) = 0.300
      XT(6) = 0.400
      XT(7) = 0.500
      XT(8) = 0.400
      XT(9) = 0.400
      XT(10) = 0.300
      XT(11) = 0.200
      XT(12) = 0.100
      YT(1) = 0.000
      YT(2) = 0.010
      YT(3) = 0.020
      YT(4) = 0.020
      YT(5) = 0.015
      YT(6) = 0.005
      YT(7) = 0.000
      YT(8) = -0.005
      YT(9) = -0.005
      YT(10) = -0.015
      YT(11) = -0.020
      YT(12) = -0.010
      CALL SCALC(XT,YT,ST,NIN)
      CALL HALF(XT,YT,ST,NIN)
      WRITE(*,9200) 'HF O N = ', NIN
      DO 130 I=1,NIN
        WRITE(*,9300) 'HF O X = ', XT(I)
        WRITE(*,9300) 'HF O Y = ', YT(I)
  130 CONTINUE
C
C---- P: smallest contour: N=3 -> 2, then N=2 -> 2
      NIN = 3
      XT(1) = 0.000
      XT(2) = 0.500
      XT(3) = 1.000
      YT(1) = 0.000
      YT(2) = 0.100
      YT(3) = 0.000
      CALL SCALC(XT,YT,ST,NIN)
      CALL HALF(XT,YT,ST,NIN)
      WRITE(*,9200) 'HF P N = ', NIN
      DO 140 I=1,NIN
        WRITE(*,9300) 'HF P X = ', XT(I)
        WRITE(*,9300) 'HF P Y = ', YT(I)
  140 CONTINUE
      CALL SCALC(XT,YT,ST,NIN)
      CALL HALF(XT,YT,ST,NIN)
      WRITE(*,9200) 'HF P2 N = ', NIN
      DO 150 I=1,NIN
        WRITE(*,9300) 'HF P2 X = ', XT(I)
        WRITE(*,9300) 'HF P2 Y = ', YT(I)
  150 CONTINUE
C
C---- Q: repeated HALF until 2 (245-point chain)
      NSIDE = 123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB0,NAME)
      CALL SCALC(XB,YB,SB,NB0)
      DO 160 I=1, 8
        CALL HALF(XB,YB,SB,NB0)
        WRITE(*,9200) 'HF Q STEP N = ', NB0
        CALL SCALC(XB,YB,SB,NB0)
  160 CONTINUE
C
      STOP
 9100 FORMAT(A, E30.20)
 9200 FORMAT(A, I5)
 9300 FORMAT(A, E30.20)
      END

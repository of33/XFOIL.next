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

C Fortran-harness for xfoil::core::geometry - YSYM/LERSCL/SSS/CANG/CLIS
C (xgeom.f). Calls the canonical routines unchanged. Fixed form;
C Lines remain <= column 72.
      PROGRAM TRANSFORM_HARNESS
      INTEGER NSIDE, NB, IDES, N, I, IMAX
      REAL W1(123), W2(123), W3(123)
      REAL XB(250), YB(250), SB(250), XBP(250), YBP(250)
      REAL XNEW(500), YNEW(500)
      REAL SLE, S1, S2, SS, XBF, YBF, DEL, RFAC, DOC
      REAL AMAX
      CHARACTER*48 NAME
      LOGICAL LCHANGE
C
C ==== YSYM ISIDE=1 on NACA 2412 ====
      NSIDE = 123
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      N = NB
      CALL YSYM(XB,XBP,YB,YBP,SB, 250, N, 1, XNEW,YNEW)
      WRITE(*,9100) 'YSYM2412 ISIDE=1 NNEW = ', N
      DO 10 I=1,N
        WRITE(*,9200) '  YSYM X = ', XB(I)
        WRITE(*,9200) '  YSYM Y = ', YB(I)
   10 CONTINUE
C
C ==== YSYM ISIDE=2 on a fresh NACA 2412 buffer ====
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      N = NB
      CALL YSYM(XB,XBP,YB,YBP,SB, 250, N, 2, XNEW,YNEW)
      WRITE(*,9100) 'YSYM2412 ISIDE=2 NNEW = ', N
      DO 20 I=1,N
        WRITE(*,9200) '  YSYM2 X = ', XB(I)
        WRITE(*,9200) '  YSYM2 Y = ', YB(I)
   20 CONTINUE
C
C ==== LERSCL on a fresh NACA 2412 buffer ====
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      DOC = 0.1
      RFAC = 2.0
      CALL LERSCL(XB,XBP,YB,YBP,SB,NB, DOC,RFAC, XNEW,YNEW)
      WRITE(*,9100) 'LERSCL2412 DOC=0.1 RFAC=2.0 NB = ', NB
      DO 30 I=1,NB
        WRITE(*,9200) '  LERSCL X = ', XNEW(I)
        WRITE(*,9200) '  LERSCL Y = ', YNEW(I)
   30 CONTINUE
C
C ==== SSS on a fresh NACA 2412 buffer ====
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      SS = SB(20)
      XBF = 0.7
      YBF = 0.0
      DEL = 0.1
      CALL SSS(SS,S1,S2,DEL,XBF,YBF,XB,XBP,YB,YBP,SB,NB, 1)
      WRITE(*,9200) 'SSS2412 DEL=0.1 ISIDE=1 S1 = ', S1
      WRITE(*,9200) 'SSS2412 DEL=0.1 ISIDE=1 S2 = ', S2
      CALL SSS(SS,S1,S2,DEL,XBF,YBF,XB,XBP,YB,YBP,SB,NB, 2)
      WRITE(*,9200) 'SSS2412 DEL=0.1 ISIDE=2 S1 = ', S1
      WRITE(*,9200) 'SSS2412 DEL=0.1 ISIDE=2 S2 = ', S2
      DEL = 0.0
      CALL SSS(SS,S1,S2,DEL,XBF,YBF,XB,XBP,YB,YBP,SB,NB, 1)
      WRITE(*,9200) 'SSS2412 DEL=0.0 ISIDE=1 S1 = ', S1
      WRITE(*,9200) 'SSS2412 DEL=0.0 ISIDE=1 S2 = ', S2
C
C ==== CANG on NACA 0012 buffer ====
      IDES = 0012
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL CANG(XB,YB,NB, 2, IMAX,AMAX)
      WRITE(*,9100) 'CANG0012 IPRINT=2 IMAX = ', IMAX
      WRITE(*,9200) 'CANG0012 IPRINT=2 AMAX = ', AMAX
      CALL CANG(XB,YB,NB, 0, IMAX,AMAX)
      WRITE(*,9100) 'CANG0012 IPRINT=0 IMAX = ', IMAX
      WRITE(*,9200) 'CANG0012 IPRINT=0 AMAX = ', AMAX
C
C ==== CLIS on NACA 0012 buffer ====
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL CLIS(XB,XBP,YB,YBP,SB,NB)
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
      END

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

C Fortran harness for xfoil::core::geometry - LE pipeline (xgeom.f).
C SCALC/SEGSPL/LEFIND/XLFIND/NSFIND/SOPPS/SEVAL/DEVAL/D2VAL/CURV on
C NACA 0012 and 2412 buffer contours. Calls the canonical routines
C unchanged (spline.f, xgeom.f). Fixed form; lines stay within column 72.
      PROGRAM LEPIPE_HARNESS
      INTEGER NSIDE, NB, IDES, I
      REAL W1(123), W2(123), W3(123)
      REAL XB(250), YB(250), SB(250), XBP(250), YBP(250)
      REAL SBLE, SOPP, SS
      CHARACTER*48 NAME
C
C ==== NACA 0012 buffer: SCALC/SEGSPL intermediate values ====
      NSIDE = 123
      IDES = 0012
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      WRITE(*,9100) 'NACA0012 LE-Pipeline NB = ', NB
      DO 10 I=1,NB
        WRITE(*,9200) '  SB = ', SB(I)
   10 CONTINUE
      DO 20 I=1,NB
        WRITE(*,9200) '  XBP = ', XBP(I)
   20 CONTINUE
      DO 30 I=1,NB
        WRITE(*,9200) '  YBP = ', YBP(I)
   30 CONTINUE
C
      CALL LEFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9200) 'LEFIND 0012 SBLE = ', SBLE
      CALL XLFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9200) 'XLFIND 0012 SBLE = ', SBLE
      CALL NSFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9200) 'NSFIND 0012 SBLE = ', SBLE
      CALL SOPPS(SOPP, SB(1), XB,XBP,YB,YBP,SB,NB, SBLE)
      WRITE(*,9200) 'SOPPS 0012 SI=SB(1) SOPP = ', SOPP
      CALL SOPPS(SOPP, SB(NB/2), XB,XBP,YB,YBP,SB,NB, SBLE)
      WRITE(*,9200) 'SOPPS 0012 SI=SB(NB/2) SOPP = ', SOPP
C
      SS = SB(1)
      WRITE(*,9200) 'SEVAL0012 X@S(1) = ', SEVAL(SS,XB,XBP,SB,NB)
      WRITE(*,9200) 'SEVAL0012 Y@S(1) = ', SEVAL(SS,YB,YBP,SB,NB)
      SS = SB(NB/2)
      WRITE(*,9200) 'SEVAL0012 X@S(NB/2) = ', SEVAL(SS,XB,XBP,SB,NB)
      WRITE(*,9200) 'SEVAL0012 Y@S(NB/2) = ', SEVAL(SS,YB,YBP,SB,NB)
      SS = SB(NB)
      WRITE(*,9200) 'SEVAL0012 X@S(NB) = ', SEVAL(SS,XB,XBP,SB,NB)
      WRITE(*,9200) 'SEVAL0012 Y@S(NB) = ', SEVAL(SS,YB,YBP,SB,NB)
      WRITE(*,9200) 'SEVAL0012 X@SLE = ', SEVAL(SBLE,XB,XBP,SB,NB)
      WRITE(*,9200) 'SEVAL0012 Y@SLE = ', SEVAL(SBLE,YB,YBP,SB,NB)
      WRITE(*,9200) 'DEVAL0012 X@SLE = ', DEVAL(SBLE,XB,XBP,SB,NB)
      WRITE(*,9200) 'DEVAL0012 Y@SLE = ', DEVAL(SBLE,YB,YBP,SB,NB)
      WRITE(*,9200) 'D2VAL0012 X@SLE = ', D2VAL(SBLE,XB,XBP,SB,NB)
      WRITE(*,9200) 'D2VAL0012 Y@SLE = ', D2VAL(SBLE,YB,YBP,SB,NB)
      WRITE(*,9200) 'CURV0012 @SLE = ', CURV(SBLE,XB,XBP,YB,YBP,SB,NB)
C
C ==== NACA 2412 buffer: same pipeline ====
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      WRITE(*,9100) 'NACA2412 LE-Pipeline NB = ', NB
      DO 40 I=1,NB
        WRITE(*,9200) '  SB = ', SB(I)
   40 CONTINUE
      DO 50 I=1,NB
        WRITE(*,9200) '  XBP = ', XBP(I)
   50 CONTINUE
      DO 60 I=1,NB
        WRITE(*,9200) '  YBP = ', YBP(I)
   60 CONTINUE
C
      CALL LEFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9200) 'LEFIND 2412 SBLE = ', SBLE
      CALL XLFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9200) 'XLFIND 2412 SBLE = ', SBLE
      CALL NSFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9200) 'NSFIND 2412 SBLE = ', SBLE
      CALL SOPPS(SOPP, SB(1), XB,XBP,YB,YBP,SB,NB, SBLE)
      WRITE(*,9200) 'SOPPS 2412 SI=SB(1) SOPP = ', SOPP
      CALL SOPPS(SOPP, SB(NB/2), XB,XBP,YB,YBP,SB,NB, SBLE)
      WRITE(*,9200) 'SOPPS 2412 SI=SB(NB/2) SOPP = ', SOPP
C
      WRITE(*,9200) 'SEVAL2412 X@SLE = ', SEVAL(SBLE,XB,XBP,SB,NB)
      WRITE(*,9200) 'SEVAL2412 Y@SLE = ', SEVAL(SBLE,YB,YBP,SB,NB)
      SS = SB(NB/2)
      WRITE(*,9200) 'SEVAL2412 X@S(NB/2) = ', SEVAL(SS,XB,XBP,SB,NB)
      WRITE(*,9200) 'SEVAL2412 Y@S(NB/2) = ', SEVAL(SS,YB,YBP,SB,NB)
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
      END

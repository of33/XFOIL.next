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

C Fortran-harness for xfoil::core::geometry - NACA4/NACA5 (naca.f).
C Calls the canonical routines unchanged. Prints all outputs with
C high visible precision (Fortran REAL = single precision).
C Important: fixed form; lines stay <= column 72.
C NSIDE=123 corresponds to xfoil.f NACA (NSIDE=IQX/3) -> NB=245
C buffer points.
      PROGRAM NACA_HARNESS
      INTEGER NSIDE, NB, IDES, I
      REAL W1(123), W2(123), W3(123)
      REAL XX(123), YT(123), YC(123)
      REAL XB(250), YB(250)
      CHARACTER*48 NAME
C
C ==== NACA4 0012, NSIDE=123 ====
      NSIDE = 123
      IDES = 0012
      CALL NACA4(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      WRITE(*,9100) 'NACA4 0012 NSIDE=123 NB = ', NB
      WRITE(*,9110) 'NAME=|'//NAME(1:9)//'|'
      DO 10 I=1,NB
        WRITE(*,9200) '  XB = ', XB(I)
        WRITE(*,9200) '  YB = ', YB(I)
   10 CONTINUE
C
C ==== NACA4 2412, NSIDE=123 (incl. side arrays) ====
      IDES = 2412
      CALL NACA4(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      WRITE(*,9100) 'NACA4 2412 NSIDE=123 NB = ', NB
      WRITE(*,9110) 'NAME=|'//NAME(1:9)//'|'
      DO 20 I=1,NSIDE
        WRITE(*,9200) '  XX = ', XX(I)
        WRITE(*,9200) '  YT = ', YT(I)
        WRITE(*,9200) '  YC = ', YC(I)
   20 CONTINUE
      DO 30 I=1,NB
        WRITE(*,9200) '  XB = ', XB(I)
        WRITE(*,9200) '  YB = ', YB(I)
   30 CONTINUE
C
C ==== NACA5 23012, NSIDE=123 ====
      IDES = 23012
      CALL NACA5(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      WRITE(*,9100) 'NACA5 23012 NSIDE=123 NB = ', NB
      WRITE(*,9110) 'NAME=|'//NAME(1:10)//'|'
      DO 40 I=1,NB
        WRITE(*,9200) '  XB = ', XB(I)
        WRITE(*,9200) '  YB = ', YB(I)
   40 CONTINUE
C
C ==== NACA4 1234, NSIDE=61 ====
      NSIDE = 61
      IDES = 1234
      CALL NACA4(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      WRITE(*,9100) 'NACA4 1234 NSIDE=61 NB = ', NB
      WRITE(*,9110) 'NAME=|'//NAME(1:9)//'|'
      DO 50 I=1,NB
        WRITE(*,9200) '  XB = ', XB(I)
        WRITE(*,9200) '  YB = ', YB(I)
   50 CONTINUE
C
C ==== NACA5 24012, NSIDE=61 ====
      IDES = 24012
      CALL NACA5(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      WRITE(*,9100) 'NACA5 24012 NSIDE=61 NB = ', NB
      WRITE(*,9110) 'NAME=|'//NAME(1:10)//'|'
      DO 60 I=1,NB
        WRITE(*,9200) '  XB = ', XB(I)
        WRITE(*,9200) '  YB = ', YB(I)
   60 CONTINUE
C
C ==== NACA5 illegal identifier 2110 ====
      NSIDE = 123
      IDES = 2110
      CALL NACA5(IDES,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      WRITE(*,9100) 'NACA5 2110 IDES nach Aufruf = ', IDES
C
      STOP
 9100 FORMAT(A, I5)
 9110 FORMAT(A)
 9200 FORMAT(A, E30.20)
      END

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

C Fortran-harness for xfoil::core::geometry_design - HIPNT (xtcam.f).
C Calls the canonical routine unchanged and uses the COMMON-
C Buffer state (CR14) as CAMB/GDES. Fixed form; lines <= column 72.
      PROGRAM HIPNT_HARNESS
      INCLUDE 'XFOIL.INC'
      DIMENSION RINPUT(20)
      REAL XX(123), YT(123), YC(123)
      INTEGER NSIDE
C
C---- COMMON-constants as in MAIN set (CR11)
      PI = ACOS(0.0)*2.0
      HOPI = 0.5/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
C
      NSIDE = 123
      CALL NACA4(2412,XX,YT,YC,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
C
C==== HIPNT(0.4, 0.35) ====
      RINPUT(1) = 0.4
      RINPUT(2) = 0.35
      CALL HIPNT(RINPUT,2)
      WRITE(*,9000) 'HIP1 NB = ', NB
      WRITE(*,9100) 'HIP1 SBLE = ', SBLE
      WRITE(*,9100) 'HIP1 CHORDB = ', CHORDB
      WRITE(*,9100) 'HIP1 THICKB = ', THICKB
      WRITE(*,9100) 'HIP1 CAMBRB = ', CAMBRB
      WRITE(*,9200) 'HIP1 XB1 = ', XB(1)
      WRITE(*,9200) 'HIP1 YB1 = ', YB(1)
      WRITE(*,9200) 'HIP1 XB62 = ', XB(62)
      WRITE(*,9200) 'HIP1 YB62 = ', YB(62)
      WRITE(*,9200) 'HIP1 XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'HIP1 YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'HIP1 XBN = ', XB(NB)
      WRITE(*,9200) 'HIP1 YBN = ', YB(NB)
C
C==== HIPNT(0.0, 0.0): keep maximum locations ====
      RINPUT(1) = 0.0
      RINPUT(2) = 0.0
      CALL HIPNT(RINPUT,2)
      WRITE(*,9000) 'HIP2 NB = ', NB
      WRITE(*,9100) 'HIP2 SBLE = ', SBLE
      WRITE(*,9100) 'HIP2 CHORDB = ', CHORDB
      WRITE(*,9100) 'HIP2 THICKB = ', THICKB
      WRITE(*,9100) 'HIP2 CAMBRB = ', CAMBRB
      WRITE(*,9200) 'HIP2 XB1 = ', XB(1)
      WRITE(*,9200) 'HIP2 YB1 = ', YB(1)
      WRITE(*,9200) 'HIP2 XB62 = ', XB(62)
      WRITE(*,9200) 'HIP2 YB62 = ', YB(62)
      WRITE(*,9200) 'HIP2 XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'HIP2 YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'HIP2 XBN = ', XB(NB)
      WRITE(*,9200) 'HIP2 YBN = ', YB(NB)
C
 9000 FORMAT(A,I6)
 9100 FORMAT(A,E15.7)
 9200 FORMAT(A,E15.7)
      END

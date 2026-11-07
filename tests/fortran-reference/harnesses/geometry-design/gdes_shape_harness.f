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

C Fortran harness for xfoil::core::geometry_design - TGAP/LERAD/SCLXY/
C FLAP/INSIDE/GETXYF (xgdes.f). Calls the canonical routines
C unchanged (plot calls via plot_stubs). Cases:
C gdes-flap (FLAP on NACA 2412) and gdes-flap-exec (ABCOPY afterwards).
C Fixed form; lines <= column 72.
      PROGRAM GDES_SHAPE_HARNESS
      INCLUDE 'XFOIL.INC'
      REAL XT(123), YT(123), YC(123)
      REAL XORG, YORG, SCL, RADIUS, CVMAX, CV
      REAL XBF, YBF, DDEF
      LOGICAL INSID
      DIMENSION RINPUT(20)
      INTEGER NSIDE
C
      PI = ACOS(0.0)*2.0
      HOPI = 0.5/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
C
      NSIDE = 123
      CALL NACA4(2412,XT,YT,YC,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
C
C==== TGAP(0.02, 0.3) ====
      RINPUT(1) = 0.02
      RINPUT(2) = 0.3
      CALL TGAP(RINPUT,2)
      WRITE(*,9000) 'TGA NB = ', NB
      WRITE(*,9100) 'TGA THICKB = ', THICKB
      WRITE(*,9100) 'TGA CAMBRB = ', CAMBRB
      WRITE(*,9100) 'TGA XB1 = ', XB(1)
      WRITE(*,9100) 'TGA YB1 = ', YB(1)
      WRITE(*,9100) 'TGA XBN = ', XB(NB)
      WRITE(*,9100) 'TGA YBN = ', YB(NB)
C
C==== LERAD(1.5, 0.15) ====
      RINPUT(1) = 1.5
      RINPUT(2) = 0.15
      CALL LERAD(RINPUT,2)
      WRITE(*,9000) 'LER NB = ', NB
      WRITE(*,9100) 'LER RADBLE = ', RADBLE
      WRITE(*,9100) 'LER THICKB = ', THICKB
      WRITE(*,9100) 'LER YB1 = ', YB(1)
      WRITE(*,9100) 'LER YB62 = ', YB(62)
C
C==== SCLXY numerical: Origin TE, SCL=1.2 ====
      XLE = SEVAL(SBLE,XB,XBP,SB,NB)
      YLE = SEVAL(SBLE,YB,YBP,SB,NB)
      XTE = 0.5*(XB(1) + XB(NB))
      YTE = 0.5*(YB(1) + YB(NB))
      XORG = XTE
      YORG = YTE
      SCL = 1.2
      DO I=1, NB
        XB(I) = SCL*(XB(I) - XORG) + XORG
        YB(I) = SCL*(YB(I) - YORG) + YORG
      ENDDO
      LGSAME = .FALSE.
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      WRITE(*,9100) 'SCX CHORDB = ', CHORDB
      WRITE(*,9100) 'SCX THICKB = ', THICKB
      WRITE(*,9100) 'SCX YB1 = ', YB(1)
C
C==== INSIDE / GETXYF ====
      XBF = 0.7
      YBF = 0.0
      INSID = INSIDE(XB,YB,NB,XBF,YBF)
      WRITE(*,9000) 'INS NB = ', NB
      WRITE(*,9100) 'INS XBF = ', XBF
      WRITE(*,9100) 'INS YBF = ', YBF
      IF(INSID) WRITE(*,9000) 'INS INSIDE = ', 1
      IF(.NOT.INSID) WRITE(*,9000) 'INS INSIDE = ', 0
      CALL GETXYF(XB,XBP,YB,YBP,SB,NB, TOPS,BOTS,XBF,YBF)
      WRITE(*,9100) 'GXY TOPS = ', TOPS
      WRITE(*,9100) 'GXY BOTS = ', BOTS
      WRITE(*,9100) 'GXY XBF = ', XBF
      WRITE(*,9100) 'GXY YBF = ', YBF
C
C==== FLAP(0.7, 0.0, 10.0) ====
      RINPUT(1) = 0.7
      RINPUT(2) = 0.0
      RINPUT(3) = 10.0
      CALL FLAP(RINPUT,3)
      WRITE(*,9000) 'FLP NB = ', NB
      IF(LBFLAP) WRITE(*,9000) 'FLP LBFLAP = ', 1
      IF(.NOT.LBFLAP) WRITE(*,9000) 'FLP LBFLAP = ', 0
      IF(LGSYM) WRITE(*,9000) 'FLP LGSYM = ', 1
      IF(.NOT.LGSYM) WRITE(*,9000) 'FLP LGSYM = ', 0
      WRITE(*,9100) 'FLP THICKB = ', THICKB
      WRITE(*,9100) 'FLP CAMBRB = ', CAMBRB
      WRITE(*,9100) 'FLP SBLE = ', SBLE
      WRITE(*,9100) 'FLP YB1 = ', YB(1)
      WRITE(*,9100) 'FLP YB62 = ', YB(62)
      WRITE(*,9100) 'FLP YBMID = ', YB((NB+1)/2)
      WRITE(*,9100) 'FLP YBN = ', YB(NB)
C
C==== EXEC (ABCOPY) after FLAP ====
      CALL ABCOPY(.TRUE.)
      WRITE(*,9000) 'FEX N = ', N
      WRITE(*,9100) 'FEX X1 = ', X(1)
      WRITE(*,9100) 'FEX Y1 = ', Y(1)
      WRITE(*,9100) 'FEX XMID = ', X((N+1)/2)
      WRITE(*,9100) 'FEX YMID = ', Y((N+1)/2)
      WRITE(*,9100) 'FEX XN = ', X(N)
      WRITE(*,9100) 'FEX YN = ', Y(N)
C
 9000 FORMAT(A,I6)
 9100 FORMAT(A,E15.7)
      END

C XFOIL.next
C
C Copyright (C) 2026 Olaf Frommann
C https://github.com/of33/XFOIL.next
C
C XFOIL.next is based on XFOIL 6.99 by Mark Drela and
C Harold Youngren.
C
C This file contains Fortran code derived from XFOIL 6.99.
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

C Fortran harness for xfoil::core::geometry_design - GDES buffer ops.
C The GDES menu commands (GSET/Tran/Scal/LINS/DERO/ADEG/UNIT/NMOV/SINT/
C CADD/SYMM) are inline sections of SUBROUTINE GDES (xgdes.f);
C this harness reproduces the numerical sections unchanged (without
C the plot/prompt parts). COMMON buffer state CR14 as in GDES.
C Fixed form; lines <= column 72.
      PROGRAM GDES_OPS_HARNESS
      INCLUDE 'XFOIL.INC'
      REAL XT(123), YT(123), YC(123)
      REAL DELX, DELY, XXFAC, YYFAC, XTMP, YTMP
      REAL XOC1, YFAC1, XOC2, YFAC2, XOC, FR1, FR2
      REAL XLE, YLE, XTE, YTE, ARAD, ADEG
      REAL DELN, ENX, ENY, ENS
      REAL ATOL, XRF1, XRF2
      INTEGER NSIDE, ISPL, NNEW, NBADD, IMAX
      DIMENSION XBT(123), YBT(123)
C
      PI = ACOS(0.0)*2.0
      HOPI = 0.5/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
C
C==== GSET (Buffer := NACA 2412) ====
      NSIDE = 123
      CALL NACA4(2412,XT,YT,YC,NSIDE,XB,YB,NB,NAME)
      LGSAME = .TRUE.
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      WRITE(*,9000) 'GSE NB = ', NB
      WRITE(*,9100) 'GSE SBLE = ', SBLE
      WRITE(*,9100) 'GSE THICKB = ', THICKB
      WRITE(*,9100) 'GSE CAMBRB = ', CAMBRB
      WRITE(*,9100) 'GSE YB1 = ', YB(1)
      WRITE(*,9100) 'GSE YB62 = ', YB(62)
      WRITE(*,9100) 'GSE YBN = ', YB(NB)
C
C==== Tran(0.1, 0.05) ====
      DELX = 0.1
      DELY = 0.05
      DO I=1, NB
        XB(I) = XB(I) + DELX
        YB(I) = YB(I) + DELY
      ENDDO
      LGSAME = .FALSE.
      WRITE(*,9100) 'TRA XB1 = ', XB(1)
      WRITE(*,9100) 'TRA YB1 = ', YB(1)
      WRITE(*,9100) 'TRA YBN = ', YB(NB)
C
C==== Scal(2.0) ====
      XXFAC = 2.0
      YYFAC = 2.0
      DO I=1, NB
        XB(I) = XB(I)*XXFAC
        YB(I) = YB(I)*YYFAC
      ENDDO
      IF(XXFAC*YYFAC .LT. 0.0) THEN
        DO I=1, NB/2
          XTMP = XB(I)
          YTMP = YB(I)
          XB(I) = XB(NB-I+1)
          YB(I) = YB(NB-I+1)
          XB(NB-I+1) = XTMP
          YB(NB-I+1) = YTMP
        ENDDO
      ENDIF
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      LGSAME = .FALSE.
      WRITE(*,9100) 'SCA CHORDB = ', CHORDB
      WRITE(*,9100) 'SCA THICKB = ', THICKB
      WRITE(*,9100) 'SCA YB1 = ', YB(1)
      WRITE(*,9100) 'SCA YBN = ', YB(NB)
C
C==== Scal with XXFAC=1, YYFAC=-1 (reversal) ====
      XXFAC = 1.0
      YYFAC = -1.0
      DO I=1, NB
        XB(I) = XB(I)*XXFAC
        YB(I) = YB(I)*YYFAC
      ENDDO
      IF(XXFAC*YYFAC .LT. 0.0) THEN
        DO I=1, NB/2
          XTMP = XB(I)
          YTMP = YB(I)
          XB(I) = XB(NB-I+1)
          YB(I) = YB(NB-I+1)
          XB(NB-I+1) = XTMP
          YB(NB-I+1) = YTMP
        ENDDO
      ENDIF
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      LGSAME = .FALSE.
      WRITE(*,9100) 'SCN XB1 = ', XB(1)
      WRITE(*,9100) 'SCN YB1 = ', YB(1)
      WRITE(*,9100) 'SCN YBN = ', YB(NB)
C
C==== LINS(0.0, 1.0, 1.0, 0.5) ====
      XOC1 = 0.0
      YFAC1 = 1.0
      XOC2 = 1.0
      YFAC2 = 0.5
      CALL LEFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      XLE = SEVAL(SBLE,XB,XBP,SB,NB)
      YLE = SEVAL(SBLE,YB,YBP,SB,NB)
      XTE = 0.5*(XB(1) + XB(NB))
      YTE = 0.5*(YB(1) + YB(NB))
      DO I=1, NB
        XOC = (XB(I)-XLE) / (XTE-XLE)
        FR1 = (XOC2-XOC )/(XOC2-XOC1)
        FR2 = (XOC -XOC1)/(XOC2-XOC1)
        YYFAC = FR1*YFAC1 + FR2*YFAC2
        YB(I) = YB(I)*YYFAC
      ENDDO
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      LGSAME = .FALSE.
      WRITE(*,9100) 'LIN THICKB = ', THICKB
      WRITE(*,9100) 'LIN YB1 = ', YB(1)
      WRITE(*,9100) 'LIN YB62 = ', YB(62)
      WRITE(*,9100) 'LIN YBN = ', YB(NB)
C
C==== DERO ====
      CALL LEFIND(SBLE,XB,XBP,YB,YBP,SB,NB)
      XLE = SEVAL(SBLE,XB,XBP,SB,NB)
      YLE = SEVAL(SBLE,YB,YBP,SB,NB)
      XTE = 0.5*(XB(1) + XB(NB))
      YTE = 0.5*(YB(1) + YB(NB))
      ARAD = ATAN2(YTE-YLE,XTE-XLE)
      CALL ROTATE(XB,YB,NB,ARAD)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      LGSAME = .FALSE.
      WRITE(*,9100) 'DER ARAD = ', ARAD
      WRITE(*,9100) 'DER THICKB = ', THICKB
      WRITE(*,9100) 'DER YB1 = ', YB(1)
      WRITE(*,9100) 'DER YBN = ', YB(NB)
C
C==== ADEG(5.0) ====
      ADEG = 5.0
      ARAD = ADEG*PI/180.0
      CALL ROTATE(XB,YB,NB,ARAD)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      APX1BA = APX1BA - ARAD
      APX2BA = APX2BA - ARAD
      APX1BT = APX1BT - ARAD
      APX2BT = APX2BT - ARAD
      WRITE(*,9100) 'ADE APX1BA = ', APX1BA
      WRITE(*,9100) 'ADE APX2BT = ', APX2BT
      WRITE(*,9100) 'ADE YB1 = ', YB(1)
      WRITE(*,9100) 'ADE YBN = ', YB(NB)
C
C==== UNIT ====
      CALL NORM(XB,XBP,YB,YBP,SB,NB)
      LGSAME = .FALSE.
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      WRITE(*,9100) 'UNI CHORDB = ', CHORDB
      WRITE(*,9100) 'UNI THICKB = ', THICKB
      WRITE(*,9100) 'UNI YB1 = ', YB(1)
C
C==== NMOV(0.01) ====
      DELN = 0.01
      DO IB = 1, NB
        ENX =  YBP(IB)
        ENY = -XBP(IB)
        ENS = SQRT(ENX**2 + ENY**2)
        ENX = ENX/ENS
        ENY = ENY/ENS
        XB(IB) = XB(IB) + ENX*DELN
        YB(IB) = YB(IB) + ENY*DELN
      ENDDO
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      LGSAME = .FALSE.
      WRITE(*,9100) 'NMO THICKB = ', THICKB
      WRITE(*,9100) 'NMO YB1 = ', YB(1)
      WRITE(*,9100) 'NMO YB62 = ', YB(62)
C
C==== SINT ====
      CALL SPLNXY(XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9100) 'SIN SB1 = ', SB(1)
      WRITE(*,9100) 'SIN SBN = ', SB(NB)
      WRITE(*,9100) 'SIN XBP1 = ', XBP(1)
      WRITE(*,9100) 'SIN YBP62 = ', YBP(62)
C
C==== CADD (Defaults) ====
      CALL CANG(XB,YB,NB,2, IMAX,AMAX)
      ATOL = 0.5*AMAX
      ISPL = 1
      XBMIN = XB(1)
      XBMAX = XB(1)
      DO I=1, NB
        XBMIN = MIN(XBMIN,XB(I))
        XBMAX = MAX(XBMAX,XB(I))
      ENDDO
      XRF1 = XBMIN - 0.1*(XBMAX-XBMIN)
      XRF2 = XBMAX + 0.1*(XBMAX-XBMIN)
      SB(1) = 0.0
      DO I = 2, NB
        IF(XB(I).EQ.XB(I-1) .AND. YB(I).EQ.YB(I-1)) THEN
          SB(I) = SB(I-1)
        ELSE
          SB(I) = SB(I-1) + 1.0
        ENDIF
      ENDDO
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL AREFINE(XB,YB,SB,XBP,YBP,NB, ATOL,
     &             IBX,NNEW,W1,W2,XRF1,XRF2)
      NBADD = NNEW - NB
      NB = NNEW
      DO I = 1, NB
        XB(I) = W1(I)
        YB(I) = W2(I)
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
      CALL CANG(XB,YB,NB,1, IMAX,AMAX)
      WRITE(*,9000) 'CAD NB = ', NB
      WRITE(*,9000) 'CAD NBADD = ', NBADD
      WRITE(*,9100) 'CAD THICKB = ', THICKB
      WRITE(*,9100) 'CAD CAMBRB = ', CAMBRB
      WRITE(*,9100) 'CAD YB1 = ', YB(1)
      WRITE(*,9100) 'CAD YBN = ', YB(NB)
C
C==== SYMM (LGSYM=true -> ZERCAM) ====
      LGSYM = .TRUE.
      CALL ZERCAM
      WRITE(*,9000) 'SYM NB = ', NB
      WRITE(*,9100) 'SYM THICKB = ', THICKB
      WRITE(*,9100) 'SYM CAMBRB = ', CAMBRB
      WRITE(*,9100) 'SYM YB1 = ', YB(1)
      WRITE(*,9100) 'SYM YBN = ', YB(NB)
C
 9000 FORMAT(A,I6)
 9100 FORMAT(A,E15.7)
      END

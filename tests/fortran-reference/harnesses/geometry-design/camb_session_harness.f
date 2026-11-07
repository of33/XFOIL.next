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

C Fortran harness for xfoil::core::geometry_design - CAMB session
C (xtcam.f).
C CAMB itself is not linkable (plot/file routines); this harness
C reproduces the canonical numerical sequence of SUBROUTINE CAMB
C (100 block, SETC, CLR, INPC, INPP, SCAL, ADD) with fixed
C input point lists. COMMON state CR19/CI19 as in CAMB.
C Fixed form; lines <= column 72.
      PROGRAM CAMB_SESSION_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER NCADD, NPADD
      REAL SBL, XBL, YBL, XBCH, YBCH, SBCH
      REAL XBR, YBR
      LOGICAL LCLEAR
      REAL XT(123), YT(123), YC(123)
      REAL PCX(5), PCY(5), PPX(3), PPY(3)
      REAL XX, YY, CLX, CMX, SCAL
      REAL SBOPP, XBOPP, YBOPP, OLDCAM, CAM
C
      PI = ACOS(0.0)*2.0
      HOPI = 0.5/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
      XCMREF = 0.25
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
C==== 100 block with LCLEAR=.TRUE. (Init) ====
      LCLEAR = .TRUE.
      CALL XLFIND(SBL,XB,XBP,YB,YBP,SB,NB)
      XBL = SEVAL(SBL, XB,XBP,SB,NB)
      YBL = SEVAL(SBL, YB,YBP,SB,NB)
      XBR = 0.5*(XB(1)+XB(NB))
      YBR = 0.5*(YB(1)+YB(NB))
      XBCH = XBR - XBL
      YBCH = 0.
      SBCH = SQRT(XBCH**2 + YBCH**2)
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,
     &            XB,XBP,YB,YBP,SB,NB )
      NCAM = MIN( 201 , NTX )
      DO K=1, NCAM
        XCAM(K) = XCM(1) + (XCM(NCM)-XCM(1))*FLOAT(K-1)/FLOAT(NCAM-1)
      ENDDO
      NCADD = 2
      XCADD(1) = XCAM(1)
      XCADD(2) = XCAM(NCAM)
      YCADD(1) = 0.0
      YCADD(2) = 0.0
      NPADD = 2
      XPADD(1) = XCAM(1)
      XPADD(2) = XCAM(NCAM)
      YPADD(1) = 0.0
      YPADD(2) = 0.0
      CALL SEGSPL(YCADD,YCADDP,XCADD,NCADD)
      CALL SEGSPL(YPADD,YPADDP,XPADD,NPADD)
      DO K=1, NCAM
        YCAM(K)  = SEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
        YCAMP(K) = DEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
        PCAM(K)  = SEVAL(XCAM(K),YPADD,YPADDP,XPADD,NPADD)
        PCAMP(K) = DEVAL(XCAM(K),YPADD,YPADDP,XPADD,NPADD)
      ENDDO
      LCLEAR = .FALSE.
      WRITE(*,9000) 'INI NCAM = ', NCAM
      WRITE(*,9000) 'INI NCADD = ', NCADD
      WRITE(*,9000) 'INI NPADD = ', NPADD
      WRITE(*,9100) 'INI SBL = ', SBL
      WRITE(*,9100) 'INI XBCH = ', XBCH
      WRITE(*,9100) 'INI SBCH = ', SBCH
      WRITE(*,9100) 'INI XCAM1 = ', XCAM(1)
      WRITE(*,9100) 'INI XCAM101 = ', XCAM(101)
      WRITE(*,9100) 'INI XCAM201 = ', XCAM(201)
      WRITE(*,9100) 'INI YCAM101 = ', YCAM(101)
      WRITE(*,9100) 'INI PCAM101 = ', PCAM(101)
C
C==== SETC ====
      NCADD = NCM
      DO K=1, NCM
        XCADD(K) = XCM(K)
        YCADD(K) = YCM(K)
      END DO
      CALL SEGSPL(YCADD,YCADDP,XCADD,NCADD)
      DO K=1, NCAM
        YCAM(K)  = SEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
        YCAMP(K) = DEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
      ENDDO
      WRITE(*,9000) 'SET NCADD = ', NCADD
      WRITE(*,9100) 'SET XCADD1 = ', XCADD(1)
      WRITE(*,9100) 'SET YCADDN = ', YCADD(NCADD)
      WRITE(*,9100) 'SET YCAM51 = ', YCAM(51)
      WRITE(*,9100) 'SET YCAM101 = ', YCAM(101)
      WRITE(*,9100) 'SET YCAM151 = ', YCAM(151)
C
C==== CLR (100 block again) ====
      LCLEAR = .TRUE.
      CALL XLFIND(SBL,XB,XBP,YB,YBP,SB,NB)
      XBL = SEVAL(SBL, XB,XBP,SB,NB)
      YBL = SEVAL(SBL, YB,YBP,SB,NB)
      XBR = 0.5*(XB(1)+XB(NB))
      YBR = 0.5*(YB(1)+YB(NB))
      XBCH = XBR - XBL
      YBCH = 0.
      SBCH = SQRT(XBCH**2 + YBCH**2)
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,
     &            XB,XBP,YB,YBP,SB,NB )
      NCAM = MIN( 201 , NTX )
      DO K=1, NCAM
        XCAM(K) = XCM(1) + (XCM(NCM)-XCM(1))*FLOAT(K-1)/FLOAT(NCAM-1)
      ENDDO
      NCADD = 2
      XCADD(1) = XCAM(1)
      XCADD(2) = XCAM(NCAM)
      YCADD(1) = 0.0
      YCADD(2) = 0.0
      NPADD = 2
      XPADD(1) = XCAM(1)
      XPADD(2) = XCAM(NCAM)
      YPADD(1) = 0.0
      YPADD(2) = 0.0
      CALL SEGSPL(YCADD,YCADDP,XCADD,NCADD)
      CALL SEGSPL(YPADD,YPADDP,XPADD,NPADD)
      DO K=1, NCAM
        YCAM(K)  = SEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
        YCAMP(K) = DEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
        PCAM(K)  = SEVAL(XCAM(K),YPADD,YPADDP,XPADD,NPADD)
        PCAMP(K) = DEVAL(XCAM(K),YPADD,YPADDP,XPADD,NPADD)
      ENDDO
      LCLEAR = .FALSE.
      WRITE(*,9000) 'CLR NCADD = ', NCADD
      WRITE(*,9100) 'CLR YCAM101 = ', YCAM(101)
C
C==== INPC with fixed points ====
      PCX(1) = 0.0 ; PCY(1) = 0.0
      PCX(2) = 0.2 ; PCY(2) = 0.02
      PCX(3) = 0.5 ; PCY(3) = 0.05
      PCX(4) = 0.8 ; PCY(4) = 0.02
      PCX(5) = 1.0 ; PCY(5) = 0.0
      NCADD = 0
      DO I=1, 5
        XX = PCX(I)
        YY = PCY(I)
        IF(XX.LE.0.0) THEN
          XX = 0.0
        ELSEIF(XX.GE.1.0) THEN
          XX = 1.0
        ENDIF
        NCADD = NCADD + 1
        XCADD(NCADD) = XCAM(1) + XX*XBCH - YY*YBCH
        YCADD(NCADD) =           XX*YBCH + YY*XBCH
      ENDDO
      CALL SORTDUP(NCADD,XCADD,YCADD)
      CALL FIXDUP (NCADD,XCADD,YCADD)
      CALL SEGSPL(YCADD,YCADDP,XCADD,NCADD)
      DO K=1, NCAM
        YCAM(K)  = SEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
        YCAMP(K) = DEVAL(XCAM(K),YCADD,YCADDP,XCADD,NCADD)
      ENDDO
      WRITE(*,9000) 'NPC NCADD = ', NCADD
      WRITE(*,9100) 'NPC XCADD1 = ', XCADD(1)
      WRITE(*,9100) 'NPC YCADD3 = ', YCADD(3)
      WRITE(*,9100) 'NPC YCAM51 = ', YCAM(51)
      WRITE(*,9100) 'NPC YCAM101 = ', YCAM(101)
      WRITE(*,9100) 'NPC YCAM151 = ', YCAM(151)
C
C==== INPP with fixed points ====
      PPX(1) = 0.1 ; PPY(1) =  1.0
      PPX(2) = 0.5 ; PPY(2) = -0.5
      PPX(3) = 0.9 ; PPY(3) =  0.0
      DO I=1, 3
        XX = PPX(I)
        YY = PPY(I)
        IF(XX.LE.0.0) THEN
          XX = 0.0
        ELSEIF(XX.GE.1.0) THEN
          XX = 1.0
        ENDIF
        NPADD = NPADD + 1
        XPADD(NPADD) = XCAM(1) + XX*XBCH
        YPADD(NPADD) = YY
      ENDDO
      CALL SORTDUP(NPADD,XPADD,YPADD)
      CALL FIXDUP (NPADD,XPADD,YPADD)
      CALL SEGSPL(YPADD,YPADDP,XPADD,NPADD)
      DO K=1, NCAM
        PCAM(K)  = SEVAL(XCAM(K),YPADD,YPADDP,XPADD,NPADD)
        PCAMP(K) = DEVAL(XCAM(K),YPADD,YPADDP,XPADD,NPADD)
      ENDDO
      CALL CPCAM(NCAM,XCAM,YCAM,YCAMP,PCAM,PCAMP)
      CLX = 0.0
      CMX = 0.0
      DO K=1, NCAM-1
        DX =      XCAM(K+1) - XCAM(K)
        XA = 0.5*(XCAM(K+1) + XCAM(K))
        PA = 0.5*(PCAM(K+1) + PCAM(K))
        CLX = CLX + PA*DX
        CMX = CMX + PA*DX*(XCMREF-XA)
      END DO
      WRITE(*,9000) 'NPP NPADD = ', NPADD
      WRITE(*,9100) 'NPP YCAM51 = ', YCAM(51)
      WRITE(*,9100) 'NPP YCAM101 = ', YCAM(101)
      WRITE(*,9100) 'NPP YCAM151 = ', YCAM(151)
      WRITE(*,9100) 'NPP CLX = ', CLX
      WRITE(*,9100) 'NPP CMX = ', CMX
C
C==== SCAL 2.0 ====
      SCAL = 2.0
      DO I = 1, NCADD
        YCADD(I)  = YCADD(I) *SCAL
        YPADD(I)  = YPADD(I) *SCAL
        YCADDP(I) = YCADDP(I)*SCAL
        YPADDP(I) = YPADDP(I)*SCAL
      END DO
      DO I = 1, NCAM
        YCAM(I)   = YCAM(I) *SCAL
        YCAMP(I)  = YCAMP(I)*SCAL
        PCAM(I)   = PCAM(I) *SCAL
        PCAMP(I)  = PCAMP(I)*SCAL
      END DO
      CLX = 0.0
      CMX = 0.0
      DO K=1, NCAM-1
        DX =      XCAM(K+1) - XCAM(K)
        XA = 0.5*(XCAM(K+1) + XCAM(K))
        PA = 0.5*(PCAM(K+1) + PCAM(K))
        CLX = CLX + PA*DX
        CMX = CMX + PA*DX*(XCMREF-XA)
      END DO
      WRITE(*,9100) 'SCL YCAM101 = ', YCAM(101)
      WRITE(*,9100) 'SCL PCAM101 = ', PCAM(101)
      WRITE(*,9100) 'SCL CLX = ', CLX
      WRITE(*,9100) 'SCL CMX = ', CMX
C
C==== ADD ====
      CALL SEGSPL(YCAM,YCAMP,XCAM,NCAM)
      DO I=1, NB
        CALL SOPPS(SBOPP, SB(I),XB,XBP,YB,YBP,SB,NB,SBL)
        XBOPP = SEVAL(SBOPP,XB,XBP,SB,NB)
        YBOPP = SEVAL(SBOPP,YB,YBP,SB,NB)
        OLDCAM = 0.5*(YB(I)+YBOPP)*XBCH/SBCH
     &        - 0.5*(XB(I)+XBOPP)*YBCH/SBCH
        CAM = OLDCAM
     &       + SEVAL(XB(I),YCAM,YCAMP,XCAM,NCAM)
        W1(I) = CAM  +  0.5*(YB(I)-YBOPP)
      END DO
      DO I=1, NB
        YB(I) = W1(I)
      END DO
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      WRITE(*,9100) 'ADD SBLE = ', SBLE
      WRITE(*,9100) 'ADD THICKB = ', THICKB
      WRITE(*,9100) 'ADD CAMBRB = ', CAMBRB
      WRITE(*,9100) 'ADD YB1 = ', YB(1)
      WRITE(*,9100) 'ADD YB62 = ', YB(62)
      WRITE(*,9100) 'ADD YBMID = ', YB((NB+1)/2)
      WRITE(*,9100) 'ADD YBN = ', YB(NB)
C
 9000 FORMAT(A,I6)
 9100 FORMAT(A,E15.7)
      END

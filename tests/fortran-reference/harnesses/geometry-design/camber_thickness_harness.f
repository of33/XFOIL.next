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

C Fortran-harness for xfoil::core::geometry_design - GETCAM/GETMAX/CPCAM
C (xtcam.f). Calls the canonical routines unchanged. Fixed form;
C Lines remain <= column 72.
      PROGRAM CAMBER_THICKNESS_HARNESS
      INTEGER NSIDE, IDES, I, NCM, NTK, NCAM
      REAL W1(123), W2(123), W3(123)
      REAL XB(250), YB(250), SB(250), XBP(250), YBP(250)
      REAL XCM(250), YCM(250), YCMP(250)
      REAL XTK(250), YTK(250), YTKP(250)
      REAL XC(60), YCP(60), DYDX(60), PC(60), DPDX(60)
      REAL XMAX, YMAX, PI, X
      CHARACTER*48 NAME
C
      PI = ACOS(-1.0)
C
C ==== GETCAM/GETMAX NACA 0012 ====
      NSIDE = 123
      IDES = 0012
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9000) 'GETCAM0012 NCM = ', NCM
      WRITE(*,9000) 'GETCAM0012 NTK = ', NTK
      DO 10 I = 1, NCM
        WRITE(*,9100) 'GETCAM0012 CM', I, XCM(I), YCM(I)
   10 CONTINUE
      DO 20 I = 1, NTK
        WRITE(*,9100) 'GETCAM0012 TK', I, XTK(I), YTK(I)
   20 CONTINUE
      CALL GETMAX(XCM,YCM,YCMP,NCM,XMAX,YMAX)
      WRITE(*,9200) 'GETMAX0012 CAM XMAX = ', XMAX
      WRITE(*,9200) 'GETMAX0012 CAM YMAX = ', YMAX
      CALL GETMAX(XTK,YTK,YTKP,NTK,XMAX,YMAX)
      WRITE(*,9200) 'GETMAX0012 THK XMAX = ', XMAX
      WRITE(*,9200) 'GETMAX0012 THK YMAX = ', YMAX
C
C ==== GETCAM/GETMAX NACA 2412 ====
      NSIDE = 123
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,XB,XBP,YB,YBP,SB,NB)
      WRITE(*,9000) 'GETCAM2412 NCM = ', NCM
      WRITE(*,9000) 'GETCAM2412 NTK = ', NTK
      DO 30 I = 1, NCM
        WRITE(*,9100) 'GETCAM2412 CM', I, XCM(I), YCM(I)
   30 CONTINUE
      DO 40 I = 1, NTK
        WRITE(*,9100) 'GETCAM2412 TK', I, XTK(I), YTK(I)
   40 CONTINUE
      CALL GETMAX(XCM,YCM,YCMP,NCM,XMAX,YMAX)
      WRITE(*,9200) 'GETMAX2412 CAM XMAX = ', XMAX
      WRITE(*,9200) 'GETMAX2412 CAM YMAX = ', YMAX
      CALL GETMAX(XTK,YTK,YTKP,NTK,XMAX,YMAX)
      WRITE(*,9200) 'GETMAX2412 THK XMAX = ', XMAX
      WRITE(*,9200) 'GETMAX2412 THK YMAX = ', YMAX
C
C ==== CPCAM: prescribed DCp distribution P = sin(pi*x), 51 points ====
      NCAM = 51
      DO 50 I = 1, NCAM
        X = FLOAT(I-1)/FLOAT(NCAM-1)
        XC(I) = X
        PC(I) = SIN(PI*X)
        DPDX(I) = PI*COS(PI*X)
   50 CONTINUE
      CALL CPCAM(NCAM,XC,YCP,DYDX,PC,DPDX)
      WRITE(*,9000) 'CPCAM NCAM = ', NCAM
      DO 60 I = 1, NCAM
        WRITE(*,9100) 'CPCAM PT', I, XC(I), YCP(I)
        WRITE(*,9150) 'CPCAM DY', I, DYDX(I)
   60 CONTINUE
C
C ==== CPCAM: constant loading P = 1, DPDX = 0, 41 points ====
      NCAM = 41
      DO 70 I = 1, NCAM
        X = FLOAT(I-1)/FLOAT(NCAM-1)
        XC(I) = X
        PC(I) = 1.0
        DPDX(I) = 0.0
   70 CONTINUE
      CALL CPCAM(NCAM,XC,YCP,DYDX,PC,DPDX)
      WRITE(*,9000) 'CPCAMC NCAM = ', NCAM
      DO 80 I = 1, NCAM
        WRITE(*,9100) 'CPCAMC PT', I, XC(I), YCP(I)
        WRITE(*,9150) 'CPCAMC DY', I, DYDX(I)
   80 CONTINUE
C
 9000 FORMAT(A,I6)
 9100 FORMAT(A,I4,2E15.7)
 9150 FORMAT(A,I4,E15.7)
 9200 FORMAT(A,E15.7)
      END

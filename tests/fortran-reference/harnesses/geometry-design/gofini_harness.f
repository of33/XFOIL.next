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

C Fortran harness for xfoil::core::geometry_design - GOFINI/GOFSET/
C SCALIT. GOFINI/GOFSET are inline routines with an AXISADJ call
C (external libPlt routine, not in fortran-src, not implemented here);
C this harness reproduces the numerical sections of xgdes.f 1038-1214,
C passing YPMIN/YPMAX/DXYP through unchanged.
C SCALIT is called canonically from plutil.f.
C Fixed form; lines <= column 72.
      PROGRAM GOFINI_HARNESS
      INCLUDE 'XFOIL.INC'
      REAL XT(123), YT(123), YC(123)
      REAL XBMIN, YBMIN, XBMAX, YBMAX, CMMIN, CMMAX, TKMIN, TKMAX
      REAL XRANGE, YRANGE, RANGE, SF, PAR, DPRANGE, DYPLT
      REAL XG1, YG1, XG2, YG2, GXSF, GYSF, GSF
      REAL CH, PLOTAR, XPAGE, YPAGE, XMARG, YMARG
      REAL YPMIN, YPMAX, PSPAN, DXYP
      INTEGER NTICS
      DIMENSION YS(3)
      REAL YSF1
C
      CH = 0.015
      PLOTAR = 0.55
      XPAGE = 11.0
      YPAGE = 8.5
      XMARG = 0.0
      YMARG = 0.0
      YPMIN = -0.4
      YPMAX =  0.4
      LGGRID = .FALSE.
      LPLCAM = .FALSE.
C
      NSIDE = 123
      CALL NACA4(2412,XT,YT,YC,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SEGSPL(XB,XBP,SB,NB)
      CALL SEGSPL(YB,YBP,SB,NB)
C
C==== SCALIT directly ====
      YS(1) = 0.95
      YS(2) = 0.2
      YS(3) = -0.7
      CALL SCALIT(3,YS,0.0,YSF1)
      WRITE(*,9100) 'SCA YSF1 = ', YSF1
      CALL SCALIT(1,YS,0.0,YSF1)
      WRITE(*,9100) 'SCA YSF2 = ', YSF1
C
C==== GOFINI (numerical tolerances, AXISADJ passed through) ====
      XBMIN = XB(1)
      YBMIN = YB(1)
      XBMAX = XB(1)
      YBMAX = YB(1)
      DO I=1, NB
        XBMIN = MIN(XBMIN,XB(I))
        YBMIN = MIN(YBMIN,YB(I))
        XBMAX = MAX(XBMAX,XB(I))
        YBMAX = MAX(YBMAX,YB(I))
      ENDDO
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,
     &            XB,XBP,YB,YBP,SB,NB )
      CMMIN = 0.
      CMMAX = 0.
      DO I=1, NCM
        CMMIN = MIN(CMMIN,YCM(I))
        CMMAX = MAX(CMMAX,YCM(I))
      ENDDO
      TKMIN = 0.
      TKMAX = 0.
      DO I=1, NTK
        TKMIN = MIN(TKMIN,YTK(I))
        TKMAX = MAX(TKMAX,YTK(I))
      ENDDO
      XRANGE = XBMAX - XBMIN
      YRANGE = YBMAX - YBMIN
      YS(1) = 0.95*XRANGE
      CALL SCALIT(1,YS,0.0,XSF)
      YS(1) = 0.95*YRANGE
      CALL SCALIT(1,YS,0.0,YSF)
      DXYG = 0.1 / MIN(XSF,YSF)
      XGMAX = DXYG*(INT(XBMAX/DXYG+1001.01) - 1000)
      XGMIN = DXYG*(INT(XBMIN/DXYG-1001.01) + 1000)
      YGMAX = DXYG*(INT(YBMAX/DXYG+1001.01) - 1000)
      YGMIN = DXYG*(INT(YBMIN/DXYG-1001.01) + 1000)
      DXYC = DXYG
      XCMIN = XGMIN
      XCMAX = XGMAX
      YCMIN = MIN(CMMIN,-TKMAX)
      YCMAX = MAX(CMMAX, TKMAX)
      YCMAX = DXYC*(INT(YCMAX/DXYC+1000.25) - 999)
      YCMIN = DXYC*(INT(YCMIN/DXYC-1000.25) + 999)
      YCMAX = MAX(YCMAX,YCMIN+DXYC)
      XRANGE = XBMAX - XBMIN
      YRANGE = YBMAX - YBMIN
      RANGE = MAX(XRANGE,YRANGE)
      SF = MIN( 1.0/XRANGE , PLOTAR/YRANGE )
      XSF = SF
      YSF = SF
      CHG = 0.75*CH * RANGE*SF
      CHG = MAX(CHG,0.0075)
      XOFF = XBMIN - 0.05*RANGE
      YOFF = YBMIN - 0.05*RANGE
      XPMIN = XGMIN
      XPMAX = XGMAX
      DYOFFC = - YGMAX + YCMIN - 2.2*CHG/YSF
      PAR = (YPAGE-2.0*YMARG)/(XPAGE-2.0*XMARG)
      DPRANGE = YPMAX-YPMIN
      DYPLT = MAX(0.1,PAR-PLOTAR)
      YSFP = 0.8*DYPLT/DPRANGE
      YSFP = YSFP/YSF
      DYOFFP = -YCMAX+DYOFFC + YPMIN*YSFP - 2.2*CHG/YSF
      WRITE(*,9100) 'GFI XSF = ', XSF
      WRITE(*,9100) 'GFI YSF = ', YSF
      WRITE(*,9100) 'GFI DXYG = ', DXYG
      WRITE(*,9100) 'GFI XGMIN = ', XGMIN
      WRITE(*,9100) 'GFI XGMAX = ', XGMAX
      WRITE(*,9100) 'GFI YGMIN = ', YGMIN
      WRITE(*,9100) 'GFI YGMAX = ', YGMAX
      WRITE(*,9100) 'GFI YCMIN = ', YCMIN
      WRITE(*,9100) 'GFI YCMAX = ', YCMAX
      WRITE(*,9100) 'GFI CHG = ', CHG
      WRITE(*,9100) 'GFI XOFF = ', XOFF
      WRITE(*,9100) 'GFI YOFF = ', YOFF
      WRITE(*,9100) 'GFI DYOFFC = ', DYOFFC
      WRITE(*,9100) 'GFI YSFP = ', YSFP
      WRITE(*,9100) 'GFI DYOFFP = ', DYOFFP
C
C==== GOFSET (numerical tolerances) ====
      SIZE = 10.0
      XWIND = 9.0
      YWIND = 6.0
      RANGE = MAX( (XWIND/SIZE)/XSF , (YWIND/SIZE)/YSF )
      XG1 = XOFF + 0.1*RANGE + 4.0*CHG/XSF
      YG1 = YOFF + 0.1*RANGE + 2.0*CHG/YSF
      XG2 = XOFF - 0.1*RANGE + (XWIND/SIZE)/XSF
      YG2 = YOFF - 0.1*RANGE + (YWIND/SIZE)/YSF
      XG1 = MAX(XG1,XBMIN)
      XG2 = MIN(XG2,XBMAX)
      YG1 = MAX(YG1,YBMIN)
      YG2 = MIN(YG2,YBMAX)
      YS(1) = 0.95*(XG2-XG1)
      CALL SCALIT(1,YS,0.0,GXSF)
      YS(1) = 0.95*(YG2-YG1)
      CALL SCALIT(1,YS,0.0,GYSF)
      GSF = GXSF
      DXYG = 0.1 / GSF
      XGMAX = DXYG*(INT(XG2/DXYG+1001.01) - 1000)
      XGMIN = DXYG*(INT(XG1/DXYG-1001.01) + 1000)
      YGMAX = DXYG*(INT(YG2/DXYG+1001.01) - 1000)
      YGMIN = DXYG*(INT(YG1/DXYG-1001.01) + 1000)
      WRITE(*,9100) 'GFS XG1 = ', XG1
      WRITE(*,9100) 'GFS YG1 = ', YG1
      WRITE(*,9100) 'GFS XG2 = ', XG2
      WRITE(*,9100) 'GFS YG2 = ', YG2
      WRITE(*,9100) 'GFS GXSF = ', GXSF
      WRITE(*,9100) 'GFS GYSF = ', GYSF
      WRITE(*,9100) 'GFS DXYG = ', DXYG
      WRITE(*,9100) 'GFS XGMIN = ', XGMIN
      WRITE(*,9100) 'GFS XGMAX = ', XGMAX
      WRITE(*,9100) 'GFS YGMIN = ', YGMIN
      WRITE(*,9100) 'GFS YGMAX = ', YGMAX
C
 9100 FORMAT(A,E15.7)
      END

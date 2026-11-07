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

C Fortran harness for xfoil::core::geometry_design -
C THKCAM/ZERCAM/TCBUF/
C TCSCAL/TCSET (xtcam.f). Calls the canonical routines unchanged
C and uses the COMMON buffer state (CR14) as CAMB/GDES. Fixed form;
C lines remain <= column 72.
      PROGRAM THKCAM_HARNESS
      INCLUDE 'XFOIL.INC'
      DIMENSION RINPUT(20)
      REAL XX(123), YT(123), YC(123)
      REAL CXMAX, CYMAX, TXMAX, TYMAX, TFAC, CFAC
      INTEGER NSIDE
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
      WRITE(*,9000) 'BASE NB = ', NB
      WRITE(*,9100) 'BASE SBLE = ', SBLE
      WRITE(*,9100) 'BASE CHORDB = ', CHORDB
      WRITE(*,9100) 'BASE THICKB = ', THICKB
      WRITE(*,9100) 'BASE CAMBRB = ', CAMBRB
      WRITE(*,9200) 'BASE XB1 = ', XB(1)
      WRITE(*,9200) 'BASE YB1 = ', YB(1)
      WRITE(*,9200) 'BASE XB62 = ', XB(62)
      WRITE(*,9200) 'BASE YB62 = ', YB(62)
      WRITE(*,9200) 'BASE XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'BASE YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'BASE XBN = ', XB(NB)
      WRITE(*,9200) 'BASE YBN = ', YB(NB)
C
C ==== TCBUF (report) ====
      CALL TCBUF
C
C ==== THKCAM(0.5, 2.0) ====
      CALL THKCAM(0.5, 2.0)
      WRITE(*,9000) 'THK NB = ', NB
      WRITE(*,9100) 'THK SBLE = ', SBLE
      WRITE(*,9100) 'THK CHORDB = ', CHORDB
      WRITE(*,9100) 'THK THICKB = ', THICKB
      WRITE(*,9100) 'THK CAMBRB = ', CAMBRB
      WRITE(*,9200) 'THK XB1 = ', XB(1)
      WRITE(*,9200) 'THK YB1 = ', YB(1)
      WRITE(*,9200) 'THK XB62 = ', XB(62)
      WRITE(*,9200) 'THK YB62 = ', YB(62)
      WRITE(*,9200) 'THK XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'THK YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'THK XBN = ', XB(NB)
      WRITE(*,9200) 'THK YBN = ', YB(NB)
C
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,XB,XBP,YB,YBP,SB,NB)
      CALL GETMAX(XCM,YCM,YCMP,NCM,CXMAX,CYMAX)
      CALL GETMAX(XTK,YTK,YTKP,NTK,TXMAX,TYMAX)
      WRITE(*,9100) 'THKD CXMAX = ', CXMAX
      WRITE(*,9100) 'THKD CYMAX = ', CYMAX
      WRITE(*,9100) 'THKD TXMAX = ', TXMAX
      WRITE(*,9100) 'THKD TYMAX = ', TYMAX
C ==== TCSET with target values 0.15 / 0.03 ====
      RINPUT(1) = 0.15
      RINPUT(2) = 0.03
      CALL TCSET(RINPUT,2)
      WRITE(*,9000) 'TCSE NB = ', NB
      WRITE(*,9100) 'TCSE CHORDB = ', CHORDB
      WRITE(*,9100) 'TCSE THICKB = ', THICKB
      WRITE(*,9100) 'TCSE CAMBRB = ', CAMBRB
      WRITE(*,9200) 'TCSE XB1 = ', XB(1)
      WRITE(*,9200) 'TCSE YB1 = ', YB(1)
      WRITE(*,9200) 'TCSE XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'TCSE YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'TCSE XBN = ', XB(NB)
      WRITE(*,9200) 'TCSE YBN = ', YB(NB)
C
C ==== ZERCAM numerical path ====
C ZERCAM itself calls PLTAIR/PLNEWP (libPlt, not in fortran-src) and can
C therefore not be linked. The harness reproduces exactly the numerical
C sequence of ZERCAM (TFAC=1, CFAC=0, THKCAM, YSYM ISIDE=1,
C GEOPAR).
      TFAC = 1.0
      CFAC = 0.0
      CALL THKCAM(TFAC,CFAC)
      CALL YSYM(XB,XBP,YB,YBP,SB,2*IQX,NB,1,W1,W2)
      CALL GEOPAR(XB,XBP,YB,YBP,SB,NB,W1,
     &            SBLE,CHORDB,AREAB,RADBLE,ANGBTE,
     &            EI11BA,EI22BA,APX1BA,APX2BA,
     &            EI11BT,EI22BT,APX1BT,APX2BT,
     &            THICKB,CAMBRB )
      WRITE(*,9000) 'ZERC NB = ', NB
      WRITE(*,9100) 'ZERC SBLE = ', SBLE
      WRITE(*,9100) 'ZERC CHORDB = ', CHORDB
      WRITE(*,9100) 'ZERC THICKB = ', THICKB
      WRITE(*,9100) 'ZERC CAMBRB = ', CAMBRB
      WRITE(*,9200) 'ZERC XB1 = ', XB(1)
      WRITE(*,9200) 'ZERC YB1 = ', YB(1)
      WRITE(*,9200) 'ZERC XB62 = ', XB(62)
      WRITE(*,9200) 'ZERC YB62 = ', YB(62)
      WRITE(*,9200) 'ZERC XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'ZERC YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'ZERC XBN = ', XB(NB)
      WRITE(*,9200) 'ZERC YBN = ', YB(NB)
C
      CALL GETCAM(XCM,YCM,NCM,XTK,YTK,NTK,XB,XBP,YB,YBP,SB,NB)
      CALL GETMAX(XCM,YCM,YCMP,NCM,CXMAX,CYMAX)
      CALL GETMAX(XTK,YTK,YTKP,NTK,TXMAX,TYMAX)
      WRITE(*,9100) 'ZERD CXMAX = ', CXMAX
      WRITE(*,9100) 'ZERD CYMAX = ', CYMAX
      WRITE(*,9100) 'ZERD TXMAX = ', TXMAX
      WRITE(*,9100) 'ZERD TYMAX = ', TYMAX
C ==== TCSCAL with Factors 1.5 / 0.5 ====
      RINPUT(1) = 1.5
      RINPUT(2) = 0.5
      CALL TCSCAL(RINPUT,2)
      WRITE(*,9000) 'TCSC NB = ', NB
      WRITE(*,9100) 'TCSC CHORDB = ', CHORDB
      WRITE(*,9100) 'TCSC THICKB = ', THICKB
      WRITE(*,9100) 'TCSC CAMBRB = ', CAMBRB
      WRITE(*,9200) 'TCSC XB1 = ', XB(1)
      WRITE(*,9200) 'TCSC YB1 = ', YB(1)
      WRITE(*,9200) 'TCSC XBMID = ', XB((NB+1)/2)
      WRITE(*,9200) 'TCSC YBMID = ', YB((NB+1)/2)
      WRITE(*,9200) 'TCSC XBN = ', XB(NB)
      WRITE(*,9200) 'TCSC YBN = ', YB(NB)
C
 9000 FORMAT(A,I6)
 9100 FORMAT(A,E15.7)
 9200 FORMAT(A,E15.7)
      END

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

C Fortran-harness for xfoil::core::paneling -
C PANGEN/ABCOPY/NCALC/APCALC.
C Calls the canonical routines unchanged. PANGEN/TECALC lie in
C xfoil.f, ABCOPY in xgdes.f, NCALC/APCALC in xpanel.f together with
C unlinked routines; the harness link uses byte-faithfully
C extracted routine blocks (see tools/build-paneling-harnesses.sh).
C
C The harness output is deterministic:
C - GAM/GAM_A are initialized with 0.0 before every PANGEN call
C   (the TECALC SIGTE/GAMTE share is irrelevant in the harness).
C - PI = 4.0*ATAN(1.0) as in the Fortran INIT (APCALC COMMON/CR11/).
C - The INIT defaults for the paneling are set before Test 1.
C
C Important: fixed form; lines remain <= column 72.
      PROGRAM PANELING_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE, IDES
      REAL GAPLOC
      CHARACTER*80 CLYFILE
C
C ==== INIT defaults and COMMON initialization (as xfoil.f INIT) ====
      PI = 4.0*ATAN(1.0)
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      XSREF1 = 1.0
      XSREF2 = 1.0
      XPREF1 = 1.0
      XPREF2 = 1.0
      DO 1 I=1,IQX
        GAM(I) = 0.0
        GAM_A(I) = 0.0
    1 CONTINUE
      SHARP = .FALSE.
C
C ==== Test 1: Default-PANE on NACA 2412 (PANGEN(.TRUE.)) ====
      NSIDE = IQX/3
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      WRITE(*,9100) 'PANE160 N = ', N
      WRITE(*,9200) 'PANE160 SLE = ', SLE
      WRITE(*,9200) 'PANE160 XLE = ', XLE
      WRITE(*,9200) 'PANE160 YLE = ', YLE
      WRITE(*,9200) 'PANE160 XTE = ', XTE
      WRITE(*,9200) 'PANE160 YTE = ', YTE
      WRITE(*,9200) 'PANE160 CHORD = ', CHORD
      DO 10 I=1,N
        WRITE(*,9200) 'PANE160 X = ', X(I)
        WRITE(*,9200) 'PANE160 Y = ', Y(I)
        WRITE(*,9200) 'PANE160 S = ', S(I)
        WRITE(*,9200) 'PANE160 XP = ', XP(I)
        WRITE(*,9200) 'PANE160 YP = ', YP(I)
        WRITE(*,9200) 'PANE160 NX = ', NX(I)
        WRITE(*,9200) 'PANE160 NY = ', NY(I)
        WRITE(*,9200) 'PANE160 APANEL = ', APANEL(I)
   10 CONTINUE
C
C ==== Test 2: PPAR non-default (NPAN=200, CVPAR=1.5, CTERAT=0.2) ====
      NPAN = 200
      CVPAR = 1.5
      CTERAT = 0.2
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANE200 N = ', N
      WRITE(*,9200) 'PANE200 SLE = ', SLE
      WRITE(*,9200) 'PANE200 CHORD = ', CHORD
      DO 20 I=1,N
        WRITE(*,9200) 'PANE200 X = ', X(I)
        WRITE(*,9200) 'PANE200 Y = ', Y(I)
        WRITE(*,9200) 'PANE200 APANEL = ', APANEL(I)
   20 CONTINUE
C
C ==== Test 3: NPAN=364 (effective GETPAN clamp value, defaults) ====
      NPAN = 364
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANE364 N = ', N
      WRITE(*,9200) 'PANE364 SLE = ', SLE
      DO 30 I=1,N
        WRITE(*,9200) 'PANE364 X = ', X(I)
        WRITE(*,9200) 'PANE364 Y = ', Y(I)
   30 CONTINUE
C
C ==== Test 4: PCOP - ABCOPY on a fresh NACA 2412 buffer ====
      NSIDE = IQX/3
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL ABCOPY(.TRUE.)
      WRITE(*,9100) 'PCOP N = ', N
      WRITE(*,9200) 'PCOP SLE = ', SLE
      WRITE(*,9200) 'PCOP CHORD = ', CHORD
      DO 40 I=1,N
        WRITE(*,9200) 'PCOP X = ', X(I)
        WRITE(*,9200) 'PCOP Y = ', Y(I)
        WRITE(*,9200) 'PCOP NX = ', NX(I)
        WRITE(*,9200) 'PCOP APANEL = ', APANEL(I)
   40 CONTINUE
C
C ==== Test 5: ABCOPY removes the duplicated end point ====
      NB = NB + 1
      XB(NB) = XB(NB-1)
      YB(NB) = YB(NB-1)
      CALL ABCOPY(.TRUE.)
      WRITE(*,9100) 'PCOPDUP N = ', N
      DO 50 I=1,N
        WRITE(*,9200) 'PCOPDUP X = ', X(I)
   50 CONTINUE
C
C ==== Test 6: ABCOPY with a 365-point contour (NB = IQX-5) ====
      NSIDE = 183
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL ABCOPY(.TRUE.)
      WRITE(*,9100) 'ABC365 N = ', N
      WRITE(*,9200) 'ABC365 XLE = ', XLE
      WRITE(*,9200) 'ABC365 CHORD = ', CHORD
      DO 60 I=1,N
        WRITE(*,9200) 'ABC365 X = ', X(I)
        WRITE(*,9200) 'ABC365 Y = ', Y(I)
   60 CONTINUE
C
C ==== Test 7: ABCOPY with 366 points (NB > IQX-5): not set ====
      NB = NB + 1
      XB(NB) = XB(NB-1)
      YB(NB) = YB(NB-1)
      CALL ABCOPY(.TRUE.)
      WRITE(*,9100) 'ABC366 N = ', N
      WRITE(*,9200) 'ABC366 XLE = ', XLE
      WRITE(*,9200) 'ABC366 CHORD = ', CHORD
C
C ==== Test 8: NCALC/APCALC blunt (4-point contour) ====
      N = 4
      X(1) = 1.0
      X(2) = 0.5
      X(3) = 0.0
      X(4) = 0.95
      Y(1) = 0.0
      Y(2) = 0.1
      Y(3) = 0.0
      Y(4) = -0.02
      CALL SCALC(X,Y,S,N)
      CALL NCALC(X,Y,S,N,NX,NY)
      SHARP = .FALSE.
      CALL APCALC
      WRITE(*,9100) 'NCALC4 N = ', N
      DO 70 I=1,N
        WRITE(*,9200) 'NCALC4 NX = ', NX(I)
        WRITE(*,9200) 'NCALC4 NY = ', NY(I)
        WRITE(*,9200) 'NCALC4 APANEL = ', APANEL(I)
   70 CONTINUE
C
C ==== Test 9: NCALC/APCALC sharp (X(1)==X(N)) ====
      X(4) = 1.0
      Y(4) = 0.0
      CALL SCALC(X,Y,S,N)
      CALL NCALC(X,Y,S,N,NX,NY)
      SHARP = .TRUE.
      CALL APCALC
      WRITE(*,9100) 'NCALC4S N = ', N
      DO 80 I=1,N
        WRITE(*,9200) 'NCALC4S NX = ', NX(I)
        WRITE(*,9200) 'NCALC4S NY = ', NY(I)
        WRITE(*,9200) 'NCALC4S APANEL = ', APANEL(I)
   80 CONTINUE
C
C ==== Test 10: PANGEN with a duplicated point (corner node insertion)
C ====
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      NSIDE = IQX/3
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      XB(62) = XB(61)
      YB(62) = YB(61)
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANECORN N = ', N
      WRITE(*,9200) 'PANECORN SLE = ', SLE
      WRITE(*,9200) 'PANECORN CHORD = ', CHORD
      DO 90 I=1,N
        WRITE(*,9200) 'PANECORN X = ', X(I)
        WRITE(*,9200) 'PANECORN Y = ', Y(I)
   90 CONTINUE
C
C ==== Test 11: NACA 0012 standard parameters ====
      NSIDE = IQX/3
      CALL NACA4(12,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      XSREF1 = 1.0
      XSREF2 = 1.0
      XPREF1 = 1.0
      XPREF2 = 1.0
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANE12 N = ', N
      WRITE(*,9200) 'PANE12 SLE = ', SLE
      WRITE(*,9200) 'PANE12 XLE = ', XLE
      WRITE(*,9200) 'PANE12 YLE = ', YLE
      WRITE(*,9200) 'PANE12 CHORD = ', CHORD
      DO 100 I=1,N
        WRITE(*,9200) 'PANE12 X = ', X(I)
        WRITE(*,9200) 'PANE12 Y = ', Y(I)
        WRITE(*,9200) 'PANE12 APANEL = ', APANEL(I)
  100 CONTINUE
C
C ==== Test 12: Clark Y standard parameters (Buffer from CLARK_Y.dat) ====
      CLYFILE = 'tests/fortran-reference/cases/'//
     &          'geometry/clark-y-load/input/CLARK_Y.dat'
      OPEN(20, FILE=CLYFILE, STATUS='OLD', ACTION='READ')
      READ(20, 9300) NAME
      NB = 0
      DO
        READ(20, *, END=98) XB(NB+1), YB(NB+1)
        NB = NB + 1
      ENDDO
   98 CLOSE(20)
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANECLY N = ', N
      WRITE(*,9200) 'PANECLY SLE = ', SLE
      WRITE(*,9200) 'PANECLY XLE = ', XLE
      WRITE(*,9200) 'PANECLY YLE = ', YLE
      WRITE(*,9200) 'PANECLY CHORD = ', CHORD
      DO 110 I=1,N
        WRITE(*,9200) 'PANECLY X = ', X(I)
        WRITE(*,9200) 'PANECLY Y = ', Y(I)
        WRITE(*,9200) 'PANECLY APANEL = ', APANEL(I)
  110 CONTINUE
C
C ==== Test 13: refinement regions (XSREF/XPREF not equal 1) ====
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      NPAN = 200
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      XSREF1 = 0.2
      XSREF2 = 0.4
      XPREF1 = 0.2
      XPREF2 = 0.4
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANEREF N = ', N
      WRITE(*,9200) 'PANEREF SLE = ', SLE
      WRITE(*,9200) 'PANEREF CHORD = ', CHORD
      DO 120 I=1,N
        WRITE(*,9200) 'PANEREF X = ', X(I)
        WRITE(*,9200) 'PANEREF Y = ', Y(I)
  120 CONTINUE
C
C ==== Test 14: sharp leading edge (doubled LE point) ====
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      NPAN = 160
      XSREF1 = 1.0
      XSREF2 = 1.0
      XPREF1 = 1.0
      XPREF2 = 1.0
      XB(NSIDE+2) = XB(NSIDE+1)
      YB(NSIDE+2) = YB(NSIDE+1)
      CALL PANGEN(.FALSE.)
      WRITE(*,9100) 'PANESLE N = ', N
      WRITE(*,9200) 'PANESLE SLE = ', SLE
      WRITE(*,9200) 'PANESLE XLE = ', XLE
      WRITE(*,9200) 'PANESLE YLE = ', YLE
      WRITE(*,9200) 'PANESLE CHORD = ', CHORD
      DO 130 I=1,N
        WRITE(*,9200) 'PANESLE X = ', X(I)
        WRITE(*,9200) 'PANESLE Y = ', Y(I)
        WRITE(*,9200) 'PANESLE APANEL = ', APANEL(I)
  130 CONTINUE
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
 9300 FORMAT(A)
      END

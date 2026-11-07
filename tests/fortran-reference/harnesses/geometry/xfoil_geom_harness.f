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

C Fortran harness for xfoil::core::geometry - ROTATE/TECALC (xfoil.f).
C Calls the canonical routines unchanged. ROTATE and TECALC
C live in xfoil.f together with PROGRAM XFOIL; the harness link
C therefore uses -Wl,--allow-multiple-definition and
C -Wl,--unresolved-symbols=ignore-all (see
C tools/build-geometry-harnesses.sh). ROTATE/TECALC call no
C external routines (only intrinsics).
C Important: fixed form; lines remain <= column 72.
      PROGRAM XFOILGEOM_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER IDES, NSIDE, I
      REAL ALFA
C
C ==== ROTATE on NACA 2412 buffer (COMMON XB/YB as xfoil.f NACA) ====
      NSIDE = IQX/3
      IDES = 2412
      CALL NACA4(IDES,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      ALFA = 0.25
      CALL ROTATE(XB,YB,NB,ALFA)
      WRITE(*,9100) 'ROTATE2412 ALFA=0.25 NB = ', NB
      DO 10 I=1,NB
        WRITE(*,9200) '  ROT+ X = ', XB(I)
        WRITE(*,9200) '  ROT+ Y = ', YB(I)
   10 CONTINUE
      CALL ROTATE(XB,YB,NB,-ALFA)
      WRITE(*,9100) 'ROTATE2412 ALFA=-0.25 NB = ', NB
      DO 20 I=1,NB
        WRITE(*,9200) '  ROT- X = ', XB(I)
        WRITE(*,9200) '  ROT- Y = ', YB(I)
   20 CONTINUE
C
C ==== TECALC open Trailing edge ====
      N = 4
      X(1) = 1.0
      X(2) = 0.5
      X(3) = 0.0
      X(4) = 0.95
      Y(1) = 0.0
      Y(2) = 0.1
      Y(3) = 0.0
      Y(4) = -0.02
      XP(1) = -1.0
      XP(2) = -0.8
      XP(3) = 0.5
      XP(4) = 1.0
      YP(1) = 0.0
      YP(2) = 0.4
      YP(3) = 0.8
      YP(4) = 0.5
      GAM(1) = 0.2
      GAM(2) = 0.4
      GAM(3) = 0.6
      GAM(4) = 0.3
      GAM_A(1) = 0.5
      GAM_A(2) = 0.6
      GAM_A(3) = 0.7
      GAM_A(4) = 0.4
      CHORD = 1.0
      CALL TECALC
      WRITE(*,9200) 'TECALC offen DSTE = ', DSTE
      WRITE(*,9200) 'TECALC offen ANTE = ', ANTE
      WRITE(*,9200) 'TECALC offen ASTE = ', ASTE
      WRITE(*,9200) 'TECALC offen SIGTE = ', SIGTE
      WRITE(*,9200) 'TECALC offen GAMTE = ', GAMTE
      WRITE(*,9200) 'TECALC offen SIGTE_A = ', SIGTE_A
      WRITE(*,9200) 'TECALC offen GAMTE_A = ', GAMTE_A
      IF(SHARP) WRITE(*,*) 'TECALC offen SHARP = T'
      IF(.NOT.SHARP) WRITE(*,*) 'TECALC offen SHARP = F'
C
C ==== TECALC sharp trailing edge (X(1)==X(N)) ====
      X(4) = 1.0
      Y(4) = 0.0
      XP(4) = -1.0
      YP(4) = 0.0
      CALL TECALC
      WRITE(*,9200) 'TECALC scharf DSTE = ', DSTE
      WRITE(*,9200) 'TECALC scharf ANTE = ', ANTE
      WRITE(*,9200) 'TECALC scharf ASTE = ', ASTE
      WRITE(*,9200) 'TECALC scharf SIGTE = ', SIGTE
      WRITE(*,9200) 'TECALC scharf GAMTE = ', GAMTE
      WRITE(*,9200) 'TECALC scharf SIGTE_A = ', SIGTE_A
      WRITE(*,9200) 'TECALC scharf GAMTE_A = ', GAMTE_A
      IF(SHARP) WRITE(*,*) 'TECALC scharf SHARP = T'
      IF(.NOT.SHARP) WRITE(*,*) 'TECALC scharf SHARP = F'
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
      END

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

C Fortran harness for the PANGEN guard case at IQX-1 (xfoil.f).
C Calls the canonical routine PANGEN unchanged. The buffer is
C built so that the corner node insertion exceeds the guard limit
C N > IQX-1 = 369 exceeds; the Fortran ends with
C STOP 'PANEL: Too many panels. Increase IQX in XFOIL.INC'
C and terminates with an exit code different from zero. The harness is
C run in the build script with expected error termination.
C
C Important: fixed form; lines remain <= column 72.
      PROGRAM PANGEN_LIMIT_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE
C
C ---- INIT defaults and COMMON initialization (as xfoil.f INIT) ----
      PI = 4.0*ATAN(1.0)
      NPAN = 364
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
C ---- NACA-2412 buffer with doubled points (every second point
C      from point 2; first and last point are not doubled) ------
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      DO 10 I=2, NB-2, 2
        XB(I+1) = XB(I)
        YB(I+1) = YB(I)
   10 CONTINUE
C
C ---- PANGEN: corner node insertion exceeds IQX-1 = 369 -> STOP -
      CALL PANGEN(.FALSE.)
C
      WRITE(*,*) 'PANGEN LIMIT HARNESS: no STOP was triggered'
      STOP
      END

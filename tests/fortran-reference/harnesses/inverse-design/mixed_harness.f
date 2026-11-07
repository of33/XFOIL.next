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

C File: tests/fortran-reference/harnesses/inverse-design/mixed_harness.f
C     Focused Fortran harness for MIXED and GAMLIN of the inverse core.
C Builds a panel geometry (NACA4 2412 -> PANGEN -> APCALC -> NCALC),
C     sets the GAM/QSPEC/PSIO/QDOF manipulators manually and runs
C a MIXED Newton iteration from it (blunt trailing edge, SHARP=.FALSE.).
C     Calls only canonical routines from
C fortran-src/{xqdes,xpanel,xfoil,xgeom,naca,spline,xsolve}.f
C (byte-faithful
C     extracted or unchanged). Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM MIXEDH
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE
      REAL ALFR
      PI = 4.0*ATAN(1.0)
      HOPI = 0.50/PI
      QOPI = 0.25/PI
      SHARP = .FALSE.
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      QINF = 1.0
      MINF = 0.0
      ALFA = 0.0
      LCPXX = .FALSE.
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      CALL APCALC
      CALL NCALC(X,Y,S,N,NX,NY)
      DO 1 I = 1, N
        GAM(I) = COS(6.2831853*(I-1)/FLOAT(N))
        GAM_A(I) = 0.0
        SIG(I) = 0.0
        QSPEC(I,1) = GAM(I)
    1 CONTINUE
      IQ1 = N/4
      IQ2 = 3*N/4
      PSIO = 0.0
      QDOF0 = 0.0
      QDOF1 = 0.0
      QDOF2 = 0.0
      QDOF3 = 0.0
      CALL MIXED(1, 1)
      WRITE(*,1000) PSIO, QDOF0, QDOF1, QDOF2, QDOF3
1000  FORMAT('MHD',5ES22.12)
      DO I = 1, N, N/8
        WRITE(*,1010) I, GAM(I), X(I), Y(I)
1010    FORMAT('MIX',I4,3ES22.12)
      ENDDO
      WRITE(*,1010) N, GAM(N), X(N), Y(N)
      END

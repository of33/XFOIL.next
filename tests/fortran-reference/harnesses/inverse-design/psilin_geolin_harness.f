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

C File: tests/fortran-reference/harnesses/inverse-design/
C psilin_geolin_harness.f
C Focused Fortran harness for PSILIN with GEOLIN=.TRUE. (dPsi/dn,
C     Z_QDOF0..3) of the inverse core. It builds a panel geometry (NACA4 2412 ->
C     PANGEN -> APCALC -> NCALC), sets GAM and the DOF shape functions
C QF0..QF3 manually and calls PSILIN with GEOLIN=.TRUE., SIGLIN=.FALSE..
C     Calls only canonical routines from
C fortran-src/{xfoil,xpanel,xgeom,naca,spline}.f (byte-faithful
C extracted
C     or unchanged). Changes no Fortran source, no raw reference
C     and no golden file.
      PROGRAM PSILGEO
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE, IQ1, IQ2
      REAL PSI, PSIN
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
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      CALL APCALC
      CALL NCALC(X,Y,S,N,NX,NY)
      DO 1 I = 1, N
        GAM(I) = COS(6.2831853*(I-1)/FLOAT(N))
        GAM_A(I) = 0.0
        QF0(I) = 0.0
        QF1(I) = 0.0
        QF2(I) = 0.0
        QF3(I) = 0.0
    1 CONTINUE
      IQ1 = N/4
      IQ2 = 3*N/4
      DO 2 I = IQ1, IQ2
        FS = (S(I)-S(IQ1)) / (S(IQ2)-S(IQ1))
        QF0(I) = 1.0 - FS
        QF1(I) = FS
        QF2(I) = EXP(-5.0*     FS )
        QF3(I) = EXP(-5.0*(1.0-FS))
    2 CONTINUE
      DO 3 I = 1, N
        CALL PSILIN(I,X(I),Y(I),NX(I),NY(I),PSI,PSIN,.TRUE.,.FALSE.)
        IF(MOD(I-1,N/8).EQ.0 .OR. I.EQ.N) THEN
          WRITE(*,1000) I, PSI, PSIN, DZDG(I), DZDN(I),
     &                  Z_QDOF0, Z_QDOF1, Z_QDOF2, Z_QDOF3
        ENDIF
    3 CONTINUE
1000  FORMAT('PG',I4,8ES22.12)
      END

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

C     File: tests/fortran-reference/harnesses/polar-io/plxadd_harness.f
C     Focused Fortran harness for PLXINI (xpol.f:383, new branch)
C     and PLXADD (xpol.f:652). The produced unformatted dump file
C     is the binary reference. Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM PLXAD
      INCLUDE 'XFOIL.INC'
      INTEGER I,IS,IBL
C
C---- small synthetic geometry (TE->LE->TE, N=7)
      N = 7
      X(1) = 1.0 ;  Y(1) = 0.0
      X(2) = 0.5  ;  Y(2) = 0.05
      X(3) = 0.0  ;  Y(3) = 0.0
      X(4) = 0.5  ;  Y(4) = -0.05
      X(5) = 1.0  ;  Y(5) = 0.0
      X(6) = 0.5  ;  Y(6) = 0.02
      X(7) = 1.0  ;  Y(7) = 0.0
      CALL SCALC(X,Y,S,N)
      CALL SEGSPL(X,XP,S,N)
      CALL SEGSPL(Y,YP,S,N)
C
      NAME = 'TEST BL DUMP'
      NNAME = 12
      LVISC = .TRUE.
      LIPAN = .TRUE.
      MINF = 0.3
      MINF1 = 0.3
      REINF1 = 3.0E6
      REINF = 3.0E6
      RETYP = 1
      MATYP = 1
      ACRIT(1) = 9.0
      ACRIT(2) = 8.5
      ALFA = 0.0349
      CL = 0.3
      CD = 0.012
      CM = -0.1
      XOCTR(1) = 0.5
      XOCTR(2) = 0.4
      IBLTE(1) = 4
      IBLTE(2) = 4
      NBL(1) = 5
      NBL(2) = 6
      DO IS = 1, 2
        DO IBL = 1, NBL(IS)
          IPAN(IBL,IS) = MOD(IBL-1,5) + 1
          XSSI(IBL,IS) = 0.2*FLOAT(IBL)
          DSTR(IBL,IS) = 0.001*FLOAT(IBL)
          THET(IBL,IS) = 0.0005*FLOAT(IBL)
          TSTR(IBL,IS) = 0.0002*FLOAT(IBL)
          TAU(IBL,IS) = 0.0001*FLOAT(IBL)
          DIS(IBL,IS) = 0.00001*FLOAT(IBL)
          CTAU(IBL,IS) = 0.00002*FLOAT(IBL)
          UEDG(IBL,IS) = 0.9 + 0.02*FLOAT(IBL)
        ENDDO
      ENDDO
      DO I = 1, N
        GAM(I) = 0.8 + 0.05*FLOAT(I)
        CPV(I) = 1.0 - GAM(I)**2
      ENDDO
      SST = 0.5*(S(1)+S(N))
C
C---- Dump-file creation (PLXINI new branch, filename via stdin)
      PFNAMX(1) = ' '
      CALL PLXINI(18,1)
      WRITE(*,1000) LPFILX, PFNAMX(1)
C
C---- append a point (PLXADD)
      CALL PLXADD(18,1)
C
C---- second point with changed values
      CL = 0.45
      CD = 0.015
      DO IS = 1, 2
        DO IBL = 1, NBL(IS)
          DSTR(IBL,IS) = 0.0012*FLOAT(IBL)
          THET(IBL,IS) = 0.0006*FLOAT(IBL)
          TAU(IBL,IS) = 0.00012*FLOAT(IBL)
        ENDDO
      ENDDO
      CALL PLXADD(18,1)
 1000 FORMAT('LPFILX=',L1,' PFNAMX=',A48)
      END

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

C     File: tests/fortran-reference/harnesses/polar-io/apcopy_harness.f
C     Focused Fortran harness for APCOPY (xpol.f:139).
C
C     Re-supplies the archived CPOLXY geometry and prints the
C     quantities computed from them. Changes no Fortran source,
C     no raw reference and no golden file.
      PROGRAM APCO
      INCLUDE 'XFOIL.INC'
      INTEGER I
      NXYPOL(1) = 9
      NAMEPOL(1) = 'APCOPY TEST'
C
C---- lens TE->LE->TE, 9 points
      X(1) = 1.00 ;  Y(1) = 0.000
      X(2) = 0.75 ;  Y(2) = 0.060
      X(3) = 0.45 ;  Y(3) = 0.080
      X(4) = 0.15 ;  Y(4) = 0.040
      X(5) = 0.00 ;  Y(5) = 0.000
      X(6) = 0.15 ;  Y(6) = -0.040
      X(7) = 0.45 ;  Y(7) = -0.080
      X(8) = 0.75 ;  Y(8) = -0.060
      X(9) = 1.00 ;  Y(9) = 0.000
      DO I = 1, 9
        CPOLXY(I,1,1) = X(I)
        CPOLXY(I,2,1) = Y(I)
      ENDDO
C
C---- current geometry deliberately different
      N = 3
      X(1) = 0.0 ;  Y(1) = 0.0
      X(2) = 0.5 ;  Y(2) = 0.1
      X(3) = 1.0 ;  Y(3) = 0.0
      CALL SCALC(X,Y,S,N)
      LBLINI = .TRUE.
C
      CALL APCOPY(1)
      WRITE(*,1000) N, NAME, LBLINI
      WRITE(*,1010) (X(I),I=1,N)
      WRITE(*,1010) (Y(I),I=1,N)
      WRITE(*,1010) (S(I),I=1,N)
      WRITE(*,1010) (XP(I),I=1,N)
      WRITE(*,1010) (YP(I),I=1,N)
      WRITE(*,1020) SLE,XLE,YLE,XTE,YTE,CHORD
      WRITE(*,1030) (APANEL(I),I=1,N)
 1000 FORMAT('N=',I4,' NAME=',A48,' LBLINI=',L1)
 1010 FORMAT(9F10.5)
 1020 FORMAT('LE/TE',6ES16.8)
 1030 FORMAT('AP ',9F10.5)
      END

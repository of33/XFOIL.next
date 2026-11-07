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

C Fortran harness for xfoil::core::math - linear algebra (xsolve.f).
C Calls the canonical routines from fortran-src/xsolve.f unchanged.
C Prints inputs, outputs and relevant intermediate values with high
C display precision (Fortran REAL = single precision).
C Important: fixed form; lines stay within column 72.
      PROGRAM LINALG_HARNESS
      INTEGER NSIZ, NN, NRHS, N, I, J, L
      REAL Z(4,4), R(4,4), A(4,4), B(4)
      COMPLEX CZ(4,4), CR(4,4)
      INTEGER INDX(4)
C
C ==== GAUSS (3x3, 2 RHS, with pivot swaps) ====
      NSIZ = 4
      NN = 3
      NRHS = 2
      Z(1,1)=0.5; Z(1,2)=1.0; Z(1,3)=0.0
      Z(2,1)=1.0; Z(2,2)=2.0; Z(2,3)=1.0
      Z(3,1)=0.0; Z(3,2)=1.0; Z(3,3)=2.0
      R(1,1)=1.5; R(1,2)=0.5
      R(2,1)=4.0; R(2,2)=2.0
      R(3,1)=3.0; R(3,2)=3.0
      CALL GAUSS(NSIZ, NN, Z, R, NRHS)
      WRITE(*,*) 'GAUSS solution R (col-major print):'
      DO L=1,NRHS
        DO I=1,NN
          WRITE(*,9200) '  R(', I, L, ') = ', R(I,L)
        ENDDO
      ENDDO
C
C ==== CGAUSS (2x2 complex, 1 RHS) ====
      NSIZ = 4
      NN = 2
      NRHS = 1
      CZ(1,1)=(1.0,1.0); CZ(1,2)=(2.0,-1.0)
      CZ(2,1)=(1.0,-1.0); CZ(2,2)=(3.0,2.0)
      CR(1,1)=(3.0,2.0)
      CR(2,1)=(4.0,1.0)
      CALL CGAUSS(NSIZ, NN, CZ, CR, NRHS)
      WRITE(*,*) 'CGAUSS solution CR:'
      DO I=1,NN
        WRITE(*,9300) '  CR(', I, ') = ', CR(I,1)
      ENDDO
C
C ==== LUDCMP + BAKSUB (3x3) ====
      N = 3
      A(1,1)=2.0; A(1,2)=1.0; A(1,3)=1.0
      A(2,1)=1.0; A(2,2)=3.0; A(2,3)=2.0
      A(3,1)=1.0; A(3,2)=2.0; A(3,3)=3.0
      CALL LUDCMP(NSIZ, N, A, INDX)
      WRITE(*,*) 'LUDCMP LU factors A:'
      DO I=1,N
        DO J=1,N
          WRITE(*,9200) '  A(', I, J, ') = ', A(I,J)
        ENDDO
      ENDDO
      WRITE(*,*) 'LUDCMP INDX:'
      DO I=1,N
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
      B(1)=5.0
      B(2)=9.0
      B(3)=9.0
      CALL BAKSUB(NSIZ, N, A, INDX, B)
      WRITE(*,*) 'BAKSUB solution B:'
      DO I=1,N
        WRITE(*,9210) '  B(', I, ') = ', B(I)
      ENDDO
C
      STOP
C
 9100 FORMAT(A, I3, A, I6)
 9200 FORMAT(A, 2(I3), A, E30.20)
 9210 FORMAT(A, I3, A, E30.20)
 9300 FORMAT(A, I3, A, 2(E30.20))
      END

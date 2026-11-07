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

C Fortran harness for xfoil::core::math - sorting routines (sort.f).
C Calls the canonical routines from fortran-src/sort.f unchanged.
C Prints inputs, outputs and relevant intermediate values with high
C display precision (Fortran REAL = single precision).
C Important: fixed form; lines must stay within column 72.
      PROGRAM SORT_HARNESS
      INTEGER N, I, KK, K, NNEW
      REAL A(20), W(20), ATMP(20), TOL
      INTEGER INDX(20)
C
C ==== HSORT ====
      N = 6
      A(1) = 5.0
      A(2) = 2.0
      A(3) = 8.0
      A(4) = 1.0
      A(5) = 9.0
      A(6) = 3.0
      CALL HSORT(N, A, INDX)
      WRITE(*,*) 'HSORT 1 (unsorted input)'
      DO I=1,N
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
C
      A(1) = 1.0
      A(2) = 2.0
      A(3) = 3.0
      A(4) = 4.0
      A(5) = 5.0
      A(6) = 6.0
      CALL HSORT(N, A, INDX)
      WRITE(*,*) 'HSORT 2 (already sorted)'
      DO I=1,N
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
C
      A(1) = 9.0
      A(2) = 7.0
      A(3) = 5.0
      A(4) = 3.0
      A(5) = 1.0
      A(6) = 0.0
      CALL HSORT(N, A, INDX)
      WRITE(*,*) 'HSORT 3 (reversed)'
      DO I=1,N
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
C
      A(1) = 4.0
      A(2) = 2.0
      A(3) = 4.0
      A(4) = 1.0
      A(5) = 2.0
      A(6) = 3.0
      CALL HSORT(N, A, INDX)
      WRITE(*,*) 'HSORT 4 (with duplicates)'
      DO I=1,N
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
C
      A(1) = 7.0
      CALL HSORT(1, A, INDX)
      WRITE(*,*) 'HSORT 5 (N=1)'
      WRITE(*,9100) '  INDX(1) = ', INDX(1)
C
C ==== ASORT ====
      A(1) = 5.0
      A(2) = 2.0
      A(3) = 8.0
      A(4) = 1.0
      A(5) = 9.0
      A(6) = 3.0
      CALL HSORT(N, A, INDX)
      A(1) = 5.0
      A(2) = 2.0
      A(3) = 8.0
      A(4) = 1.0
      A(5) = 9.0
      A(6) = 3.0
      DO I=1,N
        ATMP(I) = 0.0
      ENDDO
      CALL ASORT(N, A, INDX, ATMP)
      WRITE(*,*) 'ASORT (reorder A by HSORT INDX)'
      DO I=1,N
        WRITE(*,9200) '  A(', I, ') = ', A(I)
      ENDDO
C
C ==== REMD ====
      N = 8
      A(1) = 1.0
      A(2) = 1.0
      A(3) = 2.0
      A(4) = 2.0
      A(5) = 2.0
      A(6) = 5.0
      A(7) = 6.0
      A(8) = 6.0
      TOL = 0.0
      CALL REMD(N, A, INDX, TOL, NNEW)
      WRITE(*,*) 'REMD 1 (TOL=0) NNEW=', NNEW
      DO I=1,NNEW
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
C
      N = 5
      A(1) = 1.0
      A(2) = 1.05
      A(3) = 1.1
      A(4) = 1.16
      A(5) = 2.0
      TOL = 0.1
      CALL REMD(N, A, INDX, TOL, NNEW)
      WRITE(*,*) 'REMD 2 (TOL=0.1) NNEW=', NNEW
      DO I=1,NNEW
        WRITE(*,9100) '  INDX(', I, ') = ', INDX(I)
      ENDDO
C
C ==== SORTDUP ====
      KK = 6
      A(1) = 5.0
      A(2) = 2.0
      A(3) = 8.0
      A(4) = 1.0
      A(5) = 9.0
      A(6) = 3.0
      W(1) = 50.0
      W(2) = 20.0
      W(3) = 80.0
      W(4) = 10.0
      W(5) = 90.0
      W(6) = 30.0
      CALL SORTDUP(KK, A, W)
      WRITE(*,*) 'SORTDUP (sorted, no dup removal)'
      DO I=1,KK
        WRITE(*,9300) '  A,W(', I, ') = ', A(I), W(I)
      ENDDO
C
C ==== FIXDUP ====
      KK = 7
      A(1) = 3.0
      A(2) = 3.0
      A(3) = 1.0
      A(4) = 2.0
      A(5) = 2.0
      A(6) = 2.0
      A(7) = 3.0
      W(1) = 30.0
      W(2) = 30.0
      W(3) = 10.0
      W(4) = 20.0
      W(5) = 20.0
      W(6) = 20.0
      W(7) = 30.0
      CALL FIXDUP(KK, A, W)
      WRITE(*,*) 'FIXDUP (leading+trailing+run dups) KK=', KK
      DO I=1,KK
        WRITE(*,9300) '  A,W(', I, ') = ', A(I), W(I)
      ENDDO
C
C ==== SORT ====
      KK = 8
      A(1) = 4.0
      A(2) = 1.0
      A(3) = 1.0
      A(4) = 3.0
      A(5) = 3.0
      A(6) = 2.0
      A(7) = 5.0
      A(8) = 5.0
      W(1) = 40.0
      W(2) = 10.0
      W(3) = 10.0
      W(4) = 30.0
      W(5) = 30.0
      W(6) = 20.0
      W(7) = 50.0
      W(8) = 50.0
      CALL SORT(KK, A, W)
      WRITE(*,*) 'SORT (sort + remove pairs) KK=', KK
      DO I=1,KK
        WRITE(*,9300) '  A,W(', I, ') = ', A(I), W(I)
      ENDDO
C
C ==== SORTOL ====
      KK = 5
      A(1) = 1.0
      A(2) = 1.0
      A(3) = 1.05
      A(4) = 2.0
      A(5) = 2.02
      W(1) = 1.0
      W(2) = 0.0
      W(3) = 1.0
      W(4) = 2.0
      W(5) = 2.0
      TOL = 0.2
      CALL SORTOL(TOL, KK, A, W)
      WRITE(*,*) 'SORTOL (TOL=0.2) KK=', KK
      DO I=1,KK
        WRITE(*,9300) '  A,W(', I, ') = ', A(I), W(I)
      ENDDO
C
      STOP
C
 9100 FORMAT(A, I3, A, I6)
 9200 FORMAT(A, I3, A, E30.20)
 9300 FORMAT(A, I3, A, 2(E30.20))
      END

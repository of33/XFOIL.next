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

C     File: tests/fortran-reference/harnesses/polar-io/polref_harness.f
C     Focused Fortran harness for POLREF (iopol.f:702).
C
C     Reads a reference polar file with label, four blocks and
C     999.0 sentinel and prints the parsed result. Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM POLRF
      INCLUDE 'XFOIL.INC'
      LOGICAL ERROR
      INTEGER K,I
      CALL POLREF(17,'refpolar_input.txt',ERROR,
     &            NFX,NDREF(1,1),CPOLREF(1,1,1,1),LABREF)
      WRITE(*,1000) ERROR
      WRITE(*,1010) LABREF
      WRITE(*,1020) (NDREF(K,1),K=1,4)
      DO K = 1, 4
        DO I = 1, NDREF(K,1)
          WRITE(*,1030) K,I,CPOLREF(I,1,K,1),CPOLREF(I,2,K,1)
        ENDDO
      ENDDO
C
C---- second case: without Label (REWIND path)
      CALL POLREF(17,'refpolar_nolabel.txt',ERROR,
     &            NFX,NDREF(1,2),CPOLREF(1,1,1,2),LABREF)
      WRITE(*,1000) ERROR
      WRITE(*,1010) LABREF
      WRITE(*,1020) (NDREF(K,2),K=1,4)
      DO K = 1, 4
        DO I = 1, NDREF(K,2)
          WRITE(*,1030) K,I,CPOLREF(I,1,K,2),CPOLREF(I,2,K,2)
        ENDDO
      ENDDO
 1000 FORMAT('ERR=',L1)
 1010 FORMAT('LAB ',A80)
 1020 FORMAT('NF ',4I6)
 1030 FORMAT('RP ',2I4,2ES22.12)
      END

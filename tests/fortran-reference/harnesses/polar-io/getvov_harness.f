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

C     File: tests/fortran-reference/harnesses/polar-io/getvov_harness.f
C     Focused Fortran harness for GETVOV (xmdes.f:1769).
C
C     Resamples a V/Vinf-vs-s table (filename via stdin) onto the
C     SSPEC grid and prints QSPEC/QSPECP. Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM GTVOV
      INCLUDE 'XFOIL.INC'
      INTEGER I
      NSP = 21
      DO I = 1, NSP
        SSPEC(I) = 1.0 - 0.05*FLOAT(I-1)
      ENDDO
      MINF = 0.3
      CALL COMSET
      CALL GETVOV(1)
      DO I = 1, NSP
        WRITE(*,1000) I, SSPEC(I), QSPEC(I,1), QSPECP(I,1)
      ENDDO
C
C---- second case: table in reverse order
      CALL GETVOV(2)
      DO I = 1, NSP
        WRITE(*,1010) I, QSPEC(I,2)
      ENDDO
 1000 FORMAT('QV ',I4,3ES22.12)
 1010 FORMAT('QW ',I4,ES22.12)
      END

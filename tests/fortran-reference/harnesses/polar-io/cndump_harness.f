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

C     File: tests/fortran-reference/harnesses/polar-io/cndump_harness.f
C     Focused Fortran harness for CNDUMP (xmdes.f:1747).
C
C     Writes the canonical Cn format (1X,I3,4F11.6) to stdout.
C     Changes no Fortran source, no raw reference and no
C     golden file.
      PROGRAM CNDMP
      INCLUDE 'CIRCLE.INC'
      INTEGER M
      NC = 9
      MC = 3
      MCT = 3
      DO M = 0, MC
        CN(M) = CMPLX(0.1*FLOAT(M), -0.05*FLOAT(M+1))
      ENDDO
      DO M = 1, NC
        PIQ(M) = CMPLX(COS(0.7*FLOAT(M)), SIN(0.7*FLOAT(M)))
      ENDDO
      CALL CNDUMP(6)
      END

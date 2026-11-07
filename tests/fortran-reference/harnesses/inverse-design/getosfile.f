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

C     File: tests/fortran-reference/harnesses/inverse-design/getosfile.f
C Harness glue for OSMAP: returns the path of the Orr-Sommerfeld
C database from the environment variable OSMAP. Not a canonical XFOIL
C source; serves only the harness build (fortran-src/osmap.f calls
C GETOSFILE,
C     which is not contained in fortran-src/). Changes no Fortran source.
      SUBROUTINE GETOSFILE(NAME,LEN)
      CHARACTER*(*) NAME
      INTEGER LEN
      CHARACTER*512 ENAME
      CALL GETENV('OSMAP', ENAME)
      LEN = LEN_TRIM(ENAME)
      IF(LEN.GT.0) NAME(1:LEN) = ENAME(1:LEN)
      RETURN
      END

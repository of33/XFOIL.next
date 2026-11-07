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

C     File: tests/fortran-reference/harnesses/inverse-design/askr.f
C     Harness glue (link-only stub) for ASKR from fortran-src/userio.f.
C DZTSET/AGTSET reference ASKR in the interactive branch (NINPUT <
C 2),
C which the harness never executes. The stub aborts if it
C     is accidentally called, so that the reference data never
C     depend on interactive input.
      SUBROUTINE ASKR(PROMPT, RINPUT)
      CHARACTER*(*) PROMPT
      REAL RINPUT
      WRITE(*,*) 'HARNESS ERROR: ASKR was called'
      STOP 1
      END

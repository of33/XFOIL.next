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

C     File: tests/fortran-reference/harnesses/polar-io/getdef_harness.f
C     Focused Fortran harness for GETDEF (xfoil.f:867) and
C     WRTDEF (xfoil.f:971).
C
C     Cases: complete file (echo via WRTDEF(6)), incomplete
C     file (read-error branch), missing file. Changes
C     no Fortran source, no raw reference and no golden file.
      PROGRAM GETDF
      INCLUDE 'XFOIL.INC'
      CALL GETDEF(10,'getdef_full.def', .FALSE.)
      WRITE(*,1000) NPAN, CVPAR, MINF1, REINF1, MATYP, RETYP,
     &              ACRIT(1), ACRIT(2), XSTRIP(1), XSTRIP(2),
     &              IDEVRP, QINF, TKLAM
      CALL GETDEF(10,'getdef_missing.def', .FALSE.)
      WRITE(*,1000) NPAN, CVPAR, MINF1, REINF1, MATYP, RETYP,
     &              ACRIT(1), ACRIT(2), XSTRIP(1), XSTRIP(2),
     &              IDEVRP, QINF, TKLAM
      CALL WRTDEF(6)
 1000 FORMAT('ST',I5,ES14.6,ES14.6,ES14.6,2I4,4ES12.4,I4,2ES14.6)
      END

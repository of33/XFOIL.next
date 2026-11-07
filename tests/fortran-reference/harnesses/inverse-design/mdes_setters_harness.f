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

C File: tests/fortran-reference/harnesses/inverse-design/
C mdes_setters_harness.f
C Focused Fortran harness for the MDES-TE setter DZTSET and AGTSET
C     of the inverse core (non-interactive branch, NINPUT >= 2).
C
C     Calls only the canonical routines from fortran-src/xmdes.f
C     (extracted byte-faithfully). Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM MDSET
      INCLUDE 'CIRCLE.INC'
      DIMENSION RIN(2)
      RIN(1) = 0.01
      RIN(2) = 0.002
      CALL DZTSET(RIN, 2)
      WRITE(*,1000) REAL(DZTE), AIMAG(DZTE)
1000  FORMAT('DZT',2ES22.12)
      RIN(1) = 15.0
      RIN(2) = 0.0
      CALL AGTSET(RIN, 2)
      WRITE(*,1010) AGTE
1010  FORMAT('AGT',ES22.12)
      RIN(1) = -0.005
      RIN(2) = 0.003
      CALL DZTSET(RIN, 2)
      WRITE(*,1000) REAL(DZTE), AIMAG(DZTE)
      RIN(1) = 0.0
      RIN(2) = 0.0
      CALL AGTSET(RIN, 2)
      WRITE(*,1010) AGTE
      END

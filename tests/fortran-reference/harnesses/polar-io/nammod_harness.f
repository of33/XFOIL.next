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

C     File: tests/fortran-reference/harnesses/polar-io/nammod_harness.f
C     Focused Fortran harness for NAMMOD (xoper.f:1831).
C
C     Covers bracket increments, restarts and the MOD wrap-around at 0,
C     missing brackets with/without KMOD0 and negative MOD results.
C     Changes no Fortran source, no raw reference and no
C     golden file.
      PROGRAM NAMMO
      CHARACTER*80 NAME
      NAME = 'NACA 0012'
      CALL NAMMOD(NAME, 0, 1)
      WRITE(*,1000) NAME
      NAME = 'NACA 0012 [3]'
      CALL NAMMOD(NAME, 1, -1)
      WRITE(*,1000) NAME
      NAME = 'NACA 0012 [99]'
      CALL NAMMOD(NAME, 1, -1)
      WRITE(*,1000) NAME
      NAME = 'NACA 0012 [9]'
      CALL NAMMOD(NAME, 1, -1)
      WRITE(*,1000) NAME
      NAME = 'CLARK Y'
      CALL NAMMOD(NAME, 0, 0)
      WRITE(*,1000) NAME
      NAME = 'CLARK Y'
      CALL NAMMOD(NAME, 0, 12)
      WRITE(*,1000) NAME
      NAME = 'NACA 0012 [10]'
      CALL NAMMOD(NAME, -11, -1)
      WRITE(*,1000) NAME
      NAME = 'NACA 0012 [ 5 ]'
      CALL NAMMOD(NAME, 4, -1)
      WRITE(*,1000) NAME
      NAME = '[7]'
      CALL NAMMOD(NAME, 1, -1)
      WRITE(*,1000) NAME
 1000 FORMAT('NM ',A80)
      END

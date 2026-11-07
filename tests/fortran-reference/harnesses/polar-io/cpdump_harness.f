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

C     File: tests/fortran-reference/harnesses/polar-io/cpdump_harness.f
C     Focused Fortran harness for CPDUMP (xoper.f:2280).
C
C     Writes the canonical .cp table (KDELIM=0,1,2). The files
C     are the reference. Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM CPDMP
      INCLUDE 'XFOIL.INC'
      INTEGER I
      N = 6
      X(1) = 1.00 ;  Y(1) = 0.000
      X(2) = 0.60 ;  Y(2) = 0.050
      X(3) = 0.00 ;  Y(3) = 0.000
      X(4) = 0.60 ;  Y(4) = -0.050
      X(5) = 1.00 ;  Y(5) = 0.000
      X(6) = 0.60 ;  Y(6) = 0.010
      GAM(1) = 0.55
      GAM(2) = 0.90
      GAM(3) = 1.05
      GAM(4) = 0.95
      GAM(5) = 0.70
      GAM(6) = 0.80
      MINF = 0.3
      KDELIM = 0
      CALL CPDUMP('cpdump_case.cp')
      KDELIM = 1
      CALL CPDUMP('cpdump_case_csv.cp')
      KDELIM = 2
      CALL CPDUMP('cpdump_case_tab.cp')
      END

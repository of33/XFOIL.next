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

C     File: tests/fortran-reference/harnesses/polar-io/plradd_harness.f
C     Focused Fortran harness for PLRADD (xpol.f:550).
C
C Calls PLRADD with LVISC=.TRUE. and LVISC=.FALSE. (LPFILE=.FALSE.,
C     no file update) and prints the stored columns.
C     Changes no Fortran source, no raw reference and no
C     golden file.
      PROGRAM PLRAD
      INCLUDE 'XFOIL.INC'
      INTEGER J,K
      LPFILE = .FALSE.
      LFLAP = .FALSE.
      NAPOL(1) = 0
C
C---- viscous case
      LVISC = .TRUE.
      ADEG = 2.5
      CL = 0.31
      CD = 0.0125
      CM = -0.104
      CDP = 0.0071
      CPMN = -4.21
      MINF = 0.3
      REINF = 3.0E6
      REINF1 = 3.0E6
      ACRIT(1) = 9.0
      ACRIT(2) = 8.5
      XSTRIP(1) = 1.0
      XSTRIP(2) = 0.9
      XOCTR(1) = 0.512
      XOCTR(2) = 0.401
      TINDEX(1) = 10.0
      TINDEX(2) = 12.0
      CALL PLRADD(17,1)
C
C---- inviscid case
      LVISC = .FALSE.
      ADEG = 4.0
      CL = 0.55
      CM = -0.12
      CDP = 0.
      CPMN = -5.5
      MINF = 0.2
      CALL PLRADD(17,1)
C
      DO J = 1, NAPOL(1)
        WRITE(*,1040) J,(CPOL(J,K,1),K=1,IPTOT)
        WRITE(*,1050) J,(CPOLSD(J,1,K,1),K=1,JPTOT),
     &                        (CPOLSD(J,2,K,1),K=1,JPTOT)
      ENDDO
 1040 FORMAT('PT ',I4,13ES22.12)
 1050 FORMAT('SD ',I4,8ES22.12)
      END

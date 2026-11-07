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

C     File: tests/fortran-reference/harnesses/polar-io/plrsum_harness.f
C     Focused Fortran harness for PLRSUM (xpol.f:755).
C
C     Prints the canonical polar summary for two slots.
C     Changes no Fortran source, no raw reference and no
C     golden file.
      PROGRAM PLRSU
      INCLUDE 'XFOIL.INC'
      INTEGER J,K
      NPOL = 2
      IPACT = 1
      NAMEPOL(1) = 'NACA 0012'
      NAMEPOL(2) = 'CLARK Y [3]'
      PFNAME(1) = 'naca0012.pol'
      PFNAME(2) = 'clarky.pol'
      PFNAMX(1) = 'naca0012.dump'
      PFNAMX(2) = ' '
      NAPOL(1) = 3
      NAPOL(2) = 2
      REYNP1(1) = 3.0E6
      REYNP1(2) = 0.
      MACHP1(1) = 0.3
      MACHP1(2) = 0.0
      IRETYP(1) = 1
      IRETYP(2) = 1
      IMATYP(1) = 1
      IMATYP(2) = 1
      ACRITP(1,1) = 9.0
      ACRITP(2,1) = 8.5
      ACRITP(1,2) = 9.0
      ACRITP(2,2) = 9.0
      XSTRIPP(1,1) = 1.0
      XSTRIPP(2,1) = 0.9
      XSTRIPP(1,2) = 1.0
      XSTRIPP(2,2) = 1.0
      DO J = 1, 3
        CPOL(J,IAL,1) = -2.0 + 2.0*FLOAT(J-1)
        CPOL(J,ICL,1) = 0.2*FLOAT(J)
        CPOL(J,ICD,1) = 0.01*FLOAT(J)
        CPOL(J,ICM,1) = -0.1*FLOAT(J)
        CPOL(J,ICP,1) = 0.004*FLOAT(J)
        CPOL(J,ICV,1) = 0.006*FLOAT(J)
        CPOL(J,ICW,1) = 0.
        CPOL(J,IMA,1) = 0.3
        CPOL(J,IRE,1) = 3.0E6
        CPOL(J,ICH,1) = 0.
        CPOL(J,IMC,1) = -4.0
        CPOL(J,ICDH,1) = 0.
        CPOL(J,ICMDOT,1) = 0.
      ENDDO
      DO J = 1, 2
        CPOL(J,IAL,2) = 1.0*FLOAT(J)
        CPOL(J,ICL,2) = 0.3*FLOAT(J)
        CPOL(J,ICD,2) = 0.
        CPOL(J,ICM,2) = -0.12*FLOAT(J)
        CPOL(J,ICP,2) = 0.
        CPOL(J,ICV,2) = 0.
        CPOL(J,ICW,2) = 0.
        CPOL(J,IMA,2) = 0.
        CPOL(J,IRE,2) = 0.
        CPOL(J,ICH,2) = 0.
        CPOL(J,IMC,2) = -5.0
        CPOL(J,ICDH,2) = 0.
        CPOL(J,ICMDOT,2) = 0.
      ENDDO
      CALL PLRSUM(1,2,1)
      END

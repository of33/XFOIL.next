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

C     File: tests/fortran-reference/harnesses/polar-io/plrini_harness.f
C     Focused Fortran harness for PLRINI (xpol.f:180).
C
C     Cases: (1) a matching file exists, (2) a file exists with
C     differing parameters + ASKL y, (3) new file, (4) "none".
C     Filenames and answers come from stdin. Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM PLRIN
      INCLUDE 'XFOIL.INC'
      IPOL(1) = IAL
      IPOL(2) = ICL
      IPOL(3) = ICD
      IPOL(4) = ICP
      IPOL(5) = ICM
      NIPOL = 5
      NIPOL0 = 5
      JPOL(1) = JTN
      JPOL(2) = JTI
      NJPOL = 2
C
C---- case 1: parameters differ, ASKL y -> adoption
      NAME = 'CURRENT NAME'
      NNAME = 12
      REINF1 = 1.0E6
      MINF1 = 0.1
      RETYP = 1
      MATYP = 1
      ACRIT(1) = 5.0
      ACRIT(2) = 5.0
      XSTRIP(1) = 1.0
      XSTRIP(2) = 1.0
      PFNAME(1) = 'plrini_case.pol'
      CALL PLRINI(17,1)
      WRITE(*,1000) LPFILE, PFNAME(1)
      WRITE(*,1010) NAME, REINF1, MINF1, RETYP, MATYP,
     &              ACRIT(1), ACRIT(2), XSTRIP(1), XSTRIP(2)
C
C---- case 2: parameters differ, ASKL n -> not available
      NAME = 'OTHER NAME'
      NNAME = 10
      REINF1 = 2.0E6
      PFNAME(1) = 'plrini_case.pol'
      CALL PLRINI(17,1)
      WRITE(*,1000) LPFILE, PFNAME(1)
C
C---- case 3: new file
      PFNAME(1) = ' '
      CALL PLRINI(17,1)
      WRITE(*,1000) LPFILE, PFNAME(1)
C
C---- case 4: none
      PFNAME(1) = 'plrini_case.pol'
      CALL PLRINI(17,1)
      WRITE(*,1000) LPFILE, PFNAME(1)
 1000 FORMAT('== LPFILE=',L1,' PFNAME=',A48)
 1010 FORMAT('== ',A48,2ES14.4,2I4,4ES14.4)
      END

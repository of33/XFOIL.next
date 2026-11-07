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

C     File: tests/fortran-reference/harnesses/polar-io/plxini_harness.f
C     Focused Fortran harness for PLXINI (xpol.f:383).
C
C     Cases: (1) a new dump file, (2) a matching file exists,
C     (3) differing parameters + ASKL n, (4) "none". Filenames and
C     answers come from stdin. Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM PLXIN
      INCLUDE 'XFOIL.INC'
      INTEGER I
      N = 5
      X(1) = 1.0 ;  Y(1) = 0.0
      X(2) = 0.5 ;  Y(2) = 0.06
      X(3) = 0.0 ;  Y(3) = 0.0
      X(4) = 0.5 ;  Y(4) = -0.06
      X(5) = 1.0 ;  Y(5) = 0.0
      CALL SCALC(X,Y,S,N)
      NAME = 'NACA 0012'
      NNAME = 9
      MINF1 = 0.3
      REINF1 = 3.0E6
      RETYP = 1
      MATYP = 1
      ACRIT(1) = 9.0
      ACRIT(2) = 8.5
C
C---- case 1: new file create
      PFNAMX(1) = ' '
      CALL PLXINI(18,1)
      WRITE(*,1000) LPFILX, PFNAMX(1)
C
C---- case 2: present file, Parameter match in agreement
      PFNAMX(1) = ' '
      CALL PLXINI(18,1)
      WRITE(*,1000) LPFILX, PFNAMX(1)
C
C---- case 3: differing parameters, ASKL n
      REINF1 = 1.0E6
      PFNAMX(1) = ' '
      CALL PLXINI(18,1)
      WRITE(*,1000) LPFILX, PFNAMX(1)
C
C---- case 4: none
      PFNAMX(1) = 'plxini_case.dump'
      CALL PLXINI(18,1)
      WRITE(*,1000) LPFILX, PFNAMX(1)
C
C---- case 5: present Fixture dump-file, ASKL y
      PFNAMX(1) = ' '
      CALL PLXINI(18,1)
      WRITE(*,1000) LPFILX, PFNAMX(1)
      WRITE(*,1010) NAME, MINF1, REINF1, RETYP, MATYP,
     &              ACRIT(1), ACRIT(2)
 1000 FORMAT('== LPFILX=',L1,' PFNAMX=',A48)
 1010 FORMAT('== ',A48,2ES14.4,2I4,2ES14.4)
      END

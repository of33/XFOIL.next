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

C     File: tests/fortran-reference/harnesses/polar-io/bldump_harness.f
C     Focused Fortran harness for BLDUMP (xoper.f:1891).
C
C     Writes the canonical .bl table (KDELIM=0) with a set
C     BL state. The file is the reference. Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM BLDMP
      INCLUDE 'XFOIL.INC'
      INTEGER I,IS,IBL
      N = 6
      NW = 2
      X(1) = 1.00 ;  Y(1) = 0.000
      X(2) = 0.60 ;  Y(2) = 0.050
      X(3) = 0.00 ;  Y(3) = 0.000
      X(4) = 0.60 ;  Y(4) = -0.050
      X(5) = 1.00 ;  Y(5) = 0.000
      X(6) = 0.60 ;  Y(6) = 0.010
      S(1) = 0.0 ;  S(2) = 0.4 ;  S(3) = 0.8
      S(4) = 1.2 ;  S(5) = 1.6 ;  S(6) = 2.0
      S(7) = 2.4 ;  S(8) = 2.8
      DO I = 1, N+NW
        XP(I) = 0.0
        YP(I) = 0.0
      ENDDO
      GAM(1) = 0.55
      GAM(2) = 0.90
      GAM(3) = 1.05
      GAM(4) = 0.95
      GAM(5) = 0.70
      GAM(6) = -0.60
      GAM(7) = -0.65
      GAM(8) = -0.70
      MINF = 0.3
      LVISC = .TRUE.
      LIPAN = .TRUE.
      LWAKE = .TRUE.
      KDELIM = 0
      IBLTE(1) = 3
      IBLTE(2) = 4
      NBL(1) = 4
      NBL(2) = 6
      DO IS = 1, 2
        DO IBL = 1, NBL(IS)
          IPAN(IBL,IS) = MOD(IBL-1,5) + 1
          XSSI(IBL,IS) = 0.3*FLOAT(IBL)
          DSTR(IBL,IS) = 0.002*FLOAT(IBL)
          THET(IBL,IS) = 0.0008*FLOAT(IBL)
          TSTR(IBL,IS) = 0.0003*FLOAT(IBL)
          TAU(IBL,IS) = 0.00015*FLOAT(IBL)
          DIS(IBL,IS) = 0.00002*FLOAT(IBL)
          UEDG(IBL,IS) = 0.85 + 0.03*FLOAT(IBL)
        ENDDO
      ENDDO
      CALL BLDUMP('bldump_case.bl')
      KDELIM = 1
      CALL BLDUMP('bldump_case_csv.bl')
      KDELIM = 2
      CALL BLDUMP('bldump_case_tab.bl')
      KDELIM = 3
      CALL BLDUMP('bldump_case_bad.bl')
      LVISC = .FALSE.
      LWAKE = .FALSE.
      KDELIM = 0
      CALL BLDUMP('bldump_case_inviscid.bl')
      END

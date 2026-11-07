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

C     File: tests/fortran-reference/harnesses/polar-io/plrset_harness.f
C     Focused Fortran harness for PLRSET (xpol.f:22).
C
C     Case 1: new slot (IP > NPOL). case 2: existing slot with
C     matching geometry (no ASKI branch). Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM PLRSE
      INCLUDE 'XFOIL.INC'
      INTEGER I,J,K
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
      LCMINP = .FALSE.
      LHMOMP = .FALSE.
C
C---- case 1: new slot
      NPOL = 0
      NAME = 'NACA 0012'
      NNAME = 9
      LVISC = .TRUE.
      RETYP = 1
      MATYP = 1
      MINF1 = 0.3
      REINF1 = 3.0E6
      ACRIT(1) = 9.0
      ACRIT(2) = 8.5
      XSTRIP(1) = 1.0
      XSTRIP(2) = 0.9
      N = 5
      X(1) = 1.0 ;  Y(1) = 0.0
      X(2) = 0.5 ;  Y(2) = 0.06
      X(3) = 0.0 ;  Y(3) = 0.0
      X(4) = 0.5 ;  Y(4) = -0.06
      X(5) = 1.0 ;  Y(5) = 0.0
      CALL SCALC(X,Y,S,N)
      CALL PLRSET(1)
      WRITE(*,1000) NPOL, NAPOL(1), NXYPOL(1)
      WRITE(*,1010) NAMEPOL(1), IRETYP(1), IMATYP(1),
     &              REYNP1(1), MACHP1(1),
     &              ACRITP(1,1), ACRITP(2,1),
     &              XSTRIPP(1,1), XSTRIPP(2,1)
      WRITE(*,1020) (CPOLXY(I,1,1),I=1,NXYPOL(1))
      WRITE(*,1020) (CPOLXY(I,2,1),I=1,NXYPOL(1))
C
C---- case 2: existing slot, geometry agrees
      NAPOL(1) = 2
      DO J = 1, 2
        CPOL(J,IAL,1) = 1.0*FLOAT(J)
        CPOL(J,ICL,1) = 0.2*FLOAT(J)
        CPOL(J,ICD,1) = 0.01*FLOAT(J)
        CPOL(J,ICP,1) = 0.004*FLOAT(J)
        CPOL(J,ICM,1) = -0.1*FLOAT(J)
        CPOLSD(J,1,JNC,1) = 9.0
        CPOLSD(J,1,JTP,1) = 1.0
        CPOLSD(J,1,JTN,1) = 0.5
        CPOLSD(J,1,JTI,1) = 10.0
        CPOLSD(J,2,JNC,1) = 8.5
        CPOLSD(J,2,JTP,1) = 0.9
        CPOLSD(J,2,JTN,1) = 0.4
        CPOLSD(J,2,JTI,1) = 12.0
      ENDDO
      NAMEPOL(1) = 'NACA 0012 [7]'
      IRETYP(1) = 2
      IMATYP(1) = 3
      MACHP1(1) = 0.15
      REYNP1(1) = 6.0E6
      ACRITP(1,1) = 7.0
      ACRITP(2,1) = 6.5
      XSTRIPP(1,1) = 0.8
      XSTRIPP(2,1) = 0.7
      CALL PLRSET(1)
      WRITE(*,1030) NAME, RETYP, MATYP, MINF1, REINF1,
     &              ACRIT(1), ACRIT(2), XSTRIP(1), XSTRIP(2)
 1000 FORMAT('S1 NPOL=',I3,' NAPOL=',I4,' NXYPOL=',I4)
 1010 FORMAT('S1 ',A48,2I3,4ES14.4,4ES14.4)
 1020 FORMAT('S1 ',5F10.4)
 1030 FORMAT('S2 ',A48,I3,I3,7ES14.4)
      END

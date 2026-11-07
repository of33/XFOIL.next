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

C     File: tests/fortran-reference/harnesses/polar-io/polwrit_harness.f
C     Focused Fortran harness for POLWRIT (iopol.f:432).
C
C     Writes canonical polar file formats to stdout (unit 6):
C     header + data, header-only (IA1=0, IA2=-1), data-only (header
C     removed), extended column sets (IMC, ICH) and reference header
C     data. Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM POLWR
      INCLUDE 'XFOIL.INC'
      LOGICAL ERROR
      INTEGER I,J
C
C---- canonical column selection (xfoil.f:656-666)
      IPOL(1) = IAL
      IPOL(2) = ICL
      IPOL(3) = ICD
      IPOL(4) = ICP
      IPOL(5) = ICM
      NIPOL = 5
      JPOL(1) = JTN
      JPOL(2) = JTI
      NJPOL = 2
C
C---- deterministic polar content
      NAPOL(1) = 3
      DO J = 1, 3
        CPOL(J,IAL,1) = 1.0 + 2.0*FLOAT(J-1)
        CPOL(J,ICL,1) = 0.25*FLOAT(J)
        CPOL(J,ICD,1) = 0.01234*FLOAT(J)
        CPOL(J,ICM,1) = -0.1*FLOAT(J)
        CPOL(J,ICW,1) = 0.
        CPOL(J,ICV,1) = 0.005*FLOAT(J)
        CPOL(J,ICP,1) = 0.007*FLOAT(J)
        CPOL(J,IMA,1) = 0.3
        CPOL(J,IRE,1) = 3.0E6
        CPOL(J,ICH,1) = -0.02*FLOAT(J)
        CPOL(J,IMC,1) = -4.0 - FLOAT(J)
        CPOL(J,ICDH,1) = 0.
        CPOL(J,ICMDOT,1) = 0.
        CPOLSD(J,1,JNC,1) = 9.0
        CPOLSD(J,1,JTP,1) = 1.0
        CPOLSD(J,1,JTN,1) = 0.5 + 0.1*FLOAT(J)
        CPOLSD(J,1,JTI,1) = 10.0 + FLOAT(J)
        CPOLSD(J,2,JNC,1) = 8.5
        CPOLSD(J,2,JTP,1) = 0.9
        CPOLSD(J,2,JTN,1) = 0.4 + 0.1*FLOAT(J)
        CPOLSD(J,2,JTI,1) = 12.0 + FLOAT(J)
      ENDDO
      NAMEPOL(1) = 'NACA 0012 [3]'
      REYNP1(1) = 3.0E6
      MACHP1(1) = 0.3
      ACRITP(1,1) = 9.0
      ACRITP(2,1) = 8.5
      XSTRIPP(1,1) = 1.0
      XSTRIPP(2,1) = 0.9
      PTRATP(1) = 0.
      ETAPP(1) = 0.
      IRETYP(1) = 1
      IMATYP(1) = 1
C
C---- case 1: header + all points, canonical columns
      CALL POLWRIT(6,' ',ERROR, .TRUE.,
     &             NAX, 1,NAPOL(1), CPOL(1,1,1), IPOL,NIPOL,
     &             REYNP1(1),MACHP1(1),ACRITP(1,1),XSTRIPP(1,1),
     &             PTRATP(1),ETAPP(1),
     &             NAMEPOL(1), IRETYP(1),IMATYP(1),
     &             ISX,1,CPOLSD(1,1,1,1), JPOL,NJPOL,
     &             'XFOIL',VERSION, .FALSE.)
C
C---- case 2: header-only (IA1=0, IA2=-1)
      CALL POLWRIT(6,' ',ERROR, .TRUE.,
     &             NAX, 0,-1, CPOL(1,1,1), IPOL,NIPOL,
     &             REYNP1(1),MACHP1(1),ACRITP(1,1),XSTRIPP(1,1),
     &             PTRATP(1),ETAPP(1),
     &             NAMEPOL(1),IRETYP(1),IMATYP(1),
     &             ISX,1,CPOLSD(1,1,1,1), JPOL,NJPOL,
     &             'XFOIL',VERSION, .FALSE.)
C
C---- case 3: header removed, only data lines (append semantics)
      CALL POLWRIT(6,' ',ERROR, .FALSE.,
     &             NAX, 2,2, CPOL(1,1,1), IPOL,NIPOL,
     &             REYNP1(1),MACHP1(1),ACRITP(1,1),XSTRIPP(1,1),
     &             PTRATP(1),ETAPP(1),
     &             NAMEPOL(1), IRETYP(1),IMATYP(1),
     &             ISX,1,CPOLSD(1,1,1,1), JPOL,NJPOL,
     &             'XFOIL',VERSION, .FALSE.)
C
C---- case 4: extended columns (IMC, ICH) + pi_p/eta_p
      NIPOL = 7
      IPOL(6) = IMC
      IPOL(7) = ICH
      PTRATP(1) = 0.85
      ETAPP(1) = 0.9
      CALL POLWRIT(6,' ',ERROR, .TRUE.,
     &             NAX, 1,NAPOL(1), CPOL(1,1,1), IPOL,NIPOL,
     &             REYNP1(1),MACHP1(1),ACRITP(1,1),XSTRIPP(1,1),
     &             PTRATP(1),ETAPP(1),
     &             NAMEPOL(1), IRETYP(1),IMATYP(1),
     &             ISX,1,CPOLSD(1,1,1,1), JPOL,NJPOL,
     &             'XFOIL',VERSION, .FALSE.)
C
C---- case 5: Re/Mach types 2 and 3
      IRETYP(1) = 2
      IMATYP(1) = 3
      NIPOL = 5
      PTRATP(1) = 0.
      ETAPP(1) = 0.
      CALL POLWRIT(6,' ',ERROR, .TRUE.,
     &             NAX, 1,NAPOL(1), CPOL(1,1,1), IPOL,NIPOL,
     &             REYNP1(1),MACHP1(1),ACRITP(1,1),XSTRIPP(1,1),
     &             PTRATP(1),ETAPP(1),
     &             NAMEPOL(1), IRETYP(1),IMATYP(1),
     &             ISX,1,CPOLSD(1,1,1,1), JPOL,NJPOL,
     &             'XFOIL',VERSION, .FALSE.)
      END

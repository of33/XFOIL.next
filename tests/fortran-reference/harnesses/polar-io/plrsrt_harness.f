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

C     File: tests/fortran-reference/harnesses/polar-io/plrsrt_harness.f
C     Focused Fortran harness for PLRSRT (xpol.f:733).
C
C     Sorts a polar by IAL and by ICL and prints the
C     permuted point and side data. Changes no Fortran source,
C     no raw reference and no golden file.
      PROGRAM PLRSR
      INCLUDE 'XFOIL.INC'
      INTEGER J,K
      NAPOL(1) = 5
      DO J = 1, 5
        CPOL(J,IAL,1) = 6.0 - 2.0*FLOAT(J)
        CPOL(J,ICL,1) = 0.7 - 0.2*FLOAT(J)
        CPOL(J,ICD,1) = 0.01*FLOAT(J)
        CPOL(J,ICM,1) = -0.1*FLOAT(J)
        CPOL(J,ICP,1) = 0.005*FLOAT(J)
        CPOL(J,ICV,1) = 0.004*FLOAT(J)
        CPOL(J,IMA,1) = 0.3
        CPOL(J,IRE,1) = 3.0E6
        CPOL(J,ICH,1) = 0.
        CPOL(J,IMC,1) = -4.0
        CPOLSD(J,1,JNC,1) = 9.0
        CPOLSD(J,1,JTP,1) = 1.0
        CPOLSD(J,1,JTN,1) = 0.1*FLOAT(J)
        CPOLSD(J,1,JTI,1) = FLOAT(J)
        CPOLSD(J,2,JNC,1) = 8.0
        CPOLSD(J,2,JTP,1) = 0.8
        CPOLSD(J,2,JTN,1) = 0.2*FLOAT(J)
        CPOLSD(J,2,JTI,1) = 2.0*FLOAT(J)
      ENDDO
      CALL PLRSRT(1,IAL)
      WRITE(*,1000) 'AFTER IAL'
      DO J = 1, NAPOL(1)
        WRITE(*,1040) J,(CPOL(J,K,1),K=1,IPTOT)
        WRITE(*,1050) J,(CPOLSD(J,1,K,1),K=1,JPTOT),
     &                        (CPOLSD(J,2,K,1),K=1,JPTOT)
      ENDDO
      CALL PLRSRT(1,ICL)
      WRITE(*,1000) 'AFTER ICL'
      DO J = 1, NAPOL(1)
        WRITE(*,1040) J,(CPOL(J,K,1),K=1,IPTOT)
        WRITE(*,1050) J,(CPOLSD(J,1,K,1),K=1,JPTOT),
     &                        (CPOLSD(J,2,K,1),K=1,JPTOT)
      ENDDO
 1000 FORMAT('== ',A)
 1040 FORMAT('PT ',I4,13ES22.12)
 1050 FORMAT('SD ',I4,8ES22.12)
      END

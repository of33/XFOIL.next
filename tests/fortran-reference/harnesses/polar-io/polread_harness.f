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

C     File: tests/fortran-reference/harnesses/polar-io/polread_harness.f
C     Focused Fortran harness for POLREAD (iopol.f:21).
C
C     Reads the canonical polar reference files (unchanged) and
C     prints the fully parsed result. Changes no
C     Fortran source, no raw reference and no golden file.
      PROGRAM POLRD
      INCLUDE 'XFOIL.INC'
      CHARACTER*128 FNAMES(11)
      LOGICAL ERROR
      INTEGER NBLP(ISX,IPX)
      INTEGER I,J,K,NA,NBL
      FNAMES(1)  = 'polar-inv-0012.pol'
      FNAMES(2)  = 'inv2412.pol'
      FNAMES(3)  = 'polar-inv-clark-y.pol'
      FNAMES(4)  = 'polar-nonconverging.pol'
      FNAMES(5)  = 'polar-state-ascending.pol'
      FNAMES(6)  = 'polar-state-cl-driven.pol'
      FNAMES(7)  = 'polar-state-descending.pol'
      FNAMES(8)  = 'polar-visc-0012.pol'
      FNAMES(9)  = 'visc2412.pol'
      FNAMES(10) = 'polar-visc-2412-re3e6.pol'
      FNAMES(11) = 'polar-visc-clark-y.pol'
C
      DO J = 1, NAX
        DO K = 1, IPTOT
          CPOL(J,K,1) = 0.
        ENDDO
        DO K = 1, JPTOT
          CPOLSD(J,1,K,1) = 0.
          CPOLSD(J,2,K,1) = 0.
        ENDDO
      ENDDO
      DO I = 1, 11
        CALL POLREAD(17,FNAMES(I),ERROR,
     &             NAX,NAPOL(1),CPOL(1,1,1),
     &             REYNP1(1),MACHP1(1),ACRITP(1,1),XSTRIPP(1,1),
     &             PTRATP(1),ETAPP(1),
     &             NAMEPOL(1),IRETYP(1),IMATYP(1),
     &             ISX,NBLP(1,1),CPOLSD(1,1,1,1),
     &             CODEPOL(1),VERSPOL(1) )
        WRITE(*,1000) FNAMES(I), ERROR
C---- canonical unset columns (IPOL=0 -> RINP(0)) normalized to 0
        DO K = 1, IPTOT
          IF(IPOL(K).EQ.0) THEN
            DO J = 1, NAPOL(1)
              CPOL(J,K,1) = 0.
            ENDDO
          ENDIF
        ENDDO
        NA = NAPOL(1)
        WRITE(*,1010) NA, NBLP(1,1), IRETYP(1), IMATYP(1)
        WRITE(*,1020) REYNP1(1),MACHP1(1),ACRITP(1,1),ACRITP(2,1),
     &                XSTRIPP(1,1),XSTRIPP(2,1),
     &                PTRATP(1),ETAPP(1),VERSPOL(1)
        WRITE(*,1030) NAMEPOL(1), CODEPOL(1)
        DO J = 1, NA
          WRITE(*,1040) J,(CPOL(J,K,1),K=1,IPTOT)
          WRITE(*,1050) J,CPOLSD(J,1,JTP,1),CPOLSD(J,1,JTN,1),
     &                        CPOLSD(J,1,JTI,1),
     &                        CPOLSD(J,2,JTP,1),CPOLSD(J,2,JTN,1),
     &                        CPOLSD(J,2,JTI,1)
        ENDDO
      ENDDO
 1000 FORMAT('FILE ',A,' ERROR=',L1)
 1010 FORMAT('HDR NA=',I4,' NBL=',I2,' IRETYP=',I2,' IMATYP=',I2)
 1020 FORMAT('HDR',9ES22.12)
 1030 FORMAT('NAM ',A48,' CODE ',A8)
 1040 FORMAT('PT ',I4,13ES22.12)
 1050 FORMAT('SD ',I4,6ES22.12)
      END

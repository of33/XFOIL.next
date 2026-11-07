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

C     File: tests/fortran-reference/harnesses/polar-io/plrcop_harness.f
C     Focused Fortran harness for PLRCOP (xpol.f:838) and
C     PRFCOP (xpol.f:885). Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM PLRCO
      INCLUDE 'XFOIL.INC'
      INTEGER J,K,IS
      NAPOL(1) = 2
      NAMEPOL(1) = 'NACA 2412'
      CODEPOL(1) = 'XFOIL'
      VERSPOL(1) = 6.99
      PFNAME(1)  = 'naca2412.pol'
      PFNAMX(1)  = 'naca2412.dump'
      MACHP1(1) = 0.2
      REYNP1(1) = 6.0E6
      IMATYP(1) = 1
      IRETYP(1) = 2
      ACRITP(1,1) = 9.0
      ACRITP(2,1) = 8.0
      XSTRIPP(1,1) = 1.0
      XSTRIPP(2,1) = 0.5
      DO J = 1, 2
        DO K = 1, IPTOT
          CPOL(J,K,1) = 0.1*FLOAT(J*100+K)
        ENDDO
        DO K = 1, JPTOT
          CPOLSD(J,1,K,1) = 0.2*FLOAT(J*10+K)
          CPOLSD(J,2,K,1) = 0.3*FLOAT(J*10+K)
        ENDDO
      ENDDO
      NXYPOL(1) = 3
      CPOLXY(1,1,1) = 1.0
      CPOLXY(2,1,1) = 0.5
      CPOLXY(3,1,1) = 0.0
      CPOLXY(1,2,1) = 0.0
      CPOLXY(2,2,1) = 0.1
      CPOLXY(3,2,1) = 0.0
C
      CALL PLRCOP(1,2)
      WRITE(*,1000) NAMEPOL(2), CODEPOL(2), VERSPOL(2)
      WRITE(*,1010) MACHP1(2), REYNP1(2), IMATYP(2), IRETYP(2),
     &              ACRITP(1,2), ACRITP(2,2),
     &              XSTRIPP(1,2), XSTRIPP(2,2), NAPOL(2)
      DO J = 1, NAPOL(2)
        WRITE(*,1040) J,(CPOL(J,K,2),K=1,IPTOT)
        WRITE(*,1050) J,(CPOLSD(J,1,K,2),K=1,JPTOT),
     &                        (CPOLSD(J,2,K,2),K=1,JPTOT)
      ENDDO
      WRITE(*,1020) NXYPOL(2),(CPOLXY(I,1,2),I=1,NXYPOL(2)),
     &                       (CPOLXY(I,2,2),I=1,NXYPOL(2))
C
C---- reference polar copy
      NAMEREF(1) = 'REF POLAR 1'
      NDREF(1,1) = 2
      NDREF(2,1) = 1
      NDREF(3,1) = 0
      NDREF(4,1) = 3
      DO K = 1, 4
        DO IS = 1, 2
          DO J = 1, NDREF(K,1)
            CPOLREF(J,IS,K,1) = 0.5*FLOAT(K*100+IS*10+J)
          ENDDO
        ENDDO
      ENDDO
      CALL PRFCOP(1,2)
      WRITE(*,1030) NAMEREF(2), (NDREF(K,2),K=1,4)
      DO K = 1, 4
        DO IS = 1, 2
          DO J = 1, NDREF(K,2)
            WRITE(*,1060) K,IS,J,CPOLREF(J,IS,K,2)
          ENDDO
        ENDDO
      ENDDO
 1000 FORMAT('H1 ',A48,' CODE ',A8,' VER ',F6.2)
 1010 FORMAT('H2',2ES22.12,2I4,4ES22.12,I4)
 1020 FORMAT('XY ',I4,6ES22.12)
 1030 FORMAT('R1 ',A48,4I6)
 1040 FORMAT('PT ',I4,13ES22.12)
 1050 FORMAT('SD ',I4,8ES22.12)
 1060 FORMAT('RP ',3I4,ES22.12)
      END

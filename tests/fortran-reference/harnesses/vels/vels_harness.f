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

C     File: tests/fortran-reference/harnesses/vels/vels_harness.f
C     focused Fortran harness for the canonical
C     VELS path (xoper.f 1643-1665 Dispatch + PSILIN xpanel.f
C     99-...). The harness calls only the canonical
C     routines (INIT, NACA, SPECAL, VISCAL, PSILIN) unchanged.
C     There are no Fortran sources, references or goldens
C     changed.
C
C     Precision contract: build with -fdefault-real-8
C     -fdefault-double-8 (as fortran-bin/xfoil); unqualified
C     REAL is REAL(8), directly bit-comparable with C++ double.
C
C     Cases (per point: PSILIN outputs V1-V4 and the V5-V6
C     magnitudes derived from u/v with 1PE24.17):
C       A  NACA 0012, inviscid, Alfa 2, Point (0.5,0.1)
C       B  point (0.3,0.0) near stagnation
C       C  point (0.5,0.2) above
C       D  point (0.5,-0.2) below
C       E  point (1.2,0.0) behind the TE
C       F  NACA 2412, inviscid, Alfa 4, Point (0.5,0.1)
C       G  NACA 0012, viscous (VISCAL), Alfa 2, Point (0.5,0.1)
C       H  NACA 0012, inviscid, Alfa -2, Point (0.5,0.1)
      PROGRAM VELSH
      INCLUDE 'XFOIL.INC'
      REAL PSI, UUU, VVV, QQQ, CPP, XX, YY
      INTEGER IP

      CALL INIT

C----- NACA 0012, inviscid point ALFA 2 deg
      CALL NACA(12)
      LVISC = .FALSE.
      ALFA = DTOR*2.0
      ADEG = 2.0
      LALFA = .TRUE.
      QINF = 1.0
      CALL SPECAL

      DO IP = 1, 5
        IF(IP.EQ.1) XX = 0.5
        IF(IP.EQ.2) XX = 0.3
        IF(IP.EQ.3) XX = 0.5
        IF(IP.EQ.4) XX = 0.5
        IF(IP.EQ.5) XX = 1.2
        IF(IP.EQ.1) YY = 0.1
        IF(IP.EQ.2) YY = 0.0
        IF(IP.EQ.3) YY = 0.2
        IF(IP.EQ.4) YY = -0.2
        IF(IP.EQ.5) YY = 0.0
        CALL PSILIN(0,XX,YY,-1.0,0.0,PSI,VVV,.FALSE.,.TRUE.)
        WRITE(*,9010) 'VELS V-PART PSI VVV', PSI, VVV
        CALL PSILIN(0,XX,YY,0.0,1.0,PSI,UUU,.FALSE.,.TRUE.)
        WRITE(*,9010) 'VELS U-PART PSI UUU', PSI, UUU
        QQQ = SQRT(UUU**2 + VVV**2)
        CPP = 1.0 - (UUU**2 + VVV**2)
        WRITE(*,9040) 'VELS ABC U V Q CP', UUU, VVV, QQQ, CPP
      ENDDO

C----- case H: ALFA -2
      ALFA = -DTOR*2.0
      ADEG = -2.0
      CALL SPECAL
      XX = 0.5
      YY = 0.1
      CALL PSILIN(0,XX,YY,-1.0,0.0,PSI,VVV,.FALSE.,.TRUE.)
      WRITE(*,9010) 'VELS H V-PART PSI VVV', PSI, VVV
      CALL PSILIN(0,XX,YY,0.0,1.0,PSI,UUU,.FALSE.,.TRUE.)
      WRITE(*,9010) 'VELS H U-PART PSI UUU', PSI, UUU
      QQQ = SQRT(UUU**2 + VVV**2)
      CPP = 1.0 - (UUU**2 + VVV**2)
      WRITE(*,9040) 'VELS H U V Q CP', UUU, VVV, QQQ, CPP

C----- case F: NACA 2412, ALFA 4
      CALL NACA(2412)
      ALFA = DTOR*4.0
      ADEG = 4.0
      CALL SPECAL
      XX = 0.5
      YY = 0.1
      CALL PSILIN(0,XX,YY,-1.0,0.0,PSI,VVV,.FALSE.,.TRUE.)
      WRITE(*,9010) 'VELS F V-PART PSI VVV', PSI, VVV
      CALL PSILIN(0,XX,YY,0.0,1.0,PSI,UUU,.FALSE.,.TRUE.)
      WRITE(*,9010) 'VELS F U-PART PSI UUU', PSI, UUU
      QQQ = SQRT(UUU**2 + VVV**2)
      CPP = 1.0 - (UUU**2 + VVV**2)
      WRITE(*,9040) 'VELS F U V Q CP', UUU, VVV, QQQ, CPP

C----- case G: viscous state (SIG stays canonical 0)
      LVISC = .TRUE.
      REINF1 = 1.0E6
      ALFA = DTOR*2.0
      ADEG = 2.0
      CALL SPECAL
      CALL VISCAL(ITMAX)
      XX = 0.5
      YY = 0.1
      CALL PSILIN(0,XX,YY,-1.0,0.0,PSI,VVV,.FALSE.,.TRUE.)
      WRITE(*,9010) 'VELS G V-PART PSI VVV', PSI, VVV
      CALL PSILIN(0,XX,YY,0.0,1.0,PSI,UUU,.FALSE.,.TRUE.)
      WRITE(*,9010) 'VELS G U-PART PSI UUU', PSI, UUU
      QQQ = SQRT(UUU**2 + VVV**2)
      CPP = 1.0 - (UUU**2 + VVV**2)
      WRITE(*,9040) 'VELS G U V Q CP', UUU, VVV, QQQ, CPP

 9010 FORMAT(A,1P,2(1X,1PE24.17))
 9040 FORMAT(A,1P,4(1X,1PE24.17))
      END

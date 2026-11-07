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

C     File: tests/fortran-reference/harnesses/blf/blf_harness.f
C     focused Fortran harness for the canonical
C     BLF path (xoper.f 1547-1600 + dplot.f FBLGET 320-503).
C     The harness calls only the canonical routines
C     (INIT, NACA, SPECAL, VISCAL, SINVRT, SEVAL, DEVAL, FBLGET)
C     unchanged. No Fortran sources, reference data or golden files
C     are changed.
C
C     Precision contract: build with -fdefault-real-8
C     -fdefault-double-8 (as fortran-bin/xfoil); unqualified
C     REAL is REAL(8), directly bit-comparable with C++ double.
C
C     Cases:
C       A  NACA 0012, VISC 1e6, ALFA 2, DPR=0.01 (default case)
C       B  DPR=0.02
C       C  DPR=0.015 (a further safe positive value)
C       D  NACA 2412, XSTRIP(1)=0.05, XSTRIP(2)=0.5, DPR=0.01
C          (different transition locations)
C       E  NACA 0012, VISCAL(1): present, but not converged
C          BL solution, DPR=0.01
C     Per case and page the B0-B4 quantities are printed with 1PE24.15;
C     FBLGET writes its canonical format-9100 and profile-dump lines
C     itself; the final FPR value (B7) is printed with 1PE24.15.
      PROGRAM BLFH
      INCLUDE 'XFOIL.INC'
      REAL XPR(2), YPR(2), FPR(2)
      REAL DPRV(3)
      REAL SPR, XPRI, DOFF, XOC, SGN
      INTEGER IPR, IDCASE

      CALL INIT

C----- case A-C: NACA 0012, VISC 1e6, ALFA 2 deg
      CALL NACA(12)
      LVISC = .TRUE.
      REINF1 = 1.0E6
      ALFA = DTOR*2.0
      ADEG = 2.0
      LALFA = .TRUE.
      QINF = 1.0
      CALL SPECAL
      WRITE(*,9000) 'DBG SPECAL QINV_CL', QINV(IQX), CL
      CALL VISCAL(ITMAX)
      DOFF = 0.00001*(S(N)-S(1))
      WRITE(*,9000) 'CASE A-C NACA0012 SLE', SLE
      WRITE(*,9000) 'CASE A-C DOFF', DOFF
      DPRV(1) = 0.01
      DPRV(2) = 0.02
      DPRV(3) = 0.015
      DO IDCASE = 1, 3
        DO IPR = 1, 2
          XOC = 0.4
          IF(IPR.EQ.1) SGN = 1.0
          IF(IPR.EQ.2) SGN = -1.0
          IF(SGN.GT.0.0) THEN
            SPR = SLE + (S(1)-SLE)*XOC
          ELSE
            SPR = SLE + (S(N)-SLE)*XOC
          ENDIF
          XPRI = XLE + (XTE-XLE)*XOC
          CALL SINVRT(SPR,XPRI,X,XP,S,N)
          XPR(IPR) = SEVAL(SPR,X,XP,S,N) + DOFF*DEVAL(SPR,Y,YP,S,N)
          YPR(IPR) = SEVAL(SPR,Y,YP,S,N) - DOFF*DEVAL(SPR,X,XP,S,N)
          WRITE(*,9000) 'CASE ABC IPR SPR XPRI', REAL(IPR), SPR, XPRI
          WRITE(*,9000) 'CASE ABC XPR YPR', XPR(IPR), YPR(IPR)
          CALL FBLGET(XPR(IPR),YPR(IPR),DPRV(IDCASE),FPR(IPR))
          WRITE(*,9000) 'CASE ABC FPR', FPR(IPR)
        ENDDO
      ENDDO

C----- case D: NACA 2412 with different XSTRIP values
      LVISC = .FALSE.
      CALL NACA(2412)
      LVISC = .TRUE.
      REINF1 = 1.0E6
      XSTRIP(1) = 0.05
      XSTRIP(2) = 0.5
      ALFA = DTOR*2.0
      ADEG = 2.0
      LALFA = .TRUE.
      QINF = 1.0
      CALL SPECAL
      CALL VISCAL(ITMAX)
      DOFF = 0.00001*(S(N)-S(1))
      WRITE(*,9000) 'CASE D NACA2412 SLE', SLE
      WRITE(*,9000) 'CASE D DOFF', DOFF
      DO IPR = 1, 2
        XOC = 0.4
        IF(IPR.EQ.1) SGN = 1.0
        IF(IPR.EQ.2) SGN = -1.0
        IF(SGN.GT.0.0) THEN
          SPR = SLE + (S(1)-SLE)*XOC
        ELSE
          SPR = SLE + (S(N)-SLE)*XOC
        ENDIF
        XPRI = XLE + (XTE-XLE)*XOC
        CALL SINVRT(SPR,XPRI,X,XP,S,N)
        XPR(IPR) = SEVAL(SPR,X,XP,S,N) + DOFF*DEVAL(SPR,Y,YP,S,N)
        YPR(IPR) = SEVAL(SPR,Y,YP,S,N) - DOFF*DEVAL(SPR,X,XP,S,N)
        WRITE(*,9000) 'CASE D IPR SPR XPRI', REAL(IPR), SPR, XPRI
        WRITE(*,9000) 'CASE D XPR YPR', XPR(IPR), YPR(IPR)
        CALL FBLGET(XPR(IPR),YPR(IPR),0.01,FPR(IPR))
        WRITE(*,9000) 'CASE D FPR', FPR(IPR)
      ENDDO
      XSTRIP(1) = 1.0
      XSTRIP(2) = 1.0

C----- case E: non-converged BL solution (VISCAL(1))
      LVISC = .FALSE.
      CALL NACA(12)
      LVISC = .TRUE.
      REINF1 = 1.0E6
      ALFA = DTOR*2.0
      ADEG = 2.0
      LALFA = .TRUE.
      QINF = 1.0
      CALL SPECAL
      CALL VISCAL(1)
      DOFF = 0.00001*(S(N)-S(1))
      WRITE(*,9000) 'CASE E NONCONV SLE', SLE
      WRITE(*,9000) 'CASE E DOFF', DOFF
      DO IPR = 1, 2
        XOC = 0.4
        IF(IPR.EQ.1) SGN = 1.0
        IF(IPR.EQ.2) SGN = -1.0
        IF(SGN.GT.0.0) THEN
          SPR = SLE + (S(1)-SLE)*XOC
        ELSE
          SPR = SLE + (S(N)-SLE)*XOC
        ENDIF
        XPRI = XLE + (XTE-XLE)*XOC
        CALL SINVRT(SPR,XPRI,X,XP,S,N)
        XPR(IPR) = SEVAL(SPR,X,XP,S,N) + DOFF*DEVAL(SPR,Y,YP,S,N)
        YPR(IPR) = SEVAL(SPR,Y,YP,S,N) - DOFF*DEVAL(SPR,X,XP,S,N)
        WRITE(*,9000) 'CASE E IPR SPR XPRI', REAL(IPR), SPR, XPRI
        WRITE(*,9000) 'CASE E XPR YPR', XPR(IPR), YPR(IPR)
        CALL FBLGET(XPR(IPR),YPR(IPR),0.01,FPR(IPR))
        WRITE(*,9000) 'CASE E FPR', FPR(IPR)
      ENDDO

 9000 FORMAT(A,1PE24.15,1PE24.15,1PE24.15)
      END

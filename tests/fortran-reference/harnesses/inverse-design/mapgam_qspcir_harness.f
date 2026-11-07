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

C File: tests/fortran-reference/harnesses/inverse-design/
C mapgam_qspcir_harness.f
C Focused Fortran harness for MAPGAM and QSPCIR of the inverse core.
C
C     Builds the full MDES forward state (NACA4 2412 -> SCALC ->
C     SPLIND x2 -> panel COMMON via SETPANEL/LEFIND -> EIWSET -> SCINIT ->
C CNCALC -> ZCCALC -> ZCNORM) and then calls MAPGAM and QSPCIR via the
C XFOIL.INC glue units. The main unit INCLUDEs only CIRCLE.INC,
C because
C XFOIL.INC and CIRCLE.INC share the symbol PI in two COMMON blocks.
C     Calls only canonical routines from
C fortran-src/{xmdes,xgeom,spline,naca}.f (extracted byte-faithfully or
C     unchanged). Changes no Fortran source, no raw reference
C     and no golden file.
      PROGRAM MAPQSP
      INCLUDE 'CIRCLE.INC'
      INTEGER NB, NSIDE, IC, M, NSPO
      REAL SLE, SSPLEV, ALG, CLG, CMG, ALQ, CLQ, CMQ
      CHARACTER*10 NAME
      DIMENSION XB(400),YB(400),SB(400),XBP(400),YBP(400)
      DIMENSION QC(ICX)
      DIMENSION W1(400),W2(400),W3(400)
      DIMENSION SP(400),QG(400),XS(400),YS(400),QS(400),QSP(400)
      NSIDE = 123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SPLIND(XB,XBP,SB,NB,-999.0,-999.0)
      CALL SPLIND(YB,YBP,SB,NB,-999.0,-999.0)
      CALL SETPANEL(NB,XB,YB,XBP,YBP,SB,SLE)
      CALL EIWSET(257)
      CALL SCINIT(NB,XB,XBP,YB,YBP,SB,SLE)
      DO IC = 1, NC
        QC(IC) = COS(0.5*WC(IC))
      ENDDO
      CALL CNCALC(QC,.FALSE.)
      CALL ZCCALC(MCT)
      CALL ZCNORM(MCT)
      ALG = 0.1
      CLG = 0.0
      CMG = 0.0
      CALL RUNMAP(1, ALG, CLG, CMG, NSPO, SP, QG, XS, YS, SSPLEV)
      WRITE(*,1000) NSPO, SSPLEV
1000  FORMAT('MPH',I6,ES22.12)
      WRITE(*,1005) ALG, CLG, CMG
1005  FORMAT('MGA',3ES22.12)
      DO IC = 1, NSPO, 8
        WRITE(*,1010) IC, SP(IC), QG(IC), XS(IC), YS(IC)
1010    FORMAT('MPS',I4,4ES22.12)
      ENDDO
      ALG = 0.1
      CLG = -6.0
      CMG = 0.0
      CALL RUNMAP(2, ALG, CLG, CMG, NSPO, SP, QG, XS, YS, SSPLEV)
      WRITE(*,1015) NSPO, SSPLEV
1015  FORMAT('M2H',I6,ES22.12)
      WRITE(*,1016) ALG, CLG, CMG
1016  FORMAT('M2A',3ES22.12)
      DO IC = 1, NSPO, 8
        WRITE(*,1017) IC, SP(IC), QG(IC), XS(IC), YS(IC)
1017    FORMAT('M2S',I4,4ES22.12)
      ENDDO
      CALL RUNQSP(1, 0.1, 0.0, NSPO, QS, QSP, ALQ, CLQ, CMQ)
      WRITE(*,1025) ALQ, CLQ, CMQ
1025  FORMAT('QGA',3ES22.12)
      DO IC = 1, NSPO, 8
        WRITE(*,1020) IC, QS(IC), QSP(IC)
1020    FORMAT('QSC',I4,2ES22.12)
      ENDDO
      CALL RUNQSP(2, 0.1, -6.0, NSPO, QS, QSP, ALQ, CLQ, CMQ)
      WRITE(*,1030) ALQ, CLQ, CMQ
1030  FORMAT('Q2A',3ES22.12)
      DO IC = 1, NSPO, 8
        WRITE(*,1031) IC, QS(IC), QSP(IC)
1031    FORMAT('Q2S',I4,2ES22.12)
      ENDDO
      END

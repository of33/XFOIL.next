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
C qcalc_mapgen_harness.f
C Focused Fortran harness for the circle-level proximity and
C map-generation
C kernels of the inverse core: QCCALC, QSPINT, MAPGEN.
C
C     Calls only canonical routines from
C     fortran-src/{xmdes,xgeom,spline,naca,xsolve}.f. Changes no
C     Fortran source, no raw reference and no golden file.
C
C     Sequence: NACA4(2412) -> SCALC -> SPLIND x2 -> LEFIND -> EIWSET(257) ->
C SCINIT (fills circle state + XCOLD/YCOLD). Afterwards QCCALC(ISPEC=1,
C Alfa=0.1, MINF=0) for Cl/CM/q(w); QSPINT with q(w) as QSPEC; MAPGEN(0)
C     for the new geometry.
      PROGRAM QCALCMAPGEN
      INCLUDE 'CIRCLE.INC'
      INTEGER NB, NSIDE, IC, M, NCIR, NNEW
      REAL SLE, ALFA, CL, CM, MINF, QINF, ALQSP, CLQSP, CMQSP
      CHARACTER*10 NAME
      DIMENSION XB(400),YB(400),SB(400),XBP(400),YBP(400)
      DIMENSION W1(400),W2(400),W3(400)
      DIMENSION XCIR(ICX),YCIR(ICX),SCIR(ICX),QCIR(ICX)
      DIMENSION XN(ICX),YN(ICX)
      NSIDE = 123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SPLIND(XB,XBP,SB,NB,-999.0,-999.0)
      CALL SPLIND(YB,YBP,SB,NB,-999.0,-999.0)
      CALL LEFIND(SLE,XB,XBP,YB,YBP,SB,NB)
      CALL EIWSET(257)
      CALL SCINIT(NB,XB,XBP,YB,YBP,SB,SLE)
      MINF = 0.0
      QINF = 1.0
      ALFA = 0.1
      CL = 0.0
      CM = 0.0
      CALL QCCALC(1,ALFA,CL,CM,MINF,QINF,NCIR,XCIR,YCIR,SCIR,QCIR)
      WRITE(*,1000) NCIR, ALFA, CL, CM
1000  FORMAT('QC',I6,3ES22.12)
      DO IC = 1, 6
        WRITE(*,1010) IC, QCIR(IC), SCIR(IC), XCIR(IC), YCIR(IC)
      ENDDO
1010  FORMAT('QCS',I4,4ES22.12)
      DO IC = NC-2, NC
        WRITE(*,1020) IC, QCIR(IC), SCIR(IC), XCIR(IC), YCIR(IC)
      ENDDO
1020  FORMAT('QCSL',I4,4ES22.12)
      ALQSP = 0.1
      CLQSP = 0.0
      CMQSP = 0.0
      CALL QSPINT(ALQSP,QCIR,QINF,MINF,CLQSP,CMQSP)
      WRITE(*,1030) CLQSP, CMQSP
1030  FORMAT('QSP',2ES22.12)
      NNEW = 0
      CALL MAPGEN(0.0,NNEW,XN,YN)
      WRITE(*,1040) NNEW
1040  FORMAT('MGN',I6)
      DO IC = 1, 6
        WRITE(*,1050) IC, XN(IC), YN(IC)
      ENDDO
1050  FORMAT('MGX',I4,2ES22.12)
      DO IC = NC-2, NC
        WRITE(*,1060) IC, XN(IC), YN(IC)
      ENDDO
1060  FORMAT('MGXL',I4,2ES22.12)
      DO M = 0, 6
        WRITE(*,1070) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1070  FORMAT('MGCN',I4,2ES22.12)
      END

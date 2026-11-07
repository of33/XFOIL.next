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

C File:
C tests/fortran-reference/harnesses/specfailure/specfailure_harness.f
C Focused canonical harness for the SPECAL/SPECCL
C convergence aborts (xoper.f:2799, xoper.f:2874).
C
C The harness calls only the canonical routines from
C fortran-src/{naca,spline,xutils,xgeom,xfoil,xgdes,xpanel,xoper,xsolve}.f
C unchanged. No Fortran sources, raw references or
C golden files are changed.
C
C Proof target:
C  - SPECAL writes 'SPECAL:  Minf convergence failed' and jumps back
C    to the completion block (OFS is still written);
C  - SPECCL writes 'SPECCL:  Cl convergence failed', writes no OFS
C    and skips TECALC/QISET/CPCALC;
C  - the COMMON state after the error is written explicitly
C    (ALFA, CL, CM, CDP, MINF, REINF, GAM(1), QINV(1), LVCONV, LWAKE).
C
C The SPECAL line is not stably reachable in the interactive
C OPER path of the canonical binary (Mach forces MATYP=1;
C MTYPE 2 + Mach 0.99 converges). The harness provides the
C canonical gate state (MATYP=2, MINF1=0.99) directly here so
C that the text contract of the WRITE is evidenced.
      PROGRAM SPECFHARNESS
      INCLUDE 'XFOIL.INC'
C
      NSIDE = IQX/3
      NPAN = 160
      CALL NACA4(12,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      MINF1 = 0.0
      REINF1 = 3.0E6
      MATYP = 1
      RETYP = 1
      CALL GGCALC
C
C ==== Test 1: SPECAL Minf convergence abort (MATYP=2, MINF1=0.99) ====
      MATYP = 2
      RETYP = 1
      MINF1 = 0.99
      ALFA = 0.0
      LVCONV = .FALSE.
      LWAKE = .FALSE.
      CALL SPECAL
      WRITE(*,9100) 'SPECF-ALFA ALFA = ', ALFA
      WRITE(*,9100) 'SPECF-ALFA CL = ', CL
      WRITE(*,9100) 'SPECF-ALFA CM = ', CM
      WRITE(*,9100) 'SPECF-ALFA CDP = ', CDP
      WRITE(*,9100) 'SPECF-ALFA MINF = ', MINF
      WRITE(*,9100) 'SPECF-ALFA REINF = ', REINF
      WRITE(*,9100) 'SPECF-ALFA GAM1 = ', GAM(1)
      WRITE(*,9100) 'SPECF-ALFA QINV1 = ', QINV(1)
      WRITE(*,9200) 'SPECF-ALFA LVCONV = ', LVCONV
      WRITE(*,9200) 'SPECF-ALFA LWAKE = ', LWAKE
C
C ==== Test 2: SPECCL CL convergence abort (CLSPEC = 1.0E6) ====
      MATYP = 1
      RETYP = 1
      MINF1 = 0.0
      ALFA = 0.0
      CLSPEC = 1.0E6
      LVCONV = .FALSE.
      LWAKE = .FALSE.
      CALL SPECCL
      WRITE(*,9100) 'SPECF-CL ALFA = ', ALFA
      WRITE(*,9100) 'SPECF-CL CL = ', CL
      WRITE(*,9100) 'SPECF-CL CM = ', CM
      WRITE(*,9100) 'SPECF-CL CDP = ', CDP
      WRITE(*,9100) 'SPECF-CL MINF = ', MINF
      WRITE(*,9100) 'SPECF-CL GAM1 = ', GAM(1)
      WRITE(*,9100) 'SPECF-CL QINV1 = ', QINV(1)
      WRITE(*,9200) 'SPECF-CL LVCONV = ', LVCONV
      WRITE(*,9200) 'SPECF-CL LWAKE = ', LWAKE
9100  FORMAT(1X,A,F14.6)
9200  FORMAT(1X,A,L2)
      END

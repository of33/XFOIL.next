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

C     File: tests/fortran-reference/harnesses/fmom/fmom_harness.f
C     focused Fortran harness for the canonical
C     FMOM path (xoper.f 1617-1627 Dispatch + MHINGE xoper.f
C     2350-2487 + GETXYF xgdes.f 1880-1906).
C     The harness calls only the canonical routines
C     (INIT, NACA, SPECAL, VISCAL, SINVRT, SEVAL, MHINGE) unchanged
C     on. There are no Fortran sources, references or goldens
C     changed.
C
C     Precision contract: build with -fdefault-real-8
C     -fdefault-double-8 (as fortran-bin/xfoil); unqualified
C     REAL is REAL(8), directly bit-comparable with C++ double.
C
C     Cases:
C       A  NACA 0012, inviscid, ALFA 2, Hinge (0,0)
C       B  NACA 0012, inviscid, ALFA 2, Hinge (0.7,-0.05)
C       C  NACA 0012, viscous (VISCAL), Hinge (0.7,-0.05)  [CPV branch]
C       D  GETXYF estimate path: LFLAP=false, XOF=0.7, YOF=0.0
C          (GETXYF without prompts, since neither -999 nor 999)
C       E  Hinge near upper side (0.5,0.06) and near lower side
C          (0.5,-0.06), inviscid
C       F  NACA 2412 asymmetric, inviscid, Hinge (0.7,-0.05)
C       G  TE base contribution nonzero (NACA 0012 finite thickness;
C          identical inputs as B with separate M1 output)
C     Each case prints TOPS/BOTS (M1) with 1PE24.17 output, before
C     MHINGE is called; the final results HMOM/HFX/HFY (M7)
C     be with 1PE24.17 output.
      PROGRAM FMOMH
      INCLUDE 'XFOIL.INC'
      REAL TT, BB

      CALL INIT

C----- NACA 0012, inviscid point ALFA 2 deg
      CALL NACA(12)
      LVISC = .FALSE.
      ALFA = DTOR*2.0
      ADEG = 2.0
      LALFA = .TRUE.
      QINF = 1.0
      CALL SPECAL

C----- case A: Hinge (0,0), LFLAP=true estimate path
      LFLAP = .TRUE.
      XOF = 0.0
      YOF = 0.0
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE A TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE A HMOM HFX HFY', HMOM, HFX, HFY

C----- case B: Hinge (0.7,-0.05)
      XOF = 0.7
      YOF = -0.05
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE B TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE B HMOM HFX HFY', HMOM, HFX, HFY

C----- case E: hinge near upper side and near underside
      XOF = 0.5
      YOF = 0.06
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE E1 TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE E1 HMOM HFX HFY', HMOM, HFX, HFY
      XOF = 0.5
      YOF = -0.06
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE E2 TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE E2 HMOM HFX HFY', HMOM, HFX, HFY

C----- case G: TE basis reported separately (identical geometry
C     as case B; the SINVRT output prefix serves the
C     comparison boundary for M1)
      XOF = 0.7
      YOF = -0.05
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE G TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE G HMOM HFX HFY', HMOM, HFX, HFY

C----- case D: GETXYF estimate path (LFLAP=false, no Prompts)
      LFLAP = .FALSE.
      XOF = 0.7
      YOF = 0.0
      WRITE(*,9000) 'CASE D XOF YOF', XOF, YOF
      CALL MHINGE
      WRITE(*,9010) 'CASE D HMOM HFX HFY', HMOM, HFX, HFY

C----- case C: viscous CPV branch
      LVISC = .TRUE.
      REINF1 = 1.0E6
      CALL SPECAL
      CALL VISCAL(ITMAX)
      LFLAP = .TRUE.
      XOF = 0.7
      YOF = -0.05
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE C TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE C HMOM HFX HFY', HMOM, HFX, HFY

C----- case F: asymmetric airfoil NACA 2412, inviscid
      LVISC = .FALSE.
      CALL NACA(2412)
      ALFA = DTOR*2.0
      ADEG = 2.0
      CALL SPECAL
      LFLAP = .TRUE.
      XOF = 0.7
      YOF = -0.05
      TT = XOF
      BB = S(N) - XOF
      CALL SINVRT(TT,XOF,X,XP,S,N)
      CALL SINVRT(BB,XOF,X,XP,S,N)
      WRITE(*,9000) 'CASE F TOPS BOTS', TT, BB
      CALL MHINGE
      WRITE(*,9010) 'CASE F HMOM HFX HFY', HMOM, HFX, HFY

 9000 FORMAT(A,1P,2(1X,1PE24.17))
 9010 FORMAT(A,1P,3(1X,1PE24.17))
      END

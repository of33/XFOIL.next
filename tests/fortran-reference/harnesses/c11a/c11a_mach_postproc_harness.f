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

      PROGRAM C11A_MACH_POSTPROC_HARNESS
C--------------------------------------------------------------------
C     Mach postprocessing harness (Clark Y, ABCOPY no-PANE):
C     isolated the immediate canonical side effects of the
C     OPER command Mach (xoper.f 299-334) on an already present
C     point state, without a new solver run.
C
C     Mach leads after MINF1 assignment from:
C       MRCL(1.0,MINF_CL,REINF_CL)   (xoper.f 322)
C       COMSET                        (xoper.f 323)
C       CPCALC(N,QINV,QINF,MINF,CPI)  (xoper.f 328)
C       IF(LVISC) CPCALC(N+NW,QVIS,QINF,MINF,CPV) (xoper.f 329)
C       CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF,XCMREF,YCMREF,
C              CL,CM,CDP,CL_ALF,CL_MSQ)            (xoper.f 330)
C       CDCALC                        (xoper.f 332)
C       LVCONV = .FALSE.              (xoper.f 333)
C     No SPECAL/SPECCL/VISCAL. GAM/QINV/QVIS/TAU/Wake/Mapping remain
C     are retained; only the derived final/Cp quantities are re-evaluated.
C
C     Scenarios (each independent after COMMON new initialization):
C       C  viscous converged alpha point (Re=1e6, 2 deg, Mach=0),
C          then Mach 0.3, then the identical Mach 0.3.
C       B  inviscid alpha point (2 deg, Mach=0), then Mach 0.3.
C       A  geometry/panel without analysis point (GAM=0), then Mach 0.3.
C
C     Output per step (It24.16): Cl/CM/Cd/CDF/CDP, MINF, LVCONV,
C     Cp minimum of the airfoil nodes (CPMINI, manual min over CPI(1..N),
C     Cp influence indicator only; no FCPMIN call). Canonical
C     build flags -fdefault-real-8 -fdefault-double-8. No changes to the
C     canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, NBREAD, IERR, ISTEP, NIT, MAXSTEP
      CHARACTER ANAME*32
      EXTERNAL INITCOMMON
      EXTERNAL MACHPOST
      EXTERNAL PRINTPOINT
C
C ==== Read Clark Y into the buffer (once) ====
      OPEN(UNIT=20, FILE='CLARK_Y.dat', STATUS='OLD', IOSTAT=IERR)
      IF(IERR.NE.0) THEN
        WRITE(*,*) 'HARNESS: cannot read CLARK_Y.dat'
        STOP
      ENDIF
      READ(20,'(A)') ANAME
      NBREAD = 0
   50 CONTINUE
      IF(NBREAD.GE.IQX-5) GO TO 60
      READ(20,*,END=60,ERR=60) XB(NBREAD+1), YB(NBREAD+1)
      NBREAD = NBREAD + 1
      GO TO 50
   60 CONTINUE
      CLOSE(20)
      NB = NBREAD
C
C ================= Scenario C: viscous point =================
      CALL INITCOMMON
      CALL ABCOPY(.TRUE.)
      MINF1 = 0.0
      REINF1 = 1.0E6
      MATYP = 1
      RETYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
      ITMAX = 30
      ADEG = 2.0
      ALFA = DTOR*ADEG
      QINF = 1.0
      CLSPEC = 0.0
      CALL SPECAL
      MAXSTEP = 40
      DO 115 ISTEP=1, MAXSTEP
        CALL VISCAL(ITMAX)
        IF(LVCONV) GO TO 116
  115 CONTINUE
  116 CONTINUE
      WRITE(*,*) 'MP C_POINT'
      CALL PRINTPOINT('C_POINT ')
      MINF1 = 0.3
      CALL MACHPOST
      WRITE(*,*) 'MP C_AFTERMACH'
      CALL PRINTPOINT('C_AFTER ')
      MINF1 = 0.3
      CALL MACHPOST
      WRITE(*,*) 'MP C_IDENTICAL'
      CALL PRINTPOINT('C_IDENT ')
C
C ================= Scenario B: inviscid point =================
      CALL INITCOMMON
      CALL ABCOPY(.TRUE.)
      MINF1 = 0.0
      REINF1 = 1.0E6
      MATYP = 1
      RETYP = 1
      LVISC = .FALSE.
      LALFA = .TRUE.
      ADEG = 2.0
      ALFA = DTOR*ADEG
      QINF = 1.0
      CLSPEC = 0.0
      CALL SPECAL
      WRITE(*,*) 'MP B_POINT'
      CALL PRINTPOINT('B_POINT ')
      MINF1 = 0.3
      CALL MACHPOST
      WRITE(*,*) 'MP B_AFTERMACH'
      CALL PRINTPOINT('B_AFTER ')
C
C ================= Scenario A: no analysis point =================
      CALL INITCOMMON
      CALL ABCOPY(.TRUE.)
      MINF1 = 0.0
      REINF1 = 1.0E6
      MATYP = 1
      RETYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
      WRITE(*,*) 'MP A_POINT'
      CALL PRINTPOINT('A_POINT ')
      MINF1 = 0.3
      CALL MACHPOST
      WRITE(*,*) 'MP A_AFTERMACH'
      CALL PRINTPOINT('A_AFTER ')
      END
C
      SUBROUTINE MACHPOST
C     Canonical Mach post-processing (xoper.f 322-333).
      INCLUDE 'XFOIL.INC'
      CALL MRCL(1.0,MINF_CL,REINF_CL)
      CALL COMSET
      CALL CPCALC(N,QINV,QINF,MINF,CPI)
      IF(LVISC) CALL CPCALC(N+NW,QVIS,QINF,MINF,CPV)
      CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &           CL,CM,CDP, CL_ALF,CL_MSQ)
      CALL CDCALC
      LVCONV = .FALSE.
      RETURN
      END
C
      SUBROUTINE PRINTPOINT(TAG)
C     It24.16 final output plus hand Cp minimum of the airfoil node.
      INCLUDE 'XFOIL.INC'
      CHARACTER*(*) TAG
      REAL*8 CPMINI
      INTEGER I
      CPMINI = CPI(1)
      DO 10 I=2, N
        IF(CPI(I) .LT. CPMINI) CPMINI = CPI(I)
   10 CONTINUE
      WRITE(*,9200) TAG//' CL    ', CL
      WRITE(*,9200) TAG//' CM    ', CM
      WRITE(*,9200) TAG//' CD    ', CD
      WRITE(*,9200) TAG//' CDF   ', CDF
      WRITE(*,9200) TAG//' CDP   ', CDP
      WRITE(*,9200) TAG//' MINF  ', MINF
      WRITE(*,9200) TAG//' CPMINI', CPMINI
      WRITE(*,9110) TAG//' LVCONV', LVCONV
 9200 FORMAT(1X, A, ES24.16)
 9110 FORMAT(1X, A, L1)
      RETURN
      END
C
      SUBROUTINE INITCOMMON
C     COMMON initialization as xfoil.f INIT (without buffer XB/YB/NB).
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I
      PI = 4.0*ATAN(1.0)
      HOPI = 0.50/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
      GAMMA = 1.4
      GAMM1 = GAMMA - 1.0
      XCMREF = 0.25
      YCMREF = 0.0
      WAKLEN = 1.0
      LIMAGE = .FALSE.
      SHARP = .FALSE.
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      XSREF1 = 1.0
      XSREF2 = 1.0
      XPREF1 = 1.0
      XPREF2 = 1.0
      QINF = 1.0
      MINF = 0.0
      ALFA = 0.0
      N = 0
      NW = 0
      DO 1 I=1,IQX
        GAM(I) = 0.0
        GAM_A(I) = 0.0
    1 CONTINUE
      DO 4 I=1,IZX
        SIG(I) = 0.0
        QINV(I) = 0.0
        QVIS(I) = 0.0
        QINV_A(I) = 0.0
        QINVU(I,1) = 0.0
        QINVU(I,2) = 0.0
        CPI(I) = 0.0
        CPV(I) = 0.0
    4 CONTINUE
      DO 5 I=1,IVX
        MASS(I,1) = 0.0
        MASS(I,2) = 0.0
        UINV(I,1) = 0.0
        UINV(I,2) = 0.0
        UINV_A(I,1) = 0.0
        UINV_A(I,2) = 0.0
        THET(I,1) = 0.0
        THET(I,2) = 0.0
        DSTR(I,1) = 0.0
        DSTR(I,2) = 0.0
        CTAU(I,1) = 0.0
        CTAU(I,2) = 0.0
        UEDG(I,1) = 0.0
        UEDG(I,2) = 0.0
        TAU(I,1) = 0.0
        TAU(I,2) = 0.0
        DIS(I,1) = 0.0
        DIS(I,2) = 0.0
        CTQ(I,1) = 0.0
        CTQ(I,2) = 0.0
        DELT(I,1) = 0.0
        DELT(I,2) = 0.0
        TSTR(I,1) = 0.0
        TSTR(I,2) = 0.0
        SST_GO = 0.0
        SST_GP = 0.0
    5 CONTINUE
      MATYP = 1
      RETYP = 1
      MINF1 = 0.0
      REINF1 = 0.0
      LVISC = .FALSE.
      LALFA = .TRUE.
      LVCONV = .FALSE.
      LWAKE = .FALSE.
      LIPAN = .FALSE.
      LBLINI = .FALSE.
      LWDIJ = .FALSE.
      LADIJ = .FALSE.
      XSTRIP(1) = 1.0
      XSTRIP(2) = 1.0
      ACRIT(1) = 9.0
      ACRIT(2) = 9.0
      RLX = 1.0
      RMSBL = 0.0
      RMXBL = 0.0
      IDAMP = 0
      CALL BLPINI
      VACCEL = 0.01
      LCLOCK = .FALSE.
      AWAKE = 0.0
      AVISC = 0.0
      MVISC = 0.0
      MINF_CL = 0.0
      REINF_CL = 0.0
      CL_ALF = 0.0
      CL_MSQ = 0.0
      CL = 0.0
      CM = 0.0
      CD = 0.0
      CDP = 0.0
      CDF = 0.0
      RETURN
      END

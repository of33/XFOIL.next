C XFOIL.next
C
C Copyright (C) 2026 Olaf Frommann
C https://github.com/of33/XFOIL.next
C
C XFOIL.next is based on XFOIL 6.99 by Mark Drela and
C Harold Youngren.
C
C This file contains Fortran code derived from XFOIL 6.99.
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

      PROGRAM C11A_BATCH_STALL_HARNESS
C--------------------------------------------------------------------
C Clark-Y batch contract harness (stall-region follow-up pass):
C exactly the product contract of the Clark Y standard run
C (BatchApplication,
C     Re=1e5, Mach=0, VISC 1e5, ITER=100, ACRIT 9/9, XSTRIP 1/1,
C     VACCEL 0.01, ABCOPY without PANE): cold path per point (LIPAN/LBLINI
C     invalid per point, xoper.f 394-400 INIT toggle + 402 LIPAN),
C     LALFA=true with SPECAL per point (xoper.f 416-437 alpha single point,
C     QINF=1.0 per point), VISCAL(ITMAX=100) per point (no ITMAX+5,
C     which is the ASEQ contract), sequence 0.0 -> 14.5 step 0.5.
C     It24.16 per iteration (RMSBL/RMXBL/VMXBL/IMXBL/ISMXBL/RLX/Alfa/Cl/
C     CM/Cd/CDF/CDP/ITRAN/NBL/IBLTE/flags) and per point outcome
C     (BATCH IPOINT/ITER/NIT/CL/CM/CD/CDF/CDP/RMSBL/RMXBL/ITRAN/flags/
C     AVISC/MVISC/AWAKE/MINF). Canonical build flags -fdefault-real-8
C     -fdefault-double-8. No changes to the canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER NSIDE, NIT, IPOINT, IERR, NBREAD
      REAL AA1, AA2, DAA
      CHARACTER*48 ANM
      ITMAX = 100
C
C ==== COMMON initialization (as xfoil.f INIT, as c11a_cseq_harness)
C ====
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
        UEDG(I,1) = 0.0
        UEDG(I,2) = 0.0
        THET(I,1) = 0.0
        THET(I,2) = 0.0
        DSTR(I,1) = 0.0
        DSTR(I,2) = 0.0
        CTAU(I,1) = 0.0
        CTAU(I,2) = 0.0
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
      LVISC = .TRUE.
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
C
C ==== Read Clark Y from file into the buffer ====
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
C ==== Buffer -> current contour (ABCOPY, no PANE) ====
      CALL ABCOPY(.TRUE.)
      MINF = 0.0
      MINF1 = 0.0
      REINF1 = 1.0E5
      RETYP = 1
      MATYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
C
C ==== BATCH contract: sequence 0.0 -> 14.5 step 0.5, cold path per point
C ====
      AA1 = 0.0
      AA2 = 14.5
      DAA = 0.5
      NPOINT = INT((AA2-AA1)/DAA + 0.5) + 1
      DO 115 IPOINT=1, NPOINT
C------- cold path per point (BatchApplication coldStart): LIPAN/LBLINI
C         invalid; Fortran-equivalent INIT toggle (xoper.f 394-400)
C         with LBLINI=TRUE after the previous point.
        IF(IPOINT .GT. 1) THEN
          LBLINI = .NOT.LBLINI
          IF(.NOT.LBLINI) LIPAN = .FALSE.
        ENDIF
C------- OPER ALFA single point (xoper.f 416-437): LALFA=true, QINF=1.0,
C         SPECAL, conditional 1e-5 invalidation, VISCAL(ITMAX).
        ADEG = AA1 + DAA*FLOAT(IPOINT-1)
        LALFA = .TRUE.
        ALFA = DTOR*ADEG
        QINF = 1.0
        CALL SPECAL
        IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
        IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
        IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
        CALL VISCALDIAG(ITMAX, NIT)
        ADEG = ALFA/DTOR
        CALL POINTOUT(IPOINT, NIT)
  115 CONTINUE
      END
C
      SUBROUTINE VISCALDIAG(NITER1, NITERDONE)
C     Inline-VISCAL kernel (xoper.f 2901-3027) with complete
C     iteration history (LALFA=true branch: MRCL/COMSET, xoper.f 2984).
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      DATA EPS1 / 1.0E-4 /
      INTEGER NITER1, NITERDONE, ITER
      NITERDONE = 0
      IF(.NOT.LWAKE) CALL XYWAKE
      CALL QWCALC
      CALL QISET
      IF(.NOT.LIPAN) THEN
        IF(LBLINI) CALL GAMQV
        CALL STFIND
        CALL IBLPAN
        CALL XICALC
        CALL IBLSYS
      ENDIF
      CALL UICALC
      IF(.NOT.LBLINI) THEN
        DO 10 IBL=1, NBL(1)
          UEDG(IBL,1) = UINV(IBL,1)
   10   CONTINUE
        DO 11 IBL=1, NBL(2)
          UEDG(IBL,2) = UINV(IBL,2)
   11   CONTINUE
      ENDIF
      IF(LVCONV) THEN
        CALL QVFUE
        CALL CPCALC(N+NW,QVIS,QINF,MINF,CPV)
        CALL CPCALC(N+NW,QINV,QINF,MINF,CPI)
        CALL GAMQV
        CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &             CL,CM,CDP, CL_ALF,CL_MSQ)
        CALL CDCALC
      ENDIF
      IF(.NOT.LWDIJ .OR. .NOT.LADIJ) CALL QDCALC
      DO 1000 ITER=1, NITER1
        CALL SETBL
        CALL BLSOLV
        CALL UPDATE
        IF(LALFA) THEN
          CALL MRCL(CL,MINF_CL,REINF_CL)
          CALL COMSET
        ELSE
          CALL QISET
          CALL UICALC
        ENDIF
        CALL QVFUE
        CALL GAMQV
        CALL STMOVE
        CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &              CL,CM,CDP,CL_ALF,CL_MSQ)
        CALL CDCALC
        NITERDONE = ITER
        WRITE(*,9500) 'BIT'
        WRITE(*,9200) 'RMSBL ', RMSBL
        WRITE(*,9200) 'RMXBL ', RMXBL
        WRITE(*,9120) 'VMXBL ', VMXBL
        WRITE(*,9000) 'IMXBL ', IMXBL
        WRITE(*,9000) 'ISMXBL', ISMXBL
        WRITE(*,9200) 'RLX   ', RLX
        WRITE(*,9200) 'ALFA  ', ALFA
        WRITE(*,9200) 'CL    ', CL
        WRITE(*,9200) 'CM    ', CM
        WRITE(*,9200) 'CD    ', CD
        WRITE(*,9200) 'CDF   ', CDF
        WRITE(*,9200) 'CDP   ', CDP
        WRITE(*,9000) 'ITRAN1', ITRAN(1)
        WRITE(*,9000) 'ITRAN2', ITRAN(2)
        WRITE(*,9000) 'NBL1  ', NBL(1)
        WRITE(*,9000) 'NBL2  ', NBL(2)
        WRITE(*,9000) 'IBLTE1', IBLTE(1)
        WRITE(*,9000) 'IBLTE2', IBLTE(2)
        WRITE(*,9110) 'LBLINI', LBLINI
        WRITE(*,9110) 'LIPAN ', LIPAN
        WRITE(*,9110) 'LVCONV', LVCONV
        WRITE(*,9110) 'LWAKE ', LWAKE
        WRITE(*,9110) 'LWDIJ ', LWDIJ
        WRITE(*,9200) 'AVISC ', AVISC
        WRITE(*,9200) 'MVISC ', MVISC
        WRITE(*,9200) 'AWAKE ', AWAKE
        IF(RMSBL .LT. EPS1) THEN
          LVCONV = .TRUE.
          AVISC = ALFA
          MVISC = MINF
          GO TO 90
        ENDIF
 1000 CONTINUE
   90 CONTINUE
      WRITE(*,9000) 'NITERDONE', NITERDONE
 9000 FORMAT(1X, A, I6)
 9110 FORMAT(1X, A, L1)
 9120 FORMAT(1X, A, A1)
 9200 FORMAT(1X, A, ES24.16)
 9500 FORMAT(1X, A)
      END
C
      SUBROUTINE POINTOUT(IPOINT, NIT)
C     point outcome (It24.16) per batch point.
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER IPOINT, NIT
      WRITE(*,9000) 'BATCH IPOINT   ', IPOINT
      WRITE(*,9200) 'BATCH ADEG     ', ADEG
      WRITE(*,9200) 'BATCH CL       ', CL
      WRITE(*,9200) 'BATCH CM       ', CM
      WRITE(*,9200) 'BATCH CD       ', CD
      WRITE(*,9200) 'BATCH CDF      ', CDF
      WRITE(*,9200) 'BATCH CDP      ', CDP
      WRITE(*,9000) 'BATCH NITER    ', NIT
      WRITE(*,9110) 'BATCH LVCONV   ', LVCONV
      WRITE(*,9200) 'BATCH RMSBL    ', RMSBL
      WRITE(*,9200) 'BATCH RMXBL    ', RMXBL
      WRITE(*,9000) 'BATCH ITRAN1   ', ITRAN(1)
      WRITE(*,9000) 'BATCH ITRAN2   ', ITRAN(2)
      WRITE(*,9110) 'BATCH LIPAN    ', LIPAN
      WRITE(*,9110) 'BATCH LBLINI   ', LBLINI
      WRITE(*,9110) 'BATCH LWAKE    ', LWAKE
      WRITE(*,9110) 'BATCH LWDIJ    ', LWDIJ
      WRITE(*,9110) 'BATCH LADIJ    ', LADIJ
      WRITE(*,9200) 'BATCH AWAKE    ', AWAKE
      WRITE(*,9200) 'BATCH AVISC    ', AVISC
      WRITE(*,9200) 'BATCH MVISC    ', MVISC
      WRITE(*,9200) 'BATCH MINF     ', MINF
      WRITE(*,9200) 'BATCH STARTNEXT', ALFA
 9000 FORMAT(1X, A, I6)
 9110 FORMAT(1X, A, L1)
 9200 FORMAT(1X, A, ES24.16)
      END

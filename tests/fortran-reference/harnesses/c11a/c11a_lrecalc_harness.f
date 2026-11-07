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

      PROGRAM C11A_LRECALC_HARNESS
C--------------------------------------------------------------------
C     LRECALC harness (Clark Y, Re=1e6, Mach=0, ABCOPY without PANE):
C     canonical '!'-/LRECALC continuation semantics per xoper.f
C     85-92/416-542. A viscous alpha point that did not converge
C     with too small an iteration limit (ITMAX=1) is continued
C     only by renewed VISCAL(ITMAX) on the retained point, wake, mapping,
C     DIJ and BL state, without calling SPECAL/SPECCL again
C     and without cold-start reset.
C
C     Sequence: SPECAL once (inviscid alpha point for 2 deg), then
C     VISCAL(ITMAX=1) -> non-converged (point 1). Then follows the
C     continuation: VISCAL(ITMAX=1) again, without SPECAL in between
C (points 2, 3, ...), until convergence or the limit. The VISCAL kernel
C     (xoper.f 2901-3027) is mapped inline with an iteration counter
C     (LALFA=true branch: MRCL/COMSET). The internal LWAKE/LIPAN/LBLINI/
C LVCONV/LWDIJ flags document the state preservation: for the
C     continuation the wake (LWAKE), panel initialization (LIPAN),
C BL initialization (LBLINI) and the wake DIJ block (LWDIJ) are kept;
C     SPECAL is not re-executed.
C
C     Output per step (It24.16): LALFA, CLSPEC, achieved alpha
C     (ADEG), CL/CM/CD/CDF/CDP, iteration count, LVCONV, RMSBL, RMXBL,
C     ITRAN, Conservation states (LIPAN/LBLINI/LVCONV/LWAKE/LWDIJ/LADIJ),
C     AWAKE/AVISC/MVISC/MINF. Canonical build flags
C     -fdefault-real-8 -fdefault-double-8. No changes to the
C     canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, NBREAD, IERR, ISTEP, NIT, MAXSTEP
      CHARACTER ANAME*32
      ITMAX = 1
C
C ==== COMMON initialization (as xfoil.f INIT) ====
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
      REINF1 = 1.0E6
      RETYP = 1
      MATYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
C
C ==== Inviscid alpha point at 2 deg (SPECAL once) ====
      ADEG = 2.0
      ALFA = DTOR*ADEG
      QINF = 1.0
      CLSPEC = 0.0
      CALL SPECAL
      WRITE(*,*) 'LR SPECAL_DONE'
      WRITE(*,9200) 'LR SPECAL_ALFA', ALFA
      WRITE(*,9200) 'LR SPECAL_CL  ', CL
C
C ==== VISCAL(ITMAX=1) non-converged, then LRECALC continuation ====
      MAXSTEP = 12
      DO 115 ISTEP=1, MAXSTEP
        CALL VISCALSEQ(ITMAX, NIT)
        ADEG = ALFA/DTOR
        CALL POINTOUT(ISTEP, NIT)
        IF(LVCONV) GO TO 116
  115 CONTINUE
  116 CONTINUE
 9000 FORMAT(1X, A, I6)
 9200 FORMAT(1X, A, ES24.16)
      END
C
      SUBROUTINE VISCALSEQ(NITER1, NITERDONE)
C Inlined VISCAL kernel (xoper.f 2901-3027) without display WRITEs, with an
C     iteration counter. The LALFA=true branch uses MRCL/COMSET (xoper.f
C     2980-2984). Return value NITERDONE = iterations executed.
      INCLUDE 'XFOIL.INC'
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
        IF(LVISC) THEN
          CALL CPCALC(N+NW,QVIS,QINF,MINF,CPV)
          CALL CPCALC(N+NW,QINV,QINF,MINF,CPI)
        ELSE
          CALL CPCALC(N,QINV,QINF,MINF,CPI)
        ENDIF
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
        IF(RMSBL .LT. EPS1) THEN
          LVCONV = .TRUE.
          AVISC = ALFA
          MVISC = MINF
          GO TO 90
        ENDIF
 1000 CONTINUE
   90 CONTINUE
      CALL CPCALC(N+NW,QINV,QINF,MINF,CPI)
      CALL CPCALC(N+NW,QVIS,QINF,MINF,CPV)
      END
C
      SUBROUTINE POINTOUT(ISTEP, NIT)
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER ISTEP, NIT
      WRITE(*,9000) 'LR ISTEP    ', ISTEP
      WRITE(*,9110) 'LR LALFA    ', LALFA
      WRITE(*,9200) 'LR CLSPEC   ', CLSPEC
      WRITE(*,9200) 'LR ADEG     ', ADEG
      WRITE(*,9200) 'LR CL       ', CL
      WRITE(*,9200) 'LR CM       ', CM
      WRITE(*,9200) 'LR CD       ', CD
      WRITE(*,9200) 'LR CDF      ', CDF
      WRITE(*,9200) 'LR CDP      ', CDP
      WRITE(*,9000) 'LR NITER    ', NIT
      WRITE(*,9110) 'LR LVCONV   ', LVCONV
      WRITE(*,9200) 'LR RMSBL    ', RMSBL
      WRITE(*,9200) 'LR RMXBL    ', RMXBL
      WRITE(*,9000) 'LR ITRAN1   ', ITRAN(1)
      WRITE(*,9000) 'LR ITRAN2   ', ITRAN(2)
      WRITE(*,9110) 'LR LIPAN    ', LIPAN
      WRITE(*,9110) 'LR LBLINI   ', LBLINI
      WRITE(*,9110) 'LR LWAKE    ', LWAKE
      WRITE(*,9110) 'LR LWDIJ    ', LWDIJ
      WRITE(*,9110) 'LR LADIJ    ', LADIJ
      WRITE(*,9200) 'LR AWAKE    ', AWAKE
      WRITE(*,9200) 'LR AVISC    ', AVISC
      WRITE(*,9200) 'LR MVISC    ', MVISC
      WRITE(*,9200) 'LR MINF     ', MINF
 9000 FORMAT(1X, A, I6)
 9110 FORMAT(1X, A, L1)
 9200 FORMAT(1X, A, ES24.16)
      END

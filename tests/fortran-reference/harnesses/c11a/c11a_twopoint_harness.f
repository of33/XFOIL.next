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

      PROGRAM C11A_TWOPOINT_HARNESS
C--------------------------------------------------------------------
C     Two-point harness (Clark Y, Re=1e5, Mach=0, no-PANE):
C     canonical VISCAL sequence semantics per xoper.f for the
C     batch sequence alpha=0.0 (non-converged) -> alpha=0.5 (converged).
C     Per-point reset according to the batch contract:
C       LIPAN/LBLINI/LVCONV/LWAKE = .FALSE. per point;
C       LWDIJ/LADIJ and all COMMON states persist.
C     Snapshots at the state boundaries:
C       P0B after SPECAL alpha=0.0 (before VISCAL)
C       P0C after VISCAL alpha=0.0 (non-convergence)
C       P1A after alpha set + per-point flags, alpha=0.5
C       P1B after SPECAL alpha=0.5
C       P1C after XYWAKE alpha=0.5
C       P1D after VISCAL init (QWCALC/QISET/LIPAN/UICALC/LBLINI/QDCALC),
C           immediately before the first SETBL
C       P1E after the first SETBL, alpha=0.5
C       P1F after complete VISCAL alpha=0.5
C     Print It24.16, canonical order Page 1 IBL 1..NBL(1),
C     then side 2. Canonical build flags -fdefault-real-8
C     -fdefault-double-8. No changes to the canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, IBL, NBREAD, ITER, IERR
      REAL EPS1, ALFR
      CHARACTER ANAME*32
      EPS1 = 1.0E-4
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
      REINF1 = 3.0E6
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
C ================= Point 0: alpha = 0.0 =================
      ALFA = 0.0*PI/180.0
      LVCONV = .FALSE.
      LWAKE  = .FALSE.
      LIPAN  = .FALSE.
      LBLINI = .FALSE.
      LWDIJ  = .FALSE.
      LADIJ  = .FALSE.
      CALL SPECAL
      CALL SNAP('P0B')
C---- VISCAL init (xoper.f 2904-2964)
      CALL XYWAKE
      CALL QWCALC
      CALL QISET
      CALL STFIND
      CALL IBLPAN
      CALL XICALC
      CALL IBLSYS
      CALL UICALC
      DO 70 IBL=1, NBL(1)
        UEDG(IBL,1) = UINV(IBL,1)
   70 CONTINUE
      DO 71 IBL=1, NBL(2)
        UEDG(IBL,2) = UINV(IBL,2)
   71 CONTINUE
      CALL QDCALC
C---- Newton loop (xoper.f 2970-3022), NITER=100
      DO 100 ITER=1, 100
        CALL SETBL
        CALL BLSOLV
        CALL UPDATE
        CALL MRCL(CL,MINF_CL,REINF_CL)
        CALL COMSET
        CALL QVFUE
        CALL GAMQV
        CALL STMOVE
        CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &              CL,CM,CDP, CL_ALF,CL_MSQ)
        CALL CDCALC
        IF(RMSBL .LT. EPS1) THEN
          LVCONV = .TRUE.
          AVISC = ALFA
          MVISC = MINF
          GO TO 110
        ENDIF
  100 CONTINUE
  110 CONTINUE
      CALL CPCALC(N+NW,QINV,QINF,MINF,CPI)
      CALL CPCALC(N+NW,QVIS,QINF,MINF,CPV)
      CALL SNAP('P0C')
C
C ================= Point 1: alpha = 0.5 =================
      ALFA = 0.5*PI/180.0
      LVCONV = .FALSE.
      LWAKE  = .FALSE.
      LIPAN  = .FALSE.
      LBLINI = .FALSE.
      CALL SNAP('P1A')
      CALL SPECAL
      CALL SNAP('P1B')
      CALL XYWAKE
      CALL SNAP('P1C')
      CALL QWCALC
      CALL QISET
      CALL STFIND
      CALL IBLPAN
      CALL XICALC
      CALL IBLSYS
      CALL UICALC
      DO 72 IBL=1, NBL(1)
        UEDG(IBL,1) = UINV(IBL,1)
   72 CONTINUE
      DO 73 IBL=1, NBL(2)
        UEDG(IBL,2) = UINV(IBL,2)
   73 CONTINUE
      IF(.NOT.LWDIJ .OR. .NOT.LADIJ) CALL QDCALC
      CALL SNAP('P1D')
      CALL SETBL
      CALL SNAP('P1E')
      CALL BLSOLV
      CALL UPDATE
      CALL MRCL(CL,MINF_CL,REINF_CL)
      CALL COMSET
      CALL QVFUE
      CALL GAMQV
      CALL STMOVE
      CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &            CL,CM,CDP, CL_ALF,CL_MSQ)
      CALL CDCALC
      DO 120 ITER=2, 100
        IF(RMSBL .LT. EPS1) GO TO 130
        CALL SETBL
        CALL BLSOLV
        CALL UPDATE
        CALL MRCL(CL,MINF_CL,REINF_CL)
        CALL COMSET
        CALL QVFUE
        CALL GAMQV
        CALL STMOVE
        CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &              CL,CM,CDP, CL_ALF,CL_MSQ)
        CALL CDCALC
  120 CONTINUE
  130 CONTINUE
      IF(RMSBL .LT. EPS1) THEN
        LVCONV = .TRUE.
        AVISC = ALFA
        MVISC = MINF
      ENDIF
      CALL CPCALC(N+NW,QINV,QINF,MINF,CPI)
      CALL CPCALC(N+NW,QVIS,QINF,MINF,CPV)
      CALL SNAP('P1F')
C
      END

      SUBROUTINE SNAP(LAB)
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      CHARACTER*(*) LAB
      INTEGER I, IS, IBL, IPW, J
C---- flags
      WRITE(*,9110) 'SNAP '//LAB//' LIPAN ', LIPAN
      WRITE(*,9110) 'SNAP '//LAB//' LBLINI', LBLINI
      WRITE(*,9110) 'SNAP '//LAB//' LVCONV', LVCONV
      WRITE(*,9110) 'SNAP '//LAB//' LWAKE ', LWAKE
      WRITE(*,9110) 'SNAP '//LAB//' LWDIJ ', LWDIJ
      WRITE(*,9110) 'SNAP '//LAB//' LADIJ ', LADIJ
C---- Scalars
      WRITE(*,9100) 'SNAP '//LAB//' NBL1   ', NBL(1)
      WRITE(*,9100) 'SNAP '//LAB//' NBL2   ', NBL(2)
      WRITE(*,9100) 'SNAP '//LAB//' IBLTE1 ', IBLTE(1)
      WRITE(*,9100) 'SNAP '//LAB//' IBLTE2 ', IBLTE(2)
      WRITE(*,9100) 'SNAP '//LAB//' ITRAN1 ', ITRAN(1)
      WRITE(*,9100) 'SNAP '//LAB//' ITRAN2 ', ITRAN(2)
      WRITE(*,9100) 'SNAP '//LAB//' IST    ', IST
      WRITE(*,9100) 'SNAP '//LAB//' NW     ', NW
      WRITE(*,9100) 'SNAP '//LAB//' NSYS   ', NSYS
      WRITE(*,9200) 'SNAP '//LAB//' SST    ', SST
      WRITE(*,9200) 'SNAP '//LAB//' RMSBL  ', RMSBL
      WRITE(*,9200) 'SNAP '//LAB//' RMXBL  ', RMXBL
      WRITE(*,9200) 'SNAP '//LAB//' RLX    ', RLX
      WRITE(*,9200) 'SNAP '//LAB//' ALFA   ', ALFA
      WRITE(*,9200) 'SNAP '//LAB//' CL     ', CL
      WRITE(*,9200) 'SNAP '//LAB//' CM     ', CM
      WRITE(*,9200) 'SNAP '//LAB//' CD     ', CD
      WRITE(*,9200) 'SNAP '//LAB//' CDF    ', CDF
      WRITE(*,9200) 'SNAP '//LAB//' CDP    ', CDP
      WRITE(*,9200) 'SNAP '//LAB//' MINF   ', MINF
      WRITE(*,9200) 'SNAP '//LAB//' REINF  ', REINF
      WRITE(*,9200) 'SNAP '//LAB//' AVISC  ', AVISC
      WRITE(*,9200) 'SNAP '//LAB//' MVISC  ', MVISC
      WRITE(*,9200) 'SNAP '//LAB//' AWAKE  ', AWAKE
      WRITE(*,9200) 'SNAP '//LAB//' MINF_CL', MINF_CL
      WRITE(*,9200) 'SNAP '//LAB//' SST_GO ', SST_GO
      WRITE(*,9200) 'SNAP '//LAB//' SST_GP ', SST_GP
      WRITE(*,9200) 'SNAP '//LAB//' TKLAM  ', TKLAM
      WRITE(*,9200) 'SNAP '//LAB//' DWTE   ', DWTE
      WRITE(*,9200) 'SNAP '//LAB//' RSTBL  ', RSTBL
      WRITE(*,9200) 'SNAP '//LAB//' HSTINV ', HSTINV
      WRITE(*,9200) 'SNAP '//LAB//' REYBL  ', REYBL
      WRITE(*,9200) 'SNAP '//LAB//' IDAMPV ', FLOAT(IDAMPV)
C---- BL arc lengths and inviscid Edges velocity
      CALL SNPA(LAB, 'XSSI ', XSSI)
      CALL SNPA(LAB, 'UINV ', UINV)
      CALL SNPA(LAB, 'UINV_A', UINV_A)
C---- DIJ line of the first wake-BL panel position
      IPW = IPAN(IBLTE(2)+1, 2)
      DO 44 J=1, N+NW
        WRITE(*,9312) 'SNAP '//LAB//' DIJ  ', IPW, J, DIJ(IPW,J)
   44 CONTINUE
C---- BL station fields
      CALL SNPA(LAB, 'THET ', THET)
      CALL SNPA(LAB, 'DSTR ', DSTR)
      CALL SNPA(LAB, 'CTAU ', CTAU)
      CALL SNPA(LAB, 'UEDG ', UEDG)
      CALL SNPA(LAB, 'MASS ', MASS)
      CALL SNPA(LAB, 'TAU  ', TAU)
      CALL SNPA(LAB, 'DIS  ', DIS)
      CALL SNPA(LAB, 'CTQ  ', CTQ)
      CALL SNPA(LAB, 'DELT ', DELT)
      CALL SNPA(LAB, 'TSTR ', TSTR)
C---- Panel/system pointers
      CALL SNPI(LAB, 'IPAN ', IPAN)
      CALL SNPI(LAB, 'ISYS ', ISYS)
C---- Inviscid Velocity
      DO 43 I=1, N+NW
        WRITE(*,9210) 'SNAP '//LAB//' QINV ', I, QINV(I)
   43 CONTINUE
C---- GAM / QVIS / WGAP
      DO 40 I=1, N
        WRITE(*,9210) 'SNAP '//LAB//' GAM  ', I, GAM(I)
   40 CONTINUE
      DO 41 I=1, N+NW
        WRITE(*,9210) 'SNAP '//LAB//' QVIS ', I, QVIS(I)
   41 CONTINUE
      DO 42 IW=1, NW
        WRITE(*,9210) 'SNAP '//LAB//' WGAP ', IW, WGAP(IW)
   42 CONTINUE
 9100 FORMAT(1X, A, I6)
 9110 FORMAT(1X, A, L1)
 9200 FORMAT(1X, A, ES24.16)
 9210 FORMAT(1X, A, I6, ES24.16)
 9312 FORMAT(1X, A, I6, I6, ES24.16)
      END

      SUBROUTINE SNPA(LAB, CNAM, A)
      INCLUDE 'XFOIL.INC'
      CHARACTER*(*) LAB, CNAM
      REAL A(IVX,ISX)
      INTEGER IBL, IS
      DO 10 IS=1, ISX
        DO 20 IBL=1, NBL(IS)
          WRITE(*,9210) 'SNAP '//LAB//' '//CNAM, IS, IBL, A(IBL,IS)
   20   CONTINUE
   10 CONTINUE
 9210 FORMAT(1X, A, I6, I6, ES24.16)
      END

      SUBROUTINE SNPI(LAB, CNAM, IA)
      INCLUDE 'XFOIL.INC'
      CHARACTER*(*) LAB, CNAM
      INTEGER IA(IVX,ISX)
      INTEGER IBL, IS
      DO 10 IS=1, ISX
        DO 20 IBL=1, NBL(IS)
          WRITE(*,9311) 'SNAP '//LAB//' '//CNAM, IS, IBL, IA(IBL,IS)
   20   CONTINUE
   10 CONTINUE
 9311 FORMAT(1X, A, I6, I6, I6)
      END

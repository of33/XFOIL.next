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

      PROGRAM C11A_HARNESS
C--------------------------------------------------------------------
C     Focused state harness (Re=1e5, Clark Y): reproduces the
C     canonical VISCAL initialization (xoper.f 2904-2964) and the
C     first Newton iteration (xoper.f 2970-3003) and returns the
C     state that iteration 2 enters with, exactly (It17.8).
C     Inputs: CLARK_Y.dat (Buffer geometry), c11a_alpha.dat (alpha
C     in Degrees). output: stdout.
C     No changes to the canonical sources; the new focused
C     State harness reference.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, IS, IBL, NBREAD, ISTOP, ISTNEW, IERR
      REAL ALFADEG, ALFR
      CHARACTER ANAME*32
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
      IDAMP = 0
      CALL BLPINI
      VACCEL = 0.01
      LCLOCK = .FALSE.
C
C ==== Alpha from file ====
      OPEN(UNIT=21, FILE='c11a_alpha.dat', STATUS='OLD', IOSTAT=IERR)
      IF(IERR.NE.0) THEN
        WRITE(*,*) 'HARNESS: cannot read c11a_alpha.dat'
        STOP
      ENDIF
      READ(21,*) ALFADEG
      CLOSE(21)
C
C ==== Read Clark Y from file into the buffer (order as in the file) ====
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
      WRITE(*,9100) 'HARNESS NB = ', NB
C
C ==== Buffer -> current contour (xgdes.f ABCOPY) ====
      CALL ABCOPY(.TRUE.)
      WRITE(*,9100) 'HARNESS N = ', N
C
C ==== Viscous Specifications (xoper.f OPER specifications) ====
      MINF = 0.0
      MINF1 = 0.0
      REINF1 = 1.0E5
      RETYP = 1
      MATYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
      ALFR = ALFADEG*PI/180.0
      ALFA = ALFR
      LVCONV = .FALSE.
      LWAKE = .FALSE.
      LIPAN = .FALSE.
      LBLINI = .FALSE.
      LWDIJ = .FALSE.
      LADIJ = .FALSE.
C
C ==== Inviscid Solution (xoper.f SPECAL: GGCALC, GAM, TECALC, QISET,
C      CLM Newton loop with MRCL/COMSET/CLCALC) ====
      CALL SPECAL
C
C ==== VISCAL initialization (xoper.f 2904-2964) ====
      CALL XYWAKE
      CALL QWCALC
      CALL QISET
C----.NOT.LIPAN: GAMQV only for LBLINI (false) -> drops
      CALL STFIND
      CALL IBLPAN
      CALL XICALC
      CALL IBLSYS
      CALL UICALC
C---- .NOT.LBLINI: UEDG = UINV (IBL 1..NBL)
      DO 70 IBL=1, NBL(1)
        UEDG(IBL,1) = UINV(IBL,1)
   70 CONTINUE
      DO 71 IBL=1, NBL(2)
        UEDG(IBL,2) = UINV(IBL,2)
   71 CONTINUE
C---- LVCONV = false -> drops
      CALL QDCALC
C
C ==== Per-station instrumentation after the initialization ====
      DO 65 IS=1, 2
        DO 66 IBL=2, 40, 2
          WRITE(*,9211) 'INI UINV ', IBL, IS, UINV(IBL,IS)
          WRITE(*,9211) 'INI XSSI ', IBL, IS, XSSI(IBL,IS)
          WRITE(*,9211) 'INI UEDG ', IBL, IS, UEDG(IBL,IS)
   66 CONTINUE
   65 CONTINUE
C---- DIJ elements 1-4 (source-influence matrix, QDCALC)
      DO 69 I=1, 4
        DO 610 J=1, 4
          WRITE(*,9211) 'INI DIJ ', I, J, DIJ(I,J)
  610   CONTINUE
   69 CONTINUE
C ---- GAM on panels 1-5 and around the stagnation point (initial GAM of
C SPECAL)
      DO 67 I=1, 5
        WRITE(*,9210) 'INI GAM ', I, GAM(I)
   67 CONTINUE
      DO 68 I=IST-2, IST+2
        WRITE(*,9210) 'INI GAM ', I, GAM(I)
   68 CONTINUE
      WRITE(*,9200) 'INI S1 = ', S(1)
      WRITE(*,9200) 'INI S2 = ', S(2)
      WRITE(*,9200) 'INI SN = ', S(N)
C
C ==== Initial state before iteration 1 ====
      WRITE(*,9100) 'INIT NBL1 = ', NBL(1)
      WRITE(*,9100) 'INIT NBL2 = ', NBL(2)
      WRITE(*,9100) 'INIT IBLTE1 = ', IBLTE(1)
      WRITE(*,9100) 'INIT IBLTE2 = ', IBLTE(2)
      WRITE(*,9100) 'INIT IST = ', IST
      WRITE(*,9200) 'INIT SST = ', SST
      WRITE(*,9100) 'INIT ITRAN1 = ', ITRAN(1)
      WRITE(*,9100) 'INIT ITRAN2 = ', ITRAN(2)
C
C ==== BL state at the wake stations (INIT, before iteration 1) ====
      DO 130 JBL=IBLTE(1)+1, NBL(1)
        WRITE(*,9211) 'WBL1 ', 1, JBL, THET(JBL,1)
        WRITE(*,9211) 'WBL1 ', 2, JBL, DSTR(JBL,1)
        WRITE(*,9211) 'WBL1 ', 3, JBL, UEDG(JBL,1)
        WRITE(*,9211) 'WBL1 ', 4, JBL, MASS(JBL,1)
        WRITE(*,9211) 'WBL1 ', 5, JBL, CTAU(JBL,1)
  130 CONTINUE
      DO 131 JBL=IBLTE(2)+1, NBL(2)
        WRITE(*,9211) 'WBL2 ', 1, JBL, THET(JBL,2)
        WRITE(*,9211) 'WBL2 ', 2, JBL, DSTR(JBL,2)
        WRITE(*,9211) 'WBL2 ', 3, JBL, UEDG(JBL,2)
        WRITE(*,9211) 'WBL2 ', 4, JBL, MASS(JBL,2)
        WRITE(*,9211) 'WBL2 ', 5, JBL, CTAU(JBL,2)
  131 CONTINUE
      DO 132 IW=1, NBL(2)-IBLTE(2)
        WRITE(*,9210) 'WGAP ', IW, WGAP(IW)
  132 CONTINUE
      DO 133 K=1, 4
        WRITE(*,9210) 'FSYS VSREZ ', K, VSREZ(K)
  133 CONTINUE
      DO 134 K=1, 4
        DO 135 L=1, 5
          WRITE(*,9211) 'FSYS VS1 ', L, K, VS1(K,L)
          WRITE(*,9211) 'FSYS VS2 ', L, K, VS2(K,L)
  135   CONTINUE
  134 CONTINUE
C
C ==== Newton iteration 1 (xoper.f 2970-3003) ====
      CALL SETBL
C---- VA/VM after SETBL (Assembly, per system row JV 1-2)
      DO 78 JV=1, 2
        DO 79 IE=1, 3
          WRITE(*,9211) 'VB R ', IE, JV, VB(IE,1,JV)
          WRITE(*,9211) 'VB A ', IE, JV, VB(IE,2,JV)
   79   CONTINUE
   78 CONTINUE
      DO 90 IE=1, 3
        WRITE(*,9211) 'VZ R ', IE, 0, VZ(IE,1)
        WRITE(*,9211) 'VZ A ', IE, 0, VZ(IE,2)
   90 CONTINUE
      WRITE(*,9210) 'NSYSVAL ', 0, DBLE(NSYS)
      DO 110 IV=1, NSYS
        WRITE(*,9211) 'VAR ', 1, IV, VA(1,1,IV)
        WRITE(*,9211) 'VAR ', 2, IV, VA(2,2,IV)
        WRITE(*,9211) 'VAR ', 3, IV, VA(3,1,IV)
        WRITE(*,9211) 'VAR ', 4, IV, VA(3,2,IV)
        DO 111 L=IV, NSYS
          WRITE(*,9211) 'VMR ', 1, IV, VM(1,L,IV)
          WRITE(*,9211) 'VMR ', 2, IV, VM(2,L,IV)
          WRITE(*,9211) 'VMR ', 3, IV, VM(3,L,IV)
  111   CONTINUE
        WRITE(*,9211) 'RHS ', 1, IV, VDEL(2,1,IV)
        WRITE(*,9211) 'RHS ', 2, IV, VDEL(3,1,IV)
  110 CONTINUE
      DO 120 J=153, 173
        WRITE(*,9211) 'DIJW ', 1, J, DIJ(1,J)
        WRITE(*,9211) 'DIJW ', 2, J, DIJ(2,J)
        WRITE(*,9211) 'DIJW ', 3, J, DIJ(3,J)
  120 CONTINUE
      DO 91 IV=1, 8
        DO 92 L=1, 8
          DO 93 K=1, 3
            WRITE(*,9210) 'VMFF  ', K, VM(K,L,IV)
   93     CONTINUE
   92   CONTINUE
   91 CONTINUE
      DO 74 JV=1, 2
        DO 75 IE=1, 3
          WRITE(*,9211) 'VA R ', IE, JV, VA(IE,1,JV)
          WRITE(*,9211) 'VA A ', IE, JV, VA(IE,2,JV)
   75   CONTINUE
   74 CONTINUE
      WRITE(*,9210) 'VM 111 ', 1, VM(1,1,1)
      WRITE(*,9210) 'VM 112 ', 2, VM(1,1,2)
      WRITE(*,9210) 'VM 211 ', 3, VM(1,2,1)
      WRITE(*,9210) 'VM 212 ', 4, VM(1,2,2)
      WRITE(*,9210) 'HARN ARCSPAN ', N, S(N)-S(1)
      WRITE(*,9210) 'HARN S(1)     ', 1, S(1)
      WRITE(*,9210) 'HARN S(N)     ', N, S(N)
      DO 76 JV=1, 6
        WRITE(*,9210) 'VDELRHS 2R ', JV, VDEL(2,1,JV)
        WRITE(*,9210) 'VDELRHS 3R ', JV, VDEL(3,1,JV)
   76 CONTINUE
      CALL BLSOLV
C---- VDEL residuals after BLSOLV (per system row JV 1-6)
      DO 72 JV=1, 6
        WRITE(*,9210) 'VDEL 1R ', JV, VDEL(1,1,JV)
        WRITE(*,9210) 'VDEL 2R ', JV, VDEL(2,1,JV)
        WRITE(*,9210) 'VDEL 3R ', JV, VDEL(3,1,JV)
        WRITE(*,9210) 'VDEL 1A ', JV, VDEL(1,2,JV)
        WRITE(*,9210) 'VDEL 2A ', JV, VDEL(2,2,JV)
        WRITE(*,9210) 'VDEL 3A ', JV, VDEL(3,2,JV)
   72 CONTINUE
      CALL UPDATE
      CALL MRCL(CL,MINF_CL,REINF_CL)
      CALL COMSET
      CALL QVFUE
      CALL GAMQV
C
      ISTOP = IST
      CALL STMOVE
      ISTNEW = IST
      IF(ISTOP.NE.ISTNEW) THEN
        WRITE(*,*) 'STMOVE moved'
      ELSE
        WRITE(*,*) 'STMOVE not moved'
      ENDIF
      WRITE(*,9100) 'ITER1 ISTOLD = ', ISTOP
      WRITE(*,9100) 'ITER1 ISTNEW = ', ISTNEW
C
      CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &            CL,CM,CDP,CL_ALF,CL_MSQ)
      CALL CDCALC
C
C ==== iteration-1 diagnostics (xoper.f 3005-3011) ====
      WRITE(*,9200) 'ITER1 RMSBL = ', RMSBL
      WRITE(*,9200) 'ITER1 RMXBL = ', RMXBL
      WRITE(*,9200) 'ITER1 RLX = ', RLX
      WRITE(*,9300) 'ITER1 VMXBL = ', VMXBL
      WRITE(*,9100) 'ITER1 IMXBL = ', IMXBL
      WRITE(*,9100) 'ITER1 ISMXBL = ', ISMXBL
      WRITE(*,9200) 'ITER1 CL = ', CL
      WRITE(*,9200) 'ITER1 CM = ', CM
      WRITE(*,9200) 'ITER1 CD = ', CD
      WRITE(*,9200) 'ITER1 CDF = ', CDF
      WRITE(*,9200) 'ITER1 CDP = ', CDP
      WRITE(*,9200) 'ITER1 ALFA = ', ALFA
C
C ==== state that iteration 2 enters with ====
      WRITE(*,9100) 'STATE NBL1 = ', NBL(1)
      WRITE(*,9100) 'STATE NBL2 = ', NBL(2)
      WRITE(*,9100) 'STATE IBLTE1 = ', IBLTE(1)
      WRITE(*,9100) 'STATE IBLTE2 = ', IBLTE(2)
      WRITE(*,9100) 'STATE ITRAN1 = ', ITRAN(1)
      WRITE(*,9100) 'STATE ITRAN2 = ', ITRAN(2)
      WRITE(*,9200) 'STATE SST = ', SST
C---- Top page (IS=1): Stations 1-6 and IBLTE-2..IBLTE+2
      DO 80 IBL=1, 6
        WRITE(*,9210) 'STATE TOP THET ', IBL, THET(IBL,1)
        WRITE(*,9210) 'STATE TOP DSTR ', IBL, DSTR(IBL,1)
        WRITE(*,9210) 'STATE TOP CTAU ', IBL, CTAU(IBL,1)
        WRITE(*,9210) 'STATE TOP UEDG ', IBL, UEDG(IBL,1)
        WRITE(*,9210) 'STATE TOP MASS ', IBL, MASS(IBL,1)
   80 CONTINUE
      DO 81 IBL=IBLTE(1)-2, IBLTE(1)+2
        WRITE(*,9210) 'STATE TOP THET ', IBL, THET(IBL,1)
        WRITE(*,9210) 'STATE TOP DSTR ', IBL, DSTR(IBL,1)
        WRITE(*,9210) 'STATE TOP CTAU ', IBL, CTAU(IBL,1)
        WRITE(*,9210) 'STATE TOP UEDG ', IBL, UEDG(IBL,1)
   81 CONTINUE
C---- Bottom page (IS=2): Stations 1-6 and IBLTE-2..IBLTE+2
      DO 82 IBL=1, 6
        WRITE(*,9210) 'STATE BOT THET ', IBL, THET(IBL,2)
        WRITE(*,9210) 'STATE BOT DSTR ', IBL, DSTR(IBL,2)
        WRITE(*,9210) 'STATE BOT CTAU ', IBL, CTAU(IBL,2)
        WRITE(*,9210) 'STATE BOT UEDG ', IBL, UEDG(IBL,2)
        WRITE(*,9210) 'STATE BOT MASS ', IBL, MASS(IBL,2)
   82 CONTINUE
      DO 83 IBL=IBLTE(2)-2, IBLTE(2)+2
        WRITE(*,9210) 'STATE BOT THET ', IBL, THET(IBL,2)
        WRITE(*,9210) 'STATE BOT DSTR ', IBL, DSTR(IBL,2)
        WRITE(*,9210) 'STATE BOT CTAU ', IBL, CTAU(IBL,2)
        WRITE(*,9210) 'STATE BOT UEDG ', IBL, UEDG(IBL,2)
   83 CONTINUE
C---- GAM on panels 1-5 and around the stagnation point
      DO 84 I=1, 5
        WRITE(*,9210) 'STATE GAM ', I, GAM(I)
   84 CONTINUE
      DO 85 I=IST-2, IST+2
        WRITE(*,9210) 'STATE GAM ', I, GAM(I)
   85 CONTINUE
C
C ==== Complete VISCAL-run until convergence (xoper.f VISCAL) ====
C---- The manual iteration-1 block above is VISCAL-equivalent; VISCAL
C---- continues from here with the current state (idempotent
C---- initialization, then Newton loop until RMSBL < EPS1).
      CALL VISCAL(100)
      WRITE(*,9200) 'FINAL CL = ', CL
      WRITE(*,9200) 'FINAL CM = ', CM
      WRITE(*,9200) 'FINAL CD = ', CD
      WRITE(*,9200) 'FINAL CDF = ', CDF
      WRITE(*,9200) 'FINAL CDP = ', CDP
      WRITE(*,9200) 'FINAL RMSBL = ', RMSBL
      WRITE(*,9200) 'FINAL ALFA = ', ALFA
      WRITE(*,9100) 'FINAL ITRAN1 = ', ITRAN(1)
      WRITE(*,9100) 'FINAL ITRAN2 = ', ITRAN(2)
C
 9100 FORMAT(1X, A, I6)
 9200 FORMAT(1X, A, ES17.8)
 9210 FORMAT(1X, A, I6, ES17.8)
 9211 FORMAT(1X, A, I6, I6, ES17.8)
 9300 FORMAT(1X, A, A2)
      END

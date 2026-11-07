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

      PROGRAM C11A_OPER_SINGLEPOINT_HARNESS
C--------------------------------------------------------------------
C     OPER single-point harness (NACA 2412, PANGEN, Re=1e6,
C     Mach=0): canonical OPER single-point semantics per xoper.f
C     for the three commands ALFA (416-437), CLI (475-507) and
C     CL (510-542). Per point:
C       ALFA: LALFA=.TRUE., ALFA=DTOR*ADEG, SPECAL (428)
C       CLI : LALFA=.TRUE., ALFA=0.0, SPECCL (484)
C       CL  : LALFA=.FALSE., ALFA=0.0, SPECCL (519)
C Then the conditional single-command invalidation (430-432/487-489/
C 523-525) happen after the inviscid solution, whose ALFA/MINF are
C     authoritative; LIPAN/LBLINI are retained; for LVISC VISCAL(ITMAX)
C     (435/492/527), no ITMAX+5, no NSEQEX.
C     The VISCAL kernel (xoper.f 2901-3027) is inlined with an iteration counter
C     inline mapped; the LALFA branch (2981) selects MRCL/COMSET
C     (LALFA=true, CLI/ALFA) or QISET/UICALC (LALFA=false, CL).
C     Scenarios:
C       A  alfa 2 (cold), alfa 2 (warm, identical)  -> iterations 9/1
C       B  CLI 0.8 inviscid, CLI 0.8 viscous
C       C  Cl 0.8 viscous
C       D  CLI 0.8 viscous vs. Cl 0.8 viscous (same target Cl,
C          same fresh state, different LALFA)
C       E  Alfa 2 viscous with ITMAX=1 (non-convergence)
C     Output per point (It24.16): target Cl/Alpha, achieved ADEG,
C     CL/CM/CD/CDF/CDP, iteration count, LVCONV, RMSBL, RMXBL, ITRAN,
C     LALFA, warm-start flags (LIPAN/LBLINI/LWAKE/LWDIJ/LADIJ),
C     AWAKE/AVISC/MVISC/MINF, CLSPEC and FCPMIN (CPMN/CPMNI/CPMNV/
C     XCPMNI/XCPMNV). Canonical build flags -fdefault-real-8
C     -fdefault-double-8. No changes to the canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER NIT, NSIDE
      CHARACTER*48 ANM
      ITMAX = 100
C
C ==== A: repeated alfa single point (cold, then warm) ====
      CALL SETUP(NSIDE, ANM)
      ADEG = 2.0
      LALFA = .TRUE.
      ALFA = DTOR*ADEG
      QINF = 1.0
      CALL SPECAL
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL VISCALSEQ(ITMAX, NIT)
      ADEG = ALFA/DTOR
      CALL PT('A-ALFA2-KALT', NIT)
      ADEG = 2.0
      LALFA = .TRUE.
      ALFA = DTOR*ADEG
      QINF = 1.0
      CALL SPECAL
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL VISCALSEQ(ITMAX, NIT)
      ADEG = ALFA/DTOR
      CALL PT('A-ALFA2-WARM', NIT)
C
C ==== B1: CLI 0.8 inviscid ====
      CALL SETUP(NSIDE, ANM)
      LVISC = .FALSE.
      CLSPEC = 0.8
      LALFA = .TRUE.
      ALFA = 0.0
      QINF = 1.0
      CALL SPECCL
      ADEG = ALFA/DTOR
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL PT('B-CLI-INVISCID', 0)
C
C ==== B2: CLI 0.8 viscous (LALFA=true) ====
      CALL SETUP(NSIDE, ANM)
      LVISC = .TRUE.
      CLSPEC = 0.8
      LALFA = .TRUE.
      ALFA = 0.0
      QINF = 1.0
      CALL SPECCL
      ADEG = ALFA/DTOR
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL VISCALSEQ(ITMAX, NIT)
      ADEG = ALFA/DTOR
      CALL PT('B-CLI-VISC', NIT)
C
C ==== C: Cl 0.8 viscous (LALFA=false) ====
      CALL SETUP(NSIDE, ANM)
      LVISC = .TRUE.
      CLSPEC = 0.8
      LALFA = .FALSE.
      ALFA = 0.0
      QINF = 1.0
      CALL SPECCL
      ADEG = ALFA/DTOR
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL VISCALSEQ(ITMAX, NIT)
      ADEG = ALFA/DTOR
      CALL PT('C-CL-VISC', NIT)
C
C ==== E: Alfa 2 viscous with ITMAX=1 (non-convergence) ====
      CALL SETUP(NSIDE, ANM)
      ADEG = 2.0
      LALFA = .TRUE.
      ALFA = DTOR*ADEG
      QINF = 1.0
      CALL SPECAL
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL VISCALSEQ(1, NIT)
      ADEG = ALFA/DTOR
      CALL PT('E-ALFA2-ITMAX1', NIT)
C
      END
C
      SUBROUTINE SETUP(NSIDE, ANM)
C     COMMON initialization (as xfoil.f INIT) + NACA 2412 + PANGEN.
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, NSIDE
      CHARACTER*(*) ANM
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
      REINF1 = 1.0E6
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
      CLSPEC = 0.0
      ADEG = 0.0
      NSIDE = IQX/3
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,ANM)
      CALL PANGEN(.TRUE.)
      END
C
      SUBROUTINE VISCALSEQ(NITER1, NITERDONE)
C Inlined VISCAL kernel (xoper.f 2901-3027) without display WRITEs, with an
C     iteration counter. LALFA branch (2981): MRCL/COMSET (true) or.
C     QISET/UICALC (false). Return value NITERDONE = Iterations.
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
      SUBROUTINE PT(LAB, NIT)
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      CHARACTER*(*) LAB
      INTEGER NIT
      CALL FCPMIN
      WRITE(*,9000) 'SP '//LAB//' IPOINT  ', 1
      WRITE(*,9200) 'SP '//LAB//' CLSPEC  ', CLSPEC
      WRITE(*,9200) 'SP '//LAB//' ADEG    ', ADEG
      WRITE(*,9200) 'SP '//LAB//' CL      ', CL
      WRITE(*,9200) 'SP '//LAB//' CM      ', CM
      WRITE(*,9200) 'SP '//LAB//' CD      ', CD
      WRITE(*,9200) 'SP '//LAB//' CDF     ', CDF
      WRITE(*,9200) 'SP '//LAB//' CDP     ', CDP
      WRITE(*,9000) 'SP '//LAB//' NITER   ', NIT
      WRITE(*,9110) 'SP '//LAB//' LVCONV  ', LVCONV
      WRITE(*,9200) 'SP '//LAB//' RMSBL   ', RMSBL
      WRITE(*,9200) 'SP '//LAB//' RMXBL   ', RMXBL
      WRITE(*,9000) 'SP '//LAB//' ITRAN1  ', ITRAN(1)
      WRITE(*,9000) 'SP '//LAB//' ITRAN2  ', ITRAN(2)
      WRITE(*,9110) 'SP '//LAB//' LALFA   ', LALFA
      WRITE(*,9110) 'SP '//LAB//' LIPAN   ', LIPAN
      WRITE(*,9110) 'SP '//LAB//' LBLINI  ', LBLINI
      WRITE(*,9110) 'SP '//LAB//' LWAKE   ', LWAKE
      WRITE(*,9110) 'SP '//LAB//' LWDIJ   ', LWDIJ
      WRITE(*,9110) 'SP '//LAB//' LADIJ   ', LADIJ
      WRITE(*,9200) 'SP '//LAB//' AWAKE   ', AWAKE
      WRITE(*,9200) 'SP '//LAB//' AVISC   ', AVISC
      WRITE(*,9200) 'SP '//LAB//' MVISC   ', MVISC
      WRITE(*,9200) 'SP '//LAB//' MINF    ', MINF
      WRITE(*,9200) 'SP '//LAB//' CPMN    ', CPMN
      WRITE(*,9200) 'SP '//LAB//' CPMNI   ', CPMNI
      WRITE(*,9200) 'SP '//LAB//' CPMNV   ', CPMNV
      WRITE(*,9200) 'SP '//LAB//' XCPMNI  ', XCPMNI
      WRITE(*,9200) 'SP '//LAB//' XCPMNV  ', XCPMNV
 9000 FORMAT(1X, A, I6)
 9110 FORMAT(1X, A, L1)
 9200 FORMAT(1X, A, ES24.16)
      END

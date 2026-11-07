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

      PROGRAM C11A_ALFA_DIAG_HARNESS
C--------------------------------------------------------------------
C     OPER alpha cold-point diagnostics harness (NACA 2412, PANGEN,
C     Re=1e6, Mach=0): canonical OPER single command Alfa (xoper.f
C     416-437) from the fresh COMMON state. SPECAL, conditional
C     1e-5 invalidation, then VISCAL(ITMAX=100) with complete
C     iteration history (It24.16) per iteration: RMSBL, RMXBL, VMXBL,
C     IMXBL, ISMXBL, RLX, ALFA, CL, CM, CD, CDF, CDP, ITRAN(1/2),
C     NBL(1/2), IBLTE(1/2), LBLINI, LIPAN, LVCONV, LWAKE, LWDIJ,
C     AVISC, MVISC, AWAKE. The VISCAL kernel (xoper.f 2901-3027) is
C     inlined with the iteration history mapped (LALFA=true branch:
C     MRCL/COMSET). Canonical build flags -fdefault-real-8
C     -fdefault-double-8. No changes to the canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER NSIDE, NIT
      CHARACTER*48 ANM
      ITMAX = 100
      CALL SETUP(NSIDE, ANM)
      ADEG = 2.0
      LALFA = .TRUE.
      ALFA = DTOR*ADEG
      QINF = 1.0
      CALL SPECAL
      IF(ABS(ALFA-AWAKE) .GT. 1.0E-5) LWAKE  = .FALSE.
      IF(ABS(ALFA-AVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      IF(ABS(MINF-MVISC) .GT. 1.0E-5) LVCONV = .FALSE.
      CALL VISCALDIAG(ITMAX, NIT)
      END
C
      SUBROUTINE SETUP(NSIDE, ANM)
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
      SUBROUTINE VISCALDIAG(NITER1, NITERDONE)
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
      WRITE(*,9500) 'PRE it=0'
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
        WRITE(*,9500) 'IT'
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

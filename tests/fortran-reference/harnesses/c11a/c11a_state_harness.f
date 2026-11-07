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

      PROGRAM C11A_STATE_HARNESS
C--------------------------------------------------------------------
C     State harness: canonical VISCAL initialization (xoper.f
C     2904-2964) and the first Newton iteration (xoper.f 2970-3003) for
C     Clark Y (Re=1e5, Mach=0, alpha=4, no PANE). Snapshots S0-S5 at
C     the state boundaries of the first Newton iteration:
C       S0 immediately before BLSOLV (after SETBL)
C       S1 immediately after BLSOLV
C       S2 immediately after UPDATE
C       S3 immediately after MRCL/COMSET/QVFUE/GAMQV
C       S4 immediately after STMOVE
C       S5 immediately before SETBL in the second Newton iteration
C     Print: It24.16, canonical order Page 1 IBL 1..NBL(1),
C     afterwards Page 2 IBL 1..NBL(2).
C     Additionally the DIJ line of the first wake-BL panel position
C     (IPAN(IBLTE(2)+1,2), QDCALC regression) for Clark Y and
C     NACA 4412.
C     Input: CLARK_Y.dat (order as file). output: stdout.
C     Canonical build flags: -fdefault-real-8 -fdefault-double-8.
C     No changes to the canonical sources.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, IBL, J, NBREAD, IPW, IERR
      REAL ALFR
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
C ==== Alpha = 4 Degrees (canonical requirement) ====
      ALFR = 4.0*PI/180.0
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
C
C ==== case 1: Clark Y (Re=1e5, Mach=0, alpha=4, no PANE) ====
      CALL ABCOPY(.TRUE.)
      MINF = 0.0
      MINF1 = 0.0
      REINF1 = 1.0E5
      RETYP = 1
      MATYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
      ALFA = ALFR
      LVCONV = .FALSE.
      LWAKE = .FALSE.
      LIPAN = .FALSE.
      LBLINI = .FALSE.
      LWDIJ = .FALSE.
      LADIJ = .FALSE.
C
C ==== Inviscid Solution (xoper.f SPECAL) ====
      CALL SPECAL
C
C ==== VISCAL initialization (xoper.f 2904-2964) ====
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
C
C---- DIJ lines: the first two wake/BL panel positions
C     (QDCALC regression: wake-to-wake block, DIJ(153,152),
C      profile-source control block, row-major assignment)
      IPW = IPAN(IBLTE(2)+1, 2)
      J = IPAN(IBLTE(2)+2, 2)
      WRITE(*,9001) 'DIJ CASE CLARKY N ', N
      WRITE(*,9001) 'DIJ CASE CLARKY IPW1 ', IPW
      WRITE(*,9001) 'DIJ CASE CLARKY IPW2 ', J
      DO 76 J=1, N+NW
        WRITE(*,9311) 'DIJ CLARKY1 ', IPW, J, DIJ(IPW,J)
   76 CONTINUE
      IPW = IPAN(IBLTE(2)+2, 2)
      DO 77 J=1, N+NW
        WRITE(*,9311) 'DIJ CLARKY2 ', IPW, J, DIJ(IPW,J)
   77 CONTINUE
C
C ==== Newton iteration 1 with snapshots S0-S5 (xoper.f 2970-3003) ====
      CALL SETBL
      CALL SNAP('S0')
      CALL BLSOLV
      CALL SNAP('S1')
      CALL UPDATE
      CALL SNAP('S2')
      CALL MRCL(CL,MINF_CL,REINF_CL)
      CALL COMSET
      CALL QVFUE
      CALL GAMQV
      CALL SNAP('S3')
      CALL STMOVE
      CALL SNAP('S4')
      CALL CLCALC(N,X,Y,GAM,GAM_A,ALFA,MINF,QINF, XCMREF,YCMREF,
     &            CL,CM,CDP, CL_ALF,CL_MSQ)
      CALL CDCALC
      CALL SNAP('S5')
C
C ==== case 2: NACA 4412 (QDCALC regression) ====
      LBLINI = .FALSE.
      LWDIJ = .FALSE.
      LADIJ = .FALSE.
      LWAKE = .FALSE.
      LIPAN = .FALSE.
      LVCONV = .FALSE.
      RLX = 1.0
      RMSBL = 0.0
      RMXBL = 0.0
      CALL NACA(4412)
      CALL ABCOPY(.TRUE.)
      MINF = 0.0
      MINF1 = 0.0
      REINF1 = 1.0E5
      RETYP = 1
      MATYP = 1
      LVISC = .TRUE.
      LALFA = .TRUE.
      ALFA = ALFR
      CALL SPECAL
      CALL XYWAKE
      CALL QWCALC
      CALL QISET
      CALL STFIND
      CALL IBLPAN
      CALL XICALC
      CALL IBLSYS
      CALL UICALC
      DO 80 IBL=1, NBL(1)
        UEDG(IBL,1) = UINV(IBL,1)
   80 CONTINUE
      DO 81 IBL=1, NBL(2)
        UEDG(IBL,2) = UINV(IBL,2)
   81 CONTINUE
      CALL QDCALC
      IPW = IPAN(IBLTE(2)+1, 2)
      J = IPAN(IBLTE(2)+2, 2)
      WRITE(*,9001) 'DIJ CASE NACA4412 N ', N
      WRITE(*,9001) 'DIJ CASE NACA4412 IPW1 ', IPW
      WRITE(*,9001) 'DIJ CASE NACA4412 IPW2 ', J
      IPW = IPAN(IBLTE(2)+1, 2)
      DO 86 J=1, N+NW
        WRITE(*,9311) 'DIJ NACA44121 ', IPW, J, DIJ(IPW,J)
   86 CONTINUE
      IPW = IPAN(IBLTE(2)+2, 2)
      DO 87 J=1, N+NW
        WRITE(*,9311) 'DIJ NACA44122 ', IPW, J, DIJ(IPW,J)
   87 CONTINUE
C
 9001 FORMAT(1X, A, I6)
 9311 FORMAT(1X, A, I6, I6, ES24.16)
      END

      SUBROUTINE SNAP(LAB)
C--------------------------------------------------------------------
C     Snapshot at a state boundary of the first Newton iteration.
C     It24.16, canonical order: scalars, then BL station fields,
C     IPAN/ISYS, VDEL, GAM, QVIS, WGAP.
C--------------------------------------------------------------------
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      CHARACTER*(*) LAB
      INTEGER I, JV, IW
C---- Scalars
      WRITE(*,9100) 'SNAP '//LAB//' NBL1    ', NBL(1)
      WRITE(*,9100) 'SNAP '//LAB//' NBL2    ', NBL(2)
      WRITE(*,9100) 'SNAP '//LAB//' IBLTE1  ', IBLTE(1)
      WRITE(*,9100) 'SNAP '//LAB//' IBLTE2  ', IBLTE(2)
      WRITE(*,9100) 'SNAP '//LAB//' ITRAN1  ', ITRAN(1)
      WRITE(*,9100) 'SNAP '//LAB//' ITRAN2  ', ITRAN(2)
      WRITE(*,9100) 'SNAP '//LAB//' IST     ', IST
      WRITE(*,9100) 'SNAP '//LAB//' NW      ', NW
      WRITE(*,9100) 'SNAP '//LAB//' NSYS    ', NSYS
      WRITE(*,9200) 'SNAP '//LAB//' SST     ', SST
      WRITE(*,9200) 'SNAP '//LAB//' RMSBL   ', RMSBL
      WRITE(*,9200) 'SNAP '//LAB//' RMXBL   ', RMXBL
      WRITE(*,9200) 'SNAP '//LAB//' RLX     ', RLX
      WRITE(*,9200) 'SNAP '//LAB//' ALFA    ', ALFA
      WRITE(*,9200) 'SNAP '//LAB//' CL      ', CL
C---- BL station fields (Page 1, IBL 1..NBL(1); then Page 2)
      CALL SNPA(LAB, 'THET  ', THET)
      CALL SNPA(LAB, 'DSTR  ', DSTR)
      CALL SNPA(LAB, 'CTAU  ', CTAU)
      CALL SNPA(LAB, 'UEDG  ', UEDG)
      CALL SNPA(LAB, 'MASS  ', MASS)
      CALL SNPA(LAB, 'TAU   ', TAU)
      CALL SNPA(LAB, 'DIS   ', DIS)
      CALL SNPA(LAB, 'CTQ   ', CTQ)
      CALL SNPA(LAB, 'DELT  ', DELT)
      CALL SNPA(LAB, 'TSTR  ', TSTR)
C---- Panel/system pointers
      CALL SNPI(LAB, 'IPAN  ', IPAN)
      CALL SNPI(LAB, 'ISYS  ', ISYS)
C---- Newton system (VDEL: residual and Re columns per system row)
      DO 30 JV=1, NSYS
        WRITE(*,9212) 'SNAP '//LAB//' VDEL  ', JV, 1, VDEL(1,1,JV)
        WRITE(*,9212) 'SNAP '//LAB//' VDEL  ', JV, 2, VDEL(2,1,JV)
        WRITE(*,9212) 'SNAP '//LAB//' VDEL  ', JV, 3, VDEL(3,1,JV)
        WRITE(*,9212) 'SNAP '//LAB//' VDEL  ', JV, 4, VDEL(1,2,JV)
        WRITE(*,9212) 'SNAP '//LAB//' VDEL  ', JV, 5, VDEL(2,2,JV)
        WRITE(*,9212) 'SNAP '//LAB//' VDEL  ', JV, 6, VDEL(3,2,JV)
   30 CONTINUE
C---- Velocities
      DO 40 I=1, N
        WRITE(*,9210) 'SNAP '//LAB//' GAM   ', I, GAM(I)
   40 CONTINUE
      DO 41 I=1, N+NW
        WRITE(*,9210) 'SNAP '//LAB//' QVIS  ', I, QVIS(I)
   41 CONTINUE
C---- Wake dead air
      DO 42 IW=1, NW
        WRITE(*,9210) 'SNAP '//LAB//' WGAP  ', IW, WGAP(IW)
   42 CONTINUE
C
 9100 FORMAT(1X, A, I6)
 9200 FORMAT(1X, A, ES24.16)
 9210 FORMAT(1X, A, I6, ES24.16)
 9212 FORMAT(1X, A, I6, I6, ES24.16)
      END

      SUBROUTINE SNPA(LAB, CNAM, A)
C--------------------------------------------------------------------
C     Prints one BL station field per page in canonical order:
C     Page 1, IBL 1..NBL(1); afterwards Page 2, IBL 1..NBL(2).
C--------------------------------------------------------------------
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
C--------------------------------------------------------------------
C     Prints a BL stations pointer per page in canonical order.
C--------------------------------------------------------------------
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

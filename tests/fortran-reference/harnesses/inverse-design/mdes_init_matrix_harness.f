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

C File: tests/fortran-reference/harnesses/inverse-design/
C mdes_init_matrix_harness.f
C     Validates the canonical numerical MDES initialization chain with
C     its guard orchestration (xmdes.f 53-57 entry, 63-107 label 1):
C       NSP/=NC1 before check, EIWSET(NC1) only for .NOT.LEIW,
C       NQSP==0 before the check, SCINIT only for .NOT.LSCINI (afterwards
C LQSPEC=.FALSE.), MAPGAM(1,ALGAM) always, CNCALC(QGAMM)+QSPCIR only
C       for .NOT.LQSPEC.
C     Cases: F missing contour (N==0, no state transition),
C       A NACA 0012 via ABCOPY (xgdes.f 961), D repeated INIT
C       (SCINIT/CNCALC/QSPCIR not reached, MAPGAM runs again),
C       E INIT after panel change PANGEN (xfoil.f 1623, LSCINI+LQSPEC
C       .FALSE. xfoil.f 2087-2088), B NACA 2412 via PANGEN,
C       C CLARK Y via ABCOPY, G capacity limit NC1=ICX=257.
C     Return values are the flags, active lengths, control values and samples
C     for the core/session comparison. Uses only
C     canonical routines; changes no source, no reference, no
C     golden file. The CIRCLE-side calls run via the glue units
C     in mdes_init_matrix_glue.f (PI conflict XFOIL.INC/CIRCLE.INC).
C     The CLARK-Y contour is from the existing Test input file via
C     the environment variable CLARKY_DAT is read (input data only, no
C     canonical READ logic needed).
      PROGRAM MDESINITMATRIX
      INCLUDE 'XFOIL.INC'
      DIMENSION UU1(400),UU2(400),UU3(400)
      CHARACTER*48 CNM
      CHARACTER*256 FNM
      INTEGER ISTATUS, ILEN
      INTEGER NCG,MC0G,MCTG
      REAL WC1O,WC2O,WCNO
      EXTERNAL RUNEIWSET, RUNSCINIT, RUNCNCALC, RUNCIR, RUNWC, RUNGRID
      EXTERNAL GUARDINIT, ILOG, READDAT, WRITCASE

      WRITE(*,900) 'MDES-INIT-MATRIX-HARNESS'
C
C----- INIT defaults according to xfoil.f INIT (392-395, 530-539):
C      PI/HOPI/QOPI/DTOR, NC1 = min(2**NN+1, 257) = 257,
C      NPAN=160, CVPAR=1.0, CTERAT=0.15, CTRRAT=0.2.
C      operating point (session defaults): ALFA=0.1 (test requirement),
C      MINF=0.0, QINF=1.0 (INIT defaults xfoil.f).
      PI = 4.0*ATAN(1.0)
      HOPI = 0.50/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
      NC1 = 257
      NPAN = 160
      CVPAR = 1.0
      CTERAT = 0.15
      CTRRAT = 0.2
      ALFA = 0.1
      MINF = 0.0
      QINF = 1.0
C
C----- case F: missing contour (N==0). Entry check xmdes.f 53-57:
C      no state transition.
      N = 0
      CALL GUARDINIT(ISTATUS)
      CALL WRITCASE('F', ISTATUS)
C
C----- case A: NACA 0012, ABCOPY (Buffer->Current, LSCINI=.FALSE.
C      xgdes.f 1027), complete INIT path.
      NSIDE = 123
      CALL NACA4(12,UU1,UU2,UU3,NSIDE,XB,YB,NB,CNM)
      CALL ABCOPY(.FALSE.)
      CALL GUARDINIT(ISTATUS)
      CALL WRITCASE('A', ISTATUS)
C
C----- case D: repeated INIT without panel change: SCINIT and
C      CNCALC/QSPCIR not reached (LSCINI/LQSPEC .TRUE.), MAPGAM runs
C      again (xmdes.f 93-95 unused).
      CALL GUARDINIT(ISTATUS)
      CALL WRITCASE('D', ISTATUS)
C
C----- case E: INIT after panel change: PANGEN on the same buffer
C      (LSCINI=.FALSE. and LQSPEC=.FALSE. xfoil.f 2087-2088).
      CALL PANGEN(.TRUE.)
      CALL GUARDINIT(ISTATUS)
      CALL WRITCASE('E', ISTATUS)
C
C----- case B: NACA 2412, PANGEN, complete INIT path.
      NSIDE = 123
      CALL NACA4(2412,UU1,UU2,UU3,NSIDE,XB,YB,NB,CNM)
      CALL PANGEN(.TRUE.)
      CALL GUARDINIT(ISTATUS)
      CALL WRITCASE('B', ISTATUS)
C
C----- case C: CLARK Y, ABCOPY, complete INIT path.
      CALL GETENV('CLARKY_DAT', FNM)
      ILEN = LEN_TRIM(FNM)
      IF (ILEN .LE. 0) STOP 'CLARKY_DAT is not set'
      CALL READDAT(FNM, ILEN, CNM, ISTATUS)
      IF (ISTATUS .NE. 0) THEN
        WRITE(*,910) 'Failed to read the CLARK-Y file', ISTATUS
        STOP 2
      ENDIF
      CALL ABCOPY(.FALSE.)
      CALL GUARDINIT(ISTATUS)
      CALL WRITCASE('C', ISTATUS)
C
C----- case G: capacity limit: NC1 == ICX == 257 is exactly the
C      EIWSET boundary (CIRCLE.INC ICX=257); NC1 > ICX leads in Fortran
C      to the STOP 'EIWSET: Array overflow' (a program abort, not
C      executable here). The boundary values are output.
      CALL RUNEIWSET(NC1)
      CALL RUNGRID(NCG,MC0G,MCTG)
      WRITE(*,920) 'G', NC1, NCG, MC0G, MCTG
920   FORMAT('MI ',A2,' NC1',I5,' NC',I5,' MC',I5,' MCT',I5)
      CALL RUNWC(WC1O,WC2O,WCNO)
      WRITE(*,930) WC1O, WC2O, WCNO
930   FORMAT('MW ',3ES22.12)
      STOP 0
900   FORMAT(A)
910   FORMAT(A,I4)
      END
C
C------------------------------------------------------------
C     Canonical guard orchestration of the numerical INIT path
C     (xmdes.f 63-107 label 1) without plot/prompt/WRITE parts.
      SUBROUTINE GUARDINIT(ISTATUS)
      INCLUDE 'XFOIL.INC'
      INTEGER ISTATUS
      EXTERNAL RUNEIWSET, RUNSCINIT, RUNCNCALC
C
C----- Entry check xmdes.f 53-57: N==0 -> no transition.
      IF (N .EQ. 0) THEN
        ISTATUS = 1
        RETURN
      ENDIF
      ISTATUS = 0
C
C----- xmdes.f 65-68: the current target Q does not come from this NSP.
      IF (NSP .NE. NC1) THEN
        LQSPEC = .FALSE.
        IQ1 = 1
        IQ2 = NC1
      ENDIF
C
C----- xmdes.f 71-72: EIWSET only for .NOT. LEIW (cache contract).
      IF (.NOT.LEIW) CALL RUNEIWSET(NC1)
      LEIW = .TRUE.
C
C----- xmdes.f 75-79: Qspec alpha never set -> current ALFA.
      IF (NQSP .EQ. 0) THEN
        IACQSP = 1
        ALQSP(1) = ALFA
        NQSP = 1
      ENDIF
C
C ----- xmdes.f 81-87: SCINIT only for.NOT.LSCINI, afterwards
C LQSPEC=.FALSE.
      IF (.NOT.LSCINI) THEN
        CALL RUNSCINIT(N,X,XP,Y,YP,S,SLE)
        LSCINI = .TRUE.
        LQSPEC = .FALSE.
      ENDIF
C
C----- xmdes.f 90-91: MAPGAM always (IAC=1, given ALFA).
      ALGAM = ALFA
      CALL MAPGAM(1,ALGAM,CLGAM,CMGAM)
C
C----- xmdes.f 93-97: CNCALC+QSPCIR only for.NOT.LQSPEC.
      IF (.NOT.LQSPEC) THEN
        CALL RUNCNCALC(QGAMM,.FALSE.)
        CALL QSPCIR
      ENDIF
      RETURN
      END
C
      INTEGER FUNCTION ILOG(L)
      LOGICAL L
      IF (L) THEN
        ILOG = 1
      ELSE
        ILOG = 0
      ENDIF
      RETURN
      END
C
C     Case output: flags, active lengths, operating point, Qspec alphas.
      SUBROUTINE WRITCASE(TAGC, ISTATUS)
      INCLUDE 'XFOIL.INC'
      CHARACTER*(*) TAGC
      INTEGER ISTATUS, ILEIW, ILSCIN, ILQSPC, IC
      INTEGER NCO,MC0,MCTO
      REAL DWCO,AGTEO,AG0O,QIM0O,QIMOLDO
      REAL DZTER,DZTEI,ZLODR,ZLODI,CHZR,CHZI
      REAL CN0R,CN0I,CN1R,CN1I,CN2R,CN2I,CN3R,CN3I
      REAL OSC1,OSCM,OSCN,OXC1,OXCM,OXCN
      REAL ZC1R,ZC1I,ZCNR,ZCNI
      REAL PQ1R,PQM,PQN
      REAL ZW1R,ZW1I,ZWNR,ZWNI
      EXTERNAL RUNCIR, ILOG
      ILEIW = ILOG(LEIW)
      ILSCIN = ILOG(LSCINI)
      ILQSPC = ILOG(LQSPEC)
      WRITE(*,1000) TAGC, ISTATUS, N, NC1, NSP, NQSP, IACQSP, KQTARG,
     & IQ1, IQ2, ILEIW, ILSCIN, ILQSPC
1000  FORMAT('MI ',A2,' ST',I2,' N',I5,' NC1',I5,' NSP',I5,' NQSP',I3,
     & ' IAC',I2,' KQT',I3,' IQ1',I5,' IQ2',I5,
     & ' LEIW',I2,' LSCINI',I2,' LQSPEC',I2)
      WRITE(*,1010) ALFA, MINF, QINF, ALGAM, CLGAM, CMGAM, SSPLE
1010  FORMAT('MO ',7ES22.12)
      IF (NQSP .GE. 1) WRITE(*,1020)
     & (ALQSP(K)/DTOR,CLQSP(K),CMQSP(K),K=1,NQSP)
1020  FORMAT('MQ ',3ES22.12)
      DO 10 IC = 1, NSP, 8
        WRITE(*,1030) IC, SSPEC(IC), QGAMM(IC), XSPOC(IC), YSPOC(IC),
     &   QSPEC(IC,1), QSPECP(IC,1)
1030    FORMAT('MS ',I4,6ES22.12)
   10 CONTINUE
      CALL RUNCIR(NCO,MC0,MCTO,DWCO,AGTEO,AG0O,QIM0O,QIMOLDO,
     & DZTER,DZTEI,ZLODR,ZLODI,CHZR,CHZI,
     & CN0R,CN0I,CN1R,CN1I,CN2R,CN2I,CN3R,CN3I,
     & OSC1,OSCM,OSCN,OXC1,OXCM,OXCN,
     & ZC1R,ZC1I,ZCNR,ZCNI,
     & PQ1R,PQM,PQN,
     & ZW1R,ZW1I,ZWNR,ZWNI)
      WRITE(*,1040) NCO,MC0,MCTO,DWCO,AGTEO,AG0O,QIM0O,QIMOLDO
1040  FORMAT('MC ',3I8,5ES22.12)
      WRITE(*,1050) DZTER,DZTEI,ZLODR,ZLODI,CHZR,CHZI
1050  FORMAT('MZ ',6ES22.12)
      WRITE(*,1060) CN0R,CN0I,CN1R,CN1I,CN2R,CN2I,CN3R,CN3I
1060  FORMAT('MN ',8ES22.12)
      WRITE(*,1070) OSC1,OSCM,OSCN,OXC1,OXCM,OXCN
1070  FORMAT('MK ',6ES22.12)
      WRITE(*,1080) ZC1R,ZC1I,ZCNR,ZCNI,PQ1R,PQM,PQN,ZW1R,ZW1I,ZWNR,ZWNI
1080  FORMAT('MT ',11ES22.12)
      RETURN
      END
C
C     CLARK-Y input file read: first line the name, then x y until EOF.
C     Only data acquisition in the harness; no canonical READ logic.
      SUBROUTINE READDAT(FNM, ILEN, DNAME, IERR)
      INCLUDE 'XFOIL.INC'
      CHARACTER*(*) FNM
      CHARACTER*(*) DNAME
      INTEGER ILEN, IERR, IU, IOS
      REAL XV, YV
      DNAME = ' '
      OPEN(UNIT=99, FILE=FNM(1:ILEN), STATUS='OLD', IOSTAT=IU)
      IF (IU .NE. 0) THEN
        IERR = 3
        RETURN
      ENDIF
      READ(99,*,IOSTAT=IU) DNAME
      IF (IU .NE. 0) THEN
        CLOSE(99)
        IERR = 4
        RETURN
      ENDIF
      NB = 0
   20 CONTINUE
      READ(99,*,IOSTAT=IOS) XV, YV
      IF (IOS .NE. 0) GO TO 21
      NB = NB + 1
      IF (NB .GT. IBX) THEN
        IERR = 5
        RETURN
      ENDIF
      XB(NB) = XV
      YB(NB) = YV
      GO TO 20
   21 CONTINUE
      CLOSE(99)
      IF (NB .LE. 1) THEN
        IERR = 6
        RETURN
      ENDIF
      IERR = 0
      RETURN
      END

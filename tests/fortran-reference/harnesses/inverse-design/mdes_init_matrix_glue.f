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
C mdes_init_matrix_glue.f
C Harness glue for the MDES-INIT matrix harness
C (mdes_init_matrix_harness.f).
C XFOIL.INC and CIRCLE.INC both define the symbol PI in two
C COMMON blocks;
C     a single program unit therefore cannot INCLUDE both. These
C     glue units INCLUDE only CIRCLE.INC and call the canonical
C     CIRCLE-side routines (EIWSET, SCINIT, CNCALC) or
C     pass CIRCLE COMMON values. They change no Fortran source.
      SUBROUTINE RUNEIWSET(NC1IN)
      INCLUDE 'CIRCLE.INC'
      INTEGER NC1IN
      CALL EIWSET(NC1IN)
      RETURN
      END
C
      SUBROUTINE RUNSCINIT(NIN,XIN,XPIN,YIN,YPIN,SIN,SLEIN)
      INCLUDE 'CIRCLE.INC'
      INTEGER NIN,I
      REAL XIN(*),XPIN(*),YIN(*),YPIN(*),SIN(*),SLEIN
      DIMENSION XT(400),XPT(400),YT(400),YPT(400),ST(400)
      IF (NIN .GT. 400) STOP 'RUNSCINIT: NIN > 400'
      DO 1 I = 1, NIN
        XT(I) = XIN(I)
        XPT(I) = XPIN(I)
        YT(I) = YIN(I)
        YPT(I) = YPIN(I)
        ST(I) = SIN(I)
    1 CONTINUE
      CALL SCINIT(NIN,XT,XPT,YT,YPT,ST,SLEIN)
      RETURN
      END
C
      SUBROUTINE RUNCNCALC(QCIN,LSYMMIN)
      INCLUDE 'CIRCLE.INC'
      LOGICAL LSYMMIN
      REAL QCIN(*)
      CALL CNCALC(QCIN,LSYMMIN)
      RETURN
      END
C
      SUBROUTINE RUNCIR(NCO,MC0,MCTO,DWCO,AGTEO,AG0O,QIM0O,QIMOLDO,
     & DZTER,DZTEI,ZLODR,ZLODI,CHZR,CHZI,
     & CN0R,CN0I,CN1R,CN1I,CN2R,CN2I,CN3R,CN3I,
     & SC1,SCM,SCN,XC1,XCM,XCN,
     & ZC1R,ZC1I,ZCNR,ZCNI,
     & PQ1R,PQM,PQN,
     & ZW1R,ZW1I,ZWNR,ZWNI)
      INCLUDE 'CIRCLE.INC'
      INTEGER NCO,MC0,MCTO
      REAL DWCO,AGTEO,AG0O,QIM0O,QIMOLDO
      REAL DZTER,DZTEI,ZLODR,ZLODI,CHZR,CHZI
      REAL CN0R,CN0I,CN1R,CN1I,CN2R,CN2I,CN3R,CN3I
      REAL SC1,SCM,SCN,XC1,XCM,XCN
      REAL ZC1R,ZC1I,ZCNR,ZCNI
      REAL PQ1R,PQM,PQN
      REAL ZW1R,ZW1I,ZWNR,ZWNI
      NCO = NC
      MC0 = MC
      MCTO = MCT
      DWCO = DWC
      AGTEO = AGTE
      AG0O = AG0
      QIM0O = QIM0
      QIMOLDO = QIMOLD
      DZTER = REAL(DZTE)
      DZTEI = IMAG(DZTE)
      ZLODR = REAL(ZLEOLD)
      ZLODI = IMAG(ZLEOLD)
      CHZR = REAL(CHORDZ)
      CHZI = IMAG(CHORDZ)
      CN0R = REAL(CN(0))
      CN0I = IMAG(CN(0))
      CN1R = REAL(CN(1))
      CN1I = IMAG(CN(1))
      CN2R = REAL(CN(2))
      CN2I = IMAG(CN(2))
      CN3R = REAL(CN(3))
      CN3I = IMAG(CN(3))
      IF (NC .LE. 0) THEN
        SC1 = 0.0
        SCM = 0.0
        SCN = 0.0
        XC1 = 0.0
        XCM = 0.0
        XCN = 0.0
        ZC1R = 0.0
        ZC1I = 0.0
        ZCNR = 0.0
        ZCNI = 0.0
        PQ1R = 0.0
        PQM = 0.0
        PQN = 0.0
        ZW1R = 0.0
        ZW1I = 0.0
        ZWNR = 0.0
        ZWNI = 0.0
        RETURN
      ENDIF
      SC1 = SC(1)
      SCM = SC((NC-1)/2+1)
      SCN = SC(NC)
      XC1 = XCOLD(1)
      XCM = XCOLD((NC-1)/2+1)
      XCN = XCOLD(NC)
      ZC1R = REAL(ZC(1))
      ZC1I = IMAG(ZC(1))
      ZCNR = REAL(ZC(NC))
      ZCNI = IMAG(ZC(NC))
      PQ1R = REAL(PIQ(1))
      PQM = REAL(PIQ((NC-1)/2+1))
      PQN = REAL(PIQ(NC))
      ZW1R = REAL(ZCOLDW(1))
      ZW1I = IMAG(ZCOLDW(1))
      ZWNR = REAL(ZCOLDW(NC))
      ZWNI = IMAG(ZCOLDW(NC))
      RETURN
      END
C
      SUBROUTINE RUNWC(WC1,WC2,WCN)
      INCLUDE 'CIRCLE.INC'
      REAL WC1,WC2,WCN
      WC1 = WC(1)
      WC2 = WC(2)
      WCN = WC(NC)
      RETURN
      END
C
      SUBROUTINE RUNGRID(NCO,MC0,MCTO)
      INCLUDE 'CIRCLE.INC'
      INTEGER NCO,MC0,MCTO
      NCO = NC
      MC0 = MC
      MCTO = MCT
      RETURN
      END

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
C scinit_mapping_harness.f
C Focused Fortran harness for the mapping/geometry kernels of the
C     Inverse core: SCINIT, CNCALC, ZCCALC, ZCNORM, ZLEFIND.
C
C     Calls only the canonical routines from
C fortran-src/{xmdes,xgeom,spline,naca}.f (extracted byte-faithfully or
C     unchanged). Changes no Fortran source, no raw reference
C     and no golden file.
C
C     Sequence: NACA4(2412) -> SCALC -> SPLIND x2 -> LEFIND -> EIWSET(257) ->
C     SCINIT (sets AGTE/AG0/QIM0/DZTE/CHORDZ/ZLEOLD/SC/CN/ZC/SCOLD/...).
C Then CNCALC with a synthetic circle velocity
C QC(w)=cos(w/2),
C     then ZCCALC(MCT) and ZCNORM(MCT). output: circle grid,
C Mapping coefficients, ZC/SC intermediate values, LE/TE quantities.
      PROGRAM SCINITMAPPING
      INCLUDE 'CIRCLE.INC'
      INTEGER NB, NSIDE, IC, M
      REAL SLE
      CHARACTER*10 NAME
      DIMENSION XB(400),YB(400),SB(400),XBP(400),YBP(400)
      DIMENSION W1(400),W2(400),W3(400)
      DIMENSION QC(ICX)
      NSIDE = 123
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL SCALC(XB,YB,SB,NB)
      CALL SPLIND(XB,XBP,SB,NB,-999.0,-999.0)
      CALL SPLIND(YB,YBP,SB,NB,-999.0,-999.0)
      CALL LEFIND(SLE,XB,XBP,YB,YBP,SB,NB)
      CALL EIWSET(257)
      CALL SCINIT(NB,XB,XBP,YB,YBP,SB,SLE)
      WRITE(*,1000) NB, NC, MC, MCT
1000  FORMAT('HDR',4I6)
      WRITE(*,1010) SLE, AGTE, AG0, QIM0
1010  FORMAT('ANG',4ES22.12)
      WRITE(*,1020) REAL(DZTE),AIMAG(DZTE),REAL(CHORDZ),AIMAG(CHORDZ),
     &              REAL(ZLEOLD),AIMAG(ZLEOLD)
1020  FORMAT('CPLX',6ES22.12)
      DO IC = 1, 6
        WRITE(*,1030) IC, SC(IC), REAL(ZC(IC)), AIMAG(ZC(IC)),
     &                XCOLD(IC), YCOLD(IC)
      ENDDO
1030  FORMAT('SCZ',I4,5ES22.12)
      DO IC = NC-2, NC
        WRITE(*,1040) IC, SC(IC), REAL(ZC(IC)), AIMAG(ZC(IC)),
     &                XCOLD(IC), YCOLD(IC)
      ENDDO
1040  FORMAT('SCZL',I4,5ES22.12)
      DO M = 0, 6
        WRITE(*,1050) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1050  FORMAT('CNSC',I4,2ES22.12)
      DO IC = 1, NC
        QC(IC) = COS(0.5*WC(IC))
      ENDDO
      CALL CNCALC(QC,.FALSE.)
      DO M = 0, 6
        WRITE(*,1060) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1060  FORMAT('CNQC',I4,2ES22.12)
      DO IC = 1, 6
        WRITE(*,1070) IC, REAL(PIQ(IC)), AIMAG(PIQ(IC))
      ENDDO
1070  FORMAT('PIQQC',I4,2ES22.12)
      CALL ZCCALC(MCT)
      DO IC = 1, 6
        WRITE(*,1080) IC, SC(IC), REAL(ZC(IC)), AIMAG(ZC(IC))
      ENDDO
1080  FORMAT('ZCZC',I4,3ES22.12)
      DO M = 1, MCT
        WRITE(*,1090) M, REAL(ZC_CN(NC,M)), AIMAG(ZC_CN(NC,M))
      ENDDO
1090  FORMAT('ZCCN',I4,2ES22.12)
      CALL ZCNORM(MCT)
      DO IC = 1, 6
        WRITE(*,1100) IC, REAL(ZC(IC)), AIMAG(ZC(IC))
      ENDDO
1100  FORMAT('ZCNN',I4,2ES22.12)
      DO M = 0, 6
        WRITE(*,1110) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1110  FORMAT('CNCN',I4,2ES22.12)
      END

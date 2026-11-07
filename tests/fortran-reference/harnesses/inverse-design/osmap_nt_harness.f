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

C File:
C tests/fortran-reference/harnesses/inverse-design/osmap_nt_harness.f
C Focused Fortran harness for the Orr-Sommerfeld path of the
C Inverse core:
C OSMAP (cubic interpolation of the Orr-Sommerfeld database) and NTCALC
C     (amplitude integration).
C
C     Calls only the canonical routines from
C     fortran-src/{osmap,ntcalc}.f. GETOSFILE is harness glue
C     (getosfile.f). The harness writes no files; the database is given via
C     the environment variable OSMAP (the build script sets it).
      PROGRAM OSMAPNT
      DIMENSION X(20),HK(20),TH(20),UE(20),VE(20)
      DIMENSION W(8),A(20,8)
      REAL RSP,WSP,HSP
      REAL AR,AR_R,AR_W,AR_H,ARW_R,ARW_W,ARW_H
      REAL AI,AI_R,AI_W,AI_H,AIW_R,AIW_W,AIW_H
      LOGICAL OK
      INTEGER N,NX,NW,I,IW
      DO 5 K = 1, 3
        IF(K.EQ.1) RSP = 1000.0
        IF(K.EQ.2) RSP = 300.0
        IF(K.EQ.3) RSP = 2000.0
        WSP = 0.01
        HSP = 2.5
        CALL OSMAP(RSP,WSP,HSP,
     &             AR,AR_R,AR_W,AR_H,ARW_R,ARW_W,ARW_H,
     &             AI,AI_R,AI_W,AI_H,AIW_R,AIW_W,AIW_H, OK)
        WRITE(*,1000) K, RSP, WSP, HSP, OK
1000    FORMAT('OSR',I3,3ES22.12,L3)
        WRITE(*,1010) AR,AR_R,AR_W,AR_H,ARW_R,ARW_W,ARW_H
1010    FORMAT('OSA',7ES22.12)
        WRITE(*,1020) AI,AI_R,AI_W,AI_H,AIW_R,AIW_W,AIW_H
1020    FORMAT('OSI',7ES22.12)
    5 CONTINUE
      N = 10
      NX = 20
      NW = 5
      DO 20 I = 1, N
        X(I) = 0.1*FLOAT(I)
        HK(I) = 2.5
        TH(I) = 0.01 + 0.001*FLOAT(I)
        UE(I) = 1.0
        VE(I) = 1.0E-5
   20 CONTINUE
      CALL NTCALC(NX,N,X,HK,TH,UE,VE,NW,W,A)
      DO 30 IW = 1, NW
        WRITE(*,1030) IW, W(IW)
1030    FORMAT('NTW',I3,ES22.12)
   30 CONTINUE
      DO 40 IW = 1, NW
        DO 41 I = 1, N
          WRITE(*,1040) IW, I, A(I,IW)
1040      FORMAT('NTA',2I3,ES22.12)
   41   CONTINUE
   40 CONTINUE
      END

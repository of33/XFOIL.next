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
C wall_profile_harness.f
C Focused Fortran harness for the wall and airfoil kernels of the
C Inverse core:
C     FS (Falkner-Skan), B3SOLV (3x3 block tridiagonal solver),
C     PRWALL (Spalding wall-profile Newton) and UWALL (wall profile U(Y)).
C
C Calls only the canonical routines from fortran-src/profil.f
C     (extracted byte-faithfully). Changes no Fortran source, no
C     raw reference and no golden file.
      PROGRAM WALLPROFILE
      INCLUDE 'BLPAR.INC'
      INTEGER N, INORM, ISPEC
      REAL BSPEC, HSPEC, ETAE, GEO, DELTA
      DIMENSION ETA(61), F(61), U(61), S(61)
      DIMENSION A(3,3,2), B(3,3,2), C(3,3,2), R(3,3,2)
      CFFAC = 1.0
      N = 61
      ETAE = 10.0
      GEO = 1.0
      INORM = 1
      ISPEC = 1
      BSPEC = 0.0
      HSPEC = 0.0
      CALL FS(INORM,ISPEC,BSPEC,HSPEC,N,ETAE,GEO,ETA,F,U,S,DELTA)
      WRITE(*,1000) INORM, ISPEC, BSPEC, HSPEC, DELTA
1000  FORMAT('FSR',2I3,3ES22.12)
      DO 10 I = 1, N, 10
        WRITE(*,1010) I, ETA(I), F(I), U(I), S(I)
1010    FORMAT('FSP',I4,4ES22.12)
   10 CONTINUE
      INORM = 1
      ISPEC = 2
      BSPEC = 0.0
      HSPEC = 2.5
      CALL FS(INORM,ISPEC,BSPEC,HSPEC,N,ETAE,GEO,ETA,F,U,S,DELTA)
      WRITE(*,1000) INORM, ISPEC, BSPEC, HSPEC, DELTA
      DO 20 I = 1, N, 10
        WRITE(*,1010) I, ETA(I), F(I), U(I), S(I)
   20 CONTINUE
      INORM = 3
      ISPEC = 2
      BSPEC = 0.0
      HSPEC = 3.0
      CALL FS(INORM,ISPEC,BSPEC,HSPEC,N,ETAE,GEO,ETA,F,U,S,DELTA)
      WRITE(*,1000) INORM, ISPEC, BSPEC, HSPEC, DELTA
      DO 30 I = 1, N, 10
        WRITE(*,1010) I, ETA(I), F(I), U(I), S(I)
   30 CONTINUE
      DO 40 I = 1, 2
        DO 41 K = 1, 3
          DO 42 L = 1, 3
            A(K,L,I) = 0.0
            B(K,L,I) = 0.0
            C(K,L,I) = 0.0
            R(K,1,I) = 0.0
            R(K,2,I) = 0.0
            R(K,3,I) = 0.0
   42       CONTINUE
   41     CONTINUE
   40   CONTINUE
      A(1,1,1) = 2.0
      A(1,2,1) = 1.0
      C(1,1,1) = 1.0
      B(1,1,2) = 1.0
      A(1,1,2) = 2.0
      A(2,2,1) = 3.0
      C(2,2,1) = 1.0
      B(2,2,2) = 1.0
      A(2,2,2) = 3.0
      A(3,3,1) = 4.0
      C(3,3,1) = 1.0
      B(3,3,2) = 1.0
      A(3,3,2) = 4.0
      R(1,1,1) = 3.0
      R(2,1,1) = 4.0
      R(3,1,1) = 5.0
      R(1,1,2) = 3.0
      R(2,1,2) = 4.0
      R(3,1,2) = 5.0
      CALL B3SOLV(A,B,C,R,2,1,3)
      DO 50 I = 1, 2
        DO 51 K = 1, 3
          WRITE(*,1020) I, K, R(K,1,I)
1020      FORMAT('BSL',2I3,ES22.12)
   51   CONTINUE
   50 CONTINUE
      CALL PRWALL_HARNESS
      CALL UWALL_HARNESS
      END

      SUBROUTINE PRWALL_HARNESS
      REAL DSTAR, THETA, UO, RT, MS, CT, BB
      REAL DO, DO_DS, DO_TH, DO_UO, DO_RT, DO_MS
      REAL UI, UI_DS, UI_TH, UI_UO, UI_RT, UI_MS
      REAL HS, HS_DS, HS_TH, HS_UO, HS_RT, HS_MS
      REAL CF, CF_DS, CF_TH, CF_UO, CF_RT, CF_MS
      REAL CD, CD_DS, CD_TH, CD_UO, CD_RT, CD_MS, CD_CT
      DSTAR = 0.025
      THETA = 0.01
      UO = 1.0
      RT = 2000.0
      MS = 0.0
      CT = 0.0
      BB = 0.0
      DO = 0.0
      DO_DS = 0.0
      DO_TH = 0.0
      DO_UO = 0.0
      DO_RT = 0.0
      DO_MS = 0.0
      UI = 0.0
      UI_DS = 0.0
      UI_TH = 0.0
      UI_UO = 0.0
      UI_RT = 0.0
      UI_MS = 0.0
      HS = 0.0
      HS_DS = 0.0
      HS_TH = 0.0
      HS_UO = 0.0
      HS_RT = 0.0
      HS_MS = 0.0
      CF = 0.0
      CF_DS = 0.0
      CF_TH = 0.0
      CF_UO = 0.0
      CF_RT = 0.0
      CF_MS = 0.0
      CD = 0.0
      CD_DS = 0.0
      CD_TH = 0.0
      CD_UO = 0.0
      CD_RT = 0.0
      CD_MS = 0.0
      CD_CT = 0.0
      CALL PRWALL(DSTAR,THETA,UO,RT,MS,CT, BB,
     &        DO, DO_DS, DO_TH, DO_UO, DO_RT, DO_MS,
     &        UI, UI_DS, UI_TH, UI_UO, UI_RT, UI_MS,
     &        HS, HS_DS, HS_TH, HS_UO, HS_RT, HS_MS,
     &        CF, CF_DS, CF_TH, CF_UO, CF_RT, CF_MS,
     &        CD, CD_DS, CD_TH, CD_UO, CD_RT, CD_MS, CD_CT )
      WRITE(*,1100) BB, DO, DO_TH, DO_RT, DO_MS
1100  FORMAT('PRA',5ES22.12)
      WRITE(*,1110) UI, UI_TH, UI_RT, UI_MS
1110  FORMAT('PRB',4ES22.12)
      WRITE(*,1120) CF, CF_TH, CF_RT, CF_MS
1120  FORMAT('PRC',4ES22.12)
      WRITE(*,1130) DO_DS, DO_UO, UI_DS, UI_UO, HS, CD, CD_CT
1130  FORMAT('PRD',7ES22.12)
      RETURN
      END

      SUBROUTINE UWALL_HARNESS
      INTEGER N
      REAL TH, UO, DO, UI, RT, CF, BB
      DIMENSION Y(65), U(65)
      N = 65
      TH = 0.01
      UO = 1.0
      DO = 0.045
      UI = 0.30
      RT = 2000.0
      CF = 0.004
      BB = 1.0
      CALL UWALL(TH,UO,DO,UI,RT,CF,BB, Y,U,N)
      DO 60 I = 1, N, 8
        WRITE(*,1200) I, Y(I), U(I)
1200    FORMAT('UWP',I4,2ES22.12)
   60 CONTINUE
      RETURN
      END

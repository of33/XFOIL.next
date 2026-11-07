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

      PROGRAM BOUNDARY_LAYER_DATA_HARNESS
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, NSIDE, ISIDE, IBLS, IW
      REAL ALFR, UEI
C
C ==== COMMON initialization (as xfoil.f INIT) ====
      PI = 4.0*ATAN(1.0)
      HOPI = 0.50/PI
      QOPI = 0.25/PI
      GAMMA = 1.4
      GAMM1 = GAMMA - 1.0
      XCMREF = 0.25
      YCMREF = 0.0
      WAKLEN = 1.0
      LIMAGE = .FALSE.
      SHARP = .FALSE.
C---- Default paneling parameters (as xfoil.f INIT)
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
C
C ==== Test 1: BLPINI (BLPAR-constants) ====
      CALL BLPINI
      WRITE(*,9200) 'BLPINI SCCON = ', SCCON
      WRITE(*,9200) 'BLPINI GACON = ', GACON
      WRITE(*,9200) 'BLPINI GBCON = ', GBCON
      WRITE(*,9200) 'BLPINI GCCON = ', GCCON
      WRITE(*,9200) 'BLPINI DLCON = ', DLCON
      WRITE(*,9200) 'BLPINI CTRCON = ', CTRCON
      WRITE(*,9200) 'BLPINI CTRCEX = ', CTRCEX
      WRITE(*,9200) 'BLPINI DUXCON = ', DUXCON
      WRITE(*,9200) 'BLPINI CTCON = ', CTCON
      WRITE(*,9200) 'BLPINI CFFAC = ', CFFAC
C
C ==== Test 2: NACA 0012 + PANGEN + SPECAL + TECALC + STFIND ====
      NSIDE = IQX/3
      CALL NACA4(12,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      MINF1 = 0.0
      REINF1 = 3.0E6
      MATYP = 1
      RETYP = 1
      ALFA = 0.0
      CALL SPECAL
      CALL TECALC
      CALL STFIND
      CALL XYWAKE
      WRITE(*,9200) 'PANEL12 N = ', REAL(N)
      WRITE(*,9100) 'PANEL12 NW = ', NW
      WRITE(*,9200) 'PANEL12 SST = ', SST
      WRITE(*,9100) 'PANEL12 IST = ', IST
      WRITE(*,9200) 'PANEL12 S1 = ', S(1)
      WRITE(*,9200) 'PANEL12 SN = ', S(N)
C
C ==== Test 3: IBLPAN (panel/BL assignment) ====
      CALL IBLPAN
      WRITE(*,9100) 'IBLPAN12 IBLTE1 = ', IBLTE(1)
      WRITE(*,9100) 'IBLPAN12 IBLTE2 = ', IBLTE(2)
      WRITE(*,9100) 'IBLPAN12 NBL1 = ', NBL(1)
      WRITE(*,9100) 'IBLPAN12 NBL2 = ', NBL(2)
      WRITE(*,9100) 'IBLPAN12 IPAN1_1 = ', IPAN(1,1)
      WRITE(*,9100) 'IBLPAN12 IPAN1_2 = ', IPAN(2,1)
      WRITE(*,9100) 'IBLPAN12 IPAN_T1 = ', IPAN(IBLTE(1),1)
      WRITE(*,9100) 'IBLPAN12 IPAN2_1 = ', IPAN(1,2)
      WRITE(*,9100) 'IBLPAN12 IPAN2_2 = ', IPAN(2,2)
      WRITE(*,9100) 'IBLPAN12 IPAN_T2 = ', IPAN(IBLTE(2),2)
      WRITE(*,9100) 'IBLPAN12 IPAN_W1 = ', IPAN(IBLTE(2)+1,2)
      WRITE(*,9100) 'IBLPAN12 IPAN_W2 = ', IPAN(IBLTE(2)+2,2)
      WRITE(*,9200) 'IBLPAN12 VTI1_1 = ', VTI(1,1)
      WRITE(*,9200) 'IBLPAN12 VTI2_1 = ', VTI(2,1)
      WRITE(*,9200) 'IBLPAN12 VTI1_2 = ', VTI(1,2)
      WRITE(*,9200) 'IBLPAN12 VTI2_2 = ', VTI(2,2)
      WRITE(*,9200) 'IBLPAN12 VTIT2 = ', VTI(IBLTE(2),2)
      WRITE(*,9200) 'IBLPAN12 VTIW1 = ', VTI(IBLTE(2)+1,2)
C
C ==== Test 4: SETBL data portion (MINF=0.3) ====
      MINF1 = 0.3
      REINF1 = 3.0E6
      MATYP = 1
      RETYP = 1
      CALL MRCL(0.0, MA_CL, RE_CL)
      CALL COMSET
      MINF = MINF1
      GAMBL = GAMMA
      GM1BL = GAMM1
      QINFBL = QINF
      TKBL = TKLAM
      TKBL_MS = TKL_MSQ
      RSTBL    = (1.0 + 0.5*GM1BL*MINF**2) ** (1.0/GM1BL)
      RSTBL_MS = 0.5*RSTBL/(1.0 + 0.5*GM1BL*MINF**2)
      HSTINV    = GM1BL*(MINF/QINFBL)**2 / (1.0 + 0.5*GM1BL*MINF**2)
      HSTINV_MS = GM1BL*( 1.0/QINFBL)**2 / (1.0 + 0.5*GM1BL*MINF**2)
     &                - 0.5*GM1BL*HSTINV / (1.0 + 0.5*GM1BL*MINF**2)
      HERAT    = 1.0 - 0.5*QINFBL**2*HSTINV
      HERAT_MS =     - 0.5*QINFBL**2*HSTINV_MS
      HVRAT = 0.0
      REYBL    = REINF1 * SQRT(HERAT**3) * (1.0+HVRAT)/(HERAT+HVRAT)
      REYBL_RE =          SQRT(HERAT**3) * (1.0+HVRAT)/(HERAT+HVRAT)
      REYBL_MS = REYBL * (1.5/HERAT - 1.0/(HERAT+HVRAT))*HERAT_MS
      WRITE(*,9200) 'SETBL3 RSTBL = ', RSTBL
      WRITE(*,9200) 'SETBL3 HSTINV = ', HSTINV
      WRITE(*,9200) 'SETBL3 HSTINV_MS = ', HSTINV_MS
      WRITE(*,9200) 'SETBL3 HERAT = ', HERAT
      WRITE(*,9200) 'SETBL3 HERAT_MS = ', HERAT_MS
      WRITE(*,9200) 'SETBL3 REYBL = ', REYBL
      WRITE(*,9200) 'SETBL3 REYBL_MS = ', REYBL_MS
      WRITE(*,9200) 'SETBL3 REYBL_RE = ', REYBL_RE
      WRITE(*,9200) 'SETBL3 TKBL = ', TKBL
      WRITE(*,9200) 'SETBL3 TKBL_MS = ', TKBL_MS
C
C ==== Test 5: XICALC (airfoil and wake stations) ====
      CALL XICALC
      WRITE(*,9200) 'XICALC12 XSSI1_1 = ', XSSI(1,1)
      WRITE(*,9200) 'XICALC12 XSSI2_1 = ', XSSI(2,1)
      WRITE(*,9200) 'XICALC12 XSSI_T1 = ', XSSI(IBLTE(1),1)
      WRITE(*,9200) 'XICALC12 XSSI1_2 = ', XSSI(1,2)
      WRITE(*,9200) 'XICALC12 XSSI2_2 = ', XSSI(2,2)
      WRITE(*,9200) 'XICALC12 XSSI_T2 = ', XSSI(IBLTE(2),2)
      WRITE(*,9200) 'XICALC12 XSSI_W1 = ', XSSI(IBLTE(2)+1,2)
      WRITE(*,9200) 'XICALC12 XSSI_W2 = ', XSSI(IBLTE(2)+2,2)
      WRITE(*,9200) 'XICALC12 XSSIW1_1 = ', XSSI(IBLTE(1)+1,1)
      WRITE(*,9200) 'XICALC12 XSSIW2_1 = ', XSSI(IBLTE(1)+2,1)
C
C ==== Test 6: XIFSET IS=1, XSTRIP=1.0 (free Transition) ====
      XSTRIP(1) = 1.0
      XSTRIP(2) = 1.0
      CALL XIFSET(1)
      WRITE(*,9200) 'XIFSET1 XIFORC = ', XIFORC
      WRITE(*,9200) 'XIFSET1 XSSI_TE = ', XSSI(IBLTE(1),1)
C
C ==== Test 7: XIFSET IS=1, XSTRIP=0.3 (forced Transition) ====
      XSTRIP(1) = 0.3
      CALL XIFSET(1)
      WRITE(*,9200) 'XIFSET2 XIFORC = ', XIFORC
      WRITE(*,9200) 'XIFSET2 XSSI_TE = ', XSSI(IBLTE(1),1)
C
C ==== Test 8: XIFSET IS=2, XSTRIP=0.3 ====
      XSTRIP(2) = 0.3
      CALL XIFSET(2)
      WRITE(*,9200) 'XIFSET3 XIFORC = ', XIFORC
      WRITE(*,9200) 'XIFSET3 XSSI_TE = ', XSSI(IBLTE(2),2)
C
C ==== Test 9: BLPRV/BLKIN (station variables) ====
      UEI = 1.0
      CALL BLPRV(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, UEI)
      WRITE(*,9200) 'BLPRV9 X2 = ', X2
      WRITE(*,9200) 'BLPRV9 AMPL2 = ', AMPL2
      WRITE(*,9200) 'BLPRV9 S2 = ', S2
      WRITE(*,9200) 'BLPRV9 T2 = ', T2
      WRITE(*,9200) 'BLPRV9 D2 = ', D2
      WRITE(*,9200) 'BLPRV9 DW2 = ', DW2
      WRITE(*,9200) 'BLPRV9 U2 = ', U2
      WRITE(*,9200) 'BLPRV9 U2_UEI = ', U2_UEI
      WRITE(*,9200) 'BLPRV9 U2_MS = ', U2_MS
      CALL BLKIN
      WRITE(*,9200) 'BLKIN9 M2 = ', M2
      WRITE(*,9200) 'BLKIN9 R2 = ', R2
      WRITE(*,9200) 'BLKIN9 H2 = ', H2
      WRITE(*,9200) 'BLKIN9 HERAT = ', HERAT
      WRITE(*,9200) 'BLKIN9 V2 = ', V2
      WRITE(*,9200) 'BLKIN9 HK2 = ', HK2
      WRITE(*,9200) 'BLKIN9 RT2 = ', RT2
C
C ==== Test 10: NACA 2412 + PANGEN + IBLPAN ====
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      CALL XYWAKE
      CALL IBLPAN
      WRITE(*,9100) 'IBLPAN24 IBLTE1 = ', IBLTE(1)
      WRITE(*,9100) 'IBLPAN24 IBLTE2 = ', IBLTE(2)
      WRITE(*,9100) 'IBLPAN24 NBL1 = ', NBL(1)
      WRITE(*,9100) 'IBLPAN24 NBL2 = ', NBL(2)
      WRITE(*,9100) 'IBLPAN24 IPAN1_1 = ', IPAN(1,1)
      WRITE(*,9100) 'IBLPAN24 IPAN_T1 = ', IPAN(IBLTE(1),1)
      WRITE(*,9100) 'IBLPAN24 IPAN_T2 = ', IPAN(IBLTE(2),2)
      WRITE(*,9100) 'IBLPAN24 IPAN_W1 = ', IPAN(IBLTE(2)+1,2)
C
 9100 FORMAT(1X, A, I6)
 9200 FORMAT(1X, A, ES17.8)
      END

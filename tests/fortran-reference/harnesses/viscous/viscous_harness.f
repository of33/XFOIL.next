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

      PROGRAM VISCOUS_HARNESS
      INCLUDE 'XFOIL.INC'
      INCLUDE 'XBL.INC'
      INTEGER I, NSIDE, IBLS, ISIDE
      REAL ALFR
      LOGICAL LSB
C
C ==== COMMON initialization (as xfoil.f INIT) ====
      PI = 4.0*ATAN(1.0)
      HOPI = 0.50/PI
      QOPI = 0.25/PI
      DTOR = PI/180.0
      COSA = 1.0
      SINA = 0.0
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
    5 CONTINUE
      REINF1 = 3.0E6
      MATYP = 1
      RETYP = 1
      MINF1 = 0.0
      LVISC = .TRUE.
C---- alpha prescribed (canonical polar chain, xoper.f ASEQ):
C---- LALFA=.FALSE. (CL prescribed, CLSPEC=0) at the degenerate point
C---- alfa=0 on a symmetric airfoil gives an exact 0/0 division (DAC)
C---- in UPDATE (xbl.f:1393), hence ALFA=NaN.
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
      CVPAR = 1.0
C
C ==== Test 1: NACA 0012 + PANGEN + SPECAL + STFIND + XYWAKE ====
      NSIDE = IQX/3
      CALL NACA4(12,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      ALFA = 0.0
      CALL SPECAL
      CALL TECALC
      CALL STFIND
      CALL XYWAKE
      WRITE(*,9200) 'PIPE12 N = ', REAL(N)
      WRITE(*,9100) 'PIPE12 NW = ', NW
      WRITE(*,9200) 'PIPE12 SST = ', SST
      WRITE(*,9100) 'PIPE12 IST = ', IST
C
C ==== Test 2: BLPINI + SETBL data part (complete, incl. MRCHUE) ====
C---- Canonical VISCAL prerequisite chain (xoper.f:2892):
C---- XYWAKE->QWCALC->QISET->STFIND->IBLPAN->XICALC->IBLSYS->UICALC
      CALL QWCALC
      CALL QISET
      CALL STFIND
      CALL IBLPAN
      CALL XICALC
      CALL IBLSYS
      CALL UICALC
      CALL UESET
C---- sources influence matrix before the Newton loop (xoper.f:2964)
      CALL QDCALC
      CALL BLPINI
      CALL SETBL
      WRITE(*,9200) 'DIAG1 THET21 = ', THET(2,1)
      WRITE(*,9200) 'DIAG1 DSTR21 = ', DSTR(2,1)
      WRITE(*,9200) 'DIAG1 CTAU21 = ', CTAU(2,1)
      WRITE(*,9200) 'DIAG1 UEDG21 = ', UEDG(2,1)
      WRITE(*,9200) 'DIAG1 THET51 = ', THET(5,1)
      WRITE(*,9200) 'DIAG1 UEDG51 = ', UEDG(5,1)
      WRITE(*,9200) 'DIAG1 THET82 = ', THET(8,2)
      WRITE(*,9200) 'DIAG1 UEDG82 = ', UEDG(8,2)
      WRITE(*,9200) 'DIAG2 XSSI822 = ', XSSI(82,2)
      WRITE(*,9200) 'DIAG2 XSSI832 = ', XSSI(83,2)
      WRITE(*,9200) 'DIAG2 UEDG822 = ', UEDG(82,2)
      WRITE(*,9200) 'DIAG2 UEDG832 = ', UEDG(83,2)
      WRITE(*,9200) 'DIAG2 WGAP1 = ', WGAP(1)
      WRITE(*,9200) 'DIAG2 WGAP2 = ', WGAP(2)
      WRITE(*,9200) 'DIAG2 WGAP3 = ', WGAP(3)
      WRITE(*,9200) 'DIAG2 THET822 = ', THET(82,2)
      WRITE(*,9200) 'DIAG2 THET832 = ', THET(83,2)
      WRITE(*,9200) 'DIAG2 QINV80 = ', QINV(80)
      WRITE(*,9200) 'DIAG2 QINV81 = ', QINV(81)
      WRITE(*,9200) 'DIAG2 QINV78 = ', QINV(78)
      WRITE(*,9200) 'DIAG2 QINV79 = ', QINV(79)
      WRITE(*,9200) 'DIAG2 QINV82 = ', QINV(82)
      WRITE(*,9200) 'DIAG2 UINV21 = ', UINV(2,1)
      WRITE(*,9200) 'SETBL12 RSTBL = ', RSTBL
      WRITE(*,9200) 'SETBL12 HSTINV = ', HSTINV
      WRITE(*,9200) 'SETBL12 REYBL = ', REYBL
      WRITE(*,9200) 'SETBL12 DWTE = ', DWTE
      WRITE(*,9100) 'SETBL12 IDAMPV = ', IDAMPV
      WRITE(*,9200) 'SETBL12 THET_T1 = ', THET(1,1)
      WRITE(*,9200) 'SETBL12 DSTR_T1 = ', DSTR(1,1)
      WRITE(*,9200) 'SETBL12 CTAU_T1 = ', CTAU(1,1)
      WRITE(*,9200) 'SETBL12 UEDG_T1 = ', UEDG(1,1)
      WRITE(*,9200) 'SETBL12 THET_T2 = ', THET(1,2)
      WRITE(*,9200) 'SETBL12 DSTR_T2 = ', DSTR(1,2)
      WRITE(*,9200) 'SETBL12 CTAU_T2 = ', CTAU(1,2)
      WRITE(*,9200) 'SETBL12 UEDG_T2 = ', UEDG(1,2)
C
C ==== Test 3: VISCAL (NITER=10, let the point converge) ====
      CALL VISCAL(10)
      WRITE(*,9200) 'VISCAL12 CL = ', CL
      WRITE(*,9200) 'VISCAL12 CM = ', CM
      WRITE(*,9200) 'VISCAL12 CD = ', CD
      WRITE(*,9200) 'VISCAL12 CDF = ', CDF
      WRITE(*,9200) 'VISCAL12 CDP = ', CDP
      WRITE(*,9200) 'VISCAL12 RMSBL = ', RMSBL
      WRITE(*,9200) 'DIAG3 RLX = ', RLX
      WRITE(*,9200) 'DIAG3 W4_50 = ', W4(50)
      WRITE(*,9200) 'DIAG3 W4_51 = ', W4(51)
      WRITE(*,9200) 'DIAG3 SNEW_50 = ', SNEW(50)
      WRITE(*,9200) 'DIAG3 SNEW_51 = ', SNEW(51)
      WRITE(*,9200) 'DIAG3 SB_50 = ', SB(50)
      WRITE(*,9200) 'DIAG3 SB_51 = ', SB(51)
      WMAX = 0.0
      DSMIN = 1.0E10
      IDSMIN = 0
      DO 2 I=1, 318
        IF(ABS(W4(I)).GT.WMAX) WMAX = ABS(W4(I))
        DS = SNEW(I+1) - SNEW(I)
        IF(DS.LT.DSMIN) THEN
          DSMIN = DS
          IDSMIN = I
        ENDIF
    2 CONTINUE
      WRITE(*,9200) 'DIAG4 WMAX = ', WMAX
      WRITE(*,9200) 'DIAG4 DSMIN = ', DSMIN
      WRITE(*,9201) 'DIAG4 IDSMIN = ', IDSMIN
      WRITE(*,9200) 'DIAG4 W4_IDSMIN = ', W4(IDSMIN)
      WRITE(*,9200) 'DIAG4 W4_IDSMIN1 = ', W4(IDSMIN+1)
      WRITE(*,9200) 'DIAG4 SNEW_IDSMIN = ', SNEW(IDSMIN)
      WRITE(*,9200) 'DIAG4 SNEW_IDSMIN1 = ', SNEW(IDSMIN+1)
      WRITE(*,9201) 'DIAG5 NN = ', NN
      WRITE(*,9201) 'DIAG5 IPFAC = ', IPFAC
      WRITE(*,9201) 'DIAG5 IBLE = ', IBLE
      WRITE(*,9200) 'DIAG5 SB1 = ', SB(1)
      WRITE(*,9200) 'DIAG5 SNEW1 = ', SNEW(1)
      WRITE(*,9200) 'DIAG5 SNEW2 = ', SNEW(2)
      WRITE(*,9200) 'DIAG5 SNEW159 = ', SNEW(159)
      WRITE(*,9200) 'DIAG5 SNEW160 = ', SNEW(160)
      WRITE(*,9200) 'DIAG5 SNEW162 = ', SNEW(162)
      WRITE(*,9200) 'DIAG5 SNEW319 = ', SNEW(319)
      WRITE(*,9100) 'VISCAL12 ITER = ', ITER
      WRITE(*,9200) 'VISCAL12 ALFA = ', ALFA
      WRITE(*,9200) 'VISCAL12 THET_T1 = ', THET(1,1)
      WRITE(*,9200) 'VISCAL12 DSTR_T1 = ', DSTR(1,1)
      WRITE(*,9200) 'VISCAL12 CTAU_T1 = ', CTAU(1,1)
      WRITE(*,9200) 'VISCAL12 UEDG_T1 = ', UEDG(1,1)
      WRITE(*,9200) 'VISCAL12 THET_B1 = ', THET(2,1)
      WRITE(*,9200) 'VISCAL12 DSTR_B1 = ', DSTR(2,1)
      WRITE(*,9200) 'VISCAL12 CTAU_B1 = ', CTAU(2,1)
      WRITE(*,9200) 'VISCAL12 UEDG_B1 = ', UEDG(2,1)
      WRITE(*,9200) 'VISCAL12 MASS_T1 = ', MASS(1,1)
C
C ==== Test 4: CDCALC (drag integration) ====
      CALL CDCALC
      WRITE(*,9200) 'CDCALC12 CD = ', CD
      WRITE(*,9200) 'CDCALC12 CDF = ', CDF
      WRITE(*,9200) 'CDCALC12 CDP = ', CDP
C
C ==== Test 5: AXSET/DAMPL/DAMPL2 (Amplification rates) ====
      CALL DAMPL(3.0, 0.01, 1.0E5, AX, AX_HK, AX_TH, AX_RT)
      WRITE(*,9200) 'DAMPL1 AX = ', AX
      WRITE(*,9200) 'DAMPL1 AX_HK = ', AX_HK
      WRITE(*,9200) 'DAMPL1 AX_RT = ', AX_RT
      CALL DAMPL2(4.5, 0.01, 2.0E5, AX2, AX2_HK, AX2_TH, AX2_RT)
      WRITE(*,9200) 'DAMPL2X AX = ', AX2
      WRITE(*,9200) 'DAMPL2X AX_RT = ', AX2_RT
C---- AXSET has 19 arguments: 10 inputs + 9 outputs (xblsys.f:35)
      CALL AXSET(3.0, 0.01, 1.0E5, 0.5, 3.0, 0.01, 1.0E5, 0.5, 9.0, 0,
     &           AXS, AXS_HK1, AXS_T1, AXS_RT1, AXS_A1,
     &               AXS_HK2, AXS_T2, AXS_RT2, AXS_A2 )
      WRITE(*,9200) 'AXSET1 AX = ', AXS
      WRITE(*,9200) 'AXSET1 AX_HK1 = ', AXS_HK1
      WRITE(*,9200) 'AXSET1 AX_HK2 = ', AXS_HK2
C
C ==== Test 5b: independent correlation kernels (fixed inputs) ====
C---- Canonical working points: laminar HK=2.2/RT=100, turbulent
C---- HK=2.2/RT=300, CFFAC=1.0 (BLPINI).
      CALL HKIN(2.2, 0.0, HKO, HKO_H, HKO_M)
      WRITE(*,9200) 'HKIN1 HK = ', HKO
      WRITE(*,9200) 'HKIN1 HK_H = ', HKO_H
      CALL HKIN(2.2, 0.04, HKO, HKO_H, HKO_M)
      WRITE(*,9200) 'HKIN2 HK = ', HKO
      WRITE(*,9200) 'HKIN2 HK_M = ', HKO_M
      CALL DIL(2.2, 100.0, DIO, DIO_HK, DIO_RT)
      WRITE(*,9200) 'DIL1 DI = ', DIO
      WRITE(*,9200) 'DIL1 DI_HK = ', DIO_HK
      WRITE(*,9200) 'DIL1 DI_RT = ', DIO_RT
      CALL DILW(2.5, 100.0, DIO, DIO_HK, DIO_RT)
      WRITE(*,9200) 'DILW1 DI = ', DIO
      WRITE(*,9200) 'DILW1 DI_HK = ', DIO_HK
      CALL HSL(2.2, 100.0, 0.0, HSO, HSO_HK, HSO_RT, HSO_M)
      WRITE(*,9200) 'HSL1 HS = ', HSO
      WRITE(*,9200) 'HSL1 HS_HK = ', HSO_HK
      CALL CFL(2.2, 100.0, 0.0, CFO, CFO_HK, CFO_RT, CFO_M)
      WRITE(*,9200) 'CFL1 CF = ', CFO
      WRITE(*,9200) 'CFL1 CF_HK = ', CFO_HK
      WRITE(*,9200) 'CFL1 CF_RT = ', CFO_RT
      CALL HST(2.2, 300.0, 0.01, HSO, HSO_HK, HSO_RT, HSO_M)
      WRITE(*,9200) 'HST1 HS = ', HSO
      WRITE(*,9200) 'HST1 HS_HK = ', HSO_HK
      WRITE(*,9200) 'HST1 HS_RT = ', HSO_RT
      WRITE(*,9200) 'HST1 HS_M = ', HSO_M
      CALL CFT(2.2, 300.0, 0.01, CFO, CFO_HK, CFO_RT, CFO_M)
      WRITE(*,9200) 'CFT1 CF = ', CFO
      WRITE(*,9200) 'CFT1 CF_HK = ', CFO_HK
      WRITE(*,9200) 'CFT1 CF_RT = ', CFO_RT
      WRITE(*,9200) 'CFT1 CF_M = ', CFO_M
      CALL HCT(2.2, 0.01, HCO, HCO_HK, HCO_M)
      WRITE(*,9200) 'HCT1 HC = ', HCO
      WRITE(*,9200) 'HCT1 HC_HK = ', HCO_HK
      WRITE(*,9200) 'HCT1 HC_M = ', HCO_M
      CALL DIT(1.9, 0.5, 0.004, 0.02, DIO, DIO_HS, DIO_US, DIO_CF,
     &         DIO_ST)
      WRITE(*,9200) 'DIT1 DI = ', DIO
      WRITE(*,9200) 'DIT1 DI_HS = ', DIO_HS
      WRITE(*,9200) 'DIT1 DI_US = ', DIO_US
      WRITE(*,9200) 'DIT1 DI_CF = ', DIO_CF
      WRITE(*,9200) 'DIT1 DI_ST = ', DIO_ST
C
C ==== Test 5c: BLVAR/BLMID with fixed Primary state ====
C---- Fixed station state: XSI=0.5, AMI=0.1, CTI=0.01, THI=0.02,
C ---- DSI=0.03, DSWAKI=0.0, UEI=1.0 (BLPRV/BLKIN), SIMI=.TRUE. for
C BLMID.
      CALL BLPRV(0.5, 0.1, 0.01, 0.02, 0.03, 0.0, 1.0)
      CALL BLKIN
      SIMI = .TRUE.
      CALL BLVAR(1)
      WRITE(*,9200) 'BLVARL HK2 = ', HK2
      WRITE(*,9200) 'BLVARL HS2 = ', HS2
      WRITE(*,9200) 'BLVARL HC2 = ', HC2
      WRITE(*,9200) 'BLVARL US2 = ', US2
      WRITE(*,9200) 'BLVARL CQ2 = ', CQ2
      WRITE(*,9200) 'BLVARL CF2 = ', CF2
      WRITE(*,9200) 'BLVARL DI2 = ', DI2
      WRITE(*,9200) 'BLVARL DE2 = ', DE2
      CALL BLMID(1)
      WRITE(*,9200) 'BLMIDL CFM = ', CFM
      CALL BLVAR(2)
      WRITE(*,9200) 'BLVART HK2 = ', HK2
      WRITE(*,9200) 'BLVART HS2 = ', HS2
      WRITE(*,9200) 'BLVART HC2 = ', HC2
      WRITE(*,9200) 'BLVART US2 = ', US2
      WRITE(*,9200) 'BLVART CQ2 = ', CQ2
      WRITE(*,9200) 'BLVART CF2 = ', CF2
      WRITE(*,9200) 'BLVART DI2 = ', DI2
      WRITE(*,9200) 'BLVART DE2 = ', DE2
      CALL BLMID(2)
      WRITE(*,9200) 'BLMIDT CFM = ', CFM
      CALL BLVAR(3)
      WRITE(*,9200) 'BLVARW HK2 = ', HK2
      WRITE(*,9200) 'BLVARW HS2 = ', HS2
      WRITE(*,9200) 'BLVARW HC2 = ', HC2
      WRITE(*,9200) 'BLVARW US2 = ', US2
      WRITE(*,9200) 'BLVARW CQ2 = ', CQ2
      WRITE(*,9200) 'BLVARW CF2 = ', CF2
      WRITE(*,9200) 'BLVARW DI2 = ', DI2
      WRITE(*,9200) 'BLVARW DE2 = ', DE2
      CALL BLMID(3)
      WRITE(*,9200) 'BLMIDW CFM = ', CFM
C
C ==== Test 6: TRCHEK/TRCHEK2 (Pre-transition, laminar) ====
C---- canonical call context as in MRCHUE/MRCHDU/SETBL (xbl.f):
C---- BLPRV/BLKIN on station k, COM1=COM2, BLPRV/BLKIN on station k+1.
C---- A standalone TRCHEK with the sweep end state (wake, AMPL1>
C---- AMCRIT) produces a degenerate Newton step in TRCHEK2
C---- (SFA division) and poisons AMPL2 for all following sweeps.
      XSI = XSSI(4,1)
      AMI = CTAU(4,1)
      CTI = 0.0
      UEI = UEDG(4,1)
      THI = THET(4,1)
      MDI = MASS(4,1)
      DSI = DSTR(4,1)
      DSWAKI = 0.0
      CALL BLPRV(XSI,AMI,CTI,THI,DSI,DSWAKI,UEI)
      CALL BLKIN
      DO 8 ICOM=1, NCOM
        COM1(ICOM) = COM2(ICOM)
    8 CONTINUE
      XSI = XSSI(5,1)
      UEI = UEDG(5,1)
      THI = THET(5,1)
      MDI = MASS(5,1)
      DSI = DSTR(5,1)
      CALL BLPRV(XSI,AMI,CTI,THI,DSI,DSWAKI,UEI)
      CALL BLKIN
      CALL TRCHEK
      WRITE(*,9300) 'TRCHEK12 TRAN1 = ', TRAN
      WRITE(*,9300) 'TRCHEK12 TURB1 = ', TURB
      WRITE(*,9200) 'TRCHEK12 AMPL2 = ', AMPL2
      WRITE(*,9200) 'TRCHEK12 XT = ', XT
      WRITE(*,9100) 'TRCHEK12 ITRAN1 = ', ITRAN(1)
      WRITE(*,9100) 'TRCHEK12 ITRAN2 = ', ITRAN(2)
C
C ==== Test 6b: TRCHEK/TRCHEK2 with fixed state ====
C---- Fixed two-station state (pre-transition, laminar), BLPINI
C---- constants, XIFORC=10.0 (no forced transition), AMCRIT=9.0,
C---- IDAMPV=0. case 1: no amplification region (AX=0, punch out).
C---- case 2: free Transition (AMPL2 crosses AMCRIT, XT sensitivities).
      XIFORC = 10.0
      AMCRIT = 9.0
      IDAMPV = 0
C---- case 1: AMPL1=0, RT~6e4 below GRCRIT-DGR -> AX=0
      CALL BLPRV(0.0, 0.0, 0.01, 0.02, 0.03, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      DO 9 ICOM=1, NCOM
        COM1(ICOM) = COM2(ICOM)
    9 CONTINUE
      AMPL1 = 0.0
      CALL BLPRV(0.01, 0.0, 0.01, 0.021, 0.031, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      CALL TRCHEK
      WRITE(*,9300) 'TRCHKN TRAN = ', TRAN
      WRITE(*,9200) 'TRCHKN AMPL2 = ', AMPL2
      WRITE(*,9200) 'TRCHKN XT = ', XT
C---- case 2: AMPL1=8.9, RT~1.5e5, AX>0 -> AMPL2 crosses AMCRIT
      CALL BLPRV(0.0, 0.0, 0.01, 0.05, 0.075, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      DO 10 ICOM=1, NCOM
        COM1(ICOM) = COM2(ICOM)
   10 CONTINUE
      AMPL1 = 8.9
      CALL BLPRV(0.1, 0.0, 0.01, 0.051, 0.0765, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      CALL TRCHEK
      WRITE(*,9300) 'TRCHKT TRAN = ', TRAN
      WRITE(*,9300) 'TRCHKT TRFREE = ', TRFREE
      WRITE(*,9300) 'TRCHKT TRFORC = ', TRFORC
      WRITE(*,9200) 'TRCHKT AMPL2 = ', AMPL2
      WRITE(*,9200) 'TRCHKT XT = ', XT
      WRITE(*,9200) 'TRCHKT XT_T1 = ', XT_T1
      WRITE(*,9200) 'TRCHKT XT_T2 = ', XT_T2
      WRITE(*,9200) 'TRCHKT XT_D1 = ', XT_D1
      WRITE(*,9200) 'TRCHKT XT_D2 = ', XT_D2
      WRITE(*,9200) 'TRCHKT XT_MS = ', XT_MS
      WRITE(*,9200) 'TRCHKT XT_RE = ', XT_RE
C
C ==== Test 7: BLSYS/TESYS/BLSOLV (Newton system and block solver) ====
      CALL SETBL
      CALL IBLSYS
      WRITE(*,9100) 'IBLSYS12 NSYS = ', NSYS
      WRITE(*,9100) 'IBLSYS12 ISYS1 = ', ISYS(1,1)
      WRITE(*,9100) 'IBLSYS12 ISYST2 = ', ISYS(IBLTE(1),1)
      CALL BLSYS
      WRITE(*,9200) 'BLSYS12 VA11 = ', VA(1,1,1)
      WRITE(*,9200) 'BLSYS12 VA21 = ', VA(1,1,2)
      WRITE(*,9200) 'BLSYS12 VM1 = ', VM(1,1,1)
      WRITE(*,9200) 'BLSYS12 VDEL1 = ', VDEL(1,1,1)
      WRITE(*,9200) 'BLSYS12 VS111 = ', VS1(1,1)
      WRITE(*,9200) 'BLSYS12 VS122 = ', VS1(2,2)
      WRITE(*,9200) 'BLSYS12 VS211 = ', VS2(1,1)
      WRITE(*,9200) 'BLSYS12 VS222 = ', VS2(2,2)
      WRITE(*,9200) 'BLSYS12 VS234 = ', VS2(2,4)
      WRITE(*,9200) 'BLSYS12 VS333 = ', VS2(3,3)
      WRITE(*,9200) 'BLSYS12 REZ1 = ', VSREZ(1)
      WRITE(*,9200) 'BLSYS12 REZ2 = ', VSREZ(2)
      WRITE(*,9200) 'BLSYS12 REZ3 = ', VSREZ(3)
      WRITE(*,9200) 'BLSYS12 VSM1 = ', VSM(1)
      WRITE(*,9200) 'BLSYS12 VSR2 = ', VSR(2)
      WRITE(*,9200) 'BLSYS12 VSX3 = ', VSX(3)
      CALL TESYS(CTAU(IBLTE(1),1), THET(IBLTE(1),1), DSTR(IBLTE(1),1))
      WRITE(*,9200) 'TESYS12 VDEL1 = ', VDEL(1,1,1)
      WRITE(*,9200) 'TESYS12 VS111 = ', VS1(1,1)
      WRITE(*,9200) 'TESYS12 VS211 = ', VS2(1,1)
      WRITE(*,9200) 'TESYS12 VS222 = ', VS2(2,2)
      WRITE(*,9200) 'TESYS12 VS333 = ', VS2(3,3)
      WRITE(*,9200) 'TESYS12 REZ1 = ', VSREZ(1)
      WRITE(*,9200) 'TESYS12 REZ2 = ', VSREZ(2)
      WRITE(*,9200) 'TESYS12 REZ3 = ', VSREZ(3)
C---- Non-degenerate RHS for BLSOLV verification (converged
C---- symmetric initial state would otherwise give VM(1,1,1)=0)
      VDEL(1,1,1) = 1.0
      VDEL(2,1,1) = 0.5
      CALL BLSOLV
      WRITE(*,9200) 'BLSOLV12 VM1 = ', VM(1,1,1)
      WRITE(*,9200) 'BLSOLV12 VM2 = ', VM(1,1,2)
      WRITE(*,9200) 'BLSOLV12 DEL111 = ', VDEL(1,1,1)
      WRITE(*,9200) 'BLSOLV12 DEL211 = ', VDEL(2,1,1)
      WRITE(*,9200) 'BLSOLV12 DEL112 = ', VDEL(1,1,2)
      WRITE(*,9200) 'BLSOLV12 DEL212 = ', VDEL(2,1,2)
      CALL UPDATE
      WRITE(*,9200) 'UPDATE12 RMSBL = ', RMSBL
      WRITE(*,9200) 'UPDATE12 RMXBL = ', RMXBL
C
C ==== Test 7b: BLDIF with fixed two-station state ====
C---- Fixed two-station state (X1=0.05/T1=0.05/D1=0.075, X2=0.15/
C---- T2=0.051/D2=0.0765, U=1.0, S=0.01, AMPL2=0.0), BLPINI-constants.
C---- X1>0 is needed (LOG(X2/X1) in BLDIF for ITYP<>0).
C---- canonical call context as in BLSYS: BLVAR/BLMID before BLDIF.
      CALL BLPRV(0.05, 0.0, 0.01, 0.05, 0.075, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      DO 11 ICOM=1, NCOM
        COM1(ICOM) = COM2(ICOM)
   11 CONTINUE
      CALL BLPRV(0.15, 0.0, 0.01, 0.051, 0.0765, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      CALL BLMID(1)
      CALL BLDIF(1)
      WRITE(*,9200) 'BLDIFL VS111 = ', VS1(1,1)
      WRITE(*,9200) 'BLDIFL VS122 = ', VS1(2,2)
      WRITE(*,9200) 'BLDIFL VS133 = ', VS1(3,3)
      WRITE(*,9200) 'BLDIFL VS211 = ', VS2(1,1)
      WRITE(*,9200) 'BLDIFL VS222 = ', VS2(2,2)
      WRITE(*,9200) 'BLDIFL VS234 = ', VS2(2,4)
      WRITE(*,9200) 'BLDIFL VS333 = ', VS2(3,3)
      WRITE(*,9200) 'BLDIFL VS235 = ', VS2(3,5)
      WRITE(*,9200) 'BLDIFL REZ1 = ', VSREZ(1)
      WRITE(*,9200) 'BLDIFL REZ2 = ', VSREZ(2)
      WRITE(*,9200) 'BLDIFL REZ3 = ', VSREZ(3)
      WRITE(*,9200) 'BLDIFL VSM2 = ', VSM(2)
      WRITE(*,9200) 'BLDIFL VSR2 = ', VSR(2)
      WRITE(*,9200) 'BLDIFL VSX3 = ', VSX(3)
      CALL BLPRV(0.05, 0.0, 0.01, 0.05, 0.075, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(2)
      DO 12 ICOM=1, NCOM
        COM1(ICOM) = COM2(ICOM)
   12 CONTINUE
      CALL BLPRV(0.15, 0.0, 0.01, 0.051, 0.0765, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(2)
      CALL BLMID(2)
      CALL BLDIF(2)
      WRITE(*,9200) 'BLDIFT VS111 = ', VS1(1,1)
      WRITE(*,9200) 'BLDIFT VS122 = ', VS1(2,2)
      WRITE(*,9200) 'BLDIFT VS211 = ', VS2(1,1)
      WRITE(*,9200) 'BLDIFT VS222 = ', VS2(2,2)
      WRITE(*,9200) 'BLDIFT VS234 = ', VS2(2,4)
      WRITE(*,9200) 'BLDIFT VS333 = ', VS2(3,3)
      WRITE(*,9200) 'BLDIFT REZ1 = ', VSREZ(1)
      WRITE(*,9200) 'BLDIFT REZ2 = ', VSREZ(2)
      WRITE(*,9200) 'BLDIFT REZ3 = ', VSREZ(3)
      WRITE(*,9200) 'BLDIFT VSM2 = ', VSM(2)
      WRITE(*,9200) 'BLDIFT VSR2 = ', VSR(2)
C
C ==== Test 7c: TRDIF with fixed state (free Transition) ====
C---- station 1 (X1=0.05, AMPL1=8.9) and station 2 (X2=0.15) as test 7b;
C---- TRCHEK sets XT and XT sensitivities (free Transition, AMCRIT=9);
C ---- TRDIF builds up the transition interval system (laminar +
C---- turbulent part). Canonical call context as in BLSYS with TTRAN.
C---- X1>0 is needed (LOG(X2/X1) in BLDIF(1) of the laminar part).
      XIFORC = 10.0
      AMCRIT = 9.0
      IDAMPV = 0
      CALL BLPRV(0.05, 0.0, 0.01, 0.05, 0.075, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      DO 13 ICOM=1, NCOM
        COM1(ICOM) = COM2(ICOM)
   13 CONTINUE
      AMPL1 = 8.9
      CALL BLPRV(0.15, 0.0, 0.01, 0.051, 0.0765, 0.0, 1.0)
      CALL BLKIN
      CALL BLVAR(1)
      CALL TRCHEK
      CALL TRDIF
      WRITE(*,9200) 'TRDIF VS111 = ', VS1(1,1)
      WRITE(*,9200) 'TRDIF VS122 = ', VS1(2,2)
      WRITE(*,9200) 'TRDIF VS133 = ', VS1(3,3)
      WRITE(*,9200) 'TRDIF VS211 = ', VS2(1,1)
      WRITE(*,9200) 'TRDIF VS222 = ', VS2(2,2)
      WRITE(*,9200) 'TRDIF VS234 = ', VS2(2,4)
      WRITE(*,9200) 'TRDIF VS333 = ', VS2(3,3)
      WRITE(*,9200) 'TRDIF VS235 = ', VS2(3,5)
      WRITE(*,9200) 'TRDIF REZ1 = ', VSREZ(1)
      WRITE(*,9200) 'TRDIF REZ2 = ', VSREZ(2)
      WRITE(*,9200) 'TRDIF REZ3 = ', VSREZ(3)
      WRITE(*,9200) 'TRDIF VSM2 = ', VSM(2)
      WRITE(*,9200) 'TRDIF VSR2 = ', VSR(2)
      WRITE(*,9200) 'TRDIF VSX3 = ', VSX(3)
C
C ==== Test 8: BLVAR/BLMID (ITYP=1 laminar, ITYP=2 turbulent) ====
      ITYP = 1
      CALL BLVAR(1)
      WRITE(*,9200) 'BLVAR1 HK1 = ', COM1(23)
      WRITE(*,9200) 'BLVAR1 HS1 = ', COM1(28)
      CALL BLMID(1)
      WRITE(*,9200) 'BLMID1 HKM = ', COM1(23)
      ITYP = 2
      CALL BLVAR(2)
      WRITE(*,9200) 'BLVAR2 HK2 = ', COM2(23)
      CALL BLMID(2)
      WRITE(*,9200) 'BLMID2 HKM = ', COM2(23)
C
C ==== Test 9: coupling routines ====
      CALL UICALC
      WRITE(*,9200) 'UICALC12 UINV1 = ', UINV(1,1)
      CALL UECALC
      WRITE(*,9200) 'UECALC12 UEDG1 = ', UEDG(1,1)
      CALL QVFUE
      WRITE(*,9200) 'QVFUE12 QV1 = ', QVIS(1)
      CALL GAMQV
      WRITE(*,9200) 'GAMQV12 GAM1 = ', GAM(1)
      CALL STMOVE
      WRITE(*,9200) 'STMOVE12 SST = ', SST
      CALL UESET
      WRITE(*,9200) 'UESET12 UEDG1 = ', UEDG(1,1)
      CALL DSSET
      WRITE(*,9200) 'DSSET12 DSTR1 = ', DSTR(1,1)
C
C ==== Test 10: viscous VISCAL (Alfa=4 degrees) ====
      ALFR = 4.0*PI/180.0
      ALFA = ALFR
      LVCONV = .FALSE.
      CALL VISCAL(10)
      WRITE(*,9200) 'VISCAL12A4 CL = ', CL
      WRITE(*,9200) 'VISCAL12A4 CM = ', CM
      WRITE(*,9200) 'VISCAL12A4 CD = ', CD
      WRITE(*,9200) 'VISCAL12A4 CDF = ', CDF
      WRITE(*,9200) 'VISCAL12A4 CDP = ', CDP
      WRITE(*,9200) 'VISCAL12A4 RMSBL = ', RMSBL
C
C ==== Test 11: NACA 2412 + VISCAL ====
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      ALFA = 0.0
      LVISC = .TRUE.
      LALFA = .TRUE.
      LVCONV = .FALSE.
      LWAKE = .FALSE.
      LIPAN = .FALSE.
      LBLINI = .FALSE.
      CALL VISCAL(10)
      WRITE(*,9200) 'VISCAL24 CL = ', CL
      WRITE(*,9200) 'VISCAL24 CM = ', CM
      WRITE(*,9200) 'VISCAL24 CD = ', CD
      WRITE(*,9200) 'VISCAL24 CDF = ', CDF
      WRITE(*,9200) 'VISCAL24 CDP = ', CDP
      WRITE(*,9200) 'VISCAL24 RMSBL = ', RMSBL
C
C ==== Test 12: controlled non-convergence (Alfa=25 degrees) ====
C ---- Analogous polar/polar-nonconverging (fortran-ref case '17r4...
C ALFA 25')
      ALFA = 25.0*PI/180.0
      LVCONV = .FALSE.
      CALL VISCAL(10)
      WRITE(*,9200) 'VISCAL24A25 CL = ', CL
      WRITE(*,9200) 'VISCAL24A25 CD = ', CD
      WRITE(*,9200) 'VISCAL24A25 RMSBL = ', RMSBL
      WRITE(*,9200) 'VISCAL24A25 ALFA = ', ALFA
C
 9100 FORMAT(1X, A, I6)
 9200 FORMAT(1X, A, ES17.8)
 9201 FORMAT(1X, A, I6)
 9210 FORMAT(1X, A, I2, 1X, I2, 1X, ES17.8)
 9300 FORMAT(1X, A, L6)
      END

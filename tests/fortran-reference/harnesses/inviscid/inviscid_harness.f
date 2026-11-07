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

C Fortran-harness for xfoil::core::inviscid - MRCL/COMSET/CPCALC/CLCALC,
C GGCALC/PSILIN/QWCALC/QDCALC/STFIND/QISET/XYWAKE, SPECAL/SPECCL/MHINGE.
C Calls the canonical routines unchanged. The routines lie in
C xfoil.f, xpanel.f, xoper.f and xgdes.f together with unlinked
C routines; the harness link uses byte-faithfully extracted
C routine blocks (see tools/build-inviscid-harnesses.sh).
C
C The harness output is deterministic:
C - GAM/GAM_A/SIG/QINV/QINV_A/QINVU/CPI/CPV are initialized with 0.0.
C - PI/HOPI/QOPI as in the Fortran INIT; GAMMA=1.4, GAMM1=0.4.
C - XCMREF/YCMREF as in the INIT (0.25/0.0); WAKLEN=1.0; LIMAGE=.FALSE.
C - Alfa in radians (SPECAL takes degrees).
C
C Important: fixed form; lines remain <= column 72.
      PROGRAM INVISCID_HARNESS
      INCLUDE 'XFOIL.INC'
      INTEGER I, NSIDE, ITCL
      REAL ALFR
      REAL M_CLS
      CHARACTER*80 CLYFILE
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
      DO 1 I=1,IQX
        GAM(I) = 0.0
        GAM_A(I) = 0.0
        SIG(I) = 0.0
        QINV(I) = 0.0
        QINV_A(I) = 0.0
        QINVU(I,1) = 0.0
        QINVU(I,2) = 0.0
        CPI(I) = 0.0
        CPV(I) = 0.0
    1 CONTINUE
      GAMU(N+1,1) = 0.0
      GAMU(N+1,2) = 0.0
C
C ==== Test 1: MRCL/COMSET for MATYP=1/RETYP=1 (MINF1=0, REINF1=3e6)
C ====
      MATYP = 1
      RETYP = 1
      MINF1 = 0.0
      REINF1 = 3.0E6
      CALL MRCL(0.0,M_CLS,R_CLS)
      WRITE(*,9200) 'MRCL1 MINF = ', MINF
      WRITE(*,9200) 'MRCL1 REINF = ', REINF
      WRITE(*,9200) 'MRCL1 M_CLS = ', M_CLS
      WRITE(*,9200) 'MRCL1 R_CLS = ', R_CLS
      CALL COMSET
      WRITE(*,9200) 'COMSET1 TKLAM = ', TKLAM
      WRITE(*,9200) 'COMSET1 CPSTAR = ', CPSTAR
      WRITE(*,9200) 'COMSET1 QSTAR = ', QSTAR
      N = 3
      Q(1,1) = 1.0
      Q(2,1) = 0.9
      Q(3,1) = 1.1
      CALL CPCALC(N,Q(1,1),QINF,MINF,CPI)
      DO 5 I=1,N
        WRITE(*,9200) 'CPCALC1 CPI = ', CPI(I)
    5 CONTINUE
C
C ==== Test 2: MRCL/COMSET for MATYP=2/RETYP=2 (CLS=0.5) ====
      MATYP = 2
      RETYP = 2
      MINF1 = 0.1
      REINF1 = 3.0E6
      CALL MRCL(0.5,M_CLS,R_CLS)
      WRITE(*,9200) 'MRCL2 MINF = ', MINF
      WRITE(*,9200) 'MRCL2 REINF = ', REINF
      WRITE(*,9200) 'MRCL2 M_CLS = ', M_CLS
      WRITE(*,9200) 'MRCL2 R_CLS = ', R_CLS
      CALL COMSET
      WRITE(*,9200) 'COMSET2 TKLAM = ', TKLAM
C
C ==== Test 3: NACA 0012 + PANGEN + GGCALC + SPECAL (ALFA=0) ====
      NSIDE = IQX/3
      CALL NACA4(12,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      MINF1 = 0.0
      REINF1 = 3.0E6
      MATYP = 1
      RETYP = 1
      CALL GGCALC
      WRITE(*,9200) 'GGCALC12 AIJ11 = ', AIJ(1,1)
      WRITE(*,9200) 'GGCALC12 AIJ12 = ', AIJ(1,2)
      WRITE(*,9200) 'GGCALC12 AIJK = ', AIJ(N+1,1)
      WRITE(*,9200) 'GGCALC12 GAMU11 = ', GAMU(1,1)
      WRITE(*,9200) 'GGCALC12 GAMUN1 = ', GAMU(N+1,1)
      WRITE(*,9200) 'GGCALC12 QINVU11 = ', QINVU(1,1)
      WRITE(*,9200) 'GGCALC12 QINVU21 = ', QINVU(2,1)
      ALFA = 0.0
      CALL SPECAL
      CALL TECALC
      WRITE(*,9200) 'SPEC12-0 GAM1 = ', GAM(1)
      WRITE(*,9200) 'SPEC12-0 GAMN = ', GAM(N)
      WRITE(*,9200) 'SPEC12-0 QINV1 = ', QINV(1)
      WRITE(*,9200) 'SPEC12-0 QINVN = ', QINV(N)
      WRITE(*,9200) 'SPEC12-0 CL = ', CL
      WRITE(*,9200) 'SPEC12-0 CM = ', CM
      WRITE(*,9200) 'SPEC12-0 CDP = ', CDP
      WRITE(*,9200) 'SPEC12-0 CPI1 = ', CPI(1)
      WRITE(*,9200) 'SPEC12-0 CPI2 = ', CPI(2)
      CALL STFIND
      WRITE(*,9200) 'SPEC12-0 SST = ', SST
      WRITE(*,9100) 'SPEC12-0 IST = ', IST
      WRITE(*,9200) 'SPEC12-0 SIGTE = ', SIGTE
      WRITE(*,9200) 'SPEC12-0 GAMTE = ', GAMTE
C
C ==== Test 4: NACA 0012 SPECAL (Alfa=4 Degrees) ====
      ALFR = 4.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      CALL TECALC
      WRITE(*,9200) 'SPEC12-4 GAM1 = ', GAM(1)
      WRITE(*,9200) 'SPEC12-4 GAMN = ', GAM(N)
      WRITE(*,9200) 'SPEC12-4 QINV1 = ', QINV(1)
      WRITE(*,9200) 'SPEC12-4 CL = ', CL
      WRITE(*,9200) 'SPEC12-4 CM = ', CM
      WRITE(*,9200) 'SPEC12-4 CDP = ', CDP
      WRITE(*,9200) 'SPEC12-4 CPI1 = ', CPI(1)
      WRITE(*,9200) 'SPEC12-4 CPI2 = ', CPI(2)
      CALL STFIND
      WRITE(*,9200) 'SPEC12-4 SST = ', SST
      WRITE(*,9100) 'SPEC12-4 IST = ', IST
      WRITE(*,9200) 'SPEC12-4 SIGTE = ', SIGTE
      WRITE(*,9200) 'SPEC12-4 GAMTE = ', GAMTE
C
C ==== Test 5: NACA 2412 + PANGEN + SPECAL (ALFA=0,4,8,-2) ====
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      ALFA = 0.0
      CALL SPECAL
      WRITE(*,9200) 'SPEC24-0 CL = ', CL
      WRITE(*,9200) 'SPEC24-0 CM = ', CM
      WRITE(*,9200) 'SPEC24-0 CDP = ', CDP
      ALFR = 4.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      WRITE(*,9200) 'SPEC24-4 CL = ', CL
      WRITE(*,9200) 'SPEC24-4 CM = ', CM
      WRITE(*,9200) 'SPEC24-4 CDP = ', CDP
      WRITE(*,9200) 'SPEC24-4 CPI1 = ', CPI(1)
      WRITE(*,9200) 'SPEC24-4 CPI2 = ', CPI(2)
      ALFR = 8.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      WRITE(*,9200) 'SPEC24-8 CL = ', CL
      WRITE(*,9200) 'SPEC24-8 CM = ', CM
      ALFR = -2.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      WRITE(*,9200) 'SPEC24-2 CL = ', CL
      WRITE(*,9200) 'SPEC24-2 CM = ', CM
      WRITE(*,9200) 'SPEC24-2 CPI1 = ', CPI(1)
C
C ==== Test 6: Clark Y + PANGEN + SPECAL (ALFA=0,4) ====
      CLYFILE = 'tests/fortran-reference/cases/'//
     &          'geometry/clark-y-load/input/CLARK_Y.dat'
      OPEN(20, FILE=CLYFILE, STATUS='OLD', ACTION='READ')
      READ(20, 9300) NAME
      NB = 0
      DO
        READ(20, *, END=98) XB(NB+1), YB(NB+1)
        NB = NB + 1
      ENDDO
   98 CLOSE(20)
      CALL PANGEN(.FALSE.)
      ALFA = 0.0
      CALL SPECAL
      WRITE(*,9200) 'SPECCLY-0 CL = ', CL
      WRITE(*,9200) 'SPECCLY-0 CM = ', CM
      WRITE(*,9200) 'SPECCLY-0 CDP = ', CDP
      ALFR = 4.0*PI/180.0
      ALFA = ALFR
      CALL SPECAL
      WRITE(*,9200) 'SPECCLY-4 CL = ', CL
      WRITE(*,9200) 'SPECCLY-4 CM = ', CM
      WRITE(*,9200) 'SPECCLY-4 CDP = ', CDP
C
C ==== Test 7: SPECCL (Cl=0.5) on NACA 2412 ====
      CALL NACA4(2412,W1,W2,W3,NSIDE,XB,YB,NB,NAME)
      CALL PANGEN(.TRUE.)
      ALFA = 0.0
      CLSPEC = 0.5
      CALL SPECCL
      CALL TECALC
      CALL QISET
      WRITE(*,9200) 'SPECC24 CL = ', CL
      WRITE(*,9200) 'SPECC24 CM = ', CM
      WRITE(*,9200) 'SPECC24 CDP = ', CDP
      WRITE(*,9100) 'SPECC24 N = ', N
      ALFR = 180.0*ALFA/PI
      WRITE(*,9200) 'SPECC24 ALFAD = ', ALFR
      WRITE(*,9200) 'SPECC24 QINV1 = ', QINV(1)
      WRITE(*,9200) 'SPECC24 CPI1 = ', CPI(1)
C
C ==== Test 8: XYWAKE + QWCALC + QISET + QDCALC (wake influence) ====
      CALL XYWAKE
      WRITE(*,9100) 'WAKE NW = ', NW
      WRITE(*,9200) 'WAKE X1 = ', X(N+1)
      WRITE(*,9200) 'WAKE Y1 = ', Y(N+1)
      WRITE(*,9200) 'WAKE NX1 = ', NX(N+1)
      WRITE(*,9200) 'WAKE NY1 = ', NY(N+1)
      WRITE(*,9200) 'WAKE APANEL1 = ', APANEL(N+1)
      WRITE(*,9200) 'WAKE X2 = ', X(N+2)
      WRITE(*,9200) 'WAKE Y2 = ', Y(N+2)
      CALL QWCALC
      WRITE(*,9200) 'QWCALC QINVU11 = ', QINVU(1,1)
      WRITE(*,9200) 'QWCALC QINVUN11 = ', QINVU(N+1,1)
      WRITE(*,9200) 'QWCALC QINVUN21 = ', QINVU(N+2,1)
      CALL QISET
      WRITE(*,9200) 'QISETW QINV1 = ', QINV(1)
      WRITE(*,9200) 'QISETW QINVN1 = ', QINV(N+1)
      WRITE(*,9200) 'QISETW QINV_A1 = ', QINV_A(1)
      CALL QDCALC
      WRITE(*,9200) 'QDCALC DIJ11 = ', DIJ(1,1)
      WRITE(*,9200) 'QDCALC DIJ1N1 = ', DIJ(1,N+1)
      WRITE(*,9200) 'QDCALC DIJNN1 = ', DIJ(N,N+1)
C
C ==== Test 9: MHINGE (XOF/YOF = 0.5/0.0) ====
      LFLAP = .TRUE.
      XOF = 0.5
      YOF = 0.0
      CALL MHINGE
      WRITE(*,9200) 'MHINGE HMOM = ', HMOM
      WRITE(*,9200) 'MHINGE HFX = ', HFX
      WRITE(*,9200) 'MHINGE HFY = ', HFY
C
C ==== Test 10: MRCL with illegal RETYP (fixed to 1) ====
      RETYP = 5
      MATYP = 1
      MINF1 = 0.0
      REINF1 = 3.0E6
      CALL MRCL(0.0,M_CLS,R_CLS)
      WRITE(*,9100) 'MRCLBAD RETYP = ', RETYP
      WRITE(*,9200) 'MRCLBAD MINF = ', MINF
      WRITE(*,9200) 'MRCLBAD REINF = ', REINF
C
      STOP
 9100 FORMAT(A, I5)
 9200 FORMAT(A, E30.20)
 9300 FORMAT(A)
      END

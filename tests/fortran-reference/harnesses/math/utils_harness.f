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

C Fortran-harness for xfoil::core::math - Helper functions (SETEXP,
C ATANC).
C Calls the canonical routines from fortran-src/xutils.f unchanged.
C Prints inputs, outputs and relevant intermediate values with high
C high visible precision (Fortran REAL = single precision).
      PROGRAM UTILS_HARNESS
      REAL S(50)
      REAL DS1, SMAX, RATIOI, RATIOF
      INTEGER NN, I
C
C ==== SETEXP ====
C
C --- case 1: increasing geometric sequence
      NN = 11
      DS1 = 0.1
      SMAX = 1.0
      CALL SETEXP(S, DS1, SMAX, NN)
      WRITE(*,9000) 'SETEXP 1 NN=', NN, ' DS1=', DS1, ' SMAX=', SMAX
      DO I=1, NN
        WRITE(*,9100) '  S(', I, ') = ', S(I)
      ENDDO
C
C --- case 2: shrinking sequence (ratio < 1)
      NN = 6
      DS1 = 0.1
      SMAX = 0.3
      CALL SETEXP(S, DS1, SMAX, NN)
      WRITE(*,9000) 'SETEXP 2 NN=', NN, ' DS1=', DS1, ' SMAX=', SMAX
      DO I=1, NN
        WRITE(*,9100) '  S(', I, ') = ', S(I)
      ENDDO
C
C --- case 3: NEX=2 special branch (NN=3)
      NN = 3
      DS1 = 0.2
      SMAX = 1.0
      CALL SETEXP(S, DS1, SMAX, NN)
      WRITE(*,9000) 'SETEXP 3 NN=', NN, ' DS1=', DS1, ' SMAX=', SMAX
      DO I=1, NN
        WRITE(*,9100) '  S(', I, ') = ', S(I)
      ENDDO
C
C --- case 4: nearly uniform (Ratio near 1)
      NN = 5
      DS1 = 0.25
      SMAX = 1.0
      CALL SETEXP(S, DS1, SMAX, NN)
      WRITE(*,9000) 'SETEXP 4 NN=', NN, ' DS1=', DS1, ' SMAX=', SMAX
      DO I=1, NN
        WRITE(*,9100) '  S(', I, ') = ', S(I)
      ENDDO
C
C ==== ATANC ====
C
      CALL RUN_ATANC( -1.0, -1.0, 0.75*3.14159265358979 )
      CALL RUN_ATANC(  1.0,  1.0, 0.0 )
      CALL RUN_ATANC(  1.0, -1.0, 0.0 )
      CALL RUN_ATANC( -1.0,  1.0, 0.0 )
      CALL RUN_ATANC( -1.0, -1.0, 0.0 )
      CALL RUN_ATANC(  0.0, -1.0, 0.5*3.14159265358979 )
      CALL RUN_ATANC(  1.0, -1.0, 2.5*3.14159265358979 )
      CALL RUN_ATANC( -1.0, -1.0, 3.0*3.14159265358979 )
      CALL RUN_ATANC(  0.0,  1.0, 3.5*3.14159265358979 )
C
      STOP
C
 9000 FORMAT(A, I3, A, F12.6, A, F12.6)
 9100 FORMAT(A, I3, A, E30.20)
      END
C
      SUBROUTINE RUN_ATANC(Y, X, THOLD)
      REAL Y, X, THOLD
      REAL THNEW, DTHET, DTCORR, RES
      REAL PI, TPI
      PI  = 3.1415926535897932384
      TPI = 6.2831853071795864769
C --- locally reproduced to show intermediate values (not the
C Routine)
      THNEW = ATAN2(Y, X)
      DTHET = THNEW - THOLD
      DTCORR = DTHET - TPI*INT((DTHET + SIGN(PI,DTHET))/TPI)
      RES = ATANC(Y, X, THOLD)
      WRITE(*,9200) 'ATANC Y,X,THOLD = ', Y, X, THOLD
      WRITE(*,9300) '  THNEW  = ', THNEW
      WRITE(*,9300) '  DTHET  = ', DTHET
      WRITE(*,9300) '  DTCORR = ', DTCORR
      WRITE(*,9300) '  RES    = ', RES
      RETURN
 9200 FORMAT(A, 3(F14.8))
 9300 FORMAT(A, E30.20)
      END

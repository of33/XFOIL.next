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

C Fortran-harness for xfoil::core::math - Spline/Interpolation
C (spline.f).
C Calls the canonical routines from fortran-src/spline.f unchanged.
C Prints inputs, outputs and relevant intermediate values with high
C high visible precision (Fortran REAL = single precision).
C Important: fixed form; lines remain <= column 72.
      PROGRAM SPLINE_HARNESS
      INTEGER N, I, KK
      REAL X(10), XS(10), S(10), Y(10), YS(10)
      REAL SS, SI, XI, XS1, XS2
      REAL A(10), B(10), C(10), D(10)
C
C ==== SPLINE + SEVAL/DEVAL/D2VAL (quadratic X=S*S) ====
      N = 5
      S(1) = 0.0
      S(2) = 0.5
      S(3) = 1.0
      S(4) = 1.5
      S(5) = 2.0
      DO I=1,N
        X(I) = S(I)*S(I)
      ENDDO
      CALL SPLINE(X, XS, S, N)
      WRITE(*,*) 'SPLINE (X=S^2) XS:'
      DO I=1,N
        WRITE(*,9210) '  XS(', I, ') = ', XS(I)
      ENDDO
C
      SS = 0.0
      WRITE(*,9200) 'SEVAL node 0.0 = ', SEVAL(SS,X,XS,S,N)
      SS = 1.0
      WRITE(*,9200) 'SEVAL node 1.0 = ', SEVAL(SS,X,XS,S,N)
      SS = 0.75
      WRITE(*,9200) 'SEVAL mid  0.75 = ', SEVAL(SS,X,XS,S,N)
      SS = 2.5
      WRITE(*,9200) 'SEVAL extrap 2.5 = ', SEVAL(SS,X,XS,S,N)
      SS = -0.5
      WRITE(*,9200) 'SEVAL extrap -0.5 = ', SEVAL(SS,X,XS,S,N)
      SS = 0.75
      WRITE(*,9200) 'DEVAL 0.75 = ', DEVAL(SS,X,XS,S,N)
      WRITE(*,9200) 'D2VAL 0.75 = ', D2VAL(SS,X,XS,S,N)
      SS = 1.0
      WRITE(*,9200) 'DEVAL node 1.0 = ', DEVAL(SS,X,XS,S,N)
      WRITE(*,9200) 'D2VAL node 1.0 = ', D2VAL(SS,X,XS,S,N)
C
C ==== SPLIND (specified end derivatives) ====
      N = 4
      S(1) = 0.0
      S(2) = 1.0
      S(3) = 2.0
      S(4) = 3.0
      X(1) = 1.0
      X(2) = 2.0
      X(3) = 4.0
      X(4) = 8.0
      CALL SPLIND(X, XS, S, N, 2.0, 12.0)
      WRITE(*,*) 'SPLIND (XS1=2.0,XS2=12.0) XS:'
      DO I=1,N
        WRITE(*,9210) '  XS(', I, ') = ', XS(I)
      ENDDO
C
      CALL SPLIND(X, XS, S, N, -999.0, -999.0)
      WRITE(*,*) 'SPLIND (zero-3rd-deriv) XS:'
      DO I=1,N
        WRITE(*,9210) '  XS(', I, ') = ', XS(I)
      ENDDO
C
C ==== SPLINA (Averaging) ====
      N = 5
      S(1) = 0.0
      S(2) = 0.4
      S(3) = 0.9
      S(4) = 1.3
      S(5) = 2.0
      X(1) = 0.0
      X(2) = 0.5
      X(3) = 1.1
      X(4) = 1.8
      X(5) = 3.0
      CALL SPLINA(X, XS, S, N)
      WRITE(*,*) 'SPLINA XS:'
      DO I=1,N
        WRITE(*,9210) '  XS(', I, ') = ', XS(I)
      ENDDO
C
C ==== TRISOL (small system) ====
      KK = 4
      A(1) = 2.0
      A(2) = 3.0
      A(3) = 4.0
      A(4) = 5.0
      B(1) = 0.0
      B(2) = 1.0
      B(3) = 1.0
      B(4) = 1.0
      C(1) = 1.0
      C(2) = 1.0
      C(3) = 1.0
      C(4) = 0.0
      D(1) = 1.0
      D(2) = 2.0
      D(3) = 3.0
      D(4) = 4.0
      CALL TRISOL(A, B, C, D, KK)
      WRITE(*,*) 'TRISOL solution D:'
      DO I=1,KK
        WRITE(*,9210) '  D(', I, ') = ', D(I)
      ENDDO
C
C ==== SCALC ====
      N = 5
      X(1) = 0.0
      X(2) = 1.0
      X(3) = 1.0
      X(4) = 1.0
      X(5) = 0.0
      Y(1) = 0.0
      Y(2) = 0.0
      Y(3) = 1.0
      Y(4) = 2.0
      Y(5) = 2.0
      CALL SCALC(X, Y, S, N)
      WRITE(*,*) 'SCALC arc length S:'
      DO I=1,N
        WRITE(*,9210) '  S(', I, ') = ', S(I)
      ENDDO
C
C ==== SPLNXY (Circular arcs) ====
      N = 5
      X(1) = 1.0
      X(2) = 0.0
      X(3) = -1.0
      X(4) = 0.0
      X(5) = 1.0
      Y(1) = 0.0
      Y(2) = 1.0
      Y(3) = 0.0
      Y(4) = -1.0
      Y(5) = 0.0
      CALL SPLNXY(X, XS, Y, YS, S, N)
      WRITE(*,*) 'SPLNXY XS,YS,S:'
      DO I=1,N
        WRITE(*,9310) '  ', I, XS(I), YS(I), S(I)
      ENDDO
C
C ==== CURV/CURVS (circle, radius 1 -> curvature 1) ====
      N = 5
      X(1) = 1.0
      X(2) = 0.0
      X(3) = -1.0
      X(4) = 0.0
      X(5) = 1.0
      Y(1) = 0.0
      Y(2) = 1.0
      Y(3) = 0.0
      Y(4) = -1.0
      Y(5) = 0.0
      CALL SCALC(X, Y, S, N)
      CALL SPLINE(X, XS, S, N)
      CALL SPLINE(Y, YS, S, N)
      SS = 0.0
      WRITE(*,9200) 'CURV at 0 = ', CURV(SS,X,XS,Y,YS,S,N)
      WRITE(*,9200) 'CURVS at 0 = ', CURVS(SS,X,XS,Y,YS,S,N)
      SS = 1.0
      WRITE(*,9200) 'CURV at 1 = ', CURV(SS,X,XS,Y,YS,S,N)
C
C ==== SINVRT ====
      N = 5
      S(1) = 0.0
      S(2) = 0.5
      S(3) = 1.0
      S(4) = 1.5
      S(5) = 2.0
      DO I=1,N
        X(I) = S(I)*S(I)
      ENDDO
      CALL SPLINE(X, XS, S, N)
      XI = 1.0
      SI = 1.5
      CALL SINVRT(SI, XI, X, XS, S, N)
      WRITE(*,9200) 'SINVRT SI for XI=1.0 = ', SI
C
C ==== SEGSPL / SEGSPLD (Segment for S=1.0 duplicated) ====
      N = 6
      S(1) = 0.0
      S(2) = 0.5
      S(3) = 1.0
      S(4) = 1.0
      S(5) = 1.5
      S(6) = 2.0
      X(1) = 0.0
      X(2) = 0.25
      X(3) = 1.0
      X(4) = 1.0
      X(5) = 2.25
      X(6) = 4.0
      CALL SEGSPL(X, XS, S, N)
      WRITE(*,*) 'SEGSPL XS:'
      DO I=1,N
        WRITE(*,9210) '  XS(', I, ') = ', XS(I)
      ENDDO
      CALL SEGSPLD(X, XS, S, N, -999.0, -999.0)
      WRITE(*,*) 'SEGSPLD XS:'
      DO I=1,N
        WRITE(*,9210) '  XS(', I, ') = ', XS(I)
      ENDDO
C
      STOP
C
 9200 FORMAT(A, E30.20)
 9210 FORMAT(A, I3, A, E30.20)
 9310 FORMAT(A, I3, 3(E30.20))
      END

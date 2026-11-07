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
C circle_kernels_harness.f
C Focused Fortran harness for the locally bounded circle/
C coefficient
C kernels of the inverse core. Calls only the canonical
C routines EIWSET, PIQSUM, FTP, CNFILT, CNSYMM from fortran-src/xmdes.f
C     (extracted byte-faithfully). Changes no Fortran source, no
C     raw reference and no golden file.
C
C     The harness initializes the circle grid (EIWSET), sets CN/PIQ to
C deterministic values and calls the inverse Fourier synthesis
C (PIQSUM),
C the Fourier transform (FTP), the Hanning filter (CNFILT) and the
C     symmetrization (CNSYMM). Output: circle grid,
C     scale number EIW, coefficients CN and PIQ intermediate values.
      PROGRAM CIRCLEKERNELS
      INCLUDE 'CIRCLE.INC'
      INTEGER NC1, IC, M
      NC1 = 257
      CALL EIWSET(NC1)
      WRITE(*,1000) NC, MC, MCT
1000  FORMAT('GRID',3I6)
      WRITE(*,1010) DWC
1010  FORMAT('DWC',ES22.12)
      DO IC = 1, 6
        WRITE(*,1020) IC, WC(IC),
     &      REAL(EIW(IC,0)), AIMAG(EIW(IC,0)),
     &      REAL(EIW(IC,1)), AIMAG(EIW(IC,1)),
     &      REAL(EIW(IC,MC)), AIMAG(EIW(IC,MC))
      ENDDO
1020  FORMAT('EIW',I4,7ES22.12)
      DO M = 0, MC
        CN(M) = CMPLX( 1.0/FLOAT(M+1) , 0.5/FLOAT(M+2) )
      ENDDO
      CALL PIQSUM
      DO IC = 1, 6
        WRITE(*,1030) IC, REAL(PIQ(IC)), AIMAG(PIQ(IC))
      ENDDO
1030  FORMAT('PIQ',I4,2ES22.12)
      DO IC = 1, NC
        PIQ(IC) = CMPLX( COS(WC(IC)) , SIN(2.0*WC(IC)) )
      ENDDO
      CALL FTP
      DO M = 0, 6
        WRITE(*,1040) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1040  FORMAT('CNFTP',I4,2ES22.12)
      CALL CNFILT(2.0)
      DO M = 0, 6
        WRITE(*,1050) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1050  FORMAT('CNFILT',I4,2ES22.12)
      DO M = 0, MC
        CN(M) = CMPLX( REAL(CN(M)) , 3.0+FLOAT(M) )
      ENDDO
      CALL CNSYMM
      DO M = 0, 6
        WRITE(*,1060) M, REAL(CN(M)), AIMAG(CN(M))
      ENDDO
1060  FORMAT('CNSYMM',I4,2ES22.12)
      END

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

C Numerically irrelevant plot stubs for harnesses whose canonical
C routines contain plot calls (PLTAIR/PLNEWP). The
C calls have no influence on the numerical state; a
C plot backend deliberately does not exist in this core layer.
      SUBROUTINE PLTAIR(XX,XXP,YY,YYP,SS,NN, XOFF,XSF,YOFF,YSF,COLOR)
      RETURN
      END
      SUBROUTINE PLNEWP(COLOR)
      RETURN
      END
      SUBROUTINE PLOT(X,Y,IER)
      RETURN
      END
      SUBROUTINE PLSYMB(X,Y,CH,ICOL,ANG,NC)
      RETURN
      END
      SUBROUTINE NEWCOLOR(ICOL)
      RETURN
      END
      SUBROUTINE NEWCOLORNAME(NAME)
      RETURN
      END
      SUBROUTINE GETCOLOR(ICOL)
      ICOL = 0
      RETURN
      END
      SUBROUTINE PLTINI
      RETURN
      END
      SUBROUTINE NEWPEN(IPN)
      RETURN
      END
      SUBROUTINE GETPAT(IPAT)
      IPAT = 0
      RETURN
      END
      SUBROUTINE NEWPAT(IPAT)
      RETURN
      END

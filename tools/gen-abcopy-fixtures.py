#!/usr/bin/env python3
# XFOIL.next
#
# Copyright (C) 2026 Olaf Frommann
# https://github.com/of33/XFOIL.next
#
# XFOIL.next is based on XFOIL 6.99 by Mark Drela and
# Harold Youngren.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Produces the ABCOPY boundary fixtures abcopy365.dat / abcopy366.dat.
# Method: NACA 2412 analytic 4-digit airfoil (t=0.12, m=0.04, p=0.2),
# cosine-distributed, exactly N points, closed contour counter-clockwise.
# Geometrically valid: no duplicate points, no degenerate segments,
# positive (counter-clockwise) area. Deterministic (pure formula).
import math

def naca2412(x):
    t, m, p = 0.12, 0.04, 0.2
    if x < p:
        yc = m/p**2 * (2*p*x - x*x); dyc = 2*m/p**2 * (p - x)
    else:
        yc = m/(1-p)**2 * ((1-2*p) + 2*p*x - x*x); dyc = 2*m/(1-p)**2 * (p - x)
    yt = 5*t*(0.2969*math.sqrt(x) - 0.1260*x - 0.3516*x*x + 0.2843*x**3 - 0.1015*x**4)
    th = math.atan(dyc)
    return yc + yt*math.cos(th), yc - yt*math.cos(th)

def make_contour(N):
    n_top = (N + 1) // 2
    n_bot = N + 1 - n_top
    def xs(n, lo, hi):
        if n == 1: return [0.5*(lo+hi)]
        return [lo + (hi-lo)*0.5*(1 - math.cos(math.pi*i/(n-1))) for i in range(n)]
    xt = xs(n_top, 0.0, 1.0)[::-1]
    xb = xs(n_bot, 0.0, 1.0)[1:]
    X = xt + xb
    Y = [naca2412(x)[0] for x in xt] + [naca2412(x)[1] for x in xb]
    return X, Y

for N in (365, 366):
    X, Y = make_contour(N)
    with open(f"abcopy{N}.dat", "w") as f:
        for x, y in zip(X, Y):
            f.write(f"{x:.7f}  {y:.7f}\n")
    print(f"abcopy{N}.dat: {len(X)} Points")

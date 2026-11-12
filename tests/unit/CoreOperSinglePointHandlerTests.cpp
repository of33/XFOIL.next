// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

// File: tests/unit/CoreOperSinglePointHandlerTests.cpp
// Permanent tests of the OPER single-point handlers
// Alfa/CLI/Cl and the numerical single-point diagnostics (SPECAL-OFS
// snapshot FORMAT 333, VISCAL iteration data sets
// FORMAT 2000/2010/2020, completion record format 9922). The
// target texts are byte-exact transcribed stdout slices
// of the canonical Fortran processes (LC_ALL=C fortran-bin/xfoil,
// stdin oracles a_alfa2_inviscid, b_alfa2_twice, c_cli08_visc, d_cl08_visc, e_alfa2_iter1,
// f_invalid_alfa). The comparison covers the representable numerical
// surface; the remaining event-text gaps (SETBL/MRCHUE/MRCHDU and
// transition lines) are excluded from the expected
// strings and marked as gaps. No golden change,
// no solver logic in the handler.

#include "TestSupport.h"

#include "app/cli/CliFortranText.h"
#include "app/cli/CoreOperHandlers.h"
#include "app/cli/CoreVparHandlers.h"

#include "xfoil/AnalysisConfig.h"
#include "xfoil/AnalysisResult.h"
#include "xfoil/CancellationToken.h"
#include "xfoil/core/geometry/Naca.h"
#include "xfoil/session/SessionOps.h"
#include "xfoil/session/SessionState.h"

#include <map>
#include <iostream>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string legacyS18(std::string text) {
  return testing::normalizeS18LegacyText(std::move(text));
}

#define CHECK_LEGACY_EQ(actual, expected) \
  CHECK_EQ(legacyS18(actual), legacyS18(expected))


using namespace xfoil;
using xfoil::app::cli::handleOperAlphaCore;
using xfoil::app::cli::handleOperCliCore;
using xfoil::app::cli::handleOperClCore;
using xfoil::app::cli::handleOperBangCore;
using xfoil::app::cli::handleOperAseqCore;
using xfoil::app::cli::handleOperCseqCore;
using xfoil::app::cli::handleOperInitCore;
using xfoil::app::cli::handleVparInitCore;

// canonical Fortran WRITE(*,*) lines (xpanel.f 1000/1156/1277,
// xoper.f 2968-2969/3023, 446/503/538) and ASKR prompts (userio.f 1000).
constexpr const char* kGgcalcLine =
    " Calculating unit vorticity distributions ...\n";
constexpr const char* kXywakeLine = " Calculating wake trajectory ...\n";
constexpr const char* kQdcalcLine =
    " Calculating source influence matrix ...\n";
constexpr const char* kSolvingBlSystem = "\n Solving BL system ...\n";
constexpr const char* kViscalFailed = " VISCAL:  Convergence failed\n";
constexpr const char* kContinueHint = " Type \"!\" to continue iterating\n";
constexpr const char* kAlphaPrompt =
    "\nEnter angle of attack (deg)   r>  ";
constexpr const char* kCliPrompt =
    "\nEnter inviscid lift coefficient   r>  ";
constexpr const char* kClPrompt = "\nEnter lift coefficient   r>  ";

// format-333 OFS lines (xoper.f 2816-2818) from the oracle. The leading
// '/' produces the blank line, which appears here as a leading '\n' at
// the start of the block or after the GGCALC line.
const std::string kOfsAlfa2 =
    "\nOFS: alfa =   2.0000 cl =   0.4968 cdp =  -0.0011 cm =  -0.0587\n";
const std::string kOfsAlfa0 =
    "\nOFS: alfa =   0.0000 cl =   0.2554 cdp =  -0.0011 cm =  -0.0557\n";

// iteration blocks of oracle b_alfa2_twice (alpha 2, NACA 2412, Re=1e6,
// It24.16, ITER 100), point 1 (9 iterations): per iteration first the
// canonical BL event lines (Initializing BL, MRCHUE sides/inverse/
// error, blank line xbl.f:90, MRCHDU error, transitions format
// 9100/9200), then the format-2000/2010/2020 lines.
const std::string kAlfa2Iter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  48     Hk =   4"
    ".617\n MRCHUE: Inverse mode at  49     Hk =   5.710\n MRCHUE: Inverse mode at  50     Hk =  "
    " 6.785\n MRCHUE: Inverse mode at  51     Hk =   4.627\n MRCHUE: Inverse mode at  52     Hk ="
    "   2.500\n MRCHUE: Inverse mode at  85     Hk =   2.500\n    side            2  ...\n MRCHUE:"
    " Inverse mode at  68     Hk =   4.151\n MRCHUE: Inverse mode at  69     Hk =   4.842\n MRCHU"
    "E: Inverse mode at  70     Hk =   5.522\n MRCHUE: Inverse mode at  71     Hk =   2.500\n\n Si"
    "de 1  free  transition at x/c =  0.4575   51\n Side 2  free  transition at x/c =  0.9035   "
    "71\n\n   1   rms: 0.1272E+00   max: -.9391E+00   C at   71  2   RLX: 0.532\n       a =  2.000"
    "      CL =  0.4917\n      Cm = -0.0583     CD =  0.00557   =>   CDf =  0.00473    CDp =  0."
    "00085\n"
;
const std::string kAlfa2Iter2 =
    ""
    "\n Side 1  free  transition at x/c =  0.4716   52\n Side 2  free  transition at x/c =  0.933"
    "0   72\n\n   2   rms: 0.9231E-01   max: -.1038E+01   C at   72  2   RLX: 0.482\n       a =  2"
    ".000      CL =  0.4841\n      Cm = -0.0559     CD =  0.00565   =>   CDf =  0.00448    CDp ="
    "  0.00117\n"
;
const std::string kAlfa2Iter3 =
    ""
    "\n Side 1  free  transition at x/c =  0.4815   53\n Side 2  free  transition at x/c =  0.937"
    "8   73\n\n   3   rms: 0.8406E-01   max: -.7410E+00   C at   53  1   RLX: 0.675\n       a =  2"
    ".000      CL =  0.4651\n      Cm = -0.0517     CD =  0.00560   =>   CDf =  0.00450    CDp ="
    "  0.00110\n"
;
const std::string kAlfa2Iter4 =
    ""
    "\n Side 1  free  transition at x/c =  0.5176   55\n Side 2  free  transition at x/c =  0.950"
    "5   73\n\n   4   rms: 0.3173E-01   max: -.2732E+00   C at   55  1\n       a =  2.000      CL "
    "=  0.4431\n      Cm = -0.0467     CD =  0.00572   =>   CDf =  0.00423    CDp =  0.00150\n"
;
const std::string kAlfa2Iter5 =
    ""
    "\n MRCHDU: Convergence failed at  76  side 2    Res =  0.4630E+01\n MRCHDU: Convergence fail"
    "ed at  77  side 2    Res =  0.1818E+01\n Side 1  free  transition at x/c =  0.5267   55\n Si"
    "de 2  free  transition at x/c =  0.9992   77\n\n   5   rms: 0.3093E-01   max: 0.3265E+00   D"
    " at   73  2\n       a =  2.000      CL =  0.4467\n      Cm = -0.0473     CD =  0.00570   => "
    "  CDf =  0.00412    CDp =  0.00159\n"
;
const std::string kAlfa2Iter6 =
    ""
    "\n Side 1  free  transition at x/c =  0.5259   55\n Side 2  free  transition at x/c =  0.964"
    "9   74\n\n   6   rms: 0.2615E-01   max: -.4460E+00   D at   76  2\n       a =  2.000      CL "
    "=  0.4482\n      Cm = -0.0478     CD =  0.00576   =>   CDf =  0.00410    CDp =  0.00166\n"
;
const std::string kAlfa2Iter7 =
    ""
    "\n Side 1  free  transition at x/c =  0.5258   55\n Side 2  free  transition at x/c =  0.972"
    "7   75\n\n   7   rms: 0.1481E-01   max: 0.2520E+00   C at   75  2\n       a =  2.000      CL "
    "=  0.4489\n      Cm = -0.0479     CD =  0.00577   =>   CDf =  0.00411    CDp =  0.00166\n"
;
const std::string kAlfa2Iter8 =
    ""
    "\n Side 1  free  transition at x/c =  0.5257   55\n Side 2  free  transition at x/c =  0.967"
    "6   75\n\n   8   rms: 0.7847E-03   max: -.1330E-01   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00411    CDp =  0.00166\n"
;
const std::string kAlfa2Iter9 =
    ""
    "\n Side 1  free  transition at x/c =  0.5256   55\n Side 2  free  transition at x/c =  0.967"
    "5   75\n\n   9   rms: 0.3354E-05   max: 0.3182E-04   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00412    CDp =  0.00166\n"
;
const std::string kAlfa2WarmIter1 =
    ""
    "\n Side 1  free  transition at x/c =  0.5256   55\n Side 2  free  transition at x/c =  0.967"
    "5   75\n\n   1   rms: 0.4564E-08   max: 0.5431E-07   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00412    CDp =  0.00166\n"
;
// format 9922 (xoper.f 3060) of the point (side 1 = top).
const std::string kAlfa2Completion =
    "      9.000    3.5895   0.005778   0.000000   0.000000   0.000000"
    "    0.5256     #\n";
const std::string kIter1Only =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  48     Hk =   4"
    ".617\n MRCHUE: Inverse mode at  49     Hk =   5.710\n MRCHUE: Inverse mode at  50     Hk =  "
    " 6.785\n MRCHUE: Inverse mode at  51     Hk =   4.627\n MRCHUE: Inverse mode at  52     Hk ="
    "   2.500\n MRCHUE: Inverse mode at  85     Hk =   2.500\n    side            2  ...\n MRCHUE:"
    " Inverse mode at  68     Hk =   4.151\n MRCHUE: Inverse mode at  69     Hk =   4.842\n MRCHU"
    "E: Inverse mode at  70     Hk =   5.522\n MRCHUE: Inverse mode at  71     Hk =   2.500\n\n Si"
    "de 1  free  transition at x/c =  0.4575   51\n Side 2  free  transition at x/c =  0.9035   "
    "71\n\n   1   rms: 0.1272E+00   max: -.9391E+00   C at   71  2   RLX: 0.532\n       a =  2.000"
    "      CL =  0.4917\n      Cm = -0.0583     CD =  0.00557   =>   CDf =  0.00473    CDp =  0."
    "00085\n"
;
// oracle e_alfa2_iter1 (ITER 1, alpha 2): completion before the
// continuation hint.
const std::string kIter1Completion =
    "      9.000    6.6585   0.005574   0.001412   0.001559   0.000148"
    "    0.4575     #\n";
const std::string kNonConvIter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  34     Hk =   5"
    ".143\n MRCHUE: Inverse mode at  35     Hk =   6.730\n MRCHUE: Inverse mode at  36     Hk =  "
    " 8.323\n MRCHUE: Inverse mode at  37     Hk =   8.854\n MRCHUE: Inverse mode at  38     Hk ="
    "   2.500\n MRCHUE: Inverse mode at  57     Hk =   2.500\n MRCHUE: Inverse mode at  58     Hk"
    " =   2.500\n MRCHUE: Inverse mode at  59     Hk =   2.500\n MRCHUE: Inverse mode at  60     "
    "Hk =   2.500\n MRCHUE: Inverse mode at  61     Hk =   2.500\n MRCHUE: Inverse mode at  62   "
    "  Hk =   2.500\n MRCHUE: Inverse mode at  63     Hk =   2.500\n MRCHUE: Inverse mode at  64 "
    "    Hk =   2.500\n MRCHUE: Inverse mode at  65     Hk =   2.500\n MRCHUE: Inverse mode at  6"
    "6     Hk =   2.500\n MRCHUE: Inverse mode at  67     Hk =   2.500\n MRCHUE: Inverse mode at "
    " 68     Hk =   2.500\n MRCHUE: Inverse mode at  69     Hk =   2.500\n MRCHUE: Inverse mode a"
    "t  70     Hk =   2.500\n MRCHUE: Inverse mode at  71     Hk =   2.500\n MRCHUE: Inverse mode"
    " at  72     Hk =   2.500\n MRCHUE: Inverse mode at  73     Hk =   2.500\n MRCHUE: Inverse mo"
    "de at  74     Hk =   2.500\n MRCHUE: Inverse mode at  75     Hk =   2.500\n MRCHUE: Inverse "
    "mode at  76     Hk =   2.500\n MRCHUE: Inverse mode at  77     Hk =   2.500\n MRCHUE: Invers"
    "e mode at  78     Hk =   2.500\n MRCHUE: Inverse mode at  79     Hk =   2.500\n MRCHUE: Inve"
    "rse mode at  80     Hk =   2.500\n MRCHUE: Inverse mode at  81     Hk =   2.500\n MRCHUE: In"
    "verse mode at  82     Hk =   2.500\n MRCHUE: Inverse mode at  83     Hk =   2.500\n MRCHUE: "
    "Inverse mode at  84     Hk =   2.500\n MRCHUE: Inverse mode at  85     Hk =   2.500\n MRCHUE"
    ": Inverse mode at  86     Hk =   2.500\n MRCHUE: Inverse mode at  87     Hk =   2.500\n MRCH"
    "UE: Inverse mode at  88     Hk =   2.500\n MRCHUE: Convergence failed at  88  side 1    Res"
    " =  0.2759E-01\n MRCHUE: Convergence failed at  89  side 1    Res =  0.4939E+01\n MRCHUE: In"
    "verse mode at  90     Hk =   2.500\n MRCHUE: Inverse mode at  91     Hk =   2.500\n MRCHUE: "
    "Inverse mode at  92     Hk =   2.500\n MRCHUE: Inverse mode at  93     Hk =   2.500\n MRCHUE"
    ": Inverse mode at  94     Hk =   2.500\n MRCHUE: Inverse mode at  95     Hk =   2.500\n MRCH"
    "UE: Inverse mode at  96     Hk =   2.500\n MRCHUE: Inverse mode at  97     Hk =   2.500\n MR"
    "CHUE: Inverse mode at  98     Hk =   2.500\n MRCHUE: Inverse mode at  99     Hk =   2.500\n "
    "MRCHUE: Convergence failed at  99  side 1    Res =  0.3164E-01\n MRCHUE: Convergence failed"
    " at 100  side 1    Res =  0.5368E+01\n MRCHUE: Inverse mode at 101     Hk =   2.500\n MRCHUE"
    ": Inverse mode at 102     Hk =   2.500\n MRCHUE: Inverse mode at 103     Hk =   2.500\n MRCH"
    "UE: Inverse mode at 104     Hk =   2.500\n MRCHUE: Convergence failed at 105  side 1    Res"
    " =  0.4342E+01\n MRCHUE: Inverse mode at 106     Hk =   2.500\n MRCHUE: Inverse mode at 107 "
    "    Hk =   2.500\n MRCHUE: Inverse mode at 108     Hk =   2.500\n MRCHUE: Convergence failed"
    " at 109  side 1    Res =  0.5931E+01\n MRCHUE: Convergence failed at 110  side 1    Res =  "
    "0.5102E+01\n    side            2  ...\n MRCHUE: Inverse mode at  52     Hk =   3.154\n\n MRCH"
    "DU: Convergence failed at  89  side 1    Res =  0.1312E+01\n MRCHDU: Convergence failed at "
    " 90  side 1    Res =  0.1482E+01\n MRCHDU: Convergence failed at  91  side 1    Res =  0.15"
    "48E+01\n MRCHDU: Convergence failed at  92  side 1    Res =  0.1617E+01\n MRCHDU: Convergenc"
    "e failed at  93  side 1    Res =  0.1690E+01\n MRCHDU: Convergence failed at  94  side 1   "
    " Res =  0.1765E+01\n MRCHDU: Convergence failed at  95  side 1    Res =  0.1843E+01\n MRCHDU"
    ": Convergence failed at  96  side 1    Res =  0.1923E+01\n MRCHDU: Convergence failed at  9"
    "7  side 1    Res =  0.2006E+01\n MRCHDU: Convergence failed at  98  side 1    Res =  0.2091"
    "E+01\n MRCHDU: Convergence failed at  99  side 1    Res =  0.2180E+01\n MRCHDU: Convergence "
    "failed at 100  side 1    Res =  0.3136E+01\n MRCHDU: Convergence failed at 101  side 1    R"
    "es =  0.3461E+01\n MRCHDU: Convergence failed at 102  side 1    Res =  0.3570E+01\n MRCHDU: "
    "Convergence failed at 103  side 1    Res =  0.3681E+01\n MRCHDU: Convergence failed at 104 "
    " side 1    Res =  0.3793E+01\n MRCHDU: Convergence failed at 105  side 1    Res =  0.7443E+"
    "01\n MRCHDU: Convergence failed at 106  side 1    Res =  0.7717E+01\n MRCHDU: Convergence fa"
    "iled at 107  side 1    Res =  0.7975E+01\n MRCHDU: Convergence failed at 108  side 1    Res"
    " =  0.8758E+01\n MRCHDU: Convergence failed at 109  side 1    Res =  0.1205E+02\n MRCHDU: Co"
    "nvergence failed at 110  side 1    Res =  0.5273E+02\n Side 1  free  transition at x/c =  0"
    ".0069   37\n Side 2 forced transition at x/c =  1.0000   52\n\n   1   rms: 0.1789E+01   max: "
    "-.1774E+02   C at   52  2   RLX: 0.028\n       a = 25.000      CL =  3.8064\n      Cm = -0.4"
    "253     CD =  0.00839   =>   CDf =  0.00307    CDp =  0.00533\n"
;
const std::string kNonConvCompletion =
    "      9.000    8.8565   0.008393   0.002517   0.004565   0.002048    0.0069     #\n"
;
const std::string kCliIter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  46     Hk =   4"
    ".518\n MRCHUE: Inverse mode at  47     Hk =   2.500\n MRCHUE: Inverse mode at  89     Hk =  "
    " 2.500\n    side            2  ...\n MRCHUE: Inverse mode at  68     Hk =   4.060\n MRCHUE: I"
    "nverse mode at  69     Hk =   4.716\n MRCHUE: Inverse mode at  70     Hk =   5.323\n MRCHUE:"
    " Inverse mode at  71     Hk =   5.862\n MRCHUE: Inverse mode at  72     Hk =   6.323\n MRCHU"
    "E: Inverse mode at  73     Hk =   6.659\n\n Side 1  free  transition at x/c =  0.3183   46\n "
    "Side 2 forced transition at x/c =  1.0000   73\n\n   1   rms: 0.1934E+00   max: 0.1132E+01  "
    " D at   76  2   RLX: 0.828\n       a =  4.520      CL =  0.8002\n      Cm = -0.0636     CD ="
    "  0.00689   =>   CDf =  0.00501    CDp =  0.00188\n"
;
const std::string kCliIter2 =
    ""
    "\n Side 1  free  transition at x/c =  0.3483   48\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   2   rms: 0.3897E-01   max: -.2677E+00   C at   48  1\n       a =  4.520      CL "
    "=  0.7665\n      Cm = -0.0562     CD =  0.00734   =>   CDf =  0.00490    CDp =  0.00244\n"
;
const std::string kCliIter3 =
    ""
    "\n Side 1  free  transition at x/c =  0.3606   49\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   3   rms: 0.5191E-02   max: 0.9097E-01   C at   49  1\n       a =  4.520      CL "
    "=  0.7643\n      Cm = -0.0556     CD =  0.00729   =>   CDf =  0.00477    CDp =  0.00252\n"
;
const std::string kCliIter4 =
    ""
    "\n Side 1  free  transition at x/c =  0.3577   49\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   4   rms: 0.2104E-03   max: -.4042E-02   C at   49  1\n       a =  4.520      CL "
    "=  0.7642\n      Cm = -0.0556     CD =  0.00730   =>   CDf =  0.00478    CDp =  0.00251\n"
;
const std::string kCliIter5 =
    ""
    "\n Side 1  free  transition at x/c =  0.3578   49\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   5   rms: 0.7908E-06   max: -.6193E-05   C at   49  1\n       a =  4.520      CL "
    "=  0.7642\n      Cm = -0.0556     CD =  0.00730   =>   CDf =  0.00478    CDp =  0.00252\n"
;
const std::string kCliCompletion =
    "      9.000    3.1898   0.007299   0.000000   0.000000   0.000000"
    "    0.3578     #\n";
const std::string kClIter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  46     Hk =   4"
    ".518\n MRCHUE: Inverse mode at  47     Hk =   2.500\n MRCHUE: Inverse mode at  89     Hk =  "
    " 2.500\n    side            2  ...\n MRCHUE: Inverse mode at  68     Hk =   4.060\n MRCHUE: I"
    "nverse mode at  69     Hk =   4.716\n MRCHUE: Inverse mode at  70     Hk =   5.323\n MRCHUE:"
    " Inverse mode at  71     Hk =   5.862\n MRCHUE: Inverse mode at  72     Hk =   6.323\n MRCHU"
    "E: Inverse mode at  73     Hk =   6.659\n\n Side 1  free  transition at x/c =  0.3183   46\n "
    "Side 2 forced transition at x/c =  1.0000   73\n\n   1   rms: 0.1935E+00   max: 0.1133E+01  "
    " D at   76  2   RLX: 0.827\n       a =  4.504      CL =  0.7987\n      Cm = -0.0637     CD ="
    "  0.00689   =>   CDf =  0.00501    CDp =  0.00187\n"
;
const std::string kClIter2 =
    ""
    "\n Side 1  free  transition at x/c =  0.3493   48\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   2   rms: 0.4996E-01   max: -.4090E+00   C at   48  1\n       a =  4.862      CL "
    "=  0.8000\n      Cm = -0.0551     CD =  0.00743   =>   CDf =  0.00490    CDp =  0.00253\n"
;
const std::string kClIter3 =
    ""
    "\n Side 1  free  transition at x/c =  0.3230   47\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   3   rms: 0.5534E-02   max: -.6588E-01   C at   47  1\n       a =  4.899      CL "
    "=  0.8000\n      Cm = -0.0543     CD =  0.00765   =>   CDf =  0.00495    CDp =  0.00270\n"
;
const std::string kClIter4 =
    ""
    "\n Side 1  free  transition at x/c =  0.3245   47\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   4   rms: 0.2954E-03   max: 0.5296E-02   C at   47  1\n       a =  4.900      CL "
    "=  0.8000\n      Cm = -0.0543     CD =  0.00765   =>   CDf =  0.00493    CDp =  0.00272\n"
;
const std::string kClIter5 =
    ""
    "\n Side 1  free  transition at x/c =  0.3244   47\n Side 2 forced transition at x/c =  1.000"
    "0   73\n\n   5   rms: 0.1148E-05   max: 0.3863E-05   C at   78  2\n       a =  4.900      CL "
    "=  0.8000\n      Cm = -0.0543     CD =  0.00765   =>   CDf =  0.00493    CDp =  0.00272\n"
;
const std::string kClCompletion =
    "      9.000    3.1316   0.007649   0.000000   0.000000   0.000000"
    "    0.3244     #\n";

const std::string kClFarIter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  26     Hk =   4"
    ".634\n MRCHUE: Inverse mode at  27     Hk =   5.983\n MRCHUE: Inverse mode at  28     Hk =  "
    " 7.401\n MRCHUE: Inverse mode at  29     Hk =   8.923\n MRCHUE: Inverse mode at  30     Hk ="
    "   2.731\n MRCHUE: Inverse mode at  92     Hk =   2.500\n MRCHUE: Inverse mode at  93     Hk"
    " =   2.500\n MRCHUE: Inverse mode at  94     Hk =   2.500\n    side            2  ...\n MRCHU"
    "E: Inverse mode at  65     Hk =   3.800\n MRCHUE: Inverse mode at  66     Hk =   4.385\n MRC"
    "HUE: Inverse mode at  67     Hk =   4.881\n MRCHUE: Inverse mode at  68     Hk =   5.240\n\n "
    "Side 1  free  transition at x/c =  0.0358   30\n Side 2 forced transition at x/c =  1.0000 "
    "  68\n\n   1   rms: 0.4081E+00   max: -.3255E+01   C at   68  2   RLX: 0.154\n       a =  8.2"
    "20      CL =  1.2019\n      Cm = -0.0607     CD =  0.00757   =>   CDf =  0.00649    CDp =  "
    "0.00108\n"
;
const std::string kClFarIter2 =
    ""
    "\n Side 1  free  transition at x/c =  0.0358   30\n Side 2 forced transition at x/c =  1.000"
    "0   68\n\n   2   rms: 0.2285E+00   max: -.1937E+01   C at   68  2   RLX: 0.258\n       a =  8"
    ".436      CL =  1.2009\n      Cm = -0.0561     CD =  0.00883   =>   CDf =  0.00648    CDp ="
    "  0.00235\n"
;
const std::string kClFarIter3 =
    ""
    "\n Side 1  free  transition at x/c =  0.0372   30\n Side 2 forced transition at x/c =  1.000"
    "0   68\n\n   3   rms: 0.1542E+00   max: -.8445E+00   C at   68  2   RLX: 0.592\n       a =  8"
    ".779      CL =  1.2006\n      Cm = -0.0497     CD =  0.01133   =>   CDf =  0.00647    CDp ="
    "  0.00486\n"
;
const std::string kClFarIter4 =
    ""
    "\n Side 1  free  transition at x/c =  0.0397   30\n Side 2 forced transition at x/c =  1.000"
    "0   68\n\n   4   rms: 0.6675E-01   max: 0.3023E+00   D at   76  2\n       a =  9.184      CL "
    "=  1.2000\n      Cm = -0.0418     CD =  0.01406   =>   CDf =  0.00642    CDp =  0.00763\n"
;
const std::string kClFarIter5 =
    ""
    "\n Side 1  free  transition at x/c =  0.0405   30\n Side 2 forced transition at x/c =  1.000"
    "0   68\n\n   5   rms: 0.6170E-02   max: 0.4939E-01   C at   68  2\n       a =  9.233      CL "
    "=  1.2000\n      Cm = -0.0408     CD =  0.01436   =>   CDf =  0.00635    CDp =  0.00802\n"
;
const std::string kClFarIter6 =
    ""
    "\n Side 1  free  transition at x/c =  0.0401   30\n Side 2 forced transition at x/c =  1.000"
    "0   68\n\n   6   rms: 0.1311E-03   max: -.1097E-02   C at   68  2\n       a =  9.234      CL "
    "=  1.2000\n      Cm = -0.0407     CD =  0.01436   =>   CDf =  0.00633    CDp =  0.00803\n"
;
const std::string kClFarIter7 =
    ""
    "\n Side 1  free  transition at x/c =  0.0401   30\n Side 2 forced transition at x/c =  1.000"
    "0   68\n\n   7   rms: 0.4129E-06   max: 0.2522E-05   D at   73  2\n       a =  9.234      CL "
    "=  1.2000\n      Cm = -0.0407     CD =  0.01436   =>   CDf =  0.00633    CDp =  0.00803\n"
;
const std::string kClFarCompletion =
    "      9.000    4.9584   0.014364   0.000898   0.001167   0.000269    0.0401     #\n"
;
const std::string kRepanelP1Iter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  48     Hk =   4"
    ".617\n MRCHUE: Inverse mode at  49     Hk =   5.710\n MRCHUE: Inverse mode at  50     Hk =  "
    " 6.785\n MRCHUE: Inverse mode at  51     Hk =   4.627\n MRCHUE: Inverse mode at  52     Hk ="
    "   2.500\n MRCHUE: Inverse mode at  85     Hk =   2.500\n    side            2  ...\n MRCHUE:"
    " Inverse mode at  68     Hk =   4.151\n MRCHUE: Inverse mode at  69     Hk =   4.842\n MRCHU"
    "E: Inverse mode at  70     Hk =   5.522\n MRCHUE: Inverse mode at  71     Hk =   2.500\n\n Si"
    "de 1  free  transition at x/c =  0.4575   51\n Side 2  free  transition at x/c =  0.9035   "
    "71\n\n   1   rms: 0.1272E+00   max: -.9391E+00   C at   71  2   RLX: 0.532\n       a =  2.000"
    "      CL =  0.4917\n      Cm = -0.0583     CD =  0.00557   =>   CDf =  0.00473    CDp =  0."
    "00085\n"
;
const std::string kRepanelP1Iter2 =
    ""
    "\n Side 1  free  transition at x/c =  0.4716   52\n Side 2  free  transition at x/c =  0.933"
    "0   72\n\n   2   rms: 0.9231E-01   max: -.1038E+01   C at   72  2   RLX: 0.482\n       a =  2"
    ".000      CL =  0.4841\n      Cm = -0.0559     CD =  0.00565   =>   CDf =  0.00448    CDp ="
    "  0.00117\n"
;
const std::string kRepanelP1Iter3 =
    ""
    "\n Side 1  free  transition at x/c =  0.4815   53\n Side 2  free  transition at x/c =  0.937"
    "8   73\n\n   3   rms: 0.8406E-01   max: -.7410E+00   C at   53  1   RLX: 0.675\n       a =  2"
    ".000      CL =  0.4651\n      Cm = -0.0517     CD =  0.00560   =>   CDf =  0.00450    CDp ="
    "  0.00110\n"
;
const std::string kRepanelP1Iter4 =
    ""
    "\n Side 1  free  transition at x/c =  0.5176   55\n Side 2  free  transition at x/c =  0.950"
    "5   73\n\n   4   rms: 0.3173E-01   max: -.2732E+00   C at   55  1\n       a =  2.000      CL "
    "=  0.4431\n      Cm = -0.0467     CD =  0.00572   =>   CDf =  0.00423    CDp =  0.00150\n"
;
const std::string kRepanelP1Iter5 =
    ""
    "\n MRCHDU: Convergence failed at  76  side 2    Res =  0.4630E+01\n MRCHDU: Convergence fail"
    "ed at  77  side 2    Res =  0.1818E+01\n Side 1  free  transition at x/c =  0.5267   55\n Si"
    "de 2  free  transition at x/c =  0.9992   77\n\n   5   rms: 0.3093E-01   max: 0.3265E+00   D"
    " at   73  2\n       a =  2.000      CL =  0.4467\n      Cm = -0.0473     CD =  0.00570   => "
    "  CDf =  0.00412    CDp =  0.00159\n"
;
const std::string kRepanelP1Iter6 =
    ""
    "\n Side 1  free  transition at x/c =  0.5259   55\n Side 2  free  transition at x/c =  0.964"
    "9   74\n\n   6   rms: 0.2615E-01   max: -.4460E+00   D at   76  2\n       a =  2.000      CL "
    "=  0.4482\n      Cm = -0.0478     CD =  0.00576   =>   CDf =  0.00410    CDp =  0.00166\n"
;
const std::string kRepanelP1Iter7 =
    ""
    "\n Side 1  free  transition at x/c =  0.5258   55\n Side 2  free  transition at x/c =  0.972"
    "7   75\n\n   7   rms: 0.1481E-01   max: 0.2520E+00   C at   75  2\n       a =  2.000      CL "
    "=  0.4489\n      Cm = -0.0479     CD =  0.00577   =>   CDf =  0.00411    CDp =  0.00166\n"
;
const std::string kRepanelP1Iter8 =
    ""
    "\n Side 1  free  transition at x/c =  0.5257   55\n Side 2  free  transition at x/c =  0.967"
    "6   75\n\n   8   rms: 0.7847E-03   max: -.1330E-01   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00411    CDp =  0.00166\n"
;
const std::string kRepanelP1Iter9 =
    ""
    "\n Side 1  free  transition at x/c =  0.5256   55\n Side 2  free  transition at x/c =  0.967"
    "5   75\n\n   9   rms: 0.3354E-05   max: 0.3182E-04   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00412    CDp =  0.00166\n"
;
const std::string kRepanelP1Completion =
    "      9.000    3.5895   0.005778   0.000000   0.000000   0.000000    0.5256     #\n"
;
const std::string kRepanelP2Iter1 =
    ""
    "\n Initializing BL ...\n    side            1  ...\n MRCHUE: Inverse mode at  48     Hk =   4"
    ".617\n MRCHUE: Inverse mode at  49     Hk =   5.710\n MRCHUE: Inverse mode at  50     Hk =  "
    " 6.785\n MRCHUE: Inverse mode at  51     Hk =   4.627\n MRCHUE: Inverse mode at  52     Hk ="
    "   2.500\n MRCHUE: Inverse mode at  85     Hk =   2.500\n    side            2  ...\n MRCHUE:"
    " Inverse mode at  68     Hk =   4.151\n MRCHUE: Inverse mode at  69     Hk =   4.842\n MRCHU"
    "E: Inverse mode at  70     Hk =   5.522\n MRCHUE: Inverse mode at  71     Hk =   2.500\n\n Si"
    "de 1  free  transition at x/c =  0.4575   51\n Side 2  free  transition at x/c =  0.9035   "
    "71\n\n   1   rms: 0.1272E+00   max: -.9391E+00   C at   71  2   RLX: 0.532\n       a =  2.000"
    "      CL =  0.4917\n      Cm = -0.0583     CD =  0.00557   =>   CDf =  0.00473    CDp =  0."
    "00085\n"
;
const std::string kRepanelP2Iter2 =
    ""
    "\n Side 1  free  transition at x/c =  0.4716   52\n Side 2  free  transition at x/c =  0.933"
    "0   72\n\n   2   rms: 0.9231E-01   max: -.1038E+01   C at   72  2   RLX: 0.482\n       a =  2"
    ".000      CL =  0.4841\n      Cm = -0.0559     CD =  0.00565   =>   CDf =  0.00448    CDp ="
    "  0.00117\n"
;
const std::string kRepanelP2Iter3 =
    ""
    "\n Side 1  free  transition at x/c =  0.4815   53\n Side 2  free  transition at x/c =  0.937"
    "8   73\n\n   3   rms: 0.8406E-01   max: -.7410E+00   C at   53  1   RLX: 0.675\n       a =  2"
    ".000      CL =  0.4651\n      Cm = -0.0517     CD =  0.00560   =>   CDf =  0.00450    CDp ="
    "  0.00110\n"
;
const std::string kRepanelP2Iter4 =
    ""
    "\n Side 1  free  transition at x/c =  0.5176   55\n Side 2  free  transition at x/c =  0.950"
    "5   73\n\n   4   rms: 0.3173E-01   max: -.2732E+00   C at   55  1\n       a =  2.000      CL "
    "=  0.4431\n      Cm = -0.0467     CD =  0.00572   =>   CDf =  0.00423    CDp =  0.00150\n"
;
const std::string kRepanelP2Iter5 =
    ""
    "\n MRCHDU: Convergence failed at  76  side 2    Res =  0.4630E+01\n MRCHDU: Convergence fail"
    "ed at  77  side 2    Res =  0.1818E+01\n Side 1  free  transition at x/c =  0.5267   55\n Si"
    "de 2  free  transition at x/c =  0.9992   77\n\n   5   rms: 0.3093E-01   max: 0.3265E+00   D"
    " at   73  2\n       a =  2.000      CL =  0.4467\n      Cm = -0.0473     CD =  0.00570   => "
    "  CDf =  0.00412    CDp =  0.00159\n"
;
const std::string kRepanelP2Iter6 =
    ""
    "\n Side 1  free  transition at x/c =  0.5259   55\n Side 2  free  transition at x/c =  0.964"
    "9   74\n\n   6   rms: 0.2615E-01   max: -.4460E+00   D at   76  2\n       a =  2.000      CL "
    "=  0.4482\n      Cm = -0.0478     CD =  0.00576   =>   CDf =  0.00410    CDp =  0.00166\n"
;
const std::string kRepanelP2Iter7 =
    ""
    "\n Side 1  free  transition at x/c =  0.5258   55\n Side 2  free  transition at x/c =  0.972"
    "7   75\n\n   7   rms: 0.1481E-01   max: 0.2520E+00   C at   75  2\n       a =  2.000      CL "
    "=  0.4489\n      Cm = -0.0479     CD =  0.00577   =>   CDf =  0.00411    CDp =  0.00166\n"
;
const std::string kRepanelP2Iter8 =
    ""
    "\n Side 1  free  transition at x/c =  0.5257   55\n Side 2  free  transition at x/c =  0.967"
    "6   75\n\n   8   rms: 0.7847E-03   max: -.1330E-01   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00411    CDp =  0.00166\n"
;
const std::string kRepanelP2Iter9 =
    ""
    "\n Side 1  free  transition at x/c =  0.5256   55\n Side 2  free  transition at x/c =  0.967"
    "5   75\n\n   9   rms: 0.3354E-05   max: 0.3182E-04   D at   75  2\n       a =  2.000      CL "
    "=  0.4496\n      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00412    CDp =  0.00166\n"
;
const std::string kRepanelP2Completion =
    "      9.000    3.5895   0.005778   0.000000   0.000000   0.000000    0.5256     #\n"
;

void buildNaca2412Paned(session::SessionComposition& composition) {
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());
  core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
}

std::size_t countOccurrences(const std::string& text,
                             const std::string& needle) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

void configure(session::SessionComposition& composition, bool viscous,
               int maximumIterations) {
  xfoil::AnalysisConfig config;
  config.reynoldsNumber = 1000000.0;
  config.maximumIterations = maximumIterations;
  config.viscous = viscous;
  CHECK(session::configure(composition, config).success());
}

std::string readWholeFile(const char* path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return std::string{};
  }
  // No istreambuf_iterator: GCC -O3 (-Wnull-dereference) cannot prove
  // that the buffer pointer of a failed stream is non-zero.
  // the rdbuf detour avoids the apparent dereference.
  std::ostringstream buffer;
  buffer << f.rdbuf();
  return buffer.str();
}

std::string oracleSlice(const std::string& raw,
                        const std::string& startMarker) {
  const auto i = raw.find(startMarker);
  const auto j = raw.find("\n.OPER", i);
  return raw.substr(i, j - i);
}

}  // namespace

// C oracle (a_alfa2_inviscid): inviscid CLI emits exactly the GGCALC
// line; SPECCL writes no OFS text. Byte-exact full comparison.
TEST(unit, OperCliInviscidHandlerTextMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperCliCore(composition, in, out, "0.8");
  CHECK(status == app::cli::OperCliCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(), std::string(kGgcalcLine));

  CHECK_NEAR(composition.result.lastResult.alpha.value, 4.5196229276367355e+00,
             1.0e-9, 1.0e-9);
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             8.0000000000000082e-01, 1.0e-9, 1.0e-9);
  // CLI/Cl produce no SPECAL OFS snapshot (Fortran writes no OFS).
  CHECK(!composition.result.lastDiagnostics.specalSnapshot.valid);
}

// inviscid Cl also emits exactly only the GGCALC line.
TEST(unit, OperClInviscidHandlerTextMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperClCore(composition, in, out, "0.8");
  CHECK(status == app::cli::OperClCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(), std::string(kGgcalcLine));
  CHECK(!composition.result.lastDiagnostics.specalSnapshot.valid);
}

// SPECCL convergence abort (oracle i_speccl_fail, point Cl 1.0E6):
// GGCALC line + ' SPECCL: Cl convergence failed' byte-exact, no OFS;
// a typed error event appears in the chronological stream.
TEST(unit, OperCliSpecclConvergenceFailureMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperCliCore(composition, in, out, "1.0E6");
  CHECK(status == app::cli::OperCliCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(),
           std::string(kGgcalcLine) +
               " SPECCL:  CL convergence failed\n");
  CHECK(!composition.result.lastDiagnostics.specalSnapshot.valid);
  CHECK(composition.result.lastDiagnostics.specFailure ==
        core::inviscid::SpecifiedPointFailure::specclLiftConvergenceFailure);

  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK_EQ(ev.size(), 2u);
  CHECK(ev[0].kind ==
        session::OperPointEventKind::influenceSystemCalculation);
  CHECK(ev[1].kind ==
        session::OperPointEventKind::specclConvergenceFailure);
}

// same for the Cl command (oracle k_speccl_after_OK, second point).
TEST(unit, OperClSpecclConvergenceFailureMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperClCore(composition, in, out, "1.0E6");
  CHECK(status == app::cli::OperClCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(),
           std::string(kGgcalcLine) +
               " SPECCL:  CL convergence failed\n");
  CHECK(composition.result.lastDiagnostics.specFailure ==
        core::inviscid::SpecifiedPointFailure::specclLiftConvergenceFailure);
}

// oracle k_speccl_after_OK: only alpha 2 (OFS), then Cl 1.0E6 (error).
// the second point uses the stored influence system (no GGCALC line) and
// writes only the error message.
TEST(unit, OperSpecclFailureAfterSuccessfulPointMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream alphaOut;
  CHECK(handleOperAlphaCore(composition, in, alphaOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_LEGACY_EQ(alphaOut.str(), std::string(kGgcalcLine) + kOfsAlfa2);

  std::ostringstream clOut;
  CHECK(handleOperClCore(composition, in, clOut, "1.0E6") ==
        app::cli::OperClCoreStatus::analyzed);
  CHECK_LEGACY_EQ(clOut.str(), " SPECCL:  CL convergence failed\n");

  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK_EQ(ev.size(), 1u);
  CHECK(ev[0].kind ==
        session::OperPointEventKind::specclConvergenceFailure);
}

// oracle l_alfa180_visc: viscous alpha point at 180 degrees. GAM=-GAMU1
// has no +->- sign change; STFIND writes the stagnation point warning
// (xpanel.f:1366) between XYWAKE and QDCALC and, in Fortran, returns at
// I=N/2; VISCAL then continues.
TEST(unit, OperAlphaViscousStfindHeadMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "180");
  CHECK(status == app::cli::OperAlphaCoreStatus::nonConverged);

  const std::string expectedPrefix =
      std::string(kGgcalcLine) +
      "\nOFS: alfa = 180.0000 cl =  -0.2554 cdp =   0.0011 cm =  -0.0557\n" +
      kXywakeLine +
      " STFIND: Stagnation point not found. Continuing ...\n" +
      kQdcalcLine + kSolvingBlSystem + "\n Initializing BL ...\n";
  CHECK(legacyS18(out.str()).rfind(legacyS18(expectedPrefix), 0) == 0);

  // chronology: GGCALC, OFS-marker, XYWAKE, STFIND (preHead),
  // QDCALC (preHead), then BL-events.
  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK(ev.size() >= 5u);
  CHECK(ev[0].kind ==
        session::OperPointEventKind::influenceSystemCalculation);
  CHECK(ev[1].kind ==
        session::OperPointEventKind::specifiedPointOutput);
  CHECK(ev[2].kind ==
        session::OperPointEventKind::wakeTrajectoryCalculation);
  CHECK(ev[3].kind == session::OperPointEventKind::viscous &&
        ev[3].viscous.kind ==
            core::boundary_layer::ViscousEventKind::
                stagnationPointNotFound &&
        ev[3].viscous.phase ==
            core::boundary_layer::ViscousEventPhase::preHead);
  CHECK(ev[4].kind == session::OperPointEventKind::viscous &&
        ev[4].viscous.kind ==
            core::boundary_layer::ViscousEventKind::
                sourceInfluenceCalculation);

  // STFIND occurs again during the iterations (STMOVE, xoper.f:2998).
  // the count of iteration messages depends on the BL trajectory
  // (oracle l_alfa180_visc: 20 of 100); existence is checked.
  std::size_t stfindCount = 0;
  for (const auto& e : ev) {
    if (e.kind == session::OperPointEventKind::viscous &&
        e.viscous.kind ==
            core::boundary_layer::ViscousEventKind::
                stagnationPointNotFound) {
      ++stfindCount;
    }
  }
  CHECK(stfindCount >= 1u);
}

// COM1/COM2 lifecycle: Fortran BLSYS carries the similarity station COM2
// over into COM1 (xblsys.f:617-621); the first MRCHUE inverse branch
// reads X1=X2, T1=NaN, HK1=NaN and preserves HTARG=NaN ->
// MAX(NaN,3.8)=3.800. The core must perform the same adoption; a default
// COM1 (0,0,0) would yield HTARG=+Inf.
TEST(unit, OperAlphaViscousAlfa180FirstInverseModeUsesSimilarityCom1) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 20);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "180");
  CHECK(status == app::cli::OperAlphaCoreStatus::nonConverged);

  const auto& ev = composition.result.lastDiagnostics.events;
  const core::boundary_layer::ViscousEventRecord* first = nullptr;
  for (const auto& e : ev) {
    if (e.kind == session::OperPointEventKind::viscous &&
        e.viscous.kind ==
            core::boundary_layer::ViscousEventKind::mrchueInverseMode) {
      first = &e.viscous;
      break;
    }
  }
  CHECK(first != nullptr);
  if (first != nullptr) {
    CHECK_EQ(first->side, 0);
    CHECK_EQ(first->station, 1);  // IBL=2 (similarity station)
    CHECK(first->hk == 3.8);
  }
}

// full-text comparison against the native stdin oracle: the entire
// Alfa-180 trajectory (inverse-mode sequence, STFIND, MRCHUE/MRCHDU,
// TRCHEK2, transitions, RMSBL/RMXBL, FORMAT 2000/2010/2020/9922,
// non-convergence and continuation hint) is byte-identical.
TEST(unit, OperAlphaViscousAlfa180FullTextMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 20);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "180");
  CHECK(status == app::cli::OperAlphaCoreStatus::nonConverged);

  const std::string raw = readWholeFile(
      "tests/fortran-reference/cases/oper-state/"
      "oper-alfa180-viscous-iter20/stdout.raw.txt");
  const std::string expected =
      oracleSlice(raw, " Calculating unit vorticity distributions");
  CHECK(legacyS18(out.str()) == legacyS18(expected));
}

// COM1 persistence is limited to each ViscousState: two independent
// sessions produce the same first inverse-mode value, without a carried
// COM1 state from one session affecting the other.
TEST(unit, OperAlphaViscousAlfa180Com1NotSharedAcrossSessions) {
  auto firstInverseHk = [] {
    session::SessionComposition composition;
    buildNaca2412Paned(composition);
    configure(composition, true, 20);
    std::istringstream in;
    std::ostringstream out;
    handleOperAlphaCore(composition, in, out, "180");
    for (const auto& e : composition.result.lastDiagnostics.events) {
      if (e.kind == session::OperPointEventKind::viscous &&
          e.viscous.kind ==
              core::boundary_layer::ViscousEventKind::mrchueInverseMode) {
        return e.viscous.hk;
      }
    }
    return -1.0;
  };
  const double a = firstInverseHk();
  const double b = firstInverseHk();
  CHECK(a == 3.8);
  CHECK(b == 3.8);
}

// oracle n_alfa180_twice: second alpha-180 point (warm start). GGCALC,
// XYWAKE and the header STFIND are omitted (LIPAN/LWAKE set); OFS and the
// iteration STFIND messages (STMOVE) remain.
TEST(unit, OperAlphaViscousStfindRepeatedPointMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream first;
  handleOperAlphaCore(composition, in, first, "180");

  std::ostringstream second;
  const auto status = handleOperAlphaCore(composition, in, second, "180");
  CHECK(status == app::cli::OperAlphaCoreStatus::nonConverged);

  const std::string expectedPrefix =
      "\nOFS: alfa = 180.0000 cl =  -0.2554 cdp =   0.0011 cm =  -0.0557\n" +
      std::string(kSolvingBlSystem);
  CHECK(legacyS18(second.str()).rfind(legacyS18(expectedPrefix), 0) == 0);

  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK(ev[0].kind ==
        session::OperPointEventKind::specifiedPointOutput);
  bool headStfind = false;
  bool wakeEvent = false;
  bool iterStfind = false;
  for (const auto& e : ev) {
    if (e.kind == session::OperPointEventKind::wakeTrajectoryCalculation) {
      wakeEvent = true;
    }
    if (e.kind == session::OperPointEventKind::viscous &&
        e.viscous.kind ==
            core::boundary_layer::ViscousEventKind::
                stagnationPointNotFound) {
      if (e.viscous.phase ==
          core::boundary_layer::ViscousEventPhase::preHead) {
        headStfind = true;
      } else {
        iterStfind = true;
      }
    }
  }
  CHECK(!headStfind);
  CHECK(!wakeEvent);
  CHECK(iterStfind);
}

// oracle p_bang_nonconv: ITER 1, alpha 2 (not converged), then '!'.
// the continuation writes only VISCAL(ITMAX) text on the retained state:
// no GGCALC/OFS/XYWAKE/QDCALC/Initializing BL; the iteration count again
// starts at 1.
TEST(unit, OperBangContinuesNonConvergedViscousPointMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::nonConverged);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  const std::string expected =
      std::string(kSolvingBlSystem) + "\n" +
      " Side 1  free  transition at x/c =  0.4716   52\n"
      " Side 2  free  transition at x/c =  0.9330   72\n"
      "\n"
      "   1   rms: 0.9231E-01   max: -.1038E+01   C at   72  2   RLX: 0.482\n"
      "       a =  2.000      CL =  0.4841\n"
      "      Cm = -0.0559     CD =  0.00565   =>   CDf =  0.00448    CDp =  0.00117\n"
      " VISCAL:  Convergence failed\n"
      "      9.000    5.1087   0.005651   0.001431   0.001421  -0.000009    0.4716     #\n"
      " Type \"!\" to continue iterating\n";
  CHECK_LEGACY_EQ(bang.str(), expected);

  // event stream of the continuation run: no inviscid events.
  const auto& ev = composition.result.lastDiagnostics.events;
  for (const auto& e : ev) {
    CHECK(e.kind == session::OperPointEventKind::viscous);
  }
  CHECK(!ev.empty());
}

// oracle q_bang_conv: converged point (ITER 10, 9 iterations), then
// '!': VISCAL runs again and converges in iteration 1.
TEST(unit, OperBangAfterConvergedPointMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::continuedConverged);
  const std::string expected =
      std::string(kSolvingBlSystem) + "\n" +
      " Side 1  free  transition at x/c =  0.5256   55\n"
      " Side 2  free  transition at x/c =  0.9675   75\n"
      "\n"
      "   1   rms: 0.4564E-08   max: 0.5431E-07   D at   75  2\n"
      "       a =  2.000      CL =  0.4496\n"
      "      Cm = -0.0481     CD =  0.00578   =>   CDf =  0.00412    CDp =  0.00166\n"
      "      9.000    3.5895   0.005778   0.000000   0.000000   0.000000    0.5256     #\n";
  CHECK_LEGACY_EQ(bang.str(), expected);
}

// oracle r_bang_cli / s_bang_cl: continuation after CLI or Cl.
TEST(unit, OperBangAfterCliAndClPointsMatchFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream cli;
  CHECK(handleOperCliCore(composition, in, cli, "0.8") ==
        app::cli::OperCliCoreStatus::nonConverged);
  std::ostringstream bangCli;
  CHECK(handleOperBangCore(composition, bangCli) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  CHECK(bangCli.str().rfind(kSolvingBlSystem, 0) == 0);
  CHECK(bangCli.str().find("OFS:") == std::string::npos);
  CHECK(bangCli.str().find("Calculating") == std::string::npos);

  session::SessionComposition clSession;
  buildNaca2412Paned(clSession);
  configure(clSession, true, 1);
  std::istringstream clIn;
  std::ostringstream clOut;
  CHECK(handleOperClCore(clSession, clIn, clOut, "0.8") ==
        app::cli::OperClCoreStatus::nonConverged);
  std::ostringstream bangCl;
  CHECK(handleOperBangCore(clSession, bangCl) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  CHECK(bangCl.str().rfind(kSolvingBlSystem, 0) == 0);
  CHECK(bangCl.str().find("OFS:") == std::string::npos);
}

// oracle t_bang_inviscid: inviscid repeat -> no text (LRECALC
// skips SPECAL; with LVISC=false, VISCAL is omitted).
TEST(unit, OperBangInviscidRepeatProducesNoText) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::noRepeatOutput);
  CHECK_LEGACY_EQ(bang.str(), std::string());
}

// oracle v_bang_none: '!' without a previous point command.
TEST(unit, OperBangWithoutPreviousPointMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::noPreviousCommand);
  CHECK_LEGACY_EQ(bang.str(), " Previous .OPER command not valid\n");
}

// oracle u_bang_inviscid_then_visc: inviscid alpha point, then VISC
// and '!': VISCAL on the retained inviscid state (no SPECAL);
// because !LWAKE, the wake is built by XYWAKE.
TEST(unit, OperBangAfterInviscidPointThenViscousMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  session::setViscousMode(composition, true);
  session::setReynoldsNumber(composition, 1000000.0);

  std::ostringstream bang;
  const auto status = handleOperBangCore(composition, bang);
  CHECK(status == app::cli::OperBangCoreStatus::continuedNonConverged ||
        status == app::cli::OperBangCoreStatus::continuedConverged);
  const std::string expectedPrefix =
      std::string(kXywakeLine) + kQdcalcLine + std::string(kSolvingBlSystem) +
      "\n Initializing BL ...\n";
  CHECK(legacyS18(bang.str()).rfind(legacyS18(expectedPrefix), 0) == 0);
  CHECK(bang.str().find("OFS:") == std::string::npos);
  CHECK(bang.str().find(kGgcalcLine) == std::string::npos);

  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK(ev[0].kind ==
        session::OperPointEventKind::wakeTrajectoryCalculation);
}

// repeated '!' (oracle p_bang_nonconv, third line): each
// continuation is its own VISCAL(ITMAX) run with its own event stream;
// old events are not repeated.
TEST(unit, OperBangRepeatedUsesFreshEventStream) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream first;
  handleOperAlphaCore(composition, in, first, "2");

  std::ostringstream bang1;
  CHECK(handleOperBangCore(composition, bang1) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  const auto events1 = composition.result.lastDiagnostics.events;

  std::ostringstream bang2;
  CHECK(handleOperBangCore(composition, bang2) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  const auto& events2 = composition.result.lastDiagnostics.events;

  // the numerical iteration values differ between the
  // continuation runs (advanced BL state); the structure is
  // identical: VISCAL header, VISCAL-only events, non-convergence +
  // continuation hint.
  CHECK(bang2.str().rfind(kSolvingBlSystem, 0) == 0);
  CHECK(bang2.str().find(kViscalFailed) != std::string::npos);
  CHECK(bang2.str().find(kContinueHint) != std::string::npos);
  CHECK(!events1.empty());
  for (const auto& e : events2) {
    CHECK(e.kind == session::OperPointEventKind::viscous);
  }
}

// change between point and '!' (Fortran: COMOLD stays, only
// LVCONV=false, xoper.f 333): continuation remains permitted, with no
// SPECAL/OFS in the continuation text.
TEST(unit, OperBangAfterReynoldsChangeStaysContinuable) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream first;
  handleOperAlphaCore(composition, in, first, "2");

  session::setReynoldsNumber(composition, 2000000.0);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  CHECK(bang.str().rfind(kSolvingBlSystem, 0) == 0);
  CHECK(bang.str().find("OFS:") == std::string::npos);
  CHECK(bang.str().find(kGgcalcLine) == std::string::npos);
}

// A geometry/panel change invalidates the continuation state
// (PANGEN/ABCOPY set LIPAN/LBLINI/LVCONV=false, xfoil.f 2080-2086,
// xgdes.f 1021-1026; session: lastPointContinuable=false).
TEST(unit, OperBangAfterPanelingChangeNotContinuable) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream first;
  handleOperAlphaCore(composition, in, first, "2");

  CHECK(session::applyPaneling(composition).ok);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::notContinuable);
  CHECK_LEGACY_EQ(bang.str(), std::string());
}

// oracle w_aseq_inv: ASEQ 0 4 2 inviscid. GGCALC once, OFS per point
// (SPECAL also writes OFS in the sequence).
TEST(unit, OperAseqInviscidMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "0 4 2") ==
        app::cli::OperAseqCoreStatus::completed);
  CHECK_LEGACY_EQ(out.str(),
           std::string(kGgcalcLine) +
               "\nOFS: alfa =   0.0000 cl =   0.2554 cdp =  -0.0011 cm =  -0.0557\n"
               "\nOFS: alfa =   2.0000 cl =   0.4968 cdp =  -0.0011 cm =  -0.0587\n"
               "\nOFS: alfa =   4.0000 cl =   0.7376 cdp =  -0.0011 cm =  -0.0616\n");
}

// oracle y_aseq_down: ASEQ 4 0 2 -> a positive step is forced negative
// (xoper.f 576-580); target order 4,2,0.
TEST(unit, OperAseqDescendingForcesNegativeStep) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "4 0 2") ==
        app::cli::OperAseqCoreStatus::completed);
  CHECK_LEGACY_EQ(out.str(),
           std::string(kGgcalcLine) +
               "\nOFS: alfa =   4.0000 cl =   0.7376 cdp =  -0.0011 cm =  -0.0616\n"
               "\nOFS: alfa =   2.0000 cl =   0.4968 cdp =  -0.0011 cm =  -0.0587\n"
               "\nOFS: alfa =   0.0000 cl =   0.2554 cdp =  -0.0011 cm =  -0.0557\n");
}

// oracle z_aseq_single: DAA=0 -> exactly one point (xoper.f 583-584).
TEST(unit, OperAseqZeroStepSinglePoint) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "2 2 0") ==
        app::cli::OperAseqCoreStatus::completed);
  CHECK_LEGACY_EQ(out.str(),
           std::string(kGgcalcLine) +
               "\nOFS: alfa =   2.0000 cl =   0.4968 cdp =  -0.0011 cm =  -0.0587\n");
}

// oracle aa_aseq_nseqex: viscous ITER 1, ASEQ 0 10 1. After four
// non-converged points, halt text FORMAT 1150 exactly once;
// no continuation hint in the sequence.
TEST(unit, OperAseqNseqexHaltMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "0 10 1") ==
        app::cli::OperAseqCoreStatus::haltedNonConvergence);
  CHECK_EQ(countOccurrences(out.str(),
                            " Sequence halted since previous  4 points did not converge\n"
                            " Last-converged  alpha =   0.000    CL =   0.00000\n"),
           std::size_t{1});
  CHECK_EQ(countOccurrences(out.str(), kSolvingBlSystem), std::size_t{4});
  CHECK_EQ(countOccurrences(out.str(), kViscalFailed), std::size_t{4});
  CHECK(out.str().find(kContinueHint) == std::string::npos);
  CHECK_EQ(countOccurrences(out.str(), kGgcalcLine), std::size_t{1});
  CHECK_EQ(countOccurrences(out.str(), "OFS:"), std::size_t{0});
}

// oracle from_cseq_visc: CSEQ 0.2 0.8 0.3. OFS per point from the
// SPECAL share; the alpha comes from SPECCL and is chained.
TEST(unit, OperCseqViscousChainedAlphaMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperCseqCore(composition, in, out, "0.2 0.8 0.3") ==
        app::cli::OperCseqCoreStatus::completed);
  CHECK_EQ(countOccurrences(out.str(), "OFS:"), std::size_t{0});
  CHECK_EQ(countOccurrences(out.str(), kXywakeLine), std::size_t{3});
  CHECK_EQ(countOccurrences(out.str(), kQdcalcLine), std::size_t{3});
  CHECK(out.str().find(kContinueHint) == std::string::npos);
}

// ASEQ->CSEQ chain NACA 0012 with a raw-bit pin of the
// session alpha. Fortran carries the COMMON alpha (CR09, radians)
// unchanged between the CSEQ points (xoper.f 672); the C++ driver
// previously carried the degree round trip, shifting near-zero values
// by about 1 ULP. Fortran end alpha after ASEQ 0 2 1 and
// CSEQ 0 2 1: 0.29309823892681941 (gdb on the canonical reference
// binary, identical to the PLXADD header of the last point).
TEST(unit, OperCseqChainRawRadiansCarryMatchesFortran) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(12, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  core::paneling::PanelingParameters params =
      session::panelingParameters(composition);
  params.panelCount = 160;
  params.teToLeDensityRatio = 0.15;
  CHECK(session::setPanelingParameters(composition, params).ok);
  CHECK(session::applyPaneling(composition).ok);
  configure(composition, false, 20);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "0 2 1") ==
        app::cli::OperAseqCoreStatus::completed);
  CHECK(handleOperCseqCore(composition, in, out, "0 2 1") ==
        app::cli::OperCseqCoreStatus::completed);
  // PUB-W2: on Linux (exact platform) bitwise, as the CHECK_NEAR(...,0,0)
  // was. On Windows/MSYS2 the acceptance run measured |diff| 3.886e-16
  // (rel 1.3e-15, single radian-carry deviation of the chain); class
  // kReplayValue (1e-14 + 1e-13*|e|, margin 26x absolute) is the
  // documented portable bound for this carried value.
  testing::checkPortableNumeric(composition.viscous.viscals.alfa,
                                0.29309823892681941,
                                testing::PortableNumericClass::kReplayValue,
                                "composition.viscous.viscals.alfa",
                                "0.29309823892681941", __FILE__, __LINE__);
}

// oracle ac_cseq_down: descending CL target chain 0.8,0.5,0.2.
TEST(unit, OperCseqDescendingMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperCseqCore(composition, in, out, "0.8 0.2 0.3") ==
        app::cli::OperCseqCoreStatus::completed);
  CHECK_EQ(countOccurrences(out.str(), "OFS:"), std::size_t{0});
  CHECK_EQ(countOccurrences(out.str(), kXywakeLine), std::size_t{3});
}

// oracle ad_cseq_single: DCL=0 -> one point.
TEST(unit, OperCseqZeroStepSinglePoint) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperCseqCore(composition, in, out, "0.5 0.5 0") ==
        app::cli::OperCseqCoreStatus::completed);
  CHECK_LEGACY_EQ(out.str(),
           std::string(kGgcalcLine) +
               "\nOFS: alfa =   2.0267 cl =   0.5000 cdp =  -0.0011 cm =  -0.0587\n");
}

// oracle af_aseq_partial: ASEQ 0 -> prompt order last, step;
// byte-exact prompt texts.
TEST(unit, OperAseqPartialPromptOrderMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in("4\n2\n");
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "0") ==
        app::cli::OperAseqCoreStatus::completed);
  const std::string prompts =
      "\nEnter last  alfa value (deg)   r>  "
      "\nEnter alfa increment   (deg)   r>  ";
  CHECK(out.str().rfind(prompts, 0) == 0);
  CHECK(out.str().find(kGgcalcLine) != std::string::npos);
  CHECK_EQ(countOccurrences(out.str(), "OFS:"), std::size_t{0});
}

// oracle ah_aseq_eof: EOF in the prompt -> controlled abort without
// a sequence (C++ contract; the canonical binary aborts on the
// digit).
TEST(unit, OperAseqEofDuringPromptAbortsWithoutSequence) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in("");
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "0") ==
        app::cli::OperAseqCoreStatus::inputAborted);
  CHECK(out.str().find(kGgcalcLine) == std::string::npos);
  CHECK(out.str().find("OFS:") == std::string::npos);
}

// oracle ae_cseq_nseqex: CSEQ NSEQEX halt after four points.
TEST(unit, OperCseqNseqexHaltMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperCseqCore(composition, in, out, "0.2 2.0 0.2") ==
        app::cli::OperCseqCoreStatus::haltedNonConvergence);
  CHECK_EQ(countOccurrences(out.str(),
                            " Sequence halted since previous  4 points did not converge\n"
                            " Last-converged  alpha =   0.000    CL =   0.00000\n"),
           std::size_t{1});
  CHECK_EQ(countOccurrences(out.str(), kSolvingBlSystem), std::size_t{4});
}

// Converging viscous sequence (ITER 10): ISEQEX stays below the
// threshold, no halt text, no note.
TEST(unit, OperAseqViscousConvergedNoHalt) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAseqCore(composition, in, out, "0 2 2") ==
        app::cli::OperAseqCoreStatus::completed);
  CHECK(out.str().find("Sequence halted") == std::string::npos);
  CHECK(out.str().find(kContinueHint) == std::string::npos);
  CHECK_EQ(countOccurrences(out.str(), kSolvingBlSystem), std::size_t{2});
}

// session isolation and geometry preservation: two independent sessions
// with an identical sequence deliver identical text; panel revision and
// geometry remain unchanged.
TEST(unit, OperSequenceSessionsIndependentAndGeometryStable) {
  session::SessionComposition a;
  buildNaca2412Paned(a);
  configure(a, false, 100);
  const auto revisionA = a.paneling.panelSourceRevision;

  session::SessionComposition b;
  buildNaca2412Paned(b);
  configure(b, false, 100);

  std::istringstream in;
  std::ostringstream outA;
  std::ostringstream outB;
  handleOperAseqCore(a, in, outA, "0 4 2");
  handleOperAseqCore(b, in, outB, "0 4 2");
  CHECK_LEGACY_EQ(outA.str(), outB.str());
  CHECK_EQ(a.paneling.panelSourceRevision, revisionA);
  CHECK(a.geometry.valid);
  CHECK(a.paneling.panelingValid);
}

// A oracle (a_alfa2_inviscid, point 1): GGCALC line + OFS line
// (format 333) byte-exact; the OFS snapshot comes from the SPECAL state
// before VISCAL (inviscid: final value = snapshot).
TEST(unit, OperAlphaInviscidHandlerEmitsGgcalcAndOfsLine) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "2");
  CHECK(status == app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(), std::string(kGgcalcLine) + kOfsAlfa2);

  const session::SpecifiedPointSnapshot& snap =
      composition.result.lastDiagnostics.specalSnapshot;
  CHECK(snap.valid);
  CHECK_NEAR(snap.alpha, 2.0 * 3.141592653589793116e+00 / 180.0, 1.0e-12,
             1.0e-12);
  CHECK_NEAR(snap.cl, 4.9679999999999997e-01, 5.0e-05, 0.0);
  CHECK_NEAR(snap.cdp, -1.1e-03, 5.0e-05, 0.0);
  CHECK_NEAR(snap.cm, -5.87e-02, 5.0e-05, 0.0);
}

// second identical inviscid point: GGCALC does not run again,
// SPECAL runs again -> only the OFS line (oracle a_alfa2_inviscid point 2).
TEST(unit, OperAlphaInviscidSecondPointEmitsOnlyOfsLine) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  std::ostringstream second;
  CHECK(handleOperAlphaCore(composition, in, second, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_LEGACY_EQ(second.str(), kOfsAlfa2);
}

// ASKR prompts (userio.f 1000) byte-exact; after the input the
// canonical point text sequence follows, including OFS.
TEST(unit, OperSinglePointPromptsMatchFortranAskrFormat) {
  session::SessionComposition alphaSession;
  buildNaca2412Paned(alphaSession);
  configure(alphaSession, false, 100);
  std::istringstream alphaIn("2\n");
  std::ostringstream alphaOut;
  CHECK(handleOperAlphaCore(alphaSession, alphaIn, alphaOut, "") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_LEGACY_EQ(alphaOut.str(), std::string(kAlphaPrompt) + kGgcalcLine + kOfsAlfa2);

  session::SessionComposition cliSession;
  buildNaca2412Paned(cliSession);
  configure(cliSession, false, 100);
  std::istringstream cliIn("0.8\n");
  std::ostringstream cliOut;
  CHECK(handleOperCliCore(cliSession, cliIn, cliOut, "") ==
        app::cli::OperCliCoreStatus::analyzed);
  CHECK_LEGACY_EQ(cliOut.str(), std::string(kCliPrompt) + kGgcalcLine);

  session::SessionComposition clSession;
  buildNaca2412Paned(clSession);
  configure(clSession, false, 100);
  std::istringstream clIn("0.8\n");
  std::ostringstream clOut;
  CHECK(handleOperClCore(clSession, clIn, clOut, "") ==
        app::cli::OperClCoreStatus::analyzed);
  CHECK_LEGACY_EQ(clOut.str(), std::string(kClPrompt) + kGgcalcLine);
}

// A blank line at the prompt keeps the required value (oracle f_invalid_alfa:
// alpha with blank line -> alpha 0, OFS line with alfa 0.0000).
TEST(unit, OperSinglePointBlankLineKeepsDefaultArgument) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in("\n");
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(), std::string(kAlphaPrompt) + kGgcalcLine + kOfsAlfa0);
  CHECK_NEAR(composition.result.lastResult.alpha.value, 0.0, 0.0, 0.0);
}

// F oracle (f_invalid_alfa): an invalid inline number leads back to the
// prompt; afterwards a normal analysis with OFS.
TEST(unit, OperAlphaInvalidInlineFallsBackToPrompt) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in("2\n");
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "foo") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_LEGACY_EQ(out.str(), std::string(kAlphaPrompt) + kGgcalcLine + kOfsAlfa2);
  CHECK_NEAR(composition.result.lastResult.alpha.value, 2.0, 1.0e-12, 0.0);
}

// EOF at the prompt: controlled inputAborted without analysis.
TEST(unit, OperSinglePointPromptEofAbortsControlled) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "") ==
        app::cli::OperAlphaCoreStatus::inputAborted);
  CHECK_LEGACY_EQ(out.str(), std::string(kAlphaPrompt));
}

// B oracle (b_alfa2_twice, point 1): the complete numerical
// surface of a viscous alpha point byte-exact: GGCALC, OFS, XYWAKE,
// QDCALC, VISCAL header, nine iteration blocks including the canonical
// BL event lines (Initializing BL, MRCHUE sides / inverse,
// MRCHDU error, transitions FORMAT 9100/9200) and FORMAT
// 2000/2010/2020, completion FORMAT 9922.
TEST(unit, OperAlphaViscousFullNumericSurfaceMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "2");
  CHECK(status == app::cli::OperAlphaCoreStatus::analyzed);

  std::string expected = std::string(kGgcalcLine) + kOfsAlfa2 + kXywakeLine +
                         kQdcalcLine + kSolvingBlSystem;
  expected += kAlfa2Iter1 + kAlfa2Iter2 + kAlfa2Iter3 + kAlfa2Iter4 +
              kAlfa2Iter5 + kAlfa2Iter6 + kAlfa2Iter7 + kAlfa2Iter8 +
              kAlfa2Iter9 + kAlfa2Completion;
  CHECK_LEGACY_EQ(out.str(), expected);
  CHECK(composition.result.lastResult.converged);
  CHECK_EQ(composition.result.lastResult.iterations, 9);
  CHECK_EQ(composition.result.lastDiagnostics.iterations.size(),
           std::size_t(9));
  CHECK(composition.result.lastDiagnostics.completionValid);
}

// B oracle point 2 (warm start): OFS again, VISCAL header, one iteration
// (FORMAT 2010, RLX=1), completion 9922.
TEST(unit, OperAlphaViscousWarmStartSecondPointMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  std::ostringstream second;
  CHECK(handleOperAlphaCore(composition, in, second, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const std::string expected = std::string(kOfsAlfa2) + kSolvingBlSystem +
                               kAlfa2WarmIter1 + kAlfa2Completion;
  CHECK_LEGACY_EQ(second.str(), expected);
  CHECK_EQ(composition.result.lastResult.iterations, 1);
}

// E oracle (e_alfa2_iter1, ITER 1): iteration, VISCAL non-convergence,
// completion 9922, continuation hint in canonical order.
TEST(unit, OperAlphaViscousNonConvergedSurfaceMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "2");
  CHECK(status == app::cli::OperAlphaCoreStatus::nonConverged);

  const std::string expected = std::string(kGgcalcLine) + kOfsAlfa2 +
                               std::string(kXywakeLine) + kQdcalcLine + std::string(kSolvingBlSystem) +
                               kIter1Only + kViscalFailed + kIter1Completion +
                               kContinueHint;
  CHECK_LEGACY_EQ(out.str(), expected);

  // LPACC omission (xoper.f 445-447): note omitted, rest identical.
  session::SessionComposition polarSession;
  buildNaca2412Paned(polarSession);
  configure(polarSession, true, 1);
  std::istringstream polarIn;
  std::ostringstream polarOut;
  CHECK(handleOperAlphaCore(polarSession, polarIn, polarOut, "2", true) ==
        app::cli::OperAlphaCoreStatus::nonConverged);
  CHECK_LEGACY_EQ(polarOut.str(), expected.substr(0, expected.size() -
                                                 std::string(kContinueHint)
                                                     .size()));
}

// G oracle (g_nonconverging, alpha 25, ITER 1): MRCHUE and MRCHDU
// convergence error, recovery path and transition lines in canonical
// order, byte-exact.
TEST(unit, OperAlphaViscousMarchFailureSurfaceMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 1);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperAlphaCore(composition, in, out, "25");
  CHECK(status == app::cli::OperAlphaCoreStatus::nonConverged);

  const std::string expected = std::string(kGgcalcLine) +
                               "\nOFS: alfa =  25.0000 cl =   3.1569 cdp =  -0.0018 cm =  -0.0920\n" +
                               std::string(kXywakeLine) + kQdcalcLine + std::string(kSolvingBlSystem) +
                               kNonConvIter1 + kViscalFailed +
                               kNonConvCompletion + kContinueHint;
  CHECK_LEGACY_EQ(out.str(), expected);
}

// structure contract of the event records: 0-based sides/stations,
// iteration number, phase order and canonical chronology without
// retroactive sorting.
TEST(unit, OperViscousEventRecordsChronologyAndZeroBasedIndices) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const auto& ev = composition.result.lastDiagnostics.events;
  using session::OperPointEventKind;
  using core::boundary_layer::ViscousEventKind;
  using core::boundary_layer::ViscousEventPhase;
  CHECK(!ev.empty());
  // chronology of the first point: GGCALC, OFS marker, XYWAKE, QDCALC,
  // then BL events.
  CHECK(ev[0].kind == OperPointEventKind::influenceSystemCalculation);
  CHECK(ev[1].kind == OperPointEventKind::specifiedPointOutput);
  CHECK(ev[2].kind == OperPointEventKind::wakeTrajectoryCalculation);
  CHECK(ev[3].kind == OperPointEventKind::viscous);
  CHECK(ev[3].viscous.kind == ViscousEventKind::sourceInfluenceCalculation);
  CHECK(ev[3].viscous.phase == ViscousEventPhase::preHead);
  CHECK(ev[4].kind == OperPointEventKind::viscous);
  CHECK(ev[4].viscous.kind == ViscousEventKind::boundaryLayerInitialization);
  CHECK(ev[4].viscous.phase == ViscousEventPhase::initMarch);
  CHECK(ev[4].viscous.iteration == 0);

  int lastIteration = -1;
  int lastPhase = -1;
  for (const auto& e : ev) {
    if (e.kind != OperPointEventKind::viscous) continue;
    const auto& v = e.viscous;
    if (v.phase == ViscousEventPhase::preHead) continue;
    CHECK(v.iteration >= lastIteration);
    if (v.iteration != lastIteration) lastPhase = -1;
    const int phase = v.phase == ViscousEventPhase::initMarch ? 0 : 1;
    CHECK(phase >= lastPhase);
    lastPhase = phase;
    lastIteration = v.iteration;
    if (v.kind == ViscousEventKind::marchingSideStart) {
      CHECK(v.side == 0 || v.side == 1);
      CHECK(v.station == -1);
    }
    if (v.kind == ViscousEventKind::mrchueInverseMode) {
      CHECK(v.station >= 0);
      CHECK(v.hk > 0.0);
    }
    if (v.kind == ViscousEventKind::transitionFree ||
        v.kind == ViscousEventKind::transitionForced) {
      CHECK(v.phase == ViscousEventPhase::postMarch);
      CHECK(v.side == 0 || v.side == 1);
      CHECK(v.transitionStation >= 0);
      CHECK(v.xOverC >= 0.0 && v.xOverC <= 1.0);
    }
  }
  // per iteration exactly one transition event pair (both sides).
  std::map<int, int> transitionsPerIteration;
  for (const auto& e : ev) {
    if (e.kind == OperPointEventKind::viscous &&
        (e.viscous.kind == ViscousEventKind::transitionFree ||
         e.viscous.kind == ViscousEventKind::transitionForced)) {
      ++transitionsPerIteration[e.viscous.iteration];
    }
  }
  CHECK_EQ(transitionsPerIteration.size(),
           composition.result.lastDiagnostics.iterations.size());
  for (const auto& [iter, count] : transitionsPerIteration) {
    CHECK_EQ(count, 2);
    (void)iter;
  }
}

// C oracle (c_cli08_visc): viscous CLI point. SPECCL writes no OFS;
// XYWAKE/QDCALC lines derive from the dynamic event stream after
// SPECCL. Iteration blocks (a = 4.520 constant, LALFA=true) and 9922
// byte-exact.
TEST(unit, OperCliViscousNumericSurfaceMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperCliCore(composition, in, out, "0.8");
  CHECK(status == app::cli::OperCliCoreStatus::analyzed);

  std::string expected =
      std::string(kGgcalcLine) + kXywakeLine + kQdcalcLine + kSolvingBlSystem;
  expected += kCliIter1 + kCliIter2 + kCliIter3 + kCliIter4 + kCliIter5 +
              kCliCompletion;
  CHECK_LEGACY_EQ(out.str(), expected);
  CHECK(out.str().find("OFS:") == std::string::npos);
  CHECK_EQ(composition.result.lastResult.iterations, 5);
}

// D oracle (d_cl08_visc): viscous Cl point (LALFA=false): alpha runs in
// the 2020 lines from 4.504 to 4.900; distinguishes itself uniquely from
// CLI.
TEST(unit, OperClViscousNumericSurfaceMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperClCore(composition, in, out, "0.8");
  CHECK(status == app::cli::OperClCoreStatus::analyzed);

  std::string expected =
      std::string(kGgcalcLine) + kXywakeLine + kQdcalcLine + kSolvingBlSystem;
  expected += kClIter1 + kClIter2 + kClIter3 + kClIter4 + kClIter5 +
              kClCompletion;
  CHECK_LEGACY_EQ(out.str(), expected);
  CHECK(out.str().find("OFS:") == std::string::npos);
  CHECK_EQ(composition.result.lastResult.iterations, 5);
}

// H oracle (h_cl_far_alpha, Cl 1.2 viscous): the alpha obtained from SPECCL
// (9.234) lies far from the start alpha; XYWAKE/QDCALC lines and the
// 2020 lines carry the result alpha, not the start alpha.
TEST(unit, OperClFarAlphaViscousSurfaceMatchesFortranOracle) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  const auto status = handleOperClCore(composition, in, out, "1.2");
  CHECK(status == app::cli::OperClCoreStatus::analyzed);

  std::string expected =
      std::string(kGgcalcLine) + kXywakeLine + kQdcalcLine + kSolvingBlSystem;
  expected += kClFarIter1 + kClFarIter2 + kClFarIter3 + kClFarIter4 +
              kClFarIter5 + kClFarIter6 + kClFarIter7 + kClFarCompletion;
  CHECK_LEGACY_EQ(out.str(), expected);
  CHECK(out.str().find("OFS:") == std::string::npos);
  CHECK_EQ(composition.result.lastResult.iterations, 7);

  // the XYWAKE event carries the inviscid alpha achieved by SPECCL
  // (7.864 deg), not the start alpha 0 and not the later
  // VISCAL-converged alpha 9.234.
  const auto& ev = composition.result.lastDiagnostics.events;
  bool sawWake = false;
  for (const auto& e : ev) {
    if (e.kind == session::OperPointEventKind::wakeTrajectoryCalculation) {
      sawWake = true;
      CHECK_NEAR(e.alpha, 7.864 * 3.141592653589793116e+00 / 180.0, 1.0e-3,
                 0.0);
    }
  }
  CHECK(sawWake);
}

// G oracle behavior (g_repanel): panel invalidation forces
// GGCALC, XYWAKE and QDCALC to run again.
TEST(unit, OperRepanelInvalidationRerunsInviscidEvents) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(first.str().find("Calculating unit vorticity distributions") !=
        std::string::npos);

  CHECK(session::applyPaneling(composition).ok);

  std::ostringstream second;
  CHECK(handleOperAlphaCore(composition, in, second, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(second.str().find("Calculating unit vorticity distributions") !=
        std::string::npos);
  CHECK(second.str().find("Calculating wake trajectory") != std::string::npos);
  CHECK(second.str().find("Calculating source influence matrix") !=
        std::string::npos);

  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK(ev.front().kind ==
        session::OperPointEventKind::influenceSystemCalculation);
}

// identical repetition point: wake/system cache stays preserved;
// GGCALC/XYWAKE/QDCALC events and lines are omitted in the second
// point entirely (oracle b_alfa2_twice point 2).
TEST(unit, OperRepeatedIdenticalViscousPointEmitsNoInviscidEvents) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  std::ostringstream second;
  CHECK(handleOperAlphaCore(composition, in, second, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const std::string expectedSecond = std::string(kOfsAlfa2) + kSolvingBlSystem +
                                     kAlfa2WarmIter1 + kAlfa2Completion;
  CHECK_LEGACY_EQ(second.str(), expectedSecond);

  for (const auto& e : composition.result.lastDiagnostics.events) {
    CHECK(e.kind == session::OperPointEventKind::viscous ||
          e.kind == session::OperPointEventKind::specifiedPointOutput);
  }
}

// inviscid point: only GGCALC and OFS marker events; no
// wake/DIJ/viscous events (VISCAL does not run).
TEST(unit, OperInviscidPointHasOnlyGgcalcAndOfsEvents) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  const auto& ev = composition.result.lastDiagnostics.events;
  CHECK_EQ(ev.size(), 2u);
  CHECK(ev[0].kind == session::OperPointEventKind::influenceSystemCalculation);
  CHECK(ev[1].kind == session::OperPointEventKind::specifiedPointOutput);
  CHECK(out.str().find("Calculating wake trajectory") == std::string::npos);
  CHECK(out.str().find("Calculating source influence matrix") ==
        std::string::npos);
}

// abort before the analysis: no event is subsequently invented.
TEST(unit, OperCancelledPointProducesNoEvents) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  CancellationToken token;
  token.requestCancellation();

  std::istringstream in;
  std::ostringstream out;
  const AnalysisResult result =
      session::runOperAlphaPoint(composition, xfoil::Degrees{2.0}, &token);
  CHECK(result.status == AnalysisStatus::cancelled);
  CHECK(composition.result.lastDiagnostics.events.empty());
}
// exactly the executed iteration count; a second handler call on
// the same session starts exactly one further point.
TEST(unit, OperSinglePointRecordsDoNotRepeatSolver) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream first;
  CHECK(handleOperAlphaCore(composition, in, first, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_EQ(composition.result.lastDiagnostics.iterations.size(),
           static_cast<std::size_t>(composition.result.lastResult.iterations));
  const int firstIterations = composition.result.lastResult.iterations;

  std::ostringstream second;
  CHECK(handleOperAlphaCore(composition, in, second, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_EQ(composition.result.lastDiagnostics.iterations.size(),
           static_cast<std::size_t>(composition.result.lastResult.iterations));
  CHECK(composition.result.lastResult.iterations < firstIterations);
}

// Formatting mutates no numerical session values: the emitter is
// rendered twice from the same stored records; the
// output is identical and the session numerics stay unchanged.
TEST(unit, OperSinglePointFormattingDoesNotMutateSession) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  std::istringstream in;
  std::ostringstream out;
  CHECK(handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const double cl = composition.viscous.viscals.cl;
  const double cd = composition.viscous.viscals.cd;
  const double cdf = composition.viscous.viscals.cdf;
  const std::array<int, 2> itran = composition.viscous.viscals.itran;

  const auto& diag = composition.result.lastDiagnostics;
  std::ostringstream render1;
  std::ostringstream render2;
  for (int pass = 0; pass < 2; ++pass) {
    std::ostringstream& r = pass == 0 ? render1 : render2;
    for (const auto& rec : diag.iterations) {
      if (rec.rlx < 1.0) {
        app::cli::emitViscalIterationWithRelaxation(
            r, 1, rec.rmsbl, rec.rmxbl, rec.vmxbl, rec.imxbl + 1,
            rec.ismxbl + 1, rec.rlx);
      } else {
        app::cli::emitViscalIterationWithoutRelaxation(
            r, 1, rec.rmsbl, rec.rmxbl, rec.vmxbl, rec.imxbl + 1,
            rec.ismxbl + 1);
      }
      app::cli::emitViscalIterationLoads(
          r, rec.alfa * 180.0 / 3.141592653589793116e+00, rec.cl, rec.cm,
          rec.cd, rec.cdf, rec.clf, rec.cdp);
    }
    app::cli::emitViscalCompletion(r, diag.completion.acrit,
                                   diag.completion.hkmax, diag.completion.cd,
                                   diag.completion.psep, diag.completion.patt,
                                   diag.completion.delp, diag.completion.xoctr);
  }
  CHECK_LEGACY_EQ(render1.str(), render2.str());
  CHECK(!render1.str().empty());
  CHECK_NEAR(composition.viscous.viscals.cl, cl, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.viscals.cd, cd, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.viscals.cdf, cdf, 0.0, 0.0);
  CHECK_EQ(itran[0], composition.viscous.viscals.itran[0]);
  CHECK_EQ(itran[1], composition.viscous.viscals.itran[1]);
}

// independent sessions: identical handler output, no
// state transfer.
TEST(unit, OperSinglePointHandlerIndependentSessions) {
  std::ostringstream a;
  std::ostringstream b;
  {
    session::SessionComposition s;
    buildNaca2412Paned(s);
    configure(s, true, 100);
    std::istringstream in;
    CHECK(handleOperAlphaCore(s, in, a, "2") ==
          app::cli::OperAlphaCoreStatus::analyzed);
  }
  {
    session::SessionComposition s;
    buildNaca2412Paned(s);
    configure(s, true, 100);
    std::istringstream in;
    CHECK(handleOperAlphaCore(s, in, b, "2") ==
          app::cli::OperAlphaCoreStatus::analyzed);
  }
  CHECK_LEGACY_EQ(a.str(), b.str());
}

// Three deterministic handler runs on fresh sessions deliver
// byte-identical output (text determinism).
TEST(unit, OperSinglePointHandlerTextIsDeterministic) {
  std::string previous;
  for (int run = 0; run < 3; ++run) {
    session::SessionComposition composition;
    buildNaca2412Paned(composition);
    configure(composition, true, 100);
    std::istringstream in;
    std::ostringstream out;
    CHECK(handleOperAlphaCore(composition, in, out, "2") ==
          app::cli::OperAlphaCoreStatus::analyzed);
    if (run > 0) {
      CHECK_LEGACY_EQ(out.str(), previous);
    }
    previous = out.str();
  }
  CHECK(previous.find(kAlfa2Completion) != std::string::npos);
}

// missing panel contour: controlled missingGeometry without any output.
TEST(unit, OperSinglePointHandlerWithoutPanelingIsControlled) {
  session::SessionComposition composition;
  const auto naca = core::geometry::nacaAirfoil(2412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer).success());

  std::istringstream in;
  std::ostringstream alphaOut;
  std::ostringstream cliOut;
  std::ostringstream clOut;
  CHECK(handleOperAlphaCore(composition, in, alphaOut, "2") ==
        app::cli::OperAlphaCoreStatus::missingGeometry);
  CHECK(handleOperCliCore(composition, in, cliOut, "0.8") ==
        app::cli::OperCliCoreStatus::missingGeometry);
  CHECK(handleOperClCore(composition, in, clOut, "0.8") ==
        app::cli::OperClCoreStatus::missingGeometry);
  CHECK_LEGACY_EQ(alphaOut.str(), std::string());
  CHECK_LEGACY_EQ(cliOut.str(), std::string());
  CHECK_LEGACY_EQ(clOut.str(), std::string());
}

// API/batch/sequence paths keep their previous behavior: the
// cold path carries no warm-start fields and no OFS-snapshot
// persistence; the OPER single-point semantics routes points away.
TEST(unit, OperSinglePointDiagnosticsLeaveApiColdPathUnchanged) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);

  const AnalysisResult first =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(first.successful);
  CHECK(!composition.viscous.wakeValid);
  const AnalysisResult second =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(second.successful);
  CHECK_EQ(second.iterations, first.iterations);  // cold start unchanged
}

// OPER INIT (xoper.f 394-401): toggle/flag contract after a converged
// viscous point. Oracle c/d: the first INIT after VISCAL switches
// LBLINI=false and LIPAN=false; LVCONV, wake, station fields,
// mapping content, AVISC/MVISC and result are retained.
TEST(unit, OperInitTogglesLbliniAndLipanAfterConvergedPoint) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);

  const AnalysisResult point =
      session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr);
  CHECK(point.successful);
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.viscals.mapping.valid);
  CHECK(composition.viscous.converged);

  const auto thetTop = composition.viscous.viscals.thet.top;
  const auto ipanTop = composition.viscous.viscals.mapping.top.ipan;
  const double avisc = composition.viscous.viscals.avisc;
  const double mvisc = composition.viscous.viscals.mvisc;
  const bool wakeValid = composition.viscous.wakeValid;
  const double awake = composition.viscous.awake;
  const AnalysisResult lastBefore = composition.result.lastResult;
  const auto revision = composition.paneling.panelSourceRevision;

  std::ostringstream out;
  CHECK(handleOperInitCore(composition, out) ==
        app::cli::OperInitCoreStatus::willInitialize);
  CHECK_LEGACY_EQ(out.str(), std::string(" BLs will be initialized on next point\n"));
  CHECK(!composition.viscous.viscals.lblini);
  CHECK(!composition.viscous.viscals.mapping.valid);
  CHECK(composition.viscous.converged);
  CHECK(composition.viscous.viscals.thet.top == thetTop);
  CHECK(composition.viscous.viscals.mapping.top.ipan == ipanTop);
  CHECK_EQ(composition.viscous.viscals.avisc, avisc);
  CHECK_EQ(composition.viscous.viscals.mvisc, mvisc);
  CHECK_EQ(composition.viscous.wakeValid, wakeValid);
  CHECK_EQ(composition.viscous.awake, awake);
  CHECK_EQ(composition.result.lastResult.liftCoefficient,
           lastBefore.liftCoefficient);
  CHECK_EQ(composition.paneling.panelSourceRevision, revision);
}

// Double INIT (oracle e): the second toggle restores LBLINI=true;
// LIPAN stays false (xoper.f 396-400 switches LIPAN only when
// disabling).
TEST(unit, OperInitTwiceRestoresLbliniKeepsLipanFalse) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);

  std::ostringstream first;
  std::ostringstream second;
  CHECK(handleOperInitCore(composition, first) ==
        app::cli::OperInitCoreStatus::willInitialize);
  CHECK(handleOperInitCore(composition, second) ==
        app::cli::OperInitCoreStatus::initializedAssumed);
  CHECK_LEGACY_EQ(first.str(), std::string(" BLs will be initialized on next point\n"));
  CHECK_LEGACY_EQ(second.str(), std::string(" BLs are assumed to be initialized\n"));
  CHECK(composition.viscous.viscals.lblini);
  CHECK(!composition.viscous.viscals.mapping.valid);
}

// INIT before the first analysis point (oracle b): starting LBLINI=false
// (xfoil.f 461) is switched to true; no analysis, no OFS.
TEST(unit, OperInitBeforeFirstPointTogglesToTrue) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 100);
  CHECK(!composition.viscous.viscals.lblini);

  std::ostringstream out;
  CHECK(handleOperInitCore(composition, out) ==
        app::cli::OperInitCoreStatus::initializedAssumed);
  CHECK_LEGACY_EQ(out.str(), std::string(" BLs are assumed to be initialized\n"));
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.lastPointOrigin ==
        session::OperPointOrigin::none);
}

// OPER without geometry leads to the airfoil abort in Fortran before
// the menu (xoper.f 49-53); the handler reports only
// missingGeometry without its own text.
TEST(unit, OperInitWithoutGeometryHasNoText) {
  session::SessionComposition composition;
  std::ostringstream out;
  CHECK(handleOperInitCore(composition, out) ==
        app::cli::OperInitCoreStatus::missingGeometry);
  CHECK_LEGACY_EQ(out.str(), std::string());
}

// INIT -> '!' (oracle d): COMOLD stays the point command (INIT does not
// set COMOLD, xoper.f 471/506/541); '!' repeats VISCAL(ITMAX)
// with LRECALC and, because LBLINI=false, runs again through the
// SETBL initialization (xbl.f 82-87), without SPECAL/OFS/GGCALC.
TEST(unit, OperInitThenBangReinitializesBlWithoutSpecal) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);

  std::ostringstream initOut;
  handleOperInitCore(composition, initOut);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::continuedConverged);
  const std::string& text = bang.str();
  CHECK(text.rfind(kSolvingBlSystem, 0) == 0);
  CHECK(text.find("\n Initializing BL ...\n") != std::string::npos);
  CHECK(text.find("OFS:") == std::string::npos);
  CHECK(text.find(kGgcalcLine) == std::string::npos);
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.viscals.mapping.valid);
}

// Double INIT -> '!' (oracle e): LBLINI=true, LIPAN=false; '!'
// repeats VISCAL without SETBL initialization, but with
// STFIND/IBLPAN restoration (xoper.f 2914-2933).
TEST(unit, OperInitTwiceThenBangSkipsBlInitialization) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);

  std::ostringstream initOut;
  handleOperInitCore(composition, initOut);
  handleOperInitCore(composition, initOut);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::continuedConverged);
  CHECK(bang.str().rfind(kSolvingBlSystem, 0) == 0);
  CHECK(bang.str().find("Initializing BL") == std::string::npos);
  CHECK(composition.viscous.viscals.mapping.valid);
}

// inviscid point -> VISC -> INIT -> '!': OPER INIT switches LBLINI
// to true before any QVFUE has filled QVIS. Fortran hits the COMMON QVIS
// null field in the VISCAL header (xoper.f 2916, XFOIL.INC CR04
// zero-initialized), GAMQV sets GAM ~ 0, STFIND returns, and the
// marching runs in the degenerate NaN regime up to the normal VISCAL
// non-convergence completion (oracle
// oper-state/oper-init-inviscid-then-bang). The session supplies the
// COMMON zero state with zero-initialized capacity
// (ViscousVelocityFieldState); the path is memory-safe. Full byte parity
// of the degenerate text is still not reached (see the deviation
// report); this test pins the structural completion, not the numeric
// overflow.
TEST(unit, OperInviscidPointViscInitBangDegenerateCompletes) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 20);  // canonical ITMAX-default (xfoil.f 695)
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  session::setViscousMode(composition, true);

  std::ostringstream initOut;
  CHECK(handleOperInitCore(composition, initOut) ==
        app::cli::OperInitCoreStatus::initializedAssumed);
  CHECK(composition.viscous.viscals.lblini);
  CHECK(composition.viscous.viscals.qvisState ==
        core::boundary_layer::ViscousVelocityFieldState::commonInitialized);

  std::ostringstream bang;
  CHECK(handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::continuedNonConverged);
  const auto& diag = composition.result.lastDiagnostics;
  CHECK(diag.viscalRan);
  CHECK(!diag.viscalConverged);
  CHECK_EQ(diag.iterations.size(), std::size_t{20});
  CHECK(bang.str().find(kXywakeLine) != std::string::npos);
  CHECK(bang.str().find(" STFIND: Stagnation point not found") !=
        std::string::npos);
  CHECK(bang.str().find(kQdcalcLine) != std::string::npos);
  CHECK(bang.str().find(kSolvingBlSystem) != std::string::npos);
  CHECK(bang.str().find("Initializing BL") == std::string::npos);
  CHECK(bang.str().find("OFS:") == std::string::npos);
  CHECK(bang.str().find(kGgcalcLine) == std::string::npos);
  CHECK(bang.str().find(kViscalFailed) != std::string::npos);
  CHECK(bang.str().find(kContinueHint) != std::string::npos);
  // Fortran COMMON start values in the degenerate path (oracle
  // oper-init-inviscid-then-bang): XOCTR start 1.0 (xfoil.f 679-680),
  // VMXBL/IMXBL/ISMXBL unset (NUL, 0, 0), list-directed
  // REAL(8) NaN field width 26.
  CHECK(bang.str().find("  free  transition at x/c =  1.0000   83") !=
        std::string::npos);
  CHECK(bang.str().find(std::string("   \0 at    0  0", 15)) !=
        std::string::npos);
  CHECK(bang.str().find("SETBL: Xtr???  n1 n2:"
                        "                        NaN"
                        "                       NaN") != std::string::npos);
  CHECK(bang.str().find("    1.0000     #") != std::string::npos);
  CHECK(composition.viscous.viscals.qvisState ==
        core::boundary_layer::ViscousVelocityFieldState::solved);
}

// QVIS field state: before the first viscous point the state is the
// COMMON zero state; after a VISCAL run it is solved.
TEST(unit, QvisFieldStateTracksCommonInitAndSolved) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);
  CHECK(composition.viscous.viscals.qvisState ==
        core::boundary_layer::ViscousVelocityFieldState::commonInitialized);
  CHECK(composition.viscous.viscals.qvis.empty());
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  CHECK(composition.viscous.viscals.qvisState ==
        core::boundary_layer::ViscousVelocityFieldState::solved);
  CHECK(!composition.viscous.viscals.qvis.empty());
}

// OPER INIT and VPAR INIT are state-equivalent (xoper.f 394-401 vs.
// 2649-2653, oracle g1/g2): identical flag changes and identical
// follow-up text for the next '!'.
TEST(unit, OperInitAndVparInitAreStateEquivalent) {
  session::SessionComposition a;
  buildNaca2412Paned(a);
  configure(a, true, 10);
  CHECK(session::runSpecAlpha(a, xfoil::Degrees{2.0}, nullptr).successful);
  session::SessionComposition b;
  buildNaca2412Paned(b);
  configure(b, true, 10);
  CHECK(session::runSpecAlpha(b, xfoil::Degrees{2.0}, nullptr).successful);

  std::ostringstream outA;
  std::ostringstream outB;
  CHECK(handleOperInitCore(a, outA) ==
        app::cli::OperInitCoreStatus::willInitialize);
  CHECK(app::cli::handleVparInitCore(b, outB) ==
        app::cli::VparInitCoreStatus::willInitialize);
  CHECK_LEGACY_EQ(outA.str(), outB.str());
  CHECK_EQ(a.viscous.viscals.lblini, b.viscous.viscals.lblini);
  CHECK_EQ(a.viscous.viscals.mapping.valid, b.viscous.viscals.mapping.valid);
  CHECK_EQ(a.viscous.converged, b.viscous.converged);

  std::ostringstream bangA;
  std::ostringstream bangB;
  handleOperBangCore(a, bangA);
  handleOperBangCore(b, bangB);
  CHECK_LEGACY_EQ(bangA.str(), bangB.str());
}

// independent sessions and text determinism.
TEST(unit, OperInitIndependentSessionsDeterministic) {
  session::SessionComposition a;
  buildNaca2412Paned(a);
  configure(a, true, 10);
  CHECK(session::runSpecAlpha(a, xfoil::Degrees{2.0}, nullptr).successful);
  session::SessionComposition b;
  buildNaca2412Paned(b);
  configure(b, true, 10);
  CHECK(session::runSpecAlpha(b, xfoil::Degrees{2.0}, nullptr).successful);

  std::ostringstream outA1;
  std::ostringstream outA2;
  std::ostringstream outB;
  handleOperInitCore(a, outA1);
  handleOperInitCore(a, outA2);
  handleOperInitCore(b, outB);
  CHECK_LEGACY_EQ(outA1.str(), outB.str());
  CHECK_LEGACY_EQ(outA2.str(), std::string(" BLs are assumed to be initialized\n"));
  CHECK(a.viscous.viscals.lblini);
  CHECK(!b.viscous.viscals.lblini);
}

namespace {
std::string padTo48(std::string_view text) {
  if (text.size() >= 48) return std::string(text.substr(0, 48));
  return std::string(text) + std::string(48 - text.size(), ' ');
}
std::string namePromptLine(std::string_view nameDefault) {
  return "\n Enter airfoil name or <return> for default:  " +
         padTo48(nameDefault) + "\n";
}
}  // namespace

// OPER name inline (xoper.f 1725-1733): silent; mutates the canonical
// name field (identical to the TOP-level field, XFOIL.INC CHARACTER*48);
// no analysis/panel/geometry invalidation.
TEST(unit, OperNameInlineIsSilentAndKeepsAnalysisState) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  const auto statusBefore = composition.result.lastResult.status;
  const bool panelValidBefore = composition.paneling.panelingValid;

  std::ostringstream out;
  CHECK(app::cli::handleOperNameCore(composition, out, "FOO BAR", true, "",
                                     false) ==
        app::cli::OperNameCoreStatus::applied);
  CHECK(out.str().empty());
  CHECK_EQ(composition.geometry.name, "FOO BAR");
  CHECK_EQ(composition.geometry.buffer.name, "FOO BAR");
  CHECK(statusBefore == composition.result.lastResult.status);
  CHECK_EQ(composition.paneling.panelingValid, panelValidBefore);
}

// OPER name without an argument: NAMMOD prompt byte-exact; without an
// answer no mutation; an empty answer applies the NAMDEF requirement;
// a non-empty answer overwrites.
TEST(unit, OperNamePromptContractByteExact) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  std::ostringstream setup;
  CHECK(app::cli::handleOperNameCore(composition, setup, "FOO BAR", true, "",
                                     false) ==
        app::cli::OperNameCoreStatus::applied);

  std::ostringstream out;
  CHECK(app::cli::handleOperNameCore(composition, out, "", false, "", false) ==
        app::cli::OperNameCoreStatus::promptRequested);
  CHECK_LEGACY_EQ(out.str(), namePromptLine("FOO BAR"));
  CHECK_EQ(composition.geometry.name, "FOO BAR");

  std::ostringstream outEmpty;
  app::cli::handleOperNameCore(composition, outEmpty, "", false, "", false);
  CHECK(app::cli::handleOperNameCore(composition, outEmpty, "", false, "",
                                     true) ==
        app::cli::OperNameCoreStatus::applied);
  CHECK_LEGACY_EQ(outEmpty.str(), namePromptLine("FOO BAR"));
  CHECK_EQ(composition.geometry.name, "FOO BAR");

  std::ostringstream outAnswer;
  app::cli::handleOperNameCore(composition, outAnswer, "", false, "", false);
  CHECK(app::cli::handleOperNameCore(composition, outAnswer, "", false, "BAZ",
                                     true) ==
        app::cli::OperNameCoreStatus::applied);
  CHECK_LEGACY_EQ(outAnswer.str(), namePromptLine("FOO BAR"));
  CHECK_EQ(composition.geometry.name, "BAZ");
}

// OPER NINC/NDEC contract against the persistent oracle
// oper-state/oper-name-inline-prompt-ninc-ndec: [1]+NDEC keeps the
// tag (KMODP=0 -> NAMDEF original), NINC counts up.
TEST(unit, OperNincNdecFollowsNammodTagContract) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  std::ostringstream setup;
  CHECK(app::cli::handleOperNameCore(composition, setup, "FOO BAR", true, "",
                                     false) ==
        app::cli::OperNameCoreStatus::applied);

  std::ostringstream ninc1;
  app::cli::handleOperNincCore(composition, ninc1, "", false);
  CHECK(app::cli::handleOperNincCore(composition, ninc1, "", true) ==
        app::cli::OperNincCoreStatus::applied);
  CHECK_LEGACY_EQ(ninc1.str(), namePromptLine("FOO BAR [1]"));
  CHECK_EQ(composition.geometry.name, "FOO BAR [1]");

  std::ostringstream ndec1;
  app::cli::handleOperNdecCore(composition, ndec1, "", false);
  CHECK(app::cli::handleOperNdecCore(composition, ndec1, "", true) ==
        app::cli::OperNdecCoreStatus::applied);
  CHECK_LEGACY_EQ(ndec1.str(), namePromptLine("FOO BAR [1]"));
  CHECK_EQ(composition.geometry.name, "FOO BAR [1]");

  std::ostringstream ninc2;
  app::cli::handleOperNincCore(composition, ninc2, "", false);
  CHECK(app::cli::handleOperNincCore(composition, ninc2, "", true) ==
        app::cli::OperNincCoreStatus::applied);
  CHECK_LEGACY_EQ(ninc2.str(), namePromptLine("FOO BAR [2]"));
  CHECK_EQ(composition.geometry.name, "FOO BAR [2]");
}

// NAMMOD boundaries against oper-state/oper-name-nammod-limits:
// [99]+NINC -> MOD(100,100)=0 -> tag stays; NDEC [99]->[98];
// leading zeros; CHARACTER*48 truncation.
TEST(unit, OperNincNdecNammodLimits) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  std::ostringstream setup;
  CHECK(app::cli::handleOperNameCore(composition, setup, "X [99]", true, "",
                                     false) ==
        app::cli::OperNameCoreStatus::applied);

  std::ostringstream ninc99;
  app::cli::handleOperNincCore(composition, ninc99, "", false);
  CHECK(app::cli::handleOperNincCore(composition, ninc99, "", true) ==
        app::cli::OperNincCoreStatus::applied);
  CHECK_LEGACY_EQ(ninc99.str(), namePromptLine("X [99]"));
  CHECK_EQ(composition.geometry.name, "X [99]");

  std::ostringstream ndec99;
  app::cli::handleOperNdecCore(composition, ndec99, "", false);
  CHECK(app::cli::handleOperNdecCore(composition, ndec99, "", true) ==
        app::cli::OperNdecCoreStatus::applied);
  CHECK_LEGACY_EQ(ndec99.str(), namePromptLine("X [98]"));
  CHECK_EQ(composition.geometry.name, "X [98]");

  std::ostringstream setup007;
  CHECK(app::cli::handleOperNameCore(composition, setup007, "Y [007]", true,
                                     "", false) ==
        app::cli::OperNameCoreStatus::applied);
  std::ostringstream ninc007;
  app::cli::handleOperNincCore(composition, ninc007, "", false);
  CHECK(app::cli::handleOperNincCore(composition, ninc007, "", true) ==
        app::cli::OperNincCoreStatus::applied);
  CHECK_LEGACY_EQ(ninc007.str(), namePromptLine("Y [8]"));
  CHECK_EQ(composition.geometry.name, "Y [8]");

  const std::string longName(60, 'A');
  std::ostringstream setupLong;
  CHECK(app::cli::handleOperNameCore(composition, setupLong, longName, true,
                                     "", false) ==
        app::cli::OperNameCoreStatus::applied);
  CHECK_EQ(composition.geometry.name.size(), std::size_t{48});
  std::ostringstream nincLong;
  app::cli::handleOperNincCore(composition, nincLong, "", false);
  CHECK(app::cli::handleOperNincCore(composition, nincLong, "", true) ==
        app::cli::OperNincCoreStatus::applied);
  CHECK_LEGACY_EQ(nincLong.str(), namePromptLine(std::string(48, 'A')));
  CHECK_EQ(composition.geometry.name.size(), std::size_t{48});
}

// TOP-level name and OPER NINC share the same canonical field
// (oracle oper-state/oper-name-top-level-separation).
TEST(unit, OperNincSharesCanonicalNameFieldWithTopLevelName) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  session::setGeometryName(composition, "TOPNAME");
  std::ostringstream out;
  app::cli::handleOperNincCore(composition, out, "", false);
  CHECK(app::cli::handleOperNincCore(composition, out, "", true) ==
        app::cli::OperNincCoreStatus::applied);
  CHECK_LEGACY_EQ(out.str(), namePromptLine("TOPNAME [1]"));
  CHECK_EQ(composition.geometry.name, "TOPNAME [1]");
}

// name after an analysis point: silent; COMOLD/'!' continuation unchanged
// (oracle oper-state/oper-name-after-point-bang).
TEST(unit, OperNameAfterPointLeavesBangContinuationUnchanged) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  std::ostringstream nameOut;
  CHECK(app::cli::handleOperNameCore(composition, nameOut, "FOO", true, "",
                                     false) ==
        app::cli::OperNameCoreStatus::applied);
  CHECK(nameOut.str().empty());

  std::ostringstream bang;
  CHECK(app::cli::handleOperBangCore(composition, bang) ==
        app::cli::OperBangCoreStatus::noRepeatOutput);
  CHECK(bang.str().empty());
  CHECK(composition.viscous.lastPointOrigin ==
        session::OperPointOrigin::alpha);
}

// Agreement with the persistent oracle text: the prompt line of the
// first NINC after name FOO BAR is byte-identical.
TEST(unit, OperNincPromptMatchesPermanentOracleBytes) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  std::ostringstream setup;
  app::cli::handleOperNameCore(composition, setup, "FOO BAR", true, "", false);
  std::ostringstream ninc;
  app::cli::handleOperNincCore(composition, ninc, "", false);
  app::cli::handleOperNincCore(composition, ninc, "", true);
  const std::string oracleLine =
      " Enter airfoil name or <return> for default:  FOO BAR [1]" +
      std::string(37, ' ') + "\n";
  CHECK_LEGACY_EQ(ninc.str(), "\n" + oracleLine);
}

namespace {
std::string cpmnLine(double cp, double x) {
  std::ostringstream ss;
  app::cli::emitCpmnInviscid(ss, cp, x);
  return ss.str();
}
}  // namespace

// CPMN without a point: Fortran prints the COMMON start values 0.0000
// (oracle oper-state/oper-cpmn-no-point).
TEST(unit, OperCpmnWithoutPointPrintsCommonZeros) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  std::ostringstream out;
  CHECK(app::cli::handleOperCpmnCore(composition, out) ==
        app::cli::OperCpmnCoreStatus::noCurrentPoint);
  CHECK_LEGACY_EQ(out.str(), cpmnLine(0.0, 0.0));
}

// CPMN inviscid after Alfa (oracle oper-state/oper-cpmn-inviscid).
TEST(unit, OperCpmnInviscidMatchesOracleBytes) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  std::ostringstream out;
  CHECK(app::cli::handleOperCpmnCore(composition, out) ==
        app::cli::OperCpmnCoreStatus::available);
  CHECK_LEGACY_EQ(out.str(), cpmnLine(-0.8237, 0.0989));
}

// CPMN viscous after VISC+Alfa: format 1769 with the viscous line
// (oracle oper-state/oper-cpmn-viscous).
TEST(unit, OperCpmnViscousEmitsBothLines) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  std::ostringstream out;
  CHECK(app::cli::handleOperCpmnCore(composition, out) ==
        app::cli::OperCpmnCoreStatus::available);
  std::ostringstream expected;
  app::cli::emitCpmnViscous(expected, composition.operating.cpMinimum.cpmni,
                            composition.operating.cpMinimum.xcpmni,
                            composition.operating.cpMinimum.cpmnv,
                            composition.operating.cpMinimum.xcpmnv);
  CHECK(out.str().find("  Minimum Inviscid Cp =") == 0);
  CHECK(out.str().find("\n  Minimum Viscous  Cp =") != std::string::npos);
  CHECK_LEGACY_EQ(out.str(), expected.str());
  CHECK_LEGACY_EQ(out.str(),
           "  Minimum Inviscid Cp = -0.8237   at x =  0.0989\n"
           "  Minimum Viscous  Cp = -0.7885   at x =  0.1118\n");
  CHECK(composition.operating.cpMinimum.cpmnv < 0.0);
}

// Mach updates CPI/CPV, but not the FCPMIN snapshot
// (oracle oper-state/oper-cpmn-after-Mach).
TEST(unit, OperCpmnAfterMachKeepsPointTimeSnapshot) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  std::ostringstream before;
  CHECK(app::cli::handleOperCpmnCore(composition, before) ==
        app::cli::OperCpmnCoreStatus::available);
  session::setMachNumber(composition, 0.3);
  session::refreshExistingPointAfterMachChange(composition);
  std::ostringstream after;
  CHECK(app::cli::handleOperCpmnCore(composition, after) ==
        app::cli::OperCpmnCoreStatus::available);
  CHECK_LEGACY_EQ(after.str(), before.str());
}

// panel change preserves the FCPMIN COMMON snapshot (oracle
// oper-state/oper-cpmn-after-panel-change: Fortran still prints the
// point-time value after PANE).
TEST(unit, OperCpmnAfterPanelChangeKeepsCommonSnapshot) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  std::ostringstream before;
  CHECK(app::cli::handleOperCpmnCore(composition, before) ==
        app::cli::OperCpmnCoreStatus::available);
  CHECK(session::applyPaneling(composition).ok);
  std::ostringstream after;
  CHECK(app::cli::handleOperCpmnCore(composition, after) ==
        app::cli::OperCpmnCoreStatus::available);
  CHECK_LEGACY_EQ(after.str(), before.str());
  CHECK_LEGACY_EQ(after.str(), cpmnLine(-0.8237, 0.0989));
}

// pure read-only query: CPMN mutates no session and starts no solver.
TEST(unit, OperCpmnQueryDoesNotMutateSession) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  const auto snap = composition.operating.cpMinimum;
  const auto status = composition.result.lastResult.status;
  std::ostringstream out;
  app::cli::handleOperCpmnCore(composition, out);
  CHECK(snap.hasBeenUpdatedByFcpmin ==
        composition.operating.cpMinimum.hasBeenUpdatedByFcpmin);
  CHECK(snap.cpmni == composition.operating.cpMinimum.cpmni);
  CHECK(status == composition.result.lastResult.status);
}

// geometry sequence (NACA reset) preserves the COMMON snapshot;
// Fortran writes CPMNI/XCPMNI/CPMNV/XCPMNV only in FCPMIN.
TEST(unit, OperCpmnAfterGeometryChangeKeepsCommonSnapshot) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  CHECK(session::runSpecAlpha(composition, xfoil::Degrees{2.0}, nullptr)
            .successful);
  std::ostringstream before;
  CHECK(app::cli::handleOperCpmnCore(composition, before) ==
        app::cli::OperCpmnCoreStatus::available);
  const auto naca = core::geometry::nacaAirfoil(4412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  std::ostringstream after;
  CHECK(app::cli::handleOperCpmnCore(composition, after) ==
        app::cli::OperCpmnCoreStatus::available);
  CHECK_LEGACY_EQ(after.str(), before.str());
}

// sequence: the snapshot corresponds to the last executed point.
TEST(unit, OperCpmnAfterAseqMatchesLastSequencePoint) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, false, 10);
  std::istringstream in("0.0\n2.0\n2.0\n");
  std::ostringstream out;
  CHECK(app::cli::handleOperAseqCore(composition, in, out, "") ==
        app::cli::OperAseqCoreStatus::completed);
  std::ostringstream cpmn;
  CHECK(app::cli::handleOperCpmnCore(composition, cpmn) ==
        app::cli::OperCpmnCoreStatus::available);
  const session::CpObservation obs = session::currentCpMinimum(composition);
  CHECK(obs.valid);
  std::ostringstream expected;
  app::cli::emitCpmnInviscid(expected, obs.minimum.cpmni,
                             obs.minimum.xcpmni);
  CHECK_LEGACY_EQ(cpmn.str(), expected.str());
}

// Authoritative session-bound BLPAR state.
TEST(unit, SessionBlparDefaultsMatchBlpini) {
  session::SessionComposition composition;
  const auto& bp = composition.viscous.blpar;
  CHECK_NEAR(bp.sccon, 5.6, 0.0, 0.0);
  CHECK_NEAR(bp.gacon, 6.70, 0.0, 0.0);
  CHECK_NEAR(bp.gbcon, 0.75, 0.0, 0.0);
  CHECK_NEAR(bp.gccon, 18.0, 0.0, 0.0);
  CHECK_NEAR(bp.dlcon, 0.9, 0.0, 0.0);
  CHECK_NEAR(bp.ctrcon, 1.8, 0.0, 0.0);
  CHECK_NEAR(bp.ctrcex, 3.3, 0.0, 0.0);
  CHECK_NEAR(bp.duxcon, 1.0, 0.0, 0.0);
  CHECK_NEAR(bp.ctcon, 0.5 / (6.70 * 6.70 * 0.75), 0.0, 0.0);
  CHECK_NEAR(bp.cffac, 1.0, 0.0, 0.0);
}

TEST(unit, SessionBlparIndependentAcrossSessionsAndReachesViscal) {
  session::SessionComposition a;
  buildNaca2412Paned(a);
  configure(a, true, 100);
  std::istringstream inA;
  std::ostringstream outA;
  CHECK(app::cli::handleOperAlphaCore(a, inA, outA, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  session::SessionComposition b;
  buildNaca2412Paned(b);
  configure(b, true, 100);
  b.viscous.blpar.cffac = 2.0;
  std::istringstream inB;
  std::ostringstream outB;
  CHECK(app::cli::handleOperAlphaCore(b, inB, outB, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  // session independence: the mutation of b does not affect a.
  CHECK_NEAR(a.viscous.blpar.cffac, 1.0, 0.0, 0.0);
  CHECK_NEAR(b.viscous.blpar.cffac, 2.0, 0.0, 0.0);
  // Propagation: the changed value reaches the viscous point computation
  // (the OFS line differs).
  CHECK(outA.str() != outB.str());
}

TEST(unit, SessionBlparPersistsAcrossPaneProfileInitAndAnalysis) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  composition.viscous.blpar.sccon = 6.0;
  composition.viscous.blpar.ctrcex = 4.0;

  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(composition, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(composition.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.blpar.ctrcex, 4.0, 0.0, 0.0);

  // PANE does not reset BLPAR.
  CHECK(session::applyPaneling(composition).ok);
  CHECK_NEAR(composition.viscous.blpar.sccon, 6.0, 0.0, 0.0);

  // profile change does not reset BLPAR.
  const auto naca = core::geometry::nacaAirfoil(4412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  CHECK(session::applyPaneling(composition).ok);
  CHECK_NEAR(composition.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.blpar.ctrcex, 4.0, 0.0, 0.0);

  // VPAR INIT (LBLINI toggle) does not reset BLPAR.
  session::toggleBoundaryLayerInitialization(composition);
  session::toggleBoundaryLayerInitialization(composition);
  CHECK_NEAR(composition.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.blpar.ctrcex, 4.0, 0.0, 0.0);
}

// session-bound IDAMP state. Fortran default 0
// (INIT, xfoil.f:685); the DAMP toggle stays blocked and the value is
// only mutated internally. Oracle damp_oracle (NACA 2412, PANE, OPER,
// VISC 1e6, alpha 2, DAMP, alpha 2): the second point with IDAMP=1
// converges warm in 2 iterations, completion format 9922
// hkmax 3.5872, cd 0.005779, xoctr 0.5253; baseline IDAMP=0:
// hkmax 3.5895, cd 0.005778, xoctr 0.5256.
TEST(unit, SessionIdampDefaultMatchesFortranInit) {
  session::SessionComposition composition;
  CHECK_EQ(composition.viscous.idamp, 0);
  buildNaca2412Paned(composition);
  CHECK_EQ(composition.viscous.idamp, 0);
}

TEST(unit, SessionIdampIndependentAcrossSessionsAndReachesDampl2) {
  session::SessionComposition base;
  buildNaca2412Paned(base);
  configure(base, true, 100);
  std::istringstream inBase;
  std::ostringstream outBase;
  CHECK(handleOperAlphaCore(base, inBase, outBase, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_EQ(base.viscous.idamp, 0);
  const auto& baseComp = base.result.lastDiagnostics.completion;
  CHECK_NEAR(baseComp.hkmax, 3.5895, 5.0e-5, 0.0);
  CHECK_NEAR(baseComp.cd, 0.005778, 5.0e-7, 0.0);
  CHECK_NEAR(baseComp.xoctr, 0.5256, 5.0e-5, 0.0);

  session::SessionComposition mod;
  buildNaca2412Paned(mod);
  configure(mod, true, 100);
  std::istringstream inWarm;
  std::ostringstream outWarm;
  CHECK(handleOperAlphaCore(mod, inWarm, outWarm, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  // internal state mutation (the DAMP handler does not exist in production).
  mod.viscous.idamp = 1;
  std::istringstream inRun;
  std::ostringstream outRun;
  CHECK(handleOperAlphaCore(mod, inRun, outRun, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const auto& modComp = mod.result.lastDiagnostics.completion;
  CHECK_NEAR(modComp.hkmax, 3.5872, 5.0e-5, 0.0);
  CHECK_NEAR(modComp.cd, 0.005779, 5.0e-7, 0.0);
  CHECK_NEAR(modComp.xoctr, 0.5253, 5.0e-5, 0.0);
  CHECK(mod.result.lastDiagnostics.iterations.size() == std::size_t{2});

  // session independence: the mutation of mod does not affect base.
  CHECK_EQ(base.viscous.idamp, 0);
  CHECK_EQ(mod.viscous.idamp, 1);
  CHECK_NEAR(base.result.lastDiagnostics.completion.hkmax, 3.5895, 5.0e-5,
             0.0);
}

TEST(unit, SessionIdampPersistsAcrossPaneProfileVparInitAndMenus) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  composition.viscous.idamp = 1;

  // PANE does not reset IDAMP.
  CHECK(session::applyPaneling(composition).ok);
  CHECK_EQ(composition.viscous.idamp, 1);

  // profile change does not reset IDAMP.
  const auto naca = core::geometry::nacaAirfoil(4412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(composition, naca.value().buffer)
            .success());
  CHECK(session::applyPaneling(composition).ok);
  CHECK_EQ(composition.viscous.idamp, 1);

  // VPAR INIT (LBLINI toggle) does not reset IDAMP.
  session::toggleBoundaryLayerInitialization(composition);
  session::toggleBoundaryLayerInitialization(composition);
  CHECK_EQ(composition.viscous.idamp, 1);

  // OPER menu cycle (INIT toggle handler) does not reset IDAMP.
  std::ostringstream out;
  const auto first = handleOperInitCore(composition, out);
  CHECK(first == app::cli::OperInitCoreStatus::initializedAssumed ||
        first == app::cli::OperInitCoreStatus::willInitialize);
  const auto second = handleOperInitCore(composition, out);
  CHECK(second == app::cli::OperInitCoreStatus::initializedAssumed ||
        second == app::cli::OperInitCoreStatus::willInitialize);
  CHECK(first != second);
  CHECK_EQ(composition.viscous.idamp, 1);
}

TEST(unit, SessionIdampChangeKeepsStoredSolutionAndNextPointConsumesIt) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  std::istringstream inFirst;
  std::ostringstream outFirst;
  CHECK(handleOperAlphaCore(composition, inFirst, outFirst, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  // Retain the state of the stored point before the mutation.
  const double clBefore = composition.viscous.viscals.cl;
  const double cdBefore = composition.viscous.viscals.cd;
  const auto resultBefore = composition.result.lastResult;
  const bool convergedBefore = composition.viscous.converged;

  composition.viscous.idamp = 1;

  // No invalidation: solution and convergence flags are retained.
  CHECK_NEAR(composition.viscous.viscals.cl, clBefore, 0.0, 0.0);
  CHECK_NEAR(composition.viscous.viscals.cd, cdBefore, 0.0, 0.0);
  CHECK_EQ(composition.viscous.converged, convergedBefore);
  CHECK_NEAR(composition.result.lastResult.liftCoefficient,
             resultBefore.liftCoefficient, 0.0, 0.0);
  CHECK_NEAR(composition.result.lastResult.dragCoefficient,
             resultBefore.dragCoefficient, 0.0, 0.0);

  // the next analysis consumes the changed values.
  std::istringstream inNext;
  std::ostringstream outNext;
  CHECK(handleOperAlphaCore(composition, inNext, outNext, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(composition.result.lastDiagnostics.completion.hkmax, 3.5872,
             5.0e-5, 0.0);
}

TEST(unit, SessionIdampExplicitZeroMatchesDefaultBaseline) {
  session::SessionComposition def;
  buildNaca2412Paned(def);
  configure(def, true, 100);
  std::istringstream inDef;
  std::ostringstream outDef;
  CHECK(handleOperAlphaCore(def, inDef, outDef, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  session::SessionComposition zero;
  buildNaca2412Paned(zero);
  configure(zero, true, 100);
  zero.viscous.idamp = 0;
  std::istringstream inZero;
  std::ostringstream outZero;
  CHECK(handleOperAlphaCore(zero, inZero, outZero, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  CHECK(outDef.str() == outZero.str());
  CHECK_NEAR(zero.result.lastDiagnostics.completion.hkmax,
             def.result.lastDiagnostics.completion.hkmax, 0.0, 0.0);
}

// NT reaches the next viscous computation.
// Fortran oracle nt4_visc (VPAR NT 4.0, VISC 1e6, ALFA 2): 6
// MRCHUE messages (baseline without NT: 10), OFS line as in the oracle.
TEST(unit, SessionNcritTopReachesViscousPointMatchesFortranOracle) {
  session::SessionComposition base;
  buildNaca2412Paned(base);
  configure(base, true, 100);
  std::istringstream inBase;
  std::ostringstream outBase;
  CHECK(app::cli::handleOperAlphaCore(base, inBase, outBase, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  session::SessionComposition nt;
  buildNaca2412Paned(nt);
  configure(nt, true, 100);
  std::istringstream inNt;
  std::ostringstream vparOut;
  CHECK(app::cli::handleVparNCritTopCore(nt, inNt, vparOut, "4.0") ==
        app::cli::VparNCritCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream outNt;
  CHECK(app::cli::handleOperAlphaCore(nt, inRun, outNt, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);

  CHECK(outNt.str() != outBase.str());
  CHECK_EQ(countOccurrences(outNt.str(), "MRCHUE"), std::size_t{6});
  CHECK(outNt.str().find("OFS:") == std::string::npos);
  CHECK_NEAR(nt.operating.criticalReynoldsExponents[0], 4.0, 0.0, 0.0);
  CHECK_NEAR(nt.operating.criticalReynoldsExponents[1], 9.0, 0.0, 0.0);
}

// NB analogous: only the lower side is reduced.
TEST(unit, SessionNcritBotReachesViscousPointAndKeepsTopValue) {
  session::SessionComposition nb;
  buildNaca2412Paned(nb);
  configure(nb, true, 100);
  std::istringstream inNb;
  std::ostringstream vparOut;
  CHECK(app::cli::handleVparNCritBotCore(nb, inNb, vparOut, "4.0") ==
        app::cli::VparNCritCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream outNb;
  CHECK(app::cli::handleOperAlphaCore(nb, inRun, outNb, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK_NEAR(nb.operating.criticalReynoldsExponents[0], 9.0, 0.0, 0.0);
  CHECK_NEAR(nb.operating.criticalReynoldsExponents[1], 4.0, 0.0, 0.0);
  session::SessionComposition base;
  buildNaca2412Paned(base);
  configure(base, true, 100);
  std::istringstream inBase;
  std::ostringstream outBase;
  CHECK(app::cli::handleOperAlphaCore(base, inBase, outBase, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(outNb.str() != outBase.str());
}

// WAKE acts on the next viscous computation.
// Fortran oracle: base rms 0.1272E+00, WAKE 2.0 rms 0.1260E+00,
// WAKE 0.5 rms 0.1279E+00 (first BL iteration line, wake2_visc /
// wake05_visc / base_visc).
TEST(unit, SessionWakeGrowAndShrinkMatchFortranRmsOracle) {
  session::SessionComposition base;
  buildNaca2412Paned(base);
  configure(base, true, 100);
  std::istringstream inBase;
  std::ostringstream outBase;
  CHECK(app::cli::handleOperAlphaCore(base, inBase, outBase, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(outBase.str().find("rms: 0.1272E+00") != std::string::npos);

  session::SessionComposition grow;
  buildNaca2412Paned(grow);
  configure(grow, true, 100);
  std::istringstream inWake;
  std::ostringstream wakeOut;
  CHECK(app::cli::handleVparWakeCore(grow, inWake, wakeOut, "2.0") ==
        app::cli::VparWakeCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream outGrow;
  CHECK(app::cli::handleOperAlphaCore(grow, inRun, outGrow, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(outGrow.str().find("rms: 0.1260E+00") != std::string::npos);

  session::SessionComposition shrink;
  buildNaca2412Paned(shrink);
  configure(shrink, true, 100);
  std::istringstream inWake2;
  std::ostringstream wakeOut2;
  CHECK(app::cli::handleVparWakeCore(shrink, inWake2, wakeOut2, "0.5") ==
        app::cli::VparWakeCoreStatus::applied);
  std::istringstream inRun2;
  std::ostringstream outShrink;
  CHECK(app::cli::handleOperAlphaCore(shrink, inRun2, outShrink, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(outShrink.str().find("rms: 0.1279E+00") != std::string::npos);
}

// NW contract: NW = N/12 + 10*INT(WAKLEN) (xpanel.f 1280),
// integer truncation toward zero.
TEST(unit, SessionWakeNwMatchesFortranIntegerSemantics) {
  // 0.0 stays outside as the full-generation case: Fortran returns
  // NaN/non-convergence (wake0_visc) and no defined value.
  for (const double waklen : {0.9, 1.0, 1.9, 2.0, 3.0}) {
    session::SessionComposition composition;
    buildNaca2412Paned(composition);
    configure(composition, true, 100);
    std::istringstream inWake;
    std::ostringstream wakeOut;
    std::ostringstream waklenText;
    waklenText << waklen;
    CHECK(app::cli::handleVparWakeCore(composition, inWake, wakeOut,
                                       waklenText.str()) ==
          app::cli::VparWakeCoreStatus::applied);
    std::istringstream inRun;
    std::ostringstream outRun;
    CHECK(app::cli::handleOperAlphaCore(composition, inRun, outRun, "2") ==
          app::cli::OperAlphaCoreStatus::analyzed);
    const std::size_t n = composition.paneling.panel.x.size() - 1;
    const std::size_t expected =
        n / 12 + 10 * static_cast<std::size_t>(static_cast<int>(waklen));
    CHECK_EQ(composition.viscous.wake.nw, expected);
    CHECK(composition.viscous.wakeValid);
    CHECK(composition.viscous.viscals.mapping.valid);
  }
}

// IPAN tail contract: a shrinking WAKLEN overwrites only the
// new active range; tail values are preserved.
TEST(unit, SessionWakeShrinkPreservesIpanTail) {
  session::SessionComposition composition;
  buildNaca2412Paned(composition);
  configure(composition, true, 100);
  std::istringstream inWake;
  std::ostringstream wakeOut;
  CHECK(app::cli::handleVparWakeCore(composition, inWake, wakeOut, "3.0") ==
        app::cli::VparWakeCoreStatus::applied);
  std::istringstream inRun1;
  std::ostringstream outRun1;
  CHECK(app::cli::handleOperAlphaCore(composition, inRun1, outRun1, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<int> ipanBefore = composition.viscous.viscals.mapping.top.ipan;
  const int iblteBefore = composition.viscous.viscals.mapping.top.iblte;
  CHECK(!ipanBefore.empty());

  CHECK(app::cli::handleVparWakeCore(composition, inWake, wakeOut, "0.5") ==
        app::cli::VparWakeCoreStatus::applied);
  std::istringstream inRun2;
  std::ostringstream outRun2;
  CHECK(app::cli::handleOperAlphaCore(composition, inRun2, outRun2, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const std::vector<int>& ipanAfter = composition.viscous.viscals.mapping.top.ipan;
  CHECK_EQ(ipanAfter.size(), ipanBefore.size());
  const int iblteAfter = composition.viscous.viscals.mapping.top.iblte;
  CHECK(iblteAfter <= iblteBefore);
  // tail beyond the new TE range stays preserved.
  for (std::size_t i = static_cast<std::size_t>(iblteAfter) + 1;
       i < ipanAfter.size(); ++i) {
    CHECK_EQ(ipanAfter[i], ipanBefore[i]);
  }
}

// --- VPAR LAG/GB/CTR/CFAC on the session BLPAR state -------
// Fortran oracle (LC_ALL=C, fortran-bin/xfoil, per 3x md5-identical):
// base_visc rms 0.1272E+00, CL 0.4496, CD 0.00578, CDf 0.00412, CDp 0.00166
// lag_visc rms 0.1282E+00, CL 0.4476, CD 0.00576, CDf 0.00412, CDp 0.00165
// gb_visc rms 0.1302E+00, CL 0.4499, CD 0.00570, CDf 0.00404, CDp 0.00167
// ctr_visc rms 0.1283E+00, CL 0.4515, CD 0.00576, CDf 0.00410, CDp 0.00166
// cfac_visc rms 0.1536E+00, CL 0.4322, CD 0.00697, CDf 0.00497, CDp 0.00201

namespace {

std::array<std::uint64_t, 10> p6p3BlparBits(
    const core::boundary_layer::BoundaryLayerParameters& p) {
  const double fields[10] = {p.sccon, p.gacon, p.gbcon, p.gccon, p.dlcon,
                             p.ctrcon, p.ctrcex, p.duxcon, p.ctcon,
                             p.cffac};
  std::array<std::uint64_t, 10> bits{};
  for (std::size_t i = 0; i < 10; ++i) {
    std::memcpy(&bits[i], &fields[i], sizeof(double));
  }
  return bits;
}

std::size_t p6p3VectorHash(const std::vector<double>& v) {
  std::size_t h = v.size();
  for (double d : v) {
    std::uint64_t u{};
    std::memcpy(&u, &d, sizeof(double));
    h ^= u + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  }
  return h;
}

struct P6p3SolutionSnapshot {
  std::array<std::uint64_t, 10> blpar{};
  bool lvconv{};
  bool converged{};
  bool lblini{};
  bool wakeValid{};
  bool mappingValid{};
  double cl{};
  double cd{};
  double cm{};
  std::size_t thetTop{};
  std::size_t ctauTop{};
  std::size_t dstrBot{};
};

P6p3SolutionSnapshot p6p3Snapshot(const session::SessionComposition& s) {
  P6p3SolutionSnapshot snap;
  snap.blpar = p6p3BlparBits(s.viscous.blpar);
  snap.lvconv = s.viscous.viscals.lvconv;
  snap.converged = s.viscous.converged;
  snap.lblini = s.viscous.viscals.lblini;
  snap.wakeValid = s.viscous.wakeValid;
  snap.mappingValid = s.viscous.viscals.mapping.valid;
  snap.cl = s.result.lastResult.liftCoefficient;
  snap.cd = s.result.lastResult.dragCoefficient;
  snap.cm = s.result.lastResult.momentCoefficient;
  snap.thetTop = p6p3VectorHash(s.viscous.viscals.thet.top);
  snap.ctauTop = p6p3VectorHash(s.viscous.viscals.ctau.top);
  snap.dstrBot = p6p3VectorHash(s.viscous.viscals.dstr.bottom);
  return snap;
}

}  // namespace

// Use-case level: each session operation writes exclusively its
// BLPAR fields; the remaining seven stay bit-identical; no
// validity flag and no operating state is touched.
TEST(unit, SessionLagGbCtrCfacUseCasesChangeOnlyTheirFields) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream in;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(s, in, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const auto before = p6p3BlparBits(s.viscous.blpar);
  const bool lvconv = s.viscous.viscals.lvconv;
  const bool converged = s.viscous.converged;

  session::setShearLagConstant(s, 6.0);
  session::setUxEqWeight(s, 0.5);
  session::setWakeDissipationLengthFactor(s, 1.5);
  session::setGbConstantA(s, 7.0);
  session::setGbConstantBAndCtcon(s, 0.8);
  session::setInitialCtauConstant(s, 2.0);
  session::setInitialCtauExponent(s, 4.0);
  session::setBoundaryLayerCfScale(s, 2.0);

  const auto after = p6p3BlparBits(s.viscous.blpar);
  // only GCCON (Index 3) stays bit-identical.
  CHECK_EQ(after[3], before[3]);
  for (std::size_t i : {0u, 1u, 2u, 4u, 5u, 6u, 7u, 8u, 9u}) {
    CHECK(after[i] != before[i]);
  }
  CHECK_EQ(after[8], [&] { double v = 0.5 / (7.0 * 7.0 * 0.8);
                          std::uint64_t u{}; std::memcpy(&u, &v, 8);
                          return u; }());
  CHECK_EQ(s.viscous.viscals.lvconv, lvconv);
  CHECK_EQ(s.viscous.converged, converged);
}

// persistence: handler mutations survive PANE, profile change,
// VPAR INIT toggle and a viscous computation unchanged.
TEST(unit, SessionLagGbCtrCfacPersistAcrossPaneProfileInitAndAnalysis) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  const auto expected = p6p3BlparBits(s.viscous.blpar);

  std::istringstream inRun;
  std::ostringstream runOut;
  CHECK(app::cli::handleOperAlphaCore(s, inRun, runOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  CHECK(session::applyPaneling(s).ok);
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  const auto naca = core::geometry::nacaAirfoil(4412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(s, naca.value().buffer).success());
  CHECK(session::applyPaneling(s).ok);
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  session::toggleBoundaryLayerInitialization(s);
  session::toggleBoundaryLayerInitialization(s);
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);
}

// LAG effect: SCCON/DUXCON/DLCON reach the next turbulent
// BL/wake computation (consumer xblsys.f 1757/1769-1790/1701).
TEST(unit, SessionLagReachesViscousPointMatchesFortranOracle) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(s, inRun, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(out.str().find("rms: 0.1282E+00") != std::string::npos);
  CHECK(out.str().find("CL =  0.4476") != std::string::npos);
  CHECK(out.str().find("CD =  0.00576") != std::string::npos);
  CHECK(out.str().find("CDp =  0.00165") != std::string::npos);
}

// GB effect: GACON/GBCON (xblsys.f 1724-1728, 822-825) and the
// stored CTCON (xblsys.f 874-881) reach the consumer;
// GCCON stays unchanged.
TEST(unit, SessionGbReachesViscousPointMatchesFortranOracle) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK_NEAR(s.viscous.blpar.gccon, 18.0, 0.0, 0.0);
  std::istringstream inRun;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(s, inRun, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(out.str().find("rms: 0.1302E+00") != std::string::npos);
  CHECK(out.str().find("CL =  0.4499") != std::string::npos);
  CHECK(out.str().find("CDf =  0.00404") != std::string::npos);
}

// CTR effect: CTRCON/CTRCEX reach the initial Ctau path
// (xblsys.f 1391-1392, EXP and power structure).
TEST(unit, SessionCtrReachesViscousPointMatchesFortranOracle) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(s, inRun, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(out.str().find("rms: 0.1283E+00") != std::string::npos);
  CHECK(out.str().find("CL =  0.4515") != std::string::npos);
  CHECK(out.str().find("CDf =  0.00410") != std::string::npos);
}

// CFAC effect: CFFAC scales the direct Cf consumer CFO
// (xblsys.f 2502/blu.f 121); CDf increases and downstream Cl/CM react
// physically.
TEST(unit, SessionCfacReachesCfoCftMatchesFortranOracle) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream out;
  CHECK(app::cli::handleOperAlphaCore(s, inRun, out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(out.str().find("rms: 0.1536E+00") != std::string::npos);
  CHECK(out.str().find("CDf =  0.00497") != std::string::npos);
  CHECK(out.str().find("CL =  0.4322") != std::string::npos);
}

// No immediate back-effect: a pure parameter change without
// new analysis leaves the stored solution (lastResult, BL-
// station arrays, convergence status, wake/mapping flags) bit-identical;
// only the new computation uses the new values.
TEST(unit, SessionBlparParameterChangeKeepsExistingSolution) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inBase;
  std::ostringstream baseOut;
  CHECK(app::cli::handleOperAlphaCore(s, inBase, baseOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const P6p3SolutionSnapshot before = p6p3Snapshot(s);
  CHECK(before.lvconv);
  CHECK(before.converged);

  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);

  const P6p3SolutionSnapshot after = p6p3Snapshot(s);
  CHECK_EQ(after.lvconv, before.lvconv);
  CHECK_EQ(after.converged, before.converged);
  CHECK_EQ(after.lblini, before.lblini);
  CHECK_EQ(after.wakeValid, before.wakeValid);
  CHECK_EQ(after.mappingValid, before.mappingValid);
  CHECK_EQ(after.cl, before.cl);
  CHECK_EQ(after.cd, before.cd);
  CHECK_EQ(after.cm, before.cm);
  CHECK_EQ(after.thetTop, before.thetTop);
  CHECK_EQ(after.ctauTop, before.ctauTop);
  CHECK_EQ(after.dstrBot, before.dstrBot);

  // only a new computation reacts. Cold-start anchor against the
  // combined Fortran oracle run seq_visc (fresh airfoil, only
  // LAG+GB+CTR+CFAC, then VISC 1e6 + alpha 2): rms 0.1682E+00,
  // CL 0.4304, CDf 0.00480.
  session::SessionComposition cold;
  buildNaca2412Paned(cold);
  configure(cold, true, 100);
  std::istringstream inColdMut;
  std::ostringstream coldMutOut;
  CHECK(app::cli::handleVparLagCore(cold, inColdMut, coldMutOut,
                                    "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(cold, inColdMut, coldMutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(cold, inColdMut, coldMutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(cold, inColdMut, coldMutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  std::istringstream inRun;
  std::ostringstream runOut;
  CHECK(app::cli::handleOperAlphaCore(cold, inRun, runOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(runOut.str().find("rms: 0.1682E+00") != std::string::npos);
  CHECK(runOut.str().find("CL =  0.4304") != std::string::npos);
  CHECK(runOut.str().find("CDf =  0.00480") != std::string::npos);
  CHECK(baseOut.str().find("CL =  0.4304") == std::string::npos);
}

// VPAR REST according to
// xoper.f 2709-2715; BLPINI xbl.f 1578-1596.

TEST(unit, SessionRestRestoresAllTenBlparBits) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  s.viscous.blpar.gccon = 99.0;  // GCCON: no VPAR set command, internal mutation
  const auto mutated = p6p3BlparBits(s.viscous.blpar);
  std::ostringstream out;
  CHECK(app::cli::handleVparRestCore(s, out) ==
        app::cli::VparRestCoreStatus::applied);
  CHECK(out.str().empty());
  CHECK(p6p3BlparBits(s.viscous.blpar) ==
        p6p3BlparBits(core::boundary_layer::blparConstants()));
  CHECK(mutated != p6p3BlparBits(s.viscous.blpar));
}

// cold restart analogous to Fortran oracle all_rest_visc: only mutations, then
// REST, then the first viscous computation -> fallback-free on the default
// baseline (base_visc: rms 0.1272E+00, CL 0.4496, CD 0.00578, CDf 0.00412).
TEST(unit, SessionColdRestBeforeFirstRunMatchesDefaultOracle) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  std::ostringstream restOut;
  CHECK(app::cli::handleVparRestCore(s, restOut) ==
        app::cli::VparRestCoreStatus::applied);
  CHECK(restOut.str().empty());
  std::istringstream inRun;
  std::ostringstream runOut;
  CHECK(app::cli::handleOperAlphaCore(s, inRun, runOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(runOut.str().find("rms: 0.1272E+00") != std::string::npos);
  CHECK(runOut.str().find("CL =  0.4496") != std::string::npos);
  CHECK(runOut.str().find("CD =  0.00578") != std::string::npos);
  CHECK(runOut.str().find("CDf =  0.00412") != std::string::npos);
}

// REST changed exclusively BLPAR: convergence flags, lastResult,
// BL station arrays, wake/mapping validity remain bit-identical.
TEST(unit, SessionRestKeepsExistingSolutionSnapshot) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inBase;
  std::ostringstream baseOut;
  CHECK(app::cli::handleOperAlphaCore(s, inBase, baseOut, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  const P6p3SolutionSnapshot before = p6p3Snapshot(s);
  CHECK(before.lvconv);
  CHECK(before.converged);

  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  const auto mutated = p6p3BlparBits(s.viscous.blpar);
  CHECK(mutated != before.blpar);
  std::ostringstream restOut;
  CHECK(app::cli::handleVparRestCore(s, restOut) ==
        app::cli::VparRestCoreStatus::applied);

  const P6p3SolutionSnapshot after = p6p3Snapshot(s);
  CHECK(after.blpar == p6p3BlparBits(core::boundary_layer::blparConstants()));
  // Return exactly to the default state valid before the mutation.
  CHECK(after.blpar == before.blpar);
  CHECK_EQ(after.lvconv, before.lvconv);
  CHECK_EQ(after.converged, before.converged);
  CHECK_EQ(after.lblini, before.lblini);
  CHECK_EQ(after.wakeValid, before.wakeValid);
  CHECK_EQ(after.mappingValid, before.mappingValid);
  CHECK_EQ(after.cl, before.cl);
  CHECK_EQ(after.cd, before.cd);
  CHECK_EQ(after.cm, before.cm);
  CHECK_EQ(after.thetTop, before.thetTop);
  CHECK_EQ(after.ctauTop, before.ctauTop);
  CHECK_EQ(after.dstrBot, before.dstrBot);
}

// No invalidation contract: the solution stored with mutated parameters
// stays bit-identical after REST; only the next computation uses the
// BLPINI defaults again.
TEST(unit, SessionRestAfterMutatedRunKeepsMutatedSolution) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK(app::cli::handleVparGbCore(s, inMut, mutOut, "7.0 0.8") ==
        app::cli::VparGbCoreStatus::applied);
  CHECK(app::cli::handleVparCtrCore(s, inMut, mutOut, "2.0 4.0") ==
        app::cli::VparCtrCoreStatus::applied);
  CHECK(app::cli::handleVparCfacCore(s, inMut, mutOut, "2.0") ==
        app::cli::VparCfacCoreStatus::applied);
  std::istringstream inRun1;
  std::ostringstream run1Out;
  CHECK(app::cli::handleOperAlphaCore(s, inRun1, run1Out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(run1Out.str().find("rms: 0.1682E+00") != std::string::npos);
  CHECK(run1Out.str().find("CL =  0.4304") != std::string::npos);
  const P6p3SolutionSnapshot mutated = p6p3Snapshot(s);

  std::ostringstream restOut;
  CHECK(app::cli::handleVparRestCore(s, restOut) ==
        app::cli::VparRestCoreStatus::applied);
  const P6p3SolutionSnapshot afterRest = p6p3Snapshot(s);
  CHECK(afterRest.blpar ==
        p6p3BlparBits(core::boundary_layer::blparConstants()));
  CHECK_EQ(afterRest.cl, mutated.cl);
  CHECK_EQ(afterRest.cd, mutated.cd);
  CHECK_EQ(afterRest.cm, mutated.cm);
  CHECK_EQ(afterRest.thetTop, mutated.thetTop);
  CHECK_EQ(afterRest.ctauTop, mutated.ctauTop);
  CHECK_EQ(afterRest.dstrBot, mutated.dstrBot);
  CHECK_EQ(afterRest.lvconv, mutated.lvconv);
  CHECK_EQ(afterRest.converged, mutated.converged);

  std::istringstream inRun2;
  std::ostringstream run2Out;
  CHECK(app::cli::handleOperAlphaCore(s, inRun2, run2Out, "2") ==
        app::cli::OperAlphaCoreStatus::analyzed);
  CHECK(run2Out.str().find("CL =  0.4496") != std::string::npos);
  CHECK(run2Out.str().find("CDf =  0.00412") != std::string::npos);
}

// persistence after REST: the default state survives PANE, profile change
// and INIT toggle; sessions are independent.
TEST(unit, SessionRestPersistsAcrossPaneProfileInitAndIndependentSessions) {
  session::SessionComposition s;
  buildNaca2412Paned(s);
  configure(s, true, 100);
  std::istringstream inMut;
  std::ostringstream mutOut;
  CHECK(app::cli::handleVparLagCore(s, inMut, mutOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  std::ostringstream restOut;
  CHECK(app::cli::handleVparRestCore(s, restOut) ==
        app::cli::VparRestCoreStatus::applied);
  const auto expected = p6p3BlparBits(core::boundary_layer::blparConstants());
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  CHECK(session::applyPaneling(s).ok);
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  const auto naca = core::geometry::nacaAirfoil(4412, false, 123);
  CHECK(naca.isOk());
  CHECK(session::setBufferGeometry(s, naca.value().buffer).success());
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  session::toggleBoundaryLayerInitialization(s);
  session::toggleBoundaryLayerInitialization(s);
  CHECK(p6p3BlparBits(s.viscous.blpar) == expected);

  session::SessionComposition other;
  buildNaca2412Paned(other);
  configure(other, true, 100);
  std::istringstream inOther;
  std::ostringstream otherOut;
  CHECK(app::cli::handleVparLagCore(other, inOther, otherOut, "6.0 0.5 1.5") ==
        app::cli::VparLagCoreStatus::applied);
  CHECK_NEAR(other.viscous.blpar.sccon, 6.0, 0.0, 0.0);
  CHECK_NEAR(s.viscous.blpar.sccon, 5.6, 0.0, 0.0);
}

// end: tests/unit/CoreOperSinglePointHandlerTests.cpp

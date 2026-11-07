@echo off
rem XFOIL.next
rem
rem Copyright (C) 2026 Olaf Frommann
rem https://github.com/of33/XFOIL.next
rem
rem XFOIL.next is based on XFOIL 6.99 by Mark Drela and
rem Harold Youngren.
rem
rem This program is free software: you can redistribute it and/or modify
rem it under the terms of the GNU General Public License as published by
rem the Free Software Foundation, either version 3 of the License, or
rem (at your option) any later version.
rem
rem This program is distributed in the hope that it will be useful,
rem but WITHOUT ANY WARRANTY; without even the implied warranty of
rem MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
rem GNU General Public License for more details.
rem
rem You should have received a copy of the GNU General Public License
rem along with this program. If not, see <https://www.gnu.org/licenses/>.
rem
rem SPDX-License-Identifier: GPL-3.0-or-later

rem Reproducible Windows (MSYS2 UCRT64) build and evidence run.
rem
rem Named options only, no positional arguments:
rem
rem     tools\windows\build-and-test-msys2.bat ^
rem       --msys2-root D:\Programme\msys64 ^
rem       --log-dir D:\xfoil-next-audit\pub-w5-run1
rem
rem --msys2-root optional; fallback order: this option, MSYS2_ROOT,
rem C:\msys64 (common default only, not a required location).
rem --log-dir required. It must lie OUTSIDE this repository; logs are
rem never written into the repository. A timestamped run subdirectory
rem is created inside --log-dir, so existing logs are never overwritten.
rem
rem The batch entry point validates its arguments and then runs
rem tools\windows\build-and-test-msys2.sh with
rem <MSYS2>\usr\bin\bash.exe -l (the MSYS2 bash; MSYSTEM=UCRT64 selects
rem the UCRT64 subsystem), non-interactive and without a new window.
rem Exit code 0 only if every mandatory step passed.

set "XR_MSYS2="
set "XR_LOGDIR="

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--msys2-root" (
  if "%~2"=="" (
    echo [build-and-test-msys2] ERROR: --msys2-root requires a value 1>&2
    exit /b 2
  )
  set "XR_MSYS2=%~2"
  shift
  shift
  goto parse
)
if /i "%~1"=="--log-dir" (
  if "%~2"=="" (
    echo [build-and-test-msys2] ERROR: --log-dir requires a value 1>&2
    exit /b 2
  )
  set "XR_LOGDIR=%~2"
  shift
  shift
  goto parse
)
echo [build-and-test-msys2] ERROR: unknown argument: %~1 1>&2
echo Usage: build-and-test-msys2.bat [--msys2-root PATH] --log-dir PATH 1>&2
exit /b 2

:parsed

if not defined XR_MSYS2 if defined MSYS2_ROOT set "XR_MSYS2=%MSYS2_ROOT%"
if not defined XR_MSYS2 set "XR_MSYS2=C:\msys64"

if not defined XR_LOGDIR (
  echo [build-and-test-msys2] ERROR: --log-dir is required 1>&2
  echo Usage: build-and-test-msys2.bat [--msys2-root PATH] --log-dir PATH 1>&2
  exit /b 2
)

rem Deliberately no %%~f normalization here: in some command
rem environments it resolved against the wrong current directory.
rem The helper script performs the authoritative path normalization
rem (cygpath/pwd), absolute or relative, and also rejects a log
rem directory inside the repository.

if not exist "%XR_MSYS2%" (
  echo [build-and-test-msys2] ERROR: MSYS2 root not found: "%XR_MSYS2%" 1>&2
  exit /b 1
)
if not exist "%XR_MSYS2%\usr\bin\bash.exe" (
  echo [build-and-test-msys2] ERROR: MSYS2 bash not found: "%XR_MSYS2%\usr\bin\bash.exe" 1>&2
  echo   Is "%XR_MSYS2%" an MSYS2 installation root? 1>&2
  exit /b 1
)
if not exist "%XR_MSYS2%\ucrt64\bin" (
  echo [build-and-test-msys2] ERROR: UCRT64 subsystem not found: "%XR_MSYS2%\ucrt64\bin" 1>&2
  exit /b 1
)

rem Locate the helper script: first next to this batch file, then
rem relative to the current directory (invocation from the repository
rem root). The repository root is derived from whichever was found.
set "XR_SCRIPT=%~dpn0.sh"
if exist "%XR_SCRIPT%" (
  set "XR_REPO=%~dp0..\.."
  goto scriptfound
)
set "XR_SCRIPT=%CD%\tools\windows\build-and-test-msys2.sh"
if exist "%XR_SCRIPT%" (
  set "XR_REPO=%CD%"
  goto scriptfound
)
echo [build-and-test-msys2] ERROR: helper script missing: "%~dpn0.sh" 1>&2
echo   and also not found at: "%CD%\tools\windows\build-and-test-msys2.sh" 1>&2
exit /b 1

:scriptfound

rem UCRT64 environment for the login shell. bash.exe lives in
rem <MSYS2>\usr\bin (not ucrt64\bin); MSYSTEM selects the UCRT64
rem subsystem for /etc/profile. The script is passed directly as a
rem program argument (quoted paths, no -c command string), so no
rem shell expansion can turn it into a relative path.
set "MSYSTEM=UCRT64"
set "PATH=%XR_MSYS2%\ucrt64\bin;%XR_MSYS2%\usr\bin;%PATH%"

"%XR_MSYS2%\usr\bin\bash.exe" -l "%XR_SCRIPT%" "%XR_LOGDIR%" "%XR_REPO%"
set "XR_RC=%ERRORLEVEL%"
if not "%XR_RC%"=="0" (
  echo [build-and-test-msys2] FAILED with exit code %XR_RC% 1>&2
  exit /b %XR_RC%
)
echo [build-and-test-msys2] Overall: PASS
exit /b 0

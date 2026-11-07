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

rem Purpose: prepend an MSYS2 UCRT64 runtime directory to PATH so that
rem xfoil.next.exe finds the UCRT64 runtime DLLs.
rem
rem Invoke with CALL so that the PATH change remains visible in the
rem calling process (cmd.exe):
rem
rem     call tools\windows\xfoil-next-env.bat
rem     call tools\windows\xfoil-next-env.bat D:\Programme\msys64
rem     set MSYS2_ROOT=D:\Programme\msys64
rem     call tools\windows\xfoil-next-env.bat
rem
rem From PowerShell, run it in cmd and keep the environment interactively:
rem
rem     cmd /k "call tools\windows\xfoil-next-env.bat D:\Programme\msys64"
rem
rem Path selection order: 1) first argument, 2) MSYS2_ROOT if set,
rem 3) fallback C:\msys64. The fallback is only a common default attempt,
rem not a required installation location.
rem
rem This script installs nothing, downloads nothing and builds nothing.
rem It exits 0 on success and non-zero on failure. It deliberately uses no
rem setlocal, so the PATH change survives the call.

if not "%~1"=="" (
  set "XFOILNEXT_MSYS2=%~1"
) else if defined MSYS2_ROOT (
  set "XFOILNEXT_MSYS2=%MSYS2_ROOT%"
) else (
  set "XFOILNEXT_MSYS2=C:\msys64"
)

rem Normalize to a full path (strips trailing backslashes, resolves ".\";
rem handles spaces and any drive letter).
for %%A in ("%XFOILNEXT_MSYS2%") do set "XFOILNEXT_MSYS2=%%~fA"

if not exist "%XFOILNEXT_MSYS2%" (
  echo [xfoil-next-env] ERROR: MSYS2 root not found: "%XFOILNEXT_MSYS2%" 1>&2
  set "XFOILNEXT_MSYS2="
  exit /b 1
)
if not exist "%XFOILNEXT_MSYS2%\ucrt64\bin" (
  echo [xfoil-next-env] ERROR: UCRT64 bin directory not found: "%XFOILNEXT_MSYS2%\ucrt64\bin" 1>&2
  set "XFOILNEXT_MSYS2="
  exit /b 2
)

set "MSYS2_ROOT=%XFOILNEXT_MSYS2%"

rem Prepend ucrt64\bin to PATH, avoiding a duplicate entry.
echo ;%PATH%; | findstr /i /c:";%XFOILNEXT_MSYS2%\ucrt64\bin;" >nul
if errorlevel 1 (
  set "PATH=%XFOILNEXT_MSYS2%\ucrt64\bin;%PATH%"
  echo [xfoil-next-env] PATH prepended: "%XFOILNEXT_MSYS2%\ucrt64\bin"
) else (
  echo [xfoil-next-env] PATH already contained: "%XFOILNEXT_MSYS2%\ucrt64\bin"
)
set "XFOILNEXT_MSYS2="
exit /b 0

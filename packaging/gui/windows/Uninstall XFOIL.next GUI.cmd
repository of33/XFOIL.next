@echo off
setlocal
set "SCRIPT=%~dp0Uninstall-XFOIL.next-GUI.ps1"
cd /d "%TEMP%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" %*
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
    echo.
    pause
)
endlocal & exit /b %RC%

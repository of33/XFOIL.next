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

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$QtRoot = "",

    [switch]$Help
)

$ExpectedVersion = "6.12.0"
$IsDotSourced = $MyInvocation.InvocationName -eq "."

function Show-GuiSetupUsage {
    Write-Host @"
Usage:
  . .\scripts\setup-gui-build.ps1 -QtRoot 'C:\Qt\6.12.0\mingw_64'

Validates a Qt $ExpectedVersion installation and prepares QT_ROOT, PATH and
QT_PLUGIN_PATH for the current PowerShell process. It never installs Qt,
downloads anything, requests elevation, changes machine settings, or edits a
PowerShell profile.

If -QtRoot is omitted, QT_ROOT is used. A unique valid kit below
%USERPROFILE%\Qt, C:\Qt, or C:\msys64\ucrt64 may be selected. Multiple valid
kits are rejected.

The supported standalone GUI build uses the MSYS2 UCRT64 shell and GNU Make.
Use this PowerShell helper when you need the Qt variables in a PowerShell
child-process environment; for a normal build, open MSYS2 UCRT64 and use
scripts/setup-gui-build.sh instead.

After the environment is prepared, run in MSYS2 UCRT64:
  make gui-check-qt
  make gui-app
  make test-gui
"@
}

function Find-GuiQtTool {
    param(
        [string]$Root,
        [string]$Qmake,
        [string]$Name
    )
    $directories = @(
        (Join-Path $Root "bin"),
        (Join-Path $Root "libexec"),
        (Join-Path $Root "share\qt6\bin")
    )
    foreach ($key in @("QT_HOST_BINS", "QT_INSTALL_LIBEXECS", "QT_INSTALL_BINS")) {
        $value = (& $Qmake -query $key 2>$null | Out-String).Trim()
        if ($value) { $directories += $value }
    }
    foreach ($directory in $directories) {
        foreach ($candidate in @($Name, "$Name.exe")) {
            $path = Join-Path $directory $candidate
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                return (Resolve-Path -LiteralPath $path).Path
            }
        }
    }
    $message = ("Required Qt tool '{0}' was not found.`n" +
                "  searched: {1}`n  expected: Qt {2} {0} tool`n" +
                "  fix: set -QtRoot to a complete Qt {2} kit") -f
        $Name, ($directories -join "; "), $ExpectedVersion
    throw $message
}

function Assert-GuiQtToolVersion {
    param([string]$Path, [string]$Name, [string]$Root)
    $option = if ($Name -eq "rcc") { "--version" } else { "-v" }
    $output = (& $Path $option 2>&1 | Out-String).Trim()
    if ($output -match [regex]::Escape($ExpectedVersion)) { return }
    # Some Windows Qt tool builds do not print a version. Their path inside
    # the already version-checked kit is then the available metadata.
    if (-not $output -and $Path.StartsWith(
            $Root.TrimEnd("\") + "\", [StringComparison]::OrdinalIgnoreCase)) {
        return
    }
    $message = ("Qt tool version mismatch.`n  found: {0} => {1}`n" +
                "  expected: Qt {2}`n  fix: set -QtRoot to Qt {2}") -f
        $Path, $output, $ExpectedVersion
    throw $message
}

function Test-GuiQtRoot {
    param([string]$Root)
    $fullRoot = [System.IO.Path]::GetFullPath($Root)
    if (-not (Test-Path -LiteralPath $fullRoot -PathType Container)) {
        $message = ("Qt root does not exist.`n  searched: {0}`n" +
                    "  expected: an existing Qt {1} kit`n" +
                    "  fix: pass -QtRoot with the kit prefix") -f
            $fullRoot, $ExpectedVersion
        throw $message
    }
    $qmake = $null
    foreach ($candidate in @("qmake6.exe", "qmake.exe")) {
        $path = Join-Path $fullRoot "bin\$candidate"
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $qmake = (Resolve-Path -LiteralPath $path).Path
            break
        }
    }
    if (-not $qmake) {
        $message = ("qmake was not found in the Qt root.`n" +
                    "  searched: {0}`n  expected: bin\qmake6.exe or bin\qmake.exe`n" +
                    "  fix: set -QtRoot to a complete Qt {1} MinGW kit") -f
            (Join-Path $fullRoot "bin"), $ExpectedVersion
        throw $message
    }
    $version = (& $qmake -query QT_VERSION 2>$null | Out-String).Trim()
    if ($version -ne $ExpectedVersion) {
        $message = ("Unsupported Qt version.`n  found: {0}`n  expected: {1}`n" +
                    "  fix: set -QtRoot to a Qt {1} kit") -f
            ($(if ($version) { $version } else { "unknown" })), $ExpectedVersion
        throw $message
    }
    $xspec = (& $qmake -query QMAKE_XSPEC 2>$null | Out-String).Trim()
    if ($xspec -notlike "win32-g++*") {
        $message = ("The Qt kit is not a Windows MinGW kit.`n" +
                    "  found: {0}`n  expected: QMAKE_XSPEC=win32-g++`n" +
                    "  fix: use the MSYS2 UCRT64/MinGW Qt kit; MSVC kits are not supported") -f
            ($(if ($xspec) { $xspec } else { "unknown" }))
        throw $message
    }
    if ($env:MSYSTEM -and $env:MSYSTEM -ne "UCRT64") {
        $message = ("The active MSYS2 environment is not UCRT64.`n" +
                    "  found: {0}`n  expected: MSYSTEM=UCRT64`n" +
                    "  fix: open an MSYS2 UCRT64 shell") -f $env:MSYSTEM
        throw $message
    }

    $moc = Find-GuiQtTool -Root $fullRoot -Qmake $qmake -Name "moc"
    $uic = Find-GuiQtTool -Root $fullRoot -Qmake $qmake -Name "uic"
    $rcc = Find-GuiQtTool -Root $fullRoot -Qmake $qmake -Name "rcc"
    Assert-GuiQtToolVersion -Path $moc -Name "moc" -Root $fullRoot
    Assert-GuiQtToolVersion -Path $uic -Name "uic" -Root $fullRoot
    Assert-GuiQtToolVersion -Path $rcc -Name "rcc" -Root $fullRoot

    $include = (& $qmake -query QT_INSTALL_HEADERS 2>$null | Out-String).Trim()
    $lib = (& $qmake -query QT_INSTALL_LIBS 2>$null | Out-String).Trim()
    $plugins = (& $qmake -query QT_INSTALL_PLUGINS 2>$null | Out-String).Trim()
    if (-not $include) { $include = Join-Path $fullRoot "include" }
    if (-not $lib) { $lib = Join-Path $fullRoot "lib" }
    if (-not $plugins) { $plugins = Join-Path $fullRoot "plugins" }

    foreach ($module in @("Core", "Gui", "Widgets", "PrintSupport", "Svg", "Test")) {
        $moduleDir = Join-Path $include "Qt$module"
        if (-not (Test-Path -LiteralPath $moduleDir -PathType Container)) {
            $message = ("Required Qt module header directory is missing: Qt{0}`n" +
                        "  searched: {1}`n  fix: install a complete Qt {2} kit") -f
                $module, $moduleDir, $ExpectedVersion
            throw $message
        }
    }
    foreach ($library in @("Qt6Core", "Qt6Gui", "Qt6Widgets",
                           "Qt6PrintSupport", "Qt6Svg", "Qt6Test")) {
        $patterns = @("lib$library.*", "$library.lib", "$library.a",
                      "$library.dll.a")
        $found = $false
        foreach ($pattern in $patterns) {
            if (Get-ChildItem -LiteralPath $lib -Filter $pattern -File `
                    -ErrorAction SilentlyContinue) {
                $found = $true
                break
            }
        }
        if (-not $found) {
            $message = ("Required Qt library is missing: {0}`n" +
                        "  searched: {1}`n  fix: install a complete Qt {2} kit") -f
                $library, $lib, $ExpectedVersion
            throw $message
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $plugins "platforms") `
                -PathType Container)) {
        $message = ("Qt platform plugins were not found.`n" +
                    "  searched: {0}`n  fix: install a complete Qt {1} kit") -f
            (Join-Path $plugins "platforms"), $ExpectedVersion
        throw $message
    }
    return [pscustomobject]@{
        Root = $fullRoot
        Version = $version
        Qmake = $qmake
        Moc = $moc
        Uic = $uic
        Rcc = $rcc
        Xspec = $xspec
        Include = $include
        Lib = $lib
        Plugins = $plugins
    }
}

function Find-GuiQtRoot {
    $bases = @(
        (Join-Path $env:USERPROFILE "Qt"),
        "C:\Qt",
        "C:\msys64\ucrt64"
    )
    $candidates = @()
    foreach ($base in $bases) {
        if (-not (Test-Path -LiteralPath $base -PathType Container)) { continue }
        $candidates += $base
        $versionBase = Join-Path $base "6.12.0"
        if (Test-Path -LiteralPath $versionBase -PathType Container) {
            $candidates += Get-ChildItem -LiteralPath $versionBase -Directory |
                ForEach-Object { $_.FullName }
        }
    }
    $valid = @()
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        try {
            $info = Test-GuiQtRoot -Root $candidate
            $valid += $info
        } catch {
            # Ignore incomplete kits during discovery.
        }
    }
    if ($valid.Count -eq 1) { return $valid[0] }
    if ($valid.Count -eq 0) {
        throw ("No valid Qt $ExpectedVersion kit was found.`n" +
               "  searched: %USERPROFILE%\Qt, C:\Qt, C:\msys64\ucrt64`n" +
               "  fix: dot-source this script with -QtRoot pointing to the kit")
    }
    $names = ($valid | ForEach-Object { $_.Root }) -join "`n  "
    throw ("Multiple valid Qt $ExpectedVersion kits were found:`n  $names`n" +
           "  fix: choose one explicitly with -QtRoot")
}

function Add-GuiPathEntry {
    param([string]$Variable, [string]$Entry)
    $current = [Environment]::GetEnvironmentVariable($Variable, "Process")
    $parts = @()
    if ($current) {
        $parts = @($current -split ";" | Where-Object {
            $_ -and $_ -ne $Entry
        })
    }
    $value = if ($parts.Count -gt 0) {
        $Entry + ";" + ($parts -join ";")
    } else {
        $Entry
    }
    [Environment]::SetEnvironmentVariable($Variable, $value, "Process")
}

if ($Help) {
    Show-GuiSetupUsage
    return
}

try {
    if (-not $QtRoot) {
        $QtRoot = $env:QT_ROOT
    }
    $info = if ($QtRoot) {
        Test-GuiQtRoot -Root $QtRoot
    } else {
        Find-GuiQtRoot
    }

    Write-Host "Qt check: OK"
    Write-Host "Qt root:   $($info.Root)"
    Write-Host "Qt version: $($info.Version)"
    Write-Host "qmake:     $($info.Qmake)"
    Write-Host "moc:       $($info.Moc)"
    Write-Host "uic:       $($info.Uic)"
    Write-Host "rcc:       $($info.Rcc)"
    Write-Host "platform:  Windows MinGW ($($info.Xspec))"

    if (-not $IsDotSourced) {
        Write-Host ""
        Write-Host "The script was executed, not dot-sourced; the parent PowerShell session was not changed."
        Write-Host "To configure the current session, run:"
        Write-Host ("  . .\scripts\setup-gui-build.ps1 -QtRoot '{0}'" -f $info.Root)
        return
    }

    $env:QT_ROOT = $info.Root
    Add-GuiPathEntry -Variable "PATH" -Entry (Join-Path $info.Root "bin")
    $env:QT_PLUGIN_PATH = $info.Plugins
    Write-Host ""
    Write-Host "GUI build environment configured for this PowerShell process."
    Write-Host "The supported GNU Make build shell remains MSYS2 UCRT64:"
    Write-Host "  open UCRT64 and source scripts/setup-gui-build.sh with the kit path"
    Write-Host ("  then run: make gui-check-qt; make gui-app; make test-gui")
    Write-Host "The variables above are inherited by child processes started from this PowerShell session."
} catch {
    Write-Error $_.Exception.Message
    if (-not $IsDotSourced) { exit 1 }
    return
}

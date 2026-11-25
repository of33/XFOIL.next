param(
    [Parameter(Mandatory = $true)]
    [string]$ZipPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PackageRootName = 'XFOIL.next-GUI-1.2.1-local-g8-windows-x86_64'
$Work = Join-Path $env:TEMP ('xfoil-g8a-installer-test-' + [Guid]::NewGuid().ToString('N'))
$Failed = $false

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class XfoilWindowProbe {
    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] private static extern bool EnumWindows(
        EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] private static extern uint
        GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr hWnd, StringBuilder text,
        int count);
    [DllImport("user32.dll")]
    private static extern int GetWindowTextLength(IntPtr hWnd);

    public static string[] Titles(uint target) {
        var titles = new List<string>();
        EnumWindows((hWnd, _) => {
            uint pid;
            GetWindowThreadProcessId(hWnd, out pid);
            if (pid != target) return true;
            int length = GetWindowTextLength(hWnd);
            if (length <= 0) return true;
            var text = new StringBuilder(length + 1);
            GetWindowText(hWnd, text, text.Capacity);
            titles.Add(text.ToString());
            return true;
        }, IntPtr.Zero);
        return titles.ToArray();
    }
}
"@

function Fail([string]$Message) {
    Write-Host "FAIL: $Message" -ForegroundColor Red
    throw $Message
}

function Stop-XfoilProcesses([string]$Root) {
    Get-Process -Name 'xfoil.next-gui' -ErrorAction SilentlyContinue |
        Where-Object {
            try { $_.Path -and ($_.Path -like ($Root + '\*')) } catch { $false }
        } | ForEach-Object {
            try { $_.CloseMainWindow() | Out-Null } catch {}
            try { if (-not $_.WaitForExit(3000)) { $_.Kill() } } catch {}
        }
}

function Wait-XfoilWindow([string]$ExpectedExe, [int]$TimeoutSeconds = 15) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $process = Get-Process -Name 'xfoil.next-gui' -ErrorAction SilentlyContinue |
            Where-Object {
                try { $_.Path -and ([IO.Path]::GetFullPath($_.Path) -ieq [IO.Path]::GetFullPath($ExpectedExe)) } catch { $false }
            } | Select-Object -First 1
        if ($process) {
            $titles = [XfoilWindowProbe]::Titles([uint32]$process.Id)
            if (@($titles | Where-Object { $_ -like 'XFOIL.next*' }).Count -gt 0) {
                return $process
            }
        }
        Start-Sleep -Milliseconds 250
    }
    Fail "XFOIL.next GUI window did not appear for $ExpectedExe"
}

function Start-And-Stop([string]$LaunchPath, [string]$ExpectedExe) {
    Start-Process -FilePath $LaunchPath
    $process = Wait-XfoilWindow $ExpectedExe
    try { $process.CloseMainWindow() | Out-Null } catch {}
    try { $process.WaitForExit(10000) | Out-Null } catch {}
    if (-not $process.HasExited) { $process.Kill() }
}

function Invoke-InstallerCmd([string]$Package, [string]$Local, [string]$Desktop, [string]$StartMenu) {
    $cmd = Join-Path $Package 'Install XFOIL.next GUI.cmd'
    & $cmd -LocalAppDataDirectory $Local -DesktopDirectory $Desktop `
        -StartMenuProgramsDirectory $StartMenu | Out-Null
    return [int]$LASTEXITCODE
}

function Invoke-InstallerPs1([string]$Package, [string]$Local, [string]$Desktop, [string]$StartMenu) {
    $ps1 = Join-Path $Package 'Install-XFOIL.next-GUI.ps1'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ps1 `
        -LocalAppDataDirectory $Local -DesktopDirectory $Desktop `
        -StartMenuProgramsDirectory $StartMenu | Out-Null
    return [int]$LASTEXITCODE
}

function Assert-Link([string]$Link, [string]$TargetExe, [string]$TargetDirectory) {
    if (-not (Test-Path -LiteralPath $Link -PathType Leaf)) { Fail "missing shortcut: $Link" }
    $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($Link)
    if ([IO.Path]::GetFullPath($shortcut.TargetPath) -ine [IO.Path]::GetFullPath($TargetExe)) {
        Fail "shortcut target is wrong: $Link"
    }
    if ([IO.Path]::GetFullPath($shortcut.WorkingDirectory) -ine [IO.Path]::GetFullPath($TargetDirectory)) {
        Fail "shortcut working directory is wrong: $Link"
    }
    if (-not $shortcut.IconLocation.StartsWith($TargetExe, [StringComparison]::OrdinalIgnoreCase)) {
        Fail "shortcut icon is wrong: $Link"
    }
}

try {
    New-Item -ItemType Directory -Path $Work -Force | Out-Null
    $Extract = Join-Path $Work 'portable folder with spaces'
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $Extract -Force
    $Package = Join-Path $Extract $PackageRootName
    $Exe = Join-Path $Package 'xfoil.next-gui.exe'
    if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { Fail 'portable executable missing' }

    Stop-XfoilProcesses $Work
    Start-And-Stop $Exe $Exe
    Write-Host 'portable start: PASS'

    $Local = Join-Path $Work 'local appdata'
    $Desktop = Join-Path $Work 'Desktop'
    $StartMenu = Join-Path $Work 'Start Menu Programs'
    $Target = Join-Path (Join-Path $Local 'Programs') 'XFOIL.next GUI'
    $TargetExe = Join-Path $Target 'xfoil.next-gui.exe'
    $DesktopLink = Join-Path $Desktop 'XFOIL.next GUI.lnk'
    $StartLink = Join-Path (Join-Path $StartMenu 'XFOIL.next GUI') 'XFOIL.next GUI.lnk'

    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "installer .cmd failed: $rc" }
    if (-not (Test-Path -LiteralPath $TargetExe -PathType Leaf)) { Fail 'installed executable missing' }
    Assert-Link $DesktopLink $TargetExe $Target
    Assert-Link $StartLink $TargetExe $Target
    Write-Host 'installer: PASS'

    Start-And-Stop $DesktopLink $TargetExe
    Write-Host 'desktop shortcut start: PASS'
    Start-And-Stop $StartLink $TargetExe
    Write-Host 'start-menu shortcut start: PASS'

    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "repeated install failed: $rc" }
    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "third install failed: $rc" }
    if (@(Get-ChildItem -LiteralPath $Desktop -Filter '*.lnk').Count -ne 1) {
        Fail 'duplicate desktop shortcut'
    }
    if (@(Get-ChildItem -LiteralPath (Split-Path $StartLink) -Filter '*.lnk').Count -ne 1) {
        Fail 'duplicate start-menu shortcut'
    }
    if (Test-Path -LiteralPath (Join-Path $Target $PackageRootName)) {
        Fail 'nested package folder was created'
    }
    Write-Host 'repeated install: PASS'

    $stream = [IO.File]::Open($TargetExe, [IO.FileMode]::Open,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $rc = Invoke-InstallerPs1 $Package $Local $Desktop $StartMenu
        if ($rc -eq 0) { Fail 'installer accepted a locked target executable' }
    } finally {
        $stream.Close()
    }
    Write-Host 'locked target rejection: PASS'

    foreach ($missing in @('xfoil.next-gui.exe', 'Qt6Core.dll', 'platforms\qwindows.dll')) {
        $Bad = Join-Path $Work ('bad-' + [Guid]::NewGuid().ToString('N'))
        Copy-Item -LiteralPath $Package -Destination $Bad -Recurse
        Remove-Item -LiteralPath (Join-Path $Bad $missing) -Force
        $rc = Invoke-InstallerPs1 $Bad $Local $Desktop $StartMenu
        if ($rc -eq 0) { Fail "installer accepted missing $missing" }
    }
    Write-Host 'missing runtime rejection: PASS'

    $blockedLocal = Join-Path $Work 'blocked local appdata'
    New-Item -ItemType Directory -Path (Join-Path $blockedLocal 'Programs') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path (Join-Path $blockedLocal 'Programs') 'XFOIL.next GUI') -Value 'blocked'
    $rc = Invoke-InstallerPs1 $Package $blockedLocal $Desktop $StartMenu
    if ($rc -eq 0) { Fail 'installer accepted a blocked target path' }
    Write-Host 'blocked target rejection: PASS'

    Write-Host 'Windows installer tests: PASS'
    exit 0
} catch {
    $Failed = $true
    Write-Host "Windows installer tests: FAIL - $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    Stop-XfoilProcesses $Work
    if (Test-Path -LiteralPath $Work) {
        Remove-Item -LiteralPath $Work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

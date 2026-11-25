param(
    [Parameter(Mandatory = $true)]
    [string]$ZipPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PackageRootName = 'XFOIL.next-GUI-1.2.1-local-g8-windows-x86_64'
$Work = Join-Path $env:TEMP ('xfoil-g8b-installer-test-' + [Guid]::NewGuid().ToString('N'))
$TargetName = 'XFOIL.next GUI'
$MarkerName = '.xfoil-next-gui-install'
$HelperPatterns = @(
    'XFOIL.next-GUI-uninstall-*',
    'XFOIL.next-GUI-uninstall-cleanup-*'
)

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

function Invoke-UninstallPs1([string]$Target, [string]$Local, [string]$Desktop, [string]$StartMenu, [switch]$Quiet) {
    $ps1 = Join-Path $Target 'Uninstall-XFOIL.next-GUI.ps1'
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $ps1,
        '-LocalAppDataDirectory', $Local, '-DesktopDirectory', $Desktop,
        '-StartMenuProgramsDirectory', $StartMenu)
    if ($Quiet) { $arguments += '-Quiet' }
    & powershell.exe @arguments | Out-Null
    return [int]$LASTEXITCODE
}

function Invoke-UninstallCmd([string]$Target, [string]$Local, [string]$Desktop, [string]$StartMenu) {
    $cmd = Join-Path $Target 'Uninstall XFOIL.next GUI.cmd'
    & $cmd -LocalAppDataDirectory $Local -DesktopDirectory $Desktop `
        -StartMenuProgramsDirectory $StartMenu -Quiet | Out-Null
    return [int]$LASTEXITCODE
}

function Assert-Link([string]$Link, [string]$TargetPath, [string]$TargetDirectory, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Link -PathType Leaf)) { Fail "missing shortcut: $Link" }
    $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($Link)
    if ([IO.Path]::GetFullPath($shortcut.TargetPath) -ine [IO.Path]::GetFullPath($TargetPath)) {
        Fail "shortcut target is wrong: $Link"
    }
    if ([IO.Path]::GetFullPath($shortcut.WorkingDirectory) -ine [IO.Path]::GetFullPath($TargetDirectory)) {
        Fail "shortcut working directory is wrong: $Link"
    }
    if (-not $shortcut.IconLocation.StartsWith(
            (Join-Path $TargetDirectory 'xfoil.next-gui.exe'),
            [StringComparison]::OrdinalIgnoreCase)) {
        Fail "shortcut icon is wrong: $Link"
    }
    if ($null -ne $Description -and $shortcut.Description -ne $Description) {
        Fail "shortcut description is wrong: $Link"
    }
}

function New-TestLink([string]$Path) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($Path)
    $shortcut.TargetPath = Join-Path $env:SystemRoot 'System32\notepad.exe'
    $shortcut.WorkingDirectory = Join-Path $env:SystemRoot 'System32'
    $shortcut.IconLocation = (Join-Path $env:SystemRoot 'System32\notepad.exe,0')
    $shortcut.Description = 'Foreign XFOIL test link'
    $shortcut.Save()
}

function Assert-ForeignLinks([string]$Desktop, [string]$StartMenu) {
    foreach ($path in @((Join-Path $Desktop 'xfoil.lnk'),
                        (Join-Path $StartMenu 'xfoil.lnk'))) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            Fail "foreign xfoil shortcut was removed: $path"
        }
        $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($path)
        if ($shortcut.TargetPath -notlike '*notepad.exe') {
            Fail "foreign xfoil shortcut changed: $path"
        }
    }
}

function Helper-Artifacts {
    $items = @()
    foreach ($pattern in $HelperPatterns) {
        $items += Get-ChildItem -LiteralPath $env:TEMP -Force -Filter $pattern `
            -ErrorAction SilentlyContinue
    }
    return @($items | ForEach-Object { $_.FullName } | Sort-Object -Unique)
}

function Wait-Until([scriptblock]$Condition, [int]$TimeoutSeconds, [string]$Message) {
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 250
    }
    Fail $Message
}

function Wait-Uninstalled([string]$Target, [string]$DesktopLink, [string]$StartFolder, [string[]]$BeforeHelpers) {
    Wait-Until {
        (-not (Test-Path -LiteralPath $Target)) -and
        (-not (Test-Path -LiteralPath $DesktopLink)) -and
        (-not (Test-Path -LiteralPath $StartFolder)) -and
        (@((Helper-Artifacts) | Where-Object { $BeforeHelpers -notcontains $_ }).Count -eq 0)
    } 30 "uninstaller did not remove the installation, shortcuts and temporary helper"
}

function Assert-VersionInfo([string]$Exe) {
    $v = (Get-Item -LiteralPath $Exe).VersionInfo
    if ($v.FileDescription -ne 'XFOIL.next GUI') { Fail "FileDescription is '$($v.FileDescription)'" }
    if ($v.ProductName -ne 'XFOIL.next GUI') { Fail "ProductName is '$($v.ProductName)'" }
    if ($v.InternalName -ne 'XFOIL.next GUI') { Fail "InternalName is '$($v.InternalName)'" }
    if ($v.OriginalFilename -ne 'xfoil.next-gui.exe') { Fail "OriginalFilename is '$($v.OriginalFilename)'" }
    if (-not $v.FileVersion.StartsWith('1.2.1')) { Fail "FileVersion is '$($v.FileVersion)'" }
    if (-not $v.ProductVersion.StartsWith('1.2.1')) { Fail "ProductVersion is '$($v.ProductVersion)'" }
}

function Assert-Marker([string]$Target) {
    $marker = Join-Path $Target $MarkerName
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { Fail 'installation marker missing' }
    $content = @(Get-Content -LiteralPath $marker)
    foreach ($line in @('product = XFOIL.next GUI', 'scope = per-user',
                        'install-root = managed')) {
        if ($content -notcontains $line) { Fail "installation marker missing '$line'" }
    }
}

function Assert-StartMenuLayout([string]$StartMenu, [string]$Target) {
    $folder = Join-Path $StartMenu $TargetName
    $links = @(Get-ChildItem -LiteralPath $folder -Filter '*.lnk' -File)
    $names = @($links | ForEach-Object { $_.Name } | Sort-Object)
    $expected = @(($TargetName + '.lnk'), ('Uninstall ' + $TargetName + '.lnk')) | Sort-Object
    if (($names -join '|') -ne ($expected -join '|')) {
        Fail "unexpected Start Menu shortcuts: $($names -join ', ')"
    }
    Assert-Link (Join-Path $folder ($TargetName + '.lnk')) `
        (Join-Path $Target 'xfoil.next-gui.exe') $Target 'XFOIL.next GUI'
    Assert-Link (Join-Path $folder ('Uninstall ' + $TargetName + '.lnk')) `
        (Join-Path $Target 'Uninstall XFOIL.next GUI.cmd') $Target 'Uninstall XFOIL.next GUI'
}

try {
    New-Item -ItemType Directory -Path $Work -Force | Out-Null
    $Extract = Join-Path $Work 'portable folder with spaces'
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $Extract -Force
    $Package = Join-Path $Extract $PackageRootName
    $Exe = Join-Path $Package 'xfoil.next-gui.exe'
    if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { Fail 'portable executable missing' }
    foreach ($script in @('Uninstall XFOIL.next GUI.cmd', 'Uninstall-XFOIL.next-GUI.ps1')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Package $script) -PathType Leaf)) {
            Fail "uninstaller script missing from ZIP: $script"
        }
    }

    Stop-XfoilProcesses $Work
    Start-And-Stop $Exe $Exe
    Write-Host 'portable start: PASS'

    $Local = Join-Path $Work 'local appdata'
    $Desktop = Join-Path $Work 'Desktop'
    $StartMenu = Join-Path $Work 'Start Menu Programs'
    $Target = Join-Path (Join-Path $Local 'Programs') $TargetName
    $TargetExe = Join-Path $Target 'xfoil.next-gui.exe'
    $DesktopLink = Join-Path $Desktop ($TargetName + '.lnk')
    $StartFolder = Join-Path $StartMenu $TargetName
    $StartLink = Join-Path $StartFolder ($TargetName + '.lnk')
    $StartUninstallLink = Join-Path $StartFolder ('Uninstall ' + $TargetName + '.lnk')
    New-Item -ItemType Directory -Path $Desktop -Force | Out-Null
    New-Item -ItemType Directory -Path $StartMenu -Force | Out-Null
    New-TestLink (Join-Path $Desktop 'xfoil.lnk')
    New-TestLink (Join-Path $StartMenu 'xfoil.lnk')

    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "installer .cmd failed: $rc" }
    if (-not (Test-Path -LiteralPath $TargetExe -PathType Leaf)) { Fail 'installed executable missing' }
    Assert-Marker $Target
    Assert-VersionInfo $TargetExe
    Assert-Link $DesktopLink $TargetExe $Target 'XFOIL.next GUI'
    Assert-StartMenuLayout $StartMenu $Target
    if (Test-Path -LiteralPath (Join-Path $Desktop ('Uninstall ' + $TargetName + '.lnk'))) {
        Fail 'uninstaller shortcut was created on the Desktop'
    }
    if (Test-Path -LiteralPath (Join-Path $StartFolder 'xfoil.lnk')) {
        Fail 'package-created Start Menu shortcut named xfoil'
    }
    Assert-ForeignLinks $Desktop $StartMenu
    Write-Host 'installer: PASS'

    Start-And-Stop $DesktopLink $TargetExe
    Write-Host 'desktop shortcut start: PASS'
    Start-And-Stop $StartLink $TargetExe
    Write-Host 'start-menu shortcut start: PASS'

    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "repeated install failed: $rc" }
    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "third install failed: $rc" }
    if (@(Get-ChildItem -LiteralPath $Desktop -Filter '*.lnk').Count -ne 2) {
        Fail 'duplicate or missing desktop shortcut'
    }
    Assert-StartMenuLayout $StartMenu $Target
    if (Test-Path -LiteralPath (Join-Path $Target $PackageRootName)) {
        Fail 'nested package folder was created'
    }
    Assert-ForeignLinks $Desktop $StartMenu
    Write-Host 'repeated install: PASS'

    $marker = Join-Path $Target $MarkerName
    $savedMarker = Join-Path $Work 'saved-marker'
    Move-Item -LiteralPath $marker -Destination $savedMarker
    $rc = Invoke-UninstallPs1 $Target $Local $Desktop $StartMenu
    if ($rc -eq 0) { Fail 'uninstaller accepted a missing marker' }
    if (-not (Test-Path -LiteralPath $TargetExe)) { Fail 'missing-marker rejection removed the installation' }
    Move-Item -LiteralPath $savedMarker -Destination $marker
    Write-Host 'missing marker rejection: PASS'

    $markerContent = Get-Content -LiteralPath $marker
    Set-Content -LiteralPath $marker -Value 'product = wrong' -Encoding ascii
    $rc = Invoke-UninstallPs1 $Target $Local $Desktop $StartMenu
    if ($rc -eq 0) { Fail 'uninstaller accepted an invalid marker' }
    Set-Content -LiteralPath $marker -Value $markerContent -Encoding ascii
    Write-Host 'invalid marker rejection: PASS'

    Remove-Item -LiteralPath $marker -Force
    $rc = Invoke-InstallerPs1 $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "migration reinstall failed: $rc" }
    Assert-Marker $Target
    Write-Host 'marker migration: PASS'

    $Wrong = Join-Path $Work 'wrong uninstaller location'
    New-Item -ItemType Directory -Path $Wrong -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $Target 'Uninstall-XFOIL.next-GUI.ps1') `
        -Destination $Wrong
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
        (Join-Path $Wrong 'Uninstall-XFOIL.next-GUI.ps1') `
        -LocalAppDataDirectory $Local -DesktopDirectory $Desktop `
        -StartMenuProgramsDirectory $StartMenu | Out-Null
    if ($LASTEXITCODE -eq 0) { Fail 'uninstaller accepted a script outside the managed target' }
    if (-not (Test-Path -LiteralPath $TargetExe)) { Fail 'wrong-target rejection removed the installation' }
    Write-Host 'unexpected target rejection: PASS'

    $junction = Join-Path $Target 'junction-test'
    New-Item -ItemType Junction -Path $junction -Target $Work | Out-Null
    $rc = Invoke-UninstallPs1 $Target $Local $Desktop $StartMenu
    if ($rc -eq 0) { Fail 'uninstaller accepted a reparse point' }
    if (-not (Test-Path -LiteralPath $TargetExe)) { Fail 'reparse rejection removed the installation' }
    Remove-Item -LiteralPath $junction -Force
    Write-Host 'reparse point rejection: PASS'

    $running = Start-Process -FilePath $TargetExe -PassThru
    $null = Wait-XfoilWindow $TargetExe
    $rc = Invoke-UninstallPs1 $Target $Local $Desktop $StartMenu
    if ($rc -eq 0) { Fail 'uninstaller accepted a running application' }
    if (-not (Test-Path -LiteralPath $TargetExe)) { Fail 'running-app rejection removed the installation' }
    try { $running.CloseMainWindow() | Out-Null } catch {}
    try { $running.WaitForExit(10000) | Out-Null } catch {}
    if (-not $running.HasExited) {
        $running.Kill()
        try { $running.WaitForExit(5000) | Out-Null } catch {}
    }
    Stop-XfoilProcesses $Work
    Start-Sleep -Milliseconds 500
    Write-Host 'running application rejection: PASS'

    $stream = [IO.File]::Open($TargetExe, [IO.FileMode]::Open,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $rc = Invoke-UninstallPs1 $Target $Local $Desktop $StartMenu
        if ($rc -eq 0) { Fail 'uninstaller accepted a locked executable' }
        if (-not (Test-Path -LiteralPath $TargetExe)) { Fail 'locked rejection removed the installation' }
    } finally {
        $stream.Close()
    }
    Write-Host 'locked executable rejection: PASS'

    $beforeHelpers = @(Helper-Artifacts)
    $rc = Invoke-UninstallCmd $Target $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "uninstaller .cmd failed: $rc" }
    Wait-Uninstalled $Target $DesktopLink $StartFolder $beforeHelpers
    Assert-ForeignLinks $Desktop $StartMenu
    Write-Host 'uninstaller and shortcut cleanup: PASS'

    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "reinstall after uninstall failed: $rc" }
    Start-And-Stop $StartLink $TargetExe
    $beforeHelpers = @(Helper-Artifacts)
    $rc = Invoke-UninstallCmd $Target $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "second uninstall failed: $rc" }
    Wait-Uninstalled $Target $DesktopLink $StartFolder $beforeHelpers
    Assert-ForeignLinks $Desktop $StartMenu
    Write-Host 'install-uninstall-reinstall: PASS'

    $rc = Invoke-InstallerCmd $Package $Local $Desktop $StartMenu
    if ($rc -ne 0) { Fail "reinstall before locked-target test failed: $rc" }

    $blockedLocal = Join-Path $Work 'blocked local appdata'
    New-Item -ItemType Directory -Path (Join-Path $blockedLocal 'Programs') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path (Join-Path $blockedLocal 'Programs') $TargetName) -Value 'blocked'
    $rc = Invoke-InstallerPs1 $Package $blockedLocal $Desktop $StartMenu
    if ($rc -eq 0) { Fail 'installer accepted a blocked target path' }
    Write-Host 'blocked target rejection: PASS'

    $stream = [IO.File]::Open($TargetExe, [IO.FileMode]::Open,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $rc = Invoke-InstallerPs1 $Package $Local $Desktop $StartMenu
        if ($rc -eq 0) { Fail 'installer accepted a locked target executable' }
    } finally {
        $stream.Close()
    }
    Write-Host 'locked target rejection: PASS'

    foreach ($missing in @('xfoil.next-gui.exe', 'Qt6Core.dll', 'platforms\qwindows.dll',
                           'Uninstall XFOIL.next GUI.cmd', 'Uninstall-XFOIL.next-GUI.ps1')) {
        $Bad = Join-Path $Work ('bad-' + [Guid]::NewGuid().ToString('N'))
        Copy-Item -LiteralPath $Package -Destination $Bad -Recurse
        Remove-Item -LiteralPath (Join-Path $Bad $missing) -Force
        $rc = Invoke-InstallerPs1 $Bad $Local $Desktop $StartMenu
        if ($rc -eq 0) { Fail "installer accepted missing $missing" }
    }
    Write-Host 'missing runtime/uninstaller rejection: PASS'

    Write-Host 'Windows installer tests: PASS'
    exit 0
} catch {
    Write-Host "Windows installer tests: FAIL - $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    Stop-XfoilProcesses $Work
    if (Test-Path -LiteralPath $Work) {
        Remove-Item -LiteralPath $Work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

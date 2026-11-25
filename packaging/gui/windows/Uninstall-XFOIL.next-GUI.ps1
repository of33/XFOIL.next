param(
    [string]$LocalAppDataDirectory = [Environment]::GetFolderPath('LocalApplicationData'),
    [string]$DesktopDirectory = [Environment]::GetFolderPath('Desktop'),
    [string]$StartMenuProgramsDirectory = [Environment]::GetFolderPath('Programs'),
    [Parameter(DontShow = $true)][string]$HelperInstallRoot = '',
    [Parameter(DontShow = $true)][int]$HelperParentPid = 0,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$TargetName = 'XFOIL.next GUI'
$MarkerName = '.xfoil-next-gui-install'
$MarkerLines = @(
    'product = XFOIL.next GUI',
    'scope = per-user',
    'install-root = managed'
)
$QuietMode = $Quiet -or ($env:XFOIL_NEXT_GUI_UNINSTALL_QUIET -eq '1')

function Fail([string]$Message) {
    throw $Message
}

function Normalize([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        Fail 'A required directory path is empty.'
    }
    return [IO.Path]::GetFullPath($Path).TrimEnd('\')
}

function SamePath([string]$Left, [string]$Right) {
    return ([IO.Path]::GetFullPath($Left).TrimEnd('\') -ieq [IO.Path]::GetFullPath($Right).TrimEnd('\'))
}

function Assert-NotReparse([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    $item = Get-Item -LiteralPath $Path -Force
    if ([bool]($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        Fail "Refusing to use a reparse point: $Path"
    }
}

function Find-ReparsePoint([string]$Root) {
    $pending = New-Object System.Collections.Stack
    $pending.Push($Root)
    while ($pending.Count -gt 0) {
        $directory = $pending.Pop()
        foreach ($entry in [IO.Directory]::EnumerateFileSystemEntries($directory)) {
            $attributes = [IO.File]::GetAttributes($entry)
            if ([bool]($attributes -band [IO.FileAttributes]::ReparsePoint)) {
                return $entry
            }
            if ([bool]($attributes -band [IO.FileAttributes]::Directory)) {
                $pending.Push($entry)
            }
        }
    }
    return $null
}

function Assert-OwnedShortcut(
    [string]$ShortcutPath,
    [string]$ExpectedTarget,
    [string]$ExpectedDirectory,
    [string]$ExpectedIcon,
    [string]$Label
) {
    if (-not (Test-Path -LiteralPath $ShortcutPath -PathType Leaf)) {
        return
    }
    $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($ShortcutPath)
    if (-not (SamePath $shortcut.TargetPath $ExpectedTarget)) {
        Fail "Refusing to remove $Label with an unexpected target: $ShortcutPath"
    }
    if (-not (SamePath $shortcut.WorkingDirectory $ExpectedDirectory)) {
        Fail "Refusing to remove $Label with an unexpected working directory: $ShortcutPath"
    }
    if (-not $shortcut.IconLocation.StartsWith(
            $ExpectedIcon, [StringComparison]::OrdinalIgnoreCase)) {
        Fail "Refusing to remove $Label with an unexpected icon: $ShortcutPath"
    }
}

function Assert-UninstallLayout {
    if ([string]::IsNullOrWhiteSpace($LocalAppDataDirectory)) {
        Fail 'LOCALAPPDATA is not available.'
    }
    if ([string]::IsNullOrWhiteSpace($DesktopDirectory)) {
        Fail 'The user desktop folder is not available.'
    }
    if ([string]::IsNullOrWhiteSpace($StartMenuProgramsDirectory)) {
        Fail 'The user start-menu folder is not available.'
    }
    $script:LocalRoot = Normalize $LocalAppDataDirectory
    $script:DesktopRoot = Normalize $DesktopDirectory
    $script:StartProgramsRoot = Normalize $StartMenuProgramsDirectory
    $script:ProgramsRoot = Normalize (Join-Path $LocalRoot 'Programs')
    $script:ExpectedRoot = Normalize (Join-Path $ProgramsRoot $TargetName)
    if (-not (Test-Path -LiteralPath $ProgramsRoot -PathType Container)) {
        Fail "The per-user Programs folder does not exist: $ProgramsRoot"
    }
    Assert-NotReparse $ProgramsRoot
    Assert-NotReparse $ExpectedRoot
    if (-not (SamePath $InstallRoot $ExpectedRoot)) {
        Fail "Refusing to uninstall an unexpected location: $InstallRoot"
    }
    if (-not (Test-Path -LiteralPath $InstallRoot -PathType Container)) {
        Fail "XFOIL.next GUI is not installed for this user: $InstallRoot"
    }
    $marker = Join-Path $InstallRoot $MarkerName
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        Fail 'The installation marker is missing. Reinstall XFOIL.next GUI with the current installer before uninstalling.'
    }
    Assert-NotReparse $marker
    $markerContent = @(Get-Content -LiteralPath $marker -Encoding ascii |
        ForEach-Object { $_.TrimEnd() })
    foreach ($line in $MarkerLines) {
        if ($markerContent -notcontains $line) {
            Fail 'The installation marker is invalid or belongs to another installation.'
        }
    }
    $exe = Join-Path $InstallRoot 'xfoil.next-gui.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        Fail 'The installed GUI executable is missing.'
    }
    Assert-NotReparse $exe
    $reparse = Find-ReparsePoint $InstallRoot
    if ($null -ne $reparse) {
        Fail "Refusing to remove an installation containing a reparse point: $reparse"
    }
    $script:DesktopLink = Join-Path $DesktopRoot ($TargetName + '.lnk')
    $script:StartFolder = Join-Path $StartProgramsRoot $TargetName
    $script:StartAppLink = Join-Path $StartFolder ($TargetName + '.lnk')
    $script:StartUninstallLink = Join-Path $StartFolder (
        'Uninstall ' + $TargetName + '.lnk')
    $uninstallCmd = Join-Path $InstallRoot 'Uninstall XFOIL.next GUI.cmd'
    Assert-NotReparse $StartFolder
    Assert-OwnedShortcut $DesktopLink $exe $InstallRoot $exe 'Desktop shortcut'
    Assert-OwnedShortcut $StartAppLink $exe $InstallRoot $exe 'Start Menu shortcut'
    Assert-OwnedShortcut $StartUninstallLink $uninstallCmd $InstallRoot $exe `
        'Start Menu uninstaller shortcut'

    $running = @(Get-Process -Name 'xfoil.next-gui' -ErrorAction SilentlyContinue |
        Where-Object {
            try { $_.Path -and (SamePath $_.Path $exe) } catch { $false }
        })
    if ($running.Count -gt 0) {
        Fail 'Close XFOIL.next GUI before uninstalling.'
    }
    try {
        $stream = [IO.File]::Open($exe, [IO.FileMode]::Open,
            [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        $stream.Close()
    } catch {
        Fail 'Close XFOIL.next GUI before uninstalling. The installed executable is in use.'
    }
}

function Remove-OwnedShortcut([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        Remove-Item -LiteralPath $Path -Force
    }
}

function Remove-WithRetry([string]$Path) {
    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        if (-not (Test-Path -LiteralPath $Path)) {
            return
        }
        try {
            Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
            return
        } catch {
            if ($attempt -eq 39) {
                throw
            }
            Start-Sleep -Milliseconds 250
        }
    }
}

function Invoke-Removal {
    Assert-UninstallLayout
    Remove-OwnedShortcut $script:DesktopLink
    Remove-OwnedShortcut $script:StartAppLink
    Remove-OwnedShortcut $script:StartUninstallLink
    if (Test-Path -LiteralPath $script:StartFolder -PathType Container) {
        $remaining = @(Get-ChildItem -LiteralPath $script:StartFolder -Force)
        if ($remaining.Count -eq 0) {
            Remove-Item -LiteralPath $script:StartFolder -Force
        } else {
            Write-Warning "The Start Menu folder was left in place because it contains other files: $script:StartFolder"
        }
    }
    Remove-WithRetry $script:InstallRoot
}

function Show-UninstallResult([string]$Message, [bool]$IsError) {
    if ($QuietMode) {
        Write-Host $Message
        return
    }
    $shell = New-Object -ComObject WScript.Shell
    $icon = if ($IsError) { 0x10 } else { 0x40 }
    $shell.Popup($Message, 0, 'XFOIL.next GUI', $icon) | Out-Null
}

function Schedule-HelperCleanup([string]$HelperDirectory) {
    $cleanup = Join-Path $env:TEMP (
        'XFOIL.next-GUI-uninstall-cleanup-' + [Guid]::NewGuid().ToString('N') + '.cmd')
    $text = "@echo off`r`n" +
        "ping 127.0.0.1 -n 3 >nul`r`n" +
        "rmdir /s /q `"$HelperDirectory`"`r`n" +
        "del /f /q `"%~f0`"`r`n"
    Set-Content -LiteralPath $cleanup -Value $text -Encoding ascii
    Start-Process -FilePath $env:ComSpec -ArgumentList (
        '/d /c "' + $cleanup + '"') -WindowStyle Hidden | Out-Null
}

if ([string]::IsNullOrWhiteSpace($HelperInstallRoot)) {
    $InstallRoot = Normalize $PSScriptRoot
    Assert-UninstallLayout

    $helperDirectory = Join-Path $env:TEMP (
        'XFOIL.next-GUI-uninstall-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $helperDirectory -Force | Out-Null
    $helperScript = Join-Path $helperDirectory 'Uninstall-XFOIL.next-GUI.ps1'
    Copy-Item -LiteralPath $PSCommandPath -Destination $helperScript -Force

    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' +
        $helperScript + '" -HelperInstallRoot "' + $InstallRoot +
        '" -HelperParentPid ' + $PID +
        ' -LocalAppDataDirectory "' + $LocalAppDataDirectory +
        '" -DesktopDirectory "' + $DesktopDirectory +
        '" -StartMenuProgramsDirectory "' + $StartMenuProgramsDirectory + '"'
    if ($QuietMode) {
        $arguments += ' -Quiet'
    }
    try {
        Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments `
            -WindowStyle Hidden | Out-Null
    } catch {
        Remove-Item -LiteralPath $helperDirectory -Recurse -Force `
            -ErrorAction SilentlyContinue
        Fail 'The uninstall helper could not be started.'
    }
    Write-Host ''
    Write-Host 'XFOIL.next GUI uninstallation started for the current user.'
    Write-Host 'The installed files and shortcuts will be removed after this window closes.'
    exit 0
}

$InstallRoot = Normalize $HelperInstallRoot
if ($HelperParentPid -gt 0) {
    $parent = Get-Process -Id $HelperParentPid -ErrorAction SilentlyContinue
    if ($null -ne $parent) {
        try { $parent.WaitForExit(15000) | Out-Null } catch {}
    }
    Start-Sleep -Milliseconds 750
}

$helperDirectory = $PSScriptRoot
try {
    Invoke-Removal
} catch {
    Show-UninstallResult (
        "XFOIL.next GUI uninstallation failed:`r`n" +
        $_.Exception.Message) $true
    Schedule-HelperCleanup $helperDirectory
    exit 1
}
Show-UninstallResult 'XFOIL.next GUI was uninstalled for the current user.' $false
Schedule-HelperCleanup $helperDirectory
exit 0

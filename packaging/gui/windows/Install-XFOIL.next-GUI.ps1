param(
    [string]$LocalAppDataDirectory = [Environment]::GetFolderPath('LocalApplicationData'),
    [string]$DesktopDirectory = [Environment]::GetFolderPath('Desktop'),
    [string]$StartMenuProgramsDirectory = [Environment]::GetFolderPath('Programs')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$PackageRoot = $PSScriptRoot
$TargetName = 'XFOIL.next GUI'
$Required = @(
    'xfoil.next-gui.exe',
    'Qt6Core.dll',
    'platforms\qwindows.dll'
)

function Fail([string]$Message) {
    throw $Message
}

if ([string]::IsNullOrWhiteSpace($LocalAppDataDirectory)) {
    Fail 'LOCALAPPDATA is not available.'
}
if ([string]::IsNullOrWhiteSpace($DesktopDirectory)) {
    Fail 'The user desktop folder is not available.'
}
if ([string]::IsNullOrWhiteSpace($StartMenuProgramsDirectory)) {
    Fail 'The user start-menu folder is not available.'
}

$LocalAppDataDirectory = [IO.Path]::GetFullPath($LocalAppDataDirectory)
$DesktopDirectory = [IO.Path]::GetFullPath($DesktopDirectory)
$StartMenuProgramsDirectory = [IO.Path]::GetFullPath($StartMenuProgramsDirectory)
$ProgramsRoot = Join-Path $LocalAppDataDirectory 'Programs'
$Target = Join-Path $ProgramsRoot $TargetName
$TargetExe = Join-Path $Target 'xfoil.next-gui.exe'
$Staging = Join-Path $ProgramsRoot ('.XFOIL.next-GUI-staging-' + [Guid]::NewGuid().ToString('N'))
$Backup = Join-Path $ProgramsRoot ('.XFOIL.next-GUI-backup-' + [Guid]::NewGuid().ToString('N'))

function Assert-Package([string]$Root) {
    foreach ($relative in $Required) {
        $path = Join-Path $Root $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            Fail "The package is incomplete: missing $relative. Extract the complete ZIP and run the installer from its top-level folder."
        }
    }
}

function Assert-ShortcutTarget([string]$ShortcutPath, [string]$ExpectedExe, [string]$ExpectedDirectory) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($ShortcutPath)
    if ([IO.Path]::GetFullPath($shortcut.TargetPath) -ine [IO.Path]::GetFullPath($ExpectedExe)) {
        Fail "Shortcut target is wrong: $ShortcutPath"
    }
    if ([IO.Path]::GetFullPath($shortcut.WorkingDirectory) -ine [IO.Path]::GetFullPath($ExpectedDirectory)) {
        Fail "Shortcut working directory is wrong: $ShortcutPath"
    }
    if (-not $shortcut.IconLocation.StartsWith($ExpectedExe, [StringComparison]::OrdinalIgnoreCase)) {
        Fail "Shortcut icon is wrong: $ShortcutPath"
    }
}

function New-Shortcut([string]$ShortcutPath, [string]$TargetExe, [string]$TargetDirectory) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($ShortcutPath)
    $shortcut.TargetPath = $TargetExe
    $shortcut.WorkingDirectory = $TargetDirectory
    $shortcut.IconLocation = "$TargetExe,0"
    $shortcut.Description = 'XFOIL.next GUI'
    $shortcut.Save()
}

try {
    Assert-Package $PackageRoot
    if ([IO.Path]::GetFullPath($PackageRoot).TrimEnd('\') -ieq [IO.Path]::GetFullPath($Target).TrimEnd('\')) {
        Fail 'Run the installer from the extracted ZIP, not from the installed application folder.'
    }

    New-Item -ItemType Directory -Path $ProgramsRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $Staging -Force | Out-Null
    Get-ChildItem -LiteralPath $PackageRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Staging -Recurse -Force
    }
    Assert-Package $Staging

    if (Test-Path -LiteralPath $Target -PathType Container) {
        if (Test-Path -LiteralPath $TargetExe -PathType Leaf) {
            try {
                $stream = [IO.File]::Open($TargetExe, [IO.FileMode]::Open,
                    [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
                $stream.Close()
            } catch {
                Fail 'Close XFOIL.next GUI before installing. The installed executable or one of its runtime files is in use.'
            }
        }
        try {
            Move-Item -LiteralPath $Target -Destination $Backup
        } catch {
            Fail 'Close XFOIL.next GUI before installing. The installed folder could not be replaced.'
        }
        try {
            Move-Item -LiteralPath $Staging -Destination $Target
        } catch {
            if (Test-Path -LiteralPath $Backup) {
                Move-Item -LiteralPath $Backup -Destination $Target -Force
            }
            Fail 'The application could not be installed. The previous installation was restored.'
        }
        try {
            Remove-Item -LiteralPath $Backup -Recurse -Force
        } catch {
            Write-Warning "The previous installation backup could not be removed: $Backup"
        }
    } else {
        Move-Item -LiteralPath $Staging -Destination $Target
    }

    Assert-Package $Target
    New-Item -ItemType Directory -Path $DesktopDirectory -Force | Out-Null
    $StartMenuFolder = Join-Path $StartMenuProgramsDirectory $TargetName
    New-Item -ItemType Directory -Path $StartMenuFolder -Force | Out-Null
    $DesktopLink = Join-Path $DesktopDirectory ($TargetName + '.lnk')
    $StartMenuLink = Join-Path $StartMenuFolder ($TargetName + '.lnk')
    New-Shortcut $DesktopLink $TargetExe $Target
    New-Shortcut $StartMenuLink $TargetExe $Target
    Assert-ShortcutTarget $DesktopLink $TargetExe $Target
    Assert-ShortcutTarget $StartMenuLink $TargetExe $Target

    Write-Host ''
    Write-Host "XFOIL.next GUI was installed for the current user:"
    Write-Host "  $Target"
    Write-Host 'Desktop and Start Menu shortcuts were created or updated.'
    Write-Host 'No administrator rights were required.'
    Write-Host 'To remove it manually, delete both shortcuts and the folder above.'
    exit 0
} catch {
    Write-Host ''
    Write-Host "XFOIL.next GUI installation failed:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
} finally {
    if (Test-Path -LiteralPath $Staging) {
        Remove-Item -LiteralPath $Staging -Recurse -Force -ErrorAction SilentlyContinue
    }
}

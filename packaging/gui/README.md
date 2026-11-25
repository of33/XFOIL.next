# Portable GUI packaging

This directory contains the XFOIL.next GUI packaging inputs. The product
version remains the canonical value in `src/app/ProductInfo.h`; G8 tests use a
`local-g8` suffix to avoid presenting the artifacts as release downloads.

## Linux AppImage

The AppImage is built on Ubuntu 24.04 with GCC 13.3.0 and the Qt 6.12.0 kit
selected by `QT_ROOT`. No older-glibc compatibility promise is made for the
GUI AppImage. The bundle contains the Qt shared libraries and plugins,
`libqxcb.so`, optional `libqoffscreen.so`, image/SVG plugins, PrintSupport,
desktop/icon metadata and license/notice files. The CLI executable is not
included.

The AppImage tool set is external and pinned. Set `APPIMAGE_TOOLS_DIR` to a
directory containing these verified files:

```text
linuxdeploy-x86_64.AppImage
linuxdeploy-plugin-qt-x86_64.AppImage
appimagetool-x86_64.AppImage
runtime-x86_64
```

`packaging/gui/check_appimage_tools.py` verifies the exact SHA-256 values before
use. The build scripts never download a tool or runtime. AppImages are invoked
with `--appimage-extract-and-run` unless FUSE is explicitly being exercised.

```sh
export QT_ROOT=/path/to/Qt/6.12.0/gcc_64
export APPIMAGE_TOOLS_DIR=/path/to/pinned/appimage-tools
make gui-appimage
make gui-portable-check-linux
make gui-portable-repro-linux
```

Outputs are written below `build/portable/gui/` and are not committed.

The derived GUI PNG/ICO can be regenerated offline from the canonical root
`XFOIL.next.png` with `tools/generate-gui-icons.py` (Pillow, NumPy and SciPy are
required only for that one-time regeneration, not for normal builds).

## Windows GUI ZIP

Build in MSYS2 UCRT64 on the validated Windows 11 baseline. The ZIP uses a
single top-level directory and contains the GUI executable, Qt DLLs, MinGW
runtime DLLs, Qt platform/image/icon plugins, license/notice files and two
install helpers. The CLI executable and development files are excluded.

```sh
export QT_ROOT=/c/msys64/ucrt64
make gui-portable-windows
make gui-portable-check-windows
make gui-portable-repro-windows
make check-windows-subsystems
make test-gui-installer-windows
```

Portable use is unchanged: extract the complete ZIP and run
`xfoil.next-gui.exe` from the extracted folder. The optional installer is
`Install XFOIL.next GUI.cmd`; it calls
`Install-XFOIL.next-GUI.ps1` with `-NoProfile -ExecutionPolicy Bypass` for the
current process only. The installer copies the full runtime to
`%LOCALAPPDATA%\Programs\XFOIL.next GUI`, creates a Desktop shortcut and a
Start Menu shortcut, and does not request elevation, change the registry or use
the network. Re-running updates the same target and shortcuts.

The Windows check validates the ZIP layout, modes, timestamps, PE imports,
GUI subsystem, native icon resource and a clean-`PATH` GUI startup. The
PowerShell installer tests use temporary known-folder overrides and do not leave
shortcuts in the real user profile.

The Windows GUI executable is linked with `-mwindows`; the CLI and GUI test
programs keep their console subsystem.

## AppStream

The G8 local package deliberately uses `appimagetool --no-appstream`; no
AppStream release or screenshot metadata is invented for a test package.

## Licensing

Every GUI package contains `LICENSE`, `NOTICE`, `SOURCE-INFO.txt` and the
applicable Qt, QCustomPlot, GCC runtime and Windows runtime notices. The Linux
AppImage additionally includes the AppImage type-2 runtime notice.

## Tests

```sh
make test-gui-portable
```

The tests cover pinned-tool rejection, corrected PNG/ICO properties, forbidden
CLI/dev files, PE subsystem parsing, installer-script path checks and
deterministic ZIP construction/layout checks.

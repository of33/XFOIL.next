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

## Windows GUI ZIP

Build in MSYS2 UCRT64 on the validated Windows 11 baseline. The ZIP uses a
single top-level directory and contains the GUI executable, Qt DLLs, MinGW
runtime DLLs, Qt platform/image/icon plugins and license/notice files. The CLI
executable and development files are excluded.

```sh
export QT_ROOT=/c/msys64/ucrt64
make gui-portable-windows
make gui-portable-check-windows
make gui-portable-repro-windows
```

The Windows check validates the ZIP layout, modes, timestamps, PE imports and
a clean-`PATH` GUI startup when run on Windows.

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

The tests cover pinned-tool rejection, forbidden CLI/dev files and deterministic
ZIP construction/layout checks.

XFOIL.next GUI for Windows x86-64
==================================

This ZIP is a portable shared-Qt package. Extract the complete ZIP before use.
Do not copy only xfoil.next.exe to the desktop; it needs the Qt libraries
and plugin folders next to it.

Portable use
------------

1. Extract the complete ZIP to a folder of your choice.
2. Run xfoil.next.exe from that folder.
3. Keep the executable together with Qt6*.dll, lib*.dll and the plugin
   directories (platforms, imageformats, iconengines, styles, ...).

User installation
-----------------

1. Extract the complete ZIP.
2. Run "Install XFOIL.next GUI.cmd" from the extracted top-level folder.
3. The complete runtime is copied to:
   %LOCALAPPDATA%\Programs\XFOIL.next GUI
4. A Desktop shortcut and a Start Menu shortcut named "XFOIL.next GUI"
   are created. The Start Menu folder also contains
   "Uninstall XFOIL.next GUI".
5. No administrator rights are required.

The installer is per-user only. It does not install under Program Files,
does not change the registry, does not request elevation and does not use the
network. Re-running it updates the same installation and shortcuts.

The stable Start Menu shortcut can be pinned manually. Automatic taskbar or
Start pinning is not claimed.

Portable use remains supported. Copying only the EXE is not supported.

Uninstallation
--------------

Use "Uninstall XFOIL.next GUI" in the Start Menu, or run
"Uninstall XFOIL.next GUI.cmd" from:
%LOCALAPPDATA%\Programs\XFOIL.next GUI

The uninstaller runs for the current user only. It checks that the target is
the per-user installation managed by this package, removes the Desktop and
Start Menu shortcuts and the application folder, and then removes itself.
It does not need administrator rights, does not change the registry, does not
use the network and does not remove other XFOIL installations or shortcuts.

Close XFOIL.next GUI before uninstalling. The uninstaller accepts the current
managed executable inside the installation.

The uninstaller can also be started from an extracted ZIP. In that case it
removes the per-user installation and leaves the extracted portable folder
unchanged. If no per-user installation exists, it reports that a portable copy
is removed by deleting its extracted folder.

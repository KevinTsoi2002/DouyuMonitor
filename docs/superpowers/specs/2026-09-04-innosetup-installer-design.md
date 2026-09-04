# Inno Setup Windows Installer Design

## Goal

Replace the current IExpress plus hidden PowerShell installer with an Inno Setup 6 installer that reliably installs the Qt/libmpv application to a user-selected directory, including when a drive root such as `D:\` is selected.

## Scope

- Keep the existing Qt/C++ application, StreamGet service, Qt plugins, and libmpv DLLs unchanged.
- Replace only the packaging and installation orchestration layer.
- Preserve custom install location, Start Menu shortcut, desktop shortcut, post-install launch, and Windows uninstall registration.
- Produce a normal `.exe` installer and a `.exe` uninstaller.

## Architecture

The release build will stage the runtime files under `native/out/installer/stage/DouyuMonitor`. Inno Setup will compile that staged directory directly into a single setup executable. Its setup wizard will own directory selection, file copy, shortcut creation, uninstall registration, and success/failure reporting; no self-extracting ZIP or hidden PowerShell install process will be used.

The default destination will be `{localappdata}\Programs\DouyuMonitor`. If the user enters or selects a volume root (`C:\`, `D:\`, etc.), the installer will normalize it to `<root>\DouyuMonitor` before installation. The installer will reject a final target that is still a volume root and will display a visible validation error.

## Installer Behavior

1. Validate that the staged runtime contains:
   - `douyu_monitor_native.exe`
   - `streamget_service.exe`
   - `Uninstall DouyuMonitor.exe`
   - `douyu_monitor.ico`
   - `platforms\qwindows.dll`
2. Show a standard Inno Setup directory page.
3. Normalize or reject volume-root destinations before enabling installation.
4. Copy all staged files and Qt plugin directories.
5. Create Start Menu and optional desktop shortcuts.
6. Register the uninstall entry with the actual selected install directory.
7. Start the application only after all required files exist.
8. Show success only after post-install validation succeeds; otherwise show the error and leave a useful log.

The generated uninstaller will be Inno Setup's `.exe` uninstaller (`unins000.exe`). The legacy native `Uninstall DouyuMonitor.exe` payload will no longer be distributed or registered. The Inno uninstaller will remove installed program files, shortcuts, and the uninstall registration while leaving user data outside the install directory untouched.

## Build Integration

`native/scripts/build-windows-installer.ps1` will:

- keep the existing release-stage and runtime dependency validation;
- remove IExpress `.sed`, ZIP payload, and generated `install.ps1` creation;
- locate `ISCC.exe` from the standard Inno Setup installation paths or an explicit `INNO_SETUP_COMPILER` environment variable;
- invoke a checked-in `native/installer/DouyuMonitor.iss` script;
- fail clearly when Inno Setup is not installed;
- print the resulting installer path.

## Verification

- Compile the `.iss` script with `ISCC.exe`.
- Run the existing installer regression checks updated for Inno Setup.
- Inspect the generated installer archive/stage for all required runtime files.
- Perform an automated install into a temporary directory and verify the executable, Qt platform plugin, StreamGet service, shortcuts, registry entry, and uninstaller.
- Explicitly test a drive-root selection path such as `D:\` and verify the final target is `D:\DouyuMonitor`, never the root itself.
- Verify uninstall removes the installed program and registration.

## Non-Goals

- No Electron, Chromium, Qt WebEngine, or application runtime changes.
- No MSI/WiX migration in this phase.
- No GitHub release upload or version bump unless separately requested.

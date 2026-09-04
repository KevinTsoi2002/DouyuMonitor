# Inno Setup Installer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace IExpress and its hidden PowerShell installer with an Inno Setup 6 executable that normalizes a drive-root target into a product subfolder.

**Architecture:** PowerShell stages and validates the Qt runtime. A checked-in Inno `.iss` file packages that stage and owns directory selection, file copy, shortcuts, success reporting, and Inno's built-in `.exe` uninstaller.

**Tech Stack:** PowerShell 7, Inno Setup 6 (`ISCC.exe`), CMake/Ninja, Qt 6.

---

### Task 1: Define the Inno Setup contract

**Files:**
- Create: `native/installer/DouyuMonitor.iss`
- Modify: `native/scripts/test-installer-script.ps1`

- [ ] **Step 1: Write the failing test**

Add `Assert-InnoContains` and `Assert-NotContains` helpers. Require the missing `native/installer/DouyuMonitor.iss`, `INNO_SETUP_COMPILER`, `$innoCompiler`, `Source: "{#SourceDir}\*"`, `function IsDriveRoot`, `function NormalizeInstallDirectory`, `function PrepareToInstall`, and `Filename: "{uninstallexe}"`. Require removal of `iexpress.exe` and `install.ps1` references from the build script.

- [ ] **Step 2: Run the regression test and confirm RED**

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\native\scripts\test-installer-script.ps1`.

Expected: failure saying the Inno script or Inno contract is missing.

- [ ] **Step 3: Implement `DouyuMonitor.iss`**

Implement `/DSourceDir`, `/DOutputDir`, and `/DAppVersion` preprocessor inputs. Configure `DefaultDirName={localappdata}\Programs\DouyuMonitor`, `PrivilegesRequired=lowest`, `OutputBaseFilename=DouyuMonitor-Setup`, `Compression=lzma2/ultra64`, and recursive runtime packaging. Add Start Menu, optional desktop, and `{uninstallexe}` shortcuts. Use one normalization helper in both `NextButtonClick(wpSelectDir)` and `PrepareToInstall`: transform `D:` and `D:\` to `D:\DouyuMonitor`; reject any final root path.

- [ ] **Step 4: Run the regression test and confirm GREEN**

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\native\scripts\test-installer-script.ps1`.

Expected: `installer script regression test passed`.

- [ ] **Step 5: Commit the Inno contract**

Run `git add native/installer/DouyuMonitor.iss native/scripts/test-installer-script.ps1` and `git commit -m "build: define Inno Setup installer contract"`.

### Task 2: Replace IExpress build orchestration

**Files:**
- Modify: `native/scripts/build-windows-installer.ps1`
- Modify: `native/scripts/test-installer-script.ps1`

- [ ] **Step 1: Extend the failing test**

Require compiler discovery from `INNO_SETUP_COMPILER`, `C:\Program Files (x86)\Inno Setup 6\ISCC.exe`, and `C:\Program Files\Inno Setup 6\ISCC.exe`; require CMake-version parsing, `.iss` invocation, output existence checking, and exclusion of `douyu_monitor_uninstaller.exe` from the payload.

- [ ] **Step 2: Run the regression test and confirm RED**

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\native\scripts\test-installer-script.ps1`.

Expected: targeted failure for the remaining IExpress build implementation.

- [ ] **Step 3: Implement the minimal build migration**

Retain stage creation and `verify_runtime_dependencies.cmake`. Remove ZIP, SED, generated `install.ps1`, and IExpress execution. Locate `ISCC.exe` from the environment override and conventional paths; throw `Inno Setup 6 compiler (ISCC.exe) was not found. Install Inno Setup 6 or set INNO_SETUP_COMPILER.` if absent. Invoke the `.iss` script with stage path, output path, and the version parsed from `native/CMakeLists.txt`; require `native/out/installer/DouyuMonitor-Setup.exe` afterward.

- [ ] **Step 4: Verify build handoff**

Run the static regression check, then `cmake --build --preset windows-x64-release --target douyu_monitor_native streamget_service -- -j1`, then the installer build script. The last command must either print the installer path or fail only with the explicit missing-Inno prerequisite.

- [ ] **Step 5: Commit the build migration**

Run `git add native/scripts/build-windows-installer.ps1 native/scripts/test-installer-script.ps1` and `git commit -m "build: package Windows release with Inno Setup"`.

### Task 3: Document and verify the release artifact

**Files:**
- Modify: `README.md`
- Modify: `native/README.md`
- Modify: `docs/文件职责索引.md`

- [ ] **Step 1: Update documentation**

Replace IExpress and `DOUYU_INSTALL_ROOT` references with Inno Setup 6. Document that selecting `D:\` installs into `D:\DouyuMonitor`, and that `unins000.exe` is the supported `.exe` uninstaller.

- [ ] **Step 2: Build with Inno Setup**

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\native\scripts\build-windows-installer.ps1` and require `native/out/installer/DouyuMonitor-Setup.exe`.

- [ ] **Step 3: Verify install and uninstall**

Run the installer with `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="<temporary>\DouyuMonitor"`. Verify `douyu_monitor_native.exe`, `streamget_service.exe`, `platforms\qwindows.dll`, and `unins000.exe`; run `unins000.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART`; verify the temporary install directory is gone. Repeat root-path behavior only on a disposable test drive and require `<root>\DouyuMonitor`.

- [ ] **Step 4: Commit docs**

Run `git add README.md native/README.md docs/文件职责索引.md docs/superpowers/specs/2026-09-04-innosetup-installer-design.md` and `git commit -m "docs: document Inno Setup installer workflow"`.

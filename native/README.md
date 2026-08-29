# Native M0 Bootstrap

This directory contains the Qt Quick/QML + libmpv Windows x64 client.

## Current state

- The vcpkg manifest is pinned to the local registry baseline recorded in
  `vcpkg-configuration.json`.
- The shipped runtime uses Qt GUI, QML, Qt Quick, OpenGL, and Testlib.
- `MpvQuickItem` owns the GUI-thread libmpv handle and OpenGL render context.
- `MpvQuickItem` can load a local media file and reports the first rendered frame
  for deterministic playback tests.
- Playback state is explicit: `Idle`, `Loading`, `Playing`, `Paused`, `Ended`,
  or `Error`; pause/resume uses libmpv's native property.
- `Main.qml` is the shipped desktop shell and binds UI actions through
  `AppController`.
- libmpv is not included in vcpkg; CMake requires an explicitly selected
  `MPV_ROOT` containing the headers, runtime DLL and MSVC import library.

## Configure

Install the pinned dependencies with the bootstrap script. The script uses
the official Qt repository through the pinned `aqtinstall` version and checks
the libmpv archive and runtime SHA-256 values before generating an MSVC import
library from the DLL exports.

```powershell
.\native\scripts\bootstrap-dependencies.ps1
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\native\sdk\mpv).Path
```

Then run from a Visual Studio x64 developer prompt:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
```

The Debug executable uses Qt debug libraries and requires the Visual Studio
debug runtime when launched outside a developer prompt. For a normal
double-clickable build, configure and build the Release preset instead:

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target douyu_monitor_native
.\native\out\build\windows-x64-release\douyu_monitor_native.exe
```

The post-build deployment runs `windeployqt` with the matching `--debug` or
`--release` mode and fails the build if the matching Windows platform plugin is
missing (`platforms/qwindowsd.dll` for Debug or `platforms/qwindows.dll` for
Release). Do not copy Qt DLLs or platform plugins between the two build
directories.

The dependency probe and Qt Quick tests are covered by CTest. The native
executable supports `--media <path>` during `--self-test`; the self-test loads
the QML application, waits for a Qt Quick OpenGL render context, and exits
with a non-zero status if libmpv is not ready. Combining the options makes the
self-test wait for the first rendered frame as well.

For a deterministic media lifecycle check, run:

```powershell
.\native\out\build\windows-x64\douyu_monitor_native.exe --self-test --media <path-to-local-media>
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R native_self_test_media
```

The media self-test covers `load`, `first-frame`, `stop`, and `release`, and
prints a fixed Qt Quick renderer summary. It uses only the supplied local path
and does not print media paths, URLs, cookies, tokens, or raw libmpv
diagnostics.

## Qt-only StreamGet service

The maintained product runtime is Qt Quick/QML + C++ and libmpv. The Qt
application starts one `streamget_service.exe` child per application through
`QProcess`. The child owns Douyu discovery and StreamGet resolution, and
communicates through private stdin/stdout JSONL.

Bootstrap the pinned Python service environment from PowerShell:

```powershell
.\native\scripts\bootstrap-streamget-service.ps1
```

The bootstrap creates `native\.venv` and installs exactly `streamget==4.0.10`
and `pyinstaller==6.22.0`. Build the standalone child package with:

```powershell
.\native\scripts\build-streamget-service.ps1
```

The script prints only the resulting path:
`native\out\service\streamget_service.exe`.

The resolver URL exists only in the service protocol payload and Qt memory
needed to start playback. It is never persisted, logged, written to stderr, or
copied into crash artifacts. Service and native tests use fake data and do not
require real Douyu credentials or network access.

When Python is available during CMake configure, the service unit tests are
registered as `streamget_service_python_tests`; disable them with
`-DDOUYU_BUILD_SERVICE_TESTS=OFF` only for environments without Python.

## Windows installer

After building the Release target, create a self-contained Windows x64
installer with the built-in IExpress tool:

```powershell
.\native\scripts\build-windows-installer.ps1
```

The output is `native\out\installer\DouyuMonitor-Setup.exe`. It installs the
Qt Quick application and bundled StreamGet service under the current user's
`%LOCALAPPDATA%\Programs\DouyuMonitor` and creates a Start Menu shortcut.

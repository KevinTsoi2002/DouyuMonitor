# Native M0 Bootstrap

This directory is the start of the Qt + libmpv Windows x64 client. It is
independent from the legacy Electron entrypoints.

## Current state

- The vcpkg manifest is pinned to the local registry baseline recorded in
  `vcpkg-configuration.json`.
- M0 only requests Qt GUI, OpenGL, Widgets and Testlib features.
- `PlayerSurface` owns the GUI-thread libmpv handle and OpenGL render context.
- `PlayerSurface` can load a local media file and reports the first rendered frame
  for deterministic native playback tests.
- Playback state is explicit: `Idle`, `Loading`, `Playing`, `Paused`, `Ended`,
  or `Error`; pause/resume uses libmpv's native property.
- The single-window prototype exposes the pause/resume control through a Qt
  toolbar button with native media icons.
- `MainWindow` is a single-window prototype with one central `PlayerSurface`.
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

The dependency probe and the two GUI unit tests are covered by CTest. The
`PlayerSurface` test uses a generated local PPM fixture; it never contacts a
remote service or records a playback URL. The native executable also supports
`--media <path>` for a local file and `--self-test`, which opens the window,
waits for the OpenGL render context, and exits with a non-zero status if
libmpv is not ready. Combining them makes the self-test wait for the first
rendered frame as well.

For a deterministic media lifecycle check, run:

```powershell
.\native\out\build\windows-x64\douyu_monitor_native.exe --self-test --media <path-to-local-media>
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R native_self_test_media
```

The media self-test covers `load`, `first-frame`, `stop`, and `release`, and
prints the fixed summary `native self-test passed: load first-frame stop
release`. It uses only the supplied local path and does not print media paths,
URLs, cookies, tokens, or raw libmpv diagnostics.

## Qt-only StreamGet service

The maintained product runtime is Qt + libmpv. The Qt application starts one
`streamget_service.exe` child per application through `QProcess`; it does not
use Node, Electron, React, or a TypeScript compatibility layer. The child owns
Douyu discovery and StreamGet resolution, and communicates through private
stdin/stdout JSONL.

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

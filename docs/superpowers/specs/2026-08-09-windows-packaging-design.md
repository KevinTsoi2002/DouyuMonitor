# Windows Packaging Design

**Date:** 2026-08-09

## Goal

Produce a Windows x64 NSIS installer that can resolve Douyu streams without requiring the user to install Python or StreamGet.

## Architecture

- PyInstaller builds `scripts/streamget_bridge.py` and its Python dependencies into `streamget_bridge.exe`.
- electron-builder packages the Electron application and copies the sidecar executable to `resources/streamget/streamget_bridge.exe`.
- Development keeps the current `.venv` plus Python script path.
- Packaged runtime launches only the bundled executable. It does not fall back to a system Python installation.
- `STREAMGET_PYTHON` remains a development override and is ignored by the packaged executable path.

## Security Boundary

- The sidecar continues to emit one JSON object and never logs stream URL query values.
- The Electron main process keeps the current CDN allowlist validation.
- The package contains no cookies, credentials, room history, stream URLs, or generated test profile.

## Distribution

- Target: Windows x64 NSIS installer.
- Installer output: `release/`.
- App data and workspace settings remain under Electron's normal per-user application-data directory.
- Code signing and auto-update remain separate release tasks because no certificate or update endpoint has been provided.

## Verification

- Unit tests cover development and packaged sidecar launch resolution.
- The PyInstaller executable resolves an online room with sanitized output checks.
- The unpacked Electron application starts with a temporary user-data directory and produces a playable 1280×720 video for room 63136 when the room is online.
- The final installer is created and its artifact metadata is reported without executing an installation.

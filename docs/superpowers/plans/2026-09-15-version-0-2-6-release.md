# DouyuMonitor V0.2.6 Release Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Update DouyuMonitor to version 0.2.6, build and validate the Windows installer, and publish the release assets to GitHub.

**Architecture:** Keep the existing CMake version as the single build-time source, propagate it to the installer and package metadata, and update user-facing release references. Preserve the existing uncommitted danmaku crash fix and logging changes.

**Tech Stack:** CMake, Qt 6, MSVC 2022 Build Tools, Inno Setup 6, PowerShell, GitHub CLI.

---

### Task 1: Version Metadata

**Files:**
- Modify: `native/CMakeLists.txt`
- Modify: `native/vcpkg.json`
- Modify: `native/tests/app_controller_test.cpp`
- Modify: `README.md`
- Modify: `native/README.md`

- [ ] Update all release metadata and current-version assertions from `0.2.5` to `0.2.6`, while leaving historical update-checker fixtures unchanged.
- [ ] Run the focused version/update tests and installer script regression check.

### Task 2: Release Build

**Files:**
- Generated: `native/out/build/windows-x64-release/*`

- [ ] Configure/build the Release preset with the installed VS Build Tools environment.
- [ ] Run Release self-test and full Release CTest.

### Task 3: Installer and GitHub Release

**Files:**
- Generated: `native/out/installer/DouyuMonitor-Setup-V0.2.6.exe`

- [ ] Build the Inno Setup installer and verify the payload/stage checks.
- [ ] Review the final diff and commit all intended source/version changes without deleting existing diagnostic files.
- [ ] Push `main`, create tag `V0.2.6`, and publish a GitHub Release with the installer attached.
- [ ] Verify the remote tag and release asset through GitHub CLI.

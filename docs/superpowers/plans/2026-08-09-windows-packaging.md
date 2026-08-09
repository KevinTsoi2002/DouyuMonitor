# Windows Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a self-contained Windows x64 installer with a bundled StreamGet sidecar executable.

**Architecture:** Resolve the sidecar launch command through one tested helper: Python plus script during development, bundled EXE after packaging. Build the EXE with PyInstaller and copy it through electron-builder extra resources.

**Tech Stack:** Electron, electron-builder, NSIS, Python, PyInstaller, StreamGet.

---

### Task 1: Resolve packaged sidecar paths

**Files:**
- Modify: `src/main/streamget-bridge.ts`
- Modify: `src/main/main.ts`
- Test: `tests/streamget-bridge.test.ts`

- [ ] Write failing tests for development and packaged launch commands.
- [ ] Add the minimal launch resolver and connect `app.isPackaged` plus `process.resourcesPath`.
- [ ] Run the targeted tests and typecheck.

### Task 2: Build the Python sidecar executable

**Files:**
- Create: `requirements-streamget-build.txt`
- Modify: `package.json`
- Modify: `package-lock.json`

- [ ] Pin PyInstaller separately from runtime dependencies.
- [ ] Add a deterministic Windows sidecar build command.
- [ ] Build the EXE and run a sanitized online-room smoke test.

### Task 3: Package the Electron application

**Files:**
- Modify: `package.json`
- Modify: `package-lock.json`
- Modify: `README.md`

- [ ] Add electron-builder and the x64 NSIS configuration.
- [ ] Build the unpacked application and verify the bundled sidecar location.
- [ ] Build the installer without code signing and report its artifact metadata.

### Task 4: Verify and document

- [ ] Run all tests, typecheck, build, and dependency audit.
- [ ] Start the unpacked application with a temporary profile and verify real playback.
- [ ] Update Notion with installer status, artifact metadata, and remaining code-signing limit.

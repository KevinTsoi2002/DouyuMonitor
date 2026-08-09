# Electron Desktop Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a secure Electron main/preload shell and typed IPC search boundary around the existing React workspace without coupling the renderer to Electron or real Douyu protocol details.

**Architecture:** Shared IPC contracts define channels, request validation, and result envelopes. The main process registers handlers against a `DouyuAdapter`; the preload exposes a small `window.appApi` bridge through context isolation. The renderer remains usable in Vite browser mode and continues to use the mock adapter until Phase 0 protocol validation is complete.

**Tech Stack:** Electron, TypeScript, Node.js path utilities, existing React/Vite/Zustand stack, Vitest.

---

### Task 1: Shared IPC contract

**Files:**
- Create: `src/shared/ipc-contract.ts`
- Create: `tests/ipc-contract.test.ts`

- [ ] **Step 1: Write failing tests** for stable channel names, bounded search input validation, success/error result shapes, and safe error serialization.
- [ ] **Step 2: Run `npm test -- tests/ipc-contract.test.ts`** and confirm failure is caused by the missing contract module.
- [ ] **Step 3: Implement the contract types and pure validators** with a 200-character input limit and no raw error/detail leakage.
- [ ] **Step 4: Run the focused test and the full suite.**

### Task 2: Main-process handlers

**Files:**
- Create: `src/main/ipc-handlers.ts`
- Create: `tests/ipc-handlers.test.ts`

- [ ] **Step 1: Write failing tests** with a fake `ipcMain.handle` registry for numeric search, anchor search, blank input rejection, and adapter failure mapping.
- [ ] **Step 2: Run the focused tests and confirm the handler module is missing.**
- [ ] **Step 3: Implement `registerIpcHandlers`** using only the `DouyuAdapter` interface and shared result helpers.
- [ ] **Step 4: Run focused and full tests.**

### Task 3: Preload bridge

**Files:**
- Create: `src/preload/bridge.ts`
- Create: `src/preload/preload.ts`
- Create: `src/shared/window-api.d.ts`
- Create: `tests/preload-bridge.test.ts`

- [ ] **Step 1: Write failing tests** proving bridge methods invoke only approved channels and preserve typed result envelopes.
- [ ] **Step 2: Implement `createAppApi`** against a narrow `ipcRenderer.invoke` interface.
- [ ] **Step 3: Expose the bridge with `contextBridge.exposeInMainWorld`** and no Node or Electron objects.
- [ ] **Step 4: Run focused and full tests.**

### Task 4: Electron entrypoint and build scripts

**Files:**
- Create: `src/main/main.ts`
- Create: `tsconfig.main.json`
- Modify: `package.json`

- [ ] **Step 1: Add Electron and Node type dependencies.**
- [ ] **Step 2: Configure an ESM main/preload build to `dist/main` and `dist/preload`.**
- [ ] **Step 3: Add `build:main`, `build:renderer`, and combined `build` scripts** while keeping `npm run dev` browser-compatible.
- [ ] **Step 4: Implement secure `BrowserWindow` defaults** (`contextIsolation: true`, `nodeIntegration: false`, preload path, dev URL fallback, production file loading).
- [ ] **Step 5: Run tests, typecheck, renderer build, and main build.**

### Task 5: Verification and Notion progress

- [ ] **Step 1: Run `npm test`, `npm run typecheck`, and `npm run build`.**
- [ ] **Step 2: Start the Vite renderer and repeat browser smoke checks to ensure IPC scaffolding does not regress the web prototype.**
- [ ] **Step 3: Update the Notion development design page with the shell status and explicit remaining protocol limitations.**

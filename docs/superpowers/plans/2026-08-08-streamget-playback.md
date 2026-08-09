# StreamGet FLV Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Play a Douyu FLV returned by StreamGet in the Electron multi-room canvas while keeping signed web-resolution logic out of the project.

**Architecture:** A bounded Python sidecar call uses only StreamGet's app-search resolver. Electron validates and forwards one temporary FLV URL through the existing typed availability contract. The renderer uses mpegts.js to attach and destroy one player per room tile.

**Tech Stack:** TypeScript, Electron, React, Vitest, Python 3.10+, StreamGet 4.x, mpegts.js.

---

### Task 1: Add failing resolver and IPC tests

**Files:**
- Create: `tests/streamget-bridge.test.ts`
- Create: `tests/streamget-douyu-adapter.test.ts`
- Create: `tests/streamget-ipc-error.test.ts`

- [x] Add tests for a JSON-line response containing a valid Douyu CDN FLV URL, a malformed response, and an offline response.
- [x] Add tests for mapping live, offline, unsafe-host, and dependency-error results through the existing availability contract.
- [x] Run the focused tests; they must fail because the bridge and adapter do not exist.

### Task 2: Implement the Python sidecar and main bridge

**Files:**
- Create: `scripts/streamget_bridge.py`
- Create: `src/main/streamget-bridge.ts`
- Create: `src/infrastructure/streamget-douyu-adapter.ts`
- Modify: `src/domain/douyu-adapter.ts`

- [x] Make the sidecar accept one room ID, call `fetch_app_stream_data`, and emit only `{roomId,isLive,flvUrl}` or `{roomId,error}`.
- [x] Keep Python stderr out of the IPC response and return stable error codes.
- [x] Validate protocols and allow only hosts ending in `.douyucdn.cn`, `.douyucdn2.cn`, or `.edgesrv.com`.
- [x] Map one valid URL to an `available` FLV variant with quality `auto`; map offline to `ROOM_OFFLINE`.
- [x] Re-run focused bridge and adapter tests until green.

### Task 3: Wire the typed Electron boundary

**Files:**
- Modify: `src/main/main.ts`
- Modify: `src/shared/ipc-contract.ts`
- Modify: `vite.main.config.ts`
- Modify: `tests/main-config.test.ts`

- [x] Wrap the production HTTP adapter with the StreamGet-backed adapter.
- [x] Map resolver failures through the sanitized `STREAMGET_UNAVAILABLE` error code.
- [x] Keep the existing `playback.getAvailability` preload allowlist unchanged.
- [x] Run IPC, preload, typecheck, and build checks.

### Task 4: Add FLV renderer playback

**Files:**
- Modify: `package.json`
- Modify: `package-lock.json`
- Create: `src/renderer/components/FlvVideo.tsx`
- Modify: `src/renderer/components/RoomPlaybackSurface.tsx`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `src/renderer/styles.css`
- Create: `tests/flv-video.test.tsx`
- Modify: `tests/room-playback-surface.test.tsx`

- [x] Add `mpegts.js` and use its documented `createPlayer`, `attachMediaElement`, `load`, `play`, and `destroy` lifecycle.
- [x] Render the video only for a real available FLV variant outside demo mode.
- [x] Keep the danmaku overlay above the video and honor the audio focus room.
- [x] Render a stable error state when the player reports an error.
- [x] Keep the existing mock scene unchanged in browser demo mode.

### Task 5: Verification and runtime smoke

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-08-08-streamget-playback.md`

- [x] Add Python setup instructions and the excluded web-signature boundary.
- [x] Run `npm test`, `npm run typecheck`, and `npm run build`.
- [x] Install StreamGet in a project-local Python environment and run a sanitized sidecar smoke test for an online room.
- [x] Start Vite and inspect the browser demo with Playwright.
- [x] Start rebuilt Electron, add an online room, verify a video element and danmaku overlay, then close the app and confirm the sidecar exits.

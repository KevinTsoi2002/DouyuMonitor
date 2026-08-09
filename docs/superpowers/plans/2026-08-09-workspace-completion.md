# Douyu workspace completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** Complete the P0 workspace behaviors described in Notion: local restore, drag ordering, global danmaku/mute controls, per-room volume, and recoverable playback diagnostics.

**Architecture:** Keep transient stream URLs and runtime status in memory, and persist only a versioned, non-sensitive workspace snapshot through a small storage adapter. Extend the existing Zustand store as the single source of truth; UI components consume global state and pass effective danmaku/audio settings to the existing independent room surfaces.

**Tech Stack:** Electron, React, TypeScript, Zustand, Vitest, Playwright.

---

### Task 1: Persist workspace state safely

**Files:**
- Create: `src/renderer/store/workspace-persistence.ts`
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `src/renderer/store/workspace-context.tsx`
- Test: `tests/workspace-persistence.test.ts`
- Test: `tests/workspace-store.test.ts`

- [x] Define a `WorkspaceSnapshot` with `schemaVersion`, room metadata/order, layout, primary room, audio room, global danmaku, global mute, and per-room volume/quality. Exclude playback URLs, tokens, danmaku messages, and transient status.
- [x] Add safe `loadWorkspaceSnapshot`/`saveWorkspaceSnapshot` helpers that tolerate malformed data and unknown schema versions.
- [x] Hydrate the store from storage when present, fall back to the supplied runtime seed, and refresh stream availability for restored rooms.
- [x] Persist after every user-visible workspace mutation; expose `clearPersistedWorkspace` for settings/tests.
- [x] Add tests for round-trip serialization, secret exclusion, malformed data fallback, and restore order/settings.

### Task 2: Add ordering and global controls to the store

**Files:**
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `src/renderer/store/danmaku-context.tsx`
- Test: `tests/workspace-store.test.ts`
- Test: `tests/danmaku-store.test.ts`

- [x] Add `reorderRooms(sourceRoomId, targetRoomId)`, `setGlobalDanmakuEnabled`, `setGlobalMuted`, and `setVolume` actions with validation and persistence.
- [x] Make danmaku connections synchronize against `globalDanmakuEnabled && room.danmakuEnabled` while retaining each room's individual preference.
- [x] Make global mute suppress every player without changing the selected audio focus.
- [x] Verify removed rooms and late async availability results cannot mutate the restored workspace.

### Task 3: Wire UI interactions and stable controls

**Files:**
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/components/RoomSidebar.tsx`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `src/renderer/components/RoomPlaybackSurface.tsx`
- Modify: `src/renderer/components/FlvVideo.tsx`
- Modify: `src/renderer/components/WorkspaceGrid.tsx`
- Modify: `src/renderer/styles.css`
- Test: `tests/flv-video.test.tsx`
- Test: `tests/app-smoke.test.tsx`

- [x] Add icon-button global mute and global danmaku controls with accessible names and active state.
- [x] Add HTML5 drag-and-drop plus retained keyboard move buttons in the room sidebar.
- [x] Add a bounded per-room volume slider and pass effective muted/volume values to the FLV player.
- [x] Ensure global danmaku state hides overlays without changing room-level toggles and that controls remain stable at narrow widths.

### Task 4: Verify and document

**Files:**
- Modify: `README.md`
- Modify: Notion pages `斗鱼多直播间视角整合软件｜需求分析 v0.1` and `斗鱼多直播间视角整合软件｜项目开发设计 v0.1`

- [x] Run `npm test`, `npm run typecheck`, `npm run build`, and `npm audit --omit=dev`.
- [x] Run the Vite app with Playwright at desktop and mobile sizes; verify global controls and persistence across reload. Drag ordering is covered by store tests; browser drag simulation was not reliable in the in-app automation surface.
- [x] Record the verified implementation status, current StreamGet/quality limitations, and remaining 1/4/6/9 performance and Windows packaging checks in Notion.

### Task 5: Room action menu

**Files:**
- Create: `docs/superpowers/specs/2026-08-09-room-actions-menu-design.md`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `src/renderer/styles.css`
- Test: `tests/app-smoke.test.tsx`

- [x] Add the fixed-size overflow menu with retry, primary-room, remove, and diagnostic actions.
- [x] Reuse existing workspace actions and keep diagnostics free of sensitive playback data.
- [x] Verify the menu interaction in the browser at desktop and mobile sizes.

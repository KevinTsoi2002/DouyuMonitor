# Douyu Multi-Room Workspace Prototype Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a runnable React workspace that demonstrates adding up to nine rooms, room-local danmaku, quality selection, layout switching, room ordering, primary-room selection, and single-room audio focus using a replaceable mock Douyu adapter.

**Architecture:** Keep Douyu-specific lookups behind `DouyuAdapter`, put deterministic workspace behavior in a vanilla Zustand store, and keep React components focused on rendering and interaction. Room media sessions are represented by stable room records so layout changes only reposition DOM nodes and never recreate application state.

**Tech Stack:** React 19, TypeScript, Vite, Zustand 5, Lucide React, Vitest.

**Approved source:** [Notion development design](https://app.notion.com/p/3b40a2056c2981b1abc7cd80cb0e231d)

---

### Task 1: Workspace state and mock adapter

**Files:**
- Create: `src/domain/douyu-adapter.ts`
- Create: `src/infrastructure/mock-douyu-adapter.ts`
- Create: `src/renderer/store/workspace-store.ts`
- Test: `tests/workspace-store.test.ts`
- Test: `tests/mock-douyu-adapter.test.ts`

- [ ] **Step 1: Write failing tests** for room lookup, duplicate/limit protection, first-room audio focus, layout changes, reordering, primary-room selection, quality changes, and danmaku toggling.
- [ ] **Step 2: Run `npm test -- tests/workspace-store.test.ts tests/mock-douyu-adapter.test.ts`** and confirm failure is caused by the missing modules.
- [ ] **Step 3: Define `DouyuAdapter`, room candidate/session types, and a deterministic mock implementation** that resolves numeric room IDs directly and searches known mock anchors by name.
- [ ] **Step 4: Implement `createWorkspaceStore(adapter)`** with async search plus synchronous room and display actions; return explicit user-facing errors for duplicate and limit cases.
- [ ] **Step 5: Run the focused tests and then `npm test`**; expect all assertions to pass.

### Task 2: React workspace shell

**Files:**
- Create: `src/main.tsx`
- Create: `src/renderer/App.tsx`
- Create: `src/renderer/store/workspace-context.tsx`
- Create: `src/renderer/components/AppHeader.tsx`
- Create: `src/renderer/components/RoomSidebar.tsx`
- Create: `src/renderer/components/AddRoomDialog.tsx`
- Create: `src/renderer/components/LayoutMenu.tsx`

- [ ] **Step 1: Create a store provider and selector hook** so the app owns one store instance and components subscribe only to needed state.
- [ ] **Step 2: Build the header and sidebar** with icon buttons, tooltips, visible room health, selected layout, audio focus, and add/remove controls.
- [ ] **Step 3: Build the add-room dialog** with input resolution, loading, search results, duplicate/limit errors, keyboard dismissal, and semantic dialog markup.
- [ ] **Step 4: Keep `App.tsx` as composition glue** and avoid a monolithic component.

### Task 3: Media grid and room-local controls

**Files:**
- Create: `src/renderer/components/WorkspaceGrid.tsx`
- Create: `src/renderer/components/RoomTile.tsx`
- Create: `src/renderer/components/DanmakuOverlay.tsx`
- Create: `src/renderer/data/mock-danmaku.ts`

- [ ] **Step 1: Render slots from `calculateLayout`** using stable `roomId` keys and CSS grid placement.
- [ ] **Step 2: Add room-local controls** for audio focus, quality, danmaku visibility, primary-room selection, and removal.
- [ ] **Step 3: Render deterministic simulated video frames and moving danmaku rows** with reduced-motion support and no control overlap.
- [ ] **Step 4: Implement pointer-based room reordering** through explicit move-left/move-right controls; native drag-and-drop remains a later enhancement because keyboard parity is required.

### Task 4: Responsive visual system

**Files:**
- Create: `src/renderer/styles.css`

- [ ] **Step 1: Define neutral dark monitoring tokens** with orange live accents, green health status, compact 4-8 px radii, stable toolbar dimensions, and zero letter spacing.
- [ ] **Step 2: Implement desktop layout** with a fixed sidebar, full-width media canvas, bounded controls, and no nested card surfaces.
- [ ] **Step 3: Implement compact viewport behavior** with a collapsible visual hierarchy, horizontally scrollable toolbar groups, readable room metadata, and non-overlapping tiles.
- [ ] **Step 4: Add focus-visible, hover, disabled, loading, empty, offline, and error states.**

### Task 5: Verification and progress tracking

**Files:**
- Modify: `package.json` only if a missing verification script is required.
- Update: Notion development design page progress section.

- [ ] **Step 1: Run `npm test`, `npm run typecheck`, and `npm run build`.**
- [ ] **Step 2: Start `npm run dev` on an available localhost port.**
- [ ] **Step 3: Validate app load, console health, add-room flow, layout selection, room controls, and responsive behavior with the Browser plugin.**
- [ ] **Step 4: Inspect desktop and compact screenshots for clipping, overlap, layout shift, color balance, and text overflow.**
- [ ] **Step 5: Append a dated progress update to the Notion development design page** with completed scope, verification evidence, limitations, and next phase.

## Scope Boundaries

- This phase uses mock playback frames and mock danmaku data.
- Real Douyu stream discovery, signing, WebSocket protocol handling, Electron main/preload processes, and packaging are separate follow-up phases.
- No cookies, signatures, playback URLs, credentials, or danmaku history are persisted.
- There is no send-danmaku, recording, download, rebroadcast, or access-control bypass behavior.

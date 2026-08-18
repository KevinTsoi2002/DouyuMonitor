# Multi-Audio Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persisted single-focus and simultaneous multi-room audio modes without remounting healthy players.

**Architecture:** Add an `audioMode` field to the workspace store and snapshot schema with `single` as the backward-compatible default. Compute each tile's `muted` prop from global mute, room availability, and the selected audio mode; switching modes only changes props on existing player nodes. Expose the mode as a compact top-bar control beside the global volume control.

**Tech Stack:** React, Zustand vanilla store, Electron renderer, Vitest, TypeScript.

---

### Task 1: Extend the workspace audio state and persistence

**Files:**
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `src/renderer/store/workspace-persistence.ts`
- Test: `tests/workspace-store.test.ts`
- Test: `tests/workspace-persistence.test.ts`

- [ ] **Step 1: Add a failing store test**

Add tests that a new store defaults to `audioMode: 'single'`, `setAudioMode('multi')` changes only the mode, and the existing `audioRoomId`, room volume, and room list remain unchanged.

- [ ] **Step 2: Add a failing persistence test**

Add a snapshot round-trip test for `audioMode: 'multi'` and a legacy snapshot test asserting that a snapshot without `audioMode` loads as `single`.

- [ ] **Step 3: Implement the state field and action**

Add:

```ts
export type AudioMode = 'single' | 'multi';
```

to `workspace-store.ts`, add `audioMode` and `setAudioMode` to `WorkspaceState`, initialize to `persisted?.audioMode ?? 'single'`, and persist it from the store snapshot. Clamp unknown values to `single` when loading.

- [ ] **Step 4: Extend workspace snapshot and preset data**

Add the same `audioMode` field to `WorkspaceSnapshot` and `WorkspacePreset`, parse it with the `single` fallback, and include it in preset draft creation and preset loading. Keep older snapshots valid without migration errors.

- [ ] **Step 5: Run focused tests**

Run `npx vitest run tests/workspace-store.test.ts tests/workspace-persistence.test.ts`. Expected: all tests pass.

- [ ] **Step 6: Commit**

```powershell
git add src/renderer/store/workspace-store.ts src/renderer/store/workspace-persistence.ts tests/workspace-store.test.ts tests/workspace-persistence.test.ts
git commit -m "feat: persist audio playback mode"
```

### Task 2: Apply mode-aware mute behavior without player remounts

**Files:**
- Modify: `src/renderer/components/RoomTile.tsx`
- Test: `tests/room-playback-surface.test.tsx`
- Test: `tests/app-smoke.test.tsx`

- [ ] **Step 1: Add a failing mute-policy test**

Extract or export a pure helper from `RoomTile.tsx`:

```ts
getRoomMuted(room, audioMode, audioRoomId, globalMuted): boolean
```

Test that single mode unmutes only the focus room, multi mode unmutes every online room with `playbackAvailabilityStatus === 'available'`, and `globalMuted` always wins.

- [ ] **Step 2: Implement the helper and wire it to `RoomPlaybackSurface`**

Read `audioMode` from `useWorkspace`, then replace `muted={!hasAudioFocus || globalMuted}` with the helper result. Keep the existing `key={variant.playbackUrl}` unchanged so mode switches update the `muted` prop without remounting a healthy player.

- [ ] **Step 3: Add UI smoke assertions**

Assert that the header exposes an accessible audio-mode control and that the existing global mute control remains present.

- [ ] **Step 4: Run focused tests**

Run `npx vitest run tests/room-playback-surface.test.tsx tests/app-smoke.test.tsx`. Expected: all tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/renderer/components/RoomTile.tsx tests/room-playback-surface.test.tsx tests/app-smoke.test.tsx
git commit -m "feat: apply multi-room audio mute policy"
```

### Task 3: Add the top-bar mode switch

**Files:**
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/styles.css`
- Test: `tests/app-smoke.test.tsx`

- [ ] **Step 1: Add a failing interaction assertion**

Render the app with a workspace provider, locate the audio mode control by accessible label, click it, and assert its `aria-pressed` state and label change from single to multi. Add the reverse assertion for switching back.

- [ ] **Step 2: Implement the compact segmented control**

Place a two-button `单声道`/`多声道` control beside the volume icon. Bind both buttons to `setAudioMode`, expose `role="group"`, `aria-pressed`, and titles, and keep global mute as a separate control.

- [ ] **Step 3: Add restrained styling**

Use the existing header colors and compact dimensions; do not change player layout or add explanatory copy to the main canvas.

- [ ] **Step 4: Run the interaction tests**

Run `npx vitest run tests/app-smoke.test.tsx`. Expected: all tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/renderer/components/AppHeader.tsx src/renderer/styles.css tests/app-smoke.test.tsx
git commit -m "feat: add audio mode switch"
```

### Task 4: Verify full integration

**Files:**
- Test: `tests/workspace-presets-persistence.test.ts`
- Test: `tests/workspace-store.test.ts`

- [ ] **Step 1: Verify preset persistence**

Add an assertion that saving and loading a workspace preset preserves `audioMode` while retaining `audioRoomId`, room volume, and global mute.

- [ ] **Step 2: Run the full test suite**

Run `npm test`. Expected: all test files pass with zero failures.

- [ ] **Step 3: Run typecheck and production build**

Run `npm run typecheck` and `npm run build`. Expected: both commands exit with code 0.

- [ ] **Step 4: Inspect the final diff and commit**

Run `git status --short` and `git log --oneline -5`, confirm only the audio-mode changes are present, then commit any remaining test-only changes with:

```powershell
git add tests/workspace-presets-persistence.test.ts
git commit -m "test: cover audio mode preset persistence"
```

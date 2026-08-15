# 周期性直播状态刷新实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为生产模式的每个直播间增加错峰、可退避、可清理的周期性资料和开播状态刷新。

**Architecture:** 在 `WorkspaceProvider` 生命周期内创建独立的房间刷新调度器。调度器通过 Store 的 `refreshRoomMetadata` 复用现有在线判断和播放源检查逻辑，不新增 IPC 或播放源协议；每个房间通过递归 `setTimeout` 调度，使用在线/离线基础间隔、确定性错峰和失败退避。

**Tech Stack:** React、Zustand、TypeScript、Vitest

---

### Task 1: Add scheduler contract and failing tests

**Files:**
- Create: `src/renderer/store/workspace-refresh.ts`
- Create: `tests/workspace-refresh.test.ts`

- [x] **Step 1:** Write tests for different online/offline intervals, failure backoff, removed rooms, and disposal using injected fake timers.
- [x] **Step 2:** Run `npm test -- tests/workspace-refresh.test.ts` and confirm RED because the scheduler module does not exist.
- [x] **Step 3:** Implement `createWorkspaceRefreshScheduler(options)` with one recursive timer per room, constants for 60s online, 120s offline, and 30/60/120/240s failure backoff. Re-read current room state before each refresh and stop scheduling removed rooms.
- [x] **Step 4:** Run `npm test -- tests/workspace-refresh.test.ts` and confirm GREEN.

### Task 2: Wire scheduler into WorkspaceProvider

**Files:**
- Modify: `src/renderer/store/workspace-context.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [x] **Step 1:** Add a Provider lifecycle wiring test that verifies scheduler creation, metadata refresh delegation, and disposal hooks.
- [x] **Step 2:** Run `npm test -- tests/workspace-refresh-provider.test.ts` and confirm the new assertions fail before wiring.
- [x] **Step 3:** Subscribe only to the stable room-id list, create the scheduler for the existing Store, call `sync()` when room IDs change, skip it in demo mode, and dispose it during cleanup. Keep missing-avatar refresh separate.
- [x] **Step 4:** Run `npm test -- tests/workspace-refresh-provider.test.ts tests/workspace-refresh.test.ts` and confirm GREEN.

### Task 3: Full verification

**Files:**
- Verify: `README.md`
- Verify: `docs/superpowers/specs/2026-08-15-periodic-status-refresh-design.md`

- [x] **Step 1:** Run `npm test` and confirm all Vitest files pass.
- [x] **Step 2:** Run `npm run typecheck` and `npm run build`; confirm both exit successfully.
- [x] **Step 3:** Run `git diff --check` and inspect that only scheduler, Provider, tests, README, and planning docs changed. Confirm no playback signing or credential logic was added.
